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
* Author: Frank Ludwig, NCBI
*
* File Description:
*   Convenience wrapper around some of the other feature processing finctions
*   in the xobjedit library.
*/

#include <ncbi_pch.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/feattable_edit.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> sProductFromString(
const string str)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pProduct(new CSeq_loc(CSeq_loc::e_Whole));
    CRef<CSeq_id> pId(new CSeq_id(CSeq_id::e_Local, str));
    pProduct->SetId(*pId);
    return pProduct;
}

//  ----------------------------------------------------------------------------
string sGetCdsProductName (
    const CSeq_feat& cds, 
    CScope& scope)
//  ----------------------------------------------------------------------------
{
    string productName;
    if (cds.IsSetProduct()) {
        CBioseq_Handle bsh = sequence::GetBioseqFromSeqLoc(cds.GetProduct(), scope);
        if (bsh) {
            CFeat_CI protCi(bsh, CSeqFeatData::e_Prot);
            if (protCi) {
                const CProt_ref& prot = protCi->GetOriginalFeature().GetData().GetProt();
                if (prot.IsSetName() && prot.GetName().size() > 0) {
                    productName = prot.GetName().front();
                }
            }
            return productName;
        }  
    } 
    if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if (xref.IsSetData() && xref.GetData().IsProt()) {
                const CProt_ref& prot = xref.GetData().GetProt();
                if (prot.IsSetName() && prot.GetName().size() > 0) {
                    productName = prot.GetName().front();
                }
            }
            return productName;
        }
    }
    return productName;
}

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
    const string& locusTagPrefix,
    unsigned int locusTagNumber,
    unsigned int startingFeatId,
    ILineErrorListener* pMessageListener) :
    //  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(startingFeatId),
    mLocusTagNumber(locusTagNumber),
    mLocusTagPrefix(locusTagPrefix)
{
    mpScope.Reset(new CScope(*CObjectManager::GetInstance()));
    mpScope->AddDefaults();
    mHandle = mpScope->AddSeq_annot(mAnnot);
    mEditHandle = mpScope->GetEditHandle(mHandle);
    mTree = feature::CFeatTree(mHandle);
};


//  -------------------------------------------------------------------------
CFeatTableEdit::~CFeatTableEdit()
//  -------------------------------------------------------------------------
{
};

//  -------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingGeneForCds()
//  -------------------------------------------------------------------------
{
    xGenerateMissingGeneForSubtype(CSeqFeatData::eSubtype_cdregion);
}

//  -------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingMrnaForCds()
//  -------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CConstRef<CSeq_feat> pOverlappingRna =
            sequence::GetBestOverlappingFeat(
                cds.GetLocation(),
                CSeqFeatData::e_Rna,
                sequence::eOverlap_CheckIntRev,
                *mpScope);
        if (pOverlappingRna) {
            continue;
        }
        CRef<CSeq_feat> pRna(new CSeq_feat);
        pRna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        pRna->SetLocation().Assign(cds.GetLocation());
        pRna->SetLocation().SetPartialStart(false, eExtreme_Positional);
        pRna->SetLocation().SetPartialStop(false, eExtreme_Positional);
        pRna->ResetPartial();
        //product name
        pRna->SetData().SetRna().SetExt().SetName(
            sGetCdsProductName(cds, *mpScope));

        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);

        //add rna xref to cds
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);

        //add cds xref to rna
        CRef<CFeat_id> pFeatId(new CFeat_id);
        pFeatId->Assign(cds.GetId());
        CRef<CSeqFeatXref> pRnaXref(new CSeqFeatXref);
        pRnaXref->SetId(*pFeatId);
        pRna->SetXref().push_back(pRnaXref);

        CMappedFeat parentGene = feature::GetBestGeneForFeat(*it, &mTree);
        if (parentGene) {
            //if gene exists, add gene xref to rna
            CSeq_feat_EditHandle geh(mpScope->GetObjectHandle(
                parentGene.GetOriginalFeature()));
            geh.AddFeatXref(rnaId);
            //if gene exists, add rna xref to gene
            CRef<CFeat_id> pGeneId(new CFeat_id);
            pGeneId->Assign(parentGene.GetId());
            CRef<CSeqFeatXref> pRnaXref(new CSeqFeatXref);
            pRnaXref->SetId(*pGeneId);
            pRna->SetXref().push_back(pRnaXref);
        }

        //add new rna to feature table
        mEditHandle.AddFeat(*pRna);
        mTree.AddFeature(mpScope->GetObjectHandle(*pRna));
    }
}

