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

#include <numeric>
#include <unordered_set>
#include <unordered_map>

#include <connect/impl/connect_misc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objcopy.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <util/checksum.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include "performance.hpp"
#include "processing.hpp"

BEGIN_NCBI_SCOPE

bool CJsonResponse::sm_SetReplyType = true;
EDiagSev SParams::min_severity = eDiag_Warning;
bool SParams::verbose = false;

enum EJsonRpcErrors {
    eJsonRpc_ParseError         = -32700,
    eJsonRpc_InvalidRequest     = -32600,
    eJsonRpc_ExceptionOnRead    = -32000,
};

struct SNewRequestContext
{
    SNewRequestContext() :
        m_RequestContext(new CRequestContext)
    {
        m_RequestContext->SetRequestID();
        CDiagContext::SetRequestContext(m_RequestContext);
    }

    ~SNewRequestContext()
    {
        CDiagContext::SetRequestContext(nullptr);
    }

    CRef<CRequestContext> Get() const { return m_RequestContext; }

private:
    SNewRequestContext(const SNewRequestContext&) = delete;
    void operator=(const SNewRequestContext&) = delete;

    CRef<CRequestContext> m_RequestContext;
};

struct SInteractiveNewRequestStart : SNewRequestContext
{
    SInteractiveNewRequestStart(const string& request, CJson_ConstObject params_obj);

private:
    struct SExtra : CDiagContext_Extra
    {
        SExtra() : CDiagContext_Extra(GetDiagContext().PrintRequestStart()) {}

        void Print(const string& prefix, CJson_ConstValue  json);
        void Print(const string& prefix, CJson_ConstArray  json);
        void Print(const string& prefix, CJson_ConstObject json);
        void Print(const string& prefix, CJson_ConstNode   json);

        using CDiagContext_Extra::Print;
    };
};

string s_GetId(const CJson_Document& req_doc);
CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status);

const char* SMetricType::Name(EType t)
{
    switch (t) {
        case eStart:   return "Start";
        case eSubmit:  return "Submit";
        case eReply:   return "Reply";
        case eDone:    return "Done";
        case eSend:    return "Send";
        case eReceive: return "Receive";
        case eClose:   return "Close";
        case eRetry:   return "Retry";
        case eFail:    return "Fail";

        case eLastType:
        case eError:   break;
    }

    _TROUBLE;
    return "ERROR";
}

const char* s_StrStatus(EPSG_Status status)
{
    switch (status) {
        case EPSG_Status::eSuccess:    return "Success";
        case EPSG_Status::eInProgress: return "InProgress";
        case EPSG_Status::eNotFound:   return "NotFound";
        case EPSG_Status::eCanceled:   return "Canceled";
        case EPSG_Status::eForbidden:  return "Forbidden";
        case EPSG_Status::eError:      break;
    }

    return "Error";
}

SJsonOut& SJsonOut::operator<<(const CJson_Document& doc)
{
    stringstream ss;
    doc.Write(ss, m_Flags);

    unique_lock<mutex> lock(m_Mutex);

    if (m_Pipe) {
        cout << ss.rdbuf() << endl;
    } else {
        cout << m_Separator << '\n' << ss.rdbuf() << flush;
        m_Separator = ',';
    }

    return *this;
}

SJsonOut::~SJsonOut()
{
    // If not in pipe mode and printed some JSON
    if (!m_Pipe && (m_Separator == ',')) {
        cout << "\n]" << endl;
    }
}

template <class TItem, class... TArgs>
CJsonResponse::CJsonResponse(EPSG_Status status, TItem item, TArgs&&... args) :
    m_JsonObj(SetObject())
{
    try {
        AddRequestID(item, std::forward<TArgs>(args)...);
        Fill(status, item);
        AddMessage(std::forward<TArgs>(args)...);

        for (auto l = [&]() { return item->GetNextMessage(SParams::min_severity); }; auto message = l(); ) {
            AddMessage(message);
        }
    }
    catch (exception& e) {
        CJson_Document new_doc;
        new_doc.ResetObject()["data"].AssignCopy(*this);
        AssignCopy(new_doc);
        Set("code",    eJsonRpc_ExceptionOnRead);
        Set("message", e.what());
        m_Error = true;
    }
}

CJsonResponse::CJsonResponse(const string& id, const CJsonResponse& result) :
    CJsonResponse(id)
{
    m_JsonObj[result.m_Error ? "error" : "result"].AssignCopy(result);
}

CJsonResponse::CJsonResponse(const string& id, int code, const string& message) :
    CJsonResponse(id)
{
    CJson_Object error_obj = m_JsonObj.insert_object("error");
    Set(error_obj, "code",    code);
    Set(error_obj, "message", message);
}

CJsonResponse::CJsonResponse(const string& id) :
    m_JsonObj(SetObject())
{
    Set("jsonrpc", "2.0");

    auto id_value = m_JsonObj["id"].SetValue();

    if (id.empty()) {
        id_value.SetNull();
    } else {
        id_value.SetString(id);
    }
}

template <class TItem, class... TArgs>
void CJsonResponse::AddRequestID(TItem item, TArgs&&...)
{
    if (auto request_id = s_GetReply(item)->GetRequest()->template GetUserContext<string>()) {
        Set("request_id", *request_id);
    }
}

const char* s_GetItemName(CPSG_ReplyItem::EType type, bool trouble = true)
{
    switch (type) {
        case CPSG_ReplyItem::eBlobData:       return "BlobData";
        case CPSG_ReplyItem::eBlobInfo:       return "BlobInfo";
        case CPSG_ReplyItem::eSkippedBlob:    return "SkippedBlob";
        case CPSG_ReplyItem::eBioseqInfo:     return "BioseqInfo";
        case CPSG_ReplyItem::eNamedAnnotInfo: return "NamedAnnotInfo";
        case CPSG_ReplyItem::eNamedAnnotStatus: return "NamedAnnotStatus";
        case CPSG_ReplyItem::ePublicComment:  return "PublicComment";
        case CPSG_ReplyItem::eProcessor:      return "Processor";
        case CPSG_ReplyItem::eIpgInfo:        return "IpgInfo";
        case CPSG_ReplyItem::eAccVerHistory:  return "AccVerHistory";
        case CPSG_ReplyItem::eEndOfReply:     if (!trouble) return "Reply"; _TROUBLE;
    }

    return "UnknownItem";
}

CJsonResponse CJsonResponse::NewItem(const shared_ptr<CPSG_ReplyItem>& reply_item)
{
    CJsonResponse rv;
    rv.Set("reply", "NewItem");
    rv.Set("item", s_GetItemName(reply_item->GetType()));
    rv.Set("status", s_StrStatus(reply_item->GetStatus(CDeadline::eNoWait)));
    return rv;
}

void CJsonResponse::Fill(EPSG_Status status, shared_ptr<CPSG_Reply>)
{
    Set("status", s_StrStatus(status));
}

