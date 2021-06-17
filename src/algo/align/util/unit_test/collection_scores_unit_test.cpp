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
* Author:  Alex Kotliarov
*
* File Description:
*   Unit tests for the  CAlignmentCollectionScore.
*
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
#include <objects/general/Object_id.hpp>
#include <algo/align/util/collection_score.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <cmath>
#include <sstream>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string MakeKey(CScoreValue const&);
static string MakeKey(CSeq_align const&, string const&);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.
    arg_desc->AddKey("seq-entry", 
                     "SeqEntryPath",
                     "Path to sequences.",
                     CArgDescriptions::eInputFile,
                     CArgDescriptions::fBinary);
 
    arg_desc->AddKey("aligns", 
                     "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile,
                     CArgDescriptions::fBinary);

    arg_desc->AddFlag("input-binary", 
                      "Input data is ASN.1 Binary.");
                        
}

BOOST_AUTO_TEST_CASE(Test_AlignmentColl)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
 
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    bool ibinary = args["input-binary"];
    
    CRef<CSeq_entry> entry(new CSeq_entry);;
    args["seq-entry"].AsInputFile() >> MSerial_AsnBinary >> *entry;
    scope->AddTopLevelSeqEntry(*entry);

    CNcbiIstream& is = args["aligns"].AsInputFile();
    CRef<CSeq_align_set> aligns(new CSeq_align_set);
    list<CRef<CSeq_align> > coll;
    while ( is ) { 
        CSeq_align_set tmp;
        try {
            if ( ibinary ) {
                is >> MSerial_AsnBinary >> tmp;
            }
            else {
                is >> MSerial_AsnText >> tmp;
            }
        }
        catch (CEofException) {
            break;
        }
        coll.splice(coll.end(), tmp.Set());
    }
   
    aligns->Set() = coll;
   

    BOOST_CHECK( 120 == aligns->Get().size());

    CRef<IAlignmentCollectionScore> score = IAlignmentCollectionScore::GetInstance(*scope);

    // Retrieve qcovs scores: seq_percent_coverage.
    vector<CScoreValue> qcovs = score->Get("seq_percent_coverage", *aligns);

    const double epsilon = 0.0001;
    const double coverage = 99.8013;
 
    BOOST_CHECK( 120 == qcovs.size() );
    BOOST_CHECK( qcovs.front().GetQueryId().AsString() == "lcl|Sequence_1" );
    BOOST_CHECK( qcovs.front().GetSubjectId().AsString() == "gi|219857159" );
    BOOST_CHECK( qcovs.front().GetName() == "seq_percent_coverage" );
    BOOST_CHECK( abs(qcovs.front().GetValue() - coverage) < epsilon );

    // Retrieve qcovus scores: uniq_seq_percent_coverage.
    vector<CScoreValue> qcovus = score->Get("uniq_seq_percent_coverage", *aligns);

    BOOST_CHECK( 120 == qcovus.size() );
    BOOST_CHECK( qcovus.front().GetQueryId().AsString() == "lcl|Sequence_1" );
    BOOST_CHECK( qcovus.front().GetSubjectId().AsString() == "gi|219857159" );
    BOOST_CHECK( qcovus.front().GetName() == "uniq_seq_percent_coverage" );
    BOOST_CHECK( abs(qcovus.front().GetValue() - coverage) < epsilon );


    char const* names[] = {"seq_percent_coverage", "uniq_seq_percent_coverage"};
    vector<string> score_names(names, names + sizeof(names) / sizeof(char*));

    // Retrieve scores as a group.
    vector<CScoreValue> all = score->Get("subjects-sequence-coverage-group", score_names, *aligns); 
    BOOST_CHECK( 240 == all.size() );   

    map<string, double> score_table;
    for ( vector<CScoreValue>::const_iterator i = qcovs.begin(); i != qcovs.end(); ++i ) {
        string key = MakeKey(*i);
        score_table.insert(make_pair(MakeKey(*i), i->GetValue()));
    }

    for ( vector<CScoreValue>::const_iterator i = qcovus.begin(); i != qcovus.end(); ++i ) {
        string key = MakeKey(*i);
        score_table.insert(make_pair(MakeKey(*i), i->GetValue()));
    }

    for ( vector<CScoreValue>::const_iterator i = all.begin(); i != all.end(); ++i ) {
        string key = MakeKey(*i);
        BOOST_CHECK ( score_table.count(key) > 0 );
        BOOST_CHECK ( abs(score_table.at(key) - i->GetValue()) < epsilon );
    }
     
    // Set named scores.
    score->Set("subjects-sequence-coverage-group", score_names, *aligns);

    for (list<CRef<CSeq_align> >::const_iterator i = aligns->Get().begin(); i != aligns->Get().end(); ++i ) {
        {{ 
            string key = MakeKey(**i, "seq_percent_coverage");

            BOOST_CHECK ( score_table.count(key) > 0 );
            double value = 0.;
            (*i)->GetNamedScore("seq_percent_coverage", value);   
            BOOST_CHECK ( abs(score_table.at(key) - value) < epsilon );
        }}

        {{
            string key = MakeKey(**i, "uniq_seq_percent_coverage");

            BOOST_CHECK ( score_table.count(key) > 0 );
            double value = 0.;
            (*i)->GetNamedScore("uniq_seq_percent_coverage", value);   
            BOOST_CHECK ( abs(score_table.at(key) - value) < epsilon );
        }}
    }
}

string MakeKey(CScoreValue const& value)
{
    ostringstream oss;
    oss << value.GetQueryId().AsString()
        << '\t'
        << value.GetSubjectId().AsString()
        << '\t'
        << value.GetName();
    return oss.str();
}

string MakeKey(CSeq_align const& align, string const& score_name)
{
    ostringstream oss;
    oss << align.GetSeq_id(0).AsFastaString()
        << '\t'
        << align.GetSeq_id(1).AsFastaString()
        << '\t'
        << score_name;
    return oss.str();
}

