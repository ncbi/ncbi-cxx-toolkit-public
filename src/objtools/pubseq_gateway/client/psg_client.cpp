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

#include <condition_variable>
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
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#include "psg_client_impl.hpp"

BEGIN_NCBI_SCOPE


namespace {

long RemainingTimeMs(const CDeadline& deadline)
{
    if (deadline.IsInfinite()) return DDRPC::INDEFINITE;

    unsigned int sec, nanosec;
    deadline.GetRemainingTime().GetNano(&sec, &nanosec);
    return nanosec / 1000000L + sec * 1000L;
}

}


SPSG_BlobReader::SPSG_BlobReader(SPSG_Reply::SItem::TTS* blob)
    : m_Blob(blob)
{
    assert(blob);
}

ERW_Result SPSG_BlobReader::x_Read(void* buf, size_t count, size_t* bytes_read)
{
    assert(bytes_read);

    *bytes_read = 0;

    CheckForNewChunks();

    for (; m_Chunk < m_Data.size(); ++m_Chunk) {
        auto& data = m_Data[m_Chunk].data;

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        for (; m_Part < data.size(); ++m_Part) {
            auto& part = data[m_Part];
            auto available = part.size() - m_Index;
            auto to_copy = min(count, available);

            memcpy(buf, part.data() + m_Index, to_copy);
            buf = (char*)buf + to_copy;
            count -= to_copy;
            *bytes_read += to_copy;
            m_Index += to_copy;

            if (!count) return eRW_Success;

            m_Index = 0;
        }

        m_Part = 0;
    }

    auto blob_locked = m_Blob->GetLock();
    return blob_locked->expected.Cmp<equal_to>(blob_locked->received) ? eRW_Eof : eRW_Success;
}

const auto kDefaultReadTimeout = CTimeout(12, 0); // TODO: Make configurable

ERW_Result SPSG_BlobReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    size_t read;
    CDeadline deadline(kDefaultReadTimeout);

    while (!deadline.IsExpired()) {
        auto rv = x_Read(buf, count, &read);

        if ((rv != eRW_Success) || (read != 0)) {
            if (bytes_read) *bytes_read = read;
            return rv;
        }

        auto wait_ms = chrono::milliseconds(RemainingTimeMs(deadline));
        m_Blob->WaitFor(wait_ms);
    }

    throw runtime_error("TIMEOUT"); // TODO: CPSG_Exception
    return eRW_Error;
}

ERW_Result SPSG_BlobReader::PendingCount(size_t* count)
{
    assert(count);

    *count = 0;

    CheckForNewChunks();

    auto j = m_Part;
    auto k = m_Index;

    for (auto i = m_Chunk; i < m_Data.size(); ++i) {
        auto& data = m_Data[i].data;

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        for (; j < data.size(); ++j) {
            auto& part = data[j];
            *count += part.size() - k;
            k = 0;
        }

        j = 0;
    }

    return eRW_Success;
}

void SPSG_BlobReader::CheckForNewChunks()
{
    auto blob_locked = m_Blob->GetLock();
    auto& blob = *blob_locked;
    auto& chunks = blob.chunks;

    auto it = chunks.begin();

    while (it != chunks.end()) {
        auto& args = it->args;
        auto blob_chunk = args.GetValue("blob_chunk");
        auto index = stoul(blob_chunk);
        if (m_Data.size() <= index) m_Data.resize(index + 1);
        m_Data[index] = move(*it);
        it = chunks.erase(it);
    }
}


static_assert(is_nothrow_move_constructible<CPSG_BioId>::value, "CPSG_BioId move constructor must be noexcept");
static_assert(is_nothrow_move_constructible<CPSG_BlobId>::value, "CPSG_BlobId move constructor must be noexcept");


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


