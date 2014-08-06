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

    CRef<CScope> CreateScope()
    {
        DEFINE_STATIC_FAST_MUTEX(scope_mtx);
        CFastMutexGuard guard(scope_mtx);

        static CRef<CObjectManager> om;
        if( ! om ) {
            om.Reset(CObjectManager::GetInstance());
            CGBDataLoader::RegisterInObjectManager(*om);
        }

        CRef<CScope> s_scope( new CScope(*om));
        s_scope->AddDefaults();
        return s_scope;
    }

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> AnalyzeSeqEntryTextAsn(
        CNcbiIstream & in_text_asn,
        CGapAnalysis::TAddFlag add_flags = CGapAnalysis::fAddFlag_Default)
    {
        CRef<CSeq_entry> pSeqEntry( new CSeq_entry );
        in_text_asn >> MSerial_AsnText >> *pSeqEntry;

        CSeq_entry_Handle entry_h =
            CreateScope()->AddTopLevelSeqEntry(*pSeqEntry);
        BOOST_REQUIRE( entry_h );

        CGapAnalysis gap_analysis; 
        gap_analysis.AddSeqEntryGaps(
            entry_h,
            CSeq_inst::eMol_not_set,
            CBioseq_CI::eLevel_All,
            add_flags);

        return gap_analysis.GetGapLengthSummary();
    }

    struct SExpectedResult {
        size_t gap_length;
        size_t num_seqs;
        size_t num_gaps;
    };
    void CheckExpectedResults(
        CGapAnalysis::TVectorGapLengthSummary & basic_gap_summary,
        SExpectedResult expected_results[],
        size_t expected_results_len)
    {
        BOOST_CHECK_EQUAL( basic_gap_summary.size(), expected_results_len );
        for( size_t idx = 0; idx < basic_gap_summary.size() ; ++idx ) {
            const CGapAnalysis::SOneGapLengthSummary & one_gap_summary =
                basic_gap_summary[idx];
            const SExpectedResult & one_expected_result =
                expected_results[idx];
            BOOST_CHECK_EQUAL(
                one_gap_summary.gap_length, one_expected_result.gap_length);
            BOOST_CHECK_EQUAL(
                one_gap_summary.num_seqs, one_expected_result.num_seqs);
            BOOST_CHECK_EQUAL(
                one_gap_summary.num_gaps, one_expected_result.num_gaps);
        }
    }
}

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("in-data", "InputFile",
                     "This is the basic input file used to run the test",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey(
        "in-letter-gap-data", "InputFile",
        "This is the input file used to run the "
        "'gaps as run of unknown bases' test",
        CArgDescriptions::eInputFile);
}

BOOST_AUTO_TEST_CASE(TestBasic)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> basic_gap_summary =
        AnalyzeSeqEntryTextAsn(args["in-data"].AsInputFile());
    s_PrintSummary(*basic_gap_summary);

    SExpectedResult expected_results[] = {
        { 5, 1, 1 },
        { 10, 1, 1 },
        { 40, 1, 1 },
        { 101, 2, 3 },
    };
    CheckExpectedResults(
        *basic_gap_summary, expected_results, ArraySize(expected_results));
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

BOOST_AUTO_TEST_CASE(TestGapsAsLetters)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> basic_gap_summary =
        AnalyzeSeqEntryTextAsn(
            args["in-letter-gap-data"].AsInputFile(),
            CGapAnalysis::fAddFlag_IncludeUnknownBases);
    s_PrintSummary(*basic_gap_summary);

    SExpectedResult expected_results[] = {
        { 1, 1, 2 },
        { 2, 1, 1 },
        { 10, 1, 1 }
    };
    CheckExpectedResults(
        *basic_gap_summary, expected_results, ArraySize(expected_results));
}
