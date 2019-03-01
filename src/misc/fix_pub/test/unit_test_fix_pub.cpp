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
 * Author:  Alexey Dobronadezhdin
 *
 * File Description:
 *   Unit tests for CPubFixing.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/ncbi_message.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/medline/Medline_entry.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>


#include "../fix_pub_aux.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(Test_IsFromBook)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), false);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), false);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), true);
}

BOOST_AUTO_TEST_CASE(Test_IsInpress)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook().SetImp();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);

    art.SetFrom().SetProc();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetProc().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);

    art.SetFrom().SetJournal();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);
}

BOOST_AUTO_TEST_CASE(Test_NeedToPropagateInJournal)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetJournal();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    CRef<CTitle::C_E> title(new CTitle::C_E);
    title->SetName("journal");
    art.SetFrom().SetJournal().SetTitle().Set().push_back(title);
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetJournal().SetImp().SetVolume("1");
    art.SetFrom().SetJournal().SetImp().SetPages("2");

    art.SetFrom().SetJournal().SetImp().SetDate().SetStd();

    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), false);

    art.SetFrom().SetJournal().ResetTitle();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);
}
