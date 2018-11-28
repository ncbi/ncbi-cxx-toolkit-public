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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

#include <serial/enumvalues.hpp>

#include "processing.hpp"

BEGIN_NCBI_SCOPE

template <class TItem>
CJson_Document CProcessing::ReportErrors(shared_ptr<TItem> item, string type)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString(type);

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) break;

        json_obj.insert_array("errors").push_back(message);
    }

    return json_doc;
}

void CProcessing::SetInclude(shared_ptr<CPSG_Request_Resolve> request, function<bool(const string&)> specified)
{
    const auto& info_flags = CProcessing::GetInfoFlags();

    auto i = info_flags.begin();
    bool all_info_except = specified(i->name);
    unsigned include_info = all_info_except ? CPSG_Request_Resolve::fAllInfo : 0u;

    for (++i; i != info_flags.end(); ++i) {
        if (specified(i->name)) {
            if (all_info_except) {
                include_info &= ~i->value;
            } else {
                include_info |= i->value;
            }
        }
    }

    request->IncludeInfo(include_info);
}

CPSG_BioId s_GetBioId(const CJson_ConstObject& json_obj)
{
    auto array = json_obj["bio_id"].GetArray();
    auto id = array[0].GetValue().GetString();
    auto type = CProcessing::GetBioIdType(array.size() > 1 ? array[1].GetValue().GetString() : "");
    return CPSG_BioId(id, type);
}

template <class TRequest>
void s_SetIncludeData(TRequest& request, const CJson_ConstObject& json_obj)
{
    auto specified = [&](const string& name) {
        return json_obj.has("include_data") && (name == json_obj["include_data"].GetValue().GetString());
    };

    CProcessing::SetInclude(request, specified);
}

template <>
void CProcessing::AddRequest<CPSG_Request_Biodata>(const string& id, const CJson_ConstNode& params)
{
    CJson_ConstObject json_obj(params.GetObject());
    auto bio_id = s_GetBioId(json_obj);

    auto user_context = make_shared<string>(id);
    auto request = make_shared<CPSG_Request_Biodata>(bio_id, move(user_context));
    s_SetIncludeData(request, json_obj);

    m_Requests.emplace(move(request));
}

template <>
void CProcessing::AddRequest<CPSG_Request_Blob>(const string& id, const CJson_ConstNode& params)
{
    CJson_ConstObject json_obj(params.GetObject());
    auto blob_id = json_obj["blob_id"].GetValue().GetString();
    auto last_modified = json_obj.has("last_modified") ? json_obj["last_modified"].GetValue().GetString() : "";

    auto user_context = make_shared<string>(id);
    auto request = make_shared<CPSG_Request_Blob>(blob_id, last_modified, move(user_context));
    s_SetIncludeData(request, json_obj);

    m_Requests.emplace(move(request));
}

template <>
void CProcessing::AddRequest<CPSG_Request_Resolve>(const string& id, const CJson_ConstNode& params)
{
    CJson_ConstObject json_obj(params.GetObject());
    auto bio_id = s_GetBioId(json_obj);

    auto user_context = make_shared<string>(id);
    auto request = make_shared<CPSG_Request_Resolve>(bio_id, move(user_context));

    auto specified = [&](const string& name) {
        if (!json_obj.has("include_info")) return false;

        auto include_info = json_obj["include_info"].GetArray();
        auto equals_to = [&](const CJson_ConstNode& node) { return node.GetValue().GetString() == name; };
        return find_if(include_info.begin(), include_info.end(), equals_to) != include_info.end();
    };

    SetInclude(request, specified);
    m_Requests.emplace(move(request));
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

void SJsonOut::operator()(const CJson_Document& doc)
{
    ostringstream os;

    if (!m_Printed.exchange(true)) {
        os << "[\n";
    }

    if (m_Interactive) {
        os << doc << ",\n";
    } else {
        os << ",\n" << doc;
    }

    cout << os.str();
    cout.flush();
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
                case EPSG_Status::eNotFound:   m_JsonOut(Report("NotFound")); break;
                case EPSG_Status::eCanceled:   m_JsonOut(Report("Canceled")); break;
                case EPSG_Status::eError:      m_JsonOut(ReportErrors(reply, "Failure")); break;
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

        for (auto it = reply_items.begin(); it != reply_items.end(); ++it) {
            auto reply_item = *it;
            auto reply_item_status = reply_item->GetStatus(kTryTimeout);

            if (reply_item_status != EPSG_Status::eInProgress) {
                it = reply_items.erase(it);
                m_JsonOut(Report(reply_item_status, reply_item));
            }
        }
    }

    return 0;
}

