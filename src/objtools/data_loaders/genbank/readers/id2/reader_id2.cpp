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

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>

#include <corelib/ncbistre.hpp>
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

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <util/stream_utils.hpp>
#include <util/static_map.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <memory>
#include <iomanip>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define DEFAULT_ID2_SERVICE_NAME "ID2"

static int GetDebugLevel(void)
{
    static int var = GetConfigInt("GENBANK", "ID2_DEBUG");
    return var;
}


enum EDebugLevel
{
    eTraceConn     = 4,
    eTraceASN      = 5,
    eTraceBlob     = 8,
    eTraceBlobData = 9
};


class CDebugPrinter : public CNcbiOstrstream
{
public:
    ~CDebugPrinter()
        {
            (NcbiCout << rdbuf()).flush();
        }
};


DEFINE_STATIC_FAST_MUTEX(sx_FirstConnectionMutex);


CId2Reader::CId2Reader(TConn noConn)
    : m_NoMoreConnections(false), m_AvoidRequest(0)
{
    m_RequestSerialNumber.Set(1);
    noConn=3; // limit number of simultaneous connections to one
#if !defined(NCBI_THREADS)
    noConn=1;
#endif
    try {
        SetParallelLevel(noConn);
        m_FirstConnection.reset(x_NewConnection());
    }
    catch ( ... ) {
        // close all connections before exiting
        SetParallelLevel(0);
        throw;
    }
}


CId2Reader::~CId2Reader()
{
    SetParallelLevel(0);
}


CReader::TConn CId2Reader::GetParallelLevel(void) const
{
    return m_Pool.size();
}


void CId2Reader::SetParallelLevel(TConn size)
{
    size_t oldSize = m_Pool.size();
    for ( size_t i = size; i < oldSize; ++i ) {
        delete m_Pool[i];
        m_Pool[i] = 0;
    }

    m_Pool.resize(size);

    if ( m_FirstConnection.get() ) { // check if we should close 'first' conn
        for ( size_t i = 0; i < size; ++i ) {
            if ( m_Pool[i] == 0 ) { // there is empty slot, so let it live
                return;
            }
        }
        m_FirstConnection.reset();
    }
}


CConn_IOStream* CId2Reader::x_GetConnection(TConn conn)
{
    _ASSERT(conn < m_Pool.size());
    CConn_IOStream* ret = m_Pool[conn];
    if ( !ret ) {
        ret = x_NewConnection();

        if ( !ret ) {
            NCBI_THROW(CLoaderException, eNoConnection,
                       "too many connections failed: probably server is dead");
        }

        m_Pool[conn] = ret;
    }
    return ret;
}


void CId2Reader::Reconnect(TConn conn)
{
    _ASSERT(conn < m_Pool.size());
    _TRACE("Reconnect(" << conn << ")");
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s;
        s << "CId2Reader(" << conn << "): Closing connection...\n";
    }
    delete m_Pool[conn];
    m_Pool[conn] = 0;
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s;
        s << "CId2Reader(" << conn << "): Connection closed.\n";
    }
}


