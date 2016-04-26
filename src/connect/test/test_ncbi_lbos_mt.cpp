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
#include "test_assert.h"  /* This header must go last */


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


#define RUN_TEST(test)                                                        \
    test;                                                                     \
    TestApp_IntraGroupSyncPoint();


void Announcement__AlreadyAnnouncedInTheSameZone__ReplaceInStorage()
{                                                
    CHECK_LBOS_VERSION();
    Announcement::AlreadyAnnouncedInTheSameZone__ReplaceInStorage();
}

void g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos()
{
    CHECK_LBOS_VERSION();
    ComposeLBOSAddress::LBOSExists__ShouldReturnLbos();
}

void SERV_Reset__NoConditions__IterContainsZeroCandidates()
{
    CHECK_LBOS_VERSION();
    ResetIterator::NoConditions__IterContainsZeroCandidates();
}

void SERV_Reset__MultipleReset__ShouldNotCrash()
{
    CHECK_LBOS_VERSION();
    ResetIterator::MultipleReset__ShouldNotCrash();
}
void SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash()
{
    CHECK_LBOS_VERSION();
    ResetIterator::Multiple_AfterGetNextInfo__ShouldNotCrash();
}
void SERV_CloseIter__AfterOpen__ShouldWork()
{
    CHECK_LBOS_VERSION();
    CloseIterator::AfterOpen__ShouldWork();
}
void SERV_CloseIter__AfterReset__ShouldWork()
{
    CHECK_LBOS_VERSION();
    CloseIterator::AfterReset__ShouldWork();
}
void SERV_CloseIter__AfterGetNextInfo__ShouldWork()
{
    CHECK_LBOS_VERSION();
    CloseIterator::AfterGetNextInfo__ShouldWork();
}
void s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP()
{
    CHECK_LBOS_VERSION();
    ResolveViaLBOS::ServiceExists__ReturnHostIP();
}
void s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL()
{
    CHECK_LBOS_VERSION();
    ResolveViaLBOS::ServiceDoesNotExist__ReturnNULL();
}
void s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL()
{
    CHECK_LBOS_VERSION();
    ResolveViaLBOS::NoLBOS__ReturnNULL();
}
void g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost()
{
    CHECK_LBOS_VERSION();
    GetLBOSAddress::CustomHostNotProvided__SkipCustomHost();
}
void SERV_GetNextInfoEx__WrongMapper__ReturnNull()
{
    CHECK_LBOS_VERSION();
    GetNextInfo::WrongMapper__ReturnNull();
}
void Announcement__AllOK__ReturnSuccess()
{
    CHECK_LBOS_VERSION();
    Announcement::AllOK__ReturnSuccess();
}
void Announcement__AllOK__LBOSAnswerProvided()
{
    CHECK_LBOS_VERSION();
    Announcement::AllOK__LBOSAnswerProvided();
}
void Announcement__AllOK__AnnouncedServerSaved()
{
    CHECK_LBOS_VERSION();
    Announcement::AllOK__AnnouncedServerSaved();
}
void Announcement__IncorrectURL__ReturnInvalidArgs()
{
    CHECK_LBOS_VERSION();
    Announcement::IncorrectURL__ReturnInvalidArgs();
}
void Announcement__IncorrectPort__ReturnInvalidArgs()
{
    CHECK_LBOS_VERSION();
    Announcement::IncorrectPort__ReturnInvalidArgs();
}
void Announcement__IncorrectVersion__ReturnInvalidArgs()
{
    CHECK_LBOS_VERSION();
    Announcement::IncorrectVersion__ReturnInvalidArgs();
}
void Announcement__IncorrectServiceName__ReturnInvalidArgs()
{
    CHECK_LBOS_VERSION();
    Announcement::IncorrectServiceName__ReturnInvalidArgs();
}
void Announcement__RealLife__VisibleAfterAnnounce()
{
    CHECK_LBOS_VERSION();
    Announcement::RealLife__VisibleAfterAnnounce();
}
void Deannouncement__Deannounced__Return1()
{
    CHECK_LBOS_VERSION();
    Deannouncement::Deannounced__Return1(0);
}
void Deannouncement__Deannounced__AnnouncedServerRemoved()
{
    CHECK_LBOS_VERSION();
    Deannouncement::Deannounced__AnnouncedServerRemoved();
}
void Deannouncement__LBOSExistsDeannounce400__Return400()
{
    CHECK_LBOS_VERSION();
    Deannouncement::LBOSExistsDeannounce400__Return400();
}
void Deannouncement__RealLife__InvisibleAfterDeannounce()
{
    CHECK_LBOS_VERSION();
    Deannouncement::RealLife__InvisibleAfterDeannounce();
}

