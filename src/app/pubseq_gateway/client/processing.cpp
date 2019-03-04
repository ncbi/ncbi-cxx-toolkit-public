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

#include <serial/enumvalues.hpp>

#include "performance.hpp"
#include "processing.hpp"

BEGIN_NCBI_SCOPE

struct SUserContext : string
{
    SUserContext(string id, atomic_int& counter) : string(id), m_Counter(counter) { ++m_Counter; }
    ~SUserContext() { --m_Counter; }

private:
    atomic_int& m_Counter;
};

SJsonOut& SJsonOut::operator<<(const CJson_Document& doc)
{
    ostringstream os;

    if (!m_Printed.exchange(true)) {
        os << "[\n";
    } else if (!m_Interactive) {
        os << ",\n";
    }

    if (m_Interactive) {
        os << doc << ",\n";
    } else {
        os << doc;
    }

    unique_lock<mutex> lock(m_Mutex);
    cout << os.str() << flush;
    return *this;
}

SJsonOut::~SJsonOut()
{
    if (m_Printed) {
        if (m_Interactive) {
            cout << CJson_Document();
        }

        cout << "\n]" << endl;
    }
}

template <class TItem>
CJsonResponse::CJsonResponse(EPSG_Status status, TItem item) :
    m_JsonObj(SetObject())
{
    if (auto request_id = s_GetReply(item)->GetRequest()->template GetUserContext<string>()) {
        m_JsonObj["request_id"].SetValue().SetString(*request_id);
    }

    Fill(status, item);
}

CJsonResponse::CJsonResponse(const string& id, bool result) :
    CJsonResponse(id)
{
    m_JsonObj["result"].SetValue().SetBool(result);
}

CJsonResponse::CJsonResponse(const string& id, const CJson_Document& result) :
    CJsonResponse(id)
{
    m_JsonObj["result"].AssignCopy(result);
}

CJsonResponse::CJsonResponse(const string& id, int code, const string& message) :
    CJsonResponse(id)
{
    CJson_Object error_obj = m_JsonObj.insert_object("error");
    error_obj["code"].SetValue().SetInt4(code);
    error_obj["message"].SetValue().SetString(message);
}

CJsonResponse::CJsonResponse(const string& id, int counter) :
    CJsonResponse(id)
{
    CJson_Object result_obj = m_JsonObj.insert_object("result");
    CJson_Object requests_obj = result_obj.insert_object("requests");
    requests_obj["in_progress"].SetValue().SetInt8(counter);
}

CJsonResponse::CJsonResponse(const string& id) :
    m_JsonObj(SetObject())
{
    m_JsonObj["jsonrpc"].SetValue().SetString("2.0");

    auto id_value = m_JsonObj["id"].SetValue();

    if (id.empty()) {
        id_value.SetNull();
    } else {
        id_value.SetString(id);
    }
}

void CJsonResponse::Fill(EPSG_Status reply_status, shared_ptr<CPSG_Reply> reply)
{
    switch (reply_status) {
        case EPSG_Status::eNotFound: m_JsonObj["reply"].SetValue().SetString("NotFound"); return;
        case EPSG_Status::eCanceled: m_JsonObj["reply"].SetValue().SetString("Canceled"); return;
        case EPSG_Status::eError:    Fill(reply, "Failure");                              return;
        default: _TROUBLE;
    }
}

void CJsonResponse::Fill(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
{
    auto reply_item_type = reply_item->GetType();

    if (reply_item_status == EPSG_Status::eError) {
        switch (reply_item_type) {
            case CPSG_ReplyItem::eBlobData:       return Fill(reply_item, "BlobData");
            case CPSG_ReplyItem::eBlobInfo:       return Fill(reply_item, "BlobInfo");
            case CPSG_ReplyItem::eBioseqInfo:     return Fill(reply_item, "BioseqInfo");
            case CPSG_ReplyItem::eNamedAnnotInfo: return Fill(reply_item, "NamedAnnotInfo");
            case CPSG_ReplyItem::eEndOfReply:     _TROUBLE; return;
        }
    }

    switch (reply_item_type) {
        case CPSG_ReplyItem::eBlobData:
            return Fill(static_pointer_cast<CPSG_BlobData>(reply_item));

        case CPSG_ReplyItem::eBlobInfo:
            return Fill(static_pointer_cast<CPSG_BlobInfo>(reply_item));

        case CPSG_ReplyItem::eBioseqInfo:
            return Fill(static_pointer_cast<CPSG_BioseqInfo>(reply_item));

        case CPSG_ReplyItem::eNamedAnnotInfo:
            return Fill(static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item));

        case CPSG_ReplyItem::eEndOfReply:
            _TROUBLE;
            return;
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobData> blob_data)
{
    m_JsonObj["reply"].SetValue().SetString("BlobData");
    m_JsonObj["id"].SetValue().SetString(blob_data->GetId().Get());
    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    m_JsonObj["data"].SetValue().SetString(NStr::JsonEncode(os.str()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobInfo> blob_info)
{
    m_JsonObj["reply"].SetValue().SetString("BlobInfo");
    m_JsonObj["id"].SetValue().SetString(blob_info->GetId().Get());
    m_JsonObj["compression"].SetValue().SetString(blob_info->GetCompression());
    m_JsonObj["format"].SetValue().SetString(blob_info->GetFormat());
    m_JsonObj["version"].SetValue().SetUint8(blob_info->GetVersion());
    m_JsonObj["storage_size"].SetValue().SetUint8(blob_info->GetStorageSize());
    m_JsonObj["size"].SetValue().SetUint8(blob_info->GetSize());
    m_JsonObj["is_dead"].SetValue().SetBool(blob_info->IsDead());
    m_JsonObj["is_suppressed"].SetValue().SetBool(blob_info->IsSuppressed());
    m_JsonObj["is_withdrawn"].SetValue().SetBool(blob_info->IsWithdrawn());
    m_JsonObj["hup_release_date"].SetValue().SetString(blob_info->GetHupReleaseDate().AsString());
    m_JsonObj["owner"].SetValue().SetUint8(blob_info->GetOwner());
    m_JsonObj["original_load_date"].SetValue().SetString(blob_info->GetOriginalLoadDate().AsString());
    m_JsonObj["class"].SetValue().SetString(objects::CBioseq_set::ENUM_METHOD_NAME(EClass)()->FindName(blob_info->GetClass(), true));
    m_JsonObj["division"].SetValue().SetString(blob_info->GetDivision());
    m_JsonObj["username"].SetValue().SetString(blob_info->GetUsername());
    m_JsonObj["split_info_blob_id"].SetValue().SetString(blob_info->GetSplitInfoBlobId().Get());

    for (int i = 1; ; ++i) {
        auto blob_id = blob_info->GetChunkBlobId(i).Get();
        if (blob_id.empty()) return;
        if (i == 1) CJson_Array ar = m_JsonObj.insert_array("chunk_blob_id");
        m_JsonObj["chunk_blob_id"].SetArray().push_back(blob_id);
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    m_JsonObj["reply"].SetValue().SetString("BioseqInfo");

    const auto included_info = bioseq_info->IncludedInfo();

    if (included_info & CPSG_Request_Resolve::fCanonicalId)  m_JsonObj["canonical_id"].SetValue().SetString(bioseq_info->GetCanonicalId().Get());

    if (included_info & CPSG_Request_Resolve::fOtherIds) {
        CJson_Array ar = m_JsonObj.insert_array("other_ids");

        for (const auto& bio_id : bioseq_info->GetOtherIds()) {
            m_JsonObj["other_ids"].SetArray().push_back(bio_id.Get());
        }
    }

    if (included_info & CPSG_Request_Resolve::fMoleculeType) m_JsonObj["molecule_type"].SetValue().SetString(objects::CSeq_inst::ENUM_METHOD_NAME(EMol)()->FindName(bioseq_info->GetMoleculeType(), true));
    if (included_info & CPSG_Request_Resolve::fLength)       m_JsonObj["length"].SetValue().SetUint8(bioseq_info->GetLength());
    if (included_info & CPSG_Request_Resolve::fState)        m_JsonObj["state"].SetValue().SetInt8(bioseq_info->GetState());
    if (included_info & CPSG_Request_Resolve::fBlobId)       m_JsonObj["blob_id"].SetValue().SetString(bioseq_info->GetBlobId().Get());
    if (included_info & CPSG_Request_Resolve::fTaxId)        m_JsonObj["tax_id"].SetValue().SetInt8(bioseq_info->GetTaxId());
    if (included_info & CPSG_Request_Resolve::fHash)         m_JsonObj["hash"].SetValue().SetInt8(bioseq_info->GetHash());
    if (included_info & CPSG_Request_Resolve::fDateChanged)  m_JsonObj["date_changed"].SetValue().SetString(bioseq_info->GetDateChanged().AsString());
}

void CJsonResponse::Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    m_JsonObj["reply"].SetValue().SetString("NamedAnnotInfo");
    m_JsonObj["canonical_id"].SetValue().SetString(named_annot_info->GetCanonicalId().Get());
    m_JsonObj["name"].SetValue().SetString(named_annot_info->GetName());

    const auto range = named_annot_info->GetRange();
    CJson_Object range_obj = m_JsonObj.insert_object("range");
    range_obj["from"].SetValue().SetInt8(range.GetFrom());
    range_obj["to"].SetValue().SetInt8(range.GetTo());

    m_JsonObj["blob_id"].SetValue().SetString(named_annot_info->GetBlobId().Get());
    m_JsonObj["version"].SetValue().SetUint8(named_annot_info->GetVersion());

    CJson_Array zoom_level_array = m_JsonObj.insert_array("zoom_levels");
    for (const auto zoom_level : named_annot_info->GetZoomLevels()) {
        zoom_level_array.push_back(zoom_level);
    }

    CJson_Array annot_info_array = m_JsonObj.insert_array("annot_info_list");
    for (const auto annot_info : named_annot_info->GetAnnotInfoList()) {
        CJson_Object annot_info_obj = annot_info_array.push_back_object();
        annot_info_obj["annot_type"].SetValue().SetInt8(annot_info.annot_type);
        annot_info_obj["feat_type"].SetValue().SetInt8(annot_info.feat_type);
        annot_info_obj["feat_subtype"].SetValue().SetInt8(annot_info.feat_subtype);
    }
}

template <class TItem>
void CJsonResponse::Fill(TItem item, string type)
{
    m_JsonObj["reply"].SetValue().SetString(type);

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) return;

        m_JsonObj.insert_array("errors").push_back(message);
    }
}

int CProcessing::OneRequest(shared_ptr<CPSG_Request> request)
{
    m_Queue.SendRequest(request, CDeadline::eInfinite);
    auto reply = m_Queue.GetNextReply(CDeadline::eInfinite);

    _ASSERT(reply);

    const CTimeout kTryTimeout(0.1);
    EPSG_Status status = EPSG_Status::eInProgress;
    bool end_of_reply = false;
    list<shared_ptr<CPSG_ReplyItem>> reply_items;

    while ((status == EPSG_Status::eInProgress) || !end_of_reply || !reply_items.empty()) {
        if (status == EPSG_Status::eInProgress) {
            status = reply->GetStatus(kTryTimeout);

            switch (status) {
                case EPSG_Status::eSuccess:    continue;
                case EPSG_Status::eInProgress: continue;
                default: m_JsonOut << CJsonResponse(status, reply);
            }
        }

        if (!end_of_reply) {
            if (auto reply_item = reply->GetNextItem(kTryTimeout)) {
                if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                    end_of_reply = true;
                } else {
                    reply_items.emplace_back(move(reply_item));
                }
            }
        }

        for (auto it = reply_items.begin(); it != reply_items.end();) {
            auto reply_item = *it;
            auto reply_item_status = reply_item->GetStatus(kTryTimeout);

            if (reply_item_status != EPSG_Status::eInProgress) {
                it = reply_items.erase(it);
                m_JsonOut << CJsonResponse(reply_item_status, reply_item);
            } else {
                ++it;
            }
        }
    }

    return 0;
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_ReplyItem>& item)
{
    return item->GetReply();
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_Reply>& reply)
{
    return reply;
}

