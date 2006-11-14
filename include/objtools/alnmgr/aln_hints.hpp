#ifndef OBJTOOLS_ALNMGR___ALN_HINTS__HPP
#define OBJTOOLS_ALNMGR___ALN_HINTS__HPP
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
*   Seq-align hints
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
class CAlnHints : public CObject
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
    CAlnHints(const TAlnVector& aln_vector,
              const TAlnSeqIdVector& aln_seq_id_vector,
              const TAnchorRows* const anchor_rows = 0,
              const TBaseWidths* const base_widths = 0) :
        m_AlnVector(aln_vector),
        m_AlnSeqIdVector(aln_seq_id_vector),
        m_AnchorRows(anchor_rows),
        m_BaseWidths(base_widths)
    {
        _ASSERT(m_AlnVector.size() == GetAlnCount());
        _ASSERT(m_AlnSeqIdVector.size() == GetAlnCount());
        _ASSERT( !m_AnchorRows  ||  m_AnchorRows->size() == GetAlnCount());
        _ASSERT( !m_BaseWidths  ||  m_BaseWidths->size() == GetAlnCount());
    }


    /// How many alignments do we have?
    size_t GetAlnCount() const {
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
        return m_AnchorRows;
    }

    /// Get the anchor row within an alignment
    TDim GetAnchorRowForAln(size_t aln_idx) const {
        if (IsAnchored()) {
            _ASSERT(IsAnchored());
            _ASSERT(aln_idx < GetAlnCount());
            _ASSERT((*m_AnchorRows)[aln_idx] >= 0);
            return (*m_AnchorRows)[aln_idx];
        } else {
            return -1;
        }
    }


    /// What are the base widths?
    const int GetBaseWidthForAlnRow(size_t aln_idx, TDim row) const {
        if (m_BaseWidths) {
            _ASSERT(aln_idx < m_BaseWidths->size());
            _ASSERT((TDim)(*m_BaseWidths)[aln_idx].size() == GetDimForAln(aln_idx));
            _ASSERT(row < (TDim)(*m_BaseWidths)[aln_idx].size());
            return (*m_BaseWidths)[aln_idx][row];
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
    const TAnchorRows* const m_AnchorRows;
    const TBaseWidths* const m_BaseWidths;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2006/11/14 20:34:36  todorov
* Added typedefs.
*
* Revision 1.6  2006/11/09 00:16:38  todorov
* Fixed Dump.
*
* Revision 1.5  2006/11/08 22:27:17  todorov
* + template <class TOutStream> void Dump(TOutStream& os)
*
* Revision 1.4  2006/11/08 17:42:54  todorov
* Cleaned code.
*
* Revision 1.3  2006/11/07 20:37:10  todorov
* Simplified version.
*
* Revision 1.2  2006/11/06 19:58:57  todorov
* Added aln statistics.
*
* Revision 1.1  2006/10/19 17:21:51  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_HINTS__HPP
