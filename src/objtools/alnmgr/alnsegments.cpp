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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment segments
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnsegments.hpp>
#include <objtools/alnmgr/alnseq.hpp>
#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnmap.hpp>

#include <stack>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMixSegments::CAlnMixSegments(CRef<CAlnMixSequences>&  aln_mix_sequences,
                                 TCalcScoreMethod calc_score)
    : m_AlnMixSequences(aln_mix_sequences),
      m_Rows(m_AlnMixSequences->m_Rows),
      m_ExtraRows(m_AlnMixSequences->m_ExtraRows),
      x_CalculateScore(calc_score)
{}


void
CAlnMixSegments::Build(bool gap_join,
                       bool min_gap)
{
    m_AlnMixSequences->InitRowsStartIts();
    m_AlnMixSequences->InitExtraRowsStartIts();
#if _DEBUG
    m_AlnMixSequences->RowsStartItsContsistencyCheck(0);
#endif


    TSegmentsContainer gapped_segs;

    CAlnMixSequences::TSeqs::iterator refseq_it = m_Rows.begin();
    bool orig_refseq = true;
    while (true) {
        CAlnMixSeq * refseq = 0;
        while (refseq_it != m_Rows.end()) {
            refseq = *(refseq_it++);
            if (refseq->m_StartIt != refseq->m_Starts.end()) {
                break;
            } else {
                refseq = 0;
            }
        }
        if ( !refseq ) {
            // Done

            // add the gapped segments if any
            if (gapped_segs.size()) {
                if (gap_join) {
                    // request to try to align
                    // gapped segments w/ equal len
                    x_ConsolidateGaps(gapped_segs);
                } else if (min_gap) {
                    // request to try to align 
                    // all gapped segments
                    x_MinimizeGaps(gapped_segs);
                }
                NON_CONST_ITERATE (TSegmentsContainer,
                                   seg_i, gapped_segs) {
                    m_Segments.push_back(&**seg_i);
                }
                gapped_segs.clear();
            }
            break; // from the main loop
        }
#if _ALNMGR_TRACE
        cerr << "refseq is on row " << refseq->m_RowIdx
             << " seq " << refseq->m_SeqIdx << "\n";
#endif
        // for each refseq segment
        while (refseq->m_StartIt != refseq->m_Starts.end()) {
            stack< CRef<CAlnMixSegment> > seg_stack;
            seg_stack.push(refseq->m_StartIt->second);
#if _ALNMGR_TRACE
            cerr << "  [row " << refseq->m_RowIdx
                 << " seq " << refseq->m_SeqIdx
                 << " start " << refseq->m_StartIt->first
                 << " was pushed into stack\n";
#endif
            
            while ( !seg_stack.empty() ) {
                
                bool pop_seg = true;
                // check the gapped segments on the left
                ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                         seg_stack.top()->m_StartIts) {

                    CAlnMixSeq * row = start_its_i->first;

                    if (row->m_StartIt != start_its_i->second) {
#if _DEBUG
                        if (row->m_PositiveStrand ?
                            row->m_StartIt->first >
                            start_its_i->second->first :
                            row->m_StartIt->first <
                            start_its_i->second->first) {
                            string errstr =
                                string("CAlnMixMerger::x_CreateSegmentsVector():")
                                + " Internal error: Integrity broken" +
                                " row=" + NStr::IntToString(row->m_RowIdx) +
                                " seq=" + NStr::IntToString(row->m_SeqIdx)
                                + " row->m_StartIt->first="
                                + NStr::IntToString(row->m_StartIt->first)
                                + " start_its_i->second->first=" +
                                NStr::IntToString(start_its_i->second->first)
                                + " refseq->m_StartIt->first=" +
                                NStr::IntToString(refseq->m_StartIt->first)
                                + " strand=" +
                                (row->m_PositiveStrand ? "plus" : "minus");
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }
#endif
                        seg_stack.push(row->m_StartIt->second);
#if _ALNMGR_TRACE
                        cerr << "  [row " << row->m_RowIdx
                             << " seq " << row->m_SeqIdx
                             << " start " << row->m_StartIt->first
                             << " (left of start " << start_its_i->second->first << ") "
                             << "was pushed into stack\n";
#endif
#if _DEBUG
                        if (row == refseq) {
                            string errstr =
                                string("CAlnMixMerger::x_CreateSegmentsVector():")
                                + " Internal error: Infinite loop detected.";
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }                            
#endif
                        pop_seg = false;
                        break;
                    }
                }

                if (pop_seg) {

                    // inc/dec iterators for each row of the seg
                    ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                             seg_stack.top()->m_StartIts) {
                        CAlnMixSeq * row = start_its_i->first;

#if _DEBUG
                        if (row->m_PositiveStrand  &&
                            row->m_StartIt->first > 
                            start_its_i->second->first  ||
                            !row->m_PositiveStrand  &&
                            row->m_StartIt->first <
                            start_its_i->second->first) {
                            string errstr =
                                string("CAlnMixMerger::x_CreateSegmentsVector():")
                                + " Internal error: Integrity broken" +
                                " row=" + NStr::IntToString(row->m_RowIdx) +
                                " seq=" + NStr::IntToString(row->m_SeqIdx)
                                + " row->m_StartIt->first="
                                + NStr::IntToString(row->m_StartIt->first)
                                + " start_its_i->second->first=" +
                                NStr::IntToString(start_its_i->second->first)
                                + " refseq->m_StartIt->first=" +
                                NStr::IntToString(refseq->m_StartIt->first)
                                + " strand=" +
                                (row->m_PositiveStrand ? "plus" : "minus");
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }
#endif

                        if (row->m_PositiveStrand) {
                            row->m_StartIt++;
                        } else {
                            if (row->m_StartIt == row->m_Starts.begin()) {
                                row->m_StartIt = row->m_Starts.end();
                            } else {
                                row->m_StartIt--;
                            }
                        }
                    }

                    if (seg_stack.size() > 1) {
                        // add to the gapped segments
                        gapped_segs.push_back(seg_stack.top());
                        seg_stack.pop();
#if _ALNMGR_TRACE
                        cerr << "  seg popped].\n";
#endif
                    } else {
                        // add the gapped segments if any
                        if (gapped_segs.size()) {
                            if (gap_join) {
                                // request to try to align
                                // gapped segments w/ equal len
                                x_ConsolidateGaps(gapped_segs);
                            } else if (min_gap) {
                                // request to try to align 
                                // all gapped segments
                                x_MinimizeGaps(gapped_segs);
                            }
                            if (orig_refseq) {
                                NON_CONST_ITERATE (TSegmentsContainer,
                                                   seg_i, gapped_segs) {
                                    m_Segments.push_back(&**seg_i);
                                }
                                gapped_segs.clear();
                            }
                        }
                        // add the refseq segment
                        if (orig_refseq) {
                            m_Segments.push_back(seg_stack.top());
                        } else {
                            gapped_segs.push_back(seg_stack.top());
                        }
                        seg_stack.pop();
#if _ALNMGR_TRACE
                        cerr << "  refseq seg popped].\n";
#endif
                    } // if (seg_stack.size() > 1)
                } // if (popseg)
            } // while ( !seg_stack.empty() )
        } // while (refseq->m_StartIt != refseq->m_Starts.end())
        orig_refseq = false;
    } // while (true)

}



