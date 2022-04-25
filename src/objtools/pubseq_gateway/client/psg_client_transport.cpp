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

NCBI_PARAM_DEF(unsigned, PSG, rd_buf_size,            64 * 1024);
NCBI_PARAM_DEF(size_t,   PSG, wr_buf_size,            64 * 1024);
NCBI_PARAM_DEF(unsigned, PSG, max_concurrent_streams, 200);
NCBI_PARAM_DEF(unsigned, PSG, max_sessions,           40);
NCBI_PARAM_DEF(unsigned, PSG, num_io,                 6);
NCBI_PARAM_DEF(unsigned, PSG, reader_timeout,         12);
NCBI_PARAM_DEF(double,   PSG, rebalance_time,         10.0);
NCBI_PARAM_DEF(unsigned, PSG, request_timeout,        10);
NCBI_PARAM_DEF(size_t, PSG, requests_per_io,          1);
NCBI_PARAM_DEF(unsigned, PSG, request_retries,        2);
NCBI_PARAM_DEF(string,   PSG, request_user_args,      "");
NCBI_PARAM_DEF(bool,     PSG, user_request_ids,       false);
NCBI_PARAM_DEF(unsigned, PSG, localhost_preference,   1);
NCBI_PARAM_DEF(bool,     PSG, fail_on_unknown_items,  false);
NCBI_PARAM_DEF(bool,     PSG, fail_on_unknown_chunks, false);
NCBI_PARAM_DEF(bool,     PSG, https,                  false);
NCBI_PARAM_DEF(double,   PSG, no_servers_retry_delay, 1.0);
NCBI_PARAM_DEF(bool,     PSG, stats,                  false);
NCBI_PARAM_DEF(double,   PSG, stats_period,           0.0);

NCBI_PARAM_DEF(double,   PSG, throttle_relaxation_period,                  0.0);
NCBI_PARAM_DEF(unsigned, PSG, throttle_by_consecutive_connection_failures, 0);
NCBI_PARAM_DEF(bool,     PSG, throttle_hold_until_active_in_lb,            false);
NCBI_PARAM_DEF(string,   PSG, throttle_by_connection_error_rate,           "");

NCBI_PARAM_ENUM_ARRAY(EPSG_DebugPrintout, PSG, debug_printout)
{
    { "none", EPSG_DebugPrintout::eNone },
    { "some", EPSG_DebugPrintout::eSome },
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
    if (value == "public_comment")  return { SPSG_ArgsBase::ePublicComment,  cref(value) };
    if (value == "processor")       return { SPSG_ArgsBase::eProcessor,      cref(value) };
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

struct SContextSetter
{
    SContextSetter(CRequestContext* context) { CDiagContext::SetRequestContext(context); }
    ~SContextSetter()            { CDiagContext::SetRequestContext(nullptr);      }

    SContextSetter(const SContextSetter&) = delete;
    void operator=(const SContextSetter&) = delete;
};

void SDebugPrintout::Print(SSocketAddress address, const string& path, const string& sid, const string& phid, const string& ip)
{
    if (ip.empty()) {
        ERR_POST(Message << id << ": " << address.AsString() << path << ";SID=" << sid << ";PHID=" << phid);
    } else {
        ERR_POST(Message << id << ": " << address.AsString() << path << ";SID=" << sid << ";PHID=" << phid << ";IP=" << ip);
    }
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

        static mutex cout_mutex;
        lock_guard<mutex> lock(cout_mutex);
        cout << os.str();
        cout.flush();
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
        CPSG_ReplyItem::ePublicComment,
        CPSG_ReplyItem::eProcessor,
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
            case CPSG_ReplyItem::ePublicComment:        return "public_comment";
            case CPSG_ReplyItem::eProcessor:            return "processor";
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
        case eRequest:          TWhat::template Func<eRequest>          (forward<TArgs>(args)...);
        case eReplyItem:        TWhat::template Func<eReplyItem>        (forward<TArgs>(args)...);
        case eSkippedBlob:      TWhat::template Func<eSkippedBlob>      (forward<TArgs>(args)...);
        case eReplyItemStatus:  TWhat::template Func<eReplyItemStatus>  (forward<TArgs>(args)...);
        case eMessage:          TWhat::template Func<eMessage>          (forward<TArgs>(args)...);
    }
}

template <class... TArgs>
void SPSG_StatsCounters::Report(TArgs&&... args)
{
    Apply<SReport>(eRequest, m_Data, forward<TArgs>(args)...);
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
        if (n) ERR_POST(Note << prefix << report << "\tserver\tname=" << server.address.AsString() << "&requests_sent=" << n);
    }
}

EPSG_Status SPSG_Reply::SState::GetStatus() const volatile
{
    switch (m_State) {
        case eNotFound:     return EPSG_Status::eNotFound;
        case eForbidden:    return EPSG_Status::eForbidden;
        case eError:        return EPSG_Status::eError;
        case eSuccess:      return EPSG_Status::eSuccess;
        case eInProgress:   return EPSG_Status::eInProgress;
    }

    // Should not happen
    _TROUBLE;
    return EPSG_Status::eError;
}

string SPSG_Reply::SState::GetError()
{
    if (m_Messages.empty()) return {};

    auto rv = m_Messages.back();
    m_Messages.pop_back();
    return rv;
}

SPSG_Reply::SState::EState SPSG_Reply::SState::FromRequestStatus(int status)
{
    switch (status) {
        case CRequestStatus::e200_Ok:               return eSuccess;
        case CRequestStatus::e202_Accepted:         return eSuccess;
        case CRequestStatus::e403_Forbidden:        return eForbidden;
        case CRequestStatus::e404_NotFound:         return eNotFound;
        default:                                    return eError;
    }
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
                item->state.SetState(SState::eError);
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
    queue->CV().NotifyOne();
}

void SPSG_Reply::SetFailed(string message, SState::EState state)
{
    if (auto items_locked = items.GetLock()) {
        for (auto& item : *items_locked) {
            if (item->state.InProgress()) {
                item.GetLock()->state.AddError(message);
                item->state.SetState(SState::eError);
            }
        }
    }

    if (auto reply_item_locked = reply_item.GetLock()) {
        reply_item_locked->state.AddError(message);
        reply_item_locked->state.SetState(state);
    }

    reply_item.NotifyOne();
    queue->CV().NotifyOne();
}

SPSG_Request::SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c, const SPSG_Params& params) :
    full_path(move(p)),
    reply(r),
    context(c),
    m_State(&SPSG_Request::StatePrefix),
    m_Retries(params.request_retries)
{
    _ASSERT(reply);
}

bool SPSG_Request::StatePrefix(const char*& data, size_t& len)
{
    static const string kPrefix = "\n\nPSG-Reply-Chunk: ";

    // No retries after receiving any data
    m_Retries = 0;

    auto& index = m_Buffer.prefix_index;

    // Checking prefix
    while (*data == kPrefix[index]) {
        ++data;
        --len;

        // Full prefix matched
        if (++index == kPrefix.size()) {
            SetStateArgs();
            return true;
        }

        if (!len) return true;
    }

    // Check failed
    const auto matched = CTempString(kPrefix, 0, index);
    const auto different = CTempString(data, min(len, kPrefix.size() - index));
    stringstream ss;
    ss << "Protocol error: prefix mismatch, expected '" << kPrefix << "' vs received '" << matched << different << '\'';
    reply->reply_item.GetLock()->state.AddError(ss.str());
    return false;
}

bool SPSG_Request::StateArgs(const char*& data, size_t& len)
{
    // Accumulating args
    while (*data != '\n') {
        m_Buffer.args_buffer.push_back(*data++);
        if (!--len) return true;
    }

    ++data;
    --len;

    SPSG_Args args(m_Buffer.args_buffer);

    const auto& size_str = args.GetValue("size");
    const auto size = size_str.empty() ? 0ul : stoul(size_str);

    m_Buffer.args = move(args);

    if (size) {
        SetStateData(size);
    } else {
        SetStatePrefix();
    }

    return true;
}

bool SPSG_Request::StateData(const char*& data, size_t& len)
{
    // Accumulating data
    const auto data_size = min(m_Buffer.data_to_read, len);

    // Do not add an empty part
    if (!data_size) return true;

    auto& chunk = m_Buffer.chunk;
    chunk.append(data, data_size);
    data += data_size;
    len -= data_size;
    m_Buffer.data_to_read -= data_size;

    if (!m_Buffer.data_to_read) {
        SetStatePrefix();
    }

    return true;
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

void SPSG_Request::Add()
{
    SContextSetter setter(context);

    auto& args = m_Buffer.args;
    reply->debug_printout << args << m_Buffer.chunk << endl;

    const auto item_type = args.GetValue<SPSG_Args::eItemType>().first;
    auto& reply_item_ts = reply->reply_item;

    if (item_type == SPSG_Args::eReply) {
        if (auto item_locked = reply_item_ts.GetLock()) {
            UpdateItem(item_type, *item_locked, args);
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

        if (item_by_id) {
            auto item_locked = item_by_id->GetLock();
            UpdateItem(item_type, *item_locked, args);

        } else {
            if (auto items_locked = reply->items.GetLock()) {
                items_locked->emplace_back();
                item_by_id = &items_locked->back();
            }

            if (auto item_locked = item_by_id->GetLock()) {
                item_locked->args = move(args);
                UpdateItem(item_type, *item_locked, item_locked->args);
            }

            reply_item_ts.NotifyOne();
        }

        // Item must be unlocked before notifying
        item_by_id->NotifyOne();
    }

    reply->queue->CV().NotifyOne();
    m_Buffer = SBuffer();
}

void SPSG_Request::UpdateItem(SPSG_Args::EItemType item_type, SPSG_Reply::SItem& item, const SPSG_Args& args)
{
    ++item.received;

    auto chunk_type = args.GetValue<SPSG_Args::eChunkType>();
    auto& chunk = m_Buffer.chunk;

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

        if (severity == eDiag_Warning) {
            ERR_POST(Warning << chunk);
        } else if (severity == eDiag_Info) {
            ERR_POST(Info << chunk);
        } else if (severity == eDiag_Trace) {
            ERR_POST(Trace << chunk);
        } else {
            item.state.AddError(move(chunk));
            const auto status = NStr::StringToInt(args.GetValue("status"));
            const auto state = SPSG_Reply::SState::FromRequestStatus(status);

            switch (state) {
                case SPSG_Reply::SState::eSuccess:
                case SPSG_Reply::SState::eError:
                    break;
                default:
                    // Any message leads to eError when item completes, so need to set other states early
                    item.state.SetState(state);
            }
        }

        if (auto stats = reply->stats.lock()) stats->IncCounter(SPSG_Stats::eMessage, severity);

    } else if (chunk_type.first & SPSG_Args::eData) {
        if (auto stats = reply->stats.lock()) {
            if (item_type == SPSG_Args::eBlob) {
                auto has_blob_id = !args.GetValue<SPSG_Args::eBlobId>().get().empty();
                stats->AddData(has_blob_id, SPSG_Stats::eReceived, chunk.size());
            }
        }

        auto blob_chunk = args.GetValue("blob_chunk");
        auto index = blob_chunk.empty() ? 0 : stoul(blob_chunk);

        if (item.chunks.size() <= index) item.chunks.resize(index + 1);

        item.chunks[index] = move(chunk);
        item.state.SetNotEmpty();
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
}


#define HTTP_STATUS_HEADER ":status"


/** SPSG_IoSession */

template <class... TNgHttp2Cbs>
SPSG_IoSession::SPSG_IoSession(SPSG_Server& s, SPSG_AsyncQueue& queue, uv_loop_t* loop, TNgHttp2Cbs&&... callbacks) :
    SUvNgHttp2_SessionBase(
            loop,
            TAddrNCred(s.address, SUvNgHttp2_Tls::TCred()),
            TPSG_RdBufSize::GetDefault(),
            TPSG_WrBufSize::GetDefault(),
            TPSG_Https::GetDefault(),
            TPSG_MaxConcurrentStreams::GetDefault(),
            forward<TNgHttp2Cbs>(callbacks)...),
    server(s),
    m_Headers{{
        { ":method", "GET" },
        { ":scheme", TPSG_Https::GetDefault() ? "https" : "http" },
        { ":authority", m_Authority },
        { ":path" },
        { "user-agent", SUvNgHttp2_UserAgent::Get() },
        { "http_ncbi_sid" },
        { "http_ncbi_phid" },
        { "x-forwarded-for" }
    }},
    m_RequestTimeout(TPSG_RequestTimeout::eGetDefault),
    m_Queue(queue)
{
}

int SPSG_IoSession::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " received: " << len);
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        it->second->OnReplyData((const char*)data, len);
    }

    return 0;
}

