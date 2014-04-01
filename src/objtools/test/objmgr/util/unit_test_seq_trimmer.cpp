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
*   Test CSequenceAmbigTrimmer.
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
#include <objects/seq/Seq_annot.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbi_autoinit.hpp>
#include <objects/seq/Seq_ext.hpp>

#include <objtools/readers/fasta.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);

namespace 
{

    void s_ReverseString( string & in_out_str ) 
    {
        reverse( in_out_str.begin(), in_out_str.end() );
    }

    void s_NucSeqToAminoAcids( string & in_out_str )
    {
        NON_CONST_ITERATE(string, base_it, in_out_str) {
            char & ch = *base_it;
            switch(ch) {
            case 'N':
                // convert nuc completely unknown to amino acid completely unknown
                ch = 'X';
                break;
            case 'A':
            case 'C':
            case 'G':
            case 'T':
                // valid nucleotides happen to be valid amino
                // acids, so leave them be
                break;
            default:
                if( isalpha(ch) ) {
                    // other alphabetic characters are assumed to be
                    // ambiguous but not completely ambiguous, so we
                    // deterministically map them to the ambiguous amino
                    // acids.
                    static const char pchAmbigAminoAcids[] = "BJZ";
                    ch = pchAmbigAminoAcids[ ch % ArraySize(pchAmbigAminoAcids) ];
                } else {
                    // leave non-alphabetic characters alone
                }
                break;
            }
        }
    }

    CRef<CSeq_id> s_GetNextSeqId(void)
    {
        // makes sure the local id is different each time
        static CAtomicCounter nextIdNum;

        CNcbiOstrstream acc_strm;
        acc_strm << 'U' << setfill('0') << setw(5) << nextIdNum.Add(1);

        
        CRef<CSeq_id::TGenbank> p_genbank_id( new CSeq_id::TGenbank );
        p_genbank_id->Set( (string)CNcbiOstrstreamToString(acc_strm), "bogus_name", 1);

        CRef<CSeq_id> pNextId( new CSeq_id );
        pNextId->SetGenbank( *p_genbank_id );

        return pNextId;
    }


    CBioseq_Handle s_FastaToBioseqHandle(
        const string & sFasta,
        CSeq_inst::TMol eInputMol )
    {
        // wrap sFasta as an ILineReader
        CMemoryLineReader line_reader(sFasta.data(), sFasta.length());

        CRef<CMessageListenerLevel> pMsgListener(
            new CMessageListenerLevel(eDiag_Warning) );

        CFastaReader::TFlags fFlags = CFastaReader::fParseGaps |
            CFastaReader::fDLOptional |
            CFastaReader::fNoSplit |
            CFastaReader::fLeaveAsText |
            CFastaReader::fForceType;
        if( eInputMol == CSeq_inst::eMol_na ) {
            fFlags |= CFastaReader::fAssumeNuc;
        } else if( eInputMol == CSeq_inst::eMol_aa ) {
            fFlags |= CFastaReader::fAssumeProt;
        } else {
            BOOST_FAIL("Invalid input-mol");
        }
        CFastaReader fasta_reader(line_reader, fFlags);

        CRef<CSeq_entry> pSeqEntry =
            fasta_reader.ReadOneSeq(pMsgListener.GetPointer());
        CRef<CBioseq> pBioseq( & pSeqEntry->SetSeq() );

        pBioseq->SetId().clear();
        pBioseq->SetId().push_back(s_GetNextSeqId());

        CRef<CScope> pScope( new CScope(*CObjectManager::GetInstance()));
        pScope->AddDefaults();

        return pScope->AddBioseq(*pBioseq);
    }


    void WriteBioseqAsFasta(
        ostream & out_strm,
        const CBioseq_Handle & bioseq_h )
    {
        CFastaOstream fasta_ostream( out_strm );
        fasta_ostream.SetWidth(1000000);
        fasta_ostream.SetFlag(CFastaOstream::fShowModifiers);
        fasta_ostream.SetFlag(CFastaOstream::fShowGapModifiers);
        fasta_ostream.SetGapMode(CFastaOstream::eGM_count);
        fasta_ostream.WriteSequence(bioseq_h);
    }


