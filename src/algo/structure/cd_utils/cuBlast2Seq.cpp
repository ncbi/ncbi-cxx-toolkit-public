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
 *       Functions to call C++ BLAST
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
//#include <algo/blast/api/bl2seq.hpp>
#include <objmgr/object_manager.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/blast/blastclient.hpp>
#include <objects/blast/Blast4_database_info.hpp>
//#include <objtools/simple/simple_om.hpp>

#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuBlast2Seq.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuCdUpdater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int    CdBlaster::CDD_DATABASE_SIZE              = 1000000;
const double CdBlaster::BLAST_SCALING_FACTOR_DEFAULT   = 1.0;
const double CdBlaster::E_VAL_WHEN_NO_SEQ_ALIGN        = 1000000; // eval when Blast doesn't return a seq-align
const double CdBlaster::SCORE_WHEN_NO_SEQ_ALIGN        = -1;    
const string CdBlaster::SCORING_MATRIX_DEFAULT = BLOSUM62NAME;
const long CdBlaster::DEFAULT_NR_SIZE = 1196146007;
const int CdBlaster::DEFAULT_NR_SEQNUM = 3479934;

CdBlaster::CdBlaster(AlignmentCollection& source, string matrixName)
: m_ac(&source), m_queryRows(0), m_subjectRows(0), m_seqs(0), m_scoringMatrix(matrixName), m_useWhole(false), 
m_scoreType(CSeq_align::eScore_Score), m_psiTargetCd(0)  
{
	//m_offsets.assign(m_ac->GetNumRows(), 0);
	m_batchSizes.assign(m_ac->GetNumRows(), 0);
	m_nExt = m_cExt = 0;
	m_dbSize = DEFAULT_NR_SIZE;
	m_dbSeqNum = DEFAULT_NR_SEQNUM;
}

CdBlaster::CdBlaster(vector< CRef<CBioseq> >& seqs, string matrixName)
: m_ac(0), m_seqs(&seqs), m_scoringMatrix(matrixName), m_useWhole(false), m_psiTargetCd(0) 
{
	//m_offsets.assign(seqs.size(), 0);
	m_batchSizes.assign(seqs.size(), 0);
	m_nExt = m_cExt = 0;
	m_dbSize = DEFAULT_NR_SIZE;
	m_dbSeqNum = DEFAULT_NR_SEQNUM;
}

void CdBlaster::useWholeSequence(bool whole)
{
	m_useWhole = whole;
}

void CdBlaster::setFootprintExtension(int nExt, int cExt)
{
	m_nExt = nExt;
	m_cExt = cExt;
	m_useWhole = false;
}

//to do psi-blast
void CdBlaster::setPsiBlastTarget(CRef<CPssmWithParameters> pssm)
{
	m_psiTargetPssm = pssm;
}

CRef<CPssmWithParameters> CdBlaster::setPsiBlastTarget(CCdCore* targetCD)
{
	m_psiTargetCd = targetCD;
	cd_utils::PssmMaker pm(targetCD,true,true);   // 2rd param is useConsensus.  generally "true".
	cd_utils::PssmMakerOptions config;
	config.requestFrequencyRatios = true;
	config.matrixName = m_scoringMatrix;
	pm.setOptions(config);
	m_psiTargetPssm = pm.make();
	return(m_psiTargetPssm);
}

