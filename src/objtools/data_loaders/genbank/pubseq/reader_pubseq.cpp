/*  $Id$
* ===========================================================================
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
* ===========================================================================
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from Pubseq_OS
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/pubseq/reader_pubseq.hpp>
#include <objtools/data_loaders/genbank/pubseq/reader_pubseq_entry.hpp>
#include <objtools/data_loaders/genbank/pubseq/reader_pubseq_params.h>
#include <objtools/data_loaders/genbank/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/annot_selector.hpp>

#include <dbapi/driver/types.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>

#include <corelib/ncbicntr.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/rwstream.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <util/compress/zlib.hpp>

#include <corelib/ncbi_cookies.hpp>

#define DEFAULT_DB_SERVER   "PUBSEQ_OS_PUBLIC_GI64"
#define DEFAULT_DB_USER     "anyone"
#define DEFAULT_DB_PASSWORD "allowed"
#define DEFAULT_DB_DRIVER   "ftds;ctlib"
#define DEFAULT_NUM_CONN    2
#define MAX_MT_CONN         5
#define DEFAULT_ALLOW_GZIP  true
#define DEFAULT_EXCL_WGS_MASTER true

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Pubseq

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(int, GENBANK, PUBSEQOS_DEBUG);
NCBI_PARAM_DEF_EX(int, GENBANK, PUBSEQOS_DEBUG, 0,
                  eParam_NoThread, GENBANK_PUBSEQOS_DEBUG);

static int GetDebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, PUBSEQOS_DEBUG)> s_Value;
    return s_Value->Get();
}

#define RPC_GET_ASN         "id_get_asn"
#define RPC_GET_BLOB_INFO   "id_get_blob_prop"

enum {
    fZipType_gzipped = 2
};


static void sx_SetIntId(CDB_RPCCmd& cmd, const char* param, TIntId id)
{
#ifdef NCBI_INT8_GI
    if ( Int4(id) != id ) {
        // do not fit in Int4
        CDB_BigInt idIn(id);
        cmd.SetParam(param, &idIn);
        return;
    }
#endif
    CDB_Int idIn(static_cast<Int4>(id));
    cmd.SetParam(param, &idIn);
}


static TIntId sx_GetIntId(CDB_Result& dbr, int pos)
{
    CDB_BigInt idGot;
    dbr.GetItem(&idGot);
    Int8 id = idGot.Value();
#ifndef NCBI_INT8_GI
    if ( TIntId(id) != id ) {
        NCBI_THROW_FMT(CLoaderException, eOtherError,
                       dbr.ItemName(pos) << " value overflow: " << id);
    }
    return TIntId(id);
#else
    return id;
#endif
}


struct SPubseqReaderReceiveData {
    SPubseqReaderReceiveData()
        : zip_type(0), blob_state(0), blob_version(0) 
        {
        }
    
    AutoPtr<CDB_Result> dbr;
    int zip_type;
    CReader::TBlobState blob_state;
    CReader::TBlobVersion blob_version;
};


CPubseqReader::CPubseqReader(int max_connections,
                             const string& server,
                             const string& user,
                             const string& pswd,
                             const string& dbapi_driver)
    : m_Server(server) , m_User(user), m_Password(pswd),
      m_DbapiDriver(dbapi_driver),
      m_Context(nullptr),
      m_AllowGzip(DEFAULT_ALLOW_GZIP),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER),
      m_SetCubbyUser(false)
{
    LOG_POST_X_ONCE(1, "This app is using OM++ PubSeqOS reader which is being phased out. Please switch to using ID2 or PSG.");
    if ( m_Server.empty() ) {
        m_Server = DEFAULT_DB_SERVER;
    }
    if ( m_User.empty() ) {
        m_User = DEFAULT_DB_USER;
    }
    if ( m_Password.empty() ) {
        m_Password = DEFAULT_DB_PASSWORD;
    }
    if ( m_DbapiDriver.empty() ) {
        m_DbapiDriver = DEFAULT_DB_DRIVER;
    }

    SetMaximumConnections(max_connections, DEFAULT_NUM_CONN);
}


CPubseqReader::CPubseqReader(const TPluginManagerParamTree* params,
                             const string& driver_name)
    : m_Context(nullptr),
      m_AllowGzip(DEFAULT_ALLOW_GZIP),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER),
      m_SetCubbyUser(false)
{
    LOG_POST_X_ONCE(1, "This app is using OM++ PubSeqOS reader which is being phased out. Please switch to using ID2 or PSG.");
    CConfig conf(params);
    m_Server = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_SERVER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_SERVER);
    m_User = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_USER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_USER);
    m_Password = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_PASSWORD,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_PASSWORD);
    m_DbapiDriver = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_DRIVER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_DRIVER);
    m_AllowGzip = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_GZIP,
        CConfig::eErr_NoThrow,
        DEFAULT_ALLOW_GZIP);
    m_ExclWGSMaster = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_EXCL_WGS_MASTER,
        CConfig::eErr_NoThrow,
        DEFAULT_EXCL_WGS_MASTER);
    /*
    bool set_cubby_user = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_SET_CUBBY_USER,
        CConfig::eErr_NoThrow,
        false);
    if ( set_cubby_user ) {
        SetIncludeHUP(set_cubby_user);
    }
    */
    CReader::InitParams(conf, driver_name, DEFAULT_NUM_CONN);
}


