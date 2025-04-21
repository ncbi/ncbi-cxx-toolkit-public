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

    char                buf[64];
    long                len;

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

    json.append(1, '{');
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_CanonicalId) {
        len = PSGToString(bioseq_info.GetVersion(), buf);
        json.append(kAccessionItem)
            .append(1, '"')
            .append(NStr::JsonEncode(bioseq_info.GetAccession()))
            .append(1, '"')
            .append(kSep)
            .append(kVersionItem)
            .append(buf, len)
            .append(kSep)
            .append(kSeqIdTypeItem);

        len = PSGToString(bioseq_info.GetSeqIdType(), buf);
        json.append(buf, len);
        some = true;
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Name) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kNameItem)
            .append(1, '"')
            .append(NStr::JsonEncode(bioseq_info.GetName()))
            .append(1, '"');
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Gi) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetGI(), buf);
        json.append(kGiItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_DateChanged) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetDateChanged(), buf);
        json.append(kDateChangedItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Hash) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetHash(), buf);
        json.append(kHashItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_Length) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetLength(), buf);
        json.append(kLengthItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_MoleculeType) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetMol(), buf);
        json.append(kMolItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_BlobId) {
        if (some)   json.append(kSep);
        else        some = true;

        if (custom_blob_id.empty()) {
            len = PSGToString(bioseq_info.GetSat(), buf);
            json.append(kSatItem)
                .append(buf, len)
                .append(kSep)
                .append(kSatKeyItem);

            len = PSGToString(bioseq_info.GetSatKey(), buf);
            json.append(buf, len)
                .append(kSep);

            // Adding blob_id as a string for future replacement of the separate
            // sat and sat_key
            json.append(kBlobIdItem)
                .append(1, '"');

            len = PSGToString(bioseq_info.GetSat(), buf);
            json.append(buf, len)
                .append(1, '.');

            len = PSGToString(bioseq_info.GetSatKey(), buf);
            json.append(buf, len)
                .append(1, '"');
        } else {
            // If a custom blob_id is provided then there is no need to send
            // the one from bioseq info
            json.append(kBlobIdItem)
                .append(1, '"')
                .append(custom_blob_id)
                .append(1, '"');
        }
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqIds) {
        if (some)   json.append(kSep);
        else        some = true;

        json.append(kSeqIdsItem)
            .append(1, '[');

        bool    not_first = false;
        for (const auto &  item : bioseq_info.GetSeqIds()) {
            if (not_first)
                json.append(kSep);
            else
                not_first = true;

            len = PSGToString(get<0>(item), buf);
            json.append(1, '[')
                .append(buf, len)
                .append(kSep)
                .append(1, '"')
                .append(NStr::JsonEncode(get<1>(item)))
                .append(1, '"')
                .append(1, ']');
        }
        json.append(1, ']');
    }

    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_SeqState) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetSeqState(), buf);
        json.append(kSeqStateItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_State) {
        if (some)   json.append(kSep);
        else        some = true;

        len = PSGToString(bioseq_info.GetState(), buf);
        json.append(kStateItem)
            .append(buf, len);
    }
    if (include_data_flags & SPSGS_ResolveRequest::fPSGS_TaxId) {
        if (some)   json.append(kSep);
        // else        some = true;     // suppress dead assignment warning

        len = PSGToString(bioseq_info.GetTaxId(), buf);
        json.append(kTaxIdItem)
            .append(buf, len);
    }

    json.append(1, '}');
    return json;
}


