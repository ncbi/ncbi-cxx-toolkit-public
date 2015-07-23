#ifndef CONNECT___TEST_NCBI_LBOS__HPP
#define CONNECT___TEST_NCBI_LBOS__HPP

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
 *   Common functions for LBOS mapper tests
 *
 */

/*C++*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <connect/ncbi_conn_stream.hpp>
/*C*/
#include "../ncbi_ansi_ext.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_lbosp.h"

//#define BOOST_AUTO_TEST_MAIN
/*std*/
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#else
#  include <sys/time.h>
#endif

/* Boost Test Framework or test_mt */
#ifdef LBOS_TEST_MT
#undef     NCBITEST_CHECK_MESSAGE
#define    NCBITEST_CHECK_MESSAGE( P, M )  NCBI_ALWAYS_ASSERT(P,M)
#else  /* if LBOS_TEST_MT not defined */
// This header must be included before all Boost.Test headers if there are any
#include    <corelib/test_boost.hpp>
#endif /* #ifdef LBOS_TEST_MT */


/*test*/
#include "test_assert.h"


USING_NCBI_SCOPE;


/* First let's declare some functions that will be
 * used in different test suites. It is convenient
 * that their definitions are at the very end, so that
 * test config is as high as possible */
void                           s_PrintInfo                     (HOST_INFO);
void                           s_TestFindMethod              (ELBOSFindMethod);
template <int lines>
static EIO_Status              s_FakeReadDiscovery             (CONN,   void*,
                                                               size_t, size_t*,
                                                               EIO_ReadMethod);
template <int lines>
static EIO_Status              s_FakeReadAnnouncement       (CONN conn,
                                                             void* line,
                                                             size_t size,
                                                             size_t* n_read,
                                                           EIO_ReadMethod how);
template <int lines>
static EIO_Status              s_FakeReadDiscoveryCorrupt  (CONN, void*,
                                                             size_t, size_t*,
                                                             EIO_ReadMethod);
template<int count>
static void                    s_FakeFillCandidates        (SLBOS_Data* data,
                                                          const char* service);
static void                    s_FakeFillCandidatesWithError(SLBOS_Data*,
                                                             const char*);
static SSERV_Info**            s_FakeResolveIPPort          (const char*,
                                                             const char*,
                                                            SConnNetInfo*);
static void                    s_FakeInitialize              (void);
static void                    s_FakeInitializeCheckInstances(void);
static void                    s_FakeFillCandidatesCheckInstances(SLBOS_Data*
                                                                          data,
                                                          const char* service);
template<int instance_number>
static SSERV_Info**            s_FakeResolveIPPortSwapAddresses(const char*
                                                                  lbos_address,
                                                       const char* serviceName,
                                                      SConnNetInfo*  net_info);

/** Return a priori known LBOS address */
static char*     s_FakeComposeLBOSAddress            (void);
#ifdef NCBI_OS_MSWIN
static int       s_GetTimeOfDay                      (struct timeval*);
#else
#   define       s_GetTimeOfDay(tv)                   gettimeofday(tv, 0)
#endif
static unsigned short  s_Msb                         (unsigned short);
static const char*     s_OS                          (TNcbiOSType);
static const char*     s_Bits                        (TNcbiCapacity);
/** Count difference between two timestamps, in seconds*/
static double    s_TimeDiff                          (const struct timeval*,
                                                      const struct timeval*);


/* Static variables that are used in mock functions.
 * This is not thread-safe! */
static int              s_call_counter                      = 0;
static char*            s_last_header                       = NULL;
#ifdef ANNOUNCE_DEANNOUNCE_TEST
static char*          s_LBOS_hostport                     = NULL;
#endif

/** Get number of servers with specified IP announced in ZK  */
//int            s_CountServers                      (const char*,
//                                                             unsigned int);


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Compose_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSExists__ShouldReturnLbos();
void RoleFail__ShouldReturnNULL();
void DomainFail__ShouldReturnNULL();
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Reset_iterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void NoConditions__IterContainsZeroCandidates();
void MultipleReset__ShouldNotCrash();
void Multiple_AfterGetNextInfo__ShouldNotCrash();
} /* namespace Reset_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Close_iterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void AfterOpen__ShouldWork();
void AfterReset__ShouldWork();
void AfterGetNextInfo__ShouldWork();
void FullCycle__ShouldWork();
} /* namespace Close_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Resolve_via_LBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServiceExists__ReturnHostIP();
void ServiceDoesNotExist__ReturnNULL();
void NoLBOS__ReturnNULL();
void FakeMassiveInput__ShouldProcess();
void FakeErrorInput__ShouldNotCrash();
} /* namespace Resolve_via_LBOS */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void SpecificMethod__FirstInResult();
void CustomHostNotProvided__SkipCustomHost();
void NoConditions__AddressDefOrder();
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_candidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSNoResponse__SkipLBOS();
void LBOSResponds__Finish();
void NetInfoProvided__UseNetInfo();
} /* namespace Get_candidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void EmptyCands__RunGetCandidates();
void ErrorUpdating__ReturnNull();
void HaveCands__ReturnNext();
void LastCandReturned__ReturnNull();
void DataIsNull__ReconstructData();
void IterIsNull__ReconstructData();
void WrongMapper__ReturnNull();
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void IterIsNull__ReturnNull();
void NetInfoNull__ReturnNull();
void ServerExists__ReturnLbosOperations();
void InfoPointerProvided__WriteNull();
void NoSuchService__ReturnNull();
} /* namespace Open */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GeneralLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServerExists__ShouldReturnLbosOperations();
void ServerDoesNotExist__ShouldReturnNull();
void LbosExist__ShouldWork();
} /* namespace GeneralLBOS */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Initialization
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
   /* 1. Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
    *    initialized should not crash
    * 2. At initialization if no LBOS found, mapper must turn OFF
    * 3. At initialization if LBOS found, mapper should be ON
    * 4. If LBOS has not yet been initialized, it should be initialized
    *    at SERV_LBOS_Open()
    * 5. If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open()
    * 6. s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_
    *    Initialize()
    * 7. s_LBOS_InstancesList MUST not be NULL at beginning of
    *    s_LBOS_FillCandidates()
    * 8. s_LBOS_FillCandidates() should switch first and good LBOS
    *    addresses, if first is not responding
    */
    /**  Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
     *   initialized should not crash                                        */
    void MultithreadInitialization__ShouldNotCrash();
    /**  At initialization if no LBOS found, mapper must turn OFF            */
    void InitializationFail__TurnOff();
    /**  At initialization if LBOS found, mapper should be ON                */
    void InitializationSuccess__StayOn();
    /**  If LBOS has not yet been initialized, it should be initialized at
     *  SERV_LBOS_Open().                                                    */
    void OpenNotInitialized__ShouldInitialize();
    /**  If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open().        */
    void OpenWhenTurnedOff__ReturnNull();
    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_Initialize()                                                  */
    void s_LBOS_Initialize__s_LBOS_InstancesListNotNULL();
    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *   s_LBOS_FillCandidates()                                             */
    void s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL();
    /**  s_LBOS_FillCandidates() should switch first and good LBOS addresses,
     *   if first is not responding                                          */
    void PrimaryLBOSInactive__SwapAddresses();
} /* namespace LBOSMapperInit */

