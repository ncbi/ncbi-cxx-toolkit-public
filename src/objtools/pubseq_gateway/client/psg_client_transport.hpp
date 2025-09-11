#ifndef OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_TRANSPORT__HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_TRANSPORT__HPP

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
 * Authors: Dmitri Dmitrienko, Rafael Sadyrov
 *
 */

#include <objtools/pubseq_gateway/client/impl/misc.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#define __STDC_FORMAT_MACROS

#include <array>
#include <memory>
#include <string>
#include <cassert>
#include <ostream>
#include <iostream>
#include <atomic>
#include <functional>
#include <limits>
#include <unordered_map>
#include <cstdint>
#include <set>
#include <thread>
#include <utility>
#include <condition_variable>
#include <forward_list>
#include <chrono>
#include <sstream>
#include <random>
#include <type_traits>
#include <unordered_set>
#include <optional>

#include <connect/impl/ncbi_uv_nghttp2.hpp>
#include <connect/services/netservice_api.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_url.hpp>
#include <corelib/ncbi_cookies.hpp>


BEGIN_NCBI_SCOPE

// Different TRACE macros allow turning off tracing in some classes
#define PSG_THROTTLING_TRACE(message)      _TRACE(message)
#define PSG_IO_SESSION_TRACE(message)      _TRACE(message)
#define PSG_IO_TRACE(message)              _TRACE(message)
#define PSG_DISCOVERY_TRACE(message)       _TRACE(message)

inline uint64_t SecondsToMs(double seconds)
{
    return seconds > 0.0 ? static_cast<uint64_t>(seconds * milli::den) : 0;
}

struct SPSG_ArgsBase : CUrlArgs
{
    enum EValue {
        eItemType,
        eChunkType,
        eBlobId,
        eId2Chunk,
    };

    enum EItemType {
        eBioseqInfo,
        eBlobProp,
        eBlob,
        eReply,
        eBioseqNa,
        eNaStatus,
        ePublicComment,
        eProcessor,
        eIpgInfo,
        eAccVerHistory,
        eUnknownItem,
    };

    enum EChunkType : unsigned {
        eUnknownChunk    = 0x00,
        eMeta            = 0x01,
        eData            = 0x02,
        eMessage         = 0x04,
        eDataAndMeta     = eData     | eMeta,
        eMessageAndMeta  = eMessage  | eMeta,
    };

    using CUrlArgs::CUrlArgs;

    const string& GetValue(const string& name) const
    {
        bool not_used;
        return CUrlArgs::GetValue(name, &not_used);
    }

protected:
    template <EValue value> struct SArg;
};

template <>
struct SPSG_ArgsBase::SArg<SPSG_ArgsBase::eItemType>
{
    using TType = pair<SPSG_ArgsBase::EItemType, reference_wrapper<const string>>;
    static constexpr auto name = "item_type";
    static TType Get(const string& value);
};

template <>
struct SPSG_ArgsBase::SArg<SPSG_ArgsBase::eChunkType>
{
    using TType = pair<SPSG_ArgsBase::EChunkType, reference_wrapper<const string>>;
    static constexpr auto name = "chunk_type";
    static TType Get(const string& value);
};

template <>
struct SPSG_ArgsBase::SArg<SPSG_ArgsBase::eBlobId>
{
    using TType = reference_wrapper<const string>;
    static constexpr auto name = "blob_id";
    static TType Get(const string& value) { return value; }
};

template <>
struct SPSG_ArgsBase::SArg<SPSG_ArgsBase::eId2Chunk>
{
    using TType = reference_wrapper<const string>;
    static constexpr auto name = "id2_chunk";
    static TType Get(const string& value) { return value; }
};

struct SPSG_Args : SPSG_ArgsBase
{
    using SPSG_ArgsBase::SPSG_ArgsBase;
    using SPSG_ArgsBase::GetValue;

    template <EValue value>
    auto GetValue() const
    {
        using TArg = SArg<value>;
        auto& cached = get<SValue<value>>(m_Cached);
        return cached.has_value() ? cached.value() : cached.emplace(TArg::Get(GetValue(TArg::name)));
    }

private:
    // Cannot use std::optional template directly;
    // Otherwise, different values would have same types and get<type>(tuple) above would not work
    template <EValue value> struct SValue : std::optional<typename SArg<value>::TType> {};

    mutable tuple<SValue<eItemType>, SValue<eChunkType>, SValue<eBlobId>, SValue<eId2Chunk>> m_Cached;
};

template <typename TValue>
struct SPSG_Nullable : protected CNullable<TValue>
{
    template <template<typename> class TCmp>
    bool Cmp(TValue s) const { return !CNullable<TValue>::IsNull() && TCmp<TValue>()(*this, s); }