CJson_Document CProcessing::Report(string reply)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString(reply);
    return json_doc;
}

CJson_Document CProcessing::Report(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
{
    switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eBlobData:
            if (reply_item_status == EPSG_Status::eError) {
                return ReportErrors(reply_item, "BlobData");
            }

            return Report(static_pointer_cast<CPSG_BlobData>(reply_item));

        case CPSG_ReplyItem::eBlobInfo:
            if (reply_item_status == EPSG_Status::eError) {
                return ReportErrors(reply_item, "BlobInfo");
            }

            return Report(static_pointer_cast<CPSG_BlobInfo>(reply_item));

        case CPSG_ReplyItem::eBioseqInfo:
            if (reply_item_status == EPSG_Status::eError) {
                return ReportErrors(reply_item, "BioseqInfo");
            }

            return Report(static_pointer_cast<CPSG_BioseqInfo>(reply_item));

        case CPSG_ReplyItem::eEndOfReply:
            _ASSERT(false);
            break;
    }

    return CJson_Document();
}

CJson_Document CProcessing::Report(shared_ptr<CPSG_BlobData> blob_data)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString("BlobData");
    json_obj["id"].SetValue().SetString(blob_data->GetId().Get());
    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    json_obj["data"].SetValue().SetString(NStr::JsonEncode(os.str()));
    return json_doc;
}

CJson_Document CProcessing::Report(shared_ptr<CPSG_BlobInfo> blob_info)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString("BlobInfo");
    json_obj["id"].SetValue().SetString(blob_info->GetId().Get());
    json_obj["compression"].SetValue().SetString(blob_info->GetCompression());
    json_obj["format"].SetValue().SetString(blob_info->GetFormat());
    json_obj["version"].SetValue().SetUint8(blob_info->GetVersion());
    json_obj["storage_size"].SetValue().SetUint8(blob_info->GetStorageSize());
    json_obj["size"].SetValue().SetUint8(blob_info->GetSize());
    json_obj["is_dead"].SetValue().SetBool(blob_info->IsDead());
    json_obj["is_suppressed"].SetValue().SetBool(blob_info->IsSuppressed());
    json_obj["is_withdrawn"].SetValue().SetBool(blob_info->IsWithdrawn());
    json_obj["hup_release_date"].SetValue().SetString(blob_info->GetHupReleaseDate().AsString());
    json_obj["owner"].SetValue().SetUint8(blob_info->GetOwner());
    json_obj["original_load_date"].SetValue().SetString(blob_info->GetOriginalLoadDate().AsString());
    json_obj["class"].SetValue().SetString(objects::CBioseq_set::ENUM_METHOD_NAME(EClass)()->FindName(blob_info->GetClass(), true));
    json_obj["division"].SetValue().SetString(blob_info->GetDivision());
    json_obj["username"].SetValue().SetString(blob_info->GetUsername());
    json_obj["split_info_blob_id"].SetValue().SetString(blob_info->GetSplitInfoBlobId().Get());

    for (int i = 1; ; ++i) {
        auto blob_id = blob_info->GetChunkBlobId(i).Get();
        if (blob_id.empty()) break;
        if (i == 1) CJson_Array ar = json_obj.insert_array("chunk_blob_id");
        json_obj["chunk_blob_id"].SetArray().push_back(blob_id);
    }

    return json_doc;
}

