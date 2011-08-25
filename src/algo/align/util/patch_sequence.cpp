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


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRef<CSeq_inst> PatchTargetSequence(CRef<CSeq_align> alignment, CScope &scope)
{
    list< CRef<CSeq_align> > singleton_list;
    singleton_list.push_back(alignment);
    return PatchTargetSequence(singleton_list, scope);
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

void s_InsertPatchSequence(CDelta_ext::Tdata &patched_contents,
                           CDelta_ext::Tdata::iterator patched_contents_it,
                           CRef<CSeq_align> patch,
                           CBioseq_Handle patch_sequence,
                           bool complete_patch)
{
    TSeqRange aligned_range =
        complete_patch ? TSeqRange(0, patch_sequence.GetBioseqLength()-1)
                       : patch->GetSeqRange(0);
    CRef<CSeq_id> patch_id(new CSeq_id);
    patch_id->Assign(patch->GetSeq_id(0));
    CRef<CSeq_loc> patch_loc(new CSeq_loc(*patch_id, aligned_range.GetFrom(),
                                          aligned_range.GetTo(),
                                          patch->GetSeqStrand(0)));
    CRef<CDelta_seq> inserted_patch(new CDelta_seq);
    inserted_patch->SetLoc(*patch_loc);
    patched_contents.insert(patched_contents_it, inserted_patch);
}

CRef<CSeq_inst> PatchTargetSequence(const list< CRef<CSeq_align> > &alignments,
                                    CScope &scope)
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
    unsigned range_count = 0;

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
        if (alignment->GetSegs().IsDisc() &&
            alignment->GetSegs().GetDisc().Get().size() != 2)
        {
            NCBI_THROW(CException, eUnknown,
                "Not implemented for Disc alignments of more than two segs");
        }
        TSeqRange covered_range = alignment->GetSeqRange(1);
        cover_map[covered_range] = alignment;
        total_covered += covered_range;
        /// Covered ranges should all be separa5te from each other, so the number of ranges
        /// in total_covered should be equal to the number of ranges we put in
        if (total_covered.size() < ++range_count) {
            NCBI_THROW(CException, eUnknown, "fix patches on intersecting regions");
        }
    }
    TCoverMap::const_iterator next_patch = cover_map.begin();

    CRef<CSeq_inst> patched_seq(new CSeq_inst);
    patched_seq->Assign(target.GetInst());
    CDelta_ext::Tdata &patched_contents = patched_seq->SetExt().SetDelta().Set();
    TSignedSeqPos length_change = 0;
    TSeqPos current_pos = 0, next_pos = 0;

    NON_CONST_ITERATE (CDelta_ext::Tdata, it, patched_contents) {
        if ((*it)->IsLiteral()) {
            next_pos += (*it)->GetLiteral().GetLength();
            LOG_POST(Info << "Literal end at " << next_pos);
            if (next_pos > next_patch->first.GetFrom()) {
                NCBI_THROW(CException, eUnknown,
                           "Patch on location " +
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
            CBioseq_Handle patch_sequence =
                scope.GetBioseqHandle(next_patch->second->GetSeq_id(0));
            if (!patch_sequence) {
                NCBI_THROW(CException, eUnknown, "Can't get patch sequence");
            }

            TSeqPos patch_length = patch_sequence.GetBioseqLength();
            TSeqRange aligned_patch_range = next_patch->second->GetSeqRange(0);
            CRef<CSeq_align> patch, left_patch, right_patch;
            if (next_patch->second->GetSegs().IsDisc()) {
                if (aligned_patch_range.GetLength() < patch_length)
                {
                    NCBI_THROW(CException, eUnknown,
                               "Disc-seg patch alignments require that the "
                               "patch sequence by fully aligned");
                }
                left_patch = next_patch->second->GetSegs().GetDisc().Get().front();
                right_patch = next_patch->second->GetSegs().GetDisc().Get().back();
                if (left_patch->GetSeqRange(1) > right_patch->GetSeqRange(1)) {
                    CRef<CSeq_align> temp = left_patch;
                    left_patch = right_patch;
                    right_patch = temp;
                }
            } else {
                patch = left_patch = right_patch = next_patch->second;
            }
            if (left_patch->GetSeqStrand(1) != eNa_strand_plus ||
                right_patch->GetSeqStrand(1) != eNa_strand_plus)
            {
                NCBI_THROW(CException, eUnknown,
                           "Expect all patch alignments to align to target's "
                           "plus strand");
            }

            pair<bool,bool> must_abut_gap(false, false);
            bool gap_abut_problem = false;
            if (aligned_patch_range.GetFrom() > 0) {
                (patch->GetSeqStrand(0) == eNa_strand_minus
                    ? must_abut_gap.second : must_abut_gap.first)
                    = true;
            }
            if (aligned_patch_range.GetToOpen() < patch_length) {
                (patch->GetSeqStrand(0) == eNa_strand_minus
                    ? must_abut_gap.first : must_abut_gap.second)
                    = true;
            }
            if (must_abut_gap.first) {
                if (current_pos < next_patch->first.GetFrom()) {
                    gap_abut_problem = true;
                } else if (it != patched_contents.begin()) {
                    if (!(*--it)->IsLiteral() || 
                        !(*it)->GetLiteral().GetSeq_data().IsGap())
                    {
                        gap_abut_problem = true;
                    }
                    ++it;
                }
            }
            if (must_abut_gap.second) {
                if (next_pos > next_patch->first.GetToOpen()) {
                    gap_abut_problem = true;
                } else {
                    if (++it != patched_contents.end() &&
                        (!(*it)->IsLiteral() || 
                        !(*it)->GetLiteral().GetSeq_data().IsGap()))
                    {
                        gap_abut_problem = true;
                    }
                    --it;
                }
	    }
            if (gap_abut_problem) {
                NCBI_THROW(CException, eUnknown,
                    "patch has unaligned tail but does not abut gap");
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
            if (patch) {
                s_InsertPatchSequence(patched_contents, it,
                                      patch, patch_sequence, true);
                length_change += patch_length;
            } else {
                s_InsertPatchSequence(patched_contents, it,
                                      left_patch, patch_sequence, false);
                s_InsertPatchSequence(patched_contents, it,
                                      right_patch, patch_sequence, false);
                length_change += left_patch->GetSeqRange(0).GetLength()
                               + right_patch->GetSeqRange(0).GetLength();
            }

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

