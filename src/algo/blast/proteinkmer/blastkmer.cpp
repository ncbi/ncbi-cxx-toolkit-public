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
 *   BLAST-kmer searches
 *
 */

#include <ncbi_pch.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objmgr/seq_vector.hpp>
#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerutils.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


CBlastKmer::CBlastKmer(TSeqLocVector& query_vector,
                CRef<CBlastKmerOptions> options,
                CRef<CSeqDB> seqdb,
                string kmerfile)
 : m_QueryVector(query_vector),
   m_Opts (options),
   m_SeqDB(seqdb),
   m_GIList(NULL)
{
	if (kmerfile != "")
		m_KmerFiles.push_back(kmerfile);
	else
		seqdb->FindVolumePaths(m_KmerFiles, false);

	if(options->Validate() == false)
		NCBI_THROW(CException, eUnknown, "ERROR: kmer options validation failed");

}

CBlastKmer::CBlastKmer(SSeqLoc& query,
                CRef<CBlastKmerOptions> options,
                const string& dbname)
 : m_Opts (options),
   m_GIList(NULL)
{
	m_QueryVector.push_back(query);

	m_SeqDB = new CSeqDB(dbname, CSeqDB::eProtein);

	m_SeqDB->FindVolumePaths(m_KmerFiles, false);

	if(options->Validate() == false)
		NCBI_THROW(CException, eUnknown, "ERROR: kmer options validation failed");
}

void 
CBlastKmer::x_ProcessQuery(const string& query_seq, SOneBlastKmerSearch& kmerSearch, const SBlastKmerParameters& kmerParams, uint32_t *a, uint32_t *b, vector < vector<int> >& kvector, vector<int> badMers)
{
	
	int num_bands = kmerParams.numHashes/kmerParams.rowsPerBand;

	int do_seg=0; // FIXME

        bool kmerFound = false; 
	if (kmerParams.version < 3)
		kmerFound = minhash_query(query_seq, kmerSearch.queryHash, kmerParams.numHashes, a, b, do_seg, kmerParams.kmerNum, kmerParams.alphabetChoice, kmerParams.chunkSize);
	else
		kmerFound = minhash_query2(query_seq, kmerSearch.queryHash, kmerParams.kmerNum, 
			kmerParams.numHashes, kmerParams.alphabetChoice, badMers, kmerParams.chunkSize);

	if (!kmerFound)
		NCBI_THROW(CException, eUnknown, "WARNING: No KMERs in query");

	if (kmerParams.version < 2)
		get_LSH_hashes(kmerSearch.queryHash, kmerSearch.queryLSHHash, num_bands, kmerParams.rowsPerBand);
	else  if (kmerParams.version == 2) // 2 and 30 should be variables
		get_LSH_hashes2(kmerSearch.queryHash, kmerSearch.queryLSHHash, kmerParams.rowsPerBand, kmerParams.samples, kvector);
	else
		get_LSH_hashes5(kmerSearch.queryHash, kmerSearch.queryLSHHash, kmerParams.numHashes, kmerParams.rowsPerBand);
}

void
CBlastKmer::x_RunKmerFile(const vector < vector <uint32_t> >& query_hash, const vector < vector <uint32_t> >& query_LSH_hash, CMinHashFile& mhfile, TBlastKmerPrelimScoreVector& score_vector, BlastKmerStats& kmer_stats)
{

        int num_hashes = mhfile.GetNumHashes();

	// LSH parameters per http://infolab.stanford.edu/~ullman/mmds/ch3.pdf
	
	// populate LSH tables
	uint64_t* lsh = mhfile.GetLSHArray();
	int kmerVer = mhfile.GetVersion();

	vector< set<uint32_t > > candidates;
	candidates.resize(query_hash.size());
	get_LSH_match_from_hash(query_LSH_hash, lsh, candidates);
	int minHits = m_Opts->GetMinHits();
	if (minHits == 0)
	{ // Choose value based upon alphabet
		if (mhfile.GetAlphabet() == 0) 
			minHits=1;
		else
			minHits=2;
	}

        neighbor_query(query_hash,
                     lsh,
                     candidates,
                     mhfile,
                     num_hashes,
                     minHits,
                     m_Opts->GetThresh(),
		     score_vector,
		     kmer_stats, 
		     kmerVer);

	kmer_stats.num_sequences = mhfile.GetNumSeqs();

	return;
}

static void
s_GetQuerySequence(const TSeqLocVector& query_vector, string& query_seq, CRef<CSeq_id>& seqid, int queryNum)
{
 	query_seq.clear();
	CSeqVector seqvect(*(query_vector[queryNum].seqloc), *(query_vector[queryNum].scope));
	seqvect.SetCoding(CSeq_data::e_Ncbistdaa);
	seqvect.GetSeqData(0, seqvect.size(), query_seq);
	seqid.Reset(new CSeq_id());
	seqid->Assign(*(query_vector[queryNum].seqloc->GetId()));
}

static void
s_AdjustPrelimScoreVectorOID(TBlastKmerPrelimScoreVector& score_vector, int offset)
{
	for(TBlastKmerPrelimScoreVector::iterator iter=score_vector.begin(); iter != score_vector.end(); ++iter)
	{
		(*iter).first += offset;
	}
}

bool 
s_SortFinalResults(const pair<uint32_t, double>& one, const pair<uint32_t, double>& two)
{
	return one.second > two.second;
}

void 
s_GetAllGis(vector<TGi>& retvalue, TBlastKmerPrelimScoreVector results, CRef<CSeqDB> seqdb)
{
	for(TBlastKmerPrelimScoreVector::iterator itr=results.begin(); itr != results.end(); ++itr)
	{
		seqdb->GetGis((*itr).first, retvalue, true);
	}
}

CRef<CBlastKmerResultsSet>
CBlastKmer::x_SearchMultipleQueries(int firstQuery, int numQuery, const SBlastKmerParameters& kmerParams, uint32_t *a, uint32_t *b, vector < vector<int> >& kValues, vector<int> badMers)
{
	TQueryMessages errs;
	int numThreads = (int) GetNumberOfThreads();
	int numFiles = m_KmerFiles.size();
	if (numThreads > numQuery)
		numThreads = numFiles;

	vector<SOneBlastKmerSearch> kmerSearchVector;
	kmerSearchVector.reserve(numQuery);
	for (int i=0; i<numQuery; i++)
	{
		SOneBlastKmerSearch kmerSearch(numFiles);
		try {
			string query_seq; 
			CRef<CSeq_id> qseqid; 
			s_GetQuerySequence(m_QueryVector, query_seq, qseqid, i+firstQuery);
			if (query_seq.length() < static_cast<string::size_type>(kmerParams.kmerNum))
				NCBI_THROW(CException, eUnknown, "WARNING: Query shorter than KMER length");

			kmerSearch.qSeqid = qseqid;
			x_ProcessQuery(query_seq, kmerSearch, kmerParams, a, b, kValues, badMers);
		 } catch (const ncbi::CException& e) {
			kmerSearch.status=1;
			string msg = e.GetMsg();
			kmerSearch.errDescription=msg;
			if (msg.find("WARNING:") != std::string::npos)
				kmerSearch.severity=eBlastSevWarning;
			else
				kmerSearch.severity=eBlastSevError;
		} catch (const std::exception& e) {
			kmerSearch.status=1;
			kmerSearch.errDescription=string(e.what());
			kmerSearch.severity=eBlastSevError;
		} catch (...) {
			kmerSearch.status=1;
			kmerSearch.errDescription=string("Unknown error");
			kmerSearch.severity=eBlastSevError;
		}
		kmerSearchVector.push_back(kmerSearch);
	}

#pragma omp parallel for num_threads(numThreads)
for(int index=0; index<numFiles; index++)
{
	CMinHashFile mhfile(m_KmerFiles[index]);
	for (int i=0; i<numQuery; i++)
	{
		SOneBlastKmerSearch& kmerSearch = kmerSearchVector[i];
		if (kmerSearch.status)
			continue;
		x_RunKmerFile(kmerSearch.queryHash, kmerSearch.queryLSHHash, mhfile, kmerSearch.scoreVector[index], (kmerSearch.kmerStatsVector[index]));
	}
}


        CRef<CBlastKmerResultsSet> kmerResultSet(new CBlastKmerResultsSet()); 
        for (int i=0; i<numQuery; i++)
        {
        	CRef<CBlastKmerResults> kmerResults;
        	SOneBlastKmerSearch& kmerSearch = kmerSearchVector[i];
        	if (kmerSearch.status)
        	{
        		kmerResults.Reset(MakeEmptyResults(m_QueryVector, i+firstQuery, kmerSearch.errDescription, kmerSearch.severity));
        		kmerResultSet->push_back(kmerResults);
        		continue;
        	}
	
        	BlastKmerStats kmer_stats;

        	TBlastKmerPrelimScoreVector final_results;
        	size_t final_size=0;  // Total elements needed for all vectors.
        	for (int index=0; index<numFiles; index++)
        		final_size += kmerSearch.scoreVector[index].size();
        	final_results.reserve(final_size);

        	int offset=0;
        	for (int index=0; index<numFiles; index++)
        	{
        	   TBlastKmerPrelimScoreVector score_vector = kmerSearch.scoreVector[index];
        	   if (kmerParams.version > 1)
        	   {
        	   	s_AdjustPrelimScoreVectorOID(score_vector, offset);
        		offset += kmerSearch.kmerStatsVector[index].num_sequences;
        	   }
        	   final_results.insert(final_results.end(), score_vector.begin(), score_vector.end());
        
                   // Sum the statistics.
	           kmer_stats.hit_count += kmerSearch.kmerStatsVector[index].hit_count;
	           kmer_stats.jd_count += kmerSearch.kmerStatsVector[index].jd_count;
	           kmer_stats.oids_considered += kmerSearch.kmerStatsVector[index].oids_considered;
	           kmer_stats.jd_oid_count += kmerSearch.kmerStatsVector[index].jd_oid_count;
	           kmer_stats.total_matches += kmerSearch.kmerStatsVector[index].total_matches;
	           kmer_stats.num_sequences += kmerSearch.kmerStatsVector[index].num_sequences;
	        }

	        // Sort by score
	        sort(final_results.begin(), final_results.end(), s_SortFinalResults);
	        if (m_Opts->GetNumTargetSeqs() > 0)
	        {
		        int vec_size = final_results.size();
		        int num_matches = m_Opts->GetNumTargetSeqs();
		        if (vec_size > num_matches)
		         final_results.erase(final_results.begin()+num_matches, final_results.end());
	        }

	        CRef<CSeqDB> seqdb;
	        if (m_GIList && !m_GIList.Empty())
	        {
		        vector<TGi> myGis;
		        s_GetAllGis(myGis, final_results, m_SeqDB);
	        	CRef<CSeqDBGiList> intersect( new CIntersectionGiList(*m_GIList, myGis));
           		const string& dbname = m_SeqDB->GetDBNameList();
           		if (intersect->Size() > 0)
           			seqdb.Reset(new CSeqDB(dbname, CSeqDB::eProtein, intersect));
           		else  // No matches that have proper GI
           			final_results.erase(final_results.begin(), final_results.end());
           	}
           	else if (m_NegGIList && !m_NegGIList.Empty())
           	{
           		vector<TGi> myGis;
           		s_GetAllGis(myGis, final_results, m_SeqDB);
           		CRef<CSeqDBGiList> intersect( new CIntersectionGiList(*m_NegGIList, myGis));
           		const string& dbname = m_SeqDB->GetDBNameList();
           		if (intersect->Size() > 0)
           			seqdb.Reset(new CSeqDB(dbname, CSeqDB::eProtein, intersect));
           		else  // No matches that have proper GI
           			final_results.erase(final_results.begin(), final_results.end());
           	}
           	if (seqdb.NotEmpty())
           		kmerResults.Reset(new CBlastKmerResults(kmerSearch.qSeqid, final_results, kmer_stats, seqdb, errs)); 
           	else
           		kmerResults.Reset(new CBlastKmerResults(kmerSearch.qSeqid, final_results, kmer_stats, m_SeqDB, errs)); 
           	kmerResultSet->push_back(kmerResults);
        }
	return kmerResultSet;
}

CRef<CBlastKmerResultsSet> 
CBlastKmer::RunSearches() {
	CRef<CBlastKmerResultsSet> retval = Run();
	return retval;
}

CRef<CBlastKmerResultsSet> 
CBlastKmer::Run() {
	CMinHashFile mhfile(m_KmerFiles[0]);
	int kmerVer = mhfile.GetVersion();
        int num_hashes = mhfile.GetNumHashes();
        int samples = mhfile.GetSegStatus();
        int kmerNum = mhfile.GetKmerSize();
        int alphabetChoice = mhfile.GetAlphabet();
        uint32_t* random_nums = mhfile.GetRandomNumbers();
        int rows_per_band = mhfile.GetRows();
	vector<int> badMers;
	mhfile.GetBadMers(badMers);

	// hash coefficients
	vector<uint32_t> a(num_hashes);
	vector<uint32_t> b(num_hashes);
	
	// obtain random coefficients for hashing
	if (kmerVer == 3)
	{
		a[0] =random_nums[0];
		b[0] =random_nums[1];
	}
	else
	{
		for(int i=0;i<num_hashes;i++)
			a[i] = random_nums[i];
		for(int i=0;i<num_hashes;i++)
			b[i] = random_nums[i+num_hashes];
	}

	vector < vector<int> > kValues;
	if (kmerVer == 2) 
	{
		int total=0;
		unsigned char* kvaluesArray = mhfile.GetKValues();
		for (int i=0; i<samples; i++)
		{
			vector<int> temp;
			for (int j=0; j<rows_per_band; j++)
				temp.push_back(kvaluesArray[total++]);
			kValues.push_back(temp);
		}
	}

	SBlastKmerParameters kmerParams(num_hashes, rows_per_band, samples, kmerNum, alphabetChoice, kmerVer);
	if (kmerVer > 2)
		kmerParams.chunkSize = mhfile.GetChunkSize();

	int numQueries = m_QueryVector.size();

	CRef<CBlastKmerResultsSet> kmerResultsSet = x_SearchMultipleQueries(0, numQueries, kmerParams, a.data(), b.data(), kValues, badMers);
	
	return kmerResultsSet;
}

END_SCOPE(blast)
END_NCBI_SCOPE

