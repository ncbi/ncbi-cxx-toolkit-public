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

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#include <memory>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <cassert>
#include <exception>
#include <thread>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <functional>
#include <numeric>
#include <cmath>

#define __STDC_FORMAT_MACROS

#include <corelib/request_status.hpp>

#include "psg_client_transport.hpp"

BEGIN_NCBI_SCOPE

PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, rd_buf_size,                   64 * 1024,          1024    );
PSG_PARAM_VALUE_DEF_MIN(size_t,         PSG, wr_buf_size,                   64 * 1024,          1024    );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, max_concurrent_streams,        100,                10      );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, max_concurrent_submits,        150,                1       );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, max_sessions,                  1,                  1       );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, max_concurrent_requests_per_server, 500,           100     );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, num_io,                        1,                  1       );
PSG_PARAM_VALUE_DEF_MIN(unsigned,       PSG, reader_timeout,                12,                 1       );
PSG_PARAM_VALUE_DEF_MIN(double,         PSG, rebalance_time,                10.0,               1.0     );
PSG_PARAM_VALUE_DEF_MIN(size_t,         PSG, requests_per_io,               1,                  1       );
PSG_PARAM_VALUE_DEF_MIN(double,         PSG, io_timer_period,               1.0,                0.1     );
NCBI_PARAM_DEF(double,   PSG, request_timeout,        10.0);
NCBI_PARAM_DEF(double,   PSG, competitive_after,      0.0);
NCBI_PARAM_DEF(unsigned, PSG, request_retries,        2);
NCBI_PARAM_DEF(unsigned, PSG, refused_stream_retries, 2);
NCBI_PARAM_DEF(string,   PSG, request_user_args,      "");
NCBI_PARAM_DEF(string,   PSG, multivalued_user_args,  "");
NCBI_PARAM_DEF(bool,     PSG, user_request_ids,       false);
NCBI_PARAM_DEF(unsigned, PSG, localhost_preference,   1);
NCBI_PARAM_DEF(bool,     PSG, fail_on_unknown_items,  false);
NCBI_PARAM_DEF(bool,     PSG, fail_on_unknown_chunks, false);
NCBI_PARAM_DEF(bool,     PSG, https,                  false);
NCBI_PARAM_DEF(double,   PSG, no_servers_retry_delay, 1.0);
NCBI_PARAM_DEF(bool,     PSG, stats,                  false);
NCBI_PARAM_DEF(double,   PSG, stats_period,           0.0);
NCBI_PARAM_DEF_EX(string,   PSG, service,               "PSG2",             eParam_Default,     NCBI_PSG_SERVICE);
NCBI_PARAM_DEF_EX(string,   PSG, auth_token_name,       "WebCubbyUser",     eParam_Default,     NCBI_PSG_AUTH_TOKEN_NAME);
NCBI_PARAM_DEF_EX(string,   PSG, auth_token,            "",                 eParam_Default,     NCBI_PSG_AUTH_TOKEN);
NCBI_PARAM_DEF_EX(string,   PSG, admin_auth_token_name, "AdminAuthToken",   eParam_Default,     NCBI_PSG_ADMIN_AUTH_TOKEN_NAME);
NCBI_PARAM_DEF_EX(string,   PSG, admin_auth_token,      "",                 eParam_Default,     NCBI_PSG_ADMIN_AUTH_TOKEN);

NCBI_PARAM_DEF(double,   PSG, throttle_relaxation_period,                  0.0);
NCBI_PARAM_DEF(unsigned, PSG, throttle_by_consecutive_connection_failures, 0);
NCBI_PARAM_DEF(bool,     PSG, throttle_hold_until_active_in_lb,            false);
NCBI_PARAM_DEF(string,   PSG, throttle_by_connection_error_rate,           "");

NCBI_PARAM_ENUM_ARRAY(EPSG_DebugPrintout, PSG, debug_printout)
{
    { "none", EPSG_DebugPrintout::eNone },
    { "some", EPSG_DebugPrintout::eSome },
    { "frames", EPSG_DebugPrintout::eFrames },
    { "all",  EPSG_DebugPrintout::eAll  }
};
NCBI_PARAM_ENUM_DEF(EPSG_DebugPrintout, PSG, debug_printout, EPSG_DebugPrintout::eNone);

NCBI_PARAM_ENUM_ARRAY(EPSG_UseCache, PSG, use_cache)
{
    { "default", EPSG_UseCache::eDefault },
    { "no",      EPSG_UseCache::eNo      },
    { "yes",     EPSG_UseCache::eYes     }
};
NCBI_PARAM_ENUM_DEF(EPSG_UseCache, PSG, use_cache, EPSG_UseCache::eDefault);

// Performance reporting/request IDs for psg_client app
NCBI_PARAM_ENUM_ARRAY(EPSG_PsgClientMode, PSG, internal_psg_client_mode)
{
    { "off",         EPSG_PsgClientMode::eOff         },
    { "performance", EPSG_PsgClientMode::ePerformance },
};
NCBI_PARAM_ENUM_DEF(EPSG_PsgClientMode, PSG, internal_psg_client_mode, EPSG_PsgClientMode::eOff);

SPSG_ArgsBase::SArg<SPSG_ArgsBase::eItemType>::TType SPSG_ArgsBase::SArg<SPSG_ArgsBase::eItemType>::Get(const string& value)
{
    if (value == "bioseq_info")     return { SPSG_ArgsBase::eBioseqInfo,     cref(value) };
    if (value == "blob_prop")       return { SPSG_ArgsBase::eBlobProp,       cref(value) };
    if (value == "blob")            return { SPSG_ArgsBase::eBlob,           cref(value) };
    if (value == "reply")           return { SPSG_ArgsBase::eReply,          cref(value) };
    if (value == "bioseq_na")       return { SPSG_ArgsBase::eBioseqNa,       cref(value) };
    if (value == "na_status")       return { SPSG_ArgsBase::eNaStatus,       cref(value) };
    if (value == "public_comment")  return { SPSG_ArgsBase::ePublicComment,  cref(value) };
    if (value == "processor")       return { SPSG_ArgsBase::eProcessor,      cref(value) };
    if (value == "ipg_info")        return { SPSG_ArgsBase::eIpgInfo,        cref(value) };
    if (value == "acc_ver_history") return { SPSG_ArgsBase::eAccVerHistory,  cref(value) };
    if (value.empty())              return { SPSG_ArgsBase::eReply,          cref(value) };
    return { SPSG_ArgsBase::eUnknownItem, cref(value) };
};

SPSG_ArgsBase::SArg<SPSG_ArgsBase::eChunkType>::TType SPSG_ArgsBase::SArg<SPSG_ArgsBase::eChunkType>::Get(const string& value)
{
    if (value == "meta")              return { SPSG_ArgsBase::eMeta,            cref(value) };
    if (value == "data")              return { SPSG_ArgsBase::eData,            cref(value) };
    if (value == "message")           return { SPSG_ArgsBase::eMessage,         cref(value) };
    if (value == "data_and_meta")     return { SPSG_ArgsBase::eDataAndMeta,     cref(value) };
    if (value == "message_and_meta")  return { SPSG_ArgsBase::eMessageAndMeta,  cref(value) };
    return { SPSG_ArgsBase::eUnknownChunk, cref(value) };
};

string SPSG_Env::GetCookie(const string& name) const
{
    if (!m_Cookies) {
        m_Cookies.emplace();
        m_Cookies->Add(CHttpCookies::eHeader_Cookie, Get("HTTP_COOKIE"), nullptr);
    }

    for (const auto& cookie : *m_Cookies) {
        if (cookie.GetName() == name) {
            return NStr::URLDecode(cookie.GetValue());
        }
    }

    return {};
}

string SPSG_Params::GetCookie(function<string()> get_auth_token)
{
    auto combine = [](auto p, auto v) { return v.empty()? v : p.Get() + '=' + NStr::URLEncode(v); };

    auto admin_cookie = combine(admin_auth_token_name, admin_auth_token.Get());
    auto hup_cookie = combine(auth_token_name, auth_token.Get().empty() ? get_auth_token() : auth_token.Get());

    return admin_cookie.empty() ? hup_cookie : hup_cookie.empty() ? admin_cookie : admin_cookie + "; " + hup_cookie;
}

unsigned SPSG_Params::s_GetRequestTimeout(double io_timer_period)
{
    auto value = TPSG_RequestTimeout::GetDefault();

    if (value < io_timer_period) {
        ERR_POST(Warning << "[PSG] request_timeout ('" << value << "')"
                " was increased to the minimum allowed value ('" << io_timer_period << "')");
        value = io_timer_period;
    }

    return static_cast<unsigned>(value / io_timer_period);
}