bool CTestLBOSApp::Thread_Run(int idx)
{
    tls->SetValue(new int, TlsCleanup);
    *tls->GetValue() = idx;
    WRITE_LOG("Thread launched");
    if (idx == 0) {
        LOG_POST(Error << "Cycle started");
    }
    if (idx < 10) { /* because too much nodes are created (3 per thread) and
                        this test application cannot handle healthchecks */
        RUN_TEST(CloseIterator::FullCycle__ShouldWork());
    }
    else {
        RUN_TEST(SleepMilliSec(10000));
    }
    RUN_TEST(Announcement__AlreadyAnnouncedInTheSameZone__ReplaceInStorage());
    RUN_TEST(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos());
    RUN_TEST(SERV_Reset__NoConditions__IterContainsZeroCandidates());
    RUN_TEST(SERV_Reset__MultipleReset__ShouldNotCrash());
    RUN_TEST(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash());
    RUN_TEST(SERV_CloseIter__AfterOpen__ShouldWork());
    RUN_TEST(SERV_CloseIter__AfterReset__ShouldWork());
    RUN_TEST(SERV_CloseIter__AfterGetNextInfo__ShouldWork());
    RUN_TEST(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP());
    RUN_TEST(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL());
    RUN_TEST(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL());
    RUN_TEST(g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost());
    RUN_TEST(SERV_GetNextInfoEx__WrongMapper__ReturnNull());
    RUN_TEST(Announcement__AllOK__ReturnSuccess());
    RUN_TEST(Announcement__AllOK__LBOSAnswerProvided());
    RUN_TEST(Announcement__AllOK__AnnouncedServerSaved());
    RUN_TEST(Announcement__IncorrectURL__ReturnInvalidArgs());
    RUN_TEST(Announcement__IncorrectPort__ReturnInvalidArgs());
    RUN_TEST(Announcement__IncorrectVersion__ReturnInvalidArgs());
    RUN_TEST(Announcement__IncorrectServiceName__ReturnInvalidArgs());
    RUN_TEST(Announcement__RealLife__VisibleAfterAnnounce());
    RUN_TEST(Deannouncement__Deannounced__Return1());
    RUN_TEST(Deannouncement__Deannounced__AnnouncedServerRemoved());
    RUN_TEST(Deannouncement__LBOSExistsDeannounce400__Return400());
    RUN_TEST(Deannouncement__RealLife__InvisibleAfterDeannounce());

    RUN_TEST(Stability::GetNext_Reset__ShouldNotCrash());
    RUN_TEST(Stability::FullCycle__ShouldNotCrash());
    RUN_TEST(Performance::FullCycle__ShouldNotCrash());

    return true;
}

bool CTestLBOSApp::TestApp_Init(void)
{
    tls->SetValue(new int, TlsCleanup);
    *tls->GetValue() = kSingleThreadNumber;
    WRITE_LOG("Thread launched");
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
    s_HealthchecKThread = new CHealthcheckThread;
#ifdef NCBI_THREADS
    s_HealthchecKThread->Run();
#endif
#ifdef DEANNOUNCE_ALL_BEFORE_TEST
    s_ClearZooKeeper();
#endif
    s_CleanDTabs();
    s_ReadLBOSVersion();
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
