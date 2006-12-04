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
 *
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Data reader from ID2
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_system.hpp> // for SleepSec

#include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2_entry.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#include <objtools/data_loaders/genbank/request_result.hpp>

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

#include <serial/iterator.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <iomanip>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define DEFAULT_SERVICE  "ID2"
#define DEFAULT_NUM_CONN 3
#define DEFAULT_TIMEOUT  20
#define MAX_MT_CONN      5


//#define GENBANK_ID2_RANDOM_FAILS 1
#define GENBANK_ID2_RANDOM_FAILS_FREQUENCY 20
#define GENBANK_ID2_RANDOM_FAILS_RECOVER 10 // new + write + read

namespace {
    class CDebugPrinter : public CNcbiOstrstream
    {
    public:
        CDebugPrinter(CId2Reader::TConn conn)
            {
                flush() << "CId2Reader(" << conn << "): ";
#ifdef NCBI_THREADS
                flush() << "T" << CThread::GetSelf() << ' ';
#endif
            }
        ~CDebugPrinter()
            {
                LOG_POST(rdbuf());
                /*
                DEFINE_STATIC_FAST_MUTEX(sx_DebugPrinterMutex);
                CFastMutexGuard guard(sx_DebugPrinterMutex);
                (NcbiCout << rdbuf()).flush();
                */
            }
    };
}


#ifdef GENBANK_ID2_RANDOM_FAILS
static int& GetFailCounter(CId2Reader::TConn /*conn*/)
{
#ifdef NCBI_THREADS
    static CSafeStaticRef< CTls<int> > fail_counter;
    int* ptr = fail_counter->GetValue();
    if ( !ptr ) {
        ptr = new int(0);
        fail_counter->SetValue(ptr);
    }
    return *ptr;
#else
    static int fail_counter = 0;
    return fail_counter;
#endif
}

static void SetRandomFail(CNcbiIostream& stream, CId2Reader::TConn conn)
{
    int& value = GetFailCounter(conn);
    /*
    {{
        CDebugPrinter s(conn);
        s << "SetRandomFail: " << value;
    }}
    */
    if ( ++value <= GENBANK_ID2_RANDOM_FAILS_RECOVER ) {
        return;
    }
    if ( random() % GENBANK_ID2_RANDOM_FAILS_FREQUENCY == 0 ) {
        value = 0;
        stream.setstate(ios::badbit);
    }
}
#else
# define SetRandomFail(stream, conn) do{}while(0)
#endif


NCBI_PARAM_DECL(int, GENBANK, ID2_DEBUG);
NCBI_PARAM_DECL(int, GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE);
NCBI_PARAM_DECL(string, GENBANK, ID2_CGI_NAME);
NCBI_PARAM_DECL(string, GENBANK, ID2_SERVICE_NAME);
NCBI_PARAM_DECL(string, NCBI, SERVICE_NAME_ID2);

NCBI_PARAM_DEF_EX(int, GENBANK, ID2_DEBUG, 0,
                  eParam_NoThread, GENBANK_ID2_DEBUG);
NCBI_PARAM_DEF_EX(int, GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE, 20,
                  eParam_NoThread, GENBANK_ID2_MAX_CHUNKS_REQUEST_SIZE);
NCBI_PARAM_DEF_EX(string, GENBANK, ID2_CGI_NAME, kEmptyStr,
                  eParam_NoThread, GENBANK_ID2_CGI_NAME);
NCBI_PARAM_DEF_EX(string, GENBANK, ID2_SERVICE_NAME, kEmptyStr,
                  eParam_NoThread, GENBANK_ID2_SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, NCBI, SERVICE_NAME_ID2, DEFAULT_SERVICE,
                  eParam_NoThread, GENBANK_SERVICE_NAME_ID2);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(GENBANK, ID2_DEBUG) s_Value;
    return s_Value.Get();
}


enum EDebugLevel
{
    eTraceConn     = 4,
    eTraceASN      = 5,
    eTraceBlob     = 8,
    eTraceBlobData = 9
};


// Number of chunks allowed in a single request
// 0 = unlimited request size
// 1 = do not use packets or get-chunks requests
static size_t GetMaxChunksRequestSize(void)
{
    static NCBI_PARAM_TYPE(GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE) s_Value;
    return s_Value.Get();
}


static inline
bool
SeparateChunksRequests(size_t max_request_size = GetMaxChunksRequestSize())
{
    return max_request_size == 1;
}


static inline
bool
LimitChunksRequests(size_t max_request_size = GetMaxChunksRequestSize())
{
    return max_request_size > 0;
}


struct SId2LoadedSet
{
    typedef set<string> TStringSet;
    typedef set<CSeq_id_Handle> TSeq_idSet;
    typedef map<CBlob_id, CId2Reader::TContentsMask> TBlob_ids;
    typedef pair<int, TBlob_ids> TBlob_idsInfo;
    typedef map<CSeq_id_Handle, TBlob_idsInfo> TBlob_idSet;
    typedef map<CBlob_id, CConstRef<CID2_Reply_Data> > TSkeletons;

    TStringSet  m_Seq_idsByString;
    TSeq_idSet  m_Seq_ids;
    TBlob_idSet m_Blob_ids;
    TSkeletons  m_Skeletons;
};


CId2Reader::CId2Reader(int max_connections)
    : m_ServiceName(DEFAULT_SERVICE),
      m_Timeout(DEFAULT_TIMEOUT),
      m_AvoidRequest(0)
{
    m_RequestSerialNumber.Set(1);
    if ( max_connections == 0 ) {
        max_connections = DEFAULT_NUM_CONN;
    }
    SetMaximumConnections(max_connections);
}


