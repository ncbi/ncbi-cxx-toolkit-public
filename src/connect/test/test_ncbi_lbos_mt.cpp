/* $Id$
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
 * Author:  Dmitriy Elisov
 *
 * File Description:
 *   Standard test for named service resolution facility. Single-threaded.
 *
 */

/*delete*/#define            NCBI_USE_ERRCODE_X         Connect_LBSM

#include <ncbi_pch.hpp>
#include "test_ncbi_lbos_common.hpp"
#include <corelib/test_mt.hpp>

USING_NCBI_SCOPE;

/**  Main class of this program which will be used for testing. Based on the
 *  class from test_mt                                                       */
class CTestLBOSApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    typedef list<string> TStringList;
    void SwapAddressesTest(int idx);
    void x_TestOldFormat(TStringList& messages);
    void x_TestNewFormat(TStringList& messages);
    FLBOS_ResolveIPPortMethod* m_original_resolve;
};


/**  We add nothing here                                               */
bool CTestLBOSApp::TestApp_Args(CArgDescriptions& args)
{
    return true;
}

/**  Thread constructor. We need nothing from it                       */
bool CTestLBOSApp::Thread_Init(int idx)
{
    return true;
}


void CTestLBOSApp::SwapAddressesTest(int idx)
{
    /* Pseudo random order */
    int i = 0;
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 1, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 2, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 3, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 4, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<7, 5, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 6, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 7, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 8, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 9, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 10, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 11, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<7, 12, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 13, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 14, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 15, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 16, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 17, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<5, 18, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 19, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 20, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 21, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 22, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 23, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 24, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 25, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<5, 26, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 27, true>();
    TestApp_GlobalSyncPoint();
    Initialization::SwapAddressesTest<1, 28, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 92, true>();
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 30, true>();
    TestApp_GlobalSyncPoint();

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = m_original_resolve;
}

bool CTestLBOSApp::Thread_Run(int idx)
{
#define LIST_OF_FUNCS                                                         \
    SwapAddressesTest(idx);                                                   \
    TestApp_GlobalSyncPoint();                                              \
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = m_original_resolve;    \
    X(01,Compose_LBOS_address::LBOSExists__ShouldReturnLbos)                  \
    TestApp_IntraGroupSyncPoint();                                            \
    X(04, Reset_iterator::NoConditions__IterContainsZeroCandidates)           \
    TestApp_IntraGroupSyncPoint();                                            \
    X(05, Reset_iterator::MultipleReset__ShouldNotCrash)                      \
    TestApp_IntraGroupSyncPoint();                                            \
    X(06, Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash)          \
    TestApp_IntraGroupSyncPoint();                                            \
    X(07, Close_iterator::AfterOpen__ShouldWork)                              \
    TestApp_IntraGroupSyncPoint();                                            \
    X(08, Close_iterator::AfterReset__ShouldWork)                             \
    TestApp_IntraGroupSyncPoint();                                            \
    X(9, Close_iterator::AfterGetNextInfo__ShouldWork)                        \
    TestApp_IntraGroupSyncPoint();                                            \
    X(10, Close_iterator::FullCycle__ShouldWork)                              \
    TestApp_IntraGroupSyncPoint();                                            \
    X(11, Resolve_via_LBOS::ServiceExists__ReturnHostIP)                      \
    TestApp_IntraGroupSyncPoint();                                            \
    X(12, Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL)                  \
    TestApp_IntraGroupSyncPoint();                                            \
    X(13, Resolve_via_LBOS::NoLBOS__ReturnNULL)                               \
    TestApp_IntraGroupSyncPoint();                                            \
    X(17,Get_LBOS_address::CustomHostNotProvided__SkipCustomHost)             \
    TestApp_IntraGroupSyncPoint();                                            \
    X(27, GetNextInfo::IterIsNull__ReconstructData)                           \
    TestApp_IntraGroupSyncPoint();                                            \
    X(28, GetNextInfo::WrongMapper__ReturnNull)                               \
    TestApp_IntraGroupSyncPoint();                                            \
    X(29, Stability::GetNext_Reset__ShouldNotCrash)                           \
    TestApp_IntraGroupSyncPoint();                                            \
    X(30,Stability::FullCycle__ShouldNotCrash)                                \
    TestApp_IntraGroupSyncPoint();


    #define X(num,name) name();
        LIST_OF_FUNCS
    #undef X
    return true;
}

bool CTestLBOSApp::TestApp_Init(void)
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
    m_original_resolve = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort;
    return true;
}

bool CTestLBOSApp::TestApp_Exit(void)
{
    return true;
}

int main(int argc, const char* argv[])
{
    CTestLBOSApp app;
    app.AppMain(argc, argv);
}
