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
 * Author:  Charlie Liu	
 *
 * File Description:
 *
 *       Functions to call C++ Blast2Seq API
 *
 * ===========================================================================
 */

#ifndef CU_BLAST2SEQ_HPP
#define CU_BLAST2SEQ_HPP

#include <vector>
#include <algo/blast/api/psibl2seq.hpp>
#include <algo/structure/cd_utils/cuMatrix.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/cuPssmMaker.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

typedef void (* NotifierFunction) (int Num, int Total);

class NCBI_CDUTILS_EXPORT CdBlaster 
{
public:
	static const int    CDD_DATABASE_SIZE              ;
	static const double BLAST_SCALING_FACTOR_DEFAULT   ;
	static const double E_VAL_WHEN_NO_SEQ_ALIGN        ; // eval when Blast doesn't return a seq-align
	static const double SCORE_WHEN_NO_SEQ_ALIGN        ;  
	static const string SCORING_MATRIX_DEFAULT;
	static const long DEFAULT_NR_SIZE;
	static const int DEFAULT_NR_SEQNUM;

	//the source of the query sequences
	CdBlaster(AlignmentCollection& source, string matrixName = SCORING_MATRIX_DEFAULT);
	CdBlaster(vector< CRef<CBioseq> >& seqs, string matrixName = SCORING_MATRIX_DEFAULT);

	void setQueryRows(const vector<int>* rows) {m_queryRows = rows;};
	void setSubjectRows(const vector<int>* rows){m_subjectRows = rows;};
	void setScoreType(CSeq_align::EScoreType scoreType){m_scoreType = scoreType;};

	void useWholeSequence(bool whole);
	void setFootprintExtension(int nExt, int CExt);

    //  Returns false if there was a problem setting up the Blast query factory object
    //  for any of the sequences; results for other sequences are available even with
    //  a false return value.
	bool blast(NotifierFunction notifier=0);

	//to do psi-blast
	void setPsiBlastTarget(CRef<CPssmWithParameters> pssm);
	CRef<CPssmWithParameters> setPsiBlastTarget(CCdCore* targetCD);

	// returns number of psi-blast done
	int psiBlast();
	
	double getPairwiseScore(int row1, int row2);
	double getPairwiseEValue(int row1, int row2);

	double getPsiBlastScore(int row);
	double getPsiBlastEValue(int row);
	

	/* we don't need the actual alignments for now
		so these two methods only returns the origial seq-align with denseg */
	CRef<CSeq_align> getPsiBlastAlignement(int row);
	CRef<CSeq_align> getPairwiseBlastAlignement(int row1, int row2);

private:
	long m_dbSize;
	int m_dbSeqNum;

	AlignmentCollection* m_ac; //normally, the source of alignments and seuqneces
	const vector<int>* m_queryRows;
	const vector<int>* m_subjectRows;
	vector< CRef<CBioseq> >* m_seqs; // the source of sequences for the coreage test.
	string m_scoringMatrix;
	bool m_useWhole; // false: use aligned footprint
	int m_nExt;
	int m_cExt;
	vector< CRef<CSeq_align> > m_alignments;
	
	// for blast tree, instead of keeping alignments, just keep scores and e-vals
	CSeq_align::EScoreType m_scoreType;
	vector< double > m_scores;
	vector< double > m_evals;

	vector<int> m_offsets;
	vector<int> m_batchSizes;
	CCdCore* m_psiTargetCd;
	CRef<CPssmWithParameters> m_psiTargetPssm;
	vector< CRef< CBioseq > > m_truncatedBioseqs;

	CRef< CBioseq > truncateBioseq(int row);
	void processBlastHits(int queryRow, CSearchResultSet& hits);
	//void processBlastHits(TSeqAlignVector& hits);
	//void processBlastHits(BlastHSPResults* hits, int numSubjects);
	bool IsFootprintValid(int from, int to, int len);
	void ApplyEndShiftToRange(int& from, int nTermShift, int& to, int cTermShift, int len);
	int getCompositeIndex(int query, int subject);
	//not needed for now
	//CRef< CSeq_align > remapSeqAlign(int query, int subject, CRef< CSeq_align > seqAlign);

    void RemoveAllDataLoaders();
    bool FillOutSeqLoc(const CRef< CBioseq >& bs, CSeq_loc& seqLoc);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif //
