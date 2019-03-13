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
* Author: Colleen Bollin, based on work done by Frank Ludwig
*
* File Description:
*   functions for propagating a feature using an alignment
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>

#include <objtools/edit/cds_fix.hpp>
#include <objtools/edit/feature_propagate.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CFeaturePropagator::CFeaturePropagator
(CBioseq_Handle src, CBioseq_Handle target,
 const CSeq_align& align, 
 bool stop_at_stop, bool cleanup_partials, bool merge_abutting,
 CMessageListener_Basic* pMessageListener, CObject_id::TId* feat_id)
:     m_Src(src), m_Target(target),
    m_Scope(m_Target.GetScope()),
    m_CdsStopAtStopCodon(stop_at_stop),
    m_CdsCleanupPartials(cleanup_partials),
    m_MessageListener(pMessageListener),
    m_MaxFeatId(feat_id),
    m_MergeAbutting(merge_abutting),
      m_ExpandOverGaps(true)
{
    CSeq_loc_Mapper_Options mapper_options(CSeq_loc_Mapper::fTrimMappedLocation);
    m_Mapper = new CSeq_loc_Mapper(*m_Src.GetSeqId(), *m_Target.GetSeqId(), align, &m_Target.GetScope(), mapper_options);
    if (merge_abutting) {
        m_Mapper->SetMergeAll();
    }
    m_Mapper->SetGapRemove();
    m_Mapper->SetFuzzOption(CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr);
}

CFeaturePropagator::CFeaturePropagator
(CBioseq_Handle src, CBioseq_Handle target,
 const CSeq_align& align, 
 bool stop_at_stop, bool cleanup_partials, bool merge_abutting, bool expand_over_gaps,
 CMessageListener_Basic* pMessageListener, CObject_id::TId* feat_id)
:     m_Src(src), m_Target(target),
    m_Scope(m_Target.GetScope()),
    m_CdsStopAtStopCodon(stop_at_stop),
    m_CdsCleanupPartials(cleanup_partials),
    m_MessageListener(pMessageListener),
    m_MaxFeatId(feat_id),
    m_MergeAbutting(merge_abutting),
      m_ExpandOverGaps(expand_over_gaps)
{
    CSeq_loc_Mapper_Options mapper_options(CSeq_loc_Mapper::fTrimMappedLocation);
    m_Mapper = new CSeq_loc_Mapper(*m_Src.GetSeqId(), *m_Target.GetSeqId(), align, &m_Target.GetScope(), mapper_options);
    if (merge_abutting) {
        m_Mapper->SetMergeAll();
    }
    m_Mapper->SetGapRemove();
    m_Mapper->SetFuzzOption(CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr);
}

CRef<CSeq_feat> CFeaturePropagator::Propagate(const CSeq_feat& orig_feat)
{
    CRef<CSeq_feat> rval(new CSeq_feat());
    rval->Assign(orig_feat);

    if (rval->IsSetId() && (m_MaxFeatId && *m_MaxFeatId > 0)) {
        rval->SetId().SetLocal().SetId(++(*m_MaxFeatId));
        m_FeatIdMap.emplace(orig_feat.GetId().GetLocal().GetId(), rval->GetId().GetLocal().GetId());
    }

    // propagate feature location
    CConstRef<CSeq_id> pTargetId = m_Target.GetSeqId();
    CRef<CSeq_loc> new_loc = x_MapLocation(orig_feat.GetLocation(), *pTargetId);
    if (!new_loc) {
        if (m_MessageListener) {
            string loc_label;
            orig_feat.GetLocation().GetLabel(&loc_label);
            m_MessageListener->PostMessage(
                CMessage_Basic("Unable to propagate location of feature: " + loc_label, 
                               eDiag_Error,
                               eFeaturePropagationProblem_FeatureLocation));
        }
        rval.Reset(nullptr);
        return rval;
    }

    rval->SetLocation().Assign(*new_loc);
    rval->ResetPartial();
    rval->ResetProduct();
    if (new_loc->IsPartialStart(eExtreme_Biological) || new_loc->IsPartialStop(eExtreme_Biological)) {
        rval->SetPartial(true);
    }

    const CSeq_loc& location = orig_feat.GetLocation();
    bool origIsPartialStart = location.IsPartialStart(eExtreme_Biological);

    // depending on feature type, propagate locations in data
    switch(orig_feat.GetData().GetSubtype()) {
    case CSeqFeatData::eSubtype_cdregion:
        x_PropagateCds(*rval, *pTargetId, origIsPartialStart);
        break;
    case CSeqFeatData::eSubtype_tRNA:
        x_PropagatetRNA(*rval, *pTargetId);
        break;      
    default:
        break;
    }

    return rval;
}