unsigned SPSG_Params::s_GetCompetitiveAfter(double io_timer_period, double timeout)
{
    auto value = TPSG_CompetitiveAfter::GetDefault();
    timeout *= io_timer_period;

    if ((value > 0.0) && (value < io_timer_period)) {
        ERR_POST(Warning << "[PSG] competitive_after ('" << value << "')"
                " was increased to the minimum allowed value ('" << io_timer_period << "')");
        value = io_timer_period;
    }

    if (value >= timeout) {
        ERR_POST(Warning << "[PSG] competitive_after ('" << value << "') was disabled, "
                "as it was greater or equal to request timeout ('" << timeout << "')");
    } else if (value > 0.0) {
        timeout = value;
    }

    return static_cast<unsigned>(timeout / io_timer_period);
}

void SDebugPrintout::Print(SSocketAddress address, const string& path, const string& sid, const string& phid, const string& ip, SUv_Tcp::TPort port)
{
    ostringstream os;

    if (!ip.empty()) os << ";IP=" << ip;
    if (port)        os << ";PORT=" << port;
    if (m_Params.proxy) os << ";PROXY=" << m_Params.proxy;

    ERR_POST(Message << id << ": " << address << path << ";SID=" << sid << ";PHID=" << phid << os.str());
}

void SDebugPrintout::Print(const SPSG_Args& args, const SPSG_Chunk& chunk)
{
    ostringstream os;

    os << args.GetQueryString(CUrlArgs::eAmp_Char) << '\n';

    if ((m_Params.debug_printout == EPSG_DebugPrintout::eAll) ||
            (args.GetValue<SPSG_Args::eItemType>().first != SPSG_Args::eBlob) || (args.GetValue<SPSG_Args::eChunkType>().first != SPSG_Args::eData)) {
        os << chunk;
    } else {
        os << "<BINARY DATA OF " << chunk.size() << " BYTES>";
    }

    ERR_POST(Message << id << ": " << NStr::PrintableString(os.str()));
}

void SDebugPrintout::Print(uint32_t error_code)
{
    ERR_POST(Message << id << ": Closed with status " << SUvNgHttp2_Error::NgHttp2Str(error_code));
}

void SDebugPrintout::Print(unsigned retries, const SUvNgHttp2_Error& error)
{
    ERR_POST(Message << id << ": Retrying (" << retries << " retries remaining) after " << error);
}

void SDebugPrintout::Print(const SUvNgHttp2_Error& error)
{
    ERR_POST(Message << id << ": Gave up after " << error);
}

SDebugPrintout::~SDebugPrintout()
{
    if (IsPerf()) {
        ostringstream os;

        for (const auto& event : m_Events) {
            auto ms = get<0>(event);
            auto type = get<1>(event);
            auto thread_id = get<2>(event);
            os << fixed << id << '\t' << ms << '\t' << type << '\t' << thread_id << '\n';
        }

        cout << os.str() << flush;
    }
}

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eRequest>
{
    using type = CPSG_Request::EType;
    static constexpr size_t size = CPSG_Request::eChunk + 1;
    static constexpr auto prefix = "\trequest\ttype=";

    static constexpr array<type, size> values = {
        CPSG_Request::eBiodata,
        CPSG_Request::eResolve,
        CPSG_Request::eBlob,
        CPSG_Request::eNamedAnnotInfo,
        CPSG_Request::eChunk,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case CPSG_Request::eBiodata:            return "biodata";
            case CPSG_Request::eResolve:            return "resolve";
            case CPSG_Request::eBlob:               return "blob";
            case CPSG_Request::eNamedAnnotInfo:     return "named_annot_info";
            case CPSG_Request::eChunk:              return "chunk";
            case CPSG_Request::eIpgResolve:         return "ipg_resolve";
            case CPSG_Request::eAccVerHistory:      return "acc_ver_history";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eReplyItem>
{
    using type = CPSG_ReplyItem::EType;
    static constexpr size_t size = CPSG_ReplyItem::eEndOfReply + 1;
    static constexpr auto prefix = "\treply_item\ttype=";

    static constexpr array<type, size> values = {
        CPSG_ReplyItem::eBlobData,
        CPSG_ReplyItem::eBlobInfo,
        CPSG_ReplyItem::eSkippedBlob,
        CPSG_ReplyItem::eBioseqInfo,
        CPSG_ReplyItem::eNamedAnnotInfo,
        CPSG_ReplyItem::eNamedAnnotStatus,
        CPSG_ReplyItem::ePublicComment,
        CPSG_ReplyItem::eProcessor,
        CPSG_ReplyItem::eIpgInfo,
        CPSG_ReplyItem::eAccVerHistory,
        CPSG_ReplyItem::eEndOfReply,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case CPSG_ReplyItem::eBlobData:             return "blob_data";
            case CPSG_ReplyItem::eBlobInfo:             return "blob_info";
            case CPSG_ReplyItem::eSkippedBlob:          return "skipped_blob";
            case CPSG_ReplyItem::eBioseqInfo:           return "bioseq_info";
            case CPSG_ReplyItem::eNamedAnnotInfo:       return "named_annot_info";
            case CPSG_ReplyItem::eNamedAnnotStatus:     return "named_annot_status";
            case CPSG_ReplyItem::ePublicComment:        return "public_comment";
            case CPSG_ReplyItem::eProcessor:            return "processor";
            case CPSG_ReplyItem::eIpgInfo:              return "ipg_info";
            case CPSG_ReplyItem::eAccVerHistory:        return "acc_ver_history";
            case CPSG_ReplyItem::eEndOfReply:           return "end_of_reply";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eSkippedBlob>
{
    using type = CPSG_SkippedBlob::EReason;
    static constexpr size_t size = CPSG_SkippedBlob::eUnknown + 1;
    static constexpr auto prefix = "\tskipped_blob\treason=";

    static constexpr array<type, size> values = {
        CPSG_SkippedBlob::eExcluded,
        CPSG_SkippedBlob::eInProgress,
        CPSG_SkippedBlob::eSent,
        CPSG_SkippedBlob::eUnknown,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case CPSG_SkippedBlob::eExcluded:       return "excluded";
            case CPSG_SkippedBlob::eInProgress:     return "in_progress";
            case CPSG_SkippedBlob::eSent:           return "sent";
            case CPSG_SkippedBlob::eUnknown:        return "unknown";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eReplyItemStatus>
{
    using type = EPSG_Status;
    static constexpr size_t size = static_cast<size_t>(EPSG_Status::eError) + 1;
    static constexpr auto prefix = "\treply_item_status\tstatus=";

    static constexpr array<type, size> values = {
        EPSG_Status::eSuccess,
        EPSG_Status::eInProgress,
        EPSG_Status::eNotFound,
        EPSG_Status::eCanceled,
        EPSG_Status::eForbidden,
        EPSG_Status::eError,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case EPSG_Status::eSuccess:         return "success";
            case EPSG_Status::eInProgress:      return "in_progress";
            case EPSG_Status::eNotFound:        return "not_found";
            case EPSG_Status::eCanceled:        return "canceled";
            case EPSG_Status::eForbidden:       return "forbidden";
            case EPSG_Status::eError:           return "error";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eMessage>
{
    using type = EDiagSev;
    static constexpr size_t size = EDiagSev::eDiag_Trace + 1;
    static constexpr auto prefix = "\tmessage\tseverity=";

    static constexpr array<type, size> values = {
        EDiagSev::eDiag_Info,
        EDiagSev::eDiag_Warning,
        EDiagSev::eDiag_Error,
        EDiagSev::eDiag_Critical,
        EDiagSev::eDiag_Fatal,
        EDiagSev::eDiag_Trace,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case EDiagSev::eDiag_Info:          return "info";
            case EDiagSev::eDiag_Warning:       return "warning";
            case EDiagSev::eDiag_Error:         return "error";
            case EDiagSev::eDiag_Critical:      return "critical";
            case EDiagSev::eDiag_Fatal:         return "fatal";
            case EDiagSev::eDiag_Trace:         return "trace";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

enum EPSG_StatsCountersRetries {
    ePSG_StatsCountersRetries_Retry,
    ePSG_StatsCountersRetries_Timeout,
};

template <>
struct SPSG_StatsCounters::SGroup<SPSG_StatsCounters::eRetries>
{
    using type = EPSG_StatsCountersRetries;
    static constexpr size_t size = ePSG_StatsCountersRetries_Timeout + 1;
    static constexpr auto prefix = "\tretries\tevent=";

    static constexpr array<type, size> values = {
        ePSG_StatsCountersRetries_Retry,
        ePSG_StatsCountersRetries_Timeout,
    };

    static const char* ValueName(type value)
    {
        switch (value) {
            case ePSG_StatsCountersRetries_Retry:       return "retry";
            case ePSG_StatsCountersRetries_Timeout:     return "timeout";
        }

        // Should not happen
        _TROUBLE;
        return "unknown";
    }
};

template <SPSG_Stats::EGroup group>
void SPSG_StatsCounters::SInit::Func(TData& data)
{
    data.emplace_back(SGroup<group>::size);

    for (auto& counter : data.back()) {
        counter = 0;
    }
};

template <SPSG_Stats::EGroup group>
void SPSG_StatsCounters::SReport::Func(const TData& data, const char* prefix, unsigned report)
{
    using TGroup = SGroup<group>;

    _ASSERT(data.size() > group);
    const auto& g = data[group];
    _ASSERT(g.size() == TGroup::size);

    for (auto i : TGroup::values) {
        auto n = g[static_cast<size_t>(i)].load();
        if (n) ERR_POST(Note << prefix << report << TGroup::prefix << TGroup::ValueName(i) << "&count=" << n);
    }
}

SPSG_StatsCounters::SPSG_StatsCounters()
{
    Apply<SInit>(eRequest, m_Data);
}

template <class TWhat, class... TArgs>
void SPSG_StatsCounters::Apply(EGroup start_with, TArgs&&... args)
{
    // This method is always called with start_with == eRequest (so, all cases are run with one call, one by one).
    // This switch usage however makes compilers warn if any enum value is missing/not handled
    switch (start_with) {
        case eRequest:          TWhat::template Func<eRequest>          (std::forward<TArgs>(args)...);
        case eReplyItem:        TWhat::template Func<eReplyItem>        (std::forward<TArgs>(args)...);
        case eSkippedBlob:      TWhat::template Func<eSkippedBlob>      (std::forward<TArgs>(args)...);
        case eReplyItemStatus:  TWhat::template Func<eReplyItemStatus>  (std::forward<TArgs>(args)...);
        case eMessage:          TWhat::template Func<eMessage>          (std::forward<TArgs>(args)...);
        case eRetries:          TWhat::template Func<eRetries>          (std::forward<TArgs>(args)...);
    }
}

template <class... TArgs>
void SPSG_StatsCounters::Report(TArgs&&... args)
{
    Apply<SReport>(eRequest, m_Data, std::forward<TArgs>(args)...);
}

SPSG_StatsAvgTime::SPSG_StatsAvgTime() :
    m_Data(eTimeUntilResend + 1)
{
}

const char* SPSG_StatsAvgTime::GetName(EAvgTime avg_time)
{
    switch (avg_time) {
        case SPSG_Stats::eSentSecondsAgo:   return "sent_seconds_ago";
        case SPSG_Stats::eTimeUntilResend:  return "time_until_resend";
    }

    // Should not happen
    _TROUBLE;
    return "unknown";
}

void SPSG_StatsAvgTime::Report(const char* prefix, unsigned report)
{
    for (auto i : { eSentSecondsAgo, eTimeUntilResend }) {
        _ASSERT(m_Data.size() > i);
        const auto& data = m_Data[i];
        auto v = data.first.load();
        auto n = data.second.load();
        if (n) ERR_POST(Note << prefix << report << '\t' << GetName(i) << "\taverage=" << double(v / n) / milli::den);
    }
}

template <class TDataId>
void SPSG_StatsData::SData<TDataId>::Report(const char* prefix, unsigned report, const char* data_prefix)
{
    struct SLess {
        static auto Tuple(const CPSG_BlobId& id)  { return                       tie(id.GetId(),       id.GetLastModified()); }
        static auto Tuple(const CPSG_ChunkId& id) { return tuple<int, const string&>(id.GetId2Chunk(), id.GetId2Info());      }
        bool operator()(const TDataId& lhs, const TDataId& rhs) const { return Tuple(lhs) < Tuple(rhs); }
    };

    size_t total = 0;
    map<TDataId, unsigned, SLess> unique_ids;

    if (auto locked = m_Ids.GetLock()) {
        total = locked->size();

        if (!total) return;

        for (const auto& data_id : *locked) {
            auto created = unique_ids.emplace(data_id, 1);
            if (!created.second) ++created.first->second;
        }
    }

    ERR_POST(Note << prefix << report << data_prefix << "\ttotal=" << total << "&unique=" << unique_ids.size());

    auto received = m_Received.load();
    auto read = m_Read.load();
    if (received) ERR_POST(Note << prefix << report << data_prefix << "_data\treceived=" << received << "&read=" << read);

    map<unsigned, unsigned> group_by_count;

    for (const auto& p : unique_ids) {
        auto created = group_by_count.emplace(p.second, 1);
        if (!created.second) ++created.first->second;
    }

    for (const auto& p : group_by_count) {
        ERR_POST(Note << prefix << report << data_prefix << "_retrievals\tnumber=" << p.first << "&unique_ids=" << p.second);
    }
}

void SPSG_StatsData::Report(const char* prefix, unsigned report)
{
    m_Blobs.Report(prefix, report, "\tblob");
    m_Chunks.Report(prefix, report, "\tchunk");
    if (auto n = m_TSEs.GetLock()->size()) ERR_POST(Note << prefix << report << "\tchunk_tse\tunique=" << n);
}

uint64_t s_GetStatsPeriod()
{
    return SecondsToMs(TPSG_StatsPeriod::GetDefault());
}

SPSG_Stats::SPSG_Stats(SPSG_Servers::TTS& servers) :
    m_Timer(this, s_OnTimer, s_GetStatsPeriod(), s_GetStatsPeriod()),
    m_Report(0),
    m_Servers(servers)
{
}

void SPSG_Stats::Report()
{
    const auto prefix = "PSG_STATS\t";
    const auto report = ++m_Report;

    SPSG_StatsCounters::Report(prefix, report);
    SPSG_StatsAvgTime::Report(prefix, report);
    SPSG_StatsData::Report(prefix, report);

    auto servers_locked = m_Servers.GetLock();

    for (const auto& server : *servers_locked) {
        auto n = server.stats.load();
        if (n) ERR_POST(Note << prefix << report << "\tserver\tname=" << server.address << "&requests_sent=" << n);
    }
}

SPSG_Message SPSG_Reply::SState::GetMessage(EDiagSev min_severity)
{
    for (auto& m : m_Messages) {
        if (m && ((min_severity == eDiag_Trace) || ((m.severity != eDiag_Trace) && (m.severity >= min_severity)))) {
            return exchange(m, SPSG_Message{});
        }
    }

    return {};
}

void SPSG_Reply::SState::Reset()
{
    m_InProgress.store(true);
    m_Status.store(EPSG_Status::eSuccess);
    m_Messages.clear();
}

int SPSG_Reply::SState::SStatus::From(EPSG_Status status)
{
    switch (status) {
        case EPSG_Status::eSuccess:         return CRequestStatus::e200_Ok;
        case EPSG_Status::eForbidden:       return CRequestStatus::e403_Forbidden;
        case EPSG_Status::eNotFound:        return CRequestStatus::e404_NotFound;
        default:                            return CRequestStatus::e400_BadRequest;
    }
}

EPSG_Status SPSG_Reply::SState::SStatus::From(int status)
{
    switch (status) {
        case CRequestStatus::e200_Ok:               return EPSG_Status::eSuccess;
        case CRequestStatus::e202_Accepted:         return EPSG_Status::eSuccess;
        case CRequestStatus::e401_Unauthorized:     return EPSG_Status::eForbidden;
        case CRequestStatus::e403_Forbidden:        return EPSG_Status::eForbidden;
        case CRequestStatus::e407_ProxyAuthRequired:                return EPSG_Status::eForbidden;
        case CRequestStatus::e451_Unavailable_For_Legal_Reasons:    return EPSG_Status::eForbidden;
        case CRequestStatus::e404_NotFound:         return EPSG_Status::eNotFound;
        default:                                    return EPSG_Status::eError;
    }
}

void SPSG_Reply::SItem::Reset()
{
    chunks.clear();
    args = SPSG_Args{};
    expected = null;
    received = 0;
    state.Reset();
}

void SPSG_Reply::SetComplete()
{
    // If it were 'more' (instead of 'less'), items would not be in progress then
    const auto message = "Protocol error: received less than expected";
    bool missing = false;

    if (auto items_locked = items.GetLock()) {
        for (auto& item : *items_locked) {
            if (item->state.InProgress()) {
                item.GetLock()->state.AddError(message);
                item->state.SetComplete();
                missing = true;
            }
        }
    }

    if (auto reply_item_locked = reply_item.GetLock()) {
        if (missing || reply_item_locked->expected.Cmp<greater>(reply_item_locked->received)) {
            reply_item_locked->state.AddError(message);
        }

        reply_item_locked->state.SetComplete();
    }

    reply_item.NotifyOne();
    queue->NotifyOne();
}

void SPSG_Reply::SetFailed(string message, SState::SStatus status)
{
    if (auto items_locked = items.GetLock()) {
        for (auto& item : *items_locked) {
            if (item->state.InProgress()) {
                item.GetLock()->state.AddError(message);
                item->state.SetComplete();
            }
        }
    }

    if (auto reply_item_locked = reply_item.GetLock()) {
        reply_item_locked->state.AddError(message, status);
        reply_item_locked->state.SetComplete();
    }

    reply_item.NotifyOne();
    queue->NotifyOne();
}

optional<SPSG_Reply::SItem::TTS*> SPSG_Reply::GetNextItem(CDeadline deadline)
{
    do {
        bool was_in_progress = reply_item->state.InProgress();

        if (auto new_items_locked = new_items.GetLock()) {
            if (!new_items_locked->empty()) {
                auto rv = new_items_locked->front();
                new_items_locked->pop_front();
                return rv;
            }
        }

        // No more reply items
        if (!was_in_progress) {
            return nullptr;
        }
    }
    while (reply_item.WaitUntil(reply_item->state.InProgress(), deadline, false, true));

    return nullopt;
}

void SPSG_Reply::Reset()
{
    items.GetLock()->clear();
    new_items.GetLock()->clear();
    reply_item.GetLock()->Reset();
}

shared_ptr<void> SPSG_Request::SContext::Set()
{
    auto guard = m_ExistingGuard.lock();

    if (!guard) {
        CDiagContext::SetRequestContext(m_Context);
        guard.reset(this, [](void*) { CDiagContext::SetRequestContext(nullptr); });
        m_ExistingGuard = guard;
    }

    return guard;
}

SPSG_Request::SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c, const SPSG_Params& params) :
    full_path(std::move(p)),
    reply(r),
    context(c),
    m_State(&SPSG_Request::StatePrefix),
    m_Retries(params)
{
    _ASSERT(reply);
}

void SPSG_Request::Reset()
{
    reply->Reset();
    m_State = &SPSG_Request::StatePrefix;
    m_Buffer = SBuffer{};
    m_ItemsByID.clear();
}

SPSG_Request::EStateResult SPSG_Request::StatePrefix(const char*& data, size_t& len)
{
    static const string kPrefix = "\n\nPSG-Reply-Chunk: ";

    auto& index = m_Buffer.prefix_index;

    // Checking prefix
    while (*data == kPrefix[index]) {
        ++data;
        --len;

        // Full prefix matched
        if (++index == kPrefix.size()) {
            m_State = &SPSG_Request::StateArgs;
            return eContinue;
        }

        if (!len) return eContinue;
    }

    if (reply->raw && !index) {
        m_State = &SPSG_Request::StateData;
        m_Buffer.data_to_read = numeric_limits<size_t>::max();
        return eContinue;
    }

    // Check failed
    const auto message = "Protocol error: prefix mismatch";

    if (Retry(message)) {
        return eRetry;
    }

    reply->reply_item.GetLock()->state.AddError(message);
    return eStop;
}

SPSG_Request::EStateResult SPSG_Request::StateArgs(const char*& data, size_t& len)
{
    // Accumulating args
    while (*data != '\n') {
        m_Buffer.args_buffer.push_back(*data++);
        if (!--len) return eContinue;
    }

    ++data;
    --len;

    SPSG_Args args(m_Buffer.args_buffer);

    const auto& size_str = args.GetValue("size");
    const auto size = size_str.empty() ? 0ul : stoul(size_str);

    m_Buffer.args = std::move(args);

    if (size) {
        m_State = &SPSG_Request::StateData;
        m_Buffer.data_to_read = size;
    } else {
        m_State = &SPSG_Request::StatePrefix;
        return Add();
    }

    return eContinue;
}

SPSG_Request::EStateResult SPSG_Request::StateData(const char*& data, size_t& len)
{
    // Accumulating data
    const auto data_size = min(m_Buffer.data_to_read, len);

    // Do not add an empty part
    if (!data_size) return eContinue;

    auto& chunk = m_Buffer.chunk;
    chunk.append(data, data_size);
    data += data_size;
    len -= data_size;
    m_Buffer.data_to_read -= data_size;

    if (!m_Buffer.data_to_read) {
        m_State = &SPSG_Request::StatePrefix;
        return Add();
    }

    return eContinue;
}

EDiagSev s_GetSeverity(const string& severity)
{
    if (severity == "error")        return eDiag_Error;
    if (severity == "warning")      return eDiag_Warning;
    if (severity == "info")         return eDiag_Info;
    if (severity == "trace")        return eDiag_Trace;
    if (severity == "fatal")        return eDiag_Fatal;
    if (severity == "critical")     return eDiag_Critical;

    // Should not happen
    _TROUBLE;
    return eDiag_Error;
}

auto s_GetCode(const string& code)
{
    return code.empty() ? optional<int>{} : atoi(code.c_str());
}

SPSG_Request::EStateResult SPSG_Request::Add()
{
    auto context_guard = context.Set();

    auto& args = m_Buffer.args;
    reply->debug_printout << args << m_Buffer.chunk << endl;

    const auto item_type = args.GetValue<SPSG_Args::eItemType>().first;
    auto& reply_item_ts = reply->reply_item;

    if (item_type == SPSG_Args::eReply) {
        if (auto item_locked = reply_item_ts.GetLock()) {
            if (auto update_result = UpdateItem(item_type, *item_locked, args); update_result == eRetry503) {
                return eRetry;
            }
        }

        // Item must be unlocked before notifying
        reply_item_ts.NotifyOne();

    } else {
        if (auto reply_item_locked = reply_item_ts.GetLock()) {
            auto& reply_item = *reply_item_locked;
            ++reply_item.received;

            if (reply_item.expected.Cmp<less>(reply_item.received)) {
                reply_item.state.AddError("Protocol error: received more than expected");
            }
        }

        auto item_id = args.GetValue("item_id");
        auto& item_by_id = m_ItemsByID[item_id];
        bool to_create = !item_by_id;

        if (to_create) {
            if (auto items_locked = reply->items.GetLock()) {
                items_locked->emplace_back();
                item_by_id = &items_locked->back();
            }
        }

        if (auto item_locked = item_by_id->GetLock()) {
            auto update_result = UpdateItem(item_type, *item_locked, args);

            if (update_result == eRetry503) {
                return eRetry;
            }

            if (to_create) {
                item_locked->args = std::move(args);
            }

            if (update_result == eNewItem) {
                reply->new_items.GetLock()->emplace_back(item_by_id);
            }

            reply_item_ts.NotifyOne();
        }

        // Item must be unlocked before notifying
        item_by_id->NotifyOne();
    }

    reply->queue->NotifyOne();
    m_Buffer = SBuffer();
    return eContinue;
}

SPSG_Request::EUpdateResult SPSG_Request::UpdateItem(SPSG_Args::EItemType item_type, SPSG_Reply::SItem& item, const SPSG_Args& args)
{
    auto get_status = [&]() { return NStr::StringToInt(args.GetValue("status"), NStr::fConvErr_NoThrow); };
    auto can_retry_503 = [&](auto s, auto m) { return (s == CRequestStatus::e503_ServiceUnavailable) && Retry(m); };

    ++item.received;

    auto chunk_type = args.GetValue<SPSG_Args::eChunkType>();
    auto& chunk = m_Buffer.chunk;
    auto rv = eSuccess;

    if (chunk_type.first & SPSG_Args::eMeta) {
        auto n_chunks = args.GetValue("n_chunks");

        if (!n_chunks.empty()) {
            auto expected = stoul(n_chunks);

            if (item.expected.Cmp<not_equal_to>(expected)) {
                item.state.AddError("Protocol error: contradicting n_chunks");
            } else {
                item.expected = expected;
            }
        }

        if (const auto status = get_status(); can_retry_503(status, "Server returned a meta with status 503")) {
            return eRetry503;
        } else if (status) {
            item.state.SetStatus(status, true);
        }

        if ((item_type != SPSG_Args::eBlob) || item.chunks.empty()) {
            rv = eNewItem;
        }

    } else if (chunk_type.first == SPSG_Args::eUnknownChunk) {
        static atomic_bool reported(false);

        if (!reported.exchange(true)) {
            ERR_POST("Received unknown chunk type: " << chunk_type.second.get());
        }

        if (TPSG_FailOnUnknownChunks::GetDefault()) {
            item.state.AddError("Protocol error: unknown chunk type '" + chunk_type.second + '\'');
        }
    }

    if (chunk_type.first & SPSG_Args::eMessage) {
        auto severity = s_GetSeverity(args.GetValue("severity"));
        auto code = s_GetCode(args.GetValue("code"));

        if (severity <= eDiag_Warning) {
            item.state.AddMessage(std::move(chunk), severity, code);
        } else if (severity == eDiag_Trace) {
            _DEBUG_CODE(item.state.AddMessage(std::move(chunk), severity, code););
        } else if (const auto status = get_status(); can_retry_503(status, chunk.c_str())) {
            return eRetry503;
        } else {
            item.state.AddError(std::move(chunk), status, severity, code);
        }

        if (auto stats = reply->stats.lock()) stats->IncCounter(SPSG_Stats::eMessage, severity);

    } else if (chunk_type.first & SPSG_Args::eData) {
        auto blob_chunk = args.GetValue("blob_chunk");
        auto index = blob_chunk.empty() ? 0 : stoul(blob_chunk);

        if (item_type == SPSG_Args::eBlob) {
            if (!index) {
                rv = eNewItem;
            }

            if (auto stats = reply->stats.lock()) {
                auto has_blob_id = !args.GetValue<SPSG_Args::eBlobId>().get().empty();
                stats->AddData(has_blob_id, SPSG_Stats::eReceived, chunk.size());
            }
        }

        if (item.chunks.size() <= index) item.chunks.resize(index + 1);

        item.chunks[index] = std::move(chunk);
    }

    const bool is_not_reply = item_type != SPSG_Args::eReply;

    if (item.expected.Cmp<less>(item.received)) {
        item.state.AddError("Protocol error: received more than expected");

        // If item is not a reply itself, add the error to its reply as well
        if (is_not_reply) {
            reply->reply_item.GetLock()->state.AddError("Protocol error: received more than expected");
        }

    // Set item complete if received everything. Reply is set complete when stream closes
    } else if (is_not_reply && item.expected.Cmp<equal_to>(item.received)) {
        item.state.SetComplete();
    }

    return rv;
}


struct SPSG_NcbiPeerID
{
    static string Init()
    {
        if (auto tci = TUvNgHttp2_TestIdentity::GetDefault(); tci.empty()) {
            return GetDiagContext().GetStringUID();
        } else if (tci.starts_with("peer_id_")) {
            return tci;
        } else {
            return {};
        }
    }

    static const string& Get()
    {
        static const string ncbi_peer_id(Init());
        return ncbi_peer_id;
    }
};


#define HTTP_STATUS_HEADER ":status"


/** SPSG_IoSession */

template <class... TNgHttp2Cbs>
SPSG_IoSession::SPSG_IoSession(SPSG_Server& s, const SPSG_Params& params, SPSG_AsyncQueue& queue, uv_loop_t* loop, TNgHttp2Cbs&&... callbacks) :
    SUvNgHttp2_SessionBase(
            loop,
            TAddrNCred{{s.address, SUvNgHttp2_Tls::TCred()}, params.proxy},
            TPSG_RdBufSize::GetDefault(),
            TPSG_WrBufSize::GetDefault(),
            TPSG_Https::GetDefault(),
            TPSG_MaxConcurrentStreams::GetDefault(),
            std::forward<TNgHttp2Cbs>(callbacks)...,
            params.debug_printout >= EPSG_DebugPrintout::eFrames ? s_OnFrameRecv : nullptr),
    server(s),
    m_Params(params),
    m_Headers{{
        { ":method", "GET" },
        { ":scheme", TPSG_Https::GetDefault() ? "https" : "http" },
        { ":authority", m_Authority },
        { ":path" },
        { "user-agent", SUvNgHttp2_UserAgent::Get() },
        { "ncbi-peer-id", SPSG_NcbiPeerID::Get() },
        { "http_ncbi_sid" },
        { "http_ncbi_phid" },
        { "cookie" },
        { "x-forwarded-for" }
    }},
    m_Queue(queue),
    m_Requests(*this)
{
}

int SPSG_IoSession::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " received: " << len);

    if (auto it = m_Requests.find(stream_id); it != m_Requests.end()) {
        if (auto [processor_id, req] = it->second.Get(); req) {
            auto result = req->OnReplyData(processor_id, (const char*)data, len);

            if (result == SPSG_Request::eContinue) {
                it->second.ResetTime();
                return 0;

            } else if (result == SPSG_Request::eRetry) {
                m_Queue.Emplace(req);
                m_Queue.Signal();

            } else {
                req->OnReplyDone(processor_id)->SetComplete();
            }

            server.throttling.AddFailure();
        }

        m_Requests.erase(it);
    }

    return 0;
}

bool SPSG_Request::Retry(const SUvNgHttp2_Error& error, bool refused_stream)
{
    auto context_guard = context.Set();

    if (auto retries = GetRetries(SPSG_Retries::eRetry, refused_stream)) {
        reply->debug_printout << retries << error << endl;
        return true;
    }

    return false;
}

bool SPSG_Request::Fail(SPSG_Processor::TId processor_id, const SUvNgHttp2_Error& error, bool refused_stream)
{
    if (GetRetries(SPSG_Retries::eFail, refused_stream)) {
        return false;
    }

    auto context_guard = context.Set();

    reply->debug_printout << error << endl;
    OnReplyDone(processor_id)->SetFailed(error);
    return true;
}

void SPSG_Request::ConvertRaw()
{
    if (m_Buffer.args_buffer.empty() && !m_Buffer.chunk.empty()) {
        m_Buffer.args = "item_id=1&item_type=unknown&chunk_type=data_and_meta&n_chunks=1"s;
        Add();
        m_Buffer.args = "item_id=0&item_type=reply&chunk_type=meta&n_chunks=2"s;
        Add();
    }
}

bool SPSG_IoSession::Fail(SPSG_Processor::TId processor_id, shared_ptr<SPSG_Request> req, const SUvNgHttp2_Error& error, bool refused_stream)
{
    auto context_guard = req->context.Set();
    auto rv = req->Fail(processor_id, error, refused_stream);

    server.throttling.AddFailure();

    if (rv) {
        PSG_THROTTLING_TRACE("Server '" << GetId() << "' failed to process request '" <<
                req->reply->debug_printout.id << "', " << error);
    }

    return rv;
}

int SPSG_IoSession::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " closed: " << error_code);

    if (auto it = m_Requests.find(stream_id); it != m_Requests.end()) {
        if (auto [processor_id, req] = it->second.Get(); req) {
            auto context_guard = req->context.Set();
            auto& debug_printout = req->reply->debug_printout;
            debug_printout << error_code << endl;

            // If there is an error and the request is allowed to Retry
            if (error_code) {
                auto error(SUvNgHttp2_Error::FromNgHttp2(error_code, "on close"));

                if (RetryFail(processor_id, req, error, error_code == NGHTTP2_REFUSED_STREAM)) {
                    ERR_POST("Request for " << GetId() << " failed with " << error);
                }
            } else {
                if (req->reply->raw) {
                    req->ConvertRaw();
                }

                req->OnReplyDone(processor_id)->SetComplete();
                server.throttling.AddSuccess();
                PSG_THROTTLING_TRACE("Server '" << GetId() << "' processed request '" <<
                        debug_printout.id << "' successfully");
            }
        }

        m_Requests.erase(it);
    }

    return 0;
}

int SPSG_IoSession::OnHeader(nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name,
        size_t namelen, const uint8_t* value, size_t, uint8_t)
{
    if ((frame->hd.type == NGHTTP2_HEADERS) && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
            (namelen == sizeof(HTTP_STATUS_HEADER) - 1) && (strcmp((const char*)name, HTTP_STATUS_HEADER) == 0)) {

        auto stream_id = frame->hd.stream_id;
        auto status_str = reinterpret_cast<const char*>(value);

        PSG_IO_SESSION_TRACE(this << '/' << stream_id << " status: " << status_str);

        if (auto it = m_Requests.find(stream_id); it != m_Requests.end()) {
            const auto request_status = static_cast<CRequestStatus::ECode>(atoi(status_str));
            const auto status = SPSG_Reply::SState::SStatus(request_status);

            if (status != EPSG_Status::eSuccess) {
                if (auto [processor_id, req] = it->second.Get(); req) {
                    const auto error = to_string(request_status) + ' ' + CRequestStatus::GetStdStatusMessage(request_status);
                    req->OnReplyDone(processor_id)->SetFailed(error, status);
                } else {
                    m_Requests.erase(it);
                }
            }
        }
    }

    return 0;
}

bool SPSG_IoSession::ProcessRequest(SPSG_TimedRequest timed_req, SPSG_Processor::TId processor_id, shared_ptr<SPSG_Request> req)
{
    PSG_IO_SESSION_TRACE(this << " processing requests");

    auto context_guard = req->context.Set();
    CRequestContext& context = CDiagContext::GetRequestContext();

    const auto& path = req->full_path;
    const auto& session_id = context.GetSessionID();
    const auto& sub_hit_id = context.GetNextSubHitID();
    const auto& cookie = m_Params.GetCookie([&]() { return context.GetProperty("auth_token"); });
    const auto& client_ip = context.GetClientIP();
    auto headers_size = m_Headers.size();

    m_Headers[ePath] = path;
    m_Headers[eSessionID] = session_id;
    m_Headers[eSubHitID] = sub_hit_id;
    m_Headers[eCookie] = cookie;

    if (!client_ip.empty()) {
        m_Headers[eClientIP] = client_ip;
    } else {
        --headers_size;
    }

    auto stream_id = m_Session.Submit(m_Headers.data(), headers_size);

    if (stream_id < 0) {
        auto error(SUvNgHttp2_Error::FromNgHttp2(stream_id, "on submit"));

        // Do not reset all requests unless throttling has been activated
        if (RetryFail(processor_id, req, error) && server.throttling.Active()) {
            Reset(std::move(error));
        }

        return false;
    }

    req->submitted_by.Set(GetInternalId());
    req->reply->debug_printout << server.address << path << session_id << sub_hit_id << client_ip << m_Tcp.GetLocalPort() << endl;
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " submitted");
    m_Requests.emplace(stream_id, std::move(timed_req));
    return Send();
}

template <class TOnRetry, class TOnFail>
bool SPSG_TimedRequest::CheckExpiration(const SPSG_Params& params, const SUvNgHttp2_Error& error, TOnRetry on_retry, TOnFail on_fail)
{
    auto [processor_id, req] = Get();

    // Remove competitive requests if one is already being processed
    if (!req) {
        return true;
    }

    auto time = AddTime();

    if (time == params.competitive_after) {
        if (req->Retry(error)) {
            req->Reset();
            if (auto stats = req->reply->stats.lock()) stats->IncCounter(SPSG_Stats::eRetries, ePSG_StatsCountersRetries_Retry);
            on_retry(req);
        }
    }

    if (time >= params.request_timeout) {
        if (auto stats = req->reply->stats.lock()) stats->IncCounter(SPSG_Stats::eRetries, ePSG_StatsCountersRetries_Timeout);
        on_fail(processor_id, req);
        return true;
    }

    return false;
}

void SPSG_IoSession::CheckRequestExpiration()
{
    SUvNgHttp2_Error error("Request timeout for ");
    error << GetId();

    auto on_retry = [&](auto req) { m_Queue.Emplace(req); m_Queue.Signal(); };
    auto on_fail = [&](auto processor_id, auto req) { Fail(processor_id, req, error); };

    for (auto it = m_Requests.begin(); it != m_Requests.end(); ) {
        if (it->second.CheckExpiration(m_Params, error, on_retry, on_fail)) {
            it = m_Requests.erase(it);
        } else {
            ++it;
        }
    }
}

void SPSG_IoSession::OnReset(SUvNgHttp2_Error error)
{
    bool some_requests_failed = false;

    for (auto& pair : m_Requests) {
        if (auto [processor_id, req] = pair.second.Get(); req) {
            if (RetryFail(processor_id, req, error)) {
                some_requests_failed = true;
            }
        }
    }

    if (some_requests_failed) {
        ERR_POST("Some requests for " << GetId() << " failed with " << error);
    }

    m_Requests.clear();
}

auto s_GetFrameName(const nghttp2_frame* frame)
{
    switch (frame->hd.type) {
		case NGHTTP2_DATA:              return "DATA"sv;
		case NGHTTP2_HEADERS:           return "HEADERS"sv;
		case NGHTTP2_PRIORITY:          return "PRIORITY"sv;
		case NGHTTP2_RST_STREAM:        return "RST_STREAM"sv;
		case NGHTTP2_SETTINGS:          return "SETTINGS"sv;
		case NGHTTP2_PUSH_PROMISE:      return "PUSH_PROMISE"sv;
		case NGHTTP2_PING:              return "PING"sv;
		case NGHTTP2_GOAWAY:            return "GOAWAY"sv;
		case NGHTTP2_WINDOW_UPDATE:     return "WINDOW_UPDATE"sv;
		case NGHTTP2_CONTINUATION:      return "CONTINUATION"sv;
		case NGHTTP2_ALTSVC:            return "ALTSVC"sv;
		case NGHTTP2_ORIGIN:            return "ORIGIN"sv;
    }

    _TROUBLE;
    return ""sv;
}

int SPSG_IoSession::OnFrameRecv(nghttp2_session*, const nghttp2_frame* frame)
{
    PSG_IO_SESSION_TRACE(this << '/' << frame->hd.stream_id << " frame: " << s_GetFrameName(frame));

    auto it = m_Requests.find(frame->hd.stream_id);
    auto id = it != m_Requests.end() ? it->second.GetId() : "*"sv;
    ERR_POST(Message << id << ": Received "sv << s_GetFrameName(frame) << " frame"sv);
    return 0;
}


/** SPSG_ThrottleParams */

SPSG_ThrottleParams::SThreshold::SThreshold(string error_rate)
{
    if (error_rate.empty()) return;

    string numerator_str, denominator_str;

    if (!NStr::SplitInTwo(error_rate, "/", numerator_str, denominator_str)) return;

    const auto flags = NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces;

    int n = NStr::StringToInt(numerator_str, flags);
    int d = NStr::StringToInt(denominator_str, flags);

    if (n > 0) numerator = static_cast<size_t>(n);
    if (d > 1) denominator = static_cast<size_t>(d);

    if (denominator > kMaxDenominator) {
        numerator = (numerator * kMaxDenominator) / denominator;
        denominator = kMaxDenominator;
    }
}

SPSG_ThrottleParams::SPSG_ThrottleParams() :
    period(SecondsToMs(TPSG_ThrottlePeriod::GetDefault())),
    max_failures(TPSG_ThrottleMaxFailures::eGetDefault),
    until_discovery(TPSG_ThrottleUntilDiscovery::eGetDefault),
    threshold(TPSG_ThrottleThreshold::GetDefault())
{
}


/** SPSG_Throttling */

SPSG_Throttling::SPSG_Throttling(const SSocketAddress& address, SPSG_ThrottleParams p, uv_loop_t* l) :
    m_Address(address),
    m_Stats(std::move(p)),
    m_Active(eOff),
    m_Timer(this, s_OnTimer, Configured(), 0)
{
    m_Timer.Init(l);
    m_Signal.Init(this, l, s_OnSignal);
}

void SPSG_Throttling::StartClose()
{
    m_Signal.Unref();
    m_Timer.Close();
}

void SPSG_Throttling::FinishClose()
{
    m_Signal.Ref();
    m_Signal.Close();
}

bool SPSG_Throttling::Adjust(bool result)
{
    auto stats_locked = m_Stats.GetLock();

    if (stats_locked->Adjust(m_Address, result)) {
        m_Active.store(eOnTimer);

        // We cannot start throttle timer from any thread (it's not thread-safe),
        // so we use async signal to start timer in the discovery thread
        m_Signal.Signal();
        return true;
    }

    return false;
}

bool SPSG_Throttling::SStats::Adjust(const SSocketAddress& address, bool result)
{
    if (result) {
        failures = 0;

    } else if (params.max_failures && (++failures >= params.max_failures)) {
        ERR_POST(Warning << "Server '" << address <<
                "' reached the maximum number of failures in a row (" << params.max_failures << ')');
        Reset();
        return true;
    }

    if (params.threshold.numerator > 0) {
        auto& reg = threshold_reg.first;
        auto& index = threshold_reg.second;
        const auto failure = !result;

        if (reg[index] != failure) {
            reg[index] = failure;

            if (failure && (reg.count() >= params.threshold.numerator)) {
                ERR_POST(Warning << "Server '" << address << "' is considered bad/overloaded ("
                        << params.threshold.numerator << '/' << params.threshold.denominator << ')');
                Reset();
                return true;
            }
        }

        if (++index >= params.threshold.denominator) index = 0;
    }

    return false;
}

void SPSG_Throttling::SStats::Reset()
{
    failures = 0;
    threshold_reg.first.reset();
}


/** SPSG_IoImpl */

void SPSG_IoImpl::OnShutdown(uv_async_t*)
{
    m_Queue.Unref();

    for (auto& server : m_Sessions) {
        for (auto& session : server.sessions) {
            session.Shutdown();
        }
    }
}

void SPSG_DiscoveryImpl::OnShutdown(uv_async_t*)
{
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    for (auto& server : servers) {
        server.throttling.StartClose();
    }

    if (m_Stats) m_Stats->Stop();
}

void SPSG_DiscoveryImpl::AfterExecute()
{
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    for (auto& server : servers) {
        server.throttling.FinishClose();
    }
}

void SPSG_IoImpl::AddNewServers(uv_async_t* handle)
{
    // Add new session(s) if new server(s) have been added
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    // m_Servers->size() can be used for new_servers only after locking m_Servers to avoid a race
    const auto servers_size = m_Servers->size();
    const auto sessions_size = m_Sessions.size();

    _ASSERT(servers_size >= sessions_size);

    for (auto new_servers = servers_size - sessions_size; new_servers; --new_servers) {
        auto& server = servers[servers_size - new_servers];
        m_Sessions.emplace_back().sessions.emplace_back(server, m_Params, m_Queue, handle->loop);
        PSG_IO_TRACE("Session for server '" << server.address << "' was added");
    }
}

void SPSG_IoImpl::OnQueue(uv_async_t* handle)
{
    auto available_servers = 0;

    for (auto& server : m_Sessions) {
        server.current_rate = server->rate.load();

        if (server.current_rate) {
            ++available_servers;
        }
    }

    auto remaining_submits = m_Params.max_concurrent_submits.Get();

    auto i = m_Sessions.begin();
    auto [timed_req, processor_id, req] = decltype(m_Queue.Pop()){};
    auto d = m_Random.first;
    auto request_rate = 0.0;
    auto target_rate = 0.0;
    _DEBUG_ARG(string req_id);

    // Clang requires '&timed_req = timed_req, &processor_id = processor_id, &req = req'
    auto get_request = [&, &timed_req = timed_req, &processor_id = processor_id, &req = req]() {
        tie(timed_req, processor_id, req) = m_Queue.Pop();

        if (!req) {
            PSG_IO_TRACE("No [more] requests pending");
            return false;
        }

        request_rate = 0.0;
        target_rate = d(m_Random.second);
        _DEBUG_CODE(req_id = req->reply->debug_printout.id;);
        PSG_IO_TRACE("Ready to submit request '" << req_id << '\'');
        return true;
    };

    auto next_server = [&]() {
        if (++i == m_Sessions.end()) i = m_Sessions.begin();
    };

    auto ignore_server = [&]() {
        if (--available_servers) {
            d = uniform_real_distribution<>(0.0, d.max() - i->current_rate);
            i->current_rate = 0.0;
            next_server();
        }
    };

    auto find_session = [&]() {
        auto s = i->sessions.begin();
        for (; (s != i->sessions.end()) && s->IsFull(); ++s);
        return make_pair(s != i->sessions.end(), s);
    };

    while (available_servers && remaining_submits) {
        // Try to get a request if needed
        if (!req && !get_request()) {
            return;
        }

        // Select a server from available according to target rate
        for (; request_rate += i->current_rate, request_rate < target_rate; next_server());

        auto& server = *i;

        // If throttling has been activated (possibly in a different thread)
        if (server->throttling.Active()) {
            PSG_IO_TRACE("Server '" << server->address << "' is throttled, ignoring");
            ignore_server();

        // If server has reached its limit (possibly in a different thread)
        } else if (server->available_streams <= 0) {
            PSG_IO_TRACE("Server '" << server->address << "' is at request limit, ignoring");
            ignore_server();

        // If all server sessions are full
        } else if (auto [found, session] = find_session(); !found) {
            PSG_IO_TRACE("Server '" << server->address << "' has no sessions available, ignoring");
            ignore_server();

        // If this is a competitive stream, try a different server
        } else if (!session->CanProcessRequest(req)) {
            PSG_IO_TRACE("Server '" << session->GetId() << "' already working/worked on the request, trying to find another one");
            // Reset submitter ID and move to next server (the same server will be submitted to if it's the only one available)
            req->submitted_by.Reset();
            next_server();

        // If failed to submit
        } else if (!session->ProcessRequest(*std::move(timed_req), processor_id, std::move(req))) {
            PSG_IO_TRACE("Server '" << session->GetId() << "' failed to get request '" << req_id << "' with rate = " << target_rate);
            next_server();

        // Submitted successfully
        } else {
            PSG_IO_TRACE("Server '" << session->GetId() << "' got request '" << req_id << "' with rate = " << target_rate);
            --remaining_submits;
            ++server->stats;

            // Add new session if needed and allowed to
            if (session->IsFull() && (distance(session, server.sessions.end()) == 1)) {
                const auto single_server_single_session = m_Sessions.size() == 1 && TPSG_MaxSessions::GetDefault() == 1;
                const auto max_sessions = single_server_single_session ? 2 : TPSG_MaxSessions::GetDefault();

                if (server.sessions.size() >= max_sessions) {
                    PSG_IO_TRACE("Server '" << server->address << "' reached session limit");
                    ignore_server();
                } else {
                    server.sessions.emplace_back(*server, m_Params, m_Queue, handle->loop);
                    PSG_IO_TRACE("Additional session for server '" << server->address << "' was added");
                }
            }
        }
    }

    if (req) {
        // Do not need to signal here
        m_Queue.Emplace(*std::move(timed_req));
    }

    if (!remaining_submits) {
        PSG_IO_TRACE("Max concurrent submits reached, submitted: " << m_Params.max_concurrent_submits);
        m_Queue.Signal();
    } else {
        PSG_IO_TRACE("No sessions available [anymore], submitted: " << m_Params.max_concurrent_submits - remaining_submits);
    }
}

void SPSG_DiscoveryImpl::OnTimer(uv_timer_t* handle)
{
    const auto kRegularRate = nextafter(0.009, 1.0);
    const auto kStandbyRate = 0.001;

    const auto& service_name = m_Service.GetServiceName();
    auto discovered = m_Service();

    auto total_preferred_regular_rate = 0.0;
    auto total_preferred_standby_rate = 0.0;
    auto total_regular_rate = 0.0;
    auto total_standby_rate = 0.0;

    // Accumulate totals
    for (auto& server : discovered) {
        const auto is_server_preferred = server.first.host == CSocketAPI::GetLocalHostAddress();

        if (server.second >= kRegularRate) {
            if (is_server_preferred) {
                total_preferred_regular_rate += server.second;
            }

            total_regular_rate += server.second;

        } else if (server.second >= kStandbyRate) {
            if (is_server_preferred) {
                total_preferred_standby_rate += server.second;
            }

            total_standby_rate += server.second;
        }
    }

    if (m_NoServers(total_regular_rate || total_standby_rate, SUv_Timer::GetThat<SUv_Timer>(handle))) {
        ERR_POST("No servers in service '" << service_name << '\'');
        return;
    }

    const auto localhost_preference = TPSG_LocalhostPreference::GetDefault();
    const auto total_preferred_standby_percentage = localhost_preference ? 1.0 - 1.0 / localhost_preference : 0.0;
    const auto have_any_regular_servers = total_regular_rate > 0.0;
    const auto have_nonpreferred_regular_servers = total_regular_rate > total_preferred_regular_rate;
    const auto have_nonpreferred_standby_servers = total_standby_rate > total_preferred_standby_rate;
    const auto regular_rate_adjustment =
        (localhost_preference <= 1) ||
        (total_preferred_regular_rate != 0.0) ||
        (total_preferred_standby_rate == 0.0) ||
        (!have_nonpreferred_regular_servers && !have_nonpreferred_standby_servers);
    auto rate_total = 0.0;

    // Adjust discovered rates
    for (auto& server : discovered) {
        const auto is_server_preferred = server.first.host == CSocketAPI::GetLocalHostAddress();
        const auto old_rate = server.second;

        if (server.second >= kRegularRate) {
            if (is_server_preferred) {
                if (regular_rate_adjustment) {
                    server.second *= localhost_preference;
                } else if (have_nonpreferred_regular_servers) {
                    server.second = 0.0;
                } else if (have_nonpreferred_standby_servers) {
                    server.second = 0.0;
                }
            } else {
                if (regular_rate_adjustment) {
                    // No adjustments
                } else if (have_nonpreferred_regular_servers) {
                    server.second *= (1 - total_preferred_standby_percentage) / total_regular_rate;
                } else if (have_nonpreferred_standby_servers) {
                    server.second = 0.0;
                }
            }
        } else if (server.second >= kStandbyRate) {
            if (is_server_preferred) {
                if (regular_rate_adjustment && have_any_regular_servers) {
                    server.second = 0.0;
                } else if (regular_rate_adjustment) {
                    server.second *= localhost_preference;
                } else if (have_nonpreferred_regular_servers) {
                    server.second *= total_preferred_standby_percentage / total_preferred_standby_rate;
                } else if (have_nonpreferred_standby_servers) {
                    server.second *= total_preferred_standby_percentage / total_preferred_standby_rate;
                }
            } else {
                if (regular_rate_adjustment && have_any_regular_servers && (localhost_preference || have_nonpreferred_regular_servers)) {
                    server.second = 0.0;
                } else if (regular_rate_adjustment) {
                    // No adjustments
                } else if (have_nonpreferred_regular_servers) {
                    server.second = 0.0;
                } else if (have_nonpreferred_standby_servers) {
                    server.second *= (1 - total_preferred_standby_percentage) / (total_standby_rate - total_preferred_standby_rate);
                }
            }
        }

        rate_total += server.second;

        if (old_rate != server.second) {
            PSG_DISCOVERY_TRACE("Rate for '" << server.first << "' adjusted from " << old_rate << " to " << server.second);
        }
    }

    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    // Update existing servers
    for (auto& server : servers) {
        auto address_same = [&](CServiceDiscovery::TServer& s) { return s.first == server.address; };
        auto it = find_if(discovered.begin(), discovered.end(), address_same);

        if ((it == discovered.end()) || (it->second <= numeric_limits<double>::epsilon())) {
            server.rate = 0.0;
            PSG_DISCOVERY_TRACE("Server '" << server.address << "' disabled in service '" << service_name << '\'');

        } else {
            server.throttling.Discovered();
            auto rate = it->second / rate_total;

            if (server.rate != rate) {
                // This has to be before the rate change for the condition to work (uses old rate)
                PSG_DISCOVERY_TRACE("Server '" << server.address <<
                        (server.rate ? "' updated in service '" : "' enabled in service '" ) <<
                        service_name << "' with rate = " << rate);

                server.rate = rate;
            }

            // Reset rate to avoid adding the server below
            it->second = 0.0;
        }
    }

    // Add new servers
    for (auto& server : discovered) {
        if (server.second > numeric_limits<double>::epsilon()) {
            auto rate = server.second / rate_total;
            servers.emplace_back(server.first, rate, m_Params.max_concurrent_requests_per_server, m_ThrottleParams, handle->loop);
            _DEBUG_CODE(server.first.GetHostName();); // To avoid splitting the trace message below by gethostbyaddr
            PSG_DISCOVERY_TRACE("Server '" << server.first << "' added to service '" <<
                    service_name << "' with rate = " << rate);
        }
    }

    m_QueuesRef.SignalAll();
}

void SPSG_IoImpl::OnTimer(uv_timer_t*)
{
    if (m_Servers->fail_requests) {
        FailRequests();
    } else {
        CheckRequestExpiration();
    }

    for (auto& server : m_Sessions) {
        for (auto& session : server.sessions) {
            session.CheckRequestExpiration();
        }
    }
}

void SPSG_IoImpl::CheckRequestExpiration()
{
    auto queue_locked = m_Queue.GetLockedQueue();
    list<SPSG_TimedRequest> retries;
    SUvNgHttp2_Error error("Request timeout before submitting");

    auto on_retry = [&](auto req) { retries.emplace_back(req); m_Queue.Signal(); };
    auto on_fail = [&](auto processor_id, auto req) { req->Fail(processor_id, error); };

    for (auto it = queue_locked->begin(); it != queue_locked->end(); ) {
        if (it->CheckExpiration(m_Params, error, on_retry, on_fail)) {
            it = queue_locked->erase(it);
        } else {
            ++it;
        }
    }

    queue_locked->splice(queue_locked->end(), retries);
}

void SPSG_IoImpl::FailRequests()
{
    auto queue_locked = m_Queue.GetLockedQueue();
    SUvNgHttp2_Error error("No servers to process request");

    for (auto& timed_req : *queue_locked) {
        if (auto [processor_id, req] = timed_req.Get(); req) {
            auto context_guard = req->context.Set();
            auto& debug_printout = req->reply->debug_printout;
            debug_printout << error << endl;
            req->OnReplyDone(processor_id)->SetFailed(error);
            PSG_IO_TRACE("No servers to process request '" << debug_printout.id << '\'');
        }
    }

    queue_locked->clear();
}

void SPSG_IoImpl::OnExecute(uv_loop_t& loop)
{
    m_Queue.Init(this, &loop, s_OnQueue);
}

void SPSG_IoImpl::AfterExecute()
{
    m_Queue.Ref();
    m_Queue.Close();
    m_Sessions.clear();
}

SPSG_DiscoveryImpl::SNoServers::SNoServers(const SPSG_Params& params, SPSG_Servers::TTS& servers) :
    m_RetryDelay(SecondsToMs(TPSG_NoServersRetryDelay::GetDefault())),
    m_Timeout(SecondsToMs((params.request_timeout + params.competitive_after * params.request_retries) * params.io_timer_period)),
    m_FailRequests(const_cast<atomic_bool&>(servers->fail_requests))
{
}

bool SPSG_DiscoveryImpl::SNoServers::operator()(bool discovered, SUv_Timer* timer)
{
    // If there is a separate timer set for the case of no servers discovered
    if (m_RetryDelay) {
        if (discovered) {
            timer->ResetRepeat();
        } else {
            timer->SetRepeat(m_RetryDelay);
        }
    }

    // If there is a request timeout set
    if (m_Timeout) {
        const auto timeout_expired = m_Passed >= m_Timeout;
        m_FailRequests = timeout_expired;

        if (discovered) {
            m_Passed = 0;
        } else if (!timeout_expired) {
            // Passed is increased after fail flag is set, the flag would be set too early otherwise
            m_Passed += m_RetryDelay ? m_RetryDelay : timer->GetDefaultRepeat();
        }
    }

    return !discovered;
}


/** SPSG_IoCoordinator */

shared_ptr<SPSG_Stats> s_GetStats(SPSG_Servers::TTS& servers)
{
    if (TPSG_Stats::GetDefault()) {
        return make_shared<SPSG_Stats>(servers);
    } else {
        return {};
    }
}

uint64_t s_GetDiscoveryRepeat(const CServiceDiscovery& service)
{
    return service.IsSingleServer() ? 0 : SecondsToMs(TPSG_RebalanceTime::GetDefault());
}

SPSG_IoCoordinator::SPSG_IoCoordinator(CServiceDiscovery service) :
    stats(s_GetStats(m_Servers)),
    m_StartBarrier(TPSG_NumIo::GetDefault() + 2),
    m_StopBarrier(TPSG_NumIo::GetDefault() + 1),
    m_Discovery(m_StartBarrier, m_StopBarrier, 0, s_GetDiscoveryRepeat(service), service, stats, params, m_Servers, m_Queues),
    m_RequestCounter(0),
    m_RequestId(1)
{
    const auto io_timer_period = SecondsToMs(params.io_timer_period);

    for (unsigned i = 0; i < TPSG_NumIo::GetDefault(); i++) {
        try {
            // This timing cannot be changed without changes in SPSG_IoSession::CheckRequestExpiration
            m_Io.emplace_back(new SPSG_Thread<SPSG_IoImpl>(m_StartBarrier, m_StopBarrier, io_timer_period, io_timer_period, params, m_Servers, m_Queues.emplace_back(m_Queues)));
        } catch (const std::system_error& e) {
            ERR_POST(Fatal << "Failed to create I/O threads: " << e.what());
        }
    }

    m_StartBarrier.Wait();
}

SPSG_IoCoordinator::~SPSG_IoCoordinator()
{
    m_Discovery.Shutdown();

    for (auto& io : m_Io) {
        io->Shutdown();
    }
}

bool SPSG_IoCoordinator::AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool&, const CDeadline&)
{
    if (m_Io.size() == 0) {
        ERR_POST(Fatal << "IO is not open");
    }

    const auto idx = (m_RequestCounter++ / params.requests_per_io) % m_Io.size();
    m_Queues[idx].Emplace(std::move(req));
    m_Queues[idx].Signal();
    return true;
}


END_NCBI_SCOPE

#endif
