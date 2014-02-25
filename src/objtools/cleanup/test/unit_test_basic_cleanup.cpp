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
* Author:  Colleen Bollin, Michael Kornbluh, NCBI
*
* File Description:
*   Sample unit tests file for basic cleanup.
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
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

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

extern const char* sc_TestEntryCleanRptUnitSeq;

BOOST_AUTO_TEST_CASE(Test_CleanRptUnitSeq)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanRptUnitSeq);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() < 1) {
        BOOST_CHECK_EQUAL("missing cleanup", "Change Qualifiers");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Change Qualifiers");
        for (size_t i = 2; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    CFeat_CI f(scope->GetBioseqHandle(entry.GetSeq()));
    while (f) {
        FOR_EACH_GBQUAL_ON_SEQFEAT (q, *f) {
            if ((*q)->IsSetQual() && NStr::Equal((*q)->GetQual(), "rpt_unit_seq") && (*q)->IsSetVal()) {
                string val = (*q)->GetVal();
                string expected = NStr::ToLower(val); 
                BOOST_CHECK_EQUAL (val, expected);
            }
        }
        ++f;
    }
}


const char *sc_TestEntryCleanRptUnitSeq = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanrptunitseq\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } , \
              annot { \
                { \
                  data \
                    ftable { \
                      { \
                        data \
                        imp { \
                          key \"repeat_region\" } , \
                        location \
                          int { \
                            from 0 , \
                            to 26 , \
                            id local str \"cleanrptunitseq\" } , \
                          qual { \
                            { \
                              qual \"rpt_unit_seq\" , \
                              val \"AATT\" } } } , \
                      { \
                        data \
                        imp { \
                          key \"repeat_region\" } , \
                        location \
                          int { \
                            from 0 , \
                            to 26 , \
                            id local str \"cleanrptunitseq\" } , \
                          qual { \
                            { \
                              qual \"rpt_unit_seq\" , \
                              val \"AATT;GCC\" } } } } } } } \
";

typedef vector< CRef<CSeq_entry> > TSeqEntryVec;

static
CObjectIStreamAsn *s_OpenCompressedFile( const string &file_name )
{
    // owned by pUnzipStream
    CZipStreamDecompressor* pDecompressor = 
        new CZipStreamDecompressor(512, 512, kZlibDefaultWbits, CZipCompression::fGZip );

    // owned by pUnzipStream
    CNcbiIfstream *input_file = new CNcbiIfstream( file_name.c_str(), ios_base::binary | ios_base::in );

    // owned by the returned CObjectIStreamAsn
    CCompressionIStream* pUnzipStream = new CCompressionIStream(
        *input_file, pDecompressor, CCompressionIStream::fOwnAll );

    return new CObjectIStreamAsn( *pUnzipStream, eTakeOwnership );
}

void s_LoadAllSeqEntries( 
    TSeqEntryVec &out_expected_seq_entries, 
    CObjectIStreamAsn *in_stream )
{
    if( ! in_stream ) {
        return;
    }

    try {
        while( in_stream->InGoodState() ) {
            // get our expected output
            CRef<CSeq_entry> expected_output_seq_entry( new CSeq_entry );
            // The following line throws CEofException if we reach the end
            in_stream->Read( ObjectInfo(*expected_output_seq_entry) );
            if( expected_output_seq_entry ) {
                out_expected_seq_entries.push_back( expected_output_seq_entry );
            }
        }
    } catch(const CEofException &) {
        // okay, no problem
    }
}

static
void s_LoadExpectedOutput( 
    TSeqEntryVec &out_expected_seq_entries, 
    string input_file_name ) // yes, COPY input_file_name since we're going to change it
{
    // remove the part after the second to last period
    string::size_type last_period_pos = input_file_name.find_last_of(".");
    _ASSERT(last_period_pos != string::npos);
    input_file_name.resize( last_period_pos );
    last_period_pos = input_file_name.find_last_of(".");
    _ASSERT(last_period_pos != string::npos);
    input_file_name.resize( last_period_pos );

    auto_ptr<CObjectIStreamAsn> result;

    // now, find answer file (which could be a .answer file
    // or a .cleanasns_output file )
    string answer_file = input_file_name + ".answer.gz";
    // expected_output_file.open( answer_file.c_str() );
    result.reset( s_OpenCompressedFile(answer_file) );
    s_LoadAllSeqEntries( out_expected_seq_entries, result.get() );
    if( out_expected_seq_entries.empty() ) {
        string cleanasns_output_file = input_file_name + ".cleanasns_output.gz";
        result.reset( s_OpenCompressedFile(cleanasns_output_file) );
        s_LoadAllSeqEntries( out_expected_seq_entries, result.get() );
        if( out_expected_seq_entries.empty() ) {
            result.reset();
            BOOST_CHECK_EQUAL("Could not open answer file for", input_file_name );
        }
    }
}

enum EExtendedCleanup {
    eExtendedCleanup_DoNotRun,
    eExtendedCleanup_Run
};

static
void s_ProcessOneEntry( 
    CSeq_entry &input_seq_entry, 
    const CSeq_entry &expected_output_seq_entry,
    EExtendedCleanup eExtendedCleanup)
{
    // clean input, then compare to output
    CCleanup cleanup;
    if( eExtendedCleanup == eExtendedCleanup_Run ) {
        // extended cleanup includes basic cleanup
        cleanup.ExtendedCleanup( input_seq_entry, 
            CCleanup::eClean_NoNcbiUserObjects );
    } else {
        cleanup.BasicCleanup( input_seq_entry );
    }
    const bool bSeqEntriesAreEqual = 
        input_seq_entry.Equals(expected_output_seq_entry);
    BOOST_CHECK( bSeqEntriesAreEqual );
    if( ! bSeqEntriesAreEqual ) {
        // dump out the whole seq-entry that doesn't match, but ONLY 
        // for the first match failure to avoid flooding the logs
        static bool s_bFirstSeqEntryDump = true;
        if( s_bFirstSeqEntryDump ) {
            cerr << "The entry that was received: " << endl;
            cerr << MSerial_AsnText << input_seq_entry << endl;
            cerr << "The entry that was expected: " << endl;
            cerr << MSerial_AsnText << expected_output_seq_entry << endl;
            s_bFirstSeqEntryDump = false;
        }
    }
}

static
bool s_IsInputFileNameOkay( string file_name ) // yes, COPY file_name because we're changing it
{
    // normalize slashes (might be needed later)
    NStr::ReplaceInPlace( file_name, "\\", "/" );

    if( ! NStr::EndsWith( file_name, ".raw_test.gz" ) &&
        ! NStr::EndsWith( file_name, ".template_expanded.gz") ) 
    {
        return false;
    }

    const char *kBadSubstrings[] = {
        "highest_level", // don't process non-Seq-entry files (they can be done manually, if desired)
        "unusable",
        NULL // last must be NULL to mark end
    };

    // a file_name is bad if it contains any of the bad substrings
    for( int str_idx = 0; kBadSubstrings[str_idx] != NULL; ++str_idx ) {
        if( NStr::FindNoCase(file_name, kBadSubstrings[str_idx]) != NPOS ) {
            return false;
        }
    }

    return true;
}

static
void s_ProcessInputFile( const string &input_file_name )
{
    // load all uncleaned input Seq-entries
    TSeqEntryVec seq_entries;
    auto_ptr<CObjectIStreamAsn> input_obj_stream(
        s_OpenCompressedFile( input_file_name ) );
    if( input_obj_stream.get() ) {
        s_LoadAllSeqEntries( 
            seq_entries, 
            input_obj_stream.get() );
    }
    BOOST_CHECK( ! seq_entries.empty() );

    // load all expected cleaned output Seq-entries
    TSeqEntryVec expected_seq_entries;
    s_LoadExpectedOutput( expected_seq_entries, input_file_name );
    BOOST_CHECK( ! expected_seq_entries.empty() );

    BOOST_CHECK( seq_entries.size() == expected_seq_entries.size() );

    // see whether or not to run extended cleanup, which depends
    // on the file path
    EExtendedCleanup eExtendedCleanup = eExtendedCleanup_DoNotRun;
    if( NStr::FindNoCase(input_file_name, "ExtendedCleanup") != NPOS ) {
        eExtendedCleanup = eExtendedCleanup_Run;
    }

    // The file might contain multiple test cases
    const size_t num_entries_to_check = 
        min( seq_entries.size(), expected_seq_entries.size() );
    size_t curr_idx = 0;
    for( ; curr_idx < num_entries_to_check; ++curr_idx ) {
        s_ProcessOneEntry( *seq_entries[curr_idx], 
            *expected_seq_entries[curr_idx],
            eExtendedCleanup);
    }
}

// Holds all the matching files
class CFileRememberer : public set<string> {
public:
    void operator() (CDirEntry& de) {
        if( de.IsFile() ) {
            insert( de.GetPath() );
        }
    }
};

// This will load up various files, clean them, and
// make sure they match expected output.
BOOST_AUTO_TEST_CASE(Test_CornerCaseFiles)
{
    const static string kTestDir = "test_cases";
    CDir test_dir( kTestDir );

    // get all input files
    CFileRememberer file_rememberer;
    {
        vector<string> kInputFileMasks;
        kInputFileMasks.push_back( "*.raw_test.gz" );
        kInputFileMasks.push_back( "*.template_expanded.gz" );

        vector<string> kInputDirMasks;

        FindFilesInDir( test_dir, kInputFileMasks, kInputDirMasks, file_rememberer, fFF_Default | fFF_Recursive );
    }

    // process each input file
    BOOST_CHECK( ! file_rememberer.empty() );
    ITERATE( CFileRememberer, file_name_iter, file_rememberer ) {

        const string &input_file_name = *file_name_iter;

        if( s_IsInputFileNameOkay(input_file_name) ) {
            cout << "Processing file \"" << input_file_name << "\"" << endl;
        } else {
            // it's okay to skip  a file if it's justified
            cout << "Skipping file   \"" << input_file_name << "\"" << endl;
            continue;
        }

        // process this set of inputs and expected outputs:
        BOOST_CHECK_NO_THROW( s_ProcessInputFile(input_file_name) );
    }
}


const char *sc_TestEntryCleanAssemblyDate = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanassemblydate\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" } } , \
                user { \
                  type str \"StructuredComment\" , \
                  data { \
                    { \
                      label str \"StructuredCommentPrefix\" , \
                      data str \"##Genome-Assembly-Data-START##\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"Feb 1 2014\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"Feb 2014\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"2014\" } , \
                    { \
                      label  str \"StructuredCommentSuffix\" , \
                      data str \"##Genome-Assembly-Data-END##\" } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } } \
";


BOOST_AUTO_TEST_CASE(Test_CleanAssemblyDate)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanAssemblyDate);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() < 1) {
        BOOST_CHECK_EQUAL("missing cleanup", "Clean User-Object Or -Field");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Clean User-Object Or -Field");
        for (size_t i = 2; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    CSeqdesc_CI d(scope->GetBioseqHandle(entry.GetSeq()), CSeqdesc::e_User);
    if (d) {
        const CUser_object& usr = d->GetUser();
        BOOST_CHECK_EQUAL(usr.GetData()[1]->GetData().GetStr(), "2014-FEB-1");
        BOOST_CHECK_EQUAL(usr.GetData()[2]->GetData().GetStr(), "2014-FEB");
        BOOST_CHECK_EQUAL(usr.GetData()[3]->GetData().GetStr(), "2014");
    }
}
