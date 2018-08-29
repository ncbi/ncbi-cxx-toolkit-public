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
#include <objtools/pubseq_gateway/protobuf/psg_protobuf_data.hpp>

#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_utils.hpp"

USING_NCBI_SCOPE;


static const string     kAccession = "accession";
static const string     kVersion = "version";
static const string     kSeqIdType = "seq_id_type";
static const string     kMol = "mol";
static const string     kLength = "length";
static const string     kState = "state";
static const string     kSat = "sat";
static const string     kSatKey = "sat_key";
static const string     kTaxId = "tax_id";
static const string     kHash = "hash";
static const string     kDateChanged = "date_changed";
static const string     kSeqIds = "seq_ids";

static const string     kKey = "Key";
static const string     kLastModified = "LastModified";
static const string     kFlags = "Flags";
static const string     kSize = "Size";
static const string     kSizeUnpacked = "SizeUnpacked";
static const string     kClass = "Class";
static const string     kDateAsn1 = "DateAsn1";
static const string     kHupDate = "HupDate";
static const string     kDiv = "Div";
static const string     kId2Info = "Id2Info";
static const string     kOwner = "Owner";
static const string     kUserName = "UserName";
static const string     kNChunks = "NChunks";


void ConvertBioseqInfoToBioseqProtobuf(const SBioseqInfo &  bioseq_info,
                                       string &  bioseq_protobuf)
{
    psg::retrieval::bioseq_info         protobuf_bioseq_info;
    protobuf_bioseq_info.set_accession(bioseq_info.m_Accession);
    protobuf_bioseq_info.set_version(bioseq_info.m_Version);
    protobuf_bioseq_info.set_seq_id_type(bioseq_info.m_SeqIdType);

    psg::retrieval::bioseq_info_value *  protobuf_bioseq_info_value =
            protobuf_bioseq_info.mutable_value();

    protobuf_bioseq_info_value->set_sat(bioseq_info.m_Sat);
    protobuf_bioseq_info_value->set_sat_key(bioseq_info.m_SatKey);
    protobuf_bioseq_info_value->set_state(bioseq_info.m_State);
    protobuf_bioseq_info_value->set_mol(bioseq_info.m_Mol);
    protobuf_bioseq_info_value->set_hash(bioseq_info.m_Hash);
    protobuf_bioseq_info_value->set_length(bioseq_info.m_Length);
    protobuf_bioseq_info_value->set_date_changed(bioseq_info.m_DateChanged);
    protobuf_bioseq_info_value->set_tax_id(bioseq_info.m_TaxId);
    protobuf_bioseq_info_value->mutable_seq_ids()->insert(bioseq_info.m_SeqIds.begin(),
                                                          bioseq_info.m_SeqIds.end());

    CPubseqGatewayData::PackBioseqInfo(protobuf_bioseq_info, bioseq_protobuf);
}


void ConvertBioseqProtobufToBioseqInfo(const string &  bioseq_protobuf,
                                       SBioseqInfo &  bioseq_info)
{
    psg::retrieval::bioseq_info_value       protobuf_bioseq_info_value;

    CPubseqGatewayData::UnpackBioseqInfo(bioseq_protobuf,
                                         protobuf_bioseq_info_value);

    // These fields must be already populated in bioseq_info
    //    bioseq_info.m_Accession
    //    bioseq_info.m_Version
    //    bioseq_info.m_SeqIdType

    bioseq_info.m_DateChanged = protobuf_bioseq_info_value.date_changed();
    bioseq_info.m_Mol = protobuf_bioseq_info_value.mol();
    bioseq_info.m_Length = protobuf_bioseq_info_value.length();
    bioseq_info.m_State = protobuf_bioseq_info_value.state();
    bioseq_info.m_Sat = protobuf_bioseq_info_value.sat();
    bioseq_info.m_SatKey = protobuf_bioseq_info_value.sat_key();
    bioseq_info.m_TaxId = protobuf_bioseq_info_value.tax_id();
    bioseq_info.m_Hash = protobuf_bioseq_info_value.hash();
    bioseq_info.m_SeqIds.insert(protobuf_bioseq_info_value.seq_ids().begin(),
                                protobuf_bioseq_info_value.seq_ids().end());
}


