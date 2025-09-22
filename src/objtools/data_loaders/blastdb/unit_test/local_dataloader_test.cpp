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
*   Unit tests for remote data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/exception.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include "../local_blastdb_adapter.hpp"
#include <util/random_gen.hpp>
#include <util/sequtil/sequtil_convert.hpp>


// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
using namespace ncbi::objects;
BEGIN_SCOPE(blast)

BOOST_AUTO_TEST_CASE(LocalFetchNucleotideBioseq)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("data/test_nucl");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     BOOST_REQUIRE_NE(loader_name.find("test_nuclNucleotide"), std::string::npos);
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1(CSeq_id::e_Gi, 555);  // nucleotide

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(624U, handle1.GetInst().GetLength());
     TTaxId taxid = scope.GetTaxId(seqid1);
     BOOST_REQUIRE_EQUAL(TAX_ID_CONST(9913), taxid);
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(624, bioseq1->GetInst().GetLength());

     CSeq_id seqid2(CSeq_id::e_Gi, 129295); // protein 
     
     CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);

     BOOST_REQUIRE(handle2.State_NoData());
}

BOOST_AUTO_TEST_CASE(LocalFetchBatchData)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("data/test_nucl");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CScope::TSequenceLengths reference_L, test_L;
     CScope::TSequenceTypes reference_T, test_T;
     CScope::TTaxIds reference_TI, test_TI;

     CScope::TSeq_id_Handles idhs;
     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(4)));
     reference_L.push_back(556);
     reference_TI.push_back(TAX_ID_CONST(9646));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(7)));
     reference_L.push_back(437);
     reference_TI.push_back(TAX_ID_CONST(9913));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(9)));
     reference_L.push_back(1512);
     reference_TI.push_back(TAX_ID_CONST(9913));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(11)));
     reference_L.push_back(2367);
     reference_TI.push_back(TAX_ID_CONST(9913));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(15)));
     reference_L.push_back(540);
     reference_TI.push_back(TAX_ID_CONST(9915));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(16)));
     reference_L.push_back(1759);
     reference_TI.push_back(TAX_ID_CONST(9771));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(17)));
     reference_L.push_back(1758);
     reference_TI.push_back(TAX_ID_CONST(9771));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(18)));
     reference_L.push_back(1758);
     reference_TI.push_back(TAX_ID_CONST(9771));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(19)));
     reference_L.push_back(422);
     reference_TI.push_back(TAX_ID_CONST(9771));

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_CONST(20)));
     reference_L.push_back(410);
     reference_TI.push_back(TAX_ID_CONST(9771));

     reference_T.assign(idhs.size(), CSeq_inst::eMol_na);
     BOOST_REQUIRE_EQUAL(idhs.size(), reference_L.size());
     BOOST_REQUIRE_EQUAL(idhs.size(), reference_T.size());
     BOOST_REQUIRE_EQUAL(idhs.size(), reference_TI.size());

     scope.GetSequenceLengths(&test_L, idhs);
     scope.GetSequenceTypes(&test_T, idhs);
     scope.GetTaxIds(&test_TI, idhs);

     BOOST_REQUIRE_EQUAL(idhs.size(), test_L.size());
     BOOST_REQUIRE_EQUAL(idhs.size(), test_T.size());
     BOOST_REQUIRE_EQUAL(idhs.size(), test_TI.size());

     BOOST_CHECK_EQUAL_COLLECTIONS(test_L.begin(), test_L.end(),
                                   reference_L.begin(), reference_L.end());
     BOOST_CHECK_EQUAL_COLLECTIONS(test_TI.begin(), test_TI.end(),
                                   reference_TI.begin(), reference_TI.end());
     BOOST_CHECK_EQUAL_COLLECTIONS(test_T.begin(), test_T.end(),
                                   reference_T.begin(), reference_T.end());
}