void
CAlnMixSegments::FillUnalignedRegions()
{
    vector<TSignedSeqPos> starts;
    vector<TSeqPos> lens;
    starts.resize(m_Rows.size(), -1);
    lens.resize(m_Rows.size(), 0);
        
    TSeqPos len = 0;
    CAlnMap::TNumrow rowidx;

    TSegments::iterator seg_i = m_Segments.begin();
    while (seg_i != m_Segments.end()) {
        len = (*seg_i)->m_Len;
        ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                 (*seg_i)->m_StartIts) {
            CAlnMixSeq * row = start_its_i->first;
            rowidx = row->m_RowIdx;
            TSignedSeqPos& prev_start = starts[rowidx];
            TSeqPos& prev_len = lens[rowidx];
            TSeqPos start = start_its_i->second->first;
            const bool plus = row->m_PositiveStrand;
            const int& width = row->m_Width;
            TSeqPos prev_start_plus_len = prev_start + prev_len * width;
            TSeqPos start_plus_len = start + len * width;
            if (prev_start >= 0) {
                if (plus  &&  prev_start_plus_len < start  ||
                    !plus  &&  start_plus_len < (TSeqPos) prev_start) {
                    // create a new seg
                    CRef<CAlnMixSegment> seg (new CAlnMixSegment);
                    TSeqPos new_start;
                    if (row->m_PositiveStrand) {
                        new_start = prev_start + prev_len * width;
                        seg->m_Len = (start - new_start) / width;
                    } else {
                        new_start = start_plus_len;
                        seg->m_Len = (prev_start - new_start) / width;
                    }                            
                    row->m_Starts[new_start] = seg;
                    CAlnMixSeq::TStarts::iterator start_i =
                        start_its_i->second;
                    seg->m_StartIts[row] = 
                        row->m_PositiveStrand ?
                        --start_i :
                        ++start_i;
                            
                    seg_i = m_Segments.insert(seg_i, seg);
                    seg_i++;
                }
            }
            prev_start = start;
            prev_len = len;
        }
        seg_i++;
    }
}


