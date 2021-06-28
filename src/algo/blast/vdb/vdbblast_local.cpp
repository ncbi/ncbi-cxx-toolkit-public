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

/** @file vdbblast_local.cpp
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/psiblast.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiexpt.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <internal/blast/vdb/vdb2blast_util.hpp>
#include <internal/blast/vdb/vdbblast_local.hpp>
#ifdef _OPENMP
#include <omp.h>
#endif
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

const string k_NOT_CSRA_DB("NOT_CSRA");
const string k_CSRA_CHUNK("CSRA_CHUNK: ");

static void s_MergeAlignSet(CSeq_align_set & final_set, const CSeq_align_set & input_set, const int list_size)
{
	CSeq_align_set::Tdata & final_list = final_set.Set();
	const CSeq_align_set::Tdata & input_list = input_set.Get();

	CSeq_align_set::Tdata::const_iterator	input_it = input_list.begin();
	CSeq_align_set::Tdata::iterator		final_it = final_list.begin();
	int hit_count = 0;
	while(input_it != input_list.end())
	{
		double final_evalue;
		double input_evalue;

		if(hit_count >= list_size)
		{
			final_list.erase(final_it, final_list.end());
			break;
		}

		if(final_it == final_list.end())
		{
			// Want to keep append value to list until hit list is full or input list reaches the end
			final_evalue = (double) kMax_UI8;
		}
		else
		{
			(*final_it)->GetNamedScore(CSeq_align::eScore_EValue, final_evalue);
		}
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
		}
		hit_count ++;
	}

}


static CRef<CSearchResultSet>  s_CombineSearchSets(vector<CRef<CSearchResultSet> > & t, unsigned int num_of_threads, const int list_size)
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
					s_MergeAlignSet(*align_set, *thread_align_set, list_size);
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
														   	   	   	   	  &query_mask));
		aggregate_search_result_set->push_back(aggregate_search_results);

	}

	return aggregate_search_result_set;

}

CRef<CSearchResultSet> s_RunLocalVDBSearch(const string & dbs,
		                                   CRef<IQueryFactory> query_factory,
										   CRef<CBlastOptionsHandle> opt_handle, Int4 & num_extensions)
{
	bool isCSRA = false;
	string csras = kEmptyStr;
	if(dbs.compare(0, k_CSRA_CHUNK.size(), k_CSRA_CHUNK) == 0) {
		isCSRA = true;
		csras = dbs.substr(k_CSRA_CHUNK.size());
	}

    //CRef<CVDBBlastUtil> vdbUtil(new CVDBBlastUtil(isCSRA?csras:dbs, true, isCSRA));
    CVDBBlastUtil vdbUtil(isCSRA?csras:dbs, true, isCSRA);

    BlastSeqSrc* seqSrc = vdbUtil.GetSRASeqSrc();
    CRef<IBlastSeqInfoSrc> seqInfoSrc = vdbUtil.GetSRASeqInfoSrc();

	CLocalBlast lcl_blast(query_factory, opt_handle, seqSrc, seqInfoSrc);
	lcl_blast.SetNumberOfThreads(CThreadable::kMinNumThreads);
	CRef<CSearchResultSet> results;
	try
	{
		 results = lcl_blast.Run();
	}
	catch (const CBlastException& e) {
	        cerr << "BLAST engine error: " << e.what() << endl;
	    	// Temporary fix to avoid vdb core dump during cleanup SB-1170
	    	abort();
	}
    num_extensions = lcl_blast.GetNumExtensions();
    return results;
}

CRef<CSearchResultSet> s_RunPsiVDBSearch(const string & dbs,
		                                   CRef<CPssmWithParameters> pssm,
										   CRef<CBlastOptionsHandle> opt_handle)
{
	bool isCSRA = false;
	string csras = kEmptyStr;
	if(dbs.compare(0, k_CSRA_CHUNK.size(), k_CSRA_CHUNK) == 0) {
		isCSRA = true;
		csras = dbs.substr(k_CSRA_CHUNK.size());
	}

    CRef<CVDBBlastUtil> vdbUtil(new CVDBBlastUtil(isCSRA?csras:dbs, false, isCSRA));
	CRef<CSearchResultSet> results;
    BlastSeqSrc* seqSrc = vdbUtil->GetSRASeqSrc();
    CRef<IBlastSeqInfoSrc> seqInfoSrc = vdbUtil->GetSRASeqInfoSrc();
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(seqSrc, seqInfoSrc));
    CConstRef<CPSIBlastOptionsHandle> psi_opts;
    psi_opts.Reset(dynamic_cast <CPSIBlastOptionsHandle *> (&*opt_handle));

	CPsiBlast psi_blast(pssm, db_adapter, psi_opts);
	psi_blast.SetNumberOfThreads(CThreadable::kMinNumThreads);
	try
	{
		 results = psi_blast.Run();
	}
	catch (const CBlastException& e) {
	        cerr << "BLAST engine error: " << e.what() << endl;
	    	// Temporary fix to avoid vdb core dump during cleanup SB-1170
	    	abort();
	}
    return results;
}

struct SVDBThreadResult
{
	CRef<CSearchResultSet> thread_result_set;
	Int4 num_extensions;
};


class CVDBThread : public CThread
{
public:
	CVDBThread(CRef<IQueryFactory>  query_factory,
			   vector<string> & chunks,
	           CRef<CBlastOptions> options);

	CVDBThread(CRef<CPssmWithParameters> pssm,
	   	       vector<string> & chunks,
	   	       CRef<CBlastOptions>  options);

	void * Main(void);
private:
	CRef<CSearchResultSet>  RunTandemSearches(void);

	CVDBThread(const CVDBThread &);
	CVDBThread & operator=(const CVDBThread &);

    CRef<IQueryFactory>			m_query_factory;
    vector<string> 				m_chunks;
    CRef<CBlastOptionsHandle>	m_opt_handle;
    Int4						m_num_extensions;
    CRef<CPssmWithParameters>	m_pssm;
};

/* CVDBThread */
CVDBThread::CVDBThread(CRef<CPssmWithParameters> pssm,
		   	   	       vector<string> & chunks,
		   	   	       CRef<CBlastOptions>  options):
		   	   	       m_chunks(chunks), m_num_extensions(0), m_pssm(pssm)