string  ToJsonString(const CBlobRecord &  blob_prop)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{');
    len = PSGToString(blob_prop.GetKey(), buf);
    json.append(kKeyItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetModified(), buf);
    json.append(kSep)
        .append(kLastModifiedItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetFlags(), buf);
    json.append(kSep)
        .append(kFlagsItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetSize(), buf);
    json.append(kSep)
        .append(kSizeItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetSizeUnpacked(), buf);
    json.append(kSep)
        .append(kSizeUnpackedItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetClass(), buf);
    json.append(kSep)
        .append(kClassItem)
        .append(buf, len);
    len = PSGToString(blob_prop.GetDateAsn1(), buf);
    json.append(kSep)
        .append(kDateAsn1Item)
        .append(buf, len);
    len = PSGToString(blob_prop.GetHupDate(), buf);
    json.append(kSep)
        .append(kHupDateItem)
        .append(buf, len)

        .append(kSep)
        .append(kDivItem)
        .append(1, '"')
        .append(NStr::JsonEncode(blob_prop.GetDiv()))
        .append(1, '"')

        .append(kSep)
        .append(kId2InfoItem)
        .append(1, '"')
        .append(blob_prop.GetId2Info())     // Cannot have chars need to escape
        .append(1, '"');

    len = PSGToString(blob_prop.GetOwner(), buf);
    json.append(kSep)
        .append(kOwnerItem)
        .append(buf, len)

        .append(kSep)
        .append(kUserNameItem)
        .append(1, '"')
        .append(NStr::JsonEncode(blob_prop.GetUserName()))
        .append(1, '"');

    len = PSGToString(blob_prop.GetNChunks(), buf);
    json.append(kSep)
        .append(kNChunksItem)
        .append(buf, len)
        .append(1, '}');

    return json;
}


string ToJsonString(const CNAnnotRecord &  annot_record,
                    int32_t  sat,
                    const string &  custom_blob_id)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{');

    len = PSGToString(annot_record.GetModified(), buf);
    json.append(kLastModifiedItem)
        .append(buf, len);

    if (custom_blob_id.empty()) {
        len = PSGToString(sat, buf);
        json.append(kSep)
            .append(kSatItem)
            .append(buf, len);

        len = PSGToString(annot_record.GetSatKey(), buf);
        json.append(kSep)
            .append(kSatKeyItem)
            .append(buf, len);

        // Adding blob_id as a string for future replacement of the separate
        // sat and sat_key
        len = PSGToString(sat, buf);
        json.append(kSep)
            .append(kBlobIdItem)
            .append(1, '"')
            .append(buf, len)
            .append(1, '.');
        len = PSGToString(annot_record.GetSatKey(), buf);
        json.append(buf, len)
            .append(1, '"');
    } else {
        // If a custom blob_id is provided then there is no need to send
        // the one from bioseq info
        json.append(kSep)
            .append(kBlobIdItem)
            .append(1, '"')
            .append(custom_blob_id)
            .append(1, '"');
    }

    json.append(kSep)
        .append(kAccessionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(annot_record.GetAccession()))
        .append(1, '"');

    len = PSGToString(annot_record.GetVersion(), buf);
    json.append(kSep)
        .append(kVersionItem)
        .append(buf, len);

    len = PSGToString(annot_record.GetSeqIdType(), buf);
    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, len);

    len = PSGToString(annot_record.GetStart(), buf);
    json.append(kSep)
        .append(kStartItem)
        .append(buf, len);

    len = PSGToString(annot_record.GetStop(), buf);
    json.append(kSep)
        .append(kStopItem)
        .append(buf, len)

        .append(kSep)
        .append(kAnnotInfoItem)
        .append("\"{}\"")

        .append(kSep)
        .append(kSeqAnnotInfoItem)
        .append(1, '"')
        .append(NStr::Base64Encode(annot_record.GetSeqAnnotInfo(), 0))
        .append(1, '"')

        .append(1, '}');
    return json;
}


string ToJsonString(const map<string, int> &  per_na_results)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{');

    bool    need_sep = false;
    for (const auto &  item : per_na_results) {
        if (need_sep)
            json.append(kSep);
        need_sep = true;

        len = PSGToString(item.second, buf);
        json.append(1, '"')
            .append(NStr::JsonEncode(item.first))
            .append(1, '"')
            .append(kDictValueSep)
            .append(buf, len);
    }

    json.append(1, '}');
    return json;
}