void 
CAlnMixSegments::x_ConsolidateGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator seg1_i, seg2_i;

    seg2_i = seg1_i = gapped_segs.begin();
    if (seg2_i != gapped_segs.end()) {
        seg2_i++;
    }

    bool         cache = false;
    string       s1;
    TSeqPos      start1;
    int          score1;
    CAlnMixSeq * seq1;
    CAlnMixSeq * seq2;

    while (seg2_i != gapped_segs.end()) {

        CAlnMixSegment * seg1 = *seg1_i;
        CAlnMixSegment * seg2 = *seg2_i;

        // check if this seg possibly aligns with the previous one
        bool possible = true;
            
        if (seg2->m_Len == seg1->m_Len  && 
            seg2->m_StartIts.size() == 1) {

            seq2 = seg2->m_StartIts.begin()->first;

            // check if this seq was already used
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     (*seg1_i)->m_StartIts) {
                if (st_it->first == seq2) {
                    possible = false;
                    break;
                }
            }

            // check if score is sufficient
            if (possible  &&  x_CalculateScore) {
                if (!cache) {

                    seq1 = seg1->m_StartIts.begin()->first;
                    
                    start1 = seg1->m_StartIts[seq1]->first;
                    TSeqPos start1_plus_len = 
                        start1 + seg1->m_Len * seq1->m_Width;
                        
                    if (seq1->m_PositiveStrand) {
                        seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Plus).
                            GetSeqData(start1, start1_plus_len, s1);
                    } else {
                        CSeqVector seq_vec = 
                            seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Minus);
                        TSeqPos size = seq_vec.size();
                        seq_vec.GetSeqData(size - start1_plus_len,
                                           size - start1, 
                                           s1);
                    }                                

                    score1 = x_CalculateScore(s1,
                                              s1,
                                              seq1->m_IsAA,
                                              seq1->m_IsAA);
                    cache = true;
                }
                
                string s2;

                const TSeqPos& start2 = seg2->m_StartIts[seq2]->first;
                TSeqPos start2_plus_len = 
                    start2 + seg2->m_Len * seq2->m_Width;
                            
                if (seq2->m_PositiveStrand) {
                    seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Plus).
                        GetSeqData(start2, start2_plus_len, s2);
                } else {
                    CSeqVector seq_vec = 
                        seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Minus);
                    TSeqPos size = seq_vec.size();
                    seq_vec.GetSeqData(size - start2_plus_len,
                                       size - start2, 
                                       s2);
                }                                

                int score2 = 
                    x_CalculateScore(s1, s2, seq1->m_IsAA, seq2->m_IsAA);

                if (score2 < 75 * score1 / 100) {
                    possible = false;
                }
            }
            
        } else {
            possible = false;
        }

        if (possible) {
            // consolidate the ones so far
            
            // add the new row
            seg1->m_StartIts[seq2] = seg2->m_StartIts.begin()->second;
            
            // point the row's start position to the beginning seg
            seg2->m_StartIts.begin()->second->second = seg1;
            
            seg2_i = gapped_segs.erase(seg2_i);
        } else {
            cache = false;
            seg1_i++;
            seg2_i++;
        }
    }
}


