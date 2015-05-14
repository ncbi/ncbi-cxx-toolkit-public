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
 * Author: Sema
 *
 * File Description:
 *   Sample unit tests file for the mainstream test developing.
 *
 * This file represents basic most common usage of Ncbi.Test framework (which
 * is based on Boost.Test framework. For more advanced techniques look into
 * another sample - unit_test_alt_sample.cpp.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
//#include <corelib/ncbi_system.hpp>
//#include <corelib/ncbiapp.hpp>
//#include <objects/general/Dbtag.hpp>
//#include <objects/seq/Seq_gap.hpp>
//#include <objects/seqfeat/Imp_feat.hpp>
//#include <objects/seqfeat/RNA_ref.hpp>
//#include <objects/seqfeat/RNA_gen.hpp>
//#include <objects/seqfeat/Cdregion.hpp>
//#include <objects/seqres/Int_graph.hpp>
//#include <objects/seqres/Seq_graph.hpp>
//#include <objects/pub/Pub.hpp>
//#include <objects/biblio/Cit_art.hpp>
//#include <objects/biblio/Auth_list.hpp>
//#include <objects/biblio/Affil.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <objmgr/object_manager.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(NDiscrepancy);

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
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
    cout << "Finalization function executed" << endl;
}


BOOST_AUTO_TEST_CASE(NON_EXISTENT)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("NON-EXISTENT");
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 0);
};


BOOST_AUTO_TEST_CASE(COUNT_NUCLEOTIDES)
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("COUNT_NUCLEOTIDES");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "1 nucleotide Bioseq is present");
}


BOOST_AUTO_TEST_CASE(OVERLAPPING_CDS)
{
/*
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancyCase> dscr = GetDiscrepancyCase("OVERLAPPING_CDS");
    CContext context;
    dscr->Parse(seh, context);
*/
}


BOOST_AUTO_TEST_CASE(CONTAINED_CDS)
{
/*
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancyCase> dscr = GetDiscrepancyCase("CONTAINED_CDS");
    CContext context;
    dscr->Parse(seh, context);
*/
}
