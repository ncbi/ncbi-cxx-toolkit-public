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
 *       extend blocks
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuBlockExtender.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

BlockExtender::BlockExtender():
	m_src(0), m_scoringMatrix(0), m_cExt(0), m_nExt(0), m_numRows(0),
	m_extensionScores(0), m_extendedBlocks(0), m_extensibilities(0)
{
}

void BlockExtender::setAlignments(MultipleAlignment* ma)
{
	m_src = ma;
	m_numRows = m_src->GetNumRows();
	m_extensibilities = new int* [m_numRows];
	for (int i = 0; i < m_numRows; i++)
	{
		m_extensibilities[i] = new int[m_numRows];
	}	
    /*
	m_extendedBlocks = new BlockModel* [m_numRows];
    for (int j = 0; j < m_numRows; j++) {
		m_extendedBlocks[j] = new BlockModel[m_numRows];
        const BlockModel& jthBlockModel = m_src->getBlockModel(j);
		for(int k = 0; k < m_numRows; k++)
			m_extendedBlocks[j][k] = jthBlockModel;
    }
    */
	m_src->GetAllSequences( m_sequences); 
}

BlockExtender::BlockExtender(MultipleAlignment* ma, ScoreMatrix* sm):
	m_src(ma), m_scoringMatrix(sm), m_cExt(0), m_nExt(0), m_numRows(0),
	m_extensionScores(0), m_extendedBlocks(0)
{
	setAlignments(ma);
}

BlockExtender::~BlockExtender()
{
	for (int i = 0; i < m_numRows; i++)
	{
		if (m_extendedBlocks) delete []m_extendedBlocks[i];
		delete []m_extensibilities[i];
	}
	delete [] m_extendedBlocks;
	delete [] m_extensibilities;
}

void BlockExtender::setScoringMatrix(ScoreMatrix* sm)
{
	m_scoringMatrix = sm;
}

//positive = extension negative = shrink
void BlockExtender::setNTermExt(int ext)
{
	m_nExt = ext;
}

void BlockExtender::setCTermExt(int ext)
{
	m_cExt = ext;
}

bool BlockExtender::setMatrixForExtensionScores(double** matrix, int matrixSize)
{
	if (matrixSize < m_numRows)
		return false;
	m_extensionScores = matrix;
	return true;
}

bool BlockExtender::extendOnePair(int row1, int row2)
{
	bool extended = false;
	double score = 0.0;
    int  blockExtension = 0;
  	BlockModel bm1 = m_src->getBlockModel(row1);
	BlockModel bm2 = m_src->getBlockModel(row2);

	int nBlocks = bm1.getBlocks().size(); 
    assert( nBlocks == bm2.getBlocks().size());

    m_extensibilities[row1][row2] = 0;
    for (int i = 0; i < nBlocks; i++)
	{
        blockExtension = 0;
		score += optimizeBlockScore(row1, row2, i, bm1, bm2, blockExtension);
        if (blockExtension != 0) {
            m_extensibilities[row1][row2] += blockExtension;
            extended = true;
        }
	}

	m_extensibilities[row2][row1] = m_extensibilities[row1][row2];
	if (m_extensionScores)
	{
		m_extensionScores[row1][row2] = score;
		m_extensionScores[row2][row1] = m_extensionScores[row1][row2];
	}
	return extended;
}

//workhorse method
void BlockExtender::extendAllPairs()
{
	for (int j=0; j<m_numRows; j++) 
	{
		if (m_extensionScores)
	        m_extensionScores[j][j] = 0.0;
		m_extensibilities[j][j] = 0;
        // for each other row in the alignment
        for (int k=j+1; k<m_numRows; k++) 
		{
			extendOnePair(j,k);
        }
    }
}

/*
BlockModel& BlockExtender::getExtendedBlocks(int row1, int row2)const
{
	return m_extendedBlocks[row1][row2];
}
*/

int BlockExtender::getTotalLengthExtended(int row1, int row2) const 
{
    int result = 0;
    if (m_extensibilities && row1 < m_numRows && row2 < m_numRows) {
        result = m_extensibilities[row1][row2];
    }
    return result;
}

bool BlockExtender::isThisPairExtended(int row1, int row2)const
{
	return (m_extensibilities[row1][row2] != 0);
}

double BlockExtender::optimizeBlockScore(int row1, int row2, int block, int& extension)
{
    
	BlockModel bm1 = m_src->getBlockModel(row1);
	BlockModel bm2 = m_src->getBlockModel(row2);
//	BlockModel& bm1 = m_extendedBlocks[row1][row2];
//	BlockModel& bm2 = m_extendedBlocks[row2][row1];
    return optimizeBlockScore(row1, row2, block, bm1, bm2, extension);
}

