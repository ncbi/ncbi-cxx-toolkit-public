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
 * Author:  Anton Lavrentiev, Dmitriy Elisov
 *
 * File Description:
 *   Standard test for named service resolution facility
 *
 */

/*C++*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
/*C*/
#include "../ncbi_ansi_ext.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_lbos.h"

#define BOOST_AUTO_TEST_MAIN
/*std*/
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#else
#  include <sys/time.h>
#endif
// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
/*test*/
#include "test_assert.h"


USING_NCBI_SCOPE;


/* First let's declare some functions that will be
 * used in different test suites. It is convenient
 * that their difinitions are at the very end, so that
 * test config is as high as possible */
static void             s_PrintInfo                         (HOST_INFO);
static void             s_TestFindMethod                    (ELBOSFindMethod);
template <int lines>
    static EIO_Status   s_FakeReadDiscovery                 (CONN,   char*,
                                                             size_t, size_t*,
                                                             EIO_ReadMethod );
template <int lines>
    static EIO_Status   s_FakeReadAnnouncement              (CONN conn,
                                                             char* line,
                                                             size_t size,
                                                             size_t* n_read,
                                                             EIO_ReadMethod how);
template <int lines>
    static EIO_Status   s_FakeReadDiscoveryCorrupt          (CONN, char*,
                                                             size_t, size_t*,
                                                             EIO_ReadMethod);
template<int count>
    static void         s_FakeFillCandidates                (SLBOS_Data*,
                                                             const char*);
static void             s_FakeFillCandidatesWithError       (SLBOS_Data*,
                                                             const char*);
static SSERV_Info**     s_FakeResolveIPPort                 (const char*,
                                                             const char*,
                                                             SConnNetInfo*);
#ifdef NCBI_OS_MSWIN
    static int          s_GetTimeOfDay                      (struct timeval*);
#else
#   define              s_GetTimeOfDay(tv)                   gettimeofday(tv, 0)
#endif
static unsigned short   s_Msb                               (unsigned short);
static const char*      s_OS                                (TNcbiOSType);
static const char*      s_Bits                              (TNcbiCapacity);
/** @brief Count difference between two timestamps, in seconds*/
static double           s_TimeDiff                      (const struct timeval*,
                                                         const struct timeval*);
/** @brief Return a priori known LBOS address */
static char*            s_FakeComposeLBOSAddress            ( );
/** @brief Get number of servers with specified IP announced in ZK  */
//static int            s_CountServers                      (const char*,
//                                                             unsigned int);

/* Static variables that are used in mock functions.
 * This is not thread-safe! */
static int              s_call_counter                      = 0;
static char*            s_last_header                       = NULL;
/*static char*            s_LBOS_hostport                     = NULL;*/


NCBITEST_INIT_TREE()
{
    /* Template to skip tests (and for easy navigation with "go to definition"
     * function of your IDE). Commenting all lines is usually
     * necessary. If something is uncommented for unknown reason, comment
     * it now! */

      /* Compose LBOS address */
//    NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos);
//    NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL);
//    NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL);
//    /* Reset iterator */
//    NCBITEST_DISABLE(SERV_Reset__NoConditions__IterContainsZeroCandidates);
//    NCBITEST_DISABLE(SERV_Reset__MultipleReset__ShouldNotCrash);
//    NCBITEST_DISABLE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash);
//    /* Close iterator */
//    NCBITEST_DISABLE(SERV_CloseIter__AfterOpen__ShouldWork);
//    NCBITEST_DISABLE(SERV_CloseIter__AfterReset__ShouldWork);
//    NCBITEST_DISABLE(SERV_CloseIter__AfterGetNextInfo__ShouldWork);
//    NCBITEST_DISABLE(SERV_CloseIter__FullCycle__ShouldWork);
//    /* Resolve via LBOS */
//    NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP);
//    NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL);
//    NCBITEST_DISABLE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL);
//    NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess);
//    NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash);
//    /* Get LBOS address */
//    NCBITEST_DISABLE(g_LBOS_getLBOSAddresses__SpecificMethod__FirstInResult);
//    //NCBITEST_DISABLE(g_LBOS_getLBOSAddresses__CustomHostNotProvided__SkipCustomHost);
//    NCBITEST_DISABLE(g_LBOS_getLBOSAddresses__NoConditions__AddressDefOrder);
//    /* Get candidates */
//    NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS);
//    NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSResponse__Finish);
//    //NCBITEST_DISABLE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo);
//    /* GetNextInfo */
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__HaveCands__ReturnNext);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__DataIsNull__ReconstructData);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__IterIsNull__ReconstructData);
//    NCBITEST_DISABLE(SERV_GetNextInfoEx__WrongMapper__ReturnNull);
//    /* Open */
//    NCBITEST_DISABLE(SERV_LBOS_Open__IterIsNull__ReturnNull);
//    NCBITEST_DISABLE(SERV_LBOS_Open__NetInfoNull__ReturnNull);
//    NCBITEST_DISABLE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations);
//    NCBITEST_DISABLE(SERV_LBOS_Open__InfoPointerProvided__WriteNull);
//    NCBITEST_DISABLE(SERV_LBOS_Open__NoSuchService__ReturnNull);
//    /*General LBOS*/
//    NCBITEST_DISABLE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations);
//    NCBITEST_DISABLE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull);
//    NCBITEST_DISABLE(TestLbos_FindMethod__LbosExist__ShouldWork);
//    /* Announcement / deannouncement */
//    //NCBITEST_DISABLE(AnnouncementDeannouncement__Announce__FindMyself);
//    /* Stability */
//    NCBITEST_DISABLE(Stability__GetNext_Reset__ShouldNotCrash);
//    NCBITEST_DISABLE(Stability__FullCycle__ShouldNotCrash);
//    /* Performance */
//     NCBITEST_DISABLE(Performance__FullCycle__ShouldNotCrash);
//    /*Multi-threading*/
//    NCBITEST_DISABLE(MultiThreading_test1);
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "LBOS mapper Unit Test");
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Compose_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void LBOSExists__ShouldReturnLbos();
static void RoleFail__ShouldReturnNULL();
static void DomainFail__ShouldReturnNULL();


static void LBOSExists__ShouldReturnLbos()
{
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(!g_StringIsNullOrEmpty(result), "LBOS address was "
                           "not constructed appropriately");
}

/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multithreaded testing.
 */
static void RoleFail__ShouldReturnNULL()
{
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    int lastSlash = min(path.rfind('/'), string::npos); //UNIX
    int lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
    path = path.substr(0, max(lastSlash, lastBackSlash));
    string corruptRoleString = path + string("/ncbi_lbos_role");
    const char* corruptRole = corruptRoleString.c_str();
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(corruptRole, NULL);
    ofstream roleFile;

    /* 1. Empty role */
    roleFile.open(corruptRoleString.data());
    roleFile << "";
    roleFile.close();

    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on empty role");
    /* Intermediary cleanup*/
    if (!g_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }

    /* 2. Garbage role */
    roleFile.open(corruptRoleString.data());
    roleFile << "I play a thousand of roles";
    roleFile.close();
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on garbage role");
    /* Intermediary cleanup*/
    if (!g_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }

    /* 3. No role file (use set of symbols that certainly
     * cannot constitute a file name)*/
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("|*%&&*^", NULL);
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on bad role file");

    /* Revert changes */
    if (!g_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("/etc/ncbi/role", NULL);
}


/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multithreaded testing.
 */
static void DomainFail__ShouldReturnNULL()
{
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    int lastSlash = min(path.rfind('/'), string::npos); //UNIX
    int lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
    path = path.substr(0, max(lastSlash, lastBackSlash));
    string corruptDomainString = path + string("/ncbi_lbos_domain");
    const char* corruptDomain = corruptDomainString.c_str();
    ofstream domainFile (corruptDomain);
    domainFile << "";
    domainFile.close();

    /* 1. Empty role file */
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, corruptDomain);
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately");

    /* 2. No domain file (use set of symbols that certainly cannot constitute
     * a file name)*/
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, "|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on bad domain file");

    /* Revert changes */
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, "/etc/ncbi/domain");
}
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Reset_iterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void NoConditions__IterContainsZeroCandidates();
static void MultipleReset__ShouldNotCrash();
static void Multiple_AfterGetNextInfo__ShouldNotCrash();
/*
 * Unit tested: SERV_Reset
 * Conditions: No Conditions
 * Expected result: Iter contains zero Candidates
 */
static void NoConditions__IterContainsZeroCandidates()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's.
     */
    SERV_Reset(iter);
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);

    NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
                           "Reset did not set n_cand to 0");
    NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
                           "Reset did not set pos_cand "
                           "to 0");
    return;
}


/*
 * Unit tested: SERV_Reset
 * Conditions: Multiple reset
 * Expected result: should not crash
 */
