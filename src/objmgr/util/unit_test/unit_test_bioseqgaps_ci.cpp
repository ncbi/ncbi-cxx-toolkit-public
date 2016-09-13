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
* Author:  Michael Kornbluh, Pavel Ivanov, NCBI
*
* File Description:
*   Test CBioseqGaps_CI.
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

#include <objmgr/util/sequence.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <corelib/ncbicntr.hpp>
#include <objects/seq/Seq_ext.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);
USING_SCOPE(std);

namespace {

    // makes a string of length "len" which consists of
    // cycle_chars repeated over and over.
    // For example, s_CyclicalString(11, "abc")
    // would produce "abcabcabcab".
    string s_CyclicalString(size_t len, const CTempString & cycle_chars )
    {
        string sAnswer;
        sAnswer.reserve(len);

        const size_t num_chars = cycle_chars.length();
        ITERATE_0_IDX(idx, len) {
            sAnswer += cycle_chars[idx % num_chars];
        }

        return sAnswer;
    }

    // returns "ACGTACGTACGT..." string of length len.
    // (Note the cyclical "ACGT")
    string s_GoodNucs(size_t len) {
        return s_CyclicalString(len, "ACGT");
    }

    string s_NucGap(size_t len) {
        return string(len, 'N');
    }

    string s_GoodProts(size_t len) 
    {
        // A non-ACGT must be first so that even a sequence
        // of one residue will be recognized as a protein sequence.
        // Other than that, order doesn't really matter.
        return s_CyclicalString(len, "YWVTSRQPNMLKIHGFEDCA");
    }
    
    string s_ProtGap(size_t len) {
        return string(len, 'X');
    }

    CRef<CSeq_id> s_GetNextSeqId(void)
    {
        static CAtomicCounter_WithAutoInit s_NextLocalIdNum;

        return Ref( new CSeq_id(
            "lcl|seq" + NStr::NumericToString(
            s_NextLocalIdNum.Add(1))) );
    }

    CRef<CBioseq> 
        s_SeqToBioseq( 
            const CTempString & seq )
    {
        // auto-detect whether seq is nuc or amino acid
        CSeq_inst::EMol eMol = CSeq_inst::eMol_na;
        if( seq.find_first_not_of("ACGTN") != string::npos ) {
            eMol = CSeq_inst::eMol_aa;
        }

        CRef<CBioseq> pBioseq( new CBioseq );

        // CRef<CSeq_id> pSeqId( new CSeq_id("lcl|42"));
        pBioseq->SetId().push_back( s_GetNextSeqId() );

        CSeq_inst & seq_inst = pBioseq->SetInst();
        seq_inst.SetRepr( CSeq_inst::eRepr_raw );
        seq_inst.SetLength( (CSeq_inst::TLength)seq.length() );
        seq_inst.SetMol( eMol );
        if( eMol == CSeq_inst::eMol_na ) {
            seq_inst.SetSeq_data().SetIupacna().Set( seq );
        } else {
            BOOST_REQUIRE( eMol == CSeq_inst::eMol_aa );
            seq_inst.SetSeq_data().SetIupacaa().Set( seq );
        }

        return pBioseq;
    }

    CSeq_entry_Handle
        s_SeqsToEntry(
            const vector<string> & seqs )
    {
        BOOST_REQUIRE( ! seqs.empty() );

        // build CSeq_entry
        CRef<CSeq_entry> pSeqEntry( new CSeq_entry );
        CBioseq_set_Base::TSeq_set & seq_set =
            pSeqEntry->SetSet().SetSeq_set();
        ITERATE(vector<string>, seq_it, seqs) {
            CRef<CBioseq> pNewBioseq = s_SeqToBioseq(*seq_it);
            CRef<CSeq_entry> pBioseqHoldingSeqEntry( new CSeq_entry );
            pBioseqHoldingSeqEntry->SetSeq( *pNewBioseq );
            seq_set.push_back( pBioseqHoldingSeqEntry );
        }

        // get a handle
        CRef<CScope> pScope( new CScope( *CObjectManager::GetInstance() ) );
        return pScope->AddTopLevelSeqEntry( *pSeqEntry );
    }

    struct SGapInfo {
        TSeqPos start_pos;
        TSeqPos length;

        bool operator ==( const SGapInfo & rhs ) const 
        {
            return start_pos == rhs.start_pos &&
                length == rhs.length;
        }

        bool operator !=( const SGapInfo & rhs ) const 
        {
            return ! (*this == rhs);
        }

        void Write(ostream & strm) const {
            strm << "{ start: " << start_pos << ", len: " << length << " }";
        }
    };

    ostream & operator << (ostream & strm, const SGapInfo & info)
    {
        info.Write(strm);
        return strm;
    }

    SGapInfo makeGapInfo( TSeqPos start_pos, TSeqPos length )
    {
        SGapInfo gap_info = { start_pos, length };
        return gap_info;
    }

    typedef vector<SGapInfo> TGapInfoVec;

    void s_TestBioseqs(const vector<string> & seqs,
        const TGapInfoVec & expected_gaps,
        CBioseqGaps_CI::Params params = CBioseqGaps_CI::Params() )
    {

        CSeq_entry_Handle entry_h = s_SeqsToEntry(seqs);

        TGapInfoVec result_gaps;

        CBioseqGaps_CI gaps_ci( entry_h, params );
        for( ; gaps_ci; ++gaps_ci ) {
            SGapInfo new_gap_info = { gaps_ci->start_pos, gaps_ci->length };
            result_gaps.push_back( new_gap_info );
        }

        // using a finished iterator
        // should throw an exception
        BOOST_CHECK_THROW( *gaps_ci, CException );
        BOOST_CHECK_THROW( ++gaps_ci, CException );
        BOOST_CHECK_THROW( *gaps_ci, CException );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            result_gaps.begin(), result_gaps.end(),
            expected_gaps.begin(), expected_gaps.end() );
    }

    void s_TestOneBioseq(const CTempString & seq,
        const TGapInfoVec & expected_gaps,
        CBioseqGaps_CI::Params params = CBioseqGaps_CI::Params() )
    {
        vector<string> seqs;
        seqs.push_back( seq );
        s_TestBioseqs( seqs, expected_gaps, params );
    }
}

#define PASTE2(x, y) x ## y
#define PASTE(x, y) PASTE2(x, y)

// This macro might possibly be added to 
// include/corelib/ncbimisc.hpp
// at some point, if it can be cleaned up.
#define ITERATE_RAW_ARRAY(Type, Var, ...)              \
    const Type PASTE(raw_data_299197741, __LINE__)[] = __VA_ARGS__;                      \
    for(const Type *Var = & PASTE(raw_data_299197741, __LINE__)[0];                \
        Var < PASTE(raw_data_299197741, __LINE__) + \
            ArraySize(PASTE(raw_data_299197741, __LINE__)); \
        ++Var)                                                          \

#define ONE_BIOSEQ_TEST(_seq, ...)                   \
    do {                                                        \
        SGapInfo raw_expected_gaps[] = __VA_ARGS__;             \
        TGapInfoVec expected_gaps;                              \
        copy( &raw_expected_gaps[0],                            \
              &raw_expected_gaps[ArraySize(raw_expected_gaps)], \
              back_inserter(expected_gaps) );                   \
        s_TestOneBioseq(_seq, expected_gaps);                   \
    } while(false)

#define ONE_BIOSEQ_TEST_EXPECT_NO_GAPS(_seq) \
    s_TestOneBioseq(_seq, TGapInfoVec() );