CJson_Document CProcessing::Report(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString("BioseqInfo");

    const auto included_info = bioseq_info->IncludedInfo();

    if (included_info & CPSG_Request_Resolve::fCanonicalId)  json_obj["canonical_id"].SetValue().SetString(bioseq_info->GetCanonicalId().Get());

    if (included_info & CPSG_Request_Resolve::fOtherIds) {
        CJson_Array ar = json_obj.insert_array("other_ids");

        for (const auto& bio_id : bioseq_info->GetOtherIds()) {
            json_obj["other_ids"].SetArray().push_back(bio_id.Get());
        }
    }

    if (included_info & CPSG_Request_Resolve::fMoleculeType) json_obj["molecule_type"].SetValue().SetString(objects::CSeq_inst::ENUM_METHOD_NAME(EMol)()->FindName(bioseq_info->GetMoleculeType(), true));
    if (included_info & CPSG_Request_Resolve::fLength)       json_obj["length"].SetValue().SetUint8(bioseq_info->GetLength());
    if (included_info & CPSG_Request_Resolve::fState)        json_obj["state"].SetValue().SetInt8(bioseq_info->GetState());
    if (included_info & CPSG_Request_Resolve::fBlobId)       json_obj["blob_id"].SetValue().SetString(bioseq_info->GetBlobId().Get());
    if (included_info & CPSG_Request_Resolve::fTaxId)        json_obj["tax_id"].SetValue().SetInt8(bioseq_info->GetTaxId());
    if (included_info & CPSG_Request_Resolve::fHash)         json_obj["hash"].SetValue().SetInt8(bioseq_info->GetHash());
    if (included_info & CPSG_Request_Resolve::fDateChanged)  json_obj["date_changed"].SetValue().SetString(bioseq_info->GetDateChanged().AsString());

    return json_doc;
}

void CProcessing::SendRequests(CDeadline deadline)
{
    while (!m_Requests.empty()) {
        auto request = m_Requests.front();

        if (m_Queue.SendRequest(request, deadline)) {
            auto id = request->GetUserContext<string>();

            CJson_Document json_doc;
            CJson_Object json_obj(json_doc.SetObject());
            json_obj["jsonrpc"].SetValue().SetString("2.0");
            json_obj["result"].SetValue().SetBool(true);
            json_obj["id"].SetValue().SetString(*id);

            m_JsonOut(json_doc);
            m_Requests.pop();
        } else {
            return;
        }
    }
}

void CProcessing::GetReplies(CDeadline deadline)
{
    if (m_RepliesRequested.empty()) {
        MoveReplies(deadline);
        return;
    }

    do {
        if (m_Requests.empty() && m_Queue.IsEmpty() && m_Replies.empty() && m_ReplyItems.empty()) {
            while (!m_RepliesRequested.empty()) {
                CJson_Document json_doc;
                CJson_Object json_obj(json_doc.SetObject());
                json_obj["jsonrpc"].SetValue().SetString("2.0");
                json_obj["result"].ResetObject();
                json_obj["id"].SetValue().SetString(m_RepliesRequested.front());
                m_JsonOut(json_doc);

                m_RepliesRequested.pop(); 
            }
        }

        for (auto it = m_Replies.begin(); it != m_Replies.end(); ++it) {
            auto& reply = *it;

            CJson_Document json_doc;
            CJson_Object json_obj(json_doc.SetObject());
            json_obj["jsonrpc"].SetValue().SetString("2.0");

            auto status = reply->GetStatus(CTimeout(CTimeout::eZero));

            switch (status) {
                case EPSG_Status::eSuccess:
                    if (auto item = reply->GetNextItem(CTimeout(CTimeout::eZero))) {
                        if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                            it = m_Replies.erase(it);
                        } else {
                            m_ReplyItems.emplace_back(move(item));
                        }
                    }

                    continue;

                case EPSG_Status::eInProgress: continue;
                case EPSG_Status::eNotFound:   json_obj["result"].AssignCopy(Report("NotFound"));             break;
                case EPSG_Status::eCanceled:   json_obj["result"].AssignCopy(Report("Canceled"));             break;
                case EPSG_Status::eError:      json_obj["result"].AssignCopy(ReportErrors(reply, "Failure")); break;
            }

            auto request_id = reply->GetRequest()->GetUserContext<string>();
            json_obj["result"].SetObject()["request_id"].SetValue().SetString(*request_id);
            json_obj["id"].SetValue().SetString(m_RepliesRequested.front());
            m_JsonOut(json_doc);

            m_RepliesRequested.pop(); 

            if (m_RepliesRequested.empty()) return;
        }

        for (auto it = m_ReplyItems.begin(); it != m_ReplyItems.end(); ++it) {
            auto reply_item = *it;
            auto reply_item_status = reply_item->GetStatus(CTimeout(CTimeout::eZero));

            if (reply_item_status != EPSG_Status::eInProgress) {
                it = m_ReplyItems.erase(it);

                CJson_Document json_doc;
                CJson_Object json_obj(json_doc.SetObject());
                json_obj["jsonrpc"].SetValue().SetString("2.0");
                json_obj["result"].AssignCopy(Report(reply_item_status, reply_item));
                auto request_id = reply_item->GetReply()->GetRequest()->GetUserContext<string>();
                json_obj["result"].SetObject()["request_id"].SetValue().SetString(*request_id);
                json_obj["id"].SetValue().SetString(m_RepliesRequested.front());
                m_JsonOut(json_doc);

                m_RepliesRequested.pop(); 
            }

            if (m_RepliesRequested.empty()) return;
        }

        MoveReplies(deadline);
    } while (!m_RepliesRequested.empty() && !deadline.IsExpired());
}