static void MultipleReset__ShouldNotCrash()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SLBOS_Data* data;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's. It must have clear info by
     * implementation before GetNextInfo is called, so we can set
     * source of LBOS address now
     */
    int i;
    for (i = 0;  i < 15;  ++i) {
        /* If just don't crash here, it is good enough. No assert is
         * necessary, plus this will cause valgrind to swear if not all iter
         * is reset
         */
        SERV_Reset(iter);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
                               "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
                               "Reset did not set pos_cand "
                               "to 0");
    }
    return;
}


static void Multiple_AfterGetNextInfo__ShouldNotCrash()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    SLBOS_Data* data;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of LBOS address now
     */
    int i;
    for (i = 0;  i < 15;  ++i) {
        /* If just don't crash here, it is good enough. No assert is
         * necessary, plus this will cause valgrind to swear if not all iter
         * is reset
         */
        SERV_Reset(iter);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
                               "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
                               "Reset did not set pos_cand to 0");

        HOST_INFO host_info; // we leave garbage here for a reason
        SERV_GetNextInfoEx(iter, &host_info);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand > 0,
                               "n_cand should be more than 0 after "
                               "GetNExtInfo");
        NCBITEST_CHECK_MESSAGE(data->pos_cand > 0,
                               "pos_cand should be more than 0 after "
                               "GetNExtInfo");
        NCBITEST_CHECK_MESSAGE( iter->op != NULL,
                                "Mapper returned NULL when it should "
                                "return s_op");
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                               "Mapper returned NULL when it should "
                               "return s_op");
        NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                               "Name of mapper that returned answer "
                               "is not \"LBOS\"");
        NCBITEST_CHECK_MESSAGE(iter->op->Close != NULL,
                               "Close operation pointer "
                               "is null");
        NCBITEST_CHECK_MESSAGE(iter->op->Feedback != NULL,
                               "Feedback operation pointer is null");
        NCBITEST_CHECK_MESSAGE(iter->op->GetNextInfo != NULL,
                               "GetNextInfo operation pointer is null");
        NCBITEST_CHECK_MESSAGE(iter->op->Reset != NULL,
                               "Reset operation pointer is null");
        NCBITEST_CHECK_MESSAGE(host_info == NULL,
                               "GetNextInfoEx did write something to "
                               "host_info, when it should not");
    }
    return;
}
} /* namespace Reset_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Close_iterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void AfterOpen__ShouldWork();
static void AfterReset__ShouldWork();
static void AfterGetNextInfo__ShouldWork();
static void FullCycle__ShouldWork();

static void AfterOpen__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's.
     */
    SERV_Close(iter);

    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);

    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");
    return;
}
static void AfterReset__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's.
     */
    SERV_Reset(iter);
    SERV_Close(iter);

    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);

    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");
    return;
}

static void AfterGetNextInfo__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    SLBOS_Data* data;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's.
     */
    SERV_GetNextInfo(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Close(iter);

    data = static_cast<SLBOS_Data*>(iter->data);

    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");
    return;
}

static void FullCycle__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    SLBOS_Data* data;
    size_t i;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);

    /*
     * 1. We close after first GetNextInfo
     */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    SERV_GetNextInfo(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Reset(iter);
    SERV_Close(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");

    /*
     * 2. We close after half hosts checked with GetNextInfo
     */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*                                                            v half    */
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand / 2;  ++i) {
        SERV_GetNextInfo(iter);
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Reset(iter);
    SERV_Close(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");

    /* 3. We close after all hosts checked with GetNextInfo*/
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);

    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand;  ++i) {
        SERV_GetNextInfo(iter);
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Reset(iter);
    SERV_Close(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data == NULL, "Reset did not set data to 0");

    /* Cleanup */
    ConnNetInfo_Destroy(net_info);
    return;
}
} /* namespace Close_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Resolve_via_LBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void ServiceExists__ReturnHostIP();
static void ServiceDoesNotExist__ReturnNULL();
static void NoLBOS__ReturnNULL();
static void FakeMassiveInput__ShouldProcess();
static void FakeErrorInput__ShouldNotCrash();

static void ServiceExists__ReturnHostIP()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_lbos_funcs.ResolveIPPort(
            "lbos.dev.ac-va.ncbi.nlm.nih.gov:80", service, net_info);
    size_t i = 0;
    if (hostports != NULL) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            NCBITEST_CHECK_MESSAGE(hostports[i]->host > 0, "Problem with "
                                   "getting host for server");
            NCBITEST_CHECK_MESSAGE(hostports[i]->port > 0, "Problem with"
                                   " getting port for server");
            NCBITEST_CHECK_MESSAGE(hostports[i]->port < 65535, "Problem "
                                   "with getting port for server");
        }
    }
    NCBITEST_CHECK_MESSAGE(i > 0, "Problem with searching for service");
}


static void ServiceDoesNotExist__ReturnNULL()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_lbos_funcs.ResolveIPPort(
            "lbos.dev.ac-va.ncbi.nlm.nih.gov:80", service, net_info);
    size_t i = 0;
    if (hostports != NULL) {
        for (i = 0;  hostports[i] != NULL;  i++) continue;
    }
    NCBITEST_CHECK_MESSAGE(i == 0, "Mapper should not find service, but "
                           "it somehow found.");
}


static void NoLBOS__ReturnNULL()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_lbos_funcs.ResolveIPPort(
            "lbosdevacvancbinlmnih.gov:80", service, net_info);
    size_t i = 0;
    if (hostports != NULL) {
        for (i = 0;  hostports[i] != NULL;  i++) continue;
    }
    NCBITEST_CHECK_MESSAGE(i == 0, "Mapper should not find LBOS, but "
                           "it somehow found.");
}


static void FakeMassiveInput__ShouldProcess()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info;
    char* temp_host;
    unsigned int temp_ip;
    unsigned short temp_port;
    s_call_counter = 0;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */
    EIO_Status  (*temp_func_pointer) (CONN    conn, char*   line,
            size_t  size, size_t* n_read, EIO_ReadMethod how)
                                             = g_lbos_funcs.Read;
    g_lbos_funcs.Read = s_FakeReadDiscovery<10>;
    SSERV_Info** hostports = g_lbos_funcs.ResolveIPPort(
            "lbosdevacvancbinlmnih.gov", "/lbos", net_info);
    int i;
    NCBITEST_CHECK_MESSAGE(hostports != NULL,
                           "Problem with fake massive input, "
                           "no servers found. "
                           "Most likely, problem with test.");
    if (hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            temp_host = static_cast<char*>(calloc(sizeof(char), 100));
            sprintf (temp_host, "%d.%d.%d.%d:%d",
                     i+1, i+2, i+3, i+4, (i+1)*215);
            SOCK_StringToHostPort(temp_host, &temp_ip, &temp_port);
            NCBITEST_CHECK_MESSAGE(hostports[i]->host == temp_ip,
                                   "Problem with recognizing IP"
                                   " in massive input");
            NCBITEST_CHECK_MESSAGE(hostports[i]->port == temp_port,
                                   "Problem with recognizing IP "
                                   "in massive input");
            free(temp_host);
        }
        NCBITEST_CHECK_MESSAGE(i = 200, "Mapper should find 200 hosts, but "
                               "did not.");
        for (i = 0;  hostports[i] != NULL;  i++) {
                    free(hostports[i]);
                }
        free(hostports);
    }
    /* Return everything back*/
    g_lbos_funcs.Read = temp_func_pointer;
}


static void FakeErrorInput__ShouldNotCrash()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info;
    char* temp_host;
    unsigned int temp_ip;
    unsigned short temp_port;
    s_call_counter = 0;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */
    EIO_Status  (*temp_func_pointer) (CONN    conn, char*   line, size_t  size,
            size_t* n_read, EIO_ReadMethod) = g_lbos_funcs.Read;
    g_lbos_funcs.Read = s_FakeReadDiscoveryCorrupt<200>;
    SSERV_Info** hostports = g_lbos_funcs.ResolveIPPort(
            "lbosdevacvancbinlmnih.gov", "/lbos", net_info);
    int i=0, /* iterate test numbers*/
            j=0; /*iterate hostports from ResolveIPPort */
    NCBITEST_CHECK_MESSAGE(hostports != NULL,
                           "Problem with fake error input, no servers found. "
                           "Most likely, problem with test.");
    if (hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            /* We need to check which combination is supposed to be failure*/
            char* check = static_cast<char*>(calloc(sizeof(char), 20));
            sprintf(check, "%03d.%03d.%03d.%03d", i+1, i+2, i+3, i+4);
            if (i < 100 && (strpbrk(check, "89") != NULL)) {
                continue;
            }
            temp_host = static_cast<char*>(calloc(sizeof(char), 100));
            sprintf (temp_host, "%03d.%03d.%03d.%03d:%d", i+1, i+2, i+3, i+4,
                     (i+1)*215);
            SOCK_StringToHostPort(temp_host, &temp_ip, &temp_port);
            NCBITEST_CHECK_MESSAGE(hostports[j]->host == temp_ip,
                                   "Problem with recognizing IP "
                                   "in massive input");
            NCBITEST_CHECK_MESSAGE(hostports[j]->port == temp_port,
                                   "Problem with recognizing IP "
                                   "in massive input");
            j++;
            free(temp_host);
        }
        NCBITEST_CHECK_MESSAGE(i = 200, "Mapper should find 200 hosts, but "
                               "did not.");

        /* Return everything back*/
        for (i = 0;  hostports[i] != NULL;  i++) {
            free(hostports[i]);
        }
        free(hostports);
    }
    g_lbos_funcs.Read = temp_func_pointer;
}
} /* namespace Resolve_via_LBOS */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void SpecificMethod__FirstInResult();
static void CustomHostNotProvided__SkipCustomHost();
static void NoConditions__AddressDefOrder();