CConn_IOStream* CId2Reader::x_NewConnection(void)
{
    if ( m_FirstConnection.get() ) {
        CFastMutexGuard guard(sx_FirstConnectionMutex);
        if ( m_FirstConnection.get() ) {
            return m_FirstConnection.release();
        }
    }
    if ( !m_NextConnectTime.IsEmpty() ) {
        int wait_seconds =
            (m_NextConnectTime - CTime(CTime::eCurrent)).GetCompleteSeconds();
        if ( wait_seconds > 0 ) {
            _TRACE("CId2Reader: "
                   "waiting "<<wait_seconds<<" before new connection");
            SleepSec(wait_seconds);
        }
    }

    for ( int i = 0; !m_NoMoreConnections && i < 3; ++i ) {
        try {
            STimeout tmout;
            tmout.sec = 20;
            tmout.usec = 0;
            auto_ptr<CConn_IOStream> stream;
            
            bool is_service = false;
            string dst;
            if ( dst.empty() ) {
                static string cgi = GetConfigString("GENBANK",
                                                    "ID2_CGI_NAME");
                dst = cgi;
                is_service = false;
            }
            if ( dst.empty() ) {
                static string srv = GetConfigString("GENBANK",
                                                    "ID2_SERVICE_NAME",
                                                    DEFAULT_ID2_SERVICE_NAME);
                dst = srv;
                is_service = true;
            }

            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s;
                s << "CId2Reader: New connection to " << dst << "...\n";
            }

            if ( is_service ) {
                stream.reset(new CConn_ServiceStream(dst,
                                                     fSERV_Any,
                                                     0, 0, &tmout));
            }
            else {
                stream.reset(new CConn_HttpStream(dst/*,
                                                  fHCC_AutoReconnect,
                                                  &tmout*/));
            }
            if ( stream->bad() ) {
                ERR_POST("CId2Reader::x_NewConnection: cannot connect");
                continue;
            }
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s;
                s << "CId2Reader: New connection to " << dst << " opened.\n";
            }
            //x_InitConnection(*stream);
            return stream.release();
        }
        catch ( CException& exc ) {
            ERR_POST("CId2Reader::x_NewConnection: cannot connect: " <<
                     exc.what());
        }
    }
    m_NoMoreConnections = true;
    return 0;
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
        stream << MConnFormat << packet;
        stream.flush();
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
    get_blob_id.SetSeq_id().SetSeq_id().SetSeq_id(const_cast<CSeq_id&>(seq_id));
    get_blob_id.SetExternal();
}


void CId2Reader::x_SetDetails(CID2_Get_Blob_Details& details,
                              TBlobContentsMask mask)
{
}


CId2Reader::TBlob_id CId2Reader::GetBlob_id(const CID2_Blob_Id& blob_id)
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


void CId2Reader::ResolveString(CReaderRequestResult& result,
                               const string& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CID2_Request req;
        x_SetResolve(req.SetRequest().SetGet_blob_id(), seq_id);
        x_ProcessRequest(result, req);
    }
}


void CId2Reader::ResolveSeq_id(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CID2_Request req;
        x_SetResolve(req.SetRequest().SetGet_blob_id(), *seq_id.GetSeqId());
        x_ProcessRequest(result, req);
    }
}


void CId2Reader::ResolveSeq_ids(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CID2_Request req;
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req.SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
        x_ProcessRequest(result, req);
    }
}


CReader::TBlobVersion CId2Reader::GetBlobVersion(CReaderRequestResult& result,
                                                 const CBlob_id& blob_id)
{
    return 0;
}


void CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           const string& seq_id,
                           TBlobContentsMask /*mask*/)
{
    if ( m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
        ResolveString(result, seq_id);
        return;
    }
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(), seq_id);
    //x_SetDetails(req2.SetGet_data(), mask);
    x_ProcessRequest(result, req);
}


void CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id,
                           TBlobContentsMask mask)
{
    if ( m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
        ResolveSeq_id(result, seq_id);
        return;
    }
    CLoadLockBlob_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        // shortcut - we know Seq-id -> Blob-id resolution
        LoadBlobs(result, ids, mask);
    }
    else {
        CID2_Request req;
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                     *seq_id.GetSeqId());
        x_SetDetails(req2.SetGet_data(), mask);
        x_ProcessRequest(result, req);
    }
}


void CId2Reader::LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids blobs,
                           TBlobContentsMask mask)
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
        if ( !blob.IsLoaded() ) {
            blob_locks.push_back(blob);
            CRef<CID2_Request> req(new CID2_Request);
            packet.Set().push_back(req);
            CID2_Request_Get_Blob_Info& req2 =
                req->SetRequest().SetGet_blob_info();
            x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
            x_SetDetails(req2.SetGet_data(), mask);
        }
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet);
    }
}


void CId2Reader::LoadBlob(CReaderRequestResult& result,
                          const TBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsLoaded() ) {
        CID2_Request req;
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
        req2.SetGet_data();
        x_ProcessRequest(result, req);
    }
}


void CId2Reader::LoadChunk(CReaderRequestResult& result,
                           const CBlob_id& blob_id, int chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);
    if ( blob->GetSplitInfo().GetChunk(chunk_id).IsLoaded() ) {
        return;
    }
    CID2_Request req;
    CID2S_Request_Get_Chunks& req2 = req.SetRequest().SetGet_chunks();
    x_SetResolve(req2.SetBlob_id(), blob_id);
    if ( blob->GetBlobVersion() > 0 ) {
        req2.SetBlob_id().SetVersion(blob->GetBlobVersion());
    }
    //req2.SetSplit_version(blob->GetSplitInfo().GetSplitVersion());
    req2.SetChunks().push_back(CID2S_Chunk_Id(chunk_id));
    x_ProcessRequest(result, req);
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
    vector<bool> done(request_count);
    typedef CReaderRequestResult::TLoadedSet TLoadedSet;
    AutoPtr<TLoadedSet, ArrayDeleter<TLoadedSet> > loaded_sets;
    if ( request_count > 1 ) {
        loaded_sets.reset(new TLoadedSet[request_count]);
    }

    {{
        CReaderRequestConn conn(result);
        CNcbiIostream& stream = *x_GetConnection(conn);
        // send request
        {{
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s;
                s << "CId2Reader("<<conn.GetConn()<<"): Sending";
                if ( GetDebugLevel() >= eTraceASN ) {
                    s << ": " << MSerial_AsnText << packet;
                }
                else {
                    s << " ID2-Request-Packet";
                }
                s << "...\n";
            }
            stream << MConnFormat << packet;
            stream.flush();
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s;
                s << "CId2Reader("<<conn.GetConn()<<"): "
                    "Sent ID2-Request-Packet.\n";
            }
            if ( !stream ) {
                ERR_POST("CId2Reader: stream is bad after sending");
                x_Reconnect(result, 1);
                return;
            }
        }}

        // process replies
        size_t remaining_count = request_count;
        while ( remaining_count > 0 ) {
            CID2_Reply reply;
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s;
                s << "CId2Reader("<<conn.GetConn()<<"): "
                    "Receiving ID2-Reply...\n";
            }
            try {
                stream >> MConnFormat >> reply;
            }
            catch ( CException& exc ) {
                ERR_POST("CId2Reader: "
                         "reply deserialization failed: "<<exc.what());
                x_Reconnect(result, 1);
                return;
            }
            if ( !stream ) {
                ERR_POST("CId2Reader: stream is bad after receiving");
                x_Reconnect(result, 1);
                return;
            }
            if ( GetDebugLevel() >= eTraceConn   ) {
                CDebugPrinter s;
                s << "CId2Reader("<<conn.GetConn()<<"): Received";
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
            x_ProcessReply(result, reply);
            if ( loaded_sets.get() ) {
                ITERATE ( TLoadedSet, it, result.GetLoadedSet() ) {
                    loaded_sets.get()[num].insert(*it);
                }
                result.ResetLoadedSet();
            }
            if ( reply.IsSetEnd_of_reply() ) {
                done[num] = true;
                if ( loaded_sets.get() ) {
                    result.UpdateLoadedSet(loaded_sets.get()[num]);
                }
                else {
                    result.UpdateLoadedSet();
                }
                --remaining_count;
            }
        }
    }}
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
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
        x_ProcessReply(result, errors, reply.GetReply().GetGet_seq_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_id:
        x_ProcessReply(result, errors, reply.GetReply().GetGet_blob_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_seq_ids:
        x_ProcessReply(result, errors, reply.GetReply().GetGet_blob_seq_ids());
        break;
    case CID2_Reply::TReply::e_Get_blob:
        x_ProcessReply(result, errors, reply.GetReply().GetGet_blob());
        break;
    case CID2_Reply::TReply::e_Get_split_info:
        x_ProcessReply(result, errors, reply.GetReply().GetGet_split_info());
        break;
    case CID2_Reply::TReply::e_Get_chunk:
        x_ProcessReply(result, errors, reply.GetReply().GetGet_chunk());
        break;
    default:
        break;
    }
}


CId2Reader::TErrorFlags
CId2Reader::x_ProcessError(CReaderRequestResult& result,
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
        x_Reconnect(result, error, 1);
        break;
    case CID2_Error::eSeverity_failed_server:
        severity = "server error: ";
        error_flags |= fError_bad_connection;
        x_Reconnect(result, error, 10);
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


void CId2Reader::x_Reconnect(CReaderRequestResult& result,
                             const CID2_Error& error,
                             int retry_delay)
{
    if ( error.IsSetRetry_delay() && error.GetRetry_delay() >= 0 ) {
        retry_delay = error.GetRetry_delay();
    }
    x_Reconnect(result, retry_delay);
}


void CId2Reader::x_Reconnect(CReaderRequestResult& result,
                             int retry_delay)
{
    CReaderRequestConn conn(result);
    if ( conn.GetConn() >= 0 ) {
        CTime current(CTime::eCurrent);
        current.AddSecond(retry_delay);
        m_NextConnectTime = current;
        Reconnect(conn.GetConn());
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Seq_id& reply)
{
    // we can save this data in cache
    const CID2_Request_Get_Seq_id& request = reply.GetRequest();
    const CID2_Seq_id& request_id = request.GetSeq_id();
    switch ( request_id.Which() ) {
    case CID2_Seq_id::e_String:
    {
        CLoadLockSeq_ids ids(result, request_id.GetString());
        if ( !ids.IsLoaded() ) {
            x_ProcessReply(result, errors, ids, reply);
        }
        break;
    }

    case CID2_Seq_id::e_Seq_id:
    {
        CLoadLockSeq_ids ids(result, request_id.GetSeq_id());
        if ( !ids.IsLoaded() ) {
            x_ProcessReply(result, errors, ids, reply);
        }
        break;
    }

    default:
        break;
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags errors,
                                CLoadLockSeq_ids& ids,
                                const CID2_Reply_Get_Seq_id& reply)
{
    if ( errors & fError_no_data ) {
        // no Seq-ids
        ids->SetState(CBioseq_Handle::fState_other_error|
                      CBioseq_Handle::fState_no_data);
        ids.SetLoaded();
        return;
    }
    switch ( reply.GetRequest().GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            ids.AddSeq_id(**it);
        }
        if ( reply.IsSetEnd_of_reply() ) {
            ids.SetLoaded();
        }
        else {
            result.SetLoaded(ids);
        }
        break;
    default:
        // ???
        break;
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Blob_Id& reply)
{
    const CSeq_id& seq_id = reply.GetSeq_id();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(seq_id);
    CLoadLockBlob_ids ids(result, idh);
    if ( !(errors & fError_no_data) ) {
        CBlob_id blob_id = GetBlob_id(reply.GetBlob_id());
        TBlobContentsMask mask = 0;
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
        if (errors & fError_warning) {
            ids->SetState(CBioseq_Handle::fState_other_error);
        }
    }
    else {
        ids->SetState(CBioseq_Handle::fState_no_data);
    }
    if ( reply.IsSetEnd_of_reply() ) {
        ids.SetLoaded();
    }
    else {
        result.SetLoaded(ids);
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& /* result */,
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
                                  GetBlob_id(reply.GetBlob_id()), "");
            }
        }
    }
*/
}

/*
CTSE_Info& CId2Reader::GetTSE_Info(CReaderRequestResult& result,
                                   const CID2_Blob_Id& blob_id)
{
    CRef<CTSE_Info> tse = result.GetTSE_Info(GetBlob_id(blob_id));
    return *tse;
}


CTSE_Chunk_Info& CId2Reader::GetChunk_Info(CReaderRequestResult& result,
                                           const CID2_Blob_Id& blob_id,
                                           const CID2S_Chunk_Id& chunk_id)
{
    return GetTSE_Info(result, blob_id).GetChunk(chunk_id);
}
*/

void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags errors,
                                const CID2_Reply_Get_Blob& reply)
{
    TBlob_id blob_id = GetBlob_id(reply.GetBlob_id());
    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoaded() ) {
        m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
        ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
                 "blob already loaded: " << blob_id.ToString());
        return;
    }

    if ( blob->HasSeq_entry() ) {
        ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
                 "Seq-entry already loaded: "<<blob_id.ToString());
        return;
    }

    if ( (errors & fError_no_data) ) {
        blob->SetBlobState(CBioseq_Handle::fState_no_data);
        blob.SetLoaded();
        return;
    }

    if ( !reply.IsSetData() || reply.GetData().GetData().empty() ) {
        ERR_POST("CId2Reader: ID2-Reply-Get-Blob: "
                 "no data in reply: "<<blob_id.ToString());
        blob->SetBlobState(CBioseq_Handle::fState_no_data);
        blob.SetLoaded();
        return;
    }

    if (reply.GetBlob_id().GetSub_sat() == CID2_Blob_Id::eSub_sat_snp){
        x_ReadSNPData(*blob, reply.GetData());
    }
    else {
        CRef<CSeq_entry> entry(new CSeq_entry);
        x_ReadData(reply.GetData(), Begin(*entry));
        blob->SetSeq_entry(*entry);
    }

    if ( reply.GetBlob_id().IsSetVersion() ) {
        blob->SetBlobVersion(abs(reply.GetBlob_id().GetVersion()));
    }

    if ( reply.GetSplit_version() == 0 ) {
        // no more data
        blob.SetLoaded();
        result.AddTSE_Lock(blob);
    }
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags /* errors */,
                                const CID2S_Reply_Get_Split_Info& reply)
{
    TBlob_id blob_id = GetBlob_id(reply.GetBlob_id());
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
    if ( !blob->HasSeq_entry() ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Split-Info: "
                 "Seq-entry is not loaded yet: "<<blob_id.ToString());
        return;
    }
    if ( !reply.IsSetData() ) {
        ERR_POST("CId2Reader: ID2S-Reply-Get-Split-Info: "
                 "no data in reply: "<<blob_id.ToString());
        return;
    }

    CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
    x_ReadData(reply.GetData(), Begin(*split_info));
    CSplitParser::Attach(*blob, *split_info);
    blob->GetSplitInfo().SetSplitVersion(reply.GetSplit_version());
    blob.SetLoaded();
    result.AddTSE_Lock(blob);
}


void CId2Reader::x_ProcessReply(CReaderRequestResult& result,
                                TErrorFlags /* errors */,
                                const CID2S_Reply_Get_Chunk& reply)
{
    TBlob_id blob_id = GetBlob_id(reply.GetBlob_id());
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
    
    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    x_ReadData(reply.GetData(), Begin(*chunk));
    CTSE_Chunk_Info& chunk_info =
        blob->GetSplitInfo().GetChunk(reply.GetChunk_id());
    CSplitParser::Load(chunk_info, *chunk);
    chunk_info.SetLoaded();
}


class COctetStringSequenceReader : public CByteSourceReader
{
public:
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;
    COctetStringSequenceReader(const TOctetStringSequence& in)
        : m_Input(in),
          m_CurVec(in.begin())
        {
            x_SetVec();
        }
    