//
//namespace Announce
//{
/*
 * 1. Successfully announced: return SUCCESS
 * 2. Successfully announced self: should remember address of LBOS
 * 3. Could not find LBOS: return NO_LBOS
 * 4. Another same service exists: return ALREADY_EXISTS
 * 5. Was passed "0.0.0.0" as IP and could not manage to resolve local
 *    host IP: do not announce and return IP_RESOLVE_ERROR
 * 6. Was passed incorrect healthcheck URL (null or not starting with
 *    "http(s)://"): do not announce and return INCORRECT_CALL
 * 7. Was passed incorrect port (zero): do not announce and return
 *   INCORRECT_CALL
 * 8. Was passed incorrect version(i.e. null; another specifications still
 *    have to be written by Ivanovskiy, Vladimir): do not announce and
 *    return INCORRECT_CALL
 * 9. Was passed incorrect service name(i.e. null; another specifications
 *    still have to be written by Ivanovskiy, Vladimir): do not announce and
 *    return INCORRECT_CALL
 */
//void     AllOK__ReturnSuccess                                ();
//void     SelfAnnounceOK__RememberLBOS                        ();
//void     NoLBOS__ReturnErrorAndNotFind                       ();
//void     AlreadyExists__ReturnErrorAndFind                   ();
//void     IPResolveError__ReturnErrorAndNotFind               ();
//void     IncorrectHealthcheckURL__ReturnErrorAndNotFind      ();
//void     IncorrectPort__ReturnErrorAndNotFind                ();
//void     IncorrectVersion__ReturnErrorAndNotFind             ();
//void     IncorrectServiceName__ReturnErrorAndNotFind         ();
//
//}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Stability
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void GetNext_Reset__ShouldNotCrash();
void FullCycle__ShouldNotCrash();
} /* namespace Stability */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Performance
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void FullCycle__ShouldNotCrash();
} /* namespace Performance */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace MultiThreading
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void TryMultiThread();
} /* namespace MultiThreading */



// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Compose_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSExists__ShouldReturnLbos()
{
#ifndef NCBI_OS_MSWIN
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(result),
                             "LBOS address was not constructed appropriately");
#endif
}

/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multithreaded testing.
 */
void RoleFail__ShouldReturnNULL()
{
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    size_t lastSlash = min(path.rfind('/'), string::npos); //UNIX
    size_t lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
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
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on empty role");
    /* Intermediary cleanup before next test*/
    if (!g_LBOS_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }

    /* 2. Garbage role */
    roleFile.open(corruptRoleString.data());
    roleFile << "I play a thousand of roles";
    roleFile.close();
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on garbage role");
    /* Intermediary cleanup before next test*/
    if (!g_LBOS_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }

    /* 3. No role file (use set of symbols that certainly
     * cannot constitute a file name)*/
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("|*%&&*^", NULL);
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately "
                           "on bad role file");

    /* Revert changes */
    if (!g_LBOS_StringIsNullOrEmpty(result)) {
        free(result);
        result = NULL;
    }
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("/etc/ncbi/role", NULL);
}


/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multi threaded testing.
 */
void DomainFail__ShouldReturnNULL()
{
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    size_t lastSlash = min(path.rfind('/'), string::npos); //UNIX
    size_t lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
    path = path.substr(0, max(lastSlash, lastBackSlash));
    string corruptDomainString = path + string("/ncbi_lbos_domain");
    const char* corruptDomain = corruptDomainString.c_str();
    ofstream domainFile (corruptDomain);
    domainFile << "";
    domainFile.close();

    /* 1. Empty role file */
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, corruptDomain);
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(result), "LBOS address "
                           "construction did not fail appropriately");

    /* 2. No domain file (use set of symbols that certainly cannot constitute
     * a file name)*/
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, "|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(result), "LBOS address "
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
void NoConditions__IterContainsZeroCandidates();
void MultipleReset__ShouldNotCrash();
void Multiple_AfterGetNextInfo__ShouldNotCrash();
/*
 * Unit tested: SERV_Reset
 * Conditions: No Conditions
 * Expected result: Iter contains zero Candidates
 */
void NoConditions__IterContainsZeroCandidates()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

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
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL, "LBOS not found when should be");
        return;
    }
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
void MultipleReset__ShouldNotCrash()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SLBOS_Data* data;
    SERV_ITER iter = NULL;

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
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL, "LBOS not found when should be");
        return;
    }
    int i = 0;
    for (i = 0;  i < 15;  ++i) {
        /* If just don't crash here, it is good enough. No assert is
         * necessary, plus this will cause valgrind to swear if not all iter
         * is reset
         */
        SERV_Reset(iter);
        if (iter == NULL)
            continue;//If nothing found, and reset does not crash - good enough
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
            "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
            "Reset did not set pos_cand "
            "to 0");
    }
    return;
}


void Multiple_AfterGetNextInfo__ShouldNotCrash()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    SLBOS_Data* data;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL, "LBOS not found when should be");
        return;
    }
    /*
     * We know that iter is LBOS's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of LBOS address now
     */
    int i = 0;
    for (i = 0;  i < 15;  ++i) {
        /* If just don't crash here, it is good enough. No assert is
         * necessary, plus this will cause valgrind to swear if not all iter
         * is reset
         */
        SERV_Reset(iter);
        if (iter == NULL)
            continue;//If nothing found, and reset does not crash - good enough
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
            "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
            "Reset did not set pos_cand "
            "to 0");

        HOST_INFO host_info; // we leave garbage here for a reason
        SERV_GetNextInfoEx(iter, &host_info);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand > 0,
                               "n_cand should be more than 0 after "
                               "GetNExtInfo");
        NCBITEST_CHECK_MESSAGE(data->pos_cand > 0,
                               "pos_cand should be more than 0 after "
                               "GetNExtInfo");
        if (iter->op == NULL) {
            NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                "Mapper returned NULL when it should "
                "return s_op");
            return;
        }
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
void AfterOpen__ShouldWork();
void AfterReset__ShouldWork();
void AfterGetNextInfo__ShouldWork();
void FullCycle__ShouldWork();

void AfterOpen__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    SERV_Close(iter);
    return;
}
void AfterReset__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    SERV_Reset(iter);
    SERV_Close(iter);
    return;
}

void AfterGetNextInfo__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    SLBOS_Data* data;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    SERV_GetNextInfo(iter);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Close(iter);
    return;
}

