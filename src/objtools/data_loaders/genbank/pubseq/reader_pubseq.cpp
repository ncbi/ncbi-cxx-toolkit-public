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
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/annot_selector.hpp>

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

#define DEFAULT_DB_SERVER   "PUBSEQ_OS_PUBLIC"
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

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif

#ifdef _DEBUG
NCBI_PARAM_DECL(int, GENBANK, PUBSEQOS_DEBUG);
NCBI_PARAM_DEF_EX(int, GENBANK, PUBSEQOS_DEBUG, 0,
                  eParam_NoThread, GENBANK_PUBSEQOS_DEBUG);

static int GetDebugLevel(void)
{
    static const int s_Value =
        NCBI_PARAM_TYPE(GENBANK, PUBSEQOS_DEBUG)::GetDefault();
    return s_Value;
}
#else
# define GetDebugLevel() (0)
#endif

#define RPC_GET_ASN         "id_get_asn"
#define RPC_GET_BLOB_INFO   "id_get_blob_prop"

enum {
    fZipType_gzipped = 2
};

CPubseqReader::CPubseqReader(int max_connections,
                             const string& server,
                             const string& user,
                             const string& pswd,
                             const string& dbapi_driver)
    : m_Server(server) , m_User(user), m_Password(pswd),
      m_DbapiDriver(dbapi_driver),
      m_Context(0),
      m_AllowGzip(DEFAULT_ALLOW_GZIP),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER)
{
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

#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif
    SetMaximumConnections(max_connections, DEFAULT_NUM_CONN);
}


CPubseqReader::CPubseqReader(const TPluginManagerParamTree* params,
                             const string& driver_name)
    : m_Context(0),
      m_AllowGzip(DEFAULT_ALLOW_GZIP),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER)
{
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

#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif

    CReader::InitParams(conf, driver_name, DEFAULT_NUM_CONN);
}


CPubseqReader::~CPubseqReader()
{
#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    s_pubseq_readers.Add(-1);
#endif
}


int CPubseqReader::GetMaximumConnectionsLimit(void) const
{
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
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
        LOG_POST_X(1, Warning << "CPubseqReader("<<conn<<"): PubSeqOS"
                   " GenBank connection "<<(failed? "failed": "too old")<<
                   ": reconnecting...");
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
        str += NStr::IntToString(blob_id.GetSatKey());
        str += ",";
        str += NStr::IntToString(blob_id.GetSat());
        str += ",";
        str += NStr::IntToString(blob_id.GetSubSat());
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
    if ( !m_Context ) {
        DBLB_INSTALL_DEFAULT();
        C_DriverMgr drvMgr;
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        args["version"]="125"; // for correct connection to OpenServer
        vector<string> driver_list;
        NStr::Tokenize(m_DbapiDriver, ";", driver_list);
        size_t driver_count = driver_list.size();
        vector<string> errmsg(driver_count);
        for ( size_t i = 0; i < driver_count; ++i ) {
            try {
                m_Context = drvMgr.GetDriverContext(driver_list[i],
                                                    &errmsg[i], &args);
                if ( m_Context )
                    break;
            }
            catch ( exception& exc ) {
                errmsg[i] = exc.what();
            }
        }
        if ( !m_Context ) {
            for ( size_t i = 0; i < driver_count; ++i ) {
                LOG_POST_X(2, "Failed to create dbapi context with driver '"
                           <<driver_list[i]<<"': "<<errmsg[i]);
            }
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Cannot create dbapi context with driver '"+
                       m_DbapiDriver+"'");
        }
    }

    _TRACE("CPubseqReader::NewConnection("<<m_Server<<")");
    CPubseqValidator validator(m_AllowGzip, m_ExclWGSMaster);
    AutoPtr<CDB_Connection> conn
        (m_Context->ConnectValidated(m_Server, m_User, m_Password, validator));
    
    if ( !conn.get() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed, "connection failed");
    }
    
    if ( GetDebugLevel() >= 2 ) {
        NcbiCout << "CPubseqReader::Connected to " << conn->ServerName()
                 << NcbiEndl;
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
    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids.IsLoadedGi() ) {
        return true;
    }

    return LoadSeq_idInfo(result, seq_id, 0);
}


