#include <ncbi_pch.hpp>

#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <type_traits>

#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/rpc/DdRpcCommon.hpp>
#include <objtools/pubseq_gateway/rpc/DdRpcClient.hpp>
#include <objtools/pubseq_gateway/rpc/DdRpcDataPacker.hpp>
#include <objtools/pubseq_gateway/rpc/HttpClientTransportP.hpp>
#include <objtools/pubseq_gateway/ResolverClient.hpp>



BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

static_assert(std::is_nothrow_move_constructible<CBioId>::value, "CBioId move constructor must be noexcept");
static_assert(std::is_nothrow_move_constructible<CBlobId>::value, "CBlobId move constructor must be noexcept");

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
    if (!s_accver_resolver_data_columns) {
        DDRPC::DataColumns clm;
        clm.AssignAsText(ACCVER_RESOLVER_COLUMNS);
        s_accver_resolver_data_columns.reset(new DDRPC::DataColumns(clm.ExtractDataColumns()));
    }
    row.Unpack(data, false, *s_accver_resolver_data_columns);
}

void DumpEntry(const CBlobId& BlobId)
{
    cout << BlobId.GetBioId().GetId() << " => sat: " << BlobId.GetBlobId().sat << " => sat_key: " << BlobId.GetBlobId().sat_key << endl;
}

}

/** CBioIdResolutionQueue::CBioIdResolutionQueueItem */

CBioIdResolutionQueue::CBioIdResolutionQueueItem::CBioIdResolutionQueueItem(std::shared_ptr<HCT::io_future> afuture, CBioId BioId) 
    :   m_request(make_shared<HCT::http2_request>()),
        m_BioId(std::move(BioId))
{
    if (!HCT::HttpClientTransport::s_ioc)
        DDRPC::EDdRpcException::raise("DDRPC is not initialized, call DdRpcClient::Init() first");
    m_request->init_request(DDRPC::DdRpcClient::SericeIdToEndPoint(ACCVER_RESOLVER_SERVICE_ID), afuture, "accver=" + m_BioId.GetId());
}

void CBioIdResolutionQueue::CBioIdResolutionQueueItem::SyncResolve(CBlobId& BlobId, const CDeadline& deadline) {
    long wait_ms = RemainingTimeMs(deadline);
    bool rv = HCT::HttpClientTransport::s_ioc->add_request(m_request, wait_ms);
    if (!rv) {
        BlobId.m_Status = CBlobId::eFailed;
        BlobId.m_StatusEx = CBlobId::eError;
        BlobId.m_Message = "Resolver queue is full";
        return;
    }
    while (true) {
        if (IsDone()) {
            PopulateData(BlobId);
            return;
        }
        wait_ms = RemainingTimeMs(deadline);
        if (wait_ms <= 0) {
            BlobId.m_Status = CBlobId::eFailed;
            BlobId.m_StatusEx = CBlobId::eError;
            BlobId.m_Message = "Timeout expired";
            return;
        }
        WaitFor(wait_ms);
    }
}

void CBioIdResolutionQueue::CBioIdResolutionQueueItem::WaitFor(long timeout_ms)
{
    m_request->get_result_data().wait_for(timeout_ms);
}

void CBioIdResolutionQueue::CBioIdResolutionQueueItem::Wait()
{
    m_request->get_result_data().wait();
}

bool CBioIdResolutionQueue::CBioIdResolutionQueueItem::IsDone() const 
{
    return m_request->get_result_data().get_finished();
}

void CBioIdResolutionQueue::CBioIdResolutionQueueItem::Cancel()
{
    m_request->send_cancel();
}

