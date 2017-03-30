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
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

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

    m_Start = feat_loc.GetStart(objects::eExtreme_Positional);
    m_Stop = feat_loc.GetStop(objects::eExtreme_Positional);
    CRef<CSeq_loc> total_loc = sequence::Seq_loc_Merge(feat_loc, CSeq_loc::fMerge_SingleRange, &scope);
    CConstRef<objects::CSeqMap> seq_map = CSeqMap::GetSeqMapForSeq_loc(*total_loc, &scope);

    CSeqMap_CI seq_map_ci = seq_map->ResolvedRangeIterator(&scope,
        0,
        objects::sequence::GetLength(feat_loc, &scope),
        feat_loc.GetStrand(),
        size_t(-1),
        objects::CSeqMap::fFindGap);

    for (; seq_map_ci; ++seq_map_ci)
    {

        if (seq_map_ci.GetType() != objects::CSeqMap::eSeqGap)
        {
            continue;
        }

        TSeqPos gap_start = m_Start + seq_map_ci.GetPosition();
        TSeqPos gap_stop = gap_start + seq_map_ci.GetLength() - 1;
        bool is_unknown = seq_map_ci.IsUnknownLength();
        if (is_unknown) {
            m_Unknown = true;
        } else {
            m_Known = true;
        }
        m_Gaps.push_back(TGapInterval(is_unknown, pair<size_t, size_t>(gap_start, gap_stop)));
    }
}


void CFeatGapInfo::CalculateRelevantIntervals(bool unknown_length, bool known_length)
{
    m_InsideGaps.clear();
    m_LeftGaps.clear();
    m_RightGaps.clear();

    if (!m_Gaps.empty()) {
        // find left offset
        size_t skip_left = 0;
        TGapIntervalList::iterator it = m_Gaps.begin();
        while (it != m_Gaps.end()) {
            if (it->first && !unknown_length) {
                break;
            } else if (!it->first && !known_length) {
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
            if (rit->first && !unknown_length) {
                break;
            } else if (!rit->first && !known_length) {
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
            if ((m_Gaps[offset].first && unknown_length) || (!m_Gaps[offset].first && known_length)) {
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
    for (vector<pair<size_t, size_t> >::reverse_iterator b = m_LeftGaps.rbegin(); b != m_LeftGaps.rend(); ++b)
    {
        size_t start = b->first;
        size_t stop = b->second;
        CRef<CSeq_loc> loc2(new CSeq_loc());
        int options = edit::eSplitLocOption_split_in_exon;
        if (make_partial)
            options |= edit::eSplitLocOption_make_partial;
        edit::SplitLocationForGap(loc, *loc2, start, stop, loc.GetId(), options);
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
        edit::SplitLocationForGap(loc, *loc2, start, stop, loc.GetId(), options);
    }
}


CFeatGapInfo::TLocList CFeatGapInfo::Split(const CSeq_loc& orig, bool in_intron, bool make_partial)
{
    TLocList locs;
    CRef<CSeq_loc> left_loc(new CSeq_loc());
    left_loc->Assign(orig);
    for (vector<pair<size_t, size_t> >::reverse_iterator b = m_InsideGaps.rbegin(); b != m_InsideGaps.rend(); ++b)
    {
        size_t start = b->first;
        size_t stop = b->second;
        CRef<CSeq_loc> loc2(new CSeq_loc());
        int options = edit::eSplitLocOption_make_partial | edit::eSplitLocOption_split_in_exon;
        if (in_intron)
            options |= edit::eSplitLocOption_split_in_intron;
        edit::SplitLocationForGap(*left_loc, *loc2, start, stop, orig.GetId(), options);
        if (loc2->GetId())
        {
            if (make_partial)
            {
                loc2->SetPartialStart(true, objects::eExtreme_Positional);
                left_loc->SetPartialStop(true, objects::eExtreme_Positional);
            }
            locs.push_back(loc2);
        }
    }
    if (locs.size() > 0) {
        locs.push_back(left_loc);
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
            (*it)->SetVal(id_label + "_" + NStr::NumericToString(id_offset));
            id_offset++;
        }
    }
}


// returns a list of features to replace the original
// if list is empty, feature should be removed
// list should only contain one element if split is not specified
// coding regions should be retranslated after split
vector<CRef<CSeq_feat> > CFeatGapInfo::AdjustForRelevantGapIntervals(bool make_partial, bool trim, bool split, bool in_intron)
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
            size_t transcript_id_offset = 1;
            string transcript_id_label = kEmptyStr;
            size_t protein_id_offset = 1;
            string protein_id_label = kEmptyStr;

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
                // adjust frame
                if (split_feat->GetData().IsCdregion()) {
                    TSeqPos frame_adjust = sequence::LocationOffset(new_feat->GetLocation(), split_feat->GetLocation(),
                        sequence::eOffset_FromStart, &(m_Feature.GetScope()));
                    x_AdjustFrame(split_feat->SetData().SetCdregion(), frame_adjust);
                }

                rval.push_back(split_feat);
            }

        } else {
            rval.push_back(new_feat);
        }
    } else {
        rval.push_back(new_feat);
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
            cdregion.SetFrame(CCdregion::eFrame_two);
        } else if (orig_frame == CCdregion::eFrame_two) {
            cdregion.SetFrame(CCdregion::eFrame_three);
        } else if (orig_frame == CCdregion::eFrame_three) {
            cdregion.SetFrame(CCdregion::eFrame_one);
        }
    } else if (frame_adjust == 2) {
        if (orig_frame == CCdregion::eFrame_one) {
            cdregion.SetFrame(CCdregion::eFrame_three);
        } else if (orig_frame == CCdregion::eFrame_two) {
            cdregion.SetFrame(CCdregion::eFrame_one);
        } else if (orig_frame == CCdregion::eFrame_three) {
            cdregion.SetFrame(CCdregion::eFrame_two);
        }
    }
}


TGappedFeatList ListGappedFeatures(CFeat_CI& feat_it, CScope& scope)
{
    TGappedFeatList gapped_feats;
    while (feat_it) {
        if (!feat_it->GetData().IsProt()) {
            CRef<CFeatGapInfo> fgap(new CFeatGapInfo(*feat_it));
            if (fgap->HasKnown() || fgap->HasUnknown()) {
                gapped_feats.push_back(fgap);
            }
        }
        ++feat_it;
    }
    return gapped_feats;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