void FullCycle__ShouldWork()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    SLBOS_Data* data;
    size_t i;

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
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    SERV_GetNextInfo(iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Reset(iter);
    SERV_Close(iter);

    /*
     * 2. We close after half hosts checked with GetNextInfo
     */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    data = static_cast<SLBOS_Data*>(iter->data);
    /*                                                            v half    */
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand / 2;  ++i) {
        SERV_GetNextInfo(iter);
    }
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
    SERV_Reset(iter);
    SERV_Close(iter);

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

    /* Cleanup */
    ConnNetInfo_Destroy(net_info);
    return;
}
} /* namespace Close_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Resolve_via_LBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServiceExists__ReturnHostIP();
void ServiceDoesNotExist__ReturnNULL();
void NoLBOS__ReturnNULL();
void FakeMassiveInput__ShouldProcess();
void FakeErrorInput__ShouldNotCrash();

void ServiceExists__ReturnHostIP()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            "lbos.dev.be-md.ncbi.nlm.nih.gov:8080", service, net_info);
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


void ServiceDoesNotExist__ReturnNULL()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            "lbos.dev.be-md.ncbi.nlm.nih.gov:8080", service, net_info);
    size_t i = 0;
    if (hostports != NULL) {
        for (i = 0;  hostports[i] != NULL;  i++) continue;
    }
    NCBITEST_CHECK_MESSAGE(i == 0, "Mapper should not find service, but "
                           "it somehow found.");
}


void NoLBOS__ReturnNULL()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */

    SSERV_Info** hostports = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            "lbosdevacvancbinlmnih.gov:80", service, net_info);
    size_t i = 0;
    if (hostports != NULL) {
        for (i = 0;  hostports[i] != NULL;  i++) continue;
    }
    NCBITEST_CHECK_MESSAGE(i == 0, "Mapper should not find LBOS, but "
                           "it somehow found.");
}


void FakeMassiveInput__ShouldProcess()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info = NULL;
    char* temp_host;
    unsigned int temp_ip;
    unsigned short temp_port;
    s_call_counter = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */
    FLBOS_ConnReadMethod* temp_func_pointer =
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read;
    SLBOS_Functions* lbos_funcs = g_LBOS_UnitTesting_GetLBOSFuncs();
    lbos_funcs->Read = s_FakeReadDiscovery<10>;
    SSERV_Info** hostports = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            "lbosdevacvancbinlmnih.gov", "/lbos", net_info);
    int i = 0;
    NCBITEST_CHECK_MESSAGE(hostports != NULL,
                           "Problem with fake massive input, "
                           "no servers found. "
                           "Most likely, problem with test.");
    if (hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            temp_host = static_cast<char*>(calloc(sizeof(char), 100));
            if (temp_host == NULL) {
                NCBITEST_CHECK_MESSAGE(temp_host != NULL,
                                   "Problem with memory allocation, "
                                   "calloc failed. Not enough RAM?");
                break;
            }
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
                    hostports[i] = NULL;
                }
        free(hostports);
    }
    /* Return everything back*/
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = temp_func_pointer;
}


void FakeErrorInput__ShouldNotCrash()
{
    const char* service = "/service/doesnotexist";
    SConnNetInfo* net_info = NULL;
    char* temp_host;
    unsigned int temp_ip;
    unsigned short temp_port;
    s_call_counter = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    /*
     * We know that iter is LBOS's.
     */
    FLBOS_ConnReadMethod* temp_func_pointer =
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read;
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = s_FakeReadDiscoveryCorrupt<200>;
    SSERV_Info** hostports = g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
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
            if (check == NULL) {
                NCBITEST_CHECK_MESSAGE(check != NULL,
                    "Problem with memory allocation, "
                    "calloc failed. Not enough RAM?");
                break;
            }
            sprintf(check, "%03d.%03d.%03d.%03d", i+1, i+2, i+3, i+4);
            if (i < 100 && (strpbrk(check, "89") != NULL)) {
                continue;
            }
            temp_host = static_cast<char*>(calloc(sizeof(char), 100));
            if (temp_host == NULL) {
                NCBITEST_CHECK_MESSAGE(temp_host != NULL,
                    "Problem with memory allocation, "
                    "calloc failed. Not enough RAM?");
                break;
            }
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
            hostports[i] = NULL;
        }
        free(hostports);
    }
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = temp_func_pointer;
}
} /* namespace Resolve_via_LBOS */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void SpecificMethod__FirstInResult();
void CustomHostNotProvided__SkipCustomHost();
void NoConditions__AddressDefOrder();

/* Not thread-safe because of using s_LBOS_funcs */
void SpecificMethod__FirstInResult()
{
    int i = 0;
    string custom_lbos = "lbos.custom.host";
    char** addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                               custom_lbos.c_str());
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], custom_lbos.c_str()) == 0,
                           "Custom specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
    }
    free(addresses);

    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_Registry, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0],
                                  "lbos.dev.be-md.ncbi.nlm.nih.gov:8080") == 0,
                           "Registry specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
        addresses[i] = NULL;
    }
    free(addresses);

    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_127001, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "127.0.0.1:8080") == 0,
                           "Localhost specified lbos address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
        addresses[i] = NULL;
    }
    free(addresses);

#ifndef NCBI_OS_MSWIN
    /* We have to fake last method, because its result is dependent on
     * location */
    FLBOS_ComposeLBOSAddressMethod* temp_func_pointer =
                        g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress;
    g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress =
                                                     s_FakeComposeLBOSAddress;
    addresses =
              g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_EtcNcbiDomain, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "lbos.foo") == 0,
                           "/etc/ncbi{role, domain} specified lbos "
                           "address method error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[1], "lbos.foo:8080") == 0,
        "/etc/ncbi{role, domain} specified lbos "
        "address method error");
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
        addresses[i] = NULL;
    }
    free(addresses);

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress = temp_func_pointer;
#endif
}

void CustomHostNotProvided__SkipCustomHost()
{
    /* Test */
    int i = 0;
    char** addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                 NULL);
    for (i = 0;  addresses[i] != NULL;  ++i) continue;
    /* We check that there are only 3 LBOS addresses */
#ifdef NCBI_OS_MSWIN //in this case, no /etc/ncbi{role, domain} hosts
    NCBITEST_CHECK_MESSAGE(i == 2, "Custom host not specified, but "
                           "LBOS still provides it");
#else
    NCBITEST_CHECK_MESSAGE(i == 4, "Custom host not specified, but "
        "LBOS still provides it");
#endif
    /* Cleanup */
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
        addresses[i] = NULL;
    }
    free(addresses);
}

