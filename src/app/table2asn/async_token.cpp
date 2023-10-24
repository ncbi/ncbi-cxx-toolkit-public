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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   Converter of various files into ASN.1 format, main application function
*
*/

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistd.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <objmgr/feat_ci.hpp>

#include <objmgr/util/feature.hpp>
#include "async_token.hpp"
#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;

TAsyncToken::operator CConstRef<CSeq_entry>() const
{
    if (entry)
        return entry;
    if (seh)
        return seh.GetCompleteSeq_entry();
    return {};
}


CRef<feature::CFeatTree> TAsyncToken::FeatTree()
{
    if (feat_tree.Empty())
    {
        feat_tree.Reset(new feature::CFeatTree());
        feat_tree->AddFeatures(CFeat_CI(scope->GetBioseqHandle(*bioseq)));
    }
    return feat_tree;
}


void TAsyncToken::Clear()
{
    map_locus_to_gene.clear();
    map_protein_to_mrna.clear();
    map_transcript_to_mrna.clear();
    feat_tree.ReleaseOrNull();
}


CRef<CSeq_feat> TAsyncToken::ParentGene(const CSeq_feat& cds)
{
    if (cds.IsSetXref()) {
        for (auto pXref : cds.GetXref()) {
            if (pXref->IsSetId()) {
                auto pLinkedFeat = FindFeature(pXref->GetId());
                if (pLinkedFeat &&
                    pLinkedFeat->IsSetData() &&
                    pLinkedFeat->GetData().IsGene()) {
                    return pLinkedFeat;
                }
            }

            if (pXref->IsSetData() &&
                pXref->GetData().IsGene() &&
                pXref->GetData().GetGene().IsSetLocus_tag()) {
                auto pGene = FeatFromMap(pXref->GetData().GetGene().GetLocus_tag(), map_locus_to_gene);
                if (pGene) {
                    return pGene;
                }
            }
        }
    }

    auto pGene = FindGeneByLocusTag(cds);
    if (!pGene) {
        CMappedFeat mappedCds(scope->GetSeq_featHandle(cds));
        auto mappedGene = feature::GetBestGeneForCds(mappedCds, FeatTree());
        if (mappedGene) {
            pGene.Reset(const_cast<CSeq_feat*>(&mappedGene.GetOriginalFeature()));
        }
    }
    return pGene;
}


CRef<CSeq_feat> TAsyncToken::FindFeature(const CFeat_id& id)
{
    if (bioseq->IsSetAnnot()) {
        for (auto annot : bioseq->GetAnnot())
        {
            if (!annot->IsFtable()) continue;

            ITERATE(CSeq_annot::TData::TFtable, feat_it, annot->GetData().GetFtable())
            {
                if ((**feat_it).IsSetIds())
                {
                    ITERATE(CSeq_feat::TIds, id_it, (**feat_it).GetIds())
                    {
                        if ((**id_it).Equals(id))
                        {
                            return *feat_it;
                        }
                    }
                }
                if ((**feat_it).IsSetId() && (**feat_it).GetId().Equals(id))
                    return *feat_it;
            }
        }
    }
    return CRef<CSeq_feat>();
}


CRef<CSeq_feat> TAsyncToken::FeatFromMap(const string& key, const TFeatMap& feat_map) const
{
    if (!key.empty()) {
        auto it = feat_map.find(key);
        if (it != feat_map.end()) {
            return it->second;
        }
    }
    return CRef<CSeq_feat>();
}


CRef<CSeq_feat> TAsyncToken::FindGeneByLocusTag(const CSeq_feat& cds) const
{
    if (!cds.GetData().IsCdregion() || !cds.IsSetQual())
        return CRef<CSeq_feat>();

    const auto& locus_tag = cds.GetNamedQual("locus_tag");
    return FeatFromMap(locus_tag, map_locus_to_gene);
}


CRef<CSeq_feat> TAsyncToken::ParentMrna(const CSeq_feat& cds)
{
    if (cds.IsSetXref()) {
        for (auto pXref : cds.GetXref()) {
            if (pXref->IsSetId()) {
                auto pLinkedFeat = FindFeature(pXref->GetId());
                if (pLinkedFeat &&
                    pLinkedFeat->IsSetData() &&
                    pLinkedFeat->GetData().IsRna() &&
                    pLinkedFeat->GetData().GetRna().GetType() == CRNA_ref::eType_mRNA) {
                    return pLinkedFeat;
                }
            }
        }
    }
    auto pMrna = FindMrnaByQual(cds);
    if (!pMrna) {
        CMappedFeat mappedCds(scope->GetSeq_featHandle(cds));
        auto mappedMrna = feature::GetBestMrnaForCds(mappedCds, FeatTree());
        if (mappedMrna) {
            pMrna.Reset(const_cast<CSeq_feat*>(&mappedMrna.GetOriginalFeature()));
        }
    }
    return pMrna;
}


CRef<CSeq_feat> TAsyncToken::FindMrnaByQual(const CSeq_feat& cds) const
{
    if (!cds.GetData().IsCdregion() || !cds.IsSetQual())
        return CRef<CSeq_feat>();

    const auto& transcript_id = cds.GetNamedQual("transcript_id");
    auto pMrna = FeatFromMap(transcript_id, map_transcript_to_mrna);
    if (!pMrna) {
        const auto& protein_id = cds.GetNamedQual("protein_id");
        pMrna = FeatFromMap(protein_id, map_protein_to_mrna);
    }

    return pMrna;
}


void TAsyncToken::InitFeatures()
{
    if (!bioseq->IsSetAnnot()) {
        return;
    }
    for (auto annot: bioseq->GetAnnot()) {
        if (!annot->IsFtable()) {
            continue;
        }

        for (auto feat: annot->GetData().GetFtable()) {
            if (!feat->CanGetData()) {
                continue;
            }
            const auto& data = feat->GetData();
            bool ismrna = data.IsRna() && data.GetRna().IsSetType() && data.GetRna().GetType() == CRNA_ref::eType_mRNA;
            bool isgene = data.IsGene();

            if (feat->IsSetQual()) {
                for (auto qual: feat->GetQual()) {
                    if (!qual->CanGetQual() || !qual->CanGetVal()) {
                        continue;
                    }
                    const string& name = qual->GetQual();
                    const string& value = qual->GetVal();

                    if (name == "transcript_id") {
                        if (ismrna) {
                            map_transcript_to_mrna.insert(TFeatMap::value_type(value, feat));
                        }
                    }
                    else {
                        if (name == "protein_id") {
                            if (ismrna) {
                                map_protein_to_mrna.insert(TFeatMap::value_type(value, feat));
                            }
                        }
                        else {
                            if (name == "locus_tag") {
                                if (isgene) {
                                    map_locus_to_gene.insert(TFeatMap::value_type(value, feat));
                                }
                            }
                        }
                    }
                }
            }
            if (isgene) {
                if (feat->GetData().GetGene().IsSetLocus_tag()) {
                    map_locus_to_gene.insert(
                        TFeatMap::value_type(feat->GetData().GetGene().GetLocus_tag(), feat));
                }
            }
        }
    }
}