void CJsonResponse::Fill(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
{
    auto reply_item_type = reply_item->GetType();

    if (sm_SetReplyType) {
        Set("reply", s_GetItemName(reply_item_type));
    }

    if (SParams::verbose) {
        Set("processor_id", reply_item->GetProcessorId());
    }

    if (reply_item_status != EPSG_Status::eSuccess) {
        Set("status", s_StrStatus(reply_item_status));
        return;
    }

    switch (reply_item_type) {
        case CPSG_ReplyItem::eBlobData:
            return Fill(static_pointer_cast<CPSG_BlobData>(reply_item));

        case CPSG_ReplyItem::eBlobInfo:
            return Fill(static_pointer_cast<CPSG_BlobInfo>(reply_item));

        case CPSG_ReplyItem::eSkippedBlob:
            return Fill(static_pointer_cast<CPSG_SkippedBlob>(reply_item));

        case CPSG_ReplyItem::eBioseqInfo:
            return Fill(static_pointer_cast<CPSG_BioseqInfo>(reply_item));

        case CPSG_ReplyItem::eNamedAnnotInfo:
            return Fill(static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item));

        case CPSG_ReplyItem::eNamedAnnotStatus:
            return Fill(static_pointer_cast<CPSG_NamedAnnotStatus>(reply_item));

        case CPSG_ReplyItem::ePublicComment:
            return Fill(static_pointer_cast<CPSG_PublicComment>(reply_item));

        case CPSG_ReplyItem::eProcessor:
            return Fill(static_pointer_cast<CPSG_Processor>(reply_item));

        case CPSG_ReplyItem::eIpgInfo:
            return Fill(static_pointer_cast<CPSG_IpgInfo>(reply_item));

        case CPSG_ReplyItem::eAccVerHistory:
            return Fill(static_pointer_cast<CPSG_AccVerHistory>(reply_item));

        case CPSG_ReplyItem::eEndOfReply:
            _TROUBLE;
            return;
    }

    throw logic_error("Received unknown item: " + to_string(reply_item_type));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobData> blob_data)
{
    if (auto blob_id = CPSG_Misc::GetRawResponseBlobId(blob_data)) {
        if (CJson_Document json_doc; json_doc.Read(blob_data->GetStream())) {
            Set("reply", blob_id->GetId());

            for (const auto& p : json_doc.GetObject()) {
                m_JsonObj.insert(p.name,  p.value);
            }

            return;
        }
    }

    Set("id", blob_data);
    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    auto data = os.str();

    if (const auto data_size = data.size(); data_size <= sm_DataLimit) {
        Set("data", NStr::Base64Encode(data));
    } else {
        Set("length", static_cast<Uint8>(data_size)); // macOS requires static_cast, Set() would be ambiguous otherwise
        CHash hash;
        hash.Calculate(data.data(), data_size);
        Set("hash", hash.GetResultHex());
        data.erase(sm_PreviewSize);
        Set("preview", NStr::Base64Encode(data));
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobInfo> blob_info)
{
    Set("id",                 blob_info);
    Set("compression",        blob_info->GetCompression());
    Set("format",             blob_info->GetFormat());
    Set("storage_size",       blob_info->GetStorageSize());
    Set("size",               blob_info->GetSize());
    Set("is_dead",                      blob_info->IsState(CPSG_BlobInfo::eDead));
    Set("is_suppressed",                blob_info->IsState(CPSG_BlobInfo::eSuppressed));
    Set("is_suppressed_temporarily",    blob_info->IsState(CPSG_BlobInfo::eSuppressedTemporarily));
    Set("is_withdrawn",                 blob_info->IsState(CPSG_BlobInfo::eWithdrawn));
    Set("is_withdrawn_base",            blob_info->IsState(CPSG_BlobInfo::eWithdrawnBase));
    Set("is_withdrawn_permanently",     blob_info->IsState(CPSG_BlobInfo::eWithdrawnPermanently));
    Set("is_edit_blocked",              blob_info->IsState(CPSG_BlobInfo::eEditBlocked));
    Set("hup_release_date",   blob_info->GetHupReleaseDate().AsString());
    Set("owner",              blob_info->GetOwner());
    Set("original_load_date", blob_info->GetOriginalLoadDate().AsString());
    Set("class",              objects::CBioseq_set::ENUM_METHOD_NAME(EClass)()->FindName(blob_info->GetClass(), true));
    Set("division",           blob_info->GetDivision());
    Set("username",           blob_info->GetUsername());
    Set("id2_info",           blob_info->GetId2Info());
    Set("n_chunks",           blob_info->GetNChunks());
}

string s_ReasonToString(CPSG_SkippedBlob::EReason reason)
{
    switch (reason) {
        case CPSG_SkippedBlob::eExcluded:   return "Excluded";
        case CPSG_SkippedBlob::eInProgress: return "InProgress";
        case CPSG_SkippedBlob::eSent:       return "Sent";
        case CPSG_SkippedBlob::eUnknown:    return "Unknown";
    };

    _TROUBLE;
    return "";
}

void CJsonResponse::Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob)
{
    Set("id",     skipped_blob);
    Set("reason", s_ReasonToString(skipped_blob->GetReason()));
    Set("sent_seconds_ago", skipped_blob->GetSentSecondsAgo());
    Set("time_until_resend", skipped_blob->GetTimeUntilResend());
}

