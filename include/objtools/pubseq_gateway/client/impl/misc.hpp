#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CLIENT__IMPL__MISC__HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CLIENT__IMPL__MISC__HPP

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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <deque>
#include <thread>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_param.hpp>

#include <connect/impl/connect_misc.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#if defined(HAVE_PSG_CLIENT)
BEGIN_NCBI_SCOPE

template <>
struct SThreadSafe<void>
{
protected:
    mutex m_Mutex;
};

template <typename TType = void>
struct SPSG_CV : SThreadSafe<TType>
{
public:
    void NotifyOne() volatile { GetThis().x_NotifyOne(); }
    void NotifyAll() volatile { GetThis().x_NotifyAll(); }

    template <class... TArgs>
    bool WaitUntil(TArgs&&... args) volatile
    {
        return GetThis().x_WaitUntil(std::forward<TArgs>(args)...);
    }

    bool Reset() volatile { return GetThis().x_Reset(); }

private:
    using clock = chrono::system_clock;

    void x_NotifyOne() { x_Signal(); m_CV.notify_one(); }
    void x_NotifyAll() { x_Signal(); m_CV.notify_all(); }

    bool x_WaitUntil(const CDeadline& deadline)
    {
        return deadline.IsInfinite() ? x_Wait() : x_Wait(x_GetTP(deadline));
    }

    template <typename T = bool>
    bool x_WaitUntil(const volatile atomic<T>& a, const CDeadline& deadline, T v = false, bool rv = false)
    {
        constexpr auto kWait = chrono::milliseconds(100);
        const auto until = deadline.IsInfinite() ? clock::time_point::max() : x_GetTP(deadline);

        do {
            const auto max = clock::now() + kWait;

            if (until < max) {
                return x_Wait(until);
            }

            if (x_Wait(max)) {
                return true;
            }
        }
        while (a == v);

        return rv;
    }

    static clock::time_point x_GetTP(const CDeadline& d)
    {
        time_t seconds;
        unsigned int nanoseconds;

        d.GetExpirationTime(&seconds, &nanoseconds);
        const auto ns = chrono::duration_cast<clock::duration>(chrono::nanoseconds(nanoseconds));
        return clock::from_time_t(seconds) + ns;
    }

    template <class... TArgs>
    bool x_Wait(TArgs&&... args)
    {
        unique_lock<mutex> lock(SThreadSafe<TType>::m_Mutex);

        if (!x_CvWait(lock, std::forward<TArgs>(args)...)) return false;

        m_Signal--;
        return true;
    }

    bool x_CvWait(unique_lock<mutex>& l, const clock::time_point& t)
    {
        return m_CV.wait_until(l, t, [&](){ return m_Signal > 0; });
    }

    bool x_CvWait(unique_lock<mutex>& l)
    {
        m_CV.wait(l, [&](){ return m_Signal > 0; });
        return true;
    }

    void x_Signal()
    {
        lock_guard<mutex> lock(SThreadSafe<TType>::m_Mutex);
        m_Signal++;
    }

    bool x_Reset()
    {
        lock_guard<mutex> lock(SThreadSafe<TType>::m_Mutex);
        return exchange(m_Signal, 0);
    }

    SPSG_CV& GetThis() volatile { return const_cast<SPSG_CV&>(*this); }

    condition_variable m_CV;
    int m_Signal = 0;
};

template <class TValue>
struct CPSG_WaitingQueue : SPSG_CV<deque<TValue>>
{
    CPSG_WaitingQueue() : m_Stopped(false) {}

    void Push(TValue value)
    {
        if (m_Stopped) return;

        this->GetLock()->push_back(std::move(value));
        this->NotifyOne();
    }

    bool Pop(TValue& value, const CDeadline& deadline = CDeadline::eInfinite)
    {
        do {
            if (auto locked = this->GetLock()) {
                if (!locked->empty()) {
                    value = std::move(locked->front());
                    locked->pop_front();
                    return true;
                }
            }
        }
        while (this->WaitUntil(m_Stopped, deadline));

        return false;
    }