bool SPSG_IoSession::Retry(shared_ptr<SPSG_Request> req, const SUvNgHttp2_Error& error)
{
    SContextSetter setter(req->context);
    server.throttling.AddFailure();

    auto& debug_printout = req->reply->debug_printout;
    auto retries = req->GetRetries();

    if (retries) {
        // Return to queue for a re-send
        if (m_Queue.Push(move(req))) {
            debug_printout << retries << error << endl;
            return true;
        }
    }

    debug_printout << error << endl;
    req->reply->SetFailed(error);
    _DEBUG_ARG(const auto& server_name = server.address.AsString());
    PSG_THROTTLING_TRACE("Server '" << server_name << "' failed to process request '" <<
            debug_printout.id << '\'');
    return false;
}

int SPSG_IoSession::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " closed: " << error_code);
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        auto& req = it->second;
        auto& debug_printout = req->reply->debug_printout;

        SContextSetter setter(req->context);
        debug_printout << error_code << endl;

        // If there is an error and the request is allowed to Retry
        if (error_code) {
            auto error(SUvNgHttp2_Error::FromNgHttp2(error_code, "on close"));

            if (!Retry(req, error)) {
                ERR_POST("Request failed with " << error);
            }
        } else {
            req->reply->SetComplete();
            server.throttling.AddSuccess();
            _DEBUG_ARG(const auto& server_name = server.address.AsString());
            PSG_THROTTLING_TRACE("Server '" << server_name << "' processed request '" <<
                    debug_printout.id << "' successfully");
        }

        RequestComplete(it);
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
        auto it = m_Requests.find(stream_id);

        if (it != m_Requests.end()) {
            const auto status = static_cast<CRequestStatus::ECode>(atoi(status_str));
            const auto state = SPSG_Reply::SState::FromRequestStatus(status);

            if (state != SPSG_Reply::SState::eSuccess) {
                const auto error = to_string(status) + ' ' + CRequestStatus::GetStdStatusMessage(status);
                it->second->reply->SetFailed(error, state);
            }
        }
    }

    return 0;
}

bool SPSG_IoSession::ProcessRequest(shared_ptr<SPSG_Request>& req)
{
    PSG_IO_SESSION_TRACE(this << " processing requests");

    SContextSetter setter(req->context);
    CRequestContext& context = CDiagContext::GetRequestContext();

    const auto& path = req->full_path;
    const auto& session_id = context.GetSessionID();
    const auto& sub_hit_id = context.GetNextSubHitID();
    const auto& client_ip = context.GetClientIP();
    auto headers_size = m_Headers.size();

    m_Headers[ePath] = path;
    m_Headers[eSessionID] = session_id;
    m_Headers[eSubHitID] = sub_hit_id;

    if (!client_ip.empty()) {
        m_Headers[eClientIP] = client_ip;
    } else {
        --headers_size;
    }

    auto stream_id = m_Session.Submit(m_Headers.data(), headers_size);

    if (stream_id < 0) {
        auto error(SUvNgHttp2_Error::FromNgHttp2(stream_id, "on submit"));

        // Do not reset all requests unless throttling has been activated
        if (!Retry(req, error) && server.throttling.Active()) {
            Reset(move(error));
        }

        return false;
    }

    req->reply->debug_printout << server.address << path << session_id << sub_hit_id << client_ip << endl;
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " submitted");
    m_Requests.emplace(stream_id, move(req));
    return Send();
}

void SPSG_IoSession::RequestComplete(TRequests::iterator& it)
{
    if (IsFull()) {
        // Continue processing of requests in the IO thread queue on next UV loop iteration
        m_Queue.Signal();
    }

    it = m_Requests.erase(it);
}

void SPSG_IoSession::CheckRequestExpiration()
{
    const SUvNgHttp2_Error error("Request timeout");

    for (auto it = m_Requests.begin(); it != m_Requests.end(); ) {
        if (it->second.AddSecond() >= m_RequestTimeout) {
            Retry(it->second, error);
            RequestComplete(it);
        } else {
            ++it;
        }
    }
}

void SPSG_IoSession::OnReset(SUvNgHttp2_Error error)
{
    bool some_requests_failed = false;

    for (auto& pair : m_Requests) {
        if (!Retry(pair.second, error)) {
            some_requests_failed = true;
        }
    }

    if (some_requests_failed) {
        ERR_POST("Some requests failed with " << error);
    }

    m_Requests.clear();
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
    m_Stats(move(p)),
    m_Active(eOff),
    m_Timer(this, s_OnTimer, Configured(), 0)
{
    m_Timer.Init(l);
    m_Signal.Init(this, l, s_OnSignal);
}

void SPSG_Throttling::StartClose()
{
    m_Signal.Close();
    m_Timer.Close();
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
        ERR_POST(Warning << "Server '" << address.AsString() <<
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
                ERR_POST(Warning << "Server '" << address.AsString() << "' is considered bad/overloaded ("
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
    queue.Close();

    for (auto& server_sessions : m_Sessions) {
        for (auto& session : server_sessions.first) {
            session.Reset("Shutdown is in process");
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

void SPSG_IoImpl::AddNewServers(size_t servers_size, size_t sessions_size, uv_async_t* handle)
{
    _ASSERT(servers_size > sessions_size);

    // Add new session(s) if new server(s) have been added
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    for (auto new_servers = servers_size - sessions_size; new_servers; --new_servers) {
        auto& server = servers[servers_size - new_servers];
        m_Sessions.emplace_back(TSessions(), 0.0);
        m_Sessions.back().first.emplace_back(server, queue, handle->loop);
        _DEBUG_ARG(const auto& server_name = server.address.AsString());
        PSG_IO_TRACE("Session for server '" << server_name << "' was added");
    }
}

void SPSG_IoImpl::OnQueue(uv_async_t* handle)
{
    size_t sessions = 0;

    for (auto& server_sessions : m_Sessions) {
        // There is always at least one session per server
        _ASSERT(server_sessions.first.size());

        auto& server = server_sessions.first.front().server;
        auto& rate = server_sessions.second;
        rate = server.throttling.Active() ? 0.0 : server.rate.load();

        if (rate) {
            ++sessions;
        }
    }

    auto d = m_Random.first;
    auto i = m_Sessions.begin();
    auto next_i = [&]() { if (++i == m_Sessions.end()) i = m_Sessions.begin(); };

    // We have to find available session first and then get a request,
    // as we may not be able to put the request back into the queue after (if it's full)
    while (sessions) {
        auto rate = d(m_Random.second);
        _DEBUG_ARG(const auto original_rate = rate);

        for (;;) {
            // Skip all sessions already marked unavailable in this iteration
            while (!i->second) {
                next_i();
            }

            // There is always at least one session per server
            _ASSERT(i->first.size());

            // These references depend on i and can only be used if it stays the same
            auto& server_sessions = i->first;
            auto& server = server_sessions.front().server;
            auto& session_rate = i->second;

            _DEBUG_ARG(const auto& server_name = server.address.AsString());

            auto session = server_sessions.begin();

            // Skip all full sessions
            for (; (session != server_sessions.end()) && session->IsFull(); ++session);

            // All existing sessions are full and no new sessions are allowed
            if ((session == server_sessions.end()) && (server_sessions.size() >= TPSG_MaxSessions::GetDefault())) {
                PSG_IO_TRACE("Server '" << server_name << "' has no room for a request");

            // Session is available or can be created
            } else {
                rate -= session_rate;

                if (rate >= 0.0) {
                    // Check remaining rate against next available session
                    next_i();
                    continue;
                }

                // Checking if throttling has been activated in a different thread
                if (!server.throttling.Active()) {
                    shared_ptr<SPSG_Request> req;

                    if (!queue.Pop(req)) {
                        PSG_IO_TRACE("No [more] requests pending");
                        return;
                    }

                    // All existing sessions are full
                    if (session == server_sessions.end()) {
                        server_sessions.emplace_back(server, queue, handle->loop);
                        session = (server_sessions.rbegin() + 1).base();
                        PSG_IO_TRACE("Additional session for server '" << server_name << "' was added");
                    }

                    _DEBUG_ARG(const auto req_id = req->reply->debug_printout.id);
                    bool result = session->ProcessRequest(req);

                    if (result) {
                        PSG_IO_TRACE("Server '" << server_name << "' will get request '" <<
                                req_id << "' with rate = " << original_rate);
                        ++server.stats;
                        break;
                    } else {
                        PSG_IO_TRACE("Server '" << server_name << "' failed to process request '" <<
                                req_id << "' with rate = " << original_rate);

                        if (!server.throttling.Active()) {
                            break;
                        }
                    }
                }
            }

            // Current session is unavailable.
            // Check if there are some other sessions available
            if (--sessions) {
                d = uniform_real_distribution<>(d.min() + session_rate);
                session_rate = 0.0;
            }

            break;
        }
    }

    // Continue processing of requests in the IO thread queue on next UV loop iteration
    queue.Signal();
    PSG_IO_TRACE("No sessions available [anymore]");
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
            _DEBUG_ARG(const auto& server_name = server.first.AsString());
            PSG_DISCOVERY_TRACE("Rate for '" << server_name << "' adjusted from " << old_rate << " to " << server.second);
        }
    }

    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    // Update existing servers
    for (auto& server : servers) {
        auto address_same = [&](CServiceDiscovery::TServer& s) { return s.first == server.address; };
        auto it = find_if(discovered.begin(), discovered.end(), address_same);

        _DEBUG_ARG(const auto& server_name = server.address.AsString());

        if ((it == discovered.end()) || (it->second <= numeric_limits<double>::epsilon())) {
            server.rate = 0.0;
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' disabled in service '" << service_name << '\'');

        } else {
            server.throttling.Discovered();
            auto rate = it->second / rate_total;

            if (server.rate != rate) {
                // This has to be before the rate change for the condition to work (uses old rate)
                PSG_DISCOVERY_TRACE("Server '" << server_name <<
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
            servers.emplace_back(server.first, rate, m_ThrottleParams, handle->loop);
            _DEBUG_ARG(const auto& server_name = server.first.AsString());
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' added to service '" <<
                    service_name << "' with rate = " << rate);
        }
    }
}

void SPSG_IoImpl::OnTimer(uv_timer_t*)
{
    for (auto& server_sessions : m_Sessions) {
        for (auto& session : server_sessions.first) {
            session.CheckRequestExpiration();
        }
    }

    if (m_Servers->fail_requests) {
        const SUvNgHttp2_Error error("No servers to process request");
        shared_ptr<SPSG_Request> req;

        while (queue.Pop(req)) {
            SContextSetter setter(req->context);
            auto& debug_printout = req->reply->debug_printout;
            debug_printout << error << endl;
            req->reply->SetFailed(error);
            PSG_IO_TRACE("No servers to process request '" << debug_printout.id << '\'');
        }
    }
}

void SPSG_IoImpl::OnExecute(uv_loop_t& loop)
{
    queue.Init(this, &loop, s_OnQueue);
}

void SPSG_IoImpl::AfterExecute()
{
    m_Sessions.clear();
}

SPSG_DiscoveryImpl::SNoServers::SNoServers(SPSG_Servers::TTS& servers) :
    m_RetryDelay(SecondsToMs(TPSG_NoServersRetryDelay::GetDefault())),
    m_Timeout(SecondsToMs(TPSG_RequestTimeout::GetDefault() * (1 + TPSG_RequestRetries::GetDefault()))),
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
    m_Barrier(TPSG_NumIo::GetDefault() + 2),
    m_Discovery(m_Barrier, 0, s_GetDiscoveryRepeat(service), service, stats, m_Servers),
    m_RequestCounter(0),
    m_RequestId(1)
{
    for (unsigned i = 0; i < TPSG_NumIo::GetDefault(); i++) {
        // This timing cannot be changed without changes in SPSG_IoSession::CheckRequestExpiration
        m_Io.emplace_back(new SPSG_Thread<SPSG_IoImpl>(m_Barrier, milli::den, milli::den, m_Servers));
    }

    m_Barrier.Wait();
}

bool SPSG_IoCoordinator::AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool& stopped, const CDeadline& deadline)
{
    if (m_Io.size() == 0) {
        ERR_POST(Fatal << "IO is not open");
    }

    auto counter = m_RequestCounter++;
    const auto first = (counter++ / params.requests_per_io) % m_Io.size();
    auto idx = first;

    do {
        do {
            if (m_Io[idx]->queue.Push(move(req))) return true;

            // No room for the request

            // Try to update request counter once so the next IO thread would be tried for new requests
            if (idx == first) {
                m_RequestCounter.compare_exchange_weak(counter, counter + params.requests_per_io);
            }

            // Try next IO thread for this request, too
            idx = (idx + 1) % m_Io.size();
        }
        while (idx != first);

        this_thread::yield();
    }
    while (!deadline.IsExpired() && !stopped);

    return false;
}


END_NCBI_SCOPE

#endif