CId2Reader::CId2Reader(const TPluginManagerParamTree* params,
                       const string& driver_name)
    : m_AvoidRequest(0)
{
    m_RequestSerialNumber.Set(1);
    CConfig conf(params);
    m_ServiceName = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_ID2_PARAM_SERVICE_NAME,
        CConfig::eErr_NoThrow,
        kEmptyStr);
    if ( m_ServiceName.empty() ) {
        m_ServiceName =
            NCBI_PARAM_TYPE(GENBANK, ID2_CGI_NAME)::GetDefault();
    }
    if ( m_ServiceName.empty() ) {
        m_ServiceName =
            NCBI_PARAM_TYPE(GENBANK, ID2_SERVICE_NAME)::GetDefault();
    }
    if ( m_ServiceName.empty() ) {
        m_ServiceName =
            NCBI_PARAM_TYPE(NCBI, SERVICE_NAME_ID2)::GetDefault();
    }
    m_Timeout = conf.GetInt(
        driver_name,
        NCBI_GBLOADER_READER_ID2_PARAM_TIMEOUT,
        CConfig::eErr_NoThrow,
        DEFAULT_TIMEOUT);
    TConn max_connections = conf.GetInt(
        driver_name,
        NCBI_GBLOADER_READER_ID2_PARAM_NUM_CONN,
        CConfig::eErr_NoThrow,
        DEFAULT_NUM_CONN);
    SetMaximumConnections(max_connections);
    bool open_initial_connection = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_ID2_PARAM_PREOPEN,
        CConfig::eErr_NoThrow,
        true);
    SetPreopenConnection(open_initial_connection);
}


CId2Reader::~CId2Reader()
{
}


int CId2Reader::GetMaximumConnectionsLimit(void) const
{
#ifdef NCBI_THREADS
    return MAX_MT_CONN;
#else
    return 1;
#endif
}


void CId2Reader::x_AddConnectionSlot(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CId2Reader::x_RemoveConnectionSlot(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CId2Reader::x_DisconnectAtSlot(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CConn_IOStream>& stream = m_Connections[conn];
    if ( stream ) {
        LOG_POST(Warning << "CId2Reader("<<conn<<"): "
                 "ID2 connection failed: reconnecting...");
        stream.reset();
    }
}


void CId2Reader::x_ConnectAtSlot(TConn conn)
{
    x_GetConnection(conn);
}


CConn_IOStream* CId2Reader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CConn_IOStream>& stream = m_Connections[conn];
    if ( !stream.get() ) {
        stream.reset(x_NewConnection(conn));
    }
    return stream.get();
}


CConn_IOStream* CId2Reader::x_NewConnection(TConn conn)
{
    WaitBeforeNewConnection(conn);
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s(conn);
        s << "New connection to " << m_ServiceName << "...";
    }
        
    STimeout tmout;
    tmout.sec = m_Timeout;
    tmout.usec = 0;

    AutoPtr<CConn_IOStream> stream;
    if ( NStr::StartsWith(m_ServiceName, "http://") ) {
        stream.reset
            (new CConn_HttpStream(m_ServiceName));
    }
    else {
        stream.reset
            (new CConn_ServiceStream(m_ServiceName, fSERV_Any, 0, 0, &tmout));
    }
    SetRandomFail(*stream, conn);
    if ( stream->bad() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed,
                   "initialization of connection failed");
    }

    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s(conn);
        s << "New connection to " << m_ServiceName << " opened.";
        // need to call CONN_Wait to force connection to open
        CONN_Wait(stream->GetCONN(), eIO_Write, &tmout);
        const char* descr = CONN_Description(stream->GetCONN());
        if ( descr ) {
            s << "  description: " << descr;
        }
    }
    try {
        x_InitConnection(*stream, conn);
    }
    catch ( CException& exc ) {
        NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                     "initialization of connection failed");
    }
    SetRandomFail(*stream, conn);
    if ( stream->bad() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed,
                   "initialization of connection failed");
    }

    RequestSucceeds(conn);
    return stream.release();
}


#define MConnFormat MSerial_AsnBinary


void CId2Reader::x_InitConnection(CNcbiIostream& stream, TConn conn)
{
    // prepare init request
    CID2_Request req;
    req.SetRequest().SetInit();
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));
    // that's it for now
    // TODO: add params

    // send init request
    {{
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Sending";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << packet;
            }
            else {
                s << " ID2-Request-Packet";
            }
            s << "...";
        }
        stream << MConnFormat << packet << flush;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Sent ID2-Request-Packet.";
        }
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eConnectionFailed,
                       "stream is bad after sending");
        }
    }}
    
    // receive init reply
    CID2_Reply reply;
    {{
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Receiving ID2-Reply...";
        }
        stream >> MConnFormat >> reply;
        if ( GetDebugLevel() >= eTraceConn   ) {
            CDebugPrinter s(conn);
            s << "Received";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << reply;
            }
            else {
                s << " ID2-Reply.";
            }
        }
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "stream is bad after receiving");
        }
    }}

    // check init reply
    if ( reply.IsSetDiscard() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'discard' is set");
    }
    if ( reply.IsSetError() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'error' is set");
    }
    if ( !reply.IsSetEnd_of_reply() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'end-of-reply' is not set");
    }
    if ( reply.GetReply().Which() != CID2_Reply::TReply::e_Init ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'reply' is not 'init'");
    }
    // that's it for now
    // TODO: process params
}


