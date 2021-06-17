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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for SRA data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/sra/sraloader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eNoTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR000123.1.2");
     const TSeqPos seqlen1 = 153;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eNoTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR066117.18823.2");
     const TSeqPos seqlen1 = 112;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eNoTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR066117.1.2");
     const TSeqPos seqlen1 = 110;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR000123.1.2");
     const TSeqPos seqlen1 = 107;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchSeq5)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR066117.18823.2");
     const TSeqPos seqlen1 = 102;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
     CRef<CObjectManager> om = CObjectManager::GetInstance();
     string loader_name =
         CSRADataLoader::RegisterInObjectManager(*om, CSRADataLoader::eTrim)
         .GetLoader()->GetName();
     CScope scope(*om);
     scope.AddDataLoader(loader_name);

     CSeq_id seqid1("gnl|SRA|SRR066117.1.2");
     const TSeqPos seqlen1 = 100;

     BOOST_REQUIRE_EQUAL(seqlen1, scope.GetSequenceLength(seqid1));
     BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

     CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
     BOOST_REQUIRE_EQUAL(seqlen1, handle1.GetInst().GetLength());
}