bool CdBlaster::blast(NotifierFunction notifier)
{
	int nrows = 0;
	int numBlastsDone = 0;
	int totalBlasts = 0;
	CRef< CBioseq > bioseqRef;
	if (m_queryRows && m_subjectRows)
	{
		nrows = m_queryRows->size() + m_subjectRows->size();
		totalBlasts = m_queryRows->size() * m_subjectRows->size();
		for (unsigned int q = 0; q < m_queryRows->size(); q++)
		{
			if (m_useWhole)
			{
				m_ac->GetBioseqForRow((*m_queryRows)[q], bioseqRef);
				m_truncatedBioseqs.push_back(bioseqRef);
			}
			else
				m_truncatedBioseqs.push_back(truncateBioseq((*m_queryRows)[q]));
		}
		for (unsigned int s = 0; s < m_subjectRows->size(); s++)
		{
			if (m_useWhole)
			{
				m_ac->GetBioseqForRow((*m_subjectRows)[s], bioseqRef);
				m_truncatedBioseqs.push_back(bioseqRef);
			}
			else
				m_truncatedBioseqs.push_back(truncateBioseq((*m_subjectRows)[s]));
		}
	}
	else
	{
		nrows = m_ac->GetNumRows();
		for (int i = 0; i < nrows; i++)
		{
			if (m_useWhole)
			{
				m_ac->GetBioseqForRow(i, bioseqRef);
				m_truncatedBioseqs.push_back(bioseqRef);
			}
			m_truncatedBioseqs.push_back(truncateBioseq(i));
		}
		totalBlasts = (int)((double)nrows * (((double)nrows-1)/2));
	}
	
	CRef<CBlastAdvancedProteinOptionsHandle> options(new CBlastAdvancedProteinOptionsHandle);
	options->SetEvalueThreshold(10.0);
	//options->SetPercentIdentity(10.0);
	options->SetMatrixName(m_scoringMatrix.c_str());
	options->SetSegFiltering(false);
	options->SetDbLength(CDD_DATABASE_SIZE);
	options->SetHitlistSize(nrows);
	options->SetDbSeqNum(1);
	options->SetCompositionBasedStats(eNoCompositionBasedStats);
	CRef<CBlastProteinOptionsHandle> blastOptions((CBlastProteinOptionsHandle*)options.GetPointer());
	
	CRef< CSeq_align > nullRef;
	m_scores.reserve(totalBlasts);

	// use objmgr interface
    RemoveAllDataLoaders();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);

	int numQueries = 0;
	if (m_queryRows)
		numQueries = m_queryRows->size();
	else
		numQueries = nrows-1;

    bool result = true;
    CSeq_loc querySeqLoc, subjectSeqLoc;
	for (int qr = 0; qr < numQueries; qr++)
	{
        CBlastQueryVector queryVector, subjectVector;
        scope.ResetDataAndHistory();

        //  Set up the QueryFactory for the query sequence
        scope.AddBioseq(*m_truncatedBioseqs[qr]);
        if (FillOutSeqLoc(m_truncatedBioseqs[qr], querySeqLoc)) {
            CRef<CBlastSearchQuery> bsqQuery(new CBlastSearchQuery(querySeqLoc, scope));
            queryVector.AddQuery(bsqQuery);
        } else {
            result = false;
            m_batchSizes[qr] = 0;
            continue;
        }
        CRef<IQueryFactory> query(new CObjMgr_QueryFactory(queryVector));

/*
		CRef< CBioseq > queryBioseq = m_truncatedBioseqs[qr];
		CRef<IQueryFactory> query(new CObjMgrFree_QueryFactory(queryBioseq));

		CRef< CBioseq_set > bioseqset(new CBioseq_set);
		list< CRef< CSeq_entry > >& seqEntryList = bioseqset->SetSeq_set();
*/
		int subStart = qr +1;
		int batchSize = (nrows -1) - (qr + 1) + 1;
		if (m_queryRows)
		{
			subStart = m_queryRows->size();
			batchSize = m_subjectRows->size();
		}
		//loop for subject rows
		for (int sr = subStart; sr < nrows; sr++)
		{
/*
			CRef< CSeq_entry > seqEntry(new CSeq_entry);
			seqEntry->SetSeq(*m_truncatedBioseqs[sr]);
			seqEntryList.push_back(seqEntry);
			comIndex++;
*/

            scope.AddBioseq(*m_truncatedBioseqs[sr]);
            //  Set up the QueryFactory for the subject sequences
            if (FillOutSeqLoc(m_truncatedBioseqs[sr], subjectSeqLoc)) {
                CRef<CBlastSearchQuery> bsqSubject(new CBlastSearchQuery(subjectSeqLoc, scope));
                subjectVector.AddQuery(bsqSubject);
            }
		}

        assert((unsigned)batchSize == subjectVector.Size());
		m_batchSizes[qr] = subjectVector.Size();  // in case there was a problem w/ FillOutSeqLoc above, use the actual size submitted instead of batchSize

//		CRef<IQueryFactory> subject(new CObjMgrFree_QueryFactory(bioseqset));
		CRef<IQueryFactory> subject(new CObjMgr_QueryFactory(subjectVector));
		CPsiBl2Seq blaster(query,subject,blastOptions);
		CSearchResultSet& hits = *blaster.Run();
		numBlastsDone += batchSize;  // don't use subjectVector.Size() so notifier(...) works normally even if FillOutSeqLoc failed above
		if (notifier)
			notifier(numBlastsDone, totalBlasts);
		processBlastHits(qr, hits);
	}
	return result;
}
	
