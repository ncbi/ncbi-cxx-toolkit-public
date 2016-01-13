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
*   Pairwise and query-anchored alignments
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CPairwise_CI& CPairwise_CI::operator++(void)
{
    if ( m_Direct ) {
        if (m_It == m_GapIt) {
            // Advance to the next gap.
            ++m_It;
        }
        // Advance to the next non-gap segment if there's no unaligned range
        // to show.
        else if ( !m_Unaligned ) {
            // Advance to the next segment.
            ++m_GapIt;
            _ASSERT(m_It == m_GapIt);
        }
    }
    else {
        if (m_It == m_GapIt) {
            if (m_It == m_Aln->begin()) {
                m_It = m_Aln->end();
                m_GapIt = m_Aln->end();
            }
            else {
                --m_It;
            }
        }
        else if ( !m_Unaligned ) {
            _ASSERT(m_GapIt != m_Aln->begin());
            --m_GapIt;
            _ASSERT(m_It == m_GapIt);
        }
    }
    x_InitSegment();
    return *this;
}


void CPairwise_CI::x_Init(bool force_direct)
{
    // Mixed direction and empty alignments are iterated in direct order.
    m_Direct = force_direct ||
        ((m_Aln->GetFlags() & CPairwiseAln::fMixedDir) == CPairwiseAln::fMixedDir)  ||
        m_Aln->empty()  ||
        m_Aln->begin()->IsFirstDirect();
    if ( m_Direct ) {
        TCheckedIterator it = m_Aln->find_2(m_Range.GetFrom());
        m_It = it.first;
        m_GapIt = it.first;
        if ( !it.second ) {
            if (m_GapIt != m_Aln->begin()) {
                --m_GapIt;
            }
        }
    }
    else {
        CPairwiseAln::const_iterator last = m_Aln->end();
        if ( !m_Aln->empty() ) {
            --last;
        }
        TCheckedIterator it = m_Range.IsWhole() ?
            TCheckedIterator(last, true)
            : m_Aln->find_2(m_Range.GetTo());
        if (it.first == m_Aln->end()) {
            it.first = last;
        }
        m_It = it.first;
        m_GapIt = it.first;
        if ( !it.second ) {
            if (m_GapIt != m_Aln->end()  &&  m_GapIt != last) {
                ++m_GapIt;
            }
        }
    }
    x_InitSegment();
}