vector<CRef<CSeq_feat> > CFeaturePropagator::PropagateAll()
{
    vector<CRef<CSeq_feat> > rval;
    CFeat_CI fi(m_Src);
    while (fi) {
        CRef<CSeq_feat> new_feat = Propagate(*(fi->GetOriginalSeq_feat()));
        if (new_feat) {
            rval.push_back(new_feat);
        }
        ++fi;
    }
    return rval;
}

vector<CRef<CSeq_feat>> CFeaturePropagator::PropagateFeatureList(const vector<CConstRef<CSeq_feat>>& orig_feats)
{
    vector<CRef<CSeq_feat>> propagated_feats;

    for (auto&& it : orig_feats) {
        CRef<CSeq_feat> new_feat = Propagate(*it);
        if (new_feat) {
            propagated_feats.push_back(new_feat);
            if (new_feat->IsSetData() && new_feat->GetData().IsCdregion()) {
                if (it->IsSetProduct()) {
                    CRef<CSeq_feat> prot_feat = ConstructProteinFeatureForPropagatedCodingRegion(*it, *new_feat);
                    if (prot_feat) {
                        propagated_feats.push_back(prot_feat);
                    }
                }
            }
        }
    }

    if (!m_FeatIdMap.empty()) {
        for (auto&& it : propagated_feats) {
            if (it->IsSetXref()) {
                auto xref_it = it->SetXref().begin();
                while (xref_it != it->SetXref().end()) {
                    if ((*xref_it)->IsSetId()) {
                        CFeat_id& feat_id = (*xref_it)->SetId();
                        if (feat_id.IsLocal() && feat_id.GetLocal().IsId()) {
                            auto orig_featid = m_FeatIdMap.find(feat_id.GetLocal().GetId());
                            if (orig_featid != m_FeatIdMap.end()) {
                                feat_id.SetLocal().SetId(orig_featid->second);
                            }
                            else {
                                // it hasn't been propagated yet or it will not be propagated
                                xref_it = it->SetXref().erase(xref_it);
                                continue;
                            }
                        }
                    }
                    ++xref_it;
                }
                if (it->GetXref().empty()) {
                    it->ResetXref();
                }
            }
        }
    }

    return propagated_feats;
}

CRef<CSeq_loc> CFeaturePropagator::x_MapLocation(const CSeq_loc& sourceLoc, const CSeq_id& targetId)
{
    CSeq_loc_I loc_i;
    for (CSeq_loc_CI loc_iter(sourceLoc, CSeq_loc_CI::eEmpty_Skip);  loc_iter;  ++loc_iter)
    {
        CConstRef<CSeq_loc> loc = loc_iter.GetRangeAsSeq_loc();
        CRef<CSeq_loc> pTargetLoc = m_Mapper->Map(*loc);
        if (!pTargetLoc || pTargetLoc->IsNull()) 
        {
            continue;
        }
        if (m_ExpandOverGaps) 
        {
            pTargetLoc = pTargetLoc->Merge(CSeq_loc::fMerge_SingleRange, nullptr);
        }
        for (CSeq_loc_CI loc_iter2(*pTargetLoc, CSeq_loc_CI::eEmpty_Skip);  loc_iter2;  ++loc_iter2)
        {
            CSeq_loc_I loc_i2 = loc_i.InsertInterval(targetId, loc_iter2.GetRange(), loc_iter2.GetStrand());           
        }
    }
    CRef<objects::CSeq_loc> target;
    if (loc_i.HasChanges())
    {
        target = loc_i.MakeSeq_loc();
    }
    if (m_MergeAbutting && target) 
    {
        target = target->Merge(CSeq_loc::fMerge_All, nullptr);
    }
    CRef<CSeq_loc> all = m_Mapper->Map(sourceLoc); // Need to do a separate mapping in case one of the starting or finishing intervals falls off - this affects the partials
    if (all && all->IsPartialStart(eExtreme_Biological) && target)
    {
        target->SetPartialStart(true, eExtreme_Biological);
    }
    if (all && all->IsPartialStop(eExtreme_Biological) && target)
    {
        target->SetPartialStop(true, eExtreme_Biological);
    }
    if (sourceLoc.IsMix() && target && target->IsPacked_int())
    {
        target->ChangeToMix();
    }
    
    return target;
}

