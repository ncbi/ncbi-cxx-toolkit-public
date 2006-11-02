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
*           Base class for objects that select new block boundaries for
*           an AlignmentUtility based on some concrete algorithm.  Should
*           be configured with a ColumnScorer concrete instance.
*           File also contains virtually inherited subclasses.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <algo/structure/bma_refine/BlockBoundaryAlgorithm.hpp>

USING_NCBI_SCOPE;

BEGIN_SCOPE(align_refine)

const unsigned int BlockBoundaryAlgorithm::DEFAULT_MIN_BLOCK_SIZE = 1;

BlockBoundaryAlgorithm::BlockBoundaryAlgorithm(const ColumnScorer* scorer, double ext, double shrink, BlockBoundaryAlgorithmMethod method) 
    : m_boundaryMethod(method), m_minBlockSize(DEFAULT_MIN_BLOCK_SIZE), m_canBlocksShrink(false) {
    AddScorer(scorer, ext, shrink);
}

BlockBoundaryAlgorithm::~BlockBoundaryAlgorithm() {
//    for (unsigned int i = 0; i < m_columnScorers.size(); ++i) {
//        delete m_columnScorers[i];
//    }
//    m_columnScorers.clear();
}

void BlockBoundaryAlgorithm::AddScorer(const ColumnScorer* scorer, double ext, double shrink) {
    if (scorer) {
        m_columnScorers.push_back(scorer);
        m_extensionThresholds.push_back(ext);
        m_shrinkageThresholds.push_back((shrink == kMax_Double) ? ext : shrink);
    }
}

void BlockBoundaryAlgorithm::ComputeColumnScores(const BMA& bma, unsigned int minIndex,
                                                 unsigned int maxIndex, ColScoreMap& scores,
                                                 unsigned int scorerIndex) const {
    unsigned int i = minIndex;
    if (scorerIndex < m_columnScorers.size() && m_columnScorers[scorerIndex]) {
        for (; i <= maxIndex; ++i) {
            scores.insert(ColScoreMapValType(i, m_columnScorers[scorerIndex]->ColumnScore(bma, i)));
        }
    }
}

/*********************/

bool SimpleBoundaryExtender::PassAllTests(const BMA& bma, unsigned int alignmentIndex) const {
    bool tryNextScorer;
    double columnScore;
    vector<double> rowScoreCache;

    tryNextScorer = true;
    rowScoreCache.clear();
    for (unsigned int j = 0; tryNextScorer && j < m_columnScorers.size(); ++j) {
        columnScore = m_columnScorers[j]->ColumnScore(bma, alignmentIndex, &rowScoreCache);
        tryNextScorer = (columnScore >= m_extensionThresholds[j]);
        TRACE_MESSAGE_CL("          (aindex, score, thold, scorer) = (" << alignmentIndex+1 << ", " << columnScore << ", " << m_extensionThresholds[j] << ", " << j << ") PASSED? " << tryNextScorer);
    }
    return tryNextScorer;
}


bool SimpleBoundaryExtender::GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const {

    bool result = false;
    unsigned int i;
    unsigned int newFrom  = block.from, newTo = block.to;
    unsigned int minIndex = block.from - block.nExt;
    unsigned int maxIndex = block.to   + block.cExt;

    //  Since this class only extends the bounds, do not allow block shrinkage.
    unsigned int minIndexCore = block.from, maxIndexCore = block.to;  

    if (m_columnScorers.size() > 0 && (block.nExt > 0 || block.cExt > 0)) {
        //  extend N-terminal bound until a column score is extension threshold
        i = minIndexCore-1;
        while (i >= minIndex) {

            // if all tests are passed, extend
            TRACE_MESSAGE_CL("GetNewBoundaries:  N term check scores for block " << block.blockNum + 1);
            if (PassAllTests(bma, i)) {  
                newFrom = i;
            } else {
                i = minIndex;
            }

            //  If i == 0, --i makes i = kMax_UInt!!
            if (i == 0) break;
            --i;
        }

        //  extend C-terminal bound until a column score is extension threshold
        i = maxIndexCore+1;
        while (i <= maxIndex) {

            // if all tests are passed, extend
            TRACE_MESSAGE_CL("GetNewBoundaries:  C term check scores for block " << block.blockNum + 1);
            if (PassAllTests(bma, i)) {
                newTo = i;
            } else {
                i = maxIndex;
            }
            ++i;
        }

        result = (block.from != newFrom || block.to != newTo);
        if (result) {
            TRACE_MESSAGE_CL("\nGetNewBoundaries:  EXTENSION -- (oldfrom, oldto; newfrom, newto) = (" << block.from+1 << ", "  << block.to+1 << "; " << newFrom+1 << ", " << newTo+1 << ")\n");
        } else {
            TRACE_MESSAGE_CL("\nGetNewBoundaries:  NO EXTENSION -- (oldfrom, oldto) = (" << block.from+1 << ", " << block.to+1 << ")\n");
        }
        block.from = newFrom;
        block.to   = newTo;
    }

    return result;
}