CPubseqReader::~CPubseqReader()
{
    delete m_Context.exchange(nullptr);
}


int CPubseqReader::GetMaximumConnectionsLimit(void) const
{
#if defined(NCBI_THREADS)
    return MAX_MT_CONN;
#else
    return 1;
#endif
}


void CPubseqReader::x_AddConnectionSlot(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CPubseqReader::x_RemoveConnectionSlot(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CPubseqReader::x_DisconnectAtSlot(TConn conn, bool failed)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CDB_Connection>& stream = m_Connections[conn];
    if ( stream ) {
        x_ReportDisconnect("CPubseqReader", "PubSeqOS", conn, failed);
        stream.reset();
    }
}


CDB_Connection* CPubseqReader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CDB_Connection>& stream = m_Connections[conn];
    if ( stream.get() ) {
        return stream.get();
    }
    OpenConnection(conn);
    return m_Connections[conn].get();
}


namespace {
    class CPubseqValidator : public IConnValidator
    {
    public:
        CPubseqValidator(bool allow_gzip, bool excl_wgs_master)
            : m_AllowGzip(allow_gzip),
              m_ExclWGSMaster(excl_wgs_master)
            {
            }
        
        virtual EConnStatus Validate(CDB_Connection& conn) {
            if ( m_AllowGzip ) {
                AutoPtr<CDB_LangCmd> cmd
                    (conn.LangCmd("set accept gzip"));
                cmd->Send();
                cmd->DumpResults();
            }
            if ( m_ExclWGSMaster ) {
                AutoPtr<CDB_LangCmd> cmd
                    (conn.LangCmd("set exclude_wgs_master on"));
                cmd->Send();
                cmd->DumpResults();
            }
            return eValidConn;
        }

        virtual string GetName(void) const {
            return "CPubseqValidator";
        }

    private:
        bool m_AllowGzip, m_ExclWGSMaster;
    };
    
    I_BaseCmd* x_SendRequest2(const CBlob_id& blob_id,
                              CDB_Connection* db_conn,
                              const char* rpc)
    {
        string str = rpc;
        str += " ";
        if ( CPubseqReader::IsAnnotSat(blob_id.GetSat()) ) {
            str += NStr::NumericToString(CPubseqReader::GetExtAnnotGi(blob_id));
            str += ",";
            str += NStr::NumericToString(blob_id.GetSat());
            str += ",";
            str += NStr::NumericToString(CPubseqReader::GetExtAnnotSubSat(blob_id));
        }
        else {
            str += NStr::NumericToString(blob_id.GetSatKey());
            str += ",";
            str += NStr::NumericToString(blob_id.GetSat());
            str += ",";
            str += NStr::NumericToString(blob_id.GetSubSat());
        }
        AutoPtr<I_BaseCmd> cmd(db_conn->LangCmd(str));
        cmd->Send();
        return cmd.release();
    }
    

    bool sx_FetchNextItem(CDB_Result& result, const CTempString& name)
    {
        while ( result.Fetch() ) {
            for ( unsigned pos = 0; pos < result.NofItems(); ++pos ) {
                if ( result.ItemName(pos) == name ) {
                    return true;
                }
                result.SkipItem();
            }
        }
        return false;
    }
    
    class CDB_Result_Reader : public CObject, public IReader
    {
    public:
        CDB_Result_Reader(AutoPtr<CDB_Result> db_result)
            : m_DB_Result(db_result)
            {
            }

        ERW_Result Read(void*   buf,
                        size_t  count,
                        size_t* bytes_read)
            {
                if ( !count ) {
                    if ( bytes_read ) {
                        *bytes_read = 0;
                    }
                    return eRW_Success;
                }
                size_t ret;
                while ( (ret = m_DB_Result->ReadItem(buf, count)) == 0 ) {
                    if ( !sx_FetchNextItem(*m_DB_Result, "asn1") ) {
                        break;
                    }
                }
                if ( bytes_read ) {
                    *bytes_read = ret;
                }
                return ret? eRW_Success: eRW_Eof;
            }
        ERW_Result PendingCount(size_t* /*count*/)
            {
                return eRW_NotImplemented;
            }

    private:
        AutoPtr<CDB_Result> m_DB_Result;
    };
}