bool CPubseqReader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        return LoadGiSeq_ids(result, seq_id);
    }

    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids.IsLoaded() ) {
        return true;
    }

    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    if ( seq_ids.IsLoaded() ) { // may be loaded as extra information for gi
        return true;
    }
    TGi gi = seq_ids->GetGi();
    if (gi == ZERO_GI) {
        // no gi -> no Seq-ids
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, 0);
        return true;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    CLoadLockSeq_ids gi_ids(result, gi_handle);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);
    
    // copy Seq-id list from gi to original seq-id
    seq_ids.SetLoadedSeq_ids(gi_ids);
    return true;
}


bool CPubseqReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id,
                                       const SAnnotSelector* sel)
{
    CLoadLockBlob_ids blob_ids(result, seq_id, sel);
    if ( blob_ids.IsLoaded() ) {
        return true;
    }

    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids.IsLoaded() &&
         (seq_ids->GetState() & CBioseq_Handle::fState_no_data) ) {
        // no such seq-id
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, seq_ids->GetState());
        return true;
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
    CLoadLockBlob_ids all_blob_ids(result, seq_id, &all_sel);
    _ASSERT(all_blob_ids.IsLoaded());
    // filter accessions
    blob_ids.SetLoadedBlob_ids(all_blob_ids);
    return true;
}


bool CPubseqReader::LoadSeq_idAccVer(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids.IsLoadedAccVer() ) {
        return true;
    }

    if ( seq_id.IsGi() ) {
        _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
        TGi gi;
        if ( seq_id.IsGi() ) {
            gi = seq_id.GetGi();
        }
        else {
            gi = seq_id.GetSeqId()->GetGi();
        }
        if (gi != ZERO_GI) {
            _TRACE("ResolveGi to Acc: " << gi);

            CConn conn(result, this);
            {{
                CDB_Connection* db_conn = x_GetConnection(conn);
    
                AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_get_accn_ver_by_gi"));
                CDB_Int giIn = GI_TO(Int4, gi);
                cmd->SetParam("@gi", &giIn);
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
                            CSeq_id id(accVerGot.Value());
                            SetAndSaveSeq_idAccVer(result, seq_id, id);
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

    if ( !seq_ids.IsLoadedAccVer() ) {
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
            oss << "Seq-id ::= gi " << seq_id.GetGi();
        }
        else {
            CObjectOStreamAsn ooss(oss);
            ooss << *seq_id.GetSeqId();
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
                CDB_Int giGot;
                CDB_Int satGot;
                CDB_Int satKeyGot;
                CDB_Int extFeatGot;
                CDB_Int namedAnnotsGot;

                _TRACE("next fetch: " << dbr->NofItems() << " items");
                ++result_count;
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if (name == "gi") {
                        dbr->GetItem(&giGot);
                        _TRACE("gi: "<<giGot.Value());
                    }
                    else if (name == "sat" ) {
                        dbr->GetItem(&satGot);
                        _TRACE("sat: "<<satGot.Value());
                    }
                    else if(name == "sat_key") {
                        dbr->GetItem(&satKeyGot);
                        _TRACE("sat_key: "<<satKeyGot.Value());
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
                            named_gi = GI_FROM(Int4, giGot.Value());
                        }
                    }
                    else {
                        dbr->SkipItem();
                    }
                }

                TGi gi = GI_FROM(Int4, giGot.Value());
                int sat = satGot.Value();
                int sat_key = satKeyGot.Value();
                
                if ( GetDebugLevel() >= 5 ) {
                    NcbiCout << "CPubseqReader::ResolveSeq_id"
                        "(" << seq_id.AsString() << ")"
                        " gi=" << gi <<
                        " sat=" << sat <<
                        " satkey=" << sat_key <<
                        " extfeat=";
                    if ( extFeatGot.IsNULL() ) {
                        NcbiCout << "NULL";
                    }
                    else {
                        NcbiCout << extFeatGot.Value();
                    }
                    NcbiCout << NcbiEndl;
                }

                if ( giGot.IsNULL() || gi == ZERO_GI ) {
                    // no gi -> only one Seq-id - the one used as argument
                    TSeqIds seq_ids(1, seq_id);
                    SetAndSaveSeq_idSeq_ids(result, seq_id, 0,
                                            CFixedSeq_ids(eTakeOwnership, seq_ids));
                }
                else {
                    // we've got gi
                    SetAndSaveSeq_idGi(result, seq_id, gi);
                }
                
                if ( CProcessor::TrySNPSplit() && !IsAnnotSat(sat) ) {
                    // main blob
                    CRef<CBlob_id> blob_id(new CBlob_id);
                    blob_id->SetSat(sat);
                    blob_id->SetSatKey(sat_key);
                    blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
                    if ( !extFeatGot.IsNULL() ) {
                        int ext_feat = extFeatGot.Value();
                        while ( ext_feat ) {
                            int bit = ext_feat & ~(ext_feat-1);
                            ext_feat -= bit;
                            blob_id = new CBlob_id;
                            blob_id->SetSat(GetAnnotSat(bit));
                            blob_id->SetSatKey(gi);
                            blob_id->SetSubSat(bit);
                            blob_ids.push_back(CBlob_Info(blob_id,
                                                          fBlobHasExtAnnot));
                        }
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
                if ( !with_named_accs || named_gi == ZERO_GI ) {
                    // no need to read named annot accessions
                    SetAndSaveSeq_idBlob_ids(result, seq_id, with_named_accs, 0,
                                             CFixedBlob_ids(eTakeOwnership, blob_ids));
                }
            }
        }

        cmd.reset();

        if ( with_named_accs && named_gi != ZERO_GI ) {
            // postponed read of named annot accessions
            CDB_Int giIn(named_gi);
            AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_get_annot_types"));
            cmd->SetParam("@gi", &giIn);
            cmd->Send();
            _TRACE("id_get_annot_types "<<giIn.Value());
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
                    CDB_Int giGot;
                    CDB_Int satGot;
                    CDB_Int satKeyGot;
                    CDB_Int typeGot;
                    CDB_VarChar nameGot;
                    for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                        const string& name = dbr->ItemName(pos);
                        _TRACE("next item: " << name);
                        if (name == "gi") {
                            dbr->GetItem(&giGot);
                            _TRACE("ngi: "<<giGot.Value());
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
                    /*
                    CRef<CID2S_Feat_type_Info> feat(new CID2S_Feat_type_Info);
                    feat->SetType(typeGot.Value());
                    CRef<CID2S_Seq_annot_Info> annot(new CID2S_Seq_annot_Info);
                    annot->SetName(nameGot.Value());
                    annot->SetFeat().push_back(feat);
                    list<CRef<CID2S_Seq_annot_Info> > annot_info(1, annot);
                    SetAndSaveBlobAnnotInfo(result, blob_id, annot_info);
                    */
                }
            }
            SetAndSaveSeq_idBlob_ids(result, seq_id, with_named_accs, 0,
                                     CFixedBlob_ids(eTakeOwnership, blob_ids));
        }

        if ( result_count == 0 ) {
            SetAndSaveNoSeq_idSeq_ids(result, seq_id, 0);
            SetAndSaveNoSeq_idBlob_ids(result, seq_id, with_named_accs, 0);
        }
    }}

    conn.Release();
    return result_count > 0;
}