/*********************/
  
bool SimpleBoundaryShrinker::PassAllTests(const BMA& bma, unsigned int alignmentIndex) const {
    bool tryNextScorer;
    double columnScore;
    vector<double> rowScoreCache;

    tryNextScorer = true;
    rowScoreCache.clear();
    for (unsigned int j = 0; tryNextScorer && j < m_columnScorers.size(); ++j) {
        columnScore = m_columnScorers[j]->ColumnScore(bma, alignmentIndex, &rowScoreCache);
        tryNextScorer = (columnScore < m_shrinkageThresholds[j]);
        TRACE_MESSAGE_CL("          (aindex, score, thold, scorer) = (" << alignmentIndex+1 << ", " << columnScore << ", " << m_shrinkageThresholds[j] << ", " << j << ") PASSED? " << tryNextScorer);
    }
    return tryNextScorer;
}

bool SimpleBoundaryShrinker::GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const {

    bool result = false;
    bool stopShrinking = false;
    unsigned int newFrom  = block.from, newTo = block.to, bwidth = newTo - newFrom + 1;

//    SetDiagStream(&NcbiCout);
//    EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);

    if (m_columnScorers.size() > 0) {

        //  shrink N-terminal bound until a column score exceeds shrinkage threshold,
        //  or block is at the minimum size.
        while (!stopShrinking && bwidth > m_minBlockSize) {
            TRACE_MESSAGE_CL("GetNewBoundaries - shrink:  N term check scores for block " << block.blockNum + 1);
            if (PassAllTests(bma, newFrom)) {
                ++newFrom;
                --bwidth;
                TRACE_MESSAGE_CL("     ....n-terminal shrinking " << newFrom << " " << bwidth);
            } else {
                stopShrinking = true;
            }
        }

        //  shrink C-terminal bound until a column score exceeds shrinkage threshold,
        //  or block is at the minimum size.
        stopShrinking = false;
        while (!stopShrinking && bwidth > m_minBlockSize) {
            TRACE_MESSAGE_CL("GetNewBoundaries - shrink:  C term check scores for block " << block.blockNum + 1);
            if (PassAllTests(bma, newTo)) {
                --newTo;
                --bwidth;
                TRACE_MESSAGE_CL("     ....c-terminal shrinking " << newTo << " " << bwidth);
            } else {
                stopShrinking = true;
            }
        }


        result = (block.from != newFrom || block.to != newTo);
        if (result) {
            TRACE_MESSAGE_CL("\nGetNewBoundaries:  SHRUNK -- (oldfrom, oldto; newfrom, newto) = (" << block.from+1 << ", "  << block.to+1 << "; " << newFrom+1 << ", " << newTo+1 << ")\n");
            //cout << "\nGetNewBoundaries:  SHRUNK -- (oldfrom, oldto; newfrom, newto) = (" << block.from << ", "  << block.to << "; " << newFrom << ", " << newTo << ")\n";
        } else {
            TRACE_MESSAGE_CL("\nGetNewBoundaries:  NO SHRINK -- (oldfrom, oldto) = (" << block.from+1 << ", " << block.to+1 << ")\n");
            //cout << "\nGetNewBoundaries:  NO SHRINK -- (oldfrom, oldto) = (" << block.from << ", " << block.to << ")\n";
        }

        //  Note:  if the block disappears, newFrom > newTo
        block.from = newFrom;
        block.to   = newTo;
        if (newFrom > newTo) TRACE_MESSAGE_CL("**********  deletion....\n");
    }

//    SetDiagPostLevel(oldPostLevel);
//    SetDiagStream(&NcbiCerr);

    return result;
}