void CJsonResponse::Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    const auto included_info = bioseq_info->IncludedInfo();

    if (included_info & CPSG_Request_Resolve::fCanonicalId)  Set("canonical_id",  bioseq_info->GetCanonicalId());
    if (included_info & CPSG_Request_Resolve::fOtherIds)     Set("other_ids",     bioseq_info->GetOtherIds());
    if (included_info & CPSG_Request_Resolve::fMoleculeType) Set("molecule_type", objects::CSeq_inst::ENUM_METHOD_NAME(EMol)()->FindName(bioseq_info->GetMoleculeType(), true));
    if (included_info & CPSG_Request_Resolve::fLength)       Set("length",        bioseq_info->GetLength());
    if (included_info & CPSG_Request_Resolve::fChainState)   Set("seq_state",     bioseq_info->GetChainState());
    if (included_info & CPSG_Request_Resolve::fState)        Set("state",         bioseq_info->GetState());
    if (included_info & CPSG_Request_Resolve::fBlobId)       Set("blob_id",       bioseq_info->GetBlobId());
    if (included_info & CPSG_Request_Resolve::fTaxId)        Set("tax_id",        TAX_ID_TO(Int8, bioseq_info->GetTaxId()));
    if (included_info & CPSG_Request_Resolve::fHash)         Set("hash",          bioseq_info->GetHash());
    if (included_info & CPSG_Request_Resolve::fDateChanged)  Set("date_changed",  bioseq_info->GetDateChanged().AsString());
    if (included_info & CPSG_Request_Resolve::fGi)           Set("gi",            GI_TO(Int8, bioseq_info->GetGi()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    Set("name", named_annot_info->GetName());
    Set("blob_id", named_annot_info->GetBlobId());
    Set("id2_annot_info", named_annot_info->GetId2AnnotInfo());
}

void CJsonResponse::Fill(shared_ptr<CPSG_NamedAnnotStatus> named_annot_status)
{
    auto ar = m_JsonObj["statuses"].ResetArray();

    for (const auto& status : named_annot_status->GetId2AnnotStatusList()) {
        ar.push_back();
        auto obj = ar.back().ResetObject();
        Set(obj, "name",   status.first);
        Set(obj, "status", s_StrStatus(status.second));
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_PublicComment> public_comment)
{
    Set("id",   public_comment);
    Set("text", public_comment->GetText());
}

string s_ProgressStatusToString(CPSG_Processor::EProgressStatus progress_status)
{
    switch (progress_status) {
        case CPSG_Processor::eStart:     return "start";
        case CPSG_Processor::eDone:      return "done";
        case CPSG_Processor::eNotFound:  return "not_found";
        case CPSG_Processor::eCanceled:  return "canceled";
        case CPSG_Processor::eTimeout:   return "timeout";
        case CPSG_Processor::eError:     return "error";
        case CPSG_Processor::eUnauthorized:     return "unauthorized";
        case CPSG_Processor::eInProgress:       return "inprogress";
        case CPSG_Processor::eUnknown:   return "unknown";
    }

    _TROUBLE;
    return "";
}

void CJsonResponse::Fill(shared_ptr<CPSG_Processor> processor)
{
    const auto progress_status = processor->GetProgressStatus();

    // Progress status does not make sense without processor ID,
    // the latter is reported in the verbose mode elsewhere
    if (!SParams::verbose) {
        Set("processor_id", processor->GetProcessorId());
    }

    Set("progress_status", s_ProgressStatusToString(progress_status));
}

void CJsonResponse::Fill(shared_ptr<CPSG_IpgInfo> ipg_info)
{
    Set("protein",    ipg_info->GetProtein());
    Set("ipg",        ipg_info->GetIpg());
    Set("nucleotide", ipg_info->GetNucleotide());
    Set("tax_id",     ipg_info->GetTaxId());
    Set("gb_state",   ipg_info->GetGbState());
}

void CJsonResponse::Fill(shared_ptr<CPSG_AccVerHistory> acc_ver_history)
{
    Set("canonical_id", acc_ver_history->GetCanonicalId());
    Set("gi",           GI_TO(Int8, acc_ver_history->GetGi()));
    Set("date",         acc_ver_history->GetDate().AsString());
    Set("blob_id",      acc_ver_history->GetBlobId());
    Set("chain",        GI_TO(Int8, acc_ver_history->GetChain()));
}

void CJsonResponse::AddMessage(const SPSG_Message& message)
{
    auto messages = m_JsonObj["messages"];
    auto ar = messages.IsNull() ? messages.ResetArray() : messages.SetArray();
    ar.push_back();
    auto obj = ar.back().ResetObject();
    Set(obj, "severity", CNcbiDiag::SeverityName(message.severity));
    Set(obj, "code",     message.code);
    Set(obj, "text",     message);
}

void CJsonResponse::Set(CJson_Node node, const CPSG_BioId& bio_id)
{
    auto obj = node.ResetObject();
    Set(obj, "id", bio_id.GetId());

    const auto& type = bio_id.GetType();

    if (type != CPSG_BioId::TType::e_not_set) {
        Set(obj, "type", type);
    }
}

void CJsonResponse::Set(CJson_Node node, const vector<CPSG_BioId>& bio_ids)
{
    auto ar = node.ResetArray();

    for (const auto& bio_id : bio_ids) {
        ar.push_back();
        Set(ar.back(), bio_id);
    }
}

void CJsonResponse::Set(CJson_Node node, const CPSG_BlobId& blob_id)
{
    auto obj = node.ResetObject();
    Set(obj, "id",            blob_id.GetId());
    Set(obj, "last_modified", blob_id.GetLastModified());
}

void CJsonResponse::Set(CJson_Node node, const CPSG_ChunkId& chunk_id)
{
    auto obj = node.ResetObject();
    Set(obj, "id2_chunk", chunk_id.GetId2Chunk());
    Set(obj, "id2_info",  chunk_id.GetId2Info());
}

string SInteractiveParams::GetService(string service, bool one_server)
{
    if (one_server) {
        auto servers = CServiceDiscovery(service.empty() ? TPSG_Service::GetDefault() : service)();

        if (!servers.empty()) {
            sort(servers.begin(), servers.end(), [](const auto& l, const auto& r) { return l.second < r.second; });
            return servers.back().first.AsString();
        }
    }

    return service;
}

void SMetrics::OutputItems(ostream& os) const
{
    for (const auto& item : m_Items) {
        os << '\t' << s_GetItemName(item.first, false) << '=' << s_StrStatus(item.second);
    }
}

struct SDataOnlyCopy
{
    SDataOnlyCopy(const SOneRequestParams::SDataOnly& params) : m_Params(params) {}

    void ItemComplete(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item);
    void ReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply);

    operator int() const { return int(m_Status); }

private:
    void Process(shared_ptr<CPSG_BlobInfo> blob_info);
    void Process(shared_ptr<CPSG_BlobData> blob_data);
    void Process(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info);
    void Process(shared_ptr<CPSG_BlobInfo> blob_info, shared_ptr<CPSG_BlobData> blob_data);

    template <class TItem>
    bool ReportErrors(EPSG_Status status, TItem item, const char* prefix);

    const SOneRequestParams::SDataOnly& m_Params;
    unordered_map<string, pair<shared_ptr<CPSG_BlobInfo>, shared_ptr<CPSG_BlobData>>> m_Data;
    EPSG_Status m_Status = EPSG_Status::eSuccess;
};

template <class TItem>
bool SDataOnlyCopy::ReportErrors(EPSG_Status status, TItem item, const char* prefix)
{
    if (status == EPSG_Status::eSuccess) {
        return false;
    }

    if (m_Status == EPSG_Status::eSuccess) m_Status = status;

    stringstream ss;

    if (m_Params.messages_only) {
        auto delim = "";

        for (;;) {
            auto message = item->GetNextMessage();

            if (message.empty()) break;

            ss << delim << message;
            delim = ", ";
        }
    } else {
        ss << prefix << s_StrStatus(status);

        for (;;) {
            auto message = item->GetNextMessage();

            if (message.empty()) break;

            ss << "\n\t" << message;
        }

        ss << '\n';
    }

    cerr << ss.rdbuf();
    return true;
}

void SDataOnlyCopy::ItemComplete(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    if (ReportErrors(status, item, "Item error: ")) {
        return;

    } else if (item->GetType() == CPSG_ReplyItem::eBlobInfo) {
        Process(static_pointer_cast<CPSG_BlobInfo>(item));

    } else if (item->GetType() == CPSG_ReplyItem::eBlobData) {
        Process(static_pointer_cast<CPSG_BlobData>(item));

    } else if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
        Process(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
    }
}

void SDataOnlyCopy::ReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    ReportErrors(status, reply, "Reply error: ");
}

ESerialDataFormat s_GetInputFormat(const string& format)
{
    if (format == "asn1-text") return eSerial_AsnText;
    if (format == "xml")       return eSerial_Xml;
    if (format == "json")      return eSerial_Json;

    return eSerial_AsnBinary;
}

TTypeInfo s_GetInputType(const shared_ptr<CPSG_BlobData>& blob_data)
{
    using namespace objects;

    if (auto chunk_id = blob_data->GetId<CPSG_ChunkId>()) {
        return chunk_id->GetId2Chunk() == 999999999 ? CID2S_Split_Info::GetTypeInfo() : CID2S_Chunk::GetTypeInfo();
    }

    return CSeq_entry::GetTypeInfo();
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobInfo> blob_info)
{
    auto& p = m_Data[blob_info->GetId()->Repr()];

    if (p.second) {
        Process(std::move(blob_info), std::move(p.second));
    } else {
        p.first = std::move(blob_info);
    }
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobData> blob_data)
{
    auto& p = m_Data[blob_data->GetId()->Repr()];

    if (p.first) {
        Process(std::move(p.first), std::move(blob_data));
    } else {
        p.second = std::move(blob_data);
    }
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobInfo> blob_info, shared_ptr<CPSG_BlobData> blob_data)
{
    auto& is = blob_data->GetStream();

    if (m_Params.output_format == eSerial_None) {
        cout << is.rdbuf();
        return;
    }

    auto input_format = s_GetInputFormat(blob_info->GetFormat());
    unique_ptr<CObjectIStream> in;

    if (blob_info->GetCompression().find("zip") == string::npos) {
        in.reset(CObjectIStream::Open(input_format, is));
    } else {
        unique_ptr<CZipStreamDecompressor> zip(new CZipStreamDecompressor);
        unique_ptr<CCompressionIStream> compressed_is(new CCompressionIStream(is, zip.release(), CCompressionIStream::fOwnProcessor));
        in.reset(CObjectIStream::Open(input_format, *compressed_is.release(), eTakeOwnership));
    }

    _ASSERT(in);
    in->UseMemoryPool();

    stringstream ss;
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(m_Params.output_format, ss));
    CObjectStreamCopier copier(*in, *out);

    try {
        copier.Copy(s_GetInputType(blob_data));
    }
    catch (CException& ex) {
        cerr << "Failed to process blob '" << blob_data->GetId()->Repr() << "': " << ex.ReportThis() << endl;
        return;
    }

    cout << ss.rdbuf();
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    if (m_Params.output_format == eSerial_None) {
        cout << NStr::Base64Decode(named_annot_info->GetId2AnnotInfo());
        return;
    }

    for (const auto& info : named_annot_info->GetId2AnnotInfoList() ) {
        cout << MSerial_Format(m_Params.output_format) << *info;
    }
}

