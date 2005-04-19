#ifndef CU_DM_ALIGNED_OPTIMAL_SCORE__HPP
#define CU_DM_ALIGNED_OPTIMAL_SCORE__HPP

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
* Author:  Chris Lanczycki
*
* File Description:  cdt_dm_alignedscore.hpp
*
*      Concrete distance matrix class.
*      Distance is computed based on a scoring matrix, where the 
*      score is based on an existing alignment in a CD.  A pair of parameters
*      to extend an alignment at the N-terminal and C-terminal end can be specified.
*      Different substitution matrices can be defined (see 
*      cdt_scoring_matrices.hpp for the supported scoring matrices).
*
*/


#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuAlignedDM.hpp>
#include <algo/structure/cd_utils/cuBlockExtender.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//  The pairwise score between two rows in an alignment is the sum over the 
//  scores of each pair of aligned residues:
//
//  d[i][j] = max_Score_for_CD - sum(k)( scoring_matrix(i_k, j_k))
//
//  where x_k is the kth aligned residue of row x.  The offset is to make
//  high scores map to short distances.  Although note that each row will
//  in general have a different score for an exact match, making d=0 ambiguous.

class DMAlignedOptimalScore : public AlignedDM {

    static const EDistMethod DIST_METHOD;

public:
    DMAlignedOptimalScore(EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX);
    void setBlockExtender(BlockExtender* be);
    virtual bool ComputeMatrix(pProgressFunction pFunc);
    virtual ~DMAlignedOptimalScore();

private:
	//the pair of block and sequence index
	/* moved to algBlockExtender.hpp
	typedef pair<Block, int> ScoringTerm;
    double scoreBlockPair(const ScoringTerm& term1, const ScoringTerm& term, int** ppScores);
	bool getScoringTerm(int row, int blockNum, int nExt, int cExt, ScoringTerm& st);
	double optimizeBlockScore(int row1, int row2, int block, int** ppScores);
	double scoreOneRowPair(int row1, int row2, int** ppScores); */
	void convertScoreToDistance();
    void initDMAlignedScore(EScoreMatrixType type, int nTermExt, int cTermExt);

	BlockExtender* m_blockExtender;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */


#endif  /* CU_DM_ALIGNED_OPTIMAL_SCORE__HPP */