    enum EStop { eDrain, eClear };
    void Stop(EStop stop)
    {
        m_Stopped.store(true);
        if (stop == eClear) this->GetLock()->clear();
        this->NotifyAll();
    }

    const atomic_bool& Stopped() const { return m_Stopped; }
    bool Empty() const { return m_Stopped && this->GetLock()->empty(); }

private:
    atomic_bool m_Stopped;
};

class CPSG_Misc
{
public:
    static shared_ptr<CPSG_Request> CreateRawRequest(string abs_path_ref, shared_ptr<void> user_context, CRef<CRequestContext> request_context);
    static const CPSG_BlobId* GetRawResponseBlobId(const shared_ptr<CPSG_BlobData>& blob_data);
    static int GetReplyHttpCode(const shared_ptr<CPSG_Reply>& reply);
    static void SetTestIdentity(const string& identity);
};

template <class TParam>
struct SPSG_ParamMin
{
    using TValue = typename TParam::TValueType;

    template <class TDescription>
    static TValue Do(TValue value, const TDescription& desc)
    {
        if (value >= sm_MinValue) return value;

        if (!sm_Warned) {
            ERR_POST(Warning << "[" << desc.section << "] " << desc.name << " ('" << value << "')"
                    " was increased to the minimum allowed value ('" << sm_MinValue << "')");
            sm_Warned = true;
        }

        return sm_MinValue;
    }

    static TValue sm_MinValue;
    inline static bool sm_Warned = false;
};

template <class TParam>
struct SPSG_Param
{
    using TValue = typename TParam::TValueType;

    struct SAdjust
    {
        template <class TDescription>
        static TValue Do(TValue value, const TDescription&) { return value; }
    };

    static TValue GetDefault() { return SAdjust::Do(TParam::GetDefault(), GetParamDescription()); }

    template <typename T>
    static void SetDefault(const T& value)
    {
        // Forbid setting after it's already used
        _ASSERT(!sm_Used);

        TParam::SetDefault(static_cast<TValue>(value));
    }

    static void SetDefault(const string& value)
    {
        SetDefault(TParam::TParamParser::StringToValue(value, GetParamDescription()));
    }

    // Overriding default but only if it's not configured explicitly
    template <typename T>
    static void SetImplicitDefault(const T& value)
    {
        bool sourcing_complete;
        typename TParam::EParamSource param_source;
        TParam::GetDefault();
        TParam::GetState(&sourcing_complete, &param_source);

        if (sourcing_complete && (param_source == TParam::eSource_Default)) {
            SetDefault(value);
        }
    }

protected:
    _DEBUG_ARG(inline static bool sm_Used = false);

private:
    // TDescription is not publicly available in CParam, but it's needed for string to enum conversion.
    // This templated method circumvents that shortcoming.
    template <class TDescription>
    static const auto& GetParamDescription(const CParam<TDescription>*) { return TDescription::sm_ParamDescription; }
    static const auto& GetParamDescription() { return GetParamDescription((TParam*)nullptr); }
};

template <class TParam>
struct SPSG_ParamValue : SPSG_Param<TParam>
{
    using TValue = typename TParam::TValueType;

    // Getting default incurs some performance penalty, so this ctor is explicit
    enum EGetDefault { eGetDefault };
    explicit SPSG_ParamValue(EGetDefault)                     : SPSG_ParamValue(          SPSG_Param<TParam>::GetDefault())  {}
    explicit SPSG_ParamValue(function<TValue(TValue)> adjust) : SPSG_ParamValue(   adjust(SPSG_Param<TParam>::GetDefault())) {}

    operator TValue() const { return m_Value; }
    TValue Get() const { return m_Value; }

private:
    explicit SPSG_ParamValue(TValue value) : m_Value(value) { _DEBUG_ARG(SPSG_Param<TParam>::sm_Used = true); }

    TValue m_Value;
};

#define       PSG_PARAM_TYPE(section, name)      SPSG_Param<NCBI_PARAM_TYPE(section, name)>
#define PSG_PARAM_VALUE_TYPE(section, name) SPSG_ParamValue<NCBI_PARAM_TYPE(section, name)>

