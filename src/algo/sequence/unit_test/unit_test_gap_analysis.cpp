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
 * Author: Michael Kornbluh
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <algo/sequence/gap_analysis.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    void s_PrintSummary(
        const CGapAnalysis::TVectorGapLengthSummary & summary )
    {
        cerr << "BEGIN GAP SUMMARY" << endl;
        ITERATE( CGapAnalysis::TVectorGapLengthSummary, summary_iter, 
            summary ) 
        {
            const CGapAnalysis::SOneGapLengthSummary & one_summary =
                *summary_iter;
            cerr << "\tlen: " << one_summary.gap_length 
                << "\tnum seqs: " << one_summary.num_seqs
                << "\tnum gaps: " << one_summary.num_gaps << endl;
        
        }
        cerr << "END GAP SUMMARY" << endl;
    }
}

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("in-data", "InputFile",
                     "This is the input file used to run the test",
                     CArgDescriptions::eInputFile);
}

BOOST_AUTO_TEST_CASE(TestBasic)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CRef<CSeq_entry> pSeqEntry( new CSeq_entry );
    args["in-data"].AsInputFile() >> MSerial_AsnText >> *pSeqEntry;

    CSeq_entry_Handle entry_h = scope.AddTopLevelSeqEntry(*pSeqEntry);
    BOOST_REQUIRE( entry_h );

    CGapAnalysis gap_analysis; 
    gap_analysis.AddSeqEntryGaps(entry_h);

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> basic_gap_summary =
        gap_analysis.GetGapLengthSummary();
    s_PrintSummary(*basic_gap_summary);

    BOOST_CHECK_EQUAL( basic_gap_summary->size(), 4 );

    BOOST_CHECK_EQUAL( (*basic_gap_summary)[0].gap_length, 5 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[1].gap_length, 10 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[2].gap_length, 40 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[3].gap_length, 101 );

    BOOST_CHECK_EQUAL( (*basic_gap_summary)[0].num_seqs, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[1].num_seqs, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[2].num_seqs, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[3].num_seqs, 2 );

    BOOST_CHECK_EQUAL( (*basic_gap_summary)[0].num_gaps, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[1].num_gaps, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[2].num_gaps, 1 );
    BOOST_CHECK_EQUAL( (*basic_gap_summary)[3].num_gaps, 3 );
}

BOOST_AUTO_TEST_CASE(TestSeqIdComparison)
{
    // make sure Seq-ids are compared by contents, not pointer address.

    CRef<CSeq_id> pSeqId1( new CSeq_id("lcl|Seq1") );
    CRef<CSeq_id> pSeqId2( new CSeq_id("lcl|Seq1") );
    CRef<CSeq_id> pSeqId3( new CSeq_id("lcl|Seq2") );

    const CGapAnalysis::TGapLength kGapLength = 10;

    CGapAnalysis gap_analysis;
    gap_analysis.AddGap(pSeqId1, kGapLength);
    gap_analysis.AddGap(pSeqId2, kGapLength);
    gap_analysis.AddGap(pSeqId3, kGapLength);

    BOOST_CHECK_EQUAL( 
        gap_analysis.GetGapLengthSeqIds().find(kGapLength)->second.size(), 
        2 );
}