bool CPubseqReader::LoadGiSeq_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids load_lock(result, seq_id);
    if ( load_lock.IsLoaded() ) {
        return true;
    }
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    TGi gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == ZERO_GI ) {
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, load_lock, 0);
        return true;
    }

    _TRACE("ResolveGi to Seq-ids: " << gi);

    CConn conn(result, this);
    {{
        TSeqIds ids;
        CDB_Connection* db_conn = x_GetConnection(conn);
    
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_seqid4gi"));
        CDB_Int giIn = GI_TO(Int4, gi);
        CDB_TinyInt binIn = 1;
        cmd->SetParam("@gi", &giIn);
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
                dbr->Fetch();
                CDB_Int v;
                dbr->GetItem(&v);
                int status = v.Value();
                if ( status == 100 ) {
                    // gi does not exist
                    not_found = true;
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
        SetAndSaveSeq_idSeq_ids(result, seq_id, load_lock, state,
                                CFixedSeq_ids(eTakeOwnership, ids));
    }}
    conn.Release();
    return true;
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
            pair<AutoPtr<CDB_Result>, int> dbr
                (x_ReceiveData(result, blob_id, *cmd, false));
            if ( dbr.first ) {
                ERR_POST_X(5, "CPubseqReader: unexpected blob data");
            }
        }}
        conn.Release();
        if ( !blob_id.IsMainBlob() ) {
            CLoadLockBlob blob(result, blob_id);
            if ( !blob.IsSetBlobVersion() ) {
                SetAndSaveBlobVersion(result, blob_id, 0);
            }
        }
    }
    catch ( exception& ) {
        if ( !blob_id.IsMainBlob() ) {
            SetAndSaveBlobVersion(result, blob_id, 0);
            return;
        }
        throw;
    }
}