/* Not thread-safe because of using g_lbos_funcs */
static void SpecificMethod__FirstInResult()
{
    int i;
    string custom_lbos = "lbos.custom.host";
    char** addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_custom_host,
                                               custom_lbos.c_str());
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], custom_lbos.c_str()) == 0,
                           "Custom specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);

    addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_registry, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0],
                                  "lbos.dev.ac-va.ncbi.nlm.nih.gov:80") == 0,
                           "Registry specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);

    addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_127_0_0_1, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "127.0.0.1:8080") == 0,
                           "Localhost specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);

    /* We have to fake last method, because its result is dependent on
     * location */
    char* (*temp_func_pointer)() = g_lbos_funcs.ComposeLBOSAddress;
    g_lbos_funcs.ComposeLBOSAddress = s_FakeComposeLBOSAddress;
    addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_etc_ncbi_domain, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "lbos.foo") == 0,
                           "/etc/ncbi{role, domain} specified lbos "
                           "address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);

    /* Cleanup */
    g_lbos_funcs.ComposeLBOSAddress = temp_func_pointer;
}

static void CustomHostNotProvided__SkipCustomHost()
{
    /* Test */
    int i = 0;
    char** addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_custom_host,
                                                 NULL);
    for (i = 0;  addresses[i] != NULL;  ++i) continue;
    /* We check that there are only 3 LBOS addresses */
    NCBITEST_CHECK_MESSAGE(i == 4, "Custom host not specified, but "
                           "LBOS still provides it");
    /* Cleanup */
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);
}

static void NoConditions__AddressDefOrder()
{
    int i;
    char* (*temp_func_pointer)() = g_lbos_funcs.ComposeLBOSAddress;
    g_lbos_funcs.ComposeLBOSAddress = s_FakeComposeLBOSAddress;
    char** addresses = g_LBOS_getLBOSAddressesEx(eLBOSFindMethod_custom_host,
                                                 "lbos.custom.host");

    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0],
                                  "lbos.custom.host") == 0,
                           "Custom lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[1],
                                  "lbos.dev.ac-va.ncbi.nlm.nih.gov:80") == 0,
                           "Registry lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[2], "127.0.0.1:8080") == 0,
                           "Localhost lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[3], "lbos.foo") == 0,
                           "primary /etc/ncbi{role, domain} lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[4], "lbos.foo:8080") == 0,
                           "alternate /etc/ncbi{role, domain} lbos address error");
    NCBITEST_CHECK_MESSAGE(addresses[5] == NULL,
                           "Last lbos address NULL item error");

    /* Cleanup */
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);
    g_lbos_funcs.ComposeLBOSAddress = temp_func_pointer;
}
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_candidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void LBOSNoResponse__SkipLBOS();
static void LBOSResponds__Finish();
static void NetInfoProvided__UseNetInfo();


static void LBOSNoResponse__SkipLBOS()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    s_call_counter = 0;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    SSERV_Info** (*temp_func_pointer)(const char*,
            const char*, SConnNetInfo*) = g_lbos_funcs.ResolveIPPort;
    g_lbos_funcs.ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ((SLBOS_Data*)iter->data)->lbos = "lbos.dev.ac-va.ncbi.nlm.nih.gov";
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service
     */
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of dead LBOS");

    /* Cleanup*/
    s_call_counter = 0;
    g_lbos_funcs.ResolveIPPort = temp_func_pointer;
}

static void LBOSResponds__Finish()
{
    s_call_counter = 0;
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    s_call_counter = 2;

    SSERV_Info** (*temp_func_pointer)(const char*,
            const char*, SConnNetInfo*) = g_lbos_funcs.ResolveIPPort;
    g_lbos_funcs.ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ((SLBOS_Data*)iter->data)->lbos = "lbos.dev.ac-va.ncbi.nlm.nih.gov";
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service. We expect only one call, which means that counter
     * should increase by 1
     */
    NCBITEST_CHECK_MESSAGE(s_call_counter == 3,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of alive LBOS");

    /* Cleanup*/
    s_call_counter = 0;
    g_lbos_funcs.ResolveIPPort = temp_func_pointer;
}

/*Not thread safe because of s_last_header*/
static void NetInfoProvided__UseNetInfo()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    s_call_counter = 0;
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    s_call_counter = 2;
    SSERV_Info** (*temp_func_pointer)(const char*,
            const char*, SConnNetInfo*) = g_lbos_funcs.ResolveIPPort;
    g_lbos_funcs.ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ((SLBOS_Data*)iter->data)->lbos = "lbos.dev.ac-va.ncbi.nlm.nih.gov";
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service
     */
    NCBITEST_CHECK_MESSAGE(strcmp(s_last_header, "My header fq34facsadf\r\n")
                           == 0, "s_LBOS_FillCandidates: Incorrect "
                                   "transition of header");

    /* Cleanup*/
    s_call_counter = 0;
    g_lbos_funcs.ResolveIPPort = temp_func_pointer;
}
} /* namespace Get_candidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void EmptyCands__RunGetCandidates();
static void ErrorUpdating__ReturnNull();
static void HaveCands__ReturnNext();
static void LastCandReturned__ReturnNull();
static void DataIsNull__ReconstructData();
static void IterIsNull__ReconstructData();
static void WrongMapper__ReturnNull();


static void EmptyCands__RunGetCandidates()
{
    s_call_counter = 0;
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    SERV_ITER iter;
    string hostport = "1.2.3.4:210";
    unsigned int host;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_lbos_funcs.FillCandidates;
    g_lbos_funcs.FillCandidates = s_FakeFillCandidates<10>;

    /* If no candidates found yet, get candidates and return first of them. */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    info = SERV_GetNextInfoEx(iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_call_counter = 0;

    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(iter);
    info = SERV_GetNextInfoEx(iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
    SERV_Close(iter);
    g_lbos_funcs.FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}


static void ErrorUpdating__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_lbos_funcs.FillCandidates;
    g_lbos_funcs.FillCandidates = s_FakeFillCandidatesWithError;

    /*If no candidates found yet, get candidates, catch error and return NULL*/
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    HOST_INFO hinfo = NULL;
    info = SERV_GetNextInfoEx(iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(info == 0, "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to error in LBOS" );
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in LBOS");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_call_counter = 0;

    /* Now we first play fair, Open() iter, then Reset() iter, and in the
     * end simulate error */
    g_lbos_funcs.FillCandidates = s_FakeFillCandidates<10>;
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(iter);
    g_lbos_funcs.FillCandidates = s_FakeFillCandidatesWithError;
    info = SERV_GetNextInfoEx(iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(info == 0, "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to error in LBOS "
                           "(info not NULL)" );
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in LBOS "
                           "(fillCandidates was not called once)");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
    ConnNetInfo_Destroy(net_info);
    SERV_Close(iter);
    s_call_counter = 0;
    g_lbos_funcs.FillCandidates = temp_func_pointer;
}

static void HaveCands__ReturnNext()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    SERV_ITER iter;
    string hostport = "127.0.0.1:80";
    unsigned int host;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_lbos_funcs.FillCandidates;
    g_lbos_funcs.FillCandidates = s_FakeFillCandidates<10>;

    /* We will get 200 candidates, iterate 220 times and see how the system
     * behaves */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not "
                           "ask LBOS for candidates");

    int i = 0, found_hosts = 0;
    for (i = 0;  i < 220/*more than 200 returned by s_FakeFillCandidates*/;  i++)
    {
        info = SERV_GetNextInfoEx(iter, &hinfo);
        if (info != NULL) { /*As we suppose it will be last 20 iterations */
            found_hosts++;
            char* host_port = static_cast<char*>(calloc(sizeof(char), 100));
            sprintf (host_port, "%d.%d.%d.%d:%d", i+1, i+2, i+3, i+4, (i+1)*210);
            SOCK_StringToHostPort(host_port, &host, &port);
            NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                                   "SERV_GetNextInfoEx: fill "
                                   "candidates was called, but "
                                   "it should not be");
            NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                                   "SERV_GetNextInfoEx: mapper "
                                   "error with 'next' returned element");
            NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                                   "SERV_GetNextInfoEx: hinfo is not "
                                   "NULL (always should be NULL)");
            free(host_port);
        }
    }

    /* The main interesting here is to check if info is null, and that
     * 'fillcandidates()' was not called again internally*/
    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper error with "
                           "'after last' returned element");
    NCBITEST_CHECK_MESSAGE(found_hosts = 200, "Mapper should find 200 "
                                              "hosts, but did not.");

    /* Cleanup*/
    SERV_Close(iter);
    g_lbos_funcs.FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