/*********************/
  
SimpleBoundaryExtenderAndShrinker::SimpleBoundaryExtenderAndShrinker(bool extendFirst, const ColumnScorer* scorer, double ext, double shrink, const ColumnScorer* secondScorer, double ext2, double shrink2) : BlockBoundaryAlgorithm(scorer, ext, shrink, eSimpleExtendAndShrink), m_extendFirst(extendFirst) {

    m_canBlocksShrink = true;
    if (scorer) {
        m_secondColumnScorers.push_back((secondScorer) ? secondScorer : scorer);
        m_secondExtensionThresholds.push_back((secondScorer) ? ext2 : ext);
        m_secondShrinkageThresholds.push_back((secondScorer) ? shrink2 : shrink);
    }
}

SimpleBoundaryExtenderAndShrinker::~SimpleBoundaryExtenderAndShrinker() {
//  most often both scorers will be the same.  already cleaning up the first scorer
//  in the parent class; if clean up here too will have double-delete problems there if 
//  I'm not careful...
//    for (unsigned int i = 0; i < m_columnScorers.size(); ++i) {
//        delete m_columnScorers[i];
//    }
//    m_columnScorers.clear();
};

void SimpleBoundaryExtenderAndShrinker::AddScorer(const ColumnScorer* scorer, double ext, double shrink) {

    AddFirstScorer(scorer, ext, shrink);
    AddSecondScorer(scorer, ext, shrink);
}

void SimpleBoundaryExtenderAndShrinker::AddFirstScorer(const ColumnScorer* scorer, double ext, double shrink) {

    if (scorer) {
        m_columnScorers.push_back(scorer);
        m_extensionThresholds.push_back(ext);
        m_shrinkageThresholds.push_back((shrink == kMax_Double) ? ext : shrink);
    }
}

void SimpleBoundaryExtenderAndShrinker::AddSecondScorer(const ColumnScorer* scorer2, double ext2, double shrink2) {

    if (scorer2) {
        m_secondColumnScorers.push_back(scorer2);
        m_secondExtensionThresholds.push_back(ext2);
        m_secondShrinkageThresholds.push_back((shrink2 == kMax_Double) ? ext2 : shrink2);
    }
}

bool SimpleBoundaryExtenderAndShrinker::GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const {

    unsigned int i;
    ExtendableBlock origBlock = block;
    BlockBoundaryAlgorithm* firstAlgorithm;
    BlockBoundaryAlgorithm* secondAlgorithm;
//    const ColumnScorer* secondScorer = (m_secondColumnScorer) ? m_secondColumnScorer : m_columnScorer;

    //  Create with NULL scorer to make this a bit less verbose; nulls are not actually added.
    if (m_extendFirst) {
        firstAlgorithm  = new SimpleBoundaryExtender(NULL, 0);
        secondAlgorithm = new SimpleBoundaryShrinker(NULL, 0);
    } else {
        firstAlgorithm  = new SimpleBoundaryShrinker(NULL, 0);
        secondAlgorithm = new SimpleBoundaryExtender(NULL, 0);
    }
        
    if (!firstAlgorithm || !secondAlgorithm) return false;

    firstAlgorithm->SetMinBlockSize(m_minBlockSize);
    for (i = 0; i < m_columnScorers.size(); ++i) {
        firstAlgorithm->AddScorer(m_columnScorers[i], m_extensionThresholds[i], m_shrinkageThresholds[i]);
        if (firstAlgorithm->CanBlocksShrink()) {
            TRACE_MESSAGE_CL("GetNewBoundaries - Extend or Shrink algorithm:  try to shrink block " << block.blockNum+1 << " before trying to extend.");
        } else {
            TRACE_MESSAGE_CL("GetNewBoundaries - Extend or Shrink algorithm:  try to extend block " << block.blockNum+1 << " before trying to shrink.");
        }
    }


    bool result = firstAlgorithm->GetNewBoundaries(block, bma);

    //  If the first step performed a shrink, need to adjust the maximal extensions as
    //  the block can now be extended further due to the shrinking.
    if (firstAlgorithm->CanBlocksShrink() && result) {
        if (block.from > origBlock.from) block.nExt += (block.from - origBlock.from);
        if (block.to < origBlock.to) block.cExt += (origBlock.to - block.to);
    }

    //  Do the second algorithm unless a block deletion recommended.
    if (block.from <= block.to) {
        secondAlgorithm->SetMinBlockSize(m_minBlockSize);
        for (i = 0; i < m_secondColumnScorers.size(); ++i) {
            secondAlgorithm->AddScorer(m_secondColumnScorers[i], m_secondExtensionThresholds[i], 
                                      m_secondShrinkageThresholds[i]);
        }
        result |= secondAlgorithm->GetNewBoundaries(block, bma);
    }
    
    if (!result) 
        TRACE_MESSAGE_CL("GetNewBoundaries - Extend or Shrink algorithm:  did NOT extend or shrink block " << block.blockNum+1 << ".");

    delete firstAlgorithm;
    delete secondAlgorithm;

    //  Until they reflect the possible extensions for the new block boundaries, don't 
    //  surprise the caller w/ meaningless new values for n/cExt.
    block.nExt = origBlock.nExt;
    block.cExt = origBlock.cExt;

    return result;
}
  