void
CAlnMixSegments::x_MinimizeGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator  seg_i, seg_i_end, seg_i_begin;
    CAlnMixSegment       * seg1, * seg2;
    CRef<CAlnMixSegment> seg;
    CAlnMixSeq           * seq;
    TSegmentsContainer            new_segs;

    seg_i_begin = seg_i_end = seg_i = gapped_segs.begin();

    typedef map<TSeqPos, CRef<CAlnMixSegment> > TLenMap;
    TLenMap len_map;

    while (seg_i_end != gapped_segs.end()) {

        len_map[(*seg_i_end)->m_Len];
        
        // check if this seg can possibly be minimized
        bool possible = true;

        seg_i = seg_i_begin;
        while (seg_i != seg_i_end) {
            seg1 = *seg_i;
            seg2 = *seg_i_end;
            
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     seg2->m_StartIts) {
                seq = st_it->first;
                // check if this seq was already used
                if (seg1->m_StartIts.find(seq) != seg1->m_StartIts.end()) {
                    possible = false;
                    break;
                }
            }
            if ( !possible ) {
                break;
            }
            seg_i++;
        }
        seg_i_end++;

        if ( !possible  ||  seg_i_end == gapped_segs.end()) {
            // use the accumulated len info to create the new segs

            // create new segs with appropriate len
            TSeqPos len_so_far = 0;
            TLenMap::iterator len_i = len_map.begin();
            while (len_i != len_map.end()) {
                len_i->second = new CAlnMixSegment();
                len_i->second->m_Len = len_i->first - len_so_far;
                len_so_far += len_i->second->m_Len;
                len_i++;
            }
                
            // loop through the accumulated orig segs.
            // copy info from them into the new segs
            TLenMap::iterator len_i_end;
            seg_i = seg_i_begin;
            while (seg_i != seg_i_end) {
                TSeqPos orig_len = (*seg_i)->m_Len;

                // determine the span of the current seg
                len_i_end = len_map.find(orig_len);
                len_i_end++;

                // loop through its sequences
                NON_CONST_ITERATE (CAlnMixSegment::TStartIterators,
                                   st_it,
                                   (*seg_i)->m_StartIts) {

                    seq = st_it->first;
                    TSeqPos orig_start = st_it->second->first;

                    len_i = len_map.begin();
                    len_so_far = 0;
                    // loop through the new segs
                    while (len_i != len_i_end) {
                        seg = len_i->second;
                    
                        // calc the start
                        TSeqPos this_start = orig_start + 
                            (seq->m_PositiveStrand ? 
                             len_so_far :
                             orig_len - len_so_far - seg->m_Len) *
                            seq->m_Width;

                        // create the bindings:
                        seq->m_Starts[this_start] = seg;
                        seg->m_StartIts[seq] = seq->m_Starts.find(this_start);
                        len_i++;
                        len_so_far += seg->m_Len;
                    }
                }
                seg_i++;
            }
            NON_CONST_ITERATE (TLenMap, len_it, len_map) {
                new_segs.push_back(len_it->second);
            }
            len_map.clear();
            seg_i_begin = seg_i_end;
        }
    }
    gapped_segs.clear();
    ITERATE (TSegmentsContainer, new_seg_i, new_segs) {
        gapped_segs.push_back(*new_seg_i);
    }
}


void 
CAlnMixSegment::StartItsConsistencyCheck(const CAlnMixSeq& seq,
                                         const TSeqPos&    start,
                                         size_t            match_idx) const
{
    ITERATE(TStartIterators, st_it_i, m_StartIts) {
        // both should point to the same seg
        if ((*st_it_i).second->second != this) {
            string errstr =
                string("CAlnMixSegment::StartItsConsistencyCheck")
                + " [match_idx=" + NStr::IntToString(match_idx) + "]"
                + " The internal consistency check failed for"
                + " the segment containing ["
                + " row=" + NStr::IntToString((*st_it_i).first->m_RowIdx)
                + " seq=" + NStr::IntToString((*st_it_i).first->m_SeqIdx)
                + " strand=" +
                ((*st_it_i).first->m_PositiveStrand ? "plus" : "minus")
                + " start=" + NStr::IntToString((*st_it_i).second->first)
                + "] aligned to: ["
                + " row=" + NStr::IntToString(seq.m_RowIdx)
                + " seq=" + NStr::IntToString(seq.m_SeqIdx)
                + " strand=" +
                (seq.m_PositiveStrand ? "plus" : "minus")
                + " start=" + NStr::IntToString(start)
                + "].";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/03/10 19:33:00  todorov
* Moved a few routines out of the merger to their corresponding classes
*
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/
