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
     string dbname("nt");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     BOOST_REQUIRE_EQUAL("BLASTDB_ntNucleotide", loader_name);
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1(CSeq_id::e_Gi, 555);  // nucleotide

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(624U, handle1.GetInst().GetLength());
     int taxid = scope.GetTaxId(seqid1);
     BOOST_REQUIRE_EQUAL(9913, taxid);
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
     string dbname("nt");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CScope::TSequenceLengths reference_L, test_L;
     CScope::TSequenceTypes reference_T, test_T;
     CScope::TTaxIds reference_TI, test_TI;

     CScope::TSeq_id_Handles idhs;
     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 4)));
     reference_L.push_back(556);
     reference_TI.push_back(9646);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 7)));
     reference_L.push_back(437);
     reference_TI.push_back(9913);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 9)));
     reference_L.push_back(1512);
     reference_TI.push_back(9913);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 11)));
     reference_L.push_back(2367);
     reference_TI.push_back(9913);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 15)));
     reference_L.push_back(540);
     reference_TI.push_back(9915);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 16)));
     reference_L.push_back(1759);
     reference_TI.push_back(9771);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 17)));
     reference_L.push_back(1758);
     reference_TI.push_back(9771);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 18)));
     reference_L.push_back(1758);
     reference_TI.push_back(9771);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 19)));
     reference_L.push_back(422);
     reference_TI.push_back(9771);

     idhs.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, 20)));
     reference_L.push_back(410);
     reference_TI.push_back(9771);

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
     string dbname("nucl_dbs");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eNucleotide, false,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();
     BOOST_REQUIRE_EQUAL("BLASTDB_nucl_dbsNucleotide", loader_name);
     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1(CSeq_id::e_Other, "NC_000022");  // nucleotide

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE(handle1);
     BOOST_REQUIRE_EQUAL(50818468, handle1.GetInst().GetLength());
     BOOST_REQUIRE_EQUAL(9606, scope.GetTaxId(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(50818468, bioseq1->GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(LocalFetchProteinBioseq)
{
     CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
     string dbname("nr");
     string loader_name = 
       CBlastDbDataLoader::RegisterInObjectManager(*objmgr, dbname, CBlastDbDataLoader::eProtein, true,
           CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet).GetLoader()->GetName();

     BOOST_REQUIRE_EQUAL("BLASTDB_nrProtein", loader_name);

     CScope scope(*objmgr);

     scope.AddDataLoader(loader_name);

     CSeq_id seqid1(CSeq_id::e_Gi, 129295);  // protein
     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE(handle1);
     BOOST_REQUIRE_EQUAL(232, handle1.GetInst().GetLength());
     BOOST_REQUIRE_EQUAL(9031, scope.GetTaxId(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_aa, scope.GetSequenceType(seqid1));

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(232, bioseq1->GetInst().GetLength());

     CSeq_id seqid2(CSeq_id::e_Gi, 555); // nucleotide 
     CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);
     BOOST_REQUIRE(!handle2);
     BOOST_REQUIRE(handle2.State_NoData());
     BOOST_REQUIRE_EQUAL(-1, scope.GetTaxId(seqid2));

     CSeq_id seqid3(CSeq_id::e_Genbank, "EGA25625.1"); // by accession 
     CBioseq_Handle handle3 = scope.GetBioseqHandle(seqid3);
     BOOST_REQUIRE(handle3);
     BOOST_REQUIRE_EQUAL("EGA25625", handle3.GetSeqId()->GetSeqIdString());
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
     BOOST_REQUIRE(defline.find("iron transport protein") == 0);
}

END_SCOPE(blast)
