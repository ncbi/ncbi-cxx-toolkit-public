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
#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/error_codes.hpp>

#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Aln_Sparse


BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);


CSparseAln::CSparseAln(const CAnchoredAln& anchored_aln,
                       objects::CScope& scope)
    : m_AnchoredAln(&anchored_aln),
      m_PairwiseAlns(m_AnchoredAln->GetPairwiseAlns()),
      m_Scope(&scope),
      m_GapChar('-')
{
    UpdateCache();
}

    
CSparseAln::~CSparseAln()
{
}


CSparseAln::TDim CSparseAln::GetDim() const {
    return m_AnchoredAln->GetDim();
}

void CSparseAln::UpdateCache()
{
    const TDim& dim = m_AnchoredAln->GetDim();

    m_BioseqHandles.clear();
    m_BioseqHandles.resize(dim);

    m_SeqVectors.clear();
    m_SeqVectors.resize(dim);
    
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
    return m_PairwiseAlns[row]->GetSecondId()->GetSeqId();
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


inline CSparseAln::TAlnRngColl::ESearchDirection
GetCollectionSearchDirection(CSparseAln::ESearchDirection dir)
{
    typedef CSparseAln::TAlnRngColl   T;
    switch(dir) {
    case CSparseAln::eNone:
        return T::eNone;
    case CSparseAln::eLeft:
        return T::eLeft;
    case CSparseAln::eRight:
        return T::eRight;
    case CSparseAln::eForward:
        return T::eForward;
    case CSparseAln::eBackwards:
        return T::eBackwards;
    }
    _ASSERT(false); // invalid
    return T::eNone;
}


TSignedSeqPos 
CSparseAln::GetAlnPosFromSeqPos(TNumrow row, 
                                TSeqPos seq_pos,
                                ESearchDirection dir,
                                bool try_reverse_dir) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    TAlnRngColl::ESearchDirection c_dir = GetCollectionSearchDirection(dir);
    return m_PairwiseAlns[row]->GetFirstPosBySecondPos(seq_pos, c_dir);
}


TSignedSeqPos CSparseAln::GetSeqPosFromAlnPos(TNumrow row, TSeqPos aln_pos,
                                              ESearchDirection dir,
                                              bool try_reverse_dir) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    TAlnRngColl::ESearchDirection c_dir = GetCollectionSearchDirection(dir);
    return m_PairwiseAlns[row]->GetSecondPosByFirstPos(aln_pos, c_dir);
}


const CBioseq_Handle&  CSparseAln::GetBioseqHandle(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    if ( !m_BioseqHandles[row] ) {
        if ( !(m_BioseqHandles[row] = m_Scope->GetBioseqHandle(GetSeqId(row))) ) {
            string errstr = "Invalid bioseq handle.  Seq id \"" +
                GetSeqId(row).AsFastaString() + "\" not in scope?";
            NCBI_THROW(CAlnException, eInvalidRequest, errstr);
        }
    }
    return m_BioseqHandles[row];
}


CSeqVector& CSparseAln::x_GetSeqVector(TNumrow row) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    if ( !m_SeqVectors[row] ) {
        CSeqVector vec = GetBioseqHandle(row).GetSeqVector
            (CBioseq_Handle::eCoding_Iupac,
             IsPositiveStrand(row) ? 
             CBioseq_Handle::eStrand_Plus :
             CBioseq_Handle::eStrand_Minus);
        m_SeqVectors[row].Reset(new CSeqVector(vec));
    }
    return *m_SeqVectors[row];
}


void CSparseAln::TranslateNAToAA(const string& na,
                                 string& aa,
                                 int gencode)
{
    const CTrans_table& tbl = CGen_code_table::GetTransTable(gencode);

    size_t na_remainder = na.size() % 3;
    size_t na_size = na.size() - na_remainder;

    if (aa != na) {
        aa.resize(na_size / 3 + (na_remainder ? 1 : 0));
    }
    
    size_t aa_i = 0;
    int state = 0;
    for (size_t na_i = 0;  na_i < na_size; ) {
        for (size_t i = 0;  i < 3;  ++i, ++na_i) {
            state = tbl.NextCodonState(state, na[na_i]);
        }
        aa[aa_i++] = tbl.GetCodonResidue(state);
    }
    if (na_remainder) {
        aa[aa_i++] = '\\';
    }
    
    if (aa == na) {
        aa[aa_i] = 0;
        aa.resize(aa_i);
    }
}


string& CSparseAln::GetSeqString(TNumrow row,
                                 string &buffer,
                                 TSeqPos seq_from, TSeqPos seq_to,
                                 bool force_translation) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    buffer.erase();
    if (seq_to >= seq_from) {
        CSeqVector& seq_vector = x_GetSeqVector(row);

        size_t size = seq_to - seq_from + 1;
        buffer.resize(size, m_GapChar);

        if (IsPositiveStrand(row)) {
            seq_vector.GetSeqData(seq_from, seq_to + 1, buffer);
        } else {
            TSeqPos vec_size = seq_vector.size();
            seq_vector.GetSeqData(vec_size - seq_to - 1, vec_size - seq_from, buffer);
        }
    }
    return buffer;
}


string& CSparseAln::GetSeqString(TNumrow row,
                                 string &buffer,
                                 const TRange &seq_range,
                                 bool force_translation) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    return GetSeqString(row,
                        buffer,
                        seq_range.GetFrom(), seq_range.GetTo(),
                        force_translation);
}


string& CSparseAln::GetAlnSeqString(TNumrow row,
                                    string &buffer,
                                    const TSignedRange &aln_range,
                                    bool force_translation) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    bool translate = force_translation  ||  IsTranslated();

    buffer.erase();

    if(aln_range.GetLength() > 0)   {
        CSeqVector& seq_vector = x_GetSeqVector(row);
        TSeqPos vec_size = seq_vector.size();

        const int base_width = m_PairwiseAlns[row]->GetSecondBaseWidth();

        // buffer holds sequence for "aln_range", 0 index corresonds to aln_range.GetFrom()
        size_t size = aln_range.GetLength();
        buffer.resize(size, ' ');

        // check whether we have a gap at start position
        const CPairwiseAln& coll = *m_PairwiseAlns[row];
        size_t prev_to_open = (coll.GetFirstFrom() > aln_range.GetFrom()) ? string::npos : 0;

        string s;
        CSparse_CI it(coll, IAlnSegmentIterator::eSkipGaps, aln_range);

        //LOG_POST_X(1, "GetAlnSeqString(" << row << ") ==========================================" );
        while (it)   {
            const IAlnSegment::TSignedRange& aln_r = it->GetAlnRange(); // in alignment
            const IAlnSegment::TSignedRange& r = it->GetRange(); // on sequence

            size_t off;
            //LOG_POST_X(2, "Aln [" << aln_r.GetFrom() << ", " << aln_r.GetTo() << "], Seq  "
            //                      << r.GetFrom() << ", " << r.GetTo());
            if (base_width == 1) {
                // TODO performance issue - waiting for better API
                if (IsPositiveStrand(row)) {
                    seq_vector.GetSeqData(r.GetFrom(), r.GetToOpen(), s);
                } else {
                    seq_vector.GetSeqData(vec_size - r.GetToOpen(),
                                          vec_size - r.GetFrom(), s);
                }
                if (translate) {
                    TranslateNAToAA(s, s);
                }
                off = aln_r.GetFrom() - aln_range.GetFrom();
                if (translate) {
                    off /= 3;
                }
                off = max(prev_to_open, off);
            } else {
                _ASSERT(base_width == 3);
                IAlnSegment::TSignedRange prot_r = r;
                prot_r.SetFrom(r.GetFrom() / 3);
                prot_r.SetLength(r.GetLength() < 3 ? 1 : r.GetLength() / 3);
                if (IsPositiveStrand(row)) {
                    seq_vector.GetSeqData(prot_r.GetFrom(),
                                          prot_r.GetToOpen(), s);
                } else {
                    seq_vector.GetSeqData(vec_size - prot_r.GetToOpen(),
                                          vec_size - prot_r.GetFrom(), s);
                }
                off = max((TSignedSeqPos) prev_to_open,
                          (aln_r.GetFrom() - aln_range.GetFrom()) / 3);
            }
            /*if(it->IsReversed())    {
                std::reverse(s.begin(), s.end());
            }*/
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
        //LOG_POST_X(3, buffer);
    }
    return buffer;
}


IAlnSegmentIterator*
CSparseAln::CreateSegmentIterator(TNumrow row,
                                  const TSignedRange& range,
                                  IAlnSegmentIterator::EFlags flag) const
{
    _ASSERT(row >= 0  &&  row < GetDim());

    return new CSparse_CI(*m_PairwiseAlns[row], flag, range);
}


END_NCBI_SCOPE