/*********************/
  

bool GreedyBoundaryExtender::GetNewBoundaries(ExtendableBlock& block, const BMA& bma) const {

    //  For each scorer (vector index), map the column scores by alignment index.
    typedef map< unsigned int, double > TMap;
    typedef map< unsigned int, double >::value_type TMapVT;
    vector< TMap > columnScores;

    //  When multiple scorers, don't keep getting row scores over and over for each scorer.
    vector< double > rowScoreCache;

    bool result = false;
    bool oneScorer = (m_columnScorers.size() == 1);
    unsigned int i;
    unsigned int bestNewFrom, bestNewTo;
    unsigned int newFrom, newTo;
    unsigned int minIndex = block.from - block.nExt;
    unsigned int maxIndex = block.to   + block.cExt;
    unsigned int baseBlockSize = block.to - block.from + 1, extendedBlockSize;
    vector<unsigned int> newFroms, newTos;
    double baseBlockScore = 0.0, extendedBlockScore, avgExtBlockScore, columnScore, bestScore;

    //  Since this class only extends the bounds, do not allow block shrinkage.
    unsigned int minIndexCore = block.from, maxIndexCore = block.to;  

    if (m_columnScorers.size() == 0 || (block.nExt <= 0 && block.cExt <= 0)) {
        return result;
    }
    if (baseBlockSize <= 0) {
        ERROR_MESSAGE_CL("    Bad block size error for block " << block.blockNum + 1 << ":  min = " << minIndexCore << " and  max = " << maxIndexCore);
        return result;
    }

    //  Make sure there's a map for each scorer.
    columnScores.resize(m_columnScorers.size());

    //  collect all individual column scores needed
    i = minIndex;  
    while (i <= maxIndex) {
        rowScoreCache.clear();
        for (unsigned int j = 0; j < m_columnScorers.size(); ++j) {
            columnScore = m_columnScorers[j]->ColumnScore(bma, i, (oneScorer) ? NULL : &rowScoreCache);
            columnScores[j].insert(TMapVT(i, columnScore));
            if (i >= minIndexCore && i <= maxIndexCore) {
                baseBlockScore += columnScore;
                TRACE_MESSAGE_CL("       scorer " << j+1 << ":  existing column " << setw(4) << i << "; score " << setprecision(4) << columnScore);
            } else {
                TRACE_MESSAGE_CL("       scorer " << j+1 << ":  column " << setw(4) << i << "; score " << setprecision(4) << columnScore);
            }

        }
        ++i;
    }

//    use column scorer 'IsBetterThan' method to determine the best position among all scores
//        -- settle ties by taking the larger extension

    bestNewFrom = minIndexCore;
    bestNewTo   = maxIndexCore;
    TRACE_MESSAGE_CL("   GreedyExtend:  look at block at (" << bestNewFrom << ", " << bestNewTo << ")");
    for (unsigned int j = 0; j < m_columnScorers.size(); ++j) {
        TMap& scores = columnScores[j];

        //  Find best N-terminal score.
        newFrom   = minIndexCore;     //  original boundary
        bestScore = baseBlockScore / baseBlockSize;
        extendedBlockScore = baseBlockScore;
        extendedBlockSize  = baseBlockSize;
        i         = minIndexCore - 1;
        TRACE_MESSAGE_CL("       scorer " << j+1 << ":  N-terminal base score = " << bestScore);
        while (i >= minIndex) {
            ++extendedBlockSize;
            extendedBlockScore += scores[i];
            avgExtBlockScore = extendedBlockScore/extendedBlockSize;
            TRACE_MESSAGE_CL("           index " << i << ":  extended score = " << avgExtBlockScore);
            if (m_columnScorers[j]->IsBetterThan(avgExtBlockScore, bestScore) >= 0 && MeetsExtensionThreshold(avgExtBlockScore, j)) {
                bestScore = avgExtBlockScore;
                newFrom = i;
                TRACE_MESSAGE_CL("           ==> new N-terminus chosen at " << i);
            }

            //  If i == 0, --i makes i = kMax_UInt!!
            if (i == 0) break;
            --i;
        }
        newFroms.push_back(newFrom);

        //  Find best C-terminal score.
        newTo     = maxIndexCore;     //  original boundary
        bestScore = baseBlockScore / baseBlockSize;
        extendedBlockScore = baseBlockScore;
        extendedBlockSize  = baseBlockSize;
        i         = maxIndexCore + 1;
        TRACE_MESSAGE_CL("       scorer " << j+1 << ":  C-terminal base score = " << bestScore);
        while (i <= maxIndex) {
            ++extendedBlockSize;
            extendedBlockScore += scores[i];
            avgExtBlockScore = extendedBlockScore/extendedBlockSize;
            TRACE_MESSAGE_CL("           index " << i << ":  extended score = " << avgExtBlockScore);
            if (m_columnScorers[j]->IsBetterThan(avgExtBlockScore, bestScore) >= 0 && MeetsExtensionThreshold(avgExtBlockScore, j)) {
                bestScore = avgExtBlockScore;
                newTo = i;
                TRACE_MESSAGE_CL("           ==> new C-terminus chosen at " << i);
            }
            ++i;
        }
        newTos.push_back(newTo);

        //  With multiple scorers, choose the largest extension.
        if (newFrom < bestNewFrom) {
            bestNewFrom = newFrom;
        }
        if (newTo > bestNewTo) {
            bestNewTo = newTo;
        }
    }

    result = (block.from != bestNewFrom || block.to != bestNewTo);
    if (result) {
        TRACE_MESSAGE_CL("\nGetNewBoundaries:  EXTENSION -- (oldfrom, oldto; newfrom, newto) = (" << block.from+1 << ", "  << block.to+1 << "; " << bestNewFrom+1 << ", " << bestNewTo+1 << ")\n");
        block.from = bestNewFrom;
        block.to   = bestNewTo;
    } else {
        TRACE_MESSAGE_CL("\nGetNewBoundaries:  NO EXTENSION -- (oldfrom, oldto) = (" << block.from+1 << ", " << block.to+1 << ")\n");
    }

    return result;
}

