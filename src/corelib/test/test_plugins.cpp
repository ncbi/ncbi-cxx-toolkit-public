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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *   Plugin manager test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbireg.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE


struct ITest
{
    int  a;
    ITest() : a(10) {}
    virtual ~ITest() {}

    virtual int GetA() const = 0;
};

struct ITest2
{
    int  b;
    ITest2() : b(0) {}
    virtual ~ITest2() {}
};

class CTestDriver : public ITest
{
public:
    CTestDriver() {}
    virtual ~CTestDriver() {}

    int GetA() const { return a; }
};

class CTestDriver2 : public ITest
{
public:
    CTestDriver2() {}
    virtual ~CTestDriver2() {}

    int GetA() const { return 15; }
};


/*
template<> 
class CInterfaceVersion<ITest>
{ 
public: 
    enum { 
        eMajor      = 1, 
        eMinor      = 1,
        ePatchLevel = 0 
    };
    static const char* GetName() { return "itest"; } 
};
*/

NCBI_DECLARE_INTERFACE_VERSION(ITest,  "itest", 1, 1, 0);
NCBI_DECLARE_INTERFACE_VERSION(ITest2, "itest2", 1, 1, 0);


class CTestCF : public CSimpleClassFactoryImpl<ITest, CTestDriver>
{
public:
    typedef CSimpleClassFactoryImpl<ITest, CTestDriver> TParent;
public:
    CTestCF() : TParent("test_driver", 0)
    {
    }
    ~CTestCF()
    {
    }
};

class CTestCF2 : public CSimpleClassFactoryImpl<ITest, CTestDriver2>
{
public:
    typedef CSimpleClassFactoryImpl<ITest, CTestDriver2> TParent;
public:
    CTestCF2() : TParent("test_driver2", 0)
    {
    }
    ~CTestCF2()
    {
    }
};


//extern "C" {

void NCBI_TestEntryPoint(CPluginManager<ITest>::TDriverInfoList&   info_list,
                         CPluginManager<ITest>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CTestCF>::NCBI_EntryPointImpl(info_list, method);
}

void NCBI_Test2EntryPoint(CPluginManager<ITest>::TDriverInfoList&   info_list,
                         CPluginManager<ITest>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CTestCF2>::NCBI_EntryPointImpl(info_list, method);
}

//}  // extern "C"

END_NCBI_SCOPE


USING_NCBI_SCOPE;


static void s_TEST_PluginManager(void)
{
    CPluginManager<ITest> pm;
    CPluginManager<ITest>::FNCBI_EntryPoint ep = NCBI_TestEntryPoint;

    pm.RegisterWithEntryPoint(ep);

    try {
        ITest* itest = pm.CreateInstance();

        int a = itest->GetA();

        assert(a == 10);

        delete itest;


    } 
    catch (CPluginManagerException& ex)
    {
        cout << "Cannot create ITest instance. " << ex.what() << endl;
        assert(0);
    }

    // Try to find not yet registered driver
    try {
        ITest* itest = pm.CreateInstance("test_driver2");
        assert(0);
        itest->GetA();
    } 
    catch (CPluginManagerException&)
    {
    }

    ep = NCBI_Test2EntryPoint;
    pm.RegisterWithEntryPoint(ep);

    try {
        ITest* itest2 = pm.CreateInstance("test_driver2");
        ITest* itest  = pm.CreateInstance("test_driver");

        int a = itest2->GetA();
        assert(a == 15);

        a = itest->GetA();
        assert(a == 10);


        delete itest2;
        delete itest;
    } 
    catch (CPluginManagerException& ex)
    {
        cout << "Cannot create ITest instance. " << ex.what() << endl;
        assert(0);
    }
}

/// @internal
class CParamTreePrintFunc
{
public:
    CParamTreePrintFunc()
     : m_Level(0)
    {}

    ETreeTraverseCode 
        operator()(const TPluginManagerParamTree& tr, int delta) 
    {
        m_Level += delta;
        if (delta >= 0) { 
            PrintLevelMargin();

            // const TPluginManagerParamTree::TParent* pt = tr.GetParent();

            const string& node_id = tr.GetValue().id;
            const string& node_v = tr.GetValue().value;

            NcbiCout << node_id << (node_v.empty() ? "":" = ") << node_v 
                     //<< " (" << (pt ? pt->GetValue().id : "") << ")"
                     << NcbiEndl;
        }

        return eTreeTraverse;
    }

