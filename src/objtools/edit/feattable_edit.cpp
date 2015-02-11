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

#include <objtools/edit/cds_fix.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/feattable_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
	const string& locusTagPrefix,
    IMessageListener* pMessageListener):
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
        const CSeq_feat& rna = it->GetOriginalFeature();
        CRef<CSeq_feat> pGene = xMakeGeneForMrna(rna);
        if (!pGene) {
            if (!rna.IsSetPartial() ||  !rna.GetPartial()) {
                continue;
            }
            CConstRef<CSeq_feat> pParentGene = xGetGeneParent(rna); 
            if (pParentGene  &&  
                    !(pParentGene->IsSetPartial()  &&  pParentGene->GetPartial())) {
                CRef<CSeq_feat> pEditedGene(new CSeq_feat);
                pEditedGene->Assign(*pParentGene);
                pEditedGene->SetPartial(true);
                CSeq_feat_EditHandle geneEh(mpScope->GetObjectHandle(*pParentGene));
                geneEh.Replace(*pEditedGene);
            }
            continue;
        }
        //find proper name for gene
        string geneId(xNextFeatId());
        pGene->SetId().SetLocal().SetStr(geneId);
        //add gene xref to rna
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(rna));
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
        CConstRef<CSeq_feat> pParentRna = xGetMrnaParent(cds);
        if (pParentRna  &&  
                !(pParentRna->IsSetPartial()  &&  pParentRna->GetPartial())) {
            CRef<CSeq_feat> pEditedRna(new CSeq_feat);
            pEditedRna->Assign(*pParentRna);
            pEditedRna->SetPartial(true);
            CSeq_feat_EditHandle rnaEh(mpScope->GetObjectHandle(*pParentRna));
            rnaEh.Replace(*pEditedRna);
        }

        // make sure the gene parent is partial as well
        CConstRef<CSeq_feat> pParentGene = xGetGeneParent(cds); 
        if (pParentGene  &&  
                !(pParentGene->IsSetPartial()  &&  pParentGene->GetPartial())) {
            CRef<CSeq_feat> pEditedGene(new CSeq_feat);
            pEditedGene->Assign(*pParentGene);
            pEditedGene->SetPartial(true);
            CSeq_feat_EditHandle geneEh(mpScope->GetObjectHandle(*pParentGene));
            geneEh.Replace(*pEditedGene);
        }
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
	// that's for cds's.
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
		string proteinId = xNextProteinId(cds);
		string transcriptId = xCurrentTranscriptId(cds);
		if (proteinId.empty()) {
			continue;
		}
        CRef<CSeq_feat> pEditedCds(new CSeq_feat);
        pEditedCds->Assign(cds);
		CRef<CGb_qual> pProteinId(new CGb_qual);
		pProteinId->SetQual("protein_id");
		pProteinId->SetVal(proteinId);
		pEditedCds->SetQual().push_back(pProteinId);
		CRef<CGb_qual> pTranscriptId(new CGb_qual);
		pTranscriptId->SetQual("transcript_id");
		pTranscriptId->SetVal(transcriptId);
		pEditedCds->SetQual().push_back(pTranscriptId);
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.Replace(*pEditedCds);
	}
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateOrigProteinAndOrigTranscriptIds()
//  ----------------------------------------------------------------------------
{
	// that's for cds's.
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
		string proteinId = cds.GetNamedQual("protein_id");
		string transcriptId = cds.GetNamedQual("transcript_id");
		CConstRef<CSeq_feat> pParentRna = xGetMrnaParent(cds);
		if (!pParentRna) {
			continue;
		}
        CRef<CSeq_feat> pEditedRna(new CSeq_feat);
        pEditedRna->Assign(*pParentRna);
		CRef<CGb_qual> pOrigProteinId(new CGb_qual);
		pOrigProteinId->SetQual("orig_protein_id");
		pOrigProteinId->SetVal(proteinId);
		pEditedRna->SetQual().push_back(pOrigProteinId);
		CRef<CGb_qual> pOrigTranscriptId(new CGb_qual);
		pOrigTranscriptId->SetQual("orig_transcript_id");
		pOrigTranscriptId->SetVal(transcriptId);
		pEditedRna->SetQual().push_back(pOrigTranscriptId);
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(*pParentRna));
        feh.Replace(*pEditedRna);
	}
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusTags()
//  ----------------------------------------------------------------------------
{
	CRef<CGb_qual> pLocusTag;

    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for ( ; itGenes; ++itGenes) {
		string locusTagVal = itGenes->GetNamedQual("locus_tag");
		if (!locusTagVal.empty()) {
			continue;
		}
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itGenes)->GetOriginalFeature()));
		feh.AddQualifier("locus_tag", xNextLocusTag());
	}
	SAnnotSelector selOther;
	selOther.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itOther(mHandle, selOther);

    // mss-315:
    //  only the genes get the locus_tags. they are inherited down
    //  to the features that live on them when we generate a flat file,
    //  but we don't want them here in the asn.1
	for ( ; itOther; ++itOther) {
        const CSeq_feat& feat = itOther->GetOriginalFeature();		
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itOther)->GetOriginalFeature()));
		feh.RemoveQualifier("locus_tag");
		CConstRef<CSeq_feat> pGeneParent = xGetGeneParent(feat);
		if (!pGeneParent) {
			continue;
		}
		//string locusTag = pGeneParent->GetNamedQual("locus_tag");
		//feh.AddQualifier("locus_tag", locusTag);
	}
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
	const CSeq_feat& cds)
