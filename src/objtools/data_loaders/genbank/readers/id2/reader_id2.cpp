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
#include <corelib/ncbi_config_value.hpp>
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
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <corelib/plugin_manager_store.hpp>

#include <iomanip>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define DEFAULT_ID2_SERVICE_NAME "ID2"

static int GetDebugLevel(void)
{
    static int s_Value = -1;
    int value = s_Value;
    if ( value < 0 ) {
        value = GetConfigInt("GENBANK", "ID2_DEBUG");
        if ( value < 0 ) {
            value = 0;
        }
        s_Value = value;
    }
    return value;
}


enum EDebugLevel
{
    eTraceConn     = 4,
    eTraceASN      = 5,
    eTraceBlob     = 8,
    eTraceBlobData = 9
};


struct SId2LoadedSet
{
    typedef set<string> TStringSet;
    typedef set<CSeq_id_Handle> TSeq_idSet;
    typedef map<CBlob_id, CConstRef<CID2_Reply_Data> > TSkeletons;

    TStringSet m_Seq_idsByString;
    TSeq_idSet m_Seq_ids;
    TSeq_idSet m_Blob_ids;
    TSkeletons m_Skeletons;
};


namespace {
    class CDebugPrinter : public CNcbiOstrstream
    {
    public:
        ~CDebugPrinter()
            {
                (NcbiCout << rdbuf()).flush();
            }
    };
}


CId2Reader::CId2Reader(int max_connections)
    : m_AvoidRequest(0)
{
    m_RequestSerialNumber.Set(1);
    SetMaximumConnections(max_connections);
}


CId2Reader::~CId2Reader()
{
}


int CId2Reader::GetMaximumConnectionsLimit(void) const
{
#ifdef NCBI_THREADS
    return 3;
#else
    return 1;
#endif
}


void CId2Reader::x_Connect(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CId2Reader::x_Disconnect(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CId2Reader::x_Reconnect(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    ERR_POST("CId2Reader: ID2 GenBank connection failed: reconnecting...");
    m_Connections[conn].reset();
}


CConn_IOStream* CId2Reader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CConn_IOStream>& stream = m_Connections[conn];
    if ( !stream.get() ) {
        stream.reset(x_NewConnection());
    }
    return stream.get();
}


CConn_IOStream* CId2Reader::x_NewConnection(void)
{
    if ( !m_NextConnectTime.IsEmpty() ) {
        int wait_seconds = 
            (m_NextConnectTime - CTime(CTime::eCurrent))
            .GetCompleteSeconds();
        if ( wait_seconds > 0 ) {
            _TRACE("CId2Reader: "
                   "waiting "<<wait_seconds<<" before new connection");
            SleepSec(wait_seconds);
        }
    }
    
    STimeout tmout;
    tmout.sec = 20;
    tmout.usec = 0;

    bool is_service = false;
    string dst;
    if ( dst.empty() ) {
        string cgi = GetConfigString("GENBANK", "ID2_CGI_NAME");
        dst = cgi;
        is_service = false;
    }
    if ( dst.empty() ) {
        string srv = GetConfigString("GENBANK", "ID2_SERVICE_NAME",
                                     DEFAULT_ID2_SERVICE_NAME);
        dst = srv;
        is_service = true;
    }
        
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s;
        s << "CId2Reader: New connection to " << dst << "...\n";
    }
        
    AutoPtr<CConn_IOStream> stream;
    if ( is_service ) {
        stream.reset
            (new CConn_ServiceStream(dst, fSERV_Any, 0, 0, &tmout));
    }
    else {
        stream.reset
            (new CConn_HttpStream(dst/*, fHCC_AutoReconnect, &tmout*/));
    }
    if ( stream->bad() ) {
        NCBI_THROW(CLoaderException, eNoConnection, "connection failed");
    }

    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s;
        s << "CId2Reader: New connection to " << dst << " opened.\n";
    }
    x_InitConnection(*stream);

    return stream.release();
}


#define MConnFormat MSerial_AsnBinary


