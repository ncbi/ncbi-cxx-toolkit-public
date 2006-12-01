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
class NCBI_XALNMGR_EXPORT CPairwiseAln : 
    public CObject,
    public CAlignRangeCollection<CAlignRange<TSignedSeqPos> >
{
public:
    // Types
    typedef TSignedSeqPos                  TPos;
    typedef CRange<TPos>                   TRng; 
    typedef CAlignRange<TPos>              TAlnRng;
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;


    /// Constructor
    CPairwiseAln(int flags = fDefaultPoicy,
                 int first_base_width = 1,
                 int second_base_width = 1)
        : TAlnRngColl(flags),
          m_FirstBaseWidth(first_base_width),
          m_SecondBaseWidth(second_base_width) {}


    /// Base width of the first row
    int GetFirstBaseWidth() const {
        return m_FirstBaseWidth;
    }

    /// Base width of the second row
    int GetSecondBaseWidth() const {
        return m_SecondBaseWidth;
    }


    /// Dump in human readable text format
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        char s[32];
        sprintf(s, "%X", (unsigned int) GetFlags());
        os << "CPairwiseAln";

        os << " BaseWidths: " 
           << m_FirstBaseWidth << ", "
           << m_SecondBaseWidth << " ";

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
    int m_FirstBaseWidth;
    int m_SecondBaseWidth;
};


/// Query-anchored alignment can be 2 or multi-dimentional
class NCBI_XALNMGR_EXPORT CAnchoredAln : public CObject
{
public:
    // Types
    typedef CSeq_align::TDim TDim;
    typedef CConstRef<CSeq_id> TSeqIdPtr;
    typedef vector<TSeqIdPtr> TSeqIdVector;
    typedef vector<CRef<CPairwiseAln> > TPairwiseAlnVector;


    /// Default constructor
    CAnchoredAln()
        : m_AnchorRow(kInvalidAnchorRow),
          m_Score(0)
    {
    }

    /// NB: Copy constructor is deep on pairwise_alns so that
    /// pairwise_alns can be modified
    CAnchoredAln(const CAnchoredAln& c)
        : m_AnchorRow(c.m_AnchorRow),
          m_SeqIds(c.m_SeqIds),
          m_Score(c.m_Score)
    {
        m_PairwiseAlns.resize(c.GetDim());
        for (TDim row = 0;  row < c.GetDim();  ++row) {
            CRef<CPairwiseAln> pairwise_aln
                (new CPairwiseAln(*c.m_PairwiseAlns[row]));
            m_PairwiseAlns[row].Reset(pairwise_aln);
        }
    }
        

    /// NB: Assignment operator is deep on pairwise_alns so that
    /// pairwise_alns can be modified
    CAnchoredAln& operator=(const CAnchoredAln& c)
    {
        if (this == &c) {
            return *this; // prevent self-assignment
        }
        m_AnchorRow = c.m_AnchorRow;
        m_SeqIds = c.m_SeqIds;
        m_Score = c.m_Score;
        m_PairwiseAlns.resize(c.GetDim());
        for (TDim row = 0;  row < c.GetDim();  ++row) {
            CRef<CPairwiseAln> pairwise_aln
                (new CPairwiseAln(*c.m_PairwiseAlns[row]));
            m_PairwiseAlns[row].Reset(pairwise_aln);
        }
        return *this;
    }
        

    /// How many rows
    TDim GetDim() const {
        _ASSERT(m_SeqIds.size() == m_PairwiseAlns.size());
        _ASSERT(m_AnchorRow == (TDim) m_SeqIds.size() - 1);
        return (TDim) m_SeqIds.size();
    }

    /// Seq ids of the rows
    const TSeqIdVector& GetSeqIds() const { 
        return m_SeqIds;
    }

    /// The vector of pairwise alns
    const TPairwiseAlnVector& GetPairwiseAlns() const {
        return m_PairwiseAlns;
    }

    /// Which is the anchor row?
    TDim GetAnchorRow() const {
        _ASSERT(m_AnchorRow != kInvalidAnchorRow);
        _ASSERT(m_AnchorRow < GetDim());
        return m_AnchorRow;
    }

    /// What is the seq id of the anchor?
    const TSeqIdPtr& GetAnchorId() const {
        return m_SeqIds[m_AnchorRow];
    }

    /// What is the total score?
    int GetScore() const {
        return m_Score;
    }


    /// Modify the number of rows.  NB: This resizes the vectors and
    /// potentially invalidates the anchor row.  Never do this unless
    /// you know what you're doing)
    void SetDim(TDim dim) {
        _ASSERT(m_AnchorRow == kInvalidAnchorRow); // make sure anchor is not set yet
        m_SeqIds.resize(dim);
        m_PairwiseAlns.resize(dim);
    }

    /// Modify seq-ids
    TSeqIdVector& SetSeqIds() {
        return m_SeqIds;
    }

    /// Modify pairwise alns
    TPairwiseAlnVector& SetPairwiseAlns() {
        return m_PairwiseAlns;
    }

    /// Modify anchor row (never do this unless you are creating a new
    /// alignment and know what you're doing)
    void SetAnchorRow(TDim anchor_row) {
        m_AnchorRow = anchor_row;
    }

    /// Set the total score
    void SetScore(int score) {
        m_Score = score;
    }

    /// Non-const access to the total score
    int& SetScore() {
        return m_Score;
    }


    /// Dump in human readable text format:
    template <class TOutStream>
    void Dump(TOutStream& os) const {
        os << "CAnchorAln has score of " << m_Score << " and contains " 
           << m_PairwiseAlns.size() << " pair(s) of rows:" << endl;
        ITERATE(TPairwiseAlnVector, pairwise_aln_i, m_PairwiseAlns) {
            (*pairwise_aln_i)->Dump(os);
        }
        os << endl;
    }


private:
    static const TDim  kInvalidAnchorRow = -1;
    TDim               m_AnchorRow;
    TSeqIdVector       m_SeqIds;
    TPairwiseAlnVector m_PairwiseAlns;
    int                m_Score;
};


template<class C>
struct PScoreGreater
{
    bool operator()(const C* const c_1, const C* const c_2)
    {
        return c_1->GetScore() > c_2->GetScore();
    }
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.13  2006/12/01 17:53:44  todorov
* + NCBI_XALNMGR_EXPORT
*
* Revision 1.12  2006/11/22 00:54:13  todorov
* Prevent self-assignment in the assignment operator.
*
* Revision 1.11  2006/11/22 00:46:46  todorov
* Various fixes.
*
* Revision 1.10  2006/11/16 22:36:37  todorov
* + score
*
* Revision 1.9  2006/11/16 18:05:32  todorov
* + {G,S}etAnchorRow
*
* Revision 1.8  2006/11/16 13:42:33  todorov
* Changed the position type from TSeqPos to TSignedSeqPos.
*
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