void CPubseqReader::x_ConnectAtSlot(TConn conn_)
{
    auto context = m_Context.load(memory_order_acquire);
    if ( !context ) {
      DEFINE_STATIC_FAST_MUTEX(s_Mutex);
      CFastMutexGuard guard(s_Mutex);
      context = m_Context.load(memory_order_acquire);
      if ( !context ) {
        DBLB_INSTALL_DEFAULT_EX(eIfSet_KeepSilently);
        C_DriverMgr drvMgr;
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        args["version"]="125"; // for correct connection to OpenServer
        vector<string> driver_list;
        NStr::Split(m_DbapiDriver, ";", driver_list);
        size_t driver_count = driver_list.size();
        vector<string> errmsg(driver_count);
        for ( size_t i = 0; i < driver_count; ++i ) {
            try {
                context = drvMgr.GetDriverContext(driver_list[i],
                                                  &errmsg[i], &args);
                if ( context ) {
                    break;
                }
            }
            catch ( exception& exc ) {
                errmsg[i] = exc.what();
            }
        }
        if ( !context ) {
            for ( size_t i = 0; i < driver_count; ++i ) {
                ERR_POST_X(2, "Failed to create dbapi context with driver '"
                           <<driver_list[i]<<"': "<<errmsg[i]);
            }
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Cannot create dbapi context with driver '"+
                       m_DbapiDriver+"'");
        }
      }
      m_Context.store(context, memory_order_release);
    }

    _TRACE("CPubseqReader::NewConnection("<<m_Server<<")");
    CPubseqValidator validator(m_AllowGzip, m_ExclWGSMaster);
    AutoPtr<CDB_Connection> conn
        (context->ConnectValidated(m_Server, m_User, m_Password, validator));
    
    if ( !conn.get() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed, "connection failed");
    }
    
    if ( m_SetCubbyUser ) {
        // Using a formal parameter is typically better practice, but
        // likely won't work here (a custom Open Server).
        string encoded = NStr::SQLEncode(CSystemInfo::GetUserName(),
                                         NStr::eSqlEnc_TagNonASCII);
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set cubby_user " + encoded));
        cmd->Send();
        cmd->DumpResults();
    }

    if ( GetDebugLevel() >= 2 ) {
        CDebugPrinter s(conn_, "CPubseqReader");
        s << "Connected to " << conn->ServerName();
    }

    m_Connections[conn_].reset(conn.release());
}


// LoadSeq_idGi, LoadSeq_idSeq_ids, and LoadSeq_idBlob_ids
// are implemented here and call the same function because
// PubSeqOS has one RPC call that may suite all needs
// LoadSeq_idSeq_ids works like this only when Seq-id is not gi
// To prevent deadlocks these functions lock Seq-ids before Blob-ids.

bool CPubseqReader::LoadSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id)
{
    CLoadLockGi lock(result, seq_id);
    if ( lock.IsLoadedGi() ) {
        return true;
    }
    return LoadSeq_idInfo(result, seq_id, 0);
}


bool CPubseqReader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CLoadLockSeqIds lock(result, seq_id);
    if ( lock.IsLoaded() ) {
        return true;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        return LoadGiSeq_ids(result, seq_id);
    }

    CLoadLockGi gi_lock(result, seq_id);
    if ( !gi_lock.IsLoadedGi() ) {
        m_Dispatcher->LoadSeq_idGi(result, seq_id);
        if ( lock.IsLoaded() ) {
            return true;
        }
        if ( !gi_lock.IsLoadedGi() ) {
            return false;
        }
    }
    TSequenceGi data = gi_lock.GetGi();
    TGi gi = gi_lock.GetGi(data);
    if ( gi == ZERO_GI ) {
        // no gi -> no Seq-ids
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, 0);
        return true;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    CLoadLockSeqIds gi_ids(result, gi_handle);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);
    if ( !gi_ids.IsLoaded() ) {
        return false;
    }
    
    // copy Seq-id list from gi to original seq-id
    SetAndSaveSeq_idSeq_ids(result, seq_id, gi_ids);
    return true;
}


bool CPubseqReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id,
                                       const SAnnotSelector* sel)
{
    CLoadLockBlobIds blob_ids(result, seq_id, sel);
    if ( blob_ids.IsLoaded() ) {
        return true;
    }

    CLoadLockSeqIds seq_ids(result, seq_id, eAlreadyLoaded);
    if ( seq_ids ) {
        CFixedSeq_ids::TState state = seq_ids.GetState();
        if ( state & CBioseq_Handle::fState_no_data ) {
            // no such seq-id
            SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, state);
            return true;
        }
    }
    
    if ( !sel || !sel->IsIncludedAnyNamedAnnotAccession() ) {
        // no named annot accessions
        return LoadSeq_idInfo(result, seq_id, 0);
    }

    if ( sel->IsIncludedNamedAnnotAccession("NA*") ) {
        // all named annot accessions
        return LoadSeq_idInfo(result, seq_id, sel);
    }

    // part of named annot accessions
    SAnnotSelector all_sel;
    all_sel.IncludeNamedAnnotAccession("NA*");
    if ( !LoadSeq_idInfo(result, seq_id, &all_sel) ) {
        return false;
    }
    CLoadLockBlobIds all_blob_ids(result, seq_id, &all_sel);
    _ASSERT(all_blob_ids.IsLoaded());
    // filter accessions
    SetAndSaveSeq_idBlob_ids(result, seq_id, sel, blob_ids, all_blob_ids);
    return true;
}