CRef<CSeq_loc> CFeaturePropagator::x_TruncateToStopCodon(const CSeq_loc& loc, unsigned int truncLen)
{
    CRef<CSeq_loc> pTruncated;

    unsigned int curLen = 0;
    for (CSeq_loc_CI it(loc); it && curLen < truncLen; ++it) {
        unsigned int thisLen = it.GetRange().GetLength();
        CConstRef<CSeq_loc> pThisLoc = it.GetRangeAsSeq_loc();
        if (curLen + thisLen <= truncLen) {
            if (pTruncated) {
                pTruncated->Add(*pThisLoc);
            } else {
                pTruncated.Reset(new CSeq_loc());
                pTruncated->Assign(*pThisLoc);
            }
            curLen += thisLen;
        } else {
            CRef<CSeq_loc> pPartialLoc(new CSeq_loc());
            unsigned int missingLen = truncLen - curLen;
            const TSeqPos start = pThisLoc->GetStart(eExtreme_Biological);
            if (missingLen == 1) {
                // make a point
                pPartialLoc->SetPnt().SetPoint(start);
            } else {
                // make an interval
                if (pThisLoc->IsSetStrand() && pThisLoc->GetStrand() == eNa_strand_minus) {
                    pPartialLoc->SetInt().SetFrom(start - missingLen + 1);
                    pPartialLoc->SetInt().SetTo(start);
                } else {
                    pPartialLoc->SetInt().SetFrom(start);
                    pPartialLoc->SetInt().SetTo(start + missingLen - 1);
                }
            }
            pPartialLoc->SetId(*pThisLoc->GetId());
            if (pThisLoc->IsSetStrand()) {
                pPartialLoc->SetStrand(pThisLoc->GetStrand());
            }
            if (pTruncated) {
                pTruncated->Add(*pPartialLoc);
            } else {
                pTruncated.Reset(new CSeq_loc());
                pTruncated->Assign(*pPartialLoc);
            }
            curLen += missingLen;  
        }
    }

    return pTruncated;
}

CRef<CSeq_loc> CFeaturePropagator::x_ExtendToStopCodon (CSeq_feat& feat)
{
    CRef<CSeq_loc> new_loc;

    if (!feat.IsSetLocation()) {
        return new_loc;
    }
    const CSeq_loc& loc = feat.GetLocation();

    const CGenetic_code* code = nullptr;
    if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.GetData().GetCdregion().IsSetCode()) {
        code = &(feat.GetData().GetCdregion().GetCode());
    }

    const TSeqPos stop = loc.GetStop(eExtreme_Biological);
    // figure out if we have a partial codon at the end
    size_t orig_len = sequence::GetLength(loc, &m_Scope);
    size_t nuc_len = orig_len;
    if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.GetData().GetCdregion().IsSetFrame()) {
        CCdregion::EFrame frame = feat.GetData().GetCdregion().GetFrame();
        if (frame == CCdregion::eFrame_two) {
            nuc_len -= 1;
        } else if (frame == CCdregion::eFrame_three) {
            nuc_len -= 2;
        }
    }
    unsigned int mod = nuc_len % 3;
    CRef<CSeq_loc> vector_loc(new CSeq_loc());
    CSeq_interval& vector_int = vector_loc->SetInt();
    vector_int.SetId().Assign(*(loc.GetId()));

    if (loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
        vector_int.SetFrom(0);
        vector_int.SetTo(stop + mod - 1);
        vector_loc->SetStrand(eNa_strand_minus);
    } else {
        vector_int.SetFrom(stop - mod + 1);
        vector_int.SetTo(m_Target.GetInst_Length() - 1);
    }

    CSeqVector seq(*vector_loc, m_Scope, CBioseq_Handle::eCoding_Iupac);
    // reserve our space
    const unsigned int usable_size = seq.size();

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    CSeqVector::const_iterator start = seq.begin();

    int state = 0;
    unsigned int prot_len = usable_size / 3;

    for (unsigned int i = 0;  i < prot_len;  ++i) {
        // loop through one codon at a time
        for (size_t k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        if (tbl.GetCodonResidue (state) == '*') {
            CSeq_loc_CI it(loc);
            CSeq_loc_CI it_next = it;
            ++it_next;
            while (it_next) {
                CConstRef<CSeq_loc> this_loc = it.GetRangeAsSeq_loc();
                if (new_loc) {
                    new_loc->Add(*this_loc);
                } else {
                    new_loc.Reset(new CSeq_loc());
                    new_loc->Assign(*this_loc);
                }
                it = it_next;
                ++it_next;
            }
            CRef<CSeq_loc> last_interval(new CSeq_loc());
            CConstRef<CSeq_loc> this_loc = it.GetRangeAsSeq_loc();
            const TSeqPos this_start = this_loc->GetStart(eExtreme_Positional);
            const TSeqPos this_stop = this_loc->GetStop(eExtreme_Positional);
            unsigned int extension = ((i + 1) * 3) - mod;
            last_interval->SetInt().SetId().Assign(*(this_loc->GetId()));
            if (this_loc->IsSetStrand() && this_loc->GetStrand() == eNa_strand_minus) {
                last_interval->SetStrand(eNa_strand_minus);
                last_interval->SetInt().SetFrom(this_start - extension);
                last_interval->SetInt().SetTo(this_stop);
            } else {
                last_interval->SetInt().SetFrom(this_start);
                last_interval->SetInt().SetTo(this_stop + extension);
            }
                
            if (new_loc) {
                new_loc->Add(*last_interval);
            } else {
                new_loc.Reset(new CSeq_loc());
                new_loc->Assign(*last_interval);
            }

            return new_loc;
        }
    }

    return new_loc;
}


