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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_base64.h>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_service.h>
#include <connect/ncbi_connutil.h>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#undef ThrowError // unfortunately

#include "psg_client_impl.hpp"

BEGIN_NCBI_SCOPE


const char* CPSG_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
        case eTimeout:          return "eTimeout";
        case eServerError:      return "eServerError";
        case eInternalError:    return "eInternalError";
        case eParameterMissing: return "eParameterMissing";
        default:                return CException::GetErrCodeString();
    }
}


SPSG_BlobReader::SPSG_BlobReader(SPSG_Reply::SItem::TTS* src)
    : m_Src(src)
{
    assert(src);
}

ERW_Result SPSG_BlobReader::x_Read(void* buf, size_t count, size_t* bytes_read)
{
    assert(bytes_read);

    *bytes_read = 0;

    CheckForNewChunks();

    for (; m_Chunk < m_Data.size(); ++m_Chunk) {
        auto& data = m_Data[m_Chunk];

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        auto available = data.size() - m_Index;
        auto to_copy = min(count, available);

        memcpy(buf, data.data() + m_Index, to_copy);
        buf = (char*)buf + to_copy;
        count -= to_copy;
        *bytes_read += to_copy;
        m_Index += to_copy;

        if (!count) return eRW_Success;

        m_Index = 0;
    }

    auto src_locked = m_Src->GetLock();
    return src_locked->expected.Cmp<equal_to>(src_locked->received) ? eRW_Eof : eRW_Success;
}

ERW_Result SPSG_BlobReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    size_t read;
    const auto kSeconds = TPSG_ReaderTimeout::GetDefault();
    CDeadline deadline(kSeconds);

    do {
        auto rv = x_Read(buf, count, &read);

        if ((rv != eRW_Success) || (read != 0)) {
            if (bytes_read) *bytes_read = read;
            return rv;
        }
    }
    while (m_Src->WaitUntil(deadline));

    NCBI_THROW_FMT(CPSG_Exception, eTimeout, "Timeout on reading (after " << kSeconds << " seconds)");
    return eRW_Error;
}

ERW_Result SPSG_BlobReader::PendingCount(size_t* count)
{
    assert(count);

    *count = 0;

    CheckForNewChunks();

    auto k = m_Index;

    for (auto i = m_Chunk; i < m_Data.size(); ++i) {
        auto& data = m_Data[i];

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        *count += data.size() - k;
        k = 0;
    }

    return eRW_Success;
}

void SPSG_BlobReader::CheckForNewChunks()
{
    if (m_Src->GetMTSafe().state.Empty()) return;

    auto src_locked = m_Src->GetLock();
    auto& src = *src_locked;
    auto& chunks = src.chunks;

    if (m_Data.size() < chunks.size()) m_Data.resize(chunks.size());

    for (size_t i = 0; i < chunks.size(); ++i) {
        if (!chunks[i].empty()) {
            m_Data[i].swap(chunks[i]);
        }
    }
}


static_assert(is_nothrow_move_constructible<CPSG_BioId>::value, "CPSG_BioId move constructor must be noexcept");

string CPSG_BioId::Repr() const
{
    return m_Type == TType::e_not_set ? m_Id : m_Id + '~' + to_string(m_Type);
}

CPSG_BioId s_GetBioId(const CJsonNode& data)
{
    auto type = static_cast<CPSG_BioId::TType>(data.GetInteger("seq_id_type"));
    auto accession = data.GetString("accession");
    auto name_node = data.GetByKeyOrNull("name");
    auto name = name_node && name_node.IsString() ? name_node.AsString() : string();
    auto version = static_cast<int>(data.GetInteger("version"));
    return { objects::CSeq_id(type, accession, name, version).AsFastaString(), type };
};

ostream& operator<<(ostream& os, const CPSG_BioId& bio_id)
{
    if (bio_id.GetType()) os << "seq_id_type=" << bio_id.GetType() << '&';
    return os << "seq_id=" << bio_id.GetId();
}


