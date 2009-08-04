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

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const char* sc_TestEntry; //
extern const char* sc_TestEntry_code_break; //
extern const char* sc_TestEntry_alt_frame; //
extern const char* sc_TestEntry_internal_stop; //
extern const char* sc_TestEntry_5prime_partial;
extern const char* sc_TestEntry_3prime_partial;


BOOST_AUTO_TEST_CASE(Test_TranslateCdregion)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CCdregion_translate
            tmp.clear();
            CCdregion_translate::TranslateCdregion
                (tmp, feat_iter->GetOriginalFeature(), scope, false);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CCdregion_translate, include the stop codon
            real_prot_seq += '*';
            tmp.clear();
            CCdregion_translate::TranslateCdregion
                (tmp, feat_iter->GetOriginalFeature(), scope, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_Raw)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CBioseq_Handle bsh = *bs_iter;
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);

        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            string nucleotide_sequence;
            vec.GetSeqData(feat_iter->GetTotalRange().GetFrom(),
                           feat_iter->GetTotalRange().GetTo() + 1,
                           nucleotide_sequence);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(nucleotide_sequence, tmp,
                                      NULL, false, true);

            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate(), include the stop codon
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(nucleotide_sequence, tmp,
                                      NULL, true, true);

            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeqVector)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            CSeqVector vec(feat_iter->GetLocation(), scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(vec, tmp,
                                      NULL, false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(vec, tmp,
                                      NULL, true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}

BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_loc_1)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), *bs_iter, tmp,
                                      NULL, false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), *bs_iter, tmp,
                                      NULL, true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_loc_2)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), scope, tmp,
                                      NULL, false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), scope, tmp,
                                      NULL, true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_code_break)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_code_break);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_alt_frame)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_alt_frame);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_internal_stop)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_internal_stop);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
                 real_prot_seq[51] = '*';
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_5prime_partial)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_5prime_partial);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
                 real_prot_seq[50] = '*';
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
            for (size_t i = 0;  i < real_prot_seq.size()  &&  i < tmp.size();  ++i) {
                if (real_prot_seq[i] != tmp[i]) {
                    LOG_POST(Error << "char " << i << ": "
                             << real_prot_seq[i] << " != "
                             << tmp[i]);
                }
            }

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}



BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_3prime_partial)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_3prime_partial);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq;
            {{
                 CBioseq_Handle bsh =
                     scope.GetBioseqHandle(feat_iter->GetProduct());
                 CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
                 vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
                 real_prot_seq[51] = '*';
             }}

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}




const char* sc_TestEntry ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_code_break ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MQFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            },\
            code-break {\
              {\
                loc int {\
                  from 17,\
                  to 19,\
                  strand plus,\
                  id gi 3002526\
                },\
                aa ncbieaa 81\
              }\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";


const char* sc_TestEntry_alt_frame ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame two,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 13,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_internal_stop ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILX\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_5prime_partial ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 374,\
        topology not-set,\
        seq-data ncbieaa \"EFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILX\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 17,\
            to 1141,\
            strand plus,\
            id gi 3002526,\
            fuzz-from lim tr\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_3prime_partial ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 374,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILX\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDL\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1135,\
            strand plus,\
            id gi 3002526,\
            fuzz-from lim tl\
          }\
        }\
      }\
    }\
  }\
}";