static void LastCandReturned__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    HOST_INFO hinfo = NULL;
    SERV_ITER iter;
    string hostport = "127.0.0.1:80";
    unsigned int host;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_lbos_funcs.FillCandidates;
    g_lbos_funcs.FillCandidates = s_FakeFillCandidates<10>;

    /* If no candidates found yet, get candidates and return first of them. */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");

    int i;
    for (i = 0;  info != NULL;  i++) {
        info = SERV_GetNextInfoEx(iter, &hinfo);
    }

    NCBITEST_CHECK_MESSAGE(i = 200, "Mapper should find 200 hosts, but "
                           "did not.");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
    SERV_Close(iter);
    g_lbos_funcs.FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

static void DataIsNull__ReconstructData()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    SERV_ITER iter;
    string hostport = "1.2.3.4:210";
    unsigned int host;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_lbos_funcs.FillCandidates;
    g_lbos_funcs.FillCandidates = s_FakeFillCandidates<10>;

    /* We will get iterator, and then delete data from it and run GetNextInfo.
     * The mapper should recover from this kind of error */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    /* Now we destroy data */
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);
    g_lbos_funcs.DestroyData(data);
    iter->data = NULL;
    /* Now let's see how the mapper behaves. Let's check the first element */
    info = SERV_GetNextInfoEx(iter, &hinfo);
    /*Assert*/
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "SERV_GetNextInfoEx: mapper did "
                           "not ask LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error "
                           "with first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not "
                           "NULL (always should be NULL)");

    /* Cleanup*/
    SERV_Close(iter);
    g_lbos_funcs.FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

static void IterIsNull__ReconstructData()
{
    const SSERV_Info* info;
    HOST_INFO host_info;
    info = g_lbos_funcs.GetNextInfo(NULL, &host_info);

    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to empty iter");
}

static void WrongMapper__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    SERV_ITER iter;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    /* We will get iterator, and then change mapper name from it and run
     * GetNextInfo.
     * The mapper should return null */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    const SSERV_VTable* origTable = iter->op;
    const SSERV_VTable fakeTable = {
                                    NULL, NULL, NULL, NULL, NULL, "LBSMD"
    };
    iter->op = &fakeTable;
    /* Now let's see how the mapper behaves. Let's run GetNextInfo()*/
    info = SERV_GetNextInfoEx(iter, &hinfo);

    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to wrong mapper name");

    iter->op = origTable; /* Because we need to clean iter */

    /* Cleanup*/
    SERV_Close(iter);
}
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void IterIsNull__ReturnNull();
static void NetInfoNull__ReturnNull();
static void ServerExists__ReturnLbosOperations();
static void InfoPointerProvided__WriteNull();
static void NoSuchService__ReturnNull();


static void IterIsNull__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(1, sizeof(*iter)));

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter->op = SERV_LBOS_Open(NULL, net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op == NULL,
                           "Mapper returned operations when "
                           "it should return NULL");
}


static void NetInfoNull__ReturnNull()
{
    const char* service = "/lbos";
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(1, sizeof(*iter)));

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    NCBITEST_CHECK_MESSAGE(iter->op != NULL, "Mapper returned NULL when it "
                           "should return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                           "Name of mapper that returned "
                           "answer is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(iter->op->Close != NULL, "Close "
            "operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Feedback != NULL,
                           "Feedback operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->GetNextInfo != NULL,
                           "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Reset != NULL,
                           "Reset operation pointer is null");
}


static void ServerExists__ReturnLbosOperations()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    iter = static_cast<SERV_ITER>(calloc(1, sizeof(*iter)));
    iter->name = service;
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter->op = SERV_LBOS_Open(iter, net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op != NULL, "Mapper returned "
            "NULL when it should return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                           "Name of mapper that returned "
                           "answer is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(iter->op->Close != NULL,
                           "Close operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Feedback != NULL,
                           "Feedback operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->GetNextInfo != NULL,
                           "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Reset != NULL,
                           "Reset operation pointer is null");
}


static void InfoPointerProvided__WriteNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    iter = static_cast<SERV_ITER>(calloc(1, sizeof(*iter)));
    iter->name = service;
    SSERV_Info* info;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter->op = SERV_LBOS_Open(iter, net_info, &info);
    NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                           "Mapper returned NULL when it should "
                           "return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                           "Name of mapper that returned answer "
                           "is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(info != NULL, "LBOS mapper provided "
            "something in host info, when it should not");
}

static void NoSuchService__ReturnNull()
{
    const char* service = "/service/donotexist";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    iter = static_cast<SERV_ITER>(calloc(1, sizeof(*iter)));
    iter->name = service;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter->op = SERV_LBOS_Open(iter, net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op == NULL,
                           "Mapper returned s_op when it "
                           "should return NULL");
}
} /* namespace Open */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GeneralLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void ServerExists__ShouldReturnLbosOperations();
static void ServerDoesNotExist__ShouldReturnNull();
static void LbosExist__ShouldWork();


static void ServerExists__ShouldReturnLbosOperations()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                           "Mapper returned NULL when it "
                           "should return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                           "Name of mapper that returned "
                           "answer is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(iter->op->Close != NULL,
                           "Close operation pointer "
                           "is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Feedback != NULL,
                           "Feedback operation "
                           "pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->GetNextInfo != NULL,
                           "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE(iter->op->Reset != NULL,
                           "Reset operation pointer "
                           "is null");
}

static void ServerDoesNotExist__ShouldReturnNull()
{
    const char* service = "/asdf/idonotexist";
    SConnNetInfo* net_info;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    NCBITEST_CHECK_MESSAGE(iter == NULL,
                           "Mapper should not find service, but "
                           "it somehow found.");
}

static void LbosExist__ShouldWork()
{
    ELBOSFindMethod find_methods_arr[] = {
                                          eLBOSFindMethod_etc_ncbi_domain,
                                          eLBOSFindMethod_registry,
                                          eLBOSFindMethod_custom_host
    };

    size_t method_iter/*, error_types_iter = 0*/;
    /*Iterate through possible errors*/
    for (method_iter = 0;
            method_iter < sizeof(find_methods_arr)/sizeof(ELBOSFindMethod);
            method_iter++) {
        s_TestFindMethod(find_methods_arr[method_iter]);
    }
}
} /* namespace GeneralLBOS */

//
//namespace Announce
//{
///*
// * 1. Successfully announced: return SUCCESS
// * 2. Successfully announced self: should remember address of LBOS
// * 3. Could not find LBOS: return NO_LBOS
// * 4. Another same service exists: return ALREADY_EXISTS
// * 5. Was passed "0.0.0.0" as IP and could not manage to resolve local
// *    host IP: do not announce and return IP_RESOLVE_ERROR
// * 6. Was passed incorrect healthcheck URL (null or not starting with
// *    "http(s)://"): do not announce and return INCORRECT_CALL
// * 7. Was passed incorrect port (zero): do not announce and return INCORRECT_CALL
// * 8. Was passed incorrect version(i.e. null; another specifications still
// *    have to be written by Ivanovskiy, Vladimir): do not announce and
// *    return INCORRECT_CALL
// * 9. Was passed incorrect service name(i.e. null; another specifications still
// *    have to be written by Ivanovskiy, Vladimir): do not announce and
// *    return INCORRECT_CALL
// */
//static void     AllOK__ReturnSuccess                              ();
//static void     SelfAnnounceOK__RememberLBOS                        ();
//static void     NoLBOS__ReturnErrorAndNotFind                       ();
//static void     AlreadyExists__ReturnErrorAndFind                   ();
//static void     IPResolveError__ReturnErrorAndNotFind               ();
//static void     IncorrectHealthcheckURL__ReturnErrorAndNotFind      ();
//static void     IncorrectPort__ReturnErrorAndNotFind                ();
//static void     IncorrectVersion__ReturnErrorAndNotFind             ();
//static void     IncorrectServiceName__ReturnErrorAndNotFind         ();
//
//static void AllOK__ReturnSuccess()
//{
//    unsigned int LBOS_addr = 0;
//    unsigned short LBOS_port = 0;
//    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
//    /* Count how many servers there are before we announce */
//    int count_before = s_CountServers("lbostest", addr);
//    ELBOSAnnounceResult result =
//                        g_LBOS_AnnounceEx("lbostest",
//                                          "1.0.0",
//                                          80,
//                                          "http://intranet.ncbi.nlm.nih.gov",
//                                          &LBOS_addr, &LBOS_port);
//    /* Count how many servers there are */
//    int count_after = s_CountServers("lbostest", addr);
//    NCBITEST_CHECK_MESSAGE(count_after - count_before = 1,
//                               "Number of announced servers did not "
//                               "increase by 1 after announcement");
//    NCBITEST_CHECK_MESSAGE(result = ELBOSAnnounceResult_Success,
//                               "Announcement function did not return "
//                               "SUCCESS as expected");
//    NCBITEST_CHECK_MESSAGE(LBOS_addr != 0,
//                               "LBOS mapper did not answer with IP of LBOS"
//                               "that was used for announcement");
//    NCBITEST_CHECK_MESSAGE(LBOS_port != 0,
//                               "LBOS mapper did not answer with port of LBOS"
//                               "that was used for announcement");
//    /* Cleanup */
//    char lbos_hostport[100];
//    SOCK_HostPortToString(LBOS_addr, LBOS_port, lbos_hostport, 99);
//    g_LBOS_Deannounce(lbos_hostport, "lbostest", "1.0.0", 80,
//                      "intranet.ncbi.nlm.nih.gov");
//}
//
//static void RealLife__ResolveAfterAnnounce()
//{
//    /* First, we check that announce self remembers LBOS */
//    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
//    /* Count how many servers there are before we announce */
//    int count_before = s_CountServers("lbostest", addr);
//    /* Since we cannot guarantee that there is any working healthcheck
//     * on the current host, we will mock */
//    EIO_Status  (*temp_func_pointer) (CONN    conn, char*   line,
//            size_t  size, size_t* n_read, EIO_ReadMethod how)
//            = g_lbos_funcs.Read;
//    g_lbos_funcs.Read = s_FakeReadAnnouncement<1>;
//    ELBOSAnnounceResult result =
//            g_LBOS_AnnounceEx("lbostest",
//                              "1.0.0",
//                              80,
//                              "http://0.0.0.0:5000/checkme", 0, 0);
//
//    /* Cleanup */
//    /* Now we need to check that mapper actually remembers LBOS */
//    ELBOSDeannounceResult  (*temp_func_pointer2) (const char* lbos_hostport,
//            const char* service,
//            const char* version,
//            unsigned short port,
//            const char* ip)
//            = g_lbos_funcs.Deannounce;
//    g_lbos_funcs.Deannounce = s_FakeDeannounce;
//    g_lbos_funcs.Read = s_FakeReadAnnouncement<1>;
//    g_LBOS_DeannounceSelf();
//    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
//    NCBITEST_CHECK_MESSAGE(strcmp(s_LBOS_hostport, "1.2.3.4:5") == 0,
//                           "Problem with saving LBOS address on self-announcement");
//    g_lbos_funcs.Read = temp_func_pointer;
//    g_lbos_funcs.Deannounce = temp_func_pointer2;
//    /* Second, we check that announce another server does not remember LBOS */
//}
//
//static void NoLBOS__ReturnErrorAndNotFind()
//{
//    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000, "http://0.0.0.0:5000/checkme");
//    g_LBOS_DeannounceSelf();
//}
//
//static void AlreadyExists__ReturnErrorAndFind()
//{
//    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000, "http://0.0.0.0:5000/checkme");
//    g_LBOS_DeannounceSelf();
//}
//
//static void IPResolveError__ReturnErrorAndNotFind()
//{
//    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000, "http://0.0.0.0:5000/checkme");
//}
//}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Stability
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void GetNext_Reset__ShouldNotCrash();
static void FullCycle__ShouldNotCrash();


static void GetNext_Reset__ShouldNotCrash()
{
    int secondsBeforeStop = 60;  /* when to stop this test */
    struct timeval start; /**< we will measure time from start
                                   of test as main measure of when
                                   to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    const SSERV_Info* info;
    int i;
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    double elapsed = 0.0;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
        CORE_LOGF(eLOG_Warning, ("Stability test 1: iteration %d, "
                "%0.2f seconds passed", i, elapsed));
        do {
            info = SERV_GetNextInfoEx(iter, NULL);
        } while (info != NULL);
        SERV_Reset(iter);
        if (s_GetTimeOfDay(&stop) != 0)
            memset(&stop, 0, sizeof(stop));
        elapsed = s_TimeDiff(&stop, &start);
    }
    CORE_LOGF(eLOG_Warning, ("Stability test 1:  %d iterations\n", i));
}

static void FullCycle__ShouldNotCrash()
{
    int secondsBeforeStop = 60; /* when to stop this test */
    double elapsed = 0.0;
    struct timeval start; /**< we will measure time from start of test as main
                                           measure of when to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    SERV_ITER iter;
    const SSERV_Info* info;
    int i = 0;
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
         CORE_LOGF(eLOG_Warning, ("Stability test 2: iteration %d, "
                 "%0.2f seconds passed", i, elapsed));
        iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                          (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                          SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                          net_info, 0/*skip*/, 0/*n_skip*/,
                          0/*external*/, 0/*arg*/, 0/*val*/);
        do {
            info = SERV_GetNextInfoEx(iter, NULL);
        } while (info != NULL);
        SERV_Close(iter);
        if (s_GetTimeOfDay(&stop) != 0)
            memset(&stop, 0, sizeof(stop));
        elapsed = s_TimeDiff(&stop, &start);
    }
    CORE_LOGF(eLOG_Warning, ("Stability test 2:  %d iterations\n", i));
}
} /* namespace Stability */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Performance
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void FullCycle__ShouldNotCrash();

static void FullCycle__ShouldNotCrash()
{
    int                 secondsBeforeStop = 60;  /* when to stop this test */
    double              total_elapsed     = 0.0;
    double              cycle_elapsed     = 0.0; /**< we check performance
                                                         every second */
    struct timeval      start;                   /**< we will measure time from
                                                         start of test as main
                                                         measure of when to finish */
    struct timeval      cycle_start;             /**< to measure start of
                                                         current cycle */
    struct timeval      stop;                    /**< To check time at
                                                         the end of each iterations*/
    const char*         service            =  "/lbos";
    SConnNetInfo*       net_info;
    SERV_ITER           iter;
    const SSERV_Info*   info;
    int                 total_iters        = 0;  /**< Total number of full
                                                         iterations since start of
                                                         test */
    int                 cycle_iters        = 0;  /**< Total number of full
                                                         iterations since start of one
                                                         second cycle */
    int                 total_hosts        = 0;  /**< Total number of found
                                                         hosts since start of
                                                         test */
    int                 cycle_hosts        = 0;  /**< number of full
                                                         iterations since start of
                                                         one second cycle */
    int                 max_iters_per_cycle = 0; /**< Maximum iterations
                                                         per one second cycle */
    int                 min_iters_per_cycle = INT_MAX; /**< Minimum iterations
                                                         per one second cycle */
    int                 max_hosts_per_cycle = 0; /**< Maximum hosts found
                                                         per one second cycle */
    int                 min_hosts_per_cycle = INT_MAX; /**< Minimum hosts found
                                                         per one second cycle */
    /*
     * Basic initialization
     */
    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    if (s_GetTimeOfDay(&start) != 0) { //Initialize time of test start
        memset(&start, 0, sizeof(start));
    }
    if (s_GetTimeOfDay(&cycle_start) != 0) { /*Initialize time of
                                                     iteration start*/
        memset(&cycle_start, 0, sizeof(cycle_start));
    }
    /*
     * Start running iterations
     */
    for (total_iters = 0, cycle_iters = 0;  total_elapsed < secondsBeforeStop;
            ++total_iters, ++cycle_iters) {
        CORE_LOGF(eLOG_Warning, ("Performance test: iteration %d, "
                "%0.2f seconds passed",
                total_iters, total_elapsed));
        iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                          (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                          SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                          net_info, 0/*skip*/, 0/*n_skip*/,
                          0/*external*/, 0/*arg*/, 0/*val*/);
        int hosts_found = 0;
        while ((info = SERV_GetNextInfoEx(iter, NULL)) != NULL) {
            ++hosts_found;
            ++total_hosts;
            ++cycle_hosts;;
        }
        CORE_LOGF(eLOG_Warning,  ("Found %d hosts", hosts_found)  );
        SERV_Close(iter);
        s_call_counter = 0;
        if (s_GetTimeOfDay(&stop) != 0)
            memset(&stop, 0, sizeof(stop));
        total_elapsed = s_TimeDiff(&stop, &start);
        cycle_elapsed = s_TimeDiff(&stop, &cycle_start);
        if (cycle_elapsed > 1.0) { /* If our cycle is 1 second finished,
                                           get some analytics and restart */
            if (cycle_iters > max_iters_per_cycle) {
                max_iters_per_cycle = cycle_iters;
            }
            if (cycle_iters < min_iters_per_cycle) {
                min_iters_per_cycle = cycle_iters;
            }
            if (cycle_hosts > max_hosts_per_cycle) {
                max_hosts_per_cycle = cycle_hosts;
            }
            if (cycle_hosts < min_hosts_per_cycle) {
                min_hosts_per_cycle = cycle_hosts;
            }
            cycle_iters = 0;
            cycle_hosts = 0;
            if (s_GetTimeOfDay(&cycle_start) != 0)
                memset(&cycle_start, 0, sizeof(cycle_start));
        }
    }
    CORE_LOGF(eLOG_Warning, ("Performance test:\n"
            "Iterations:\n"
            "\t   Min: %d iters/sec\n"
            "\t   Max: %d iters/sec\n"
            "\t   Avg: %f iters/sec\n"
            "Found hosts:\n"
            "\t   Min: %d hosts/sec\n"
            "\t   Max: %d hosts/sec\n"
            "\t   Avg: %f hosts/sec\n",
            min_iters_per_cycle,
            max_iters_per_cycle,
            static_cast<double>(total_iters)/total_elapsed,
            min_hosts_per_cycle,
            max_hosts_per_cycle,
            static_cast<double>(total_hosts)/total_elapsed));
}
} /* namespace Performance */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace MultiThreading
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
class CMainLoopThread : public CThread
{
public:
    CMainLoopThread(void (*testFunc)())
{
        this->testFunc = testFunc;
}
    ~CMainLoopThread()
    {
    }

private:
    void* Main(void) {
        testFunc();
        return NULL;
    }
    void (*testFunc)();
};
CMainLoopThread* thread1;
CMainLoopThread* thread2;
CMainLoopThread* thread3;


