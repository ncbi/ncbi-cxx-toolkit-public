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
 * Author:  Eugene Vasilchenko, NCBI
 *
 * File Description:
 *   Unit test for CUser_object and CUser_field
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

//#include <objects/general/general__.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <util/util_exception.hpp>

#include <objects/general/Dbtag.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(s_TestDBTag1)
{
    CDbtag::EDbtagType tag;

    CDbtag dbtag1;
    dbtag1.SetDb("AAA");
    tag = dbtag1.GetType();
    BOOST_CHECK_EQUAL(tag, CDbtag::eDbtagType_bad);

    CDbtag dbtag2;
    dbtag2.SetDb("BioSample");
    tag = dbtag2.GetType();
    BOOST_CHECK_EQUAL(tag, CDbtag::eDbtagType_BioSample);

    CDbtag dbtag3;
    dbtag3.SetDb("BioSamplE"); // wrong case
    tag = dbtag3.GetType();
    BOOST_CHECK_EQUAL(tag, CDbtag::eDbtagType_bad);

    BOOST_CHECK(!dbtag2.IsApproved());
    BOOST_CHECK(dbtag2.IsApproved(CDbtag::eIsRefseq_Yes));
    BOOST_CHECK(!dbtag3.IsApproved());
    BOOST_CHECK(dbtag3.IsApprovedNoCase(CDbtag::eIsRefseq_Yes));

    string str;
    CDbtag::TDbtagGroup group;

    group = dbtag2.GetDBFlags(str);
    BOOST_CHECK_EQUAL(str, "BioSample");
    BOOST_CHECK_EQUAL(group, CDbtag::fRefSeq);

    group = dbtag3.GetDBFlags(str);
    BOOST_CHECK_EQUAL(str, "BioSample");
    BOOST_CHECK_EQUAL(group, CDbtag::fRefSeq);
}