bool CPubseqReader::LoadSeq_idAccVer(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    CLoadLockAcc lock(result, seq_id);
    if ( lock.IsLoadedAccVer() ) {
        return true;
    }

    if ( seq_id.IsGi() ) {
        _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
        TGi gi = seq_id.GetGi();
        if (gi != ZERO_GI) {
            _TRACE("ResolveGi to Acc: " << gi);

            CConn conn(result, this);
            {{
                CDB_Connection* db_conn = x_GetConnection(conn);
    
                AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_get_accn_ver_by_gi"));
                sx_SetIntId(*cmd, "@gi", GI_TO(TIntId, CProcessor::ConvertGiFromOM(gi)));
                cmd->Send();
                
                //bool not_found = false;
                while ( cmd->HasMoreResults() ) {
                    AutoPtr<CDB_Result> dbr(cmd->Result());
                    if ( !dbr.get() ) {
                        continue;
                    }
            
                    if ( dbr->ResultType() == eDB_StatusResult ) {
                        dbr->Fetch();
                        CDB_Int v;
                        dbr->GetItem(&v);
                        int status = v.Value();
                        if ( status == 100 ) {
                            // gi does not exist
                            // but there no way to report it except by
                            // trying to load all Seq-ids later
                            //not_found = true;
                        }
                    }
                    else if ( dbr->ResultType() == eDB_RowResult &&
                              sx_FetchNextItem(*dbr, "accver") ) {
                        CDB_VarChar accVerGot;
                        dbr->GetItem(&accVerGot);
                        try {
                            TSequenceAcc data;
                            data.acc_ver =
                                CSeq_id_Handle::GetHandle(accVerGot.Value());
                            data.sequence_found = true;
                            SetAndSaveSeq_idAccVer(result, seq_id, data);
                            while ( dbr->Fetch() )
                                ;
                            cmd->DumpResults();
                            break;
                        }
                        catch ( exception& /*exc*/ ) {
                            /*
                            ERR_POST_X(7,
                                       "CPubseqReader: bad accver data: "<<
                                       " gi "<<gi<<" -> "<<accVerGot.Value()<<
                                       ": "<< exc.GetMsg());
                            */
                        }
                    }
                    while ( dbr->Fetch() )
                        ;
                }
            }}
            conn.Release();
        }
    }

    if ( !lock.IsLoadedAccVer() ) {
        return CId1ReaderBase::LoadSeq_idAccVer(result, seq_id);
    }

    return true;
}


