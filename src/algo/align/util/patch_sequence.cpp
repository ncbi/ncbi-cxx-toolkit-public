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
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static set<CSeq_id_Handle> s_NovelPatches;

void AddNovelPatch(const CSeq_id_Handle &idh)
{
    s_NovelPatches.insert(idh);
}

CRef<CSeq_inst> PatchTargetSequence(CRef<CSeq_align> alignment, CScope &scope)
{
    list< CRef<CSeq_align> > singleton_list;
    singleton_list.push_back(alignment);
    return PatchTargetSequence(singleton_list, scope);
}

static CRef<CDelta_seq> s_SubLocDeltaSeq(const CSeq_loc &loc, 
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

static void s_SubtractTail(CSeq_literal &gap, TSeqPos tail)
{
    TSeqPos gap_size = gap.GetLength();
    if (gap_size > tail) {
        gap_size -= tail;
    } else {
        switch (gap.GetSeq_data().GetGap().GetType()) {
        case CSeq_gap::eType_centromere:
            gap_size = 3000000;
            break;
        case CSeq_gap::eType_telomere:
            gap_size = 10000;
            break;
        case CSeq_gap::eType_contig:
        case CSeq_gap::eType_clone:
            gap_size = 50000;
            break;
        default:
            NCBI_THROW(CException, eUnknown, "Unsupported gap type");
        }
    }
    if (gap_size < gap.GetLength()) {
        gap.SetLength(gap_size);
    }
}

static void s_ProcessIntraScaffoldTail(CBioseq_Handle patch_sequence,
                                       CBioseq_Handle scaffold,
                                       TSeqPos loc_start,
                                       TSeqPos &patch_boundary,
                                       TSeqPos tail_length,
                                       CRef<CDelta_seq> &inserted_gap,
                                       TSignedSeqPos &length_change,
                                       bool is_right_tail)
{
    if (!scaffold) {
        NCBI_THROW(CException, eUnknown, "Can't get scaffold");
    }
    if(!scaffold.CanGetInst_Repr() ||
       scaffold.GetInst_Repr() != CSeq_inst::eRepr_delta)
    {
        NCBI_THROW(CException, eUnknown,
                   "Scaffold does not have delta representation");
    }

    TSeqPos current_pos = 0;

    CDelta_ext::Tdata::const_iterator delta_it =
        scaffold.GetInst().GetExt().GetDelta().Get().begin();
    for (; current_pos < loc_start+patch_boundary; ++delta_it) {
        current_pos += (*delta_it)->IsLiteral()
                     ? (*delta_it)->GetLiteral().GetLength()
                     : (*delta_it)->GetLoc().GetTotalRange().GetLength();
    }
    /// If alignment ends in the middle of a delta-seq, we're now pointing
    /// one ahead of it, so move it back. If alignment ends on the boundary
    /// between two delta-seqs, make sure delta_it points to the one
    /// unaligned tail is on
    if (!is_right_tail || current_pos > loc_start+patch_boundary) {
        --delta_it;
    }
    if ((*delta_it)->IsLiteral()
        && (*delta_it)->GetLiteral().GetSeq_data().IsGap())
    {
        if (current_pos == loc_start+patch_boundary) {
            /// alignment abuts an intra-scaffold gap
            inserted_gap = *delta_it;
            TSeqPos previous_gap_size =
                inserted_gap->GetLiteral().GetLength();
            if (is_right_tail) {
                patch_boundary += previous_gap_size;
            } else {
                patch_boundary -= previous_gap_size;
            }
            s_SubtractTail(inserted_gap->SetLiteral(), tail_length);
            length_change -= previous_gap_size
                           - inserted_gap->GetLiteral().GetLength();
            return;
       } else {
           NCBI_THROW(CException, eUnknown,
               "patch alignment boundary in the middle of intra-scaffold gap");
       }
    }

    /// alignment does not abut a gap; only allowed for NOVEL patches
    if (!s_NovelPatches.count(
        sequence::GetId(patch_sequence, sequence::eGetId_Canonical)))
    {
        NCBI_THROW(CException, eUnknown,
                 "non-NOVEL patch has unaligned tail of length " +
                 NStr::UIntToString(tail_length) + " that does not abut a gap");
    }

    /// If boundary is in the middle of a location, eliminate the remainder 
    /// of that location
    if (current_pos > loc_start+patch_boundary) {
        if (is_right_tail) {
            length_change -= current_pos - loc_start - patch_boundary;
        } else {
            current_pos -= (*delta_it)->GetLoc().GetTotalRange().GetLength();
            length_change -= loc_start + patch_boundary - current_pos;
        }
        patch_boundary = current_pos-loc_start;
    }

    /// insert new gap
    inserted_gap.Reset(new CDelta_seq);
    inserted_gap->SetLiteral().SetLength(50000);
    inserted_gap->SetLiteral().SetSeq_data().SetGap()
        .SetType(CSeq_gap::eType_clone);
    inserted_gap->SetLiteral().SetSeq_data().SetGap()
        .SetLinkage(CSeq_gap::eLinkage_linked);
    length_change += 50000;
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
        CBioseq_Handle patch_sequence;
        for (; next_patch != cover_map.end() &&
               next_pos > next_patch->first.GetFrom(); ++next_patch)
        {
        try {
            patch_sequence =
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
            if(patch_strand == eNa_strand_minus)
            {
                TSeqPos temp = patch_tail.first;
                patch_tail.first = patch_tail.second;
                patch_tail.second = temp;
            }

            /// Remove aligned region from target sequence
            TSeqPos unaligned_left_length =
                next_patch->first.GetFrom() - current_pos;
            if (unaligned_left_length) {
                CRef<CDelta_seq> inserted_gap;
                if (patch_tail.first) {
                    if (target_loc.GetStrand() != eNa_strand_plus) {
                        NCBI_THROW(CException, eUnknown,
                                   "Scaffold locations expected to always "
                                   "be on plus strand");
                    }
                    CBioseq_Handle scaffold =
                        scope.GetBioseqHandle(*target_loc.GetId());
                    s_ProcessIntraScaffoldTail(patch_sequence, scaffold,
                                               target_loc.GetStart(eExtreme_Positional),
                                               unaligned_left_length,
                                               patch_tail.first, inserted_gap,
                                               length_change, false);
                }
                /// we have an unaligned area in the target loc to the left of the patch
                patched_contents.insert(it,
                    s_SubLocDeltaSeq(target_loc, 0, unaligned_left_length));
                if (inserted_gap) {
                    patched_contents.insert(it, inserted_gap);
                }
            } else if(patch_tail.first) {
                if (it == patched_contents.begin() ||
                    !(*--it)->IsLiteral() || 
                    !(*it)->GetLiteral().GetSeq_data().IsGap())
                {
                    NCBI_THROW(CException, eUnknown,
                        "unaligned tail of length "
                        + NStr::UIntToString(patch_tail.first)
                        + " not abutting a gap");
                }
                TSeqPos previous_gap_size =
                    (*it)->GetLiteral().GetLength();
                s_SubtractTail((*it)->SetLiteral(), patch_tail.first);
                length_change -= previous_gap_size
                               - (*it)->GetLiteral().GetLength();
                ++it;
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

            TSeqPos length_to_replace =
                next_patch->first.GetToOpen() - current_pos;
            if(current_pos + length_to_replace < next_pos) {
                CRef<CDelta_seq> inserted_gap;
                if (patch_tail.second) {
                    CBioseq_Handle scaffold =
                        scope.GetBioseqHandle(*target_loc.GetId());
                    if (target_loc.GetStrand() != eNa_strand_plus) {
                        NCBI_THROW(CException, eUnknown,
                                   "Scaffold locations expected to always "
                                   "be on plus strand");
                    }
                    s_ProcessIntraScaffoldTail(patch_sequence, scaffold,
                                               target_loc.GetStart(eExtreme_Positional),
                                               length_to_replace,
                                               patch_tail.second, inserted_gap,
                                               length_change, true);
                    if (inserted_gap) {
                        patched_contents.insert(it, inserted_gap);
                    }
                }
                /// we have an unaligned area in the last target loc to the
                /// right of the patch; insert it
                CRef<CDelta_seq> scaffold_remainder =
                    s_SubLocDeltaSeq(target_loc, length_to_replace);
                target_loc.Assign(scaffold_remainder->GetLoc());
                patched_contents.insert(it, scaffold_remainder);
            } else if(patch_tail.second) {
                if (it == patched_contents.end() ||
                    !(*it)->IsLiteral() || 
                    !(*it)->GetLiteral().GetSeq_data().IsGap())
                {
                    NCBI_THROW(CException, eUnknown,
                        "unaligned tail of length "
                        + NStr::UIntToString(patch_tail.second)
                        + " not abutting a gap");
                }
                TSeqPos previous_gap_size =
                    (*it)->GetLiteral().GetLength();
                s_SubtractTail((*it)->SetLiteral(), patch_tail.second);
                length_change -= next_gap_change =
                    previous_gap_size - (*it)->GetLiteral().GetLength();
            }
            current_pos += length_to_replace;
            --it;
        } catch (exception &e) {
            LOG_POST(Error << "While applying patch "
                           << sequence::GetId(patch_sequence,
                                              sequence::eGetId_Best)
                           << " on " << sequence::GetId(target,
                                              sequence::eGetId_Best)
                           << ": " << e.what() << "; skipping");
            /// Re-run without patch that caused problem
            list< CRef<CSeq_align> > new_alignments;
            ITERATE (list< CRef<CSeq_align> >, align_it, alignments) {
                if (*align_it != next_patch->second) {
                    new_alignments.push_back(*align_it);
                }
                if (new_alignments.empty()) {
                    patched_seq->Assign(target.GetInst());
                    return patched_seq;
                }
                return PatchTargetSequence(new_alignments, scope);
            }
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