    void PrintLevelMargin()
    {
        for (int i = 0; i < m_Level; ++i) {
            NcbiCout << "  ";
        }
    }
private:
    int            m_Level;
};


static
void s_CreateTestRegistry1(CNcbiRegistry& reg)
{
    reg.Clear();

    reg.Set("PARENT", ".SubNode",  "Section2, Section1");
    reg.Set("PARENT", "p1",  "1");
    reg.Set("PARENT", "p2",  "blah");

//    reg.Set("Section1", ".NodeName", "XXX");
    reg.Set("Section1", ".SubNode",  "Section2");
    reg.Set("Section1", "s1A",  "ugh");
    reg.Set("Section1", "s1B",  "33");

    reg.Set("Section2", ".NodeName", "AAA");
    reg.Set("Section2", "s2A",  "eee");
    reg.Set("Section2", "s2B",  "boo");

    reg.Set("XXX", "s1A",  "duh");
    reg.Set("XXX", "s1B",  "777");

#if 1
    reg.Set("p", "p1",  "1");
    reg.Set("p", "a",  "a1");
    reg.Set("p/a", "pa1",  "1");
    reg.Set("p", "p2",  "2");
    reg.Set("p/a/b", "pab1",  "1");
    reg.Set("p/a/b", ".SubNode",  "Section2");
#endif
}

static
void s_CreateTestRegistry2(CNcbiRegistry& reg)
{
    reg.Clear();
#if 0
    reg.Set("PARENT", "p1",  "1");
    reg.Set("PARENT", "p2",  "blah");

    reg.Set("PARENT/AAA", "s2A",  "eee");
    reg.Set("PARENT/AAA", "s2B",  "boo");

    reg.Set("PARENT/Section1", "s1A",  "ugh");
    reg.Set("PARENT/Section1", "s1B",  "33");

    reg.Set("PARENT/Section1/AAA", "s2A",  "eee");
    reg.Set("PARENT/Section1/AAA", "s2B",  "boo");

    reg.Set("XXX", "s1A",  "duh");
    reg.Set("XXX", "s1B",  "777");
#else
    reg.Set("XXX", "s1A",  "duh");
    reg.Set("PARENT", "p1",  "1");
    reg.Set("PARENT/Section1/AAA", "s2B",  "boo");
    reg.Set("PARENT/Section1", "s1B",  "33");
    reg.Set("PARENT/AAA", "s2A",  "eee");
    reg.Set("PARENT/Section1", "s1A",  "ugh");
    reg.Set("PARENT/Section1/AAA", "s2A",  "eee");
    reg.Set("XXX", "s1B",  "777");
    reg.Set("PARENT", "p2",  "blah");
    reg.Set("PARENT/AAA", "s2B",  "boo");
#endif
}

static
void s_TestParamTree(TPluginManagerParamTree* tr)
{
    const TPluginManagerParamTree* nd = tr->FindNode("PARENT");
    _ASSERT(nd);    
    _ASSERT(nd->GetKey() == "PARENT");

    nd = nd->FindNode("Section1");
    _ASSERT(nd);    
    _ASSERT(nd->GetKey() == "Section1");

    nd = nd->FindNode("AAA");
    _ASSERT(nd);    
    _ASSERT(nd->GetKey() == "AAA");

    nd = nd->FindNode("PARENT");
    _ASSERT(nd);    
    _ASSERT(nd->GetKey() == "PARENT");
}

static
void s_TestRegConvert()
{
    CNcbiRegistry reg;
    CParamTreePrintFunc func;
    TPluginManagerParamTree *tr, *ptr;
    

    s_CreateTestRegistry1( reg );

    tr = CConfig::ConvertRegToTree(reg);
    ptr = tr;
    TreeDepthFirstTraverse(*ptr, func);
    s_TestParamTree( tr );
    delete tr;


    s_CreateTestRegistry2( reg );

    tr = CConfig::ConvertRegToTree(reg);
    ptr = tr;
    TreeDepthFirstTraverse(*ptr, func);
    s_TestParamTree( tr );
    delete tr;
}



////////////////////////////////
/// Test application
///
/// @internal
///
class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_plugin", "DLL accessory class");
    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    cout << "Run test" << endl << endl;

    s_TEST_PluginManager();

    s_TestRegConvert();

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv);
}