bool CPubseqReader::LoadSeq_idInfo(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id,
                                   const SAnnotSelector* with_named_accs)
{
    // Get gi by seq-id
    _TRACE("ResolveSeq_id to gi/sat/satkey: " << seq_id.AsString());

    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        if ( seq_id.IsGi() ) {
            oss << "Seq-id ::= gi " << CProcessor::ConvertGiFromOM(seq_id.GetGi());
        }
        else {
            CObjectOStreamAsn ooss(oss);
            ooss << *CProcessor::ConvertIdFromOM(seq_id).GetSeqId();
        }
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    int result_count = 0;
    TGi named_gi = ZERO_GI;
    CConn conn(result, this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);

        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn"));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();
        
        TBlobIds blob_ids;
        
        while(cmd->HasMoreResults()) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get() ) {
                continue;
            }
            
            if ( dbr->ResultType() != eDB_RowResult) {
                while ( dbr->Fetch() )
                    ;
                continue;
            }
        
            while ( dbr->Fetch() ) {
                TGi gi = ZERO_GI;
                CDB_Int satGot;
                CDB_Int sat_keyGot;
                CDB_Int extFeatGot;
                CDB_Int namedAnnotsGot;

                _TRACE("next fetch: " << dbr->NofItems() << " items");
                ++result_count;
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if (name == "gi") {
                        gi = GI_FROM(TIntId, sx_GetIntId(*dbr, pos));
                        _TRACE("gi: "<<gi);
                        CProcessor::OffsetGiToOM(gi);
                    }
                    else if (name == "sat" ) {
                        dbr->GetItem(&satGot);
                        _TRACE("sat: "<<satGot.Value());
                    }
                    else if(name == "sat_key") {
                        dbr->GetItem(&sat_keyGot);
                        _TRACE("sat_key: "<<sat_keyGot.Value());
                    }
                    else if(name == "extra_feat" || name == "ext_feat") {
                        dbr->GetItem(&extFeatGot);
#ifdef _DEBUG
                        if ( extFeatGot.IsNULL() ) {
                            _TRACE("ext_feat = NULL");
                        }
                        else {
                            _TRACE("ext_feat = "<<extFeatGot.Value());
                        }
#endif
                    }
                    else if (name == "named_annots" ) {
                        dbr->GetItem(&namedAnnotsGot);
                        _TRACE("named_annots = "<<namedAnnotsGot.Value());
                        if ( namedAnnotsGot.Value() ) {
                            named_gi = gi;
                        }
                    }
                    else {
                        dbr->SkipItem();
                    }
                }

                int sat = satGot.Value();
                int sat_key = sat_keyGot.Value();
                
                if ( GetDebugLevel() >= 5 ) {
                    CDebugPrinter s(conn, "CPubseqReader");
                    s << "ResolveSeq_id"
                        "(" << seq_id.AsString() << ")"
                        " gi=" << gi <<
                        " sat=" << sat <<
                        " satkey=" << sat_key <<
                        " extfeat=";
                    if ( extFeatGot.IsNULL() ) {
                        s << "NULL";
                    }
                    else {
                        s << extFeatGot.Value();
                    }
                }

                if ( gi == ZERO_GI && !sat && !sat_key ) {
                    // no sequence found
                    SetAndSaveSeq_idGi(result, seq_id, TSequenceGi());
                }
                else {
                    // we've got gi, possibly ZERO_GI meaning no GI on sequence
                    TSequenceGi data;
                    data.gi = gi;
                    data.sequence_found = true;
                    SetAndSaveSeq_idGi(result, seq_id, data);
                }
                
                if ( CProcessor::TrySNPSplit() && !IsAnnotSat(sat) ) {
                    // main blob
                    CRef<CBlob_id> blob_id(new CBlob_id);
                    blob_id->SetSat(sat);
                    blob_id->SetSatKey(sat_key);
                    blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
                    if ( !extFeatGot.IsNULL() ) {
                        CreateExtAnnotBlob_ids(blob_ids, GI_TO(TIntId, CProcessor::ConvertGiFromOM(gi)), extFeatGot.Value());
                    }
                }
                else {
                    // whole blob
                    CRef<CBlob_id> blob_id(new CBlob_id);
                    blob_id->SetSat(sat);
                    blob_id->SetSatKey(sat_key);
                    if ( !extFeatGot.IsNULL() ) {
                        blob_id->SetSubSat(extFeatGot.Value());
                    }
                    blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
                }
            }
        }

        cmd.reset();

        if ( with_named_accs && named_gi != ZERO_GI ) {
            // postponed read of named annot accessions
            _TRACE("id_get_annot_types "<<named_gi);
            AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_get_annot_types"));
            sx_SetIntId(*cmd, "@gi", GI_TO(TIntId, CProcessor::ConvertGiFromOM(named_gi)));
            cmd->Send();
            while(cmd->HasMoreResults()) {
                AutoPtr<CDB_Result> dbr(cmd->Result());
                if ( !dbr.get() ) {
                    continue;
                }

                if ( dbr->ResultType() != eDB_RowResult) {
                    while ( dbr->Fetch() )
                        ;
                    continue;
                }
                
                while ( dbr->Fetch() ) {
                    TGi gi = ZERO_GI;
                    CDB_Int satGot;
                    CDB_Int satKeyGot;
                    CDB_Int typeGot;
                    CDB_VarChar nameGot;
                    for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                        const string& name = dbr->ItemName(pos);
                        _TRACE("next item: " << name);
                        if (name == "gi") {
                            gi = GI_FROM(TIntId, sx_GetIntId(*dbr, pos));
                            _TRACE("ngi: "<<gi);
                            CProcessor::OffsetGiToOM(gi);
                        }
                        else if (name == "sat" ) {
                            dbr->GetItem(&satGot);
                            _TRACE("nsat: "<<satGot.Value());
                        }
                        else if(name == "sat_key") {
                            dbr->GetItem(&satKeyGot);
                            _TRACE("nsat_key: "<<satKeyGot.Value());
                        }
                        else if(name == "type") {
                            dbr->GetItem(&typeGot);
                            _TRACE("ntype: "<<typeGot.Value());
                        }
                        else if(name == "name") {
                            dbr->GetItem(&nameGot);
                            _TRACE("nname: "<<nameGot.Value());
                        }
                        else {
                            dbr->SkipItem();
                        }
                    }
                    CRef<CBlob_id> blob_id(new CBlob_id);
                    blob_id->SetSat(satGot.Value());
                    blob_id->SetSatKey(satKeyGot.Value());
                    CBlob_Info info(blob_id, fBlobHasNamedFeat);
                    CRef<CBlob_Annot_Info> annot_info(new CBlob_Annot_Info);
                    annot_info->AddNamedAnnotName(nameGot.Value());
                    info.SetAnnotInfo(annot_info);
                    blob_ids.push_back(info);
                }
            }
        }

        if ( result_count == 0 ) {
            SetAndSaveSeq_idGi(result, seq_id, TSequenceGi());
            SetAndSaveNoSeq_idSeq_ids(result, seq_id, 0);
            SetAndSaveNoSeq_idBlob_ids(result, seq_id, with_named_accs, 0);
        }
        else {
            SetAndSaveSeq_idBlob_ids(result, seq_id, with_named_accs,
                                     CFixedBlob_ids(eTakeOwnership, blob_ids));
        }
    }}

    conn.Release();
    return result_count > 0;
}


