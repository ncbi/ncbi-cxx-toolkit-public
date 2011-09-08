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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <util/range_coll.hpp>

#include <algo/align/util/patch_sequence.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRef<CSeq_inst> PatchTargetSequence(CRef<CSeq_align> alignment, CScope &scope,
                                    TSeqPos max_ignored_tail)
{
    list< CRef<CSeq_align> > singleton_list;
    singleton_list.push_back(alignment);
    return PatchTargetSequence(singleton_list, scope, max_ignored_tail);
}

CRef<CDelta_seq> s_SubLocDeltaSeq(const CSeq_loc &loc, 
                                  TSeqPos start, TSeqPos length = 0)
{
    if (!loc.IsInt()) {
        NCBI_THROW(CException, eUnknown,
          "sequence contains unsupported type of CSeq_loc");
    }
    CRef<CDelta_seq> result(new CDelta_seq);
    result->SetLoc().Assign(loc);
    CSeq_interval &interval = result->SetLoc().SetInt();
    if (start) {
        interval.SetFrom(interval.GetFrom() + start);
    }
    if (length) {
        interval.SetTo(interval.GetFrom() + length - 1);
    }
    return result;
}

CRef<CSeq_inst> PatchTargetSequence(const list< CRef<CSeq_align> > &alignments,
                                    CScope &scope,
                                    TSeqPos max_ignored_tail)
{
    CBioseq_Handle target =
        scope.GetBioseqHandle(alignments.front()->GetSeq_id(1));
    if (!target) {
        NCBI_THROW(CException, eUnknown, "Can't get target sequence");
    }
    if(!target.CanGetInst_Repr() ||
       target.GetInst_Repr() != CSeq_inst::eRepr_delta)
    {
        NCBI_THROW(CException, eUnknown,
                   "PatchTargetSequence() implemented only for sequences "
                   "with delta representation");
    }

    typedef map<TSeqRange, CRef<CSeq_align> > TCoverMap;
    TCoverMap cover_map;

    CRangeCollection<TSeqPos> total_covered;

    ITERATE (list< CRef<CSeq_align> >, it, alignments) {
        CRef<CSeq_align> alignment = *it;
        if (alignment->CheckNumRows() != 2) {
            NCBI_THROW(CException, eUnknown,
                "Alignment does not have two rows");
        }
        if (!(target.GetSeq_id_Handle() == alignment->GetSeq_id(1))) {
            NCBI_THROW(CException, eUnknown,
                "Alignments in list are patching different sequences");
        }
        TSeqRange covered_range = alignment->GetSeqRange(1);
        CRef<CSeq_align> &old_alignment = cover_map[covered_range];
        if (!old_alignment) {
            old_alignment = alignment;
            if (total_covered.IntersectingWith(covered_range)) {
                ITERATE (list< CRef<CSeq_align> >, it2, alignments) {
                    LOG_POST(Error << "Patches on "
                                   << target.GetSeq_id_Handle().AsString());
                    LOG_POST(Error << (*it2)->GetSeq_id(0).AsFastaString()
                                   << " covers " << (*it2)->GetSeqStart(1)
                                   << " - " << (*it2)->GetSeqStop(1));
                }
                NCBI_THROW(CException, eUnknown, "fix patches on intersecting regions");
            }
            total_covered += covered_range;
        } else if (!old_alignment->GetSeq_id(0).IsGi() ||
                   !alignment->GetSeq_id(0).IsGi())
        {
            NCBI_THROW(CException, eUnknown,
                       "Several patches on same range; need gis to choose newest");

        } else if (old_alignment->GetSeq_id(0).GetGi() <
                   alignment->GetSeq_id(0).GetGi())
        {
            /// Several alignments on same range; take the one with the higher-gi
            /// patch sequence, which is assumed to be newer
            old_alignment = alignment;
        }
    }
    TCoverMap::const_iterator next_patch = cover_map.begin();

    CRef<CSeq_inst> patched_seq(new CSeq_inst);
    patched_seq->Assign(target.GetInst());
    CDelta_ext::Tdata &patched_contents = patched_seq->SetExt().SetDelta().Set();
    TSignedSeqPos length_change = 0;
    TSeqPos current_pos = 0, next_pos = 0;
    TSignedSeqPos next_gap_change = 0;

    NON_CONST_ITERATE (CDelta_ext::Tdata, it, patched_contents) {
        if ((*it)->IsLiteral()) {
            next_pos += (*it)->GetLiteral().GetLength() + next_gap_change;
            next_gap_change = 0;
            LOG_POST(Info << "Literal end at " << next_pos);
            if (next_pos > next_patch->first.GetFrom()) {
                NCBI_THROW(CException, eUnknown,
                           target.GetSeq_id_Handle().AsString() + ": Patch "
                           + next_patch->second->GetSeq_id(0).AsFastaString()
                           + " on location " +
                           NStr::UIntToString(next_patch->first.GetFrom()) +
                           " which is in sequence gap");
            }
            continue;
        }
        CSeq_loc target_loc;
        target_loc.Assign((*it)->GetLoc());
        current_pos = next_pos;
        next_pos += target_loc.GetTotalRange().GetLength();
        LOG_POST(Info << "Loc end at " << next_pos);
        for (; next_patch != cover_map.end() &&
               next_pos > next_patch->first.GetFrom(); ++next_patch)
        {
        try {
            CBioseq_Handle patch_sequence =
                scope.GetBioseqHandle(next_patch->second->GetSeq_id(0));
            if (!patch_sequence) {
                NCBI_THROW(CException, eUnknown, "Can't get patch sequence");
            }

            TSeqPos patch_length = patch_sequence.GetBioseqLength();
            TSeqRange aligned_patch_range = next_patch->second->GetSeqRange(0);
            CRef<CSeq_align> patch = next_patch->second;
            if (patch->GetSeqStrand(1) != eNa_strand_plus) {
                NCBI_THROW(CException, eUnknown,
                           "Expect all patch alignments to align to target's "
                           "plus strand");
            }
            ENa_strand patch_strand = patch->GetSeqStrand(0);
            if (patch->GetSegs().IsDisc()) {
                if (aligned_patch_range.GetLength() < patch_length)
                {
                    NCBI_THROW(CException, eUnknown,
                               "Disc-seg patch alignments require that the "
                               "patch sequence by fully aligned");
                }
                ITERATE (CSeq_align_set::Tdata, seg_it,
                         patch->GetSegs().GetDisc().Get())
                {
                    if ((*seg_it)->GetSeqStrand(1) != eNa_strand_plus) {
                        NCBI_THROW(CException, eUnknown,
                                   "Expect all patch alignments to align to "
                                   "target's plus strand");
                    }
                    if ((*seg_it)->GetSeqStrand(1) != patch_strand) {
                        // If not all segments have the same strand, use plus
                        patch_strand = eNa_strand_plus;
                    }
                }
            }

            pair<TSeqPos,TSeqPos> patch_tail(
                aligned_patch_range.GetFrom(),
                patch_length - aligned_patch_range.GetToOpen());
            bool gap_abut_problem = false;
            if(patch_strand == eNa_strand_minus)
            {
                TSeqPos temp = patch_tail.first;
                patch_tail.first = patch_tail.second;
                patch_tail.second = temp;
            }
            if (patch_tail.first > max_ignored_tail) {
                if (current_pos != next_patch->first.GetFrom() ||
                    it == patched_contents.begin())
                {
                    gap_abut_problem = true;
                } else {
                    if (!(*--it)->IsLiteral() || 
                        !(*it)->GetLiteral().GetSeq_data().IsGap())
                    {
                        gap_abut_problem = true;
                    } else {
                        CSeq_literal &gap = (*it)->SetLiteral();
                        TSignedSeqPos new_gap_size =
                            gap.GetLength() - patch_tail.first;
                        if (new_gap_size <= 0) {
                            new_gap_size = 50000;
                        }
                        length_change -= gap.GetLength() - new_gap_size;
                        gap.SetLength(new_gap_size);
                    }
                    ++it;
                }
            }
            if (patch_tail.second > max_ignored_tail) {
                if (next_pos != next_patch->first.GetToOpen()) {
                    gap_abut_problem = true;
                } else {
                    if (++it == patched_contents.end() ||
                        !(*it)->IsLiteral() || 
                        !(*it)->GetLiteral().GetSeq_data().IsGap())
                    {
                        gap_abut_problem = true;
                    } else {
                        CSeq_literal &gap = (*it)->SetLiteral();
                        TSignedSeqPos new_gap_size =
                            gap.GetLength() - patch_tail.second;
                        if (new_gap_size <= 0) {
                            new_gap_size = 50000;
                        }
                        length_change -= next_gap_change
                                       = gap.GetLength() - new_gap_size;
                        gap.SetLength(new_gap_size);
                    }
                    --it;
                }
	    }
            if (gap_abut_problem) {
                LOG_POST(Warning << "patch "
                    << sequence::GetId(patch_sequence, sequence::eGetId_Best)
                    << " on "
                    << sequence::GetId(target, sequence::eGetId_Best)
                    << " has unaligned tail of length "
                    << NStr::UIntToString(max(patch_tail.first,patch_tail.second))
                    << " but does not abut gap; skipping");
                continue;
            }

            /// Remove aligned region from target sequence
            unsigned int unaligned_left_length =
                next_patch->first.GetFrom() - current_pos;
            if (unaligned_left_length) {
                /// we have an unaligned area in the target loc to the left of the patch
                patched_contents.insert(it,
                    s_SubLocDeltaSeq(target_loc, 0, unaligned_left_length));
            }
            while (next_pos < next_patch->first.GetToOpen()) {
                it = patched_contents.erase(it);
                current_pos = next_pos;
                next_pos += (*it)->IsLiteral()
                          ? (*it)->GetLiteral().GetLength()
                          : (*it)->GetLoc().GetTotalRange().GetLength();
            }
            if (!(*it)->IsLoc()) {
                NCBI_THROW(CException, eUnknown,
                           "Patch on location " +
                           NStr::UIntToString(next_patch->first.GetTo()) +
                           " which is in sequence gap");
            }
            target_loc.Assign((*it)->GetLoc());
            it = patched_contents.erase(it);
            length_change -= next_patch->first.GetLength();

            /// Add aligned fix patch in place of the region we removed
            CRef<CSeq_id> patch_id(new CSeq_id);
            patch_id->Assign(patch->GetSeq_id(0));
            CRef<CSeq_loc> patch_loc(
                new CSeq_loc(*patch_id, 0, patch_length-1, patch_strand));
            CRef<CDelta_seq> inserted_patch(new CDelta_seq);
            inserted_patch->SetLoc(*patch_loc);
            patched_contents.insert(it, inserted_patch);
            length_change += patch_length;

            unsigned aligned_length =
                next_patch->first.GetToOpen() - current_pos;
            current_pos += aligned_length;
            if (current_pos == next_pos) {
                /// We're done with target loc, so we know we will exit from inner
                /// loop in this iteration. Iterator "it" now points to the next
                /// CDelta_seq we need to process in outer loop. Decrement it, so
                /// after it is incremented in the outer loop it will point here again
                --it;
            } else {
                /// we have an unaligned area in the last target loc to the
                /// right of the patch; insert it, and then continue to process
                /// it in the inner loop, to check if there are further patches to it
                it = patched_contents.insert(it,
                    s_SubLocDeltaSeq(target_loc, aligned_length));
                target_loc.Assign((*it)->GetLoc());
            }
        } catch (exception &) {
            LOG_POST(Error << "While applying patch "
                           << next_patch->second->GetSeq_id(0).AsFastaString()
                           << " on "
                           << next_patch->second->GetSeq_id(1).AsFastaString());
            throw;
        }
        }
        if (next_patch == cover_map.end()) {
            /// no more patches to apply, so we're done
            break;
        }
    }

    if (patched_seq->IsSetLength() && length_change) {
        patched_seq->SetLength(patched_seq->GetLength() + length_change);
    }
    return patched_seq;
}


END_NCBI_SCOPE

