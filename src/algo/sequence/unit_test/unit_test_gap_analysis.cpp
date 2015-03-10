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
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>

#include <algo/sequence/gap_analysis.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {

    // for convenience
    typedef CGapAnalysis GA;

    void s_PrintSummary(
        const string & summary_name,
        GA::EGapType eGapType,
        const GA::TVectorGapLengthSummary & summary )
    {
        cerr << "BEGIN " << GA::s_GapTypeToStr(eGapType)
             << " SUMMARY for " << summary_name << endl;
        ITERATE( GA::TVectorGapLengthSummary, summary_iter, 
            summary ) 
        {
            const GA::SOneGapLengthSummary & one_summary =
                **summary_iter;
            cerr << "\tlen: " << one_summary.gap_length 
                << "\tnum seqs: " << one_summary.num_seqs
                << "\tnum gaps: " << one_summary.num_gaps << endl;
        }
        cerr << "END GAP SUMMARY" << endl;
    }

    void s_PrintSummaryForAllGapTypes(
        const string & summary_name,
        const CGapAnalysis & gap_analysis )
    {
        cout << endl;
        cout << "GAP SUMMARY FOR ALL GAP_TYPES" << endl;

        GA::EGapType gap_types[] = {
            GA::eGapType_SeqGap,
            GA::eGapType_UnknownBases
        };
        ITERATE_0_IDX(gap_idx, ArraySize(gap_types)) {
            GA::EGapType eGapType = gap_types[gap_idx];
            s_PrintSummary(
                summary_name,
                eGapType,
                *gap_analysis.GetGapLengthSummary(eGapType));
        }
        cout << endl;
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

    void LoadSeqEntryIntoGapAnalysis(
        CGapAnalysis & gap_analysis,
        CRef<CSeq_entry> pSeqEntry,
        GA::TAddFlag add_flags,
        GA::TFlag fFlags = 0,
        CScope *p_scope = NULL,
        bool do_rev_comp = false)
    {
        CRef<CScope> scope(
            p_scope
            ? p_scope
            : CreateScope().GetPointer() );

        if( do_rev_comp ) {
            for( CTypeIterator<CSeq_entry> entry = Begin(*pSeqEntry);
                 entry; ++entry)
            {
                if( entry->IsSeq() ) {
                    ReverseComplement(
                        entry->SetSeq().SetInst(), scope.GetPointer());
                }
            }
        }

        CSeq_entry_Handle entry_h =
            scope->AddTopLevelSeqEntry(*pSeqEntry);
        BOOST_REQUIRE( entry_h );

        gap_analysis.AddSeqEntryGaps(
            entry_h,
            CSeq_inst::eMol_not_set,
            CBioseq_CI::eLevel_All,
            add_flags,
            fFlags);
    }

    AutoPtr<GA::TVectorGapLengthSummary> AnalyzeSeqEntryTextAsn(
        CNcbiIstream & in_text_asn,
        GA::TAddFlag add_flags = GA::fAddFlag_All,
        GA::EGapType gap_type = GA::eGapType_All,
        GA::TFlag fFlags = 0)
    {
        in_text_asn.seekg(0);

        CGapAnalysis gap_analysis;
        CRef<CSeq_entry> pSeqEntry(new CSeq_entry);
        in_text_asn >> MSerial_AsnText >> *pSeqEntry;

        LoadSeqEntryIntoGapAnalysis(
            gap_analysis, pSeqEntry, add_flags, fFlags);

        return gap_analysis.GetGapLengthSummary(gap_type);
    }

    struct SExpectedResult {
        size_t gap_length;
        size_t num_seqs;
        size_t num_gaps;

        bool empty(void) const {
            // all zeros
            return 0 == (gap_length | num_seqs | num_gaps); }
    };

    static const SExpectedResult s_ExpectedResultEnd = {0, 0, 0};

    ostream& operator<<(
        ostream& s, const SExpectedResult & expected_result)
    {
        s << "SExpectedResult ("
          << "gap_length: " << expected_result.gap_length
          << ", num_seqs: " << expected_result.num_seqs
          << ", num_gaps: " << expected_result.num_gaps << ")";
        return s;
    }

    ostream& operator<<(
        ostream& s, const SExpectedResult expected_results[] )
    {
        s << "The expected results: (" << endl;

        for( size_t idx = 0; ! expected_results[idx].empty(); ++idx ) {
            const SExpectedResult & one_expected_result =
                expected_results[idx];
            s << one_expected_result << endl;
        }

        s << ")" << endl;
        return s;
    }

    void CheckExpectedResults(
        const GA::TVectorGapLengthSummary & basic_gap_summary,
        // last expected result is {0, 0, 0} to indicate we're at
        // the end.
        const SExpectedResult expected_results[])
    {
        cout << "CheckExpectedResults basic_gap_summary: "
             << basic_gap_summary << endl;

        size_t idx = 0;
        for( ; idx < basic_gap_summary.size(); ++idx) {
            const GA::SOneGapLengthSummary & one_gap_summary =
                *basic_gap_summary[idx];
            const SExpectedResult & one_expected_result =
                expected_results[idx];
            BOOST_CHECK_EQUAL(
                one_gap_summary.gap_length, one_expected_result.gap_length);
            BOOST_CHECK_EQUAL(
                one_gap_summary.num_seqs, one_expected_result.num_seqs);
            BOOST_CHECK_EQUAL(
                one_gap_summary.num_gaps, one_expected_result.num_gaps);
        }
        BOOST_CHECK( expected_results[idx].empty() );
    }

    // expected results when getting summary for each type
    struct SGapTypeExpectedResult {
        const GA::EGapType gap_type;
        // allow up to that many SExpectedResult's.
        // Feel free to adjust this number as needed.
        const SExpectedResult expected_result[5];
    };
    
    void CheckGapTypeExpectedResult(
        const CTempString & test_name,
        const SGapTypeExpectedResult & gap_type_expected_result,
        const GA::TVectorGapLengthSummary & basic_gap_summary)
    {
        cout << "In " << test_name << " running expected results for "
             << GA::s_GapTypeToStr(gap_type_expected_result.gap_type) << ": "
             << gap_type_expected_result.expected_result << endl;

        CheckExpectedResults(
            basic_gap_summary,
            gap_type_expected_result.expected_result);
    }
}

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("basic-data", "InputFile",
                     "This is the basic input file used to run the test",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey(
        "in-letter-gap-data", "InputFile",
        "This is the input file used to run the "
        "'gaps as run of unknown bases' test",
        CArgDescriptions::eInputFile);
    arg_desc->AddKey(
        "mixed-gap-type-data", "InputFile",
        "This is the input file used to run the "
        "'gaps as run of unknown bases and seq-gaps' test, distinguishing "
        "between the two.",
        CArgDescriptions::eInputFile);

}

