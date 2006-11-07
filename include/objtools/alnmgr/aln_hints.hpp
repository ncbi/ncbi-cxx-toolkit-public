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

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


template <class TAlnVector,
          class TSeqIdPtrComp,
          class TSeqIdPtr = const CSeq_id*>
class CAlnHints : public CObject
{
public:
    typedef CSeq_align::TDim TDim;
    typedef typename CAlnSeqIdVector<TAlnVector,
                                     TSeqIdPtrComp,
                                     TSeqIdPtr> TAlnSeqIdVector;
    typedef vector<TSeqIdPtr> > TSeqIdVector;
    typedef vector<vector<int> > TBaseWidths;
    typedef vector<vector<TDim> > TAnchorRows;

    /// Constructor
    CAlnHints(const TAlnVector& aln_vector,
              TSeqIdPtrComp& seq_id_ptr_comp,
              const TAlnSeqIdVector& aln_seq_id_vector,
              const TAnchorRows* const anchor_rows = 0,
              const TBaseWidths* const base_widths = 0) :
        m_AlnVector(aln_vector),
        m_Comp(seq_id_ptr_comp),
        m_AlnSeqIdVector(aln_seq_id_vector),
        m_AnchorRows(anchor_rows),
        m_BaseWidths(base_widths),
        m_AlnCount(m_AlnVector.size())
    {
        _ASSERT(m_AlnVector.size() == m_AlnCount);
        _ASSERT(m_AlnSeqIdVector.size() == m_AlnCount);
        _ASSERT( !m_AnchorRows  ||  m_AnchorRows->size() == m_AlnCount);
        _ASSERT( !m_BaseWidths  ||  m_BaseWidths->size() == m_AlnCount);
    }

    /// How many alignments do we have?
    size_t GetAlnCount() {
        return m_AlnCount;
    }

    /// Access the underlying vector of alignments
    const TAlnVector& GetAlnVector() const {
        return m_AlnVector;
    }

    /// Do the alignments share a common sequence (anchor)?
    bool IsAnchored() const {
        return m_AnchorRows;
    }

    /// Get the anchor row within an alignment
    const TDim& GetAnchorRowForAln(size_t aln_idx) const {
        if (IsAnchored()) {
            _ASSERT(IsAnchored());
            _ASSERT(aln_idx < GetAlnCount());
            _ASSERT(m_AnchorRows[aln_idx] >= 0);
            return m_AnchorRows[aln_idx];
        } else {
            return -1;
        }
    }

    /// Access a vector of seq-ids for an alignment
    const TSeqIdVector& GetSeqIdsForAln(size_t aln_idx) const {
        return m_AlnSeqIdVector[aln_idx];
    }

    /// What is the dimension of an alignment?
    const TDim& GetDimForAln(size_t aln_idx) {
        _ASSERT(aln_idx < GetAlnCount());
        return m_AlnSeqIdVector[aln_idx].size();
    }

    /// What are the base widths?
    const int GetBaseWidthForAlnRow(size_t aln_idx, TDim row) {
        if (m_BaseWidths) {
            _ASSERT(aln_idx < m_BaseWidths->size());
            _ASSERT(m_BaseWidths->[aln_idx].size() == GetDimForAln(aln_idx));
            _ASSERT(row < m_BaseWidths->[aln_idx].size());
            return m_BaseWidths->[aln_idx][row];
        } else {
            return 1;
        }
    }

private:
    cosnt TAlnVector& m_AlnVector;
    TSeqIdPtrComp& m_Comp;
    const TAlnSeqIdVector& m_AlnSeqIdVector;

    TSeqIdPtr m_AnchorId;
    vector<TDim> m_AnchorRows;

    const TBaseWidths* const m_BaseWidths;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
