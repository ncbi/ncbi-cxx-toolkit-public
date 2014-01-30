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
*   Sample unit tests file for the mainstream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework (which
* is based on Boost.Test framework. For more advanced techniques look into
* another sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/test_boost.hpp>

#include <objtools/discrepancy_report/hDiscRep_tests.hpp>

#include <objtools/discrepancy_report/hDiscRep_output.hpp>
#include <objtools/discrepancy_report/hDiscRep_config.hpp>

const char* sc_TestEntryCollidingLocusTags ="Seq-entry ::= seq {\
     id {\
       local str \"LocusCollidesWithLocusTag\" } ,\
     inst {\
       repr raw ,\
       mol dna ,\
       length 24 ,\
       seq-data\
         iupacna \"AATTGGCCAANNAATTGGCCAANN\" } ,\
     annot {\
       {\
         data\
           ftable {\
             {\
               data\
                 gene {\
                   locus \"foo\" ,\
                   locus-tag \"foo\" } ,\
               location\
                 int {\
                   from 0 ,\
                   to 4 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"bar\" ,\
                   locus-tag \"foo\" } ,\
               location\
                 int {\
                   from 5 ,\
                   to 9 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"bar\" ,\
                   locus-tag \"baz\" } ,\
               location\
                 int {\
                   from 10 ,\
                   to 14 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"quux\" ,\
                   locus-tag \"baz\" } ,\
               location\
                 int {\
                   from 15 ,\
                   to 19 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } } } } }\
 ";


BEGIN_NCBI_SCOPE
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default in"
         "order to avoid unwanted failure during the daily automated builds.");
}



NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    cout << "Finalization function executed" << endl;
}

BOOST_AUTO_TEST_CASE(DISC_COUNT_NUCLEOTIDES)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;

   CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
   CRef <CScope> scope(new CScope(*objmgr));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

   CRef <CRepConfig>
     config (CRepConfig::factory("Discrepancy", &seh));

   config->SetArg("d", "All");
   config->SetArg("e", "DISC_COUNT_NUCLEOTIDES");
   config->CollectTests();
   config->Run();
   CRef <CClickableItem> c_item(0);
   CDiscRepOutput output_obj;
   output_obj.Export(c_item);

   NCBITEST_CHECK_MESSAGE(!c_item.Empty(),
                     "DISC_COUNT_NUCLEOTIDES did not find any nucleotide");
   NCBITEST_CHECK_MESSAGE(c_item->item_list.size() == c_item->obj_list.size(),
                          "the sizes of item_list and obj_list are not equal");
   string msg = "1 nucleotide Bioseq is present.";
   NCBITEST_CHECK_MESSAGE(c_item->description == msg, 
                         "Test result is incorrect: " + c_item->description);

};


END_NCBI_SCOPE
