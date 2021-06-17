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
 *   Class to MT KMER BLAST searches
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/format/blast_async_format.hpp>
#include <math.h>

#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/api/seqinfosrc_seqvec.hpp>

#include <objtools/readers/fasta.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>

#include "kblastthr.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

CFastMutex globalGuard;
int globalBatchNumber=0;
int globalNumThreadsDone=0;


CRef<CBlastQueryVector> s_GetBlastQueryVector(TSeqLocVector& tsl_v)
{
	CRef<CBlastQueryVector> q_vec(new CBlastQueryVector());
	for(TSeqLocVector::iterator iter=tsl_v.begin(); iter!=tsl_v.end(); ++iter)
	{
		CRef<CBlastSearchQuery> blast_q;
		blast_q.Reset(new CBlastSearchQuery(*(*iter).seqloc, *(*iter).scope));
		q_vec->AddQuery(blast_q);
	}
	return q_vec;
}

CBlastKmerThread::~CBlastKmerThread()
{
	// cerr << "Thread number " << GetSelf() << " exiting\n";
}

/////////////////////////////////////////////////////////////////////////////
//  Main program for CThread

void* CBlastKmerThread::Main(void)
{
	CRef<CSearchDatabase> target_db = m_DbArgs->GetSearchDatabase();
	// FIXME.  Do not want this with GILIST: target_db->GetSeqDb()->SetNumberOfThreads(1, true);
	CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*target_db));
	const bool kIsProtein = true;
        SDataLoaderConfig dlconfig(kIsProtein, SDataLoaderConfig::eUseNoDataLoaders);
        CRef<CScope> scope(CBlastScopeSource(dlconfig).NewScope());
	int batchNumber=-1;
	CRef<CBlastOptions> newOpts = m_BlastOptsHandle->GetOptions().Clone();
	CRef<CBlastpKmerOptionsHandle> optsHndle(new CBlastpKmerOptionsHandle(newOpts));
	// FIXME: should be done as part of clone.
	optsHndle->SetThresh(m_BlastOptsHandle->GetThresh());
	optsHndle->SetMinHits(m_BlastOptsHandle->GetMinHits());
	optsHndle->SetCandidateSeqs(m_BlastOptsHandle->GetCandidateSeqs());
	CBlastKmerSearch blastSearch(optsHndle, db_adapter);
	const CRef< CSeqDBGiList > gilist = target_db->GetGiList();
	if (gilist && gilist->NotEmpty())
		blastSearch.SetGiListLimit(gilist);
	const CRef< CSeqDBGiList > neggilist = target_db->GetNegativeGiList();
	if (neggilist && neggilist->NotEmpty())
	{
		vector<TGi> gis;
		neggilist->GetGiList(gis);
		CSeqDBIdSet idset(gis, CSeqDBIdSet::eGi, false);
		blastSearch.SetGiListLimit(idset.GetNegativeList());
	}

// CStopWatch watch;
// watch.Start();
	bool isDone=false;
	while (!isDone) 
	{
		TSeqLocVector query_vector;
		globalGuard.Lock();
		isDone = m_Input.End();  
		if (isDone)
		{
			globalNumThreadsDone++;
			globalGuard.Unlock();
			break;
		}
		else
		{
			query_vector = m_Input.GetNextSeqLocBatch(*scope);
			CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_vector));
			blastSearch.SetQuery(qf);
			batchNumber=globalBatchNumber++;
		}
		globalGuard.Unlock();
		CRef<CSearchResultSet> blast_results = blastSearch.Run();
		CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*target_db));
		CRef<CBlastFormat> formatter(new CBlastFormat(optsHndle->GetOptions(), *db_adapter,
                                        m_FormattingArgs->GetFormattedOutputChoice(),
                                        m_BelieveQuery, m_OutFile, m_FormattingArgs->GetNumDescriptions(),
                                        m_FormattingArgs->GetNumAlignments(), *scope, BLAST_DEFAULT_MATRIX,
                                        false, false,  BLAST_GENETIC_CODE,  BLAST_GENETIC_CODE, false, false, -1,
                                        m_FormattingArgs->GetCustomOutputFormatSpec()));

		vector<SFormatResultValues> results_v;
		CRef<CBlastQueryVector> q_vec = s_GetBlastQueryVector(query_vector);
		results_v.push_back(SFormatResultValues(q_vec, blast_results, formatter));
		m_FormattingThr->QueueResults(batchNumber, results_v);
	}

     return (void*) NULL;
}
  
