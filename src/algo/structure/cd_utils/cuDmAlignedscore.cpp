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
* File Description:  dm_alignedscore.hpp
*
*      Concrete distance matrix class.
*      Distance is computed based on a scoring matrix, where the 
*      score is based on an existing alignment in a CD.  A pair of parameters
*      to extend an alignment at the N-terminal and C-terminal end can be specified.
*      Different substitution matrices can be defined (see 
*      cdt_scoring_matrices.hpp for the supported scoring matrices).
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuDmAlignedscore.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
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

const EDistMethod DM_AlignedScore::DIST_METHOD = eScoreAligned;

DM_AlignedScore::~DM_AlignedScore() {
}


DM_AlignedScore::DM_AlignedScore(EScoreMatrixType type, int ext) : AlignedDM() {
    initDMAlignedScore(type, ext, ext);
}

DM_AlignedScore::DM_AlignedScore(EScoreMatrixType type, int nTermExt, int cTermExt) : AlignedDM() {
    initDMAlignedScore(type, nTermExt, cTermExt);
}

/*
DM_AlignedScore::DM_AlignedScore(const CCd* cd, EScoreMatrixType type, int ext) :
    DistanceMatrix(cd) {
    initDMAlignedScore(type, ext, ext);
}
	
DM_AlignedScore::DM_AlignedScore(const CCd* cd, EScoreMatrixType type, int nTermExt, int cTermExt) :
    DistanceMatrix(cd) {
    initDMAlignedScore(type, nTermExt, cTermExt);
}*/
	
void DM_AlignedScore::initDMAlignedScore(EScoreMatrixType type, int nTermExt, int cTermExt) {
    m_scoreMatrix = new ScoreMatrix(type);
    m_useAligned = true;
    m_nTermExt = nTermExt;
    m_cTermExt = cTermExt;
    m_dMethod    = DIST_METHOD;
    if (m_nTermExt != 0 || m_cTermExt != 0) {
        m_useAligned = false;
    }
}

bool DM_AlignedScore::ComputeMatrix(pProgressFunction pFunc) {

    bool result;
//    if (m_cdref == null) {
//        return false;
//    }

//    if (m_cdref != null && GetResidueListsWithShifts()) {
    if (m_maligns && GetResidueListsWithShifts()) {
        CalcPairwiseScores(pFunc);
        result = true;
    } else {
        result = false;
    }
	//#if _DEBUG
	//    ofstream ofs;
	//	ofs.open("dm.txt");
	//	DistanceMatrix::writeMat(ofs, *this);
	//	ofs.close();
	//#endif
	
	return result;
}

void DM_AlignedScore::CalcPairwiseScores(pProgressFunction pFunc) {

    int j, k;
    int nrows = m_maligns->GetNumRows();

    // pre-fetch map
    int** ppScores;
    ppScores = new int*[256];
    for (j=0; j<256; j++) {
        ppScores[j] = new int[256];
    }
    for (j=0; j<256; j++) {
        for (k=0; k<256; k++) {
            ppScores[j][k] = m_scoreMatrix->GetScore(char(j), char(k));
        }
    }

    // for each row in the alignment
    int count = 0;
    int total = (int)((double)nrows * (((double)nrows-1)/2));
    for (j=0; j<nrows; j++) {
        m_Array[j][j] = 0.0;
        // for each other row in the alignment
        for (k=j+1; k<nrows; k++) {
            m_Array[j][k] = GetScore(m_ppAlignedResidues[j], m_ppAlignedResidues[k], ppScores);
            m_Array[k][j] = m_Array[j][k];
        }
        count += nrows - (j+1);
        pFunc(count, total);
    }

    ConvertScoresToDistances();

    assert(count == total);

    // free space allocated for scores
    for (j=0; j<256; j++) {
        delete []ppScores[j];
    }
    delete []ppScores;
}

//  No contribution to the score if either residue has value of '0'.  This is to 
//  represent the case where an extension is present and the terminal end is shorter than the 
//  total extension length (e.g.  residue two of Seq A is first aligned; n-terminal ext
//  of five; Seq B alignment starts @ 10 --> three entries for Seq A of '0' where
//  cannot extend the full way).  Don't want to penalize these as gaps.
double DM_AlignedScore::GetScore(CharPtr residuesRow1, CharPtr residuesRow2, int** ppScores) {

    double pairwiseScore = 0;
    int alignLen = m_maligns->GetAlignmentLength() + Max(0, m_nTermExt) + Max(0, m_cTermExt);

    // for each column of the alignment
    for (int i=0; i<alignLen; i++) {
        if (residuesRow1[i] == 0 || residuesRow2[i] == 0) {
            continue;
        }
        pairwiseScore += ppScores[residuesRow1[i]][residuesRow2[i]];
    }
    return pairwiseScore;
}

//  Distance = OFFSET - sum_score_for_aligned_residues_in_pair (from GetScore, above).
//  OFFSET is the maximum pairwise score for the current set of rows so that
//  D(min) = 0; D(max) = maxScore_of_rows - m_minScore.
//  NOTE:  there will always be one or more zero-distance entries corresponding to
//  the largest pairwise score --> this pair most likely will not be identical, counter
//  to the typical interpretation of zero distance.  Conversely, two identically aligned 
//  rows may have non-zero distance if their pairwise score is not the maximum pairwise
//  score for the set of rows under consideration.
void DM_AlignedScore::ConvertScoresToDistances() {
    int nrows = m_maligns->GetNumRows();
	double offset = GetMaxEntry();
    if (FRACTIONAL_EXTRA_OFFSET > 0) {
        offset += (FRACTIONAL_EXTRA_OFFSET*offset > 1) ? FRACTIONAL_EXTRA_OFFSET*offset : 1;
    }

    for (int j=0; j<nrows; j++) {
        for (int k=j+1; k<nrows; k++) {
            m_Array[j][k] = offset - m_Array[j][k];
            m_Array[k][j] = m_Array[j][k];
        }
    }
}

// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
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
