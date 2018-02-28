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

static thread_local unique_ptr<DDRPC::DataColumns> s_accver_resolver_data_columns;

void AccVerResolverUnpackData(DDRPC::DataRow& row, const string& data)
{
    constexpr const char kACCVER_RESOLVER_COLUMNS[] =
        "ACCVER "   "CVARCHAR NOTNULL KEY, "
        "GI "       "INT8, "
        "LEN "      "UINT4, "
        "SAT "      "UINT1, "
        "SAT_KEY "  "UINT4, "
        "TAXID "    "UINT4, "
        "DATE "     "DATETIME, "
        "SUPPRESS " "BIT NOTNULL";

    if (!s_accver_resolver_data_columns) {
        DDRPC::DataColumns clm;
        clm.AssignAsText(kACCVER_RESOLVER_COLUMNS);
        s_accver_resolver_data_columns.reset(new DDRPC::DataColumns(clm.ExtractDataColumns()));
    }
    row.Unpack(data, false, *s_accver_resolver_data_columns);
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


/** CPSG_BioIdResolutionQueue::SItem */

struct CPSG_BioIdResolutionQueue::SItem
{
    SItem(const string& service, shared_ptr<HCT::io_future> afuture, CPSG_BioId bio_id);
    bool AddRequest(long wait_ms) { return SHCT::GetIoc().add_request(m_Request, wait_ms); }
    void SyncResolve(CPSG_BlobId& blob_id, const CDeadline& deadline);
    void WaitFor(long timeout_ms);
    void Wait();
    bool IsDone() const;
    void Cancel();
    void PopulateData(CPSG_BlobId& blob_id) const;
    shared_ptr<HCT::http2_request> m_Request;
    CPSG_BioId m_BioId;
};

CPSG_BioIdResolutionQueue::SItem::SItem(const string& service, shared_ptr<HCT::io_future> afuture, CPSG_BioId bio_id)
    :   m_Request(make_shared<HCT::http2_request>()),
        m_BioId(move(bio_id))
{
    m_Request->init_request(SHCT::GetEndPoint(service), afuture, "/ID/accver.resolver", "accver=" + m_BioId.GetId());
}

void CPSG_BioIdResolutionQueue::SItem::SyncResolve(CPSG_BlobId& blob_id, const CDeadline& deadline)
{
    bool has_timeout = !deadline.IsInfinite();
    long wait_ms = has_timeout ? RemainingTimeMs(deadline) : DDRPC::INDEFINITE;
    bool rv = AddRequest(wait_ms);
    if (!rv) {
        blob_id.m_Status = CPSG_BlobId::eError;
        blob_id.m_Message = "Resolver queue is full";
        return;
    }
    while (true) {
        if (IsDone()) {
            PopulateData(blob_id);
            return;
        }
        if (has_timeout) wait_ms = RemainingTimeMs(deadline);
        if (wait_ms <= 0) {
            blob_id.m_Status = CPSG_BlobId::eError;
            blob_id.m_Message = "Timeout expired";
            return;
        }
        WaitFor(wait_ms);
    }
}

void CPSG_BioIdResolutionQueue::SItem::WaitFor(long timeout_ms)
{
    m_Request->get_result_data().wait_for(timeout_ms);
}

void CPSG_BioIdResolutionQueue::SItem::Wait()
{
    m_Request->get_result_data().wait();
}

bool CPSG_BioIdResolutionQueue::SItem::IsDone() const
{
    return m_Request->get_result_data().get_finished();
}

void CPSG_BioIdResolutionQueue::SItem::Cancel()
{
    m_Request->send_cancel();
}

void CPSG_BioIdResolutionQueue::SItem::PopulateData(CPSG_BlobId& blob_id) const
{
    if (m_Request->get_result_data().get_cancelled()) {
        blob_id.m_Status = CPSG_BlobId::eCanceled;
        blob_id.m_Message = "Request for resolution was canceled";
    }
    else if (m_Request->has_error()) {
        blob_id.m_Status = CPSG_BlobId::eError;
        blob_id.m_Message = m_Request->get_error_description();
    }
    else switch (m_Request->get_result_data().get_http_status()) {
        case 200: {
            string data = m_Request->get_reply_data_move();
            DDRPC::DataRow rec;
            try {
                AccVerResolverUnpackData(rec, data);
                blob_id.m_Status = CPSG_BlobId::eSuccess;
                blob_id.m_BlobInfo.gi          = rec[0].AsInt8;
                blob_id.m_BlobInfo.seq_length  = rec[1].AsUint4;
                blob_id.m_BlobInfo.sat         = rec[2].AsUint1;
                blob_id.m_BlobInfo.sat_key     = rec[3].AsUint4;
                blob_id.m_BlobInfo.tax_id      = rec[4].AsUint4;
                blob_id.m_BlobInfo.date_queued = rec[5].AsDateTime;
                blob_id.m_BlobInfo.state       = rec[6].AsUint1;
            }
            catch (const DDRPC::EDdRpcException& e) {
                blob_id.m_Status = CPSG_BlobId::eError;
                blob_id.m_Message = e.what();
            }
            break;
        }
        case 404: {
            blob_id.m_Status = CPSG_BlobId::eNotFound;
            blob_id.m_Message = "Bio id is not found";
            break;
        }
        default: {
            blob_id.m_Status = CPSG_BlobId::eError;
            blob_id.m_Message = "Unexpected result";
        }
    }

}

/** CPSG_BioIdResolutionQueue */

CPSG_BioIdResolutionQueue::CPSG_BioIdResolutionQueue(const string& service) :
    m_Future(make_shared<HCT::io_future>()),
    m_Service(service)
{
}

CPSG_BioIdResolutionQueue::~CPSG_BioIdResolutionQueue()
{
}

void CPSG_BioIdResolutionQueue::Resolve(TPSG_BioIds* bio_ids, const CDeadline& deadline)
{
    if (!bio_ids)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> lock(m_ItemsMtx);
    long wait_ms = 0;
    auto rev_it = bio_ids->rbegin();
    while (rev_it != bio_ids->rend()) {

        auto future = static_pointer_cast<HCT::io_future>(m_Future);
        unique_ptr<SItem> qi(new SItem(m_Service, future, *rev_it));

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
                m_Items.emplace_back(move(qi));
                bio_ids->erase(next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

CPSG_BlobId CPSG_BioIdResolutionQueue::Resolve(const string& service, CPSG_BioId bio_id, const CDeadline& deadline)
{
    CPSG_BlobId rv(bio_id);
    unique_ptr<SItem> qi(new SItem(service, HCT::io_coordinator::get_tls_future(), move(bio_id)));
    qi->SyncResolve(rv, deadline);
    return rv;
}

TPSG_BlobIds CPSG_BioIdResolutionQueue::GetBlobIds(const CDeadline& deadline, size_t max_results)
{
    TPSG_BlobIds rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> lock(m_ItemsMtx);
    if (m_Items.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_Items.front();
            last->WaitFor(wait_ms);
        }
        for (auto rev_it = m_Items.rbegin(); rev_it != m_Items.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            const auto& req = *rev_it;
            if (req->IsDone()) {
                auto fw_it = next(rev_it).base();
                rv.push_back(move((*fw_it)->m_BioId));
                req->PopulateData(rv.back());
                m_Items.erase(fw_it);
            }
        }
    }
    return rv;
}

void CPSG_BioIdResolutionQueue::Clear(TPSG_BioIds* bio_ids)
{
    unique_lock<mutex> lock(m_ItemsMtx);
    if (bio_ids) {
        bio_ids->clear();
        bio_ids->reserve(m_Items.size());
    }
    for (auto& it : m_Items) {
        it->Cancel();
        if (bio_ids)
            bio_ids->emplace_back(move(it->m_BioId));
    }
    m_Items.clear();
}

bool CPSG_BioIdResolutionQueue::IsEmpty() const
{
    unique_lock<mutex> lock(m_ItemsMtx);
    return m_Items.size() == 0;
}


/* CPSG_BlobRetrievalQueue::SItem */

struct CPSG_BlobRetrievalQueue::SItem
{
    SItem(const string& service, shared_ptr<HCT::io_future> afuture, CPSG_BlobId blob_id);
    bool AddRequest(long wait_ms) { return SHCT::GetIoc().add_request(m_Request, wait_ms); }
    void SyncRetrieve(CPSG_Blob& blob, const CDeadline& deadline);
    void WaitFor(long timeout_ms);
    void Wait();
    bool IsDone() const;
    void Cancel();
    void PopulateData(CPSG_Blob& blob) const;
    shared_ptr<HCT::http2_request> m_Request;
    CPSG_BlobId m_BlobId;
};

CPSG_BlobRetrievalQueue::SItem::SItem(const string& service, shared_ptr<HCT::io_future> afuture, CPSG_BlobId blob_id)
    :   m_Request(make_shared<HCT::http2_request>()),
        m_BlobId(move(blob_id))
{
    const auto& info = blob_id.GetBlobInfo();
    string query("sat=" + to_string(info.sat) + "&sat_key=" + to_string(info.sat_key));
    m_Request->init_request(SHCT::GetEndPoint(service), afuture, "/ID/getblob", move(query));
}

void CPSG_BlobRetrievalQueue::SItem::SyncRetrieve(CPSG_Blob& blob, const CDeadline& deadline)
{
    bool has_timeout = !deadline.IsInfinite();
    long wait_ms = has_timeout ? RemainingTimeMs(deadline) : DDRPC::INDEFINITE;
    bool rv = AddRequest(wait_ms);
    if (!rv) {
        blob.m_Status = CPSG_Blob::eError;
        blob.m_Message = "Retriever queue is full";
        return;
    }
    while (true) {
        if (IsDone()) {
            PopulateData(blob);
            return;
        }
        if (has_timeout) wait_ms = RemainingTimeMs(deadline);
        if (wait_ms <= 0) {
            blob.m_Status = CPSG_Blob::eError;
            blob.m_Message = "Timeout expired";
            return;
        }
        WaitFor(wait_ms);
    }
}

void CPSG_BlobRetrievalQueue::SItem::WaitFor(long timeout_ms)
{
    m_Request->get_result_data().wait_for(timeout_ms);
}

void CPSG_BlobRetrievalQueue::SItem::Wait()
{
    m_Request->get_result_data().wait();
}

bool CPSG_BlobRetrievalQueue::SItem::IsDone() const
{
    return m_Request->get_result_data().get_finished();
}

void CPSG_BlobRetrievalQueue::SItem::Cancel()
{
    m_Request->send_cancel();
}

void CPSG_BlobRetrievalQueue::SItem::PopulateData(CPSG_Blob& blob) const
{
    if (m_Request->get_result_data().get_cancelled()) {
        blob.m_Status = CPSG_Blob::eCanceled;
        blob.m_Message = "Request for retrieval was canceled";
    }
    else if (m_Request->has_error()) {
        blob.m_Status = CPSG_Blob::eError;
        blob.m_Message = m_Request->get_error_description();
    }
    else switch (m_Request->get_result_data().get_http_status()) {
        case 200: {
            blob.m_Stream.reset(new stringstream(m_Request->get_reply_data_move()));
            blob.m_Status = CPSG_Blob::eSuccess;
            break;
        }
        case 404: {
            blob.m_Status = CPSG_Blob::eNotFound;
            blob.m_Message = "Blob is not found";
            break;
        }
        default: {
            blob.m_Status = CPSG_Blob::eError;
            blob.m_Message = "Unexpected result";
        }
    }

}

/** CPSG_BlobRetrievalQueue */

CPSG_BlobRetrievalQueue::CPSG_BlobRetrievalQueue(const string& service) :
    m_Future(make_shared<HCT::io_future>()),
    m_Service(service)
{
}

CPSG_BlobRetrievalQueue::~CPSG_BlobRetrievalQueue()
{
}

void CPSG_BlobRetrievalQueue::Retrieve(TPSG_BlobIds* blob_ids, const CDeadline& deadline)
{
    if (!blob_ids)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> lock(m_ItemsMtx);
    long wait_ms = 0;
    auto rev_it = blob_ids->rbegin();
    while (rev_it != blob_ids->rend()) {

        auto future = static_pointer_cast<HCT::io_future>(m_Future);
        unique_ptr<SItem> qi(new SItem(m_Service, future, *rev_it));

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
                m_Items.emplace_back(move(qi));
                blob_ids->erase(next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

CPSG_Blob CPSG_BlobRetrievalQueue::Retrieve(const string& service, CPSG_BlobId blob_id, const CDeadline& deadline)
{
    CPSG_Blob rv(blob_id);
    unique_ptr<SItem> qi(new SItem(service, HCT::io_coordinator::get_tls_future(), move(blob_id)));
    qi->SyncRetrieve(rv, deadline);
    return rv;
}

TPSG_Blobs CPSG_BlobRetrievalQueue::GetBlobs(const CDeadline& deadline, size_t max_results)
{
    TPSG_Blobs rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> lock(m_ItemsMtx);
    if (m_Items.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_Items.front();
            last->WaitFor(wait_ms);
        }
        for (auto rev_it = m_Items.rbegin(); rev_it != m_Items.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            const auto& req = *rev_it;
            if (req->IsDone()) {
                auto fw_it = next(rev_it).base();
                rv.push_back(move((*fw_it)->m_BlobId));
                req->PopulateData(rv.back());
                m_Items.erase(fw_it);
            }
        }
    }
    return rv;
}

void CPSG_BlobRetrievalQueue::Clear(TPSG_BlobIds* blob_ids)
{
    unique_lock<mutex> lock(m_ItemsMtx);
    if (blob_ids) {
        blob_ids->clear();
        blob_ids->reserve(m_Items.size());
    }
    for (auto& it : m_Items) {
        it->Cancel();
        if (blob_ids)
            blob_ids->emplace_back(move(it->m_BlobId));
    }
    m_Items.clear();
}

bool CPSG_BlobRetrievalQueue::IsEmpty() const
{
    unique_lock<mutex> lock(m_ItemsMtx);
    return m_Items.size() == 0;
}


END_NCBI_SCOPE
