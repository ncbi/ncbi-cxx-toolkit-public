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

static const string     kAccessionItem = "\"accession\": ";
static const string     kVersionItem = "\"version\": ";
static const string     kSeqIdTypeItem = "\"seq_id_type\": ";
static const string     kNameItem = "\"name\": ";
static const string     kGiItem = "\"gi\": ";
static const string     kDateChangedItem = "\"date_changed\": ";
static const string     kHashItem = "\"hash\": ";
static const string     kLengthItem = "\"length\": ";
static const string     kMolItem = "\"mol\": ";
static const string     kSatItem = "\"sat\": ";
static const string     kSatKeyItem = "\"sat_key\": ";
static const string     kBlobIdItem = "\"blob_id\": ";
static const string     kSeqIdsItem = "\"seq_ids\": ";
static const string     kSeqStateItem = "\"seq_state\": ";
static const string     kStateItem = "\"state\": ";
static const string     kTaxIdItem = "\"tax_id\": ";
static const string     kSep = ", ";
static const string     kDictValueSep = ": ";
static const string     kKeyItem = "\"key\": ";
static const string     kLastModifiedItem = "\"last_modified\": ";
static const string     kFlagsItem = "\"flags\": ";
static const string     kSizeItem = "\"size\": ";
static const string     kSizeUnpackedItem = "\"size_unpacked\": ";
static const string     kClassItem = "\"class\": ";
static const string     kDateAsn1Item = "\"date_asn1\": ";
static const string     kHupDateItem = "\"hup_date\": ";
static const string     kDivItem = "\"div\": ";
static const string     kId2InfoItem = "\"id2_info\": ";
static const string     kOwnerItem = "\"owner\": ";
static const string     kUserNameItem = "\"username\": ";
static const string     kNChunksItem = "\"n_chunks\": ";
static const string     kStartItem = "\"start\": ";
static const string     kStopItem = "\"stop\": ";
static const string     kAnnotInfoItem = "\"annot_info\": ";
static const string     kSeqAnnotInfoItem = "\"seq_annot_info\": ";
static const string     kRequestItem = "\"request\": ";
static const string     kDatacenterItem = "\"datacenter\": ";
static const string     kSecSeqIdItem = "\"sec_seq_id\": ";
static const string     kSecSeqIdTypeItem = "\"sec_seq_id_type\": ";
static const string     kSecSeqStateItem = "\"sec_seq_state\": ";
static const string     kSatNameItem = "\"sat_name\": ";
static const string     kChunksRequestedItem = "\"chunk_requested\": ";
static const string     kBlopPropProvidedItem = "\"blob_prop_provided\": ";
static const string     kSplitVersionItem = "\"split_version\": ";
static const string     kAnnotNamesItem = "\"annotation_names\": ";
static const string     kDateItem = "\"date\": ";
static const string     kChainItem = "\"chain\": ";
static const string     kIPGItem = "\"ipg\": ";
static const string     kProteinItem = "\"protein\": ";
static const string     kNucleotideItem = "\"nucleotide\": ";
static const string     kProductNameItem = "\"product_name\": ";
static const string     kDivisionItem = "\"division\": ";
static const string     kAssemblyItem = "\"assembly\": ";
static const string     kStrainItem = "\"strain\": ";
static const string     kBioProjectItem = "\"bio_project\": ";
static const string     kGBStateItem = "\"gb_state\": ";



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


string ToJsonString(const CBioseqInfoRecord &  bioseq_info,
                    SPSGS_ResolveRequest::TPSGS_BioseqIncludeData  include_data_flags,
                    const string &  custom_blob_id)
{
    string              json;
    bool                some = false;

    char                buf[kPSGToStringBufferSize];

    // PIR and PRF seq id is moved from the 'accession' field to the 'name'
    // field so if canonical id is requested and the 'accession' field is empty
    // then include the 'name' field
    auto    seq_id_type = bioseq_info.GetSeqIdType();
    if (seq_id_type == CSeq_id::e_Prf || seq_id_type == CSeq_id::e_Pir) {
        if (include_data_flags & SPSGS_ResolveRequest::fPSGS_CanonicalId) {
            if (bioseq_info.GetAccession().empty()) {
                include_data_flags |= SPSGS_ResolveRequest::fPSGS_Name;
            }
        }
    }

    json.push_back('{');
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_CanonicalId) {
        json.append(kAccessionItem)
            .push_back('"');
        json.append(NStr::JsonEncode(bioseq_info.GetAccession()))
            .push_back('"');
        json.append(kSep)
            .append(kVersionItem)
            .append(buf, PSGToString(bioseq_info.GetVersion(), buf))
            .append(kSep)
            .append(kSeqIdTypeItem);

        json.append(buf, PSGToString(bioseq_info.GetSeqIdType(), buf));
        some = true;
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Name) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kNameItem)
            .push_back('"');
        json.append(NStr::JsonEncode(bioseq_info.GetName()))
            .push_back('"');
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Gi) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kGiItem)
            .append(buf, PSGToString(bioseq_info.GetGI(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_DateChanged) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kDateChangedItem)
            .append(buf, PSGToString(bioseq_info.GetDateChanged(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Hash) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kHashItem)
            .append(buf, PSGToString(bioseq_info.GetHash(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Length) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kLengthItem)
            .append(buf, PSGToString(bioseq_info.GetLength(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_MoleculeType) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kMolItem)
            .append(buf, PSGToString(bioseq_info.GetMol(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_BlobId) {
        if (some)   json.append(kSep);
        else        some = true;

        if (custom_blob_id.empty()) {
            json.append(kSatItem)
                .append(buf, PSGToString(bioseq_info.GetSat(), buf))
                .append(kSep)
                .append(kSatKeyItem);

            json.append(buf, PSGToString(bioseq_info.GetSatKey(), buf))
                .append(kSep);

            // Adding blob_id as a string for future replacement of the separate
            // sat and sat_key
            json.append(kBlobIdItem)
                .push_back('"');

            json.append(buf, PSGToString(bioseq_info.GetSat(), buf))
                .push_back('.');

            json.append(buf, PSGToString(bioseq_info.GetSatKey(), buf))
                .push_back('"');
        } else {
            // If a custom blob_id is provided then there is no need to send
            // the one from bioseq info
            json.append(kBlobIdItem)
                .push_back('"');
            json.append(custom_blob_id)
                .push_back('"');
        }
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqIds) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kSeqIdsItem)
            .push_back('[');

        bool    not_first = false;
        for (const auto &  item : bioseq_info.GetSeqIds()) {
            if (not_first)
                json.append(kSep);
            else
                not_first = true;

            json.push_back('[');
            json.append(buf, PSGToString(get<0>(item), buf))
                .append(kSep)
                .push_back('"');
            json.append(NStr::JsonEncode(get<1>(item)))
                .push_back('"');
            json.push_back(']');
        }
        json.push_back(']');
    }

    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqState) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kSeqStateItem)
            .append(buf, PSGToString(bioseq_info.GetSeqState(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_State) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kStateItem)
            .append(buf, PSGToString(bioseq_info.GetState(), buf));
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_TaxId) {
        if (some)   json.append(kSep);
        // else        some = true;     // suppress dead assignment warning

        json.append(kTaxIdItem)
            .append(buf, PSGToString(bioseq_info.GetTaxId(), buf));
    }

    json.push_back('}');
    return json;
}


string  ToJsonString(const CBlobRecord &  blob_prop)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kKeyItem)
        .append(buf, PSGToString(blob_prop.GetKey(), buf));
    json.append(kSep)
        .append(kLastModifiedItem)
        .append(buf, PSGToString(blob_prop.GetModified(), buf));
    json.append(kSep)
        .append(kFlagsItem)
        .append(buf, PSGToString(blob_prop.GetFlags(), buf));
    json.append(kSep)
        .append(kSizeItem)
        .append(buf, PSGToString(blob_prop.GetSize(), buf));
    json.append(kSep)
        .append(kSizeUnpackedItem)
        .append(buf, PSGToString(blob_prop.GetSizeUnpacked(), buf));
    json.append(kSep)
        .append(kClassItem)
        .append(buf, PSGToString(blob_prop.GetClass(), buf));
    json.append(kSep)
        .append(kDateAsn1Item)
        .append(buf, PSGToString(blob_prop.GetDateAsn1(), buf));
    json.append(kSep)
        .append(kHupDateItem)
        .append(buf, PSGToString(blob_prop.GetHupDate(), buf))

        .append(kSep)
        .append(kDivItem)
        .push_back('"');
    json.append(NStr::JsonEncode(blob_prop.GetDiv()))
        .push_back('"');

    json.append(kSep)
        .append(kId2InfoItem)
        .push_back('"');
    json.append(blob_prop.GetId2Info())     // Cannot have chars need to escape
        .push_back('"');

    json.append(kSep)
        .append(kOwnerItem)
        .append(buf, PSGToString(blob_prop.GetOwner(), buf))

        .append(kSep)
        .append(kUserNameItem)
        .push_back('"');
    json.append(NStr::JsonEncode(blob_prop.GetUserName()))
        .push_back('"');

    json.append(kSep)
        .append(kNChunksItem)
        .append(buf, PSGToString(blob_prop.GetNChunks(), buf))
        .push_back('}');

    return json;
}


string ToJsonString(const CNAnnotRecord &  annot_record,
                    int32_t  sat,
                    const string &  custom_blob_id)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');

    json.append(kLastModifiedItem)
        .append(buf, PSGToString(annot_record.GetModified(), buf));

    if (custom_blob_id.empty()) {
        json.append(kSep)
            .append(kSatItem)
            .append(buf, PSGToString(sat, buf));

        json.append(kSep)
            .append(kSatKeyItem)
            .append(buf, PSGToString(annot_record.GetSatKey(), buf));

        // Adding blob_id as a string for future replacement of the separate
        // sat and sat_key
        json.append(kSep)
            .append(kBlobIdItem)
            .push_back('"');
        json.append(buf, PSGToString(sat, buf))
            .push_back('.');
        json.append(buf, PSGToString(annot_record.GetSatKey(), buf))
            .push_back('"');
    } else {
        // If a custom blob_id is provided then there is no need to send
        // the one from bioseq info
        json.append(kSep)
            .append(kBlobIdItem)
            .push_back('"');
        json.append(custom_blob_id)
            .push_back('"');
    }

    json.append(kSep)
        .append(kAccessionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(annot_record.GetAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kVersionItem)
        .append(buf, PSGToString(annot_record.GetVersion(), buf));

    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, PSGToString(annot_record.GetSeqIdType(), buf));

    json.append(kSep)
        .append(kStartItem)
        .append(buf, PSGToString(annot_record.GetStart(), buf));

    json.append(kSep)
        .append(kStopItem)
        .append(buf, PSGToString(annot_record.GetStop(), buf))

        .append(kSep)
        .append(kAnnotInfoItem)
        .append("\"{}\"")

        .append(kSep)
        .append(kSeqAnnotInfoItem)
        .push_back('"');
    json.append(NStr::Base64Encode(annot_record.GetSeqAnnotInfo(), 0))
        .push_back('"');

    json.push_back('}');
    return json;
}


string ToJsonString(const map<string, int> &  per_na_results)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');

    bool    need_sep = false;
    for (const auto &  item : per_na_results) {
        if (need_sep)
            json.append(kSep);
        need_sep = true;

        json.push_back('"');
        json.append(NStr::JsonEncode(item.first))
            .push_back('"');
        json.append(kDictValueSep)
            .append(buf, PSGToString(item.second, buf));
    }

    json.push_back('}');
    return json;
}


string ToJsonString(const CBioseqInfoFetchRequest &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"BioseqInfo request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eAccession)) {
        json.append(kSep)
            .append(kAccessionItem)
            .push_back('"');
        json.append(NStr::JsonEncode(request.GetAccession()))
            .push_back('"');
    } else {
        json.append(kSep)
            .append(kAccessionItem)
            .append("null");
    }

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eVersion)) {
        json.append(kSep)
            .append(kVersionItem)
            .append(buf, PSGToString(request.GetVersion(), buf));
    } else {
        json.append(kSep)
            .append(kVersionItem)
            .append("null");
    }

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eSeqIdType)) {
        json.append(kSep)
            .append(kSeqIdTypeItem)
            .append(buf, PSGToString(request.GetSeqIdType(), buf));
    } else {
        json.append(kSep)
            .append(kSeqIdTypeItem)
            .append("null");
    }

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eGI)) {
        json.append(kSep)
            .append(kGiItem)
            .append(buf, PSGToString(request.GetGI(), buf));
    } else {
        json.append(kSep)
            .append(kGiItem)
            .append("null");
    }

    json.push_back('}');
    return json;
}