bool CPubseqReader::LoadGiSeq_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id)
{
    CLoadLockSeqIds load_lock(result, seq_id);
    if ( load_lock.IsLoaded() ) {
        return true;
    }
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    TGi gi = seq_id.GetGi();
    if ( gi == ZERO_GI ) {
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, 0);
        return true;
    }

    _TRACE("ResolveGi to Seq-ids: " << gi);

    CConn conn(result, this);
    {{
        TSeqIds ids;
        CDB_Connection* db_conn = x_GetConnection(conn);
    
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_seqid4gi"));
        sx_SetIntId(*cmd, "@gi", GI_TO(TIntId, CProcessor::ConvertGiFromOM(gi)));
        CDB_TinyInt binIn = 1;
        cmd->SetParam("@bin", &binIn);
        cmd->Send();

        int state = 0;
        bool not_found = false;
        while ( cmd->HasMoreResults() ) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get() ) {
                continue;
            }
            
            if ( dbr->ResultType() == eDB_StatusResult ) {
                while ( dbr->Fetch() ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    int status = v.Value();
                    if ( status == 100 ) {
                        // gi does not exist
                        not_found = true;
                        state |= CBioseq_Handle::fState_not_found;
                    }
                }
                if ( not_found ) {
                    cmd->Cancel();
                    break;
                }
            }
            else if ( dbr->ResultType() == eDB_RowResult &&
                      sx_FetchNextItem(*dbr, "seqid") ) {
                CDB_Result_Reader reader(dbr);
                CRStream stream(&reader);
                CObjectIStreamAsnBinary in(stream);
                CSeq_id id;
                while ( in.HaveMoreData() ) {
                    in >> id;
                    CProcessor::OffsetIdToOM(id);
                    ids.push_back(CSeq_id_Handle::GetHandle(id));
                }
                if ( in.HaveMoreData() ) {
                    ERR_POST_X(4, "CPubseqReader: extra seqid data");
                }
                while ( dbr->Fetch() )
                    ;
                cmd->DumpResults();
                break;
            }
            while ( dbr->Fetch() )
                ;
        }
        if ( ids.empty() && !not_found ) {
            // artificially add argument Seq-id if empty set was received
            ids.push_back(seq_id);
        }
        SetAndSaveSeq_idSeq_ids(result, seq_id,
                                CFixedSeq_ids(eTakeOwnership, ids, state));
    }}
    conn.Release();
    return true;
}


bool CPubseqReader::LoadSequenceHash(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    CLoadLockHash lock(result, seq_id);
    if ( lock.IsLoaded() ) {
        return true;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        return LoadGiHash(result, seq_id);
    }

    CLoadLockGi gi_lock(result, seq_id);
    if ( !gi_lock.IsLoadedGi() ) {
        m_Dispatcher->LoadSeq_idGi(result, seq_id);
        if ( lock.IsLoaded() ) {
            return true;
        }
        if ( !gi_lock.IsLoadedGi() ) {
            return false;
        }
    }
    TSequenceGi gi = gi_lock.GetGi();
    if ( !gi_lock.IsFound(gi) ) {
        // no sequence
        result.SetLoadedHash(seq_id, TSequenceHash());
        return true;
    }
    if ( gi_lock.GetGi(gi) == ZERO_GI ) {
        // no gi -> no hash known
        TSequenceHash hash;
        hash.sequence_found = true;
        result.SetLoadedHash(seq_id, hash);
        return true;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi_lock.GetGi(gi));
    CLoadLockHash gi_hash(result, gi_handle);
    m_Dispatcher->LoadSequenceHash(result, gi_handle);
    if ( !gi_hash.IsLoadedHash() ) {
        return false;
    }
    
    // copy Seq-id list from gi to original seq-id
    result.SetLoadedHash(seq_id, gi_hash.GetHash());
    return true;
}


