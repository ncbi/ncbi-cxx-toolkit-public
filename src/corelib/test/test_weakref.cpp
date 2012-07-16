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

#include <common/test_assert.h>  /* This header must go last */


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
    CRef<CTestObject> ref2;

    // These blocks are for the sake of conformance with C++ standard:
    // temporary object returned from Lock() is not mandatory to be destroyed
    // at the end of the statement, but at the end of the block. If we
    // remove these blocks test can fail on some compilers, e.g. WorkShop 5.5.
    {{
        ref2 = weak.Lock();
        BOOST_CHECK(ref2.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    {{
        ref.Reset();
        ref = weak.Lock();
        BOOST_CHECK(ref.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}


BOOST_AUTO_TEST_CASE(IRefMainUsage)
{
    CIRef<CTestInterface> ref2;
    CIRef<CTestInterface> ref(new CTestObject());
    CWeakIRef<CTestInterface> weak(ref);

    // These blocks are for the sake of conformance with C++ standard:
    // temporary object returned from Lock() is not mandatory to be destroyed
    // at the end of the statement, but at the end of the block. If we
    // remove these blocks test can fail on some compilers, e.g. WorkShop 5.5.
    {{
        ref2 = weak.Lock();
        BOOST_CHECK(ref2.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    {{
        ref.Reset();
        ref = weak.Lock();
        BOOST_CHECK(ref.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}



class CWeakReferencable : public CObject,
                          public CWeakObject
{
};

BOOST_AUTO_TEST_CASE(CWeakObjectBased)
{
    CRef<CWeakReferencable>      ref(new CWeakReferencable());
    CWeakRef<CWeakReferencable>  weak(ref);
    CRef<CWeakReferencable>      ref2;

    // These blocks are for the sake of conformance with C++ standard:
    // temporary object returned from Lock() is not mandatory to be destroyed
    // at the end of the statement, but at the end of the block. If we
    // remove these blocks test can fail on some compilers, e.g. WorkShop 5.5.
    {{
        ref2 = weak.Lock();
        BOOST_CHECK(ref2.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    {{
        ref.Reset();
        ref = weak.Lock();
        BOOST_CHECK(ref.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}


// The test below will work for non debug configurations only.
// In case of debug the CPtrToObjectProxy::ReportIncompatibleType()
// method calls the ERR_POST_X(...) macro with Fatal type of error so
// abort() will be called.
// The non-debug version of the CPtrToObjectProxy::ReportIncompatibleType()
// will throw an exception which could be caught.
#ifndef _DEBUG
// The class does not derive from CObject so it cannot be used
// for weak referencing
class CNotWeakReferencable : public CWeakObject
{
};

BOOST_AUTO_TEST_CASE(CWeakNotReferencable)
{
    CNotWeakReferencable* ptr = new CNotWeakReferencable();
    BOOST_CHECK_THROW( CWeakRef<CNotWeakReferencable> weak(ptr),
                       CCoreException );
    delete ptr;
}
#endif



BOOST_AUTO_TEST_CASE(CWeakNotCRefControllable)
{
    CWeakReferencable* ptr = new CWeakReferencable();
    // The new object is not controlled by CRef
    // so an exception is expected.
    BOOST_CHECK_THROW( CWeakRef<CWeakReferencable> weak(ptr),
                       CObjectException );
    delete ptr;
}


BOOST_AUTO_TEST_CASE(WeakReferencableOnStack)
{
    CWeakReferencable            stackObject;
    CRef<CWeakReferencable>      ref(&stackObject);
    CWeakRef<CWeakReferencable>  weak(ref);
    CRef<CWeakReferencable>      ref2;

    // These blocks are for the sake of conformance with C++ standard:
    // temporary object returned from Lock() is not mandatory to be destroyed
    // at the end of the statement, but at the end of the block. If we
    // remove these blocks test can fail on some compilers, e.g. WorkShop 5.5.
    {{
        ref2 = weak.Lock();
        BOOST_CHECK(ref2.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    {{
        ref.Reset();
        ref = weak.Lock();
        BOOST_CHECK(ref.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}


static CWeakReferencable    staticMemoryObject;

BOOST_AUTO_TEST_CASE(WeakReferencableInStaticMemory)
{
    CRef<CWeakReferencable>      ref(&staticMemoryObject);
    CWeakRef<CWeakReferencable>  weak(ref);
    CRef<CWeakReferencable>      ref2;

    // These blocks are for the sake of conformance with C++ standard:
    // temporary object returned from Lock() is not mandatory to be destroyed
    // at the end of the statement, but at the end of the block. If we
    // remove these blocks test can fail on some compilers, e.g. WorkShop 5.5.
    {{
        ref2 = weak.Lock();
        BOOST_CHECK(ref2.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    {{
        ref.Reset();
        ref = weak.Lock();
        BOOST_CHECK(ref.NotNull());
        BOOST_CHECK_EQUAL(ref, ref2);
    }}

    ref.Reset();
    ref2.Reset();
    ref2 = weak.Lock();
    BOOST_CHECK(ref2.IsNull());
}