BOOST_AUTO_TEST_CASE(CBioseqGaps_CI_OneBioseqTests)
{
    // test a few simple cases
    ITERATE_RAW_ARRAY( string, psPrefix, {s_GoodNucs(20), kEmptyStr} ) {
    ITERATE_RAW_ARRAY( string, psSuffix, {s_GoodNucs(20), kEmptyStr} ) {

    // gap of size 9 should not be found, regardless of position
    ITERATE_RAW_ARRAY( string, psGap, {s_NucGap(10), kEmptyStr} ) {
        ONE_BIOSEQ_TEST_EXPECT_NO_GAPS(
            *psPrefix + *psGap + *psSuffix );
    }

    // gap of size 10 or more should be found, regardless of position
    ITERATE_RAW_ARRAY( string, psGap, {s_NucGap(11), s_NucGap(15)} ) {
        ONE_BIOSEQ_TEST(
            *psPrefix + *psGap + *psSuffix,
            { { (TSeqPos) psPrefix->length(), (TSeqPos) psGap->length() } });
    }

    }
    }


    // test lines with multiple gaps
    ITERATE_RAW_ARRAY( int, piPrefixLen, {20, 0} ) {
    ITERATE_RAW_ARRAY( int, piFirstGapLen,  { 1, 5, 9, 10, 11, 50 }) {
    ITERATE_RAW_ARRAY( int, piMiddleGoodBasesRun,  { 1, 10, 23 }) {
    ITERATE_RAW_ARRAY( int, piSecondGapLen, { 1, 5, 9, 10, 11, 50 }) {
    ITERATE_RAW_ARRAY( int, piSuffixLen, {20, 0} ) {
        TGapInfoVec expected_gaps;
        if( *piFirstGapLen > 10 ) {
            expected_gaps.push_back( makeGapInfo(*piPrefixLen, *piFirstGapLen) );
        }
        if( *piSecondGapLen > 10 ) {
            SGapInfo second_gap_info = { 
                (TSeqPos) *piPrefixLen + *piFirstGapLen + *piMiddleGoodBasesRun,
                (TSeqPos) *piSecondGapLen
            };
            expected_gaps.push_back( second_gap_info );
        }
        s_TestOneBioseq(
            s_GoodNucs(*piPrefixLen) + s_NucGap(*piFirstGapLen) + 
            s_GoodNucs(*piMiddleGoodBasesRun) + s_NucGap(*piSecondGapLen) + 
            s_GoodNucs(*piSuffixLen), 
            expected_gaps);  
    }
    }
    }
    }
    }

    // see if the max number of gaps functionality works
    ITERATE_RAW_ARRAY( Uint4, piGapsInInput, {0, 1, 2, 5, 10, 100} ) {
    ITERATE_RAW_ARRAY( Uint4, piMaxGapsToFind, {1, 2, 5, 10, 100} ) {
        const string kSeparator = s_GoodNucs(17);
        const string kGap       = s_NucGap(19);

        // build sequence
        string sSeq;
        sSeq += kSeparator;
        ITERATE_0_IDX(dummy, *piGapsInInput) {
            sSeq += kGap;
            sSeq += kSeparator;
        }

        // build expected gaps array
        TGapInfoVec expected_gaps;
        ITERATE_0_IDX(gap_idx, min(*piGapsInInput, *piMaxGapsToFind) ) {
            SGapInfo gap_info = { 
                (TSeqPos) kSeparator.length(),
                (TSeqPos) kGap.length()
            };
            gap_info.start_pos += (TSeqPos)
                (gap_idx * (kSeparator.length() + kGap.length()));
            expected_gaps.push_back(gap_info);
        }

        // run the test
        CBioseqGaps_CI::Params params;
        params.max_num_gaps_per_seq = *piMaxGapsToFind;
        s_TestOneBioseq( sSeq, expected_gaps, params );

    }
    }

    // test alternate max ignored gap size
    ITERATE_RAW_ARRAY( int, piGapLen, {1, 2, 5, 10, 100} ) {
    ITERATE_RAW_ARRAY( int, piMaxGapLenToIgnore, {0, 1, 2, 5, 10, 100} ) {
        // build seq
        const string kSeparator = s_GoodNucs(23);
        string sSeq = kSeparator + s_NucGap(*piGapLen) + kSeparator;

        // build expected gaps array
        TGapInfoVec expected_gaps;
        if( *piGapLen > *piMaxGapLenToIgnore ) {
            expected_gaps.push_back(makeGapInfo((TSeqPos)kSeparator.length(), *piGapLen));
        }

        // run test
        CBioseqGaps_CI::Params params;
        params.max_gap_len_to_ignore = *piMaxGapLenToIgnore;
        s_TestOneBioseq( sSeq, expected_gaps, params );
    }
    }
}