#define PSG_PARAM_VALUE_DECL_MIN(type, section, name)                                                                           \
    NCBI_PARAM_DECL(type, section, name);                                                                                       \
    template <>                                                                                                                 \
    struct PSG_PARAM_TYPE(section, name)::SAdjust : SPSG_ParamMin<NCBI_PARAM_TYPE(section, name)> {};                           \
    template <> PSG_PARAM_TYPE(section, name)::TValue SPSG_ParamMin<NCBI_PARAM_TYPE(section, name)>::sm_MinValue

#define PSG_PARAM_VALUE_DEF_MIN(type, section, name, default_value, min_value)                                                  \
    NCBI_PARAM_DEF(type, section, name, default_value);                                                                         \
    template <> PSG_PARAM_TYPE(section, name)::TValue SPSG_ParamMin<NCBI_PARAM_TYPE(section, name)>::sm_MinValue = min_value

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, rd_buf_size);
using TPSG_RdBufSize = PSG_PARAM_TYPE(PSG, rd_buf_size);

PSG_PARAM_VALUE_DECL_MIN(size_t, PSG, wr_buf_size);
using TPSG_WrBufSize = PSG_PARAM_TYPE(PSG, wr_buf_size);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, max_concurrent_streams);
using TPSG_MaxConcurrentStreams = PSG_PARAM_TYPE(PSG, max_concurrent_streams);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, max_concurrent_submits);
using TPSG_MaxConcurrentSubmits = PSG_PARAM_VALUE_TYPE(PSG, max_concurrent_submits);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, max_sessions);
using TPSG_MaxSessions = PSG_PARAM_TYPE(PSG, max_sessions);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, max_concurrent_requests_per_server);
using TPSG_MaxConcurrentRequestsPerServer = PSG_PARAM_VALUE_TYPE(PSG, max_concurrent_requests_per_server);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, num_io);
using TPSG_NumIo = PSG_PARAM_VALUE_TYPE(PSG, num_io);

PSG_PARAM_VALUE_DECL_MIN(unsigned, PSG, reader_timeout);
using TPSG_ReaderTimeout = PSG_PARAM_TYPE(PSG, reader_timeout);

PSG_PARAM_VALUE_DECL_MIN(double, PSG, rebalance_time);
using TPSG_RebalanceTime = PSG_PARAM_TYPE(PSG, rebalance_time);

PSG_PARAM_VALUE_DECL_MIN(double, PSG, io_timer_period);
using TPSG_IoTimerPeriod = PSG_PARAM_VALUE_TYPE(PSG, io_timer_period);

NCBI_PARAM_DECL(double, PSG, request_timeout);
typedef NCBI_PARAM_TYPE(PSG, request_timeout) TPSG_RequestTimeout;

NCBI_PARAM_DECL(double, PSG, competitive_after);
typedef NCBI_PARAM_TYPE(PSG, competitive_after) TPSG_CompetitiveAfter;

PSG_PARAM_VALUE_DECL_MIN(size_t, PSG, requests_per_io);
using TPSG_RequestsPerIo = PSG_PARAM_VALUE_TYPE(PSG, requests_per_io);

NCBI_PARAM_DECL(unsigned, PSG, request_retries);
using TPSG_RequestRetries = PSG_PARAM_VALUE_TYPE(PSG, request_retries);

NCBI_PARAM_DECL(unsigned, PSG, refused_stream_retries);
using TPSG_RefusedStreamRetries = PSG_PARAM_VALUE_TYPE(PSG, refused_stream_retries);

NCBI_PARAM_DECL(string, PSG, request_user_args);
typedef NCBI_PARAM_TYPE(PSG, request_user_args) TPSG_RequestUserArgs;

NCBI_PARAM_DECL(string, PSG, multivalued_user_args);
typedef NCBI_PARAM_TYPE(PSG, multivalued_user_args) TPSG_MultivaluedUserArgs;

NCBI_PARAM_DECL(bool, PSG, user_request_ids);
using TPSG_UserRequestIds = PSG_PARAM_VALUE_TYPE(PSG, user_request_ids);