void CBioIdResolutionQueue::CBioIdResolutionQueueItem::PopulateData(CBlobId& BlobId) const
{
    if (m_request->get_result_data().get_cancelled()) {
        BlobId.m_Status = CBlobId::eFailed;
        BlobId.m_StatusEx = CBlobId::eCanceled;
        BlobId.m_Message = "Request for resolution was canceled";
    }
    else if (m_request->has_error()) {
        BlobId.m_Status = CBlobId::eFailed;
        BlobId.m_StatusEx = CBlobId::eError;
        BlobId.m_Message = m_request->get_error_description();
    }
    else switch (m_request->get_result_data().get_http_status()) {
        case 200: {
            string data = m_request->get_reply_data_move();
            DDRPC::DataRow rec;
            try {
                AccVerResolverUnpackData(rec, data);
                BlobId.m_Status = CBlobId::eResolved;
                BlobId.m_StatusEx = CBlobId::eNone;
                BlobId.m_BlobInfo.gi              = rec[0].AsUint8;
                BlobId.m_BlobInfo.seq_length      = rec[1].AsUint4;
                BlobId.m_BlobInfo.blob_id.sat     = rec[2].AsUint1;
                BlobId.m_BlobInfo.blob_id.sat_key = rec[3].AsUint4;
                BlobId.m_BlobInfo.tax_id          = rec[4].AsUint4;
                BlobId.m_BlobInfo.date_queued     = rec[5].AsDateTime;
                BlobId.m_BlobInfo.state           = rec[6].AsUint1;
            }
            catch (const DDRPC::EDdRpcException& e) {
                BlobId.m_Status = CBlobId::eFailed;
                BlobId.m_StatusEx = CBlobId::eError;
                BlobId.m_Message = e.what();
            }
            break;
        }
        case 404: {
            BlobId.m_Status = CBlobId::eFailed;
            BlobId.m_StatusEx = CBlobId::eNotFound;
            BlobId.m_Message = "Bio id is not found";
            break;
        }
        default: {
            BlobId.m_Status = CBlobId::eFailed;
            BlobId.m_StatusEx = CBlobId::eError;
            BlobId.m_Message = "Unexpected result";
        }
    }
        
}

/** CBioIdResolutionQueue */

CBioIdResolutionQueue::CBioIdResolutionQueue()
    :   m_future(make_shared<HCT::io_future>())
{
}

CBioIdResolutionQueue::~CBioIdResolutionQueue()
{
}

void CBioIdResolutionQueue::Resolve(std::vector<CBioId>* bio_ids, const CDeadline& deadline)
{    
    if (!bio_ids)
        return;
    bool has_timeout = !deadline.IsInfinite();
    unique_lock<mutex> _(m_items_mtx);
    long wait_ms = 0;
    auto rev_it = bio_ids->rbegin();
    while (rev_it != bio_ids->rend()) {
        
        unique_ptr<CBioIdResolutionQueueItem> Qi(new CBioIdResolutionQueueItem(m_future, *rev_it));

        while (true) {
            bool rv = HCT::HttpClientTransport::s_ioc->add_request(Qi->m_request, wait_ms);

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
                m_items.emplace_back(std::move(Qi));
                bio_ids->erase(std::next(rev_it.base()));
                ++rev_it;
                break;
            }
        }
        if (wait_ms < 0)
            break;
    }
}

CBlobId CBioIdResolutionQueue::Resolve(CBioId bio_id, const CDeadline& deadline)
{
    CBlobId rv(bio_id);
    unique_ptr<CBioIdResolutionQueueItem> Qi(new CBioIdResolutionQueueItem(HCT::io_coordinator::get_tls_future(), std::move(bio_id)));
    Qi->SyncResolve(rv, deadline);
    return rv;
}

std::vector<CBlobId> CBioIdResolutionQueue::GetBlobIds(const CDeadline& deadline, size_t max_results)
{
    std::vector<CBlobId> rv;
    bool has_limit = max_results != 0;
    unique_lock<mutex> _(m_items_mtx);
    if (m_items.size() > 0) {
        bool has_timeout = !deadline.IsInfinite();
        if (has_timeout) {
            long wait_ms = RemainingTimeMs(deadline);
            auto& last = m_items.front();
            last->WaitFor(wait_ms);
        }
        for (auto rev_it = m_items.rbegin(); rev_it != m_items.rend(); ++rev_it) {
            if (has_limit && max_results-- == 0)
                break;
            const auto& req = *rev_it;
            if (req->IsDone()) {
                auto fw_it = std::next(rev_it).base();
                rv.emplace_back(std::move((*fw_it)->m_BioId));
                req->PopulateData(rv.back());
                m_items.erase(fw_it);
            }
        }
    }
    return rv;
}

void CBioIdResolutionQueue::Clear(std::vector<CBioId>* bio_ids)
{
    unique_lock<mutex> _(m_items_mtx);
    if (bio_ids) {
        bio_ids->clear();
        bio_ids->reserve(m_items.size());
    }
    for (auto& it : m_items) {
        it->Cancel();
        if (bio_ids)
            bio_ids->emplace_back(std::move(it->m_BioId));
    }
    m_items.clear();
}

bool CBioIdResolutionQueue::IsEmpty() const
{
    unique_lock<mutex> _(m_items_mtx);
    return m_items.size() == 0;
}


/** CBlobRetrievalQueue */


END_objects_SCOPE
END_NCBI_SCOPE
