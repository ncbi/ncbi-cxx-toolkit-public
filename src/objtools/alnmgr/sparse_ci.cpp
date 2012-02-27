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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/alnmgr/sparse_ci.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

////////////////////////////////////////////////////////////////////////////////
/// CSparseSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

CSparseSegment::CSparseSegment(void)
    : m_Type(fInvalid),
      m_AlnRange(TSignedRange::GetEmpty()),
      m_RowRange(TSignedRange::GetEmpty())
{
}


CSparseSegment::operator bool(void) const
{
    return !IsInvalidType();
}


CSparseSegment::TSegTypeFlags CSparseSegment::GetType(void) const
{
    return m_Type;
}


const CSparseSegment::TSignedRange& CSparseSegment::GetAlnRange(void) const
{
    return m_AlnRange;
}


const CSparseSegment::TSignedRange& CSparseSegment::GetRange(void) const
{
    return m_RowRange;
}


////////////////////////////////////////////////////////////////////////////////
/// CSparse_CI

void CSparse_CI::x_InitSegment(void)
{
    bool anchor_gap = !m_AnchorIt  ||
        m_AnchorIt.GetSegType() == CPairwise_CI::eGap;
    bool row_gap = !m_RowIt  ||
        m_RowIt.GetSegType() == CPairwise_CI::eGap;

    TSignedRange& aln_rg = m_Segment.m_AlnRange;
    TSignedRange& row_rg = m_Segment.m_RowRange;

    TSignedSeqPos from = 0;
    TSignedSeqPos to = 0;
    TSignedSeqPos left_offset = 0;
    TSignedSeqPos right_offset = 0;

    if ( !m_AnchorIt ) {
        if ( !m_RowIt ) {
            // End of the iterator
            m_Aln.Reset();
            aln_rg = TSignedRange::GetEmpty();
            row_rg = TSignedRange::GetEmpty();
            m_Segment.m_Type = IAlnSegment::fInvalid;
            return;
        }
        // Only row iterator is valid. Gap or indel. Use the whole remaining
        // range of the row segment.
        aln_rg = m_NextRowRg;
        row_rg = m_RowIt.GetSecondRange();
        from = m_NextRowRg.GetFrom();
        to = m_NextRowRg.GetToOpen();
        left_offset = m_NextRowRg.GetFrom() - m_RowIt.GetFirstRange().GetFrom();
        right_offset = 0;
    }
    else if ( !m_RowIt ) {
        from = m_NextAnchorRg.GetFrom();
        to = m_NextAnchorRg.GetToOpen();
        // Row sequence is missing - set offsets to make the range empty
        // at the last range end.
        left_offset = row_rg.GetLength();
        right_offset = 0;
    }
    else {
        // Both iterators are valid - select nearest segment start.
        from = min(m_NextAnchorRg.GetFrom(), m_NextRowRg.GetFrom());
        // Calculate offset from the pairwise row segment start (to skip it).
        left_offset = from - m_RowIt.GetFirstRange().GetFrom();
        if (m_NextAnchorRg.GetFrom() > from) {
            // Use part of row range up to the anchor segment start
            // or the whole row segment if the anchor starts later.
            to = min(m_NextAnchorRg.GetFrom(), m_NextRowRg.GetToOpen());
            right_offset = m_NextRowRg.GetToOpen() - to;
        }
        else if (m_NextRowRg.GetFrom() > from) {
            // Use part of anchor range up the the row segment start
            // or the whole anchor segment if the row starts later.
            to = min(m_NextRowRg.GetFrom(), m_NextAnchorRg.GetToOpen());
            // Row range will become empty starting at the nearest row
            // segment from/to depending on the strand.
            left_offset = 0;
            right_offset = m_RowIt.GetSecondRange().GetLength();
        }
        else {
            // Both ranges start at the same point - find the nearest end.
            to = min(m_NextAnchorRg.GetToOpen(), m_NextRowRg.GetToOpen());
            right_offset = m_NextRowRg.GetToOpen() - to;
        }

        // Adjust gap flags if one of the pariwise segments starts past
        // the sparse segment end.
        anchor_gap = anchor_gap  ||
            m_AnchorIt.GetFirstRange().GetFrom() >= to;
        row_gap = row_gap  ||
            m_RowIt.GetFirstRange().GetFrom() >= to;
    }

    aln_rg.SetOpen(from, to);

    // Trim ranges to leave only unused range
    if (m_NextAnchorRg.GetFrom() < to) {
        m_NextAnchorRg.SetFrom(to);
    }
    if (m_NextRowRg.GetFrom() < to) {
        m_NextRowRg.SetFrom(to);
    }

    // Adjust row range according to the alignment range.
    _ASSERT(left_offset >= 0);
    _ASSERT(right_offset >= 0);
    if ( !m_RowDirect ) {
        swap(left_offset, right_offset);
    }
    if ( m_RowIt ) {
        row_rg = m_RowIt.GetSecondRange();
    }
    // Adjust offsets so that the range length is never negative.
    if (left_offset > row_rg.GetLength()) {
        left_offset = row_rg.GetLength();
    }
    if (right_offset > row_rg.GetLength() - left_offset) {
        right_offset = row_rg.GetLength() - left_offset;
    }
    row_rg.SetOpen(row_rg.GetFrom() + left_offset,
        row_rg.GetToOpen() - right_offset);

    // Set segment type.
    if ( row_gap ) {
        if ( aln_rg.Empty() ) {
            m_Segment.m_Type = IAlnSegment::fUnaligned;
        }
        else {
            m_Segment.m_Type = anchor_gap ?
                IAlnSegment::fGap : IAlnSegment::fIndel;
        }
    }
    else {
        m_Segment.m_Type = anchor_gap ?
            IAlnSegment::fIndel : IAlnSegment::fAligned;
    }

    // The flag shows relative row direction.
    if ( !m_RowDirect ) {
        m_Segment.m_Type |= IAlnSegment::fReversed;
    }
}

