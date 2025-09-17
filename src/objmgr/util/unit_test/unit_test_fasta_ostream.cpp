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
* Author:  Pavel Ivanov, NCBI
*
* File Description:
*   Sample unit tests file for main stream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework based
* on Boost.Test framework. For more advanced techniques look into another
* sample - unit_test_alt_sample.cpp.
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

#include <corelib/ncbi_autoinit.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Linkage_evidence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const string sc_TestEntry;

CRef<CSeq_entry> s_ReadData()
{
    static CRef<CSeq_entry> s_entry;
    if ( ! s_entry ) {
        s_entry.Reset(new CSeq_entry);

        CNcbiIstrstream istr(sc_TestEntry);
        istr >> MSerial_AsnText >> *s_entry;
    }

    return s_entry;
}

BOOST_AUTO_TEST_CASE(Test_FastaRaw)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// first: raw formatting
    ///
    {{
         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              fasta_os.SetGapMode(CFastaOstream::eGM_dashes);
              /// FIXME: this should be the default!!
              //fasta_os.SetFlag(CFastaOstream::fInstantiateGaps);
              //fasta_os.SetFlag(CFastaOstream::fAssembleParts);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
"GCTGCCTTTGGCCACCAAAATCCCAAACT--------------------AATTAAAGAATTAAATAATTC\n"
"GAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}

}

BOOST_AUTO_TEST_CASE(Test_FastaGap)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);


    ///
    /// next: alternate gap processing
    ///
    {{
         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              /// FIXME: this should be the default!!
              fasta_os.ResetFlag(CFastaOstream::fInstantiateGaps);
              fasta_os.ResetFlag(CFastaOstream::fAssembleParts);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
"GCTGCCTTTGGCCACCAAAATCCCAAACT-\n"
"AATTAAAGAATTAAATAATTCGAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGAT\n"
"ATCTAGTATACATTTCGATACATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}

BOOST_AUTO_TEST_CASE(Test_FastaMask_SimpleSoft)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);


    ///
    /// next: simple lower-case masking
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> loc(new CSeq_loc);
         for (TSeqPos pos = 10;  pos < 200;  pos += 27) {
             CRef<CSeq_interval> ival(new CSeq_interval);
             ival->SetFrom(pos);
             ival->SetTo(pos + 9);
             loc->SetPacked_int().Set().push_back(ival);
         }
         loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              // fasta_os.SetGapMode(CFastaOstream::eGM_letters);
              fasta_os.SetMask(CFastaOstream::eSoftMask, loc);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGggttttataaCATCAGTCAGTGACAGGcatttccagaGTTGCCCTGTTCAACAAtcgata\n"
"gctgCCTTTGGCCACCAAAATcccaaactnnNNNNNNNNNNNNNNNNNnaattaaagaATTAAATAATTC\n"
"GAATAataattaagcCCAGTAACCTACGCAGCttgagtgcgtAACCGATATCTAGTATAcatttcgataC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}


BOOST_AUTO_TEST_CASE(Test_FastaMask_SimpleHard)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);


    ///
    /// next: simple hard ('N') masking
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> loc(new CSeq_loc);
         for (TSeqPos pos = 10;  pos < 200;  pos += 27) {
             CRef<CSeq_interval> ival(new CSeq_interval);
             ival->SetFrom(pos);
             ival->SetTo(pos + 9);
             loc->SetPacked_int().Set().push_back(ival);
         }
         loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              fasta_os.SetGapMode(CFastaOstream::eGM_dashes);
              fasta_os.SetMask(CFastaOstream::eHardMask, loc);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGNNNNNNNNNNCATCAGTCAGTGACAGGNNNNNNNNNNGTTGCCCTGTTCAACAANNNNNN\n"
"NNNNCCTTTGGCCACCAAAATNNNNNNNN--------------------NNNNNNNNNATTAAATAATTC\n"
"GAATANNNNNNNNNNCCAGTAACCTACGCAGCNNNNNNNNNNAACCGATATCTAGTATANNNNNNNNNNC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}


