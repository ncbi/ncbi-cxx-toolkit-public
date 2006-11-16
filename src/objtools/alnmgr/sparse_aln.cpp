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

#include <objtools/alnmgr/sparse_aln.hpp>
#include <objtools/alnmgr/sparse_ci.hpp>

#include <objects/seqalign/Sparse_align.hpp>

#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);


CSparseAln::CSparseAln(const CAnchoredAln& anchored_aln,
                       objects::CScope& scope)
    : m_AnchoredAln(anchored_aln),
      m_PairwiseAlns(m_AnchoredAln.GetPairwiseAlns()),
      m_SeqIds(m_AnchoredAln.GetSeqIds()),
      m_Scope(&scope),
      m_GapChar('-')
{
    _ASSERT(m_PairwiseAlns.size() == m_SeqIds.size());
    UpdateCache();
}

    
CSparseAln::~CSparseAln()
{
}


CSparseAln::TDim CSparseAln::GetDim() const {
    _ASSERT(m_PairwiseAlns.size() == m_SeqIds.size());
    return m_AnchoredAln.GetDim();
}

void CSparseAln::UpdateCache()
{
    const TDim& dim = m_AnchoredAln.GetDim();

    m_BioseqHandles.clear();
    m_BioseqHandles.resize(dim);
    
    m_SecondRanges.resize(dim);
    for (TDim row = 0;  row < dim;  ++row) {

        /// Check collections flags
        _ASSERT( !m_PairwiseAlns[row]->IsSet(TAlnRngColl::fInvalid) );
        _ASSERT( !m_PairwiseAlns[row]->IsSet(TAlnRngColl::fUnsorted) );
        _ASSERT( !m_PairwiseAlns[row]->IsSet(TAlnRngColl::fOverlap) );
        _ASSERT( !m_PairwiseAlns[row]->IsSet(TAlnRngColl::fMixedDir) );

        /// Determine m_FirstRange
        if (row == 0) {
            m_FirstRange = m_PairwiseAlns[row]->GetFirstRange();
        } else {
            m_FirstRange.CombineWith(m_PairwiseAlns[row]->GetFirstRange());
        }

        /// Determine m_SecondRanges
        CAlignRangeCollExtender<TAlnRngColl> ext(*m_PairwiseAlns[row]);
        ext.UpdateIndex();
        m_SecondRanges[row] = ext.GetSecondRange();
    }
}


CSparseAln::TRng CSparseAln::GetAlnRange() const
{
    return m_FirstRange;
}


void CSparseAln::SetGapChar(TResidue gap_char)
{
    m_GapChar = gap_char;
}


CRef<CScope> CSparseAln::GetScope() const
{
    return m_Scope;
}


const CSparseAln::TAlnRngColl&
CSparseAln::GetAlignCollection(TNumrow row)
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return *m_PairwiseAlns[row];
}


const CSeq_id& CSparseAln::GetSeqId(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return *m_SeqIds[row];
}


TSignedSeqPos   CSparseAln::GetSeqAlnStart(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return m_PairwiseAlns[row]->GetFirstFrom();
}


TSignedSeqPos CSparseAln::GetSeqAlnStop(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return m_PairwiseAlns[row]->GetFirstTo();
}


CSparseAln::TSignedRange CSparseAln::GetSeqAlnRange(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return TSignedRange(GetSeqAlnStart(row), GetSeqAlnStop(row));
}


TSeqPos CSparseAln::GetSeqStart(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return m_SecondRanges[row].GetFrom();
}


TSeqPos CSparseAln::GetSeqStop(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return m_SecondRanges[row].GetTo();
}


CSparseAln::TRange CSparseAln::GetSeqRange(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    return TRange(GetSeqStart(row), GetSeqStop(row));
}


bool CSparseAln::IsPositiveStrand(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    _ASSERT( !m_PairwiseAlns[row]->IsSet(CPairwiseAln::fMixedDir) );
    return m_PairwiseAlns[row]->IsSet(CPairwiseAln::fDirect);
}


bool CSparseAln::IsNegativeStrand(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    _ASSERT( !m_PairwiseAlns[row]->IsSet(CPairwiseAln::fMixedDir) );
    return m_PairwiseAlns[row]->IsSet(CPairwiseAln::fReversed);
}