//	----------------------------------------------------------------------------
{
	// format: mLocusTagPrefix|<locus tag of gene>[_numeric disambiguation]
	CConstRef<CSeq_feat> pGene = xGetGeneParent(cds);
    if (!pGene) {
		return "";
	}
	string locusTag = pGene->GetNamedQual("locus_tag");
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
string CFeatTableEdit::xCurrentTranscriptId(
	const CSeq_feat& cds)
//	----------------------------------------------------------------------------
{
	// format: mLocusTagPrefix|mrna.<locus tag of gene>[_numeric disambiguation]
	CConstRef<CSeq_feat> pGene = xGetGeneParent(cds);
    if (!pGene) {
		return "";
	}
	string locusTag = pGene->GetNamedQual("locus_tag");
	string disAmbig = "";
	map<string, int>::iterator it = mMapProtIdCounts.find(locusTag);
	if (it != mMapProtIdCounts.end()  &&  mMapProtIdCounts[locusTag] != 0) {
		disAmbig = string("_") + NStr::IntToString(mMapProtIdCounts[locusTag]);
	}
	return (mLocusTagPrefix + "|mrna." + locusTag + disAmbig);
}

//	----------------------------------------------------------------------------
CConstRef<CSeq_feat> CFeatTableEdit::xGetGeneParent(
	const CSeq_feat& feat)
//	----------------------------------------------------------------------------
{
	CConstRef<CSeq_feat> pGene;
	CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(feat);
	CSeq_annot_Handle sah = sfh.GetAnnot();
	if (!sah) {
		return pGene;
	}

    size_t bestLength(0);
    CFeat_CI findGene(sah, CSeqFeatData::eSubtype_gene);
    for ( ; findGene; ++findGene) {
        Int8 compare = sequence::TestForOverlap64(
            findGene->GetLocation(), feat.GetLocation(),
            sequence::eOverlap_Contained);
        if (compare == -1) {
            continue;
        }
        size_t currentLength = sequence::GetLength(findGene->GetLocation(), mpScope);
        if (!bestLength  ||  currentLength > bestLength) {
            pGene.Reset(&(findGene->GetOriginalFeature()));
            bestLength = currentLength;
        }
    }
	return pGene;
}

//	----------------------------------------------------------------------------
CConstRef<CSeq_feat> CFeatTableEdit::xGetMrnaParent(
	const CSeq_feat& feat)
//	----------------------------------------------------------------------------
{
	CConstRef<CSeq_feat> pMrna;
	CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(feat);
	CSeq_annot_Handle sah = sfh.GetAnnot();
	if (!sah) {
		return pMrna;
	}

    size_t bestLength(0);
    CFeat_CI findGene(sah, CSeqFeatData::eSubtype_mRNA);
    for ( ; findGene; ++findGene) {
        Int8 compare = sequence::TestForOverlap64(
            findGene->GetLocation(), feat.GetLocation(),
            sequence::eOverlap_Contained);
        if (compare == -1) {
            continue;
        }
        size_t currentLength = sequence::GetLength(findGene->GetLocation(), mpScope);
        if (!bestLength  ||  currentLength > bestLength) {
            pMrna.Reset(&(findGene->GetOriginalFeature()));
            bestLength = currentLength;
        }
    }
	return pMrna;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForMrna(
    const CSeq_feat& rna)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(rna);
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CConstRef<CSeq_feat> pExistingGene = xGetGeneParent(rna);
    if (pExistingGene) {
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

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

