/* $Id$
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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   For adjusting features for gaps
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/edit/gap_trim.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objtools/edit/cds_fix.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CFeatGapInfo::CFeatGapInfo(CSeq_feat_Handle sf)
{
    m_Feature = sf;
    CollectGaps(sf.GetLocation(), sf.GetScope());
}


void CFeatGapInfo::CollectGaps(const CSeq_loc& feat_loc, CScope& scope)
{
    m_Gaps.clear();
    m_Unknown = false;
    m_Known = false;
    m_Ns = false;

    m_Start = feat_loc.GetStart(objects::eExtreme_Positional);
    m_Stop = feat_loc.GetStop(objects::eExtreme_Positional);
    CRef<CSeq_loc> total_loc = sequence::Seq_loc_Merge(feat_loc, CSeq_loc::fMerge_SingleRange, &scope);
    if (total_loc->IsSetStrand()) total_loc->ResetStrand();
    CConstRef<objects::CSeqMap> seq_map = CSeqMap::GetSeqMapForSeq_loc(*total_loc, &scope);

    // use CSeqVector for finding Ns
    CSeqVector vec(*seq_map, scope, CBioseq_Handle::eCoding_Iupac);

    CSeqMap_CI seq_map_ci = seq_map->ResolvedRangeIterator(&scope,
        0,
        m_Stop - m_Start + 1,
        eNa_strand_plus, //feat_loc.GetStrand(),
        size_t(-1),
        objects::CSeqMap::fFindGap | objects::CSeqMap::fFindData);

    for (; seq_map_ci; ++seq_map_ci)
    {

        if (seq_map_ci.GetType() == objects::CSeqMap::eSeqGap)
        {
            TSeqPos gap_start = m_Start + seq_map_ci.GetPosition();
            TSeqPos gap_stop = gap_start + seq_map_ci.GetLength() - 1;
            bool is_unknown = seq_map_ci.IsUnknownLength();
            if (is_unknown) {
                m_Unknown = true;
            } else {
                m_Known = true;
            }
            m_Gaps.push_back(TGapInterval(is_unknown ? eGapIntervalType_unknown : eGapIntervalType_known, pair<size_t, size_t>(gap_start, gap_stop)));
        } else {
            // look for Ns
            TSeqPos map_start = seq_map_ci.GetPosition();
            TSeqPos map_stop = map_start + seq_map_ci.GetLength() - 1;
            bool in_ns = false;
            TSeqPos gap_start;
            for (TSeqPos i = map_start; i <= map_stop; ++i) {
                char letter = vec[i];
                if (letter == 'N') {
                    if (!in_ns) {
                        // start new gap
                        gap_start = m_Start + i;
                        in_ns = true;
                    }
                } else if (in_ns) {
                    // end previous gap
                    TSeqPos gap_stop = m_Start + i - 1;
                    m_Gaps.push_back(TGapInterval(eGapIntervalType_n, pair<size_t, size_t>(gap_start, gap_stop)));
                    m_Ns = true;
                    in_ns = false;
                }
            }
            if (in_ns) {
                TSeqPos gap_stop = m_Start + map_stop;
                m_Gaps.push_back(TGapInterval(eGapIntervalType_n, pair<size_t, size_t>(gap_start, gap_stop)));
                m_Ns = true;
            }
        }
    }
}


bool CFeatGapInfo::x_UsableInterval(const TGapInterval& interval, bool unknown_length, bool known_length, bool ns)
{
    if (interval.first == eGapIntervalType_unknown && !unknown_length) {
        return false;
    } else if (interval.first == eGapIntervalType_known && !known_length) {
        return false;
    } else if (interval.first == eGapIntervalType_n && !ns) {
        return false;
    } else {
        return true;
    }
}


void CFeatGapInfo::CalculateRelevantIntervals(bool unknown_length, bool known_length, bool ns)
{
    m_InsideGaps.clear();
    m_LeftGaps.clear();
    m_RightGaps.clear();

    if (!m_Gaps.empty()) {
        // find left offset
        size_t skip_left = 0;
        TGapIntervalList::iterator it = m_Gaps.begin();
        while (it != m_Gaps.end()) {
            if (!x_UsableInterval(*it, unknown_length, known_length, ns)) {
                break;
            }

            if (m_LeftGaps.empty()) {
                if (it->second.first <= m_Start && it->second.second >= m_Start) {
                    m_LeftGaps.push_back(it->second);
                    skip_left++;
                } else {
                    break;
                }
            } else if (it->second.first <= m_LeftGaps.front().second + 1 && it->second.second >= m_LeftGaps.front().second) {
                m_LeftGaps.front().second = it->second.second;
                skip_left++;
            } else {
                break;
            }
            ++it;
        }
        TGapIntervalList::reverse_iterator rit = m_Gaps.rbegin();
        size_t skip_right = 0;
        while (rit != m_Gaps.rend()) {
            if (!x_UsableInterval(*rit, unknown_length, known_length, ns)) {
                break;
            }
            if (m_RightGaps.empty()) {
                if (rit->second.first <= m_Stop && rit->second.second >= m_Stop) {
                    m_RightGaps.push_back(rit->second);
                    skip_right++;
                } else {
                    break;
                }
            } else if (rit->second.first <= m_RightGaps.front().first - 1 && rit->second.second >= m_RightGaps.front().second) {
                m_RightGaps.front().first = rit->second.first;
                skip_right++;
            } else {
                break;
            }
            ++rit;
        }
        for (size_t offset = skip_left; offset < m_Gaps.size() - skip_right; offset++) {
            if (x_UsableInterval(m_Gaps[offset], unknown_length, known_length, ns)) {
                m_InsideGaps.push_back(m_Gaps[offset].second);
            }
        }
    }
}


bool CFeatGapInfo::Trimmable() const
{
    if (ShouldRemove()) {
        return false;
    } else if (!m_LeftGaps.empty() || !m_RightGaps.empty()) {
        return true;
    } else {
        return false;
    }
}


bool CFeatGapInfo::Splittable() const
{
    if (!m_InsideGaps.empty()) {
        return true;
    } else {
        return false;
    }
}


bool CFeatGapInfo::ShouldRemove() const
{
    if (!m_LeftGaps.empty() && m_LeftGaps.front().second >= m_Stop) {
        return true;
    } else {
        return false;
    }
}


void CFeatGapInfo::Trim(CSeq_loc& loc, bool make_partial, CScope& scope)
{
  CRef<CSeq_id> seqid(new CSeq_id);
  seqid->Assign(*loc.GetId());
    for (vector<pair<size_t, size_t> >::reverse_iterator b = m_LeftGaps.rbegin(); b != m_LeftGaps.rend(); ++b)
    {
        size_t start = b->first;
        size_t stop = b->second;
        CRef<CSeq_loc> loc2(new CSeq_loc());
        int options = edit::eSplitLocOption_split_in_exon | edit::eSplitLocOption_split_in_intron;
        if (make_partial)
            options |= edit::eSplitLocOption_make_partial;
        edit::SplitLocationForGap(loc, *loc2, start, stop, seqid.GetPointer(), options);
        if (loc2->Which() != CSeq_loc::e_not_set)
        {
            loc.Assign(*loc2);
        }
    }
    for (vector<pair<size_t, size_t> >::reverse_iterator b = m_RightGaps.rbegin(); b != m_RightGaps.rend(); ++b)
    {
        size_t start = b->first;
        size_t stop = b->second;
        CRef<CSeq_loc> loc2(new CSeq_loc());
        int options = edit::eSplitLocOption_split_in_exon;
        if (make_partial)
            options |= edit::eSplitLocOption_make_partial;
        edit::SplitLocationForGap(loc, *loc2, start, stop, seqid.GetPointer(), options);
    }
}


CFeatGapInfo::TLocList CFeatGapInfo::Split(const CSeq_loc& orig, bool in_intron, bool make_partial)
{
    TLocList locs;
    CRef<CSeq_loc> left_loc(new CSeq_loc());
    left_loc->Assign(orig);
    CRef<CSeq_id> seqid(new CSeq_id);
    seqid->Assign(*orig.GetId());
    for (vector<pair<size_t, size_t> >::reverse_iterator b = m_InsideGaps.rbegin(); b != m_InsideGaps.rend(); ++b)
    {
        size_t start = b->first;
        size_t stop = b->second;
        CRef<CSeq_loc> loc2(new CSeq_loc());
        int options = edit::eSplitLocOption_make_partial | edit::eSplitLocOption_split_in_exon;
        if (in_intron)
            options |= edit::eSplitLocOption_split_in_intron;
        edit::SplitLocationForGap(*left_loc, *loc2, start, stop, seqid.GetPointer(), options);
        if (loc2->GetId())
        {
            if (make_partial)
            {
                loc2->SetPartialStart(true, objects::eExtreme_Positional);
		        if (left_loc->Which() != CSeq_loc::e_not_set)
		        {
		            left_loc->SetPartialStop(true, objects::eExtreme_Positional);
		        }
            }
            locs.push_back(loc2);
        }
    }
    if (locs.size() > 0) {
        if (left_loc->Which() != CSeq_loc::e_not_set)
        {
            locs.push_back(left_loc);
        }
        reverse(locs.begin(), locs.end());
    }
    return locs;
}


void CFeatGapInfo::x_AdjustOrigLabel(CSeq_feat& feat, size_t& id_offset, string& id_label, const string& qual)
{
    if (!feat.IsSetQual()) {
        return;
    }
    NON_CONST_ITERATE(CSeq_feat::TQual, it, feat.SetQual()) {
        if ((*it)->IsSetQual() && (*it)->IsSetVal() &&
            !NStr::IsBlank((*it)->GetVal()) &&
            NStr::EqualNocase((*it)->GetQual(), qual) &&
            (id_label.empty() || NStr::Equal((*it)->GetVal(), id_label) || NStr::Equal((*it)->GetVal(), id_label + "_1"))) {
            if (id_label.empty()) {
                id_label = (*it)->GetVal();
            }
            if (id_offset != 0)
                (*it)->SetVal(id_label + "_" + NStr::NumericToString(id_offset));
            id_offset++;
        }
    }
}

CRef<CBioseq> CFeatGapInfo::AdjustProteinSeq(const CBioseq& seq, const CSeq_feat& feat, const CSeq_feat& orig_cds, CScope& scope)
{
    if (!feat.IsSetProduct() || !feat.GetProduct().IsWhole() || !seq.IsAa()) {
        return CRef<CBioseq>(NULL);
    }

    TSeqPos orig_len = seq.GetInst().GetLength();

    string prot;
    CSeqTranslator::Translate(feat, scope, prot);
    if (prot.empty()) {
        return CRef<CBioseq>(NULL);
    }

    CRef<CBioseq> prot_seq(new CBioseq);
    prot_seq->Assign(seq);
    prot_seq->SetInst().ResetExt();
    prot_seq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    prot_seq->SetInst().SetSeq_data().SetIupacaa().Set(prot);
    prot_seq->SetInst().SetLength(TSeqPos(prot.length()));
    prot_seq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    // fix sequence ID
    const CSeq_id& feat_prod = feat.GetProduct().GetWhole();
    if (!feat_prod.Equals(orig_cds.GetProduct().GetWhole())) {
        NON_CONST_ITERATE(CBioseq::TId, id, prot_seq->SetId()) {
            if ((*id)->Which() == feat_prod.Which()) {
                bool do_replace = false;
                if ((*id)->IsGeneral()) {
                    if ((*id)->GetGeneral().IsSetDb()) {
                        if (feat_prod.GetGeneral().IsSetDb() &&
                            NStr::Equal(feat_prod.GetGeneral().GetDb(), (*id)->GetGeneral().GetDb())) {
                            do_replace = true;
                        }
                    } else if (!feat_prod.GetGeneral().IsSetDb()) {
                        do_replace = true;
                    }
                } else {
                    do_replace = true;
                }
                if (do_replace) {
                    (*id)->Assign(feat.GetProduct().GetWhole());
                }
            }
        }
    }
    // fix molinfo
    if (prot_seq->IsSetDescr()) {
        NON_CONST_ITERATE(CBioseq::TDescr::Tdata, mi, prot_seq->SetDescr().Set()) {
            if ((*mi)->IsMolinfo()) {
                feature::AdjustProteinMolInfoToMatchCDS((*mi)->SetMolinfo(), feat);
            }
        }
    }

    // also fix protein feature locations
    if (prot_seq->IsSetAnnot()) {
        
        CRef<CSeq_loc_Mapper> nuc2prot_mapper(
            new CSeq_loc_Mapper(orig_cds, CSeq_loc_Mapper::eLocationToProduct, &scope));
        CRef<CSeq_loc> prot_shadow = nuc2prot_mapper->Map(feat.GetLocation());
        TSeqPos start = prot_shadow->GetStart(eExtreme_Positional);
        TSeqPos stop = prot_shadow->GetStop(eExtreme_Positional);
        NON_CONST_ITERATE(CBioseq::TAnnot, ait, prot_seq->SetAnnot()) {
            if ((*ait)->IsFtable()) {
                CSeq_annot::TData::TFtable::iterator fit = (*ait)->SetData().SetFtable().begin();
                while (fit != (*ait)->SetData().SetFtable().end()) {
                    CRef<CSeq_loc> new_prot_loc(new CSeq_loc());
                    new_prot_loc->Assign((*fit)->GetLocation());
                    bool complete_cut = false;
                    bool adjusted = false;
                    TSeqPos removed = 0;
                    if (start > 0) {
                        SeqLocAdjustForTrim(*new_prot_loc, 0, start - 1, orig_cds.GetProduct().GetId(), complete_cut, removed, adjusted);
                    }
                    if (!complete_cut && stop < orig_len - 1) {
                        SeqLocAdjustForTrim(*new_prot_loc, stop + 1, orig_len - 1, orig_cds.GetProduct().GetId(), complete_cut, removed, adjusted);
                    }
                    if (complete_cut) {
                        // out of range, drop this one
                        fit = (*ait)->SetData().SetFtable().erase(fit);
                    } else {
                        new_prot_loc->SetId(feat_prod);
                        AdjustProteinFeaturePartialsToMatchCDS(**fit, feat);
                        if (!feat_prod.Equals(orig_cds.GetProduct().GetWhole())) {
                            // fix sequence ID
                            (*fit)->SetLocation().Assign(*new_prot_loc);
                        }
                    }
                    ++fit;
                }
            }
        }
    }

    return prot_seq;
}


void CFeatGapInfo::x_AdjustCodebreaks(CSeq_feat& feat)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() ||
        !feat.GetData().GetCdregion().IsSetCode_break()) {
        return;
    }
    CCdregion& cdr = feat.SetData().SetCdregion();
    CCdregion::TCode_break::iterator cit = cdr.SetCode_break().begin();
    while (cit != cdr.SetCode_break().end()) {
        bool do_remove = false;
        if ((*cit)->IsSetLoc()) {
            CRef<CSeq_loc> new_loc = feat.GetLocation().Intersect((*cit)->GetLoc(), 0, NULL);
            if (new_loc && !new_loc->IsEmpty() && !new_loc->IsNull()) {
                (*cit)->SetLoc().Assign(*new_loc);
            } else {
                do_remove = true;
            }
        }
        if (do_remove) {
            cit = cdr.SetCode_break().erase(cit);
        } else {
            ++cit;
        }
    }
    if (cdr.GetCode_break().empty()) {
        cdr.ResetCode_break();
    }
}


void CFeatGapInfo::x_AdjustAnticodons(CSeq_feat& feat)
{
    if (!feat.IsSetData() || !feat.GetData().IsRna() ||
        !feat.GetData().GetRna().IsSetExt() ||
        !feat.GetData().GetRna().GetExt().IsTRNA()) {
        return;
    }
    CTrna_ext& trna = feat.SetData().SetRna().SetExt().SetTRNA();
    if (!trna.IsSetAnticodon()) {
        return;
    }
    CRef<CSeq_loc> new_loc = feat.GetLocation().Intersect(trna.GetAnticodon(), 0, NULL);
    if (new_loc && !new_loc->IsEmpty() && !new_loc->IsNull()) {
        trna.SetAnticodon().Assign(*new_loc);
    } else {
        trna.ResetAnticodon();
    }
}


void s_FixPartial(CSeq_feat& feat)
{
    if (feat.GetLocation().IsPartialStart(eExtreme_Biological) ||
        feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
        feat.SetPartial(true);
    }
}


// returns a list of features to replace the original
// if list is empty, feature should be removed
// list should only contain one element if split is not specified
// coding regions should be retranslated after split
vector<CRef<CSeq_feat> > CFeatGapInfo::AdjustForRelevantGapIntervals(bool make_partial, bool trim, bool split, bool in_intron, bool create_general_only)
{
    CRef<objects::CSeq_feat> new_feat(new CSeq_feat);
    new_feat->Assign(*m_Feature.GetOriginalSeq_feat());
    vector<CRef<CSeq_feat> > rval;

    if (!trim && !split) {
        rval.push_back(new_feat);
        return rval;
    } else if (ShouldRemove()) {
        return rval;
    }

    if (trim && Trimmable()) {
        Trim(new_feat->SetLocation(), make_partial, m_Feature.GetScope());
        new_feat->SetPartial(new_feat->GetLocation().IsPartialStart(objects::eExtreme_Positional) || new_feat->GetLocation().IsPartialStop(objects::eExtreme_Positional));
        if (new_feat->GetData().IsCdregion()) {
            // adjust frame
            TSeqPos frame_adjust = sequence::LocationOffset(m_Feature.GetLocation(), new_feat->GetLocation(),
                sequence::eOffset_FromStart, &(m_Feature.GetScope()));
            x_AdjustFrame(new_feat->SetData().SetCdregion(), frame_adjust);
        }
    }

    if (split) {
        const string cds_gap_comment = "coding region disrupted by sequencing gap";

        vector<CRef<CSeq_loc> > locs = Split(new_feat->GetLocation(), in_intron, make_partial);
        if (locs.size() > 0) {
            // set comment
            if (!new_feat->IsSetComment())
            {
                new_feat->SetComment(cds_gap_comment);
            } else if (new_feat->IsSetComment() && new_feat->GetComment().find(cds_gap_comment) == string::npos)
            {
                string comment = new_feat->GetComment();
                comment = comment + "; " + cds_gap_comment;
                new_feat->SetComment(comment);
            }

            // adjust transcript id if splitting
            size_t transcript_id_offset = 0;
            string transcript_id_label;
            size_t protein_id_offset = 0;
            string protein_id_label;
            int protein_seqid_offset = 0;
            string protein_seqid_label;


            ITERATE(vector<CRef<CSeq_loc> >, lit, locs) {
                CRef<CSeq_feat> split_feat(new CSeq_feat());
                // copy from original
                split_feat->Assign(*new_feat);
                // with new location
                split_feat->SetLocation().Assign(**lit);
                split_feat->SetPartial(split_feat->GetLocation().IsPartialStart(eExtreme_Positional) || new_feat->GetLocation().IsPartialStop(eExtreme_Positional));
                //adjust transcript id
                x_AdjustOrigLabel(*split_feat, transcript_id_offset, transcript_id_label, "orig_transcript_id");
                x_AdjustOrigLabel(*split_feat, protein_id_offset, protein_id_label, "orig_protein_id");
                if (split_feat->GetData().IsCdregion()) {
                    // adjust frame
                    TSeqPos frame_adjust = sequence::LocationOffset(new_feat->GetLocation(), split_feat->GetLocation(),
                        sequence::eOffset_FromStart, &(m_Feature.GetScope()));
                    x_AdjustFrame(split_feat->SetData().SetCdregion(), frame_adjust);
                    // adjust product ID
                    if (split_feat->IsSetProduct() && split_feat->GetProduct().IsWhole() && protein_seqid_offset > 0) {
                        objects::CBioseq_Handle product = m_Feature.GetScope().GetBioseqHandle(split_feat->GetProduct());
                        CRef<CSeq_id> new_id;
                        if (product)
                        {
                            vector<CRef<objects::CSeq_id> > new_ids = objects::edit::GetNewProtIdFromExistingProt(product, protein_seqid_offset, protein_seqid_label);
                            new_id = FindBestChoice(new_ids, CSeq_id::Score);
                        }
                        else
                        {
                            objects::CBioseq_Handle bsh = m_Feature.GetScope().GetBioseqHandle(split_feat->GetLocation());
                            new_id = objects::edit::GetNewProtId(bsh, protein_seqid_offset, protein_seqid_label, create_general_only);
                        }
                        split_feat->SetProduct().SetWhole().Assign(*new_id);
                    }
                    protein_seqid_offset++;                       
                }
                rval.push_back(split_feat);
            }

        } else {
            rval.push_back(new_feat);
        }
    } else {
        rval.push_back(new_feat);
    }
    NON_CONST_ITERATE(vector<CRef<CSeq_feat> >, it, rval) {
        x_AdjustCodebreaks(**it);
        x_AdjustAnticodons(**it);
        s_FixPartial(**it);
    }
    return rval;
}


void CFeatGapInfo::x_AdjustFrame(CCdregion& cdregion, TSeqPos frame_adjust)
{
    frame_adjust = frame_adjust % 3;
    if (frame_adjust == 0) {
        return;
    } 

    CCdregion::TFrame orig_frame = cdregion.SetFrame();
    if (orig_frame == CCdregion::eFrame_not_set) {
        orig_frame = CCdregion::eFrame_one;
    }
    
    if (frame_adjust == 1) {
        if (orig_frame == CCdregion::eFrame_one) {
            cdregion.SetFrame(CCdregion::eFrame_three); //
        } else if (orig_frame == CCdregion::eFrame_two) {
            cdregion.SetFrame(CCdregion::eFrame_one); //
        } else if (orig_frame == CCdregion::eFrame_three) {
            cdregion.SetFrame(CCdregion::eFrame_two);
        }
    } else if (frame_adjust == 2) {
        if (orig_frame == CCdregion::eFrame_one) {
            cdregion.SetFrame(CCdregion::eFrame_two);
        } else if (orig_frame == CCdregion::eFrame_two) {
            cdregion.SetFrame(CCdregion::eFrame_three);
        } else if (orig_frame == CCdregion::eFrame_three) {
            cdregion.SetFrame(CCdregion::eFrame_one); //
        }
    }
}


TGappedFeatList ListGappedFeatures(CFeat_CI& feat_it, CScope& scope)
{
    TGappedFeatList gapped_feats;
    while (feat_it) {
        if (!feat_it->GetData().IsProt()) {
            CRef<CFeatGapInfo> fgap(new CFeatGapInfo(*feat_it));
            if (fgap->HasKnown() || fgap->HasUnknown() || fgap->HasNs()) {
                gapped_feats.push_back(fgap);
            }
        }
        ++feat_it;
    }
    return gapped_feats;
}


void ProcessForTrimAndSplitUpdates(CSeq_feat_Handle cds, vector<CRef<CSeq_feat> > updates)
{
    CBioseq_Handle orig_prot_handle = cds.GetScope().GetBioseqHandle(cds.GetProduct());
    CConstRef<CBioseq> orig_prot = orig_prot_handle.GetCompleteBioseq();
    CBioseq_set_Handle nph = orig_prot_handle.GetParentBioseq_set();
    CBioseq_set_EditHandle npeh(nph);
    // need to remove original first if not changing sequence IDs
    CBioseq_EditHandle beh(orig_prot_handle);
    beh.Remove();
    ITERATE(vector<CRef<CSeq_feat> >, it, updates) {
        CRef<CBioseq> new_prot = CFeatGapInfo::AdjustProteinSeq(*orig_prot, **it, *(cds.GetSeq_feat()), cds.GetScope());
        if (new_prot) {
            npeh.AttachBioseq(*new_prot);
        }
    }

    CSeq_annot_Handle sah = cds.GetAnnot();
    CSeq_annot_EditHandle saeh = sah.GetEditHandle();
    CSeq_feat_EditHandle feh(cds);
    if (updates.size() == 0) {
        feh.Remove();
    } else {
        feh.Replace(*(updates[0]));
        for (size_t i = 1; i < updates.size(); i++) {
            saeh.AddFeat(*(updates[i]));
        }
    }
}



// for fixing feature IDs and feature ID xrefs
void FixFeatureIdsForUpdates(CSeq_feat& feat, CObject_id::TId& next_id)
{
    if (feat.IsSetId() && feat.GetId().IsLocal() &&
        feat.GetId().GetLocal().IsId()) {
        feat.SetId().SetLocal().SetId(next_id);
        ++next_id;
    }

}


void FixFeatureIdsForUpdates(vector<CRef<CSeq_feat> > updates, CObject_id::TId& next_id)
{
    for (size_t i = 1; i < updates.size(); i++) {
        FixFeatureIdsForUpdates(*(updates[i]), next_id);
    }
}


bool s_IsRelated(const CSeq_feat& f1, CObject_id::TId search)
{
    if (!f1.IsSetXref()) {
        return false;
    }
    ITERATE(CSeq_feat::TXref, xit, f1.GetXref()) {
        if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal() &&
            (*xit)->GetId().GetLocal().IsId() &&
            (*xit)->GetId().GetLocal().GetId() == search) {
            return true;
        }
    }
    return false;
}


bool s_IsRelated(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (f1.IsSetId() && f1.GetId().IsLocal() && f1.GetId().GetLocal().IsId() &&
        s_IsRelated(f2, f1.GetId().GetLocal().GetId())) {
        return true;
    } else if (f2.IsSetId() && f2.GetId().IsLocal() && f2.GetId().GetLocal().IsId() &&
        s_IsRelated(f1, f2.GetId().GetLocal().GetId())) {
        return true;
    } else {
        return false;
    }
}


bool CFeatGapInfo::IsRelatedByCrossRef(const CFeatGapInfo& other) const
{
    return s_IsRelated(*(GetFeature().GetSeq_feat()), *(other.GetFeature().GetSeq_feat()));
}



void s_ReplaceFeatureIdXref(CSeq_feat& f, CObject_id::TId orig_id, CObject_id::TId new_id)
{
    if (orig_id > 0 && new_id > 0 && f.IsSetXref()) {
        NON_CONST_ITERATE(CSeq_feat::TXref, xit, f.SetXref()) {
            if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal() &&
                (*xit)->GetId().GetLocal().IsId() &&
                (*xit)->GetId().GetLocal().GetId() == orig_id) {
                (*xit)->SetId().SetLocal().SetId(new_id);
            }
        }
    }
}


void FixFeatureIdsForUpdatePair(vector<CRef<CSeq_feat> >& updates1, vector<CRef<CSeq_feat> >& updates2)
{
    if (updates1.size() != updates2.size()) {
        // can't fix if lists are different lengths
        return;
    }
    if (updates1.size() < 2) {
        // nothing to fix if there's only one object
    }
    vector<CRef<CSeq_feat> >::iterator u1 = updates1.begin();
    vector<CRef<CSeq_feat> >::iterator u2 = updates2.begin();

    CObject_id::TId orig_id_1 = 0;
    if ((*u1)->IsSetId() && (*u1)->GetId().IsLocal() && (*u1)->GetId().GetLocal().IsId()) {
        orig_id_1 = (*u1)->GetId().GetLocal().GetId();
    }
    CObject_id::TId orig_id_2 = 0;
    if ((*u2)->IsSetId() && (*u2)->GetId().IsLocal() && (*u2)->GetId().GetLocal().IsId()) {
        orig_id_2 = (*u2)->GetId().GetLocal().GetId();
    }
    u1++;
    u2++;

    while (u1 != updates1.end() && u2 != updates2.end()) {
        CObject_id::TId new_id_1 = 0;
        if ((*u1)->IsSetId() && (*u1)->GetId().IsLocal() && (*u1)->GetId().GetLocal().IsId()) {
            new_id_1 = (*u1)->GetId().GetLocal().GetId();
        }
        CObject_id::TId new_id_2 = 0;
        if ((*u2)->IsSetId() && (*u2)->GetId().IsLocal() && (*u2)->GetId().GetLocal().IsId()) {
            new_id_2 = (*u2)->GetId().GetLocal().GetId();
        }
        s_ReplaceFeatureIdXref(**u1, orig_id_2, new_id_2);
        s_ReplaceFeatureIdXref(**u2, orig_id_1, new_id_1);
        ++u1;
        ++u2;
    }
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