bool CSparseAln::IsTranslated() const {
    /// TODO: Does BaseWidth of 1 always mean nucleotide?  Should we
    /// have an enum (with an invalid (unasigned) value?
    const int k_unasigned_base_width = 0;
    int base_width = k_unasigned_base_width;
    for (TDim row = 0;  row < GetDim();  ++row) {
        if (base_width == k_unasigned_base_width) {
            base_width = m_PairwiseAlns[row]->GetFirstBaseWidth();
        }
        if (base_width != m_PairwiseAlns[row]->GetFirstBaseWidth()  ||
            base_width != m_PairwiseAlns[row]->GetSecondBaseWidth()) {
            return true; //< there *at least one* base diff base width
        }
        /// TODO: or should this check be stronger:
        if (base_width != 1) {
            return true;
        }
    }
    return false;
}


inline  CSparseAln::TAlnRngColl::ESearchDirection
    GetCollectionSearchDirection(CAlignUtils::ESearchDirection dir)
{
    typedef CSparseAln::TAlnRngColl   T;
    switch(dir) {
    case CAlignUtils::eNone:
        return T::eNone;
    case CAlignUtils::eLeft:
        return T::eLeft;
    case CAlignUtils::eRight:
        return T::eRight;
    case CAlignUtils::eForward:
        return T::eForward;
    case CAlignUtils::eBackwards:
        return T::eBackwards;
    }
    _ASSERT(false); // invalid
    return T::eNone;
}


TSignedSeqPos 
CSparseAln::GetAlnPosFromSeqPos(TNumrow row, 
                                      TSeqPos seq_pos,
                                      TUtils::ESearchDirection dir,
                                      bool try_reverse_dir) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    TAlnRngColl::ESearchDirection c_dir = GetCollectionSearchDirection(dir);
    return m_PairwiseAlns[row]->GetFirstPosBySecondPos(seq_pos, c_dir);
}


TSignedSeqPos CSparseAln::GetSeqPosFromAlnPos(TNumrow row, TSeqPos aln_pos,
                                                    TUtils::ESearchDirection dir,
                                                    bool try_reverse_dir) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    TAlnRngColl::ESearchDirection c_dir = GetCollectionSearchDirection(dir);
    return m_PairwiseAlns[row]->GetSecondPosByFirstPos(aln_pos, c_dir);
}


string& CSparseAln::GetSeqString(TNumrow row, string &buffer,
                                 TSeqPos seq_from, TSeqPos seq_to) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    buffer.erase();
    if (seq_to >= seq_from) {
        const CBioseq_Handle& handle = GetBioseqHandle(row);
        bool positive = IsPositiveStrand(row);
        CBioseq_Handle::EVectorStrand strand = positive ?
                CBioseq_Handle::eStrand_Plus : CBioseq_Handle::eStrand_Minus;

        CSeqVector seq_vector =
            handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, strand);

        size_t size = seq_to - seq_from + 1;
        buffer.resize(size, m_GapChar);

        if (positive) {
            seq_vector.GetSeqData(seq_from, seq_to + 1, buffer);
        } else {
            TSeqPos vec_size = seq_vector.size();
            seq_vector.GetSeqData(vec_size - seq_to - 1, vec_size - seq_from, buffer);
        }
    }
    return buffer;
}


string& CSparseAln::GetSeqString(TNumrow row, string &buffer,
                                 const TUtils::TRange &seq_range) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    return GetSeqString(row, buffer, seq_range.GetFrom(), seq_range.GetTo());
}


string& CSparseAln::GetAlnSeqString(TNumrow row, string &buffer,
                                    const TSignedRange &aln_range) const
{
    _ASSERT(row >= 0  &&  row < GetDim());
    _ASSERT(m_PairwiseAlns[row]->GetSecondBaseWidth() == 1); //< TODO: add support

    buffer.erase();

    if(aln_range.GetLength() > 0)   {
        const CBioseq_Handle& handle = GetBioseqHandle(row);
        bool positive = IsPositiveStrand(row);
        CBioseq_Handle::EVectorStrand strand = positive ?
                CBioseq_Handle::eStrand_Plus : CBioseq_Handle::eStrand_Minus;

        CSeqVector seq_vector =
            handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, strand);
        TSeqPos vec_size = seq_vector.size();

        // buffer holds sequence for "aln_range", 0 index corresonds to aln_range.GetFrom()
        size_t size = aln_range.GetLength();
        buffer.resize(size, ' ');

        // check whether we have a gap at start position
        const CPairwiseAln& coll = *m_PairwiseAlns[row];
        size_t prev_to_open = (coll.GetFirstFrom() > aln_range.GetFrom()) ? string::npos : 0;

        string s;
        CSparse_CI it(coll, IAlnSegmentIterator::eSkipGaps, aln_range);

        //LOG_POST("GetAlnSeqString(" << row << ") ==========================================" );
        while (it)   {
            const IAlnSegment::TSignedRange& aln_r = it->GetAlnRange(); // in alignment
            const IAlnSegment::TSignedRange& r = it->GetRange(); // on sequence
            //LOG_POST("Aln [" << aln_r.GetFrom() << ", " << aln_r.GetTo() << "], Seq  "
            //                 << r.GetFrom() << ", " << r.GetTo());

            // TODO performance issue - waiting for better API
            if(positive)    {
                seq_vector.GetSeqData(r.GetFrom(), r.GetTo() + 1, s);
            } else {
                seq_vector.GetSeqData(vec_size - r.GetTo() - 1,
                                      vec_size - r.GetFrom(), s);
            }
            /*if(it->IsReversed())    {
                std::reverse(s.begin(), s.end());
            }*/
            size_t off = max((TSignedSeqPos) 0, aln_r.GetFrom() - aln_range.GetFrom());
            size_t len = min(buffer.size() - off, s.size());

            if(prev_to_open != string::npos) {   // this is not the first segement
                int gap_size = off - prev_to_open;
                buffer.replace(prev_to_open, gap_size, gap_size, m_GapChar);
            }

            _ASSERT(off + len <= buffer.size());

            buffer.replace(off, len, s, 0, len);
            prev_to_open = off + len;
            ++it;
        }
        int fill_len = size - prev_to_open;
        if(prev_to_open != string::npos  &&  fill_len > 0  &&  coll.GetFirstTo() > aln_range.GetTo()) {
            // there is gap on the right
            buffer.replace(prev_to_open, fill_len, fill_len, m_GapChar);
        }
        //LOG_POST(buffer);
    }
    return buffer;
}


const CBioseq_Handle&  CSparseAln::GetBioseqHandle(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    if ( !m_BioseqHandles[row] ) {
        m_BioseqHandles[row] = m_Scope->GetBioseqHandle(*m_SeqIds[row]);
    }
    return m_BioseqHandles[row];
}


IAlnSegmentIterator*
CSparseAln::CreateSegmentIterator(TNumrow row,
                                  const TUtils::TSignedRange& range,
                                  IAlnSegmentIterator::EFlags flag) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    return new CSparse_CI(*m_PairwiseAlns[row], flag, range);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/11/16 13:46:28  todorov
 * Moved over from gui/widgets/aln_data and refactored to adapt to the
 * new aln framework.
 *
 * Revision 1.9  2006/10/05 12:14:29  dicuccio
 * Use Sparse_seg.hpp from objects/seqalign
 *
 * Revision 1.8  2006/09/05 12:35:31  dicuccio
 * White space changes: tabs -> spaces; trim trailing spaces
 *
 * Revision 1.7  2006/07/03 14:46:07  yazhuk
 * Redesigned according to the new approach
 *
 * Revision 1.6  2005/09/20 12:58:54  dicuccio
 * BUGZID: 114 Be explicit about use of LOG_POST()
 *
 * Revision 1.5  2005/09/19 12:21:08  dicuccio
 * White space changes: trim trailing white space; use spaces not tabs
 *
 * Revision 1.4  2005/08/23 17:48:34  yazhuk
 * Minor fix
 *
 * Revision 1.3  2005/06/27 14:40:41  yazhuk
 * Updated to work with new Sparse-align; bugfixes
 *
 * Revision 1.2  2005/06/14 15:35:19  yazhuk
 * Removed unused variable
 *
 * Revision 1.1  2005/06/13 19:32:48  yazhuk
 * Initial revision
 *

 * ===========================================================================
 */