// assuming clipping range
void CSparse_CI::x_InitIterator(void)
{
    if (m_Row >= TDim(m_Aln->GetPairwiseAlns().size())) {
        // Invalid row selected - nothing to iterate.
        m_Aln.Reset();
        return;
    }
    const CPairwiseAln& anchor_pw =
        *m_Aln->GetPairwiseAlns()[m_Aln->GetAnchorRow()];
    const CPairwiseAln& pw = *m_Aln->GetPairwiseAlns()[m_Row];
    m_AnchorIt = CPairwise_CI(anchor_pw, m_TotalRange);
    m_RowIt = CPairwise_CI(pw, m_TotalRange);
    // Pairwise alignments in CSparseAln can not have mixed directions.
    // Remember the first one and use for all segments.
    m_AnchorDirect = m_AnchorIt ? m_AnchorIt.IsFirstDirect() : true;
    m_RowDirect = m_RowIt ? m_RowIt.IsDirect() : true;
    if ( m_AnchorIt ) {
        m_NextAnchorRg = m_AnchorIt.GetFirstRange();
    }
    else {
        m_NextAnchorRg = TSignedRange::GetEmpty();
    }
    if ( m_RowIt ) {
        m_NextRowRg = m_RowIt.GetFirstRange();
    }
    else {
        m_NextRowRg = TSignedRange::GetEmpty();
    }
    m_Segment.m_AlnRange = TSignedRange::GetEmpty();
    x_InitSegment();
    x_CheckSegment();
}


void CSparse_CI::x_CheckSegment(void)
{
    if (m_Flags == eAllSegments) {
        return;
    }
    while ( *this ) {
        if (m_Flags == eSkipGaps) {
            if ( m_Segment.IsAligned() ) {
                break;
            }
        }
        else {
            // Distinguish between insertions and deletions.
            bool ins = (m_Segment.m_Type & IAlnSegment::fIndel) != 0  &&
                m_Segment.m_RowRange.Empty();
            if ((m_Flags == eInsertsOnly  &&  ins)  ||
                (m_Flags == eSkipInserts  &&  !ins)) {
                break;
            }
        }
        x_NextSegment();
    }
}


void CSparse_CI::x_NextSegment(void)
{
    if ( !*this ) return;
    if (m_AnchorIt  &&  m_NextAnchorRg.Empty()) {
        // Advance anchor iterator, skip unaligned segments if any.
        do {
            ++m_AnchorIt;
        }
        while (m_AnchorIt  &&  m_AnchorIt.GetFirstRange().Empty());
        if ( m_AnchorIt ) {
            m_NextAnchorRg = m_AnchorIt.GetFirstRange();
        }
    }
    if (m_RowIt  &&  m_NextRowRg.Empty()) {
        ++m_RowIt;
        if ( m_RowIt ) {
            m_NextRowRg = m_RowIt.GetFirstRange();
        }
    }
    x_InitSegment();
}


bool CSparse_CI::x_Equals(const CSparse_CI& other) const
{
    return m_Aln == other.m_Aln  &&
        m_Flags == other.m_Flags  &&
        m_Row == other.m_Row  &&
        m_TotalRange == other.m_TotalRange  &&
        m_AnchorIt == other.m_AnchorIt  &&
        m_RowIt == other.m_RowIt  &&
        m_NextAnchorRg == other.m_NextAnchorRg  &&
        m_NextRowRg == other.m_NextRowRg  &&
        m_Segment == other.m_Segment;
}


CSparse_CI::CSparse_CI(void)
:   m_Flags(eAllSegments),
    m_Aln(NULL),
    m_Row(0),
    m_AnchorDirect(true),
    m_RowDirect(true)
{
    m_Segment.m_AlnRange = TSignedRange::GetEmpty();
    m_Segment.m_RowRange = TSignedRange::GetEmpty();
    m_Segment.m_Type = IAlnSegment::fInvalid;
}


CSparse_CI::CSparse_CI(const CSparseAln&   aln,
                       TDim                row,
                       EFlags              flags)
    : m_Flags(flags),
      m_Aln(aln.m_Aln),
      m_Row(row),
      m_TotalRange(TSignedRange::GetWhole())
{
    x_InitIterator();
}


CSparse_CI::CSparse_CI(const CSparseAln&   aln,
                       TDim                row,
                       EFlags              flags,
                       const TSignedRange& range)
    : m_Flags(flags),
      m_Aln(aln.m_Aln),
      m_Row(row),
      m_TotalRange(range)
{
    x_InitIterator();
}


CSparse_CI::CSparse_CI(const CSparse_CI& orig)
{
    *this = orig;
}


CSparse_CI::~CSparse_CI(void)
{
}


IAlnSegmentIterator* CSparse_CI::Clone(void) const
{
    return new CSparse_CI(*this);
}


CSparse_CI::operator bool(void) const
{
    return m_Aln  &&  (m_AnchorIt || m_RowIt);
}


IAlnSegmentIterator& CSparse_CI::operator++(void)
{
    x_NextSegment();
    x_CheckSegment();
    return *this;
}


bool CSparse_CI::operator==(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CSparse_CI* sparse_it = dynamic_cast<const CSparse_CI*>(&it);
        return x_Equals(*sparse_it);
    }
    return false;
}


bool CSparse_CI::operator!=(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CSparse_CI* sparse_it = dynamic_cast<const CSparse_CI*>(&it);
        return !x_Equals(*sparse_it);
    }
    return true;
}


const CSparse_CI::value_type& CSparse_CI::operator*(void) const
{
    _ASSERT(*this);
    return m_Segment;
}


const CSparse_CI::value_type* CSparse_CI::operator->(void) const
{
    _ASSERT(*this);
    return &m_Segment;
}


END_NCBI_SCOPE