template <class... TArgs>
struct SNonVerboseBase
{
    static void ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
    {
        json_out << CJsonResponse(status, item);
    }

    static void ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
    {
        json_out << CJsonResponse(status, reply);
    }
};

template <class... TArgs>
struct SNonVerbose : SNonVerboseBase<TArgs...>
{
    using SNonVerboseBase<TArgs...>::SNonVerboseBase;

    void ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item);
    void ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply);

private:
    using TBase = SNonVerboseBase<TArgs...>;

    map<CPSG_ReplyItem::EType, shared_ptr<CPSG_ReplyItem>> m_Items;
};

template <class... TArgs>
void SNonVerbose<TArgs...>::ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    const auto type = item->GetType();

    if (status == EPSG_Status::eNotFound) {
        m_Items.try_emplace(type, item);
    } else {
        TBase::ItemComplete(json_out, status, item);

        if (auto [it, result] = m_Items.try_emplace(type, nullptr); !result && it->second) {
            it->second.reset();
        }
    }
}

template <class... TArgs>
void SNonVerbose<TArgs...>::ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    for (const auto& p : m_Items) {
        if (auto& item = p.second) {
            TBase::ItemComplete(json_out, EPSG_Status::eNotFound, item);
        }
    }

    if (status != EPSG_Status::eSuccess) {
        TBase::ReplyComplete(json_out, status, reply);
    }
}

template <class... TArgs>
void s_ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    json_out << CJsonResponse(status, item);
}

template <class... TArgs>
void s_ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    json_out << CJsonResponse(status, reply);
}

template <>
void s_ReplyComplete<SOneRequestParams>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        s_ReplyComplete<>(json_out, status, reply);
    }
}

template <class... TArgs>
void s_NewItem(SJsonOut&, const shared_ptr<CPSG_ReplyItem>&)
{
}

template <>
void s_NewItem<SOneRequestParams>(SJsonOut& json_out, const shared_ptr<CPSG_ReplyItem>& reply_item)
{
    json_out << CJsonResponse::NewItem(reply_item);
}

int CProcessing::OneRequest(const SOneRequestParams& params, shared_ptr<CPSG_Request> request)
{
    SDataOnlyCopy data_only_copy(params.data_only);
    CLogLatencyReport latency_report{
        "/objtools/pubseq_gateway/client/",
        R"(\S+: (\S+:[0-9]+)/\S+?\S+&client_id=\S+)",
        R"(\S+: Closed with status \S+)",
        R"(\S+: \S+&item_type=reply&\S+&exec_time=([0-9]+)\\n)"
    };

    if (params.latency.which) {
        latency_report.Start(params.latency.which);
        latency_report.SetDebug(params.latency.debug);
        TPSG_DebugPrintout::SetDefault(TPSG_DebugPrintout::TValue::eSome);
    }

    CPSG_EventLoop queue;
    SJsonOut json_out;
    SNonVerbose<> non_verbose;

    using namespace placeholders;

    if (params.data_only.enabled) {
        auto item_complete = bind(&SDataOnlyCopy::ItemComplete, &data_only_copy, _1, _2);
        auto reply_complete = bind(&SDataOnlyCopy::ReplyComplete, &data_only_copy, _1, _2);
        queue = CPSG_EventLoop(params.service, item_complete, reply_complete);
    } else if (!params.verbose) {
        auto item_complete = bind(&SNonVerbose<>::ItemComplete, &non_verbose, ref(json_out), _1, _2);
        auto reply_complete = bind(&SNonVerbose<>::ReplyComplete, &non_verbose, ref(json_out), _1, _2);
        queue = CPSG_EventLoop(params.service, item_complete, reply_complete);
    } else {
        auto item_complete = bind(&s_ItemComplete<SOneRequestParams>, ref(json_out), _1, _2);
        auto reply_complete = bind(&s_ReplyComplete<SOneRequestParams>, ref(json_out), _1, _2);
        auto new_item = bind(&s_NewItem<SOneRequestParams>, ref(json_out), _1);
        queue = CPSG_EventLoop(params.service, item_complete, reply_complete, new_item);
    }

    CJsonResponse::SetReplyType(true);
    _VERIFY(queue.SendRequest(request, CDeadline::eInfinite));
    queue.Stop();
    _VERIFY(queue.Run(CDeadline::eInfinite));
    return data_only_copy;
}

template <>
struct SNonVerboseBase<SBatchResolveParams> : string
{
    set<EPSG_Status> reported;
    SNonVerboseBase(string id) : string(std::move(id)) {}

    void ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item);
    void ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply);
};

template <class TParams>
void CParallelProcessing<TParams>::SImpl::Init(const TParams&)
{
        CJsonResponse::SetReplyType(false);
}

template <>
void CParallelProcessing<SBatchResolveParams>::SImpl::Submitter(CPSG_Queue& output)
{
    string id;

    while (m_InputQueue.Pop(id)) {
        _ASSERT(!id.empty()); // ReadLine makes sure it's not empty
        auto bio_id = CPSG_BioId(id, m_Params.type);
        auto user_context = make_shared<SNonVerbose<SBatchResolveParams>>(std::move(id));
        auto request = make_shared<CPSG_Request_Resolve>(std::move(bio_id), m_Params.bio_id_resolution, std::move(user_context));

        request->IncludeInfo(m_Params.include_info);
        request->SetAccSubstitution(m_Params.acc_substitution);

        _VERIFY(output.SendRequest(std::move(request), CDeadline::eInfinite));
    }

    output.Stop();
}

using verbose = true_type;
using no_verbose = false_type;

template <>
void s_ItemComplete<no_verbose>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    auto context = item->GetReply()->GetRequest()->GetUserContext<SNonVerbose<SBatchResolveParams>>();
    _ASSERT(context);
    context->ItemComplete(json_out, status, item);
}

void SNonVerboseBase<SBatchResolveParams>::ItemComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    reported.emplace(status);
    s_ItemComplete<verbose>(json_out, status, item);
}

template <>
void s_ReplyComplete<no_verbose>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    auto context = reply->GetRequest()->GetUserContext<SNonVerbose<SBatchResolveParams>>();
    _ASSERT(context);
    context->ReplyComplete(json_out, status, reply);
}

void SNonVerboseBase<SBatchResolveParams>::ReplyComplete(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (auto first_message = reply->GetNextMessage(SParams::min_severity); first_message || (reported.find(status) == reported.end())) {
        CJsonResponse result_doc(status, reply, first_message);
        json_out << result_doc;
    }
}

template <class TParams>
typename CParallelProcessing<TParams>::SImpl::TItemComplete CParallelProcessing<TParams>::SImpl::GetItemComplete()
{
    // MS VS does not like the ternary operator here anymore
    if (SParams::verbose) {
        return &s_ItemComplete<verbose>;
    } else {
        return &s_ItemComplete<no_verbose>;
    }
}

template <class TParams>
typename CParallelProcessing<TParams>::SImpl::TReplyComplete CParallelProcessing<TParams>::SImpl::GetReplyComplete()
{
    // MS VS does not like the ternary operator here anymore
    if (SParams::verbose) {
        return &s_ReplyComplete<verbose>;
    } else {
        return &s_ReplyComplete<no_verbose>;
    }
}

template <class TParams>
typename CParallelProcessing<TParams>::SImpl::TNewItem CParallelProcessing<TParams>::SImpl::GetNewItem()
{
    return &s_NewItem<TParams>;
}

template <>
void CParallelProcessing<TIpgBatchResolveParams>::SImpl::Submitter(CPSG_Queue& output)
{
    string line, protein, n;

    while (m_InputQueue.Pop(line)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty

        auto nucleotide = NStr::SplitInTwo(line, ",", protein, n) ? CPSG_Request_IpgResolve::TNucleotide(n) : null;
        auto user_context = make_shared<SNonVerbose<SBatchResolveParams>>(std::move(line));
        auto request = make_shared<CPSG_Request_IpgResolve>(std::move(protein), 0, std::move(nucleotide),  std::move(user_context));

        _VERIFY(output.SendRequest(std::move(request), CDeadline::eInfinite));
    }

    output.Stop();
}