void CFeaturePropagator::x_PropagateCds(CSeq_feat& feat, const CSeq_id& targetId, bool origIsPartialStart)
{
    bool ambiguous = false;
    feat.SetData().SetCdregion().SetFrame(CSeqTranslator::FindBestFrame(feat, m_Scope, ambiguous));

    x_CdsMapCodeBreaks(feat, targetId);
    if (m_CdsStopAtStopCodon) {
        x_CdsStopAtStopCodon(feat);
    }
    if (m_CdsCleanupPartials) {
        x_CdsCleanupPartials(feat, origIsPartialStart);
    }
}


void CFeaturePropagator::x_CdsMapCodeBreaks(CSeq_feat& feat, const CSeq_id& targetId)
{
    CCdregion& cds = feat.SetData().SetCdregion();
    if (cds.IsSetCode_break()) {
        auto it = cds.SetCode_break().begin();
        while (it != cds.SetCode_break().end()) {
            bool remove = false;
            if ((*it)->IsSetLoc()) {
                const CSeq_loc& codebreak = (*it)->GetLoc(); 
                CRef<CSeq_loc> new_codebreak = x_MapLocation(codebreak, targetId);
                if (new_codebreak) {
                    (*it)->SetLoc(*new_codebreak);
                } else {
                    if (m_MessageListener) {
                        string loc_label;
                        (*it)->GetLoc().GetLabel(&loc_label);
                        m_MessageListener->PostMessage(
                            CMessage_Basic("Unable to propagate location of translation exception: " + loc_label,
                                           eDiag_Error,
                                           eFeaturePropagationProblem_CodeBreakLocation));
                    }
                    remove = true;
                }
            }
            if (remove) {
                it = cds.SetCode_break().erase(it);
            } else {
                ++it;
            }
        }
        if (cds.GetCode_break().size() == 0) {
            cds.ResetCode_break();
        }
    }
}


void CFeaturePropagator::x_CdsCleanupPartials(CSeq_feat& cds, bool origIsPartialStart) 
{
    if (cds.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return;
    }

    // assume that the original is marked correctly, i.e. if there is not fuzz-from
    // then it is 5'complete and it there is no fuzz-to then it is 3' prime complete.
    // no need to recheck whther there are actual start and stop codons.

    // for the copies:
    // the copy ends in a stop codon: it is 3' complete. Nothing needs to be done.
    // the copy does not end in a stop codon. hence it is not 3' complete.
    // the original is 5' incomplete: the copy is 5' incomplete
    // the original is 5' complete, copy starts with a start codon: the copy is
    // 5' complete

    try {
        string prot;
        CSeqTranslator::Translate(cds, m_Scope, prot);

        bool targetIsPartialStart = ! (NStr::StartsWith(prot, 'M')) || cds.GetLocation().IsPartialStart(eExtreme_Biological);
        bool targetIsPartialStop = ! (NStr::EndsWith(prot, '*'));

        if (!targetIsPartialStart  && !origIsPartialStart) {
            //propagated feature is 5' complete
            cds.SetLocation().SetPartialStart(false, eExtreme_Biological);
        }
        else {
            //propagated feature is 5' partial
            cds.SetLocation().SetPartialStart(true, eExtreme_Biological);
        }
        if (!targetIsPartialStop) {
            //propagated feature ends in stop codon hence is 3' complete
            cds.SetLocation().SetPartialStop(false, eExtreme_Biological);
        }
        else {
            //propagated feature is 3' partial
            cds.SetLocation().SetPartialStop(true, eExtreme_Biological);
        }

        if (cds.GetLocation().IsPartialStart(eExtreme_Biological) ||
            cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
            cds.SetPartial(true);
        }
    }
    catch (...) {
    }
}