BOOST_AUTO_TEST_CASE(Test_FastaMask_ComplexSoft)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// next: complex lower-case masking
    /// we do two things - first, provide a set of overlapping locations and
    /// second, clear the masks
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> loc(new CSeq_loc);
         for (TSeqPos pos = 10;  pos < 200;  pos += 27) {
             CRef<CSeq_interval> ival(new CSeq_interval);
             ival->SetFrom(pos);
             ival->SetTo(pos + 9);
             loc->SetPacked_int().Set().push_back(ival);
         }

         CRef<CSeq_interval> ival(new CSeq_interval);
         ival->SetFrom(100);
         ival->SetTo(150);
         loc->SetPacked_int().Set().push_back(ival);
         loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              // fasta_os.SetGapMode(CFastaOstream::eGM_letters);
              fasta_os.SetMask(CFastaOstream::eSoftMask, loc);
              fasta_os.Write(seh);
              fasta_os.SetMask(CFastaOstream::eSoftMask, CConstRef<CSeq_loc>());
              CSeq_loc loc2(*id, (TSeqPos)0, 24);
              fasta_os.Write(seh, &loc2);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGggttttataaCATCAGTCAGTGACAGGcatttccagaGTTGCCCTGTTCAACAAtcgata\n"
"gctgCCTTTGGCCACCAAAATcccaaactnnnnnnnnnnnnnnnnnnnnaattaaagaattaaataattc\n"
"gaataataattaagcCCAGTAACCTACGCAGCttgagtgcgtAACCGATATCTAGTATAcatttcgataC\n"
"ATCGAAAT\n"
">lcl|test-seq:1-25 test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCA\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}



BOOST_AUTO_TEST_CASE(Test_FastaMask_ComplexSoftHard)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// next: complex lower-case masking
    /// first, test just supplying hard and soft masks
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> soft_loc(new CSeq_loc);
         for (TSeqPos pos = 10;  pos < 200;  pos += 27) {
             CRef<CSeq_interval> ival(new CSeq_interval);
             ival->SetFrom(pos);
             ival->SetTo(pos + 9);
             soft_loc->SetPacked_int().Set().push_back(ival);
         }

         CRef<CSeq_interval> ival(new CSeq_interval);
         ival->SetFrom(100);
         ival->SetTo(150);
         soft_loc->SetPacked_int().Set().push_back(ival);
         soft_loc->SetId(*id);

         CRef<CSeq_loc> hard_loc(new CSeq_loc);
         hard_loc->SetInt().SetFrom(105);
         hard_loc->SetInt().SetTo(145);
         hard_loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              // fasta_os.SetGapMode(CFastaOstream::eGM_letters);
              fasta_os.SetMask(CFastaOstream::eSoftMask, soft_loc);
              fasta_os.SetMask(CFastaOstream::eHardMask, hard_loc);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGggttttataaCATCAGTCAGTGACAGGcatttccagaGTTGCCCTGTTCAACAAtcgata\n"
"gctgCCTTTGGCCACCAAAATcccaaactnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn\n"
"nnnnnntaattaagcCCAGTAACCTACGCAGCttgagtgcgtAACCGATATCTAGTATAcatttcgataC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}

    ///
    /// complex, part deux:
    /// test setting and clearing masks
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> soft_loc(new CSeq_loc);
         for (TSeqPos pos = 10;  pos < 200;  pos += 27) {
             CRef<CSeq_interval> ival(new CSeq_interval);
             ival->SetFrom(pos);
             ival->SetTo(pos + 9);
             soft_loc->SetPacked_int().Set().push_back(ival);
         }

         CRef<CSeq_interval> ival(new CSeq_interval);
         ival->SetFrom(100);
         ival->SetTo(150);
         soft_loc->SetPacked_int().Set().push_back(ival);
         soft_loc->SetId(*id);

         CRef<CSeq_loc> hard_loc(new CSeq_loc);
         hard_loc->SetInt().SetFrom(105);
         hard_loc->SetInt().SetTo(145);
         hard_loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              // fasta_os.SetGapMode(CFastaOstream::eGM_letters);
              fasta_os.SetMask(CFastaOstream::eSoftMask, soft_loc);
              fasta_os.SetMask(CFastaOstream::eHardMask, hard_loc);
              fasta_os.Write(seh);
              fasta_os.SetMask(CFastaOstream::eSoftMask, CConstRef<CSeq_loc>());
              CSeq_loc loc2(*id, (TSeqPos)0, 217);
              fasta_os.Write(seh, &loc2);
              fasta_os.SetMask(CFastaOstream::eHardMask, CConstRef<CSeq_loc>());
              fasta_os.Write(seh, &loc2);
              fasta_os.SetMask(CFastaOstream::eSoftMask, soft_loc);
              fasta_os.Write(seh, &loc2);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGggttttataaCATCAGTCAGTGACAGGcatttccagaGTTGCCCTGTTCAACAAtcgata\n"