bool CPubseqReader::LoadGiHash(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id)
{
    CLoadLockSeqIds load_lock(result, seq_id);
    if ( load_lock.IsLoaded() ) {
        return true;
    }
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    TGi gi = seq_id.GetGi();
    if ( gi == ZERO_GI ) {
        result.SetLoadedHash(seq_id, TSequenceHash());
        return true;
    }

    _TRACE("ResolveGi to Hash: " << gi);

    CConn conn(result, this);
    {{
        TSeqIds ids;
        CDB_Connection* db_conn = x_GetConnection(conn);
    
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_class"));
        sx_SetIntId(*cmd, "@gi", GI_TO(TIntId, CProcessor::ConvertGiFromOM(gi)));
        CDB_TinyInt hashIn = 1;
        cmd->SetParam("@ver", &hashIn);
        cmd->Send();

        TSequenceHash hash;
        while ( cmd->HasMoreResults() ) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get() ) {
                continue;
            }
            
            if ( dbr->ResultType() == eDB_RowResult &&
                      sx_FetchNextItem(*dbr, "hash") ) {
                CDB_Int v;
                dbr->GetItem(&v);
                hash.hash = v.Value();
                hash.sequence_found = true;
                hash.hash_known = true;
            }
            while ( dbr->Fetch() )
                ;
        }
        SetAndSaveSequenceHash(result, seq_id, hash);
    }}
    conn.Release();
    return true;
}


void CPubseqReader::GetBlobState(CReaderRequestResult& result, 
                                 const CBlob_id& blob_id)
{
    CConn conn(result, this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<I_BaseCmd> cmd
            (x_SendRequest2(blob_id, db_conn, RPC_GET_BLOB_INFO));
        SPubseqReaderReceiveData data;
        x_ReceiveData(result, data, blob_id, *cmd, false);
        if ( data.dbr ) {
            ERR_POST_X(3, "CPubseqReader: unexpected blob data");
        }
    }}
    conn.Release();
    if ( !blob_id.IsMainBlob() ) {
        result.SetAndSaveBlobState(blob_id, 0);
    }
}


void CPubseqReader::GetBlobVersion(CReaderRequestResult& result, 
                                   const CBlob_id& blob_id)
{
    try {
        CConn conn(result, this);
        {{
            CDB_Connection* db_conn = x_GetConnection(conn);
            AutoPtr<I_BaseCmd> cmd
                (x_SendRequest2(blob_id, db_conn, RPC_GET_BLOB_INFO));
            SPubseqReaderReceiveData data;
            x_ReceiveData(result, data, blob_id, *cmd, false);
            if ( data.dbr ) {
                ERR_POST_X(5, "CPubseqReader: unexpected blob data");
            }
        }}
        conn.Release();
        if ( !blob_id.IsMainBlob() ) {
            result.SetAndSaveBlobVersion(blob_id, 0);
        }
    }
    catch ( exception& ) {
        if ( !blob_id.IsMainBlob() ) {
            result.SetAndSaveBlobVersion(blob_id, 0);
            return;
        }
        throw;
    }
}


void CPubseqReader::GetBlob(CReaderRequestResult& result,
                            const TBlobId& blob_id,
                            TChunkId chunk_id)
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    if ( blob.IsLoadedChunk() ) {
        return;
    }
    CConn conn(result, this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<I_BaseCmd> cmd(x_SendRequest(blob_id, db_conn, RPC_GET_ASN));
        SPubseqReaderReceiveData data;
        x_ReceiveData(result, data, blob_id, *cmd, true);
        if ( data.dbr ) {
            CDB_Result_Reader reader(data.dbr);
            CRStream stream(&reader);
            CProcessor::EType processor_type;
            if ( blob_id.GetSubSat() == eSubSat_SNP ) {
                processor_type = CProcessor::eType_Seq_entry_SNP;
            }
            else {
                processor_type = CProcessor::eType_Seq_entry;
            }
            if ( data.zip_type & fZipType_gzipped ) {
                CCompressionIStream unzip(stream,
                                          new CZipStreamDecompressor,
                                          CCompressionIStream::fOwnProcessor);
                m_Dispatcher->GetProcessor(processor_type)
                    .ProcessStream(result, blob_id, chunk_id, unzip);
            }
            else {
                m_Dispatcher->GetProcessor(processor_type)
                    .ProcessStream(result, blob_id, chunk_id, stream);
            }
            char buf[1];
            if ( stream.read(buf, 1) && stream.gcount() ) {
                ERR_POST_X(6, "CPubseqReader: extra blob data: "<<blob_id);
            }
            cmd->DumpResults();
        }
        else {
            SetAndSaveNoBlob(result, blob_id, chunk_id, data.blob_state);
        }
    }}
    conn.Release();
}


