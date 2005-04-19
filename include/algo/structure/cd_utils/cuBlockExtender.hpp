/* $Id$
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
 * Author:  Charlie
 *
 * File Description:
 *
 *       to extend blocks in a MultipleAlignment
 *
 * ===========================================================================
 */
#ifndef CU_BLOCK_EXTENDER_HPP
#define CU_BLOCK_EXTENDER_HPP

#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class BlockExtender
{
public:
	BlockExtender();
	BlockExtender(MultipleAlignment* ma, ScoreMatrix* sm);
	~BlockExtender();

	void setAlignments(MultipleAlignment* ma);
	void setScoringMatrix(ScoreMatrix* sm);
	void setNTermExt(int ext);
	void setCTermExt(int ext);
	bool setMatrixForExtensionScores(double** matrix, int matrixSize);
	
	//workhorse method
	bool extendOnePair(int primaryRow, int secondaryRow);
	void extendAllPairs();

	//BlockModel& getExtendedBlocks(int primaryRow, int secondaryRow)const;
	int getTotalLengthExtended(int primaryRow, int secondaryRow)const;
	bool isThisPairExtended(int primaryRow, int secondaryRow)const;
	/*
	BlockExtensionMatrix* subset();
	*/
	int findCommonExtension(const vector<int>& rows);

private:

	typedef pair<Block, int> ScoringTerm;
    inline double scoreBlockPair(const ScoringTerm& term1, const ScoringTerm& term);
	inline bool getScoringTerm(BlockModel& bm, int blockNum, int nExt, int cExt, ScoringTerm& st);
	inline double optimizeBlockScore(int row1, int row2, int block, int& extension);
    //  same as above except returns the modified block models.
	double optimizeBlockScore(int row1, int row2, int block, BlockModel& bm1, BlockModel& bm2, int& extension);

	MultipleAlignment* m_src;
	int m_numRows;
	int** m_extensibilities;
	BlockModel** m_extendedBlocks;
	ScoreMatrix* m_scoringMatrix;
	int m_nExt;
	int m_cExt;
	double** m_extensionScores;
	std::vector< std::string >  m_sequences;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:00  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