void NoConditions__AddressDefOrder()
{
    int i = 0;
    FLBOS_ComposeLBOSAddressMethod* temp_func_pointer =
        g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress;
    g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress =
                                                    s_FakeComposeLBOSAddress;
    char** addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                 "lbos.custom.host");

    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0],
                                  "lbos.custom.host") == 0,
                           "Custom lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[1],
                                  "lbos.dev.be-md.ncbi.nlm.nih.gov:8080") == 0,
                           "Registry lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[2], "127.0.0.1:8080") == 0,
                           "Localhost lbos address error");
#ifndef NCBI_OS_MSWIN
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[3], "lbos.foo") == 0,
                         "primary /etc/ncbi{role, domain} lbos address error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[4], "lbos.foo:8080") == 0,
                       "alternate /etc/ncbi{role, domain} lbos address error");
    NCBITEST_CHECK_MESSAGE(addresses[5] == NULL,
                           "Last lbos address NULL item error");
#else
    NCBITEST_CHECK_MESSAGE(addresses[3] == NULL,
                                          "Last lbos address NULL item error");
#endif


    /* Cleanup */
    for (i = 0;  addresses[i] != NULL;  ++i) {
        free(addresses[i]);
        addresses[i] = NULL;
    }
    free(addresses);
    g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress = temp_func_pointer;
}
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_candidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSNoResponse__SkipLBOS()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    s_call_counter = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    FLBOS_ResolveIPPortMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort;
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov:8080";
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = temp_func_pointer;
}

void LBOSResponds__Finish()
{
    s_call_counter = 0;
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    s_call_counter = 2;

    FLBOS_ResolveIPPortMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort;
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov:8080";
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = temp_func_pointer;
}

/*Not thread safe because of s_last_header*/
void NetInfoProvided__UseNetInfo()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    s_call_counter = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    s_call_counter = 2;
    FLBOS_ResolveIPPortMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort;
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = s_FakeResolveIPPort;

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                             "lbos.dev.be-md.ncbi.nlm.nih.gov";
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = temp_func_pointer;
}
} /* namespace Get_candidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

void EmptyCands__RunGetCandidates()
{
    s_call_counter = 0;
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    SERV_ITER iter = NULL;
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    FLBOS_FillCandidatesMethod* temp_func_pointer =
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                     s_FakeFillCandidates<10>;

    /* If no candidates found yet, get candidates and return first of them. */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}


void ErrorUpdating__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    SERV_ITER iter = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_SetUserHeader(net_info, "My header fq34facsadf");
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    FLBOS_FillCandidatesMethod* temp_func_pointer =
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                 s_FakeFillCandidatesWithError;

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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                     s_FakeFillCandidates<10>;
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(iter);
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                s_FakeFillCandidatesWithError;
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
}

void HaveCands__ReturnNext()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    SERV_ITER iter = NULL;
    string hostport = "127.0.0.1:80";
    unsigned int host = 0;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    FLBOS_FillCandidatesMethod* temp_func_pointer =
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                     s_FakeFillCandidates<10>;

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
    for (i = 0;  i < 220/*200+ returned by s_FakeFillCandidates*/;  i++)
    {
        info = SERV_GetNextInfoEx(iter, &hinfo);
        if (info != NULL) { /*As we suppose it will be last 20 iterations */
            found_hosts++;
            char* host_port = static_cast<char*>(calloc(sizeof(char), 100));
            if (host_port == NULL) {
                NCBITEST_CHECK_MESSAGE(host_port != NULL,
                    "Problem with memory allocation, "
                    "calloc failed. Not enough RAM?");
                break;
            }
            sprintf (host_port, "%d.%d.%d.%d:%d",
                     i+1, i+2, i+3, i+4, (i+1)*210);
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

void LastCandReturned__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    HOST_INFO hinfo = NULL;
    SERV_ITER iter = NULL;
    string hostport = "127.0.0.1:80";
    unsigned int host = 0;
    unsigned short port = 0;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    void (*temp_func_pointer) (SLBOS_Data* data, const char* service) =
            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                     s_FakeFillCandidates<10>;

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

    int i = 0;
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

void DataIsNull__ReconstructData()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    SERV_ITER iter = NULL;
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    FLBOS_FillCandidatesMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                      s_FakeFillCandidates<10>;

    /* We will get iterator, and then delete data from it and run GetNextInfo.
     * The mapper should recover from this kind of error */
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    HOST_INFO hinfo;
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /* Now we destroy data */
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);
    g_LBOS_UnitTesting_GetLBOSFuncs()->DestroyData(data);
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
    g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
    s_call_counter = 0;
}

void IterIsNull__ReconstructData()
{
    const SSERV_Info* info = NULL;
    HOST_INFO host_info;
    info = g_LBOS_UnitTesting_GetLBOSFuncs()->GetNextInfo(NULL, &host_info);

    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to empty iter");
}

void WrongMapper__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    SERV_ITER iter = NULL;

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
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
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
void IterIsNull__ReturnNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(sizeof(*iter), 1));
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        return;
    }
    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter->op = SERV_LBOS_Open(NULL, net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op == NULL,
                           "Mapper returned operations when "
                           "it should return NULL");
}


void NetInfoNull__ReturnNull()
{
    const char* service = "/lbos";
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(sizeof(*iter), 1));
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        return;
    }
    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
            "Mapper returned NULL when it should return s_op");
        return;
    }
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


void ServerExists__ReturnLbosOperations()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(sizeof(*iter), 1));
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter->op = SERV_LBOS_Open(iter, net_info, NULL);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
            "LBOS not found when it should be");
        return;
    }
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


void InfoPointerProvided__WriteNull()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(sizeof(*iter), 1));
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service;
    SSERV_Info* info;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    iter->op = SERV_LBOS_Open(iter, net_info, &info);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
            "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "LBOS") == 0,
                           "Name of mapper that returned answer "
                           "is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(info == NULL, "LBOS mapper provided "
            "something in host info, when it should not");
}

void NoSuchService__ReturnNull()
{
    const char* service = "/service/donotexist";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = static_cast<SERV_ITER>(calloc(sizeof(*iter), 1));
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service;

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
void ServerExists__ShouldReturnLbosOperations()
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                           "Mapper returned NULL when it "
                           "should return s_op");
    if (iter->op == NULL) return;
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

