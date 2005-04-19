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
*/
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuDmAlignedOptimalScore.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//  DM_AlignedScore class

//  The pairwise score between two rows in an alignment is the sum over the 
//  scores of each pair of aligned residues:
//
//  d[i][j] = max_Score_for_row - sum(k)( scoring_matrix(i_k, j_k))
//
//  where x_k is the kth aligned residue of row x.  The offset is to make
//  high scores map to short distances.  Although note that each row will
//  in general have a different score for an exact match, making d=0 ambiguous.

const EDistMethod DMAlignedOptimalScore::DIST_METHOD = eScoreAlignedOptimal;

DMAlignedOptimalScore::~DMAlignedOptimalScore() 
{
}

DMAlignedOptimalScore::DMAlignedOptimalScore(EScoreMatrixType type) : 
	m_blockExtender(0), AlignedDM() 
{
    initDMAlignedScore(type, 0,0);
}
	
void DMAlignedOptimalScore::initDMAlignedScore(EScoreMatrixType type, int nTermExt, int cTermExt) 
{
    m_scoreMatrix = new ScoreMatrix(type);
    m_useAligned = true;
    m_nTermExt = nTermExt;
    m_cTermExt = cTermExt;
    m_dMethod    = DIST_METHOD;
    if (m_nTermExt != 0 || m_cTermExt != 0) {
        m_useAligned = false;
    }
}

void DMAlignedOptimalScore::setBlockExtender(BlockExtender* be)
{
	m_blockExtender = be;
}

bool DMAlignedOptimalScore::ComputeMatrix(pProgressFunction pFunc) 
{
    if (!m_maligns || !m_blockExtender) 
        return false;
    m_ConvertedSequences.clear();
	m_blockExtender->setAlignments(m_maligns);
	m_blockExtender->setNTermExt(m_nTermExt);
	m_blockExtender->setCTermExt(m_cTermExt);
	m_blockExtender->setScoringMatrix(m_scoreMatrix);
	int nrows = m_maligns->GetNumRows();
	m_blockExtender->setMatrixForExtensionScores(m_Array, nrows);

	//m_maligns->GetAllSequences( m_ConvertedSequences); 
    int count = 0;
    int total = (int)((double)nrows * (((double)nrows-1)/2));

    // pre-fetch map
	/*
    int** ppScores;
    ppScores = new int*[256];
    for (int j=0; j<256; j++) {
        ppScores[j] = new int[256];
    }
    for (int j=0; j<256; j++) {
        for (int k=0; k<256; k++) {
            ppScores[j][k] = m_scoreMatrix->GetScore(char(j), char(k));
        }
    }*/


	// for each row in the alignment
    for (int j=0; j<nrows; j++) 
	{
        m_Array[j][j] = 0.0;
        // for each other row in the alignment
        for (int k=j+1; k<nrows; k++) 
		{
			m_blockExtender->extendOnePair(j,k);
            //m_Array[j][k] = scoreOneRowPair(j, k, ppScores);
            m_Array[k][j] = m_Array[j][k];
            // tell callback function about progress
        }
        count += nrows - (j+1);
        pFunc(count, total);
    }
    assert(count == total);
	convertScoreToDistance();

    // free space allocated for scores
	/*
    for (int j=0; j<256; j++) {
        delete []ppScores[j];
    }
    delete []ppScores;
	*/
	return true;
}

//d= max(log(s))- log(s)
void DMAlignedOptimalScore::convertScoreToDistance()
{
	/*
	ReplaceZeroWithTinyValue();
	 for (int i=1; i<GetNumRows(); ++i) 
	{
        for (int j=0; j<i; ++j) 
		{
            m_Array[i][j] = log(m_Array[i][j]);
            m_Array[j][i] = m_Array[i][j];
        }
    }*/	
	double max = GetMaxEntry();
    if (FRACTIONAL_EXTRA_OFFSET > 0) {
        max += (FRACTIONAL_EXTRA_OFFSET*max > 1) ? FRACTIONAL_EXTRA_OFFSET*max : 1;
    }
    for (int i=1; i<GetNumRows(); ++i) 
	{
        for (int j=0; j<i; ++j) 
		{
            m_Array[i][j] = (max - m_Array[i][j]);
            m_Array[j][i] = m_Array[i][j];
        }
    }	
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
*/
