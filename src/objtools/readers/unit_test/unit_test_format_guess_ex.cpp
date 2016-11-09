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
* Author:  Nathan Bouk
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objtools/readers/format_guess_ex.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


///
/// Test a simple BED one-liner, that CFormatGuess will fail
///
BOOST_AUTO_TEST_CASE(Test_BED_OneLiner)
{
	char BEDLine[] = "chr1\t16759829\t16778548\tchr1:21667704\t270866\t-\n";
	CNcbiIstrstream Stream(BEDLine, strlen(BEDLine));

	CFormatGuessEx Guesser(Stream);
	CFormatGuess::EFormat Guess;
	Guess = Guesser.GuessFormat();


    BOOST_CHECK_EQUAL(Guess, CFormatGuess::eBed);
}

///
/// Test a simple GFF one-liner, that CFormatGuess will fail
///
BOOST_AUTO_TEST_CASE(Test_GFF_OneLiner)
{
	char GFFLine[] = "NC_000008.9\tdbVar\tmisc\t151699\t186841\t.\t.\t.\n";
	CNcbiIstrstream Stream(GFFLine, strlen(GFFLine));

	CFormatGuessEx Guesser(Stream);
	CFormatGuess::EFormat Guess;
	Guess = Guesser.GuessFormat();


    BOOST_CHECK_EQUAL(Guess, CFormatGuess::eGff2);
}


