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
#include <objtools/edit/feattable_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
    IMessageListener* pMessageListener):
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(1)
{
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
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(mAnnot);
    CSeq_annot_EditHandle saeh = scope.GetEditHandle(sah);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(sah, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, scope);
        if (!pRna) {
            continue;
        }
        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);
        //add rna xref to cds
        CSeq_feat_EditHandle feh(scope.GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);
        //add new rna to feature table
        saeh.AddFeat(*pRna);
    }
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(mAnnot);
    CSeq_annot_EditHandle saeh = scope.GetEditHandle(sah);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI it(sah, sel);
    for ( ; it; ++it) {
        const CSeq_feat& rna = it->GetOriginalFeature();
        CRef<CSeq_feat> pGene = xMakeGeneForMrna(rna, scope);
        const CSeq_loc& rnaLoc = rna.GetLocation();
        if (!pGene) {
            continue;
        }
        //find proper name for gene
        string geneId(xNextFeatId());
        pGene->SetId().SetLocal().SetStr(geneId);
        //add gene xref to rna
        CSeq_feat_EditHandle feh(scope.GetObjectHandle(rna));
        feh.AddFeatXref(geneId);
        //add new gene to feature table
        saeh.AddFeat(*pGene);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::InferPartials()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::EliminateBadQualifiers()
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual QUALS;

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(mAnnot);
    CSeq_annot_EditHandle saeh = scope.GetEditHandle(sah);

    CFeat_CI it(sah);
    for ( ; it; ++it) {
        CSeq_feat_EditHandle feh(scope.GetObjectHandle(
            (it)->GetOriginalFeature()));
        const QUALS& quals = (*it).GetQual();
        for (QUALS::const_iterator qual = quals.begin(); qual != quals.end(); 
                ++qual) {
            string qualVal = (*qual)->GetQual();
            CSeqFeatData::EQualifier qualType = CSeqFeatData::GetQualifierType(qualVal);
            if (qualType == CSeqFeatData::eQual_bad) {
                feh.RemoveQualifier(qualVal);
            }
        }
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
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForMrna(
    const CSeq_feat& rna,
    CScope& scope)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    CSeq_feat_Handle sfh = scope.GetSeq_featHandle(rna);
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CConstRef<CSeq_feat> pExistingGene;
    size_t bestLength(0);
    CFeat_CI findGene(sah, CSeqFeatData::eSubtype_gene);
    for ( ; findGene; ++findGene) {
        Int8 compare = sequence::TestForOverlap64(
            findGene->GetLocation(), rna.GetLocation(),
            sequence::eOverlap_CheckIntervals);
        if (compare == -1) {
            continue;
        }
        size_t currentLength = sequence::GetLength(findGene->GetLocation(), &scope);
        if (!bestLength  ||  currentLength > bestLength) {
            pExistingGene.Reset(&(findGene->GetOriginalFeature()));
            bestLength = currentLength;
        }
    }
    if (pExistingGene) {
        const CSeq_interval& geneLoc = pGene->GetLocation().GetInt();
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

