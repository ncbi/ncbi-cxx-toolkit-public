#ifndef CU_DM_BLASTSCORE__HPP
#define CU_DM_BLASTSCORE__HPP

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
* File Description:  cdt_dm_blastscore.hpp
*
*      Concrete distance matrix class.
*      Distance is computed based on a scoring matrix, where the 
*      score is derived from a pairwise BLAST of sequences in the CD.
*      There is an option to extend an alignment at the N-terminal and C-terminal
*      end of an existing alignment by a specified amount.  One can also specify
*      to do an unrestricted BLAST of the complete sequences.
*      (See cdt_scoring_matrices.hpp for the supported scoring matrices).
*
*/

#include <algo/structure/cd_utils/cuDistmat.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
//BEGIN_NCBI_SCOPE


//  The distance between two rows in an alignment is pairwise BLAST
//  score of the specified region of the sequences:
//
//  d[i][j] = offset - pairwise_BlastScore(i, j)
//
//  where 'offset' is a CD-dependent constant that allows transformation of the
//  largest Blast scores to the shortest distances.  Note that each row will
//  in general have a different score for a Blast against itself, making d=0 ambiguous.

class DM_BlastScore : public DistanceMatrix {

	static const bool USE_FULL_SEQUENCE_DEFAULT;
	static const double E_VAL_ON_BLAST_FAILURE;
public:
	static const double SCORE_ON_BLAST_FAILURE;
//    static EDistMethod DIST_METHOD;

public:

    DM_BlastScore(EScoreMatrixType type, int ext);
    DM_BlastScore(EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX, int nTermExt=0, int cTermExt=0);
    //DM_BlastScore(const CCd* cd, EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX, int ext=NO_EXTENSION);
    //DM_BlastScore(const CCd* cd, EScoreMatrixType type, int nTermExt, int cTermExt);

	bool useFullSequence() { return m_useFullSequence;}
	void SetUseFullSequence(bool value);

    //double  GetBlastScore(int id1, int id2, string matrix_name, bool unrestricted=false);
    virtual bool ComputeMatrix(pProgressFunction pFunc);
    virtual ~DM_BlastScore();

private:
    
	//  m_useFullSequence tells whether to use all residues in sequences, or
	//  will be using a subset.  Differs from m_useAligned in parent in that
	//  even if don't use full sequence, don't necessarily need to use the 
	//  aligned residues.  Even in the footprint (first aligned to last aligned
	//  residue on a sequence) the non-aligned intervening residues will be 
	//  used in the blast here.  To exclude even those, use eScoreAligned.
	bool m_useFullSequence;
	//CCd::AlignmentUsage m_alignUse;
	vector<CRef <CSeq_entry> > m_sequences;

    void initDMBlastScore(EScoreMatrixType type, int nTermExt, int cTermExt);
//	void ExtractScoreFromSeqAlign(const CSeq_align* seqAlign, double& result, bool eval);
//	int  ExtractScoreFromScoreList(const CSeq_align::TScore& scores, double& result, bool eval);

    //     Distance is shifted Blast score over a specified region
    //void    CalcPairwiseScores(pProgressFunction pFunc);            //  slow way; set up for each blast
    bool    CalcPairwiseScoresOnTheFly(pProgressFunction pFunc);    //  faster way; set up for all blasts w/ row i at once

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif 