{
	m_opt_handle.Reset(new  CPSIBlastOptionsHandle(options));
}


CVDBThread::CVDBThread(CRef<IQueryFactory>  query_factory,
		   	   	       vector<string> & chunks,
		   	   	       CRef<CBlastOptions>  options):
		   	   	       m_query_factory(query_factory), m_chunks(chunks), m_num_extensions(0)

{
	if(options->GetProgramType() == eBlastTypeBlastn)
		m_opt_handle.Reset(new CBlastNucleotideOptionsHandle(options));
	else
		m_opt_handle.Reset(new CTBlastnOptionsHandle(options));
	//m_opt_handle->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
}

void* CVDBThread::Main(void)
{
	SVDBThreadResult * result = new (SVDBThreadResult);
	if(m_chunks.size() == 1) {
		if(m_pssm.Empty()) {
			result->thread_result_set = s_RunLocalVDBSearch(m_chunks[0], m_query_factory,
				 	 	 	 	 	                        m_opt_handle, m_num_extensions);
		}
		else {
			result->thread_result_set = s_RunPsiVDBSearch(m_chunks[0], m_pssm, m_opt_handle);
		}
	}
	else {
		result->thread_result_set = RunTandemSearches();
	}

	result->num_extensions = m_num_extensions;
	return result;

}

CRef<CSearchResultSet> CVDBThread::RunTandemSearches(void)
{
	unsigned int num_of_chunks = m_chunks.size();
	vector<CRef<CSearchResultSet> > results;

	for(unsigned int i=0; i < num_of_chunks; i++) {
		Int4 num_exts = 0;
		if(m_pssm.Empty()){
			results.push_back(s_RunLocalVDBSearch(m_chunks[i], m_query_factory,
					                              m_opt_handle, num_exts));
		}
		else {
			results.push_back(s_RunPsiVDBSearch(m_chunks[i], m_pssm, m_opt_handle));
		}
		m_num_extensions +=num_exts;
	}

	return s_CombineSearchSets(results, num_of_chunks,m_opt_handle->GetOptions().GetHitlistSize());
}





/* CLocalVDBBlast */
CLocalVDBBlast::CLocalVDBBlast(CRef<CBlastQueryVector> query_vector,
              	  	  	  	   CRef<CBlastOptionsHandle> options,
              	  	  	  	   SLocalVDBStruct & local_vdb):
              	  	  	  	   m_query_vector(query_vector),
              	  	  	  	   m_opt_handle(options),
              	  	  	  	   m_total_num_seqs(local_vdb.total_num_seqs),
              	  	  	  	   m_total_length(local_vdb.total_length),
							   m_chunks_for_thread(local_vdb.chunks_for_thread),
							   m_num_threads(local_vdb.chunks_for_thread.size()),
							   m_num_extensions(0)
{
}

/* CLocalVDBBlast */
CLocalVDBBlast::CLocalVDBBlast(CRef<objects::CPssmWithParameters> pssm,
              	  	  	  	   CRef<CBlastOptionsHandle> options,
              	  	  	  	   SLocalVDBStruct & local_vdb):
              	  	  	  	   m_opt_handle(options),
              	  	  	  	   m_total_num_seqs(local_vdb.total_num_seqs),
              	  	  	  	   m_total_length(local_vdb.total_length),
							   m_chunks_for_thread(local_vdb.chunks_for_thread),
							   m_num_threads(local_vdb.chunks_for_thread.size()),
							   m_num_extensions(0),
							   m_pssm(pssm)
{
}


struct SSortStruct
{
	SSortStruct():db_name(kEmptyStr),num_seqs(0),length(0) {}
	string db_name;
	Uint8 num_seqs;
	Uint8 length;
};
static bool s_SortDbSize(const SSortStruct & a, const SSortStruct  & b)
{
	return(a.length > b.length);
}

static void
s_DivideDBsForThread(unsigned int num_threads,  vector<SSortStruct> & in_list,
		             vector<vector<SSortStruct> > & out_list, vector<Uint8> & acc_size)
{
	sort(in_list.begin(), in_list.end(),s_SortDbSize);

	for(unsigned int i=0; i < in_list.size(); i++)
	{
		unsigned int min_index = 0;
		for(unsigned int j=1; j<num_threads; j++) {
			if(acc_size[j] < acc_size[min_index])
				min_index = j;
		}
		acc_size[min_index] += in_list[i].length;
		out_list[min_index].push_back(in_list[i]);
	}
}

static void
s_RemoveNonCSRAEntry(vector<SSortStruct> & in_list)
{
	vector<SSortStruct>  filtered_list;
	for(unsigned int i; i < in_list.size(); i++) {
		if(in_list[i].db_name == k_NOT_CSRA_DB)
			continue;

		filtered_list.push_back(in_list[i]);
 	}
	in_list.swap(filtered_list);
}


static void s_GetChunksForThread(vector<SSortStruct> & in_list, vector<string> & chunks,
		                         const unsigned int dbs_per_chunk, const string tag)
{
	if(in_list.size() == 0)
		return;

	Uint8 num_seqs_count = 0;
	string dbs = tag;
	unsigned int db_count = 0;
	for(unsigned int i=0; i < in_list.size(); i++) {
		if((in_list[i].db_name != kEmptyStr) && (in_list[i].num_seqs != 0)) {
			num_seqs_count += in_list[i].num_seqs;
			if(num_seqs_count > (Uint8)kMax_I4 || db_count >= dbs_per_chunk) {
				chunks.push_back(dbs);
				_TRACE("Chunk: " << dbs << " Num Seqs: " << num_seqs_count -in_list[i].num_seqs);
				dbs = tag + in_list[i].db_name;
				num_seqs_count = in_list[i].num_seqs;
				db_count =1;
			}
			else {
				dbs +=in_list[i].db_name;
				db_count ++;
			}
			dbs +=" ";
		}
	}
	if(dbs != tag) {
		chunks.push_back(dbs);
		_TRACE("Chunk: " << dbs << " Num Seqs: " << num_seqs_count);
	}
}

static const unsigned int DEFAULT_MAX_DBS_OPEN = 8000;
static const unsigned int DEFAULT_MAX_DBS_PER_CHUNK = 1500;

static unsigned int s_GetNumDbsPerChunk(unsigned int num_threads, unsigned int num_dbs)
{
	unsigned int dbs_per_chunk = min(DEFAULT_MAX_DBS_OPEN/num_threads, num_dbs/num_threads);

	CNcbiEnvironment env;
	string  max_dbs_env = env.Get("VDB_MAX_DBS_PER_CHUNK");
	unsigned int max_dbs_per_chunk = DEFAULT_MAX_DBS_PER_CHUNK;
	if(max_dbs_env != kEmptyStr) {
		max_dbs_per_chunk = min(max_dbs_per_chunk, NStr::StringToUInt(max_dbs_env));
	}

	if(max_dbs_per_chunk && max_dbs_per_chunk < dbs_per_chunk)
		dbs_per_chunk = max_dbs_per_chunk;

	return dbs_per_chunk;

}

string  CLocalVDBBlast::PreprocessDBs(SLocalVDBStruct & local_vdb, const string db_names, unsigned int threads,
		                              ESRASearchMode  search_mode)
{
	vector<string> dbs;
	NStr::Split(db_names, " ", dbs);
	std::sort(dbs.begin(), dbs.end());
	vector<string>::iterator uq = std::unique(dbs.begin(), dbs.end());
	dbs.erase(uq, dbs.end());

	unsigned int num_dbs = dbs.size();
	unsigned int num_threads = (num_dbs < threads) ? num_dbs : threads;

	vector <SSortStruct> p, r;
	Uint8 total_length = 0;
	Uint8 total_num_seqs = 0;
	string openmp_exception = kEmptyStr;

	//CStopWatch sw(CStopWatch::eStart);
	if(search_mode == eUnaligned) {
		p.resize(num_dbs);
		#pragma omp parallel for num_threads(num_threads) schedule(static) if (num_threads > 1) \
			shared(num_dbs, p, dbs) reduction(+ : total_length, total_num_seqs)
		for(unsigned int i=0; i < num_dbs; i++) {
			try {
				p[i].db_name = dbs[i];
				CVDBBlastUtil::GetVDBStats(p[i].db_name, p[i].num_seqs, p[i].length);
				if(p[i].num_seqs > (Uint8)kMax_I4) {
					NCBI_THROW(CException, eUnknown, p[i].db_name + " has more than 2 billions seqs, exceeds max num of seqs supported");
				}
				total_length += p[i].length;
				total_num_seqs += p[i].num_seqs;
			} catch (CException & e) {
				#pragma omp critical
				openmp_exception += e.what();
			}
		}
	}
	else if(search_mode == eBoth) {
		p.resize(num_dbs);
		r.resize(num_dbs);
	    #pragma omp parallel for num_threads(num_threads) schedule(static) if (num_threads > 1) \
		        shared(num_dbs, p,r, dbs) reduction(+ : total_length, total_num_seqs)
		for(unsigned int i=0; i < num_dbs; i++) {
			try {
				p[i].db_name = dbs[i];
				if(CVDBBlastUtil::IsCSRA(dbs[i])) {
					r[i].db_name = dbs[i];
					CVDBBlastUtil::GetAllStats(p[i].db_name, p[i].num_seqs, p[i].length, r[i].num_seqs, r[i].length);
				}
				else {
					CVDBBlastUtil::GetVDBStats(p[i].db_name, p[i].num_seqs, p[i].length);
					r[i].db_name = k_NOT_CSRA_DB;
				}
				if((p[i].num_seqs > (Uint8)kMax_I4) || (r[i].num_seqs > (Uint8)kMax_I4)) {
						NCBI_THROW(CException, eUnknown, dbs[i] + " has more than 2 billions seqs, exceeds max num of seqs supported");
				}
				total_length += (p[i].length + r[i].length);
				total_num_seqs += (p[i].num_seqs + r[i].num_seqs);
			} catch (CException & e) {
				#pragma omp critical
				openmp_exception += e.what();
			}
		}
	}
	else if (search_mode == eAligned) {
		r.resize(num_dbs);
		#pragma omp parallel for num_threads(num_threads) schedule(static) if (num_threads > 1) \
			    shared(num_dbs, r, dbs) reduction(+ : total_length, total_num_seqs)
		for(unsigned int i=0; i < num_dbs; i++) {
			try {
				if(CVDBBlastUtil::IsCSRA(dbs[i])) {
					r[i].db_name = dbs[i];
					CVDBBlastUtil::GetVDBStats(r[i].db_name, r[i].num_seqs, r[i].length, true);
					if(r[i].num_seqs > (Uint8)kMax_I4) {
						NCBI_THROW(CException, eUnknown, dbs[i] + " has more than 2 billions seqs, exceeds max num of seqs supported");
					}
					total_length += r[i].length;
					total_num_seqs += r[i].num_seqs;
				}
				else {
					r[i].db_name = k_NOT_CSRA_DB;
				}
			} catch (CException & e) {
				#pragma omp critical
				openmp_exception += e.what();
			}
		}
	}
	else {
		NCBI_THROW(CException, eUnknown, " Invalid Search Mode");
	}

    if(openmp_exception != kEmptyStr) {
		NCBI_THROW(CException, eUnknown, openmp_exception);
    }
	//cerr << "Time to process dbs : " << sw.Elapsed() << endl;

    if(search_mode != eUnaligned){
    	s_RemoveNonCSRAEntry(r);
    	num_dbs = r.size() + p.size();
    	if(r.size() == 0) {
    		ERR_POST(Warning << "No CSRA db found.");
    	}
    	else {
    		Uint4 max_csra_thread = CVDBBlastUtil::GetMaxNumCSRAThread();
    		if(max_csra_thread != 0 && threads > max_csra_thread) {
    			threads = max_csra_thread;
    		}
    		num_threads = (num_dbs < threads) ? num_dbs : threads;
    	}
    }

	local_vdb.total_length = total_length;

	if(total_num_seqs > (Uint8) kMax_I4) {
		local_vdb.total_num_seqs = kMax_I4;
	}
	else if(total_num_seqs == 0){
		local_vdb.total_num_seqs = 0;
		string zero_seq_err = "DB list contains no searchable seqs in sra_mode " + NStr::IntToString(search_mode) +".";
		NCBI_THROW(CException, eUnknown, zero_seq_err);

	}
	else {
		local_vdb.total_num_seqs = total_num_seqs;
	}

	vector<Uint8> acc_size(num_threads, 0);
	local_vdb.chunks_for_thread.resize(num_threads);
	unsigned int dbs_per_chunk = s_GetNumDbsPerChunk(num_threads, num_dbs);

	if(search_mode  !=  eAligned){
		vector<vector<SSortStruct> > list_thread(num_threads);
		if(num_threads != kDisableThreadedSearch) {
			s_DivideDBsForThread(num_threads,  p, list_thread, acc_size);
		}
		else {
			list_thread[0]= p;
		}
		for(unsigned int t=0; t < num_threads; t++) {
			s_GetChunksForThread(list_thread[t], local_vdb.chunks_for_thread[t], dbs_per_chunk, kEmptyStr);
		}
	}

	if((search_mode  != eUnaligned)  && (r.size() > 0)){
		vector<vector<SSortStruct> > list_thread(num_threads);
		if(num_threads != kDisableThreadedSearch) {
			s_DivideDBsForThread(num_threads,  r, list_thread, acc_size);
		}
		else {
			list_thread[0]= r;
		}
		for(unsigned int t=0; t < num_threads; t++) {
			s_GetChunksForThread(list_thread[t], local_vdb.chunks_for_thread[t], dbs_per_chunk, k_CSRA_CHUNK);
		}
	}

	return NStr::Join (dbs, " ");
}

void CLocalVDBBlast::x_AdjustDbSize(void)
{
	if(m_num_threads == kDisableThreadedSearch)
	{
		if(m_chunks_for_thread[0].size() == 1)
			return;
	}

	if(m_opt_handle->GetOptions().GetEffectiveSearchSpace()!= 0)
		return;

	if(m_opt_handle->GetOptions().GetDbLength()!= 0)
		return;

	m_opt_handle->SetOptions().SetDbLength(m_total_length);
	m_opt_handle->SetOptions().SetDbSeqNum(m_total_num_seqs);

	return;
}

void s_TrimResults(CSearchResultSet & rs, int hit_list_size)
{
	for(unsigned int i=0; i < rs.size(); i++)
	{
		rs[i].TrimSeqAlign(hit_list_size);
	}
}

static int s_GetModifiedHitlistSize(const int  orig_size)
{
	if(orig_size <= 200)
	{
		return (orig_size + 100);
	}
	else if(orig_size < 500)
	{
		return (orig_size + 75);
	}

	return (orig_size + 50);

}

CRef<CSearchResultSet> CLocalVDBBlast::Run(void)
{
	x_AdjustDbSize();

	if(kDisableThreadedSearch == m_num_threads)
	{
		// For 2na seq, the start and end of a seq are not byte align,
		// making the hit list size bigger than normal to compensate
		// for match and mismatch of the "unused" bases which could
		// bump a better hit off the list
		int hls = m_opt_handle->GetOptions().GetHitlistSize() ;
		m_opt_handle->SetOptions().SetHitlistSize(s_GetModifiedHitlistSize(hls));
		vector<string> & chunks=m_chunks_for_thread[0];
		unsigned int num_chunks = m_chunks_for_thread[0].size();

		if(num_chunks == 1) {
			//m_opt_handle->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
			CRef<CSearchResultSet> retval;
			if(m_pssm.Empty()){
				CRef<IQueryFactory> 	queries(new CObjMgr_QueryFactory(*m_query_vector));
				retval = s_RunLocalVDBSearch(chunks[0], queries, m_opt_handle, m_num_extensions);
			}
			else {
				retval = s_RunPsiVDBSearch(chunks[0], m_pssm, m_opt_handle);
			}
			m_opt_handle->SetOptions().SetHitlistSize(hls);
			s_TrimResults(*retval, hls);
			return retval;
		}
		else
		{
			vector<CRef<CSearchResultSet> >   results;
			for(unsigned int i=0; i < num_chunks; i++)
			{
				if(m_pssm.Empty()){
					Int4 num_exts = 0;
					CRef<IQueryFactory> 	queries(new CObjMgr_QueryFactory(*m_query_vector));
					results.push_back(s_RunLocalVDBSearch(chunks[i], queries, m_opt_handle, num_exts));
					m_num_extensions += num_exts;
				}
				else {
					results.push_back(s_RunPsiVDBSearch(chunks[i], m_pssm, m_opt_handle));
				}
			}

			m_opt_handle->SetOptions().SetHitlistSize(hls);
			CRef<CSearchResultSet> retval =s_CombineSearchSets(results, num_chunks, m_opt_handle->GetOptions().GetHitlistSize());
			s_TrimResults(*retval, hls);
			return retval;
		}
	}
	else
	{
		return RunThreadedSearch();
	}
}