template <>
void CParallelProcessing<SInteractiveParams>::SImpl::Init(const SInteractiveParams& params)
{
    CJsonResponse::SetDataLimit(params.data_limit);
    CJsonResponse::SetPreviewSize(params.preview_size);
}

template <>
void CParallelProcessing<SInteractiveParams>::SImpl::Submitter(CPSG_Queue& output)
{
    CJson_Document json_schema_doc(CProcessing::RequestSchema());
    CJson_Schema json_schema(json_schema_doc);
    string line;

    while (m_InputQueue.Pop(line)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty

        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            json_out << CJsonResponse(s_GetId(json_doc), eJsonRpc_ParseError, json_doc.GetReadError());
        } else if (!json_schema.Validate(json_doc)) {
            json_out << CJsonResponse(s_GetId(json_doc), eJsonRpc_InvalidRequest, json_schema.GetValidationError());
        } else {
            if (m_Params.echo) json_out << json_doc;

            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto id = json_obj["id"].GetValue().GetString();
            auto params_obj = json_obj["params"].GetObject();
            auto user_context = make_shared<string>(id);
            auto request_context = m_Params.testing ? null : SInteractiveNewRequestStart(method, params_obj).Get();

            if (auto request = SRequestBuilder::Build(method, params_obj, std::move(user_context), std::move(request_context))) {
                _VERIFY(output.SendRequest(std::move(request), CDeadline::eInfinite));
            }
        }
    }

    output.Stop();
}

using testing = true_type;
using no_testing = false_type;
using server_mode = true_type;
using no_server_mode = false_type;

template <>
void s_ReplyComplete<SInteractiveParams, testing, server_mode>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    const auto request = reply->GetRequest();
    const auto& request_id = *request->GetUserContext<string>();

    CJsonResponse result_doc(status, reply, CJsonResponse::eDoNotAddRequestID);
    json_out << CJsonResponse(request_id, result_doc);
}

template <>
void s_ReplyComplete<SInteractiveParams, testing, no_server_mode>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        s_ReplyComplete<SInteractiveParams, testing, server_mode>(json_out, status, reply);
    }
}

template <>
void s_ReplyComplete<SInteractiveParams, no_testing, server_mode>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    const auto request = reply->GetRequest();
    CRequestContextGuard_Base guard(request->GetRequestContext()->Clone());
    guard.SetStatus(s_PsgStatusToRequestStatus(status));

    s_ReplyComplete<SInteractiveParams, testing, server_mode>(json_out, status, reply);
}

template <>
void s_ReplyComplete<SInteractiveParams, no_testing, no_server_mode>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    const auto request = reply->GetRequest();
    CRequestContextGuard_Base guard(request->GetRequestContext()->Clone());
    guard.SetStatus(s_PsgStatusToRequestStatus(status));

    s_ReplyComplete<SInteractiveParams, testing, no_server_mode>(json_out, status, reply);
}

template <>
void s_ItemComplete<SInteractiveParams>(SJsonOut& json_out, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    const auto request = item->GetReply()->GetRequest();
    const auto& request_id = *request->GetUserContext<string>();

    CJsonResponse result_doc(status, item, CJsonResponse::eDoNotAddRequestID);
    json_out << CJsonResponse(request_id, result_doc);
}

template <>
CParallelProcessing<SInteractiveParams>::SImpl::TItemComplete CParallelProcessing<SInteractiveParams>::SImpl::GetItemComplete()
{
    return &s_ItemComplete<SInteractiveParams>;
}

template <>
CParallelProcessing<SInteractiveParams>::SImpl::TReplyComplete CParallelProcessing<SInteractiveParams>::SImpl::GetReplyComplete()
{
    if (m_Params.testing) {
        // MS VS does not like the ternary operator here anymore
        if (m_Params.server) {
            return &s_ReplyComplete<SInteractiveParams, testing, server_mode>;
        } else {
            return &s_ReplyComplete<SInteractiveParams, testing, no_server_mode>;
        }
    } else {
        // MS VS does not like the ternary operator here anymore
        if (m_Params.server) {
            return &s_ReplyComplete<SInteractiveParams, no_testing, server_mode>;
        } else {
            return &s_ReplyComplete<SInteractiveParams, no_testing, no_server_mode>;
        }
    }
}

template <>
void s_NewItem<SInteractiveParams, verbose>(SJsonOut& json_out, const shared_ptr<CPSG_ReplyItem>& item)
{
    const auto request = item->GetReply()->GetRequest();
    const auto& request_id = *request->GetUserContext<string>();

    auto result_doc = CJsonResponse::NewItem(item);
    json_out << CJsonResponse(request_id, result_doc);
}

template <>
CParallelProcessing<SInteractiveParams>::SImpl::TNewItem CParallelProcessing<SInteractiveParams>::SImpl::GetNewItem()
{
    // MS VS does not like the ternary operator here anymore
    if (SParams::verbose) {
        return &s_NewItem<SInteractiveParams, verbose>;
    } else {
        return &s_NewItem<SInteractiveParams, no_verbose>;
    }
}

template <class TParams>
CParallelProcessing<TParams>::SImpl::SImpl(const TParams& params) :
    json_out(params.pipe, params.server),
    m_Params(params)
{
    Init(params);
}

template <class TParams>
CParallelProcessing<TParams>::CParallelProcessing(const TParams& params) :
    m_Impl(params)
{
    using namespace placeholders;
    auto item_complete = bind(m_Impl.GetItemComplete(), ref(m_Impl.json_out), _1, _2);
    auto reply_complete = bind(m_Impl.GetReplyComplete(), ref(m_Impl.json_out), _1, _2);
    auto new_item = bind(m_Impl.GetNewItem(), ref(m_Impl.json_out), _1);

    for (int n = params.worker_threads; n > 0; --n) {
        m_PsgQueues.emplace_back(params.service, item_complete, reply_complete, new_item);
        auto& queue = m_PsgQueues.back();
        queue.SetRequestFlags(params.request_flags);
        queue.SetUserArgs(params.user_args);
        m_Threads.emplace_back(&CPSG_EventLoop::Run, ref(queue), CDeadline::eInfinite);
        m_Threads.emplace_back(&SImpl::Submitter, &m_Impl, ref(queue));
    }
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_ReplyItem>& item)
{
    return item->GetReply();
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_Reply>& reply)
{
    return reply;
}

CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status)
{
    switch (psg_status) {
        case EPSG_Status::eSuccess:  return CRequestStatus::e200_Ok;
        case EPSG_Status::eNotFound: return CRequestStatus::e404_NotFound;
        case EPSG_Status::eCanceled: return CRequestStatus::e499_BrokenConnection;
        case EPSG_Status::eForbidden:return CRequestStatus::e403_Forbidden;
        case EPSG_Status::eError:    return CRequestStatus::e400_BadRequest;
        case EPSG_Status::eInProgress: _TROUBLE; break;
    }

    return CRequestStatus::e500_InternalServerError;
}

string s_GetId(const CJson_Document& req_doc)
{
    string id;

    if (req_doc.IsObject()) {
        auto req_obj = req_doc.GetObject();

        if (req_obj.has("id")) {
            auto id_node = req_obj["id"];

            if (id_node.IsValue()) {
                auto id_value = id_node.GetValue();

                if (id_value.IsString()) {
                    id = id_value.GetString();
                }
            }
        }
    }

    return id;
}