//  -------------------------------------------------------------------------
void CFeatTableEdit::InferParentMrnas()
//  -------------------------------------------------------------------------
{
    GenerateMissingMrnaForCds();
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingGeneForMrna()
//  ---------------------------------------------------------------------------
{
    xGenerateMissingGeneForSubtype(CSeqFeatData::eSubtype_mRNA);
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    GenerateMissingGeneForMrna();
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::InferPartials()
//  ----------------------------------------------------------------------------
{
    bool infered(false);
    edit::CLocationEditPolicy editPolicy(
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd,
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd, 
        false, //extend 5'
        false, //extend 3'
        edit::CLocationEditPolicy::eMergePolicy_NoChange);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pEditedCds(new CSeq_feat);
        pEditedCds->Assign(cds);
        infered = editPolicy.ApplyPolicyToFeature(*pEditedCds, *mpScope);
        if (!infered) {
            continue;
        }   
        CSeq_feat_EditHandle cdsEh(mpScope->GetObjectHandle(cds));
        cdsEh.Replace(*pEditedCds);

        // make sure the parent rna is partial as well
        CMappedFeat parentRna = feature::GetBestMrnaForCds(*it, &mTree);
        if (parentRna  &&
            !(parentRna.IsSetPartial() && parentRna.GetPartial())) {
            CRef<CSeq_feat> pEditedRna(new CSeq_feat);
            pEditedRna->Assign(parentRna.GetOriginalFeature());
            pEditedRna->SetPartial(true);
            CSeq_feat_EditHandle rnaEh(
                mpScope->GetObjectHandle(parentRna.GetOriginalFeature()));
            rnaEh.Replace(*pEditedRna);
        }

        // make sure the gene parent is partial as well
        CMappedFeat parentGene = feature::GetBestGeneForCds(*it);
        if (parentGene  &&  
                !(parentGene.IsSetPartial()  &&  parentGene.GetPartial())) {
            CRef<CSeq_feat> pEditedGene(new CSeq_feat);
            pEditedGene->Assign(parentGene.GetOriginalFeature());
            pEditedGene->SetPartial(true);
            CSeq_feat_EditHandle geneEh(
                mpScope->GetObjectHandle(parentGene.GetOriginalFeature()));
            geneEh.Replace(*pEditedGene);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::SubmitFixProducts()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    //sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    for (CFeat_CI it(mHandle, sel); it; ++it){
        CMappedFeat mf = *it;
        if (mf.IsSetProduct()) {
            continue;
        }
        //debug CSeqFeatData::ESubtype st = mf.GetFeatSubtype();
        string product = mf.GetNamedQual("Product");
        CRef<CSeq_feat> pEditedFeature(new CSeq_feat);
        pEditedFeature->Assign(mf.GetOriginalFeature());
        pEditedFeature->ResetProduct();
        if (!product.empty()) {
            pEditedFeature->AddQualifier("product", product);
            pEditedFeature->RemoveQualifier("Product");
        }
        CSeq_feat_EditHandle feh(mf);
        feh.Replace(*pEditedFeature);
    }   
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::EliminateBadQualifiers()
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual QUALS;

    vector<string> specialQuals{
        "Protein", "protein",
        "go_function", "go_component", "go_process" };

    CFeat_CI it(mHandle);
    for ( ; it; ++it) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (it)->GetOriginalFeature()));
        const QUALS& quals = (*it).GetQual();
        vector<string> badQuals;
        for (QUALS::const_iterator qual = quals.begin(); qual != quals.end(); 
                ++qual) {
            string qualKey = (*qual)->GetQual();
            if (std::find(specialQuals.begin(), specialQuals.end(), qualKey) 
                    != specialQuals.end()) {
                continue;
            }
            if (subtype == CSeqFeatData::eSubtype_cdregion  ||  
                    subtype == CSeqFeatData::eSubtype_mRNA) {
                if (qualKey == "protein_id"  ||  qualKey == "transcript_id") {
                    continue;
                }
                if (qualKey == "orig_protein_id"  ||  qualKey == "orig_transcript_id") {
                    continue;
                }
            }
            CSeqFeatData::EQualifier qualType = CSeqFeatData::GetQualifierType(qualKey);
            if (CSeqFeatData::IsLegalQualifier(subtype, qualType)) {
                continue;
            }
            badQuals.push_back(qualKey);
        }
        for (vector<string>::const_iterator badIt = badQuals.begin();
                badIt != badQuals.end(); ++badIt) {
            feh.RemoveQualifier(*badIt);
        }
    }
}