void CId2Reader::x_InitConnection(CNcbiIostream& stream)
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
            CDebugPrinter s;
            s << "CId2Reader(new): Sending";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << packet;
            }
            else {
                s << " ID2-Request-Packet";
            }
            s << "...\n";
        }
        stream << MConnFormat << packet << flush;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s;
            s << "CId2Reader(new): Sent ID2-Request-Packet.\n";
        }
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "stream is bad after sending");
        }
    }}
    
    // receive init reply
    CID2_Reply reply;
    {{
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s;
            s << "CId2Reader(new): Receiving ID2-Reply...\n";
        }
        stream >> MConnFormat >> reply;
        if ( GetDebugLevel() >= eTraceConn   ) {
            CDebugPrinter s;
            s << "CId2Reader(new): Received";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << reply;
            }
            else {
                s << " ID2-Reply.";
            }
            s << '\n';
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
    if ( !ids.IsLoaded() &&
         (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) ) {
        if ( !LoadSeq_idBlob_ids(result, seq_id) ) {
            return false;
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
        x_ProcessRequest(result, req);
        return true;
    }
}


bool CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids blobs,
                           TContentsMask mask)
{
    CID2_Request_Packet packet;
    vector<CLoadLockBlob> blob_locks;
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( (info.GetContentsMask() & mask) == 0 ) {
            continue; // skip this blob
        }
        const CBlob_id& blob_id = it->first;
        CLoadLockBlob blob(result, blob_id);
        if ( blob.IsLoaded() ) {
            continue;
        }

        blob_locks.push_back(blob);
        CRef<CID2_Request> req(new CID2_Request);
        packet.Set().push_back(req);
        CID2_Request_Get_Blob_Info& req2 =
            req->SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
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
                           const CBlob_id& blob_id, int chunk_id)
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
    _ASSERT(chunk_info.IsLoaded());
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
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s;
            s << "CId2Reader(" << conn <<"): Sending";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << packet;
            }
            else {
                s << " ID2-Request-Packet";
            }
            s << "...\n";
        }
        stream << MConnFormat << packet << flush;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s;
            s << "CId2Reader("<< conn <<"): "
                "Sent ID2-Request-Packet.\n";
        }
        if ( !stream ) {
            ERR_POST("CId2Reader: stream is bad after sending");
            return;
        }
    }}

    // process replies
    size_t remaining_count = request_count;
    while ( remaining_count > 0 ) {
        CID2_Reply reply;
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s;
            s << "CId2Reader("<< conn <<"): "
                "Receiving ID2-Reply...\n";
        }
        try {
            stream >> MConnFormat >> reply;
        }
        catch ( CException& exc ) {
            ERR_POST("CId2Reader: "
                     "reply deserialization failed: "<<exc.what());
            stream.setstate(ios::badbit);
            return;
        }
        if ( !stream ) {
            ERR_POST("CId2Reader: stream is bad after receiving");
            return;
        }
        if ( GetDebugLevel() >= eTraceConn   ) {
            CDebugPrinter s;
            s << "CId2Reader("<< conn <<"): Received";
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
            s << '\n';
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
    ITERATE ( SId2LoadedSet::TSeq_idSet, it, loaded_set.m_Blob_ids ) {
        SetAndSaveSeq_idBlob_ids(result, *it);
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
    const char* message;
    if ( error.IsSetMessage() ) {
        message = error.GetMessage().c_str();
    }
    else {
        message = "<empty>";
    }
    /*
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
        CTime current(CTime::eCurrent);
        current.AddSecond(retry_delay);
        m_NextConnectTime = current;
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
        ids->SetState(CBioseq_Handle::fState_other_error|
                      CBioseq_Handle::fState_no_data);
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
        ids->SetState(CBioseq_Handle::fState_other_error|
                      CBioseq_Handle::fState_no_data);
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
    CLoadLockBlob_ids ids(result, idh);
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( !(errors & fError_no_data) ) {
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
        ids.AddBlob_id(blob_id, mask);
        if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
            SetAndSaveBlobVersion(result, blob_id, src_blob_id.GetVersion());
        }
        if (errors & fError_warning) {
            ids->SetState(CBioseq_Handle::fState_other_error);
        }
    }
    else {
        ids->SetState(CBioseq_Handle::fState_no_data);
        SetAndSaveSeq_idBlob_ids(result, idh, ids);
    }
    if ( reply.IsSetEnd_of_reply() ) {
        SetAndSaveSeq_idBlob_ids(result, idh, ids);
    }
    else {
        loaded_set.m_Blob_ids.insert(idh);
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
            ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
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
        ERR_POST("CId2Reader: ID2S-Reply-Get-Split-Info: "
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
