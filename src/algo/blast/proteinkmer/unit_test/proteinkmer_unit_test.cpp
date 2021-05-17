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
* Author:  Tom Madden, NCBI
*
* File Description:
*   Unit tests for proteinkmer library.
*
*
* ===========================================================================
*/

#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/simple/simple_om.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <util/sequtil/sequtil_convert.hpp>
#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerutils.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>
#include <algo/blast/proteinkmer/blastkmerindex.hpp>
#include <algo/blast/proteinkmer/blastkmerindex.hpp>
#include <algo/blast/proteinkmer/kblastapi.hpp>

#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>

#include <algo/blast/blastinput/blast_input.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(proteinkmers)


BOOST_AUTO_TEST_CASE(KmerResults)
{
	const int kNumHits = 6;
	// nr_test has GIs from array subjid in the order given.
	const TGi subjid[kNumHits] = { 129296, 385145541, 448824824, 510032768, 129295, 677974076};
	const double scores[kNumHits] = {0.359375, 0.710938, 0.242188, 0.234375, 0.28125, 0.234375};
	TBlastKmerPrelimScoreVector prelim_vector;
	for (int index=0; index<kNumHits; index++)
	{
		pair<uint32_t, double> retval(index, scores[index]);
		prelim_vector.push_back(retval);
	}
	CRef<CSeq_id> qid(new CSeq_id("gi|129296"));
	CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	// values in stats are: hit_count, jd_count, jd_oid_count, oids_considered, total_matches
	BlastKmerStats stats( 3256, 506, 253, 77623, 27); 

	CBlastKmerResults results(qid, prelim_vector, stats, seqdb, TQueryMessages());
	
        CRef<CScope> scope(CSimpleOM::NewScope());
	TSeqLocVector tsl;
	results.GetTSL(tsl, scope);
	int index=0;
	for (TSeqLocVector::iterator iter=tsl.begin(); iter != tsl.end(); ++iter)
	{
		BOOST_REQUIRE_EQUAL((*iter).seqloc->GetId()->GetGi(), subjid[index]);
		index++;
	}

	const TBlastKmerScoreVector kmerscores = results.GetScores();
	index=0;
	for (TBlastKmerScoreVector::const_iterator iter=kmerscores.begin(); iter != kmerscores.end(); ++iter)
	{
		CRef<CSeq_id> sid = (*iter).first;
		BOOST_REQUIRE_EQUAL(sid->GetGi(), subjid[index]);
		BOOST_REQUIRE_EQUAL((*iter).second, scores[index]);
		index++;
	}

	const BlastKmerStats& newstats = results.GetStats();
	BOOST_REQUIRE_EQUAL(newstats.hit_count, stats.hit_count);
	BOOST_REQUIRE_EQUAL(newstats.jd_count, stats.jd_count);
}