static_assert(is_nothrow_move_constructible<CPSG_BlobId>::value, "CPSG_BlobId move constructor must be noexcept");

string CPSG_BlobId::Repr() const
{
    return m_LastModified.IsNull() ? m_Id : m_Id + '~' + to_string(m_LastModified.GetValue());
}

unique_ptr<CPSG_DataId> s_GetDataId(const SPSG_Args& args)
{
    try {
        const auto& blob_id = args.GetValue("blob_id");

        if (blob_id.empty()) {
            auto id2_chunk = NStr::StringToNumeric<int>(args.GetValue("id2_chunk"));
            return unique_ptr<CPSG_DataId>(new CPSG_ChunkId(id2_chunk, args.GetValue("id2_info")));
        }

        CPSG_BlobId::TLastModified last_modified;
        const auto& last_modified_str = args.GetValue("last_modified");

        if (last_modified_str.empty()) {
            return unique_ptr<CPSG_DataId>(new CPSG_BlobId(blob_id));
        }

        last_modified = NStr::StringToNumeric<Int8>(last_modified_str);
        return unique_ptr<CPSG_DataId>(new CPSG_BlobId(blob_id, move(last_modified)));
    }
    catch (...) {
        NCBI_THROW_FMT(CPSG_Exception, eServerError,
                "Both blob_id[+last_modified] and id2_chunk+id2_info pairs are missing/corrupted in server response: " <<
                args.GetQueryString(CUrlArgs::eAmp_Char));
    }
}

CPSG_BlobId s_GetBlobId(const CJsonNode& data)
{
    CPSG_BlobId::TLastModified last_modified;

    if (data.HasKey("last_modified")) {
        last_modified = data.GetInteger("last_modified");
    }

    if (data.HasKey("blob_id")) {
        return { data.GetString("blob_id"), last_modified };
    }

    auto sat = static_cast<int>(data.GetInteger("sat"));
    auto sat_key = static_cast<int>(data.GetInteger("sat_key"));
    return { sat, sat_key, last_modified };
}

ostream& operator<<(ostream& os, const CPSG_BlobId& blob_id)
{
    if (!blob_id.GetLastModified().IsNull()) os << "last_modified=" << blob_id.GetLastModified().GetValue() << '&';
    return os << "blob_id=" << blob_id.GetId();
}


string CPSG_ChunkId::Repr() const
{
    return to_string(m_Id2Chunk) + '~' + m_Id2Info;
}

ostream& operator<<(ostream& os, const CPSG_ChunkId& chunk_id)
{
    return os << "id2_chunk=" << chunk_id.GetId2Chunk() << "&id2_info=" << chunk_id.GetId2Info();
}


template <class TReplyItem>
TReplyItem* CPSG_Reply::SImpl::CreateImpl(TReplyItem* item, const vector<SPSG_Chunk>& chunks)
{
    if (chunks.empty()) return item;

    unique_ptr<TReplyItem> rv(item);
    rv->m_Data = CJsonNode::ParseJSON(chunks.front(), CJsonNode::fStandardJson);

    return rv.release();
}

struct SItemTypeAndReason : pair<CPSG_ReplyItem::EType, CPSG_SkippedBlob::EReason>
{
    static SItemTypeAndReason Get(const SPSG_Args& args);

private:
    using TBase = pair<CPSG_ReplyItem::EType, CPSG_SkippedBlob::EReason>;
    using TBase::TBase;
    SItemTypeAndReason(CPSG_ReplyItem::EType type) : TBase(type, CPSG_SkippedBlob::eUnknown) {}
};