int  CdBlaster::psiBlast()
{
	//debug
	assert( !m_psiTargetPssm.Empty());
	int nrows = 0;
	if (m_ac)
		nrows = m_ac->GetNumRows();
	else
		nrows= m_seqs->size();
	
	CRef<CPSIBlastOptionsHandle> options(new CPSIBlastOptionsHandle);
	options->SetMatrixName(m_scoringMatrix.c_str());
	options->SetDbLength(m_dbSize);
	options->SetDbSeqNum(m_dbSeqNum);
	options->SetHitlistSize(nrows);
	options->SetCompositionBasedStats(eCompositionBasedStats);
	options->SetSegFiltering(false);
	options->SetPseudoCount(m_psiTargetPssm->GetParams().GetPseudocount());
	//options->SetEffectiveSearchSpace(27608309120);
	// debugging
//	options->SetCompositionBasedStatsMode(true);

	// use objmgr interface
    RemoveAllDataLoaders();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    CBlastQueryVector subjectVector;

/*
	CRef< CBioseq_set > bioseqset(new CBioseq_set);
	list< CRef< CSeq_entry > >& seqEntryList = bioseqset->SetSeq_set();
*/
    CSeq_loc subjectSeqLoc;
	for (int sr = 0; sr < nrows; sr++)
	{
        CRef< CBioseq > bs = truncateBioseq(sr);
        scope.AddBioseq(*bs);
        //  Set up the QueryFactory for the subject sequences
        if (FillOutSeqLoc(bs, subjectSeqLoc)) {
            CRef<CBlastSearchQuery> bsqSubject(new CBlastSearchQuery(subjectSeqLoc, scope));
            subjectVector.AddQuery(bsqSubject);
        }
/*
		// use footprint
		
		CRef< CSeq_entry > seqEntry(new CSeq_entry);
		seqEntry->SetSeq(*truncateBioseq(sr));
		seqEntryList.push_back(seqEntry);
*/
		// use whole sequence
		/*
		CRef< CSeq_entry > seqEntry;
		if (m_ac->GetSeqEntryForRow(sr, seqEntry))
			seqEntryList.push_back(seqEntry);
		else
			bool wrong = true;
			*/
	}
    CRef<IQueryFactory> subject(new CObjMgr_QueryFactory(subjectVector));
//	CRef<IQueryFactory> subject(new CObjMgrFree_QueryFactory(bioseqset));

	CPsiBl2Seq blaster(m_psiTargetPssm, subject, options);
	CRef<CSearchResultSet> hits = blaster.Run();
	unsigned int index, total = hits->GetNumResults();
    for (index = 0; index < total; ++index) 
    {
		const list< CRef< CSeq_align > >& seqAlignList = (*hits)[index].GetSeqAlign()->Get();
		if (seqAlignList.empty())
			m_alignments.push_back(CRef< CSeq_align>());
		else
			m_alignments.push_back(*(seqAlignList.begin()));
    }	
	assert (m_alignments.size() == (unsigned) nrows);
	return m_alignments.size();
}

CRef<CSeq_align> CdBlaster::getPairwiseBlastAlignement(int row1, int row2)
{
	return m_alignments[getCompositeIndex(row1, row2)];
}

