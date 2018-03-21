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

}


struct SHCT
{
    using TIoc = HCT::io_coordinator;
    using TEndPoint = shared_ptr<HCT::http2_end_point>;

    static TIoc& GetIoc()
    {
        return *m_Ioc.second;
    }

    static TEndPoint GetEndPoint(const string& service)
    {
        auto result = m_LocalEndPoints.find(service);
        return result != m_LocalEndPoints.end() ? result->second : x_GetEndPoint(service);
    }

private:
    using TEndPoints = unordered_map<string, TEndPoint>;

    static TEndPoint x_GetEndPoint(const string& service);

    static pair<once_flag, shared_ptr<TIoc>> m_Ioc;
    static thread_local TEndPoints m_LocalEndPoints;
    static pair<mutex, TEndPoints> m_EndPoints;
};

template <class TOutput>
struct SPSG_Output : TOutput
{
    using TStatus = typename TOutput::EStatus;

    TStatus& status;
    string& message;

    SPSG_Output(TOutput base, TStatus& s, string& m) : TOutput(move(base)), status(s), message(m) {}

    void SetSuccess()               { status = TStatus::eSuccess;       message.clear();   }
    void SetNotFound(string m)      { status = TStatus::eNotFound;      message = move(m); }
    void SetCanceled(string m)      { status = TStatus::eCanceled;      message = move(m); }
    void SetTimeout(string m)       { status = TStatus::eTimeout;       message = move(m); }
    void SetUnknownError(string m)  { status = TStatus::eUnknownError;  message = move(m); }
};

template <class TItem>
struct SPSG_Queue
{
    using TInput = typename TItem::TInput;
    using TOutput = typename TItem::TOutput;

    static void ExecuteImpl(TOutput& output, TItem& item, const CDeadline& deadline);
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

        SItem(const string& service, shared_ptr<HCT::io_future> afuture, TInput input);

        bool AddRequest(long wait_ms) { return SHCT::GetIoc().add_request(m_Request, wait_ms); }
        void WaitFor(long timeout_ms);
        bool IsDone() const;
        void Cancel();
        void Process(TOutput& output);

        shared_ptr<HCT::http2_request> m_Request;
        TInput m_Input;
    };

    struct TQueue
    {
        using TItem = SItem;
        using TInput = CPSG_BioId;
        using TOutput = SOutput;
        using TInputSet = vector<TInput>;
        using TOutputSet = vector<CPSG_BlobId>;

        TQueue(const string& service);

        void Push(TInputSet* input_set, const CDeadline& deadline);
        static CPSG_BlobId Execute(const string& service, TInput input, const CDeadline& deadline);
        TOutputSet Pop(const CDeadline& deadline, size_t max_results);
        void Clear(TInputSet* input_set);
        bool IsEmpty() const;

    private:
        mutable pair<vector<unique_ptr<TItem>>, mutex> m_Items;
        shared_ptr<void> m_Future;
        string m_Service;
    };

    TQueue q;

    SImpl(const string& service) : q(service) {}
};

struct CPSG_BlobRetrievalQueue::SImpl
{
    struct SSatOutput : SPSG_Output<CPSG_Blob>
    {
        using TBase = SPSG_Output<CPSG_Blob>;
        SSatOutput(CPSG_Blob base) : TBase(move(base), TBase::m_Status, TBase::m_Message) {}
    };

    struct SSatItem
    {
        using TInput = CPSG_BlobId;
        using TOutput = SSatOutput;

        SSatItem(const string& service, shared_ptr<HCT::io_future> afuture, TInput input);

        bool AddRequest(long wait_ms) { return SHCT::GetIoc().add_request(m_Request, wait_ms); }
        void WaitFor(long timeout_ms);
        bool IsDone() const;
        void Cancel();
        void Process(TOutput& output);

        unique_ptr<iostream> m_Stream;
        shared_ptr<HCT::http2_request> m_Request;
        TInput m_Input;
    };

