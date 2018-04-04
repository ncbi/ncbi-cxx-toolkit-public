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
 * Author: Dmitri Dmitrienko
 *
 */

#include <ncbi_pch.hpp>

#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <type_traits>

#include <corelib/ncbitime.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_service.h>
#include <connect/ncbi_connutil.h>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcCommon.hpp>
#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>
#include "HttpClientTransportP.hpp"
#include <objtools/pubseq_gateway/client/psg_client.hpp>

BEGIN_NCBI_SCOPE


static_assert(is_nothrow_move_constructible<CPSG_BioId>::value, "CPSG_BioId move constructor must be noexcept");
static_assert(is_nothrow_move_constructible<CPSG_BlobId>::value, "CPSG_BlobId move constructor must be noexcept");
static_assert(is_nothrow_move_constructible<CPSG_Blob>::value, "CPSG_Blob move constructor must be noexcept");

namespace {

long RemainingTimeMs(const CDeadline& deadline)
{
    unsigned int sec, nanosec;
    deadline.GetRemainingTime().GetNano(&sec, &nanosec);
    return nanosec / 1000000L + sec * 1000L;
}

template <class TOutput>
void s_UnpackData(TOutput& output, CPSG_BlobId::SBlobInfo& blob_info, string data)
{
    try {
        DDRPC::DataRow rec;
        DDRPC::AccVerResolverUnpackData(rec, data);
        blob_info.gi               = rec[0].AsInt8;
        blob_info.seq_length       = rec[1].AsUint4;
        blob_info.sat              = rec[2].AsUint1;
        blob_info.sat_key          = rec[3].AsUint4;
        blob_info.tax_id           = rec[4].AsUint4;
        blob_info.last_modified    = rec[5].AsDateTime;
        blob_info.state            = rec[6].AsUint1;
        output.SetSuccess();
    }
    catch (const DDRPC::EDdRpcException& e) {
        output.SetUnknownError(e.what());
    }
}

}


using TPSG_IocValue = HCT::io_coordinator;
using TPSG_Ioc = shared_ptr<TPSG_IocValue>;

using TPSG_EndPointValue = HCT::http2_end_point;
using TPSG_EndPoint = shared_ptr<TPSG_EndPointValue>;
using TPSG_EndPoints = unordered_map<string, TPSG_EndPoint>;

using TPSG_RequestValue = HCT::http2_request;
using TPSG_Request = shared_ptr<TPSG_RequestValue>;

using TPSG_FutureValue = HCT::io_future;
using TPSG_Future = shared_ptr<TPSG_FutureValue>;

struct SHCT
{
    static TPSG_IocValue& GetIoc()
    {
        return *m_Ioc.second;
    }

    static TPSG_EndPoint GetEndPoint(const string& service)
    {
        auto result = m_LocalEndPoints.find(service);
        return result != m_LocalEndPoints.end() ? result->second : x_GetEndPoint(service);
    }

private:
    static TPSG_EndPoint x_GetEndPoint(const string& service);

    static pair<once_flag, TPSG_Ioc> m_Ioc;
    static thread_local TPSG_EndPoints m_LocalEndPoints;
    static pair<mutex, TPSG_EndPoints> m_EndPoints;
};

template <class TOutput>
struct SPSG_Result
{
    using TStatus = typename TOutput::EStatus;

    TStatus& status;
    string& message;

    SPSG_Result(TStatus& s, string& m) : status(s), message(m) {}

    void SetSuccess()               { status = TStatus::eSuccess;       message.clear();   }
    void SetNotFound(string m)      { status = TStatus::eNotFound;      message = move(m); }
    void SetCanceled(string m)      { status = TStatus::eCanceled;      message = move(m); }
    void SetTimeout(string m)       { status = TStatus::eTimeout;       message = move(m); }
    void SetUnknownError(string m)  { status = TStatus::eUnknownError;  message = move(m); }
};

template <class TOutput>
struct SPSG_Output : TOutput, SPSG_Result<TOutput>
{
    using TStatus = typename TOutput::EStatus;

    SPSG_Output(TOutput base, TStatus& s, string& m) :
        TOutput(move(base)),
        SPSG_Result<TOutput>(s, m)
    {
    }
};

template <class TBase>
struct SPSG_Item : TBase
{
    using TInput = typename TBase::TInput;
    using TOutput = typename TBase::TOutput;
    using TOutputSet = typename TBase::TOutputSet;

    SPSG_Item(const string& service, TPSG_Future afuture, TInput input);

    bool AddRequest(long wait_ms) { return SHCT::GetIoc().add_request(m_Request, wait_ms); }

    void WaitFor(long timeout_ms) {        m_Request->get_result_data().wait_for(timeout_ms); }
    bool IsDone() const           { return m_Request->get_result_data().get_finished(); }
    void Cancel()                 {        m_Request->send_cancel(); }

    void Process(TOutput& output);

    TPSG_Request m_Request;
    TInput m_Input;
};

template <class TItem>
struct SPSG_Queue
{
    using TInput = typename TItem::TInput;
    using TOutput = typename TItem::TOutput;

    using TInputSet = vector<TInput>;
    using TOutputSet = typename TItem::TOutputSet;

    SPSG_Queue(const string& service);

    void Push(TInputSet* input_set, const CDeadline& deadline);
    TOutputSet Pop(const CDeadline& deadline, size_t max_results);
    void Clear(TInputSet* input_set);
    bool IsEmpty() const;

    static TOutput Execute(const string& service, TInput input, const CDeadline& deadline);

private:
    mutable pair<vector<TItem>, mutex> m_Items;
    shared_ptr<void> m_Future;
    string m_Service;
};

struct CPSG_BioIdResolutionQueue::SImpl
{
    struct SOutput : SPSG_Output<CPSG_BlobId>
    {
        using TBase = SPSG_Output<CPSG_BlobId>;
        SOutput(CPSG_BlobId base) : TBase(move(base), TBase::m_Status, TBase::m_Message) {}
    };

    struct SItem
    {
        using TInput = CPSG_BioId;
        using TOutput = SOutput;
        using TOutputSet = vector<CPSG_BlobId>;

        TPSG_Request Request(const string& service, TPSG_Future afuture, TInput input);
        void Complete(TOutput& output, TPSG_Request request);

        static TOutput Output(TInput input) { return TOutput(move(input)); }
    };

    using TQueue = SPSG_Queue<SPSG_Item<SItem>>;

    TQueue q;

    SImpl(const string& service) : q(service) {}
};

struct CPSG_BlobRetrievalQueue::SImpl
{
    struct SAccOutput : SPSG_Output<CPSG_Blob>
    {
        using TBase = SPSG_Output<CPSG_Blob>;
        using TBlobIdResult = SPSG_Result<CPSG_BlobId>;

        TBlobIdResult blob_id_result;

        SAccOutput(CPSG_Blob base) :
            TBase(move(base), TBase::m_Status, TBase::m_Message),
            blob_id_result(TBase::m_BlobId.m_Status, TBase::m_BlobId.m_Message)
        {
        }
    };

    struct SSatOutput : SPSG_Output<CPSG_Blob>
    {
        using TBase = SPSG_Output<CPSG_Blob>;
        SSatOutput(CPSG_Blob base) : TBase(move(base), TBase::m_Status, TBase::m_Message) {}
    };

    struct SItem
    {
        using TOutputSet = vector<CPSG_Blob>;

        TPSG_Request Request(const string& service, TPSG_Future afuture, string query);

        unique_ptr<stringstream> m_Stream;
    };

    struct SAccItem : SItem
    {
        using TInput = CPSG_BioId;
        using TOutput = SAccOutput;

        TPSG_Request Request(const string& service, TPSG_Future afuture, TInput input);
        void Complete(TOutput& output, TPSG_Request request);

        static TOutput Output(TInput input) { return TOutput(CPSG_BlobId(move(input))); }
    };

    struct SSatItem : SItem
    {
        using TInput = CPSG_BlobId;
        using TOutput = SSatOutput;

        TPSG_Request Request(const string& service, TPSG_Future afuture, TInput input);
        void Complete(TOutput& output, TPSG_Request request);

        static TOutput Output(TInput input) { return TOutput(move(input)); }
    };

    using TAccQueue = SPSG_Queue<SPSG_Item<SAccItem>>;
    using TSatQueue = SPSG_Queue<SPSG_Item<SSatItem>>;

    TAccQueue acc_q;
    TSatQueue sat_q;

    SImpl(const string& service) : acc_q(service), sat_q(service) {}
};


pair<once_flag, TPSG_Ioc> SHCT::m_Ioc;
thread_local TPSG_EndPoints SHCT::m_LocalEndPoints;
pair<mutex, TPSG_EndPoints> SHCT::m_EndPoints;

string s_Resolve(const string& service)
{
    const auto s = service.c_str();
    auto net_info = make_c_unique(ConnNetInfo_Create(s), ConnNetInfo_Destroy);
    auto it = make_c_unique(SERV_Open(s, fSERV_All, SERV_LOCALHOST, net_info.get()), SERV_Close);

    // The service was not found
    if (!it) return service;

    for (;;) {
        // No need to free info after, it is done by SERV_Close
        const auto info = SERV_GetNextInfo(it.get());

        // No more servers
        if (!info) return service;

        if (info->time > 0 && info->time != NCBI_TIME_INFINITE && info->rate > 0) {
            return CSocketAPI::ntoa(info->host) + ":" + to_string(info->port);
        }
    }
}

TPSG_EndPoint SHCT::x_GetEndPoint(const string& service)
{
    call_once(m_Ioc.first, [&]() { m_Ioc.second = make_shared<TPSG_IocValue>(); });

    lock_guard<mutex> lock(m_EndPoints.first);
    auto result = m_EndPoints.second.emplace(service, nullptr);
    auto& pair = *result.first;

    // If actually added, initialize
    if (result.second) {
        pair.second.reset(new TPSG_EndPointValue{"http", s_Resolve(service)});
    }

    m_LocalEndPoints.insert(pair);
    return pair.second;
}


template <class TItem>
typename TItem::TOutput SPSG_Queue<TItem>::Execute(const string& service, TInput input, const CDeadline& deadline)
{
    TOutput output(TItem::Output(input));
    auto future = make_shared<TPSG_FutureValue>();
    TItem item(service, future, move(input));
    bool has_timeout = !deadline.IsInfinite();
    long wait_ms = has_timeout ? RemainingTimeMs(deadline) : DDRPC::INDEFINITE;
    bool rv = item.AddRequest(wait_ms);
    if (!rv) {
        output.SetTimeout("Timeout on adding request");
        return output;
    }
    while (true) {
        if (item.IsDone()) {
            item.Process(output);
            return output;
        }
        if (has_timeout) wait_ms = RemainingTimeMs(deadline);
        if (wait_ms <= 0) {
            item.Cancel();
            output.SetTimeout("Timeout on waiting result");
            return output;
        }
        item.WaitFor(wait_ms);
    }
}


template <class TBase>
SPSG_Item<TBase>::SPSG_Item(const string& service, TPSG_Future afuture, TInput input)
    :   m_Request(TBase::Request(service, afuture, input)),
        m_Input(move(input))
{
}

template <class TBase>
void SPSG_Item<TBase>::Process(TOutput& output)
{
    if (m_Request->get_result_data().get_cancelled()) {
        output.SetCanceled("Request was canceled");
    }
    else if (m_Request->has_error()) {
        output.SetUnknownError(m_Request->get_error_description());
    }
    else switch (m_Request->get_result_data().get_http_status()) {
        case 200: {
            TBase::Complete(output, m_Request);
            break;
        }
        case 404: {
            output.SetNotFound("Not found");
            break;
        }
        default: {
            output.SetUnknownError("Unexpected result");
        }
    }

}


template <class TItem>
SPSG_Queue<TItem>::SPSG_Queue(const string& service) :
    m_Future(make_shared<TPSG_FutureValue>()),
    m_Service(service)
{
}

template <class TItem>
void SPSG_Queue<TItem>::Push(TInputSet* input_set, const CDeadline& deadline)
{
    if (!input_set)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> lock(m_Items.second);
    long wait_ms = 0;
    auto rev_it = input_set->rbegin();
    while (rev_it != input_set->rend()) {

        auto future = static_pointer_cast<TPSG_FutureValue>(m_Future);
        TItem item(m_Service, future, *rev_it);

        while (true) {
            bool rv = item.AddRequest(wait_ms);

            if (!rv) { // internal queue is full
                if (has_timeout) {
                    wait_ms = RemainingTimeMs(deadline);
                    if (wait_ms < 0)
                        break;
                }
                else
                    wait_ms = DDRPC::INDEFINITE;
            }
            else {
                m_Items.first.emplace_back(move(item));
                input_set->erase(next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

template <class TItem>
typename TItem::TOutputSet SPSG_Queue<TItem>::Pop(const CDeadline& deadline, size_t max_results)
{
    TOutputSet rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> lock(m_Items.second);
    if (m_Items.first.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_Items.first.front();
            last.WaitFor(wait_ms);
        }
        for (auto rev_it = m_Items.first.rbegin(); rev_it != m_Items.first.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            if (rev_it->IsDone()) {
                auto fw_it = next(rev_it).base();
                auto output = TItem::Output(move(fw_it->m_Input));
                rev_it->Process(output);
                rv.push_back(move(output));
                m_Items.first.erase(fw_it);
            }
        }
    }
    return rv;
}

template <class TItem>
void SPSG_Queue<TItem>::Clear(TInputSet* input_set)
{
    unique_lock<mutex> lock(m_Items.second);
    if (input_set) {
        input_set->clear();
        input_set->reserve(m_Items.first.size());
    }
    for (auto& it : m_Items.first) {
        it.Cancel();
        if (input_set)
            input_set->emplace_back(move(it.m_Input));
    }
    m_Items.first.clear();
}

template <class TItem>
bool SPSG_Queue<TItem>::IsEmpty() const
{
    unique_lock<mutex> lock(m_Items.second);
    return m_Items.first.size() == 0;
}


TPSG_Request CPSG_BioIdResolutionQueue::SImpl::SItem::Request(const string& service, TPSG_Future afuture, TInput input)
{
    auto rv = make_shared<TPSG_RequestValue>();
    rv->init_request(SHCT::GetEndPoint(service), afuture, "/ID/resolve", "accession=" + input.GetId());
    return rv;
}

void CPSG_BioIdResolutionQueue::SImpl::SItem::Complete(TOutput& output, TPSG_Request request)
{
    s_UnpackData(output, output.m_BlobInfo, request->get_reply_data_move());
}


CPSG_BioIdResolutionQueue::CPSG_BioIdResolutionQueue(const string& service) :
    m_Impl(new SImpl(service))
{
}

CPSG_BioIdResolutionQueue::~CPSG_BioIdResolutionQueue()
{
}

void CPSG_BioIdResolutionQueue::Resolve(TPSG_BioIds* bio_ids, const CDeadline& deadline)
{
    m_Impl->q.Push(bio_ids, deadline);
}

CPSG_BlobId CPSG_BioIdResolutionQueue::Resolve(const string& service, CPSG_BioId bio_id, const CDeadline& deadline)
{
    return SImpl::TQueue::Execute(service, bio_id, deadline);
}

TPSG_BlobIds CPSG_BioIdResolutionQueue::GetBlobIds(const CDeadline& deadline, size_t max_results)
{
    return m_Impl->q.Pop(deadline, max_results);
}

void CPSG_BioIdResolutionQueue::Clear(TPSG_BioIds* bio_ids)
{
    m_Impl->q.Clear(bio_ids);
}

bool CPSG_BioIdResolutionQueue::IsEmpty() const
{
    return m_Impl->q.IsEmpty();
}


TPSG_Request CPSG_BlobRetrievalQueue::SImpl::SItem::Request(const string& service, TPSG_Future afuture, string query)
{
    m_Stream.reset(new stringstream);
    auto rv = make_shared<TPSG_RequestValue>(m_Stream.get());
    rv->init_request(SHCT::GetEndPoint(service), afuture, "/ID/getblob", move(query));
    return rv;
}


TPSG_Request CPSG_BlobRetrievalQueue::SImpl::SAccItem::Request(const string& service, TPSG_Future afuture, TInput input)
{
    string query("accession=" + input.GetId());
    return SItem::Request(service , afuture, move(query));
}

void CPSG_BlobRetrievalQueue::SImpl::SAccItem::Complete(TOutput& output, TPSG_Request)
{
    uint32_t data_size = 0;
    m_Stream->read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
    data_size = ntohl(data_size);
    
    string data(data_size, 0);
    m_Stream->read(&data.front(), data.size());
    s_UnpackData(output.blob_id_result, output.m_BlobId.m_BlobInfo, data);

    auto blob_size = m_Stream->str().size() - data_size - sizeof(data_size);
    string blob(blob_size, 0);
    m_Stream->read(&blob.front(), blob.size());
    m_Stream->str(blob);

    output.m_Stream = move(m_Stream);
    output.SetSuccess();
}


TPSG_Request CPSG_BlobRetrievalQueue::SImpl::SSatItem::Request(const string& service, TPSG_Future afuture, TInput input)
{
    const auto& info = input.GetBlobInfo();
    string query("sat=" + to_string(info.sat) + "&sat_key=" + to_string(info.sat_key));
    return SItem::Request(service , afuture, move(query));
}

void CPSG_BlobRetrievalQueue::SImpl::SSatItem::Complete(TOutput& output, TPSG_Request)
{
    output.m_Stream = move(m_Stream);
    output.SetSuccess();
}


CPSG_BlobRetrievalQueue::~CPSG_BlobRetrievalQueue()
{
}


CPSG_BlobRetrievalQueue::CPSG_BlobRetrievalQueue(const string& service) :
    m_Impl(new SImpl(service))
{
}

void CPSG_BlobRetrievalQueue::Retrieve(TPSG_BioIds* bio_ids, const CDeadline& deadline)
{
    m_Impl->acc_q.Push(bio_ids, deadline);
}

CPSG_Blob CPSG_BlobRetrievalQueue::Retrieve(const string& service, CPSG_BioId bio_id, const CDeadline& deadline)
{
    return SImpl::TAccQueue::Execute(service, bio_id, deadline);
}

void CPSG_BlobRetrievalQueue::Retrieve(TPSG_BlobIds* blob_ids, const CDeadline& deadline)
{
    m_Impl->sat_q.Push(blob_ids, deadline);
}

CPSG_Blob CPSG_BlobRetrievalQueue::Retrieve(const string& service, CPSG_BlobId blob_id, const CDeadline& deadline)
{
    return SImpl::TSatQueue::Execute(service, blob_id, deadline);
}

TPSG_Blobs CPSG_BlobRetrievalQueue::GetBlobs(const CDeadline& deadline, size_t max_results)
{
    auto acc_rv = m_Impl->acc_q.Pop(deadline, max_results);
    auto sat_rv = m_Impl->sat_q.Pop(deadline, max_results);
    acc_rv.insert(acc_rv.end(), make_move_iterator(sat_rv.begin()), make_move_iterator(sat_rv.end()));
    return acc_rv;
}

void CPSG_BlobRetrievalQueue::Clear(TPSG_BioIds* bio_ids, TPSG_BlobIds* blob_ids)
{
    m_Impl->acc_q.Clear(bio_ids);
    m_Impl->sat_q.Clear(blob_ids);
}

bool CPSG_BlobRetrievalQueue::IsEmpty() const
{
    return m_Impl->acc_q.IsEmpty() && m_Impl->sat_q.IsEmpty();
}


END_NCBI_SCOPE
