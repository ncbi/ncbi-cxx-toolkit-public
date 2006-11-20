#ifndef OBJTOOLS_ALNMGR___ALN_STATS__HPP
#define OBJTOOLS_ALNMGR___ALN_STATS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-align statistics
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>


BEGIN_NCBI_SCOPE


template <class _TAlnVector,
          class _TSeqIdVector,
          class _TAlnSeqIdVector>
class CAlnStats : public CObject
{
public:
    /// Typedefs
    typedef _TAlnVector TAlnVector;
    typedef _TSeqIdVector TSeqIdVector;
    typedef _TAlnSeqIdVector TAlnSeqIdVector;
    typedef objects::CSeq_align::TDim TDim;
    typedef vector< vector<int> > TBaseWidths;
    typedef vector<TDim> TAnchorRows;

    /// Constructor
    CAlnStats(const TAlnVector& aln_vector,
              const TAlnSeqIdVector& aln_seq_id_vector,
              const TAnchorRows& anchor_rows,
              const TBaseWidths& base_widths) :
        m_AlnVector(aln_vector),
        m_AlnSeqIdVector(aln_seq_id_vector),
        m_AnchorRows(anchor_rows),
        m_BaseWidths(base_widths)
    {
        _ASSERT(m_AlnVector.size() == GetAlnCount());
        _ASSERT(m_AlnSeqIdVector.size() == GetAlnCount());
        _ASSERT(m_AnchorRows.empty()  ||  m_AnchorRows.size() == GetAlnCount());
        _ASSERT(m_BaseWidths.empty()  ||  m_BaseWidths.size() == GetAlnCount());
    }

    /// How many alignments do we have?
    size_t GetAlnCount() const {
        _ASSERT(m_AlnVector.size() ==  m_AlnSeqIdVector.size());
        return m_AlnVector.size();
    }

    /// Access the underlying vector of alignments
    const TAlnVector& GetAlnVector() const {
        return m_AlnVector;
    }


    /// What is the dimension of an alignment?
    TDim GetDimForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnSeqIdVector[aln_idx].size();
    }

    /// Access a vector of seq-ids for an alignment
    const TSeqIdVector& GetSeqIdsForAln(size_t aln_idx) const {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnSeqIdVector[aln_idx];
    }


    /// Do the alignments share a common sequence (anchor)?
    bool IsAnchored() const {
        _ASSERT(m_AnchorRows.empty()  ||  m_AnchorRows.size() == GetAlnCount());
        return !m_AnchorRows.empty();
    }

    /// Get the anchor row within an alignment
    TDim GetAnchorRowForAln(size_t aln_idx) const {
        if (IsAnchored()) {
            _ASSERT(m_AnchorRows.size() == GetAlnCount());
            _ASSERT(aln_idx < m_AnchorRows.size());
            _ASSERT(m_AnchorRows[aln_idx] >= 0);
            _ASSERT(m_AnchorRows[aln_idx] < GetDimForAln(aln_idx));
            return m_AnchorRows[aln_idx];
        } else {
            return -1;
        }
    }


    /// What are the base widths?
    const int GetBaseWidthForAlnRow(size_t aln_idx, TDim row) const {
        if ( !m_BaseWidths.empty() ) {
            _ASSERT(m_BaseWidths.size() == GetAlnCount());
            _ASSERT(aln_idx < m_BaseWidths.size());
            _ASSERT((TDim)m_BaseWidths[aln_idx].size() == GetDimForAln(aln_idx));
            _ASSERT(row < (TDim)m_BaseWidths[aln_idx].size());
            return m_BaseWidths[aln_idx][row];
        } else {
            return 1;
        }
    }


    /// Dump in human readable text format:
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        os << "Number of alignments: " << GetAlnCount() << endl;
        os << "QueryAnchoredTest:"     << IsAnchored() << endl;
        os << endl;
        for (size_t aln_idx = 0;  aln_idx < GetAlnCount();  ++aln_idx) {
            TDim dim = GetDimForAln(aln_idx);
            os << "Alignment " << aln_idx << " has " 
               << dim << " rows:" << endl;
            for (TDim row = 0;  row < dim;  ++row) {
                GetSeqIdsForAln(aln_idx)[row]->WriteAsFasta(os);
                os << " [base width: " 
                   << GetBaseWidthForAlnRow(aln_idx, row)
                   << "]";
                if (IsAnchored()  &&  
                    row == GetAnchorRowForAln(aln_idx)) {
                    os << " (anchor)";
                }
                os << endl;
            }
            os << endl;
        }
    }


private:
    const TAlnVector& m_AlnVector;
    const TAlnSeqIdVector& m_AlnSeqIdVector;
    const TAnchorRows& m_AnchorRows;
    const TBaseWidths& m_BaseWidths;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/11/20 18:43:23  todorov
* anchor rows and base widtsh are now const refs.
*
* Revision 1.1  2006/11/17 05:33:35  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_STATS__HPP