void CId2Reader::x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                              const string& seq_id)
{
    get_blob_id.SetSeq_id().SetSeq_id().SetString(seq_id);
    get_blob_id.SetExternal();
}


void CId2Reader::x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                              const CSeq_id& seq_id)
{
    get_blob_id.SetSeq_id().SetSeq_id().SetSeq_id
        (const_cast<CSeq_id&>(seq_id));
    get_blob_id.SetExternal();
}


void CId2Reader::x_SetDetails(CID2_Get_Blob_Details& /*details*/,
                              TContentsMask /*mask*/)
{
}


void CId2Reader::x_SetExclude_blobs(CID2_Request_Get_Blob_Info& get_blob_info,
                                    const CSeq_id_Handle& idh,
                                    CReaderRequestResult& result)
{
    if ( SeparateChunksRequests() ) {
        // Minimize size of request rather than response
        return;
    }
    CReaderRequestResult::TLoadedBlob_ids loaded_blob_ids;
    result.GetLoadedBlob_ids(idh, loaded_blob_ids);
    if ( loaded_blob_ids.empty() ) {
        return;
    }
    CID2_Request_Get_Blob_Info::C_Blob_id::C_Resolve::TExclude_blobs&
        exclude_blobs =
        get_blob_info.SetBlob_id().SetResolve().SetExclude_blobs();
    ITERATE(CReaderRequestResult::TLoadedBlob_ids, id, loaded_blob_ids) {
        CRef<CID2_Blob_Id> blob_id(new CID2_Blob_Id);
        x_SetResolve(*blob_id, *id);
        exclude_blobs.push_back(blob_id);
    }
}


CId2Reader::TBlobId CId2Reader::GetBlobId(const CID2_Blob_Id& blob_id)
{
    CBlob_id ret;
    ret.SetSat(blob_id.GetSat());
    ret.SetSubSat(blob_id.GetSub_sat());
    ret.SetSatKey(blob_id.GetSat_key());
    //ret.SetVersion(blob_id.GetVersion());
    return ret;
}


void CId2Reader::x_SetResolve(CID2_Blob_Id& blob_id, const CBlob_id& src)
{
    blob_id.SetSat(src.GetSat());
    blob_id.SetSub_sat(src.GetSubSat());
    blob_id.SetSat_key(src.GetSatKey());
    //blob_id.SetVersion(src.GetVersion());
}


bool CId2Reader::LoadStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    x_SetResolve(req.SetRequest().SetGet_blob_id(), seq_id);
    x_ProcessRequest(result, req);
    return true;
}


bool CId2Reader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
    x_ProcessRequest(result, req);
    return true;
}


bool CId2Reader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    x_SetResolve(req.SetRequest().SetGet_blob_id(), *seq_id.GetSeqId());
    x_ProcessRequest(result, req);
    return true;
}


bool CId2Reader::LoadBlobVersion(CReaderRequestResult& result,
                                 const CBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsSetBlobVersion() ) {
        return true;
    }

    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    x_ProcessRequest(result, req);
    return true;
}


bool CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           const string& seq_id,
                           TContentsMask /*mask*/)
{
    if ( m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
        return LoadStringSeq_ids(result, seq_id);
    }
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    return LoadStringSeq_ids(result, seq_id);
    /*
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(), seq_id);
    //x_SetDetails(req2.SetGet_data(), mask);
    x_ProcessRequest(result, req);
    */
}


bool CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id,
                           TContentsMask mask)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        if ( (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) ||
             !(mask & fBlobHasAllLocal) ) {
            if ( !LoadSeq_idBlob_ids(result, seq_id) ) {
                return false;
            }
        }
    }
    if ( ids.IsLoaded() ) {
        // shortcut - we know Seq-id -> Blob-id resolution
        return LoadBlobs(result, ids, mask);
    }
    else {
        CID2_Request req;
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                     *seq_id.GetSeqId());
        x_SetDetails(req2.SetGet_data(), mask);
        x_SetExclude_blobs(req2, seq_id, result);
        x_ProcessRequest(result, req);
        return true;
    }
}


bool CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids blobs,
                           TContentsMask mask)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( LimitChunksRequests(max_request_size) ) {
        return CReader::LoadBlobs(result, blobs, mask);
    }

    CID2_Request_Packet packet;
    vector<CLoadLockBlob> blob_locks;
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( (info.GetContentsMask() & mask) == 0 ) {
            continue; // skip this blob
        }
        CConstRef<CBlob_id> blob_id = it->first;
        const TChunkId chunk_id = CProcessor::kMain_ChunkId;
        CLoadLockBlob blob(result, *blob_id);
        if ( CProcessor::IsLoaded(*blob_id, chunk_id, blob) ) {
            continue;
        }
        
        if ( CProcessor_ExtAnnot::IsExtAnnot(*blob_id) ) {
            dynamic_cast<const CProcessor_ExtAnnot&>
                (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
                .Process(result, *blob_id, chunk_id);
            _ASSERT(CProcessor::IsLoaded(*blob_id, chunk_id, blob));
            continue;
        }

        blob_locks.push_back(blob);
        CRef<CID2_Request> req(new CID2_Request);
        packet.Set().push_back(req);
        CID2_Request_Get_Blob_Info& req2 =
            req->SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), *blob_id);
        x_SetDetails(req2.SetGet_data(), mask);
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet);
    }
    return true;
}


