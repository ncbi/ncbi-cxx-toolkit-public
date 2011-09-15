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
* File Description:
*
*      Concrete distance matrix class.
*      Distance is computed based on a scoring matrix, where the 
*      score is derived from a pairwise BLAST of sequences in the CD.
*      There is an option to extend an alignment at the N-terminal and C-terminal
*      end of an existing alignment by a specified amount.  One can also specify
*      to do an unrestricted BLAST of the complete sequences.
*      
*
*/

#include <ncbi_pch.hpp>
//#include <objects/seqalign/Score.hpp>

#include <algo/structure/cd_utils/cuDmBlastscore.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuBlast2Seq.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//  DM_BlastScore class

//  The distance between two rows in an alignment is pairwise BLAST
//  score of the specified region of the sequences:
//
//  d[i][j] = offset - pairwise_BlastScore(i, j)
//
//  where 'offset' is a CD-dependent constant that allows transformation of the
//  largest Blast scores to the shortest distances.  Note that each row will
//  in general have a different score for a Blast against itself, making d=0 ambiguous.

const bool   DM_BlastScore::USE_FULL_SEQUENCE_DEFAULT = true;
const double DM_BlastScore::E_VAL_ON_BLAST_FAILURE    = E_VAL_NOT_FOUND;
const double DM_BlastScore::SCORE_ON_BLAST_FAILURE    = SCORE_NOT_FOUND;

DM_BlastScore::~DM_BlastScore() {
}

DM_BlastScore::DM_BlastScore(EScoreMatrixType type, int ext) : DistanceMatrix() {
    initDMBlastScore(type, ext, ext);
}

DM_BlastScore::DM_BlastScore(EScoreMatrixType type, int nTermExt, int cTermExt) 
: DistanceMatrix()
{
    initDMBlastScore(type, nTermExt, cTermExt);
}
/*
DM_BlastScore::DM_BlastScore(const CCd* cd, EScoreMatrixType type, int ext) :
    DistanceMatrix(cd) {
    initDMBlastScore(type, ext, ext);
}
	
DM_BlastScore::DM_BlastScore(const CCd* cd, EScoreMatrixType type, int nTermExt, int cTermExt) :
    DistanceMatrix(cd){
    initDMBlastScore(type, nTermExt, cTermExt);
}
*/	
void DM_BlastScore::initDMBlastScore(EScoreMatrixType type, int nTermExt, int cTermExt) {
    m_scoreMatrix = new ScoreMatrix(type);
    m_useAligned = false;
    m_nTermExt = nTermExt;
    m_cTermExt = cTermExt;
	SetUseFullSequence(USE_FULL_SEQUENCE_DEFAULT);
    if (m_dMethod == eScoreBlastFoot && m_nTermExt == 0 && m_cTermExt == 0) {
        m_useAligned = true;
    }
}
    
void DM_BlastScore::SetUseFullSequence(bool value) {
	m_useFullSequence = value;
	m_dMethod = (m_useFullSequence) ? eScoreBlastFull : eScoreBlastFoot;
}

bool DM_BlastScore::ComputeMatrix(pProgressFunction pFunc) {
	bool result = false;
    if (m_aligns == NULL) {
        return result;
    }
	result = CalcPairwiseScoresOnTheFly(pFunc);
	//#if _DEBUG
	//    ofstream ofs;
	//	ofs.open("dm.txt");
	//	DistanceMatrix::writeMat(ofs, *this);
	//	ofs.close();
	//#endif
	
	return result;
}

bool DM_BlastScore::CalcPairwiseScoresOnTheFly(pProgressFunction pFunc) {
    int i, j, k;
	int nrows = 0;
	double maxScore, minScore;

    vector<double> AllScores;
    nrows = m_aligns->GetNumRows();
	CdBlaster blaster(*m_aligns, GetMatrixName());
	if (m_useFullSequence)
		blaster.useWholeSequence(true);
	else
		blaster.setFootprintExtension(GetNTermExt(), GetCTermExt());
	blaster.blast(pFunc);
	//  The seq_aligns are returned in order (10),(20),(21),(30),(31),(32),...
	i=0;
	m_Array[0][0] = 0.0;
    for (j=1; j<nrows; j++) {
        m_Array[j][j] = 0.0;
        // for each other row in the alignment
        for (k=0; k<j; k++) {
			/*
            m_Array[j][k] = AllScores[i];*/
			m_Array[j][k] = blaster.getPairwiseScore(j,k);
            m_Array[k][j] = m_Array[j][k];
            ++i;
        }
    }

	//  FudgeFactor:  don't want a zero-distance for maxScore
	double FudgeFactor = 1.01;
	GetExtremalEntries(maxScore, minScore, true);
	LinearTransform(maxScore*FudgeFactor, -1.0, true);   
	return true;
}
END_SCOPE(cd_utils)
END_NCBI_SCOPE