double  CdBlaster::getPairwiseScore(int row1, int row2)
{
	return(m_scores[getCompositeIndex(row1, row2)]);
}

double  CdBlaster::getPairwiseEValue(int row1, int row2)
{
	return(m_evals[getCompositeIndex(row1, row2)]);
}

double CdBlaster::getPsiBlastScore(int row)
{
	double score = SCORE_WHEN_NO_SEQ_ALIGN;
	CRef< CSeq_align > sa = getPsiBlastAlignement(row);
	if (!sa.Empty())
	{
		sa->GetNamedScore("score", score);
	}
	return score;
}

double CdBlaster::getPsiBlastEValue(int row)
{
	double evalue = E_VAL_WHEN_NO_SEQ_ALIGN;
	CRef< CSeq_align > sa = getPsiBlastAlignement(row);
	if (!sa.Empty())
	{
		sa->GetNamedScore("e_value", evalue);
	}
	return evalue;
}

CRef<CSeq_align> CdBlaster::getPsiBlastAlignement(int row)
{
	return m_alignments[row];
}

CRef< CBioseq > CdBlaster::truncateBioseq(int row)
{
	CRef<CBioseq> bioseq;
	int len = 0;
	int from = 0;
	int to = 0;

	if (m_ac)
	{
		if (!m_ac->GetBioseqForRow(row, bioseq))
			return bioseq;
		from = m_ac->GetLowerBound(row);
		to = m_ac->GetUpperBound(row);
	}
	else if (m_seqs)
	{
		bioseq = (*m_seqs)[row];
		if (bioseq.Empty())
			return bioseq;
		from = 0;
		to = bioseq->GetInst().GetLength() - 1;
		if (bioseq->IsSetAnnot())
		{
			const list< CRef< CSeq_annot > >& annots = bioseq->GetAnnot();
			for (list< CRef< CSeq_annot > >::const_iterator cit = annots.begin(); cit != annots.end(); cit++)
			{
				if ((*cit)->IsSetData())
				{
					if ((*cit)->GetData().IsLocs())
					{
						const list< CRef< CSeq_loc > >& locs = (*cit)->GetData().GetLocs();
						if (locs.size() > 0)
						{
							CRef< CSeq_loc > seqLoc = *locs.begin();
							if (seqLoc->IsInt())
							{
								from = seqLoc->GetInt().GetFrom();
								to = seqLoc->GetInt().GetTo();
							}
						}
					}
				}
			}
		}

	}
	len = bioseq->GetInst().GetLength();
	if(m_useWhole)
		return bioseq;
	CRef<CBioseq> tbioseq(new CBioseq);
	tbioseq->Assign(*bioseq);
	
	string seqData;
	GetNcbieaaString(*bioseq, seqData);
	ApplyEndShiftToRange(from, m_nExt, to, m_cExt, len);
	//m_offsets[row] = from; //keep this for using to remap seq-align later.
	tbioseq->SetInst().SetLength(to - from + 1);
	CNCBIeaa tr(seqData.substr(from, to - from + 1));
	tbioseq->SetInst().SetSeq_data().SetNcbieaa(tr);
	return tbioseq;
}

void CdBlaster::processBlastHits(int queryRow, CSearchResultSet& hits)
{
    double score, idScore;
	int seqLen = m_truncatedBioseqs[queryRow]->GetInst().GetLength();
	int nhits = hits.GetNumResults();
	assert (nhits == m_batchSizes[queryRow]);
	for (int i = 0; i < nhits; i++)
	{
		score = 0.0;
		const list< CRef< CSeq_align > >& seqAlignList = hits[i].GetSeqAlign()->Get();
		if (seqAlignList.size() > 0) 
		{
            CRef< CSeq_align > sa = ExtractFirstSeqAlign(*(seqAlignList.begin()));  
            if (!sa.Empty()) {
                if (m_scoreType == CSeq_align::eScore_PercentIdentity)
                    {
                        idScore = 0.0;
                        sa->GetNamedScore(CSeq_align::eScore_IdentityCount, idScore);
                        if (seqLen != 0) {
                            score = 100*idScore/seqLen;
                        }
                    }
                else
                    sa->GetNamedScore(m_scoreType, score);
            }
		}
		m_scores.push_back(score);
	}
}

