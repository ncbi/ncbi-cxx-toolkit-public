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
* Author:  Greg Boratyn
*
* File Description:
*   Unit tests for CMultiAlignerOptions
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <objects/blast/Blast4_archive.hpp>
#include <algo/cobalt/cobalt.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

BOOST_AUTO_TEST_SUITE(options)

BOOST_AUTO_TEST_CASE(TestOptionsModes)
{
    // Default constructor results in standard mode
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    BOOST_CHECK(opts->IsStandardMode());
    BOOST_CHECK(opts->GetMode() & CMultiAlignerOptions::fNoRpsBlast);

    // Setting RPS data base preserves standard mode
    opts->SetRpsDb("RPSDB");
    BOOST_CHECK(opts->IsStandardMode());
    BOOST_CHECK(!(opts->GetMode() & CMultiAlignerOptions::fNoRpsBlast));

    // Changing any other setting results in non-standard mode
    opts->SetUseQueryClusters(false);
    BOOST_CHECK(!opts->IsStandardMode());
    BOOST_CHECK(opts->GetMode() & CMultiAlignerOptions::fNonStandard);

    // Setting user constraints results in non-standard mode
    opts.Reset(new CMultiAlignerOptions());
    opts->SetUserConstraints();
    BOOST_CHECK(!opts->IsStandardMode());
    BOOST_CHECK(opts->GetMode() & CMultiAlignerOptions::fNonStandard);

    // Constructor with non-standard mode throws
    BOOST_CHECK_THROW(opts.Reset(
                new CMultiAlignerOptions(CMultiAlignerOptions::fNonStandard)),
                CMultiAlignerException);
}


BOOST_AUTO_TEST_CASE(TestPatterns)
{
    // Default constructor assigns default patterns
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    BOOST_CHECK(opts->GetCddPatterns().size());

    // eNoPatterns mode results in no patterns assigned
    opts.Reset(new CMultiAlignerOptions(CMultiAlignerOptions::kDefaultMode
                                 | CMultiAlignerOptions::fNoPatterns));
    BOOST_CHECK(opts->IsStandardMode());
    BOOST_CHECK(!opts->GetCddPatterns().size());
}


BOOST_AUTO_TEST_CASE(TestOptionsValidation)
{
    // Default options validate and validation does not throw
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    BOOST_CHECK_NO_THROW(opts->Validate());
    BOOST_CHECK(opts->Validate());

    // Selecting mode with RPS search and not specifying RPS database
    // does not pass validation
    opts.Reset(new CMultiAlignerOptions(CMultiAlignerOptions::kDefaultMode));
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);
    opts->SetRpsDb("RPSDB");

    // Negative RPS e-value does not pass validation if RPS Blast is selected
    opts->SetRpsEvalue(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);
    opts->SetRpsEvalue(0.1);

    // Negative domain hit list size does not pass validation if RPS-BLAST is
    // selected
    opts->SetDomainHitlistSize(-1);
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);
    opts->SetDomainHitlistSize(250);

    // Negative Blastp e-value does not pass validation
    opts->SetBlastpEvalue(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);
    opts->SetBlastpEvalue(0.1);

    // Negative range in user constraint does not pass validation
    vector<CMultiAlignerOptions::SConstraint>& constr
        = opts->SetUserConstraints();
    constr.resize(1);
    constr[0].seq1_start = 1;
    constr[0].seq1_stop = 5;
    constr[0].seq2_start = 3;
    constr[0].seq2_stop = 1;
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);

    // Empty patterns do not pass validation
    opts.Reset(new CMultiAlignerOptions());
    vector<CMultiAlignerOptions::CPattern>& patterns = opts->SetCddPatterns();
    patterns.clear();
    patterns.resize(2);
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);

    // Too large kmer length does not pass validation
    opts.Reset(new CMultiAlignerOptions());
    opts->SetKmerLength(10);
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);

    // Too small kmer length produces a warning
    opts->SetKmerLength(1);
    BOOST_CHECK(!opts->Validate());
    BOOST_CHECK(opts->GetMessages().size() > 0);

    // Make sure that all alhapeths are accepted
    opts.Reset(new CMultiAlignerOptions());
    opts->SetKmerAlphabet(CMultiAligner::TKMethods::eRegular);
    BOOST_CHECK(opts->Validate());

    opts->SetKmerAlphabet(CMultiAligner::TKMethods::eSE_V10);
    BOOST_CHECK(opts->Validate());

    opts->SetKmerAlphabet(CMultiAligner::TKMethods::eSE_B15);
    BOOST_CHECK(opts->Validate());

    // Check pre-computed domain hits
    CRef<objects::CBlast4_archive> archive(new objects::CBlast4_archive);
    CNcbiIfstream istr("data/rps_archive_seqloclist.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *archive;

    // verify that setting domain hits is valid
    opts->SetDomainHits(archive);
    opts->SetRpsDb("some_db");
    BOOST_CHECK(opts->Validate());

    // verify that setting domain hits with no CDD throws exception
    opts->SetRpsDb("");
    BOOST_CHECK_THROW(opts->Validate(), CMultiAlignerException);    
}

// verify the CMultiAlignerOptions::CanGetDomainHits() function
BOOST_AUTO_TEST_CASE(TestCanGetDomainHits)
{
    CRef<objects::CBlast4_archive> archive(new objects::CBlast4_archive);
    CNcbiIfstream istr("data/rps_archive_seqloclist.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *archive;

    CMultiAlignerOptions opts;
    
    // verify for domain hits not set
    BOOST_REQUIRE(opts.GetDomainHits().Empty());
    BOOST_REQUIRE(!opts.CanGetDomainHits());

    // verify for domain hits set
    opts.SetDomainHits(archive);
    BOOST_REQUIRE(!opts.GetDomainHits().Empty());
    BOOST_REQUIRE(opts.CanGetDomainHits());
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
