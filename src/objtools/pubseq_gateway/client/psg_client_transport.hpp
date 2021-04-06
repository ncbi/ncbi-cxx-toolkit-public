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

#include "mpmc_nw.hpp"
#include <connect/impl/ncbi_uv_nghttp2.hpp>
#include <connect/services/netservice_api.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_url.hpp>


BEGIN_NCBI_SCOPE

// Different TRACE macros allow turning off tracing in some classes
#define PSG_THROTTLING_TRACE(message)      _TRACE(message)
#define PSG_IO_SESSION_TRACE(message)      _TRACE(message)
#define PSG_IO_TRACE(message)              _TRACE(message)
#define PSG_DISCOVERY_TRACE(message)       _TRACE(message)

struct SPSG_Args : CUrlArgs
{
    using CUrlArgs::CUrlArgs;
    using CUrlArgs::operator=;

    const string& GetValue(const string& name) const
    {
        bool not_used;
        return CUrlArgs::GetValue(name, &not_used);
    }
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

struct SPSG_Params
{
    TPSG_DebugPrintout debug_printout;
    TPSG_RequestsPerIo requests_per_io;
    TPSG_RequestRetries request_retries;
    TPSG_PsgClientMode client_mode;

    SPSG_Params() :
        debug_printout(TPSG_DebugPrintout::eGetDefault),
        requests_per_io(TPSG_RequestsPerIo::eGetDefault),
        request_retries(TPSG_RequestRetries::eGetDefault),
        client_mode(TPSG_PsgClientMode::eGetDefault)
    {}
};

struct SDebugPrintout
{
    const string id;

    SDebugPrintout(string i, const SPSG_Params& params) :
        id(move(i)),
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
        if (IsPerf()) return Event(forward<TArgs>(args)...);

        if (m_Params.debug_printout == EPSG_DebugPrintout::eNone) return;

        Print(forward<TArgs>(args)...);
    }

    enum EType { eSend = 1000, eReceive, eClose, eRetry, eFail };

    void Event(SSocketAddress, const string&)       { Event(eSend);    }
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

    void Print(SSocketAddress address, const string& path);
    void Print(const SPSG_Args& args, const SPSG_Chunk& chunk);
    void Print(uint32_t error_code);
    void Print(unsigned retries, const SUvNgHttp2_Error& error);
    void Print(const SUvNgHttp2_Error& error);

    bool IsPerf() const
    {
        return (m_Params.client_mode == EPSG_PsgClientMode::ePerformance) ||
            (m_Params.client_mode == EPSG_PsgClientMode::eIo);
    }

    SPSG_Params m_Params;
    vector<tuple<double, EType, thread::id>> m_Events;
};

template <class TArg, class ...TRest>
struct SDebugPrintout::SPack : SPack<TRest...>
{
    SPack(SPack<TRest...>&& base, const TArg* arg) :
        SPack<TRest...>(move(base)),
        m_Arg(arg)
    {}

    template <class TNextArg>
    SPack<TNextArg, TArg, TRest...> operator<<(const TNextArg& next_arg)
    {
        return { move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs&&... args)
    {
        SPack<TRest...>::Process(*m_Arg, forward<TArgs>(args)...);
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
        return { move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs&&... args)
    {
        m_DebugPrintout->Process(*m_Arg, forward<TArgs>(args)...);
    }

private:
    SDebugPrintout* m_DebugPrintout;
    const TArg* m_Arg;
};

struct SPSG_Reply
{
    struct SState
    {
        enum EState {
            eInProgress,
            eSuccess,
            eNotFound,
            eError,
        };

        SPSG_CV<0> change;

        SState() : m_State(eInProgress), m_Returned(false), m_Empty(true) {}

        const volatile atomic<EState>& GetState() const volatile { return m_State; }
        string GetError();

        bool InProgress() const volatile { return m_State == eInProgress; }
        bool Returned() const volatile { return m_Returned; }
        bool Empty() const volatile { return m_Empty; }

        void SetState(EState state) volatile
        {
            auto expected = eInProgress;

            if (m_State.compare_exchange_strong(expected, state)) {
                change.NotifyOne();
            }
        }

        void AddError(string message, EState new_state = eError);
        void SetReturned() volatile { bool expected = false; m_Returned.compare_exchange_strong(expected, true); }
        void SetNotEmpty() volatile { bool expected = true; m_Empty.compare_exchange_strong(expected, false); }

    private:
        atomic<EState> m_State;
        atomic_bool m_Returned;
        atomic_bool m_Empty;
        vector<string> m_Messages;
    };

    struct SItem
    {
        using TTS = SPSG_CV<0, SThreadSafe<SItem>>;

        vector<SPSG_Chunk> chunks;
        SPSG_Args args;
        SPSG_Nullable<size_t> expected;
        size_t received = 0;
        SState state;

        void SetSuccess();
    };

    SThreadSafe<list<SItem::TTS>> items;
    SItem::TTS reply_item;
    SDebugPrintout debug_printout;

    SPSG_Reply(string id, const SPSG_Params& params) : debug_printout(move(id), params) {}
    void SetSuccess();
    void AddError(string message);
};

struct SPSG_Request
{
    const string full_path;
    shared_ptr<SPSG_Reply> reply;
    CRef<CRequestContext> context;

    SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c, const SPSG_Params& params);

    bool OnReplyData(const char* data, size_t len)
    {
        while (len) {
            if (!(this->*m_State)(data, len)) {
                return false;
            }
        }

        return true;
    }

    unsigned GetRetries()
    {
        return reply->reply_item.GetMTSafe().state.InProgress() && (m_Retries > 0) ? m_Retries-- : 0;
    }

private:
    bool StatePrefix(const char*& data, size_t& len);
    bool StateArgs(const char*& data, size_t& len);
    bool StateData(const char*& data, size_t& len);
    bool StateIo(const char*& data, size_t& len) { data += len; len = 0; return true; }

    void SetStatePrefix()  { Add(); m_State = &SPSG_Request::StatePrefix; }
    void SetStateArgs()           { m_State = &SPSG_Request::StateArgs;   }
    void SetStateData(size_t dtr) { m_State = &SPSG_Request::StateData;   m_Buffer.data_to_read = dtr; }

    void Add();
    void AddIo();

    using TState = bool (SPSG_Request::*)(const char*& data, size_t& len);
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
    unsigned m_Retries;
};

struct SPSG_TimedRequest
{
    SPSG_TimedRequest(shared_ptr<SPSG_Request> r) : m_Request(move(r)) {}

    shared_ptr<SPSG_Request> operator->() { m_Seconds = 0; return m_Request; }
    operator shared_ptr<SPSG_Request>()   { m_Seconds = 0; return m_Request; }
    unsigned AddSecond() { return m_Seconds++; }

private:
    shared_ptr<SPSG_Request> m_Request;
    unsigned m_Seconds = 0;
};

struct SPSG_AsyncQueue : SUv_Async
{
    using TRequest = shared_ptr<SPSG_Request>;

    bool Pop(TRequest& request)
    {
        return m_Queue.PopMove(request);
    }

    bool Push(TRequest&& request)
    {
        if (m_Queue.PushMove(request)) {
            Signal();
            return true;
        } else {
            return false;
        }
    }

    using SUv_Async::Signal;

private:
    CMPMCQueue<TRequest> m_Queue;
};

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

    void Discovered()
    {
        if (!Configured()) return;

        EThrottling expected = eUntilDiscovery;

        if (m_Active.compare_exchange_strong(expected, eOff)) {
            ERR_POST(Warning << "Disabling throttling for server " << m_Address.AsString() << " after wait and rediscovery");
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

    uint64_t Configured() const { return m_Stats.GetMTSafe().params.period; }
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
            ERR_POST(Warning << "Disabling throttling for server " << that->m_Address.AsString() << " after wait");
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
    SPSG_Throttling throttling;

    SPSG_Server(SSocketAddress a, double r, SPSG_ThrottleParams p, uv_loop_t* l) :
        address(move(a)),
        rate(r),
        throttling(address, move(p), l)
    {}
};

struct SPSG_IoSession : SUvNgHttp2_SessionBase
{
    SPSG_Server& server;

    template <class... TNgHttp2Cbs>
    SPSG_IoSession(SPSG_Server& s, SPSG_AsyncQueue& queue, uv_loop_t* loop, TNgHttp2Cbs&&... callbacks);

    bool ProcessRequest(shared_ptr<SPSG_Request>& req);
    void CheckRequestExpiration();
    bool IsFull() const { return m_Session.GetMaxStreams() <= m_Requests.size(); }

protected:
    int OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len);
    int OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code);
    int OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags);
    int OnError(nghttp2_session*, const char* msg, size_t) { Reset(msg); return 0; }