void CLocalVDBBlast::x_PreparePssm(vector<CRef<CPssmWithParameters> > & pssm)
{
	pssm.clear();
	pssm.resize(m_num_threads);
	pssm[0] = m_pssm;
	for(unsigned int v=1; v < m_num_threads; v++) {
		CNcbiStrstream oss;
		oss << MSerial_AsnBinary << *m_pssm;
		pssm[v].Reset(new CPssmWithParameters);
		 oss >> MSerial_AsnBinary >> *(pssm[v]);
	}
}

CRef<CBlastQueryVector> s_CloneBlastQueryVector(const CBlastQueryVector & orig )
{
	CRef<CBlastQueryVector> clone(new CBlastQueryVector());
	for(unsigned int i=0; i < orig.Size(); i++)
	{
		CRef<CBlastSearchQuery> q(new CBlastSearchQuery(*(orig[i]->GetQuerySeqLoc()), *(orig[i]->GetScope())));
		q->SetMaskedRegions(orig[i]->GetMaskedRegions());
		q->SetGeneticCodeId(orig[i]->GetGeneticCodeId());
		clone->push_back(q);
	}
	return clone;
}

void CLocalVDBBlast::x_PrepareQuery(vector<CRef<IQueryFactory> > & qf_v)
{

	vector<CRef<CBlastQueryVector> >  query_v(m_num_threads);
	query_v[0] = m_query_vector;
	for(unsigned int v=1; v < m_num_threads; v++)
	{
		query_v[v]= s_CloneBlastQueryVector(*m_query_vector);
	}

	qf_v.resize(m_num_threads);
	for(unsigned int v=0; v < m_num_threads; v++)
	{
		CRef<IQueryFactory> 	queries(new CObjMgr_QueryFactory(*m_query_vector));
		CRef<ILocalQueryData> query_data = queries->MakeLocalQueryData(&(m_opt_handle->GetOptions()));
		query_data->GetSequenceBlk();
		qf_v[v] = queries;
	}
}

