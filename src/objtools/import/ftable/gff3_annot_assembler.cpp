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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/import/import_error.hpp>
#include "feat_util.hpp"
#include "featid_generator.hpp"
#include "gff3_annot_assembler.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGff3AnnotAssembler::CGff3AnnotAssembler(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatAnnotAssembler(errorReporter)
{
}

//  ============================================================================
CGff3AnnotAssembler::~CGff3AnnotAssembler()
//  ============================================================================
{
}

//  ============================================================================
void
CGff3AnnotAssembler::ProcessRecord(
    const CFeatImportData& record_,
    CSeq_annot& annot)
//  ============================================================================
{
    assert(dynamic_cast<const CGff3ImportData*>(&record_));
    const CGff3ImportData& record = static_cast<const CGff3ImportData&>(record_);

    auto recordId = record.Id();
    auto parentId = record.Parent();
    auto pFeature = record.GetData();

    switch (pFeature->GetData().GetSubtype()) {
    default:
        return xProcessFeatureDefault(recordId, parentId, pFeature, annot);
    case CSeqFeatData::eSubtype_exon:
        return xProcessFeatureExon(recordId, parentId, pFeature, annot);
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_misc_RNA:
    case CSeqFeatData::eSubtype_ncRNA:
    case CSeqFeatData::eSubtype_rRNA:
    case CSeqFeatData::eSubtype_tmRNA:
    case CSeqFeatData::eSubtype_tRNA:
        return xProcessFeatureRna(recordId, parentId, pFeature, annot);
    case CSeqFeatData::eSubtype_cdregion:
        return xProcessFeatureCds(recordId, parentId, pFeature, annot);
    }
}

//  ============================================================================
void
CGff3AnnotAssembler::xProcessFeatureDefault(
    const std::string& recordId,
    const std::string& parentId,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot)
//  ============================================================================
{
    string featureType = CSeqFeatData::SubtypeValueToName(
        pFeature->GetData().GetSubtype());
    NStr::ToLower(featureType);
    pFeature->SetId(*mIdGenerator.GetIdFor(featureType));
    annot.SetData().SetFtable().push_back(pFeature);
    if (!recordId.empty()) {
        mFeatureMap.AddFeature(recordId, pFeature);
    }
}


//  ============================================================================
void
CGff3AnnotAssembler::xProcessFeatureCds(
    const std::string& recordId,
    const std::string& parentId,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot)
//  ============================================================================
{
    auto pExistingCds = mFeatureMap.FindFeature(recordId);
    if (pExistingCds) {
        // add new piece to existing piece
        CRef<CSeq_loc> pUpdatedLocation = FeatUtil::AddLocations(
            pExistingCds->GetLocation(), pFeature->GetLocation());
        pExistingCds->SetLocation().Assign(*pUpdatedLocation);

        // update frame if necessary
        auto cdsStrand = pExistingCds->GetLocation().GetStrand();
        auto& existingCds = pExistingCds->SetData().SetCdregion();
        const auto& newCds = pFeature->GetData().GetCdregion();
        if (cdsStrand == eNa_strand_plus) {
            auto existingStart = 
                pExistingCds->GetLocation().GetStart(eExtreme_Positional);
            auto contributedStart =
                pFeature->GetLocation().GetStart(eExtreme_Positional);
            if (existingStart == contributedStart) {
                existingCds.SetFrame(newCds.GetFrame());
            }
        }
        else if (cdsStrand == eNa_strand_minus) {
            auto existingStop = 
                pExistingCds->GetLocation().GetStart(eExtreme_Positional);
            auto contributedStop =
                pFeature->GetLocation().GetStart(eExtreme_Positional);
            if (existingStop == contributedStop) {
                existingCds.SetFrame(newCds.GetFrame());
            }
        }
    }
    else {
        pFeature->SetId(*mIdGenerator.GetIdFor("cds"));
        annot.SetData().SetFtable().push_back(pFeature);
        if (!recordId.empty()) {
            mFeatureMap.AddFeature(recordId, pFeature);
        }
        if (!recordId.empty()  &&  !parentId.empty()) {
            mXrefMap[recordId] = parentId;
        }
    }
}