"gctgCCTTTGGCCACCAAAATcccaaactnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn\n"
"nnnnnntaattaagcCCAGTAACCTACGCAGCttgagtgcgtAACCGATATCTAGTATAcatttcgataC\n"
"ATCGAAAT\n"
">lcl|test-seq:1-218 test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
"GCTGCCTTTGGCCACCAAAATCCCAAACTNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN\n"
"NNNNNNTAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
"ATCGAAAT\n"
">lcl|test-seq:1-218 test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
"GCTGCCTTTGGCCACCAAAATCCCAAACTNNNNNNNNNNNNNNNNNNNNAATTAAAGAATTAAATAATTC\n"
"GAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
"ATCGAAAT\n"
">lcl|test-seq:1-218 test sequence\n"
"CGGTTGCTTGggttttataaCATCAGTCAGTGACAGGcatttccagaGTTGCCCTGTTCAACAAtcgata\n"
"gctgCCTTTGGCCACCAAAATcccaaactnnnnnnnnnnnnnnnnnnnnaattaaagaattaaataattc\n"
"gaataataattaagcCCAGTAACCTACGCAGCttgagtgcgtAACCGATATCTAGTATAcatttcgataC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}



BOOST_AUTO_TEST_CASE(Test_FastaMask_SoftHardSimpleOverlap)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// next: complex lower-case masking
    /// we do two things - first, provide a set of overlapping locations and
    /// second, clear the masks
    ///
    {{
         CRef<CSeq_id> id(new CSeq_id("lcl|test-seq"));
         CRef<CSeq_loc> soft_loc(new CSeq_loc);
         soft_loc->SetInt().SetFrom(50);
         soft_loc->SetInt().SetTo(75);
         soft_loc->SetId(*id);

         CRef<CSeq_loc> hard_loc(new CSeq_loc);
         hard_loc->SetInt().SetFrom(60);
         hard_loc->SetInt().SetTo(80);
         hard_loc->SetId(*id);

         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              // fasta_os.SetGapMode(CFastaOstream::eGM_letters);
              fasta_os.SetMask(CFastaOstream::eSoftMask, soft_loc);
              fasta_os.SetMask(CFastaOstream::eHardMask, hard_loc);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTgccctgttcannnnnnnnnn\n"
"nnnnnnNNNNNCCACCAAAATCCCAAACTNNNNNNNNNNNNNNNNNNNNAATTAAAGAATTAAATAATTC\n"
"GAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
     }}
}

BOOST_AUTO_TEST_CASE(Test_FastaMods)
{
    CRef<CSeq_entry> entry = s_ReadData();

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// formatting with modifiers
    ///
    {{
         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              fasta_os.SetGapMode(CFastaOstream::eGM_dashes);
              /// FIXME: this should be the default!!
              //fasta_os.SetFlag(CFastaOstream::fInstantiateGaps);
              fasta_os.SetFlag(CFastaOstream::fShowModifiers);
              fasta_os.Write(seh);
          }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));
         static const string sc_Expected = 
">lcl|test-seq [div=CON] [organism=\"Sarcophilus='harrisii\"] [pathovar=fake data] [strain=some strain] [gcode=1] [tech=physical map] test sequence\n"
"CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
"GCTGCCTTTGGCCACCAAAATCCCAAACT--------------------AATTAAAGAATTAAATAATTC\n"
"GAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
"ATCGAAAT\n";
         BOOST_CHECK_EQUAL(s, sc_Expected);
    }}

    // check with topology circular
    {{
        seh.GetEditHandle().SetSeq().SetInst_Topology( CSeq_inst::eTopology_circular );

        CNcbiOstrstream os;
        {{
            CFastaOstream fasta_os(os);
            fasta_os.SetGapMode(CFastaOstream::eGM_dashes);
            /// FIXME: this should be the default!!
            //fasta_os.SetFlag(CFastaOstream::fInstantiateGaps);
            fasta_os.SetFlag(CFastaOstream::fShowModifiers);
            fasta_os.Write(seh);
        }}
        os.flush();
        string s = string(CNcbiOstrstreamToString(os));
        static const string sc_Expected = 
            ">lcl|test-seq [div=CON] [topology=circular] [organism=\"Sarcophilus='harrisii\"] [pathovar=fake data] [strain=some strain] [gcode=1] [tech=physical map] test sequence\n"
            "CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
            "GCTGCCTTTGGCCACCAAAATCCCAAACT--------------------AATTAAAGAATTAAATAATTC\n"
            "GAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGATATCTAGTATACATTTCGATAC\n"
            "ATCGAAAT\n";
        BOOST_CHECK_EQUAL(s, sc_Expected);
    }}
}

