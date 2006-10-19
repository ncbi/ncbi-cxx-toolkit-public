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

#include <objtools/alnmgr/aln_hints.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CAlnToAnchoredAlnConverter
{
public:
    typedef vector<CRef<CAnchoredAln> > TAnchoredAlnContainer;

    CAlnToAnchoredAlnConverter(const CAlnHints& hints) : 
        m_Hints(hints),
        m_Comp(m_Hints.m_Comp)
    {}

    template<TAlnContainer>
    void Convert(const TAlnContainer&   input_aln_container,     
                 TAnchoredAlnContainer& output_anchored_aln_container)
    { 
        ITERATE(typename TAlnContainer, sa_it, input_seq_aln_container) {
            CRef<CAnchoredAln> anchored_aln;
            Convert(**sa_it, *anchored_aln);
            output_anchored_aln_container.push_back(anchored_aln);
        }
    }

    void  Convert(const CSeq_align& input_aln,
                  CAnchoredAln&     output_anchored_aln);

    /// TAlnRngColl
    
    typedef TSignedSeqPos TPos;
    typedef CAlignRange<TPos> TAlnRng;
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;

    CreateAlnRngColl(const CDense_seg& ds,
                     CDense_seg::TDim row_1,
                     CDense_seg::TDim row_2,
                     TAlnRngColl& coll,
                     TAlnRng* rng = 0);


private:
    void x_Convert(const CDense_seg& dense_seg);
    void x_Convert(const CSparse_align& sparse_align);
    void x_Convert(const CSparse_seg& sparse_seg);

    const CAlnHints& m_Hints;
    CAlnHints::TSeqIdComp& m_Comp;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/10/19 17:22:31  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
