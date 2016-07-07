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
    ILineErrorListener* pMessageListener) :
    //  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(1),
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
            if (qualKey == "transcript_id") {
                if (!NStr::StartsWith((*qual)->GetVal(), "gnl|")) {
                    badQuals.push_back(qualKey);
                }
                continue;
            }
            if (qualKey == "protein_id") {
                if (!NStr::StartsWith((*qual)->GetVal(), "gnl|")) {
                    badQuals.push_back(qualKey);
                }
                continue;
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
void CFeatTableEdit::GenerateProteinAndTranscriptIds()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    for (CFeat_CI it(mHandle, sel); it; ++it){
        CMappedFeat mf = *it;
        xFeatureAddProteinId(mf);
        xFeatureAddTranscriptId(mf);
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
void CFeatTableEdit::xFeatureAddProteinId(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    //mss-375:
    // make sure we got a protein_id qualifier:
    //  if we already got one from the GFF then keep it
    //  otherwise if the parent/child got one then use that
    //  otherwise generate one based on the gene locustag

    string protein_id = mf.GetNamedQual("protein_id");
    if (!protein_id.empty()) {
        return;
    }
    CMappedFeat associateFeat;

    switch (mf.GetFeatSubtype()) {
        default:
            // we do this only for select feature types
            return;
        case CSeqFeatData::eSubtype_mRNA:
            associateFeat = feature::GetBestCdsForMrna(mf, &mTree);
            break;
        case CSeqFeatData::eSubtype_cdregion:
            associateFeat = feature::GetBestMrnaForCds(mf, &mTree);
            break;
    }
    if (associateFeat) {
        protein_id = associateFeat.GetNamedQual("protein_id");
    }
    if (protein_id.empty()) {
        protein_id = xNextProteinId(mf);
    }
    if (!protein_id.empty()) {
        xFeatureAddQualifier(mf, "protein_id", protein_id);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::xFeatureAddTranscriptId(
    CMappedFeat mf)
    //  ----------------------------------------------------------------------------
{
    //mss-375:
    // make sure we got a transcript_id qualifier:
    //  if we already got one from the GFF then keep it
    //  otherwise if the parent/child got one then use that
    //  otherwise generate one based on the gene locustag

    string transcript_id = mf.GetNamedQual("transcript_id");
    if (!transcript_id.empty()) {
        return;
    }
    CMappedFeat associateFeat;

    switch (mf.GetFeatSubtype()) {
    default:
        // we do this only for select feature types
        return;
    case CSeqFeatData::eSubtype_mRNA:
        associateFeat = feature::GetBestCdsForMrna(mf, &mTree);
        break;
    case CSeqFeatData::eSubtype_cdregion:
        associateFeat = feature::GetBestMrnaForCds(mf, &mTree);
        break;
    }
    if (associateFeat) {
        transcript_id = associateFeat.GetNamedQual("transcript_id");
    }
    if (transcript_id.empty()) {
        transcript_id = xNextTranscriptId(mf);
    }
    if (!transcript_id.empty()) {
        xFeatureAddQualifier(mf, "transcript_id", transcript_id);
    }
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
        // we will generate orig_protein_id and orig_transcript_id as needed.

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

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusTags()
//  ----------------------------------------------------------------------------
{
    if (mLocusTagPrefix.empty()) {
        return;
    }

	CRef<CGb_qual> pLocusTag;
    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for ( ; itGenes; ++itGenes) {
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            itGenes->GetOriginalFeature()));
        CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
        pEditedFeat->Assign(itGenes->GetOriginalFeature());
        pEditedFeat->RemoveQualifier("locus_tag");
        pEditedFeat->SetData().SetGene().SetLocus_tag(xNextLocusTag());
		feh.Replace(*pEditedFeat);
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
    CRef<CSeq_feat> pEditedFeat(new CSeq_feat);
    pEditedFeat->Assign(origFeat);
    CRef<CGb_qual> pQual(new CGb_qual);
    pQual->SetQual(qualKey);
    pQual->SetVal(qualVal);
    pEditedFeat->SetQual().push_back(pQual);
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

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

