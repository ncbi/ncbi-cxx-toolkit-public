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
* Authors:  Chris Lanczycki
*
* File Description:
*           Base class for objects that recommends new block boundaries for
*           an AlignmentUtility based on some concrete algorithm.  
*           Concrete instances DO NOT actually edit the alignment - only 
*           reports where the new block boundaries should be placed.
*           Configure instances with a ColumnScorer concrete instance.
*           File also contains virtually inherited subclasses.
*
* ===========================================================================
*/

#ifndef AR_BLOCK_BOUNDARY_ALGORITHM__HPP
#define AR_BLOCK_BOUNDARY_ALGORITHM__HPP

#include <map>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algo/structure/bma_refine/ColumnScorer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

//  Provide location & allowed extension sizes for a block; extension sizes may or may not
//  be the absolute maximum extensions depending on the ExtensionLoc used at their creation.
//  (E.g., a block that could have n and c terminal extentions of 8 residues would
//   produce an ExtendableBlock with nExt=0, cExt=8 if created with ExtensionLoc = eCTerm.)
typedef struct {
    unsigned int blockNum; // aligned block number, from 0
    unsigned int from;   // alignment index of n-terminal end of block
    unsigned int to;     // alignment index of c-terminal end of block
//    unsigned int seqFrom;     // sequence index on master of n-terminal end of block
//    unsigned int seqTo;       // sequence index on master of c-terminal end of block
    unsigned int nExt;     // maximum n-terminal extension size
    unsigned int cExt;     // maximum c-terminal extension size
} ExtendableBlock;


//  Map the alignment index to the score computed for that index.
typedef map< unsigned int, double > ColScoreMap;
typedef ColScoreMap::iterator ColScoreMapIt;
typedef ColScoreMap::value_type ColScoreMapValType;

enum BlockBoundaryAlgorithmMethod {
    eInvalidBBAMethod = 0,
    eSimpleExtend,          //  only try to extend while column score exceeds all thresholds
    eSimpleShrink,          //  only try to shrink while column score doesn't exceed all thresholds
    eSimpleExtendAndShrink, //  try to extend(shrink) a block, then try to shrink(extend) it
    eGreedyExtend           //  evaluate all possible boundary extensions and choose the 'best'
};

class NCBI_BMAREFINE_EXPORT BlockBoundaryAlgorithm {

    static const unsigned int DEFAULT_MIN_BLOCK_SIZE;
public:

    BlockBoundaryAlgorithm(const ColumnScorer* scorer, double ext, double shrink, BlockBoundaryAlgorithmMethod method); 
    virtual ~BlockBoundaryAlgorithm();

    //  Return true if new block boundaries have been recommended.  
    //  If so, the 'block' argument's 'from' and 'to' fields will contain the new boundaries.
    //  This method does not update the n/cExt fields to reflect the new boundaries.
    //  Note that these classes do not actually edit the alignment.
    virtual bool GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const = 0;

    //  If try to add a NULL scorer, it is ignored and nothing is added.
    //  Use same shrinking and extension threshold unless specify otherwise.
    virtual void AddScorer(const ColumnScorer* scorer, double ext, double shrink = kMax_Double);

    const ColumnScorer* GetScorer(unsigned int scorerIndex = 0) const;
    double GetExtensionThreshold(unsigned int scorerIndex = 0) const;
    double GetShrinkageThreshold(unsigned int scorerIndex = 0) const;

    void SetMinBlockSize(unsigned int minBlockSize);
    unsigned int GetMinBlockSize() const;

    void SetCanBlocksShrink(bool canBlocksShrink);
    bool CanBlocksShrink() const;

    BlockBoundaryAlgorithmMethod GetMethod() const;

    void ComputeColumnScores(const BMA& bma, unsigned int minIndex, unsigned int maxIndex, ColScoreMap& scores, unsigned int scorerIndex = 0) const;

protected:

    BlockBoundaryAlgorithmMethod m_boundaryMethod;
    unsigned int m_minBlockSize;
    bool m_canBlocksShrink;

    vector<const ColumnScorer*> m_columnScorers;
    vector<double> m_extensionThresholds;    //  minimal column score needed in order to extend a column
    vector<double> m_shrinkageThresholds; //  minimal column score for a column to be kept in a block
};


//  The algorithm is to independently extend the N- and C-termini column by column
//  while the column score equals or exceeds the threshold value in 'm_extensionThreshold'.  
//  for each scorer present.  Stop extending at the last such column on each terminus.
class NCBI_BMAREFINE_EXPORT SimpleBoundaryExtender : public BlockBoundaryAlgorithm {
   
public:

    SimpleBoundaryExtender(const ColumnScorer* scorer, double ext) : BlockBoundaryAlgorithm(scorer, ext, 0.0, eSimpleExtend) {
        m_canBlocksShrink = false;
    }
    virtual ~SimpleBoundaryExtender() {};
    
    virtual bool GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const;

private:
    //  True only if column scores all excede corresponding extension thresholds.
    bool PassAllTests(const BMA& bma, unsigned int alignmentIndex) const;

};
 

//  The algorithm is to independently shrink the N- and C-termini column by column
//  while the column score is less than the threshold value 'm_shrinkageThreshold'.  
//  Stop shrinking at the last such column on each terminus.  If 'm_minBlockSize' is zero,
//  block deletion is possible; such an event is indicated by the ExtendableBlock returned
//  in GetNewBoundaries having its 'from' field greater than its 'to' field.
class NCBI_BMAREFINE_EXPORT SimpleBoundaryShrinker : public BlockBoundaryAlgorithm {

public:

    SimpleBoundaryShrinker(const ColumnScorer* scorer, double shrink) : BlockBoundaryAlgorithm(scorer, 0.0, shrink, eSimpleExtend) {
          m_canBlocksShrink = true;
}

    virtual ~SimpleBoundaryShrinker() {};

    virtual bool GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const;

private:
    //  True only if column scores all are smaller than corresponding shrinkage thresholds
    //  and a shrink will occur.
    bool PassAllTests(const BMA& bma, unsigned int alignmentIndex) const;

};


static const bool EXTEND_FIRST = true;
static const bool SHRINK_FIRST = false;


//  The algorithm is to:
//  i)  m_extendFirst = true:   for each block, apply the SimpleBoundaryExtender algorithm 
//      accepting extensions and then try and shrink the block by applying 
//      the SimpleBoundaryShrinker algorithm.  False returned if no change to the block.
//  ii) m_extendFirst = false:  for each block, apply the SimpleBoundaryShrinker algorithm 
//      accepting new boundaries and then try and extend the block by applying 
//      the SimpleBoundaryExtender algorithm.  False returned if no change to the block.
//
//  Allow two scorers to be passed in so that the extension/shrinkage can be controlled by
//  different scoring methods.  If 'secondScorer' is NULL, same scorer is used in both modes.
//
//  Warning:  depending on how parameters to scores & algorithm are set step two may
//            undo in part or in total the boundary moves from step one.  Roughly equal
//            extension and shrinkage thresholds should minimize this occurring.
class NCBI_BMAREFINE_EXPORT SimpleBoundaryExtenderAndShrinker : public BlockBoundaryAlgorithm {

public:

    SimpleBoundaryExtenderAndShrinker(bool extendFirst, const ColumnScorer* scorer, double ext, double shrink, const ColumnScorer* secondScorer = NULL, double ext2 = 0, double shrink2 = 0);  
    virtual ~SimpleBoundaryExtenderAndShrinker();

    virtual bool GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const;

    //  First version uses same scorer set for extend and shrink phases; 
    //  second/third adds only to the specified vector to allow for distinct scorer sets
    virtual void AddScorer(const ColumnScorer* scorer, double ext, double shrink = kMax_Double);
    void AddFirstScorer(const ColumnScorer* scorer2, double ext2, double shrink2 = kMax_Double);
    void AddSecondScorer(const ColumnScorer* scorer2, double ext2, double shrink2 = kMax_Double);

private:  

    bool m_extendFirst;
    vector<const ColumnScorer*> m_secondColumnScorers;
    vector<double> m_secondExtensionThresholds;
    vector<double> m_secondShrinkageThresholds;

};

//  The algorithm is to independently extend the N- and C-termini.  At each termini,
//  scores for each possible block boundary are evaluated and the boundary position 
//  with the best score is chosen.  If 'ext' is a valid score, the best score must be equal 
//  to or better than this value.  In case of ties, choose the larger extension.
//  NOTE:  if multiple scorers, choose the largest extension from the possibilities.
class NCBI_BMAREFINE_EXPORT GreedyBoundaryExtender : public BlockBoundaryAlgorithm {
   
public:

    GreedyBoundaryExtender(const ColumnScorer* scorer, double ext = ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED) : BlockBoundaryAlgorithm(scorer, ext, 0.0, eGreedyExtend) {
        m_canBlocksShrink = false;
    }
    virtual ~GreedyBoundaryExtender() {};
    
    virtual bool GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const;

protected:
    bool MeetsExtensionThreshold(double score, unsigned int scorerIndex = 0) const;
};
 

/*
* ---------------------------------------------------------------------------
* Inlined methods
* ---------------------------------------------------------------------------
*/


inline 
BlockBoundaryAlgorithmMethod BlockBoundaryAlgorithm::GetMethod() const {
    return m_boundaryMethod;
}

inline 
const ColumnScorer* BlockBoundaryAlgorithm::GetScorer(unsigned int index) const {
    return (index < m_columnScorers.size()) ? m_columnScorers[index] : NULL;
}

inline
double BlockBoundaryAlgorithm::GetExtensionThreshold(unsigned int index) const {
    return (index < m_extensionThresholds.size()) ? m_extensionThresholds[index] : 0;
}

inline 
double BlockBoundaryAlgorithm::GetShrinkageThreshold(unsigned int index) const {
    return (index < m_shrinkageThresholds.size()) ? m_shrinkageThresholds[index] : 0;
}

inline
void BlockBoundaryAlgorithm::SetMinBlockSize(unsigned int minBlockSize) {
    m_minBlockSize = minBlockSize;
}

inline
unsigned int BlockBoundaryAlgorithm::GetMinBlockSize() const {
    return m_minBlockSize;
}

inline
void BlockBoundaryAlgorithm::SetCanBlocksShrink(bool canBlocksShrink) {
    m_canBlocksShrink = canBlocksShrink;
}

inline
bool BlockBoundaryAlgorithm::CanBlocksShrink() const {
    return m_canBlocksShrink;
}

END_SCOPE(align_refine)

#endif // AR_BLOCK_BOUNDARY_ALGORITHM__HPP