SItemTypeAndReason SItemTypeAndReason::Get(const SPSG_Args& args)
{
    const auto item_type = args.GetValue("item_type");

    if (item_type == "blob") {
        const auto reason = args.GetValue("reason");

        if (reason.empty()) {
            return CPSG_ReplyItem::eBlobData;

        } else if (reason == "excluded") {
            return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eExcluded };

        } else if (reason == "inprogress") {
            return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eInProgress };

        } else if (reason == "sent") {
            return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eSent };

        } else {
            return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eUnknown };
        }

    } else if (item_type == "bioseq_info") {
        return CPSG_ReplyItem::eBioseqInfo;

    } else if (item_type == "blob_prop") {
        return CPSG_ReplyItem::eBlobInfo;

    } else if (item_type == "bioseq_na") {
        return CPSG_ReplyItem::eNamedAnnotInfo;

    } else if (item_type == "public_comment") {
        return CPSG_ReplyItem::ePublicComment;

    } else if (item_type == "processor") {
        return CPSG_ReplyItem::eProcessor;

    } else {
        if (TPSG_FailOnUnknownItems::GetDefault()) {
            NCBI_THROW_FMT(CPSG_Exception, eServerError, "Received unknown item type: " << item_type);
        }

        static atomic_bool reported(false);

        if (!reported.exchange(true)) {
            ERR_POST("Received unknown item type: " << item_type);
        }

        return CPSG_ReplyItem::eEndOfReply;
    }
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

    auto& chunks = item_locked->chunks;
    auto& args = item_locked->args;
    auto processor_id = args.GetValue("processor_id");

    const auto& state = item_locked->state.GetState();
    const auto itar = SItemTypeAndReason::Get(args);

    if ((state == SPSG_Reply::SState::eNotFound) || (state == SPSG_Reply::SState::eError)) {
        rv.reset(new CPSG_ReplyItem(itar.first));

    } else if (itar.first == CPSG_ReplyItem::eBlobData) {
        unique_ptr<CPSG_BlobData> blob_data(new CPSG_BlobData(s_GetDataId(args)));
        blob_data->m_Stream.reset(new SPSG_RStream(item_ts));
        rv.reset(blob_data.release());

    } else if (itar.first == CPSG_ReplyItem::eSkippedBlob) {
        auto data_id = s_GetDataId(args);
        auto blob_id = move(dynamic_cast<CPSG_BlobId&>(*data_id));
        rv.reset(new CPSG_SkippedBlob(move(blob_id), itar.second));

    } else if (itar.first == CPSG_ReplyItem::eBioseqInfo) {
        rv.reset(CreateImpl(new CPSG_BioseqInfo, chunks));

    } else if (itar.first == CPSG_ReplyItem::eBlobInfo) {
        rv.reset(CreateImpl(new CPSG_BlobInfo(s_GetDataId(args)), chunks));

    } else if (itar.first == CPSG_ReplyItem::eNamedAnnotInfo) {
        auto name = args.GetValue("na");
        rv.reset(CreateImpl(new CPSG_NamedAnnotInfo(name), chunks));

    } else if (itar.first == CPSG_ReplyItem::ePublicComment) {
        auto text = chunks.empty() ? string() : chunks.front();
        rv.reset(new CPSG_PublicComment(s_GetDataId(args), text));

    } else if (itar.first == CPSG_ReplyItem::eProcessor) {
        rv.reset(new CPSG_ReplyItem(CPSG_ReplyItem::eProcessor));

    } else {
        return rv;
    }

    rv->m_Impl.reset(impl.release());
    rv->m_Reply = user_reply_locked;
    rv->m_ProcessorId = move(processor_id);
    return rv;
}


pair<mutex, weak_ptr<CPSG_Queue::SImpl::CService::TMap>> CPSG_Queue::SImpl::CService::sm_Instance;

SPSG_IoCoordinator& CPSG_Queue::SImpl::CService::GetIoC(const string& service)
{
    if (service.empty()) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "Service name is empty");
    }

    unique_lock<mutex> lock(sm_Instance.first);

    auto found = m_Map->find(service);

    if (found != m_Map->end()) {
        return *found->second;
    }

    auto created = m_Map->emplace(service, unique_ptr<SPSG_IoCoordinator>(new SPSG_IoCoordinator(service)));
    return *created.first->second;
}

shared_ptr<CPSG_Queue::SImpl::CService::TMap> CPSG_Queue::SImpl::CService::GetMap()
{
    unique_lock<mutex> lock(sm_Instance.first);

    auto rv = sm_Instance.second.lock();

    if (!rv) {
        rv = make_shared<TMap>();
        sm_Instance.second = rv;
    }

    return rv;
}


CPSG_Queue::SImpl::SImpl(const string& service) :
    queue(make_shared<TPSG_Queue>()),
    m_Service(service)
{
}

const char* s_GetTSE(CPSG_Request_Biodata::EIncludeData include_data)
{
    switch (include_data) {
        case CPSG_Request_Biodata::eDefault:  return nullptr;
        case CPSG_Request_Biodata::eNoTSE:    return "none";
        case CPSG_Request_Biodata::eSlimTSE:  return "slim";
        case CPSG_Request_Biodata::eSmartTSE: return "smart";
        case CPSG_Request_Biodata::eWholeTSE: return "whole";
        case CPSG_Request_Biodata::eOrigTSE:  return "orig";
    }

    return nullptr;
}

string CPSG_Queue::SImpl::x_GetAbsPathRef(shared_ptr<const CPSG_Request> user_request)
{
    ostringstream os;
    user_request->x_GetAbsPathRef(os);
    os << m_Service.ioc.GetUrlArgs();

    if (const auto hops = user_request->m_Hops) os << "&hops=" << hops;
    return os.str();
}

const char* s_GetAccSubstitution(EPSG_AccSubstitution acc_substitution)
{
    switch (acc_substitution) {
        case EPSG_AccSubstitution::Default: break;
        case EPSG_AccSubstitution::Limited: return "&acc_substitution=limited";
        case EPSG_AccSubstitution::Never:   return "&acc_substitution=never";
    }

    return "";
}


const char* s_GetAutoBlobSkipping(ESwitch value)
{
    switch (value) {
        case eDefault: break;
        case eOn:      return "&auto_blob_skipping=yes";
        case eOff:     return "&auto_blob_skipping=no";
    }

    return "";
}


void CPSG_Request_Biodata::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get?" << m_BioId;

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;

    if (!m_ExcludeTSEs.empty()) {
        os << "&exclude_blobs";

        char delimiter = '=';
        for (const auto& blob_id : m_ExcludeTSEs) {
            os << delimiter << blob_id.GetId();
            delimiter = ',';
        }
    }

    os << s_GetAccSubstitution(m_AccSubstitution);
    os << s_GetAutoBlobSkipping(m_AutoBlobSkipping);
}

void CPSG_Request_Resolve::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/resolve?" << m_BioId << "&fmt=json";

    auto value = "yes";
    auto include_info = m_IncludeInfo;
    const auto max_bit = (numeric_limits<unsigned>::max() >> 1) + 1;

    if (include_info & CPSG_Request_Resolve::TIncludeInfo(max_bit)) {
        os << "&all_info=yes";
        value = "no";
        include_info = ~include_info;
    }

    if (include_info & CPSG_Request_Resolve::fCanonicalId)  os << "&canon_id=" << value;
    if (include_info & CPSG_Request_Resolve::fName)         os << "&name=" << value;
    if (include_info & CPSG_Request_Resolve::fOtherIds)     os << "&seq_ids=" << value;
    if (include_info & CPSG_Request_Resolve::fMoleculeType) os << "&mol_type=" << value;
    if (include_info & CPSG_Request_Resolve::fLength)       os << "&length=" << value;
    if (include_info & CPSG_Request_Resolve::fChainState)   os << "&seq_state=" << value;
    if (include_info & CPSG_Request_Resolve::fState)        os << "&state=" << value;
    if (include_info & CPSG_Request_Resolve::fBlobId)       os << "&blob_id=" << value;
    if (include_info & CPSG_Request_Resolve::fTaxId)        os << "&tax_id=" << value;
    if (include_info & CPSG_Request_Resolve::fHash)         os << "&hash=" << value;
    if (include_info & CPSG_Request_Resolve::fDateChanged)  os << "&date_changed=" << value;
    if (include_info & CPSG_Request_Resolve::fGi)           os << "&gi=" << value;

    os << s_GetAccSubstitution(m_AccSubstitution);
}

