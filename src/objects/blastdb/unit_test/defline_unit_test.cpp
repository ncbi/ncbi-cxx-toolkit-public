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
*   Unit test for sorting of Seq-ids in BLAST deflines
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>

// generated includes
#include <objects/blastdb/Blast_def_line_set.hpp>

#include <util/random_gen.hpp>



// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>




USING_NCBI_SCOPE;
USING_SCOPE(objects);


NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default in"
         "order to avoid unwanted failure during the daily automated builds.");
}



NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
}

// Make one defline.
static CRef<CBlast_def_line> s_MakeDefline(const string& id, const string& title)
{
	CRef<CBlast_def_line> defline(new CBlast_def_line());
	list<CRef<CSeq_id> >& seqid_list = defline->SetSeqid();
	CSeq_id::ParseFastaIds(seqid_list, id);
	defline->SetTitle(title);

	return defline;
}

// Make a series of deflines in random order.
CRef<CBlast_def_line_set>
s_MakeRandomDeflineSet(const char* const theIds[], size_t array_size)
{
 	vector<CRef<CBlast_def_line> > defline_v;
	for (size_t i=0; i<array_size; ++i)
	{
		defline_v.push_back(s_MakeDefline(theIds[i], "the title"));
	}

	CRandom rnd(1);
	for (size_t i=0; i<array_size; ++i)
	{
		swap(defline_v[i], defline_v[rnd.GetRand(0, defline_v.size()-1)]);
	}

	CRef<CBlast_def_line_set> defline_set(new CBlast_def_line_set());
	for (size_t index=0; index<array_size; index++)
	{
		const string fasta_str = defline_v[index]->GetSeqid().back()->AsFastaString();
		// cerr << fasta_str << "\n";
		defline_set->Set().push_back(defline_v[index]);
	}
	return defline_set;
}



// NP before YP before WP
BOOST_AUTO_TEST_CASE(SortRefSeqProteinSet1)
{
	// theIds has the canonical order
	const char* const theIds[] = {
		"gi|15674171|ref|NP_268346.1|",
		"gi|116513137|ref|YP_812044.1|",
		"gi|125625229|ref|YP_001033712.1|",
		"gi|281492845|ref|YP_003354825.1|",
		"gi|385831755|ref|YP_005869568.1|",
		"gi|289223532|ref|WP_003131952.1|",
		"gi|13878750|sp|Q9CDN0.1|RS18_LACLA",
		"gi|122939895|sp|Q02VU1.1|RS18_LACLS",
		"gi|166220956|sp|A2RNZ2.1|RS18_LACLM",
		"gi|12725253|gb|AAK06287.1|AE006448_5",
		"gi|116108791|gb|ABJ73931.1|",
		"gi|124494037|emb|CAL99037.1|",
		"gi|281376497|gb|ADA65983.1|",
		"gi|300072039|gb|ADJ61439.1|",
		"gi|326407763|gb|ADZ64834.1|"
	};

	CRef<CBlast_def_line_set> defline_set = s_MakeRandomDeflineSet(theIds, ArraySize(theIds));

	defline_set->SortBySeqIdRank(true);

	int index=0;
	ITERATE(CBlast_def_line_set::Tdata, itr, defline_set->Get())
	{
		const string fasta_str = (*itr)->GetSeqid().back()->AsFastaString();
		// cerr << fasta_str << "\n";
		string startId(theIds[index]);
		BOOST_CHECK_MESSAGE(startId.find(fasta_str) != string::npos, "Error for " << fasta_str);
		index++;
	}
}

// YP before WP
BOOST_AUTO_TEST_CASE(SortRefSeqProteinSet2)
{
	const char* const theIds[] = {
		"gi|443615715|ref|YP_007379571.1|",
		"gi|444353545|ref|YP_007389689.1|",
		"gi|448240163|ref|YP_007404216.1|",
		"gi|449306713|ref|YP_007439069.1|",
		"gi|446057344|ref|WP_000135199.1|",
		"gi|67472372|sp|P0A7T7.2|RS18_ECOLI",
		"gi|67472373|sp|P0A7T8.2|RS18_ECOL6",
		"gi|67472374|sp|P0A7T9.2|RS18_ECO57",
	};

	CRef<CBlast_def_line_set> defline_set = s_MakeRandomDeflineSet(theIds, ArraySize(theIds));

	defline_set->SortBySeqIdRank(true);

	int index=0;
	ITERATE(CBlast_def_line_set::Tdata, itr, defline_set->Get())
	{
		const string fasta_str = (*itr)->GetSeqid().back()->AsFastaString();
		// cerr << fasta_str << "\n";
		string startId(theIds[index]);
		// cerr << startId << "\n";
		BOOST_CHECK_MESSAGE(startId.find(fasta_str) != string::npos, "Error for " << fasta_str);
		index++;
	}
}

// NP before XP
BOOST_AUTO_TEST_CASE(SortRefSeqProteinSet3)
{
	// theIds has the canonical order
	const char* const theIds[] = {
		"gi|4757812|ref|NP_004880.1|",
		"gi|114614837|ref|XP_001139040.1|",
		"gi|426357086|ref|XP_004045879.1|",
		"gi|7404340|sp|P56134.3|ATPK_HUMAN",
		"gi|3335128|gb|AAC39887.1|",
		"gi|3552030|gb|AAC34895.1|",
		"gi|48145899|emb|CAG33172.1|",
		"gi|49457306|emb|CAG46952.1|",
		"gi|51094625|gb|EAL23877.1|",
		"gi|119597067|gb|EAW76661.1|"
	};

	CRef<CBlast_def_line_set> defline_set = s_MakeRandomDeflineSet(theIds, ArraySize(theIds));

	defline_set->SortBySeqIdRank(true);

	int index=0;
	ITERATE(CBlast_def_line_set::Tdata, itr, defline_set->Get())
	{
		const string fasta_str = (*itr)->GetSeqid().back()->AsFastaString();
		// cerr << fasta_str << "\n";
		string startId(theIds[index]);
		BOOST_CHECK_MESSAGE(startId.find(fasta_str) != string::npos, "Error for " << fasta_str);
		index++;
	}
}


// NM befor XM
BOOST_AUTO_TEST_CASE(SortRefSeqNucleotideSet1)
{
	// theIds has the canonical order
	const char* const theIds[] = {
		"gi|133922597|ref|NM_001083308.1|",
		"gi|55621735|ref|XM_526424.1|",
		"gi|397465444|ref|XM_003804458.1|",
		"gi|61741314|gb|AY858112.1|"
	};

	CRef<CBlast_def_line_set> defline_set = s_MakeRandomDeflineSet(theIds, ArraySize(theIds));

	defline_set->SortBySeqIdRank(false);

	int index=0;
	ITERATE(CBlast_def_line_set::Tdata, itr, defline_set->Get())
	{
		const string fasta_str = (*itr)->GetSeqid().back()->AsFastaString();
		// cerr << fasta_str << "\n";
		string startId(theIds[index]);
		BOOST_CHECK_MESSAGE(startId.find(fasta_str) != string::npos, "Error for " << fasta_str);
		index++;
	}
}


BOOST_AUTO_TEST_CASE(Test_GetTaxIds)
{
    CBlast_def_line def_line;   // initially empty

    // Test GetTaxIds with nothing in 'taxid', nothing in 'links'.
    CBlast_def_line::TTaxIds taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 0);

    // Test GetTaxIds with value in 'taxid', nothing in 'links'.
    def_line.SetTaxid(100001);
    def_line.ResetLinks();
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 1);

    list<int> taxid_list;       // initially empty

    // Test GetTaxIds with nothing in 'taxid', values in 'links'.
    taxid_list.push_back(200003);
    taxid_list.push_back(200002);
    taxid_list.push_back(200001);
    def_line.ResetTaxid();
    def_line.SetLinks() = taxid_list;
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 3);

    // Test GetTaxIds with value in 'taxid', same values in 'links',
    // 'taxid' value in 'links'.
    def_line.SetTaxid(*taxid_list.begin());
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 3);

    // Test GetTaxIds with value in 'taxid', same values in 'links',
    // 'taxid' value not in 'links'.
    def_line.SetTaxid(200004);
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 4);
}


BOOST_AUTO_TEST_CASE(Test_SetTaxIds)
{
    CBlast_def_line def_line;   // initially empty

    CBlast_def_line::TTaxIds taxid_set;          // initially empty

    // Test SetTaxIds with no taxids.
    def_line.SetTaxIds(taxid_set);
    CBlast_def_line::TTaxIds taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxids.size(), 0);
    BOOST_CHECK(!def_line.IsSetTaxid());
    BOOST_CHECK(!def_line.IsSetLinks());

    // Test SetTaxIds with single taxid.
    def_line.ResetTaxid();
    def_line.ResetLinks();
    taxid_set.insert(100001);
    def_line.SetTaxIds(taxid_set);
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxid_set.size(), taxids.size());
    BOOST_CHECK(def_line.IsSetTaxid());
    BOOST_CHECK(!def_line.IsSetLinks());

    // Test SetTaxIds with multiple taxids.
    def_line.ResetTaxid();
    def_line.ResetLinks();
    taxid_set.insert(100002);
    taxid_set.insert(100003);
    taxid_set.insert(100004);
    taxid_set.insert(100005);
    def_line.SetTaxIds(taxid_set);
    taxids = def_line.GetTaxIds();
    BOOST_CHECK_EQUAL(taxid_set.size(), taxids.size());
    BOOST_CHECK(def_line.IsSetTaxid());
    BOOST_CHECK(def_line.IsSetLinks());
}