bool CId2Reader::LoadBlob(CReaderRequestResult& result,
                          const TBlobId& blob_id)
{
    TChunkId chunk_id = CProcessor::kMain_ChunkId;
    CLoadLockBlob blob(result, blob_id);
    if ( CProcessor::IsLoaded(blob_id, chunk_id, blob) ) {
        return true;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        dynamic_cast<const CProcessor_ExtAnnot&>
            (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
            .Process(result, blob_id, chunk_id);
        _ASSERT(CProcessor::IsLoaded(blob_id, chunk_id, blob));
        return true;
    }

    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    req2.SetGet_data();
    x_ProcessRequest(result, req);
    return true;
}


bool CId2Reader::LoadChunk(CReaderRequestResult& result,
                           const CBlob_id& blob_id,
                           TChunkId chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);
    CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(chunk_id);
    if ( chunk_info.IsLoaded() ) {
        return true;
    }
    CInitGuard init(chunk_info, result);
    if ( !init ) {
        _ASSERT(chunk_info.IsLoaded());
        return true;
    }

    CID2_Request req;
    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id, chunk_id) ) {
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
        req2.SetGet_data();
        x_ProcessRequest(result, req);
        if ( !chunk_info.IsLoaded() ) {
            ERR_POST("ExtAnnot chunk is not loaded: "<<blob_id.ToString());
            chunk_info.SetLoaded();
        }
    }
    else {
        CID2S_Request_Get_Chunks& req2 = req.SetRequest().SetGet_chunks();
        x_SetResolve(req2.SetBlob_id(), blob_id);

        if ( blob->GetBlobVersion() > 0 ) {
            req2.SetBlob_id().SetVersion(blob->GetBlobVersion());
        }
        //req2.SetSplit_version(blob->GetSplitInfo().GetSplitVersion());
        req2.SetChunks().push_back(CID2S_Chunk_Id(chunk_id));
        x_ProcessRequest(result, req);
    }
    //_ASSERT(chunk_info.IsLoaded());
    return true;
}


void LoadedChunksPacket(CID2_Request_Packet& packet,
                        vector<CTSE_Chunk_Info*>& chunks,
                        const CBlob_id& blob_id,
                        vector< AutoPtr<CInitGuard> >& guards)
{
    NON_CONST_ITERATE(vector<CTSE_Chunk_Info*>, it, chunks) {
        if ( !(*it)->IsLoaded() ) {
            ERR_POST("ExtAnnot chunk is not loaded: " << blob_id.ToString());
            (*it)->SetLoaded();
        }
    }
    packet.Set().clear();
    chunks.clear();
    guards.clear();
}


bool CId2Reader::LoadChunks(CReaderRequestResult& result,
                            const CBlob_id& blob_id,
                            const TChunkIds& chunk_ids)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( SeparateChunksRequests(max_request_size) ) {
        return CReader::LoadChunks(result, blob_id, chunk_ids);
    }
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);

    CID2_Request_Packet packet;

    CRef<CID2_Request> chunks_req(new CID2_Request);
    CID2S_Request_Get_Chunks& get_chunks =
        chunks_req->SetRequest().SetGet_chunks();

    x_SetResolve(get_chunks.SetBlob_id(), blob_id);
    if ( blob->GetBlobVersion() > 0 ) {
        get_chunks.SetBlob_id().SetVersion(blob->GetBlobVersion());
    }
    CID2S_Request_Get_Chunks::TChunks& chunks = get_chunks.SetChunks();

    vector< AutoPtr<CInitGuard> > guards;
    vector< AutoPtr<CInitGuard> > ext_guards;
    vector<CTSE_Chunk_Info*> ext_chunks;
    ITERATE(TChunkIds, id, chunk_ids) {
        CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(*id);
        if ( chunk_info.IsLoaded() ) {
            continue;
        }
        if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id, *id) ) {
            AutoPtr<CInitGuard> init(new CInitGuard(chunk_info, result));
            if ( !init ) {
                _ASSERT(chunk_info.IsLoaded());
                continue;
            }
            ext_guards.push_back(init);
            CRef<CID2_Request> ext_req(new CID2_Request);
            CID2_Request_Get_Blob_Info& ext_req_data =
                ext_req->SetRequest().SetGet_blob_info();
            x_SetResolve(ext_req_data.SetBlob_id().SetBlob_id(), blob_id);
            ext_req_data.SetGet_data();
            packet.Set().push_back(ext_req);
            ext_chunks.push_back(&chunk_info);
            if ( LimitChunksRequests(max_request_size) &&
                 packet.Get().size() >= max_request_size ) {
                // Request collected chunks
                x_ProcessPacket(result, packet);
                LoadedChunksPacket(packet, ext_chunks, blob_id, ext_guards);
            }
        }
        else {
            AutoPtr<CInitGuard> init(new CInitGuard(chunk_info, result));
            if ( !init ) {
                _ASSERT(chunk_info.IsLoaded());
                continue;
            }
            guards.push_back(init);
            chunks.push_back(CID2S_Chunk_Id(*id));
            if ( LimitChunksRequests(max_request_size) &&
                 chunks.size() >= max_request_size ) {
                // Process collected chunks
                x_ProcessRequest(result, *chunks_req);
                guards.clear();
                chunks.clear();
            }
        }
    }
    if ( !chunks.empty() ) {
        if ( LimitChunksRequests(max_request_size) &&
             packet.Get().size() + chunks.size() > max_request_size ) {
            // process chunks separately from packet
            x_ProcessRequest(result, *chunks_req);
        }
        else {
            // Use the same packet for chunks
            packet.Set().push_back(chunks_req);
        }
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet);
        LoadedChunksPacket(packet, ext_chunks, blob_id, ext_guards);
    }
    return true;
}