void TryMultiThread()
{
#define LIST_OF_FUNCS \
X(01,Compose_LBOS_address::LBOSExists__ShouldReturnLbos)\
/*X(02,Compose_LBOS_address::
 *                    RoleFail__ShouldReturnNULL)*/ \
/*X(03,Compose_LBOS_address::
 *                  DomainFail__ShouldReturnNULL)*/ \
X(04,Reset_iterator::NoConditions__IterContainsZeroCandidates)     \
X(05,Reset_iterator::MultipleReset__ShouldNotCrash)                \
X(06,Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash)    \
X(07,Close_iterator::AfterOpen__ShouldWork)                    \
X(08,Close_iterator::AfterReset__ShouldWork)                   \
X(09,Close_iterator::AfterGetNextInfo__ShouldWork)             \
X(10,Close_iterator::FullCycle__ShouldWork)                    \
X(11,Resolve_via_LBOS::ServiceExists__ReturnHostIP)      \
X(12,Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL)  \
X(13,Resolve_via_LBOS::NoLBOS__ReturnNULL)               \
/*X(14,Resolve_via_LBOS::
 *                    FakeMassiveInput__ShouldProcess)*/ \
/*X(15,Resolve_via_LBOS::
 *                    FakeErrorInput__ShouldNotCrash)*/  \
/*X(16,Get_LBOS_address::
 *                    SpecificMethod__FirstInResult)*/\
X(17,Get_LBOS_address::                                                        \
               CustomHostNotProvided__SkipCustomHost) \
/*X(18,Get_LBOS_address::
 *                    NoConditions__AddressDefOrder)*/\
/*X(19,Get_candidates::
 *                  LBOSNoResponse__SkipLBOS)*/         \
/*X(20,Get_candidates::
 *                  LBOSResponse__Finish)*/             \
/*X(21,Get_candidates::
 *                  NetInfoProvided__UseNetInfo*/       \
/*X(22,GetNextInfo::
 *               EmptyCands__RunGetCandidates)*/           \
/*X(23,GetNextInfo::
 *               ErrorUpdating__ReturnNull)*/              \
/*X(24,GetNextInfo::
 *               HaveCands__ReturnNext)*/                  \
/*X(25,GetNextInfo::
 *               LastCandReturned__ReturnNull)*/           \
/*X(26,GetNextInfo::
 *               DataIsNull__ReconstructData)*/            \
X(27,GetNextInfo::IterIsNull__ReconstructData)             \
X(28,GetNextInfo::WrongMapper__ReturnNull)                 \
X(29,Stability::GetNext_Reset__ShouldNotCrash)                      \
X(30,Stability::FullCycle__ShouldNotCrash)


#define X(num,name) CMainLoopThread* thread##num = new CMainLoopThread(name);
    LIST_OF_FUNCS
#undef X

#define X(num,name) thread##num->Run();
    LIST_OF_FUNCS
#undef X


#define X(num,name) thread##num->Join();
    LIST_OF_FUNCS
#undef X

#undef LIST_OF_FUNCS
}
} /* namespace MultiThreading */



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Compose_LBOS_address )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos)
{
    Compose_LBOS_address::LBOSExists__ShouldReturnLbos();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL)
{
    Compose_LBOS_address::RoleFail__ShouldReturnNULL();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL)
{
    Compose_LBOS_address::DomainFail__ShouldReturnNULL();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Reset_iterator)/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should make capacity of elements in data->cand equal zero
 * 2. Should be able to reset iter N times consequently without crash
 * 3. Should be able to "reset iter, then getnextinfo" N times consequently
 *    without crash
 */

BOOST_AUTO_TEST_CASE(SERV_Reset__NoConditions__IterContainsZeroCandidates)
{
    Reset_iterator::NoConditions__IterContainsZeroCandidates();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__MultipleReset__ShouldNotCrash)
{
    Reset_iterator::MultipleReset__ShouldNotCrash();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash)
{
    Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Close_iterator)/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should work immediately after Open
 * 2. Should work immediately after Reset
 * 3. Should work immediately after Open, GetNextInfo
 * 4. Should work immediately after Open, GetNextInfo, Reset
 */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterOpen__ShouldWork)
{
    Close_iterator::AfterOpen__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterReset__ShouldWork)
{
    Close_iterator::AfterReset__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterGetNextInfo__ShouldWork)
{
    Close_iterator::AfterGetNextInfo__ShouldWork();
}

/* In this test we check three different situations: when getnextinfo was
 * called once, was called to see half of all found servers, and was called to
 * iterate through all found servers. */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__FullCycle__ShouldWork)
{
    Close_iterator::FullCycle__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Resolve_via_LBOS )//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if LBOS answered "not found"
 * 3. Should return NULL if LBOS is not reachable
 * 4. Should be able to support up to M IP:port combinations (not checking for
 *    repeats) with storage overhead not more than same as size needed (that
 *    is, all space consumed is twice as size needed, used and unused space
 *    together)
 * 5. Should be able to correctly process incorrect LBOS output. If from 5
 *    servers one is not valid, 4 other servers should be still returned
 *    by mapper
 */

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP)
{
    Resolve_via_LBOS::ServiceExists__ReturnHostIP();
}


BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL)
{
    Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL)
{
    Resolve_via_LBOS::NoLBOS__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess)
{
    Resolve_via_LBOS::FakeMassiveInput__ShouldProcess();
}


BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash)
{
    Resolve_via_LBOS::FakeErrorInput__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Compose_LBOS_address )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_getLBOSAddresses__SpecificMethod__FirstInResult)
{
    Get_LBOS_address::SpecificMethod__FirstInResult();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(
        g_LBOS_getLBOSAddresses__CustomHostNotProvided__SkipCustomHost)
{
    Get_LBOS_address::CustomHostNotProvided__SkipCustomHost();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_getLBOSAddresses__NoConditions__AddressDefOrder)
{
    Get_LBOS_address::NoConditions__AddressDefOrder();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_candidates )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Iterate through received LBOS's addresses, if there is not response from
 *    current LBOS
 * 2. If one LBOS works, do not try another LBOS
 * 3. If net_info was provided for Serv_OpenP, the same net_info should be
 *    available while getting candidates via lbos to provide DTABs*/

/* We need this variables and function to count how many times algorithm will
 * try to resolve IP, and to know if it uses the headers we provided.
 */

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS){
    Get_candidates::LBOSNoResponse__SkipLBOS();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSResponse__Finish) {
    Get_candidates::LBOSResponds__Finish();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo) {
    Get_candidates::NetInfoProvided__UseNetInfo();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(GetNextInfo)/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * GetNextInfo:
 * 1. If no candidates found yet, or reset was just made, get candidates
 *    and return first
 * 2. If no candidates found yet, or reset was just made, and unrecoverable
 *    error while getting candidates, return 0
 * 3. If candidates already found, return next
 * 4. If last candidate was already returned, return 0
 * 5. If data is NULL for some reason, construct new data
 * 6. If iter is NULL, return NULL
 * 7. If SERV_MapperName(iter) returns name of another mapper, return NULL
 */

/* To be sure that mapper returns objects correctly, we need to be sure of
 * what LBOS sends to mapper. The best way is to emulate LBOS
 */

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates)
{
    GetNextInfo::EmptyCands__RunGetCandidates();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull)
{
    GetNextInfo::ErrorUpdating__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__HaveCands__ReturnNext)
{
    GetNextInfo::HaveCands__ReturnNext();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull)
{
    GetNextInfo::LastCandReturned__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__DataIsNull__ReconstructData)
{
    GetNextInfo::DataIsNull__ReconstructData();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__IterIsNull__ReconstructData)
{
    GetNextInfo::IterIsNull__ReconstructData();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__WrongMapper__ReturnNull)
{
    GetNextInfo::WrongMapper__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Open)////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* Open:
 * 1. If iter is NULL, return NULL
 * 2. If net_info is NULL, construct own net_info
 * 3. If read from LBOS successful, return s_op
 * 4. If read from LBOS successful and host info pointer != NULL, write NULL
 *    to host info
 * 5. If read from LBOS unsuccessful or no such service, return 0
 */
BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__IterIsNull__ReturnNull)
{
    IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NetInfoNull__ReturnNull)
{
    NetInfoNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations)
{
    ServerExists__ReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__InfoPointerProvided__WriteNull)
{
    InfoPointerProvided__WriteNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NoSuchService__ReturnNull)
{
    NoSuchService__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( GeneralLBOS )///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * We use such name of service that we MUST get LBOS's answer
 * We check that it finds service and returns non-NULL list of
 * operations
 */
BOOST_AUTO_TEST_CASE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations)
{
    GeneralLBOS::ServerExists__ShouldReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull)
{
    GeneralLBOS::ServerDoesNotExist__ShouldReturnNull();
}

BOOST_AUTO_TEST_CASE(TestLbos_FindMethod__LbosExist__ShouldWork)
{
    GeneralLBOS::LbosExist__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_SUITE( Announcement )//////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_CASE(Announcement__AllOK__AnnounceAndFind)
//{
//    Announce::AllOK__AnnounceAndFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__SelfAnnounceOK__RememberLBOS)
//{
//    Announce::SelfAnnounceOK__RememberLBOS();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__ReturnErrorAndNotFind)
//{
//    Announce::NoLBOS__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__AlreadyExists__ReturnErrorAndFind)
//{
//    Announce::AlreadyExists__ReturnErrorAndFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IPResolveError__ReturnErrorAndNotFind)
//{
//    Announce::IPResolveError__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectHealthcheckURL__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectHealthcheckURL__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectPort__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectPort__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectVersion__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectVersion__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectServiceName__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectServiceName__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Stability)///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Just reset
 * 2. Full cycle: open mapper, get all servers, close iterator, repeat.
 */
/* @brief A simple stability test. We open iterator, and then, not closing it,
 * we get all servers and reset iterator and start getting all servers again
 */
BOOST_AUTO_TEST_CASE(Stability__GetNext_Reset__ShouldNotCrash)
{
    Stability::GetNext_Reset__ShouldNotCrash();
}

BOOST_AUTO_TEST_CASE(Stability__FullCycle__ShouldNotCrash)
{
    Stability::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Performance)/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Perform full cycle: open mapper, get all servers, close iterator, repeat.
 *    Test lasts 60 seconds and measures min, max and avg performance.
 */
BOOST_AUTO_TEST_CASE(Performance__FullCycle__ShouldNotCrash)
{
    Performance::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(MultiThreading)//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Run all tests at once in concurrent threads and expect all tests to run
 *    with the same results, except those in which mocks are not thread safe
 */
BOOST_AUTO_TEST_CASE(MultiThreading_test1)
{
    MultiThreading::TryMultiThread();
}

BOOST_AUTO_TEST_SUITE_END()


static unsigned short s_Msb(unsigned short x)
{
    unsigned int y;
    while ((y = x & (x - 1)) != 0)
        x = y;
    return x;
}


static const char* s_OS(TNcbiOSType ostype)
{
    static char buf[40];
    TNcbiOSType msb = s_Msb(ostype);
    switch (msb) {
    case fOS_Unknown:
        return "unknown";
    case fOS_IRIX:
        return "IRIX";
    case fOS_Solaris:
        return "Solaris";
    case fOS_BSD:
        return ostype == fOS_Darwin ? "Darwin" : "BSD";
    case fOS_Windows:
        return (ostype & fOS_WindowsServer) == fOS_WindowsServer
                ? "WindowsServer" : "Windows";
    case fOS_Linux:
        return "Linux";
    default:
        break;
    }
    sprintf(buf, "(%hu)", ostype);
    return buf;
}


static const char* s_Bits(TNcbiCapacity capacity)
{
    static char buf[40];
    switch (capacity) {
    case fCapacity_Unknown:
        return "unknown";
    case fCapacity_32:
        return "32";
    case fCapacity_64:
        return "64";
    case fCapacity_32_64:
        return "32+64";
    default:
        break;
    }
    sprintf(buf, "(%hu)", capacity);
    return buf;
}


#ifdef NCBI_OS_MSWIN

static int s_GetTimeOfDay(struct timeval* tv)
{
    FILETIME         systime;
    unsigned __int64 sysusec;

    if (!tv)
        return -1;

    GetSystemTimeAsFileTime(&systime);

    sysusec   = systime.dwHighDateTime;
    sysusec <<= 32;
    sysusec  |= systime.dwLowDateTime;
    sysusec  += 5;
    sysusec  /= 10;

    tv->tv_usec = (long)(sysusec % 1000000);
    tv->tv_sec  = (long)(sysusec / 1000000 - 11644473600Ui64);

    return 0;
}

#else

#  define s_GetTimeOfDay(tv)  gettimeofday(tv, 0)

#endif


static double s_TimeDiff(const struct timeval* end,
                         const struct timeval* beg)
{
    if (end->tv_sec < beg->tv_sec)
        return 0.0;
    if (end->tv_usec < beg->tv_usec) {
        if (end->tv_sec == beg->tv_sec)
            return 0.0;
        return (end->tv_sec - beg->tv_sec - 1)
                + (end->tv_usec - beg->tv_usec + 1000000) / 1000000.0;
    }
    return (end->tv_sec - beg->tv_sec)
            + (end->tv_usec - beg->tv_usec) / 1000000.0;
}


static void s_PrintInfo(HOST_INFO hinfo)
{
    static const char kTimeFormat[] = "%m/%d/%y %H:%M:%S";
    time_t t;
    char buf[80];
    double array[5];
    SHINFO_Params params;
    const char* e = HINFO_Environment(hinfo);
    const char* a = HINFO_AffinityArgument(hinfo);
    const char* v = HINFO_AffinityArgvalue(hinfo);
    CORE_LOG(eLOG_Note, "  Host info available:");
    CORE_LOGF(eLOG_Note, ("    Number of CPUs:      %d",
            HINFO_CpuCount(hinfo)));
    CORE_LOGF(eLOG_Note, ("    Number of CPU units: %d @ %.0fMHz",
            HINFO_CpuUnits(hinfo),
            HINFO_CpuClock(hinfo)));
    CORE_LOGF(eLOG_Note, ("    Number of tasks:     %d",
            HINFO_TaskCount(hinfo)));
    if (HINFO_MachineParams(hinfo, &params)) {
        CORE_LOGF(eLOG_Note, ("    Arch:       %d",
                params.arch));
        CORE_LOGF(eLOG_Note, ("    OSType:     %s",
                s_OS(params.ostype)));
        t = (time_t) params.bootup;
        strftime(buf, sizeof(buf), kTimeFormat, localtime(&t));
        CORE_LOGF(eLOG_Note, ("    Kernel:     %hu.%hu.%hu @ %s",
                params.kernel.major,
                params.kernel.minor,
                params.kernel.patch, buf));
        CORE_LOGF(eLOG_Note, ("    Bits:       %s",
                s_Bits(params.bits)));
        CORE_LOGF(eLOG_Note, ("    Page size:  %lu",
                (unsigned long) params.pgsize));
        t = (time_t) params.startup;
        strftime(buf, sizeof(buf), kTimeFormat, localtime(&t));
        CORE_LOGF(eLOG_Note, ("    LBSMD:      %hu.%hu.%hu @ %s",
                params.daemon.major,
                params.daemon.minor,
                params.daemon.patch, buf));
    } else
        CORE_LOG (eLOG_Note,  "    Machine params: unavailable");
    if (HINFO_Memusage(hinfo, array)) {
        CORE_LOGF(eLOG_Note, ("    Total RAM:  %.2fMB", array[0]));
        CORE_LOGF(eLOG_Note, ("    Cache RAM:  %.2fMB", array[1]));
        CORE_LOGF(eLOG_Note, ("    Free  RAM:  %.2fMB", array[2]));
        CORE_LOGF(eLOG_Note, ("    Total Swap: %.2fMB", array[3]));
        CORE_LOGF(eLOG_Note, ("    Free  Swap: %.2fMB", array[4]));
    } else
        CORE_LOG (eLOG_Note,  "    Memory usage: unavailable");
    if (HINFO_LoadAverage(hinfo, array)) {
        CORE_LOGF(eLOG_Note, ("    Load averages: %f, %f (BLAST)",
                array[0], array[1]));
    } else
        CORE_LOG (eLOG_Note,  "    Load averages: unavailable");
    if (a) {
        assert(*a);
        CORE_LOGF(eLOG_Note, ("    Affinity argument: %s", a));
    }
    if (a  &&  v)
        CORE_LOGF(eLOG_Note, ("    Affinity value:    %s%s%s",
                *v ? "" : "\"", v, *v ? "" : "\""));
    CORE_LOGF(eLOG_Note, ("    Host environment: %s%s%s",
            e? "\"": "", e? e: "NULL", e? "\"": ""));
    free(hinfo);
}


static void s_TestFindMethod(ELBOSFindMethod find_method)
{
    const char* service = "/lbos";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    struct timeval start;
    int n_found = 0;
    SERV_ITER iter;

    setlocale(LC_ALL, "");
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /*
     * We know that iter is LBOS's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of LBOS address now
     */
    ((SLBOS_Data*)iter->data)->lbos = "lbos.dev.ac-va.ncbi.nlm.nih.gov";
    ConnNetInfo_Destroy(net_info);
    if (iter) {
        g_LBOS_UnitTesting_SetLBOSFindMethod(iter, find_method);
        HOST_INFO hinfo;
        while ((info = SERV_GetNextInfoEx(iter, &hinfo)) != 0) {
            struct timeval stop;
            double elapsed;
            char* info_str;
            ++n_found;
            if (s_GetTimeOfDay(&stop) != 0)
                memset(&stop, 0, sizeof(stop));
            elapsed = s_TimeDiff(&stop, &start);
            info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Server #%-2d (%.6fs) `%s' = %s",
                    ++n_found, elapsed, SERV_CurrentName(iter),
                    info_str ? info_str : "?"));
            if (hinfo) {
                s_PrintInfo(hinfo);
            }
            if (info_str)
                free(info_str);
            if (s_GetTimeOfDay(&start) != 0)
                memcpy(&start, &stop, sizeof(start));
        }
        CORE_LOGF(eLOG_Trace, ("Resetting the %s service mapper",
                                                    SERV_MapperName(iter)));
        SERV_Reset(iter);
        CORE_LOG(eLOG_Trace, "Service mapper has been reset");
        NCBITEST_CHECK_MESSAGE (n_found && (info = SERV_GetNextInfo(iter)),
                                         "Service not found after reset");
    }
    CORE_LOG(eLOG_Trace, "Closing service mapper");
    SERV_Close(iter);

    if (n_found != 0)
        CORE_LOGF(eLOG_Note, ("%d server(s) found", n_found));
    else
        CORE_LOG(eLOG_Fatal, "Requested service not found");

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return;
}