//  ----------------------------------------------------------------------------
bool CFeatTableEdit::xGatherProteinAndTranscriptIds()
//  ----------------------------------------------------------------------------
{
    mMapCdsTranscriptId.clear();
    mMapCdsProteinId.clear();

    mMapMrnaTranscriptId.clear();
    mMapMrnaProteinId.clear();

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);

    for (CFeat_CI it(mHandle, sel); it; ++it) {
        CMappedFeat mf = *it;
        auto feat_subtype = mf.GetFeatSubtype();
        if (feat_subtype == CSeqFeatData::eSubtype_cdregion) {
            const string& protein_id = mf.GetNamedQual("protein_id");
            if (NStr::IsBlank(protein_id)) {
                const string& id = mf.GetNamedQual("ID");
                if (!NStr::IsBlank(id)) {
                    mMapCdsProteinId[mf] = id;
                }
            }
            else {
                mMapCdsProteinId[mf] = protein_id;
            }

            const string& transcript_id = mf.GetNamedQual("transcript_id");
            if (!NStr::IsBlank(transcript_id)) {
                mMapCdsTranscriptId[mf] = transcript_id;
            }
        }
        else { // CSeqFeatData::eSubtype_mRNA
            const string& transcript_id = mf.GetNamedQual("transcript_id");
            if (NStr::IsBlank(transcript_id)) {
                const string& id = mf.GetNamedQual("ID");
                if (!NStr::IsBlank(id)) {
                    mMapMrnaTranscriptId[mf] = id;
                }
            }
            else {
                mMapMrnaTranscriptId[mf] = transcript_id;
            }

            const string& protein_id = mf.GetNamedQual("protein_id");
            if (!NStr::IsBlank(protein_id)) {
                mMapMrnaProteinId[mf] = protein_id;
            }
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
void CFeatTableEdit::GenerateProteinAndTranscriptIds()
// ---------------------------------------------------------------------------
{
    xGatherProteinAndTranscriptIds();

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);

    for (CFeat_CI it(mHandle, sel); it; ++it) {
        CMappedFeat mf = *it;
        switch(mf.GetFeatSubtype()) {
        default:
            break;
        case CSeqFeatData::eSubtype_cdregion:
            xFeatureAddTranscriptAndProteinIdsCds(mf);
            break;
        case CSeqFeatData::eSubtype_mRNA:
            xFeatureAddTranscriptAndProteinIdsMrna(mf);
            break;
        }
    }

    mProcessedFeats.clear();
}

// ---------------------------------------------------------------------------
static string s_GetGeneralId(const string& locus_tag_prefix, const string& id)
// ---------------------------------------------------------------------------
{
    if (!NStr::IsBlank(id) &&
        !NStr::StartsWith(id, "gb|") &&
        !NStr::StartsWith(id, "gnl|") &&
        !NStr::StartsWith(id, "cds.gb|") &&
        !NStr::StartsWith(id, "cds.gnl|")) {
        return "gnl|" + locus_tag_prefix + "|" + id;
    }

    return id;
}


// ---------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptAndProteinIdsCds(CMappedFeat& cds)
// ---------------------------------------------------------------------------
{

    if (mProcessedFeats.find(cds) != mProcessedFeats.end()) {
        return;
    }

    string protein_id;
    if (mMapCdsProteinId.find(cds) != mMapCdsProteinId.end()) {
        protein_id = mMapCdsProteinId[cds];
    }

    string transcript_id;
    if (mMapCdsTranscriptId.find(cds) != mMapCdsTranscriptId.end()) {
        transcript_id = mMapCdsTranscriptId[cds];
    }

    if ((NStr::StartsWith(protein_id, "gb|") ||
         NStr::StartsWith(protein_id, "gnl")) &&
        (NStr::StartsWith(transcript_id, "gb|") ||
         NStr::StartsWith(transcript_id, "gnl|"))) {
        return;
    } 

    // Need to consider the case where the transcript_id is type general 
    // and the protein_id is not specified, and vice-versa.
    
    if (!NStr::IsBlank(protein_id) && 
        !NStr::IsBlank(transcript_id)) {

        if (transcript_id == protein_id) { // Scenario 2
            protein_id = "cds." + protein_id;
        }
        // else Scenario 1
    } 
    else {
        CMappedFeat mrna = feature::GetBestMrnaForCds(cds, &mTree);
        if (!NStr::IsBlank(protein_id)) {
            if (mrna) {
                auto it = mMapMrnaTranscriptId.find(mrna);
                if (it != mMapMrnaTranscriptId.end()) {
                    transcript_id = it->second; // Scenario 5
                    if (transcript_id == protein_id) {
                        protein_id = "cds." + protein_id; // Note add cds. to protein_id to distinguish
                    }
                }
            }
            if (NStr::IsBlank(transcript_id)) {
                transcript_id = "mrna." + protein_id; // Scenario 6
            } 
        }
        else if (!NStr::IsBlank(transcript_id)) {
            if (mrna) {
                auto it = mMapMrnaProteinId.find(mrna);
                if (it != mMapMrnaProteinId.end()) {
                    protein_id = it->second; // Scenario 3
                    if (protein_id == transcript_id) {
                        protein_id = "cds." + protein_id;
                    }
                }
            }
            if (NStr::IsBlank(protein_id)) {
                protein_id = "cds." + transcript_id; // Scenario 4
            }
        }
        else { // both transcript_id and protein_id are blank
            bool ids_found = false;
            if (mrna) {
                protein_id = mMapMrnaProteinId[mrna];          // Scenario 7 (if protein_id and transcript_id
                transcript_id = mMapMrnaTranscriptId[mrna];    // are not blank)
                if (NStr::IsBlank(transcript_id)) {
                    if (!NStr::IsBlank(protein_id)) {          // Scenario 10
                        transcript_id = "mrna." + protein_id;
                        ids_found = true;
                    }
                }
                else // transcript_id not blank
                {
                    ids_found = true;
                    if (NStr::IsBlank(protein_id) ||          // Scenario 9 
                        protein_id == transcript_id) {
                        protein_id = "cds." + transcript_id;
                    }
                }
            }
            if (!ids_found) {
                protein_id = xNextProteinId(cds);
                transcript_id = xNextTranscriptId(cds);
                xFeatureSetQualifier(cds, "protein_id", protein_id);
                xFeatureSetQualifier(cds, "transcript_id", transcript_id);
                // generate ids from locus_tag - if possible, should place ids on both features
                if (mrna) {
                    xFeatureSetQualifier(mrna, "protein_id", protein_id);
                    xFeatureSetQualifier(mrna, "transcript_id", transcript_id);

                    mMapMrnaProteinId[mrna] = protein_id;
                    mMapMrnaTranscriptId[mrna] = transcript_id;
                }
                return;
            }
        }
    }

    auto locus_tag_prefix  =  xGetCurrentLocusTagPrefix(cds);
    protein_id    = s_GetGeneralId(locus_tag_prefix, protein_id);
    transcript_id = s_GetGeneralId(locus_tag_prefix, transcript_id);

    xFeatureSetQualifier(cds, "protein_id", protein_id);
    xFeatureSetQualifier(cds, "transcript_id", transcript_id);
}

//
//
void CFeatTableEdit::xFeatureAddTranscriptAndProteinIdsCds(const string& mrna_transcript_id, 
                                                           const string& mrna_protein_id, 
                                                           CMappedFeat& cds)
{
    string transcript_id;
    {
        auto it = mMapCdsTranscriptId.find(cds);
        transcript_id = (it != mMapCdsTranscriptId.end()) ? it->second : mrna_transcript_id;
    }

    string protein_id;
    {
        auto it = mMapCdsProteinId.find(cds);
        protein_id = (it != mMapCdsProteinId.end()) ? it->second : mrna_protein_id;
    } 
   
    if (protein_id == transcript_id) {
        protein_id = "cds." + protein_id;
    }


    auto locus_tag_prefix  =  xGetCurrentLocusTagPrefix(cds);
    protein_id    = s_GetGeneralId(locus_tag_prefix, protein_id);
    transcript_id = s_GetGeneralId(locus_tag_prefix, transcript_id);

    xFeatureSetQualifier(cds, "protein_id", protein_id);
    xFeatureSetQualifier(cds, "transcript_id", transcript_id);

    mProcessedFeats.insert(cds);
}



// ---------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptAndProteinIdsMrna(CMappedFeat& mrna)
// ---------------------------------------------------------------------------
{
    string protein_id;
    if (mMapMrnaProteinId.find(mrna) != mMapMrnaProteinId.end()) {
        protein_id = mMapMrnaProteinId[mrna];
    }
    string transcript_id;
    if (mMapMrnaTranscriptId.find(mrna) != mMapMrnaTranscriptId.end()) {
        transcript_id = mMapMrnaTranscriptId[mrna];
    }
    

    if ((NStr::StartsWith(protein_id, "gb|") ||
         NStr::StartsWith(protein_id, "gnl")) &&
        (NStr::StartsWith(transcript_id, "gb|") ||
         NStr::StartsWith(transcript_id, "gnl|"))) {
        return;
    } 


    // Need to consider the case where the transcript_id is type general 
    // and the protein_id is not specified, and vice-versa.
    
    if (!NStr::IsBlank(protein_id) && 
        !NStr::IsBlank(transcript_id)) {

        if (transcript_id == protein_id) { // Scenario 2
            protein_id = "cds." + protein_id;
        }
        // else Scenario 1
    } 
    else {
        CMappedFeat cds = feature::GetBestCdsForMrna(mrna, &mTree);
        if (!NStr::IsBlank(protein_id)) {
            if (cds) {
                auto it = mMapCdsTranscriptId.find(cds);
                if (it != mMapCdsTranscriptId.end()) {
                    transcript_id = it->second; // Scenario 5
                    if (transcript_id == protein_id) {
                        protein_id = "cds." + protein_id; // Note add cds. to protein_id to distinguish
                    }   
                }
            }
            if (NStr::IsBlank(transcript_id)) {
                transcript_id = "mrna." + protein_id; // Scenario 6
            } 
        }
        else if (!NStr::IsBlank(transcript_id)) {
            if (cds) {
                auto it = mMapCdsProteinId.find(cds);
                if (it != mMapCdsProteinId.end()) {
                    protein_id = it->second; // Scenario 3
                    if (protein_id == transcript_id) {
                        protein_id = "cds." + protein_id;
                    }
                }
            }
            if (NStr::IsBlank(protein_id)) {
                protein_id = "cds." + transcript_id; // Scenario 4
            }
        }
        else { // both transcript_id and protein_id are blank
            bool ids_found = false;
            if (cds) {
                auto protein_it = mMapCdsProteinId.find(cds);
                if (protein_it != mMapCdsProteinId.end()) {
                    protein_id = protein_it->second;          // Scenario 7 (if protein_id and transcript_id
                }
                auto transcript_it = mMapCdsTranscriptId.find(cds);
                if (transcript_it != mMapCdsTranscriptId.end()) {
                    transcript_id = transcript_it->second;    // are not blank)
                }
                if (NStr::IsBlank(transcript_id)) {
                    if (!NStr::IsBlank(protein_id)) {          // Scenario 10
                        transcript_id = "mrna." + protein_id;
                        ids_found = true;
                    }
                }
                else // transcript_id not blank
                {
                    ids_found = true;
                    if (NStr::IsBlank(protein_id) ||          // Scenario 9 
                        protein_id == transcript_id) {
                        protein_id = "cds." + transcript_id;
                    }
                }
            }
            if (!ids_found) {
                protein_id = xNextProteinId(mrna);
                transcript_id = xNextTranscriptId(mrna);

                if (!NStr::IsBlank(protein_id) &&
                    !NStr::IsBlank(transcript_id)) {
            /*
                    if (cds) {
                        xFeatureSetQualifier(cds, "protein_id", protein_id);
                        xFeatureSetQualifier(cds, "transcript_id", transcript_id);
                        mMapCdsProteinId[cds] = protein_id;
                        mMapCdsTranscriptId[cds] = transcript_id;
                    }
                    */
                    xFeatureSetQualifier(mrna, "protein_id", protein_id);
                    xFeatureSetQualifier(mrna, "transcript_id", transcript_id);
                }
            }
        }
        if (cds) {
          xFeatureAddTranscriptAndProteinIdsCds(transcript_id,  
                                                protein_id,
                                                cds);
        }
    }


    auto locus_tag_prefix  =  xGetCurrentLocusTagPrefix(mrna);
    protein_id    = s_GetGeneralId(locus_tag_prefix, protein_id);
    transcript_id = s_GetGeneralId(locus_tag_prefix, transcript_id);

    xFeatureSetQualifier(mrna, "protein_id", protein_id);
    xFeatureSetQualifier(mrna, "transcript_id", transcript_id);

    mProcessedFeats.insert(mrna);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::InstantiateProducts()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel1;
    sel1.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel1.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    for (CFeat_CI it(mHandle, sel1); it; ++it) {
        CMappedFeat mf = *it;

        auto tid = mf.GetNamedQual("transcript_id");
        if (!tid.empty()) {
            xFeatureRemoveQualifier(mf, "transcript_id");
            xFeatureAddQualifier(mf, "orig_transcript_id", tid);
        }

        if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion) {
            auto pid = mf.GetNamedQual("protein_id");
            if (!pid.empty()) {
                if (!mf.IsSetProduct()) {
                    xFeatureSetProduct(mf, pid);
                }
                xFeatureRemoveQualifier(mf, "protein_id");
            }
        }
    }
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::xGenerateMissingGeneForChoice(
    CSeqFeatData::E_Choice choice)
//  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatType(choice);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        CMappedFeat mf = *it;
        if (xCreateMissingParentGene(mf)) {
            xAdjustExistingParentGene(mf);
        }
    }
}