    void s_FastasWorkCorrectly(
        const string & sFasta_input,
        const string & sFasta_expected_output,
        CSeq_inst::TMol eInputMol = CSeq_inst::eMol_na)
    {
        CSequenceAmbigTrimmer trimmer(
            CSequenceAmbigTrimmer::eMeaningOfAmbig_OnlyCompletelyUnknown,
            CSequenceAmbigTrimmer::fFlags_DoNotTrimSeqGap );

        CBioseq_Handle bioseq_handle1 = s_FastaToBioseqHandle(
            sFasta_input, eInputMol);
        trimmer.DoTrim(bioseq_handle1);
        CNcbiOstrstream fasta1_strm;
        WriteBioseqAsFasta(
            fasta1_strm, bioseq_handle1 );

        CBioseq_Handle bioseq_handle2 = s_FastaToBioseqHandle(
            sFasta_expected_output, eInputMol);
        CNcbiOstrstream fasta2_strm;
        WriteBioseqAsFasta(
            fasta2_strm, bioseq_handle2 );

        BOOST_CHECK_EQUAL(
            (string)CNcbiOstrstreamToString(fasta1_strm),
            (string)CNcbiOstrstreamToString(fasta2_strm));
    }
    

    CBioseq_Handle s_SequenceToBioseqHandle(
        const string & sInputSeq,
        CSeq_inst::TMol eInputMol
        )
    {
        CRef<CBioseq> pBioseq( new CBioseq );

        pBioseq->SetId().push_back( s_GetNextSeqId() );

        CSeq_inst & seq_inst = pBioseq->SetInst();
        seq_inst.SetRepr( CSeq_inst::eRepr_raw );
        seq_inst.SetMol( eInputMol );
        seq_inst.SetLength( sInputSeq.length() );

        CSeq_data & seq_data = seq_inst.SetSeq_data();
        if( eInputMol == CSeq_inst::eMol_na ) {
            seq_data.SetIupacna().Set() = sInputSeq;
        } else if( eInputMol == CSeq_inst::eMol_aa ) {
            seq_data.SetIupacaa().Set() = sInputSeq;
        } else {
            BOOST_FAIL("Unknown mol: " << static_cast<int>(eInputMol) );
        }

        CRef<CScope> pScope( new CScope(*CObjectManager::GetInstance()));
        pScope->AddDefaults();

        return pScope->AddBioseq(*pBioseq);
    }

    string s_ExtractSeqFromBioseq(
        CBioseq_Handle bioseq_h )
    {
        string sAnswer;
        CSeqVector seqvec( bioseq_h, CBioseq_Handle::eCoding_Iupac );
        seqvec.GetSeqData( 0, seqvec.size(), sAnswer );
        return sAnswer;
    }

    CBioseq_Handle s_CopyButDeltaSeqify(
        CBioseq_Handle bioseq_h )
    {
        CRef<CBioseq> pNewBioseq( SerialClone(*bioseq_h.GetCompleteBioseq()) );
        // uniquify the id, though
        pNewBioseq->SetId().clear();
        pNewBioseq->SetId().push_back( s_GetNextSeqId() );
        pNewBioseq->PackAsDeltaSeq(true);

        // BOOST_ERROR( cerr << "DELTA-SEQIFIED: " << MSerial_AsnText << *pNewBioseq );

        CRef<CScope> pScope( new CScope(*CObjectManager::GetInstance()));
        return pScope->AddBioseq(*pNewBioseq);
    }