void CPubseqReader::GetBlob(CReaderRequestResult& result,
                            const TBlobId& blob_id,
                            TChunkId chunk_id)
{
    CConn conn(result, this);
    if ( chunk_id == CProcessor::kMain_ChunkId ) {
        CLoadLockBlob blob(result, blob_id);
        if ( blob.IsLoaded() ) {
            conn.Release();
            return;
        }
    }
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<I_BaseCmd> cmd(x_SendRequest(blob_id, db_conn, RPC_GET_ASN));
        pair<AutoPtr<CDB_Result>, int> dbr
            (x_ReceiveData(result, blob_id, *cmd, true));
        if ( dbr.first ) {
            CDB_Result_Reader reader(dbr.first);
            CRStream stream(&reader);
            CProcessor::EType processor_type;
            if ( blob_id.GetSubSat() == eSubSat_SNP ) {
                processor_type = CProcessor::eType_Seq_entry_SNP;
            }
            else {
                processor_type = CProcessor::eType_Seq_entry;
            }
            if ( dbr.second & fZipType_gzipped ) {
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
            SetAndSaveNoBlob(result, blob_id, chunk_id);
        }
    }}
    conn.Release();
}


I_BaseCmd* CPubseqReader::x_SendRequest(const CBlob_id& blob_id,
                                        CDB_Connection* db_conn,
                                        const char* rpc)
{
    AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC(rpc));
    CDB_SmallInt satIn(blob_id.GetSat());
    CDB_Int satKeyIn(blob_id.GetSatKey());
    CDB_Int ext_feat(blob_id.GetSubSat());

    _TRACE("x_SendRequest: "<<blob_id.ToString());

    cmd->SetParam("@sat_key", &satKeyIn);
    cmd->SetParam("@sat", &satIn);
    cmd->SetParam("@ext_feat", &ext_feat);
    cmd->Send();
    return cmd.release();
}


pair<AutoPtr<CDB_Result>, int>
CPubseqReader::x_ReceiveData(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             I_BaseCmd& cmd,
                             bool force_blob)
{
    pair<AutoPtr<CDB_Result>, int> ret;

    enum {
        kState_dead = 125
    };
    TBlobState blob_state = 0;

    CLoadLockBlob blob(result, blob_id);

    // new row
    while( !ret.first && cmd.HasMoreResults() ) {
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
        
        while ( !ret.first && dbr->Fetch() ) {
            _TRACE("next fetch: " << dbr->NofItems() << " items");
            for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                const string& name = dbr->ItemName(pos);
                _TRACE("next item: " << name);
                if ( name == "confidential" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("confidential: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |= CBioseq_Handle::fState_confidential;
                    }
                }
                else if ( name == "suppress" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("suppress: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |= (v.Value() & 4)
                            ? CBioseq_Handle::fState_suppress_temp
                            : CBioseq_Handle::fState_suppress_perm;
                    }
                }
                else if ( name == "override" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("withdrawn: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |= CBioseq_Handle::fState_withdrawn;
                    }
                }
                else if ( name == "last_touched_m" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("version: " << v.Value());
                    m_Dispatcher->SetAndSaveBlobVersion(result, blob_id,
                                                        v.Value());
                }
                else if ( name == "state" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("state: "<<v.Value());
                    if ( v.Value() == kState_dead ) {
                        blob_state |= CBioseq_Handle::fState_dead;
                    }
                }
                else if ( name == "zip_type" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("zip_type: "<<v.Value());
                    ret.second = v.Value();
                }
                else if ( name == "asn1" ) {
                    ret.first.reset(dbr.release());
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
    if ( !ret.first && force_blob ) {
        // no data
        _TRACE("actually no data");
        blob_state |= CBioseq_Handle::fState_no_data;
    }
    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    return ret;
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
