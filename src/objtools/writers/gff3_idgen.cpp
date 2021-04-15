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

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>

#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3_idgen.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

#include <array>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ------------------------------------------------------------------------------
string CGffIdGenerator::GetGffId(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
//  -----------------------------------------------------------------------------t-
{
    auto id = mf.GetNamedQual("ID");
    if (id.empty()) {
        auto subType = mf.GetFeatSubtype();
        switch(subType) {
        default:
            if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                id = xGetIdForRna(mf, fc);
                break;
            }
            id = xGetGenericId(mf, fc);
            break;
        case CSeqFeatData::eSubtype_gene:
            id = xGetIdForGene(mf, fc);
            break;
        case CSeqFeatData::eSubtype_cdregion:
            id = xGetIdForCds(mf,fc);
            break;
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
string CGffIdGenerator::GetGffSourceId(
    CBioseq_Handle bsh)
//  -----------------------------------------------------------------------------
{
    string locationId("unknown");

    string bestId;
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if (pId) {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*pId);
        if (CGenbankIdResolve::Get().GetBestId(idh, bsh.GetScope(), bestId)) {
            locationId = bestId;
        }
    }
    else {
        auto ids = bsh.GetId();
        if (!ids.empty()) {
            auto id = ids.front();
            CGenbankIdResolve::Get().GetBestId(id, bsh.GetScope(), bestId);
            locationId = bestId;
        }
    }

    string seqStart = "1";//always for source
    string seqStop = NStr::NumericToString(bsh.GetBioseqLength());

    locationId += ":";
    locationId += seqStart;
    locationId += "..";
    locationId += seqStop;
    return locationId;
}
 
//  -----------------------------------------------------------------------------
void CGffIdGenerator::Reset()
//  -----------------------------------------------------------------------------
{
    mExistingIds.clear();
    mLastUsedExonIds.clear();
    mLastTrulyGenericSuffix = 0;
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
    CGffFeatureContext& fc) 
    //  -----------------------------------------------------------------------------
{
    const string commonPrefix("gene-");

    //try locus_tag or locus
    auto stem = xExtractGeneLocusTagOrLocus(mf);
    if (!stem.empty()) {
        return (commonPrefix + stem);
    }

    //fall back: generic stem
    return (commonPrefix + xGetGenericSuffix(mf, fc));
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForRna(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
//  ----------------------------------------------------------------------------
{
    const string commonPrefix("rna-");

    //try to use far accession
    auto farAccession = xExtractFarAccession(mf);
    if (!farAccession.empty()) {
        return (commonPrefix + farAccession);
    }

    //try to use orig_transcript_id
    auto origTranscriptId = mf.GetNamedQual("orig_transcript_id");
    if (!origTranscriptId.empty()) {
        return (commonPrefix + origTranscriptId);
    }

    //try to inherit from gene
    auto subtype = mf.GetFeatSubtype();
    CMappedFeat gene;
    if (subtype == CSeqFeatData::eSubtype_mRNA) {
        gene = feature::GetBestGeneForMrna(mf, &fc.FeatTree());
    }
    else {
        gene = feature::GetBestGeneForFeat(mf, &fc.FeatTree());
    }
    auto stem = xExtractGeneLocusTagOrLocus(gene);
    if (!stem.empty()) {
        return (commonPrefix + stem);
    }

    //fall back: generic suffix
    return (commonPrefix + xGetGenericSuffix(mf, fc));
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForCds(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
//  -----------------------------------------------------------------------------
{
    const string commonPrefix("cds-");

    //try far accession
    auto farAccession = xExtractFarAccession(mf);
    if (!farAccession.empty()) {
        return (commonPrefix + farAccession);
    }

    //try orig_protein_id
    auto origTranscriptId = mf.GetNamedQual("orig_protein_id");
    if (!origTranscriptId.empty()) {
        return (commonPrefix + origTranscriptId);
    }

    //try to inherit from gene
    auto gene = feature::GetBestGeneForCds(mf, &fc.FeatTree());
    auto stem = xExtractGeneLocusTagOrLocus(gene);
    if (!stem.empty()) {
        return (commonPrefix + stem);
    }

    //last resort: generic suffix
    return (commonPrefix + xGetGenericSuffix(mf, fc));
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetGenericId(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
//  -----------------------------------------------------------------------------
{
    const string commonPrefix("id-");
    string rawId;

    //try to inherit from gene:
    auto stem = xExtractGeneLocusTagOrLocus(mf);
    if (!stem.empty()) {
        rawId = commonPrefix + stem;
    }

    //fall back: generic suffix
    if (rawId.empty()) {
        rawId = commonPrefix + xGetGenericSuffix(mf, fc);
    }

    //for native exons: attach exon number if available
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_exon) {
        auto exonNumber = mf.GetNamedQual("number");
        if (!exonNumber.empty()) {
            rawId += string("-") + exonNumber;
        }
    }
    return rawId;
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xGetGenericSuffix(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
//  ----------------------------------------------------------------------------
{
    const auto dbxrefs = mf.GetDbxref();
    for (const auto& ref: dbxrefs) {
        if (ref->GetDb() == "GeneID") {
            stringstream ostr;
            ostr << "GeneID:";
            ref->GetTag().AsString(ostr);
            return ostr.str();
        }
    }
    return xExtractFeatureLocation(mf, fc);
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xExtractGeneLocusTagOrLocus(
    const CMappedFeat& mf) 
//  -----------------------------------------------------------------------------
{
    if (!mf) {
        return "";
    }
    auto gene = mf;
    if (gene.GetFeatSubtype() != CSeqFeatData::eSubtype_gene) {
        gene = feature::GetBestGeneForFeat(mf);
    }
    if (!gene) {
        return "";
    }

    const auto& geneRef = gene.GetData().GetGene();
    if (geneRef.IsSetLocus_tag()) {
        return geneRef.GetLocus_tag();
    }
    if (geneRef.IsSetLocus()) {
        return geneRef.GetLocus();
    }
    return "";
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xExtractFeatureLocation(
    const CMappedFeat& mf,
    CGffFeatureContext& fc) 
    //  ----------------------------------------------------------------------------
{
    string locationId;
    if (!CGenbankIdResolve::Get().GetBestId(mf, locationId)) {
        locationId = "unknown";
    } 
    auto locationType = mf.GetLocation().Which();
    if (locationType == CSeq_loc::e_Whole) {
        auto bsh = fc.BioseqHandle();
        if (bsh  &&  bsh.CanGetInst_Length()) {
            locationId += ":1.." + NStr::NumericToString(bsh.GetInst_Length());
            return locationId;
        }
        locationId += ":whole"; //for lack of better ideas
        return locationId;
    }
    auto inPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetFrom() + 1);
    auto outPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetTo() + 1);
    locationId += ":";
    locationId += inPoint;
    locationId += "..";
    locationId += outPoint;
    return locationId;
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