string ToJsonString(const CBioseqInfoFetchRequest &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"BioseqInfo request\"");

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eAccession))
        json.append(kSep)
            .append(kAccessionItem)
            .append(1, '"')
            .append(NStr::JsonEncode(request.GetAccession()))
            .append(1, '"');
    else
        json.append(kSep)
            .append(kAccessionItem)
            .append("null");

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eVersion)) {
        len = PSGToString(request.GetVersion(), buf);
        json.append(kSep)
            .append(kVersionItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kVersionItem)
            .append("null");
    }

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eSeqIdType)) {
        len = PSGToString(request.GetSeqIdType(), buf);
        json.append(kSep)
            .append(kSeqIdTypeItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kSeqIdTypeItem)
            .append("null");
    }

    if (request.HasField(CBioseqInfoFetchRequest::EFields::eGI)) {
        len = PSGToString(request.GetGI(), buf);
        json.append(kSep)
            .append(kGiItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kGiItem)
            .append("null");
    }

    json.append(1, '}');
    return json;
}


string ToJsonString(const CSi2CsiFetchRequest &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Si2Csi request\"");

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqId))
        json.append(kSep)
            .append(kSecSeqIdItem)
            .append(1, '"')
            .append(NStr::JsonEncode(request.GetSecSeqId()))
            .append(1, '"');
    else
        json.append(kSep)
            .append(kSecSeqIdItem)
            .append("null");

    if (request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqIdType)) {
        len = PSGToString(request.GetSecSeqIdType(), buf);
        json.append(kSep)
            .append(kSecSeqIdTypeItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kSecSeqIdTypeItem)
            .append("null");
    }
    json.append(1, '}');
    return json;
}

string ToJsonString(const CBlobFetchRequest &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Blob prop request\"");

    if (request.HasField(CBlobFetchRequest::EFields::eSat)) {
        len = PSGToString(request.GetSat(), buf);
        json.append(kSep)
            .append(kSatItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kSatItem)
            .append("null");
    }

    if (request.HasField(CBlobFetchRequest::EFields::eSatKey)) {
        len = PSGToString(request.GetSatKey(), buf);
        json.append(kSep)
            .append(kSatKeyItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kSatKeyItem)
            .append("null");
    }

    if (request.HasField(CBlobFetchRequest::EFields::eLastModified)) {
        len = PSGToString(request.GetLastModified(), buf);
        json.append(kSep)
            .append(kLastModifiedItem)
            .append(buf, len);
    } else {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append("null");
    }

    json.append(1, '}');
    return json;
}


string ToJsonString(const CSI2CSIRecord &  record)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kSecSeqIdItem)
        .append(1, '"')
        .append(NStr::JsonEncode(record.GetSecSeqId()))
        .append(1, '"');

    len = PSGToString(record.GetSecSeqIdType(), buf);
    json.append(kSep)
        .append(kSecSeqIdTypeItem)
        .append(buf, len)

        .append(kSep)
        .append(kAccessionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(record.GetAccession()))
        .append(1, '"');

    len = PSGToString(record.GetGI(), buf);
    json.append(kSep)
        .append(kGiItem)
        .append(buf, len);

    len = PSGToString(record.GetSecSeqState(), buf);
    json.append(kSep)
        .append(kSecSeqStateItem)
        .append(buf, len);

    len = PSGToString(record.GetSeqIdType(), buf);
    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, len);

    len = PSGToString(record.GetVersion(), buf);
    json.append(kSep)
        .append(kVersionItem)
        .append(buf, len);

    json.append(1, '}');
    return json;
}


string ToJsonString(const CCassBlobTaskLoadBlob &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Blob request\"")

        .append(kSep)
        .append(kSatNameItem)
        .append(1, '"')
        .append(request.GetKeySpace())
        .append(1, '"');

    len = PSGToString(request.GetSatKey(), buf);
    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, len);

    auto    last_modified = request.GetModified();
    if (last_modified == CCassBlobTaskLoadBlob::kAnyModified) {
        json.append(kSep)
            .append(kLastModifiedItem)
            .append("null");
    } else {
        len = PSGToString(last_modified, buf);
        json.append(kSep)
            .append(kLastModifiedItem)
            .append(buf, len);
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

    json.append(1, '}');
    return json;
}


