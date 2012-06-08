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
 * Authors:  Amelia Fong
 *
 */

/** @file rpsblast_local.cpp
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiexpt.hpp>
#include <algo/blast/api/rpsblast_local.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/api/blast_rps_options.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

static string delimiter="#rps#";


static void s_ConvertConcatStringToVectorOfString(const string & s, vector<string> & v)
{
	int pos_start = 0;
	while(1)
	{
		size_t pos_find = s.find(delimiter, pos_start);
		if(pos_find == string::npos)
			break;
		TSeqPos length = pos_find - pos_start;
		v.push_back(s.substr(pos_start, length));
		pos_start = pos_find + delimiter.size();
	}

	v.push_back(s.substr(pos_start, s.size() - pos_start ));
}

static void s_MergeAlignSet(CSeq_align_set & final_set, const CSeq_align_set & input_set)
{
	CSeq_align_set::Tdata & final_list = final_set.Set();
	const CSeq_align_set::Tdata & input_list = input_set.Get();

	CSeq_align_set::Tdata::const_iterator	input_it = input_list.begin();
	CSeq_align_set::Tdata::iterator		final_it = final_list.begin();
	while(input_it != input_list.end())
	{
		double final_evalue;
		double input_evalue;

		(*final_it)->GetNamedScore(CSeq_align::eScore_EValue, final_evalue);
		(*input_it)->GetNamedScore(CSeq_align::eScore_EValue, input_evalue);

		if(input_evalue == final_evalue)
		{
			//Pulling a trick here to keep the program flow simple
			//Replace the final evalue with input bitscore and vice versa
			(*final_it)->GetNamedScore(CSeq_align::eScore_BitScore, input_evalue);
			(*input_it)->GetNamedScore(CSeq_align::eScore_BitScore, final_evalue);
		}

		if(input_evalue <  final_evalue)
		{
			CSeq_align_set::Tdata::const_iterator start_input_it = input_it;
			while(1)
			{
				const CSeq_id &  id_prev = (*input_it)->GetSeq_id(1);
				input_it++;
				if(input_it == input_list.end())
				{
					break;
				}

				if(! id_prev.Match((*input_it)->GetSeq_id(1)))
				{
					break;
				}
			}

			final_list.insert(final_it, start_input_it, input_it);
		}
		else
		{
			while(1)
			{
				const CSeq_id &  id_prev = (*final_it)->GetSeq_id(1);
				final_it++;

				if(final_it == final_list.end())
				{
					break;
				}

				if(! id_prev.Match((*final_it)->GetSeq_id(1)))
				{
					break;
				}
			}

			if(final_it == final_list.end())
			{
				final_list.insert(final_it, input_it, input_list.end());
				break;
			}
		}
	}
}

static CRef<CSearchResultSet>  s_CombineSearchSets(vector<CRef<CSearchResultSet> > & t, unsigned int num_of_threads)
{
	CRef<CSearchResultSet>   aggregate_search_result_set (new CSearchResultSet());
	aggregate_search_result_set->clear();

	for(unsigned int i=0; i < t[0]->GetNumQueries(); i++)
	{
		vector< CRef<CSearchResults> >  thread_results;
		thread_results.push_back (CRef<CSearchResults> (&((*(t[0]))[i])));
		const CSeq_id & id = *(thread_results[0]->GetSeqId());

		for(unsigned int d=1; d < num_of_threads; d++)
		{
			thread_results.push_back ((*(t[d]))[id]);
		}

		CRef<CSeq_align_set>  align_set(new CSeq_align_set);
		TQueryMessages aggregate_messages;
		for(unsigned int d=0; d< num_of_threads; d++)
		{
			if(thread_results[d]->HasAlignments())
			{
				CConstRef<CSeq_align_set>  thread_align_set = thread_results[d]->GetSeqAlign();
				if(align_set->IsEmpty())
				{
					align_set->Set().insert(align_set->Set().begin(),
										thread_align_set->Get().begin(),
										thread_align_set->Get().end());
				}
				else
				{
					s_MergeAlignSet(*align_set, *thread_align_set);
				}
			}
			aggregate_messages.Combine(thread_results[d]->GetErrors());
		}

		TMaskedQueryRegions  query_mask;
		thread_results[0]->GetMaskedQueryRegions(query_mask);
		CRef<CSearchResults> aggregate_search_results (new CSearchResults(thread_results[0]->GetSeqId(),
														   	   	   	   	  align_set,
														   	   	   	   	  aggregate_messages,
														   	   	   	   	  thread_results[0]->GetAncillaryData(),
														   	   	   	   	  &query_mask,
														   	   	   	   	  thread_results[0]->GetRID()));
		aggregate_search_result_set->push_back(aggregate_search_results);

	}

	return aggregate_search_result_set;

}

static void s_ModifyVolumePaths(vector<string> & rps_database)
{
	for(unsigned int i=0; i < rps_database.size(); i++)
	{
		size_t found = rps_database[i].find(".pal");
		if(string::npos != found)
			rps_database[i]= rps_database[i].substr(0, found);
	}
}

static bool s_SortDbSize(const pair<string, Int8> & a, const pair<string, Int8>  & b)
{
	return(a.second > b.second);
}

static void s_MapDbToThread(vector<string> & db, unsigned int num_of_threads)
{
	unsigned int db_size = db.size();
	vector <pair <string, Int8> > 	 p;

	for(unsigned int i=0; i < db_size; i++)
	{
		vector<string>	path;
		CSeqDB::FindVolumePaths(db[i], CSeqDB::eProtein, path, NULL, true);
		_ASSERT(path.size() == 1);
		CFile f(path[0]+".loo");
		Int8 length = f.GetLength();
		_ASSERT(length > 0 );
		//Scale down, just in case
		p.push_back(make_pair(db[i], length/1000));
	}

	sort(p.begin(), p.end(),s_SortDbSize);

	db.resize(num_of_threads);
	vector<Int8> acc_size(num_of_threads, 0);

	for(unsigned char i=0; i < num_of_threads; i++)
	{
		db[i] = p[i].first;
		acc_size[i] = p[i].second;
	}

	for(unsigned int i= num_of_threads; i < db_size; i++)
	{
		unsigned int min_index = 0;
		for(unsigned int j=1; j<num_of_threads; j++)
		{
			if(acc_size[j] < acc_size[min_index])
				min_index = j;
		}

		acc_size[min_index] += p[i].second;
		db[min_index] = db[min_index] + delimiter + p[i].first;
	}

}

CRef<CSearchResultSet> s_RunLocalRpsSearch(const string & db,
										   CBlastQueryVector  & query_vector,
										   CRef<CBlastOptionsHandle> opt_handle)
{
	CSearchDatabase			search_db(db, CSearchDatabase::eBlastDbIsProtein);
	CRef<CLocalDbAdapter> 	db_adapter(new CLocalDbAdapter(search_db));
	CRef<IQueryFactory> 	queries(new CObjMgr_QueryFactory(query_vector));

    CLocalBlast lcl_blast(queries, opt_handle, db_adapter);
    CRef<CSearchResultSet> results = lcl_blast.Run();

    return results;
}


class CRPSThread : public CThread
{
public:
	CRPSThread(CRef<CBlastQueryVector> query_vector,
			   const string & db,
	           CRef<CBlastOptions> options);

	void * Main(void);

private:
	CRef<CSearchResultSet>  RunTandemSearches(void);

	CRPSThread(const CRPSThread &);
	CRPSThread & operator=(const CRPSThread &);

    vector<string> 				m_db;
    CRef<CBlastOptionsHandle>	m_opt_handle;
    CRef<CBlastQueryVector>		m_query_vector;
};

/* CRPSThread */

CRPSThread::CRPSThread(CRef<CBlastQueryVector>  query_vector,
		   	   	       const string & db,
		   	   	       CRef<CBlastOptions>  options):
		   	   	       m_query_vector(query_vector)

{
	m_opt_handle.Reset(new CBlastRPSOptionsHandle(options));

    s_ConvertConcatStringToVectorOfString(db, m_db);
}

void* CRPSThread::Main(void)
{
	CRef<CSearchResultSet> * result = new (CRef<CSearchResultSet>);
	if(m_db.size() == 1)
	{
		*result = s_RunLocalRpsSearch(m_db[0],
				 	 	 	 	 	  *m_query_vector,
				 	 	 	 	 	   m_opt_handle);
	}
	else
	{
		*result = RunTandemSearches();
	}
	return result;

}

CRef<CSearchResultSet> CRPSThread::RunTandemSearches(void)
{
	unsigned int num_of_db = m_db.size();
	vector<CRef<CSearchResultSet> > results;

	for(unsigned int i=0; i < num_of_db; i++)
	{
		results.push_back(s_RunLocalRpsSearch(m_db[i],
											  *m_query_vector,
											   m_opt_handle));
	}

	return s_CombineSearchSets(results, num_of_db);
}

/* CThreadedRpsBlast */
CLocalRPSBlast::CLocalRPSBlast(CRef<CBlastQueryVector> query_vector,
              	  	  	  	   const string & db,
              	  	  	  	   CRef<CBlastOptionsHandle> options,
              	  	  	  	   unsigned int num_of_threads):
              	  	  	  	   m_num_of_threads(num_of_threads),
              	  	  	  	   m_db_name(db),
              	  	  	  	   m_opt_handle(options),
              	  	  	  	   m_query_vector(query_vector),
              	  	  	  	   m_num_of_dbs(0)
{
	CSeqDB::FindVolumePaths(db, CSeqDB::eProtein, m_rps_databases, NULL, false);
	m_num_of_dbs = m_rps_databases.size();
	if( 1 == m_num_of_dbs)
	{
		m_num_of_threads = kDisableThreadedSearch;
	}
}

void CLocalRPSBlast::x_AdjustDbSize(void)
{
	if(m_opt_handle->GetOptions().GetEffectiveSearchSpace()!= 0)
		return;

	if(m_opt_handle->GetOptions().GetDbLength()!= 0)
		return;

	CSeqDB db(m_db_name, CSeqDB::eProtein);

	Uint8 db_size = db.GetTotalLengthStats();
	int num_seq = db.GetNumSeqsStats();

	if(0 == db_size)
	    db_size = db.GetTotalLength();
	    
	if(0 == num_seq)
	    num_seq = db.GetNumSeqs();

	m_opt_handle->SetOptions().SetDbLength(db_size);
	m_opt_handle->SetOptions().SetDbSeqNum(num_seq);

	return;
}

CRef<CSearchResultSet> CLocalRPSBlast::Run(void)
{
	if(1 != m_num_of_dbs)
	{
		x_AdjustDbSize();
	}

	if(kDisableThreadedSearch == m_num_of_threads)
	{
		if(1 == m_num_of_dbs)
		{
			return s_RunLocalRpsSearch(m_db_name, *m_query_vector, m_opt_handle);
		}
		else
		{
		   	s_ModifyVolumePaths(m_rps_databases);

			vector<CRef<CSearchResultSet> >   results;
			for(unsigned int i=0; i < m_num_of_dbs; i++)
			{
				results.push_back(s_RunLocalRpsSearch(m_rps_databases[i],
													  *m_query_vector,
													   m_opt_handle));

			}
			return s_CombineSearchSets(results, m_num_of_dbs);
		}

	}
	else
	{
		return RunThreadedSearch();
	}
}

CRef<CSearchResultSet> CLocalRPSBlast::RunThreadedSearch(void)
{

   	s_ModifyVolumePaths(m_rps_databases);

   	if((kAutoThreadedSearch == m_num_of_threads) ||
   	  (m_num_of_threads > m_rps_databases.size()))
   	{
   		//Default num of thread : a thread for each db
   		m_num_of_threads = m_rps_databases.size();
   	}
   	else if(m_num_of_threads < m_rps_databases.size())
   	{
   		// Combine databases, modified the size of rps_database
   		s_MapDbToThread(m_rps_databases, m_num_of_threads);
   	}

   	vector<CRef<CSearchResultSet> * > 	thread_results(m_num_of_threads, NULL);
   	vector <CRPSThread* >				thread(m_num_of_threads, NULL);
   	vector<CRef<CSearchResultSet> >   results;

   	for(unsigned int t=0; t < m_num_of_threads; t++)
   	{
   		// CThread destructor is protected, all threads destory themselves when terminated
   		thread[t] = (new CRPSThread(m_query_vector, m_rps_databases[t], m_opt_handle->SetOptions().Clone()));
   		thread[t]->Run();
   	}

   	for(unsigned int t=0; t < m_num_of_threads; t++)
   	{
   		thread[t]->Join(reinterpret_cast<void**> (&thread_results[t]));
   	}

   	for(unsigned int t=0; t < m_num_of_threads; t++)
   	{
   		results.push_back(*(thread_results[t]));
   	}

   	CRef<CBlastRPSInfo>  rpsInfo = CSetupFactory::CreateRpsStructures(m_db_name,
   	            												CRef<CBlastOptions> (&(m_opt_handle->SetOptions())));
   	return s_CombineSearchSets(results, m_num_of_threads);

}



END_SCOPE(blast)
END_NCBI_SCOPE