string ToJsonString(const CSi2CsiFetchRequest &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Si2Csi request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqId)) {
        json.append(kSep)
            .append(kSecSeqIdItem)
            .push_back('"');
        json.append(NStr::JsonEncode(request.GetSecSeqId()))
            .push_back('"');
    } else {
        json.append(kSep)
            .append(kSecSeqIdItem)
            .append("null");
    }

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqIdType)) {
        json.append(kSep)
            .append(kSecSeqIdTypeItem)
            .append(buf, PSGToString(request.GetSecSeqIdType(), buf));
    } else {
        json.append(kSep)
            .append(kSecSeqIdTypeItem)
            .append("null");
    }
    json.push_back('}');
    return json;
}

string ToJsonString(const CBlobFetchRequest &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Blob prop request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    if (request.HasField(CBlobFetchRequest::EFields::eSat)) {
        json.append(kSep)
            .append(kSatItem)
            .append(buf, PSGToString(request.GetSat(), buf));
    } else {
        json.append(kSep)
            .append(kSatItem)
            .append("null");
    }

    if (request.HasField(CBlobFetchRequest::EFields::eSatKey)) {
        json.append(kSep)
            .append(kSatKeyItem)
            .append(buf, PSGToString(request.GetSatKey(), buf));
    } else {
        json.append(kSep)
            .append(kSatKeyItem)
            .append("null");
    }

    if (request.HasField(CBlobFetchRequest::EFields::eLastModified)) {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append(buf, PSGToString(request.GetLastModified(), buf));
    } else {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append("null");
    }

    json.push_back('}');
    return json;
}


string ToJsonString(const CSI2CSIRecord &  record)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kSecSeqIdItem)
        .push_back('"');
    json.append(NStr::JsonEncode(record.GetSecSeqId()))
        .push_back('"');

    json.append(kSep)
        .append(kSecSeqIdTypeItem)
        .append(buf, PSGToString(record.GetSecSeqIdType(), buf))

        .append(kSep)
        .append(kAccessionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(record.GetAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kGiItem)
        .append(buf, PSGToString(record.GetGI(), buf));

    json.append(kSep)
        .append(kSecSeqStateItem)
        .append(buf, PSGToString(record.GetSecSeqState(), buf));

    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, PSGToString(record.GetSeqIdType(), buf));

    json.append(kSep)
        .append(kVersionItem)
        .append(buf, PSGToString(record.GetVersion(), buf));

    json.push_back('}');
    return json;
}


string ToJsonString(const CCassBlobTaskLoadBlob &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Blob request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kSatNameItem)
        .push_back('"');
    json.append(request.GetKeySpace())
        .push_back('"');

    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, PSGToString(request.GetSatKey(), buf));

    auto    last_modified = request.GetModified();
    if (last_modified == CCassBlobTaskLoadBlob::kAnyModified) {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append("null");
    } else {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append(buf, PSGToString(last_modified, buf));
    }

    json.append(kSep)
        .append(kBlopPropProvidedItem);
    if (request.BlobPropsProvided())
        json.append("true");
    else
        json.append("false");

    json.append(kSep)
        .append(kChunksRequestedItem);
    if (request.LoadChunks())
        json.append("true");
    else
        json.append("false");

    json.push_back('}');
    return json;
}


string ToJsonString(const CCassBlobTaskFetchSplitHistory &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Split history request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kSatNameItem)
        .push_back('"');
    json.append(request.GetKeySpace())
        .push_back('"');

    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, PSGToString(request.GetKey(), buf));

    auto    split_version = request.GetSplitVersion();
    if (split_version == CCassBlobTaskFetchSplitHistory::kAllVersions) {
        json.append(kSep)
            .append(kSplitVersionItem)
            .append("null");
    } else {
        json.append(kSep)
            .append(kSplitVersionItem)
            .append(buf, PSGToString(split_version, buf));
    }

    json.push_back('}');
    return json;
}