BOOST_AUTO_TEST_CASE(TestBasic)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIfstream basic_data_fstrm(args["basic-data"].AsString().c_str());
    AutoPtr<GA::TVectorGapLengthSummary> basic_gap_summary =
        AnalyzeSeqEntryTextAsn(
            basic_data_fstrm, GA::fAddFlag_All, GA::eGapType_All,
            GA::fFlag_IncludeEndGaps);
    s_PrintSummary(
        "TestBasic - basic-data", GA::eGapType_All, *basic_gap_summary);

    SExpectedResult expected_results[] = {
        { 5, 1, 1 },
        { 10, 1, 1 },
        { 40, 1, 1 },
        { 101, 2, 3 },
        s_ExpectedResultEnd
    };
    CheckExpectedResults(*basic_gap_summary, expected_results);
}

BOOST_AUTO_TEST_CASE(TestSeqIdComparison)
{
    // make sure Seq-ids are compared by contents, not pointer address.

    CRef<CSeq_id> pSeqId1( new CSeq_id("lcl|Seq_no_gaps") );
    CRef<CSeq_id> pSeqId2( new CSeq_id("lcl|Seq_no_gaps") );
    CRef<CSeq_id> pSeqId3( new CSeq_id("lcl|Seq_misc_seq_gaps_and_raw_bases"));

    const GA::TGapLength kGapLength = 10;
    const TSeqPos kBioseqlen = 100;

    CGapAnalysis gap_analysis;
    gap_analysis.AddGap(
        GA::eGapType_All, pSeqId1, kGapLength, kBioseqlen, 2, 12);
    gap_analysis.AddGap(
        GA::eGapType_All, pSeqId2, kGapLength, kBioseqlen, 20, 30);
    gap_analysis.AddGap(
        GA::eGapType_All, pSeqId3, kGapLength, kBioseqlen, 40, 50);

    BOOST_CHECK_EQUAL( 
        gap_analysis.GetGapLengthSeqIds(GA::eGapType_All).find(
            kGapLength)->second.size(),
        2u );
}

