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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_source_data.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objtools/writers/gff3_writer.hpp>

#include <array>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ------------------------------------------------------------------------------
string CGffIdGenerator::GetGffId(
    const CMappedFeat& mf,
    feature::CFeatTree* pFeatTree) 
//  -----------------------------------------------------------------------------t-
{
    auto id = mf.GetNamedQual("ID");
    if (id.empty()) {
        switch(mf.GetFeatSubtype()) {
        default:
            id = xGetGenericId(mf, pFeatTree);
            break;
        case CSeqFeatData::eSubtype_gene:
            id = xGetIdForGene(mf, pFeatTree);
            break;
        case CSeqFeatData::eSubtype_mRNA:
            id = xGetIdForMrna(mf, pFeatTree);
            break;
        case CSeqFeatData::eSubtype_cdregion:
            id = xGetIdForCds(mf, pFeatTree);
            break;
        case CSeqFeatData::eSubtype_exon:
            return xGetIdForNativeExon(mf, pFeatTree); //implicitly disambiguates
        }
    }
    if (!id.empty()) {
        id = xDisambiguate(id);
        mExistingIds.emplace(id);
    }
    return id;
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::GetGffId()
//  -----------------------------------------------------------------------------
{
    return string("id-") + NStr::NumericToString(++mLastTrulyGenericSuffix);
}
    
//  -----------------------------------------------------------------------------
void CGffIdGenerator::Reset()
//  -----------------------------------------------------------------------------
{
    mExistingIds.clear();
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::GetNextGffExonId(
    const string& rnaId)
//  -----------------------------------------------------------------------------
{
    auto idIt = mLastUsedExonIds.find(rnaId);
    if (idIt == mLastUsedExonIds.end()) {
        mLastUsedExonIds[rnaId] = 1;
    }
    else {
        mLastUsedExonIds[rnaId]++;
    }
    string id("exon-");
    auto suffix = string("-") + NStr::NumericToString(mLastUsedExonIds[rnaId]);
    if (NStr::StartsWith(rnaId, "rna-")) {
        id += rnaId.substr(4) + suffix;
    }
    else {
        id += rnaId + suffix;
    }
    return id;
}
 
//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForGene(
    const CMappedFeat& mf,
    feature::CFeatTree* ) 
    //  -----------------------------------------------------------------------------
{
    string id("gene-");

    //1st choice: locus_tag
    const auto& geneRef = mf.GetData().GetGene();
    if (geneRef.IsSetLocus_tag()) {
        id += geneRef.GetLocus_tag();
        return id;
    }

    //2nd choice: locus
    if (geneRef.IsSetLocus()) {
        id += geneRef.GetLocus();
        return id;
    }
    //3rd choice: generic suffix
    return id + xGetGenericSuffix(mf);
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForMrna(
    const CMappedFeat& mf,
    feature::CFeatTree* pFeatTree) 
//  ----------------------------------------------------------------------------
{
    string id("rna-");

    //1st choice: far accession
    auto farAccession = xExtractFarAccession(mf);
    if (!farAccession.empty()) {
        id += farAccession;
        return id;
    }

    //2nd choice: orig_transcript_id
    auto origTranscriptId = mf.GetNamedQual("orig_transcript_id");
    if (!origTranscriptId.empty()) {
        id += origTranscriptId;
        return id;
    }

    auto gene = feature::GetBestGeneForMrna(mf, pFeatTree);
    if (gene) {
        const auto& geneRef = gene.GetData().GetGene();
        //3rd choice: gene locustag
        if (geneRef.IsSetLocus_tag()) {
            id += geneRef.GetLocus_tag();
            return id;
        }
    
        //3rd choice: gene locus
        if (geneRef.IsSetLocus()) {
            id += geneRef.GetLocus();
            return id;
        }
    }
    //last choice: generic suffix
    return id + xGetGenericSuffix(mf);
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForCds(
    const CMappedFeat& mf,
    feature::CFeatTree* pFeatTree) 
    //  -----------------------------------------------------------------------------
{
    string id("cds-");

    //1st choice: far accession
    auto farAccession = xExtractFarAccession(mf);
    if (!farAccession.empty()) {
        id += farAccession;
        return id;
    }

    //2nd choice: orig_protein_id
    auto origTranscriptId = mf.GetNamedQual("orig_protein_id");
    if (!origTranscriptId.empty()) {
        id += origTranscriptId;
        return id;
    }

    auto gene = feature::GetBestGeneForCds(mf, pFeatTree);
    if (gene) {
        const auto& geneRef = gene.GetData().GetGene();
        //3rd choice: gene locustag
        if (geneRef.IsSetLocus_tag()) {
            id += geneRef.GetLocus_tag();
            return id;
        }

        //3rd choice: gene locus
        if (geneRef.IsSetLocus()) {
            id += geneRef.GetLocus();
            return id;
        }
    }
    //3rd choice: generic suffix
    return id + xGetGenericSuffix(mf);
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForNativeExon(
    const CMappedFeat& mf,
    feature::CFeatTree* pFeatTree) 
    //  -----------------------------------------------------------------------------
{
    const string commonPrefix("id-");
    string rawId;

    auto gene = feature::GetBestGeneForFeat(mf, pFeatTree);
    if (gene) {
        const auto& geneRef = gene.GetData().GetGene();
        //1st choice: gene locustag
        if (geneRef.IsSetLocus_tag()) {
            rawId = commonPrefix + geneRef.GetLocus_tag();
        }

        //2nd choice: gene locus
        if (rawId.empty()  &&  geneRef.IsSetLocus()) {
            rawId = commonPrefix + geneRef.GetLocus();
        }
    }
    //3rd choice: generic suffix
    if (rawId.empty()) {
        rawId = commonPrefix + xGetGenericSuffix(mf);
    }

    //important: attach exon number before returning !!!
    auto idIt = mLastUsedExonIds.find(rawId);
    if (idIt == mLastUsedExonIds.end()) {
        mLastUsedExonIds[rawId] = 1;
    }
    else {
        mLastUsedExonIds[rawId]++;
    }
    auto exonNumber = string("-") + NStr::NumericToString(mLastUsedExonIds[rawId]);
    return rawId + exonNumber;
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetGenericId(
    const CMappedFeat& mf,
    feature::CFeatTree* ) 
    //  -----------------------------------------------------------------------------
{
    string id("id-");

    //only choice: generic suffix
    return id + xGetGenericSuffix(mf);
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xGetGenericSuffix(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    //1st choice: CombinedFeatureUserObjects:TrackingID
    auto trackingId = xExtractTrackingId(mf);
    if (!trackingId.empty()) {
        return trackingId;
    }

    //2nd choice: local ID
    auto localId = xExtractLocalId(mf);
    if (!localId.empty()) {
        return localId;
    }

    //3rd choice: best ID:start..stop
    string suffix;
    if (!CWriteUtil::GetBestId(mf, suffix)) {
        suffix = "unknown";
    } 
    auto inPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetFrom());
    auto outPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetTo());
    suffix += ":";
    suffix += inPoint;
    suffix += "..";
    suffix += outPoint;
    return suffix;
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xExtractLocalId(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    ostringstream idStream;
    if (mf.IsSetId()) {
        auto& id = mf.GetId();
        if (id.IsLocal()) {
            id.GetLocal().AsString(idStream);
            return idStream.str();
        }
    }
    if (mf.IsSetIds()) {
        for (auto id: mf.GetIds()) {
            if (id->IsLocal()) {
                id->GetLocal().AsString(idStream);
                return idStream.str();
            }
        }
    }
    return "";
}

//  ------------------------------------------------------------------------------
string CGffIdGenerator::xExtractTrackingId(
    const CMappedFeat& mf)
//  ------------------------------------------------------------------------------
{
    CConstRef< CUser_object > cfob = mf.GetOriginalFeature().FindExt(
        "CombinedFeatureUserObjects");
    if (!cfob) {
        return "";
    }
    if (!cfob->HasField("TrackingId")) {
        return "";
    }
    auto trackingNumber = cfob->GetField("TrackingId").GetInt();
    return NStr::NumericToString(trackingNumber);
}

//  ------------------------------------------------------------------------------
string CGffIdGenerator::xExtractFarAccession(
    const CMappedFeat& mf)
//  ------------------------------------------------------------------------------
{
    const auto productIdHandle = mf.GetProductId();
    if (!productIdHandle) {
        return "";
    }
    auto bestIdHandle = sequence::GetId(
        productIdHandle, mf.GetScope(), sequence::eGetId_ForceAcc);
    if (!bestIdHandle) {
        return "";
    }
    return bestIdHandle.GetSeqId()->GetSeqIdString(true);
}

//  ------------------------------------------------------------------------------
string CGffIdGenerator::xDisambiguate(
    const string& baseId)
//  ------------------------------------------------------------------------------
{
    auto preExisting = mExistingIds.find(baseId);
    if (preExisting == mExistingIds.end()) {
        return baseId;
    }
    for (int suffix = 2; true; ++suffix) {
        auto disambiguated = baseId + "-" + NStr::NumericToString(suffix);
        preExisting = mExistingIds.find(disambiguated);
        if (preExisting == mExistingIds.end()) {
            return disambiguated;
        }
    }
    return baseId;
}

END_NCBI_SCOPE

