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
 *   API for KMER BLAST searches
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <math.h>

#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/api/seqinfosrc_seqvec.hpp>

#include <util/sequtil/sequtil_convert.hpp>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>
#include <algo/blast/proteinkmer/kblastapi.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

/// Places all the needed subject sequences into a scope.  
/// The database loader (or cache perhaps?) does not seem to be thread safe, so it
/// is avoided.
static void 
s_GetSequencesIntoScope(CRef<CBlastKmerResultsSet> resultSet, CRef<CScope> scope, CRef<CSeqDB> seqdb)
{
        int numSearches=resultSet->GetNumQueries();
        vector<int> oid_v;
        for (int index=0; index<numSearches; index++)
        {
                CBlastKmerResults& results = (*resultSet)[index];
                TBlastKmerScoreVector scores = results.GetScores();
                int oid;
                for(TBlastKmerScoreVector::const_iterator iter=scores.begin(); iter != scores.end(); ++iter)
                {
                        CRef<CSeq_id> sid = (*iter).first;
			if (sid->IsGi())
				seqdb->GiToOid(sid->GetGi(), oid);
			else
                        	seqdb->SeqidToOid(*sid, oid);
                        oid_v.push_back(oid);
                }
        }
        if (oid_v.size() == 0) // Nothing to be loaded.
                return;

        sort(oid_v.begin(), oid_v.end());

        for(vector<int>::iterator iter=oid_v.begin(); iter!=oid_v.end(); ++iter)
        {
                CRef<CBioseq> bioseq = seqdb->GetBioseq(*iter);
                scope->AddBioseq(*bioseq);
        }
        return;
}

void s_AddNewResultSet(CRef<CSearchResultSet> resultSet, CRef<CSearchResultSet> myResultSet)
{
	for (CSearchResultSet::iterator iter=myResultSet->begin(); iter!=myResultSet->end(); ++iter)
	{
		resultSet->push_back(*iter);
	}
}

CRef<CSearchResultSet> s_MakeEmptyResults(CRef<IQueryFactory> qf,  const CBlastOptions& opts, 
CRef<CLocalDbAdapter> dbAdapter, TSearchMessages& msg_vec) 
{
         // Search was not run, but we send back an empty CSearchResultSet.
         CRef<ILocalQueryData> local_query_data = qf->MakeLocalQueryData(&opts);
         vector< CConstRef<objects::CSeq_id> > seqid_vec;
         vector< CRef<CBlastAncillaryData> > ancill_vec;
         TSeqAlignVector sa_vec;
         size_t index;
         EResultType res_type = eDatabaseSearch;
         unsigned int num_subjects = 0;
         if (dbAdapter.NotEmpty() && !dbAdapter->IsBlastDb() && !dbAdapter->IsDbScanMode()) {
        	 res_type = eSequenceComparison;
             IBlastSeqInfoSrc *  subject_infosrc = dbAdapter->MakeSeqInfoSrc();
             if(subject_infosrc != NULL) {
            	 num_subjects = subject_infosrc->Size();
             }
         }
         for (index=0; index<local_query_data->GetNumQueries(); index++)
         {
              CConstRef<objects::CSeq_id> query_id(local_query_data->GetSeq_loc(index)->GetId());
              TQueryMessages q_msg;
	      /// FIXME, PROBLEM??
              // local_query_data->GetQueryMessages(index, q_msg);
              // msg_vec.push_back(q_msg);
              seqid_vec.push_back(query_id);
              CRef<objects::CSeq_align_set> tmp_align;
              sa_vec.push_back(tmp_align);
              pair<double, double> tmp_pair(-1.0, -1.0);
              CRef<CBlastAncillaryData>  tmp_ancillary_data(new CBlastAncillaryData(tmp_pair, tmp_pair, tmp_pair, 0));
              ancill_vec.push_back(tmp_ancillary_data);

              for(unsigned int i =1; i < num_subjects; i++)
              {
            	  TQueryMessages msg;
                  msg_vec.push_back(msg);
                  seqid_vec.push_back(query_id);
                  CRef<objects::CSeq_align_set> tmp_align;
                  sa_vec.push_back(tmp_align);
           	      CRef<CBlastAncillaryData>  tmp_ancillary_data(new CBlastAncillaryData(tmp_pair, tmp_pair, tmp_pair, 0));
           	      ancill_vec.push_back(tmp_ancillary_data);
              }
         }
	 msg_vec.resize(seqid_vec.size());  // FIXME
         CRef<CSearchResultSet> result_set(new CSearchResultSet(seqid_vec, sa_vec, msg_vec, ancill_vec, 0, res_type));
         return result_set;
}

/////////////////////////////////////////////////////////////////////////////
//  Perform a KMER search then a BLAST search.

CRef<CSearchResultSet> CBlastKmerSearch::Run(void)
{
	CRef<CSeqDB> seqdb(new CSeqDB(m_Database->GetDatabaseName(), CSeqDB::eProtein));
	seqdb->SetNumberOfThreads(1, true);

	if (m_OptsHandle->GetDbLength() == 0)
		m_OptsHandle->SetDbLength(seqdb->GetTotalLength());

	CBlastpKmerOptionsHandle* kmerOptHndl = dynamic_cast<CBlastpKmerOptionsHandle*> (&*m_OptsHandle);
	CRef<CBlastKmerOptions> opts(new CBlastKmerOptions()); // KMER specific options.
	opts->SetThresh(kmerOptHndl->GetThresh());
	opts->SetMinHits(kmerOptHndl->GetMinHits());
	opts->SetNumTargetSeqs(kmerOptHndl->GetCandidateSeqs());

	CObjMgr_QueryFactory* objmgr_qf = NULL;
	TSeqLocVector tsl_v;
	if ( (objmgr_qf = dynamic_cast<CObjMgr_QueryFactory*>(&*m_QueryFactory)) )
    	{
		tsl_v = objmgr_qf->GetTSeqLocVector();
		_ASSERT(!tsl_v.empty());
	}
	CRef<CBlastKmer> blastkmer(new CBlastKmer(tsl_v, opts, seqdb));
	if (!m_GIList.Empty())
		blastkmer->SetGiListLimit(m_GIList);
	else if(!m_NegGIList.Empty())
		blastkmer->SetGiListLimit(m_NegGIList);

	CRef<CBlastKmerResultsSet> resultSet = blastkmer->RunSearches();

	//FIXME: check if all are same or use all?
	vector< CRef<CScope> > scope_v = objmgr_qf->ExtractScopes();
	s_GetSequencesIntoScope(resultSet, scope_v[0], seqdb);

	int numSearches=resultSet->GetNumQueries();
	CRef<CSearchResultSet> search_results(new CSearchResultSet());
	for (int index=0; index<numSearches; index++)
	{
		CBlastKmerResults& results = (*resultSet)[index];
		if (results.HasErrors() || results.HasWarnings())
		{
			TQueryMessages msg = results.GetErrors(eBlastSevWarning);
			string queryId = msg.GetQueryId();
			for(TQueryMessages::const_iterator iter=msg.begin(); iter != msg.end(); ++iter)
			{
				cerr << queryId << " " << (*iter)->GetMessage() << '\n';
			}
		}
		const TBlastKmerScoreVector& scores = results.GetScores();

     		if (scores.size() > 0)
     		{
			TSeqLocVector subjectTSL;
			results.GetTSL(subjectTSL, scope_v[0]);

			TSeqLocVector query_vector_temp;
			query_vector_temp.push_back(tsl_v[index]);
			CRef<IQueryFactory> qfactory(new CObjMgr_QueryFactory(query_vector_temp));

			BlastSeqSrc* seq_src = MultiSeqBlastSeqSrcInit(subjectTSL, eBlastTypeBlastp, true);
			CRef<IBlastSeqInfoSrc> seqinfo_src(new CSeqVecSeqInfoSrc(subjectTSL));

			CLocalBlast lcl_blast(qfactory, CRef<CBlastOptionsHandle> (m_OptsHandle.GetPointer()), seq_src, seqinfo_src);
			CRef<CSearchResultSet> my_blast_results = lcl_blast.Run();
			s_AddNewResultSet(search_results, my_blast_results);
			seq_src = BlastSeqSrcFree(seq_src);
		}
		else
		{
			TSeqLocVector query_vector_temp;
			query_vector_temp.push_back(tsl_v[index]);
			CRef<IQueryFactory> qfactory(new CObjMgr_QueryFactory(query_vector_temp));
			TSearchMessages msg;
			if (results.HasErrors() || results.HasWarnings())
			{
				TQueryMessages qmsg = results.GetErrors(eBlastSevWarning);
				msg.push_back(qmsg);
			}
			CRef<CSearchResultSet> my_blast_results=s_MakeEmptyResults(qfactory, m_OptsHandle->GetOptions(), CRef<CLocalDbAdapter>(NULL), msg);
			s_AddNewResultSet(search_results, my_blast_results);
		}
     }
	
     return search_results;
}
  