void CPSG_Request_Blob::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/getblob?" << m_BlobId;

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;
}

void CPSG_Request_NamedAnnotInfo::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get_na?" << m_BioId << "&names=";

    for (const auto& name : m_AnnotNames) {
        os << name << ",";
    }

    // Remove last comma (there must be some output after seekp to succeed)
    os.seekp(-1, ios_base::cur);

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;

    os << s_GetAccSubstitution(m_AccSubstitution);
}

void CPSG_Request_Chunk::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get_tse_chunk?" << m_ChunkId;
}

shared_ptr<CPSG_Reply> CPSG_Queue::SImpl::SendRequestAndGetReply(shared_ptr<CPSG_Request> r, CDeadline deadline)
{
    _ASSERT(queue);

    auto user_request = const_pointer_cast<const CPSG_Request>(r);
    auto& ioc = m_Service.ioc;
    auto& params = ioc.params;

    auto user_context = params.client_mode == EPSG_PsgClientMode::eOff ?
        nullptr : user_request->GetUserContext<string>();
    const auto request_id = user_context ? *user_context : ioc.GetNewRequestId();
    auto reply = make_shared<SPSG_Reply>(move(request_id), params, queue);
    auto abs_path_ref = x_GetAbsPathRef(user_request);
    auto request = make_shared<SPSG_Request>(move(abs_path_ref), reply, user_request->m_RequestContext, params);

    if (ioc.AddRequest(request, queue->Stopped(), deadline)) {
        shared_ptr<CPSG_Reply> user_reply(new CPSG_Reply);
        user_reply->m_Impl->reply = move(reply);
        user_reply->m_Impl->user_reply = user_reply;
        user_reply->m_Request = move(user_request);
        return user_reply;
    }

    return {};
}

bool CPSG_Queue::SImpl::SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(queue);

    if (auto user_reply = SendRequestAndGetReply(move(request), move(deadline))) {
        queue->Push(move(user_reply));
        return true;
    }

    return false;
}

bool CPSG_Queue::SImpl::WaitForEvents(CDeadline deadline)
{
    _ASSERT(queue);

    if (queue->CV().WaitUntil(queue->Stopped(), move(deadline), false, true)) {
        queue->CV().Reset();
        return true;
    }

    return false;
}

EPSG_Status s_GetStatus(SPSG_Reply::SItem::TTS* ts, const CDeadline& deadline)
{
    assert(ts);

    auto& state = ts->GetMTSafe().state;

    do {
        switch (state.GetState()) {
            case SPSG_Reply::SState::eNotFound:   return EPSG_Status::eNotFound;
            case SPSG_Reply::SState::eError:      return EPSG_Status::eError;
            case SPSG_Reply::SState::eSuccess:    return EPSG_Status::eSuccess;
            case SPSG_Reply::SState::eInProgress: break;
        }
    }
    while (state.change.WaitUntil(deadline));

    return EPSG_Status::eInProgress;
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


CPSG_BlobData::CPSG_BlobData(unique_ptr<CPSG_DataId> id) :
    CPSG_ReplyItem(eBlobData),
    m_Id(move(id))
{
}


CPSG_BlobInfo::CPSG_BlobInfo(unique_ptr<CPSG_DataId> id) :
    CPSG_ReplyItem(eBlobInfo),
    m_Id(move(id))
{
}

enum EPSG_BlobInfo_Flags
{
    fPSGBI_CheckFailed = 1 << 0,
    fPSGBI_Gzip        = 1 << 1,
    fPSGBI_Not4Gbu     = 1 << 2,
    fPSGBI_Withdrawn   = 1 << 3,
    fPSGBI_Suppress    = 1 << 4,
    fPSGBI_Dead        = 1 << 5,
};

string CPSG_BlobInfo::GetCompression() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Gzip ? "gzip" : "";
}