//  ---------------------------------------------------------------------------
void CFeatTableEdit::xGenerateMissingGeneForSubtype(
    CSeqFeatData::ESubtype subType)
//  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(subType);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        CMappedFeat mf = *it;
        if (xCreateMissingParentGene(mf)) {
            xAdjustExistingParentGene(mf);
        }
    }
}

//  ----------------------------------------------------------------------------
bool
CFeatTableEdit::xAdjustExistingParentGene(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetPartial()  ||  !mf.GetPartial()) {
        return true;
    }
    CMappedFeat parentGene = feature::GetBestGeneForFeat(mf, &mTree);
    if (!parentGene) {
        return false;
    }

    if (parentGene.IsSetPartial()  &&  parentGene.GetPartial()) {
        return true;
    }
    CRef<CSeq_feat> pEditedGene(new CSeq_feat);
    pEditedGene->Assign(parentGene.GetOriginalFeature());
    pEditedGene->SetPartial(true);
    CSeq_feat_EditHandle geneEH(
        mpScope->GetObjectHandle(parentGene.GetOriginalFeature()));
    geneEH.Replace(*pEditedGene);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CFeatTableEdit::xCreateMissingParentGene(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene = xMakeGeneForFeature(mf);
    if (!pGene) {
        return false;
    }
    // missing gene was created. now attach ids and xrefs:
    string geneId(xNextFeatId());
    pGene->SetId().SetLocal().SetStr(geneId);
    CSeq_feat_EditHandle feh(
        mpScope->GetObjectHandle(mf.GetOriginalFeature()));
    feh.AddFeatXref(geneId);

    CRef<CFeat_id> pFeatId(new CFeat_id);
    pFeatId->Assign(mf.GetId());
    CRef<CSeqFeatXref> pGeneXref(new CSeqFeatXref);
    pGeneXref->SetId(*pFeatId);
    pGene->SetXref().push_back(pGeneXref);

    mEditHandle.AddFeat(*pGene);
    mTree.AddFeature(mpScope->GetObjectHandle(*pGene));
    return true;
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdMrna(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //rw-451 rules for mRNA:
    // no mRNA survives with an orig_protein_id
    // almost all mRNA features should have protein_ids
    // if one exists already then police it
    // if none exists then if possible inherit it from the decendent CDS

    auto orig_tid = mf.GetNamedQual("orig_protein_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_protein_id");
    }
    auto pid = mf.GetNamedQual("protein_id");
    if (NStr::StartsWith(pid, "gb|")  ||  NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    CMappedFeat child = feature::GetBestCdsForMrna(mf, &mTree);
    if (!child) {
        // only permitted case of an mRNA without a protein_id
        return;
    }
    pid = child.GetNamedQual("protein_id");
    xFeatureAddQualifier(mf, "protein_id", pid);
    return;
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdCds(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //rw-451 rules for CDS:
    // no CDS survives with an orig_protein_id
    // all CDS features should have transcript_ids
    // if one exists already then police it
    // if it doen't have one then generate one following a strict set of rules


    auto orig_pid = mf.GetNamedQual("orig_protein_id");
    if (!orig_pid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_protein_id");
    }

    auto pid = mf.GetNamedQual("protein_id");
    if (NStr::StartsWith(pid, "gb|")  ||  NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    auto id = mf.GetNamedQual("ID");
    if (!id.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + id;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    auto tid = mf.GetNamedQual("transcript_id");
    if (!tid.empty()) {
        CMappedFeat parent = feature::GetBestMrnaForCds(mf, &mTree);
        auto mRnaTid = parent.GetNamedQual("transcript_id");
        if (tid == mRnaTid) {
            tid = string("cds.") + tid;
        }
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    pid = xNextProteinId(mf);
    if (!pid.empty()) {
        xFeatureSetQualifier(mf, "protein_id", pid);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddProteinIdDefault(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //rw-451 rules for non CDS, non mRNA:
    // we won't touch orig_protein_id
    // we don't generate any protein_ids
    // if it comes with a protein_id then we add it to the "gnl|locus_tag|"
    //  namespace if necessary.

    auto pid = mf.GetNamedQual("protein_id");
    if (pid.empty()) {
        return;
    }
    if (NStr::StartsWith(pid, "gb|")  ||  NStr::StartsWith(pid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!pid.empty()) {
        pid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + pid;
        xFeatureSetQualifier(mf, "protein_id", pid);
        return;
    }

    //pid = xGenerateTranscriptOrProteinId(mf, pid);
    pid = xNextProteinId(mf);
    if (!pid.empty()) {
        xFeatureSetQualifier(mf, "protein_id", pid);
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdMrna(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for mRNA:
    // no mRNA survives with an orig_transcript_id
    // every mRNA must have a transcript_id.
    // if it already got one then police it.
    // if it doen't have one then generate one following a strict set of rules

    auto orig_tid = mf.GetNamedQual("orig_transcript_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_transcript_id");
    }

    auto tid = mf.GetNamedQual("transcript_id");
    if (NStr::StartsWith(tid, "gb|")  ||  NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }
    else {
    }



    auto id = mf.GetNamedQual("ID");
    if (!id.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + id;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    //tid = xGenerateTranscriptOrProteinId(mf, tid);
    tid = xNextTranscriptId(mf);
    if (!tid.empty()) {
        xFeatureSetQualifier(mf, "transcript_id", tid);
    }
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdCds(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //rw-451 rules for CDS:
    // no CDS survives with an orig_transcript_id
    // almost all CDS features should have transcript_ids
    // if one exists already then police it
    // if none exists then if possible inherit it from the parent mRNA

    auto orig_tid = mf.GetNamedQual("orig_transcript_id");
    if (!orig_tid.empty()) {
        xFeatureRemoveQualifier(mf, "orig_transcript_id");
    }

    auto tid = mf.GetNamedQual("transcript_id");
    // otherwise, we need to police the existing transcript_id:
    if (NStr::StartsWith(tid, "gb|")  ||  NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    //reformat any tags we already have:
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    CMappedFeat parent = feature::GetBestMrnaForCds(mf, &mTree);
    if (!parent) {
        // only permitted case of a CDS without a transcript_id
        return;
    }
    tid = parent.GetNamedQual("transcript_id");
    xFeatureAddQualifier(mf, "transcript_id", tid);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptIdDefault(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //rw-451 rules for non CDS, non mRNA:
    // we won't touch orig_transcript_id
    // we don't generate any transcript_ids
    // if it comes with a transcript_id then we add it to the "gnl|locus_tag|"
    //  namespace if necessary.

    auto tid = mf.GetNamedQual("transcript_id");
    if (tid.empty()) {
        return;
    }
    if (NStr::StartsWith(tid, "gb|")  ||  NStr::StartsWith(tid, "gnl|")) {
        // already what we want
        return;
    }
    if (!tid.empty()) {
        tid = string("gnl|") + xGetCurrentLocusTagPrefix(mf) + "|" + tid;
        xFeatureSetQualifier(mf, "transcript_id", tid);
        return;
    }

    tid = this->xNextTranscriptId(mf);
    if (!tid.empty()) {
        xFeatureSetQualifier(mf, "transcript_id", tid);
    }
}


//  ----------------------------------------------------------------------------
string CFeatTableEdit::xGenerateTranscriptOrProteinId(
    CMappedFeat mf,
    const string& rawId)
//  ----------------------------------------------------------------------------
{
    //weed out badly formatted original tags:
     if (string::npos != rawId.find("|")) {
        xPutError(
            ILineError::eProblem_InvalidQualifier,
            "Feature " + xGetIdStr(mf) + 
                " does not have a usable transcript_ or protein_id.");
        return "";
    }

    //make sure we got the locus tag prefix necessary for tag generation:
    auto locusTagPrefix = xGetCurrentLocusTagPrefix(mf);
    if (locusTagPrefix.empty()) {
        xPutError(
            ILineError::eProblem_InvalidQualifier,
            "Cannot generate transcript_/protein_id for feature " + xGetIdStr(mf) +
            " without a locus tag.");
        return "";
    }

    //reformat any tags we already have:
    if (!rawId.empty()) {
        return string("gnl|") + locusTagPrefix + "|" + rawId;
    }
    
    //attempt to make transcript_id from protein_id, or protein_id from transcript_id:
    switch (mf.GetFeatSubtype()) {
        case CSeqFeatData::eSubtype_mRNA: {
                auto id = mf.GetNamedQual("protein_id");
                if (id.empty()) {
                    id = mf.GetNamedQual("ID");
                }
                if (id.empty()) {
                    if (mf.GetId().IsLocal()) {
                        id = mf.GetId().GetLocal().GetStr();
                    }
                }
                if (!id.empty()) {
                    return string("gnl|") + locusTagPrefix + "|mrna." + id;
                }
            }
            break;

        case CSeqFeatData::eSubtype_cdregion: {
                auto id = mf.GetNamedQual("transcript_id");
                if (id.empty()) {
                    id = mf.GetNamedQual("ID");
                }
                if (!id.empty()) {
                    return string("gnl|") + locusTagPrefix + "|cds." + id;
                }
            }
            break;

        default:
            break;
    }


    xPutError(
        ILineError::eProblem_InvalidQualifier,
        "Cannot generate transcript_/protein_id for feature " + xGetIdStr(mf) +
        " - insufficient context.");
    return "";
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusIds()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_imp);

    if (mLocusTagPrefix.empty()) {
        return xGenerateLocusIdsUseExisting();
    }
    else {
        return xGenerateLocusIdsRegenerate();
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xGenerateLocusIdsRegenerate()
//  ----------------------------------------------------------------------------
{
    //mss-362:
    // blow away any locus_tag, protein_id, transcript_id attributes and
    // regenerate from scratch using mLocusTagPrefix

    //make sure genes got proper locus tags
    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    selGenes.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    for (CFeat_CI it(mHandle, selGenes); it; ++it) {
        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);
        CRef<CSeq_feat> pReplacement(new CSeq_feat);
        pReplacement->Assign(*mf.GetSeq_feat());
        pReplacement->SetData().SetGene().SetLocus_tag(xNextLocusTag());
        feh.Replace(*pReplacement);
    }

    //make sure all locus related junk is removed and that rnas are
    // labeled properly
    SAnnotSelector selOthers;
    selOthers.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    for (CFeat_CI it(mHandle, selOthers); it; ++it) {
        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);

        feh.RemoveQualifier("orig_protein_id");
        feh.RemoveQualifier("orig_transcript_id");

        CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
        switch (subtype) {
            case CSeqFeatData::eSubtype_mRNA: {
                string proteinId = xNextProteinId(mf);
                feh.AddQualifier("orig_protein_id", proteinId);
                string transcriptId = xNextTranscriptId(mf);
                feh.AddQualifier("orig_transcript_id", transcriptId);
                break;
            }
            default: {
                break;
            }
        }
    }

    //finally, down inherit transcript ids from the mrna's to the cdregions
    SAnnotSelector selCds;
    selCds.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);

    for (CFeat_CI it(mHandle, selCds); it; ++it) {

        CMappedFeat mf = *it;
        CSeq_feat_EditHandle feh(mf);
        CMappedFeat rna = feature::GetBestMrnaForCds(mf);
        string transcriptId = rna.GetNamedQual("transcript_id");
        feh.AddQualifier("orig_transcript_id", transcriptId);
        string proteinId = rna.GetNamedQual("protein_id");
        feh.AddQualifier("orig_protein_id", proteinId);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xGenerateLocusIdsUseExisting()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_imp);

    for (CFeat_CI it(mHandle, sel); it; ++it) {
        //mss-362: every feature that needs them must come with a complete set
        // of locus_tag, protein_id, and transcript_id.

        CMappedFeat mf = *it;
        CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
        
        switch (subtype) {
            case CSeqFeatData::eSubtype_gene: {
                if (!mf.GetData().GetGene().IsSetLocus_tag()) {
                    xPutErrorMissingLocustag(mf);
                }
                break;
            }
            case CSeqFeatData::eSubtype_mRNA: {
                string transcriptId = mf.GetNamedQual("transcript_id");
                if (transcriptId.empty()) {
                    xPutErrorMissingTranscriptId(mf);
                }
                string proteinId = mf.GetNamedQual("protein_id");
                if (proteinId.empty()) {
                    xPutErrorMissingProteinId(mf);
                }
                break;
            }
            case CSeqFeatData::eSubtype_cdregion: {
                string transcriptId = mf.GetNamedQual("transcript_id");
                if (transcriptId.empty()) {
                    xPutErrorMissingTranscriptId(mf);
                }
                break;
            }
            default: {
                break;
            }
        }
    }
}

bool idAlpha(const CSeq_id_Handle& idh1, const CSeq_id_Handle idh2) {
    return (idh1.AsString() < idh2.AsString());
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusTags()
//  ----------------------------------------------------------------------------
{
    if (mLocusTagPrefix.empty()) {
        return;
    }

	CRef<CGb_qual> pLocusTag;
    SAnnotSelector selGenes;
    vector<CSeq_id_Handle> annotIds;
    selGenes.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for ( ; itGenes; ++itGenes) {
        CSeq_feat_Handle fh = *itGenes;
        CSeq_id_Handle idh = fh.GetLocationId();
        vector<CSeq_id_Handle>::const_iterator compIt;
        for ( compIt = annotIds.begin(); 
                compIt != annotIds.end(); 
                ++compIt) {
            if (*compIt == idh) {
                break;
            }
        }
        if (compIt == annotIds.end()) {
            annotIds.push_back(idh);
        }
    }
    std::sort(annotIds.begin(), annotIds.end(), idAlpha);

    for (vector<CSeq_id_Handle>::const_iterator idIt = annotIds.begin();
            idIt != annotIds.end();
            ++idIt) {
        CSeq_id_Handle curId = *idIt;

	    CRef<CGb_qual> pLocusTag;
        SAnnotSelector selGenes;
        selGenes.SetSortOrder(SAnnotSelector::eSortOrder_None);
        selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
        CFeat_CI itGenes(mHandle, selGenes);
        for ( ; itGenes; ++itGenes) {
            CSeq_feat_Handle fh = *itGenes;
            string id1 = fh.GetLocationId().AsString();
            string id2 = curId.AsString();
            if (fh.GetLocationId() != curId) {
                continue;
            }
            CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
                itGenes->GetOriginalFeature()));
            CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
            pEditedFeat->Assign(itGenes->GetOriginalFeature());
            pEditedFeat->RemoveQualifier("locus_tag");
            pEditedFeat->SetData().SetGene().SetLocus_tag(xNextLocusTag());
		    feh.Replace(*pEditedFeat);
        }
    }
	SAnnotSelector selOther;
	selOther.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itOther(mHandle, selOther);

    // mss-315:
    //  only the genes get the locus_tags. they are inherited down
    //  to the features that live on them when we generate a flat file,
    //  but we don't want them here in the ASN.1
	for ( ; itOther; ++itOther) {
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itOther)->GetOriginalFeature()));
		feh.RemoveQualifier("locus_tag");
	}
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeatures(
    bool forEukaryote)
//  ----------------------------------------------------------------------------
{
    if (forEukaryote) {
        GenerateMissingParentFeaturesForEukaryote();
    }
    else {
        GenerateMissingParentFeaturesForProkaryote();
    }
    mTree = feature::CFeatTree(mHandle);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeaturesForEukaryote()
//  ----------------------------------------------------------------------------
{
    GenerateMissingMrnaForCds();
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Rna);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateMissingParentFeaturesForProkaryote()
//  ----------------------------------------------------------------------------
{
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Cdregion);
    xGenerateMissingGeneForChoice(CSeqFeatData::e_Rna);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddQualifier(
    CMappedFeat mf,
    const string& qualKey,
    const string& qualVal)
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.AddQualifier(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureRemoveQualifier(
    CMappedFeat mf,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.RemoveQualifier(qualKey);
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureSetQualifier(
    CMappedFeat mf,
    const string& qualKey,
    const string& qualVal)
//  ----------------------------------------------------------------------------
{
    auto existing = mf.GetNamedQual(qualKey);
    if (!existing.empty()) {
        xFeatureRemoveQualifier(mf, qualKey);
    }
    xFeatureAddQualifier(mf, qualKey, qualVal);
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureSetProduct(
    CMappedFeat mf,
    const string& proteinIdStr)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> pProteinId( 
        new CSeq_id(proteinIdStr, 
            CSeq_id::fParse_ValidLocal|CSeq_id::fParse_PartialOK));

    const CSeq_feat& origFeat = mf.GetOriginalFeature();
    CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
    pEditedFeat->Assign(origFeat);
    pEditedFeat->SetProduct().SetWhole(*pProteinId);
    CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(origFeat));
    feh.Replace(*pEditedFeat);
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextFeatId()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mNextFeatId++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH-suffix.size()) + suffix;
    }
    string nextTag("auto");
    return nextTag + suffix;
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextLocusTag()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mLocusTagNumber++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH-suffix.size()) + suffix;
    }
    string nextTag = mLocusTagPrefix + "_" + suffix;
    return nextTag;
}

//	----------------------------------------------------------------------------
string CFeatTableEdit::xNextProteinId(
	const CMappedFeat& mf)
//	----------------------------------------------------------------------------
{
    const string dbPrefix("gnl|");

    // format: mLocusTagPrefix|<locus tag of gene>[_numeric disambiguation]
    CMappedFeat parentGene = feature::GetBestGeneForFeat(mf, &mTree);
    if (!parentGene) {
		return "";
	}
    if (!parentGene.GetData().GetGene().IsSetLocus_tag()) {
        return "";
    }
    string locusTag = parentGene.GetData().GetGene().GetLocus_tag();
	string disAmbig = "";
	map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
	if (it == mMapProtIdCounts.end()) {
		mMapProtIdCounts[locusTag] = 0;
	}
	else {
		++mMapProtIdCounts[locusTag];
		disAmbig = string("_") + NStr::IntToString(mMapProtIdCounts[locusTag]);
	}
    string db = mLocusTagPrefix;
    if (db.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        db = prefix;
    }
    string proteinId = dbPrefix + db + "|" + locusTag + disAmbig;
    return proteinId;
}

//	----------------------------------------------------------------------------
string CFeatTableEdit::xNextTranscriptId(
	const CMappedFeat& cds)
//	----------------------------------------------------------------------------
{
    const string dbPrefix("gnl|");

	// format: mLocusTagPrefix|mrna.<locus tag of gene>[_numeric disambiguation]
	CMappedFeat parentGene = feature::GetBestGeneForFeat(cds, &mTree);
    if (!parentGene) {
		return "";
	}
    if (!parentGene.GetData().GetGene().IsSetLocus_tag()) {
        return "";
    }
    string locusTag = parentGene.GetData().GetGene().GetLocus_tag();
    string disAmbig = "";
	map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
	if (it != mMapProtIdCounts.end()  &&  mMapProtIdCounts[locusTag] != 0) {
		disAmbig = string("_") + NStr::IntToString(mMapProtIdCounts[locusTag]);
	}
    string db = mLocusTagPrefix;
    if (db.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        db = prefix;
    }
    string transcriptId = dbPrefix + db + "|mrna." + locusTag + disAmbig;
	return transcriptId;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForFeature(
    const CMappedFeat& rna)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    //const CSeq_loc& loc = rna.GetOriginalFeature().GetLocation();
    CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(rna.GetOriginalFeature());
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CMappedFeat existingGene = feature::GetBestGeneForFeat(rna, &mTree);
    if (existingGene) {
        return pGene;
    }
    pGene.Reset(new CSeq_feat);
    pGene->SetLocation().SetInt();
    pGene->SetLocation().SetId(*rna.GetLocation().GetId());
    pGene->SetLocation().SetInt().SetFrom(rna.GetLocation().GetStart(
        eExtreme_Positional));
    pGene->SetLocation().SetInt().SetTo(rna.GetLocation().GetStop(
        eExtreme_Positional));
    pGene->SetLocation().SetInt().SetStrand(rna.GetLocation().GetStrand());
    pGene->SetData().SetGene();
    return pGene;
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutError(
    ILineError::EProblem problem,
    const string& message)
//  ----------------------------------------------------------------------------
{
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Error,
        0,
        message,
        problem));
    pErr->SetLineNumber(0);
    mpMessageListener->PutError(*pErr);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingLocustag(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Error,
        0,
        subName + " feature is missing locus tag.",
        ILineError::eProblem_Missing));
    pErr->SetLineNumber(0);
    mpMessageListener->PutError(*pErr);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingTranscriptId(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Error,
        0,
        subName + " feature is missing transcript ID.",
        ILineError::eProblem_Missing));
    pErr->SetLineNumber(0);
    mpMessageListener->PutError(*pErr);
}

//  ----------------------------------------------------------------------------
void
CFeatTableEdit::xPutErrorMissingProteinId(
CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mpMessageListener) {
        return;
    }

    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    string subName = CSeqFeatData::SubtypeValueToName(subtype);
    unsigned int lower = mf.GetLocation().GetStart(eExtreme_Positional);
    unsigned int upper = mf.GetLocation().GetStop(eExtreme_Positional);
    subName = NStr::IntToString(lower) + ".." + NStr::IntToString(upper) + " " +
        subName;

    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Error,
        0,
        subName + " feature is missing protein ID.",
        ILineError::eProblem_Missing));
    pErr->SetLineNumber(0);
    mpMessageListener->PutError(*pErr);
}

//  ----------------------------------------------------------------------------
string
CFeatTableEdit::xGetIdStr(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    stringstream strstr;
    auto& id = mf.GetId();
    switch (id.Which()) {
    default:
        return "\"UNKNOWN ID\"";
    case CFeat_id::e_Local:
        id.GetLocal().AsString(strstr);
        return strstr.str();
    }
}

//  ----------------------------------------------------------------------------
string
CFeatTableEdit::xGetCurrentLocusTagPrefix(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    if (!mLocusTagPrefix.empty()) {
        return mLocusTagPrefix;
    }
    CMappedFeat geneFeature = mf;
    if (geneFeature.GetFeatSubtype() != CSeqFeatData::eSubtype_gene) {
        geneFeature = feature::GetBestGeneForFeat(mf, &mTree);
    }
    if (!geneFeature) {
        return "";
    }
	const auto& geneRef = geneFeature.GetData().GetGene();
    if (geneRef.IsSetLocus_tag()) {
        const auto& locusTag = geneFeature.GetData().GetGene().GetLocus_tag();
        string prefix, suffix;
        NStr::SplitInTwo(locusTag, "_", prefix, suffix);
        return prefix;
    }
    auto locusTagFromQualifier = geneFeature.GetNamedQual("locus_tag");
    if (!locusTagFromQualifier.empty()) {
        string prefix, suffix;
        NStr::SplitInTwo(locusTagFromQualifier, "_", prefix, suffix);
        return prefix;
    }
    return "";
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