BOOST_AUTO_TEST_CASE(KmerResultsSet)
{
	const int kNumQueries = 2;
	const int kNumHits1 = 6;
	const int kNumHits2 = 2;
	// nr_test has GIs from array subjid in the order given by subjid1.
	const TGi subjid1[kNumHits1] = { 129296, 385145541, 448824824, 510032768, 129295, 677974076};
	const TGi subjid2[kNumHits2] = { 129295, 677974076};
	const double scores1[kNumHits1] = {0.359375, 0.710938, 0.242188, 0.234375, 0.28125, 0.234375};
	const double scores2[kNumHits1] = {0.359375, 0.710938};
	TBlastKmerPrelimScoreVector prelim_vector1;
	TBlastKmerPrelimScoreVector prelim_vector2;
	for (int index=0; index<kNumHits1; index++)
	{
		pair<uint32_t, double> retval(index, scores1[index]);
		prelim_vector1.push_back(retval);
	}
        // Ordering of OID to GIs are off here.
	for (int index=0; index<kNumHits2; index++)
	{	// Matches start at fourth sequence
		pair<uint32_t, double> retval(index+4, scores2[index]);
		prelim_vector2.push_back(retval);
	}
	CBlastKmerResultsSet::TBlastKmerPrelimScoreVectorSet vec_set;
	vec_set.push_back(prelim_vector1);
	vec_set.push_back(prelim_vector2);
	CRef<CSeq_id> qid1(new CSeq_id("gi|129296"));
	CRef<CSeq_id> qid2(new CSeq_id("gi|129295"));
	CBlastKmerResultsSet::TQueryIdVector id_vec;
	id_vec.push_back(qid1);
	id_vec.push_back(qid2);

	CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

	// values in stats are: hit_count, jd_count, jd_oid_count, oids_considered, total_matches
	BlastKmerStats stats1( 3256, 506, 253, 77623, 27); 
	BlastKmerStats stats2( 3255, 505, 255, 77625, 25); 
	CBlastKmerResultsSet::TBlastKmerStatsVector stats_vec;
	stats_vec.push_back(stats1);
	stats_vec.push_back(stats2);
	TSearchMessages errs;
	errs.resize(kNumQueries);

	CBlastKmerResultsSet result_set(id_vec, vec_set, stats_vec, seqdb, errs);

	BOOST_REQUIRE_EQUAL(result_set.GetNumQueries(), kNumQueries);
	
	CBlastKmerResults results = result_set[0];

	const TBlastKmerScoreVector kmerscores = results.GetScores();
	int index=0;
	for (TBlastKmerScoreVector::const_iterator iter=kmerscores.begin(); iter != kmerscores.end(); ++iter)
	{
		CRef<CSeq_id> sid = (*iter).first;
		BOOST_REQUIRE_EQUAL(sid->GetGi(), subjid1[index]);
		BOOST_REQUIRE_EQUAL((*iter).second, scores1[index]);
		index++;
	}

	const BlastKmerStats& newstats = results.GetStats();
	BOOST_REQUIRE_EQUAL(newstats.hit_count, stats1.hit_count);
	BOOST_REQUIRE_EQUAL(newstats.jd_count, stats1.jd_count);

	// Results from second query.
	results = result_set[1];
	const TBlastKmerScoreVector kmerscores2 = results.GetScores();
	index=0;
	for (TBlastKmerScoreVector::const_iterator iter=kmerscores2.begin(); iter != kmerscores2.end(); ++iter)
	{
		CRef<CSeq_id> sid = (*iter).first;
		BOOST_REQUIRE_EQUAL(sid->GetGi(), subjid2[index]);
		BOOST_REQUIRE_EQUAL((*iter).second, scores2[index]);
		index++;
	}

	// ID in this search set
	CRef<CBlastKmerResults> resultsNotNULL = result_set[*qid1];
	BOOST_REQUIRE(resultsNotNULL.IsNull() == false);

	// ID not in this search set.
	CRef<CSeq_id> qid_bogus(new CSeq_id("gi|555"));
	CRef<CBlastKmerResults> resultsNull = result_set[*qid_bogus];
	BOOST_REQUIRE(resultsNull.IsNull() == true);
}

