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

#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// A pairwise aln is a collection of ranges for a pair of rows
class CPairwiseAln : 
    public CObject,
    public CAlignRangeCollection<CAlignRange<TSeqPos> >
{
public:
    /// Types
    typedef CAlignRange<TSeqPos>                 TAlnRng;
    typedef CAlignRangeCollection<TAlnRng>       TAlnRngColl;


    /// Constructor
    CPairwiseAln(int flags = fDefaultPoicy,
                 int base_width_1 = 1,
                 int base_width_2 = 1)
        : TAlnRngColl(flags),
          m_BaseWidth1(base_width_1),
          m_BaseWidth2(base_width_2) {}


    /// Accessors:
    int GetBaseWidth1() const {
        return m_BaseWidth1;
    }

    int GetBaseWidth2() const {
        return m_BaseWidth2;
    }

    /// Dump in human readable text format:
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        char s[32];
        sprintf(s, "%X", (unsigned int) GetFlags());
        os << "CPairwiseAln";

        os << " BaseWidths: " 
           << m_BaseWidth1 << ", "
           << m_BaseWidth2 << " ";

        os << " Flags = " << s << ":" << endl;

        if (m_Flags & fKeepNormalized) os << "fKeepNormalized" << endl;
        if (m_Flags & fAllowMixedDir) os << "fAllowMixedDir" << endl;
        if (m_Flags & fAllowOverlap) os << "fAllowOverlap" << endl;
        if (m_Flags & fAllowAbutting) os << "fAllowAbutting" << endl;
        if (m_Flags & fNotValidated) os << "fNotValidated" << endl;
        if (m_Flags & fInvalid) os << "fInvalid" << endl;
        if (m_Flags & fUnsorted) os << "fUnsorted" << endl;
        if (m_Flags & fDirect) os << "fDirect" << endl;
        if (m_Flags & fReversed) os << "fReversed" << endl;
        if ((m_Flags & fMixedDir) == fMixedDir) os << "fMixedDir" << endl;
        if (m_Flags & fOverlap) os << "fOverlap" << endl;
        if (m_Flags & fAbutting) os << "fAbutting" << endl;
        
        ITERATE (TAlnRngColl, rng_it, *this) {
            const TAlnRng& rng = *rng_it;
            os << "[" 
               << rng.GetFirstFrom() << ", " 
               << rng.GetSecondFrom() << ", "
               << rng.GetLength() << ", " 
               << (rng.IsDirect() ? "true" : "false") 
               << "]";
        }
        os << endl;
    }

private:
    int m_BaseWidth1;
    int m_BaseWidth2;
};


/// Query-anchored alignment can be 2 or multi-dimentional
class CAnchoredAln : public CObject
{
public:
    /// Types:
    typedef CSeq_align::TDim TDim;
    typedef vector<CConstRef<CSeq_id> > TSeqIds;
    typedef vector<CRef<CPairwiseAln> > TPairwiseAlnVector;


    /// Accessors:
    TDim GetDim() const {
        _ASSERT(m_SeqIds.size() == m_PairwiseAlns.size());
        return (TDim) m_SeqIds.size();
    }

    const TSeqIds& GetSeqIds() const {
        return m_SeqIds;
    }

    const TPairwiseAlnVector& GetPairwiseAlns() const {
        return m_PairwiseAlns;
    }

    /// Modifiers:
    TSeqIds& SetSeqIds() {
        return m_SeqIds;
    }

    TPairwiseAlnVector& SetPairwiseAlns() {
        return m_PairwiseAlns;
    }


    /// Dump in human readable text format:
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        os << "CAnchorAln contains " 
           << m_PairwiseAlns.size() << " pair(s) of rows:" << endl;
        ITERATE(TPairwiseAlnVector, pairwise_aln_i, m_PairwiseAlns) {
            (*pairwise_aln_i)->Dump(os);
        }
        os << endl;
    }


private:
    TSeqIds            m_SeqIds;
    TPairwiseAlnVector m_PairwiseAlns;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2006/11/14 20:38:38  todorov
* CPairwiseAln derives directly from CAlignRangeCollection and it's
* construction from CSeq-align is moved to aln_converters.hpp
*
* Revision 1.6  2006/11/09 00:17:27  todorov
* Fixed Dump and added CreateAnchoredAlnFromAln.
*
* Revision 1.5  2006/11/08 22:28:14  todorov
* + template <class TOutStream> void Dump(TOutStream& os)
*
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