NCBI_PARAM_DECL(unsigned, PSG, localhost_preference);
typedef NCBI_PARAM_TYPE(PSG, localhost_preference) TPSG_LocalhostPreference;

NCBI_PARAM_DECL(bool, PSG, fail_on_unknown_items);
typedef NCBI_PARAM_TYPE(PSG, fail_on_unknown_items) TPSG_FailOnUnknownItems;

NCBI_PARAM_DECL(bool, PSG, fail_on_unknown_chunks);
typedef NCBI_PARAM_TYPE(PSG, fail_on_unknown_chunks) TPSG_FailOnUnknownChunks;

NCBI_PARAM_DECL(bool, PSG, https);
typedef NCBI_PARAM_TYPE(PSG, https) TPSG_Https;

NCBI_PARAM_DECL(double, PSG, no_servers_retry_delay);
typedef NCBI_PARAM_TYPE(PSG, no_servers_retry_delay) TPSG_NoServersRetryDelay;

NCBI_PARAM_DECL(string, PSG, service);
using TPSG_Service = NCBI_PARAM_TYPE(PSG, service);

NCBI_PARAM_DECL(string, PSG, auth_token_name);
using TPSG_AuthTokenName = PSG_PARAM_VALUE_TYPE(PSG, auth_token_name);

NCBI_PARAM_DECL(string, PSG, auth_token);
using TPSG_AuthToken = PSG_PARAM_VALUE_TYPE(PSG, auth_token);

NCBI_PARAM_DECL(string, PSG, admin_auth_token_name);
using TPSG_AdminAuthTokenName = PSG_PARAM_VALUE_TYPE(PSG, admin_auth_token_name);

NCBI_PARAM_DECL(string, PSG, admin_auth_token);
using TPSG_AdminAuthToken = PSG_PARAM_VALUE_TYPE(PSG, admin_auth_token);

NCBI_PARAM_DECL(bool, PSG, stats);
typedef NCBI_PARAM_TYPE(PSG, stats) TPSG_Stats;

NCBI_PARAM_DECL(double, PSG, stats_period);
typedef NCBI_PARAM_TYPE(PSG, stats_period) TPSG_StatsPeriod;

NCBI_PARAM_DECL(double, PSG, throttle_relaxation_period);
using TPSG_ThrottlePeriod = NCBI_PARAM_TYPE(PSG, throttle_relaxation_period);

NCBI_PARAM_DECL(unsigned, PSG, throttle_by_consecutive_connection_failures);
using TPSG_ThrottleMaxFailures = PSG_PARAM_VALUE_TYPE(PSG, throttle_by_consecutive_connection_failures);

NCBI_PARAM_DECL(bool, PSG, throttle_hold_until_active_in_lb);
using TPSG_ThrottleUntilDiscovery = PSG_PARAM_VALUE_TYPE(PSG, throttle_hold_until_active_in_lb);

NCBI_PARAM_DECL(string, PSG, throttle_by_connection_error_rate);
using TPSG_ThrottleThreshold = NCBI_PARAM_TYPE(PSG, throttle_by_connection_error_rate);

enum class EPSG_DebugPrintout { eNone, eSome, eFrames, eAll };
NCBI_PARAM_ENUM_DECL(EPSG_DebugPrintout, PSG, debug_printout);
using TPSG_DebugPrintout = PSG_PARAM_VALUE_TYPE(PSG, debug_printout);

enum class EPSG_UseCache { eDefault, eNo, eYes };
NCBI_PARAM_ENUM_DECL(EPSG_UseCache, PSG, use_cache);
using TPSG_UseCache = PSG_PARAM_VALUE_TYPE(PSG, use_cache);

// Performance reporting/request IDs for psg_client app
enum class EPSG_PsgClientMode { eOff, ePerformance };
NCBI_PARAM_ENUM_DECL(EPSG_PsgClientMode, PSG, internal_psg_client_mode);
using TPSG_PsgClientMode = PSG_PARAM_VALUE_TYPE(PSG, internal_psg_client_mode);

END_NCBI_SCOPE
#endif

#endif
