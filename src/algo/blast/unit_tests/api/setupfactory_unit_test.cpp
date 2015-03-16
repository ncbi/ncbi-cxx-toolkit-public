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
* Author:  Tom Madden
*
* File Description:
*   Unit test module to test utilities in setupfactory.c
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/setup_factory.hpp>

#include "blast_memento_priv.hpp"

#include "test_objmgr.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);


BOOST_AUTO_TEST_SUITE(setup_factory)

BOOST_AUTO_TEST_CASE(CreateScoreBlockBadReward)
{

     CRef<CSeq_id> qid(new CSeq_id("gi|555"));

     CRef<CBlastSearchQuery> Q1 = CTestObjMgr::Instance()
         .CreateBlastSearchQuery(*qid, eNa_strand_plus);

     CBlastQueryVector query;
     query.AddQuery(Q1);
     CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));

     CBlastNucleotideOptionsHandle options_handle; 
     options_handle.SetMatchReward(1);
     options_handle.SetMismatchPenalty(-1);
     CBlastOptions& opts = options_handle.SetOptions();

     CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&opts));

     auto_ptr<const CBlastOptionsMemento> opts_memento (opts.CreateSnapshot());
     
     BlastSeqLoc* blast_seq_loc = NULL;
     TSearchMessages search_messages;

     BOOST_CHECK_THROW(CSetupFactory::CreateScoreBlock(opts_memento.get(), query_data, &blast_seq_loc, search_messages), 
         CBlastException);
}


BOOST_AUTO_TEST_CASE(CreateScoreBlockMaskedSequence)
{
     // This sequence is mostly XXXXXXXXXXXXX
     CRef<CSeq_id> qid(new CSeq_id("gi|53690285"));
     TSeqRange range(60, 120);

     auto_ptr<SSeqLoc> ssloc(
          CTestObjMgr::Instance().CreateSSeqLoc(*qid, range, eNa_strand_unknown));

     TSeqLocVector tslv;
     tslv.push_back(*ssloc.get());

     CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(tslv));

     CBlastProteinOptionsHandle options_handle; 
     CBlastOptions& opts = options_handle.SetOptions();

     CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&opts));

     auto_ptr<const CBlastOptionsMemento> opts_memento (opts.CreateSnapshot());
     
     BlastSeqLoc* blast_seq_loc = NULL;
     TSearchMessages search_messages;

     BlastScoreBlk* sbp = CSetupFactory::CreateScoreBlock(opts_memento.get(), query_data, &blast_seq_loc, search_messages);

     BOOST_REQUIRE(search_messages.ToString().find(kBlastErrMsg_CantCalculateUngappedKAParams) != string::npos);
     BOOST_REQUIRE(blast_seq_loc != NULL);
     BOOST_REQUIRE(sbp != NULL);
 
     sbp = BlastScoreBlkFree(sbp);
     BOOST_REQUIRE(sbp == NULL);
     blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
     BOOST_REQUIRE(blast_seq_loc == NULL);
}

BOOST_AUTO_TEST_CASE(CreateScoreBlockOK)
{
     CRef<CSeq_id> qid(new CSeq_id("gi|129295"));

     auto_ptr<SSeqLoc> ssloc(
          CTestObjMgr::Instance().CreateSSeqLoc(*qid, eNa_strand_unknown));

     TSeqLocVector tslv;
     tslv.push_back(*ssloc.get());

     CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(tslv));

     CBlastProteinOptionsHandle options_handle; 
     CBlastOptions& opts = options_handle.SetOptions();

     CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&opts));

     auto_ptr<const CBlastOptionsMemento> opts_memento (opts.CreateSnapshot());
     
     BlastSeqLoc* blast_seq_loc = NULL;
     TSearchMessages search_messages;

     BlastScoreBlk* sbp = CSetupFactory::CreateScoreBlock(opts_memento.get(), query_data, &blast_seq_loc, search_messages);

     BOOST_REQUIRE(search_messages.HasMessages() == false);
     BOOST_REQUIRE(blast_seq_loc != NULL);
     BOOST_REQUIRE(sbp != NULL);
 
     sbp = BlastScoreBlkFree(sbp);
     BOOST_REQUIRE(sbp == NULL);
     blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
     BOOST_REQUIRE(blast_seq_loc == NULL);
}


BOOST_AUTO_TEST_SUITE_END()