string CPSG_BlobInfo::GetFormat() const
{
    return "asn.1";
}

Uint8 CPSG_BlobInfo::GetStorageSize() const
{
    return static_cast<Uint8>(m_Data.GetInteger("size"));
}

Uint8 CPSG_BlobInfo::GetSize() const
{
    return static_cast<Uint8>(m_Data.GetInteger("size_unpacked"));
}

bool CPSG_BlobInfo::IsDead() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Dead;
}

bool CPSG_BlobInfo::IsSuppressed() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Suppress;
}

bool CPSG_BlobInfo::IsWithdrawn() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Withdrawn;
}

CTime s_GetTime(Int8 milliseconds)
{
    return milliseconds > 0 ? CTime(static_cast<time_t>(milliseconds / kMilliSecondsPerSecond)) : CTime();
}

CTime CPSG_BlobInfo::GetHupReleaseDate() const
{
    return s_GetTime(m_Data.GetInteger("hup_date"));
}

Uint8 CPSG_BlobInfo::GetOwner() const
{
    return static_cast<Uint8>(m_Data.GetInteger("owner"));
}

CTime CPSG_BlobInfo::GetOriginalLoadDate() const
{
    return s_GetTime(m_Data.GetInteger("date_asn1"));
}

objects::CBioseq_set::EClass CPSG_BlobInfo::GetClass() const
{
    return static_cast<objects::CBioseq_set::EClass>(m_Data.GetInteger("class"));
}

string CPSG_BlobInfo::GetDivision() const
{
    return m_Data.GetString("div");
}

string CPSG_BlobInfo::GetUsername() const
{
    return m_Data.GetString("username");
}

string CPSG_BlobInfo::GetId2Info() const
{
    return m_Data.GetString("id2_info");
}

Uint8 CPSG_BlobInfo::GetNChunks() const
{
    return static_cast<Uint8>(m_Data.GetInteger("n_chunks"));
}


CPSG_SkippedBlob::CPSG_SkippedBlob(CPSG_BlobId id, EReason reason) :
    CPSG_ReplyItem(eSkippedBlob),
    m_Id(id),
    m_Reason(reason)
{
}


CPSG_BioseqInfo::CPSG_BioseqInfo()
    : CPSG_ReplyItem(eBioseqInfo)
{
}

CPSG_BioId CPSG_BioseqInfo::GetCanonicalId() const
{
    return s_GetBioId(m_Data);
};

vector<CPSG_BioId> CPSG_BioseqInfo::GetOtherIds() const
{
    auto seq_ids = m_Data.GetByKey("seq_ids");
    vector<CPSG_BioId> rv;
    bool error = !seq_ids.IsArray();

    for (CJsonIterator it = seq_ids.Iterate(); !error && it.IsValid(); it.Next()) {
        auto seq_id = it.GetNode();
        error = !seq_id.IsArray() || (seq_id.GetSize() != 2);

        if (!error) {
            auto type = static_cast<CPSG_BioId::TType>(seq_id.GetAt(0).AsInteger());
            auto content = seq_id.GetAt(1).AsString();
            rv.emplace_back(string(objects::CSeq_id::WhichFastaTag(type))
                            + '|' + content, type);
        }
    }

    if (error) {
        auto reply = GetReply();
        _ASSERT(reply);

        auto request = reply->GetRequest().get();
        _ASSERT(request);

        NCBI_THROW_FMT(CPSG_Exception, eServerError, "Wrong seq_ids format: '" << seq_ids.Repr() <<
                "' for " << request->GetType() << " request '" << request->GetId() << '\'');
    }

    return rv;
}

objects::CSeq_inst::TMol CPSG_BioseqInfo::GetMoleculeType() const
{
    return static_cast<objects::CSeq_inst::TMol>(m_Data.GetInteger("mol"));
}