shared_ptr<CPSG_ReplyItem> CPSG_Reply::SImpl::Create(SPSG_Reply::SItem::TTS* item_ts)
{
    auto user_reply_locked = user_reply.lock();

    assert(user_reply_locked);
    assert(item_ts);

    auto item_locked = item_ts->GetLock();

    item_locked->state.SetReturned();

    unique_ptr<CPSG_ReplyItem::SImpl> impl(new CPSG_ReplyItem::SImpl);
    impl->item = item_ts;

    shared_ptr<CPSG_ReplyItem> rv;

    auto& chunk = item_locked->chunks.front();
    auto& args = chunk.args;
    auto item_type = args.GetValue("item_type");

    if (item_type == "blob") {
        auto blob_id = args.GetValue("blob_id");

        unique_ptr<CPSG_Blob> blob(new CPSG_Blob(blob_id));
        blob->m_Stream.reset(new SPSG_RStream(item_ts));
        rv.reset(blob.release());

    } else {
        // TODO
        rv.reset(new CPSG_ReplyItem(CPSG_ReplyItem::eBlobInfo));
    }

    rv->m_Impl.reset(impl.release());
    rv->m_Reply = user_reply_locked;
    return rv;
}


CPSG_Queue::SImpl::SRequest::SRequest(shared_ptr<const CPSG_Request> user_request,
        shared_ptr<HCT::http2_request> http_request,
        shared_ptr<SPSG_Reply::TTS> reply) :
    m_UserRequest(user_request),
    m_HttpRequest(http_request),
    m_Reply(reply)
{
}

shared_ptr<CPSG_Reply> CPSG_Queue::SImpl::SRequest::GetNextReply()
{
    auto reply_locked = m_Reply->GetLock();

    // A reply has already been returned
    if (reply_locked->reply_item.GetLock()->state.Returned()) return {};

    auto returned = [](SPSG_Reply::SItem::TTS& item) { return item.GetLock()->state.Returned(); };
    auto& items = reply_locked->items;

    // No reply items to return yet
    if (all_of(items.begin(), items.end(), returned)) return {};

    reply_locked->reply_item.GetLock()->state.SetReturned();
    reply_locked.Unlock();

    shared_ptr<CPSG_Reply> rv(new CPSG_Reply);
    rv->m_Impl->reply = m_Reply;
    rv->m_Impl->user_reply = rv;
    rv->m_Request = move(m_UserRequest);

    return rv;
}

void CPSG_Queue::SImpl::SRequest::Reset()
{
    assert(m_HttpRequest);

    m_HttpRequest->send_cancel();
}

bool CPSG_Queue::SImpl::SRequest::IsEmpty() const
{
    auto reply_locked = m_Reply->GetLock();

    if (reply_locked->reply_item.GetLock()->state.InProgress()) return false;

    auto& items = reply_locked->items;
    auto returned = [](SPSG_Reply::SItem::TTS& item) { return item.GetLock()->state.Returned(); };

    return all_of(items.begin(), items.end(), returned);
}


CPSG_Queue::SImpl::SImpl(const string& service) :
    m_Requests(new SPSG_ThreadSafe<TRequests>),
    m_Service(service)
{
    if (m_Service.empty()) {
        throw invalid_argument("Service name is empty"); // TODO: CPSG_Exception
    }
}

string CPSG_Queue::SImpl::GetQuery(const CPSG_Request_Biodata* request_biodata)
{
    ostringstream os;
    const auto& bio_id = request_biodata->GetBioId();

    os << "seq_id=" << bio_id.Get();

    if (const auto type = bio_id.GetType()) os << "&seq_id_type=" << type;

    const auto include_data = request_biodata->GetIncludeData();

    if (include_data & CPSG_Request_Biodata::fNoTSE)        os << "&no_tse=yes";
    if (include_data & CPSG_Request_Biodata::fFastInfo)     os << "&fast_info=yes";
    if (include_data & CPSG_Request_Biodata::fWholeTSE)     os << "&whole_tse=yes";
    if (include_data & CPSG_Request_Biodata::fOrigTSE)      os << "&orig_tse=yes";
    if (include_data & CPSG_Request_Biodata::fCanonicalId)  os << "&canon_id=yes";
    if (include_data & CPSG_Request_Biodata::fOtherIds)     os << "&seq_ids=yes";
    if (include_data & CPSG_Request_Biodata::fMoleculeType) os << "&mol_type=yes";
    if (include_data & CPSG_Request_Biodata::fLength)       os << "&length=yes";
    if (include_data & CPSG_Request_Biodata::fState)        os << "&state=yes";
    if (include_data & CPSG_Request_Biodata::fBlobId)       os << "&blob_id=yes";
    if (include_data & CPSG_Request_Biodata::fTaxId)        os << "&tax_id=yes";
    if (include_data & CPSG_Request_Biodata::fHash)         os << "&hash=yes";
    if (include_data & CPSG_Request_Biodata::fDateChanged)  os << "&date_changed=yes";

    return os.str();
}

string CPSG_Queue::SImpl::GetQuery(const CPSG_Request_Blob* request_blob)
{
    ostringstream os;

    os << "blob_id=" << request_blob->GetBlobId().Get();

    const auto& last_modified = request_blob->GetLastModified();

    if (!last_modified.empty()) os << "&last_modified=" << last_modified;

    return os.str();
}

bool CPSG_Queue::SImpl::SendRequest(shared_ptr<const CPSG_Request> user_request, CDeadline deadline)
{
    long wait_ms = 0;
    shared_ptr<HCT::http2_request> http_request;
    auto reply = make_shared<SPSG_Reply::TTS>();

    if (auto request_biodata = dynamic_cast<const CPSG_Request_Biodata*>(user_request.get())) {
        string query(GetQuery(request_biodata));
        http_request = make_shared<TPSG_RequestValue>(reply, SHCT::GetEndPoint(m_Service), m_Requests, "/ID/get", query);

    } else if (auto request_blob = dynamic_cast<const CPSG_Request_Blob*>(user_request.get())) {
        string query(GetQuery(request_blob));
        http_request = make_shared<TPSG_RequestValue>(reply, SHCT::GetEndPoint(m_Service), m_Requests, "/ID/getblob", query);

    } else {
        throw invalid_argument("UNKNOWN REQUEST TYPE"); // TODO: CPSG_Exception
    }

    for (;;) {
        if (SHCT::GetIoc().add_request(http_request, wait_ms)) {
            auto requests_locked = m_Requests->GetLock();
            requests_locked->emplace_back(user_request, http_request, move(reply));
            return true;
        }

        // Internal queue is full
        wait_ms = RemainingTimeMs(deadline);
        if (wait_ms < 0)  return false;
    }
}

shared_ptr<CPSG_Reply> CPSG_Queue::SImpl::GetNextReply(CDeadline deadline)
{
    for (;;) {
        auto requests_locked = m_Requests->GetLock();

        auto it = requests_locked->begin();
        
        while (it != requests_locked->end()) {
            if (auto rv = it->GetNextReply()) return rv;

            if (it->IsEmpty()) {
                it = requests_locked->erase(it);
            } else {
                ++it;
            }
        }

        requests_locked.Unlock();

        if (deadline.IsExpired()) return {};

        auto wait_ms = chrono::milliseconds(RemainingTimeMs(deadline));
        m_Requests->WaitFor(wait_ms);
    }
}

void CPSG_Queue::SImpl::Reset()
{
    auto requests_locked = m_Requests->GetLock();

    for (auto& request : *requests_locked) {
        request.Reset();
    }

    requests_locked->clear();
}

bool CPSG_Queue::SImpl::IsEmpty() const
{
    auto requests_locked = m_Requests->GetLock();

    for (auto& request : *requests_locked) {
        if (!request.IsEmpty()) return false;
    }

    return true;
}


template <class TTsPtr>
EPSG_Status s_GetStatus(TTsPtr ts, CDeadline deadline)
{
    assert(ts);

    for (;;) {
        switch (ts->GetLock()->state.GetState()) {
            case SPSG_Reply::SState::eSuccess:  return EPSG_Status::eSuccess;
            case SPSG_Reply::SState::eCanceled: return EPSG_Status::eCanceled;
            case SPSG_Reply::SState::eNotFound: return EPSG_Status::eNotFound;
            case SPSG_Reply::SState::eError:    return EPSG_Status::eError;

            default: // SPSG_Reply::SState::eInProgress;
                if (deadline.IsExpired()) return EPSG_Status::eInProgress;
        }

        auto wait_ms = chrono::milliseconds(RemainingTimeMs(deadline));
        ts->WaitFor(wait_ms);
    }
}

EPSG_Status CPSG_ReplyItem::GetStatus(CDeadline deadline) const
{
    assert(m_Impl);

    return s_GetStatus(m_Impl->item, deadline);
}

string CPSG_ReplyItem::GetNextMessage() const
{
    assert(m_Impl);
    assert(m_Impl->item);

    return m_Impl->item->GetLock()->state.GetError();
}

CPSG_ReplyItem::~CPSG_ReplyItem()
{
}

CPSG_ReplyItem::CPSG_ReplyItem(EType type) :
    m_Type(type)
{
}


CPSG_Blob::CPSG_Blob(CPSG_BlobId id) :
    CPSG_ReplyItem(eBlob),
    m_Id(move(id))
{
}


CPSG_BlobInfo::CPSG_BlobInfo()
    : CPSG_ReplyItem(eBlobInfo)
{
}


CPSG_BioseqInfo::CPSG_BioseqInfo()
    : CPSG_ReplyItem(eBioseqInfo)
{
}


CPSG_Reply::CPSG_Reply() :
    m_Impl(new SImpl)
{
}

EPSG_Status CPSG_Reply::GetStatus(CDeadline deadline) const
{
    assert(m_Impl);
    auto reply_item = &m_Impl->reply->GetLock()->reply_item;

    // Do not hold lock on reply around this call!
    return s_GetStatus(reply_item, deadline);
}

string CPSG_Reply::GetNextMessage() const
{
    assert(m_Impl);
    assert(m_Impl->reply);

    return m_Impl->reply->GetLock()->reply_item.GetLock()->state.GetError();
}

shared_ptr<CPSG_ReplyItem> CPSG_Reply::GetNextItem(CDeadline deadline)
{
    assert(m_Impl);
    assert(m_Impl->reply);

    for (;;) {
        auto reply_locked = m_Impl->reply->GetLock();

        for (auto& item_ts : reply_locked->items) {
            if (item_ts.GetLock()->state.Returned()) continue;

            if (item_ts.GetLock()->chunks.empty()) continue;

            // Do not hold lock on item_ts around this call!
            return m_Impl->Create(&item_ts);
        }

        if (deadline.IsExpired()) return {};

        auto reply_item = &reply_locked->reply_item;

        // No more reply items
        if (!reply_item->GetLock()->state.InProgress()) {
            return shared_ptr<CPSG_ReplyItem>(new CPSG_ReplyItem(CPSG_ReplyItem::eEndOfReply));
        }

        reply_locked.Unlock();

        auto wait_ms = chrono::milliseconds(RemainingTimeMs(deadline));
        reply_item->WaitFor(wait_ms);
    }

    return {};
}

CPSG_Reply::~CPSG_Reply()
{
}


CPSG_Queue::CPSG_Queue(const string& service) :
    m_Impl(new SImpl(service))
{
}

CPSG_Queue::~CPSG_Queue()
{
}

bool CPSG_Queue::SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    return m_Impl->SendRequest(const_pointer_cast<const CPSG_Request>(request), deadline);
}

shared_ptr<CPSG_Reply> CPSG_Queue::GetNextReply(CDeadline deadline)
{
    return m_Impl->GetNextReply(deadline);
}

void CPSG_Queue::Reset()
{
    m_Impl->Reset();
}

bool CPSG_Queue::IsEmpty() const
{
    return m_Impl->IsEmpty();
}


END_NCBI_SCOPE
