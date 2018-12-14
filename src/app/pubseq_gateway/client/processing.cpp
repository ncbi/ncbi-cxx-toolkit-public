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

#include <queue>
#include <sstream>

#include <serial/enumvalues.hpp>

#include "processing.hpp"

BEGIN_NCBI_SCOPE

template <class TItem>
CJson_Document CReporter::ReportErrors(shared_ptr<TItem> item, string type)
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

SJsonOut& SJsonOut::operator<<(const CJson_Document& doc)
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
                case EPSG_Status::eNotFound:   m_JsonOut << CReporter::Report("NotFound"); break;
                case EPSG_Status::eCanceled:   m_JsonOut << CReporter::Report("Canceled"); break;
                case EPSG_Status::eError:      m_JsonOut << CReporter::ReportErrors(reply, "Failure"); break;
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
                m_JsonOut << CReporter::Report(reply_item_status, reply_item);
            }
        }
    }

    return 0;
}

CJson_Document CReporter::Report(string reply)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["reply"].SetValue().SetString(reply);
    return json_doc;
}

CJson_Document CReporter::Report(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
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

CJson_Document CReporter::Report(shared_ptr<CPSG_BlobData> blob_data)
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

CJson_Document CReporter::Report(shared_ptr<CPSG_BlobInfo> blob_info)
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

CJson_Document CReporter::Report(shared_ptr<CPSG_BioseqInfo> bioseq_info)
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

void CSender::Run()
{
    const CTimeout kTryTimeout(CTimeout::eInfinite);

    for (;;) {
        unique_lock<mutex> lock(m_Mutex);
        m_CV.wait(lock, [&]() { return m_Stop || !m_Requests.empty(); });

        if (m_Requests.empty()) return;

        auto request = m_Requests.front();
        auto id = request->GetUserContext<string>();
        m_Requests.pop();

        CJson_Document json_doc;

        if (m_Queue.SendRequest(request, kTryTimeout)) {
            CJson_Object json_obj(json_doc.SetObject());
            json_obj["jsonrpc"].SetValue().SetString("2.0");
            json_obj["result"].SetValue().SetBool(true);
            json_obj["id"].SetValue().SetString(*id);
        } else {
            json_doc = CReporter::ReportError(-32000, "Timeout on sending a request", *id);
        }

        m_JsonOut << json_doc;
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

        for (auto it = m_Replies.begin(); it != m_Replies.end(); ++it) {
            auto reply = *it;

            while (auto item = reply->GetNextItem(kZeroTimeout)) {
                if (item->GetType() != CPSG_ReplyItem::eEndOfReply) {
                    m_Items.emplace_back(move(item));

                } else if (reply->GetStatus(kZeroTimeout) != EPSG_Status::eInProgress) {
                    it = m_Replies.erase(it);
                    m_Reporter.Add(reply);
                    break;
                }
            }
        }

        for (auto it = m_Items.begin(); it != m_Items.end(); ++it) {
            auto item = *it;
            auto status = item->GetStatus(kZeroTimeout);

            if (status != EPSG_Status::eInProgress) {
                it = m_Items.erase(it);
                m_Reporter.Add(item);
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

        shared_ptr<CPSG_Reply> reply;
        CJson_Document result_doc;

        if (!m_Replies.empty()) {
            reply = m_Replies.front();
            m_Replies.pop();

            auto status = reply->GetStatus(kTryTimeout);

            switch (status) {
                case EPSG_Status::eSuccess:                                                 continue;
                case EPSG_Status::eInProgress: _TROUBLE;                                    break;
                case EPSG_Status::eNotFound:   result_doc = Report("NotFound");             break;
                case EPSG_Status::eCanceled:   result_doc = Report("Canceled");             break;
                case EPSG_Status::eError:      result_doc = ReportErrors(reply, "Failure"); break;
            }

        } else if (!m_Items.empty()) {
            auto item = m_Items.front();
            reply = item->GetReply();
            m_Items.pop();

            auto status = item->GetStatus(kTryTimeout);
            _ASSERT(status != EPSG_Status::eInProgress);
            result_doc = Report(status, item);

        } else {
            result_doc.ResetObject();
        }

        CJson_Document json_doc;
        CJson_Object json_obj(json_doc.SetObject());
        json_obj["jsonrpc"].SetValue().SetString("2.0");
        json_obj["result"].AssignCopy(result_doc);

        if (reply) {
            auto request_id = reply->GetRequest()->GetUserContext<string>();
            json_obj["result"].SetObject()["request_id"].SetValue().SetString(*request_id);
        }

        json_obj["id"].SetValue().SetString(m_Requests.front());
        m_JsonOut << json_doc;

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

void CProcessing::ReadCommands(bool echo)
{
    string request;

    for (;;) {
        if (!getline(cin, request)) {
            return;
        } else if (request.empty()) {
            continue;
        }

        CJson_Document json_doc;

        if (!json_doc.ParseString(request)) {
            m_JsonOut << CReporter::ReportError(-32700, json_doc.GetReadError(), json_doc);
        } else if (!RequestSchema().Validate(json_doc)) {
            m_JsonOut << CReporter::ReportError(-32600, RequestSchema().GetValidationError(), json_doc);
        } else {
            if (echo) m_JsonOut << json_doc;

            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto id = json_obj["id"].GetValue().GetString();
            auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();

            if (method == "next_reply") {
                m_Reporter.Add(id);
                continue;
            }

            auto user_context = make_shared<string>(id);
            shared_ptr<CPSG_Request> request;
            CJson_ConstObject params_obj(params.GetObject());

            if (method == "biodata") {
                request = CreateRequest<CPSG_Request_Biodata>(move(user_context), params_obj);
            } else if (method == "blob") {
                request = CreateRequest<CPSG_Request_Blob>(move(user_context), params_obj);
            } else if (method == "resolve") {
                request = CreateRequest<CPSG_Request_Resolve>(move(user_context), params_obj);
            }

            m_Sender.Add(move(request));
        }
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

CJson_Document CReporter::ReportError(int code, const string& message, const CJson_Document& req_doc)
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

    return ReportError(code, message, id);
}

CJson_Document CReporter::ReportError(int code, const string& message, const string& id)
{
    CJson_Document json_doc;
    CJson_Object json_obj(json_doc.SetObject());
    json_obj["jsonrpc"].SetValue().SetString("2.0");

    CJson_Object error_obj = json_obj.insert_object("error");
    error_obj["code"].SetValue().SetInt4(code);
    error_obj["message"].SetValue().SetString(message);

    auto id_value = json_obj["id"].SetValue();

    if (id.empty()) {
        id_value.SetNull();
    } else {
        id_value.SetString(id);
    }

    return json_doc;
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
        }
    ]
}
)REQUEST_SCHEMA"));
    return json_schema;
}

END_NCBI_SCOPE
