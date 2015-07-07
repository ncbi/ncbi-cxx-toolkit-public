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
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/edit/cds_fix.hpp>
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

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
	const string& locusTagPrefix,
    ILineErrorListener* pMessageListener):
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(1),
	mLocusTagNumber(1),
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
void CFeatTableEdit::InferParentMrnas()
//  -------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, *mpScope);
        if (!pRna) {
            continue;
        }
        pRna->SetLocation().SetPartialStart(false, eExtreme_Positional);
        pRna->SetLocation().SetPartialStop(false, eExtreme_Positional);
        pRna->ResetPartial();
        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);
        //add rna xref to cds
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);
        //add new rna to feature table
        mEditHandle.AddFeat(*pRna);
    }
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        CMappedFeat rna = *it;
        CRef<CSeq_feat> pGene = xMakeGeneForMrna(rna);
        if (!pGene) {
            if (!rna.IsSetPartial() ||  !rna.GetPartial()) {
                continue;
            }
            CMappedFeat parentGene = feature::GetBestGeneForMrna(rna); 
            if (parentGene  &&  
                    !(parentGene.IsSetPartial()  &&  parentGene.GetPartial())) {
                CRef<CSeq_feat> pEditedGene(new CSeq_feat);
                pEditedGene->Assign(parentGene.GetOriginalFeature());
                pEditedGene->SetPartial(true);
                CSeq_feat_EditHandle geneEh(
                    mpScope->GetObjectHandle(parentGene.GetOriginalFeature()));
                geneEh.Replace(*pEditedGene);
            }
            continue;
        }
        //find proper name for gene
        string geneId(xNextFeatId());
        pGene->SetId().SetLocal().SetStr(geneId);
        //add gene xref to rna
        CSeq_feat_EditHandle feh(
            mpScope->GetObjectHandle(rna.GetOriginalFeature()));
        feh.AddFeatXref(geneId);
        //add new gene to feature table
        mEditHandle.AddFeat(*pGene);
    }
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

    CFeat_CI it(mHandle);
    for ( ; it; ++it) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (it)->GetOriginalFeature()));
        const QUALS& quals = (*it).GetQual();
        vector<string> badQuals;
        for (QUALS::const_iterator qual = quals.begin(); qual != quals.end(); 
                ++qual) {
            string qualVal = (*qual)->GetQual();
            if (qualVal == "transcript_id") {
                continue;
            }
            if (qualVal == "protein_id") {
                continue;
            }
            if (qualVal == "Protein") {
                continue;
            }
            if (qualVal == "protein") {
                continue;
            }
            CSeqFeatData::EQualifier qualType = CSeqFeatData::GetQualifierType(qualVal);
            if (!CSeqFeatData::IsLegalQualifier(subtype, qualType)) {
                badQuals.push_back(qualVal);
            }
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

    CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
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
    xFeatureAddQualifier(mf, "protein_id", protein_id);
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

    CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
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
    xFeatureAddQualifier(mf, "transcript_id", transcript_id);
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
        const CSeq_feat& f = mf.GetOriginalFeature();
        CSeq_feat_EditHandle feh(mf);

        //feh.RemoveQualifier("locus_tag");
        //feh.RemoveQualifier("protein_id");
        feh.RemoveQualifier("orig_protein_id");
        //feh.RemoveQualifier("transcript_id");
        feh.RemoveQualifier("orig_transcript_id");

        CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
        switch (subtype) {
            case CSeqFeatData::eSubtype_mRNA: {
                string proteinId = xNextProteinId(mf);
                //feh.AddQualifier("protein_id", proteinId);
                feh.AddQualifier("orig_protein_id", proteinId);
                string transcriptId = xNextTranscriptId(mf);
                //feh.AddQualifier("transcript_id", transcriptId);
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
        const CSeq_feat& feat = itOther->GetOriginalFeature();		
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itOther)->GetOriginalFeature()));
		feh.RemoveQualifier("locus_tag");
	}
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
	// format: mLocusTagPrefix|<locus tag of gene>[_numeric disambiguation]
    CMappedFeat parentGene = feature::GetBestGeneForFeat(mf, &mTree);
    if (!parentGene) {
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
	return (mLocusTagPrefix + "|" + locusTag + disAmbig);
}

//	----------------------------------------------------------------------------
string CFeatTableEdit::xNextTranscriptId(
	const CMappedFeat& cds)
//	----------------------------------------------------------------------------
{
	// format: mLocusTagPrefix|mrna.<locus tag of gene>[_numeric disambiguation]
	CMappedFeat parentGene = feature::GetBestGeneForFeat(cds, &mTree);
    if (!parentGene) {
		return "";
	}
    string locusTag = parentGene.GetData().GetGene().GetLocus_tag();
    string disAmbig = "";
	map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
	if (it != mMapProtIdCounts.end()  &&  mMapProtIdCounts[locusTag] != 0) {
		disAmbig = string("_") + NStr::IntToString(mMapProtIdCounts[locusTag]);
	}
	return (mLocusTagPrefix + "|mrna." + locusTag + disAmbig);
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForMrna(
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
    CMappedFeat existingGene = feature::GetBestGeneForMrna(rna, &mTree);
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