    void s_TestTrimming(
        const string & sInputSeq_arg,
        const string & sExpectedIfStartTrimmed_arg,
        const string & sExpectedIfEndTrimmed_arg,
        CSequenceAmbigTrimmer::EMeaningOfAmbig eMeaningOfAmbig,
        const CSequenceAmbigTrimmer::TTrimRuleVec & vecTrimRules
            = CSequenceAmbigTrimmer::GetDefaultTrimRules(),
        TSignedSeqPos uMinSeqLen = 50 )
    {
        // sanity check the input
        BOOST_CHECK( NStr::EndsWith(
            sInputSeq_arg, sExpectedIfStartTrimmed_arg) );
        BOOST_CHECK( NStr::StartsWith(
            sInputSeq_arg, sExpectedIfEndTrimmed_arg) );

        ITERATE_BOTH_BOOL_VALUES(bUseAminoAcids) {
        ITERATE_BOTH_BOOL_VALUES(bReverse) {
        ITERATE_BOTH_BOOL_VALUES(bDeltaSeqify) {
        ITERATE_BOTH_BOOL_VALUES(bTrimBeginning) {
        ITERATE_BOTH_BOOL_VALUES(bTrimEnd) {

            string sInputSeq = sInputSeq_arg;
            string sExpectedIfStartTrimmed = sExpectedIfStartTrimmed_arg;
            string sExpectedIfEndTrimmed = sExpectedIfEndTrimmed_arg;

            // sInputSeq_arg always represents nucs, but we then
            // turn that into amino acids and should get the same
            // sort of results
            const CSeq_inst::TMol eInputMol = ( 
                bUseAminoAcids 
                ? CSeq_inst::eMol_aa 
                : CSeq_inst::eMol_na );
            if(bUseAminoAcids) {
                // convert the nucs to amino acid equivalents
                s_NucSeqToAminoAcids(sInputSeq);
                s_NucSeqToAminoAcids(sExpectedIfStartTrimmed);
                s_NucSeqToAminoAcids(sExpectedIfEndTrimmed);
            }

            // the trimming rules should be symmetrical

            if( bReverse ) {
                s_ReverseString( sInputSeq );
                sExpectedIfStartTrimmed.swap( sExpectedIfEndTrimmed );
                s_ReverseString( sExpectedIfStartTrimmed );
                s_ReverseString( sExpectedIfEndTrimmed );
            }

            CBioseq_Handle bioseq_h = s_SequenceToBioseqHandle(
                sInputSeq, eInputMol);

            // should get same result regardless of whether
            // we use a raw sequence or a delta-seq
            if( bDeltaSeqify ) {
                bioseq_h = s_CopyButDeltaSeqify(bioseq_h);
            } else {
                NCBITEST_CHECK( 
                    bioseq_h.GetCompleteBioseq()->GetInst().IsSetSeq_data() );
            }

            CSequenceAmbigTrimmer::TFlags fTrimmerFlags = 0;
            if( ! bTrimBeginning ) {
                fTrimmerFlags |= 
                    CSequenceAmbigTrimmer::fFlags_DoNotTrimBeginning;
            }
            if( ! bTrimEnd ) {
                fTrimmerFlags |= 
                    CSequenceAmbigTrimmer::fFlags_DoNotTrimEnd;
            }

            CSequenceAmbigTrimmer trimmer( 
                eMeaningOfAmbig,
                fTrimmerFlags,
                vecTrimRules,
                uMinSeqLen );

            // figure out what part of sInputSeq to copy
            string sExpected;
            // calculate value of sExpected in here:
            {{
                string::const_iterator input_seq_begin = sInputSeq.begin();
                if( bTrimBeginning ) {
                    input_seq_begin += (
                        sInputSeq.length() - sExpectedIfStartTrimmed.length());
                }
                string::const_iterator input_seq_end   = sInputSeq.end();
                if( bTrimEnd ) {
                    input_seq_end -= 
                        ( sInputSeq.length() - sExpectedIfEndTrimmed.length());
                }
                // note that sExpected isn't quite correct for
                // the case of trimming *both* ends.
                if( input_seq_begin < input_seq_end ) {
                    copy( input_seq_begin, input_seq_end,
                        back_inserter(sExpected) );
                }
            }}

            // BOOST_ERROR( "BEFORE DOTRIM: " << MSerial_AsnText << *bioseq_h.GetCompleteBioseq() );

            CSequenceAmbigTrimmer::EResult eTrimResult =
                trimmer.DoTrim( bioseq_h );
            const string sResult = s_ExtractSeqFromBioseq(bioseq_h);
            switch( eTrimResult ) {
            case CSequenceAmbigTrimmer::eResult_SuccessfullyTrimmed:
                BOOST_CHECK_NE( sResult, sInputSeq );
                break;
            case CSequenceAmbigTrimmer::eResult_NoTrimNeeded:
                BOOST_CHECK_EQUAL( sResult, sInputSeq );
                break;
            default:
                BOOST_ERROR("unknown CSequenceAmbigTrimmer::EResult: "
                    << static_cast<int>(eTrimResult) );
                break;
            }

            // BOOST_ERROR( "AFTER DOTRIM: " << MSerial_AsnText << *bioseq_h.GetCompleteBioseq() );

            if( bTrimBeginning && bTrimEnd ) {
                // special logic if both ends are being trimmed

                // since left is trimmed and then right, the leftmost
                // part of the result should be a prefix of what would
                // be left over if just left-trimming occurred.
                BOOST_CHECK( NStr::StartsWith(
                    sExpectedIfStartTrimmed, sResult) );
                // It should be at least as long as the bogus
                // "sExpected" which is not necessarily right in the case
                // of both ends being trimmed
                BOOST_CHECK( sResult.length() >= sExpected.length() );
            } else if( sExpected != sResult ) {
                BOOST_CHECK_EQUAL( 
                    sExpected, 
                    sResult );
            }
        } // end of testing with and without delta-seqification
        } // end of testing with and without trimming at end
        } // end of testing with and without trimming at beginning
        } // end of testing with and without reversed sequence
        } // end of testing for nucleotides vs. amino acids
    } // end of function s_TestTrimming

    // creates a new CSeq_feat and attaches it to the given
    // annot.
    void s_AttachSeqFeat( 
        CSeq_annot_EditHandle & annot_eh, 
        TSeqPos start_pos,
        TSeqPos length )
    {
        CAutoInitRef<CSeq_feat> pFeat;
        pFeat->SetData().SetComment();

        // set feat location
        CSeq_id_Handle bioseq_seq_id_h =
            annot_eh.GetParentEntry().GetSeq().GetAccessSeq_id_Handle();
        CRef<CSeq_id> pSeqId( SerialClone( *bioseq_seq_id_h.GetSeqId() ) );
        CRef<CSeq_loc> pLoc( 
            new CSeq_loc( *pSeqId, start_pos, start_pos + length) );
        pFeat->SetLocation().Assign( *pLoc );

        // add feat to annot
        annot_eh.AddFeat( *pFeat );
    }
}


// Test CSequenceAmbigTrimmer::fFlags_DoNotTrimSeqGap
BOOST_AUTO_TEST_CASE(SequenceAmbigTrimmer_TestDoNotTrimSeqGap)
{
    const string kSeqGap = ">?42 [gap-type=between-scaffolds] [linkage-evidence=align_genus]\n";

    // test just seq-gap
    s_FastasWorkCorrectly(kSeqGap, kSeqGap);

    // test when seq-gap touches the left side
    s_FastasWorkCorrectly(kSeqGap + string(20, 'A'),
        kSeqGap + string(20, 'A'));

    // test when N's, then seq-gap
    s_FastasWorkCorrectly(string(5, 'N') + "\n" + kSeqGap + string(20, 'A'),
        kSeqGap + string(20, 'A'));
    s_FastasWorkCorrectly(string(52, 'N') + "\n" + kSeqGap + string(20, 'A'),
        kSeqGap + string(20, 'A'));

    // test when bases, then N's, then seq-gap
    s_FastasWorkCorrectly(
        string(4, 'C') + string(52, 'N') + "\n" + kSeqGap + string(20, 'A'),
        kSeqGap + string(20, 'A'));
    s_FastasWorkCorrectly(
        string(4, 'C') + string(6, 'N') + "\n" + kSeqGap + string(20, 'A'),
        kSeqGap + string(20, 'A'));
    s_FastasWorkCorrectly(
        string(35, 'C') + string(52, 'N') + "\n" + kSeqGap + string(20, 'A'),
        string(35, 'C') + string(52, 'N') + "\n" + kSeqGap + string(20, 'A'));

    // test when N's then bases then seq-gap
    s_FastasWorkCorrectly(
        string(48, 'N') + string(35, 'C') + "\n" + kSeqGap + string(20, 'A'),
        string(35, 'C') + "\n" + kSeqGap + string(20, 'A'));
    s_FastasWorkCorrectly(
        string(47, 'N') + string(1, 'C') + "\n" + kSeqGap + string(20, 'A'),
        string(1, 'C') + "\n" + kSeqGap + string(20, 'A'));

    // test prot
    s_FastasWorkCorrectly(
        kSeqGap + string(20, 'W'),
        kSeqGap + string(20, 'W'),
        CSeq_inst::eMol_aa);
    s_FastasWorkCorrectly(
        string(12, 'X') + "\n" + kSeqGap + string(20, 'W'),
        kSeqGap + string(20, 'W'),
        CSeq_inst::eMol_aa);

    // test when trimming would occur from right
    s_FastasWorkCorrectly(
        kSeqGap + string(8, 'X'),
        kSeqGap);
    s_FastasWorkCorrectly(
        kSeqGap + string(4, 'T') + string(8, 'X'),
        kSeqGap + string(4, 'T'));
}


// example:
// const SFoo abc[] = { ..., ..., ...};
// ITERATE_ARRAY(SFoo, foo_iter, abc) 
#define ITERATE_ARRAY(_elem_type, _iter, _array)        \
    for( _elem_type * _iter = &_array[0];               \
         iter != & _array[ArraySize(_array)] ;          \
         ++iter )

// example:
// ITERATE_ARRAY(int, int_iter, {1, 2, 4, 5}) 
#define ITERATE_IN_PLACE_ARRAY_IMPL(_elem_type, _iter, _array_literal, _line) \
    /* bogus for-loop declares variable without opening a scope */      \
    /* (The number in there is completely random and has no meaning) */ \
    for( const _elem_type _array_266033595_##_line[] = _array_literal ; false; ) \
        ITERATE_ARRAY(_elem_type, _iter, _array_266033595_##_line[])

#define ITERATE_IN_PLACE_ARRAY(_elem_type, _iter, _array_literal) \
    ITERATE_IN_PLACE_ARRAY_IMPL(_elem_type, _iter, _array_literal, __LINE__)

// BOOST_AUTO_TEST_CASE(SequenceAmbigTrimmer_TestTrimAnnot)
//{
//    const string kTheGoodBases = string(100, 'A');
//    const string kTestSeq = 
//        string(100, 'N') + kTheGoodBases + string(100, 'N');
//
//    ITERATE_BOTH_BOOL_VALUES(bTrimAnnot) {
//        // create the bioseq, and add its annots and descs
//        CBioseq_Handle bioseq_h =
//            s_SequenceToBioseqHandle(kTestSeq, CSeq_inst::eMol_na);
//        CBioseq_EditHandle bioseq_eh = bioseq_h.GetEditHandle();
//
//        CRef<CSeq_annot> pAnnot( new CSeq_annot );
//        CSeq_annot_EditHandle annot_eh = bioseq_eh.AttachAnnot(*pAnnot);
//        s_AttachSeqFeat( annot_eh, 25,  50 );
//        s_AttachSeqFeat( annot_eh, 127, 155 );
//        s_AttachSeqFeat( annot_eh, 229, 60 );
//
//        CRef<CSeqdesc> pSeqdesc( new CSeqdesc );
//        pSeqdesc->SetComment( "some comment " );
//        bioseq_eh.AddSeqdesc( *pSeqdesc );
//
//        // set up trimming flags
//        CSequenceAmbigTrimmer::TFlags fTrimmerFlags = 0;
//        if( bTrimAnnot ) {
//            fTrimmerFlags |= 
//                CSequenceAmbigTrimmer::fFlags_TrimAnnot;
//        }
//
//        // do the trimming
//        CSequenceAmbigTrimmer trimmer( 
//            CSequenceAmbigTrimmer::eMeaningOfAmbig_OnlyCompletelyUnknown,
//            fTrimmerFlags );
//        BOOST_CHECK_EQUAL(
//            trimmer.DoTrim( bioseq_h ), 
//            CSequenceAmbigTrimmer::eResult_SuccessfullyTrimmed );
//        
//        // make sure trimmed on both sides
//        const string sResult = s_ExtractSeqFromBioseq(bioseq_h);
//        BOOST_CHECK_EQUAL( kTheGoodBases,
//            sResult );
//
//        // check if it trimmed the annots correctly
//
//        typedef CSeq_feat_Handle::TRange TRange;
//        typedef vector<TRange> TRangeVec;
//        TRangeVec resultingRangeVec;
//
//        // get the resulting range of each feature
//        CFeat_CI feat_ci( bioseq_eh );
//        for( ; feat_ci; ++feat_ci ) {
//            CSeq_feat_Handle feat_h = *feat_ci;
//            resultingRangeVec.push_back( feat_h.GetLocationTotalRange() );
//        }
//
//        // calculate the expected range for each feature
//        TRangeVec expectedRangeVec;
//        if( bTrimAnnot ) {
//            expectedRangeVec.push_back( TRange(27, 100) );
//        } else {
//            expectedRangeVec.push_back( TRange(25, 75) );
//            expectedRangeVec.push_back( TRange(127, 282) );
//            expectedRangeVec.push_back( TRange(229, 289) );
//        }
//
//        BOOST_CHECK_EQUAL_COLLECTIONS(
//            resultingRangeVec.begin(), resultingRangeVec.end(),
//            expectedRangeVec.begin(), expectedRangeVec.end() );
//    }
//}

BOOST_AUTO_TEST_CASE(SequenceAmbigTrimmer_GeneralTests)
{
    // build string of k100GoodBases
    string k100GoodBases;
    ITERATE_0_IDX(idx, 10) {
        k100GoodBases += "GATTACATGC";
    }

    // test empty input with all sorts of flags
    ITERATE_BOTH_BOOL_VALUES(bTrimAnyAmbig) {
        const CSequenceAmbigTrimmer::EMeaningOfAmbig eCurrMeaning = 
            ( bTrimAnyAmbig 
              ? CSequenceAmbigTrimmer::eMeaningOfAmbig_AnyAmbig 
              : CSequenceAmbigTrimmer::eMeaningOfAmbig_OnlyCompletelyUnknown );

        // empty sequence
        s_TestTrimming(
            "",
            "",
            "",
            eCurrMeaning );

        // test all-gap sequence
        s_TestTrimming(
            "A" + string(1000, 'N') + "C",
            "C",
            "A",
            eCurrMeaning );

        // test sequences with one base
        s_TestTrimming(
            "A", "A", "A", // unambiguous base
            eCurrMeaning ); 
        s_TestTrimming(
            "N", "", "", // completely unknown base
            eCurrMeaning );
        s_TestTrimming(
            "Y", 
            ( bTrimAnyAmbig ? "" : "Y"), 
            ( bTrimAnyAmbig ? "" : "Y"), 
            eCurrMeaning );

        // test sequences with two bases
        // (note that s_TestTrimming tests the mirror cases)
        s_TestTrimming(
            "AC", "AC", "AC", 
            eCurrMeaning );
        s_TestTrimming(
            "AN", "AN", "A", 
            eCurrMeaning );
        s_TestTrimming(
            "AY", 
            "AY",
            ( bTrimAnyAmbig ? "A" : "AY" ),
            eCurrMeaning );
        s_TestTrimming(
            "NN", 
            "",
            "",
            eCurrMeaning );
        s_TestTrimming(
            "YY", 
            ( bTrimAnyAmbig ? "" : "YY" ),
            ( bTrimAnyAmbig ? "" : "YY" ),
            eCurrMeaning );
        s_TestTrimming(
            "YN",
            ( bTrimAnyAmbig ? "" : "YN" ),
            ( bTrimAnyAmbig ? "" : "Y" ),
            eCurrMeaning );

        // test basic short rule-triggering
        s_TestTrimming(
            "AGNNNANNNTCACGT",
            "AGNNNANNNTCACGT",
            "AGNNNANNNTCACGT",
            eCurrMeaning ); // no trigger because seq is too small

        // test trimming on both ends
        ITERATE_BOTH_BOOL_VALUES(b)
        s_TestTrimming(
            "AGNNNANNNTG" + k100GoodBases + "GACAYYNNNNA",
            "TG" + k100GoodBases + "GACAYYNNNNA",
            "AGNNNANNNTG" + k100GoodBases + 
            ( bTrimAnyAmbig 
            ? "GACA"
            : "GACAYYNNNNA" ),
            eCurrMeaning ); 

        // test if rule triggered, and then there's a little trimming
        s_TestTrimming(
            "AGNNNANNNTNN" + k100GoodBases + "NYACAYYNNNNA",
            k100GoodBases + "NYACAYYNNNNA",
            "AGNNNANNNTNN" + k100GoodBases + ( bTrimAnyAmbig 
            ? ""
            : "NYACAYYNNNNA" ),
            eCurrMeaning );

        // test if rule triggered multiple times
        s_TestTrimming(
            "AGNNNANNNTNNAGNNNANNNTNN" + k100GoodBases + "NYACAYYNNNNANYACAYYNNNNA",
            k100GoodBases + "NYACAYYNNNNANYACAYYNNNNA",
            "AGNNNANNNTNNAGNNNANNNTNN" + k100GoodBases + 
            ( bTrimAnyAmbig 
            ? ""
            : "NYACAYYNNNNANYACAYYNNNNA" ),
            eCurrMeaning );

        // test if just long rule triggered
        s_TestTrimming(
            "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC" + k100GoodBases + "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC",
            "AC" + k100GoodBases + "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC",
            "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC" + k100GoodBases + "ACGT",
            eCurrMeaning );

        // test if short then long rule triggered
        s_TestTrimming(
            "ANNNNCNNNNACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC" + k100GoodBases + "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACANNNNCNNNN",
            "AC" + k100GoodBases + "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACANNNNCNNNN",
            "ANNNNCNNNNACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC" + k100GoodBases + "ACGT",
            eCurrMeaning );

        // test if long then short rule triggered
        s_TestTrimming(
            "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACANNNNCNNNN" + k100GoodBases + "ANNNNCNNNNACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC",
            k100GoodBases + "ANNNNCNNNNACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNAC",
            "ACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACACGTNNNNACANNNNCNNNN" + k100GoodBases + "A",
            eCurrMeaning );

        // test with huge gap
        s_TestTrimming(
            string(10000, 'N') + k100GoodBases + string(10000, 'N'),
            k100GoodBases + string(10000, 'N'),
            string(10000, 'N') + k100GoodBases,
            eCurrMeaning );

        // test with alternative rules
        CSequenceAmbigTrimmer::STrimRule rule1 = {5, 2};
        CSequenceAmbigTrimmer::TTrimRuleVec alt_rules;
        alt_rules.push_back( rule1 );
        s_TestTrimming(
            "NNACGACGTACGTNNNNNNNNNNNNNNN" + k100GoodBases + "ANCGNN",
            "ACGACGTACGTNNNNNNNNNNNNNNN" + k100GoodBases + "ANCGNN",
            "NNACGACGTACGTNNNNNNNNNNNNNNN" + k100GoodBases + "A",
            eCurrMeaning,
            alt_rules);

        // test with no rules
        s_TestTrimming(
            "NN" + k100GoodBases + "NNN",
            k100GoodBases + "NNN",
            "NN" + k100GoodBases,
            eCurrMeaning,
            CSequenceAmbigTrimmer::TTrimRuleVec() );

        // test with alternative min-seq-len
        {
            const string kSeqThatWontBeTrimmed = 
                "CNNACGACGTACGTNNNNNNNNNNNNNNN" + k100GoodBases + "ANNCGNNC";
            s_TestTrimming(
                kSeqThatWontBeTrimmed,
                kSeqThatWontBeTrimmed,
                kSeqThatWontBeTrimmed,
                eCurrMeaning,
                alt_rules,
                200);

            // only some bases will be trimmed, due to
            // the minimum seqlen
            s_TestTrimming(
                "ANNNA" "ANNNAANNNA" + k100GoodBases,
                "AANNNA" + k100GoodBases,
                "ANNNA" "ANNNAANNNA" + k100GoodBases,
                eCurrMeaning,
                alt_rules,
                110 );
        }

        // test sequences that are all gaps
        const static size_t arrGapSizesToCheck[] = {
            10, 50, 43, 110, 200, 139
        };
        ITERATE_0_IDX(idx, ArraySize(arrGapSizesToCheck)) {
            s_TestTrimming(
                string(arrGapSizesToCheck[idx], 'N'),
                "",
                "",
                eCurrMeaning);
        }

        // test sequences that are almost all gaps
        s_TestTrimming(
            string(1000, 'N') + "A" + string(1000, 'N'),
            "",
            "",
            eCurrMeaning );
        s_TestTrimming(
            string(1000, 'N') + k100GoodBases + string(1000, 'N'),
            k100GoodBases + string(1000, 'N'),
            string(1000, 'N') + k100GoodBases,
            eCurrMeaning );
    }

   

}

BOOST_AUTO_TEST_CASE(SequenceAmbigTrimmer_TestSeqInstRepr)
{
    string file_name = "test_data/KF927036-na.asn";
    ifstream file_in(file_name.c_str());
    BOOST_REQUIRE(file_in);
    AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, file_in));
    BOOST_REQUIRE(!in->EndOfData());
    BOOST_REQUIRE_EQUAL(in->GetStreamPos(), NcbiInt8ToStreampos(0));
    CBioseq bioseq;
    *in >> bioseq;
    CRef<CScope> pScope( new CScope(*CObjectManager::GetInstance()));
    pScope->AddDefaults();
    CBioseq_Handle bsh = pScope->AddBioseq(bioseq);
    
    CSequenceAmbigTrimmer trimmer( CSequenceAmbigTrimmer::eMeaningOfAmbig_OnlyCompletelyUnknown, CSequenceAmbigTrimmer::fFlags_DoNotTrimSeqGap );
    trimmer.DoTrim(bsh);
    BOOST_CHECK_EQUAL(bioseq.GetInst().GetRepr(), CSeq_inst::eRepr_raw);
}