template <class TCreateContext>
vector<shared_ptr<CPSG_Request>> CProcessing::ReadCommands(TCreateContext create_context, size_t report_progress_after)
{
    static CJson_Schema json_schema(RequestSchema());
    string line;
    vector<shared_ptr<CPSG_Request>> requests;
    unordered_set<string> ids;

    // Read requests from cin
    while (ReadLine(line)) {
        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            cerr << "Error in request '" << line << "': " << json_doc.GetReadError() << endl;
            return {};
        }

        auto id = s_GetId(json_doc);

        if (id.empty()) {
            cerr << "Error in request '" << line << "': no id or id is empty" << endl;
            return {};
        } else if (!json_schema.Validate(json_doc)) {
            cerr << "Error in request '" << id << "': " << json_schema.GetValidationError() << endl;
            return {};
        } else if (!ids.insert(id).second) {
            cerr << "Error in request '" << id << "': duplicate ID" << endl;
            return {};
        } else {
            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto params_obj = json_obj["params"].GetObject();
            auto user_context = create_context(id, params_obj);

            if (!user_context) return {};

            if (auto request = SRequestBuilder::Build(method, params_obj, std::move(user_context))) {
                requests.emplace_back(std::move(request));
                if (report_progress_after && (requests.size() % report_progress_after == 0)) cerr << '.';
            }
        }
    }

    return requests;
}

int CProcessing::Performance(const SPerformanceParams& params)
{
    if (params.delay < 0.0) {
        cerr << "DELAY must be non-negative" << endl;
        return -1;
    }

    const size_t kReportProgressAfter = 2000;

    using TReplyStorage = deque<shared_ptr<CPSG_Reply>>;

    if (SParams::verbose) cerr << "Preparing requests: ";
    auto requests = ReadCommands([](string id, CJson_ConstNode&){ return make_shared<SMetrics>(std::move(id)); }, SParams::verbose ? kReportProgressAfter : 0);

    if (requests.empty()) return -1;

    atomic_size_t start(params.user_threads);
    atomic_int to_submit(static_cast<int>(requests.size()));
    auto wait = [&]() { while (start > 0) this_thread::sleep_for(chrono::microseconds(1)); };

    auto l = [&](CPSG_Queue& queue, TReplyStorage& replies) {
        start--;
        wait();

        for (;;) {
            auto i = to_submit--;

            if (i <= 0) break;

            // Submit and get response
            auto& request = requests[requests.size() - i];
            auto metrics = request->GetUserContext<SMetrics>();

            metrics->Set(SMetricType::eStart);
            auto reply = queue.SendRequestAndGetReply(request, CDeadline::eInfinite);
            metrics->Set(SMetricType::eSubmit);

            _ASSERT(reply);

            metrics->Set(SMetricType::eReply);

            for (;;) {
                auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
                _ASSERT(reply_item);

                const auto type = reply_item->GetType();

                if (type == CPSG_ReplyItem::eEndOfReply) break;

                const auto status = reply_item->GetStatus(CDeadline::eInfinite);
                metrics->AddItem({type, status});
            }

            const auto status = reply->GetStatus(CDeadline::eInfinite);
            metrics->Set(SMetricType::eDone);
            metrics->AddItem({CPSG_ReplyItem::eEndOfReply, status});

            if (params.report_immediately) {
                // Metrics are reported on destruction
                metrics.reset();
                request.reset();
                reply.reset();
            } else {
                // Store the reply for now to prevent metrics from being written to cout (affects performance)
                replies.emplace_back(reply);
            }

            if (params.delay) {
                this_thread::sleep_for(chrono::duration<double>(params.delay));
            }
        }
    };

    vector<CPSG_Queue> queues;
    vector<TReplyStorage> replies(params.user_threads);

    for (size_t i = 0; i < (params.local_queue ? params.user_threads : 1); ++i) {
        queues.emplace_back(params.service);
        queues.back().SetRequestFlags(params.request_flags);
        queues.back().SetUserArgs(params.user_args);
    }

    vector<thread> threads;
    threads.reserve(params.user_threads);

    // Start threads in advance so it won't affect metrics
    for (size_t i = 0; i < params.user_threads; ++i) {
        threads.emplace_back(l, ref(queues[params.local_queue ? i : 0]), ref(replies[i]));
    }

    wait();

    // Start processing replies
    if (SParams::verbose) {
        cerr << "\nSubmitting requests: ";
        size_t previous = requests.size() / kReportProgressAfter;

        while (to_submit > 0) {
            size_t current = to_submit / kReportProgressAfter;

            for (auto i = current; i < previous; ++i) {
                cerr << '.';
            }

            previous = current;
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        cerr << "\nWaiting for threads: " << params.user_threads << '\n';
    }

    for (auto& t : threads) {
        t.join();
    }

    // Release all replies held in queues, if any
    queues.clear();

    // Output metrics
    if (SParams::verbose) cerr << "Outputting metrics: ";
    requests.clear();
    size_t output = 0;

    for (auto& thread_replies : replies) {
        for (auto& reply : thread_replies) {
            reply.reset();
            if (SParams::verbose && (++output % kReportProgressAfter == 0)) cerr << '.';
        }
    }

    if (SParams::verbose) cerr << '\n';
    return 0;
}

bool CProcessing::ReadLine(string& line, istream& is, bool single_doc)
{
    for (;;) {
        if (single_doc ? !getline(is, line, '\x00') : !getline(is, line)) {
            return false;
        } else if (!line.empty()) {
            return true;
        }
    }
}

CParallelProcessing<SBatchResolveParams> CProcessing::CreateParallelProcessing(const SBatchResolveParams& params) { return params; }
CParallelProcessing<TIpgBatchResolveParams> CProcessing::CreateParallelProcessing(const TIpgBatchResolveParams& params) { return params; }
CParallelProcessing<SInteractiveParams> CProcessing::CreateParallelProcessing(const SInteractiveParams& params) { return params; }

SInteractiveNewRequestStart::SInteractiveNewRequestStart(const string& request, CJson_ConstObject params_obj)
{
    // All JSON types have already been validated with the scheme

    auto context = params_obj.find("context");
    auto& ctx = CDiagContext::GetRequestContext();

    if (context != params_obj.end()) {
        auto context_obj = context->value.GetObject();

        auto sid = context_obj.find("sid");

        if (sid != context_obj.end()) {
            ctx.SetSessionID(sid->value.GetValue().GetString());
        }

        auto phid = context_obj.find("phid");

        if (phid != context_obj.end()) {
            ctx.SetHitID(phid->value.GetValue().GetString());
        }

        auto auth_token = context_obj.find("auth_token");

        if (auth_token != context_obj.end()) {
            ctx.SetProperty("auth_token", auth_token->value.GetValue().GetString());
        }

        auto client_ip = context_obj.find("client_ip");

        if (client_ip != context_obj.end()) {
            ctx.SetClientIP(client_ip->value.GetValue().GetString());
        }
    }

    if (!ctx.IsSetSessionID()) ctx.SetSessionID();
    if (!ctx.IsSetHitID())     ctx.SetHitID();

    SExtra extra;
    extra.Print("request", request);
    extra.Print("params", params_obj);
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstValue json)
{
    _ASSERT(json.IsNumber());

    if (json.IsInt4()) {
        Print(prefix, json.GetInt4());
    } else if (json.IsUint4()) {
        Print(prefix, json.GetUint4());
    } else if (json.IsInt8()) {
        Print(prefix, json.GetInt8());
    } else if (json.IsUint8()) {
        Print(prefix, json.GetUint8());
    } else if (json.IsDouble()) {
        Print(prefix, json.GetDouble());
    } else {
        _TROUBLE;
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstArray json)
{
    for (size_t i = 0; i < json.size(); ++i) {
        Print(prefix + '[' + to_string(i) + ']', json[i]);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstObject json)
{
    for (const auto& pair : json) {
        Print(prefix + '.' + pair.name, pair.value);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstNode json)
{
    switch (json.GetType()) {
        case CJson_ConstNode::eNull:
            Print(prefix, "<null>");
            break;

        case CJson_ConstNode::eBool:
            Print(prefix, json.GetValue().GetBool() ? "true" : "false");
            break;

        case CJson_ConstNode::eString:
            Print(prefix, json.GetValue().GetString());
            break;

        case CJson_ConstNode::eNumber:
            Print(prefix, json.GetValue());
            break;

        case CJson_ConstNode::eArray:
            Print(prefix, json.GetArray());
            break;

        case CJson_ConstNode::eObject:
            Print(prefix, json.GetObject());
            break;
    };
}

int CProcessing::JsonCheck(istream* schema_is, bool single_doc)
{
    CJson_Document schema_doc;

    if (!schema_is) {
        schema_doc = RequestSchema();

    } else if (!schema_doc.Read(*schema_is)) {
        cerr << "Error on reading JSON schema: " << schema_doc.GetReadError() << endl;
        return -1;

    } else if (CJson_MetaSchema meta_schema; !meta_schema.Validate(schema_doc)) {
        cerr << "Error on validating JSON schema: " << meta_schema.GetValidationError() << endl;
        return -1;
    }

    CJson_Schema schema(schema_doc);
    string line;
    size_t line_no = 0;
    int rv = 0;
    auto cout_prefix = [&](const auto p) -> auto& { cout << p; if (!single_doc) cout << " (" << line_no << ")"; return cout; };

    while (ReadLine(line, cin, single_doc)) {
        CJson_Document input_doc;
        ++line_no;

        if (!input_doc.ParseString(line)) {
            cout_prefix("Error on reading JSON document") << ": " << input_doc.GetReadError() << endl;
            if (rv == 0) rv = -2;
        } else if (schema.Validate(input_doc)) {
            cout_prefix("JSON document") << " is valid" << endl;
        } else {
            cout_prefix("Error on validating JSON document") << ": " << schema.GetValidationError() << endl;
            if (rv == 0) rv = -3;
        }
    }

    if (single_doc) return rv;

    cout << line_no << " JSON document(s) have been checked" << endl;
    return rv;
}

CPSG_BioId::TType SRequestBuilder::GetBioIdType(const string& type)
{
    CObjectTypeInfo info(objects::CSeq_id::GetTypeInfo());

    if (auto index = info.FindVariantIndex(type)) return static_cast<CPSG_BioId::TType>(index);
    if (auto value = objects::CSeq_id::WhichInverseSeqId(type)) return value;

    return static_cast<CPSG_BioId::TType>(atoi(type.c_str()));
}

CPSG_BioId SRequestBuilder::SReader<CJson_ConstObject>::GetBioId(const CJson_ConstArray& array) const
{
    auto id = array[0].GetValue().GetString();

    if (array.size() == 1) return CPSG_BioId(id);

    auto value = array[1].GetValue();
    auto type = value.IsString() ? GetBioIdType(value.GetString()) : static_cast<CPSG_BioId::TType>(value.GetInt4());
    return CPSG_BioId(id, type);
}

CPSG_BioIds SRequestBuilder::SReader<CJson_ConstObject>::GetBioIds() const
{
    CPSG_BioIds rv;

    if (input.has("bio_ids")) {
        auto bio_ids = input["bio_ids"].GetArray();

        for (const auto& bio_id : bio_ids) {
            rv.push_back(GetBioId(bio_id.GetArray()));
        }
    } else {
        rv.push_back(GetBioId(input["bio_id"].GetArray()));
    }

    return rv;
}

CPSG_BlobId SRequestBuilder::SReader<CJson_ConstObject>::GetBlobId() const
{
    auto array = input["blob_id"].GetArray();
    auto id = array[0].GetValue().GetString();
    return array.size() > 1 ? CPSG_BlobId(std::move(id), array[1].GetValue().GetInt8()) : std::move(id);
}

CPSG_ChunkId SRequestBuilder::SReader<CJson_ConstObject>::GetChunkId() const
{
    auto array = input["chunk_id"].GetArray();
    return { static_cast<int>(array[0].GetValue().GetInt4()), array[1].GetValue().GetString() };
}

vector<string> SRequestBuilder::SReader<CJson_ConstObject>::GetNamedAnnots() const
{
    auto na_array = input["named_annots"].GetArray();
    CPSG_Request_NamedAnnotInfo::TAnnotNames names;

    for (const auto& na : na_array) {
        names.push_back(na.GetValue().GetString());
    }

    return names;
}

template <>
CPSG_Request_Resolve::TIncludeInfo SRequestBuilder::SImpl<CPSG_Request_Resolve>::GetIncludeInfo(TSpecified specified)
{
    const auto& info_flags = GetInfoFlags();

    auto i = info_flags.begin();
    bool all_info_except = specified(i->name);
    CPSG_Request_Resolve::TIncludeInfo include_info = all_info_except ? CPSG_Request_Resolve::fAllInfo : CPSG_Request_Resolve::TIncludeInfo(0);

    for (++i; i != info_flags.end(); ++i) {
        if (specified(i->name)) {
            if (all_info_except) {
                include_info &= ~i->value;
            } else {
                include_info |= i->value;
            }
        }
    }

    // Provide all info if nothing is specified explicitly
    return include_info ? include_info : CPSG_Request_Resolve::fAllInfo;
}

void SRequestBuilder::SReader<CJson_ConstObject>::ForEachTSE(TExclude exclude) const
{
    if (!input.has("exclude_blobs")) return;

    auto blob_ids = input["exclude_blobs"].GetArray();

    for (const auto& blob_id : blob_ids) {
        exclude(blob_id.GetValue().GetString());
    }
}

void SRequestBuilder::SReader<CJson_ConstObject>::SetRequestFlags(shared_ptr<CPSG_Request> request) const
{
    if (!input.has("request_flags")) return;

    for (const auto& request_flag : input["request_flags"].GetArray()) {
        const auto value = request_flag.GetValue().GetString();

        if (value == "exclude-hup") {
            request->SetFlags(CPSG_Request::fExcludeHUP);

        } else if (value == "include-hup") {
            request->SetFlags(CPSG_Request::fIncludeHUP);
        }
    }
}

const initializer_list<SDataFlag> kDataFlags =
{
    { "no-tse",    "Return only the info",                                  CPSG_Request_Biodata::eNoTSE    },
    { "slim-tse",  "Return split info blob if available, or nothing",       CPSG_Request_Biodata::eSlimTSE  },
    { "smart-tse", "Return split info blob if available, or original blob", CPSG_Request_Biodata::eSmartTSE },
    { "whole-tse", "Return all split blobs if available, or original blob", CPSG_Request_Biodata::eWholeTSE },
    { "orig-tse",  "Return original blob",                                  CPSG_Request_Biodata::eOrigTSE  },
};

const initializer_list<SDataFlag>& SRequestBuilder::GetDataFlags()
{
    return kDataFlags;
}

const initializer_list<SInfoFlag> kInfoFlags =
{
    { "all-info-except", "Return all info except explicitly specified by other flags", CPSG_Request_Resolve::fAllInfo },
    { "canonical-id",    "Return canonical ID info",                    CPSG_Request_Resolve::fCanonicalId  },
    { "name",            "Use name for canonical ID info, if returned", CPSG_Request_Resolve::fName         },
    { "other-ids",       "Return other IDs info",                       CPSG_Request_Resolve::fOtherIds     },
    { "molecule-type",   "Return molecule type info",                   CPSG_Request_Resolve::fMoleculeType },
    { "length",          "Return length info",                          CPSG_Request_Resolve::fLength       },
    { "chain-state",     "Return chain state info (in seq_state pair)", CPSG_Request_Resolve::fChainState   },
    { "state",           "Return state info",                           CPSG_Request_Resolve::fState        },
    { "blob-id",         "Return blob ID info",                         CPSG_Request_Resolve::fBlobId       },
    { "tax-id",          "Return tax ID info",                          CPSG_Request_Resolve::fTaxId        },
    { "hash",            "Return hash info",                            CPSG_Request_Resolve::fHash         },
    { "date-changed",    "Return date changed info",                    CPSG_Request_Resolve::fDateChanged  },
    { "gi",              "Return GI",                                   CPSG_Request_Resolve::fGi           },
};

const initializer_list<SInfoFlag>& SRequestBuilder::GetInfoFlags()
{
    return kInfoFlags;
}

CJson_Document CProcessing::RequestSchema()
{
    CJson_Document rv(R"REQUEST_SCHEMA(
{
    "type": "object",
    "definitions": {
        "jsonrpc": {
            "$id": "#jsonrpc",
            "enum": [
                "2.0"
            ]
        },
        "bio_id": {
            "$id": "#bio_id",
            "type": "array",
            "items": [
                {
                    "type": "string"
                },
                {
                    "oneOf": [
                        {
                            "type": "string"
                        },
                        {
                            "type": "number"
                        }
                    ]
                }
            ],
            "minItems": 1,
            "maxItems": 2
        },
        "bio_ids": {
            "$id": "#bio_ids",
            "type": "array",
            "items": {
                "$ref": "#/definitions/bio_id"
            },
            "minItems": 1
        },
        "blob_id": {
            "$id": "#blob_id",
            "type": "array",
            "items": [
                {
                    "type": "string"
                },
                {
                    "type": "number"
                }
            ],
            "minItems": 1,
            "maxItems": 2
        },
        "chunk_id": {
            "$id": "#chunk_id",
            "type": "array",
            "items": [
                {
                    "type": "number"
                },
                {
                    "type": "string"
                }
            ],
            "minItems": 2,
            "maxItems": 2
        },
        "include_data": {
            "$id": "#include_data",
            "enum": [
                "no-tse",
                "slim-tse",
                "smart-tse",
                "whole-tse",
                "orig-tse"
            ]
        },
        "include_info": {
            "$id": "#include_info",
            "type": "array",
            "items": {
                "type": "string",
                "enum": [
                    "all-info-except",
                    "canonical-id",
                    "name",
                    "other-ids",
                    "molecule-type",
                    "length",
                    "chain-state",
                    "state",
                    "blob-id",
                    "tax-id",
                    "hash",
                    "date-changed",
                    "gi"
                ]
            },
            "uniqueItems": true
        },
        "named_annots": {
            "$id": "#named_annots",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "exclude_blobs": {
            "$id": "#exclude_blobs",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "acc_substitution": {
            "$id": "#acc_substitution",
            "enum": [
                "default",
                "limited",
                "never"
            ]
        },
        "snp_scale_limit": {
            "$id": "#snp_scale_limit",
            "enum": [
                "default",
                "unit",
                "contig",
                "supercontig",
                "chromosome"
            ]
        },
        "context": {
            "$id": "#context",
            "type": "object",
            "properties": {
                "sid": {
                    "type": "string"
                },
                "phid": {
                    "type": "string"
                },
                "auth_token": {
                    "type": "string"
                },
                "client_ip": {
                    "type": "string"
                }
            }
        },
        "request_flags": {
            "$id": "#request_flags",
            "type": "array",
            "items": {
                "type": "string",
                "enum": [
                    "exclude-hup",
                    "include-hup"
                ]
            },
            "uniqueItems": true
        },
    )REQUEST_SCHEMA"
    R"REQUEST_SCHEMA(
        "biodata": {
            "$id": "#biodata",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "biodata"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id": {
                            "$ref": "#/definitions/bio_id"
                        },
                        "include_data": {
                            "$ref": "#/definitions/include_data"
                        },
                        "exclude_blobs": {
                            "$ref": "#/definitions/exclude_blobs"
                        },
                        "acc_substitution": {
                            "$ref": "#/definitions/acc_substitution"
                        },
                        "bio_id_resolution": {
                            "type": "boolean"
                        },
                        "resend_timeout": {
                            "type": "number"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "bio_id"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "blob": {
            "$id": "#blob",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "blob"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "blob_id": {
                            "$ref": "#/definitions/blob_id"
                        },
                        "include_data": {
                            "$ref": "#/definitions/include_data"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "blob_id"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "resolve": {
            "$id": "#resolve",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "resolve"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id": {
                            "$ref": "#/definitions/bio_id"
                        },
                        "include_info": {
                            "$ref": "#/definitions/include_info"
                        },
                        "acc_substitution": {
                            "$ref": "#/definitions/acc_substitution"
                        },
                        "bio_id_resolution": {
                            "type": "boolean"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "bio_id"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "named_annot": {
            "$id": "#named_annot",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "named_annot"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id": {
                            "$ref": "#/definitions/bio_id"
                        },
                        "bio_ids": {
                            "$ref": "#/definitions/bio_ids"
                        },
                        "named_annots": {
                            "$ref": "#/definitions/named_annots"
                        },
                        "acc_substitution": {
                            "$ref": "#/definitions/acc_substitution"
                        },
                        "bio_id_resolution": {
                            "type": "boolean"
                        },
                        "snp_scale_limit": {
                            "$ref": "#/definitions/snp_scale_limit"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "named_annots"
                    ],
                    "oneOf": [
                        {
                            "required": [
                                "bio_id"
                            ]
                        },
                        {
                            "required": [
                                "bio_ids"
                            ]
                        }
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "chunk": {
            "$id": "#chunk",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "chunk"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "chunk_id": {
                            "$ref": "#/definitions/chunk_id"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "chunk_id"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "ipg_resolve": {
            "$id": "#ipg_resolve",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "ipg_resolve"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "protein": {
                            "type": "string"
                        },
                        "ipg": {
                            "type": "number"
                        },
                        "nucleotide": {
                            "type": "string"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "dependencies": {
                        "nucleotide": [
                            "protein"
                        ]
                    },
                    "anyOf": [
                        {
                            "required": [
                                "protein"
                            ]
                        },
                        {
                            "required": [
                                "ipg"
                            ]
                        }
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "acc_ver_history": {
            "$id": "#acc_ver_history",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "acc_ver_history"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id": {
                            "$ref": "#/definitions/bio_id"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "bio_id"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        },
        "raw": {
            "$id": "#raw",
            "type": "object",
            "properties": {
                "jsonrpc": {
                    "$ref": "#/definitions/jsonrpc"
                },
                "method": {
                    "enum": [
                        "raw"
                    ]
                },
                "params": {
                    "type": "object",
                    "properties": {
                        "abs_path_ref": {
                            "type": "string"
                        },
                        "context": {
                            "$ref": "#/definitions/context"
                        },
                        "request_flags": {
                            "$ref": "#/definitions/request_flags"
                        },
                        "user_args": {
                            "type": "string"
                        }
                    },
                    "required": [
                        "abs_path_ref"
                    ]
                },
                "id": {
                    "type": "string"
                }
            },
            "required": [
                "jsonrpc",
                "method",
                "params",
                "id"
            ]
        }
    },
    "oneOf": [
        {
            "$ref": "#/definitions/biodata"
        },
        {
            "$ref": "#/definitions/blob"
        },
        {
            "$ref": "#/definitions/resolve"
        },
        {
            "$ref": "#/definitions/named_annot"
        },
        {
            "$ref": "#/definitions/chunk"
        },
        {
            "$ref": "#/definitions/ipg_resolve"
        },
        {
            "$ref": "#/definitions/acc_ver_history"
        },
        {
            "$ref": "#/definitions/raw"
        }
    ]
}
        )REQUEST_SCHEMA");

    _DEBUG_CODE(if (!rv.ReadSucceeded()) { auto error = rv.GetReadError(); NCBI_ALWAYS_TROUBLE(error.c_str()); });
    _DEBUG_CODE(if (CJson_MetaSchema m; !m.Validate(rv)) { auto error = m.GetValidationError(); NCBI_ALWAYS_TROUBLE(error.c_str()); });
    return rv;
}

END_NCBI_SCOPE
