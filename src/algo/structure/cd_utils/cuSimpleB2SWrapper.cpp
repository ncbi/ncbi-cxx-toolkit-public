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

#include <ncbi_pch.hpp>
#include <vector>
#include <string>
#include <list>

#include <objmgr/scope.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Object_id.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuSimpleB2SWrapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
BEGIN_SCOPE(cd_utils)

const unsigned int CSimpleB2SWrapper::HITLIST_SIZE_DEFAULT     = 100;
const unsigned int CSimpleB2SWrapper::MAX_HITLIST_SIZE         = 10000;
const Int8   CSimpleB2SWrapper::CDD_DATABASE_SIZE              = 1000000;
const double CSimpleB2SWrapper::E_VAL_DEFAULT                  = 10.0;    // default e-value threshold
const double CSimpleB2SWrapper::E_VAL_WHEN_NO_SEQ_ALIGN        = 1000000; // e-value when Blast doesn't return a seq-align
const double CSimpleB2SWrapper::SCORE_WHEN_NO_SEQ_ALIGN        = -1.0;    
const ECompoAdjustModes CSimpleB2SWrapper::COMPOSITION_ADJ_DEF = eNoCompositionBasedStats;
const string CSimpleB2SWrapper::SCORING_MATRIX_DEFAULT         = BLOSUM62NAME;
const double CSimpleB2SWrapper::DO_NOT_USE_PERC_ID_THRESHOLD   = -1.0;    
const Int8   CSimpleB2SWrapper::DO_NOT_USE_EFF_SEARCH_SPACE    = -1; 

CSimpleB2SWrapper::CSimpleB2SWrapper(double percIdThold, string matrixName)
{
    InitializeToDefaults();     //  must be called before any Set...() methods
    SetPercIdThreshold(percIdThold);
    SetMatrixName(matrixName);
}

CSimpleB2SWrapper::CSimpleB2SWrapper(CRef<CBioseq>& seq1, CRef<CBioseq>& seq2, double percIdThold, string matrixName)
{
    InitializeToDefaults();    //  must be called before any Set...() methods
    SetSeq(seq1, true, 0, 0);
    SetSeq(seq2, false, 0, 0);
    SetPercIdThreshold(percIdThold);
    SetMatrixName(matrixName);
}

void CSimpleB2SWrapper::InitializeToDefaults() 
{
    m_hitlistSize = HITLIST_SIZE_DEFAULT;
    m_dbLength = CDD_DATABASE_SIZE;
    m_eValueThold = E_VAL_DEFAULT;
    m_effSearchSpace = DO_NOT_USE_EFF_SEARCH_SPACE;
    m_caMode = COMPOSITION_ADJ_DEF;
    m_scoringMatrix = SCORING_MATRIX_DEFAULT;


	m_options.Reset(new CBlastAdvancedProteinOptionsHandle);

    //  Fill object with defaults we want to set.
    if (m_options.NotEmpty()) {
        m_options->SetEvalueThreshold(E_VAL_DEFAULT);
        m_options->SetHitlistSize(HITLIST_SIZE_DEFAULT);
        m_options->SetMatrixName(SCORING_MATRIX_DEFAULT.c_str());
        m_options->SetSegFiltering(false);
        m_options->SetDbLength(CDD_DATABASE_SIZE);
        m_options->SetCompositionBasedStats(COMPOSITION_ADJ_DEF);

        m_options->SetDbSeqNum(1);
    }

}

void CSimpleB2SWrapper::SetSeq(CRef<CBioseq>& seq, bool isSeq1, unsigned int from, unsigned int to)
{
    bool full = (from == 0 && to == 0);
    unsigned int len = GetSeqLength(*seq);

    //  If invalid range, also use the full sequence.
    if (full || from > to) {
        full = true;
        from = 0;
		to = (seq.NotEmpty()) ? len - 1 : 0;
//		to = (seq.NotEmpty()) ? seq->GetInst().GetLength() - 1 : 0;
    }

    //  Clip end of range so it does not extend beyond end of the sequence
    if (to >= len) {
        to = len - 1;
    }

    SB2SSeq tmp = {full, from, to, seq};
    if (isSeq1) {
        m_seq1 = tmp;
    } else {
        m_seq2 = tmp;
    }
}

bool CSimpleB2SWrapper::FillOutSeqLoc(const SB2SSeq& s, CSeq_loc& seqLoc)
{
    bool result = true;
    CSeq_interval& seqInt = seqLoc.SetInt();
    CSeq_id& seqId = seqInt.SetId();
    seqInt.SetFrom(s.from);
    seqInt.SetTo(s.to);
    
    //  Assign the first identifier from the bioseq
    if (s.bs.NotEmpty() && s.bs->GetId().size() > 0) {
        seqId.Assign(*(s.bs->GetId().front()));
    } else {
        result = false;
    }

    return result;
}


double CSimpleB2SWrapper::SetPercIdThreshold(double percIdThold)
{
    if ((percIdThold == DO_NOT_USE_PERC_ID_THRESHOLD || (percIdThold >= 0 && percIdThold <= 100.0)) && m_options.NotEmpty()) {
        m_percIdThold = percIdThold;
        m_options->SetPercentIdentity(m_percIdThold);
    }
    return m_percIdThold;
}

unsigned int CSimpleB2SWrapper::SetHitlistSize(unsigned int hitlistSize)
{
    if (hitlistSize > 0 && hitlistSize <= MAX_HITLIST_SIZE && m_options.NotEmpty()) {
        m_hitlistSize = hitlistSize;
        m_options->SetHitlistSize(m_hitlistSize);
    }
    return m_hitlistSize;
}

Int8 CSimpleB2SWrapper::SetDbLength(Int8 dbLength)
{
    if (dbLength > 0 && m_options.NotEmpty()) {
        m_dbLength = dbLength;
        m_options->SetDbLength(m_dbLength);
    }
    return m_dbLength;
}

Int8 CSimpleB2SWrapper::SetEffSearchSpace(Int8 effSearchSpace)
{
    if (effSearchSpace > 0 && m_options.NotEmpty()) {
        m_effSearchSpace = effSearchSpace;
        m_options->SetEffectiveSearchSpace(m_effSearchSpace);
    }
    return m_effSearchSpace;
}

ECompoAdjustModes CSimpleB2SWrapper::SetCompoAdjustMode(ECompoAdjustModes caMode)
{
    if (m_options.NotEmpty()) {
        m_caMode = caMode;
        m_options->SetCompositionBasedStats(m_caMode);
    }
    return m_caMode;
}

double CSimpleB2SWrapper::SetEValueThreshold(double eValueThold)
{
    if (eValueThold >= 0 && m_options.NotEmpty()) {
        m_eValueThold = eValueThold;
        m_options->SetEvalueThreshold(m_eValueThold);
    }
    return m_eValueThold;
}

string CSimpleB2SWrapper::SetMatrixName(string matrixName)
{
    bool isNameKnown = false;

    if (matrixName == BLOSUM62NAME || matrixName == BLOSUM45NAME || matrixName == BLOSUM80NAME ||
        matrixName == PAM30NAME || matrixName == PAM70NAME || matrixName == PAM250NAME) {
        isNameKnown = true;
    }

    if (isNameKnown && m_options.NotEmpty()) {
        m_scoringMatrix = matrixName;
        m_options->SetMatrixName(m_scoringMatrix.c_str());
    }
    return m_scoringMatrix;
}

bool CSimpleB2SWrapper::DoBlast2Seqs()
{
    if (m_options.Empty()) return false;

/*	
	CRef<CBlastAdvancedProteinOptionsHandle> options(new CBlastAdvancedProteinOptionsHandle);
	options->SetEvalueThreshold(m_eValueThold);
	options->SetMatrixName(m_scoringMatrix.c_str());
	options->SetSegFiltering(false);
	options->SetDbLength(CDD_DATABASE_SIZE);
    options->SetHitlistSize(m_hitlistSize);
	options->SetDbSeqNum(1);
	options->SetCompositionBasedStats(eNoCompositionBasedStats);

    if (m_percIdThold != DO_NOT_USE_PERC_ID_THRESHOLD) options->SetPercentIdentity(m_percIdThold);
//    cout << "m_percIdThold = " << m_percIdThold << ";  DO_... = " << DO_NOT_USE_PERC_ID_THRESHOLD << endl;
*/

    CSeq_loc querySeqLoc, subjectSeqLoc;
    if (!FillOutSeqLoc(m_seq1, querySeqLoc) || !FillOutSeqLoc(m_seq2, subjectSeqLoc)) {
        return false;
    }

    RemoveAllDataLoaders();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    CBioseq_Handle handle1 = scope.AddBioseq(*m_seq1.bs);
    CBioseq_Handle handle2 = scope.AddBioseq(*m_seq2.bs);


    CRef<CBlastSearchQuery> bsqQuery(new CBlastSearchQuery(querySeqLoc, scope));
    CRef<CBlastSearchQuery> bsqSubject(new CBlastSearchQuery(subjectSeqLoc, scope));
    CBlastQueryVector queryVector, subjectVector;
    queryVector.AddQuery(bsqQuery);
    subjectVector.AddQuery(bsqSubject);


    CRef<IQueryFactory> query(new CObjMgr_QueryFactory(queryVector));
    CRef<IQueryFactory> subject(new CObjMgr_QueryFactory(subjectVector));
	CRef<CBlastProteinOptionsHandle> blastOptions((CBlastProteinOptionsHandle*)m_options.GetPointer());
	

    //  perform the blast 2 sequences and process the results...
    CPsiBl2Seq blaster(query, subject, blastOptions);
    CSearchResultSet& hits = *blaster.Run();
    int total = hits.GetNumResults();
    for (int index=0; index<total; index++)
       processBlastHits(hits[index]);

//  dump the CSearchResults object... 
//    string err;
//    if (total > 0) WriteASNToStream(cout, hits[0], false, &err);

    return (m_alignments.size() > 0);
}