int CdBlaster::getCompositeIndex(int query, int subject)
{
	//make sure query < subject, otherwise swap
	int comp = -1;
	if (m_queryRows == 0)
	{
		int realQuery = query;
		int realSubject = subject;
		if (query > subject)
		{
			realQuery = subject;
			realSubject = query;
		}
		int nrows = m_ac->GetNumRows();
		int totalBeforeQuery = (nrows - 1 + nrows - realQuery)*realQuery/2;
		comp = totalBeforeQuery + (realSubject - realQuery - 1);
	}
	else
	{
		comp = query * m_subjectRows->size() + subject;
	}
	return comp;
}

//  A couple functions to manage extensions at end of aligned range
bool CdBlaster::IsFootprintValid(int from, int to, int len) {
	
	//  The positions on a sequence are offsets from start of sequence and hence
	//  run from [0, length-1].  See Bioseq section of C Toolkit docs.
	bool result = false;
	if (from < 0 || to < 0 || len <=0) return result;
	if (from <= to && to < len && (to - from + 1 >= 0)) {
		result = true;
	}
	return result;
}

void CdBlaster::ApplyEndShiftToRange(int& from, int nTermShift, int& to, int cTermShift, int len) 
{

    //  Shift ends of range by indicated amounts:  positive values extend, and negative 
    //  values shorten.
	//  A full sequence is indicated if from = to = 0, or from = 0 and to = len - 1,
    //  and in this case it should be negative.  For a footprint, the positive shift extends
	//  the footprint defined by [from, to] by the shift or to the end, whichever is closer.
	//  If on shortening, the shifts cause a crossing of from/to values, revert to using
	//  the zero shifts.  
	
	if (nTermShift == 0 && cTermShift == 0) {
		return;
	}
//	nTermShift = (nTermShift < 0) ? -nTermShift : nTermShift;
//	cTermShift = (cTermShift < 0) ? -cTermShift : cTermShift;

    //  truncate full sequence; if shifts aren't negative, set them to zero.
    if (from == 0 && (to == 0 || to == len - 1)) {
        nTermShift = (nTermShift < 0) ? nTermShift : 0;
        cTermShift = (cTermShift < 0) ? cTermShift : 0;
		if (-nTermShift < len - 1 + cTermShift ) {
			from = -nTermShift;
			to  += cTermShift;	
		} else {
			from = 0;
			to   = len - 1;
		}

	} else {   //  extend or shrink footprint
        if (nTermShift >= 0) {
    		from = (nTermShift <= from)         ? from - nTermShift : 0;
        } else {
    		from = (-nTermShift < to - 1 + cTermShift)         ? from - nTermShift : from;
        }
        if (cTermShift >= 0) {
    		to   = (cTermShift <= len - 1 - to) ? to + cTermShift : len - 1;
        } else {
    		to   = (-cTermShift <= to - from - 1 + nTermShift) ? to + cTermShift : to;
        }
	}

}

bool CdBlaster::FillOutSeqLoc(const CRef< CBioseq>& bs, CSeq_loc& seqLoc)
{
    bool result = true;
    CSeq_interval& seqInt = seqLoc.SetInt();
    CSeq_id& seqId = seqInt.SetId();
    seqInt.SetFrom(0);
    
    //  Assign the first identifier from the bioseq
    if (bs.NotEmpty() && bs->GetFirstId() != 0) {
        seqInt.SetTo(bs->GetLength() - 1);
        seqId.Assign(*(bs->GetFirstId()));
    } else {
        result = false;
    }

    return result;
}

void CdBlaster::RemoveAllDataLoaders() {
    int i = 1;
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    om->GetRegisteredNames(loader_names);
    ITERATE(CObjectManager::TRegisteredNames, itr, loader_names) {
        om->RevokeDataLoader(*itr);
        ++i;
    }
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE

