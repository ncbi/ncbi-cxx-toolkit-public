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
#include <corelib/ncbi_config_value.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq_entry.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <corelib/ncbicntr.hpp>
#include <corelib/plugin_manager_impl.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <util/rwstream.hpp>
#include <corelib/plugin_manager_store.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif

#ifdef _DEBUG
static int GetDebugLevel(void)
{
    static int s_Value = -1;
    int value = s_Value;
    if ( value < 0 ) {
        value = GetConfigInt("GENBANK", "PUBSEQOS_DEBUG");
        if ( value < 0 ) {
            value = 0;
        }
        s_Value = value;
    }
    return value;
}
#else
# define GetDebugLevel() (0)
#endif

#define RPC_GET_ASN         "id_get_asn"
#define RPC_GET_BLOB_INFO   "id_get_blob_prop"

CPubseqReader::CPubseqReader(int max_connections,
                             const string& server,
                             const string& user,
                             const string& pswd)
    : m_Server(server) , m_User(user), m_Password(pswd), m_Context(0)
{
#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif
    SetMaximumConnections(max_connections);
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
    return 1;
#else
    return 1;
#endif
}


void CPubseqReader::x_Connect(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CPubseqReader::x_Disconnect(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CPubseqReader::x_Reconnect(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    ERR_POST("CPubseqReader: PubSeqOS GenBank connection failed: "
             "reconnecting...");
    m_Connections[conn].reset();
}


CDB_Connection* CPubseqReader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CDB_Connection>& stream = m_Connections[conn];
    if ( !stream.get() ) {
        stream.reset(x_NewConnection());
    }
    return stream.get();
}


CDB_Connection* CPubseqReader::x_NewConnection(void)
{
    if ( !m_Context ) {
        C_DriverMgr drvMgr;
        //DBAPI_RegisterDriver_CTLIB(drvMgr);
        //DBAPI_RegisterDriver_DBLIB(drvMgr);
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        string errmsg;
        m_Context = drvMgr.GetDriverContext("ctlib", &errmsg, &args);
        if ( !m_Context ) {
            LOG_POST(errmsg);
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Cannot create dbapi context");
#else
            m_Context = drvMgr.GetDriverContext("dblib", &errmsg, &args);
            if ( !m_Context ) {
                LOG_POST(errmsg);
                NCBI_THROW(CLoaderException, eNoConnection,
                           "Cannot create dbapi context");
            }
#endif
        }
    }

    AutoPtr<CDB_Connection> conn
        (m_Context->Connect(m_Server, m_User, m_Password, 0));
    
    if ( !conn.get() ) {
        NCBI_THROW(CLoaderException, eNoConnection, "connection failed");
    }

    {{
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set blob_stream on"));
        if ( cmd.get() ) {
            cmd->Send();
        }
    }}
    
    return conn.release();
}


bool CPubseqReader::LoadSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() || ids.IsLoaded() ) {
        return true;
    }

    // Get gi by seq-id
    _TRACE("ResolveSeq_id to gi: " << seq_id.AsString());

    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        {{
            CObjectOStreamAsn ooss(oss);
            ooss << *seq_id.GetSeqId();
        }}
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    CDB_Int giOut;
    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);

        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn", 1));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();
    
        while(cmd->HasMoreResults()) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get()  ||  dbr->ResultType() != eDB_RowResult) {
                continue;
            }
        
            while ( dbr->Fetch() ) {
                _TRACE("next fetch: " << dbr->NofItems() << " items");
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if (name == "gi") {
                        dbr->GetItem(&giOut);
                    }
                    else {
                        dbr->SkipItem();
                    }
                }
            }
        }
    }}
    conn.Release();

    int gi = 0;
    if ( !giOut.IsNULL() ) {
        gi = giOut.Value();
    }

    SetAndSaveSeq_idGi(result, seq_id, ids, gi);
    return true;
}


void CPubseqReader::GetSeq_idSeq_ids(CReaderRequestResult& result,
                                     CLoadLockSeq_ids& ids,
                                     const CSeq_id_Handle& seq_id)
{
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        GetGiSeq_ids(result, seq_id, ids);
        return;
    }

    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    int gi = ids->GetGi();
    if ( !gi ) {
        // no gi -> no Seq-ids
        return;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);
    
    // copy Seq-id list from gi to original seq-id
    CLoadLockSeq_ids gi_ids(result, gi_handle);
    ids->m_Seq_ids = gi_ids->m_Seq_ids;
    ids->SetState(gi_ids->GetState());
}


namespace {
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
    

    class CDB_Result_Reader : public IReader
    {
    public:
        CDB_Result_Reader(CDB_Result* db_result)
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
                    if ( !m_DB_Result->Fetch() )
                        break;
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
        CDB_Result* m_DB_Result;
    };
}


void CPubseqReader::GetGiSeq_ids(CReaderRequestResult& /*result*/,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& ids)
{
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    int gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == 0 ) {
        return;
    }

    _TRACE("ResolveGi to Seq-ids: " << gi);

    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
    
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_seqid4gi", 2));
        CDB_Int giIn = gi;
        CDB_TinyInt binIn = 1;
        cmd->SetParam("@gi", &giIn);
        cmd->SetParam("@bin", &binIn);
        cmd->Send();
    
        while ( cmd->HasMoreResults() ) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get() || dbr->ResultType() != eDB_RowResult) {
                continue;
            }
        
            while ( dbr->Fetch() ) {
                _TRACE("next fetch: " << dbr->NofItems() << " items");
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if ( name == "seqid" ) {
                        CDB_Result_Reader reader(dbr.get());
                        CRStream stream(&reader);
                        CObjectIStreamAsnBinary in(stream);
                        CSeq_id id;
                        while ( in.HaveMoreData() ) {
                            in >> id;
                            ids.AddSeq_id(id);
                        }
                    }
                    else {
                        dbr->SkipItem();
                    }
                }
            }
        }
    }}
    conn.Release();
}


