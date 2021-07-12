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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_utils.hpp"

USING_NCBI_SCOPE;


static const string     kAccession = "accession";
static const string     kVersion = "version";
static const string     kSeqIdType = "seq_id_type";
static const string     kGi = "gi";
static const string     kMol = "mol";
static const string     kLength = "length";
static const string     kSeqState = "seq_state";
static const string     kState = "state";
static const string     kSat = "sat";
static const string     kSatKey = "sat_key";
static const string     kTaxId = "tax_id";
static const string     kHash = "hash";
static const string     kDateChanged = "date_changed";
static const string     kSeqIds = "seq_ids";
static const string     kName = "name";
static const string     kBlobId = "blob_id";

static const string     kKey = "key";
static const string     kLastModified = "last_modified";
static const string     kFlags = "flags";
static const string     kSize = "size";
static const string     kSizeUnpacked = "size_unpacked";
static const string     kClass = "class";
static const string     kDateAsn1 = "date_asn1";
static const string     kHupDate = "hup_date";
static const string     kDiv = "div";
static const string     kId2Info = "id2_info";
static const string     kOwner = "owner";
static const string     kUserName = "username";
static const string     kNChunks = "n_chunks";
static const string     kStart = "start";
static const string     kStop = "stop";
static const string     kAnnotInfo = "annot_info";
static const string     kSeqAnnotInfo = "seq_annot_info";

static const string     kRequest = "request";
static const string     kSecSeqId = "sec_seq_id";
static const string     kSecSeqIdType = "sec_seq_id_type";
static const string     kSecSeqState = "sec_seq_state";
static const string     kSatName = "sat_name";
static const string     kSplitVersion = "split_version";
static const string     kBlopPropProvided = "blob_prop_provided";
static const string     kChunksRequested = "chunk_requested";
static const string     kAnnotNames = "annotation_names";

static const string     kDate = "date";
static const string     kChain = "chain";


string ToBioseqProtobuf(const CBioseqInfoRecord &  bioseq_info)
{
    // Used to prepare a binary content out of the structured data
    // BioseqInfoReply needs to be sent back
    psg::retrieval::BioseqInfoReply     protobuf_bioseq_info_reply;

    // Reply status
    psg::retrieval::ReplyStatus *       protobuf_bioseq_info_reply_status =
        protobuf_bioseq_info_reply.mutable_status();
    protobuf_bioseq_info_reply_status->set_status_code(psg::retrieval::STATUS_SUCCESS);

    // Reply key
    psg::retrieval::BioseqInfoKey *     protobuf_bioseq_info_reply_key =
        protobuf_bioseq_info_reply.mutable_key();
    protobuf_bioseq_info_reply_key->set_accession(bioseq_info.GetAccession());
    protobuf_bioseq_info_reply_key->set_version(bioseq_info.GetVersion());
    protobuf_bioseq_info_reply_key->set_seq_id_type(
            static_cast<psg::retrieval::EnumSeqIdType>(bioseq_info.GetSeqIdType()));
    protobuf_bioseq_info_reply_key->set_gi(bioseq_info.GetGI());

    // Reply value
    psg::retrieval::BioseqInfoValue *   protobuf_bioseq_info_value =
        protobuf_bioseq_info_reply.mutable_value();

    psg::retrieval::BlobPropKey *       protobuf_blob_prop_key =
        protobuf_bioseq_info_value->mutable_blob_key();
    protobuf_blob_prop_key->set_sat(bioseq_info.GetSat());
    protobuf_blob_prop_key->set_sat_key(bioseq_info.GetSatKey());

    protobuf_bioseq_info_value->set_seq_state(
            static_cast<psg::retrieval::EnumSeqState>(bioseq_info.GetSeqState()));
    protobuf_bioseq_info_value->set_state(
            static_cast<psg::retrieval::EnumSeqState>(bioseq_info.GetState()));
    protobuf_bioseq_info_value->set_mol(
            static_cast<psg::retrieval::EnumSeqMolType>(bioseq_info.GetMol()));
    protobuf_bioseq_info_value->set_hash(bioseq_info.GetHash());
    protobuf_bioseq_info_value->set_length(bioseq_info.GetLength());
    protobuf_bioseq_info_value->set_date_changed(bioseq_info.GetDateChanged());
    protobuf_bioseq_info_value->set_tax_id(bioseq_info.GetTaxId());

    for (const auto &  item : bioseq_info.GetSeqIds()) {
        psg::retrieval::BioseqInfoValue_SecondaryId *   protobuf_secondary_id =
            protobuf_bioseq_info_value->add_seq_ids();
        protobuf_secondary_id->set_sec_seq_id_type(
            static_cast<psg::retrieval::EnumSeqIdType>(get<0>(item)));
        protobuf_secondary_id->set_sec_seq_id(get<1>(item));
    }

    protobuf_bioseq_info_value->set_name(bioseq_info.GetName());

    // Convert to binary
    string      bioseq_protobuf;
    protobuf_bioseq_info_reply.SerializeToString(&bioseq_protobuf);
    return bioseq_protobuf;
}


CJsonNode  ToJson(const CBioseqInfoRecord &  bioseq_info,
                  SPSGS_ResolveRequest::TPSGS_BioseqIncludeData  include_data_flags,
                  const string &  custom_blob_id)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_CanonicalId) {
        json.SetString(kAccession, bioseq_info.GetAccession());
        json.SetInteger(kVersion, bioseq_info.GetVersion());
        json.SetInteger(kSeqIdType, bioseq_info.GetSeqIdType());
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Name)
        json.SetString(kName, bioseq_info.GetName());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Gi)
        json.SetInteger(kGi, bioseq_info.GetGI());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_DateChanged)
        json.SetInteger(kDateChanged, bioseq_info.GetDateChanged());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Hash)
        json.SetInteger(kHash, bioseq_info.GetHash());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Length)
        json.SetInteger(kLength, bioseq_info.GetLength());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_MoleculeType)
        json.SetInteger(kMol, bioseq_info.GetMol());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_BlobId) {
        if (custom_blob_id.empty()) {
            json.SetInteger(kSat, bioseq_info.GetSat());
            json.SetInteger(kSatKey, bioseq_info.GetSatKey());

            // Adding blob_id as a string for future replacement of the separate
            // sat and sat_key
            json.SetString(kBlobId, to_string(bioseq_info.GetSat()) + "." +
                                    to_string(bioseq_info.GetSatKey()));
        } else {
            // If a custom blob_id is provided then there is no need to send
            // the one from bioseq info
            json.SetString(kBlobId, custom_blob_id);
        }
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqIds) {
        CJsonNode       seq_ids(CJsonNode::NewArrayNode());
        for (const auto &  item : bioseq_info.GetSeqIds()) {
            CJsonNode       item_tuple(CJsonNode::NewArrayNode());
            item_tuple.AppendInteger(get<0>(item));
            item_tuple.AppendString(get<1>(item));
            seq_ids.Append(item_tuple);
        }
        json.SetByKey(kSeqIds, seq_ids);
    }

    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqState)
        json.SetInteger(kSeqState, bioseq_info.GetSeqState());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_State)
        json.SetInteger(kState, bioseq_info.GetState());
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_TaxId)
        json.SetInteger(kTaxId, bioseq_info.GetTaxId());

    return json;
}


CJsonNode  ToJson(const CBlobRecord &  blob_prop)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetInteger(kKey, blob_prop.GetKey());
    json.SetInteger(kLastModified, blob_prop.GetModified());
    json.SetInteger(kFlags, blob_prop.GetFlags());
    json.SetInteger(kSize, blob_prop.GetSize());
    json.SetInteger(kSizeUnpacked, blob_prop.GetSizeUnpacked());
    json.SetInteger(kClass, blob_prop.GetClass());
    json.SetInteger(kDateAsn1, blob_prop.GetDateAsn1());
    json.SetInteger(kHupDate, blob_prop.GetHupDate());
    json.SetString(kDiv, blob_prop.GetDiv());
    json.SetString(kId2Info, blob_prop.GetId2Info());
    json.SetInteger(kOwner, blob_prop.GetOwner());
    json.SetString(kUserName, blob_prop.GetUserName());
    json.SetInteger(kNChunks, blob_prop.GetNChunks());
    return json;
}


CJsonNode ToJson(const CNAnnotRecord &  annot_record,
                 int32_t  sat,
                 const string &  custom_blob_id)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetInteger(kLastModified, annot_record.GetModified());
    if (custom_blob_id.empty()) {
        json.SetInteger(kSat, sat);
        json.SetInteger(kSatKey, annot_record.GetSatKey());

        // Adding blob_id as a string for future replacement of the separate
        // sat and sat_key
        json.SetString(kBlobId, to_string(sat) + "." +
                                to_string(annot_record.GetSatKey()));
    } else {
        // If a custom blob_id is provided then there is no need to send
        // the one from bioseq info
        json.SetString(kBlobId, custom_blob_id);
    }

    json.SetString(kAccession, annot_record.GetAccession());
    json.SetInteger(kVersion, annot_record.GetVersion());
    json.SetInteger(kSeqIdType, annot_record.GetSeqIdType());
    json.SetInteger(kStart, annot_record.GetStart());
    json.SetInteger(kStop, annot_record.GetStop());
    json.SetString(kAnnotInfo, annot_record.GetAnnotInfo());
    json.SetString(kSeqAnnotInfo, NStr::Base64Encode(annot_record.GetSeqAnnotInfo(), 0));
    return json;
}