bool CId2Reader::x_LoadSeq_idBlob_idsSet(CReaderRequestResult& result,
                                         const TSeqIds& seq_ids)
{
    CID2_Request_Packet packet;
    ITERATE(TSeqIds, id, seq_ids) {
        CLoadLockBlob_ids ids(result, *id);
        if ( ids.IsLoaded() ) {
            continue;
        }

        CRef<CID2_Request> req(new CID2_Request);
        x_SetResolve(req->SetRequest().SetGet_blob_id(), *id->GetSeqId());
        packet.Set().push_back(req);
    }
    if ( packet.Get().empty() ) {
        return false;
    }
    x_ProcessPacket(result, packet);
    return true;
}


bool CId2Reader::LoadBlobSet(CReaderRequestResult& result,
                             const TSeqIds& seq_ids)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( SeparateChunksRequests(max_request_size) ) {
        return CReader::LoadBlobSet(result, seq_ids);
    }

    bool loaded_blob_ids = false;
    if (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) {
        if ( !x_LoadSeq_idBlob_idsSet(result, seq_ids) ) {
            return false;
        }
        loaded_blob_ids = true;
    }

    vector<CLoadLockBlob> blob_locks;
    CID2_Request_Packet packet;
    ITERATE(TSeqIds, id, seq_ids) {
        CLoadLockBlob_ids ids(result, *id);
        if ( ids.IsLoaded() ) {
            // shortcut - we know Seq-id -> Blob-id resolution
            ITERATE ( CLoadInfoBlob_ids, it, *ids ) {
                const CBlob_Info& info = it->second;
                if ( (info.GetContentsMask() & fBlobHasCore) == 0 ) {
                    continue; // skip this blob
                }
                CConstRef<CBlob_id> blob_id = it->first;
                CLoadLockBlob blob(result, *blob_id);
                if ( blob.IsLoaded() ) {
                    continue;
                }

                blob_locks.push_back(blob);
                CRef<CID2_Request> req(new CID2_Request);
                CID2_Request_Get_Blob_Info& req2 =
                    req->SetRequest().SetGet_blob_info();
                x_SetResolve(req2.SetBlob_id().SetBlob_id(), *blob_id);
                x_SetDetails(req2.SetGet_data(), fBlobHasCore);
                x_SetExclude_blobs(req2, *id, result);
                packet.Set().push_back(req);
                if ( LimitChunksRequests(max_request_size) &&
                     packet.Get().size() >= max_request_size ) {
                    x_ProcessPacket(result, packet);
                    packet.Set().clear();
                }
            }
        }
        else {
            CRef<CID2_Request> req(new CID2_Request);
            CID2_Request_Get_Blob_Info& req2 =
                req->SetRequest().SetGet_blob_info();
            x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                        *id->GetSeqId());
            x_SetDetails(req2.SetGet_data(), fBlobHasCore);
            x_SetExclude_blobs(req2, *id, result);
            packet.Set().push_back(req);
        }
        if ( LimitChunksRequests(max_request_size) &&
             packet.Get().size() >= max_request_size ) {
            x_ProcessPacket(result, packet);
            packet.Set().clear();
        }
    }
    if ( packet.Get().empty() ) {
        return loaded_blob_ids;
    }
    x_ProcessPacket(result, packet);
    return true;
}


void CId2Reader::x_ProcessRequest(CReaderRequestResult& result,
                                  CID2_Request& req)
{
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));
    x_ProcessPacket(result, packet);
}


void CId2Reader::x_ProcessPacket(CReaderRequestResult& result,
                                 CID2_Request_Packet& packet)
{
    // prepare serial nums and result state
    size_t request_count = packet.Get().size();
    int start_serial_num =
        m_RequestSerialNumber.Add(request_count) - request_count;
    {{
        int cur_serial_num = start_serial_num;
        NON_CONST_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
            (*it)->SetSerial_number(cur_serial_num++);
        }
    }}
    vector<char> done(request_count);
    vector<SId2LoadedSet> loaded_sets(request_count);

    CConn conn(this);
    CNcbiIostream& stream = *x_GetConnection(conn);
    // send request
    {{
        SetRandomFail(stream, conn);
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Sending";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << packet;
            }
            else {
                s << " ID2-Request-Packet";
            }
            s << "...";
        }
        stream << MConnFormat << packet << flush;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Sent ID2-Request-Packet.";
        }
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eConnectionFailed,
                       "stream is bad after sending");
        }
    }}

    // process replies
    size_t remaining_count = request_count;
    while ( remaining_count > 0 ) {
        CID2_Reply reply;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn);
            s << "Receiving ID2-Reply...";
        }
        try {
            SetRandomFail(stream, conn);
            stream >> MConnFormat >> reply;
        }
        catch ( CException& exc ) {
            stream.setstate(ios::badbit);
            NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                         "reply deserialization failed");
        }
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eConnectionFailed,
                       "stream is bad after receiving");
        }
        if ( GetDebugLevel() >= eTraceConn   ) {
            CDebugPrinter s(conn);
            s << "Received";
            if ( GetDebugLevel() >= eTraceASN ) {
                if ( GetDebugLevel() >= eTraceBlobData ) {
                    s << ": " << MSerial_AsnText << reply;
                }
                else {
                    CTypeIterator<CID2_Reply_Data> iter = Begin(reply);
                    if ( iter && iter->IsSetData() ) {
                        CID2_Reply_Data::TData save;
                        save.swap(iter->SetData());
                        size_t size = 0, count = 0, max_chunk = 0;
                        ITERATE ( CID2_Reply_Data::TData, i, save ) {
                            ++count;
                            size_t chunk = (*i)->size();
                            size += chunk;
                            max_chunk = max(max_chunk, chunk);
                        }
                        s << ": " << MSerial_AsnText << reply <<
                            "Data: " << size << " bytes in " <<
                            count << " chunks with " <<
                            max_chunk << " bytes in chunk max";
                        save.swap(iter->SetData());
                    }
                    else {
                        s << ": " << MSerial_AsnText << reply;
                    }
                }
            }
            else {
                s << " ID2-Reply.";
            }
        }
        if ( GetDebugLevel() >= eTraceBlob ) {
            for ( CTypeIterator<CID2_Reply_Data> it(Begin(reply)); it; ++it ) {
                if ( it->IsSetData() ) {
                    CProcessor_ID2::DumpDataAsText(*it, NcbiCout);
                }
            }
        }
        size_t num = reply.GetSerial_number() - start_serial_num;
        if ( reply.IsSetDiscard() ) {
            // discard whole reply for now
            continue;
        }
        if ( num >= request_count || done[num] ) {
            // unknown serial num - bad reply
            NCBI_THROW(CLoaderException, eOtherError,
                       "CId2Reader: bad reply serial number");
        }
        x_ProcessReply(result, loaded_sets[num], reply);
        if ( reply.IsSetEnd_of_reply() ) {
            done[num] = true;
            x_UpdateLoadedSet(result, loaded_sets[num]);
            --remaining_count;
        }
    }
    conn.Release();
}


