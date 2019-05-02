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

#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/import/import_error.hpp>
#include "featid_generator.hpp"
#include "bed_annot_assembler.hpp"
#include "bed_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CBedAnnotAssembler::CBedAnnotAssembler(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatAnnotAssembler(errorReporter)
{
    mpIdGenerator.reset(new CFeatureIdGenerator);
}

//  ============================================================================
CBedAnnotAssembler::~CBedAnnotAssembler()
//  ============================================================================
{
}

//  ============================================================================
void
CBedAnnotAssembler::ProcessRecord(
    const CFeatImportData& record_,
    CSeq_annot& annot)
//  ============================================================================
{
    assert(dynamic_cast<const CBedImportData*>(&record_));
    const CBedImportData& record = static_cast<const CBedImportData&>(record_);

    CRef<CSeq_feat> pGene(0), pMrna(0), pCds(0);

    const auto& geneLocation = record.ChromLocation();
    if (!geneLocation.IsNull()) {
        pGene.Reset(new CSeq_feat);
        //pGene->SetData().SetGene();
        pGene->SetData().SetRegion(record.Name());
        pGene->SetLocation().Assign(geneLocation);

        const auto& displayData = record.DisplayData(); 
        if (displayData.IsSetData()) {
            CRef<CUser_object> pDisplayData(new CUser_object);
            pDisplayData->Assign(displayData);
            pGene->SetExts().push_back(pDisplayData);
        }

        pGene->SetId(*mpIdGenerator->GetIdFor("gene"));
        annot.SetData().SetFtable().push_back(pGene);
    }

    const auto mRnaLocation = 
        record.BlocksLocation().Merge(CSeq_loc::fMerge_All, nullptr);
    if (!mRnaLocation->IsNull()) {
        pMrna.Reset(new CSeq_feat);
        //pMrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        pMrna->SetData().SetRegion(record.Name());
        pMrna->SetLocation(). Assign(*mRnaLocation);
        pMrna->SetId(*mpIdGenerator->GetIdFor("mrna"));
        annot.SetData().SetFtable().push_back(pMrna);
    }

    CRef<CSeq_loc> pCdsLocation(new CSeq_loc);
    pCdsLocation->Assign(record.ThickLocation());
    if (!mRnaLocation->IsNull()) {
        CRef<CSeq_loc> pIntersection = pCdsLocation->Intersect(
            *mRnaLocation, 0, nullptr);
        pCdsLocation->Assign(*pIntersection);   
    }
    if (!pCdsLocation->IsNull()) {
        pCds.Reset(new CSeq_feat);
        //pCds->SetData().SetCdregion();
        pCds->SetData().SetRegion(record.Name());
        pCds->SetLocation().Assign(*pCdsLocation);
        pCds->SetId(*mpIdGenerator->GetIdFor("cds"));
        annot.SetData().SetFtable().push_back(pCds);
    }

    if (pGene  &&  pMrna) {
        pGene->AddSeqFeatXref(pMrna->GetId());
        pMrna->AddSeqFeatXref(pGene->GetId());
    }
    if (pGene  &&  pCds) {
        pGene->AddSeqFeatXref(pCds->GetId());
        pCds->AddSeqFeatXref(pGene->GetId());
    }
    if (pMrna  &&  pCds) {
        pMrna->AddSeqFeatXref(pCds->GetId());
        pCds->AddSeqFeatXref(pMrna->GetId());
    }
}


//  ============================================================================
void
CBedAnnotAssembler::FinalizeAnnot(
    const CAnnotImportData& annotInfo,
    CSeq_annot& annot)
    //  ============================================================================
{
    auto description = annotInfo.ValueOf("description");
    if (!description.empty()) {
        annot.SetTitleDesc(description);
    }
    auto name = annotInfo.ValueOf("name");
    if (!name.empty()) {
        annot.SetNameDesc(name);
    }

    CRef<CUser_object> pTrackData(new CUser_object());
    pTrackData->SetType().SetStr("Track Data");
    pTrackData->SetData();
    for (auto info: annotInfo) {
        pTrackData->AddField(info.first, info.second);
    }
    CRef<CAnnotdesc> user(new CAnnotdesc());
    user->SetUser(*pTrackData);
    annot.SetDesc().Set().push_back(user);
}