BOOST_AUTO_TEST_CASE(Test_GapMods)
{
    CRef<CSeq_entry> entry = s_ReadData();

    /// Give the seq entry's gaps gap-type and linkage-evidence
    CTypeIterator<CSeq_literal> literal_iter(Begin(*entry));
    for( ; literal_iter; ++literal_iter) {
        if( ! literal_iter->IsSetSeq_data() ) {
            CSeq_gap & seq_gap = literal_iter->SetSeq_data().SetGap();

            seq_gap.SetType(CSeq_gap::eType_clone);

            seq_gap.SetLinkage(CSeq_gap::eLinkage_linked);

            CSeq_gap::TLinkage_evidence & linkage_evidence_vec =
                seq_gap.SetLinkage_evidence();
            CAutoInitRef<CLinkage_evidence> mapEvid;
            mapEvid->SetType( CLinkage_evidence::eType_map );
            linkage_evidence_vec.push_back( Ref(&*mapEvid) );
            CAutoInitRef<CLinkage_evidence> strobeEvid;
            strobeEvid->SetType( CLinkage_evidence::eType_strobe );
            linkage_evidence_vec.push_back( Ref(&*strobeEvid) );
        }
    }

    ///
    /// we have one bioseq
    /// add this to a scope and get it back so we can format
    ///
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CRef<CScope> scope(new CScope(*om));

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    ///
    /// formatting with and without gap modifiers
    ///
    ITERATE_BOTH_BOOL_VALUES(bShowGapModifiers)
    {
         CNcbiOstrstream os;
         {{
              CFastaOstream fasta_os(os);
              fasta_os.SetGapMode(CFastaOstream::eGM_count);
              /// FIXME: this should be the default!!
              //fasta_os.SetFlag(CFastaOstream::fInstantiateGaps);
              if( bShowGapModifiers ) {
                  fasta_os.SetFlag(CFastaOstream::fShowGapModifiers);
              }
              fasta_os.Write(seh);
         }}
         os.flush();
         string s = string(CNcbiOstrstreamToString(os));

         CNcbiOstrstream expected_os;
         expected_os << ">lcl|test-seq test sequence\n"
             "CGGTTGCTTGGGTTTTATAACATCAGTCAGTGACAGGCATTTCCAGAGTTGCCCTGTTCAACAATCGATA\n"
             "GCTGCCTTTGGCCACCAAAATCCCAAACT\n"
             ">?20";
         if( bShowGapModifiers ) {
             expected_os << " [gap-type=within scaffold] [linkage-evidence=map;strobe]";
         }
         expected_os << '\n';
         expected_os << "AATTAAAGAATTAAATAATTCGAATAATAATTAAGCCCAGTAACCTACGCAGCTTGAGTGCGTAACCGAT\n"
             "ATCTAGTATACATTTCGATACATCGAAAT\n";

         BOOST_CHECK_EQUAL(s, string(CNcbiOstrstreamToString(expected_os)));
    }
}

BOOST_AUTO_TEST_CASE(Test_OverlappingLocation) 
{
    auto pEntry = s_ReadData();
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));


    auto pSeqId = Ref(new CSeq_id("lcl|test-seq"));
    auto pLoc = Ref(new CSeq_loc());
    auto& packedInt = pLoc->SetPacked_int();
    packedInt.AddInterval(*pSeqId, 10, 20);
    packedInt.AddInterval(*pSeqId, 15, 25);

    string expected = ">lcl|test-seq:11-21,16-26 test sequence\n"
                      "GGTTTTATAACTATAACATCAG\n";

    { 
        CNcbiOstrstream os;
        CFastaOstream fasta_os(os);
        auto seh = pScope->AddTopLevelSeqEntry(*pEntry);
        fasta_os.Write(seh, pLoc.GetPointer());
        auto s = string(CNcbiOstrstreamToString(os));
        BOOST_CHECK_EQUAL(s, expected);
    }
    
    {
        CNcbiOstrstream os;
        CFastaOstream fasta_os(os);
        auto bsh = pScope->GetBioseqHandle(*pLoc); 
        fasta_os.Write(bsh, pLoc.GetPointer());
        auto s = string(CNcbiOstrstreamToString(os));
        BOOST_CHECK_EQUAL(s, expected);
    }
}

