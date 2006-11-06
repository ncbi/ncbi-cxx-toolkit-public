#ifndef OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
#define OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
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
*   Seq-align converters
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class objects::CSeq_align;


template <THints>
class CSeqAlignToAnchored : public binary_function
{
public:
    CSeqAlignToAnchored(const THints& hints) :
        m_Hints(hints)
    {
    };

    CRef<CAnchoredAln> operator()(size_t aln_idx) {
        _ASSERT(m_Hints.IsAnchored());

        cosnt CSeq_align& sa = m_Hints.GetAlnForIdx(aln_idx);
        const TDim& anchor_row = m_Hints.GetAnchorRowForAln(aln_idx++);

        CRef<CAnchoredAln> anchored_aln;

        for (TDim row = 0; row < ;  ++row) {
            if (row != anchor_row) {
                CConstRef<CPairwiseAln> pairwise_aln
                    (new CPairwiseAln(sa,
                                      anchor_row,
                                      row,
                                      GetBaseWidthForAlnRow(aln_idx, anchor_row),
                                      GetBaseWidthForAlnRow(aln_idx, row)));
                anchored_aln.push_back(pairwise_aln);
            }
        }
        return anchored_aln;
    }

private:
    typedef typename THints::TDim TDim;

    cosnt THints& m_Hints;
    typename THints::TSeqIdComp& m_Comp;

};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/11/06 19:53:50  todorov
* CSeqAlignToAnchored converts alignments given hints and using CPairwiseAln.
*
* Revision 1.1  2006/10/19 17:22:31  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
