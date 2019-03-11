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
 * Author: Tom Madden
 *
 * File Description:
 *   Results for BLAST-kmer searches
 *
 */

#include <ncbi_pch.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastKmerResults::CBlastKmerResults(CConstRef<objects::CSeq_id>     query,
                        TBlastKmerPrelimScoreVector& scores,
                        BlastKmerStats& stats,
			CRef<CSeqDB> seqdb,
			const TQueryMessages& errs)
    : m_QueryId(query), m_Stats(stats), m_SeqDB(seqdb), m_Errors(errs)
{
	x_InitScoreVec(scores);
}

void CBlastKmerResults::x_InitScoreVec(TBlastKmerPrelimScoreVector& scores) 
{
	m_Scores.reserve(scores.size());
	for(TBlastKmerPrelimScoreVector::iterator iter=scores.begin(); iter != scores.end(); ++iter)
	{
		list< CRef<CSeq_id> > seqids = m_SeqDB->GetSeqIDs((*iter).first);
		if (seqids.size() > 0) // Removed by GIList limit.
		{
		    // logic is similar to the one in GetSequenceLengthAndId  blast_seqinfosrc_aux.cpp 
			CRef<CSeq_id> id = FindBestChoice(seqids, CSeq_id::BestRank);
			if( !id->IsGi() ) id =  seqids.front(); // if GI-less, just grab first accession
			pair<CRef<CSeq_id>, double> retval(id, (*iter).second);
			m_Scores.push_back(retval);
		}
	}
}

void
CBlastKmerResults::GetTSL(TSeqLocVector& tsl, CRef<CScope> scope) const
{
	const TBlastKmerScoreVector& scores = GetScores();

        for(TBlastKmerScoreVector::const_iterator iter=scores.begin(); iter != scores.end(); ++iter)
        {
		  CRef<CSeq_loc> seqloc(new CSeq_loc);
    		  seqloc->SetWhole().Assign(*((*iter).first));
		  auto_ptr<SSeqLoc> ssl(new SSeqLoc(*seqloc, *scope));
		  tsl.push_back(*ssl);
        }
	return;
}

TQueryMessages
CBlastKmerResults::GetErrors(int min_severity) const
{
    TQueryMessages errs;
    
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() >= min_severity) {
            errs.push_back(*iter);
        }
    }
    errs.SetQueryId(m_Errors.GetQueryId());
    
    return errs;
}

bool
CBlastKmerResults::HasErrors() const
{
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() >= eBlastSevError) {
            return true;
        }
    }
    return false;
}

bool
CBlastKmerResults::HasWarnings() const
{
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() == eBlastSevWarning) {
            return true;
        }
    }
    return false;
}

void
CBlastKmerResultsSet::x_Init(TQueryIdVector& ids,
                             TBlastKmerPrelimScoreVectorSet& scores,
                             TBlastKmerStatsVector& stats,
                             CRef<CSeqDB> seqdb,
                             TSearchMessages msg_vec)
{
	m_NumQueries = ids.size();

	m_Results.resize(m_NumQueries);

	for (size_t i=0; i<ids.size(); i++)
	{
		m_Results[i].Reset(new CBlastKmerResults(ids[i], scores[i], stats[i], seqdb, msg_vec[i]));	
	}
}

CBlastKmerResultsSet::CBlastKmerResultsSet(TQueryIdVector& ids,
					   TBlastKmerPrelimScoreVectorSet& scores,
					   TBlastKmerStatsVector& stats,
					   CRef<CSeqDB> seqdb,
                                           TSearchMessages msg_vec)
{
	x_Init(ids, scores, stats, seqdb, msg_vec);
}


CBlastKmerResultsSet::CBlastKmerResultsSet()
{
	m_NumQueries=0;
}


void
CBlastKmerResultsSet::push_back(CBlastKmerResultsSet::value_type& element)
{
	m_Results.push_back(element);
	m_NumQueries++;
}

CRef<CBlastKmerResults>
CBlastKmerResultsSet::operator[](const objects::CSeq_id & ident)
{
	for (size_t i=0; i<m_Results.size(); i++)
	{
		if ( CSeq_id::e_YES == ident.Compare(*m_Results[i]->GetSeqId()) ) 
			return m_Results[i];
        }
	return CRef<CBlastKmerResults>();

}

CRef<CBlastKmerResults>
MakeEmptyResults(TSeqLocVector& queryVector, int queryNum, const string& errMsg, EBlastSeverity severity)
{
	CRef<CBlastKmerResults> retval;
	CRef<CSeq_id> seqid(new CSeq_id());
	seqid->Assign(*(queryVector[queryNum].seqloc->GetId()));
	CRef<CSearchMessage> msg(new CSearchMessage(severity, -1, errMsg));
	TQueryMessages errs;
	errs.push_back(msg);
	TBlastKmerPrelimScoreVector score_vec;
	BlastKmerStats stats;
	errs.SetQueryId(seqid->AsFastaString());
	retval.Reset(new CBlastKmerResults(seqid, score_vec, stats, CRef<CSeqDB>(), errs));

	return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE

