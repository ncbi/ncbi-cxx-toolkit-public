#ifndef CONNECT__NCBI_HTTP2_SESSION_IMPL__HPP
#define CONNECT__NCBI_HTTP2_SESSION_IMPL__HPP

/*  $Id$
 * ===========================================================================
 *
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
 * Authors: Rafael Sadyrov
 *
 */

#include <connect/impl/ncbi_uv_nghttp2.hpp>
#include <connect/ncbi_http2_session.hpp>

#include <corelib/reader_writer.hpp>

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>


BEGIN_NCBI_SCOPE


#define H2S_RW_TRACE(message)      _TRACE(message)
#define H2S_SESSION_TRACE(message) _TRACE(message)
#define H2S_IOC_TRACE(message)     _TRACE(message)


using TH2S_Data = vector<char>;

template <class TBase>
struct SH2S_Event : TBase
{
    using TStart = typename TBase::SStart;

    enum EType { eStart, eData, eEof, eError };

    template <class ...TArgs>
    SH2S_Event(TStart start, TArgs&&... args) : TBase(forward<TArgs>(args)...), m_Type(eStart) , m_Start(move(start)) {}

    template <class ...TArgs>
    SH2S_Event(TH2S_Data data, TArgs&&... args) : TBase(forward<TArgs>(args)...), m_Type(eData) , m_Data(move(data)) {}

    template <class ...TArgs>
    SH2S_Event(EType type, TArgs&&... args) : TBase(forward<TArgs>(args)...), m_Type(type) {}

    SH2S_Event(SH2S_Event&& other);

    ~SH2S_Event();

    EType GetType() const { return m_Type; }

    TStart&    GetStart() { _ASSERT(m_Type == eStart); return m_Start; }
    TH2S_Data& GetData()  { _ASSERT(m_Type == eData);  return m_Data;  }

private:
    const char* GetTypeName() const;

    EType m_Type;

    union {
        TStart m_Start;
        TH2S_Data m_Data;
    };

    friend ostream& operator<<(ostream& os, const SH2S_Event& e)
    {
        return os << e.GetBaseName() << e.GetTypeName() << &e;
    }
};

template <class TEvent>
using TH2S_Queue = SThreadSafe<queue<TEvent>>;

struct SH2S_Response
{
    using SStart = CHttpHeaders::THeaders;

protected:
    const char* GetBaseName() const { return "response "; }
};

using TH2S_ResponseEvent = SH2S_Event<SH2S_Response>;
using TH2S_ResponseQueue = TH2S_Queue<TH2S_ResponseEvent>;

struct TH2S_WeakResponseQueue : weak_ptr<TH2S_ResponseQueue>
{
    TH2S_WeakResponseQueue(const std::shared_ptr<TH2S_ResponseQueue>& s) :
        weak_ptr(s),
        m_Id(s.get())
    {}

private:
    friend ostream& operator<<(ostream& os, const TH2S_WeakResponseQueue& q)
    {
        return os << q.m_Id;
    }

    const void* const m_Id;
};

struct SH2S_Request
{
    struct SStart
    {
        EReqMethod method;
        CUrl url;
        CHttpHeaders::THeaders headers;

        SStart(EReqMethod m, CUrl u, CHttpHeaders::THeaders h);
    };

    TH2S_WeakResponseQueue response_queue;

    SH2S_Request(TH2S_WeakResponseQueue q) : response_queue(move(q)) {}

protected:
    const char* GetBaseName() const { return "request "; }
};

using TH2S_RequestEvent = SH2S_Event<SH2S_Request>;
using TH2S_RequestQueue = TH2S_Queue<TH2S_RequestEvent>;

struct SH2S_IoStream
{
    TH2S_WeakResponseQueue response_queue;
    int32_t stream_id = 0;
    bool in_progress = true;
    size_t sent = 0;
    queue<TH2S_Data> pending;
    bool eof = false;
    CHttpHeaders::THeaders headers;

    SH2S_IoStream(TH2S_WeakResponseQueue q) : response_queue(q) {}

    ssize_t DataSourceRead(void* session, uint8_t* buf, size_t length, uint32_t* data_flags);
};

struct SH2S_Session;

using TH2S_SessionsByQueues = map<TH2S_WeakResponseQueue, reference_wrapper<SH2S_Session>, owner_less<weak_ptr<TH2S_ResponseQueue>>>;

struct SH2S_Session : SUvNgHttp2_SessionBase
{
    template <class... TNgHttp2Cbs>
    SH2S_Session(uv_loop_t* loop, const SSocketAddress& address, bool https, TH2S_SessionsByQueues& sessions_by_queues, TNgHttp2Cbs&&... callbacks);

    bool Request(TH2S_RequestEvent request);

    template <class TFunc>
    bool Event(TH2S_RequestEvent& event, TFunc f);

    bool IsFull() const { return m_Session.GetMaxStreams() <= m_Streams.size(); }

protected:
    int OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len);
    int OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code);
    int OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags);
    int OnError(nghttp2_session*, const char* msg, size_t) { Reset(msg); return 0; }

private:
    using TStreams = list<SH2S_IoStream>;

    TStreams::iterator Find(int32_t stream_id)
    {
        auto it = m_StreamsByIds.find(stream_id);
        return it == m_StreamsByIds.end() ? m_Streams.end() : it->second;
    }

    TStreams::iterator Find(const TH2S_WeakResponseQueue& response_queue)
    {
        auto it = m_StreamsByQueues.find(response_queue);
        return it == m_StreamsByQueues.end() ? m_Streams.end() : it->second;
    }

    int OnFrameRecv(nghttp2_session *session, const nghttp2_frame *frame);
    void OnReset(SUvNgHttp2_Error error) override;

    void Push(TH2S_WeakResponseQueue& response_queue, TH2S_ResponseEvent event)
    {
        // We can only send event if we still have corresponding queue
        if (auto queue = response_queue.lock()) {
            H2S_SESSION_TRACE(this << '/' << response_queue << " push " << event);
            queue->GetLock()->emplace(move(event));
        }
    }

    static int s_OnFrameRecv(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
    {
        _ASSERT(user_data);
        return static_cast<SH2S_Session*>(user_data)->OnFrameRecv(session, frame);
    }

    static ssize_t s_DataSourceRead(nghttp2_session*, int32_t,
            uint8_t* buf, size_t length, uint32_t* data_flags, nghttp2_data_source* source, void* user_data)
    {
        _ASSERT(source);
        _ASSERT(source->ptr);
        return static_cast<SH2S_IoStream*>(source->ptr)->DataSourceRead(user_data, buf, length, data_flags);
    }

    TStreams m_Streams;
    unordered_map<int32_t, TStreams::iterator> m_StreamsByIds;
    map<TH2S_WeakResponseQueue, TStreams::iterator, owner_less<weak_ptr<TH2S_ResponseQueue>>> m_StreamsByQueues;
    TH2S_SessionsByQueues& m_SessionsByQueues;
};

struct SH2S_IoCoordinator
{
    ~SH2S_IoCoordinator();

    void Process();

private:
    SH2S_Session* NewSession(const CUrl& url);

    SUv_Loop m_Loop;
    multimap<SSocketAddress, SUvNgHttp2_Session<SH2S_Session>> m_Sessions;
    TH2S_SessionsByQueues m_SessionsByQueues;
};

struct SH2S_Io
{
    TH2S_RequestQueue request_queue;
    SThreadSafe<SH2S_IoCoordinator> coordinator;

    static SH2S_Io& GetInstance() { static SH2S_Io io; return io; }
};

struct SH2S_ReaderWriter : IReaderWriter
{
    using TUpdateResponse = function<void(CHttpHeaders::THeaders)>;

    SH2S_ReaderWriter(TUpdateResponse update_response, shared_ptr<TH2S_ResponseQueue> response_queue, TH2S_RequestEvent request);

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override
    {
        return ReadFsm([&]() { return ReadImpl(buf, count, bytes_read); });
    }

    ERW_Result PendingCount(size_t* count) override
    {
        return ReadFsm([&]() { return PendingCountImpl(count); });
    }

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;
    ERW_Result Flush() override;

private:
    enum EState { eWriting, eWaiting, eReading, eEof, eError };

    ERW_Result ReadFsm(function<ERW_Result()> impl);
    ERW_Result ReadImpl(void* buf, size_t count, size_t* bytes_read);
    ERW_Result PendingCountImpl(size_t* count);

    ERW_Result Receive(ERW_Result (SH2S_ReaderWriter::*member)(TH2S_ResponseEvent&));
    ERW_Result ReceiveData(TH2S_ResponseEvent& incoming);
    ERW_Result ReceiveResponse(TH2S_ResponseEvent& incoming);

    void Push(TH2S_RequestEvent event)
    {
        H2S_RW_TRACE(m_ResponseQueue.get() << " push " << event);
        SH2S_Io::GetInstance().request_queue.GetLock()->emplace(move(event));
    }

    void Process() { SH2S_Io::GetInstance().coordinator.GetLock()->Process(); }

    TUpdateResponse m_UpdateResponse;
    shared_ptr<TH2S_ResponseQueue> m_ResponseQueue;
    TH2S_Data m_OutgoingData;
    TH2S_Data m_IncomingData;
    EState m_State = eWriting;
};


END_NCBI_SCOPE


#endif