void CSender::Run()
{
    const CTimeout kTryTimeout(CTimeout::eInfinite);

    for (;;) {
        unique_lock<mutex> lock(m_Mutex);
        m_CV.wait(lock, [&]() { return m_Stop || !m_Requests.empty(); });

        if (m_Requests.empty()) return;

        auto request = m_Requests.front();
        auto id = request->GetUserContext<string>();
        _ASSERT(id);
        m_Requests.pop();
        lock.unlock();

        if (m_Queue.SendRequest(request, kTryTimeout)) {
            m_JsonOut << CJsonResponse(*id, true);
        } else {
            m_JsonOut << CJsonResponse(*id, -32000, "Timeout on sending a request");
        }
    }
}

void CRetriever::Run()
{
    const CTimeout kTryTimeout(0.1);
    const CTimeout kZeroTimeout(CTimeout::eZero);

    while (!ShouldStop() || !m_Queue.IsEmpty()) {
        if (auto reply = m_Queue.GetNextReply(kTryTimeout)) {
            m_Replies.emplace_back(move(reply));
        }

        for (auto it = m_Replies.begin(); it != m_Replies.end();) {
            auto reply = *it;

            for (;;) {
                auto item = reply->GetNextItem(kZeroTimeout);
                
                if (!item) {
                    ++it;
                    break;
                }

                if (item->GetType() != CPSG_ReplyItem::eEndOfReply) {
                    m_Items.emplace_back(move(item));

                } else if (reply->GetStatus(kZeroTimeout) != EPSG_Status::eInProgress) {
                    it = m_Replies.erase(it);
                    m_Reporter.Add(reply);
                    break;
                }
            }
        }

        for (auto it = m_Items.begin(); it != m_Items.end();) {
            auto item = *it;
            auto status = item->GetStatus(kZeroTimeout);

            if (status != EPSG_Status::eInProgress) {
                it = m_Items.erase(it);
                m_Reporter.Add(item);
            } else {
                ++it;
            }
        }
    }
}

