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


template <class TAlnSeqIdVector
class CAlnHints : public CObject
{
public:
    typedef CSeq_align::TDim TDim;
    typedef vector<TSeqIdPtr> > TSeqIdVector;


    /// Constructor
    CAlnHints(const TAlnVector& aln_vector,
              TSeqIdPtrComp& seq_id_ptr_comp) :
        m_AlnVector(aln_vector),
        m_Comp(seq_id_ptr_comp),
        m_AlnSeqIdVector(m_AlnVector, m_Comp)
    {
    }


    /// Access the underlying vector of alignments
    const TAlnVector& GetAlnVector() const {
        return m_AlnVector;
    }

    /// Do the alignments have a common anchor?
    bool IsAnchored() const {
        return m_AnchorHandle;
    }

    /// Get the anchor bioseq handle
    const CBioseq_Handle& GetAnchorHandle() const {
        _ASSERT(IsAnchored());
        return m_AnchorHandle;
    }

    /// Get the anchor row within an alignment
    const TDim& GetAnchorRowForAln(size_t aln_idx) const {
        _ASSERT(IsAnchored());
        if (m_AnchorRows.empty()) {
            m_AnchorRows.resize(m_AlnVector.size());
        }
        _ASSERT( !m_AnchorRows.empty() );
        if (m_AnchorRows[aln_idx] < 0) {
            /// Determine anchor row
            const TSeqIdVector& seq_ids = GetSeqIdsForAln(aln_idx);
            for (size_t row = 0; row < seq_ids.size(); ++row)   {
                if ( !(comp(ids[row], m_AnchorId) ||
                       comp(m_AnchorId, ids[row])) ) {
                    m_AnchorRows[aln_idx] = row;
                    break;
                }
            }
        }
        _ASSERT(m_AnchorRows[aln_idx] >= 0);
        return m_AnchorRows[aln_idx];
    }

    /// Access a vector of seq-ids for an alignment
    const TSeqIdVector& GetSeqIdsForAln(size_t aln_idx) const {
        return m_AlnSeqIdVector[aln_idx];
    }

    /// What is the dimension of an alignment?
    const TDim& GetDimForAln(size_t aln_idx) {
        return m_AlnSeqIdVector[aln_idx].size();
    }

    const int GetBaseWidthForAlnRow(size_t aln_idx, TDim row) {
        return 0;
    }

private:
    cosnt TAlnVector& m_AlnVector;
    TSeqIdPtrComp& m_Comp;

    typedef typename CAlnSeqIdVector<TAlnVector,
                                     TSeqIdPtrComp,
                                     TSeqIdPtr> TAlnSeqIdVector;

    TAlnSeqIdVector m_AlnSeqIdVector;

    CBioseq_Handle m_AnchorHandle;
    TSeqIdPtr m_AnchorId;
    vector<TDim> m_AnchorRows;

};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/11/06 19:58:57  todorov
* Added aln statistics.
*
* Revision 1.1  2006/10/19 17:21:51  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_HINTS__HPP
