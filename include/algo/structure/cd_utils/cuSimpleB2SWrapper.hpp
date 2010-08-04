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
#include <objmgr/object_manager.hpp>
#include <algo/blast/api/psibl2seq.hpp>
#include <algo/structure/cd_utils/cuMatrix.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CSimpleB2SWrapper
{
    static void RemoveAllDataLoaders() {
        int i = 1;
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        CObjectManager::TRegisteredNames loader_names;
        om->GetRegisteredNames(loader_names);
        ITERATE(CObjectManager::TRegisteredNames, itr, loader_names) {
            cout << "data loader " << i << ":  " << *itr << endl;
            om->RevokeDataLoader(*itr);
            ++i;
        }
    }
public:
	static const unsigned int HITLIST_SIZE_DEFAULT    ;
	static const unsigned int MAX_HITLIST_SIZE        ;
	static const Int8   CDD_DATABASE_SIZE             ;
	static const double E_VAL_DEFAULT                 ; // default e-value threshold
	static const double E_VAL_WHEN_NO_SEQ_ALIGN       ; // eval when Blast doesn't return a seq-align
	static const double SCORE_WHEN_NO_SEQ_ALIGN       ;  
	static const string SCORING_MATRIX_DEFAULT        ;
    static const ECompoAdjustModes COMPOSITION_ADJ_DEF;
	static const double DO_NOT_USE_PERC_ID_THRESHOLD  ; // user must provide this; no default is set
	static const Int8   DO_NOT_USE_EFF_SEARCH_SPACE   ; // user must provide this; no default is set

    //  If the default for 'percIdThold' is used, then the %identity filter will be off in B2S.
    CSimpleB2SWrapper(double percIdThold = DO_NOT_USE_PERC_ID_THRESHOLD, string matrixName = SCORING_MATRIX_DEFAULT);

    //  Searches using the full-length sequences specified.
    //  If the default for 'percIdThold' is used, then the %identity filter will be off in B2S.
    CSimpleB2SWrapper(CRef<CBioseq>& seq1, CRef<CBioseq>& seq2, double percIdThold = DO_NOT_USE_PERC_ID_THRESHOLD, string matrixName = SCORING_MATRIX_DEFAULT);

    //  If from = to = 0, or from > to, use the full length.  
    //  From/to are not validated vs. sequence data in 'seq'.
    void SetSeq1(CRef<CBioseq>& seq, unsigned int from = 0, unsigned int to = 0) { SetSeq(seq, true, from, to);}
    void SetSeq2(CRef<CBioseq>& seq, unsigned int from = 0, unsigned int to = 0) { SetSeq(seq, false, from, to);}

    //  GENERAL NOTE on Set....() functions:
    //  the return value is the value of the corresponding member variable on completion
    //  (which is also the corresponding value in m_options when m_options is valid).
    //  Invalid values used in these functions are ignored and no changes are made
    //  to the settings, and return value will therefore differ from the passed argument.

    //  Must be between 0 and 100; otherwise no change is made.
    //  NOTE:  this sets the %identity filter in the b2s algorithm, which seems to calculate %identity using 
    //         the smaller of the two sequences.
    double SetPercIdThreshold(double percIdThold);

    //  Must be between 1 and MAX_HITLIST_SIZE, otherwise change is not made.  
    unsigned int SetHitlistSize(unsigned int hitlistSize);

    //  Must be > 0, otherwise change is not made.  
    Int8 SetDbLength(Int8 dbLength);

    //  Must be > 0, otherwise change is not made.  
    Int8 SetEffSearchSpace(Int8 effSearchSpace);

    //  Must be > 0, otherwise change is not made.  
    ECompoAdjustModes SetCompoAdjustMode(ECompoAdjustModes caMode);

    //  Sanity checks that eValueThold is non-negative.
    double SetEValueThreshold(double eValueThold);

    //  Set matrix name; sanity checks that matrixName is one of those defined in cuScoringMatrix.hpp
    string SetMatrixName(string matrixName);

    //  Expose the options handle object so any parameter can be manipulated.
    CRef< CBlastAdvancedProteinOptionsHandle >& GetOptionsHandle() { return m_options; }

    //  Do all parameter configurations before calling this method.
    //  E-value threshold is 10.0 unless user has previously called 'SetEValueThreshold'.
    //  Uses Object Manager enabled Blast interface.
    bool DoBlast2Seqs();

    //  Do all parameter configurations before calling this method.
    //  E-value threshold is 10.0 unless user has previously called 'SetEValueThreshold'.
    //  Uses Object Manager free Blast interface.
//    bool DoBlast2Seqs_OMFree();

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
    Int8 m_dbLength;
    double m_eValueThold;
    double m_percIdThold;    //  only used if explicitly set by user.
    Int8 m_effSearchSpace;   //  only used if explicitly set by user.
    ECompoAdjustModes m_caMode;
	vector< CRef<CSeq_align> > m_alignments;
	
	vector< double > m_scores;
	vector< double > m_evals;
	vector< double > m_percIdents;

    // Stores all options needed for the query.
	CRef<CBlastAdvancedProteinOptionsHandle> m_options;

    //  Create the options object and set defaults.
    void InitializeToDefaults();

	void SetSeq(CRef<CBioseq>& seq, bool isSeq1, unsigned int from, unsigned int to);

    //  False if there was a problem (e.g., SB2SSeq couldn't provide a Seq-id).
    bool FillOutSeqLoc(const SB2SSeq& s, CSeq_loc& seqLoc);

	void processBlastHits(ncbi::blast::CSearchResults&);
//    void processBlastHits_OMFree(ncbi::blast::CSearchResults&);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_SIMPLEB2SWRAPPER_HPP
