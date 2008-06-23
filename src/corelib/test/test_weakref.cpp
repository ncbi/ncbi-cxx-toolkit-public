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
* Author:  Pavel Ivanov
*
* File Description:
*   Unit test for CObjectEx, CWeakRef<> and CWeakIRef<> classes
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


class CTestInterface
{
public:
    virtual ~CTestInterface() {}
};

class CTestObject : public CObjectEx, public CTestInterface
{
};


BOOST_AUTO_TEST_CASE(RefMainUsage)
{
    CRef<CTestObject> ref(new CTestObject());
    CWeakRef<CTestObject> weak(ref);

    CRef<CTestObject> ref2(weak.Lock());
    BOOST_CHECK(ref2.NotNull());
    BOOST_CHECK_EQUAL(ref, ref2);

    ref.Reset();
    ref = weak.Lock().GetPointer();
    BOOST_CHECK(ref.NotNull());
    BOOST_CHECK_EQUAL(ref, ref2);

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}

BOOST_AUTO_TEST_CASE(IRefMainUsage)
{
    CIRef<CTestInterface> ref(new CTestObject());
    CWeakIRef<CTestInterface> weak(ref);

    CIRef<CTestInterface> ref2(weak.Lock());
    BOOST_CHECK(ref2.NotNull());
    BOOST_CHECK_EQUAL(ref, ref2);

    ref.Reset();
    ref = weak.Lock().GetPointer();
    BOOST_CHECK(ref.NotNull());
    BOOST_CHECK_EQUAL(ref, ref2);

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}