CRef<CSearchResultSet> CLocalVDBBlast::RunThreadedSearch(void)
{
   	vector<SVDBThreadResult *>			thread_results(m_num_threads, NULL);
   	vector <CVDBThread* >				thread(m_num_threads, NULL);
   	vector<CRef<CSearchResultSet> >   	results;
   	vector<CRef<IQueryFactory> >		query_factory;
   	vector<CRef<CPssmWithParameters> >  pssm;
   	bool isPSI = m_pssm.NotEmpty();

   	if(isPSI) {
   		pssm.resize(m_num_threads);
   		x_PreparePssm(pssm);
   	}
   	else {
   		query_factory.resize(m_num_threads);
   		x_PrepareQuery(query_factory);
   	}

  	for(unsigned int t=0; t < m_num_threads; t++)
   	{
   		// CThread destructor is protected, all threads destory themselves when terminated
  		if (isPSI) {
  			thread[t] = (new CVDBThread(pssm[t], m_chunks_for_thread[t], m_opt_handle->SetOptions().Clone()));
  		}
  		else {
  			thread[t] = (new CVDBThread(query_factory[t], m_chunks_for_thread[t], m_opt_handle->SetOptions().Clone()));
  		}
   		thread[t]->Run();
   	}

   	for(unsigned int t=0; t < m_num_threads; t++)
   	{
   		thread[t]->Join(reinterpret_cast<void**> (&thread_results[t]));
   	}

   	for(unsigned int t=0; t < m_num_threads; t++)
   	{
   		if(thread_results[t] == NULL) {
   			NCBI_THROW(CException, eUnknown, "Search Error");
   		}
   		m_num_extensions += thread_results[t]->num_extensions;
   		results.push_back(thread_results[t]->thread_result_set);
   		delete (thread_results[t]);
   	}

   	return s_CombineSearchSets(results, m_num_threads, m_opt_handle->GetOptions().GetHitlistSize());

}

Int4 CLocalVDBBlast::GetNumExtensions()
{
	return m_num_extensions;
}

END_SCOPE(blast)
END_NCBI_SCOPE