private:
    enum EHeaders { eMethod, eScheme, eAuthority, ePath, eUserAgent, eSessionID, eSubHitID, eClientIP, eSize };

    using TRequests = unordered_map<int32_t, SPSG_TimedRequest>;

    bool Retry(shared_ptr<SPSG_Request> req, const SUvNgHttp2_Error& error);
    void RequestComplete(TRequests::iterator& it);

    void OnReset(SUvNgHttp2_Error error) override;

    array<SNgHttp2_Header<NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE>, eSize> m_Headers;
    const TPSG_RequestTimeout m_RequestTimeout;
    SPSG_AsyncQueue& m_Queue;
    TRequests m_Requests;
};

template <class TImpl>
struct SPSG_Thread : public TImpl
{
    template <class... TArgs>
    SPSG_Thread(SUv_Barrier& barrier, uint64_t timeout, uint64_t repeat, TArgs&&... args) :
        TImpl(forward<TArgs>(args)...),
        m_Timer(this, s_OnTimer, timeout, repeat),
        m_Thread(s_Execute, this, ref(barrier))
    {}

    ~SPSG_Thread()
    {
        if (m_Thread.joinable()) {
            m_Shutdown.Signal();
            m_Thread.join();
        }
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

    static void s_Execute(SPSG_Thread* io, SUv_Barrier& barrier)
    {
        SUv_Loop loop;

        io->TImpl::OnExecute(loop);
        io->m_Shutdown.Init(io, &loop, s_OnShutdown);
        io->m_Timer.Init(&loop);
        io->m_Timer.Start();

        barrier.Wait();

        loop.Run();

        io->TImpl::AfterExecute();
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
        TBase::emplace_back(forward<TArgs>(args)...);
        ++m_Size;
    }

    size_t size() const volatile { return m_Size; }

private:
    atomic_size_t m_Size;
};

struct SPSG_IoImpl
{
    using TSpaceCV = SPSG_CV<1000>;

    SPSG_AsyncQueue queue;
    TSpaceCV* space;

    SPSG_IoImpl(TSpaceCV* s, SPSG_Servers::TTS& servers) :
        space(s),
        m_Servers(servers),
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
        const auto servers_size = m_Servers.GetMTSafe().size();
        const auto sessions_size = m_Sessions.size();

        if (servers_size > sessions_size) {
            AddNewServers(servers_size, sessions_size, handle);
        }
    }

    void AddNewServers(size_t servers_size, size_t sessions_size, uv_async_t* handle);
    void OnQueue(uv_async_t* handle);

    static void s_OnQueue(uv_async_t* handle)
    {
        SPSG_IoImpl* io = static_cast<SPSG_IoImpl*>(handle->data);
        io->CheckForNewServers(handle);
        io->OnQueue(handle);
    }

    SPSG_Servers::TTS& m_Servers;
    deque<pair<SUvNgHttp2_Session<SPSG_IoSession>, double>> m_Sessions;
    pair<uniform_real_distribution<>, default_random_engine> m_Random;
};

struct SPSG_DiscoveryImpl
{
    SPSG_DiscoveryImpl(CServiceDiscovery service, SPSG_Servers::TTS& servers) :
        m_NoServers(servers),
        m_Service(move(service)),
        m_Servers(servers)
    {}

protected:
    void OnShutdown(uv_async_t*);
    void OnTimer(uv_timer_t* handle);
    void OnExecute(uv_loop_t&) {}
    void AfterExecute() {}

private:
    struct SNoServers
    {
        SNoServers(SPSG_Servers::TTS& servers);

        bool operator()(bool discovered, SUv_Timer* timer);

    private:
        const uint64_t m_RetryDelay;
        const uint64_t m_Timeout;
        atomic_bool& m_FailRequests;
        uint64_t m_Passed = 0;
    };

    SNoServers m_NoServers;
    CServiceDiscovery m_Service;
    SPSG_Servers::TTS& m_Servers;
    SPSG_ThrottleParams m_ThrottleParams;
};

struct SPSG_IoCoordinator
{
    SPSG_Params params;

    SPSG_IoCoordinator(CServiceDiscovery service);
    bool AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool& stopped, const CDeadline& deadline);
    string GetNewRequestId() { return to_string(m_RequestId++); }
    const string& GetUrlArgs() const { return m_UrlArgs; }

private:
    SUv_Barrier m_Barrier;
    SPSG_IoImpl::TSpaceCV m_Space;
    SPSG_Servers::TTS m_Servers;
    SPSG_Thread<SPSG_DiscoveryImpl> m_Discovery;
    vector<unique_ptr<SPSG_Thread<SPSG_IoImpl>>> m_Io;
    atomic<size_t> m_RequestCounter;
    atomic<size_t> m_RequestId;
    const string m_UrlArgs;
};

END_NCBI_SCOPE

#endif
#endif
