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
* Author:  Thomas W. Rackers, NCBI/NLM/NIH [C]
*
* File Description:
*   Criteria class unit tests file.
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objtools/blast/seqdb_writer/impl/criteria.hpp>


USING_NCBI_SCOPE;

USING_SCOPE(objects);


NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default "
         "in order to avoid unwanted failure during the daily automated "
         "builds.");
}



NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
}


BOOST_AUTO_TEST_SUITE(criteria)


BOOST_AUTO_TEST_CASE(Test_Criteria_Subclasses)
{
    {
        CCriteria_EST_HUMAN crit;
        BOOST_CHECK(crit.GetLabel() == string("est_human"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eUNASSIGNED);

        SDIRecord sdiRecord;
        sdiRecord.taxid = 9606;                     // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 9605;                     // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
    }

    {
        CCriteria_EST_MOUSE crit;
        BOOST_CHECK(crit.GetLabel() == string("est_mouse"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eUNASSIGNED);

        SDIRecord sdiRecord;
        sdiRecord.taxid = 10090;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 10091;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 10092;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 35531;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 80274;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 57486;                    // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 4000;                     // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
    }

    {
        CCriteria_EST_OTHERS crit;
        BOOST_CHECK(crit.GetLabel() == string("est_others"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eUNASSIGNED);

        SDIRecord sdiRecord;
        sdiRecord.taxid = 4000;                     // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.taxid = 9606;                     // wrong, is EST_HUMAN
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.taxid = 10090;                    // wrong, is EST_MOUSE
        BOOST_CHECK(crit.is(&sdiRecord) == false);
    }

    {
        CCriteria_SWISSPROT crit;
        BOOST_CHECK(crit.GetLabel() == string("swissprot"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eSWISSPROT);

        SDIRecord sdiRecord;
        sdiRecord.owner = 6;                        // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.owner = 7;                        // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
    }

    {
        CCriteria_PDB crit;
        BOOST_CHECK(crit.GetLabel() == string("pdb"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::ePDB);

        SDIRecord sdiRecord;
        sdiRecord.owner = 10;                       // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.owner = 11;                       // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
    }

    {
        CCriteria_REFSEQ crit;
        BOOST_CHECK(crit.GetLabel() == string("refseq"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eREFSEQ);

        SDIRecord sdiRecord;
        sdiRecord.acc = "?B_123456";                // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "A?_123456";                // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "AB?123456";                // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "AB_12345";                 // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "AB_123456";                // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
        sdiRecord.acc = "AB_1234567";               // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
    }

    {
        CCriteria_REFSEQ_RNA crit;
        BOOST_CHECK(crit.GetLabel() == string("refseq_rna"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eREFSEQ);

        SDIRecord sdiRecord;
        sdiRecord.mol = 2;                          // ok
        sdiRecord.acc = "?";                        // wrong
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.mol = 1;                          // wrong
        sdiRecord.acc = "AB_123456";                // ok
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.mol = 2;                          // ok
        sdiRecord.acc = "AB_123456";                // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
    }

    {
        CCriteria_REFSEQ_GENOMIC crit;
        BOOST_CHECK(crit.GetLabel() == string("refseq_genomic"));
        BOOST_CHECK(crit.GetMembershipBit() == ICriteria::eREFSEQ);

        SDIRecord sdiRecord;
        sdiRecord.acc = "AB_123456";                // ok
        sdiRecord.mol = 2;                          // wrong, is REFSEQ_RNA
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "?";                        // wrong, not REFSEQ
        sdiRecord.mol = 1;                          // ok
        BOOST_CHECK(crit.is(&sdiRecord) == false);
        sdiRecord.acc = "AB_123456";                // ok
        sdiRecord.mol = 1;                          // ok
        BOOST_CHECK(crit.is(&sdiRecord) == true);
    }

}


BOOST_AUTO_TEST_CASE(Test_CCriteriaSet)
{
    CCriteriaSet critSet;

    // Verify set is initially empty.
    BOOST_CHECK(critSet.GetCriteriaCount() == 0);

    // Try to add criteria with bogus labels.
    const string bogus_string = "bogus_string";
    const char*  bogus_chars  = "bogus_chars";
    BOOST_CHECK(critSet.AddCriteria(bogus_string) == false);
    BOOST_CHECK(critSet.AddCriteria(bogus_chars)  == false);

    // Should still be empty.
    BOOST_CHECK(critSet.GetCriteriaCount() == 0);

    // Try adding a valid one twice, should succeed only first time.
    // Mix up the case just to make sure it still works the first time.
    const string legit_string_camel_case = "SwissProt";
    BOOST_CHECK(critSet.AddCriteria(legit_string_camel_case) == true);
    BOOST_CHECK(critSet.AddCriteria(legit_string_camel_case) == false);

    // Should have one entry now.
    BOOST_CHECK(critSet.GetCriteriaCount() == 1);

    // Search for a legit one that has NOT been added yet.
    // Both searches should fail.
    const string legit_string_upper_case = "REFSEQ";
    const string legit_string_lower_case = "refseq";
    BOOST_CHECK(critSet.FindCriteria(legit_string_upper_case) == NULL);
    BOOST_CHECK(critSet.FindCriteria(legit_string_lower_case) == NULL);

    // Now add it using upper case and search for it using lower case.
    // Search should succeed.
    BOOST_CHECK(critSet.AddCriteria(legit_string_upper_case) == true);
    BOOST_CHECK(critSet.FindCriteria(legit_string_lower_case) != NULL);

    // Should have two entries now.
    BOOST_CHECK(critSet.GetCriteriaCount() == 2);

    // Try to fetch and access the actual container.
    const TCriteriaMap* pContainer_const = &(critSet.GetCriteriaMap());

    // Verify previous step did not return NULL.
    BOOST_CHECK(pContainer_const != NULL);

    // Verify container also has two entries.
    BOOST_CHECK(pContainer_const->size() == 2);

    // Empty the container manually, then verify criteria set is also empty.
    // (Ordinarily, one shouldn't be doing a const_cast like this.
    // That's why GetCriteriaMap returns a const object, to keep you from
    // messing about with it like this.  But I wrote it, so I'm allowed. ^_^ )
    TCriteriaMap* pContainer_non_const =
            const_cast<TCriteriaMap*>(pContainer_const);
    pContainer_non_const->clear();
    BOOST_CHECK(critSet.GetCriteriaCount() == 0);
}


BOOST_AUTO_TEST_CASE(Test_CalculateMemberships_Function)
{
    // Remember, membership bit numbers are 1-indexed and so must be
    // reduced by 1 before being used as bit-shift values.

    {
        // Create a DI record that will set NO membership bits.
        // That means it cannot match any of the following:
        // - swissprot          owner == 6
        // - pdb                owner == 10
        // - refseq_genomic     is refseq and is not refseq_rna,
        //                      equiv to (is refseq and mol != 2)
        // - refseq_rna         is refseq and mol == 2
        // Cannot be 'swissprot' and 'pdb' at same time.
        // Cannot be 'refseq_genomic' and 'refseq_rna' at same time.
        SDIRecord sdiRecord;
        sdiRecord.owner = 4;
        sdiRecord.acc = "?";
        CBlast_def_line::TMemberships memberships =
                CCriteriaSet_CalculateMemberships(sdiRecord);
        BOOST_CHECK(memberships.size() == 0);
    }

    {
        // Create a DI record that will set SWISSPROT and REFSEQ
        // membership bits.
        SDIRecord sdiRecord;
        sdiRecord.owner = 6;            // is SWISSPROT
        sdiRecord.acc = "CC_456789";    // is REFSEQ
        sdiRecord.mol = 1;              // is REFSEQ_GENOMIC
        CBlast_def_line::TMemberships memberships =
                CCriteriaSet_CalculateMemberships(sdiRecord);
        BOOST_CHECK(memberships.size() == 1);
        BOOST_CHECK(
                (memberships.front() & (0x1 << (ICriteria::eSWISSPROT - 1)) &&
                (memberships.front() & (0x1 << (ICriteria::eREFSEQ - 1))))
        );
        
        // even though a membership bit is not set, this SDIRecord should still
        // be identified as refseq_genomic
        CCriteria_REFSEQ_GENOMIC criteria;
        BOOST_CHECK(criteria.is(&sdiRecord));
    }

    {
        // Create a DI record that will set PDB and REFSEQ
        // membership bits.
        SDIRecord sdiRecord;
        sdiRecord.owner = 10;           // is PDB
        sdiRecord.acc = "CC_456789";    // is REFSEQ
        sdiRecord.mol = 2;              // is REFSEQ_RNA
        CBlast_def_line::TMemberships memberships =
                CCriteriaSet_CalculateMemberships(sdiRecord);
        BOOST_CHECK(memberships.size() == 1);
        BOOST_CHECK(
                (memberships.front() & (0x1 << (ICriteria::ePDB - 1)) &&
                (memberships.front() & (0x1 << (ICriteria::eREFSEQ - 1))))
        );

        // even though a membership bit is not set, this SDIRecord should still
        // be identified as refseq_rna
        CCriteria_REFSEQ_RNA criteria;
        BOOST_CHECK(criteria.is(&sdiRecord));
    }
}

BOOST_AUTO_TEST_SUITE_END()