    virtual size_t Read(char* buffer, size_t bufferLength)
        {
            size_t size = m_CurSize - m_CurPos;
            while ( size == 0 ) {
                if ( m_CurVec == m_Input.end() ) {
                    return 0;
                }
                ++m_CurVec;
                x_SetVec();
                size = m_CurSize - m_CurPos;
            }
            size = min(size, bufferLength);
            memcpy(buffer, &(**m_CurVec)[m_CurPos], size);
            m_CurPos += size;
            return size;
        }
    virtual bool EndOfData(void) const
        {
            return m_CurPos == m_CurSize && m_CurVec == m_Input.end();
        }
    virtual bool Pushback(const char* /* data */, size_t size)
        {
            while ( size ) {
                size_t chunk = min(size, m_CurPos);
                if ( chunk == 0 ) {
                    if ( m_CurVec == m_Input.begin() ) {
                        return false;
                    }
                    --m_CurVec;
                    x_SetVec();
                }
                else {
                    m_CurPos -= chunk;
                    size -= chunk;
                }
            }
            return true;
        }

protected:
    void x_SetVec(void)
        {
            m_CurPos = 0;
            m_CurSize = m_CurVec == m_Input.end()? 0: (**m_CurVec).size();
        }
private:
    const TOctetStringSequence& m_Input;
    TOctetStringSequence::const_iterator m_CurVec;
    size_t m_CurPos;
    size_t m_CurSize;
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


class COctetStringSequenceStream : public CConn_MemoryStream
{
public:
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;
    COctetStringSequenceStream(const TOctetStringSequence& inp)
        {
            ITERATE( TOctetStringSequence, it, inp ) {
                write(&(**it)[0], (*it)->size());
            }
        }
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


CObjectIStream* CId2Reader::x_OpenDataStream(const CID2_Reply_Data& data)
{
    // TEMP: TODO: remove this
    if ( data.GetData_format() == CID2_Reply_Data::eData_format_xml &&
         data.GetData_compression()==CID2_Reply_Data::eData_compression_gzip ){
        // FIX old/wrong split fields
        const_cast<CID2_Reply_Data&>(data)
            .SetData_format(CID2_Reply_Data::eData_format_asn_binary);
        const_cast<CID2_Reply_Data&>(data)
            .SetData_compression(CID2_Reply_Data::eData_compression_nlmzip);
        if ( data.GetData_type() > CID2_Reply_Data::eData_type_seq_entry ) {
            const_cast<CID2_Reply_Data&>(data)
                .SetData_type(data.GetData_type()+1);
        }
    }
    CRef<CByteSourceReader> reader
        (new COctetStringSequenceReader(data.GetData()));
    auto_ptr<CNcbiIstream> stream;
    switch ( data.GetData_compression() ) {
    case CID2_Reply_Data::eData_compression_none:
        break;
    case CID2_Reply_Data::eData_compression_nlmzip:
        reader.Reset(new CNlmZipBtRdr(&*reader));
        break;
    case CID2_Reply_Data::eData_compression_gzip:
        stream.reset(new CCompressionIStream
                     (*new COctetStringSequenceStream(data.GetData()),
                      new CZipStreamDecompressor,
                      CCompressionIStream::fOwnAll));
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data compression");
    }
    ESerialDataFormat format;
    switch ( data.GetData_format() ) {
    case CID2_Reply_Data::eData_format_asn_binary:
        format = eSerial_AsnBinary;
        break;
    case CID2_Reply_Data::eData_format_asn_text:
        format = eSerial_AsnText;
        break;
    case CID2_Reply_Data::eData_format_xml:
        format = eSerial_Xml;
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data format");
    }
    auto_ptr<CObjectIStream> in;
    if ( stream.get() ) {
        in.reset(CObjectIStream::Open(format, *stream.release(), true));
    }
    else {
        in.reset(CObjectIStream::Create(format, *reader));
    }
    return in.release();
}


void CId2Reader::x_ReadData(const CID2_Reply_Data& data,
                            const CObjectInfo& object)
{
    auto_ptr<CObjectIStream> in(x_OpenDataStream(data));
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
        if ( object.GetTypeInfo() != CSeq_entry::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected Seq-entry");
        }
        break;
    case CID2_Reply_Data::eData_type_id2s_split_info:
        if ( object.GetTypeInfo() != CID2S_Split_Info::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected ID2S-Split-Info");
        }
        break;
    case CID2_Reply_Data::eData_type_id2s_chunk:
        if ( object.GetTypeInfo() != CID2S_Chunk::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected ID2S-Chunk");
        }
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data type");
    }
    SetSeqEntryReadHooks(*in);
    in->Read(object);
    if ( GetDebugLevel() >= eTraceBlob ) {
        CDebugPrinter s;
        s << "Data contents: " << MSerial_AsnText << object;
    }
}


void CId2Reader::x_ReadSNPData(CTSE_Info& tse, const CID2_Reply_Data& data)
{
    auto_ptr<CObjectIStream> in(x_OpenDataStream(data));
    if ( data.GetData_type() !=  CID2_Reply_Data::eData_type_seq_entry ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadSNPData(): Seq-entry expected");
    }
#if 1
    CRef<CSeq_entry> entry(new CSeq_entry);
    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    CSeq_annot_SNP_Info_Reader::Parse(*in, *entry, snps);
    tse.SetSeq_entry(*entry, snps);
#else
    CRef<CSeq_entry> entry(new CSeq_entry);
    SetSeqEntryReadHooks(*in);
    *in >> *entry;
    if ( GetDebugLevel() >= eTraceBlob ) {
        CDebugPrinter s;
        s << "Data contents: " << MSerial_AsnText << *entry;
    }
    tse.SetSeq_entry(*entry);
#endif
}


END_SCOPE(objects)

void GenBankReaders_Register_Id2(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_Id2Reader);
}


const string kId2ReaderDriverName("id2");


/// Class factory for ID2 reader
///
/// @internal
///
class CId2ReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CId2Reader>
{
public:
    typedef 
      CSimpleClassFactoryImpl<objects::CReader, objects::CId2Reader> TParent;
public:
    CId2ReaderCF() : TParent(kId2ReaderDriverName, 0)
    {
    }
    ~CId2ReaderCF()
    {
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
