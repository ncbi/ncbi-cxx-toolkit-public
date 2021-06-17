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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/obj_store.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <common/test_assert.h>  /* This header must go last */

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


/// @internal
class CTestObject : public CObject
{
public: 
    CTestObject(int v) : m_V(v) {}
    ~CTestObject() { NcbiCout << m_V << NcbiEndl; }
protected:
    int   m_V;
};

static void TestCReverseDataStore(void)
{
    CReverseObjectStore<string, CTestObject> rds;
    rds.PutObject("obj1", new CTestObject(1));
    rds.PutObject("obj2", new CTestObject(2));

    bool has = rds.HasObject("obj1");
    assert(has);
    has = rds.HasObject("obj2");
    assert(has);
    has = rds.HasObject("obj20");
    assert(!has);

    rds.Clear();

    has = rds.HasObject("obj2");
    assert(!has);

    rds.PutObject("obj1", new CTestObject(1));
    has = rds.HasObject("obj1");
    assert(has);

    rds.ReleaseObject("obj1");

    has = rds.HasObject("obj1");
    assert(!has);

    has = rds.HasObject((CObject*)0);
    assert(!has);
}

BEGIN_NCBI_SCOPE

/// @internal
struct ITest
{
    int  a;
    ITest() : a(10) {}

    virtual int GetA() const = 0;
};

NCBI_DECLARE_INTERFACE_VERSION(ITest,  "itest", 1, 1, 0);

END_NCBI_SCOPE

void TestPluginManagerStore()
{
//    string pm_name = CInterfaceVersion<ITest>::GetName();
    
    CPluginManager<ITest>* pm = ncbi::CPluginManagerGetter<ITest>::Get();
    assert(pm);

}


/////////////////////////////////
/// Test application
///
/// @internal
class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestCReverseDataStore();
    TestPluginManagerStore();

    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
    assert( IsDiagStream(0) );
    assert( !IsDiagStream(&NcbiCout) );
}


int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