string ToJsonString(const CCassBlobTaskFetchSplitHistory &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Split history request\"")

        .append(kSep)
        .append(kSatNameItem)
        .append(1, '"')
        .append(request.GetKeySpace())
        .append(1, '"');

    len = PSGToString(request.GetKey(), buf);
    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, len);

    auto    split_version = request.GetSplitVersion();
    if (split_version == CCassBlobTaskFetchSplitHistory::kAllVersions) {
        json.append(kSep)
            .append(kSplitVersionItem)
            .append("null");
    } else {
        len = PSGToString(split_version, buf);
        json.append(kSep)
            .append(kSplitVersionItem)
            .append(buf, len);
    }

    json.append(1, '}');
    return json;
}


string ToJsonString(const CCassNAnnotTaskFetch &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Named annotation request\"")

        .append(kSep)
        .append(kSatNameItem)
        .append(1, '"')
        .append(request.GetKeySpace())
        .append(1, '"')

        .append(kSep)
        .append(kAccessionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(request.GetAccession()))
        .append(1, '"');

    len = PSGToString(request.GetVersion(), buf);
    json.append(kSep)
        .append(kVersionItem)
        .append(buf, len);

    len = PSGToString(request.GetSeqIdType(), buf);
    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, len);

    json.append(kSep)
        .append(kAnnotNamesItem)
        .append(1, '[');

    bool    is_empty_list = true;
    for (const auto &  item : request.GetAnnotNames()) {
        if (is_empty_list)  is_empty_list = false;
        else                json.append(kSep);

        json.append(1, '"')
            .append(NStr::JsonEncode(item))
            .append(1, '"');
    }

    json.append(1, ']');
    json.append(1, '}');
    return json;
}


string ToJsonString(const CCassStatusHistoryTaskGetPublicComment &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Public comment request\"")

        .append(kSep)
        .append(kSatNameItem)
        .append(1, '"')
        .append(request.GetKeySpace())
        .append(1, '"');

    len = PSGToString(request.GetKey(), buf);
    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, len);

    json.append(1, '}');
    return json;
}


string ToJsonString(const CCassAccVerHistoryTaskFetch &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"Accession version history request\"")

        .append(kSep)
        .append(kSatNameItem)
        .append(1, '"')
        .append(request.GetKeySpace())
        .append(1, '"')

        .append(kSep)
        .append(kAccessionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(request.GetAccession()))
        .append(1, '"');

    len = PSGToString(request.GetVersion(), buf);
    json.append(kSep)
        .append(kVersionItem)
        .append(buf, len);

    len = PSGToString(request.GetSeqIdType(), buf);
    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, len)

        .append(1, '}');
    return json;
}


string ToJsonString(const CPubseqGatewayFetchIpgReportRequest &  request)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{')
        .append(kRequestItem)
        .append("\"IPG resolve request\"")
        .append(kSep)
        .append(kProteinItem)
        .append(1, '"');

    if (request.HasProtein())
        json.append(SanitizeInputValue(NStr::JsonEncode(request.GetProtein())));
    else
        json.append("<null>");

    json.append(1, '"')
        .append(kSep)
        .append(kNucleotideItem)
        .append(1, '"');

    if (request.HasNucleotide())
        json.append(SanitizeInputValue(NStr::JsonEncode(request.GetNucleotide())));
    else
        json.append("<null>");

    json.append(1, '"')
        .append(kSep)
        .append(kIPGItem);

    if (request.HasIpg()) {
        len = PSGToString(request.GetIpg(), buf);
        json.append(buf, len);
    } else {
        json.append("<null>");
    }
    json.append(1, '}');

    return json;
}


string ToJsonString(const SAccVerHistRec &  history_record)
{
    string              json;
    char                buf[64];
    long                len;

    json.append(1, '{');

    len = PSGToString(history_record.gi, buf);
    json.append(kGiItem)
        .append(buf, len)

        .append(kSep)
        .append(kAccessionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(history_record.accession))
        .append(1, '"');

    len = PSGToString(history_record.version, buf);
    json.append(kSep)
        .append(kVersionItem)
        .append(buf, len);

    len = PSGToString(history_record.seq_id_type, buf);
    json.append(kSep)
        .append(kSeqIdTypeItem)
        .append(buf, len);

    len = PSGToString(history_record.date, buf);
    json.append(kSep)
        .append(kDateItem)
        .append(buf, len);

    len = PSGToString(history_record.sat_key, buf);
    json.append(kSep)
        .append(kSatKeyItem)
        .append(buf, len);

    len = PSGToString(history_record.sat, buf);
    json.append(kSep)
        .append(kSatItem)
        .append(buf, len);

    len = PSGToString(history_record.chain, buf);
    json.append(kSep)
        .append(kChainItem)
        .append(buf, len)

        .append(1, '}');
    return json;
}


