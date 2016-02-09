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
* Author:  Andrea Asztalos
*
* File Description:
*   Unit tests for publication field edits
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/general/Name_std.hpp>
#include <objtools/edit/publication_edit.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

NCBITEST_AUTO_INIT()
{
}

NCBITEST_AUTO_FINI()
{
}


BOOST_AUTO_TEST_CASE(Test_GetFirstInitial)
{
    bool skip_rest = false;
    string initial = GetFirstInitial("Donald", skip_rest);
    BOOST_CHECK_EQUAL(initial, "D.");

    initial = GetFirstInitial(".Donald", skip_rest);
    BOOST_CHECK_EQUAL(initial, "D.");

    initial = GetFirstInitial("MJ", skip_rest);
    BOOST_CHECK_EQUAL(initial, "M.J.");

    initial = GetFirstInitial("MJ", true);
    BOOST_CHECK_EQUAL(initial, "M.");

    initial = GetFirstInitial("Mac-Alister", skip_rest);
    BOOST_CHECK_EQUAL(initial, "M.-A.");

    initial = GetFirstInitial("Mac-Alister", true);
    BOOST_CHECK_EQUAL(initial, "M.-A.");

    initial = GetFirstInitial("X.Gabriel", skip_rest);
    BOOST_CHECK_EQUAL(initial, "X.G.");

    initial = GetFirstInitial("X.Gabriel", true);
    BOOST_CHECK_EQUAL(initial, "X.G.");

    initial = GetFirstInitial("Moises Thiago Souza", skip_rest);
    BOOST_CHECK_EQUAL(initial, "M.T.S.");

    initial = GetFirstInitial(kEmptyStr, skip_rest);
    BOOST_CHECK_EQUAL(initial, kEmptyStr);
}

BOOST_AUTO_TEST_CASE(Test_FixInitials)
{
    CName_std stdname;
    stdname.SetLast("Freitas");
    BOOST_CHECK_EQUAL(FixInitials(stdname), false);

    stdname.SetFirst("Thiago");
    BOOST_CHECK_EQUAL(FixInitials(stdname), false);
    // no initials are fixed until that member has a value

    stdname.SetInitials("Moises");
    BOOST_CHECK_EQUAL(FixInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "T.M.");

    stdname.SetInitials("Moises Souza");
    BOOST_CHECK_EQUAL(FixInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "T.M.S.");
}

BOOST_AUTO_TEST_CASE(Test_GenerateInitials)
{
    CName_std stdname;
    stdname.SetLast("Freitas");
    BOOST_CHECK_EQUAL(GenerateInitials(stdname), false);
    BOOST_CHECK(!stdname.IsSetInitials());

    stdname.SetFirst("Thiago");
    BOOST_CHECK_EQUAL(GenerateInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "T.");

    stdname.ResetInitials();
    stdname.SetFirst("MJ Gabriel");
    BOOST_CHECK_EQUAL(GenerateInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "M.G."); // and not M.J.G.

    stdname.SetFirst("Albert");
    BOOST_CHECK_EQUAL(GenerateInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "A.M.G."); // and not A.M.J.G.
}

BOOST_AUTO_TEST_CASE(Test_MoveMiddleToFirst)
{
    CName_std stdname;
    stdname.SetLast("Freitas");
    BOOST_CHECK_EQUAL(MoveMiddleToFirst(stdname), false);

    stdname.SetFirst("Thiago");
    BOOST_CHECK_EQUAL(MoveMiddleToFirst(stdname), false);

    stdname.SetInitials("Souza");
    BOOST_CHECK_EQUAL(MoveMiddleToFirst(stdname), false);
    BOOST_CHECK_EQUAL(stdname.GetFirst(), "Thiago");
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "Souza");
    // nothing happens as the initials field does not contain a dot

    stdname.SetFirst("Moises");
    stdname.SetInitials("M.Souza");
    BOOST_CHECK_EQUAL(MoveMiddleToFirst(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetFirst(), "Moises Souza");
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "M.Souza");  // it requires fixing
    BOOST_CHECK_EQUAL(FixInitials(stdname), true);
    BOOST_CHECK_EQUAL(stdname.GetInitials(), "M.S.");


    stdname.SetFirst("Wei Ning");
    stdname.SetInitials("W.N.");
    BOOST_CHECK_EQUAL(MoveMiddleToFirst(stdname), false);
    // nothing needs to be moved
}