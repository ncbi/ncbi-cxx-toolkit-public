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
* Author:  Aaron Ucko
*
* File Description:
*   Simple unit test for CProt_ref::GetECNumber{Status,Replacement}.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seqfeat/Prot_ref.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <util/util_misc.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_AUTO_INIT()
{
    // force use of built-in tables
    g_IgnoreDataFile("ecnum_*.txt");
}

BOOST_AUTO_TEST_CASE(s_TestGetStatus)
{
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("1.1.1.1"),
                      CProt_ref::eEC_specific);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("2.7.3.6"),
                      CProt_ref::eEC_specific);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("6.6.1.2"),
                      CProt_ref::eEC_specific);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("1.-.-.-"),
                      CProt_ref::eEC_ambiguous);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("3.4.-.-"),
                      CProt_ref::eEC_ambiguous);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("3.4.N.n"),
                      CProt_ref::eEC_ambiguous);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("3.4.n.N"),
                      CProt_ref::eEC_ambiguous);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("6.n.n.n"),
                      CProt_ref::eEC_ambiguous);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("1.1.1.5"),
                      CProt_ref::eEC_replaced);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("4.2.2.4"),
                      CProt_ref::eEC_replaced);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("6.3.5.8"),
                      CProt_ref::eEC_replaced);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("1.1.1.74"),
                      CProt_ref::eEC_deleted);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("5.4.3.1"),
                      CProt_ref::eEC_deleted);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("6.3.2.15"),
                      CProt_ref::eEC_replaced);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("asdf"),
                      CProt_ref::eEC_unknown);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("3.4.n.-"),
                      CProt_ref::eEC_unknown);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberStatus("9.8.7.6"),
                      CProt_ref::eEC_unknown);
}

BOOST_AUTO_TEST_CASE(s_TestGetReplacement)
{
    BOOST_CHECK_THROW(CProt_ref::GetECNumberReplacement("2.7.3.6"),
                      CCoreException);
    BOOST_CHECK_THROW(CProt_ref::GetECNumberReplacement("3.4.-.-"),
                      CCoreException);
    BOOST_CHECK_EQUAL(CProt_ref::GetECNumberReplacement("4.2.1.16"),
                      string("4.3.1.19"));
    BOOST_CHECK_THROW(CProt_ref::GetECNumberReplacement("5.4.3.1"),
                      CCoreException);
    BOOST_CHECK_THROW(CProt_ref::GetECNumberReplacement("9.8.7.6"),
                      CCoreException);
}