double BlockExtender::optimizeBlockScore(int row1, int row2, int block, BlockModel& bm1, BlockModel& bm2, int& extension)
{
	int nExt =0;
	int cExt = 0;
	double blockScore = 0.0;
	ScoringTerm term1, term2;
	term1.second = row1;
	term2.second = row2;
    
	//get the original score
	if (getScoringTerm(bm1, block, nExt, cExt, term1) && 
		getScoringTerm(bm2, block, nExt, cExt, term2))
		blockScore = scoreBlockPair(term1, term2);
	//n terminal extension
	nExt--;
	while (getScoringTerm(bm1, block, nExt, cExt, term1) && getScoringTerm(bm2, block, nExt, cExt, term2))
	{
		double tmpScore = scoreBlockPair(term1, term2);
		if (tmpScore > blockScore)
		{
			blockScore = tmpScore;
			nExt--;
		}
		else
		{
			break;
		}
	}
	//roll back the last extension because it leads to a descrease in score 
	//or it is invalid
	nExt++;
	//c terminal extension
	cExt++;
	while (getScoringTerm(bm1, block, nExt, cExt, term1) && getScoringTerm(bm2, block, nExt, cExt, term2))
	{
		double tmpScore = scoreBlockPair(term1, term2);
		if (tmpScore > blockScore)
		{
			blockScore = tmpScore;
			cExt++;
		}
		else
			break;
	}
	//roll back the last extension because it leads to a descrease in score 
	//or it is invalid
	cExt--;

    extension = -nExt + cExt;


	if (nExt || cExt)  //extend this block
	{
		bm1.getBlocks()[block].extendSelf(nExt, cExt);
		bm2.getBlocks()[block].extendSelf(nExt, cExt);
	}

	return blockScore;
}

//the pair of block and sequence index
//typedef pair<Block, int> ScoringTerm;
double BlockExtender::scoreBlockPair(const ScoringTerm& term1, const ScoringTerm& term2)
{
	double score=0;
	string& seq1 = m_sequences[term1.second];
	string& seq2 = m_sequences[term2.second];
    for (int i = 0; i < term1.first.getLen(); i++)
    {
        score += m_scoringMatrix->GetScore(seq1[term1.first.getStart() + i], seq2[term2.first.getStart() + i]);
    }
	return score;
}

bool BlockExtender::getScoringTerm(BlockModel& bm, int blockNum, int nExt, int cExt, ScoringTerm& st)
{
	const vector<Block>& blocks = bm.getBlocks();
	if (blockNum >= blocks.size() || blockNum < 0 || blocks.size() == 0)
		return false;
	const Block& src = blocks[blockNum];
	Block block = src.extend(nExt, cExt);
	if (block.getStart() < 0 || block.getLen() <= 0)
		return false;
	int extFootprintLo = bm.getFirstAlignedPosition() - m_nExt;
	if (extFootprintLo < 0)
		extFootprintLo = 0;
	int minStart = (blockNum == 0) ? extFootprintLo : blocks[blockNum-1].getEnd() + 1;
	int extFootprintHi = bm.getLastAlignedPosition() + m_cExt;
	if (extFootprintHi > m_sequences[st.second].size() - 1)
		extFootprintHi = m_sequences[st.second].size() -1;
	int maxEnd = (blockNum == blocks.size()-1) ?  extFootprintHi : blocks[blockNum+1].getStart() - 1;
	if (block.getStart() < minStart || block.getEnd() > maxEnd)
		return false;
	st.first = block;
	return true;
}


int BlockExtender::findCommonExtension(const vector<int>& rows)
{
    int nRows = rows.size();
	if (nRows == 0)
		return 0;

	int primaryRow = rows[0];
	BlockIntersector bi(m_sequences[primaryRow].size());

    // want to compute these on the fly...
    int row, blockExtension;
    double score = 0.0;
    BlockModel primaryBlockModel = m_src->getBlockModel(rows[0]), secondaryBlockModel;
	int nBlocks = primaryBlockModel.getBlocks().size(); 

    for (int i = 1; i < nRows; i++) {
        row = rows[i];
        secondaryBlockModel = m_src->getBlockModel(row);
        for (int j = 0; j < nBlocks; j++) {
		    score += optimizeBlockScore(primaryRow, row, j, primaryBlockModel, secondaryBlockModel, blockExtension);
        }
        bi.addOneAlignment(primaryBlockModel);

        //  reset to the original block model for the primary row
        primaryBlockModel = m_src->getBlockModel(rows[0]);  
    }

	BlockModel* bm = bi.getIntersectedAlignment();
	int dLen = bm->getTotalBlockLength() - primaryBlockModel.getTotalBlockLength();   
    delete bm;

	return dLen;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

