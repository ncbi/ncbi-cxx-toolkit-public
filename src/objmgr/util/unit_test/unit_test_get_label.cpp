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
* Author:  Colleen Bollin, NCBI
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
#include <objmgr/util/feature.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const char* sc_TestEntry; //


BOOST_AUTO_TEST_CASE(Test_feature_GetLabel)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CFeat_CI cds(seh, CSeqFeatData::eSubtype_cdregion);

    string label = kEmptyStr;
    
    feature::GetLabel(*(cds->GetSeq_feat()), &label, feature::fFGL_Content, &scope);
    BOOST_CHECK_EQUAL(label, "neuronal thread protein AD7c-NTP");
    label = kEmptyStr;

    feature::GetLabel(*(cds->GetSeq_feat()), &label, feature::fFGL_Type, &scope);
    BOOST_CHECK_EQUAL(label, "CDS");
    label = kEmptyStr;

    feature::GetLabel(*(cds->GetSeq_feat()), &label, feature::fFGL_Both, &scope);
    BOOST_CHECK_EQUAL(label, "CDS: neuronal thread protein AD7c-NTP");
    label = kEmptyStr;

    CFeat_CI prot(seh, CSeqFeatData::eSubtype_prot);
    feature::GetLabel(*(prot->GetSeq_feat()), &label, feature::fFGL_Content, &scope);
    BOOST_CHECK_EQUAL(label, "neuronal thread protein AD7c-NTP");
    label = kEmptyStr;

    feature::GetLabel(*(prot->GetSeq_feat()), &label, feature::fFGL_Type, &scope);
    BOOST_CHECK_EQUAL(label, "Prot");
    label = kEmptyStr;

    feature::GetLabel(*(prot->GetSeq_feat()), &label, feature::fFGL_Both, &scope);
    BOOST_CHECK_EQUAL(label, "Prot: neuronal thread protein AD7c-NTP");
    label = kEmptyStr;


}



//////////////////////////////////////////////////////////////////////////////

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