string ToJsonString(const CCassNAnnotTaskFetch &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Named annotation request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kSatNameItem)
        .push_back('"');
    json.append(request.GetKeySpace())
        .push_back('"');

    json.append(kSep)
        .append(kAccessionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(request.GetAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kVersionItem)
        .append(buf, PSGToString(request.GetVersion(), buf));

    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, PSGToString(request.GetSeqIdType(), buf));

    json.append(kSep)
        .append(kAnnotNamesItem)
        .push_back('[');

    bool    is_empty_list = true;
    for (const auto &  item : request.GetAnnotNames()) {
        if (is_empty_list)  is_empty_list = false;
        else                json.append(kSep);

        json.push_back('"');
        json.append(NStr::JsonEncode(item))
            .push_back('"');
    }

    json.push_back(']');
    json.push_back('}');
    return json;
}


string ToJsonString(const CCassStatusHistoryTaskGetPublicComment &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Public comment request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kSatNameItem)
        .push_back('"');
    json.append(request.GetKeySpace())
        .push_back('"');

    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, PSGToString(request.GetKey(), buf));

    json.push_back('}');
    return json;
}


string ToJsonString(const CCassAccVerHistoryTaskFetch &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"Accession version history request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kSatNameItem)
        .push_back('"');
    json.append(request.GetKeySpace())
        .push_back('"');

    json.append(kSep)
        .append(kAccessionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(request.GetAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kVersionItem)
        .append(buf, PSGToString(request.GetVersion(), buf));

    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, PSGToString(request.GetSeqIdType(), buf))

        .push_back('}');
    return json;
}


string ToJsonString(const CPubseqGatewayFetchIpgReportRequest &  request,
                    const string &  datacenter)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kRequestItem)
        .append("\"IPG resolve request\"")

        .append(kSep)
        .append(kDatacenterItem)
        .push_back('"');
    json.append(datacenter)
        .push_back('"');

    json.append(kSep)
        .append(kProteinItem)
        .push_back('"');

    if (request.HasProtein())
        json.append(SanitizeInputValue(NStr::JsonEncode(request.GetProtein())));
    else
        json.append("<null>");

    json.push_back('"');
    json.append(kSep)
        .append(kNucleotideItem)
        .push_back('"');

    if (request.HasNucleotide())
        json.append(SanitizeInputValue(NStr::JsonEncode(request.GetNucleotide())));
    else
        json.append("<null>");

    json.push_back('"');
    json.append(kSep)
        .append(kIPGItem);

    if (request.HasIpg()) {
        json.append(buf, PSGToString(request.GetIpg(), buf));
    } else {
        json.append("<null>");
    }
    json.push_back('}');

    return json;
}


string ToJsonString(const SAccVerHistRec &  history_record)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');

    json.append(kGiItem)
        .append(buf, PSGToString(history_record.gi, buf))

        .append(kSep)
        .append(kAccessionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(history_record.accession))
        .push_back('"');

    json.append(kSep)
        .append(kVersionItem)
        .append(buf, PSGToString(history_record.version, buf));

    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, PSGToString(history_record.seq_id_type, buf));

    json.append(kSep)
        .append(kDateItem)
        .append(buf, PSGToString(history_record.date, buf));

    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, PSGToString(history_record.sat_key, buf));

    json.append(kSep)
        .append(kSatItem)
        .append(buf, PSGToString(history_record.sat, buf));

    json.append(kSep)
        .append(kChainItem)
        .append(buf, PSGToString(history_record.chain, buf))

        .push_back('}');
    return json;
}


string ToJsonString(const CIpgStorageReportEntry &  ipg_entry)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    // The incoming parameter is 'protein' so it is returned accordingly
    json.push_back('{');
    json.append(kProteinItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kIPGItem)
        .append(buf, PSGToString(ipg_entry.GetIpg(), buf))

    // The incoming parameter is 'nucleotide' so it is returned accordingly
        .append(kSep)
        .append(kNucleotideItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetNucAccession()))
        .push_back('"');

    json.append(kSep)
        .append(kProductNameItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetProductName()))
        .push_back('"');

    json.append(kSep)
        .append(kDivisionItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetDiv()))
        .push_back('"');

    json.append(kSep)
        .append(kAssemblyItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetAssembly()))
        .push_back('"');

    json.append(kSep)
        .append(kTaxIdItem)
        .append(buf, PSGToString(ipg_entry.GetTaxid(), buf))

        .append(kSep)
        .append(kStrainItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetStrain()))
        .push_back('"');

    json.append(kSep)
        .append(kBioProjectItem)
        .push_back('"');
    json.append(NStr::JsonEncode(ipg_entry.GetBioProject()))
        .push_back('"');

    json.append(kSep)
        .append(kLengthItem)
        .append(buf, PSGToString(ipg_entry.GetLength(), buf));

    json.append(kSep)
        .append(kGBStateItem)
        .append(buf, PSGToString(ipg_entry.GetGbState(), buf))

        .push_back('}');
    return json;
}