    struct TSatQueue
    {
        using TItem = SSatItem;
        using TInput = CPSG_BlobId;
        using TOutput = SSatOutput;
        using TInputSet = vector<TInput>;
        using TOutputSet = vector<CPSG_Blob>;

        TSatQueue(const string& service);

        void Push(TInputSet* input_set, const CDeadline& deadline);
        static CPSG_Blob Execute(const string& service, TInput input, const CDeadline& deadline);
        TOutputSet Pop(const CDeadline& deadline, size_t max_results);
        void Clear(TInputSet* input_set);
        bool IsEmpty() const;

    private:
        mutable pair<vector<unique_ptr<TItem>>, mutex> m_Items;
        shared_ptr<void> m_Future;
        string m_Service;
    };

    TSatQueue sat_q;

    SImpl(const string& service) : sat_q(service) {}
};


pair<once_flag, shared_ptr<SHCT::TIoc>> SHCT::m_Ioc;
thread_local SHCT::TEndPoints SHCT::m_LocalEndPoints;
pair<mutex, SHCT::TEndPoints> SHCT::m_EndPoints;

SHCT::TEndPoint SHCT::x_GetEndPoint(const string& service)
{
    call_once(m_Ioc.first, [&]() { m_Ioc.second = make_shared<TIoc>(); });

    lock_guard<mutex> lock(m_EndPoints.first);
    auto result = m_EndPoints.second.emplace(service, nullptr);
    auto& pair = *result.first;

    // If actually added, initialize
    if (result.second) {
        pair.second.reset(new HCT::http2_end_point{"http", service});
    }

    m_LocalEndPoints.insert(pair);
    return pair.second;
}


template <class TItem>
void SPSG_Queue<TItem>::ExecuteImpl(TOutput& output, TItem& item, const CDeadline& deadline)
{
    bool has_timeout = !deadline.IsInfinite();
    long wait_ms = has_timeout ? RemainingTimeMs(deadline) : DDRPC::INDEFINITE;
    bool rv = item.AddRequest(wait_ms);
    if (!rv) {
        output.SetTimeout("Timeout on adding request");
        return;
    }
    while (true) {
        if (item.IsDone()) {
            item.Process(output);
            return;
        }
        if (has_timeout) wait_ms = RemainingTimeMs(deadline);
        if (wait_ms <= 0) {
            item.Cancel();
            output.SetTimeout("Timeout on waiting result");
            return;
        }
        item.WaitFor(wait_ms);
    }
}


CPSG_BioIdResolutionQueue::SImpl::SItem::SItem(const string& service, shared_ptr<HCT::io_future> afuture, TInput input)
    :   m_Request(make_shared<HCT::http2_request>()),
        m_Input(move(input))
{
    m_Request->init_request(SHCT::GetEndPoint(service), afuture, "/ID/resolve", "accession=" + m_Input.GetId());
}

void CPSG_BioIdResolutionQueue::SImpl::SItem::WaitFor(long timeout_ms)
{
    m_Request->get_result_data().wait_for(timeout_ms);
}

bool CPSG_BioIdResolutionQueue::SImpl::SItem::IsDone() const
{
    return m_Request->get_result_data().get_finished();
}

void CPSG_BioIdResolutionQueue::SImpl::SItem::Cancel()
{
    m_Request->send_cancel();
}

void CPSG_BioIdResolutionQueue::SImpl::SItem::Process(TOutput& output)
{
    if (m_Request->get_result_data().get_cancelled()) {
        output.SetCanceled("Request was canceled");
    }
    else if (m_Request->has_error()) {
        output.SetUnknownError(m_Request->get_error_description());
    }
    else switch (m_Request->get_result_data().get_http_status()) {
        case 200: {
            string data = m_Request->get_reply_data_move();
            DDRPC::DataRow rec;
            try {
                DDRPC::AccVerResolverUnpackData(rec, data);
                output.SetSuccess();
                output.m_BlobInfo.gi               = rec[0].AsInt8;
                output.m_BlobInfo.seq_length       = rec[1].AsUint4;
                output.m_BlobInfo.sat              = rec[2].AsUint1;
                output.m_BlobInfo.sat_key          = rec[3].AsUint4;
                output.m_BlobInfo.tax_id           = rec[4].AsUint4;
                output.m_BlobInfo.last_modified    = rec[5].AsDateTime;
                output.m_BlobInfo.state            = rec[6].AsUint1;
            }
            catch (const DDRPC::EDdRpcException& e) {
                output.SetUnknownError(e.what());
            }
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


CPSG_BioIdResolutionQueue::SImpl::TQueue::TQueue(const string& service) :
    m_Future(make_shared<HCT::io_future>()),
    m_Service(service)
{
}

void CPSG_BioIdResolutionQueue::SImpl::TQueue::Push(TInputSet* input_set, const CDeadline& deadline)
{
    if (!input_set)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> lock(m_Items.second);
    long wait_ms = 0;
    auto rev_it = input_set->rbegin();
    while (rev_it != input_set->rend()) {

        auto future = static_pointer_cast<HCT::io_future>(m_Future);
        unique_ptr<TItem> qi(new TItem(m_Service, future, *rev_it));

        while (true) {
            bool rv = qi->AddRequest(wait_ms);

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
                m_Items.first.emplace_back(move(qi));
                input_set->erase(next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

CPSG_BlobId CPSG_BioIdResolutionQueue::SImpl::TQueue::Execute(const string& service, TInput input, const CDeadline& deadline)
{
    auto future = make_shared<HCT::io_future>();
    TOutput rv(input);
    unique_ptr<TItem> qi(new TItem(service, future, move(input)));
    SPSG_Queue<TItem>::ExecuteImpl(rv, *qi, deadline);
    return move(rv);
}

TPSG_BlobIds CPSG_BioIdResolutionQueue::SImpl::TQueue::Pop(const CDeadline& deadline, size_t max_results)
{
    TOutputSet rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> lock(m_Items.second);
    if (m_Items.first.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_Items.first.front();
            last->WaitFor(wait_ms);
        }
        for (auto rev_it = m_Items.first.rbegin(); rev_it != m_Items.first.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            const auto& req = *rev_it;
            if (req->IsDone()) {
                auto fw_it = next(rev_it).base();
                TOutput output(move((*fw_it)->m_Input));
                req->Process(output);
                rv.push_back(move(output));
                m_Items.first.erase(fw_it);
            }
        }
    }
    return rv;
}

void CPSG_BioIdResolutionQueue::SImpl::TQueue::Clear(TInputSet* input_set)
{
    unique_lock<mutex> lock(m_Items.second);
    if (input_set) {
        input_set->clear();
        input_set->reserve(m_Items.first.size());
    }
    for (auto& it : m_Items.first) {
        it->Cancel();
        if (input_set)
            input_set->emplace_back(move(it->m_Input));
    }
    m_Items.first.clear();
}

bool CPSG_BioIdResolutionQueue::SImpl::TQueue::IsEmpty() const
{
    unique_lock<mutex> lock(m_Items.second);
    return m_Items.first.size() == 0;
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


CPSG_BlobRetrievalQueue::SImpl::SSatItem::SSatItem(const string& service, shared_ptr<HCT::io_future> afuture, TInput input)
    :   m_Stream(new stringstream),
        m_Request(make_shared<HCT::http2_request>(m_Stream.get())),
        m_Input(move(input))
{
    const auto& info = input.GetBlobInfo();
    string query("sat=" + to_string(info.sat) + "&sat_key=" + to_string(info.sat_key));
    m_Request->init_request(SHCT::GetEndPoint(service), afuture, "/ID/getblob", move(query));
}

void CPSG_BlobRetrievalQueue::SImpl::SSatItem::WaitFor(long timeout_ms)
{
    m_Request->get_result_data().wait_for(timeout_ms);
}

bool CPSG_BlobRetrievalQueue::SImpl::SSatItem::IsDone() const
{
    return m_Request->get_result_data().get_finished();
}

void CPSG_BlobRetrievalQueue::SImpl::SSatItem::Cancel()
{
    m_Request->send_cancel();
}

void CPSG_BlobRetrievalQueue::SImpl::SSatItem::Process(TOutput& output)
{
    if (m_Request->get_result_data().get_cancelled()) {
        output.SetCanceled("Request was canceled");
    }
    else if (m_Request->has_error()) {
        output.SetUnknownError(m_Request->get_error_description());
    }
    else switch (m_Request->get_result_data().get_http_status()) {
        case 200: {
            output.m_Stream = move(m_Stream);
            output.SetSuccess();
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


CPSG_BlobRetrievalQueue::SImpl::TSatQueue::TSatQueue(const string& service) :
    m_Future(make_shared<HCT::io_future>()),
    m_Service(service)
{
}

CPSG_BlobRetrievalQueue::~CPSG_BlobRetrievalQueue()
{
}

void CPSG_BlobRetrievalQueue::SImpl::TSatQueue::Push(TInputSet* input_set, const CDeadline& deadline)
{
    if (!input_set)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> lock(m_Items.second);
    long wait_ms = 0;
    auto rev_it = input_set->rbegin();
    while (rev_it != input_set->rend()) {

        auto future = static_pointer_cast<HCT::io_future>(m_Future);
        unique_ptr<TItem> qi(new TItem(m_Service, future, *rev_it));

        while (true) {
            bool rv = qi->AddRequest(wait_ms);

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
                m_Items.first.emplace_back(move(qi));
                input_set->erase(next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

CPSG_Blob CPSG_BlobRetrievalQueue::SImpl::TSatQueue::Execute(const string& service, TInput input, const CDeadline& deadline)
{
    auto future = make_shared<HCT::io_future>();
    TOutput rv(input);
    unique_ptr<TItem> qi(new TItem(service, future, move(input)));
    SPSG_Queue<TItem>::ExecuteImpl(rv, *qi, deadline);
    return move(rv);
}

TPSG_Blobs CPSG_BlobRetrievalQueue::SImpl::TSatQueue::Pop(const CDeadline& deadline, size_t max_results)
{
    TOutputSet rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> lock(m_Items.second);
    if (m_Items.first.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_Items.first.front();
            last->WaitFor(wait_ms);
        }
        for (auto rev_it = m_Items.first.rbegin(); rev_it != m_Items.first.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            const auto& req = *rev_it;
            if (req->IsDone()) {
                auto fw_it = next(rev_it).base();
                TOutput output(move((*fw_it)->m_Input));
                req->Process(output);
                rv.push_back(move(output));
                m_Items.first.erase(fw_it);
            }
        }
    }
    return rv;
}

void CPSG_BlobRetrievalQueue::SImpl::TSatQueue::Clear(TInputSet* input_set)
{
    unique_lock<mutex> lock(m_Items.second);
    if (input_set) {
        input_set->clear();
        input_set->reserve(m_Items.first.size());
    }
    for (auto& it : m_Items.first) {
        it->Cancel();
        if (input_set)
            input_set->emplace_back(move(it->m_Input));
    }
    m_Items.first.clear();
}

bool CPSG_BlobRetrievalQueue::SImpl::TSatQueue::IsEmpty() const
{
    unique_lock<mutex> lock(m_Items.second);
    return m_Items.first.size() == 0;
}


CPSG_BlobRetrievalQueue::CPSG_BlobRetrievalQueue(const string& service) :
    m_Impl(new SImpl(service))
{
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
    return m_Impl->sat_q.Pop(deadline, max_results);
}

void CPSG_BlobRetrievalQueue::Clear(TPSG_BlobIds* blob_ids)
{
    m_Impl->sat_q.Clear(blob_ids);
}

bool CPSG_BlobRetrievalQueue::IsEmpty() const
{
    return m_Impl->sat_q.IsEmpty();
}


END_NCBI_SCOPE