void ConvertBioseqInfoToJson(const SBioseqInfo &  bioseq_info,
                             TServIncludeData  include_data_flags,
                             string &  bioseq_json)
{
    CJsonNode   json = ConvertBioseqInfoToJson(bioseq_info, include_data_flags);
    bioseq_json = json.Repr();
}


CJsonNode  ConvertBioseqInfoToJson(const SBioseqInfo &  bioseq_info,
                                   TServIncludeData  include_data_flags)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    if (include_data_flags & fServCanonicalId) {
        json.SetString(kAccession, bioseq_info.m_Accession);
        json.SetInteger(kVersion, bioseq_info.m_Version);
        json.SetInteger(kSeqIdType, bioseq_info.m_SeqIdType);
    }
    if (include_data_flags & fServMoleculeType)
        json.SetInteger(kMol, bioseq_info.m_Mol);
    if (include_data_flags & fServLength)
        json.SetInteger(kLength, bioseq_info.m_Length);
    if (include_data_flags & fServState)
        json.SetInteger(kState, bioseq_info.m_State);
    if (include_data_flags & fServBlobId) {
        json.SetInteger(kSat, bioseq_info.m_Sat);
        json.SetInteger(kSatKey, bioseq_info.m_SatKey);
    }
    if (include_data_flags & fServTaxId)
        json.SetInteger(kTaxId, bioseq_info.m_TaxId);
    if (include_data_flags & fServHash)
        json.SetInteger(kHash, bioseq_info.m_Hash);
    if (include_data_flags & fServDateChanged)
        json.SetInteger(kDateChanged, bioseq_info.m_DateChanged);

    if (include_data_flags & fServSeqIds) {
        CJsonNode       seq_ids(CJsonNode::NewObjectNode());
        for (map<int, string>::const_iterator  it = bioseq_info.m_SeqIds.begin();
             it != bioseq_info.m_SeqIds.end(); ++it) {
            seq_ids.SetString(NStr::NumericToString(it->first), it->second);
        }
        json.SetByKey(kSeqIds, seq_ids);
    }

    return json;
}


CJsonNode  ConvertBlobPropToJson(const CBlobRecord &  blob_prop)
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


void ConvertSi2csiToBioseqKey(const string &  si2csi_protobuf,
                              SBioseqKey &  bioseq_key)
{
    psg::retrieval::si2csi_value    protobuf_si2csi_value;
    CPubseqGatewayData::UnpackSiInfo(si2csi_protobuf, protobuf_si2csi_value);

    bioseq_key.m_Accession = protobuf_si2csi_value.accession();
    bioseq_key.m_Version = protobuf_si2csi_value.version();
    bioseq_key.m_SeqIdType = protobuf_si2csi_value.seq_id_type();
}


void ConvertBlobPropProtobufToBlobRecord(int  sat_key,
                                         int64_t  last_modified,
                                         const string &  blob_prop_protobuf,
                                         CBlobRecord &  blob_record)
{
    psg::retrieval::blob_prop_value     protobuf_blob_prop_value;
    CPubseqGatewayData::UnpackBlobPropInfo(blob_prop_protobuf,
                                           protobuf_blob_prop_value);

    blob_record.SetModified(last_modified);
    blob_record.SetFlags(protobuf_blob_prop_value.flags());
    blob_record.SetSize(protobuf_blob_prop_value.size());
    blob_record.SetSizeUnpacked(protobuf_blob_prop_value.size_unpacked());
    blob_record.SetDateAsn1(protobuf_blob_prop_value.date_asn1());
    blob_record.SetHupDate(protobuf_blob_prop_value.hup_date());
    blob_record.SetDiv(protobuf_blob_prop_value.div());
    blob_record.SetId2Info(protobuf_blob_prop_value.id2_info());
    blob_record.SetUserName(protobuf_blob_prop_value.username());
    blob_record.SetKey(sat_key);
    blob_record.SetNChunks(protobuf_blob_prop_value.n_chunks());
    blob_record.SetOwner(protobuf_blob_prop_value.owner());
    blob_record.SetClass(protobuf_blob_prop_value.class_());
}

