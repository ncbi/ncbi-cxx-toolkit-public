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

#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Object_id.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <objects/seqalign/Dense_seg.hpp>

//#include <algo/structure/cd_utils/cuBlast2Seq.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuSimpleB2SWrapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
BEGIN_SCOPE(cd_utils)

const unsigned int CSimpleB2SWrapper::HITLIST_SIZE_DEFAULT     = 100;
const unsigned int CSimpleB2SWrapper::MAX_HITLIST_SIZE         = 10000;
const int    CSimpleB2SWrapper::CDD_DATABASE_SIZE              = 1000000;
const double CSimpleB2SWrapper::E_VAL_WHEN_NO_SEQ_ALIGN        = 1000000; // eval when Blast doesn't return a seq-align
const double CSimpleB2SWrapper::SCORE_WHEN_NO_SEQ_ALIGN        = -1.0;    
const double CSimpleB2SWrapper::DO_NOT_USE_PERC_ID_THRESHOLD   = -1.0;    
const string CSimpleB2SWrapper::SCORING_MATRIX_DEFAULT = BLOSUM62NAME;

CSimpleB2SWrapper::CSimpleB2SWrapper(double percIdThold, string matrixName)
    : m_scoringMatrix(matrixName), m_hitlistSize(HITLIST_SIZE_DEFAULT)
{
    SetPercIdThreshold(percIdThold);
}

CSimpleB2SWrapper::CSimpleB2SWrapper(CRef<CBioseq>& seq1, CRef<CBioseq>& seq2, double percIdThold, string matrixName)
: m_scoringMatrix(matrixName), m_hitlistSize(HITLIST_SIZE_DEFAULT)
{
    SetSeq(seq1, true, 0, 0);
    SetSeq(seq2, false, 0, 0);
    SetPercIdThreshold(percIdThold);
}


void CSimpleB2SWrapper::SetSeq(CRef<CBioseq>& seq, bool isSeq1, unsigned int from, unsigned int to)
{
    bool full = (from == 0 && to == 0);

    //  If invalid range, also use the full sequence.
    if (full || from > to) {
        full = true;
        from = 0;
		to = (seq.NotEmpty()) ? GetSeqLength(*seq) - 1 : 0;
//		to = (seq.NotEmpty()) ? seq->GetInst().GetLength() - 1 : 0;
    }

    SB2SSeq tmp = {full, from, to, seq};
    if (isSeq1) {
        m_seq1 = tmp;
    } else {
        m_seq2 = tmp;
    }
}

double CSimpleB2SWrapper::SetPercIdThreshold(double percIdThold)
{
    if (percIdThold == DO_NOT_USE_PERC_ID_THRESHOLD || (percIdThold >= 0 && percIdThold <= 100.0)) {
        m_percIdThold = percIdThold;
    }
    return m_percIdThold;
}

unsigned int CSimpleB2SWrapper::SetHitlistSize(unsigned int hitlistSize)
{
    if (hitlistSize > 0 && hitlistSize <= MAX_HITLIST_SIZE) {
        m_hitlistSize = hitlistSize;
    }
    return m_hitlistSize;
}

bool CSimpleB2SWrapper::DoBlast2Seqs()
{
	CRef< CSeq_align > nullRef;
	
	CRef<CBlastAdvancedProteinOptionsHandle> options(new CBlastAdvancedProteinOptionsHandle);
	options->SetEvalueThreshold(10.0);
	//options->SetPercentIdentity(10.0);
	options->SetMatrixName(m_scoringMatrix.c_str());
	options->SetSegFiltering(false);
	options->SetDbLength(CDD_DATABASE_SIZE);
    options->SetHitlistSize(m_hitlistSize);
	options->SetDbSeqNum(1);
	options->SetCompositionBasedStats(eNoCompositionBasedStats);

    if (m_percIdThold != DO_NOT_USE_PERC_ID_THRESHOLD) options->SetPercentIdentity(m_percIdThold);
//    cout << "m_percIdThold = " << m_percIdThold << ";  DO_... = " << DO_NOT_USE_PERC_ID_THRESHOLD << endl;

	CRef<CBlastProteinOptionsHandle> blastOptions((CBlastProteinOptionsHandle*)options.GetPointer());
	
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
       processBlastHits(hits[index]);

//  dump the CSearchResults object... 
//    string err;
//    WriteASNToStream(cout, *hits, false, &err);

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
//    unsigned int len2 = (m_seq2.to >= m_seq2.from) ? m_seq2.to - m_seq2.from + 1 : 0;
    double invLen1 = (len1) ? 1.0/(double) len1 : 0;
	const list< CRef< CSeq_align > >& seqAlignList = hits.GetSeqAlign()->Get();

//    cout << "Processing " << seqAlignList.size() << " blast hits (len1 = " << len1 << ", len2 = " << len2 << ").\n";

    m_scores.clear();
    m_evals.clear();
    m_percIdents.clear();
    m_alignments.clear();

    if (seqAlignList.size() == 0) return;

    int nIdent = 0;
	double score = 0.0, eVal = kMax_Double, percIdent = 0.0;
	CRef< CSeq_align > sa = *(seqAlignList.begin());
	if (!sa.Empty()) 
	{
		sa->GetNamedScore(CSeq_align::eScore_Score, score);
		sa->GetNamedScore(CSeq_align::eScore_EValue, eVal);
        if (sa->GetNamedScore(CSeq_align::eScore_IdentityCount, nIdent)) {
            percIdent = 100.0 * invLen1 * (double) nIdent; 
//                cout << "nIdent = " << nIdent << "; percIdent = " << percIdent << endl;
//         } else {
//              cout << "????  Didn't find identity count\n";
        }


//            if (!sa->GetNamedScore(CSeq_align::eScore_PercentIdentity, percIdent))
//                cout << "????  Didn't find percent identity\n";
//            cout << "saving values:   score = " <<score << "; eval = " << eVal << "; id% = " << percIdent << endl;
        m_scores.push_back(score);
        m_evals.push_back(eVal);
        m_percIdents.push_back(percIdent);
        m_alignments.push_back(sa);
	}
}

//input seqAlign may actually contain CSeq_align_set
CRef< CSeq_align > CSimpleB2SWrapper::extractOneSeqAlign(CRef< CSeq_align > seqAlign)
{
	if (seqAlign.Empty())
		return seqAlign;
	if (!seqAlign->GetSegs().IsDisc())
		return seqAlign;
	if (seqAlign->GetSegs().GetDisc().CanGet())
	{
		const list< CRef< CSeq_align > >& saList = seqAlign->GetSegs().GetDisc().Get();
		if (saList.begin() != saList.end())
			return *saList.begin();
	}
	CRef< CSeq_align > nullRef;
	return nullRef;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
