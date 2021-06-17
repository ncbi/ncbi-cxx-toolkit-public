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
* Author:  Michael Kornbluh, NCBI
*
* File Description:
*   Unit test CSeq_entry_CI
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
void throw_exception( std::exception const & e ) {
    throw e;
}
}

#endif

namespace {
    // helper functions
    CSeq_entry_Handle s_GetEntry(const char *pchTextASN) {
        CRef<CSeq_entry> pEntry( new CSeq_entry );
        {{
            CNcbiIstrstream istr(pchTextASN);
            istr >> MSerial_AsnText >> *pEntry;
        }}
        pEntry->Parentize();

        // load the entry
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));

        return scope->AddTopLevelSeqEntry(*pEntry);
    }

    template<typename THandle>
    bool s_IsHandleABioseqSet( const THandle & data_h )
    {
        const string & sTypeName = THandle::TObject::GetTypeInfo()->GetName();
        if( sTypeName == "Bioseq-set" ) {
            return true;
        } else if( sTypeName == "Seq-entry" ) {
            return false;
        } else {
            BOOST_FAIL( "unknown type: " << sTypeName );
            return false;
        }
    }

    struct SExpectedEntry {
        const char * m_pchExpectedId;
        int          m_iExpectedDepth;
    };

    ostream & operator<<(ostream & ostrm, const SExpectedEntry & expectedEntry )
    {
        ostrm << "(expected id: '" << expectedEntry.m_pchExpectedId 
            << "', expected depth: " << expectedEntry.m_iExpectedDepth << ')';
        return ostrm;
    }

    // exact value should be something very unlikely to be used as data
    const int kEndOfExpectationArrayMarker = kMin_Int;

    template<typename TSeqEntryIter, typename THandle>
    void s_TestExpectations( const THandle & data_h,
        const SExpectedEntry expected_entries[], // last one must have a NULL id
        const int filter_modes_to_test[] = NULL, // NULL means "try all possible combos". 
        const typename TSeqEntryIter::TFlags flags_on_mask = 0, // flags which are set on for every test done in here
        const typename TSeqEntryIter::TFlags flags_to_try_on_and_off_mask = 0 // for these flags, we will do all the tests with each flag on and each flag off.  O(2^N), where N is the number of flags set
        )
    {

        if( filter_modes_to_test == NULL ) {
            static const int sc_DefaultFilterModes[] = {
                CSeq_entry::e_not_set,
                CSeq_entry::e_Seq,
                CSeq_entry::e_Set,
                kMin_Int  };
            filter_modes_to_test = sc_DefaultFilterModes;
        }

        // there must be no overlap between the two masks, otherwise it's
        // not clear what to do
        BOOST_REQUIRE( 0 == (flags_on_mask & flags_to_try_on_and_off_mask) );

        // produce all possible flag combos that we want to try
        vector<typename TSeqEntryIter::TFlags> vecAllFlagCombosToTry;
        vecAllFlagCombosToTry.push_back(flags_on_mask);

        typename TSeqEntryIter::TFlags current_flag_to_process = 1;
        for( ;
            current_flag_to_process <= flags_to_try_on_and_off_mask;
            current_flag_to_process <<= 1 ) 
        {
            if( flags_to_try_on_and_off_mask & current_flag_to_process ) {
                const size_t old_size_of_vecAllFlagCombosToTry = vecAllFlagCombosToTry.size();
                // take the combos we found so far, and add in all combinations
                // with the current flag set
                vecAllFlagCombosToTry.resize( 2 * vecAllFlagCombosToTry.size() );
                for( size_t src_idx = 0; 
                    src_idx < old_size_of_vecAllFlagCombosToTry;
                    ++src_idx ) 
                {
                    vecAllFlagCombosToTry[src_idx + old_size_of_vecAllFlagCombosToTry] =
                        (vecAllFlagCombosToTry[src_idx] | current_flag_to_process);
                }
            }
        }

        // test every combination requested
        for( size_t iFilterIdx = 0; 
            filter_modes_to_test[iFilterIdx] != kEndOfExpectationArrayMarker;
            ++iFilterIdx) 
        {
            const CSeq_entry::E_Choice eFilterMode = 
                static_cast<CSeq_entry::E_Choice>(filter_modes_to_test[iFilterIdx]);
            for( size_t iFlagIdx = 0; 
                iFlagIdx < vecAllFlagCombosToTry.size();
                ++iFlagIdx) 
            {
                const typename TSeqEntryIter::TFlags flags = vecAllFlagCombosToTry[iFlagIdx];

                TSeqEntryIter entry_ci( data_h,
                    flags,
                    eFilterMode );

                // the iteration should match what we expect at each step
                size_t iEntryIdx = 0;
                for( ;
                    entry_ci && expected_entries[iEntryIdx].m_pchExpectedId != NULL;
                    ++iEntryIdx, ++entry_ci ) 
                {
                    const SExpectedEntry & expectedEntry = expected_entries[iEntryIdx];

                    // check that name matches
                    const string & sEntryName = ( 
                        entry_ci->IsSeq() ? 
                        entry_ci->GetSeq().GetSeqId()->GetLocal().GetStr() :
                    entry_ci->GetSet().GetId().GetStr() );
                    BOOST_CHECK_MESSAGE( sEntryName == expectedEntry.m_pchExpectedId, 
                        "Expected: " << expectedEntry << ", but entry name was: " << sEntryName);

                    // check depth
                    // (iDepthOffset is a correction because when we include the given entry, 
                    // the depths are increased by one)
                    const int iDepthOffset = ( 
                        (flags & TSeqEntryIter::fIncludeGivenEntry) ? 0 : 1 );
                    const int iDepth = entry_ci.GetDepth();
                    BOOST_CHECK_MESSAGE( iDepth == (expectedEntry.m_iExpectedDepth - iDepthOffset),
                        "Expected: " << expectedEntry << ", but depth was: " << iDepth);
                }

                // check if we ended too early or too late
                if( expected_entries[iEntryIdx].m_pchExpectedId == NULL ) {
                    BOOST_CHECK_MESSAGE( ! entry_ci, "TSeqEntryIter returned extra items" );
                } else {
                    BOOST_CHECK_MESSAGE( entry_ci, "TSeqEntryIter ended too soon" );
                }
            }
        }
    }

    const char * sc_EntryIsJustBioseq =
        "Seq-entry ::= seq { id { local str \"lone\" }, inst { repr virtual, mol dna, length 100 } }";

    const char * sc_ComplexTestEntry =
        "Seq-entry ::= set {"
        "    id str \"top-level-set\","
        "    seq-set {"
        "        seq { id { local str \"foo\" }, inst { repr virtual, mol dna, length 100 } },"
        "        seq { id { local str \"bar\" }, inst { repr virtual, mol dna, length 100 } },"
        "        set {"
        "            id str \"abc\","
        "            seq-set {"
        "                set {"
        "                    id str \"some set\","
        "                    seq-set {"
        "                        seq { id { local str \"xyz\" }, inst { repr virtual, mol dna, length 100 } }"
        "                    }"
        "                },"
        "                set {"
        "                    id str \"another set\","
        "                    seq-set {"
        "                        seq { id { local str \"abcd\" }, inst { repr virtual, mol dna, length 100 } },"
        "                        seq { id { local str \"123\" }, inst { repr virtual, mol dna, length 100 } },"
        "                        seq { id { local str \"test\" }, inst { repr virtual, mol dna, length 100 } }"
        "                    }"
        "                }"
        "            }"
        "        },"
        "        seq { id { local str \"last one\" }, inst { repr virtual, mol dna, length 100 } }"
        "    }"
        "}";


    template<typename TSeqEntryIter, typename THandle>
    void sTest_EntryWithJustABioseq( const THandle & data_h )
    {
        BOOST_REQUIRE( ! s_IsHandleABioseqSet(data_h) );

        // check all the combinations that lead to just the bioseq
        SExpectedEntry expect_just_bioseq[] = {
            { "lone", 0 },
            { NULL  , kMin_Int }
        };
        int filters_that_include_bioseq[] = {
            CSeq_entry::e_not_set,
            CSeq_entry::e_Seq,
            kEndOfExpectationArrayMarker
        };
        s_TestExpectations<TSeqEntryIter>( data_h, 
            expect_just_bioseq, 
            filters_that_include_bioseq, 
            TSeqEntryIter::fIncludeGivenEntry, // include given, if possible
            TSeqEntryIter::fRecursive // try with recursion on and off
            );

        // check all the combinations that lead to nothing
        SExpectedEntry expect_nothing[] = {
            { NULL  , kMin_Int }
        };
        int filters_that_exclude_bioseq[] = {
            CSeq_entry::e_Set,
            kEndOfExpectationArrayMarker
        };
        s_TestExpectations<TSeqEntryIter>( data_h, 
            expect_nothing, 
            filters_that_exclude_bioseq,
            TSeqEntryIter::fIncludeGivenEntry, // include given, if possible
            TSeqEntryIter::fRecursive // try with recursion on and off 
            );
        s_TestExpectations<TSeqEntryIter>( data_h, 
            expect_nothing, 
            NULL, // any filter
            0, // exclude given
            TSeqEntryIter::fRecursive // try with recursion on and off
            );
    }

    template<typename TSeqEntryIter, typename THandle>
    void sTest_ComplexEntry(const THandle & data_h )
    {
        // if top is bioseq-set, then we sometimes need an offset
        // to make sure that we skip the given entry, since that's
        // impossible to have if the given is a bioseq-set
        const size_t iHandleOffset = ( s_IsHandleABioseqSet(data_h) ? 1 : 0 );

        const int filtering_off[] = {
            CSeq_entry::e_not_set,
            kEndOfExpectationArrayMarker
        };
        const int bioseqs_only[] = {
            CSeq_entry::e_Seq,
            kEndOfExpectationArrayMarker
        };
        const int bioseq_sets_only[] = {
            CSeq_entry::e_Set,
            kEndOfExpectationArrayMarker
        };

        // recursive
        // no filter
        {{
            const SExpectedEntry expectedEntry[] = {
                { "top-level-set", 0 },
                { "foo",           1 },
                { "bar",           1 },
                { "abc",           1 },
                { "some set",      2 },
                { "xyz",           3 },
                { "another set",   2 },
                { "abcd",          3 },
                { "123",           3 },
                { "test",          3 },
                { "last one",      1 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                (iHandleOffset + expectedEntry),
                filtering_off,
                TSeqEntryIter::fRecursive | TSeqEntryIter::fIncludeGivenEntry,
                0 );
            // exclude given, so we just skip the first one
            s_TestExpectations<TSeqEntryIter>( data_h,
                (1 + expectedEntry),
                filtering_off,
                TSeqEntryIter::fRecursive,
                0 );
        }}

        // recursive
        // bioseqs only
        {{
            const SExpectedEntry expectedEntry[] = {
                { "foo",           1 },
                { "bar",           1 },
                { "xyz",           3 },
                { "abcd",          3 },
                { "123",           3 },
                { "test",          3 },
                { "last one",      1 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                expectedEntry,
                bioseqs_only,
                TSeqEntryIter::fRecursive,
                TSeqEntryIter::fIncludeGivenEntry // including given entry shouldn't matter
                );
        }}

        // recursive
        // bioseq-sets only
        {{
            const SExpectedEntry expectedEntry[] = {
                { "top-level-set", 0 },
                { "abc",           1 },
                { "some set",      2 },
                { "another set",   2 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                (iHandleOffset + expectedEntry),
                bioseq_sets_only,
                TSeqEntryIter::fRecursive | TSeqEntryIter::fIncludeGivenEntry,
                0);
            // exclude given, so just skip the first one
            s_TestExpectations<TSeqEntryIter>( data_h,
                (1 + expectedEntry),
                bioseq_sets_only,
                TSeqEntryIter::fRecursive,
                0 );
        }}

        // non-recursive
        // no filter
        {{
            const SExpectedEntry expectedEntry[] = {
                { "top-level-set", 0 },
                { "foo",           1 },
                { "bar",           1 },
                { "abc",           1 },
                { "last one",      1 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                (iHandleOffset + expectedEntry),
                filtering_off,
                TSeqEntryIter::fIncludeGivenEntry,
                0 );
            // exclude given, so we just skip the first one
            s_TestExpectations<TSeqEntryIter>( data_h,
                (1 + expectedEntry),
                filtering_off,
                0,
                0 );
        }}

        // non-recursive
        // bioseqs only
        {{
            const SExpectedEntry expectedEntry[] = {
                { "foo",           1 },
                { "bar",           1 },
                { "last one",      1 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                expectedEntry,
                bioseqs_only,
                0,
                TSeqEntryIter::fIncludeGivenEntry // same regardless of whether given entry allowed
                );
        }}

        // non-recursive
        // bioseq-sets only
        {{
            const SExpectedEntry expectedEntry[] = {
                { "top-level-set", 0 },
                { "abc",           1 },
                { NULL,            kMin_Int }
            };
            // include given
            s_TestExpectations<TSeqEntryIter>( data_h,
                (iHandleOffset + expectedEntry),
                bioseq_sets_only,
                TSeqEntryIter::fIncludeGivenEntry,
                0 );
            // exclude given, so we just skip the first one
            s_TestExpectations<TSeqEntryIter>( data_h,
                (1 + expectedEntry),
                bioseq_sets_only,
                0,
                0 );
        }}
    }
}

BOOST_AUTO_TEST_CASE(Test_EntryWithJustABioseq)
{
    CSeq_entry_Handle just_bioseq_entry_h = s_GetEntry( sc_EntryIsJustBioseq );

    // test when given is a seq-entry that is a mere bioseq
    sTest_EntryWithJustABioseq<CSeq_entry_CI>(just_bioseq_entry_h);
    sTest_EntryWithJustABioseq<CSeq_entry_I>(just_bioseq_entry_h.GetEditHandle());
}

BOOST_AUTO_TEST_CASE(Test_ComplexEntry)
{
    CSeq_entry_Handle complex_entry_h = s_GetEntry( sc_ComplexTestEntry );

    // test when given is a seq-entry
    sTest_ComplexEntry<CSeq_entry_CI>(complex_entry_h);
    sTest_ComplexEntry<CSeq_entry_I>(complex_entry_h.GetEditHandle());   

    // test when given is a bioseq-set
    sTest_ComplexEntry<CSeq_entry_CI>(complex_entry_h.GetSet());
    sTest_ComplexEntry<CSeq_entry_I>(complex_entry_h.GetSet().GetEditHandle());
}
