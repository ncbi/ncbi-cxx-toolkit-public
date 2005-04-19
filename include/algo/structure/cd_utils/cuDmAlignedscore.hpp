#ifndef CU_DM_ALIGNEDSCORE__HPP
#define CU_DM_ALIGNEDSCORE__HPP

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


#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuAlignedDM.hpp>

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

class DM_AlignedScore : public AlignedDM {

    static const EDistMethod DIST_METHOD;

public:

    DM_AlignedScore(EScoreMatrixType type, int ext);
    DM_AlignedScore(EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX, int nTermExt=0, int cTermExt=0);
    //DM_AlignedScore(const CCd* cd, EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX, int ext=NO_EXTENSION);
    //DM_AlignedScore(const CCd* cd, EScoreMatrixType type, int nTermExt, int cTermExt);

    virtual bool ComputeMatrix(pProgressFunction pFunc);
    virtual ~DM_AlignedScore();

private:
    
    //     Distance is sum over scores in aligned region
	void    ConvertScoresToDistances();
    double  GetScore(CharPtr residuesRow1, CharPtr residuesRow2, int** ppScores);
    void    CalcPairwiseScores(pProgressFunction pFunc);
    void    initDMAlignedScore(EScoreMatrixType type, int nTermExt, int cTermExt);

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


#endif /*  CU_DM_ALIGNEDSCORE__HPP */