string ToJsonString(const CIpgStorageReportEntry &  ipg_entry)
{
    string              json;
    char                buf[64];
    long                len;

    // The incoming parameter is 'protein' so it is returned accordingly
    json.append(1, '{')
        .append(kProteinItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetAccession()))
        .append(1, '"');

    len = PSGToString(ipg_entry.GetIpg(), buf);
    json.append(kSep)
        .append(kIPGItem)
        .append(buf, len)

    // The incoming parameter is 'nucleotide' so it is returned accordingly
        .append(kSep)
        .append(kNucleotideItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetNucAccession()))
        .append(1, '"')

        .append(kSep)
        .append(kProductNameItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetProductName()))
        .append(1, '"')

        .append(kSep)
        .append(kDivisionItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetDiv()))
        .append(1, '"')

        .append(kSep)
        .append(kAssemblyItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetAssembly()))
        .append(1, '"');

    len = PSGToString(ipg_entry.GetTaxid(), buf);
    json.append(kSep)
        .append(kTaxIdItem)
        .append(buf, len)

        .append(kSep)
        .append(kStrainItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetStrain()))
        .append(1, '"')

        .append(kSep)
        .append(kBioProjectItem)
        .append(1, '"')
        .append(NStr::JsonEncode(ipg_entry.GetBioProject()))
        .append(1, '"');

    len = PSGToString(ipg_entry.GetLength(), buf);
    json.append(kSep)
        .append(kLengthItem)
        .append(buf, len);

    len = PSGToString(ipg_entry.GetGbState(), buf);
    json.append(kSep)
        .append(kGBStateItem)
        .append(buf, len)

        .append(1, '}');
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
string ToJsonString(const SConnectionRunTimeProperties &  conn_props)
{
    string              json;
    char                buf[64];
    long                len;

    len = PSGToString(conn_props.m_Id, buf);

    json.append(1, '{')
        .append(kIdItem)
        .append(buf, len);

    len = PSGToString(conn_props.m_ConnCntAtOpen, buf);
    json.append(kSep)
        .append(kConnCntAtOpenItem)
        .append(buf, len)
        .append(kSep)
        .append(kOpenTimestampItem)
        .append(1, '"')
        .append(FormatPreciseTime(conn_props.m_OpenTimestamp))
        .append(1, '"');

    if (conn_props.m_LastRequestTimestamp.has_value()) {
        json.append(kSep)
            .append(kLastRequestTimestampItem)
            .append(1, '"')
            .append(FormatPreciseTime(conn_props.m_LastRequestTimestamp.value()))
            .append(1, '"');
    } else {
        json.append(kSep)
            .append(kLastRequestTimestampItem)
            .append("null");
    }

    len = PSGToString(conn_props.m_NumFinishedRequests, buf);
    json.append(kSep)
        .append(kFinishedReqsCntItem)
        .append(buf, len);

    len = PSGToString(conn_props.m_RejectedDueToSoftLimit, buf);
    json.append(kSep)
        .append(kRejectedDueToSoftLimitReqsCntItem)
        .append(buf, len);

    len = PSGToString(conn_props.m_NumBackloggedRequests, buf);
    json.append(kSep)
        .append(kBacklogReqsCntItem)
        .append(buf, len);

    len = PSGToString(conn_props.m_NumRunningRequests, buf);
    json.append(kSep)
        .append(kRunningReqsCntItem)
        .append(buf, len)
        .append(kSep)
        .append(kPeerIpItem)
        .append(1, '"')
        .append(conn_props.m_PeerIp)
        .append(1, '"');

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

    json.append(1, '}');
    return json;
}

