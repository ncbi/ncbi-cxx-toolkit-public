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
    args.AddOptionalPositional("lbos", "Primary address to LBOS",
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

bool CTestLBOSApp::Thread_Run(int idx)
{    
    //SwapAddressesTest(idx);
    Compose_LBOS_address::LBOSExists__ShouldReturnLbos();
    TestApp_IntraGroupSyncPoint();
    Reset_iterator::NoConditions__IterContainsZeroCandidates();
    TestApp_IntraGroupSyncPoint();
    Reset_iterator::MultipleReset__ShouldNotCrash();
    TestApp_IntraGroupSyncPoint();
    Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash();
    TestApp_IntraGroupSyncPoint();
    Close_iterator::AfterOpen__ShouldWork();
    TestApp_IntraGroupSyncPoint();
    Close_iterator::AfterReset__ShouldWork();
    TestApp_IntraGroupSyncPoint();
    Close_iterator::AfterGetNextInfo__ShouldWork();
    TestApp_IntraGroupSyncPoint();
    Close_iterator::FullCycle__ShouldWork();
    TestApp_IntraGroupSyncPoint();
    Resolve_via_LBOS::ServiceExists__ReturnHostIP();
    TestApp_IntraGroupSyncPoint();
    Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL();
    TestApp_IntraGroupSyncPoint();
    Resolve_via_LBOS::NoLBOS__ReturnNULL();
    TestApp_IntraGroupSyncPoint();
    Get_LBOS_address::CustomHostNotProvided__SkipCustomHost();
    TestApp_IntraGroupSyncPoint();
    GetNextInfo::IterIsNull__ReturnNull();
    TestApp_IntraGroupSyncPoint();
    GetNextInfo::WrongMapper__ReturnNull();
    TestApp_IntraGroupSyncPoint();

    Announcement::AllOK__ReturnSuccess(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::AllOK__LBOSAnswerProvided(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::AllOK__AnnouncedServerSaved(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::AlreadyAnnouncedInTheSameZone__ReplaceInStorage(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::IncorrectURL__ReturnInvalidArgs(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::IncorrectPort__ReturnInvalidArgs(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::IncorrectVersion__ReturnInvalidArgs(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::IncorrectServiceName__ReturnInvalidArgs(idx);
    TestApp_IntraGroupSyncPoint();
    Announcement::RealLife__VisibleAfterAnnounce(idx);
    TestApp_IntraGroupSyncPoint();

    Deannouncement::Deannounced__Return1(0, idx);
    TestApp_IntraGroupSyncPoint();
    Deannouncement::Deannounced__AnnouncedServerRemoved(idx);
    TestApp_IntraGroupSyncPoint();
    Deannouncement::LBOSExistsDeannounce400__Return400(idx);
    TestApp_IntraGroupSyncPoint();
    Deannouncement::RealLife__InvisibleAfterDeannounce(idx);
    TestApp_IntraGroupSyncPoint();

    TestApp_IntraGroupSyncPoint();
    Stability::GetNext_Reset__ShouldNotCrash();
    TestApp_IntraGroupSyncPoint();
    Stability::FullCycle__ShouldNotCrash();
    TestApp_IntraGroupSyncPoint();

    return true;
}

bool CTestLBOSApp::TestApp_Init(void)
{
    CConnNetInfo net_info;
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
    if (GetArgs()["lbos"]) {
        string custom_lbos = GetArgs()["lbos"].AsString();
        CNcbiApplication::GetConfig().Set("CONN", "LBOS", custom_lbos);
    }
    ERR_POST(Info << "LBOS=" <<
             CNcbiApplication::GetConfig().Get("CONN", "LBOS"));
    char *lbos_ouput_orig = g_LBOS_UnitTesting_GetLBOSFuncs()->
        UrlReadAll(*net_info, "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080"
        "/lbos/text/service", NULL, NULL);
    string lbos_output = string(lbos_ouput_orig);
    free(lbos_ouput_orig);
    LBOS_Deannounce("/lbostest", /* for initialization */
                    "1.0.0",
                    "lbos.dev.be-md.ncbi.nlm.nih.gov",
                    5000, NULL, NULL);
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
}
