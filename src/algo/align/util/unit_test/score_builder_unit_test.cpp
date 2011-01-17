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
* Author:  Christiam Camacho, NCBI
*
* File Description:
*   Unit tests for the CScoreBuilder class.
*
* NOTE:
*   Boost.Test reports some memory leaks when compiled in MSVC even for this
*   simple code. Maybe it's related to some toolkit static variables.
*   To avoid annoying messages about memory leaks run this program with
*   parameter --detect_memory_leak=0
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */

extern const char *sc_TestEntries;

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);
}


BOOST_AUTO_TEST_CASE(Test_Score_Builder)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    {{
         CNcbiIstrstream istr(sc_TestEntries);
         while (istr) {
             CRef<CSeq_entry> entry(new CSeq_entry);
             try {
                 istr >> MSerial_AsnText >> *entry;
             }
             catch (CEofException&) {
                 break;
             }
             scope->AddTopLevelSeqEntry(*entry);
         }
    }}


    CScoreBuilder score_builder(blast::eBlastn);

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();
    while (istr) {
        CSeq_align alignment;
        try {
            istr >> MSerial_AsnText >> alignment;
        }
        catch (CEofException&) {
            break;
        }

        switch (alignment.GetSegs().Which()) {
        case CSeq_align::TSegs::e_Denseg:
            LOG_POST(Error << "checking alignment: Dense-seg");
            break;
        case CSeq_align::TSegs::e_Disc:
            LOG_POST(Error << "checking alignment: Disc");
            break;
        case CSeq_align::TSegs::e_Std:
            LOG_POST(Error << "checking alignment: Std-seg");
            break;
        case CSeq_align::TSegs::e_Spliced:
            LOG_POST(Error << "checking alignment: Spliced-seg");
            break;

        default:
            LOG_POST(Error << "checking alignment: unknown");
            break;
        }

        ///
        /// we test several distinct scores
        ///

        int kExpectedIdentities = 0;
        bool has_kExpectedIdentities = 
        alignment.GetNamedScore(CSeq_align::eScore_IdentityCount,
                                kExpectedIdentities);

        int kExpectedMismatches = 0;
        bool has_kExpectedMismatches = 
        alignment.GetNamedScore(CSeq_align::eScore_MismatchCount,
                                kExpectedMismatches);

        int kExpectedGapOpen = 0;
        bool has_kExpectedGapOpen = 
        alignment.GetNamedScore("gapopen",
                                kExpectedGapOpen);

        int kExpectedGaps = 0;
        bool has_kExpectedGaps = 
        alignment.GetNamedScore("gap_bases",
                                kExpectedGaps);

        int kExpectedLength = 0;
        bool has_kExpectedLength = 
        alignment.GetNamedScore("length",
                                kExpectedLength);

        int kExpectedScore = 0;
        bool has_kExpectedScore = 
        alignment.GetNamedScore(CSeq_align::eScore_Score,
                                kExpectedScore);

        double kExpectedPctIdentity_Gapped = 0;
        bool has_kExpectedPctIdentity_Gapped = 
        alignment.GetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped,
                                kExpectedPctIdentity_Gapped);

        double kExpectedPctIdentity_Ungapped = 0;
        bool has_kExpectedPctIdentity_Ungapped = 
        alignment.GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped,
                                kExpectedPctIdentity_Ungapped);

        double kExpectedPctIdentity_GapOpeningOnly = 0;
        bool has_kExpectedPctIdentity_GapOpeningOnly = 
        alignment.GetNamedScore(CSeq_align::eScore_PercentIdentity_GapOpeningOnly,
                                kExpectedPctIdentity_GapOpeningOnly);

        double kExpectedPctCoverage = 0;
        bool has_kExpectedPctCoverage = 
        alignment.GetNamedScore(CSeq_align::eScore_PercentCoverage,
                                kExpectedPctCoverage);

        double kExpectedHighQualityPctCoverage = 0;
        bool has_kExpectedHighQualityPctCoverage = 
        alignment.GetNamedScore(CSeq_align::eScore_HighQualityPercentCoverage,
                                kExpectedHighQualityPctCoverage);

        /// reset scores to avoid any taint in score generation
        alignment.ResetScore();

        /// check alignment length
        if(has_kExpectedLength)
        {{
             int actual =
                 score_builder.GetAlignLength(alignment);
             LOG_POST(Error << "Verifying score: length: "
                      << kExpectedLength << " == " << actual);
             BOOST_CHECK_EQUAL(kExpectedLength, actual);
         }}

        /// check identity count
        /// NB: not for std-segs!
        if(has_kExpectedIdentities)
        if ( !alignment.GetSegs().IsStd() ) {
            int actual =
                score_builder.GetIdentityCount(*scope, alignment);
            LOG_POST(Error << "Verifying score: num_ident: "
                     << kExpectedIdentities << " == " << actual);
            BOOST_CHECK_EQUAL(kExpectedIdentities, actual);
        }

        /// check mismatch count
        if(has_kExpectedMismatches)
        if ( !alignment.GetSegs().IsStd() ) {
            int actual =
                score_builder.GetMismatchCount(*scope, alignment);
            LOG_POST(Error << "Verifying score: num_mismatch: "
                     << kExpectedMismatches << " == " << actual);
            BOOST_CHECK_EQUAL(kExpectedMismatches, actual);
        }

        /// check uninitialized / wrongly initialized variables
        /// (CXX-1594 - GetMismatchCount() adds to incoming values blindly)
        if(has_kExpectedMismatches || has_kExpectedIdentities)
        if ( !alignment.GetSegs().IsStd() ) {
            int mismatches = 1000;
            int identities = 1000;
            score_builder.GetMismatchCount(*scope, alignment,
                                           identities, mismatches);
            BOOST_CHECK_EQUAL(kExpectedMismatches, mismatches);
            BOOST_CHECK_EQUAL(kExpectedIdentities, identities);
        }

        /// check gap count (= gap openings)
        if(has_kExpectedGapOpen)
        {{
             int actual =
                 score_builder.GetGapCount(alignment);
             LOG_POST(Error << "Verifying score: gapopen: "
                      << kExpectedGapOpen << " == " << actual);
             BOOST_CHECK_EQUAL(kExpectedGapOpen, actual);
         }}

        /// check gap base length (= sum of lengths of all gaps)
        if(has_kExpectedGaps)
        {{
             int actual =
                 score_builder.GetGapBaseCount(alignment);
             LOG_POST(Error << "Verifying score: gap_bases: "
                      << kExpectedGaps << " == " << actual);
             BOOST_CHECK_EQUAL(kExpectedGaps, actual);
         }}

        /// check percent identity (gapped)
        if(has_kExpectedPctIdentity_Gapped)
        if ( !alignment.GetSegs().IsStd() ) {
            double actual =
                score_builder.GetPercentIdentity(*scope, alignment,
                                                 CScoreBuilder::eGapped);
            LOG_POST(Error << "Verifying score: pct_identity_gap: "
                     << kExpectedPctIdentity_Gapped << " == " << actual);

            /// machine precision is a problem here
            /// we verify to 12 digits of precision
            Uint8 int_pct_identity_gapped =
                kExpectedPctIdentity_Gapped * 1e12;
            Uint8 int_pct_identity_actual = actual * 1e12;
            BOOST_CHECK_EQUAL(int_pct_identity_gapped,
                              int_pct_identity_actual);

            /**
              CScore score;
              score.SetId().SetStr("pct_identity_gap");
              score.SetValue().SetReal(actual);
              cerr << MSerial_AsnText << score;
             **/
        }

        /// check percent identity (ungapped)
        if(has_kExpectedPctIdentity_Ungapped)
        if ( !alignment.GetSegs().IsStd() ) {
            double actual =
                score_builder.GetPercentIdentity(*scope, alignment,
                                                 CScoreBuilder::eUngapped);
            LOG_POST(Error << "Verifying score: pct_identity_ungap: "
                     << kExpectedPctIdentity_Ungapped << " == " << actual);

            /// machine precision is a problem here
            /// we verify to 12 digits of precision
            Uint8 int_pct_identity_ungapped =
                kExpectedPctIdentity_Ungapped * 1e12;
            Uint8 int_pct_identity_actual = actual * 1e12;
            BOOST_CHECK_EQUAL(int_pct_identity_ungapped,
                              int_pct_identity_actual);

            /**
              CScore score;
              score.SetId().SetStr("pct_identity_ungap");
              score.SetValue().SetReal(actual);
              cerr << MSerial_AsnText << score;
             **/
        }

        /// check percent identity (GapOpeningOnly-style)
        if(has_kExpectedPctIdentity_GapOpeningOnly)
        if ( !alignment.GetSegs().IsStd() ) {
            double actual =
                score_builder.GetPercentIdentity(*scope, alignment,
                                                 CScoreBuilder::eGBDNA);
            LOG_POST(Error << "Verifying score: pct_identity_gapopen_only: "
                     << kExpectedPctIdentity_GapOpeningOnly << " == " << actual);

            /// machine precision is a problem here
            /// we verify to 12 digits of precision
            Uint8 int_pct_identity_gbdna =
                kExpectedPctIdentity_GapOpeningOnly * 1e12;
            Uint8 int_pct_identity_actual = actual * 1e12;
            BOOST_CHECK_EQUAL(int_pct_identity_gbdna,
                              int_pct_identity_actual);

            /**
              CScore score;
              score.SetId().SetStr("pct_identity_gbdna");
              score.SetValue().SetReal(actual);
              cerr << MSerial_AsnText << score;
             **/
        }

        /// check percent coverage
        if(has_kExpectedPctCoverage)
        {{
             double actual =
                 score_builder.GetPercentCoverage(*scope, alignment);
             LOG_POST(Error << "Verifying score: pct_coverage: "
                      << kExpectedPctCoverage << " == " << actual);

             /// machine precision is a problem here
             /// we verify to 12 digits of precision
             Uint8 int_pct_coverage_expected = kExpectedPctCoverage * 1e12;
             Uint8 int_pct_coverage_actual = actual * 1e12;
             BOOST_CHECK_EQUAL(int_pct_coverage_expected,
                               int_pct_coverage_actual);

             /**
             CScore score;
             score.SetId().SetStr("pct_coverage");
             score.SetValue().SetReal(actual);
             cerr << MSerial_AsnText << score;
             **/
         }}

        /// check high-quality percent coverage if data has it
        if(has_kExpectedHighQualityPctCoverage)
        {{
             double actual;
             score_builder.AddScore(*scope, alignment, CSeq_align::eScore_HighQualityPercentCoverage);
             alignment.GetNamedScore(CSeq_align::eScore_HighQualityPercentCoverage, actual);
             LOG_POST(Error << "Verifying score: pct_coverage_hiqual: "
                      << kExpectedHighQualityPctCoverage << " == " << actual);

             /// machine precision is a problem here
             /// we verify to 12 digits of precision
             Uint8 int_hq_pct_coverage_expected = kExpectedHighQualityPctCoverage * 1e12;
             Uint8 int_hq_pct_coverage_actual = actual * 1e12;
             BOOST_CHECK_EQUAL(int_hq_pct_coverage_expected,
                               int_hq_pct_coverage_actual);

             /**
             CScore score;
             score.SetId().SetStr("pct_coverage_hiqual");
             score.SetValue().SetReal(actual);
             cerr << MSerial_AsnText << score;
             **/
         }}

        if(has_kExpectedScore)
        if (alignment.GetSegs().IsDenseg()) {
            ///
            /// our encoded dense-segs have a BLAST-style 'score'
            ///
            int actual =
                score_builder.GetBlastScore(*scope, alignment);
            LOG_POST(Error << "Verifying score: score: "
                     << kExpectedScore << " == " << actual);
            BOOST_CHECK_EQUAL(kExpectedScore, actual);
        }
    }
}

const char *sc_TestEntries = "\
Seq-entry ::= seq {\
  id {\
    general {\
      db \"dbEST\",\
      tag id 5929311\
    },\
    genbank {\
      accession \"BE669435\",\
      version 1\
    },\
    gi 10029976\
  },\
  descr {\
    molinfo {\
      biomol mRNA,\
      tech est\
    },\
    title \"7e24e11.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3283436 3'\
 similar to gb:U02368 PAIRED BOX PROTEIN PAX-3 (HUMAN);.\",\
    create-date std {\
      year 2000,\
      month 9,\
      day 8\
    },\
    update-date std {\
      year 2011,\
      month 1,\
      day 8\
    },\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
        },\
        orgname {\
          name binomial {\
            genus \"Homo\",\
            species \"sapiens\"\
          },\
          mod {\
            {\
              subtype other,\
              subname \"Organ: lung; Vector: pT7T3D-PacI; Plasmid DNA from the\
 normalized library NCI_CGAP_Lu5 was prepared, and ss circles were made in\
 vitro. Following HAP purification, this DNA was used as tracer in a\
 subtractive hybridization reaction. The driver was PCR-amplified cDNAs from a\
 pool of 5,000 clones made from the same library (cloneIDs 1414920-1417991 and\
 1520904-1522439). Subtraction by Bento Soares and M. Fatima Bonaldo.  \"\
            }\
          },\
          lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Mammalia; Eutheria; Euarchontoglires; Primates; Haplorrhini;\
 Catarrhini; Hominidae; Homo\",\
          gcode 1,\
          mgcode 2,\
          div \"PRI\"\
        }\
      },\
      subtype {\
        {\
          subtype clone,\
          name \"IMAGE:3283436\"\
        },\
        {\
          subtype clone-lib,\
          name \"LIBEST_001611 NCI_CGAP_Lu24\"\
        },\
        {\
          subtype tissue-type,\
          name \"carcinoid\"\
        },\
        {\
          subtype lab-host,\
          name \"DH10B\"\
        }\
      }\
    }\
  },\
  inst {\
    repr raw,\
    mol rna,\
    length 683,\
    strand ss,\
    seq-data ncbi2na 'FFC14200700A8BEB8084DFE879F774BD79ED210DE0B1FF0BB05E7470\
5749784549CEEDBEDF847BBA827FAFA9044FB40BC0350ECDD4D4E2B4F5838E8F53B44B7099D0E0\
4E5350B47EA27DD7AE882953DE53253E49E7468A2C9554A91D3793AAEA92291FB12BB7D1FAB4A6\
BD3158AEEA93AE7C5BBAA4AA195BC1E48EDE7893B526EACEA27A8F4D3FE7388E5E9E532BE13858\
3CA9D3CE1BDE852C769EFA9EA7144E3413BB8C3B7C1EA1A22C20'H\
  },\
  annot {\
    {\
      desc {\
        name \"NCBI_GPIPE\"\
      },\
      data ftable {\
        {\
          data region \"alignable\",\
          location int {\
            from 0,\
            to 426,\
            strand unknown,\
            id gi 10029976\
          }\
        }\
      }\
    }\
  }\
}\
Seq-entry ::= seq {\
  id {\
    general {\
      db \"dbEST\",\
      tag id 5929424\
    },\
    genbank {\
      accession \"BE669548\",\
      version 1\
    },\
    gi 10030089\
  },\
  descr {\
    molinfo {\
      biomol mRNA,\
      tech est\
    },\
    title \"7e14g09.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3282496 3'\
 similar to SW:RL2B_HUMAN P29316 60S RIBOSOMAL PROTEIN L23A. ;.\",\
    create-date std {\
      year 2000,\
      month 9,\
      day 8\
    },\
    update-date std {\
      year 2011,\
      month 1,\
      day 8\
    },\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
        },\
        orgname {\
          name binomial {\
            genus \"Homo\",\
            species \"sapiens\"\
          },\
          mod {\
            {\
              subtype other,\
              subname \"Organ: lung; Vector: pT7T3D-PacI; Plasmid DNA from the\
 normalized library NCI_CGAP_Lu5 was prepared, and ss circles were made in\
 vitro. Following HAP purification, this DNA was used as tracer in a\
 subtractive hybridization reaction. The driver was PCR-amplified cDNAs from a\
 pool of 5,000 clones made from the same library (cloneIDs 1414920-1417991 and\
 1520904-1522439). Subtraction by Bento Soares and M. Fatima Bonaldo.  \"\
            }\
          },\
          lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Mammalia; Eutheria; Euarchontoglires; Primates; Haplorrhini;\
 Catarrhini; Hominidae; Homo\",\
          gcode 1,\
          mgcode 2,\
          div \"PRI\"\
        }\
      },\
      subtype {\
        {\
          subtype clone,\
          name \"IMAGE:3282496\"\
        },\
        {\
          subtype clone-lib,\
          name \"LIBEST_001611 NCI_CGAP_Lu24\"\
        },\
        {\
          subtype tissue-type,\
          name \"carcinoid\"\
        },\
        {\
          subtype lab-host,\
          name \"DH10B\"\
        }\
      }\
    }\
  },\
  inst {\
    repr raw,\
    mol rna,\
    length 470,\
    strand ss,\
    seq-data ncbi2na 'E6965809820A09D79575C09E09409829FC0A504292EF80AED4494400\
8208D646D1517D69A58211E61D6884950335DA0899D52880427E14739CD342FD67851E2DE53810\
8C81104111FBBD3EE8EF0250424523C04A7B8209ECE13E3BAD06D04578F7978E88820A4CEF61E9\
D78F18E7FA3BE50403E234D0'H\
  },\
  annot {\
    {\
      desc {\
        name \"NCBI_GPIPE\"\
      },\
      data ftable {\
        {\
          data region \"alignable\",\
          location int {\
            from 0,\
            to 417,\
            strand unknown,\
            id gi 10030089\
          }\
        }\
      }\
    }\
  }\
}\
Seq-entry ::= seq {\
  id {\
    general {\
      db \"dbEST\",\
      tag id 5929425\
    },\
    genbank {\
      accession \"BE669549\",\
      version 1\
    },\
    gi 10030090\
  },\
  descr {\
    molinfo {\
      biomol mRNA,\
      tech est\
    },\
    title \"7e14h04.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3282487 3'\
 similar to SW:RL2B_HUMAN P29316 60S RIBOSOMAL PROTEIN L23A. ;.\",\
    create-date std {\
      year 2000,\
      month 9,\
      day 8\
    },\
    update-date std {\
      year 2011,\
      month 1,\
      day 8\
    },\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
        },\
        orgname {\
          name binomial {\
            genus \"Homo\",\
            species \"sapiens\"\
          },\
          mod {\
            {\
              subtype other,\
              subname \"Organ: lung; Vector: pT7T3D-PacI; Plasmid DNA from the\
 normalized library NCI_CGAP_Lu5 was prepared, and ss circles were made in\
 vitro. Following HAP purification, this DNA was used as tracer in a\
 subtractive hybridization reaction. The driver was PCR-amplified cDNAs from a\
 pool of 5,000 clones made from the same library (cloneIDs 1414920-1417991 and\
 1520904-1522439). Subtraction by Bento Soares and M. Fatima Bonaldo.  \"\
            }\
          },\
          lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Mammalia; Eutheria; Euarchontoglires; Primates; Haplorrhini;\
 Catarrhini; Hominidae; Homo\",\
          gcode 1,\
          mgcode 2,\
          div \"PRI\"\
        }\
      },\
      subtype {\
        {\
          subtype clone,\
          name \"IMAGE:3282487\"\
        },\
        {\
          subtype clone-lib,\
          name \"LIBEST_001611 NCI_CGAP_Lu24\"\
        },\
        {\
          subtype tissue-type,\
          name \"carcinoid\"\
        },\
        {\
          subtype lab-host,\
          name \"DH10B\"\
        }\
      }\
    }\
  },\
  inst {\
    repr raw,\
    mol rna,\
    length 490,\
    strand ss,\
    seq-data ncbi2na 'D108E9960260828275E55D70278250260A7F029420A4BBE02BB51251\
0020823591B4545F5A69608479875A212540CD768226754A20109F851CE734D0BF59E1478B794E\
0823208410447EEF4FBA3BC094109148F0129EE0827B384F8EE942B4115E3DA5E3A22082933BD8\
7A75E3C639FE8EF94100FA8D3701E2D490'H\
  },\
  annot {\
    {\
      desc {\
        name \"NCBI_GPIPE\"\
      },\
      data ftable {\
        {\
          data region \"alignable\",\
          location int {\
            from 0,\
            to 438,\
            strand unknown,\
            id gi 10030090\
          }\
        }\
      }\
    }\
  }\
}\
";