static const string     kIdItem = "\"id\": ";
static const string     kConnCntAtOpenItem = "\"conn_cnt_at_open\": ";
static const string     kOpenTimestampItem = "\"open_timestamp\": ";
static const string     kLastRequestTimestampItem = "\"last_request_timestamp\": ";
static const string     kFinishedReqsCntItem = "\"finished_reqs_cnt\": ";
static const string     kRejectedDueToSoftLimitReqsCntItem = "\"rejected_due_to_soft_limit_reqs_cnt\": ";
static const string     kBacklogReqsCntItem = "\"backlog_reqs_cnt\": ";
static const string     kRunningReqsCntItem = "\"running_reqs_cnt\": ";
static const string     kPeerIpItem = "\"peer_ip\": ";
static const string     kExceedSoftLimitFlagItem = "\"exceed_soft_limit_flag\": ";
static const string     kMovedFromBadToGoodItem = "\"moved_from_bad_to_good\": ";
static const string     kPeerIdItem = "\"peer_id\": ";
static const string     kPeerUserAgentItem = "\"peer_user_agent\": ";
string ToJsonString(const SConnectionRunTimeProperties &  conn_props)
{
    string              json;
    char                buf[kPSGToStringBufferSize];

    json.push_back('{');
    json.append(kIdItem)
        .append(buf, PSGToString(conn_props.m_Id, buf));

    json.append(kSep)
        .append(kConnCntAtOpenItem)
        .append(buf, PSGToString(conn_props.m_ConnCntAtOpen, buf))
        .append(kSep)
        .append(kOpenTimestampItem)
        .push_back('"');
    json.append(FormatPreciseTime(conn_props.m_OpenTimestamp))
        .push_back('"');

    if (conn_props.m_LastRequestTimestamp.has_value()) {
        json.append(kSep)
            .append(kLastRequestTimestampItem)
            .push_back('"');
        json.append(FormatPreciseTime(conn_props.m_LastRequestTimestamp.value()))
            .push_back('"');
    } else {
        json.append(kSep)
            .append(kLastRequestTimestampItem)
            .append("null");
    }

    json.append(kSep)
        .append(kFinishedReqsCntItem)
        .append(buf, PSGToString(conn_props.m_NumFinishedRequests, buf));

    json.append(kSep)
        .append(kRejectedDueToSoftLimitReqsCntItem)
        .append(buf, PSGToString(conn_props.m_RejectedDueToSoftLimit, buf));

    json.append(kSep)
        .append(kBacklogReqsCntItem)
        .append(buf, PSGToString(conn_props.m_NumBackloggedRequests, buf));

    json.append(kSep)
        .append(kRunningReqsCntItem)
        .append(buf, PSGToString(conn_props.m_NumRunningRequests, buf))
        .append(kSep)
        .append(kPeerIpItem)
        .push_back('"');
    json.append(conn_props.m_PeerIp)
        .push_back('"');

    if (conn_props.m_PeerId.has_value()) {
        json.append(kSep)
            .append(kPeerIdItem)
            .push_back('"');
        json.append(NStr::JsonEncode(conn_props.m_PeerId.value()))
            .push_back('"');
    }

    if (conn_props.m_PeerUserAgent.has_value()) {
        json.append(kSep)
            .append(kPeerUserAgentItem)
            .push_back('"');
        json.append(NStr::JsonEncode(conn_props.m_PeerUserAgent.value()))
            .push_back('"');
    }

    if (conn_props.m_ExceedSoftLimitFlag) {
        json.append(kSep)
            .append(kExceedSoftLimitFlagItem)
            .append("true");
    } else {
        json.append(kSep)
            .append(kExceedSoftLimitFlagItem)
            .append("false");
    }

    if (conn_props.m_MovedFromBadToGood) {
        json.append(kSep)
            .append(kMovedFromBadToGoodItem)
            .append("true");
    } else {
        json.append(kSep)
            .append(kMovedFromBadToGoodItem)
            .append("false");
    }

    json.push_back('}');
    return json;
}

