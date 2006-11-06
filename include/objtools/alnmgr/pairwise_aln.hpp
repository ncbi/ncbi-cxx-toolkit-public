#ifndef OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
#define OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
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
*   Pairwise and query-anchored alignments
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objtools/alnmgr/diag_rng_coll.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// A pairwise aln is a collection of diag ranges for a pair of rows
class CPairwiseAln : public CObject, public CDiagRngColl
{
public:
    typedef CSeq_align::TDim TDim;
    
    /// Constructors:
    CPairwiseAln(const CSeq_align& seq_align,
                 TDim row_1,
                 TDim row_2,
                 int base_width_1 = 1,
                 int base_width_2 = 1);

    /// Accessors:
    const CSeq_align& GetSeqAlign() const {
        return *m_SeqAlign;
    }
    const TDim& GetRow1() const {
        return m_Row1;
    }
    const TDim& GetRow2() const {
        return m_Row2;
    }

private:
    CConstRef<CSeq_align> m_SeqAlign;
    TDim m_Row1;
    TDim m_Row2;
    int m_BaseWidth1;
    int m_BaseWidth2;

    void x_BuildFromDenseg();
    void x_BuildFromStdseg();

    typedef CRange<TSeqPos> TRng;
    TRng m_Rng;
};


/// Query-anchored alignment can be 2 or multi-dimentional
class CAnchoredAln : public CObject
{
public:
    CConstRef<CSeq_align> m_SeqAlign;
    
    typedef vector<CConstRef<CSeq_id> > TSeqIds;
    TSeqIds m_SeqIds;

    typedef vector<CConstRef<CPairwiseAln> > TPairwiseAlns;
    TPairwiseAlns m_PairwiseAlns;
};



END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2006/11/06 19:55:08  todorov
* Added base widths.
*
* Revision 1.3  2006/10/19 20:19:11  todorov
* CPairwiseAln is a CDiagRngColl now.
*
* Revision 1.2  2006/10/19 17:11:05  todorov
* Minor refactoring.
*
* Revision 1.1  2006/10/17 19:59:36  todorov
* Initial revision
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