//  ============================================================================
void
CGff3AnnotAssembler::xProcessFeatureRna(
    const std::string& recordId,
    const std::string& parentId,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot)
//  ============================================================================
{
    annot.SetData().SetFtable().push_back(pFeature);
    if (!recordId.empty()) {
        mFeatureMap.AddFeature(recordId, pFeature);
    }
    if (!recordId.empty()  &&  !parentId.empty()) {
        mXrefMap[recordId] = parentId;
    }

    pFeature->SetId(*mIdGenerator.GetIdFor("mrna"));
    xMarkLocationPending(*pFeature);

    vector<CRef<CSeq_feat>> pendingExons;
    if (!mPendingFeatures.FindPendingFeatures(recordId, pendingExons)) {
        return;
    }
    for (auto pExon: pendingExons) {
        CRef<CSeq_loc> pUpdatedLocation = FeatUtil::AddLocations(
            pFeature->GetLocation(), pExon->GetLocation());
        pFeature->SetLocation().Assign(*pUpdatedLocation);
    }
    mPendingFeatures.MarkFeaturesDone(recordId);
}


//  ============================================================================
void
CGff3AnnotAssembler::xProcessFeatureExon(
    const std::string& recordId,
    const std::string& parentId,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot)
//  ============================================================================
{
    auto pParentRna = mFeatureMap.FindFeature(parentId);
    if (pParentRna) {
        if (xIsLocationPending(*pParentRna)) {
            pParentRna->SetLocation().Assign(pFeature->GetLocation());
            xUnmarkLocationPending(*pParentRna);
        }
        else {
            CRef<CSeq_loc> pUpdatedLocation = FeatUtil::AddLocations(
                pParentRna->GetLocation(), pFeature->GetLocation());
            pParentRna->SetLocation().Assign(*pUpdatedLocation);
        }
    }
    else {
        mPendingFeatures.AddFeature(parentId, pFeature);
    }
}

//  ============================================================================
void
CGff3AnnotAssembler::FinalizeAnnot(
    const CAnnotImportData& annotData,
    CSeq_annot& annot)
//  ============================================================================
{
    // generate crefs between genes, mRNAs, and coding regions:
    for (auto entry: mXrefMap) {
        auto childId = entry.first;
        auto parentId = entry.second;

        auto pChild = mFeatureMap.FindFeature(childId);
        auto pParent = mFeatureMap.FindFeature(parentId);
        if (!pChild  ||  !pParent) {
            continue;
        }
        pChild->AddSeqFeatXref(pParent->GetId());
        pParent->AddSeqFeatXref(pChild->GetId());

        auto itGrandParent = mXrefMap.find(parentId);
        if (itGrandParent == mXrefMap.end()) {
            continue;
        }
        auto grandParentId = itGrandParent->second;
        auto pGrandParent = mFeatureMap.FindFeature(grandParentId);
        if (!pGrandParent) {
            continue;
        }
        pChild->AddSeqFeatXref(pGrandParent->GetId());
        pGrandParent->AddSeqFeatXref(pChild->GetId());
        pParent->AddSeqFeatXref(pGrandParent->GetId());
        pGrandParent->AddSeqFeatXref(pParent->GetId());
    }

    // remove any remaining "under construction" markers:
    auto& ftable = annot.SetData().SetFtable();
    for (auto& pFeature: ftable) {
        xUnmarkLocationPending(*pFeature);
    }
}

//  ============================================================================
bool
CGff3AnnotAssembler::xIsLocationPending(
    const CSeq_feat& feat)
//  ============================================================================
{
    if (!feat.IsSetQual()) {
        return false;
    }
    for (const auto& pQual: feat.GetQual()) {
        if (pQual->IsSetQual()  &&  pQual->GetQual() == "__location_pending") {
            return true;
        }
    }
    return false;
}

//  ============================================================================
void
CGff3AnnotAssembler::xMarkLocationPending(
    CSeq_feat& feat)
//  ============================================================================
{
    feat.AddQualifier("__location_pending", "true");
}

//  ============================================================================
void
CGff3AnnotAssembler::xUnmarkLocationPending(
    CSeq_feat& feat)
//  ============================================================================
{
    feat.RemoveQualifier("__location_pending");
}