void CPairwise_CI::x_InitSegment(void)
{
    if ( !*this ) {
        m_FirstRg = TSignedRange::GetEmpty();
        m_SecondRg = TSignedRange::GetEmpty();
        return;
    }
    _ASSERT(m_It != m_Aln->end()  &&  m_GapIt != m_Aln->end());
    if (m_It == m_GapIt) {
        // Normal segment
        m_FirstRg = m_It->GetFirstRange();
        m_SecondRg = m_It->GetSecondRange();
    }
    else {
        // Gap
        TSignedSeqPos it_second_from = m_It->GetSecondFrom();
        TSignedSeqPos it_second_to = m_It->GetSecondToOpen();
        TSignedSeqPos gap_second_from = m_GapIt->GetSecondFrom();
        TSignedSeqPos gap_second_to = m_GapIt->GetSecondToOpen();
        if ( m_Direct ) {
            m_FirstRg.SetOpen(m_GapIt->GetFirstToOpen(), m_It->GetFirstFrom());
            if ( m_It->IsDirect() ) {
                if ( m_GapIt->IsDirect() ) {
                    m_SecondRg.SetOpen(gap_second_to, it_second_from);
                }
                else {
                    TSignedSeqPos from = m_GapIt->GetSecondFrom();
                    TSignedSeqPos to = m_It->GetSecondFrom();
                    m_SecondRg.SetOpen(min(gap_second_from, it_second_from),
                        max(gap_second_from, it_second_from));
                }
            }
            else {
                if ( !m_GapIt->IsDirect() ) {
                    m_SecondRg.SetOpen(it_second_to, gap_second_from);
                }
                else {
                    m_SecondRg.SetOpen(min(gap_second_to, it_second_to),
                        max(gap_second_to, it_second_to));
                }
            }
            if ( !m_Unaligned ) {
                if (!m_FirstRg.Empty()  &&  !m_SecondRg.Empty()) {
                    // Show gap first, then unaligned segment.
                    m_SecondRg.SetToOpen(m_SecondRg.GetFrom());
                    m_Unaligned = true;
                }
            }
            else {
                // Show unaligned segment after gap.
                m_FirstRg.SetFrom(m_FirstRg.GetToOpen());
                m_Unaligned = false;
                // Don't clip unaligned segments.
                return;
            }
        }
        else {
            m_FirstRg.SetOpen(m_It->GetFirstToOpen(), m_GapIt->GetFirstFrom());
            if ( m_It->IsDirect() ) {
                if ( m_GapIt->IsDirect() ) {
                    m_SecondRg.SetOpen(it_second_to, gap_second_from);
                }
                else {
                    m_SecondRg.SetOpen(min(it_second_to, gap_second_to),
                        max(it_second_to, gap_second_to));
                }
            }
            else {
                if ( !m_GapIt->IsDirect() ) {
                    m_SecondRg.SetOpen(gap_second_to, it_second_from);
                }
                else {
                    m_SecondRg.SetOpen(min(it_second_from, gap_second_from),
                        max(it_second_from, gap_second_from));
                }
            }
            if ( !m_Unaligned ) {
                if ( !m_FirstRg.Empty()  &&  !m_SecondRg.Empty()) {
                    m_SecondRg.SetFrom(m_SecondRg.GetToOpen());
                    m_Unaligned = true;
                }
            }
            else {
                m_FirstRg.SetToOpen(m_FirstRg.GetFrom());
                m_Unaligned = false;
                return;
            }
        }
    }
    if ( m_Range.IsWhole() ) {
        return;
    }
    // Take both direction into account, adjust ranges if clipped.
    TSignedSeqPos left_shift = 0;
    TSignedSeqPos right_shift = 0;
    if (m_FirstRg.GetFrom() < m_Range.GetFrom()) {
        left_shift = m_Range.GetFrom() - m_FirstRg.GetFrom();
    }
    if (m_FirstRg.GetToOpen() > m_Range.GetToOpen()) {
        right_shift = m_FirstRg.GetToOpen() - m_Range.GetToOpen();
    }
    m_FirstRg.IntersectWith(m_Range);
    if (left_shift != 0  ||  right_shift != 0) {
        if ( m_It->IsReversed() ) {
            swap(left_shift, right_shift);
        }
        m_SecondRg.SetOpen(m_SecondRg.GetFrom() + left_shift,
            m_SecondRg.GetToOpen() - right_shift);
        if (m_SecondRg.GetToOpen() < m_SecondRg.GetFrom()) {
            m_SecondRg.SetToOpen(m_SecondRg.GetFrom());
        }
    }
}


/// Split rows with mixed dir into separate rows
/// returns true if the operation was performed
bool CAnchoredAln::SplitStrands()
{
    TDim dim = GetDim();
    TDim new_dim = dim;
    TDim row;
    TDim new_row;

    for (row = 0;  row < dim;  ++row) {
        if (m_PairwiseAlns[row]->IsSet(CPairwiseAln::fMixedDir)) {
            ++new_dim;
        }
    }
    _ASSERT(dim <= new_dim);
    if (new_dim > dim) {
        m_PairwiseAlns.resize(new_dim);
        row = dim - 1;
        new_row = new_dim - 1;
        while (row < new_row) {
            _ASSERT(row >= 0);
            _ASSERT(new_row > 0);
            if (row == m_AnchorRow) {
                m_AnchorRow = new_row;
            }
            const CPairwiseAln& aln = *m_PairwiseAlns[row];
            if (aln.IsSet(CPairwiseAln::fMixedDir)) {
                m_PairwiseAlns[new_row].Reset
                    (new CPairwiseAln(aln.GetFirstId(),
                                      aln.GetSecondId(),
                                      aln.GetPolicyFlags()));
                CPairwiseAln& reverse_aln = *m_PairwiseAlns[new_row--];
                m_PairwiseAlns[new_row].Reset
                    (new CPairwiseAln(aln.GetFirstId(),
                                      aln.GetSecondId(),
                                      aln.GetPolicyFlags()));
                CPairwiseAln& direct_aln = *m_PairwiseAlns[new_row--];
                ITERATE (CPairwiseAln, aln_rng_it, aln) {
                    if (aln_rng_it->IsDirect()) {
                        direct_aln.push_back(*aln_rng_it);
                    } else {
                        reverse_aln.push_back(*aln_rng_it);
                    }
                }
            } else {
                m_PairwiseAlns[new_row--].Reset
                    (new CPairwiseAln(aln));
            }
            --row;
            _ASSERT(row <= new_row);
        }
        return true;
    } else {
        _ASSERT(dim == new_dim);
        return false;
    }
}


END_NCBI_SCOPE
