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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Pairwise and query-anchored alignments
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>

#include <objtools/alnmgr/aln_seqid.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// A pairwise aln is a collection of ranges for a pair of rows
class CPairwiseAln : 
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
    CPairwiseAln(const TAlnSeqIdIRef& first_id,
                 const TAlnSeqIdIRef& second_id,
                 int flags = fDefaultPolicy)
        : TAlnRngColl(flags),
          m_FirstId(first_id),
          m_SecondId(second_id) {}


    /// Base width of the first row
    int GetFirstBaseWidth() const {
        return m_FirstId->GetBaseWidth();
    }

    /// Base width of the second row
    int GetSecondBaseWidth() const {
        return m_SecondId->GetBaseWidth();
    }

    const TAlnSeqIdIRef& GetFirstId() const {
        return m_FirstId;
    }

    const TAlnSeqIdIRef& GetSecondId() const {
        return m_SecondId;
    }

private:
    TAlnSeqIdIRef m_FirstId;
    TAlnSeqIdIRef m_SecondId;
};


/// Query-anchored alignment can be 2 or multi-dimentional
class NCBI_XALNMGR_EXPORT CAnchoredAln : public CObject
{
public:
    // Types
    typedef int TDim;
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
        _ASSERT(m_AnchorRow == (TDim) m_PairwiseAlns.size() - 1);
        return (TDim) m_PairwiseAlns.size();
    }

    /// Seq ids of the rows
    const TAlnSeqIdIRef& GetId(TDim row) const { 
        return GetPairwiseAlns()[row]->GetSecondId();
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
    const TAlnSeqIdIRef& GetAnchorId() const {
        return GetId(m_AnchorRow);
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
        m_PairwiseAlns.resize(dim);
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


    /// Split rows with mixed dir into separate rows
    /// returns true if the operation was performed
    bool SplitStrands();


private:
    static const TDim  kInvalidAnchorRow = -1;
    TDim               m_AnchorRow;
    TPairwiseAlnVector m_PairwiseAlns;
    int                m_Score;
};


typedef vector<CRef<CAnchoredAln> > TAnchoredAlnVec;


template<class C>
struct PScoreGreater
{
    bool operator()(const C* const c_1, const C* const c_2) const
    {
        return c_1->GetScore() > c_2->GetScore();
    }
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