/* Emulate a lot of records to be sure that algorithm can take so much.
 * The IP number start with zero (octal format), which makes some of
 * IPs with digits more than 7 invalid */
template <int lines>
static EIO_Status s_FakeReadDiscoveryCorrupt(CONN conn,
                           char* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(100, sizeof(char)));
        sprintf (buf,
                 "STANDALONE %0d.0%d.0%d.0%d:%d Regular R=1000.0 L=yes T=30\n",
                 s_call_counter, s_call_counter+1,
                 s_call_counter+2, s_call_counter+3, s_call_counter*215);
        strncat(line, buf, size-1);
        *n_read = strlen(buf);
        free(buf);
        return eIO_Success;
    } else {
        *n_read = 0;
        return eIO_Closed;
    }
}


/* Emulate a lot of records to be sure that algorithm can take so much */
template <int lines>
static EIO_Status s_FakeReadDiscovery(CONN conn,
                           char* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(100, sizeof(char)));
        sprintf (buf,
                 "STANDALONE %d.%d.%d.%d:%d Regular R=1000.0 L=yes T=30\n",
                 s_call_counter, s_call_counter+1,
                 s_call_counter+2, s_call_counter+3, s_call_counter*215);
        strncat(line, buf, size-1);
        *n_read = strlen(buf);
        free(buf);
        return eIO_Success;
    } else {
        *n_read = 0;
        return eIO_Closed;
    }
}


/* Emulate a lot of records to be sure that algorithm can take so much */
template <int lines>
static EIO_Status s_FakeReadAnnouncement(CONN conn,
                           char* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(100, sizeof(char)));
        sprintf (buf, "1.2.3.4:5");
        strncat(line, buf, size-1);
        *n_read = strlen(buf);
        free(buf);
        return eIO_Success;
    } else {
        *n_read = 0;
        return eIO_Closed;
    }
}


template<int count>
static void s_FakeFillCandidates (SLBOS_Data* data,
                                const char* service)
{
    s_call_counter++;
    unsigned int host = 0;
    unsigned short int port = 0;
    data->n_cand = count;
    data->pos_cand = 0;
    data->cand = static_cast<SLBOS_Candidate*>(calloc(sizeof(SLBOS_Candidate),
                                                      data->n_cand + 1));
    char* hostport = static_cast<char*>(calloc(sizeof(char), 100));
    for (int i = 0;  i < count;  i++) {
        sprintf(hostport, "%d.%d.%d.%d:%d", i+1, i+2, i+3, i+4, (i+1)*210);
        SOCK_StringToHostPort(hostport, &host, &port);
        data->cand[i].info = static_cast<SSERV_Info*>(
                calloc(sizeof(SSERV_Info), 1));
        data->cand[i].info->host = host;
        data->cand[i].info->port = port;
    }
    free(hostport);
}


static void s_FakeFillCandidatesWithError (SLBOS_Data* data,
                                           const char* service)
{
    s_call_counter++;
    return;
}


static SSERV_Info** s_FakeResolveIPPort (const char* lbos_address,
                                const char* serviceName,
                                SConnNetInfo* net_info   )
{
    s_call_counter++;
    if (net_info->http_user_header) {
        s_last_header = static_cast<char*>(calloc(sizeof(char),
                                          strlen(net_info->http_user_header)));
        strcat(s_last_header, net_info->http_user_header);
    }
    if (s_call_counter < 2) {
        return NULL;
    } else {
        SSERV_Info** hostports = static_cast<SSERV_Info**>(
                malloc(sizeof(SSERV_Info*)*2));
        hostports[0] = static_cast<SSERV_Info*>(calloc(sizeof(SSERV_Info), 1));
        unsigned int host;
        unsigned short int port;
        SOCK_StringToHostPort("127.0.0.1:80", &host, &port);
        hostports[0]->host = host;
        hostports[0]->port = port;
        hostports[1] = NULL;
        return   hostports;
    }
}


/* Because we cannot be sure in which zone the app will be tested, we
 * will return specified LBOS address and just check that the function is
 * called. Original function is thoroughly tested in another test module.*/
static char* s_FakeComposeLBOSAddress()
{
    char* temp = static_cast<char*>(calloc(20, sizeof(char)));
    strcpy(temp, "lbos.foo");
    return temp;
}

//static int s_CountServers(const char * service, unsigned int host)
//{
//    int servers = 0;
//    if (s_GetTimeOfDay(&start) != 0) {
//        memset(&start, 0, sizeof(start));
//    }
//    SConnNetInfo* net_info;
//    SERV_ITER iter;
//
//    net_info = ConnNetInfo_Create(service);
//    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
//                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
//                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
//                      net_info, 0/*skip*/, 0/*n_skip*/,
//                      0/*external*/, 0/*arg*/, 0/*val*/);
//    do {
//        info = SERV_GetNextInfoEx(iter, NULL);
//        if (info != NULL && info->host == host)
//            servers++;
//    } while (info != NULL);
//    return servers - 1;
//}

//static ELBOSDeannounceResult FakeDeannounce (const char* lbos_hostport,
//        const char* service,
//        const char* version,
//        unsigned short port,
//        const char* ip) {
//    s_LBOS_hostport = strdup(lbos_hostport);
//    return ELBOSAnnounceResult_Success;
//}