#if 0
BOOST_AUTO_TEST_CASE(Test_AutoGenerateData)
{
    CArgs args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIstream& istr = args["data-in"].AsInputFile();
    CSeq_entry entry;
    istr >> MSerial_AsnText >> entry;

    /// modify the seq-entry - truncate the sequence, add a delta
    string fasta1;
    string fasta2;

    {{
         CSeqVector vec(entry.GetSeq(), NULL, CBioseq_Handle::eCoding_Iupac);
         vec.GetSeqData(0, 99, fasta1);
         vec.GetSeqData(100, 199, fasta2);
     }}

    CSeq_inst& inst = entry.SetSeq().SetInst();
    inst.Reset();
    inst.SetRepr(CSeq_inst::eRepr_delta);
    inst.SetMol(CSeq_inst::eMol_dna);
    inst.SetLength(fasta1.size() + fasta2.size() + 20);

    CRef<CDelta_seq> del1(new CDelta_seq);
    del1->SetLiteral().SetLength(fasta1.size());
    del1->SetLiteral().SetSeq_data().SetIupacna(*new CIUPACna(fasta1));
    CSeqportUtil::Pack(&del1->SetLiteral().SetSeq_data());
    inst.SetExt().SetDelta().Set().push_back(del1);

    CRef<CDelta_seq> del_gap(new CDelta_seq);
    del_gap->SetLiteral().SetLength(20);
    inst.SetExt().SetDelta().Set().push_back(del_gap);

    CRef<CDelta_seq> del2(new CDelta_seq);
    del2->SetLiteral().SetLength(fasta2.size());
    del2->SetLiteral().SetSeq_data().SetIupacna(*new CIUPACna(fasta2));
    CSeqportUtil::Pack(&del2->SetLiteral().SetSeq_data());
    inst.SetExt().SetDelta().Set().push_back(del2);

    cerr << MSerial_AsnText << entry;
}
#endif


const string sc_TestEntry = "\
Seq-entry ::= seq {\
  id {\
    local str \"test-seq\"\
  },\
  descr {\
    title \"test sequence\" ,\
    source {\
      org {\
        taxname \"Sarcophilus=\"\"harrisii\" ,\
        orgname {\
          mod {\
            { subtype pathovar, subname \"fake data\" } ,\
            { subtype strain, subname \"some strain\" }\
          } ,\
          gcode 1\
        }\
      }\
    } ,\
    molinfo {\
      tech physmap\
    }\
  },\
  inst {\
    repr delta,\
    mol dna,\
    length 218,\
    ext delta {\
      literal {\
        length 99,\
        seq-data ncbi2na '6BE7EAFF304D2D2E1293F522F95EF410D8C9E5FE94500D501C'H\
      },\
      literal {\
        length 20\
      },\
      literal {\
        length 99,\
        seq-data ncbi2na '0F020F030F60C30F0952C171927E2E6C163372CC4FD8C4D80C'H\
      }\
    }\
  }\
}";


BOOST_AUTO_TEST_CASE(Test_LocationMitochondrion)
{
    // RW-1277
    string testEntry = 
    R"(Seq-entry ::= seq {
        id {
            local str "id1"
        },
        descr {
            source {
            genome mitochondrion,
            org {
            }
            },
            molinfo {
                biomol genomic,
                tech wgs
            }
        },
        inst {
            repr raw,
            mol dna,
            length 68,
            seq-data ncbi2na '1E555555555555555555555555555555C9'H
        }
    })";

    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(testEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    CNcbiOstrstream os;
    CFastaOstream fasta_os(os);
    fasta_os.Write(seh);
    auto s = string(CNcbiOstrstreamToString(os));
    string expected = ">lcl|id1 Mitochondrion, whole genome shotgun sequence\n"
    "ACTGCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCTAGC\n";

    BOOST_CHECK_EQUAL(s, expected);
}