void ServerDoesNotExist__ShouldReturnNull()
{
    const char* service = "/asdf/idonotexist";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;

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

void LbosExist__ShouldWork()
{
    ELBOSFindMethod find_methods_arr[] = {
                                          eLBOSFindMethod_EtcNcbiDomain,
                                          eLBOSFindMethod_Registry,
                                          eLBOSFindMethod_CustomHost
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


#ifdef ANNOUNCE_DEANNOUNCE_TEST

namespace Announce
{
/*
 * 1. Successfully announced: return SUCCESS
 * 2. Successfully announced self: should remember address of LBOS
 * 3. Could not find LBOS: return NO_LBOS
 * 4. Another same service exists: return ALREADY_EXISTS
 * 5. Was passed "0.0.0.0" as IP and could not manage to resolve local
 *    host IP: do not announce and return IP_RESOLVE_ERROR
 * 6. Was passed incorrect healthcheck URL (null or not starting with
 *    "http(s)://"): do not announce and return INCORRECT_CALL
 * 7. Was passed incorrect port (zero): do not announce and return
 *    INCORRECT_CALL
 * 8. Was passed incorrect version(i.e. null; another specifications still
 *    have to be written by Ivanovskiy, Vladimir): do not announce and
 *    return INCORRECT_CALL
 * 9. Was passed incorrect service name(i.e. null; another specifications still
 *    have to be written by Ivanovskiy, Vladimir): do not announce and
 *    return INCORRECT_CALL
 */
void     AllOK__ReturnSuccess                                ();
void     SelfAnnounceOK__RememberLBOS                        ();
void     NoLBOS__ReturnErrorAndNotFind                       ();
void     AlreadyExists__ReturnErrorAndFind                   ();
void     IPResolveError__ReturnErrorAndNotFind               ();
void     IncorrectHealthcheckURL__ReturnErrorAndNotFind      ();
void     IncorrectPort__ReturnErrorAndNotFind                ();
void     IncorrectVersion__ReturnErrorAndNotFind             ();
void     IncorrectServiceName__ReturnErrorAndNotFind         ();

void AllOK__ReturnSuccess()
{
    unsigned int LBOS_addr = 0;
    unsigned short LBOS_port = 0;
    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
    /* Count how many servers there are before we announce */
    int count_before = s_CountServers("lbostest", addr);
    ELBOSAnnounceResult result =
                        g_LBOS_AnnounceEx("lbostest",
                                          "1.0.0",
                                          80,
                                          "http://intranet.ncbi.nlm.nih.gov",
                                          &LBOS_addr, &LBOS_port);
    /* Count how many servers there are */
    int count_after = s_CountServers("lbostest", addr);
    NCBITEST_CHECK_MESSAGE(count_after - count_before = 1,
                               "Number of announced servers did not "
                               "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(result = eLBOSAnnounceResult_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    NCBITEST_CHECK_MESSAGE(LBOS_addr != 0,
                               "LBOS mapper did not answer with IP of LBOS"
                               "that was used for announcement");
    NCBITEST_CHECK_MESSAGE(LBOS_port != 0,
                               "LBOS mapper did not answer with port of LBOS"
                               "that was used for announcement");
    /* Cleanup */
    char lbos_hostport[100];
    SOCK_HostPortToString(LBOS_addr, LBOS_port, lbos_hostport, 99);
    g_LBOS_Deannounce(lbos_hostport, "lbostest", "1.0.0", 80,
                      "intranet.ncbi.nlm.nih.gov");
}

void RealLife__ResolveAfterAnnounce()
{
    /* First, we check that announce self remembers LBOS */
    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
    /* Count how many servers there are before we announce */
    int count_before = s_CountServers("lbostest", addr);
    /* Since we cannot guarantee that there is any working healthcheck
     * on the current host, we will mock */
    EIO_Status  (*temp_func_pointer) (CONN    conn, char*   line,
            size_t  size, size_t* n_read, EIO_ReadMethod how)
            = g_LBOS_UnitTesting_GetLBOSFuncs()->Read;
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = s_FakeReadAnnouncement<1>;
    ELBOSAnnounceResult result =
            g_LBOS_AnnounceEx("lbostest",
                              "1.0.0",
                              80,
                              "http://0.0.0.0:5000/checkme", 0, 0);

    /* Cleanup */
    /* Now we need to check that mapper actually remembers LBOS */
    ELBOSDeannounceResult  (*temp_func_pointer2) (const char* lbos_hostport,
            const char* service,
            const char* version,
            unsigned short port,
            const char* ip)
            = g_LBOS_UnitTesting_GetLBOSFuncs()->Deannounce;
    g_LBOS_UnitTesting_GetLBOSFuncs()->Deannounce = s_FakeDeannounce;
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = s_FakeReadAnnouncement<1>;
    g_LBOS_DeannounceSelf();
    unsigned int addr = SOCK_GetLocalHostAddress(eOn);
    NCBITEST_CHECK_MESSAGE(strcmp(s_LBOS_hostport, "1.2.3.4:5") == 0,
                     "Problem with saving LBOS address on self-announcement");
    g_LBOS_UnitTesting_GetLBOSFuncs()->Read = temp_func_pointer;
    g_LBOS_UnitTesting_GetLBOSFuncs()->Deannounce = temp_func_pointer2;
    /* Second, we check that announce another server does not remember LBOS */
}

void NoLBOS__ReturnErrorAndNotFind()
{
    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000,
        "http://0.0.0.0:5000/checkme");
    g_LBOS_DeannounceSelf();
}

void AlreadyExists__ReturnErrorAndFind()
{
    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000,
        "http://0.0.0.0:5000/checkme");
    g_LBOS_DeannounceSelf();
}

void IPResolveError__ReturnErrorAndNotFind()
{
    g_LBOS_AnnounceEx("lbostest", "1.0.0", 5000,
        "http://0.0.0.0:5000/checkme");
}
}

namespace SelfAnnounce
{
/* 1. #include "Announcement"
 * 2. Successfully announced self: should remember LBOS address and
 *    port in static variable
 * 3. If was passed "0.0.0.0" as IP, should replace it with local IP
 *     or hostname
 * 4. Was passed "0.0.0.0" as IP and could not manage to resolve local
 *    host IP: do not announce and return DNS_RESOLVE_ERROR   */
void  IncludeAnnouncement__AllOK();
void  SelfAnnounced__RememberInstance();
void  IP0000__ReplaceWithLocalIP();
void  ResolveLocalIPError__Return_DNS_RESOLVE_ERROR();
}

namespace Deannounce
{
/* 1. Successfully deannounced: return 1
 * 2. Could not connect to provided LBOS: fail and return 0
 * 3. Successfully connected to LBOS, but deannounce returned
 *    error: return 0
 * 4. Real-life test: after deannouncement server should be invisible
 *    to resolve  */
void Deannounced__Return1();
void NoLBOS__Return0();
void LBOSExistsDeannounceError__Return0();
void ResolveLocalIPError__Return_DNS_RESOLVE_ERROR();
}

namespace SelfDeannounce
{
/* 1. If this application has not announced itself during current
 *    run, return 0
 * 2. If this application has announced itself, but now cannot manage
 *    to connect to LBOS that was used for announcement, then keep
 *    information about self-announcement from static variables, return 0
 * 3. If this application did announce itself, and managed to connect
 *    to LBOS that was used for announcement, but LBOS returned error,
 *    then keep information about self-announcement from static
 *    variables, return 0
 * 4. Successfully deannounced: remove information about self-announcement
 *    from static variables and return 1 */
void NotAnnouncedPreviously__Return0();
void LBOSNotExists__Return0_KeepInfo();
void LBOSExistsDeannounceError__Return0();
void Deannounced__Return1();
}

#endif


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Initialization
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
    /** Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
     * initialized should not crash                                          */
    void MultithreadInitialization__ShouldNotCrash()
    {
        #ifdef LBOS_TEST_MT
            int* init_status = g_LBOS_UnitTesting_InitStatus();
            *init_status = 0;
            GeneralLBOS::LbosExist__ShouldWork();
        #endif
    }


    /**  At initialization if no LBOS found, mapper must turn OFF            */
    void InitializationFail__TurnOff()
    {
        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        const char* service = "/lbos";
        s_call_counter = 0;

        FLBOS_FillCandidatesMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
        //should return nothing
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                       s_FakeFillCandidates<0>;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        NCBITEST_CHECK_MESSAGE(iter == NULL,
                               "LBOS found when it should not be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 0,
                               "LBOS has not been shut down as it should be");

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
        SERV_Close(iter);
    }


    /**  At initialization if LBOS found, mapper should be ON                */
    void InitializationSuccess__StayOn()
    {
        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        const char* service = "/lbos";
        s_call_counter = 0;

        FLBOS_FillCandidatesMethod* temp_func_pointer =
            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
        //should return something
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                                       s_FakeFillCandidates<1>;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        NCBITEST_CHECK_MESSAGE(iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 1,
                               "LBOS is not turned ON as it should be");

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
        SERV_Close(iter);
    }


    /** If LBOS has not yet been initialized, it should be initialized at
     * SERV_LBOS_Open()                                                      */
    void OpenNotInitialized__ShouldInitialize()
    {
        FLBOS_Initialize* temp_func_pointer =
            g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize;
        g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = s_FakeInitialize;

        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        const char* service = "/lbos";
        s_call_counter = 0;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
            "Initialization was not called when it should be");

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = temp_func_pointer;
        SERV_Close(iter);
    }


    /**  If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open()         */
    void OpenWhenTurnedOff__ReturnNull()
    {
        *(g_LBOS_UnitTesting_InitStatus()) = 1;
        *(g_LBOS_UnitTesting_PowerStatus()) = 0;
        const char* service = "/lbos";
        s_call_counter = 0;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        NCBITEST_CHECK_MESSAGE(iter == NULL,
            "SERV_LBOS_Open did not return NULL when it is disabled");

        /* Cleanup */
        *(g_LBOS_UnitTesting_PowerStatus()) = 1;
        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        SERV_Close(iter);
    }


    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_Initialize()                                                  */
    void s_LBOS_Initialize__s_LBOS_InstancesListNotNULL()
    {
        FLBOS_Initialize* temp_func_pointer =
                                 g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize;
        g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize =
                                                s_FakeInitializeCheckInstances;

        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        *(g_LBOS_UnitTesting_PowerStatus()) = 1;
        const char* service = "/lbos";
        s_call_counter = 0;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                      "Fake initialization was not called when it should be");

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = temp_func_pointer;
        SERV_Close(iter);
    }


    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_FillCandidates()                                              */
    void s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL()
    {
        FLBOS_FillCandidatesMethod* temp_func_pointer =
                             g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates;
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates =
                                            s_FakeFillCandidatesCheckInstances;

        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        *(g_LBOS_UnitTesting_PowerStatus()) = 1;
        const char* service = "/lbos";
        s_call_counter = 0;

        SERV_ITER iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);

        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
            "Fill candidates was not called when it should be");

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
        SERV_Close(iter);
    }


    /** Template parameter instance_num
     *   LBOS instance with which number (in original order as from
     *   g_LBOS_GetLBOSAddresses()) is emulated to be working
     *  Template parameter testnum
     *   Number of test. Used for debug output in multithread tests
     *  Template parameter predictable_leader
     *   If test is run in mode which allows to predict first address. It is
     *   either single threaded mode or test_mt with synchronization points
     *   between different tests                                             */
    template<int instance_num, int testnum, bool predictable_first>
    void SwapAddressesTest()
    {
        s_call_counter = 0;
        /* <Debugging> */
        int test = instance_num - 1;
        test++;
        bool pred_first = !predictable_first;
        pred_first = !pred_first;
        /* </Debugging> */
        const char* service = "/lbos";
        SConnNetInfo* net_info = ConnNetInfo_Create(service);
        SERV_ITER iter;
        char** lbos_addresses = g_LBOS_GetLBOSAddresses();
        int addresses_count = 0;
        while (lbos_addresses[++addresses_count] != NULL) continue;
        if (addresses_count < instance_num) return;
        char** addresses_control_list = new char*[addresses_count + 1];
        for (int i = 0; i < addresses_count; i++) {
            addresses_control_list[i] = strdup(lbos_addresses[i]);
        }
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort =
            s_FakeResolveIPPortSwapAddresses < instance_num > ;
        s_call_counter = 0;

        /* We will check results after open */
        iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            net_info, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);

        /* We launch */
        CORE_LOCK_READ;
        {
            for (int i = 0; i < addresses_count; i++) {
                lbos_addresses[i] =
                    strdup(g_LBOS_UnitTesting_InstancesList()[i]);
            }
        }
        CORE_UNLOCK;

        if (predictable_first) {
            CORE_LOGF(eLOG_Warning, (
                "Test %d. Expecting `%s', reality '%s'",
                testnum,
                addresses_control_list[instance_num - 1], lbos_addresses[0]));
            NCBITEST_CHECK_MESSAGE(
                strcmp(lbos_addresses[0],
                       addresses_control_list[instance_num - 1]) == 0,
                "priority LBOS instance error");
        }
        /* Actually, there can be duplicates, and that is why it is commented
           now and not completely deleted. The problem with this test is that
           there can be duplicates even in etc/ncbi/{role, domain} and
           registry. No problems were found with duplicates. */
        /*
        for (int i = 0; i < addresses_count; i++) {
            for (int j = i + 1; j < addresses_count; j++) {
                if (lbos_addresses[i] == NULL ||
                    lbos_addresses[j] == NULL) {
                    NCBITEST_CHECK_MESSAGE(
                        lbos_addresses[i] != lbos_addresses[j],
                        "Duplicate NULL encountered after"
                        " swapping algorithm");
                } else {
                    NCBITEST_CHECK_MESSAGE(
                        strcmp(lbos_addresses[i], lbos_addresses[j]) != 0,
                        "Duplicate after swapping algorithm");
                }
            }
        }*/

        SERV_Close(iter);
    }
    /** @brief
     * s_LBOS_FillCandidates() should switch first and good LBOS addresses,
     * if first is not responding
     *
     *  Our fake fill candidates designed that way that we can tell when to
     *  tell nothing and when to tell good through template parameter. Of
     *  course, we have to hardcode every value, but there are 7 max possible,
     *  not that much       */
    inline void PrimaryLBOSInactive__SwapAddresses()
    {
        // We need to first initialize mapper
        const char* service = "/lbos";
        SERV_ITER iter;
        /* We will check results after open */
        iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
            (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/);
        SERV_Close(iter);


        *(g_LBOS_UnitTesting_InitStatus()) = 0;
        *(g_LBOS_UnitTesting_PowerStatus()) = 1;
        FLBOS_ResolveIPPortMethod* temp_func_pointer =
            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort;

        /* Pseudo random order */
        SwapAddressesTest<1, 1, false>();
        SwapAddressesTest<2, 2, false>();
        SwapAddressesTest<3, 3, false>();
        SwapAddressesTest<1, 4, false>();
        SwapAddressesTest<7, 5, false>();
        SwapAddressesTest<2, 6, false>();
        SwapAddressesTest<4, 7, false>();
        SwapAddressesTest<1, 8, false>();
        SwapAddressesTest<2, 9, false>();
        SwapAddressesTest<3, 10, false>();
        SwapAddressesTest<1, 11, false>();
        SwapAddressesTest<7, 12, false>();
        SwapAddressesTest<2, 13, false>();
        SwapAddressesTest<4, 14, false>();
        SwapAddressesTest<2, 15, false>();
        SwapAddressesTest<6, 16, false>();
        SwapAddressesTest<3, 17, false>();
        SwapAddressesTest<5, 18, false>();
        SwapAddressesTest<2, 19, false>();
        SwapAddressesTest<1, 20, false>();
        SwapAddressesTest<6, 21, false>();
        SwapAddressesTest<4, 22, false>();
        SwapAddressesTest<2, 23, false>();
        SwapAddressesTest<6, 24, false>();
        SwapAddressesTest<3, 25, false>();
        SwapAddressesTest<5, 26, false>();
        SwapAddressesTest<2, 27, false>();
        SwapAddressesTest<1, 28, false>();
        SwapAddressesTest<6, 29, false>();
        SwapAddressesTest<4, 30, false>();

        /* Cleanup */
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort = temp_func_pointer;
    }
} /* namespace Initialization */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Stability
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void GetNext_Reset__ShouldNotCrash();
void FullCycle__ShouldNotCrash();


