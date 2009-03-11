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
     BOOST_REQUIRE_EQUAL(624, handle1.GetInst().GetLength());

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(624, bioseq1->GetInst().GetLength());

     CSeq_id seqid2(CSeq_id::e_Gi, 129295); // protein 
     
     CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);

     BOOST_REQUIRE(handle2.State_NoData());
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
     BOOST_REQUIRE_EQUAL(49691432, handle1.GetInst().GetLength());

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(49691432, bioseq1->GetInst().GetLength());

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

     CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
     BOOST_REQUIRE_EQUAL(232, bioseq1->GetInst().GetLength());

     CSeq_id seqid2(CSeq_id::e_Gi, 555); // nucleotide 
     CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);
     BOOST_REQUIRE(!handle2);
     BOOST_REQUIRE(handle2.State_NoData());
}

END_SCOPE(blast)