Uint8 CPSG_BioseqInfo::GetLength() const
{
    return m_Data.GetInteger("length");
}

CPSG_BioseqInfo::TState CPSG_BioseqInfo::GetChainState() const
{
    return static_cast<TState>(m_Data.GetInteger("seq_state"));
}

CPSG_BioseqInfo::TState CPSG_BioseqInfo::GetState() const
{
    return static_cast<TState>(m_Data.GetInteger("state"));
}

CPSG_BlobId CPSG_BioseqInfo::GetBlobId() const
{
    return s_GetBlobId(m_Data);
}

TTaxId CPSG_BioseqInfo::GetTaxId() const
{
    return TAX_ID_FROM(Int8, m_Data.GetInteger("tax_id"));
}

int CPSG_BioseqInfo::GetHash() const
{
    return static_cast<int>(m_Data.GetInteger("hash"));
}

CTime CPSG_BioseqInfo::GetDateChanged() const
{
    return s_GetTime(m_Data.GetInteger("date_changed"));
}

TGi CPSG_BioseqInfo::GetGi() const
{
    return static_cast<TGi>(m_Data.GetInteger("gi"));
}

CPSG_Request_Resolve::TIncludeInfo CPSG_BioseqInfo::IncludedInfo() const
{
    CPSG_Request_Resolve::TIncludeInfo rv = {};

    if (m_Data.HasKey("accession") && m_Data.HasKey("seq_id_type"))       rv |= CPSG_Request_Resolve::fCanonicalId;
    if (m_Data.HasKey("name"))                                            rv |= CPSG_Request_Resolve::fName;
    if (m_Data.HasKey("seq_ids") && m_Data.GetByKey("seq_ids").GetSize()) rv |= CPSG_Request_Resolve::fOtherIds;
    if (m_Data.HasKey("mol"))                                             rv |= CPSG_Request_Resolve::fMoleculeType;
    if (m_Data.HasKey("length"))                                          rv |= CPSG_Request_Resolve::fLength;
    if (m_Data.HasKey("seq_state"))                                       rv |= CPSG_Request_Resolve::fChainState;
    if (m_Data.HasKey("state"))                                           rv |= CPSG_Request_Resolve::fState;
    if (m_Data.HasKey("blob_id") ||
        (m_Data.HasKey("sat") && m_Data.HasKey("sat_key")))               rv |= CPSG_Request_Resolve::fBlobId;
    if (m_Data.HasKey("tax_id"))                                          rv |= CPSG_Request_Resolve::fTaxId;
    if (m_Data.HasKey("hash"))                                            rv |= CPSG_Request_Resolve::fHash;
    if (m_Data.HasKey("date_changed"))                                    rv |= CPSG_Request_Resolve::fDateChanged;
    if (m_Data.HasKey("gi"))                                              rv |= CPSG_Request_Resolve::fGi;

    return rv;
}


CPSG_NamedAnnotInfo::CPSG_NamedAnnotInfo(string name) :
    CPSG_ReplyItem(eNamedAnnotInfo),
    m_Name(move(name))
{
}


string CPSG_NamedAnnotInfo::GetId2AnnotInfo() const
{
    auto node = m_Data.GetByKeyOrNull("seq_annot_info");
    return node && node.IsString() ? node.AsString() : string();
}


CPSG_NamedAnnotInfo::TId2AnnotInfoList CPSG_NamedAnnotInfo::GetId2AnnotInfoList() const
{
    TId2AnnotInfoList ret;
    auto info_str = GetId2AnnotInfo();
    if (!info_str.empty()) {
        auto in_string = NStr::Base64Decode(info_str);
        istringstream in_stream(in_string);
        CObjectIStreamAsnBinary in(in_stream);
        while ( in.HaveMoreData() ) {
            CRef<TId2AnnotInfo> info(new TId2AnnotInfo);
            in >> *info;
            ret.push_back(info);
        }
    }
    return ret;
}