void GetNext_Reset__ShouldNotCrash()
{
    int secondsBeforeStop = 10;  /* when to stop this test */
    struct ::timeval start; /**< we will measure time from start
                                   of test as main measure of when
                                   to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    const SSERV_Info* info = NULL;
    int i = 0;

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
    CORE_LOGF(eLOG_Trace, ("Stability test 1:  %d iterations\n", i));
}

void FullCycle__ShouldNotCrash()
{
    int secondsBeforeStop = 10; /* when to stop this test */
    double elapsed = 0.0;
    struct timeval start; /**< we will measure time from start of test as main
                                           measure of when to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    const SSERV_Info* info = NULL;
    int i = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");

    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
         CORE_LOGF(eLOG_Trace, ("Stability test 2: iteration %d, "
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
    CORE_LOGF(eLOG_Trace, ("Stability test 2:  %d iterations\n", i));
}
} /* namespace Stability */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Performance
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void FullCycle__ShouldNotCrash();

void FullCycle__ShouldNotCrash()
{
    int                 secondsBeforeStop = 10;  /* when to stop this test */
    double              total_elapsed     = 0.0;
    double              cycle_elapsed     = 0.0; /**< we check performance
                                                      every second */
    struct timeval      start;                   /**< we will measure time from
                                                      start of test as main
                                                      measure of when to
                                                      finish */
    struct timeval      cycle_start;             /**< to measure start of
                                                      current cycle */
    struct timeval      stop;                    /**< To check time at
                                                      the end of each
                                                      iterations*/
    const char*         service            =  "/lbos";
    SConnNetInfo*       net_info;
    SERV_ITER           iter;
    const SSERV_Info*   info;
    int                 total_iters        = 0;  /**< Total number of full
                                                      iterations since start of
                                                      test */
    int                 cycle_iters        = 0;  /**< Total number of full
                                                      iterations since start
                                                      of one second cycle */
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
                                                         per one second cycle*/
    int                 min_hosts_per_cycle = INT_MAX; /**< Minimum hosts found
                                                      per one second cycle */
    /*
     * Basic initialization
     */
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
        CORE_LOGF(eLOG_Trace, ("Performance test: iteration %d, "
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
        CORE_LOGF(eLOG_Trace,  ("Found %d hosts", hosts_found)  );
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
    CORE_LOGF(eLOG_Trace, ("Performance test:\n"
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
#define LIST_OF_FUNCS                                                         \
X(01,Compose_LBOS_address::LBOSExists__ShouldReturnLbos)                      \
X(04,Reset_iterator::NoConditions__IterContainsZeroCandidates)                \
X(05,Reset_iterator::MultipleReset__ShouldNotCrash)                           \
X(06,Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash)               \
X(07,Close_iterator::AfterOpen__ShouldWork)                                   \
X(08,Close_iterator::AfterReset__ShouldWork)                                  \
X(09,Close_iterator::AfterGetNextInfo__ShouldWork)                            \
X(10,Close_iterator::FullCycle__ShouldWork)                                   \
X(11,Resolve_via_LBOS::ServiceExists__ReturnHostIP)                           \
X(12,Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL)                       \
X(13,Resolve_via_LBOS::NoLBOS__ReturnNULL)                                    \
X(17,Get_LBOS_address::                                                       \
               CustomHostNotProvided__SkipCustomHost)                         \
X(27,GetNextInfo::IterIsNull__ReconstructData)                                \
X(28,GetNextInfo::WrongMapper__ReturnNull)                                    \
X(29,Stability::GetNext_Reset__ShouldNotCrash)                                \
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



unsigned short s_Msb(unsigned short x)
{
    unsigned int y;
    while ((y = x & (x - 1)) != 0)
        x = y;
    return x;
}


const char* s_OS(TNcbiOSType ostype)
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


const char* s_Bits(TNcbiCapacity capacity)
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

int s_GetTimeOfDay(struct timeval* tv)
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


double s_TimeDiff(const struct timeval* end,
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


void s_PrintInfo(HOST_INFO hinfo)
{
    const char kTimeFormat[] = "%m/%d/%y %H:%M:%S";
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


void s_TestFindMethod(ELBOSFindMethod find_method)
{
    const char* service = "/lbos";
    SConnNetInfo* net_info = NULL;
    const SSERV_Info* info = NULL;
    struct timeval start;
    int n_found = 0;
    SERV_ITER iter = NULL;

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
    if (iter == NULL) {
        NCBITEST_CHECK_MESSAGE(iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of LBOS address now
     */
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov";
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
EIO_Status s_FakeReadDiscoveryCorrupt(CONN conn,
                           void* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(sizeof(char), 100));
        NCBITEST_CHECK_MESSAGE(buf != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (buf == NULL) return eIO_Closed;
        sprintf (buf,
                 "STANDALONE %0d.0%d.0%d.0%d:%d Regular R=1000.0 L=yes T=30\n",
                 s_call_counter, s_call_counter+1,
                 s_call_counter+2, s_call_counter+3, s_call_counter*215);
        strncat(static_cast<char*>(line), buf, size-1);
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
EIO_Status s_FakeReadDiscovery(CONN conn,
                           void* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(sizeof(char), 100));
        NCBITEST_CHECK_MESSAGE(buf != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (buf == NULL) return eIO_Closed;
        sprintf (buf,
                 "STANDALONE %d.%d.%d.%d:%d Regular R=1000.0 L=yes T=30\n",
                 s_call_counter, s_call_counter+1,
                 s_call_counter+2, s_call_counter+3, s_call_counter*215);
        strncat(static_cast<char*>(line), buf, size-1);
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
EIO_Status s_FakeReadAnnouncement(CONN conn,
                                  void* line,
                                  size_t size,
                                  size_t* n_read,
                                  EIO_ReadMethod how)
{
    /* We use conn's  r_pos to remember how many strings we have written */
    if (s_call_counter++ < lines) {
        char* buf = static_cast<char*>(calloc(sizeof(char), 100));
        sprintf (buf, "1.2.3.4:5");
        strncat(static_cast<char*>(line), buf, size-1);
        *n_read = strlen(buf);
        free(buf);
        return eIO_Success;
    } else {
        *n_read = 0;
        return eIO_Closed;
    }
}


template<int count>
void s_FakeFillCandidates (SLBOS_Data* data,
                           const char* service)
{
    s_call_counter++;
    unsigned int host = 0;
    unsigned short int port = 0;
    data->n_cand = count;
    data->pos_cand = 0;
    data->cand = static_cast<SLBOS_Candidate*>(calloc(sizeof(SLBOS_Candidate),
                                                      data->n_cand + 1));
    NCBITEST_CHECK_MESSAGE(data->cand != NULL,
        "Problem with memory allocation, "
        "calloc failed. Not enough RAM?");
    if (data->cand == NULL) return;
    char* hostport = static_cast<char*>(calloc(sizeof(char), 100));
    NCBITEST_CHECK_MESSAGE(hostport != NULL,
        "Problem with memory allocation, "
        "calloc failed. Not enough RAM?");
    if (hostport == NULL) return;
    for (int i = 0;  i < count;  i++) {
        sprintf(hostport, "%d.%d.%d.%d:%d", i+1, i+2, i+3, i+4, (i+1)*210);
        SOCK_StringToHostPort(hostport, &host, &port);
        data->cand[i].info = static_cast<SSERV_Info*>(
                calloc(sizeof(SSERV_Info), 1));
        NCBITEST_CHECK_MESSAGE(data->cand[i].info != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (data->cand[i].info == NULL) return;
        data->cand[i].info->host = host;
        data->cand[i].info->port = port;
    }
    free(hostport);
}


void s_FakeFillCandidatesCheckInstances(SLBOS_Data* data,
    const char* service)
{
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_InstancesList() != NULL,
        "s_LBOS_InstancesList is empty at a critical place");
    s_FakeFillCandidates<2>(data, service);
}


void s_FakeFillCandidatesWithError (SLBOS_Data* data, const char* service)
{
    s_call_counter++;
    return;
}


/** This version works only on specific call number to emulate working and 
 * non-working LBOS instances                                                */
template<int instance_number>
SSERV_Info** s_FakeResolveIPPortSwapAddresses(const char* lbos_address,
                                const char* serviceName,
                                SConnNetInfo* net_info)
{
    s_call_counter++;
    char** original_list = g_LBOS_GetLBOSAddresses();
    SSERV_Info** hostports = NULL;
    if (strcmp(lbos_address, original_list[instance_number-1]) == 0) {
        if (net_info->http_user_header) {
            s_last_header = static_cast<char*>(calloc(sizeof(char),
                strlen(net_info->http_user_header) + 1));
            strcat(s_last_header, net_info->http_user_header);
        }
        hostports = static_cast<SSERV_Info**>(
            calloc(sizeof(SSERV_Info*), 2));
        NCBITEST_CHECK_MESSAGE(hostports != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (hostports == NULL) return NULL;
        hostports[0] = static_cast<SSERV_Info*>(calloc(sizeof(SSERV_Info), 1));
        unsigned int host = 0;
        unsigned short int port = 0;
        SOCK_StringToHostPort("127.0.0.1:80", &host, &port);
        hostports[0]->host = host;
        hostports[0]->port = port;
        hostports[1] = NULL;
    }
    for (int i = 0; original_list[i] != NULL; i++) {
        free(original_list[i]);
    }
    free(original_list);
    return   hostports;
}

void s_FakeInitialize()
{
    s_call_counter++;
    return;
}



void s_FakeInitializeCheckInstances()
{
    s_call_counter++;
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_InstancesList() != NULL,
        "s_LBOS_InstancesList is empty at a critical place");
    return;
}



SSERV_Info** s_FakeResolveIPPort (const char* lbos_address,
                                const char* serviceName,
                                SConnNetInfo* net_info)
{
    s_call_counter++;
    if (net_info->http_user_header) {
        s_last_header = static_cast<char*>(calloc(sizeof(char),
                                      strlen(net_info->http_user_header) + 1));
        strcat(s_last_header, net_info->http_user_header);
    }
    if (s_call_counter < 2) {
        return NULL;
    } else {
        SSERV_Info** hostports = static_cast<SSERV_Info**>(
                calloc(sizeof(SSERV_Info*), 2));
        NCBITEST_CHECK_MESSAGE(hostports != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (hostports == NULL) return NULL;
        hostports[0] = static_cast<SSERV_Info*>(calloc(sizeof(SSERV_Info), 1));
        unsigned int host = 0;
        unsigned short int port = 0;
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
char* s_FakeComposeLBOSAddress()
{
    return strdup("lbos.foo");
}


#ifdef ANNOUNCE_DEANNOUNCE_TEST
int s_CountServers(const char * service, unsigned int host)
{
    int servers = 0;
    SConnNetInfo* net_info = NULL;
    SERV_ITER iter = NULL;
    const SSERV_Info* info;

    net_info = ConnNetInfo_Create(service);
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    do {
        info = SERV_GetNextInfoEx(iter, NULL);
        if (info != NULL && info->host == host)
            servers++;
    } while (info != NULL);
    return servers - 1;
}


int FakeDeannounce (const char* lbos_hostport,
        const char* service,
        const char* version,
        unsigned short port,
        const char* ip) {
    s_LBOS_hostport = strdup(lbos_hostport);
    return eLBOSAnnounceResult_Success;
}
#endif


#endif /* CONNECT___TEST_NCBI_LBOS__HPP*/