void CProcessing::MoveReplies(CDeadline deadline)
{

    while (auto reply = m_Queue.GetNextReply(deadline)) {
        m_Replies.emplace_back(move(reply));
    }

    for (auto& reply : m_Replies) {
        while (auto item = reply->GetNextItem(deadline)) {
            if (item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

            m_ReplyItems.emplace_back(move(item));
        }
    }
}

int CProcessing::Interactive(bool echo)
{
    mutex m;
    condition_variable cv;
    queue<string> commands;

    // Asynchronous reading of stdin
    thread reader([&]() {
        bool read = true;
        string line;

        while (read) {
            read = getline(cin, line);

            if (!read) {
                line.clear();
            } else if (line.empty()) {
                continue;
            }

            unique_lock<mutex> lock(m);
            commands.push(line);
            cv.notify_one();
        }
    });

    bool read = true;
    const auto kTrySeconds = 0.1;
    const CTimeout kTryTimeout(kTrySeconds);

    while (read || !m_Requests.empty() || !m_Queue.IsEmpty()) {
        CDeadline deadline(kTryTimeout);

        if (read) {
            unique_lock<mutex> lock(m);
            cv.wait_for(lock, chrono::duration<double>(kTrySeconds), [&]() { return !commands.empty(); });

            while (!commands.empty()) {
                string line = commands.front();
                commands.pop();

                if (line.empty()) {
                    read = false;
                } else {
                    AddRequest(move(line), echo);
                }
            }
        }

        SendRequests(deadline);
        GetReplies(deadline);
    }

    reader.join();
    return 0;
}

extern const string kRequestSchema;

void CProcessing::AddRequest(string request, bool echo)
{
    CJson_Document json_schema_doc(kRequestSchema);
    CJson_Schema json_schema(json_schema_doc);
    CJson_Document json_doc;

    if (!json_doc.ParseString(request)) {
        Error(-32700, json_doc.GetReadError(), json_doc);
    } else if (!json_schema.Validate(json_doc)) {
        Error(-32600, json_schema.GetValidationError(), json_doc);
    } else {
        if (echo) m_JsonOut(json_doc);

        CJson_ConstObject json_obj(json_doc.GetObject());
        auto method = json_obj["method"].GetValue().GetString();
        auto id = json_obj["id"].GetValue().GetString();
        auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();

        if (method == "biodata") {
            AddRequest<CPSG_Request_Biodata>(id, params);
        } else if (method == "blob") {
            AddRequest<CPSG_Request_Blob>(id, params);
        } else if (method == "resolve") {
            AddRequest<CPSG_Request_Resolve>(id, params);
        } else if (method == "next_reply") {
            m_RepliesRequested.push(id);
        }
    }
}

CPSG_BioId::TType CProcessing::GetBioIdType(string type)
{
    CObjectTypeInfo info(objects::CSeq_id::GetTypeInfo());
    auto index = info.FindVariantIndex(type);
    return index ? static_cast<CPSG_BioId::TType>(index) : objects::CSeq_id::WhichInverseSeqId(type);
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

void CProcessing::Error(int code, string message, const CJson_Document& req_doc)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["jsonrpc"].SetValue().SetString("2.0");

    CJson_Object error_obj = json_obj.insert_object("error");
    error_obj["code"].SetValue().SetInt4(code);
    error_obj["message"].SetValue().SetString(message);

    json_obj["id"].SetValue().SetNull();

    if (req_doc.IsObject()) {
        auto req_obj = req_doc.GetObject();

        if (req_obj.has("id")) {
            auto id_node = req_obj["id"];

            if (id_node.IsValue()) {
                auto id_value = id_node.GetValue();

                if (id_value.IsString()) {
                    json_obj["id"].SetValue().SetString(id_value.GetString());
                }
            }
        }
    }

    m_JsonOut(json_doc);
}

const string kRequestSchema = R"REQUEST_SCHEMA(
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
        }
    ]
}
)REQUEST_SCHEMA";