void CPubseqReader::GetSeq_idBlob_ids(CReaderRequestResult& /*result*/,
                                      CLoadLockBlob_ids& ids,
                                      const CSeq_id_Handle& seq_id)
{
    _TRACE("ResolveBlob_ids: " << seq_id.AsString());
    // note: this was
    //CDB_VarChar asnIn(static_cast<string>(CNcbiOstrstreamToString(oss)));
    // but MSVC doesn't like this.  This is the only version that
    // will compile:
    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        {{
            CObjectOStreamAsn ooss(oss);
            ooss << *seq_id.GetSeqId();
        }}
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    
    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn", 1));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();
        
        while ( cmd->HasMoreResults() ) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get()  ||  dbr->ResultType() != eDB_RowResult) {
                continue;
            }
            
            while ( dbr->Fetch() ) {
                CDB_Int giGot;
                CDB_Int satGot;
                CDB_Int satKeyGot;
                CDB_Int extFeatGot;
            
                _TRACE("next fetch: " << dbr->NofItems() << " items");
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
                    else {
                        dbr->SkipItem();
                    }
                }
                
                int gi = giGot.Value();
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
                
                if ( CProcessor::TrySNPSplit() && sat != eSat_ANNOT ) {
                    {{
                        // main blob
                        CBlob_id blob_id;
                        blob_id.SetSat(sat);
                        blob_id.SetSatKey(sat_key);
                        ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                    }}
                    if ( !extFeatGot.IsNULL() ) {
                        int ext_feat = extFeatGot.Value();
                        while ( ext_feat ) {
                            int bit = ext_feat & ~(ext_feat-1);
                            ext_feat -= bit;
#ifdef GENBANK_USE_SNP_SATELLITE_15
                            if ( bit == eSubSat_SNP ) {
                                AddSNPBlob_id(ids, gi);
                                continue;
                            }
#endif
                            CBlob_id blob_id;
                            blob_id.SetSat(eSat_ANNOT);
                            blob_id.SetSatKey(gi);
                            blob_id.SetSubSat(bit);
                            ids.AddBlob_id(blob_id, fBlobHasExtAnnot);
                        }
                    }
                }
                else {
                    // whole blob
                    CBlob_id blob_id;
                    blob_id.SetSat(sat);
                    blob_id.SetSatKey(sat_key);
                    if ( !extFeatGot.IsNULL() ) {
                        blob_id.SetSubSat(extFeatGot.Value());
                    }
                    ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                }
            }
        }
    }}
    conn.Release();
}


void CPubseqReader::GetBlobVersion(CReaderRequestResult& result, 
                                   const CBlob_id& blob_id)
{
    try {
        CConn conn(this);
        {{
            CDB_Connection* db_conn = x_GetConnection(conn);
            AutoPtr<I_BaseCmd> cmd
                (x_SendRequest2(blob_id, db_conn, RPC_GET_BLOB_INFO));
            AutoPtr<CDB_Result> dbr
                (x_ReceiveData(result, blob_id, *cmd, false));
        }}
        conn.Release();
        if ( !blob_id.IsMainBlob() ) {
            CLoadLockBlob blob(result, blob_id);
            if ( !blob.IsSetBlobVersion() ) {
                SetAndSaveBlobVersion(result, blob_id, 0);
            }
        }
    }
    catch ( ... ) {
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
    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<I_BaseCmd> cmd(x_SendRequest(blob_id, db_conn, RPC_GET_ASN));
        AutoPtr<CDB_Result> dbr(x_ReceiveData(result, blob_id, *cmd, true));
        if ( dbr.get() ) {
            CDB_Result_Reader reader(dbr.get());
            CRStream stream(&reader);
            CProcessor::EType processor_type;
            if ( blob_id.GetSubSat() == eSubSat_SNP ) {
                processor_type = CProcessor::eType_Seq_entry_SNP;
            }
            else {
                processor_type = CProcessor::eType_Seq_entry;
            }
            m_Dispatcher->GetProcessor(processor_type)
                .ProcessStream(result, blob_id, chunk_id, stream);
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
    AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC(rpc, 3));
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


CDB_Result* CPubseqReader::x_ReceiveData(CReaderRequestResult& result,
                                         const TBlobId& blob_id,
                                         I_BaseCmd& cmd,
                                         bool force_blob)
{
    AutoPtr<CDB_Result> ret;

    CLoadLockBlob blob(result, blob_id);

    enum {
        kState_dead = 125
    };

    TBlobState blob_state = 0;
    // new row
    while( !ret.get() && cmd.HasMoreResults() ) {
        _TRACE("next result");
        if ( cmd.HasFailed() ) {
            break;
        }
        
        AutoPtr<CDB_Result> dbr(cmd.Result());
        if ( !dbr.get() || dbr->ResultType() != eDB_RowResult ) {
            continue;
        }
        
        while ( !ret.get() && dbr->Fetch() ) {
            _TRACE("next fetch: " << dbr->NofItems() << " items");
            for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                const string& name = dbr->ItemName(pos);
                _TRACE("next item: " << name);
                if ( name == "confidential" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("confidential: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |=
                            CBioseq_Handle::fState_confidential |
                            CBioseq_Handle::fState_no_data;
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
                        blob_state |=
                            CBioseq_Handle::fState_withdrawn |
                            CBioseq_Handle::fState_no_data;
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
                else if ( name == "asn1" ) {
                    ret.reset(dbr.release());
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
    if ( !ret.get() && force_blob ) {
        // no data
        blob_state |= CBioseq_Handle::fState_no_data;
    }
    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    return ret.release();
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