void CFeaturePropagator::x_CdsStopAtStopCodon(CSeq_feat& cds)
{
    if (cds.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return;
    }
    CRef<CBioseq> pTranslated = CSeqTranslator::TranslateToProtein(cds, m_Scope);
    if (!pTranslated) {
        return;
    }
    CRef<CSeq_loc> pAdjustedLoc;
    CSeqVector seqv(*pTranslated, &m_Scope, CBioseq_Handle::eCoding_Ncbi);
    seqv.SetCoding(CSeq_data::e_Ncbieaa);
    CSeqVector_CI it(seqv);
    while (it && *it != '*') {
        it++;
    }
    if (it) {
        //found stop codon inside the given feature
        unsigned int newSize = 3*(it.GetPos()+1);
        if (cds.GetData().GetCdregion().IsSetFrame()) {
            switch(cds.GetData().GetCdregion().GetFrame()) {
            default:
                break;
            case CCdregion::eFrame_two:
                newSize += 1;
                break;
            case CCdregion::eFrame_three:
                newSize += 2;
                break;
            }
        }
        pAdjustedLoc = x_TruncateToStopCodon(cds.GetLocation(), newSize);
    }
    else {
        pAdjustedLoc = x_ExtendToStopCodon(cds);
    }
    if (pAdjustedLoc) {
        cds.SetLocation(*pAdjustedLoc);
    }
}


void CFeaturePropagator::x_PropagatetRNA(CSeq_feat& feat, const CSeq_id& targetId)
{
    if (feat.GetData().GetRna().IsSetExt()) {
        const CRNA_ref::C_Ext& ext = feat.GetData().GetRna().GetExt();
        if (ext.IsTRNA()) {
            const CTrna_ext& trna_ext = ext.GetTRNA();
            if (trna_ext.IsSetAnticodon()) {
                const CSeq_loc& anticodon = trna_ext.GetAnticodon();
                CRef<CSeq_loc> new_anticodon = x_MapLocation(anticodon, targetId);
                if (new_anticodon) {
                    feat.SetData().SetRna().SetExt().SetTRNA().SetAnticodon(*new_anticodon);
                } else {
                    if (m_MessageListener) {
                        string loc_label;
                        anticodon.GetLabel(&loc_label);
                        m_MessageListener->PostMessage(
                            CMessage_Basic("Unable to propagate location of anticodon: " + loc_label,
                                           eDiag_Error,
                                           eFeaturePropagationProblem_AnticodonLocation));
                    }
                    feat.SetData().SetRna().SetExt().SetTRNA().ResetAnticodon();
                }
            }
        }
    }
}


CRef<CSeq_feat> CFeaturePropagator::ConstructProteinFeatureForPropagatedCodingRegion(const CSeq_feat& orig_cds, const CSeq_feat& new_cds)
{    
    if (!orig_cds.IsSetProduct()) {
        return CRef<CSeq_feat>(nullptr);
    }
    CRef<CSeq_feat> prot_feat;
    CBioseq_Handle prot = m_Scope.GetBioseqHandle(orig_cds.GetProduct());
    if (prot) {
        CFeat_CI pf(prot, CSeqFeatData::eSubtype_prot);
        if (pf) {
            prot_feat.Reset(new CSeq_feat());
            const CSeq_feat& orig_prot = *pf->GetOriginalSeq_feat();
            prot_feat->Assign(orig_prot);

            if ((m_MaxFeatId && *m_MaxFeatId > 0) && (pf->GetOriginalSeq_feat()->IsSetId() || orig_cds.IsSetId())) {
                prot_feat->SetId().SetLocal().SetId(++(*m_MaxFeatId));
                // protein features don't have Xrefs
            }
            // fix partials
            prot_feat->SetLocation().SetPartialStart(new_cds.GetLocation().IsPartialStart(eExtreme_Biological), eExtreme_Biological);
            prot_feat->SetLocation().SetPartialStop(new_cds.GetLocation().IsPartialStop(eExtreme_Biological), eExtreme_Biological);
            prot_feat->SetPartial(prot_feat->GetLocation().IsPartialStart(eExtreme_Biological) ||
                                  prot_feat->GetLocation().IsPartialStop(eExtreme_Biological));
            if (new_cds.IsSetProduct() && !new_cds.GetProduct().GetId()->Equals(*(orig_cds.GetProduct().GetId()))) {
                prot_feat->SetLocation().SetId(new_cds.GetProduct().GetWhole());
            }
        }
    }
    return prot_feat;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