bool GreedyBoundaryExtender::MeetsExtensionThreshold(double score, unsigned int scorerIndex) const {
    bool result = false;
    if (scorerIndex < m_columnScorers.size()) {
        if (score != ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED) {
            result = (m_columnScorers[scorerIndex]->IsBetterThan(score, m_extensionThresholds[scorerIndex]) >= 0);
        }
    }
    return result;
}

END_SCOPE(align_refine)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2006/11/02 21:33:20  lanczyck
* bug fix:  do not decrement unsigned int when it's zero!
*
* Revision 1.1  2005/06/28 13:44:23  lanczyck
* block multiple alignment refiner code from internal/structure/align_refine
*
* Revision 1.5  2005/03/08 21:30:59  lanczyck
* changes to add greedy extension algorithm
*
* Revision 1.4  2005/01/18 18:44:26  lanczyck
* output message changes
*
* Revision 1.3  2004/12/03 23:15:27  lanczyck
* fix bug:  using same algorithm object twice in Extender&Shrinker
*
* Revision 1.2  2004/12/03 02:05:12  lanczyck
* big changes...\nadd compound column scorer; modify boundary algorithms to take multiple scorers; new options in application to support new scoring options
*
* Revision 1.1  2004/11/16 23:06:02  lanczyck
* perform block edits after LOO; add new options for block editing; rename extend/shrink class to reflect can now both extend AND shrink (vs. OR)
*
* ---------------------------------------------------------------------------
*/