void CReporter::Run()
{
    const CTimeout kTryTimeout(CTimeout::eZero);
    auto pred = [&]() { return m_Stop || (!m_Requests.empty() && (!m_Replies.empty() || !m_Items.empty())); };

    for (;;) {
        unique_lock<mutex> lock(m_Mutex);
        m_CV.wait(lock, pred);

        // If stop requested and no more requests for replies/items
        if (m_Requests.empty()) return;

        CJson_Document result_doc;

        if (!m_Replies.empty()) {
            auto reply = m_Replies.front();
            m_Replies.pop();

            auto status = reply->GetStatus(kTryTimeout);

            switch (status) {
                case EPSG_Status::eSuccess:    continue;
                case EPSG_Status::eInProgress: _TROUBLE; break;
                default: result_doc = CJsonResponse(status, reply);
            }

        } else if (!m_Items.empty()) {
            auto item = m_Items.front();
            m_Items.pop();

            auto status = item->GetStatus(kTryTimeout);
            _ASSERT(status != EPSG_Status::eInProgress);
            result_doc = CJsonResponse(status, item);

        } else {
            result_doc.ResetObject();
        }

        m_JsonOut << CJsonResponse(m_Requests.front(), result_doc);

        m_Requests.pop();
    }
}

int CProcessing::Interactive(bool echo)
{
    m_Reporter.Start();
    m_Retriever.Start();
    m_Sender.Start();

    ReadCommands(echo);

    m_Sender.Stop();
    m_Retriever.Stop();
    m_Reporter.Stop();
    return 0;
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

int CProcessing::Performance(size_t user_threads, bool raw_metrics, const string& service)
{
    SPostProcessing post_processing(raw_metrics);

    string request;
    vector<shared_ptr<CPSG_Request>> requests;
    cerr << "Preparing requests: ";

    // Read requests from cin
    while (ReadRequest(request)) {
        CJson_Document json_doc;

        if (!json_doc.ParseString(request)) {
            cerr << "Error in request '" << s_GetId(json_doc) << "': " << json_doc.GetReadError() << endl;
            return -1;
        } else if (!RequestSchema().Validate(json_doc)) {
            cerr << "Error in request '" << s_GetId(json_doc) << "': " << RequestSchema().GetValidationError() << endl;
            return -1;
        } else {
            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();
            auto user_context = make_shared<SMetrics>();

            if (auto request = CreateRequest(method, move(user_context), params.GetObject())) {
                requests.emplace_back(move(request));
                if (requests.size() % 2000 == 0) cerr << '.';
            }
        }
    }

    atomic_int start(user_threads);
    atomic_int to_submit(requests.size());
    auto wait = [&]() { while (start > 0) this_thread::sleep_for(chrono::nanoseconds(1)); };

    auto l = [&]() {
        shared_ptr<CPSG_Queue> queue;
        
        if (service.empty()) {
            queue = shared_ptr<CPSG_Queue>(shared_ptr<CPSG_Queue>(), &m_Queue);
        } else {
            queue = make_shared<CPSG_Queue>(service);
        }

        start--;
        wait();

        for (;;) {
            auto i = to_submit--;

            if (i <= 0) break;

            // Submit
            {
                auto& request = requests[requests.size() - i];
                auto metrics = request->GetUserContext<SMetrics>();

                metrics->Set(SMetricType::eStart);
                _VERIFY(queue->SendRequest(request, CDeadline::eInfinite));
                metrics->Set(SMetricType::eSubmit);
            }

            // Response
            auto reply = queue->GetNextReply(CDeadline::eInfinite);
            _ASSERT(reply);

            auto request = reply->GetRequest();
            auto metrics = request->GetUserContext<SMetrics>();

            metrics->Set(SMetricType::eReply);
            bool success = reply->GetStatus(CDeadline::eInfinite) == EPSG_Status::eSuccess;
            metrics->Set(SMetricType::eDone);

            while (success) {
                auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
                _ASSERT(reply_item);

                if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

                metrics->NewItem();
                success = reply_item->GetStatus(CDeadline::eInfinite) == EPSG_Status::eSuccess;
            }

            if (success) metrics->SetSuccess();
        }
    };

    vector<thread> threads;
    threads.reserve(user_threads);

    // Start threads in advance so it won't affect metrics
    for (size_t i = 0; i < user_threads; ++i) {
        threads.emplace_back(l);
    }

    wait();

    // Start processing replies
    cerr << "\nSubmitting requests: ";
    int previous = requests.size() / 2000;

    while (to_submit > 0) {
        int current = to_submit / 2000;

        if (current < previous) {
            cerr << '.';
            previous = current;
        }
    }

    cerr << "\nWaiting for threads: " << user_threads << '\n';

    for (auto& t : threads) {
        t.join();
    }

    // Output metrics
    cerr << "Outputting metrics: ";
    size_t output = 0;

    for (auto& request : requests) {
        auto metrics = request->GetUserContext<SMetrics>();
        cout << *metrics;
        if (++output % 2000 == 0) cerr << '.';
    }

    cerr << '\n';
    return 0;
}

bool CProcessing::ReadRequest(string& request)
{
    for (;;) {
        if (!getline(cin, request)) {
            return false;
        } else if (!request.empty()) {
            return true;
        }
    }
}

void CProcessing::ReadCommands(bool echo)
{
    string request;

    while (ReadRequest(request)) {
        CJson_Document json_doc;

        if (!json_doc.ParseString(request)) {
            m_JsonOut << CJsonResponse(s_GetId(json_doc), -32700, json_doc.GetReadError());
        } else if (!RequestSchema().Validate(json_doc)) {
            m_JsonOut << CJsonResponse(s_GetId(json_doc), -32600, RequestSchema().GetValidationError());
        } else {
            if (echo) m_JsonOut << json_doc;

            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto id = json_obj["id"].GetValue().GetString();
            auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();

            if (method == "next_reply") {
                m_Reporter.Add(id);
                continue;

            } else if (method == "status") {
                m_JsonOut << CJsonResponse(id, m_RequestsCounter);
                continue;
            }

            auto user_context = make_shared<SUserContext>(id, m_RequestsCounter);

            if (auto request = CreateRequest(method, move(user_context), params.GetObject())) {
                m_Sender.Add(move(request));
            }
        }
    }
}

shared_ptr<CPSG_Request> CProcessing::CreateRequest(const string& method, shared_ptr<void> user_context,
        const CJson_ConstObject& params_obj)
{
    if (method == "biodata") {
        return CreateRequest<CPSG_Request_Biodata>(move(user_context), params_obj);
    } else if (method == "blob") {
        return CreateRequest<CPSG_Request_Blob>(move(user_context), params_obj);
    } else if (method == "resolve") {
        return CreateRequest<CPSG_Request_Resolve>(move(user_context), params_obj);
    } else if (method == "annot") {
        return CreateRequest<CPSG_Request_NamedAnnotInfo>(move(user_context), params_obj);
    } else {
        return {};
    }
}

CPSG_BioId::TType CProcessing::GetBioIdType(string type)
{
    CObjectTypeInfo info(objects::CSeq_id::GetTypeInfo());

    if (auto index = info.FindVariantIndex(type)) return static_cast<CPSG_BioId::TType>(index);
    if (auto value = objects::CSeq_id::WhichInverseSeqId(type)) return value;

    return static_cast<CPSG_BioId::TType>(atoi(type.c_str()));
}

const initializer_list<SDataFlag> kDataFlags =
{
    { "no-tse",    "Return only the info",                                  CPSG_Request_Biodata::eNoTSE    },
    { "slim-tse",  "Return split info blob if available, or nothing",       CPSG_Request_Biodata::eSlimTSE  },
    { "smart-tse", "Return split info blob if available, or original blob", CPSG_Request_Biodata::eSmartTSE },
    { "whole-tse", "Return all split blobs if available, or original blob", CPSG_Request_Biodata::eWholeTSE },
    { "orig-tse",  "Return original blob",                                  CPSG_Request_Biodata::eOrigTSE  },
};

const initializer_list<SDataFlag>& CProcessing::GetDataFlags()
{
    return kDataFlags;
}

const initializer_list<SInfoFlag> kInfoFlags =
{
    { "all-info-except", "Return all info except explicitly specified", CPSG_Request_Resolve::fAllInfo      },
    { "canonical-id",    "Return canonical ID info",                    CPSG_Request_Resolve::fCanonicalId  },
    { "other-ids",       "Return other IDs info",                       CPSG_Request_Resolve::fOtherIds     },
    { "molecule-type",   "Return molecule type info",                   CPSG_Request_Resolve::fMoleculeType },
    { "length",          "Return length info",                          CPSG_Request_Resolve::fLength       },
    { "state",           "Return state info",                           CPSG_Request_Resolve::fState        },
    { "blob-id",         "Return blob ID info",                         CPSG_Request_Resolve::fBlobId       },
    { "tax-id",          "Return tax ID info",                          CPSG_Request_Resolve::fTaxId        },
    { "hash",            "Return hash info",                            CPSG_Request_Resolve::fHash         },
    { "date-changed",    "Return date changed info",                    CPSG_Request_Resolve::fDateChanged  },
};

const initializer_list<SInfoFlag>& CProcessing::GetInfoFlags()
{
    return kInfoFlags;
}

CJson_Schema& CProcessing::RequestSchema()
{
    static CJson_Schema json_schema(CJson_Document(R"REQUEST_SCHEMA(
{
    "$schema": "http://json-schema.org/schema#",
    "type": "object",
    "definitions": {
        "jsonrpc": {
            "$id": "#jsonrpc",
            "enum": [ "2.0" ]
        },
        "id": {
            "$id": "#id",
            "type": "string"
        },
        "bio_id": {
            "$id": "#bio_id",
            "type": "array",
            "items": {
                "type": "string"
            },
            "minItems": 1,
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
                    "other-ids",
                    "molecule-type",
                    "length",
                    "state",
                    "blob-id",
                    "tax-id",
                    "hash",
                    "date-changed"
                ]
            },
            "uniqueItems": true
        }
    },
    "oneOf": [
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "biodata" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#bio_id" },
                        "include_data": { "$ref": "#include_data" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "blob" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "blob_id": { "type": "string" },
                        "last_modified": { "type": "string" },
                        "include_data": { "$ref": "#include_data" }
                    },
                    "required": [ "blob_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "resolve" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#bio_id" },
                        "include_info": { "$ref": "#include_info" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "next_reply" ] },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "status" ] },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "id" ]
        }
    ]
}
)REQUEST_SCHEMA"));
    return json_schema;
}

END_NCBI_SCOPE