I_BaseCmd* CPubseqReader::x_SendRequest(const CBlob_id& blob_id,
                                        CDB_Connection* db_conn,
                                        const char* rpc)
{
    _TRACE("x_SendRequest: "<<blob_id.ToString());
    AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC(rpc));
    if ( IsAnnotSat(blob_id.GetSat()) ) {
        // extract actual GI from sat_key and sub_sat fields
        CDB_BigInt idIn(GetExtAnnotGi(blob_id));
        cmd->SetParam("@gi", &idIn);
        CDB_SmallInt satIn(blob_id.GetSat());
        cmd->SetParam("@sat", &satIn);
        CDB_Int ext_feat(GetExtAnnotSubSat(blob_id));
        cmd->SetParam("@ext_feat", &ext_feat);
    }
    else {
        CDB_SmallInt satIn(blob_id.GetSat());
        cmd->SetParam("@sat", &satIn);
        CDB_Int satKeyIn(blob_id.GetSatKey());
        cmd->SetParam("@sat_key", &satKeyIn);
        CDB_Int ext_feat(blob_id.GetSubSat());
        cmd->SetParam("@ext_feat", &ext_feat);
    }
    cmd->Send();
    return cmd.release();
}


void CPubseqReader::x_ReceiveData(CReaderRequestResult& result,
                                  SPubseqReaderReceiveData& ret,
                                  const TBlobId& blob_id,
                                  I_BaseCmd& cmd,
                                  bool force_blob)
{
    enum {
        kState_dead = 125
    };

    // new row
    while( !ret.dbr && cmd.HasMoreResults() ) {
        _TRACE("next result");
        if ( cmd.HasFailed() ) {
            break;
        }
        
        AutoPtr<CDB_Result> dbr(cmd.Result());
        if ( !dbr.get() ) {
            continue;
        }

        if ( dbr->ResultType() != eDB_RowResult ) {
            while ( dbr->Fetch() )
                ;
            continue;
        }
        
        while ( !ret.dbr && dbr->Fetch() ) {
            _TRACE("next fetch: " << dbr->NofItems() << " items");
            for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                const string& name = dbr->ItemName(pos);
                _TRACE("next item: " << name);
                if ( name == "confidential" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("confidential: "<<v.Value());
                    if ( v.Value() ) {
                        ret.blob_state |= CBioseq_Handle::fState_confidential;
                    }
                }
                else if ( name == "suppress" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("suppress: "<<v.Value());
                    if ( v.Value() & 5 ) { // suppressed(=1) & temporary(=4)
                        ret.blob_state |= (v.Value() == 4)
                            ? CBioseq_Handle::fState_suppress_temp
                            : CBioseq_Handle::fState_suppress_perm;
                    }
                }
                else if ( name == "override" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("withdrawn: "<<v.Value());
                    if ( v.Value() ) {
                        ret.blob_state |= CBioseq_Handle::fState_withdrawn;
                    }
                }
                else if ( name == "last_touched_m" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("version: " << v.Value());
                    ret.blob_version = v.Value();
                }
                else if ( name == "state" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("state: "<<v.Value());
                    if ( v.Value() == kState_dead ) {
                        ret.blob_state |= CBioseq_Handle::fState_dead;
                    }
                }
                else if ( name == "zip_type" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("zip_type: "<<v.Value());
                    ret.zip_type = v.Value();
                }
                else if ( name == "asn1" ) {
                    ret.dbr.reset(dbr.release());
                    break;
                }
                else {
#ifdef _DEBUG
                    AutoPtr<CDB_Object> item(dbr->GetItem(0));
                    _TRACE("item type: " << item->GetType());
                    switch ( item->GetType() ) {
                    case eDB_Int:
                    case eDB_SmallInt:
                    case eDB_TinyInt:
                    {
                        CDB_Int v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    case eDB_VarChar:
                    {
                        CDB_VarChar v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    default:
                        break;
                    }
#else
                    dbr->SkipItem();
#endif
                }
            }
        }
    }
    if ( !ret.dbr && force_blob ) {
        // no data
        _TRACE("actually no data");
        ret.blob_state |= CBioseq_Handle::fState_no_data;
    }
    result.SetAndSaveBlobState(blob_id, ret.blob_state);
    result.SetAndSaveBlobVersion(blob_id, ret.blob_version);
}


void CPubseqReader::SetIncludeHUP(bool include_hup, const string& web_cookie)
{
    x_SetIncludeHUP(include_hup);
    m_SetCubbyUser = include_hup;
    if ( !web_cookie.empty() ) {
        // link-in corelib classes used by CPubseqReader2
        CHttpCookies();
    }
}


END_SCOPE(objects)

void GenBankReaders_Register_Pubseq(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_ReaderPubseqos);
}


/// Class factory for Pubseq reader
///
/// @internal
///
class CPubseqReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader,
                                   objects::CPubseqReader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CPubseqReader> TParent;
public:
    CPubseqReaderCF()
        : TParent(NCBI_GBLOADER_READER_PUBSEQ_DRIVER_NAME, 0) {}

    ~CPubseqReaderCF() {}

    objects::CReader* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        objects::CReader* drv = 0;
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CReader)) 
                            != CVersionInfo::eNonCompatible) {
            drv = new objects::CPubseqReader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_ReaderPubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CPubseqReaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}


END_NCBI_SCOPE
