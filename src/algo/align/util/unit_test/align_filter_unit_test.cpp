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
* Author:  Eyal Mozes
*
* File Description:
*   Unit tests for the CAlignFilter class.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/align/util/align_filter.hpp>
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
    arg_desc->AddKey("filters", "InputFilters",
                     "File containing filters and data-in subset expected "
                     "to match each one",
                     CArgDescriptions::eInputFile);
}

CNcbiOstream &operator<<(CNcbiOstream &ostr, const set<unsigned int> &t)
{
    ostr << '{';
    ITERATE (set<unsigned int>, it, t) {
        if (it != t.begin()) {
            ostr << ',';
        }
        ostr << *it;
    }
    ostr << '}';
    return ostr;
}

BOOST_AUTO_TEST_CASE(Test_Align_Filter)
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


    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    vector< CRef<CSeq_align> > alignments;
    CNcbiIstream& istr = args["data-in"].AsInputFile();
    while (istr) {
        CRef<CSeq_align> alignment(new CSeq_align);
        try {
            istr >> MSerial_AsnText >> *alignment;
        }
        catch (CEofException&) {
            break;
        }
        alignments.push_back(alignment);
    }

    CNcbiIstream& filters = args["filters"].AsInputFile();
    while (filters) {
        string filter_string;
        if (!NcbiGetlineEOL(filters, filter_string)) {
            break;
        }
        string results_string;
        NcbiGetlineEOL(filters, results_string);
        vector<string> tokens;
        NStr::Split(NStr::TruncateSpaces(results_string), " \t", tokens,
                       NStr::eNoMergeDelims);
        set<unsigned int> expected_results;
        ITERATE (vector<string>, it, tokens) {
            expected_results.insert(NStr::StringToUInt(*it));
        }
        set<unsigned int> actual_results;
        CAlignFilter filter(filter_string);
        filter.SetScope(*scope);
        ITERATE (vector< CRef<CSeq_align> >, it, alignments) {
        try {
            if (filter.Match(**it)) {
                actual_results.insert(it - alignments.begin());
            }
        } catch (CException &) {}
        }
        BOOST_CHECK(actual_results == expected_results);
        if (actual_results != expected_results) {
            cerr << filter_string << ": expected " << expected_results
                 << " actual " << actual_results << endl;
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
    title \"7e24e11.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3283436 3'\
 similar to gb:U02368 PAIRED BOX PROTEIN PAX-3 (HUMAN);.\",\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"biosample\",\
            tag id 156185\
          },\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
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
    title \"7e24e11.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3283436 3'\
 similar to SW:RL2B_HUMAN P29316 60S RIBOSOMAL PROTEIN L23A. ;.\",\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"biosample\",\
            tag id 156185\
          },\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
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
    title \"7e14h04.x1 NCI_CGAP_Lu24 Homo sapiens cDNA clone IMAGE:3282487 3'\
 similar to SW:RL2B_HUMAN P29316 60S RIBOSOMAL PROTEIN L23A. ;.\",\
    source {\
      org {\
        taxname \"Homo sapiens\",\
        common \"human\",\
        db {\
          {\
            db \"biosample\",\
            tag id 156185\
          },\
          {\
            db \"taxon\",\
            tag id 9606\
          }\
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
Seq-entry ::= seq {\
  id {\
    general {\
      db \"GENEMARK\",\
      tag id 4295\
    }\
  },\
  descr {\
    molinfo {\
      biomol peptide,\
      completeness complete\
    }\
  },\
  inst {\
    repr raw,\
    mol aa,\
    length 642,\
    seq-data iupacaa \"MFITRTFSDMKIGKKLGLSFGVLIVATLAIALLAFKGFQSIKENSAKQDVTV\
NMVNTLSKARMNRLLYQYTKDEQYAQVNARALNELSAHFDTLKKFDWNAQGEQQLDVLGSALQSYQTLRQAFYLASKK\
TFAASAVIQGNDLLTLGQSLDGVNIPAQPEAMLQVLRLASLLKEVAGDVERFIDKPTEASKVEIYGNITSIEQIRTQL\
SALAIPEIQTVLNTQKTDLTQLKQAFTDYMTAVGAEAAASSQLSAVAEKLNTSVAELFDYQASESTSALLNAERQIAV\
VAALCILLSLLVAWRITRAITVPLKETLSVAQRISEGDLTATLSTTRRDELGQLMQAVSVMNESLQNIITNVRDGVNS\
VARASSEIAAGNMDLSSRTEQQSAAVVQTAASMEELTSTVKQNAENAHHASQLATEASANAGRGGDIIRNVVTTMQGI\
TTSSGKIGEIISVINGISFQTNILALNAAVEAARAGEQGRGFAVVAGEVRNLAQRSSVAAKEIETLIRDSLHRVNEGS\
TLVDQAGSTMDEIVLSVTQVKDIMSEIAAASDEQNRGISQIAQAMTEMDTTTQQNAALVEESSAAASSLESQAEELEK\
TVAVFRLPANKSGMAVSHSTAKSVTKAPVSLRQPNPAEGNWETF\"\
  }\
}\
";