BOOST_AUTO_TEST_CASE(TestGapsAsLetters)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIfstream gap_data_strm(args["in-letter-gap-data"].AsString().c_str());
    AutoPtr<GA::TVectorGapLengthSummary> basic_gap_summary =
        AnalyzeSeqEntryTextAsn(
            gap_data_strm, GA::fAddFlag_IncludeUnknownBases,
            GA::eGapType_All, GA::fFlag_IncludeEndGaps);
    s_PrintSummary(
        "TestGapsAsLetters - in-letter-gap-data",
        GA::eGapType_All, *basic_gap_summary);

    SExpectedResult expected_results[] = {
        { 1, 1, 2 },
        { 2, 1, 1 },
        { 10, 1, 1 },
        s_ExpectedResultEnd
    };
    CheckExpectedResults(*basic_gap_summary, expected_results);
}

BOOST_AUTO_TEST_CASE(TestEndGaps)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIfstream gap_data_fstrm(
        args["mixed-gap-type-data"].AsString().c_str());
    CRef<CSeq_entry> pSeqEntry(new CSeq_entry);
    gap_data_fstrm >> MSerial_AsnText >> *pSeqEntry;

    CGapAnalysis gap_analysis;
    LoadSeqEntryIntoGapAnalysis(
        gap_analysis, pSeqEntry, GA::fAddFlag_All,
        GA::fFlag_IncludeEndGaps);
    s_PrintSummaryForAllGapTypes(
        "TestEndGaps - mixed-gap-type-data", gap_analysis);

    const SGapTypeExpectedResult gap_type_expected_results[] = {
        {
            GA::eGapType_All,
            {
                { 2, 1, 1},
                { 3, 1, 1},
                { 8, 1, 1 },
                { 23, 1, 2 },
                s_ExpectedResultEnd
            } },
        {
            GA::eGapType_SeqGap,
            {
                { 23, 1, 1 },
                s_ExpectedResultEnd
            }
        },
        {
            GA::eGapType_UnknownBases,
            {
                { 2, 1, 1},
                { 3, 1, 1},
                { 8, 1, 1 },
                { 23, 1, 1 },
                s_ExpectedResultEnd
            }
        }
    };
    ITERATE_0_IDX(gap_type_idx, ArraySize(gap_type_expected_results)) {

        CheckGapTypeExpectedResult(
            "TestEndGaps",
            gap_type_expected_results[gap_type_idx],
            *gap_analysis.GetGapLengthSummary(
                gap_type_expected_results[gap_type_idx].gap_type));
    }
}

BOOST_AUTO_TEST_CASE(TestAllGapTypes)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();



    // recall that end gaps will be ignored here
    const SGapTypeExpectedResult gap_type_expected_results[] = {
        {
            GA::eGapType_All,
            {
                { 8, 1, 1 },
                { 23, 1, 2 },
                s_ExpectedResultEnd
            } },
        {
            GA::eGapType_SeqGap,
            {
                { 23, 1, 1 },
                s_ExpectedResultEnd
            }
        },
        {
            GA::eGapType_UnknownBases,
            {
                { 8, 1, 1 },
                { 23, 1, 1 },
                s_ExpectedResultEnd
            }
        }
    };

    // should get same result regardless of strandedness
    ITERATE_BOTH_BOOL_VALUES(is_minus_strand) {

        cout << "TestAllGapTypes "
             << (is_minus_strand ? "minus" : "plus")
             << " strand" << endl;

        CNcbiIfstream gap_data_fstrm(
            args["mixed-gap-type-data"].AsString().c_str());
        CRef<CSeq_entry> pSeqEntry(new CSeq_entry);
        gap_data_fstrm >> MSerial_AsnText >> *pSeqEntry;

        CGapAnalysis gap_analysis;
        LoadSeqEntryIntoGapAnalysis(
            // it complements, too, but that doesn't matter
            gap_analysis, pSeqEntry, GA::fAddFlag_All, 0, NULL, true);
        s_PrintSummaryForAllGapTypes(
            "TestAllGapTypes - mixed-gap-type-data", gap_analysis);

        ITERATE_0_IDX(gap_type_idx, ArraySize(gap_type_expected_results)) {

            const SGapTypeExpectedResult & gap_type_expected_result =
                gap_type_expected_results[gap_type_idx];

            CheckGapTypeExpectedResult(
                "TestAllGapTypes",
                gap_type_expected_result,
                *gap_analysis.GetGapLengthSummary(
                    gap_type_expected_result.gap_type));
        }
    }
}

// so we can print CSerialObject's from a debugger
void PS(const CSerialObject * obj)
{
    if( obj ) {
        cout << MSerial_AsnText << *obj << endl;
    } else {
        cout << "(NULL)" << endl;
    }
}
