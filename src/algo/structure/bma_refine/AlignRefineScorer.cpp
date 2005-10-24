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
*           Base class for objects that compute the score of an alignment.
*           Default is to use a score vs. PSSM computed from the alignment;
*           subclasses will override ComputeScore.
*      
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
//#include "../local_struct_util/struct_util.hpp"
//#include "../local_struct_util/su_block_multiple_alignment.hpp"
#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>
#include <algo/structure/struct_util/su_pssm.hpp>
#include <algo/structure/bma_refine/AlignRefineScorer.hpp>


USING_NCBI_SCOPE;
//USING_SCOPE(objects);
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

const double RowScorer::SCORE_INVALID_OR_NOT_COMPUTED = kMin_Int;

RowScorer::RowScorer() {
    m_scoringMethod = ePSSMScoring;
    m_scoreComputed = false;
    m_score = SCORE_INVALID_OR_NOT_COMPUTED;
}

//  provide default implementation:  sum of scores of seqs vs PSSM
double RowScorer::ComputeScore(struct_util::AlignmentUtility& au, unsigned int row) {
    int nRows = 0;
    double score = SCORE_INVALID_OR_NOT_COMPUTED;

    //    cout << "in RowScorer::ComputeScore" << endl;
    m_scoreComputed = false;
    if (au.Okay()) {
        m_scoreComputed = true;
        //  In default case, sum over rows ...
        if (row == kMax_UInt) {
            score = 0;
            nRows = au.GetBlockMultipleAlignment()->NRows();
            for (int i = 0; i < nRows; ++i) {
                score += (double) au.ScoreRowByPSSM(i);
            }

        //  ... otherwise, return the score of the specified row.  
        //      (ScoreRowByPSSM return kMin_Int if row is too big)
        } else {
            score = (double) au.ScoreRowByPSSM(row);
        }
    }
    //    cout << "return from compute score" << endl;
    m_score = score;
    return m_score;

}

double RowScorer::ComputeBlockScores(struct_util::AlignmentUtility& au, vector<double>& blockScores, unsigned int row) {

    char residue;
    unsigned int i, j, k, pssmIndex;
    unsigned int rowInit, rowFinal, nRows, nBlocks;
    double totalScore = SCORE_INVALID_OR_NOT_COMPUTED;
    double blockScore = SCORE_INVALID_OR_NOT_COMPUTED;
    int tmp;
    const BlockMultipleAlignment* bma;
    BlockMultipleAlignment::UngappedAlignedBlockList alignedBlocks;

    //    cout << "in RowScorer::computeBlockScores; ";
    blockScores.clear();
    m_scoreComputed = false;
	if (au.Okay() && au.GetBlockMultipleAlignment()->GetPSSM()) {
        totalScore = 0;
        m_scoreComputed = true;
        bma = au.GetBlockMultipleAlignment();
        bma->GetUngappedAlignedBlocks(&alignedBlocks);
        nBlocks = alignedBlocks.size();
        nRows = bma->NRows();
        blockScores.resize(nBlocks);

        //  In default case, sum over rows ...
        if (row == kMax_UInt) {
            rowInit = 0;
            rowFinal = nRows - 1;
        } else {
            rowInit = row;
            rowFinal = row;  
        }

        for (i = rowInit; i <= rowFinal; ++i) {
            //            cout << "get block scores for row " << i << "  n blocks = " << nBlocks << endl;
            for (j = 0; j < nBlocks; ++j) {
                blockScore = 0;
                if (alignedBlocks[j]) {
                    _ASSERT(alignedBlocks[j]->IsAligned());
                    pssmIndex = alignedBlocks[j]->GetRangeOfRow(0)->from;
                    //                    cout << "  block " << j << " width = " << alignedBlocks[j]->m_width << "  block start = " << (int) alignedBlocks[j]->GetIndexAt(0, i) << " align index = " << pssmIndex << endl;
                    for (k = 0; k < alignedBlocks[j]->m_width; ++k, ++pssmIndex) {
                        residue = alignedBlocks[j]->GetCharacterAt(k, i);
						tmp = GetPSSMScoreOfCharWithAverageOfBZ(bma->GetPSSM(), pssmIndex, residue);
                        //                        cout << "    " << residue << " " << pssmIndex << " " << tmp << endl;
                        blockScore += (double) tmp; 
                    }
                }
                blockScores[j] += blockScore;
            }
        }

        for (j = 0; j < nBlocks; ++j) totalScore += blockScores[j];

    }
    //    cout << "return from RowScorer::computeBlockScores" << endl;
    m_score = totalScore;
    return m_score;

}
  
END_SCOPE(align_refine)

    /*
     * ---------------------------------------------------------------------------
     * $Log$
     * Revision 1.2  2005/10/24 23:24:52  thiessen
     * struct_util now uses C++ PSSM generation; remove C-toolkit dependency
     *
     * Revision 1.1  2005/06/28 13:44:23  lanczyck
     * block multiple alignment refiner code from internal/structure/align_refine
     *
     * Revision 1.9  2005/05/26 18:51:50  lanczyck
     * make invalid score constant consistent w/ RefinerDefs.hpp
     *
     * Revision 1.8  2004/11/05 22:15:58  lanczyck
     * change class name to RowScorer
     *
     * Revision 1.7  2004/11/02 23:52:08  lanczyck
     * remove CCdCore in favor of CCdd & AlignmentUtility anticipating deployment w/i cn3d
     *
     * Revision 1.6  2004/11/01 16:30:23  lanczyck
     * use GetPSSMScoreOf.... from struct_util.hpp
     *
     * Revision 1.5  2004/10/22 16:56:42  lanczyck
     * add block score analysis; modify output formatting and default ERR_POST flags
     *
     * Revision 1.4  2004/10/12 20:26:18  lanczyck
     * include and makefile mods to use a local struct_util library
     *
     * Revision 1.3  2004/09/03 22:19:52  lanczyck
     * add 'always accept' functionality to simulated annealer; tested w/ new block editor but not activated
     *
     * Revision 1.2  2004/07/29 16:16:25  lanczyck
     * only compute nRows if necessary
     *
     * Revision 1.1  2004/06/23 22:52:47  lanczyck
     * initial checkin:  base class for different scoring methods
     *
     *
     */