    using CNullable<TValue>::operator TValue;
    using CNullable<TValue>::operator=;
};

struct SPSG_Chunk : string
{
    using string::string;

    SPSG_Chunk() = default;

    SPSG_Chunk(SPSG_Chunk&&) = default;
    SPSG_Chunk& operator=(SPSG_Chunk&&) = default;

    SPSG_Chunk(const SPSG_Chunk&) = delete;
    SPSG_Chunk& operator=(const SPSG_Chunk&) = delete;
};

struct SPSG_Env : CNcbiEnvironment
{
    string GetCookie(const string& name) const;

private:
    mutable optional<CHttpCookies> m_Cookies;
};

struct SPSG_Params
{
    TPSG_DebugPrintout debug_printout;
    TPSG_MaxConcurrentSubmits max_concurrent_submits;
    TPSG_MaxConcurrentRequestsPerServer max_concurrent_requests_per_server;
    TPSG_RequestsPerIo requests_per_io;
    TPSG_IoTimerPeriod io_timer_period;
    const unsigned request_timeout;
    const unsigned competitive_after;
    TPSG_RequestRetries request_retries;
    TPSG_RefusedStreamRetries refused_stream_retries;
    TPSG_UserRequestIds user_request_ids;
    TPSG_AuthTokenName auth_token_name;
    TPSG_AuthToken auth_token;
    TPSG_AdminAuthTokenName admin_auth_token_name;
    TPSG_AdminAuthToken admin_auth_token;
    SSocketAddress proxy;
    TPSG_PsgClientMode client_mode;

    SPSG_Params(SPSG_Env env = {}) :
        debug_printout(TPSG_DebugPrintout::eGetDefault),
        max_concurrent_submits(TPSG_MaxConcurrentSubmits::eGetDefault),
        max_concurrent_requests_per_server(TPSG_MaxConcurrentRequestsPerServer::eGetDefault),
        requests_per_io(TPSG_RequestsPerIo::eGetDefault),
        io_timer_period(TPSG_IoTimerPeriod::eGetDefault),
        request_timeout(s_GetRequestTimeout(io_timer_period)),
        competitive_after(s_GetCompetitiveAfter(io_timer_period, request_timeout)),
        request_retries(TPSG_RequestRetries::eGetDefault),
        refused_stream_retries(TPSG_RefusedStreamRetries::eGetDefault),
        user_request_ids(TPSG_UserRequestIds::eGetDefault),
        auth_token_name(TPSG_AuthTokenName::eGetDefault),
        auth_token([&](string v) { return v.empty() ? env.GetCookie(auth_token_name) : v; }),
        admin_auth_token_name(TPSG_AdminAuthTokenName::eGetDefault),
        admin_auth_token([&](string v) { return v.empty() ? env.GetCookie(admin_auth_token_name) : v; }),
        proxy(SSocketAddress::Parse(env.Get("HTTP_PROXY"), SSocketAddress::SHost::EName::eOriginal)),
        client_mode(TPSG_PsgClientMode::eGetDefault)
    {}

    string GetCookie(function<string()> get_auth_token);

private:
    static unsigned s_GetRequestTimeout(double io_timer_period);
    static unsigned s_GetCompetitiveAfter(double io_timer_period, double timeout);
};

struct SDebugPrintout
{
    const string id;

    SDebugPrintout(string i, const SPSG_Params& params) :
        id(std::move(i)),
        m_Params(params)
    {
        if (IsPerf()) m_Events.reserve(20);
    }

    ~SDebugPrintout();

    template <class TArg, class ...TRest>
    struct SPack;

    template <class TArg>
    SPack<TArg> operator<<(const TArg& arg) { return { this, &arg }; }

private:
    template <class ...TArgs>
    void Process(TArgs&&... args)
    {
        if (IsPerf()) return Event(std::forward<TArgs>(args)...);

        if (m_Params.debug_printout == EPSG_DebugPrintout::eNone) return;

        Print(std::forward<TArgs>(args)...);
    }

    enum EType { eSend = 1000, eReceive, eClose, eRetry, eFail };

    template <class ...TArgs>
    void Event(SSocketAddress, TArgs&&...)          { Event(eSend);    }
    void Event(const SPSG_Args&, const SPSG_Chunk&) { Event(eReceive); }
    void Event(uint32_t)                            { Event(eClose);   }
    void Event(unsigned, const SUvNgHttp2_Error&)   { Event(eRetry);   }
    void Event(const SUvNgHttp2_Error&)             { Event(eFail);    }

    void Event(EType type)
    {
        auto ms = chrono::duration<double, milli>(chrono::steady_clock::now().time_since_epoch()).count();
        auto thread_id = this_thread::get_id();
        m_Events.emplace_back(ms, type, thread_id);
    }

    void Print(SSocketAddress address, const string& path, const string& sid, const string& phid, const string& ip, SUv_Tcp::TPort port);
    void Print(const SPSG_Args& args, const SPSG_Chunk& chunk);
    void Print(uint32_t error_code);
    void Print(unsigned retries, const SUvNgHttp2_Error& error);
    void Print(const SUvNgHttp2_Error& error);

    bool IsPerf() const
    {
        return m_Params.client_mode == EPSG_PsgClientMode::ePerformance;
    }

    SPSG_Params m_Params;
    vector<tuple<double, EType, thread::id>> m_Events;
};

template <class TArg, class ...TRest>
struct SDebugPrintout::SPack : SPack<TRest...>
{
    SPack(SPack<TRest...>&& base, const TArg* arg) :
        SPack<TRest...>(std::move(base)),
        m_Arg(arg)
    {}

    template <class TNextArg>
    SPack<TNextArg, TArg, TRest...> operator<<(const TNextArg& next_arg)
    {
        return { std::move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs&&... args)
    {
        SPack<TRest...>::Process(*m_Arg, std::forward<TArgs>(args)...);
    }

private:
    const TArg* m_Arg;
};

template <class TArg>
struct SDebugPrintout::SPack<TArg>
{
    SPack(SDebugPrintout* debug_printout, const TArg* arg) :
        m_DebugPrintout(debug_printout),
        m_Arg(arg)
    {}

    template <class TNextArg>
    SPack<TNextArg, TArg> operator<<(const TNextArg& next_arg)
    {
        return { std::move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs&&... args)
    {
        m_DebugPrintout->Process(*m_Arg, std::forward<TArgs>(args)...);
    }

private:
    SDebugPrintout* m_DebugPrintout;
    const TArg* m_Arg;
};

struct SPSG_Stats;

using TPSG_Queue = CPSG_WaitingQueue<shared_ptr<CPSG_Reply>>;

struct SPSG_Reply
{
    struct SState
    {
        struct SStatus
        {
            template <typename T, enable_if_t<is_convertible_v<T, int>, int> = 0>
            SStatus(T h) : m_Http(static_cast<int>(h)), m_Psg(From(m_Http)) {}
            SStatus(EPSG_Status p) : m_Http(From(p)), m_Psg(p) {}

            int GetHttpCode() const { return m_Http; }
            operator EPSG_Status() const { return m_Psg; }

            auto CanBeChangedTo(const SStatus& other) const { return m_Psg < other.m_Psg; }

        private:
            static int From(EPSG_Status status);
            static EPSG_Status From(int status);

            int m_Http;
            EPSG_Status m_Psg;
        };

        SPSG_CV<> change;

        SState() : m_InProgress(true), m_Status(EPSG_Status::eSuccess) {}

        SStatus GetStatus() const volatile { return m_Status; }
        SPSG_Message GetMessage(EDiagSev min_severity);
        const volatile atomic_bool& InProgress() const volatile { return m_InProgress; }

        void SetStatus(SStatus status, bool reset) volatile
        {
            SStatus expected = m_Status;
            while (((expected.CanBeChangedTo(status)) || reset) && !m_Status.compare_exchange_weak(expected, status));
        }

        void AddError(string message, SStatus status = EPSG_Status::eError, EDiagSev severity = eDiag_Error, optional<int> code = nullopt)
        {
            AddMessage(std::move(message), severity, code);
            SetStatus(status, false);
        }

        void AddMessage(string message, EDiagSev severity, optional<int> code)
        {
            m_Messages.emplace_back(SPSG_Message{std::move(message), severity, code});
        }

        void SetComplete() volatile { if (m_InProgress.exchange(false)) change.NotifyOne(); }
        void Reset();

    private:
        atomic_bool m_InProgress;
        atomic<SStatus> m_Status;
        deque<SPSG_Message> m_Messages;
    };

    struct SItem
    {
        using TTS = SPSG_CV<SItem>;

        vector<SPSG_Chunk> chunks;
        SPSG_Args args;
        SPSG_Nullable<size_t> expected;
        size_t received = 0;
        SState state;

        void Reset();
    };

    const bool raw;
    SThreadSafe<list<SItem::TTS>> items;
    SThreadSafe<list<SItem::TTS*>> new_items;
    SItem::TTS reply_item;
    SDebugPrintout debug_printout;
    shared_ptr<TPSG_Queue> queue;
    weak_ptr<SPSG_Stats> stats;

    SPSG_Reply(string id, const SPSG_Params& params, shared_ptr<TPSG_Queue> q, weak_ptr<SPSG_Stats> s = weak_ptr<SPSG_Stats>(), bool r = false) :
        raw(r),
        debug_printout(std::move(id), params),
        queue(std::move(q)),
        stats(std::move(s))
    {}

    void SetComplete();
    void SetFailed(string message, SState::SStatus status = EPSG_Status::eError);
    optional<SItem::TTS*> GetNextItem(CDeadline deadline);
    void Reset();
};

struct SPSG_Retries
{
    enum EType { eRetry, eFail };

    SPSG_Retries(const SPSG_Params& params) : SPSG_Retries(make_pair(params.request_retries, params.refused_stream_retries)) {}

    unsigned Get(EType type, bool refused_stream)
    {
        auto& values_pair = type == eRetry ? m_Values.first : m_Values.second;
        auto& values = refused_stream ? values_pair.second : values_pair.first;
        return values ? values-- : 0;
    }

    void Zero() { m_Values = TValues(); }

private:
    using TValuesPair = pair<unsigned, unsigned>;
    using TValues = pair<TValuesPair, TValuesPair>;

    SPSG_Retries(const TValuesPair& values_pair) : m_Values(values_pair, values_pair) {}

    TValues m_Values;
};

struct SPSG_Submitter
{
    using TId = const void*;

    bool CanBe(TId id) const { return !m_Id || (m_Id != id); }
    void Set(TId id) { m_Id = id; }
    void Reset() { m_Id = nullptr; }

private:
    TId m_Id = nullptr;
};

struct SPSG_Processor
{
    using TId = unsigned;

    bool CanBe(TId id) const { return (m_Id == id) || !m_Id; }
    void Set(TId id) { _ASSERT(CanBe(id)); m_Id = id; }

    static auto GetNextId() { return ++sm_NextId; }

private:
    TId m_Id = TId{};
    inline static atomic<TId> sm_NextId;
};

struct SPSG_Request
{
    struct SContext
    {
        SContext(CRef<CRequestContext> context) : m_Context(context) {}
        shared_ptr<void> Set();

    private:
        CRef<CRequestContext> m_Context;
        weak_ptr<void> m_ExistingGuard;
    };

    const string full_path;
    shared_ptr<SPSG_Reply> reply;
    SContext context;
    SPSG_Submitter submitted_by;
    SPSG_Processor processed_by;

    SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c, const SPSG_Params& params);

    enum EStateResult { eContinue, eStop, eRetry };
    EStateResult OnReplyData(SPSG_Processor::TId processor_id, const char* data, size_t len)
    {
        while (len) {
            auto rv = (this->*m_State)(data, len);

            if (rv == eRetry) {
                // Reduce failure counter as well (retry counter is reduced in Retry() before returning eRetry)
                GetRetries(SPSG_Retries::eFail, false);
                Reset();
            }

            if (rv != eContinue) {
                return rv;
            }
        }

        return eContinue;
    }

    auto& OnReplyDone(SPSG_Processor::TId processor_id)
    {
        processed_by.Set(processor_id);
        m_Retries.Zero();
        return reply;
    }

    unsigned GetRetries(SPSG_Retries::EType type, bool refused_stream)
    {
        return m_Retries.Get(type, refused_stream);
    }

    bool Retry(const SUvNgHttp2_Error& error, bool refused_stream = false);
    bool Fail(SPSG_Processor::TId processor_id, const SUvNgHttp2_Error& error, bool refused_stream = false);
    void Reset();

    void ConvertRaw();

private:
    EStateResult StatePrefix(const char*& data, size_t& len);
    EStateResult StateArgs  (const char*& data, size_t& len);
    EStateResult StateData  (const char*& data, size_t& len);
    EStateResult Add();

    enum EUpdateResult { eSuccess, eNewItem, eRetry503 };
    EUpdateResult UpdateItem(SPSG_Args::EItemType item_type, SPSG_Reply::SItem& item, const SPSG_Args& args);

    using TState = EStateResult (SPSG_Request::*)(const char*& data, size_t& len);
    TState m_State;

    struct SBuffer
    {
        size_t prefix_index = 0;
        string args_buffer;
        SPSG_Args args;
        SPSG_Chunk chunk;
        size_t data_to_read = 0;
    };

    SBuffer m_Buffer;
    unordered_map<string, SPSG_Reply::SItem::TTS*> m_ItemsByID;
    SPSG_Retries m_Retries;
};

struct SPSG_TimedRequest
{
    SPSG_TimedRequest(shared_ptr<SPSG_Request> r) : m_Id(SPSG_Processor::GetNextId()), m_Request(std::move(r)) {}

    SPSG_TimedRequest(SPSG_TimedRequest&&) = default;
    SPSG_TimedRequest& operator=(SPSG_TimedRequest&&) = default;

    auto Get()
    {
        _ASSERT(m_Request);
        return make_pair(m_Id, m_Request->processed_by.CanBe(m_Id) ? m_Request : nullptr);
    }

    string_view GetId()
    {
        _ASSERT(m_Request);
        return m_Request->reply->debug_printout.id;
    }

    unsigned AddTime() { return ++m_Time; }
    void ResetTime() { m_Time = 0; }

    template <class TOnRetry, class TOnFail>
    bool CheckExpiration(const SPSG_Params& params, const SUvNgHttp2_Error& error, TOnRetry on_retry, TOnFail on_fail);

private:
    SPSG_Processor::TId m_Id;
    shared_ptr<SPSG_Request> m_Request;
    unsigned m_Time = 0;
};

struct SPSG_AsyncQueue;

using TPSG_AsyncQueues = deque<SPSG_AsyncQueue>;

struct SPSG_AsyncQueuesRef
{
    SPSG_AsyncQueuesRef(TPSG_AsyncQueues& queues) : m_Queues(queues) {}
    void SignalAll();

private:
    TPSG_AsyncQueues& m_Queues;
};

struct SPSG_AsyncQueue : SUv_Async, SPSG_AsyncQueuesRef
{
private:
    static auto s_Pop(list<SPSG_TimedRequest>& queue)
    {
        auto timed_req = std::move(queue.front());
        auto p = timed_req.Get();
        queue.pop_front();
        return tuple_cat(make_tuple(make_optional(std::move(timed_req))), std::move(p));
    }

public:
    using SPSG_AsyncQueuesRef::SPSG_AsyncQueuesRef;

    auto Pop()
    {
        auto locked = m_Queue.GetLock();
        return locked->empty() ? decltype(s_Pop(*locked)){} : s_Pop(*locked);
    }

    template <class... TArgs>
    void Emplace(TArgs&&... args)
    {
        m_Queue.GetLock()->emplace_back(std::forward<TArgs>(args)...);
    }

    auto GetLockedQueue() { return m_Queue.GetLock(); }

    SThreadSafe<list<SPSG_TimedRequest>> m_Queue;
};

inline void SPSG_AsyncQueuesRef::SignalAll() { for (auto& queue : m_Queues) queue.Signal(); }

struct SPSG_ThrottleParams
{
    struct SThreshold
    {
        size_t numerator = 0;
        size_t denominator = 1;
        constexpr static size_t kMaxDenominator = 128;

        SThreshold(string error_rate);
    };

    const volatile uint64_t period;
    TPSG_ThrottleMaxFailures max_failures;
    TPSG_ThrottleUntilDiscovery until_discovery;
    SThreshold threshold;

    SPSG_ThrottleParams();
};

struct SPSG_Throttling
{
    SPSG_Throttling(const SSocketAddress& address, SPSG_ThrottleParams p, uv_loop_t* l);

    bool Active() const { return m_Active != eOff; }
    bool AddSuccess() { return AddResult(true); }
    bool AddFailure() { return AddResult(false); }
    void StartClose();
    void FinishClose();

    void Discovered()
    {
        if (!Configured()) return;

        EThrottling expected = eUntilDiscovery;

        if (m_Active.compare_exchange_strong(expected, eOff)) {
            ERR_POST(Warning << "Disabling throttling for server " << m_Address << " after wait and rediscovery");
        }
    }

private:
    struct SStats
    {
        SPSG_ThrottleParams params;
        unsigned failures = 0;
        pair<bitset<SPSG_ThrottleParams::SThreshold::kMaxDenominator>, size_t> threshold_reg;

        SStats(SPSG_ThrottleParams p) : params(p) {}

        bool Adjust(const SSocketAddress& address, bool result);
        void Reset();
    };
    enum EThrottling { eOff, eOnTimer, eUntilDiscovery };

    uint64_t Configured() const { return m_Stats->params.period; }
    bool AddResult(bool result) { return Configured() && (Active() || Adjust(result)); }
    bool Adjust(bool result);

    static void s_OnSignal(uv_async_t* handle)
    {
        auto that = static_cast<SPSG_Throttling*>(handle->data);
        that->m_Timer.Start();
    }

    static void s_OnTimer(uv_timer_t* handle)
    {
        auto that = static_cast<SPSG_Throttling*>(handle->data);
        auto new_value = that->m_Stats.GetLock()->params.until_discovery ? eUntilDiscovery : eOff;
        that->m_Active.store(new_value);

        if (new_value == eOff) {
            ERR_POST(Warning << "Disabling throttling for server " << that->m_Address << " after wait");
        }
    }

    const SSocketAddress& m_Address;
    SThreadSafe<SStats> m_Stats;
    atomic<EThrottling> m_Active;
    SUv_Timer m_Timer;
    SUv_Async m_Signal;
};

struct SPSG_Server
{
    const SSocketAddress address;
    atomic<double> rate;
    atomic_int available_streams;
    atomic_uint stats;
    SPSG_Throttling throttling;

    SPSG_Server(SSocketAddress a, double r, int as, SPSG_ThrottleParams p, uv_loop_t* l) :
        address(std::move(a)),
        rate(r),
        available_streams(as),
        stats(0),
        throttling(address, std::move(p), l)
    {}
};

template <class TSession>
struct SPSG_Requests : private unordered_map<int32_t, SPSG_TimedRequest>
{
    SPSG_Requests(TSession& session) : m_Session(session) {}

    using unordered_map::begin;
    using unordered_map::end;
    using unordered_map::size;
    using unordered_map::find;

    template <class... TArgs>
    auto emplace(TArgs&&... args)
    {
        m_Session.RemoveStream();
        return unordered_map::emplace(std::forward<TArgs>(args)...);
    }

    auto erase(iterator it)
    {
        m_Session.AddStreams(1);
        return unordered_map::erase(it);
    }

    auto clear()
    {
        m_Session.AddStreams(static_cast<int>(size()));
        return unordered_map::clear();
    }

private:
    TSession& m_Session;
};

struct SPSG_IoSession : SUvNgHttp2_SessionBase
{
    SPSG_Server& server;

    template <class... TNgHttp2Cbs>
    SPSG_IoSession(SPSG_Server& s, const SPSG_Params& params, SPSG_AsyncQueue& queue, uv_loop_t* loop, TNgHttp2Cbs&&... callbacks);

    bool CanProcessRequest(shared_ptr<SPSG_Request>& req) { return req->submitted_by.CanBe(GetInternalId()); }
    bool ProcessRequest(SPSG_TimedRequest timed_req, SPSG_Processor::TId processor_id, shared_ptr<SPSG_Request> req);
    void CheckRequestExpiration();
    bool IsFull() const { return m_Session.GetMaxStreams() <= m_Requests.size(); }

    void RemoveStream()
    {
        if (!--server.available_streams) {
            PSG_IO_TRACE("Server '" << server.address << "' reached request limit");
        }
    }

    void AddStreams(int v)
    {
        if (auto before = server.available_streams.fetch_add(v); (before <= 0) && (before + v > 0) ) {
            PSG_IO_TRACE("Server '" << server.address << "' became available");
            m_Queue.SignalAll();

        } else if (IsFull()) {
            m_Queue.Signal();
        }
    }

protected:
    int OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len);
    int OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code);
    int OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags);

private:
    enum EHeaders { eMethod, eScheme, eAuthority, ePath, eUserAgent, ePeerID, eSessionID, eSubHitID, eCookie, eClientIP, eSize };

    SPSG_Submitter::TId GetInternalId() const { return this; }

    bool Fail(SPSG_Processor::TId processor_id, shared_ptr<SPSG_Request> req, const SUvNgHttp2_Error& error, bool refused_stream = false);

    bool RetryFail(SPSG_Processor::TId processor_id, shared_ptr<SPSG_Request> req, const SUvNgHttp2_Error& error, bool refused_stream = false)
    {
        if (req->Retry(error, refused_stream)) {
            req->Reset();
            m_Queue.Emplace(req);
            m_Queue.Signal();
        }

        return Fail(processor_id, req, error, refused_stream);
    }

    void OnReset(SUvNgHttp2_Error error) override;
    int OnFrameRecv(nghttp2_session *session, const nghttp2_frame *frame);

    static int s_OnFrameRecv(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
    {
        _ASSERT(user_data);
        return static_cast<SPSG_IoSession*>(user_data)->OnFrameRecv(session, frame);
    }

    SPSG_Params m_Params;
    array<SNgHttp2_Header<NGHTTP2_NV_FLAG_NO_COPY_NAME>, eSize> m_Headers;
    SPSG_AsyncQueue& m_Queue;
    SPSG_Requests<SPSG_IoSession> m_Requests;
};

template <class TImpl>
struct SPSG_Thread : public TImpl
{
    template <class... TArgs>
    SPSG_Thread(SUv_Barrier& start_barrier, SUv_Barrier& stop_barrier, uint64_t timeout, uint64_t repeat, TArgs&&... args) :
        TImpl(std::forward<TArgs>(args)...),
        m_Timer(this, s_OnTimer, timeout, repeat),
        m_Thread(s_Execute, this, ref(start_barrier), ref(stop_barrier))
    {}

    ~SPSG_Thread()
    {
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
    }

    void Shutdown()
    {
        m_Shutdown.Signal();
    }

private:
    static void s_OnShutdown(uv_async_t* handle)
    {
        SPSG_Thread* io = static_cast<SPSG_Thread*>(handle->data);
        io->m_Shutdown.Close();
        io->m_Timer.Close();
        io->TImpl::OnShutdown(handle);
    }

    static void s_OnTimer(uv_timer_t* handle)
    {
        SPSG_Thread* io = static_cast<SPSG_Thread*>(handle->data);
        io->TImpl::OnTimer(handle);
    }

    static void s_Execute(SPSG_Thread* io, SUv_Barrier& start_barrier, SUv_Barrier& stop_barrier)
    {
        SUv_Loop loop;

        io->TImpl::OnExecute(loop);
        io->m_Shutdown.Init(io, &loop, s_OnShutdown);
        io->m_Timer.Init(&loop);
        io->m_Timer.Start();

        start_barrier.Wait();

        loop.Run();

        stop_barrier.Wait();

        io->TImpl::AfterExecute();

        loop.Run();
    }

    SUv_Async m_Shutdown;
    SUv_Timer m_Timer;
    thread m_Thread;
};

struct SPSG_Servers : protected deque<SPSG_Server>
{
    using TBase = deque<SPSG_Server>;
    using TTS = SThreadSafe<SPSG_Servers>;

    // Only methods that do not use/change size can be used directly
    using TBase::begin;
    using TBase::end;
    using TBase::operator[];

    atomic_bool fail_requests;

    SPSG_Servers() : fail_requests(false), m_Size(0) {}

    template <class... TArgs>
    void emplace_back(TArgs&&... args)
    {
        TBase::emplace_back(std::forward<TArgs>(args)...);
        ++m_Size;
    }

    size_t size() const volatile { return m_Size; }

private:
    atomic_size_t m_Size;
};

struct SPSG_StatsCounters
{
    enum EGroup : size_t { eRequest, eReplyItem, eSkippedBlob, eReplyItemStatus, eMessage, eRetries };

    void IncCounter(EGroup group, unsigned counter)
    {
        _ASSERT(m_Data.size() > group);
        _ASSERT(m_Data[group].size() > counter);
        ++m_Data[group][counter];
    }

protected:
    SPSG_StatsCounters();

    template <class... TArgs>
    void Report(TArgs&&... args);

private:
    using TData = vector<vector<atomic_uint>>;

    template <EGroup group>
    struct SGroup;

    struct SInit   { template <EGroup group> static void Func(TData& data);                                            };
    struct SReport { template <EGroup group> static void Func(const TData& data, const char* prefix, unsigned report); };

    template <class TWhat, class... TArgs>
    void Apply(EGroup start_with, TArgs&&... args);

    TData m_Data;
};

struct SPSG_StatsAvgTime
{
    enum EAvgTime : size_t { eSentSecondsAgo, eTimeUntilResend };

    void AddTime(EAvgTime avg_time, double value)
    {
        _ASSERT(m_Data.size() > avg_time);
        m_Data[avg_time].first += SecondsToMs(value);
        ++m_Data[avg_time].second;
    }

protected:
    SPSG_StatsAvgTime();

    void Report(const char* prefix, unsigned report);

private:
    static const char* GetName(EAvgTime avg_time);

    vector<pair<atomic_uint64_t, atomic_uint>> m_Data;
};

struct SPSG_StatsData
{
    enum EDataType { eReceived, eRead };

    void AddId(const CPSG_BlobId& blob_id)
    {
        m_Blobs.AddId(blob_id);
    }

    void AddId(const CPSG_ChunkId& chunk_id)
    {
        m_Chunks.AddId(chunk_id);
        m_TSEs.GetLock()->emplace(chunk_id.GetId2Info());
    }

    void AddData(bool has_blob_id, EDataType type, size_t size)
    {
        if (has_blob_id) {
            m_Blobs.AddData(type, size);
        } else {
            m_Chunks.AddData(type, size);
        }
    }

protected:
    void Report(const char* prefix, unsigned report);

private:
    template <class TDataId>
    struct SData
    {
        SData() : m_Received(0), m_Read(0) {}

        void AddId(const TDataId& data_id)
        {
            m_Ids.GetLock()->emplace_back(data_id);
        }

        void AddData(EDataType type, size_t size)
        {
            auto& data = type == eReceived ? m_Received : m_Read;
            data += size;
        }

        void Report(const char* prefix, unsigned report, const char* name);

    private:
        atomic_uint64_t m_Received;
        atomic_uint64_t m_Read;
        SThreadSafe<deque<TDataId>> m_Ids;
    };

    SData<CPSG_BlobId> m_Blobs;
    SData<CPSG_ChunkId> m_Chunks;
    SThreadSafe<unordered_set<string>> m_TSEs;
};

struct SPSG_Stats : SPSG_StatsCounters, SPSG_StatsAvgTime, SPSG_StatsData
{
    SPSG_Stats(SPSG_Servers::TTS& servers);
    ~SPSG_Stats() { Report(); }

    void Init(uv_loop_t* loop) { m_Timer.Init(loop); m_Timer.Start(); }
    void Stop() { m_Timer.Close(); }

private:
    void Report();

    static void s_OnTimer(uv_timer_t* handle)
    {
        auto that = static_cast<SPSG_Stats*>(handle->data);
        that->Report();
    }

    SUv_Timer m_Timer;
    atomic_uint m_Report;
    SPSG_Servers::TTS& m_Servers;
};

struct SPSG_IoImpl
{
    SPSG_IoImpl(const SPSG_Params& params, SPSG_Servers::TTS& servers, SPSG_AsyncQueue& queue) :
        m_Params(params),
        m_Servers(servers),
        m_Queue(queue),
        m_Random(piecewise_construct, {}, forward_as_tuple(random_device()()))
    {}

protected:
    void OnShutdown(uv_async_t* handle);
    void OnTimer(uv_timer_t* handle);
    void OnExecute(uv_loop_t& loop);
    void AfterExecute();

private:
    void CheckForNewServers(uv_async_t* handle)
    {
        if (m_Servers->size() > m_Sessions.size()) {
            AddNewServers(handle);
        }
    }

    void AddNewServers(uv_async_t* handle);
    void OnQueue(uv_async_t* handle);
    void CheckRequestExpiration();
    void FailRequests();

    static void s_OnQueue(uv_async_t* handle)
    {
        SPSG_IoImpl* io = static_cast<SPSG_IoImpl*>(handle->data);
        io->CheckForNewServers(handle);
        io->OnQueue(handle);
    }

    struct SServerSessions
    {
        deque<SUvNgHttp2_Session<SPSG_IoSession>> sessions;
        double current_rate = 0.0;

        SPSG_Server& operator*() { _ASSERT(!sessions.empty()); return sessions.front().server; }
        SPSG_Server* operator->() { return &operator*(); }
    };

    SPSG_Params m_Params;
    SPSG_Servers::TTS& m_Servers;
    SPSG_AsyncQueue& m_Queue;
    deque<SServerSessions> m_Sessions;
    pair<uniform_real_distribution<>, default_random_engine> m_Random;
};

struct SPSG_DiscoveryImpl
{
    SPSG_DiscoveryImpl(CServiceDiscovery service, shared_ptr<SPSG_Stats> stats, const SPSG_Params& params, SPSG_Servers::TTS& servers, SPSG_AsyncQueuesRef queues_ref) :
        m_Params(params),
        m_NoServers(params, servers),
        m_Service(std::move(service)),
        m_Stats(std::move(stats)),
        m_Servers(servers),
        m_QueuesRef(std::move(queues_ref))
    {}

protected:
    void OnShutdown(uv_async_t*);
    void OnTimer(uv_timer_t* handle);
    void OnExecute(uv_loop_t& loop) { if (m_Stats) m_Stats->Init(&loop); }
    void AfterExecute();

private:
    struct SNoServers
    {
        SNoServers(const SPSG_Params& params, SPSG_Servers::TTS& servers);

        bool operator()(bool discovered, SUv_Timer* timer);

    private:
        const uint64_t m_RetryDelay;
        const uint64_t m_Timeout;
        atomic_bool& m_FailRequests;
        uint64_t m_Passed = 0;
    };

    SPSG_Params m_Params;
    SNoServers m_NoServers;
    CServiceDiscovery m_Service;
    shared_ptr<SPSG_Stats> m_Stats;
    SPSG_Servers::TTS& m_Servers;
    SPSG_AsyncQueuesRef m_QueuesRef;
    SPSG_ThrottleParams m_ThrottleParams;
};

struct SPSG_IoCoordinator
{
private:
    SPSG_Servers::TTS m_Servers;

public:
    SPSG_Params params;
    shared_ptr<SPSG_Stats> stats;

    SPSG_IoCoordinator(CServiceDiscovery service);
    ~SPSG_IoCoordinator();
    bool AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool& stopped, const CDeadline& deadline);
    string GetNewRequestId() { return to_string(m_RequestId++); }
    bool RejectsRequests() const { return m_Servers->fail_requests; }

private:
    SUv_Barrier m_StartBarrier;
    SUv_Barrier m_StopBarrier;
    TPSG_AsyncQueues m_Queues;
    vector<unique_ptr<SPSG_Thread<SPSG_IoImpl>>> m_Io;
    SPSG_Thread<SPSG_DiscoveryImpl> m_Discovery;
    atomic<size_t> m_RequestCounter;
    atomic<size_t> m_RequestId;
};

END_NCBI_SCOPE

#endif
#endif
