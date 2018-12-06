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
 * Authors:  Amelia Fong
 *
 * File Description:
 *   Unit test.
 *
 */
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>

#include <corelib/test_boost.hpp>
#ifndef SKIP_DOXYGEN_PROCESSING

#ifdef NCBI_OS_MSWIN
#  define DEV_NULL "nul:"
#else
#  define DEV_NULL "/dev/null"
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);
// Test Cases
BOOST_AUTO_TEST_SUITE(seqdb_lmdb)


BOOST_AUTO_TEST_CASE(Test_GetVolInfo)
{
	CSeqDBLMDB lmdb("data/seqp_v5.pdb");
	vector<string> vol_names;
	vector<blastdb::TOid> vol_num_oids;
	lmdb.GetVolumesInfo(vol_names, vol_num_oids);
    BOOST_REQUIRE_EQUAL(vol_names.size(), 1);
    BOOST_REQUIRE_EQUAL(vol_num_oids.size(), 1);
    BOOST_REQUIRE_EQUAL(vol_names[0], "seqp_v5");
    BOOST_REQUIRE_EQUAL(vol_num_oids[0], 100);
}

BOOST_AUTO_TEST_CASE(Test_MixVersionDBs)
{
    BOOST_REQUIRE_THROW(
            CSeqDB db("data/seqp_v5 data/wp_nr", CSeqDB::eProtein),
            CSeqDBException
    );
}

BOOST_AUTO_TEST_CASE(Test_UserSeqIdList)
{
    CRef<CSeqDBGiList> gi_list(
            new CSeqDBFileGiList("data/prot345.sil.bsl", CSeqDBFileGiList::eSiList)
    );
    CSeqDB db("data/seqp_v5", CSeqDB::eProtein, 0, 0, true, gi_list);

    int found = 0;
    for (blastdb::TOid oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(58, found);

    string seq = kEmptyStr;
    const string ref_seq("MEGGGKAGYS");
    TSeqRange range(0,9);
    db.GetSequenceAsString(45, seq, range);
    BOOST_REQUIRE_EQUAL(ref_seq, seq);

    // Skipped oids not in seqid list
    blastdb::TOid check_oid = 21;
    db.CheckOrFindOID(check_oid);
    BOOST_REQUIRE_EQUAL(check_oid, 24);
}

BOOST_AUTO_TEST_CASE(Test_UserSeqIdList_MultiDB)
{
    CRef<CSeqDBGiList> gi_list(
            new CSeqDBFileGiList(
                    "data/wp_single_id.list.bsl",
                    CSeqDBFileGiList::eSiList
            )
    );
    CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein,
            0, 0, true, gi_list);

    int found = 0;
    blastdb::TOid oid = 0;
    db.CheckOrFindOID(oid);
    BOOST_REQUIRE_EQUAL(oid, 101);
    for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(1, found);

    string seq = kEmptyStr;
    const string ref_seq("MTNPFENDNY");
    TSeqRange range(0,9);
    db.GetSequenceAsString(101, seq, range);
    BOOST_REQUIRE_EQUAL(ref_seq, seq);
}

BOOST_AUTO_TEST_CASE(Test_SeqIdList_AliasFile)
{
    CSeqDB db("data/prot_alias_v5", CSeqDB::eProtein, 0, 0, true);

    int found = 0;
    for(blastdb::TOid oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(55, found);
}

BOOST_AUTO_TEST_CASE(Test_SeqIdList_FilteredID)
{
    CSeqDB db("data/test_seqidlist_v5", CSeqDB::eProtein, 0, 0, true);

    /// Oid 1 has 11 ids but only one in seqid list should be in the id list
    list< CRef<CSeq_id> > ids = db.GetSeqIDs(1);
    string fasta_id = kEmptyStr;
    int num_acc =0;
    ITERATE(list< CRef<CSeq_id> >, itr, ids) {
    	 if((*itr)->IsGi()) {
    		 continue;
    	 }
    	 else {
    		 // special  prf id
    		 fasta_id = (*itr)->AsFastaString();
    		 num_acc ++;
    	 }
    }
    BOOST_REQUIRE_EQUAL(1 , num_acc);
    BOOST_REQUIRE_EQUAL(fasta_id , "prf||2209341B");
}

BOOST_AUTO_TEST_CASE(Test_Multi_SeqIdList_AliasFile)
{
    CSeqDB db("data/alias_2_v5", CSeqDB::eProtein, 0, 0, true);

    int found = 0;
    for(blastdb::TOid oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(63, found);
}

BOOST_AUTO_TEST_CASE(Test_Mix_GI_SeqId_List_AliasFile)
{
    CSeqDB db("data/multi_list_alias_v5", CSeqDB::eProtein, 0, 0, true);

    int found = 0;
    blastdb::TOid oid = 0;
    for(blastdb::TOid i=0; db.CheckOrFindOID(i); i++) {
    	oid = i;
        found++;
    }
    BOOST_REQUIRE_EQUAL(1, found);
    BOOST_REQUIRE_EQUAL(3, oid);
}

BOOST_AUTO_TEST_CASE(Test_Mix_User_SeqIdList_AliasFile)
{
	CRef<CSeqDBGiList> gi_list( new CSeqDBFileGiList( "data/test.seqidlist.bsl", CSeqDBFileGiList::eSiList));
    CSeqDB db("data/test_seqidlist_v5", CSeqDB::eProtein, 0, 0, true, gi_list);

    int found = 0;
    for(blastdb::TOid i=0; db.CheckOrFindOID(i); i++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(2, found);
}

BOOST_AUTO_TEST_CASE(Test_Mix_Negative_User_SeqIdList)
{
	CRef<CSeqDBGiList> list_file( new CSeqDBFileGiList( "data/test.seqidlist.bsl", CSeqDBFileGiList::eSiList));
	CRef<CSeqDBNegativeList> n_list(new CSeqDBNegativeList());
	n_list->SetListInfo(list_file->GetListInfo());
	vector<string> sis;
	list_file->GetSiList(sis);
	n_list->ReserveSis(sis.size());
	ITERATE(vector<string>, iter, sis) {
		n_list->AddSi(*iter);
	}

    CSeqDB db("data/test_v5", CSeqDB::eProtein, n_list.GetNonNullPointer());

    int found = 0;
    for(blastdb::TOid i=0; db.CheckOrFindOID(i); i++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(7, found);
}

BOOST_AUTO_TEST_CASE(Test_Negative_UserSeqIdList_MultiDB)
{
    CRef<CSeqDBGiList> list_file( new CSeqDBFileGiList( "data/wp_single_neg_list.bsl", CSeqDBFileGiList::eSiList));
	CRef<CSeqDBNegativeList> n_list(new CSeqDBNegativeList());
	n_list->SetListInfo(list_file->GetListInfo());
	vector<string> sis;
	list_file->GetSiList(sis);
	n_list->ReserveSis(sis.size());
	ITERATE(vector<string>, iter, sis) {
		n_list->AddSi(*iter);
	}
    CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, n_list);

    int found = 0;
    blastdb::TOid oid = 101;
    bool rv = db.CheckOrFindOID(oid);
    BOOST_REQUIRE_EQUAL(rv, false);
    for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(101, found);
}

BOOST_AUTO_TEST_CASE(Test_Negative_SeqIdList_With_AliasFile)
{
	CRef<CSeqDBGiList> list_file( new CSeqDBFileGiList( "data/alias.seqidlist.bsl", CSeqDBFileGiList::eSiList));
	CRef<CSeqDBNegativeList> n_list(new CSeqDBNegativeList());
	n_list->SetListInfo(list_file->GetListInfo());
	vector<string> sis;
	list_file->GetSiList(sis);
	n_list->ReserveSis(sis.size());
	ITERATE(vector<string>, iter, sis) {
		n_list->AddSi(*iter);
	}
    CSeqDB db("data/prot_alias_v5", CSeqDB::eProtein, n_list);

    int found = 0;
    for(blastdb::TOid oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(19, found);
}

BOOST_AUTO_TEST_CASE(Test_Negative_Duplicate_SeqIdList_MultiDB)
{
    CRef<CSeqDBGiList> list_file( new CSeqDBFileGiList( "data/wp_duplicate_ids_negative_list.bsl", CSeqDBFileGiList::eSiList));
	CRef<CSeqDBNegativeList> n_list(new CSeqDBNegativeList());
	n_list->SetListInfo(list_file->GetListInfo());
	vector<string> sis;
	list_file->GetSiList(sis);
	n_list->ReserveSis(sis.size());
	ITERATE(vector<string>, iter, sis) {
		n_list->AddSi(*iter);
	}
    CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, n_list);

    int found = 0;
    blastdb::TOid oid = 101;
    bool rv = db.CheckOrFindOID(oid);
    BOOST_REQUIRE_EQUAL(rv, false);
    for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(101, found);
}

BOOST_AUTO_TEST_CASE(Test_TaxIdList)
{
	set<int> tax_ids;
    tax_ids.insert(1386);

	CRef<CSeqDBGiList> taxid_list(new CSeqDBGiList());
	taxid_list->AddTaxIds(tax_ids);
    CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, taxid_list.GetPointer());

    int found = 0;
    blastdb::TOid oid = 101;
    bool rv = db.CheckOrFindOID(oid);
    BOOST_REQUIRE_EQUAL(101, oid);
    BOOST_REQUIRE_EQUAL(rv, true);
    for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(1, found);
}

BOOST_AUTO_TEST_CASE(Test_TaxIdNotFound)
{
	set<int> tax_ids;
    tax_ids.insert(1386);

	CRef<CSeqDBGiList> taxid_list(new CSeqDBGiList());
	taxid_list->AddTaxIds(tax_ids);
	BOOST_REQUIRE_THROW(CSeqDB db("data/seqp_v5 data/test_v5", CSeqDB::eProtein, taxid_list.GetPointer()), CSeqDBException);
}


BOOST_AUTO_TEST_CASE(Test_NeagtiveTaxIdList)
{
	set<int> tax_ids;
    tax_ids.insert(1386);
    {
    	CSeqDB wp("data/wp_nr_v5", CSeqDB::eProtein);
    	set<int> tmp;
    	wp.GetAllTaxIDs(0, tmp);
    	tax_ids.insert(tmp.begin(), tmp.end());
    }

	CRef<CSeqDBNegativeList> taxid_list(new CSeqDBNegativeList());
	taxid_list->AddTaxIds(tax_ids);
    CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, taxid_list.GetPointer());

    int found = 0;
    blastdb::TOid oid = 100;
    bool rv = db.CheckOrFindOID(oid);
    BOOST_REQUIRE_EQUAL(rv, true);
    BOOST_REQUIRE_EQUAL(oid, 101);
    for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        found++;
    }
    BOOST_REQUIRE_EQUAL(101, found);
}

BOOST_AUTO_TEST_CASE(Test_TaxIdZero)
{
	set<int> tax_ids;
    tax_ids.insert(0);

    {
    	CRef<CSeqDBGiList> taxid_list(new CSeqDBGiList());
    	taxid_list->AddTaxIds(tax_ids);
        CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, taxid_list.GetPointer());

        int found = 0;
        blastdb::TOid oid = 100;
        bool rv = db.CheckOrFindOID(oid);
        BOOST_REQUIRE_EQUAL(rv, false);
        for(oid = 0; db.CheckOrFindOID(oid); oid++) {
            found++;
        }
        BOOST_REQUIRE_EQUAL(100, found);
    }

    {
    	CRef<CSeqDBNegativeList> taxid_list(new CSeqDBNegativeList());
		taxid_list->AddTaxIds(tax_ids);
    	CSeqDB db("data/seqp_v5 data/wp_nr_v5", CSeqDB::eProtein, taxid_list.GetPointer());

    	int found = 0;
    	blastdb::TOid oid = 0;
    	bool rv = db.CheckOrFindOID(oid);
    	BOOST_REQUIRE_EQUAL(rv, true);
    	BOOST_REQUIRE_EQUAL(oid, 100);
    	for(oid = 0; db.CheckOrFindOID(oid); oid++) {
        	found++;
    	}
    	BOOST_REQUIRE_EQUAL(2, found);
    }
}

BOOST_AUTO_TEST_CASE(Test_GetTaxIdsForOids)
{
	{
		set<Int4> tax_ids;
		CSeqDB db("data/15_seqs_v5", CSeqDB::eNucleotide);
		db.GetDBTaxIds(tax_ids);
		BOOST_REQUIRE_EQUAL(tax_ids.size(), 4);
	}
	{
		set<Int4> tax_ids;
		CSeqDB db("data/10_seqs_alias", CSeqDB::eNucleotide);
		db.GetDBTaxIds(tax_ids);
		BOOST_REQUIRE_EQUAL(tax_ids.size(), 2);
	}
	{
		set<Int4> tax_ids;
		CSeqDB db("data/skip_vols_mix", CSeqDB::eNucleotide);
		db.GetDBTaxIds(tax_ids);
		BOOST_REQUIRE_EQUAL(tax_ids.size(), 3);
	}
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