CJsonNode ToJson(const CBioseqInfoFetchRequest &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "BioseqInfo request");

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eAccession))
        json.SetString(kAccession, request.GetAccession());
    else
        json.SetNull(kAccession);

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eVersion))
        json.SetInteger(kVersion, request.GetVersion());
    else
        json.SetNull(kVersion);

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eSeqIdType))
        json.SetInteger(kSeqIdType, request.GetSeqIdType());
    else
        json.SetNull(kSeqIdType);

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eGI))
        json.SetInteger(kGi, request.GetGI());
    else
        json.SetNull(kGi);

    return json;
}


CJsonNode ToJson(const CSi2CsiFetchRequest &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Si2Csi request");

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqId))
        json.SetString(kSecSeqId, request.GetSecSeqId());
    else
        json.SetNull(kSecSeqId);

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqIdType))
        json.SetInteger(kSecSeqIdType, request.GetSecSeqIdType());
    else
        json.SetNull(kSecSeqIdType);

    return json;
}

CJsonNode ToJson(const CBlobFetchRequest &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Blob prop request");

    if (request.HasField(CBlobFetchRequest::EFields::eSat))
        json.SetInteger(kSat, request.GetSat());
    else
        json.SetNull(kSat);

    if (request.HasField(CBlobFetchRequest::EFields::eSatKey))
        json.SetInteger(kSatKey, request.GetSatKey());
    else
        json.SetNull(kSatKey);

    if (request.HasField(CBlobFetchRequest::EFields::eLastModified))
        json.SetInteger(kLastModified, request.GetLastModified());
    else
        json.SetNull(kLastModified);

    return json;
}


CJsonNode ToJson(const CSI2CSIRecord &  record)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kSecSeqId, record.GetSecSeqId());
    json.SetInteger(kSecSeqIdType, record.GetSecSeqIdType());
    json.SetString(kAccession, record.GetAccession());
    json.SetInteger(kGi, record.GetGI());
    json.SetInteger(kSecSeqState, record.GetSecSeqState());
    json.SetInteger(kSeqIdType, record.GetSeqIdType());
    json.SetInteger(kVersion, record.GetVersion());
    return json;
}


CJsonNode ToJson(const CCassBlobTaskLoadBlob &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Blob request");
    json.SetString(kSatName, request.GetKeyspace());
    json.SetInteger(kSatKey, request.GetSatKey());

    auto    last_modified = request.GetModified();
    if (last_modified == CCassBlobTaskLoadBlob::kAnyModified)
        json.SetNull(kLastModified);
    else
        json.SetInteger(kLastModified, last_modified);

    json.SetBoolean(kBlopPropProvided, request.BlobPropsProvided());
    json.SetBoolean(kChunksRequested, request.LoadChunks());
    return json;
}


CJsonNode ToJson(const CCassBlobTaskFetchSplitHistory &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Split history request");
    json.SetString(kSatName, request.GetKeyspace());
    json.SetInteger(kSatKey, request.GetKey());

    auto    split_version = request.GetSplitVersion();
    if (split_version == CCassBlobTaskFetchSplitHistory::kAllVersions)
        json.SetNull(kSplitVersion);
    else
        json.SetInteger(kSplitVersion, split_version);

    return json;
}


CJsonNode ToJson(const CCassNAnnotTaskFetch &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Named annotation request");
    json.SetString(kSatName, request.GetKeyspace());
    json.SetString(kAccession, request.GetAccession());
    json.SetInteger(kVersion, request.GetVersion());
    json.SetInteger(kSeqIdType, request.GetSeqIdType());

    CJsonNode       names(CJsonNode::NewArrayNode());
    for (const auto &  item : request.GetAnnotNames()) {
        names.AppendString(item);
    }
    json.SetByKey(kAnnotNames, names);
    return json;
}


CJsonNode ToJson(const CCassStatusHistoryTaskGetPublicComment &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Public comment request");
    json.SetString(kSatName, request.GetKeySpace());
    json.SetInteger(kSatKey, request.key());
    return json;
}


CJsonNode ToJson(const CCassAccVerHistoryTaskFetch &  request)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString(kRequest, "Accession version history request");
    json.SetString(kSatName, request.GetKeyspace());
    json.SetString(kAccession, request.GetAccession());
    json.SetInteger(kVersion, request.GetVersion());
    json.SetInteger(kSeqIdType, request.GetSeqIdType());

    return json;
}


CJsonNode ToJson(const SAccVerHistRec &  history_record)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetInteger(kGi, history_record.gi);
    json.SetString(kAccession, history_record.accession);
    json.SetInteger(kVersion, history_record.version);
    json.SetInteger(kSeqIdType, history_record.seq_id_type);
    json.SetInteger(kDate, history_record.date);
    json.SetInteger(kSatKey, history_record.sat_key);
    json.SetInteger(kSat, history_record.sat);
    json.SetInteger(kChain, history_record.chain);
    return json;
}