CRef<CSeq_align> CSimpleB2SWrapper::getBestB2SAlignment(double* score, double* eval, double* percIdent) const
{
    if (m_alignments.size() == 0) {
        CRef< CSeq_align > nullRef;
        return nullRef;
    }

    if (score && m_scores.size() > 0) {
        *score = m_scores[0];
    }
    if (eval && m_evals.size() > 0) {
        *eval = m_evals[0];
    }
    if (percIdent && m_percIdents.size() > 0) {
        *percIdent = m_percIdents[0];
    }

    return m_alignments[0];
}


bool CSimpleB2SWrapper::getPairwiseBlastAlignment(unsigned int hitNum, CRef< CSeq_align >& seqAlign) const
{
    bool result = false;
    if (hitNum < m_alignments.size()) {
        if (m_alignments[hitNum].NotEmpty()) {
            seqAlign->Assign(*m_alignments[hitNum]);
            result = true;
        }
    }
    return result;
}

double  CSimpleB2SWrapper::getPairwiseScore(unsigned int hitNum) const
{
	return (m_scores.size() > hitNum) ? m_scores[hitNum] : SCORE_WHEN_NO_SEQ_ALIGN;
}

double  CSimpleB2SWrapper::getPairwiseEValue(unsigned int hitNum) const
{
	return (m_evals.size() > hitNum) ? m_evals[hitNum] : E_VAL_WHEN_NO_SEQ_ALIGN;
}

double  CSimpleB2SWrapper::getPairwisePercIdent(unsigned int hitNum) const
{
	return (m_percIdents.size() > hitNum) ? m_percIdents[hitNum] : DO_NOT_USE_PERC_ID_THRESHOLD;
}

void CSimpleB2SWrapper::processBlastHits(CSearchResults& hits)
{
    unsigned int len1 = (m_seq1.to >= m_seq1.from) ? m_seq1.to - m_seq1.from + 1 : 0;
    double invLen1 = (len1) ? 1.0/(double) len1 : 0;
	const list< CRef< CSeq_align > >& seqAlignList = hits.GetSeqAlign()->Get();

//    unsigned int len2 = (m_seq2.to >= m_seq2.from) ? m_seq2.to - m_seq2.from + 1 : 0;
//    cout << "Processing " << seqAlignList.size() << " blast hits (len1 = " << len1 << ", len2 = " << len2 << ").\n";

    m_scores.clear();
    m_evals.clear();
    m_percIdents.clear();
    m_alignments.clear();

    if (seqAlignList.size() == 0) return;

    int nIdent = 0;
	double score = 0.0, eVal = kMax_Double, percIdent = 0.0;
	CRef< CSeq_align > sa = ExtractFirstSeqAlign(*(seqAlignList.begin()));
	if (!sa.Empty()) 
	{
		sa->GetNamedScore(CSeq_align::eScore_Score, score);
		sa->GetNamedScore(CSeq_align::eScore_EValue, eVal);
        if (sa->GetNamedScore(CSeq_align::eScore_IdentityCount, nIdent)) {
            percIdent = 100.0 * invLen1 * (double) nIdent; 
//            cout << "nIdent = " << nIdent << "; percIdent = " << percIdent << endl;
//        } else {
//            cout << "????  Didn't find identity count\n";
        }


//        if (!sa->GetNamedScore(CSeq_align::eScore_PercentIdentity, percIdent))
//            cout << "????  Didn't find percent identity\n";
//        cout << "saving values:   score = " <<score << "; eval = " << eVal << "; id% = " << percIdent << endl;

        m_scores.push_back(score);
        m_evals.push_back(eVal);
        m_percIdents.push_back(percIdent);
        m_alignments.push_back(sa);
	}

}

/*	
bool CSimpleB2SWrapper::DoBlast2Seqs_OMFree()
{
    if (m_options.Empty()) return false;

//	CRef<CBlastAdvancedProteinOptionsHandle> options(new CBlastAdvancedProteinOptionsHandle);
//	options->SetEvalueThreshold(m_eValueThold);
//	options->SetMatrixName(m_scoringMatrix.c_str());
//	options->SetSegFiltering(false);
//	options->SetDbLength(CDD_DATABASE_SIZE);
//    options->SetHitlistSize(m_hitlistSize);
//	options->SetDbSeqNum(1);
//	options->SetCompositionBasedStats(eNoCompositionBasedStats);

//    if (m_percIdThold != DO_NOT_USE_PERC_ID_THRESHOLD) options->SetPercentIdentity(m_percIdThold);
//    cout << "m_percIdThold = " << m_percIdThold << ";  DO_... = " << DO_NOT_USE_PERC_ID_THRESHOLD << endl;

	CRef<CBlastProteinOptionsHandle> blastOptions((CBlastProteinOptionsHandle*)m_options.GetPointer());
	
    //  construct 'query' from m_seq1
    CRef< CBioseq > queryBioseq = m_seq1.bs;   
    CRef<IQueryFactory> query(new CObjMgrFree_QueryFactory(queryBioseq));


    //  construct 'subject' from m_seq2
	CRef< CBioseq_set > bioseqset(new CBioseq_set);
	list< CRef< CSeq_entry > >& seqEntryList = bioseqset->SetSeq_set();
    CRef< CSeq_entry > seqEntry(new CSeq_entry);
    seqEntry->SetSeq(*m_seq2.bs);
    seqEntryList.push_back(seqEntry);
    CRef<IQueryFactory> subject(new CObjMgrFree_QueryFactory(bioseqset));

    //  perform the blast 2 sequences and process the results...
    CPsiBl2Seq blaster(query,subject,blastOptions);
    CSearchResultSet& hits = *blaster.Run();
    int total = hits.GetNumResults();
    for (int index=0; index<total; index++)
       processBlastHits_OMFree(hits[index]);

//  dump the CSearchResults object... 
//    string err;
//    if (total > 0) WriteASNToStream(cout, hits[0], false, &err);

    return (m_alignments.size() > 0);
}

void CSimpleB2SWrapper::processBlastHits_OMFree(CSearchResults& hits)
{
    unsigned int len1 = (m_seq1.to >= m_seq1.from) ? m_seq1.to - m_seq1.from + 1 : 0;
    unsigned int len2 = (m_seq2.to >= m_seq2.from) ? m_seq2.to - m_seq2.from + 1 : 0;
    double invLen1 = (len1) ? 1.0/(double) len1 : 0;
	const list< CRef< CSeq_align > >& seqAlignList = hits.GetSeqAlign()->Get();

    cout << "Processing " << seqAlignList.size() << " blast hits (len1 = " << len1 << ", len2 = " << len2 << ").\n";

    m_scores.clear();
    m_evals.clear();
    m_percIdents.clear();
    m_alignments.clear();

    if (seqAlignList.size() == 0) return;

    int nIdent = 0;
	double score = 0.0, eVal = kMax_Double, percIdent = 0.0;
	CRef< CSeq_align > sa = ExtractFirstSeqAlign(*(seqAlignList.begin()));
	if (!sa.Empty()) 
	{
		sa->GetNamedScore(CSeq_align::eScore_Score, score);
		sa->GetNamedScore(CSeq_align::eScore_EValue, eVal);
        if (sa->GetNamedScore(CSeq_align::eScore_IdentityCount, nIdent)) {
            percIdent = 100.0 * invLen1 * (double) nIdent; 
//            cout << "nIdent = " << nIdent << "; percIdent = " << percIdent << endl;
//        } else {
//            cout << "????  Didn't find identity count\n";
        }


//        if (!sa->GetNamedScore(CSeq_align::eScore_PercentIdentity, percIdent))
//            cout << "????  Didn't find percent identity\n";
//        cout << "saving values:   score = " <<score << "; eval = " << eVal << "; id% = " << percIdent << endl;

        m_scores.push_back(score);
        m_evals.push_back(eVal);
        m_percIdents.push_back(percIdent);
        m_alignments.push_back(sa);
	}
}
*/

END_SCOPE(cd_utils)
END_NCBI_SCOPE
