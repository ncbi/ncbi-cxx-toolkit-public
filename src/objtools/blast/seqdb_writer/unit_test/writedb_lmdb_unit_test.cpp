/*
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
 * Authors: Amelia Fong
 *
 * File Description:
 *   cwritedb lmdb_unit test.
 *
 */
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>

#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <corelib/test_boost.hpp>
#include <boost/current_function.hpp>
#include <objtools/blast/seqdb_writer/writedb_lmdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <corelib/ncbifile.hpp>

#include <unordered_map>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(writedb_lmdb)

BOOST_AUTO_TEST_CASE(CreateLMDBFile)
{
	const string base_name = "tmp_lmdb";
	DeleteLMDBFiles(true, base_name);
	const string lmdb_name = BuildLMDBFileName(base_name, true);
	const string tax_lmdb = GetFileNameFromExistingLMDBFile(lmdb_name, ELMDBFileType::eTaxId2Offsets);
	const int kNumVols = 4;
	CSeqDB source_db("data/writedb_prot",CSeqDB::eProtein);
	vector<string> vol_names;
	vector<blastdb::TOid> vol_num_oids;
	for(unsigned int k=0; k < kNumVols; k++) {
		vol_names.push_back("tmp_lmdb" + NStr::IntToString(k));
		vol_num_oids.push_back(k*1234);
	}

	{
		CWriteDB_LMDB test_db(lmdb_name, 100000);
		for (int i=0; i < source_db.GetNumOIDs(); i++) {
			list< CRef<CSeq_id> >  ids = source_db.GetSeqIDs(i);
			test_db.InsertEntries(ids, i);
		}
		test_db.InsertVolumesInfo(vol_names, vol_num_oids);

		CWriteDB_TaxID taxdb(tax_lmdb,100000);
	    const TTaxId taxids[5] = { TAX_ID_CONST(9606), TAX_ID_CONST(562), TAX_ID_CONST(0), TAX_ID_CONST(2), TAX_ID_CONST(10239) };
		for (int i=0; i < source_db.GetNumOIDs(); i++) {
			set<TTaxId> t;
			for(int j=0; j < (i % 5 + 1); j++) {
				t.insert(taxids[j]);
			}
			taxdb.InsertEntries(t, i);
		}
	}

	{
		vector<string> test_neg_accs;
		CSeqDBLMDB test_db(lmdb_name);

		/* Test GetOids from Seq IDs */
		for(int i=0; i < source_db.GetNumOIDs(); i++) {
			vector<string> test_accs;
			vector<blastdb::TOid> test_oids;
			list< CRef<CSeq_id> >  ids = source_db.GetSeqIDs(i);
			CRef<CSeq_id> n_id = FindBestChoice(ids, CSeq_id::WorstRank);
			test_neg_accs.push_back(n_id->GetSeqIdString(false));
			ITERATE(list< CRef<CSeq_id> >, itr, ids) {
				if((*itr)->IsGi()) {
					continue;
				}
				test_accs.push_back((*itr)->GetSeqIdString(true));
				test_accs.push_back((*itr)->GetSeqIdString(false));
			}
			test_db.GetOids(test_accs, test_oids);
			for(unsigned int j=0; j < test_accs.size(); j++) {
				BOOST_REQUIRE_EQUAL(test_oids[j], i);
			}
		}

		/* Test Negative Seq IDs  to OIDs */
		vector<blastdb::TOid> neg_oids;
		test_db.NegativeSeqIdsToOids(test_neg_accs, neg_oids);
		BOOST_REQUIRE_EQUAL(neg_oids.size(), 65);

		/* Test Vol Info */
		vector<string> test_vol_names;
		vector<blastdb::TOid> test_vol_num_oids;
		test_db.GetVolumesInfo(test_vol_names, test_vol_num_oids);
		for(unsigned int k=0; k < kNumVols; k++) {
			BOOST_REQUIRE_EQUAL(test_vol_num_oids[k], vol_num_oids[k]);
			BOOST_REQUIRE_EQUAL(test_vol_names[k], vol_names[k]);
		}

		/* Test Tax Ids */
		vector<blastdb::TOid> tax_oids;
		set<TTaxId> tax_ids;
		tax_ids.insert(TAX_ID_CONST(10239));
		vector<TTaxId> rv_tax_ids;
		test_db.GetOidsForTaxIds(tax_ids, tax_oids, rv_tax_ids);
		for(unsigned int i=0; i < tax_ids.size(); i++) {
			BOOST_REQUIRE_EQUAL(tax_oids[i] % 5, 4);
		}

		test_db.NegativeTaxIdsToOids(tax_ids, tax_oids, rv_tax_ids);
		BOOST_REQUIRE_EQUAL(tax_oids.size(), 0);

		tax_ids.clear();
		tax_ids.insert(9606);
		tax_ids.insert(562);
		test_db.NegativeTaxIdsToOids(tax_ids, tax_oids, rv_tax_ids);
		for(unsigned int i=0; i < rv_tax_ids.size(); i++) {
			BOOST_REQUIRE((tax_oids[i] % 5 < 2));
		}

	}
	DeleteLMDBFiles(true, base_name);
}


BOOST_AUTO_TEST_CASE(TestLMDBMapSize)
{
	const string base_name = "tmp_lmdb";
	DeleteLMDBFiles(true, base_name);
	const string lmdb_name = BuildLMDBFileName(base_name, true);
	const string tax_lmdb = GetFileNameFromExistingLMDBFile(lmdb_name, ELMDBFileType::eTaxId2Offsets);
	const int kNumVols = 4;
	CSeqDB source_db("data/writedb_prot",CSeqDB::eProtein);
	vector<string> vol_names;
	vector<blastdb::TOid> vol_num_oids;
	for(unsigned int k=0; k < kNumVols; k++) {
		vol_names.push_back("tmp_lmdb" + NStr::IntToString(k));
		vol_num_oids.push_back(k*1234);
	}

	{
		CWriteDB_LMDB test_db(lmdb_name, 10);
		for (int i=0; i < source_db.GetNumOIDs(); i++) {
			list< CRef<CSeq_id> >  ids = source_db.GetSeqIDs(i);
			test_db.InsertEntries(ids, i);
		}
		test_db.InsertVolumesInfo(vol_names, vol_num_oids);

		CWriteDB_TaxID taxdb(tax_lmdb,10);
	    const TTaxId taxids[5] = { TAX_ID_CONST(9606), TAX_ID_CONST(562), TAX_ID_CONST(0), TAX_ID_CONST(2), TAX_ID_CONST(10239) };
		for (int i=0; i < source_db.GetNumOIDs(); i++) {
			set<TTaxId> t;
			for(int j=0; j < (i % 5 + 1); j++) {
				t.insert(taxids[j]);
			}
			taxdb.InsertEntries(t, i);
		}
	}

	{
		vector<string> test_neg_accs;
		CSeqDBLMDB test_db(lmdb_name);

		/* Test GetOids from Seq IDs */
		for(int i=0; i < source_db.GetNumOIDs(); i++) {
			vector<string> test_accs;
			vector<blastdb::TOid> test_oids;
			list< CRef<CSeq_id> >  ids = source_db.GetSeqIDs(i);
			CRef<CSeq_id> n_id = FindBestChoice(ids, CSeq_id::WorstRank);
			test_neg_accs.push_back(n_id->GetSeqIdString(false));
			ITERATE(list< CRef<CSeq_id> >, itr, ids) {
				if((*itr)->IsGi()) {
					continue;
				}
				test_accs.push_back((*itr)->GetSeqIdString(true));
				test_accs.push_back((*itr)->GetSeqIdString(false));
			}
			test_db.GetOids(test_accs, test_oids);
			for(unsigned int j=0; j < test_accs.size(); j++) {
				BOOST_REQUIRE_EQUAL(test_oids[j], i);
			}
		}

		/* Test Negative Seq IDs  to OIDs */
		vector<blastdb::TOid> neg_oids;
		test_db.NegativeSeqIdsToOids(test_neg_accs, neg_oids);
		BOOST_REQUIRE_EQUAL(neg_oids.size(), 65);

		/* Test Vol Info */
		vector<string> test_vol_names;
		vector<blastdb::TOid> test_vol_num_oids;
		test_db.GetVolumesInfo(test_vol_names, test_vol_num_oids);
		for(unsigned int k=0; k < kNumVols; k++) {
			BOOST_REQUIRE_EQUAL(test_vol_num_oids[k], vol_num_oids[k]);
			BOOST_REQUIRE_EQUAL(test_vol_names[k], vol_names[k]);
		}

		/* Test Tax Ids */
		vector<blastdb::TOid> tax_oids;
		set<TTaxId> tax_ids;
		tax_ids.insert(TAX_ID_CONST(10239));
		vector<TTaxId> rv_tax_ids;
		test_db.GetOidsForTaxIds(tax_ids, tax_oids, rv_tax_ids);
		for(unsigned int i=0; i < tax_ids.size(); i++) {
			BOOST_REQUIRE_EQUAL(tax_oids[i] % 5, 4);
		}

		test_db.NegativeTaxIdsToOids(tax_ids, tax_oids, rv_tax_ids);
		BOOST_REQUIRE_EQUAL(tax_oids.size(), 0);

		tax_ids.clear();
		tax_ids.insert(9606);
		tax_ids.insert(562);
		test_db.NegativeTaxIdsToOids(tax_ids, tax_oids, rv_tax_ids);
		for(unsigned int i=0; i < rv_tax_ids.size(); i++) {
			BOOST_REQUIRE((tax_oids[i] % 5 < 2));
		}

	}
	DeleteLMDBFiles(true, base_name);
}


BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