void CId2Reader::x_UpdateLoadedSet(CReaderRequestResult& result,
                                   const SId2LoadedSet& loaded_set)
{
    ITERATE ( SId2LoadedSet::TStringSet, it, loaded_set.m_Seq_idsByString ) {
        SetAndSaveStringSeq_ids(result, *it);
    }
    ITERATE ( SId2LoadedSet::TSeq_idSet, it, loaded_set.m_Seq_ids ) {
        SetAndSaveSeq_idSeq_ids(result, *it);
    }
    ITERATE ( SId2LoadedSet::TBlob_idSet, it, loaded_set.m_Blob_ids ) {
        CLoadLockBlob_ids ids(result, it->first);
        if ( ids.IsLoaded() ) {
            continue;
        }
        ids->SetState(it->second.first);
        ITERATE ( SId2LoadedSet::TBlob_ids, it2, it->second.second ) {
            ids.AddBlob_id(it2->first, it2->second);
        }
        SetAndSaveSeq_idBlob_ids(result, it->first, ids);
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                const CID2_Reply& reply)
{
    TErrorFlags errors = 0;
    if ( reply.IsSetError() ) {
        ITERATE ( CID2_Reply::TError, it, reply.GetError() ) {
            errors |= x_ProcessError(result, **it);
        }
    }
    if ( errors & (fError_bad_command | fError_bad_connection) ) {
        return;
    }
    switch ( reply.GetReply().Which() ) {
    case CID2_Reply::TReply::e_Get_seq_id:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_seq_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_id:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_blob_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_seq_ids:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_blob_seq_ids());
        break;
    case CID2_Reply::TReply::e_Get_blob:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_blob());
        break;
    case CID2_Reply::TReply::e_Get_split_info:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_split_info());
        break;
    case CID2_Reply::TReply::e_Get_chunk:
        x_ProcessReply(result, loaded_set, errors,
                       reply.GetReply().GetGet_chunk());
        break;
    default:
        break;
    }
}


CId2Reader::TErrorFlags
CId2Reader::x_ProcessError(CReaderRequestResult& /*result*/,
                           const CID2_Error& error)
{
    TErrorFlags error_flags = 0;
    const char* severity = "";
    switch ( error.GetSeverity() ) {
    case CID2_Error::eSeverity_warning:
        severity = "warning: ";
        error_flags |= fError_warning;
        if ( error.IsSetMessage() ) {
            if ( NStr::FindNoCase(error.GetMessage(), "obsolete") != NPOS ) {
                error_flags |= fError_warning_dead;
            }
            if ( NStr::FindNoCase(error.GetMessage(), "removed") != NPOS ) {
                error_flags |= fError_warning_suppressed;
            }
        }
        break;
    case CID2_Error::eSeverity_failed_command:
        severity = "request error: ";
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_failed_connection:
        severity = "connection error: ";
        error_flags |= fError_bad_connection;
        //        x_Reconnect(result, error, 1);
        break;
    case CID2_Error::eSeverity_failed_server:
        severity = "server error: ";
        error_flags |= fError_bad_connection;
        //        x_Reconnect(result, error, 10);
        break;
    case CID2_Error::eSeverity_no_data:
        severity = "no data: ";
        error_flags |= fError_no_data;
        break;
    case CID2_Error::eSeverity_restricted_data:
        severity = "restricted data: ";
        error_flags |= fError_no_data;
        if ( error.IsSetMessage() &&
             (NStr::FindNoCase(error.GetMessage(), "withdrawn") != NPOS ||
              NStr::FindNoCase(error.GetMessage(), "removed") != NPOS) ) {
            error_flags |= fError_withdrawn;
        }
        else {
            error_flags |= fError_restricted;
        }
        break;
    case CID2_Error::eSeverity_unsupported_command:
        m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
        severity = "unsupported command: ";
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_invalid_arguments:
        severity = "invalid argument: ";
        error_flags |= fError_bad_command;
        break;
    }
    /*
    const char* message;
    if ( error.IsSetMessage() ) {
        message = error.GetMessage().c_str();
    }
    else {
        message = "<empty>";
    }
    if ( (error_flags & ~fError_warning) ) {
        ERR_POST(message);
    }
    else {
        //ERR_POST(Warning << message);
    }
    */
    return error_flags;
}


/*void CId2Reader::x_Reconnect(CReaderRequestResult& result,
                             const CID2_Error& error,
                             int retry_delay)
{
    if ( error.IsSetRetry_delay() && error.GetRetry_delay() >= 0 ) {
        retry_delay = error.GetRetry_delay();
    }
    x_Reconnect(result, retry_delay);
    }*/


/*void CId2Reader::x_Reconnect(CReaderRequestResult& result,
                             int retry_delay)
{
    CReaderRequestConn conn(result);
    if ( conn.GetConn() >= 0 ) {
        Reconnect(conn.GetConn());
    }
    }*/


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Seq_id& reply)
{
    // we can save this data in cache
    const CID2_Request_Get_Seq_id& request = reply.GetRequest();
    const CID2_Seq_id& request_id = request.GetSeq_id();
    switch ( request_id.Which() ) {
    case CID2_Seq_id::e_String:
        x_ProcessReply(result, loaded_set, errors,
                       request_id.GetString(),
                       reply);
        break;

    case CID2_Seq_id::e_Seq_id:
        x_ProcessReply(result, loaded_set, errors,
                       CSeq_id_Handle::GetHandle(request_id.GetSeq_id()),
                       reply);
        break;

    default:
        break;
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags errors,
                                const string& seq_id,
                                const CID2_Reply_Get_Seq_id& reply)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( errors & fError_no_data ) {
        // no Seq-ids
        int state = CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        ids->SetState(state);
        SetAndSaveStringSeq_ids(result, seq_id, ids);
        return;
    }

    switch ( reply.GetRequest().GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            ids.AddSeq_id(**it);
        }
        if ( reply.IsSetEnd_of_reply() ) {
            SetAndSaveStringSeq_ids(result, seq_id, ids);
        }
        else {
            loaded_set.m_Seq_idsByString.insert(seq_id);
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_gi:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).IsGi() ) {
                SetAndSaveStringGi(result, seq_id, ids, (**it).GetGi());
                break;
            }
        }
        break;
    }}
    default:
        // ???
        break;
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags errors,
                                const CSeq_id_Handle& seq_id,
                                const CID2_Reply_Get_Seq_id& reply)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( errors & fError_no_data ) {
        // no Seq-ids
        int state = CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        ids->SetState(state);
        SetAndSaveSeq_idSeq_ids(result, seq_id, ids);
        return;
    }
    switch ( reply.GetRequest().GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            ids.AddSeq_id(**it);
        }
        if ( reply.IsSetEnd_of_reply() ) {
            SetAndSaveSeq_idSeq_ids(result, seq_id, ids);
        }
        else {
            loaded_set.m_Seq_ids.insert(seq_id);
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_gi:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).IsGi() ) {
                SetAndSaveSeq_idGi(result, seq_id, ids, (**it).GetGi());
                break;
            }
        }
        break;
    }}
    default:
        // ???
        break;
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Blob_Id& reply)
{
    const CSeq_id& seq_id = reply.GetSeq_id();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(seq_id);
    if ( errors & fError_no_data ) {
        int state = CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        CLoadLockBlob_ids ids(result, idh);
        ids->SetState(state);
        SetAndSaveSeq_idBlob_ids(result, idh, ids);
        return;
    }
    
    SId2LoadedSet::TBlob_idsInfo& ids = loaded_set.m_Blob_ids[idh];
    if ( errors & fError_warning ) {
        ids.first |= CBioseq_Handle::fState_other_error;
    }
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    CBlob_id blob_id = GetBlobId(src_blob_id);
    TContentsMask mask = 0;
    {{ // TODO: temporary logic, this info should be returned by server
        if ( blob_id.GetSubSat() == CID2_Blob_Id::eSub_sat_main ) {
            mask |= fBlobHasAllLocal;
        }
        else {
            if ( seq_id.IsGeneral() ) {
                const CObject_id& obj_id = seq_id.GetGeneral().GetTag();
                if ( obj_id.IsId() &&
                     obj_id.GetId() == blob_id.GetSatKey() ) {
                    mask |= fBlobHasAllLocal;
                }
                else {
                    mask |= fBlobHasExtAnnot;
                }
            }
            else {
                mask |= fBlobHasExtAnnot;
            }
        }
    }}
    ids.second[blob_id] = mask;
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        SetAndSaveBlobVersion(result, blob_id, src_blob_id.GetVersion());
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& /* result */,
                                SId2LoadedSet& /*loaded_set*/,
                                TErrorFlags /* errors */,
                                const CID2_Reply_Get_Blob_Seq_ids& /* reply */)
{
/*
    if ( reply.IsSetIds() ) {
        CID2_Blob_Seq_ids ids;
        x_ReadData(reply.GetIds(), Begin(ids));
        ITERATE ( CID2_Blob_Seq_ids::Tdata, it, ids.Get() ) {
            if ( !(*it)->IsSetReplaced() ) {
                result.AddBlob_id((*it)->GetSeq_id(),
                                  GetBlobId(reply.GetBlob_id()), "");
            }
        }
    }
*/
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Blob& reply)
{
    TChunkId chunk_id = CProcessor::kMain_ChunkId;
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    TBlobId blob_id = GetBlobId(src_blob_id);
    CLoadLockBlob blob(result, blob_id);
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        SetAndSaveBlobVersion(result, blob_id, blob, src_blob_id.GetVersion());
    }
    if ( blob.IsLoaded() ) {
        if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id, blob) ) {
            chunk_id = CProcessor::kDelayedMain_ChunkId;
        }
        else {
            m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
            ERR_POST(Info << "CId2Reader: ID2-Reply-Get-Blob: "
                     "blob already loaded: " << blob_id.ToString());
            return;
        }
    }

    if ( blob->HasSeq_entry() ) {
        ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
                 "Seq-entry already loaded: "<<blob_id.ToString());
        return;
    }

    if ( errors & fError_no_data ) {
        int state = CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        blob.SetBlobState(state);
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
        _ASSERT(CProcessor::IsLoaded(blob_id, chunk_id, blob));
        return;
    }

    if ( !reply.IsSetData() ) {
        // assume only blob info reply
        return;
    }

    if ( reply.GetData().GetData().empty() ) {
        ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
                 "no data in reply: "<<blob_id.ToString());
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
        _ASSERT(CProcessor::IsLoaded(blob_id, chunk_id, blob));
        return;
    }

    if ( reply.GetSplit_version() != 0 ) {
        // split info will follow
        // postpone parsing this blob
        loaded_set.m_Skeletons[blob_id] = &reply.GetData();
        return;
    }
    if ( errors & fError_warning_dead ) {
        //ERR_POST("blob.SetBlobState(CBioseq_Handle::fState_dead)");
        blob.SetBlobState(CBioseq_Handle::fState_dead);
    }
    if ( errors & fError_warning_suppressed ) {
        //ERR_POST("blob.SetBlobState(CBioseq_Handle::fState_suppress_perm)");
        blob.SetBlobState(CBioseq_Handle::fState_suppress_perm);
    }
    if ( reply.GetBlob_id().GetSub_sat() == CID2_Blob_Id::eSub_sat_snp ) {
        m_Dispatcher->GetProcessor(CProcessor::eType_Seq_entry_SNP)
            .ProcessBlobFromID2Data(result, blob_id, chunk_id,
                                    reply.GetData());
    }
    else {
        dynamic_cast<const CProcessor_ID2&>
            (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
            .ProcessData(result, blob_id, chunk_id, reply.GetData());
    }
    _ASSERT(CProcessor::IsLoaded(blob_id, chunk_id, blob));
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& loaded_set,
                                TErrorFlags /* errors */,
                                const CID2S_Reply_Get_Split_Info& reply)
{
    TChunkId chunk_id = CProcessor::kMain_ChunkId;
    TBlobId blob_id = GetBlobId(reply.GetBlob_id());
    CLoadLockBlob blob(result, blob_id);
    if ( !blob ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Split-Info: "
                 "no blob: " << blob_id.ToString());
        return;
    }
    if ( blob.IsLoaded() ) {
        ERR_POST(Info << "CId2Reader: ID2S-Reply-Get-Split-Info: "
                 "blob already loaded: " << blob_id.ToString());
        return;
    }
    if ( !reply.IsSetData() ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Split-Info: "
                 "no data in reply: "<<blob_id.ToString());
        return;
    }

    CConstRef<CID2_Reply_Data> skel;
    {{
        SId2LoadedSet::TSkeletons::iterator iter =
            loaded_set.m_Skeletons.find(blob_id);
        if ( iter != loaded_set.m_Skeletons.end() ) {
            skel = iter->second;
        }
    }}

    dynamic_cast<const CProcessor_ID2&>
        (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
        .ProcessData(result, blob_id, chunk_id,
                     reply.GetData(), reply.GetSplit_version(), skel);

    _ASSERT(CProcessor::IsLoaded(blob_id, chunk_id, blob));
    loaded_set.m_Skeletons.erase(blob_id);
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                SId2LoadedSet& /*loaded_set*/,
                                TErrorFlags /* errors */,
                                const CID2S_Reply_Get_Chunk& reply)
{
    TBlobId blob_id = GetBlobId(reply.GetBlob_id());
    CLoadLockBlob blob(result, blob_id);
    if ( !blob ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Chunk: "
                 "no blob: " << blob_id.ToString());
        return;
    }
    if ( !blob.IsLoaded() ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Chunk: "
                 "blob is not loaded yet: " << blob_id.ToString());
        return;
    }
    if ( !reply.IsSetData() ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Chunk: "
                 "no data in reply: "<<blob_id.ToString());
        return;
    }
    
    dynamic_cast<const CProcessor_ID2&>
        (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
        .ProcessData(result, blob_id, reply.GetChunk_id(), reply.GetData());
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)

void GenBankReaders_Register_Id2(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_Id2Reader);
}


/// Class factory for ID2 reader
///
/// @internal
///
class CId2ReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CId2Reader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CId2Reader> TParent;
public:
    CId2ReaderCF()
        : TParent(NCBI_GBLOADER_READER_ID2_DRIVER_NAME, 0) {}
    ~CId2ReaderCF() {}

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
            drv = new objects::CId2Reader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_Id2Reader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CId2ReaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_id2(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id2Reader(info_list, method);
}


END_NCBI_SCOPE
