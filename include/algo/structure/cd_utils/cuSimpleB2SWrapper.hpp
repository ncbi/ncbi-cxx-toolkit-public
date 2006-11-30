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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Simplified API for a single blast-two-sequences call.
 *       Does not involve CDs, and NOT optimized (or intended) to be called
 *       in batch mode.  If you need to make many calls, use CdBlaster!
 *
 * ===========================================================================
 */

#ifndef CU_SIMPLEB2SWRAPPER_HPP
#define CU_SIMPLEB2SWRAPPER_HPP

#include <vector>
#include <algo/blast/api/psibl2seq.hpp>
#include <algo/structure/cd_utils/cuMatrix.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CSimpleB2SWrapper
{
public:
	static const unsigned int HITLIST_SIZE_DEFAULT;
	static const unsigned int MAX_HITLIST_SIZE;
	static const int    CDD_DATABASE_SIZE              ;
	static const double E_VAL_WHEN_NO_SEQ_ALIGN        ; // eval when Blast doesn't return a seq-align
	static const double SCORE_WHEN_NO_SEQ_ALIGN        ;  
	static const double DO_NOT_USE_PERC_ID_THRESHOLD;
	static const string SCORING_MATRIX_DEFAULT;

    CSimpleB2SWrapper(double percIdThold = DO_NOT_USE_PERC_ID_THRESHOLD, string matrixName = SCORING_MATRIX_DEFAULT);

    //  Uses full-length sequences.
    CSimpleB2SWrapper(CRef<CBioseq>& seq1, CRef<CBioseq>& seq2, double percIdThold = DO_NOT_USE_PERC_ID_THRESHOLD, string matrixName = SCORING_MATRIX_DEFAULT);

    //  If from = to = 0, or from > to, use the full length.  
    //  From/to are not validated vs. sequence data in 'seq'.
    void SetSeq1(CRef<CBioseq>& seq, unsigned int from = 0, unsigned int to = 0) { SetSeq(seq, true, from, to);}
    void SetSeq2(CRef<CBioseq>& seq, unsigned int from = 0, unsigned int to = 0) { SetSeq(seq, false, from, to);}

    //  Must be between 0 and 100; otherwise no change is made.  Returns the resulting value of m_percIdThold.
    //  NOTE:  this sets the %identity filter in the b2s algorithm, which seems to calculate %identity using 
    //         the smaller of the two sequences.
    double SetPercIdThreshold(double percIdThold);

    //  Must be between 1 and MAX_HITLIST_SIZE, otherwise change is not made.  
    //  Returns the resulting value of m_hitlistSize.
    unsigned int SetHitlistSize(unsigned int hitlistSize);

    bool DoBlast2Seqs();

    //  If there are no hits, the returned CRef will be invalid.  Test the CRef before using.
    CRef<CSeq_align> getBestB2SAlignment(double* score = NULL, double* eval = NULL, double* percIdent = NULL) const;

    const vector<CRef<CSeq_align> >& getAllHits() const {return m_alignments;}
    unsigned int getNumHits() const {return m_alignments.size();}

    //  All hitIndex values are zero-based!
    bool getPairwiseBlastAlignment(unsigned int hitIndex, CRef< CSeq_align >& seqAlign) const;
    double getPairwiseScore(unsigned int hitIndex) const;
    double getPairwiseEValue(unsigned int hitIndex) const;
    double getPairwisePercIdent(unsigned int hitIndex) const;  // %id vs. m_seq1!!!

private:
//	long m_dbSize;
//	int m_dbSeqNum;

    struct SB2SSeq {
        bool useFull;
        unsigned int from;
        unsigned int to;
        CRef<CBioseq> bs; 
    };

    SB2SSeq m_seq1;
    SB2SSeq m_seq2;

	string m_scoringMatrix;
    unsigned int m_hitlistSize;
    double m_percIdThold;   //  only used if explicitly set by user.
	vector< CRef<CSeq_align> > m_alignments;
	
	vector< double > m_scores;
	vector< double > m_evals;
	vector< double > m_percIdents;

	void SetSeq(CRef<CBioseq>& seq, bool isSeq1, unsigned int from, unsigned int to);

	void processBlastHits(CRef<ncbi::blast::CSearchResults>);
	//input seqAlign may actually contain CSeq_align_set
	CRef< CSeq_align > extractOneSeqAlign(CRef< CSeq_align > seqAlign);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_SIMPLEB2SWRAPPER_HPP

/* 
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/11/30 16:25:46  lanczyck
 * simple wrapper to blast 2 sequences
 *
 * ===========================================================================
 */