BOOST_AUTO_TEST_CASE(KmerResultsSetPushBack)
{
	const int kNumQueries = 2;
	const int kNumHits1 = 6;
	const int kNumHits2 = 2;
	// nr_test has GIs from array subjid in the order given by subjid1.
	const TGi subjid1[kNumHits1] = { 129296, 385145541, 448824824, 510032768, 129295, 677974076};
    const TGi subjid2[kNumHits2] = { 129295, 677974076};
	const double scores1[kNumHits1] = {0.359375, 0.710938, 0.242188, 0.234375, 0.28125, 0.234375};
	const double scores2[kNumHits1] = {0.359375, 0.710938};
	TBlastKmerPrelimScoreVector prelim_vector1;
	TBlastKmerPrelimScoreVector prelim_vector2;
	for (int index=0; index<kNumHits1; index++)
	{
		pair<uint32_t, double> retval(index, scores1[index]);
		prelim_vector1.push_back(retval);
	}
        // Ordering of OID to GIs are off here.
	for (int index=0; index<kNumHits2; index++)
	{	// Matches start at fourth sequence
		pair<uint32_t, double> retval(index+4, scores2[index]);
		prelim_vector2.push_back(retval);
	}

	CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	TQueryMessages errs = TQueryMessages();

	CRef<CSeq_id> qid1(new CSeq_id("gi|129296"));
	BlastKmerStats stats1( 3256, 506, 253, 77623, 27); 
	CRef<CBlastKmerResults> results1;
	results1.Reset(new CBlastKmerResults(qid1, prelim_vector1, stats1, seqdb, TQueryMessages()));

	CRef<CSeq_id> qid2(new CSeq_id("gi|129295"));
	BlastKmerStats stats2( 3255, 505, 255, 77625, 25); 
	CRef<CBlastKmerResults> results2;
	results2.Reset(new CBlastKmerResults(qid2, prelim_vector2, stats2, seqdb, errs));

	CBlastKmerResultsSet result_set;

	result_set.push_back(results1);
	result_set.push_back(results2);

	BOOST_REQUIRE_EQUAL(result_set.GetNumQueries(), kNumQueries);
	
	CBlastKmerResults results = result_set[0];

	const TBlastKmerScoreVector kmerscores = results.GetScores();
	int index=0;
	for (TBlastKmerScoreVector::const_iterator iter=kmerscores.begin(); iter != kmerscores.end(); ++iter)
	{
		CRef<CSeq_id> sid = (*iter).first;
		BOOST_REQUIRE_EQUAL(sid->GetGi(), subjid1[index]);
		BOOST_REQUIRE_EQUAL((*iter).second, scores1[index]);
		index++;
	}

	const BlastKmerStats& newstats = results.GetStats();
	BOOST_REQUIRE_EQUAL(newstats.hit_count, stats1.hit_count);
	BOOST_REQUIRE_EQUAL(newstats.jd_count, stats1.jd_count);

	// Results from second query.
	results = result_set[1];
	const TBlastKmerScoreVector kmerscores2 = results.GetScores();
	index=0;
	for (TBlastKmerScoreVector::const_iterator iter=kmerscores2.begin(); iter != kmerscores2.end(); ++iter)
	{
		CRef<CSeq_id> sid = (*iter).first;
		BOOST_REQUIRE_EQUAL(sid->GetGi(), subjid2[index]);
		BOOST_REQUIRE_EQUAL((*iter).second, scores2[index]);
		index++;
	}
}

/* Test subset??
BOOST_AUTO_TEST_CASE(SearchWithBadQuery)
{
	CBioseq bioseq;
	CNcbiIfstream i_file("data/bad.asn");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	unique_ptr<SSeqLoc> ssl(new SSeqLoc(*loc, *scope));
	TSeqLocVector query_vector;
	query_vector.push_back(*ssl);
	CRef<CSeqDB> db(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());

	CBlastKmer kmersearch(query_vector, options, db);

	CRef<CBlastKmerResultsSet> results = kmersearch.Run();
	BOOST_REQUIRE((*results)[0].HasErrors() == false);
	BOOST_REQUIRE((*results)[0].HasWarnings() == true);
}
*/

BOOST_AUTO_TEST_CASE(SearchWithBadDatabase)
{
	CBioseq bioseq;
	CNcbiIfstream i_file("data/129295.stdaa");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	SSeqLoc ssl(*loc, *scope);
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());

	string dbname("JUNK");
	BOOST_REQUIRE_THROW(CBlastKmer kmersearch(ssl, options, dbname), CSeqDBException);	
}


/* smaller test??
BOOST_AUTO_TEST_CASE(GIListLimitSearch)
{
	CBioseq bioseq;
	CNcbiIfstream i_file("data/129295.iupacaa");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	unique_ptr<SSeqLoc> ssl(new SSeqLoc(*loc, *scope));
	TSeqLocVector query_vector;
	query_vector.push_back(*ssl);
	CRef<CSeqDB> db(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());
	options->SetThresh(0.15);

	CBlastKmer kmersearch(query_vector, options, db);	
	CRef<CSeqDBGiList> seqdb_gilist(new CSeqDBGiList());
	seqdb_gilist->AddGi(3091);
	seqdb_gilist->AddGi(129296);
	seqdb_gilist->AddGi(448824824);
	kmersearch.SetGiListLimit(seqdb_gilist);
	CRef<CBlastKmerResultsSet> results = kmersearch.Run();

	const TBlastKmerScoreVector& scores = (*results)[0].GetScores();
	BOOST_REQUIRE_EQUAL(scores.size(), 2);
}
*/

/*
BOOST_AUTO_TEST_CASE(NegativeGIListLimitSearch)
{
	CBioseq bioseq;
	CNcbiIfstream i_file("data/129295.iupacaa");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	unique_ptr<SSeqLoc> ssl(new SSeqLoc(*loc, *scope));
	TSeqLocVector query_vector;
	query_vector.push_back(*ssl);
	CRef<CSeqDB> db(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());
	options->SetThresh(0.15);

	CBlastKmer kmersearch(query_vector, options, db);	
	CRef<CSeqDBNegativeList> seqdb_gilist(new CSeqDBNegativeList());
	seqdb_gilist->AddGi(3091);
	seqdb_gilist->AddGi(129296);
	seqdb_gilist->AddGi(448824824);
	kmersearch.SetGiListLimit(seqdb_gilist);
	CRef<CBlastKmerResultsSet> resultSet = kmersearch.RunSearches();
	CBlastKmerResults results = (*resultSet)[0];

	const TBlastKmerScoreVector& scores = results.GetScores();
	BOOST_REQUIRE_EQUAL(scores.size(), 3);
}
*/

void s_GetRandomNumbers(uint32_t* a, uint32_t* b, int numHashes)
{
        CRandom random(1);  // Always have the same random numbers.
        for(int i=0;i<numHashes;i++)
        {
                do
                {
                        a[i]=random.GetRand();
                }
                while (a[i] == 0);

                b[i]=random.GetRand();
        }
}


BOOST_AUTO_TEST_CASE(CheckQueryHashes)
{
	string queryseq_eaa = 
	"MDSISVTNAKFCFDVFNEMKVHHVNENILYCPLSILTALAMVYLGARGNTESQMKKVLHFDSITGAGSTTDSQCGSSEYV"
	"HNLFKELLSEITRPNATYSLEIADKLYVDKTFSVLPEYLSCARKFYTGGVEEVNFKTAAEEARQLINSWVEKETNGQIKD"
	"LLVSSSIDFGTTMVFINTIYFKGIWKIAFNTEDTREMPFSMTKEESKPVQMMCMNNSFNVATLPAEKMKILELPYASGDL";

	string queryseq_stdaa;
	CSeqConvert::Convert(queryseq_eaa, CSeqUtil::e_Ncbieaa, 0, queryseq_eaa.length(), 
		queryseq_stdaa, CSeqUtil::e_Ncbistdaa);

	const int kNumHashes=128;
	const int kDoSeg=0;
	const int kKmerNum=5;
	const int kAlphabet=0; // 15 letters
	uint32_t a[kNumHashes];
	uint32_t b[kNumHashes];
	const int kChunkSize=150;

	s_GetRandomNumbers(a, b, kNumHashes);
	vector < vector <uint32_t> > seq_hash;
	
	minhash_query(queryseq_stdaa, seq_hash, kNumHashes, a, b, kDoSeg, kKmerNum, kAlphabet, kChunkSize);

	BOOST_REQUIRE_EQUAL(seq_hash.size(), 2);
	BOOST_REQUIRE_EQUAL(seq_hash[0].size(), kNumHashes);
	BOOST_REQUIRE_EQUAL(seq_hash[0][0], 529895);
	BOOST_REQUIRE_EQUAL(seq_hash[0][1], 798115);
	BOOST_REQUIRE_EQUAL(seq_hash[0][63], 90979);
	BOOST_REQUIRE_EQUAL(seq_hash[0][83], 336201);
	
	// Expected (correct) values.
	const int lsh_hash_length = 13;
	const int lsh_hash_vals[lsh_hash_length] = { 973119,1097197,1157729,1681152,1913970,1933659,2018075,2123893,2355301,2800673,2940688,2941967,3535701};

	const int kRowsPerBand=2;
	const int kNumBands = kNumHashes/kRowsPerBand;
	vector< vector <uint32_t> > lsh_hash_vec;
	get_LSH_hashes(seq_hash, lsh_hash_vec, kNumBands, kRowsPerBand);
	int index=0;
	int numChunks = lsh_hash_vec.size();
	for (int i=0; i<numChunks && index<lsh_hash_length; i++)
	{
		for(vector<uint32_t>::iterator iter=lsh_hash_vec[i].begin(); iter != lsh_hash_vec[i].end(); ++iter)
		{
  			BOOST_REQUIRE_EQUAL(*iter, lsh_hash_vals[index]);
			index++;
			if (index == lsh_hash_length)
				break;
		}
	}
}

BOOST_AUTO_TEST_CASE(CheckQueryHashesVersion3)
{
	string queryseq_eaa = 
	"MDSISVTNAKFCFDVFNEMKVHHVNENILYCPLSILTALAMVYLGARGNTESQMKKVLHFDSITGAGSTTDSQCGSSEYV"
	"HNLFKELLSEITRPNATYSLEIADKLYVDKTFSVLPEYLSCARKFYTGGVEEVNFKTAAEEARQLINSWVEKETNGQIKD"
	"LLVSSSIDFGTTMVFINTIYFKGIWKIAFNTEDTREMPFSMTKEESKPVQMMCMNNSFNVATLPAEKMKILELPYASGDL";

	string queryseq_stdaa;
	CSeqConvert::Convert(queryseq_eaa, CSeqUtil::e_Ncbieaa, 0, queryseq_eaa.length(), 
		queryseq_stdaa, CSeqUtil::e_Ncbistdaa);

	const int kNumHashes=32;
	const int kKmerNum=5;
	const int kAlphabet=0; // 15 letters
	vector<int> badMers;
	const int kChunkSize=150;

	vector < vector <uint32_t> > seq_hash;
	
	minhash_query2(queryseq_stdaa, seq_hash, kKmerNum, kNumHashes, kAlphabet, badMers, kChunkSize);

	BOOST_REQUIRE_EQUAL(seq_hash.size(), 2);
	BOOST_REQUIRE_EQUAL(seq_hash[0].size(), kNumHashes);
	BOOST_REQUIRE_EQUAL(seq_hash[0][0], 2683052);
	BOOST_REQUIRE_EQUAL(seq_hash[0][1], 26519505);
	BOOST_REQUIRE_EQUAL(seq_hash[0][2], 45619224);
	BOOST_REQUIRE_EQUAL(seq_hash[0][15], 396863844);
        // Values are ordered from smallest to largest
	BOOST_REQUIRE(seq_hash[1][0] < seq_hash[0][1]);
	BOOST_REQUIRE(seq_hash[1][1] < seq_hash[0][2]);
	BOOST_REQUIRE(seq_hash[1][3] < seq_hash[0][kNumHashes-1]);
	
	// Expected (correct) values.
	const int lsh_hash_length = 13;
	const int lsh_hash_vals[lsh_hash_length] = { 700168,1293774,1377419,1712432,1819660,2314660,2484152,2944352,2951476,3273866,3625878,3709806,3837843};

	const int kRowsPerBand=2;
	vector< vector <uint32_t> > lsh_hash_vec;
	get_LSH_hashes5(seq_hash, lsh_hash_vec, kNumHashes, kRowsPerBand);
	int index=0;
	int numChunks = lsh_hash_vec.size();
	for (int i=0; i<numChunks && index<lsh_hash_length; i++)
	{
		for(vector<uint32_t>::iterator iter=lsh_hash_vec[i].begin(); iter != lsh_hash_vec[i].end(); ++iter)
		{
 			BOOST_REQUIRE_EQUAL(*iter, lsh_hash_vals[index]);
			index++;
			if (index == lsh_hash_length)
				break;
		}
	}
}

int s_GetNumLSHHits(uint64_t* lsh, int lshSize)
{
	int count=0;
	for (int index=0; index<lshSize; index++)
		if (lsh[index] != 0)
			count++;

	return count;
}

BOOST_AUTO_TEST_CASE(BuildIndex)
{


        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=5;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct);

	string index_name("nr_test");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);

	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(2, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
	BOOST_REQUIRE_EQUAL(187, lsh_counts);
}

BOOST_AUTO_TEST_CASE(BuildIndexRepeats)
{
        CRef<CSeqDB> seqdb(new CSeqDB("data/XP_001468867", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=5;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct);

	string index_name("XP_001468867");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);
	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(2, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
 	BOOST_REQUIRE_EQUAL(213, lsh_counts);

}

BOOST_AUTO_TEST_CASE(BuildIndexNotValidSequence)
{
        CRef<CSeqDB> seqdb(new CSeqDB("data/manyXs", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=5;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct);

	string index_name("manyXs");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();
	// data file is empty/missing.  Should there be an exception for that?

	BOOST_REQUIRE_THROW(CMinHashFile mhfile(index_name), CMinHashException);	
}

BOOST_AUTO_TEST_CASE(BuildIndexWidth4Kmer4)
{


        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=4;
	const int kWidth=4;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct, 0, kWidth);

	string index_name("nr_test");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);
	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(kWidth, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
	BOOST_REQUIRE_EQUAL(172, lsh_counts);
}

BOOST_AUTO_TEST_CASE(BuildIndexFewerBands)
{

        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

        const int kHashFct=64;
	const int kKmerSize=4;
	const int kWidth=2;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct, 0, kWidth);

	string index_name("nr_test");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);
	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(kWidth, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
	BOOST_REQUIRE_EQUAL(339, lsh_counts);
}

BOOST_AUTO_TEST_CASE(BuildIndex10letterAlphabet)
{

        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=5;
	const int kWidth=2;
	const int kAlphabet=1;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct, 0, kWidth, kAlphabet);

	string index_name("nr_test");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);
	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(kWidth, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());
	BOOST_REQUIRE_EQUAL(kAlphabet, mhfile.GetAlphabet());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
	BOOST_REQUIRE_EQUAL(173, lsh_counts);
}

BOOST_AUTO_TEST_CASE(BuildIndex10letterVersion3)
{

        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

        const int kHashFct=32;
	const int kKmerSize=5;
	const int kSamples=30;
	const int kWidth=2;
	const int kAlphabet=1;
	const int kVersion=3;
	const int kLSHStart=312;

        CBlastKmerBuildIndex build_index(seqdb, kKmerSize, kHashFct, kSamples, kWidth, kAlphabet, kVersion);

	string index_name("nr_test");
	CFileDeleteAtExit::Add(index_name + ".pki");
	CFileDeleteAtExit::Add(index_name + ".pkd");
        build_index.Build();

	CMinHashFile mhfile(index_name);
	BOOST_REQUIRE_EQUAL(kHashFct, mhfile.GetNumHashes());
	BOOST_REQUIRE_EQUAL(kWidth, mhfile.GetDataWidth());
	BOOST_REQUIRE_EQUAL(kKmerSize, mhfile.GetKmerSize());
	BOOST_REQUIRE_EQUAL(kAlphabet, mhfile.GetAlphabet());
	BOOST_REQUIRE_EQUAL(kVersion, mhfile.GetVersion());
	BOOST_REQUIRE_EQUAL(kLSHStart, mhfile.GetLSHStart());

	uint64_t* lsh_array = mhfile.GetLSHArray();
	int lsh_size = mhfile.GetLSHSize();
	BOOST_REQUIRE_EQUAL(0x1000001, lsh_size);

	int lsh_counts = s_GetNumLSHHits(lsh_array, lsh_size-1);
	BOOST_REQUIRE_EQUAL(563, lsh_counts);

	int chunkSize = mhfile.GetChunkSize();
	BOOST_REQUIRE_EQUAL(150, chunkSize);
}

BOOST_AUTO_TEST_CASE(CheckEmptyIndexName)
{

	string index_name="";
	BOOST_REQUIRE_THROW(CMinHashFile mhfile(index_name), CMinHashException);	
}


BOOST_AUTO_TEST_CASE(CheckNoIndex)
{
	CBioseq bioseq;
	CNcbiIfstream i_file("data/129295.ncbieaa");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	unique_ptr<SSeqLoc> ssl(new SSeqLoc(*loc, *scope));
	TSeqLocVector query_vector;
	query_vector.push_back(*ssl);
        CRef<CSeqDB> seqdb(new CSeqDB("data/XP_001468867", CSeqDB::eProtein));
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());

	CBlastKmer kmersearch(query_vector, options, seqdb);
	BOOST_REQUIRE_THROW(kmersearch.Run(), CFileException);
}

BOOST_AUTO_TEST_CASE(CheckOptionValidation)
{
	CRef<CBlastKmerOptions>  opts(new CBlastKmerOptions());

	double myThresh=0.1;
	opts->SetThresh(myThresh);
	BOOST_REQUIRE_EQUAL(opts->GetThresh(), myThresh);

	int hits=373;
	opts->SetMinHits(hits);
	BOOST_REQUIRE_EQUAL(opts->GetMinHits(), hits);

	int targetSeqs=234;
	opts->SetNumTargetSeqs(targetSeqs);
	BOOST_REQUIRE_EQUAL(opts->GetNumTargetSeqs(), targetSeqs);

	BOOST_REQUIRE_EQUAL(opts->Validate(), true);

	opts->SetMinHits(0);
	BOOST_REQUIRE_EQUAL(opts->Validate(), true);

	opts->SetThresh(-0.707);
	BOOST_REQUIRE_EQUAL(opts->Validate(), false);

	opts->SetMinHits(-1);
	BOOST_REQUIRE_EQUAL(opts->Validate(), false);
}

BOOST_AUTO_TEST_CASE(BadOptionsThrow)
{

        CRef<CSeqDB> seqdb(new CSeqDB("data/nr_test", CSeqDB::eProtein));

	CBioseq bioseq;
	CNcbiIfstream i_file("data/129295.stdaa");
	unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, i_file));
	
	*is >> bioseq;
	CRef<CScope> scope(CSimpleOM::NewScope(false));
	scope->AddBioseq(bioseq);
	CRef<CSeq_loc> loc(new CSeq_loc);
    	loc->SetWhole().Assign(*(bioseq.GetId().front()));

	unique_ptr<SSeqLoc> ssl(new SSeqLoc(*loc, *scope));
	TSeqLocVector query_vector;
	query_vector.push_back(*ssl);
	CRef<CSeqDB> db(new CSeqDB("data/nr_test", CSeqDB::eProtein));
	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());
	options->SetThresh(1.3); // Value should be one or less.
	BOOST_REQUIRE_THROW(CBlastKmer kmersearch(query_vector, options, db), CException);	
}


BOOST_AUTO_TEST_SUITE_END()
