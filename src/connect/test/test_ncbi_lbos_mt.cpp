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
#include <ncbi_pch.hpp>
#include "test_ncbi_lbos_common.hpp"
#include <corelib/test_mt.hpp>


USING_NCBI_SCOPE;

#ifdef NCBI_THREADS
static CHealthcheckThread* s_HealthchecKThread;
#endif


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
};


bool CTestLBOSApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalPositional("lbos", 
                                "Primary_LBOS_address", 
                                CArgDescriptions::eString);
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "LBOS mapper API MT test");
    return true;
}

/**  Thread constructor. We need nothing from it                       */
bool CTestLBOSApp::Thread_Init(int idx)
{
    return true;
}


void CTestLBOSApp::SwapAddressesTest(int idx)
{
    /* Save current function pointer. It will be changed inside
     * test functions */
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
                        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort,
                        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort);
    /* Pseudo random order */
    int i = 0;
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 1, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 2, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 3, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 4, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<7, 5, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 6, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 7, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<-1, 8, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 9, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 10, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 11, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<-1, 12, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 13, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 14, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 15, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<-1, 16, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 17, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<-1, 18, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 19, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<1, 20, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 21, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 22, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 23, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 24, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<3, 25, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<5, 26, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<2, 27, true>(mock);
    TestApp_GlobalSyncPoint();
    Initialization::SwapAddressesTest<1, 28, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<6, 92, true>(mock);
    TestApp_GlobalSyncPoint();
    ++i;
    Initialization::SwapAddressesTest<4, 30, true>(mock);
    TestApp_GlobalSyncPoint();
}


#define RUN_TEST(test)                                                         \
    test;                                                                      \
    TestApp_IntraGroupSyncPoint();                                             \
    if (idx == 1) {                                                            \
        s_ClearZooKeeper();                                                    \
    } else {                                                                   \
        SleepMilliSec(500);                                                    \
    }                                                                          \
    TestApp_IntraGroupSyncPoint();



bool CTestLBOSApp::Thread_Run(int idx)
{    
    //SwapAddressesTest(idx);
    if (idx < 10) { /* because too much nodes are created (3 per thread) and
                       this test application cannot handle healthchecks */
        RUN_TEST(CloseIterator::FullCycle__ShouldWork());
    } else {
        RUN_TEST(SleepMilliSec(10000));
    }
    RUN_TEST(Announcement::AlreadyAnnouncedInTheSameZone__ReplaceInStorage(idx));
    RUN_TEST(ComposeLBOSAddress::LBOSExists__ShouldReturnLbos());
    RUN_TEST(ResetIterator::NoConditions__IterContainsZeroCandidates());
    RUN_TEST(ResetIterator::MultipleReset__ShouldNotCrash());
    RUN_TEST(ResetIterator::Multiple_AfterGetNextInfo__ShouldNotCrash());
    RUN_TEST(CloseIterator::AfterOpen__ShouldWork());
    RUN_TEST(CloseIterator::AfterReset__ShouldWork());
    RUN_TEST(CloseIterator::AfterGetNextInfo__ShouldWork());
    RUN_TEST(ResolveViaLBOS::ServiceExists__ReturnHostIP());
    RUN_TEST(ResolveViaLBOS::ServiceDoesNotExist__ReturnNULL());
    RUN_TEST(ResolveViaLBOS::NoLBOS__ReturnNULL());
    RUN_TEST(GetLBOSAddress::CustomHostNotProvided__SkipCustomHost());
    RUN_TEST(GetNextInfo::WrongMapper__ReturnNull());
    RUN_TEST(Announcement::AllOK__ReturnSuccess(idx));
    RUN_TEST(Announcement::AllOK__LBOSAnswerProvided(idx));
    RUN_TEST(Announcement::AllOK__AnnouncedServerSaved(idx));
    RUN_TEST(Announcement::IncorrectURL__ReturnInvalidArgs(idx));
    RUN_TEST(Announcement::IncorrectPort__ReturnInvalidArgs(idx));
    RUN_TEST(Announcement::IncorrectVersion__ReturnInvalidArgs(idx));
    RUN_TEST(Announcement::IncorrectServiceName__ReturnInvalidArgs(idx));
    RUN_TEST(Announcement::RealLife__VisibleAfterAnnounce(idx));
    RUN_TEST(Deannouncement::Deannounced__Return1(0, idx));
    RUN_TEST(Deannouncement::Deannounced__AnnouncedServerRemoved(idx));
    RUN_TEST(Deannouncement::LBOSExistsDeannounce400__Return400(idx));
    RUN_TEST(Deannouncement::RealLife__InvisibleAfterDeannounce(idx));
    RUN_TEST(Stability::GetNext_Reset__ShouldNotCrash(idx));
    RUN_TEST(Stability::FullCycle__ShouldNotCrash(idx));
    RUN_TEST(Performance::FullCycle__ShouldNotCrash(idx));

    return true;
}

bool CTestLBOSApp::TestApp_Init(void)
{
    CConnNetInfo net_info;
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
    if (GetArgs()["lbos"]) {
        string custom_lbos = GetArgs()["lbos"].AsString();
        config.Set("CONN", "LBOS", custom_lbos);
    }
    LOG_POST(Error << "LBOS tested: " << config.Get("CONN", "LBOS"));
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> status_message(NULL);
    LBOS_ServiceVersionSet("/lbostest", "1.0.0",
                              &*lbos_answer, &*status_message);
#ifdef NCBI_THREADS
    s_HealthchecKThread = new CHealthcheckThread;
    s_HealthchecKThread->Run();
#endif
#ifdef DEANNOUNCE_ALL_BEFORE_TEST
    s_ClearZooKeeper();
#endif
    return true;
}


bool CTestLBOSApp::TestApp_Exit(void)
{
#ifdef NCBI_THREADS
    s_HealthchecKThread->Stop(); // Stop listening on the socket
    s_HealthchecKThread->Join();
#endif /* NCBI_THREADS */
    return true;
}

int main(int argc, const char* argv[])
{
    CTestLBOSApp app;
    app.AppMain(argc, argv);
    return 0;
}