CPSG_BlobId CPSG_NamedAnnotInfo::GetBlobId() const
{
    return s_GetBlobId(m_Data);
}


CPSG_PublicComment::CPSG_PublicComment(unique_ptr<CPSG_DataId> id, string text) :
    CPSG_ReplyItem(ePublicComment),
    m_Id(move(id)),
    m_Text(move(text))
{
}


CPSG_Reply::CPSG_Reply() :
    m_Impl(new SImpl)
{
}

EPSG_Status CPSG_Reply::GetStatus(CDeadline deadline) const
{
    assert(m_Impl);

    return s_GetStatus(&m_Impl->reply->reply_item, deadline);
}

string CPSG_Reply::GetNextMessage() const
{
    assert(m_Impl);
    assert(m_Impl->reply);

    return m_Impl->reply->reply_item.GetLock()->state.GetError();
}

shared_ptr<CPSG_ReplyItem> CPSG_Reply::GetNextItem(CDeadline deadline)
{
    assert(m_Impl);
    assert(m_Impl->reply);

    auto& reply_item = m_Impl->reply->reply_item;
    auto& reply_state = reply_item.GetMTSafe().state;

    do {
        bool was_in_progress = reply_state.InProgress();

        if (auto items_locked = m_Impl->reply->items.GetLock()) {
            auto& items = *items_locked;

            for (auto& item_ts : items) {
                const auto& item_state = item_ts.GetMTSafe().state;

                if (item_state.Returned()) continue;

                if (item_state.Empty()) {
                    auto item_locked = item_ts.GetLock();
                    auto& item = *item_locked;

                    // Wait for more chunks on this item
                    if (!item.expected.Cmp<less_equal>(item.received)) continue;
                }

                // Do not hold lock on item_ts around this call!
                if (auto rv = m_Impl->Create(&item_ts)) {
                    return rv;
                }

                continue;
            }
        }

        // No more reply items
        if (!was_in_progress) {
            return shared_ptr<CPSG_ReplyItem>(new CPSG_ReplyItem(CPSG_ReplyItem::eEndOfReply));
        }
    }
    while (reply_item.WaitUntil(reply_state.GetState(), deadline, SPSG_Reply::SState::eInProgress, true));

    return {};
}

CPSG_Reply::~CPSG_Reply()
{
}


CPSG_Queue::CPSG_Queue() = default;
CPSG_Queue::CPSG_Queue(CPSG_Queue&&) = default;
CPSG_Queue& CPSG_Queue::operator=(CPSG_Queue&&) = default;
CPSG_Queue::~CPSG_Queue() = default;

CPSG_Queue::CPSG_Queue(const string& service) :
    m_Impl(new SImpl(service))
{
}

bool CPSG_Queue::SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->SendRequest(move(request), move(deadline));
}

shared_ptr<CPSG_Reply> CPSG_Queue::GetNextReply(CDeadline deadline)
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    shared_ptr<CPSG_Reply> rv;
    m_Impl->queue->Pop(rv, deadline);
    return rv;
}

shared_ptr<CPSG_Reply> CPSG_Queue::SendRequestAndGetReply(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->SendRequestAndGetReply(move(request), move(deadline));
}

void CPSG_Queue::Stop()
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    m_Impl->queue->Stop(m_Impl->queue->eDrain);
}

bool CPSG_Queue::WaitForEvents(CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->WaitForEvents(move(deadline));
}

void CPSG_Queue::Reset()
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    m_Impl->queue->Stop(m_Impl->queue->eClear);
}

bool CPSG_Queue::IsEmpty() const
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    return m_Impl->queue->Empty();
}

bool CPSG_Queue::RejectsRequests() const
{
    _ASSERT(m_Impl);
    return m_Impl->RejectsRequests();
}


CPSG_Queue::TApiLock CPSG_Queue::GetApiLock()
{
    return SImpl::GetApiLock();
}


END_NCBI_SCOPE

#endif