BOOST_AUTO_TEST_CASE(MultiBioseqTests)
{
    // test with max number of sequences and/or max gaps per sequence
    ITERATE_BOTH_BOOL_VALUES(bLimitNumSeqs) {
    ITERATE_BOTH_BOOL_VALUES(bLimitGapsPerSeq) {
        const string kFirstSeq = 
            s_GoodNucs(17) + s_NucGap(23) + s_GoodNucs(47) +
            s_NucGap(12) + s_GoodNucs(19) + s_NucGap(48);
        const string kSecondSeq = 
            s_GoodNucs(28) + s_NucGap(7) + s_GoodNucs(47) +
            s_NucGap(42) + s_GoodNucs(47) + s_NucGap(31);

        // build expected gaps array
        TGapInfoVec expected_gaps;
        expected_gaps.push_back( makeGapInfo(17, 23) );
        if( ! bLimitGapsPerSeq ) {
            expected_gaps.push_back( makeGapInfo(17 + 23 + 47, 12) );
            expected_gaps.push_back( makeGapInfo(17 + 23 + 47 + 12 + 19, 48) );
        }
        if( ! bLimitNumSeqs ) {
            expected_gaps.push_back( makeGapInfo(28 + 7 + 47, 42) );
            if( ! bLimitGapsPerSeq ) {
                expected_gaps.push_back( makeGapInfo(28 + 7 + 47+ 42 + 47, 31) );
            }
        }

        vector<string> vecSeqs;
        vecSeqs.push_back( kFirstSeq );
        vecSeqs.push_back( kSecondSeq );

        // run the test
        CBioseqGaps_CI::Params params;
        if( bLimitNumSeqs ) {
            params.max_num_seqs = 1;
        }
        if( bLimitGapsPerSeq ) {
            params.max_num_gaps_per_seq = 1;
        }
        s_TestBioseqs( vecSeqs, expected_gaps, params );
    }
    }
}

BOOST_AUTO_TEST_CASE(ProteinTests)
{
    // basic protein test
    {
        const string kBasicProtein = 
            s_GoodProts(17) + s_ProtGap(24) +
            s_GoodProts(4)  + s_ProtGap(48) +
            s_GoodProts(27) + s_ProtGap(8) +
            s_GoodProts(56) + s_ProtGap(11) +
            s_GoodProts(90) + s_ProtGap(9) +
            s_GoodProts(11);
        ONE_BIOSEQ_TEST(
            kBasicProtein,
            { {17, 24}, 
              {17 + 24 + 4, 48},
              {17 + 24 + 4 + 48 + 27 + 8 + 56, 11 } } );
    }

    // test the molecule filter
    {
        // build the vector of seqs to use for the test below
        vector<string> vecSeqs;

        vecSeqs.push_back( s_GoodNucs(11)  + s_NucGap(12)  + s_GoodNucs(14)  );
        vecSeqs.push_back( s_GoodProts(4)  + s_ProtGap(17) + s_GoodProts(14) );
        vecSeqs.push_back( s_GoodNucs(9)   + s_NucGap(26)  + s_GoodNucs(14)  );
        vecSeqs.push_back( s_GoodNucs(24)  + s_NucGap(33)  + s_GoodNucs(14)  );
        vecSeqs.push_back( s_GoodProts(89) + s_ProtGap(22) + s_GoodProts(14) );
        vecSeqs.push_back( s_GoodProts(30) + s_ProtGap(11) + s_GoodProts(14) );
        vecSeqs.push_back( s_GoodNucs(10)  + s_NucGap(51)  + s_GoodNucs(14)  );


        ITERATE_BOTH_BOOL_VALUES(bIncludeNucSeqs) {
        ITERATE_BOTH_BOOL_VALUES(bIncludeProtSeqs) {

            if( ! bIncludeNucSeqs && ! bIncludeProtSeqs ) {
                // the filter has no value that would make it
                // skip both nuc and prot sequences.
                continue;
            }

            // build expected gaps array
            TGapInfoVec expected_gaps;
            if( bIncludeNucSeqs ) expected_gaps.push_back(makeGapInfo(11, 12));
            if( bIncludeProtSeqs ) expected_gaps.push_back(makeGapInfo(4, 17));
            if( bIncludeNucSeqs ) expected_gaps.push_back(makeGapInfo(9, 26));
            if( bIncludeNucSeqs ) expected_gaps.push_back(makeGapInfo(24, 33));
            if( bIncludeProtSeqs ) expected_gaps.push_back(makeGapInfo(89, 22));
            if( bIncludeProtSeqs ) expected_gaps.push_back(makeGapInfo(30, 11));
            if( bIncludeNucSeqs ) expected_gaps.push_back(makeGapInfo(10, 51));

            // run the test
            CBioseqGaps_CI::Params params;
            if( bIncludeNucSeqs && ! bIncludeProtSeqs ) {
                params.mol_filter = CSeq_inst::eMol_na;
            } else if( ! bIncludeNucSeqs && bIncludeProtSeqs ) {
                params.mol_filter = CSeq_inst::eMol_aa;
            }
            s_TestBioseqs( vecSeqs, expected_gaps, params );
        }
        }
    }
}