BOOST_AUTO_TEST_CASE(LocalFetchNucleotideBioseqNotFixedSize)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("refseq_genomic");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, false,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     BOOST_REQUIRE_EQUAL("BLASTDB_refseq_genomicNucleotide", loader_name);
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1(CSeq_id::e_Other, "NC_000022");  // nucleotide

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE(handle1);
     BOOST_REQUIRE_EQUAL(50818468, handle1.GetInst().GetLength());
     BOOST_REQUIRE_EQUAL(TAX_ID_CONST(9606), scope.GetTaxId(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(50818468, bioseq1->GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(LocalFetchProteinBioseq)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("data/test_prot");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eProtein, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();

     BOOST_REQUIRE_NE(loader_name.find("test_protProtein"), std::string::npos);

     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("P01013.1");  // protein
     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE(handle1);
     BOOST_REQUIRE_EQUAL(232, handle1.GetInst().GetLength());
     BOOST_REQUIRE_EQUAL(TAX_ID_CONST(9031), scope.GetTaxId(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_aa, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(232, bioseq1->GetInst().GetLength());

     CSeq_id seqid2(CSeq_id::e_Gi, 555); // nucleotide 
     CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);
     BOOST_REQUIRE(!handle2);
     BOOST_REQUIRE(handle2.State_NoData());
     BOOST_REQUIRE_EQUAL(INVALID_TAX_ID, scope.GetTaxId(seqid2));

     CSeq_id seqid3(CSeq_id::e_Genbank, "KJT71719"); // by accession 
     CBioseq_Handle handle3 = scope.GetBioseqHandle(seqid3);
     BOOST_REQUIRE(handle3);
     BOOST_REQUIRE_EQUAL("KJT71719", handle3.GetSeqId()->GetSeqIdString());
     CConstRef<CBioseq> bioseq3 = handle3.GetCompleteBioseq();
     string defline = "";
     if (bioseq3->IsSetDescr()) {
          const CBioseq::TDescr::Tdata& data = bioseq3->GetDescr().Get();
          ITERATE(CBioseq::TDescr::Tdata, iter, data) {
             if((*iter)->IsTitle()) {
                 defline += (*iter)->GetTitle();
             }
          }
     }
     // Finds beginning of EGA25625 title.
     BOOST_REQUIRE(defline.find("Manganese ABC transporter") == 0);
}

// Motivated by WB-1712
BOOST_AUTO_TEST_CASE(FetchNonRedundantEntry)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("data/test_prot");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eProtein, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();

     BOOST_REQUIRE_NE(loader_name.find("test_protProtein"), std::string::npos);

     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     const size_t kExpectedLength(536);
     const TTaxId kExpectedTaxid = TAX_ID_CONST(9606);

     CSeq_id seqid1("NP_001308920");  // human protein
     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE(handle1);
     BOOST_REQUIRE_EQUAL(kExpectedLength, handle1.GetInst().GetLength());
     BOOST_CHECK_EQUAL(kExpectedTaxid, scope.GetTaxId(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_aa, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE(bioseq1.NotNull());
     BOOST_REQUIRE_EQUAL(kExpectedLength, bioseq1->GetInst().GetLength());
     BOOST_CHECK_EQUAL(kExpectedTaxid, bioseq1->GetTaxId());

     CSeq_id monkey_id("XP_001165763"); // monkey sequence
     CBioseq_Handle monkey_handle = scope.GetBioseqHandle(monkey_id);
     BOOST_REQUIRE(monkey_handle);
     BOOST_REQUIRE_EQUAL(kExpectedLength, monkey_handle.GetInst().GetLength());
     BOOST_CHECK_EQUAL(TAX_ID_CONST(9598), scope.GetTaxId(monkey_id));
}

#ifdef NCBI_THREADS

class CLocalAdapterThread : public CThread
{
public:
    CLocalAdapterThread(CRef<CSeqDB> seq_db)
        : m_SeqDB(seq_db) {
    }

    virtual void* Main() {
    	CLocalBlastDbAdapter ldb(m_SeqDB);

    	TSeqPos length_0 = ldb.GetSeqLength(0);
    	TSeqPos length_1 = ldb.GetSeqLength(1);
    	CRandom r;
    	for(int i=0; i < 100; i++) {
    		CRef<CSeq_data> s1 = ldb.GetSequence(0);
    		{
    			TSeqPos from = r.GetRand(0, length_0 -100);
    			TSeqPos to = r.GetRand(from, length_0);
    			 CRef<CSeq_data> s2 = ldb.GetSequence(0, from, to);
    		}
    		{
    			TSeqPos from = r.GetRand(0, length_1 -100);
    			TSeqPos to = r.GetRand(from, length_1);
    			CRef<CSeq_data> s3 = ldb.GetSequence(1, from, to);
    		}
    	}
        return (void*)0;
    }

private:
    CRef<CSeqDB> m_SeqDB;
};

BOOST_AUTO_TEST_CASE(LocalBlastDbAdapterMT)
{
	string dbname("data/testdb");
	typedef vector< CRef<CLocalAdapterThread> > TTesterThreads;
	const TSeqPos kNumThreads = 48;
	TTesterThreads the_threads(kNumThreads);

	CRef<CSeqDB> seqdb(new CSeqDB(dbname, CSeqDB::eNucleotide));
	for (TSeqPos i = 0; i < kNumThreads; i++) {
	    the_threads[i].Reset(new CLocalAdapterThread(seqdb));
	    BOOST_REQUIRE(the_threads[i].NotEmpty());
	}

	NON_CONST_ITERATE(TTesterThreads, thread, the_threads) {
	    (*thread)->Run();
	}

	NON_CONST_ITERATE(TTesterThreads, thread, the_threads) {
	    long result = 0;
	    (*thread)->Join(reinterpret_cast<void**>(&result));
	    BOOST_REQUIRE_EQUAL(0L, result);
	}

	for (TSeqPos i = 0; i < kNumThreads; i++) {
	    the_threads[i].Reset();
	}

}

#endif

BOOST_AUTO_TEST_CASE(BlastDbAdapterGetSequenceWithRange)
{
	string dbname("data/testdb");
	const string s1("GTTTTCAATAAT");
	const string s2("ACCGTTTCACAAGTAGGGCGTAGCGCATTTGCAG");
	const string s3("AATTGGCTGTTTTTGAACTACTGTA");
	const string s4("AGATTAATTATCATTTGCAG");

	CRef<CSeqDB> seqdb(new CSeqDB(dbname, CSeqDB::eNucleotide));
	CLocalBlastDbAdapter ldb(seqdb);
	{
		CRef<CSeq_data>  d1 = ldb.GetSequence(0, 1233, 1245);
		string t1;
		vector<char> ncbi4na = d1->GetNcbi4na().Get();
		CSeqConvert::Convert(ncbi4na, CSeqUtil::e_Ncbi4na, 0, s1.size(), t1, CSeqUtil::e_Iupacna);
		BOOST_REQUIRE_EQUAL(t1, s1);
	}

	{
		CRef<CSeq_data>  d2 = ldb.GetSequence(1, 98764, 98798);
		string t2;
		vector<char> ncbi4na = d2->GetNcbi4na().Get();
		CSeqConvert::Convert(ncbi4na, CSeqUtil::e_Ncbi4na, 0, s2.size(), t2, CSeqUtil::e_Iupacna);
		BOOST_REQUIRE_EQUAL(t2, s2);
	}
	{
		CRef<CSeq_data>  d3 = ldb.GetSequence(1, 100245, 100270);
		string t3;
		vector<char> ncbi4na = d3->GetNcbi4na().Get();
		CSeqConvert::Convert(ncbi4na, CSeqUtil::e_Ncbi4na, 0, s3.size(), t3, CSeqUtil::e_Iupacna);
		BOOST_REQUIRE_EQUAL(t3, s3);
	}
	{
		CRef<CSeq_data>  d4 = ldb.GetSequence(0, 12439, 12459);
		string t4;
		vector<char> ncbi4na = d4->GetNcbi4na().Get();
		CSeqConvert::Convert(ncbi4na, CSeqUtil::e_Ncbi4na, 0, s4.size(), t4, CSeqUtil::e_Iupacna);
		BOOST_REQUIRE_EQUAL(t4, s4);
	}


}

END_SCOPE(blast)
