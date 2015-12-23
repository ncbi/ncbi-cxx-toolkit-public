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
#include <sstream>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_lbos.hpp>
#include "../ncbi_lbosp.hpp"
#include <connect/server.hpp>
#include <util/random_gen.hpp>
/*C*/
#include "../ncbi_ansi_ext.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_lbosp.h"

//#define BOOST_AUTO_TEST_MAIN
/*std*/
#include <locale.h>
#define _CRT_RAND_S
#include <stdlib.h>
#undef _CRT_RAND_S
#include <time.h>
#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#else
#  include <sys/time.h>
#endif

/* Boost Test Framework or test_mt */
#ifdef LBOS_TEST_MT
#   undef  NCBITEST_CHECK_MESSAGE
#   define NCBITEST_CHECK_MESSAGE(P,M)                                        \
        NCBI_ALWAYS_ASSERT(P, M)
#   undef  NCBITEST_REQUIRE_MESSAGE
#   define NCBITEST_REQUIRE_MESSAGE(P,M)  NCBI_ALWAYS_ASSERT(P,M)
#   undef  BOOST_CHECK_EXCEPTION
#   define BOOST_CHECK_EXCEPTION(S,E,P) \
     do {                                                                     \
        try {                                                                 \
            S;                                                                \
        }                                                                     \
        catch (const E& ex) {                                                 \
            NCBITEST_CHECK_MESSAGE(                                           \
               P(ex),                                                         \
               "LBOS exception contains wrong error type");                   \
            break;                                                            \
        }                                                                     \
        catch (...) {                                                         \
            NCBITEST_CHECK_MESSAGE(false,                                     \
                                   "Unexpected exception was thrown");        \
        }                                                                     \
        NCBITEST_CHECK_MESSAGE(false,                                         \
                               "No exception was thrown");                    \
    } while (false);                                                          
    //because assert and exception are the same
#   undef  BOOST_CHECK_NO_THROW
#   define BOOST_CHECK_NO_THROW(S)  S
#   undef  NCBITEST_CHECK_EQUAL
#   define NCBITEST_CHECK_EQUAL(S,E)                                          \
    do {                                                                      \
        stringstream ss, lh_str, rh_str;                                      \
        lh_str << S;                                                          \
        rh_str << E;                                                          \
        ss << " (" << #S << " != " << #E <<  ")"                              \
           << " (" << lh_str.str() << " != " << rh_str.str() <<  ")";         \
        NCBITEST_CHECK_MESSAGE(lh_str.str() == rh_str.str(),                  \
                                   ss.str().c_str());                         \
    } while(false);

#   undef  NCBITEST_CHECK_NE
#   define NCBITEST_CHECK_NE(S,E)                                             \
    do {                                                                      \
           stringstream ss;                                                   \
           ss << " (" << S << " == " << E <<  ")";                            \
           NCBITEST_CHECK_MESSAGE(S != E, ss.str().c_str());                  \
    } while(false);
#   undef  NCBITEST_REQUIRE_NE
#   define NCBITEST_REQUIRE_NE(S,E)     NCBITEST_CHECK_NE(S,E)
#else  /* if LBOS_TEST_MT not defined */
//  This header must be included before all Boost.Test headers
#   include <corelib/test_boost.hpp>
#endif /* #ifdef LBOS_TEST_MT */


/* LBOS returns 500 on announcement unexpectedly */
//#define QUICK_AND_DIRTY

/* We might want to clear ZooKeeper from nodes before running tests.
* This is generally not good, because if this test application runs
* on another host at the same moment, it will miss a lot of nodes and
* tests will fail.
*/
#define DEANNOUNCE_ALL_BEFORE_TEST

/*test*/
#include "test_assert.h"


USING_NCBI_SCOPE;


/* First let's declare some functions that will be
 * used in different test suites. It is convenient
 * that their definitions are at the very end, so that
 * test config is as high as possible */

static void            s_PrintInfo                  (HOST_INFO);
static void            s_TestFindMethod             (ELBOSFindMethod);
static string          s_PrintThreadNum             (int);
static void          s_PrintAnnouncedServers      ();

/** Return a priori known LBOS address */
template <unsigned int lines>
static char*           s_FakeComposeLBOSAddress     (void);
#ifdef NCBI_OS_MSWIN
static int             s_GetTimeOfDay               (struct timeval*);
#else
#   define             s_GetTimeOfDay(tv)            gettimeofday(tv, 0)
#endif
static unsigned short  s_Msb                        (unsigned short);
static const char*     s_OS                         (TNcbiOSType);
static const char*     s_Bits                       (TNcbiCapacity);
/** Count difference between two timestamps, in seconds*/
static double          s_TimeDiff                   (const struct timeval*,
                                                     const struct timeval*);
static string          s_GenerateNodeName           (void);
static unsigned short  s_GeneratePort               (int thread_num);
static const int       kThreadsNum                  = 34;
static bool            s_CheckIfAnnounced           (string         service,
                                                     string         version,
                                                     unsigned short server_port,
                                                     string      health_suffix,
                                                     bool      expectedAnnounced 
                                                                         = true,
                                                     string          host = "");
/** Static variables that are used in mock functions.
 * This is not thread-safe! */
static int             s_CallCounter                = 0;
/** It is yet impossible on Teamcity, but useful for local tests, where 
   local LBOS can be easily run                                              */
static string          s_LastHeader;
/** When the test is run in single-threaded mode, we set the number of the 
 * main thread to -1 to distinguish betwenn MT and ST    */
const int              kSingleThreadNumber          = -1;
/* Seconds to try to find server. We wait maximum of 60 seconds. */
const int              kDiscoveryDelaySec = 60; 
const unsigned short   kDefaultPort                 = 5000; /* for tests where
                        port is not necessary (they fail before announcement) */
/* msecs to wait after announcement.de-announcement */

#define PORT 8085 /* port for healtcheck */
#define PORT_STR_HELPER(port) #port
#define PORT_STR(port) PORT_STR_HELPER(port)

#include "test_ncbi_lbos_mocks.hpp"

#define WRITE_LOG(text,thread_num)                                             \
        LOG_POST(s_PrintThreadNum(thread_num) << "\t" <<                       \
                 __FILE__ << "\t" << __LINE__ << "\t" << text);

static void s_PrintAnnouncementDetails(const char* name,
                                       const char* version,
                                       unsigned short port,
                                       const char* health,
                                       int thread_num)
{
    WRITE_LOG("Announcing server \"" <<
                  (name ? name : "<NULL>") <<
                  "\" with version " << (version ? version : "<NULL>") <<
                  ", port " << port << ", ip " << s_GetMyIP() << 
                  ", healthcheck \"" << (health ? health : "<NULL>") << "\"", 
              thread_num);
}

static void s_PrintAnnouncedDetails(const char* lbos_ans, 
                                    const char* lbos_mes, 
                                    unsigned short result,
                                    double time_elapsed,
                                    int thread_num)
{
    WRITE_LOG("Announce returned code " << result <<
                " after " << time_elapsed << " seconds, " 
                "LBOS status message: \"" <<
                (lbos_mes ? lbos_mes : "<NULL>") <<
                "\", LBOS answer: \"" <<
                (lbos_ans ? lbos_ans : "<NULL>") << "\"", thread_num);
}

static void s_PrintRegistryAnnounceParams(const char* registry_section,
    int thread_num)
{
    const char* kLBOSAnnouncementSection = "LBOS_ANNOUNCEMENT";
    const char* kLBOSServiceVariable = "SERVICE";
    const char* kLBOSVersionVariable = "VERSION";
    const char* kLBOSHostVariable = "HOST";
    const char* kLBOSPortVariable = "PORT";
    const char* kLBOSHealthcheckUrlVariable = "HEALTHCHECK";
    if (g_LBOS_StringIsNullOrEmpty(registry_section)) {
        registry_section = kLBOSAnnouncementSection;
    }
    CCObjHolder<char> srvc(g_LBOS_RegGet(registry_section, kLBOSServiceVariable, NULL));
    CCObjHolder<char> vers(g_LBOS_RegGet(registry_section, kLBOSVersionVariable, NULL));
    CCObjHolder<char> host(g_LBOS_RegGet(registry_section, kLBOSHostVariable, NULL));
    CCObjHolder<char> port_str(g_LBOS_RegGet(registry_section, kLBOSPortVariable, NULL));
    CCObjHolder<char> hlth(g_LBOS_RegGet(registry_section, kLBOSHealthcheckUrlVariable, NULL));
    WRITE_LOG("Announcing server \"" <<
        (*srvc ? *srvc : "<NULL>") << "\" from registry section " <<
        (registry_section ? registry_section : "<NULL>") <<
        " with version " << (*vers ? *vers : "<NULL>") <<
        " with host " << (*host ? *host : "<NULL>") <<
        ", port " << (*port_str ? *port_str : "<NULL>") <<
        ", ip " << s_GetMyIP() <<
        ", healthcheck \"" << (*hlth ? *hlth : "<NULL>") << "\"",
        thread_num);
}


#define MEASURE_TIME_START                                                     \
    struct timeval      time_start;     /**< to measure start of               \
                                                 announcement */               \
    struct timeval      time_stop;                    /**< To check time at    \
                                                 the end of each               \
                                                 iteration*/                   \
    double              time_elapsed = 0.0; /**< difference between start      \
                                                 and end time */               \
    if (s_GetTimeOfDay(&time_start) != 0) { /*  Initialize time of             \
                                                 iteration start*/             \
        memset(&time_start, 0, sizeof(time_start));                            \
    }


#define MEASURE_TIME_FINISH                                                    \
    if (s_GetTimeOfDay(&time_stop) != 0)                                       \
        memset(&time_stop, 0, sizeof(time_stop));                              \
    time_elapsed = s_TimeDiff(&time_stop, &time_start);    


/** Announce using C interface. If annoucement finishes with error - return 
 * error */
static unsigned short s_AnnounceC(const char*       name,
                                  const char*       version,
                                  const char*       host,
                                  unsigned short    port,
                                  const char*       health,
                                  char**            lbos_ans,
                                  char**            lbos_mes,
                                  int               thread_num)
{       
    unsigned short result;
    MEASURE_TIME_START
    if (health != NULL) {
        stringstream healthcheck;
        healthcheck << health << "/port" << port << 
                       "/host" << (host ? host : "") <<
                       "/version" << (version ? version : "");
        s_PrintAnnouncementDetails(name, version, port, 
                                   healthcheck.str().c_str(),
                                   thread_num);
        result = LBOS_Announce(name, version, host, port, 
                               healthcheck.str().c_str(), lbos_ans, lbos_mes);
    } else {
        s_PrintAnnouncementDetails(name, version, port, 
                                   NULL, thread_num);
        result = LBOS_Announce(name, version, host, port, 
                               NULL, lbos_ans, lbos_mes);
    }
    MEASURE_TIME_FINISH
    s_PrintAnnouncedDetails(lbos_ans ? *lbos_ans : NULL,
                            lbos_mes ? *lbos_mes : NULL,
                            result, time_elapsed, thread_num);
    return result;
}
        

/** Announce using C interface. If announcement finishes with error - try again
 * until success (with invalid parameters means infinite loop) */
static unsigned short s_AnnounceCSafe(const char*       name,
                                       const char*      version,
                                       const char*      host,
                                       unsigned short   port,
                                       const char*      health,
                                       char**           lbos_ans,
                                       char**           lbos_mes,
                                       int              thread_num)
{       
    unsigned short result;
    MEASURE_TIME_START
    if (health != NULL) {
        stringstream healthcheck;
        healthcheck << health << "/port" << port << 
                       "/host" << (host ? host : "") <<
                       "/version" << (version ? version : "");
        s_PrintAnnouncementDetails(name, version, port, 
                                   healthcheck.str().c_str(), 
                                   thread_num);
        result = s_LBOS_Announce(name, version, host, port, 
                                 healthcheck.str().c_str(), 
                                 lbos_ans, lbos_mes);
    } else {
        s_PrintAnnouncementDetails(name, version, port, NULL, thread_num);
        result = s_LBOS_Announce(name, version, host, port, NULL, 
                                 lbos_ans, lbos_mes);
    }    
    MEASURE_TIME_FINISH
    s_PrintAnnouncedDetails(lbos_ans ? *lbos_ans : NULL,
                            lbos_mes ? *lbos_mes : NULL,
                            result, time_elapsed, thread_num);
    return result;
}


/** Announce using C interface and using values from registry. If announcement 
 * finishes with error - try again until success (with invalid parameters means
 * infinite loop) */
static unsigned short s_AnnounceCRegistry(const char*   registry_section,
                                          char**        lbos_ans,
                                          char**        lbos_mes,
                                          int           thread_num)
{
    unsigned short result;
    s_PrintRegistryAnnounceParams(registry_section, thread_num);
    MEASURE_TIME_START
        result = s_LBOS_AnnounceReg(registry_section, lbos_ans, lbos_mes);
    MEASURE_TIME_FINISH
    s_PrintAnnouncedDetails(lbos_ans ? *lbos_ans : NULL,
                            lbos_mes ? *lbos_mes : NULL,
                            result, time_elapsed, thread_num);
    return result;
}


/** Announce using C++ interface. If annoucement finishes with error - return
 * error */
static void s_AnnounceCPP(const string& name,
                          const string& version,
                          const string& host,
                          unsigned short port,
                          const string& health,
                          int thread_num)
{
    s_PrintAnnouncementDetails(name.c_str(), version.c_str(), port, 
                               health.c_str(), thread_num);
    stringstream healthcheck;
    healthcheck << health << "/port" << port << "/host" << host <<
                   "/version" << version;
    MEASURE_TIME_START
    try {
        LBOS::Announce(name, version, host, port, healthcheck.str());
    }
    catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
        s_PrintAnnouncedDetails(ex.what(), ex.GetErrCodeString(),
                                ex.GetErrCode(), time_elapsed, thread_num);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintAnnouncedDetails("<C++ does not show answer from LBOS>",
                            "OK", 200, time_elapsed, thread_num);
}


/** Announce using C++ interface. If announcement finishes with error - try 
 * again until success (with invalid parameters means infinite loop) */
static void s_AnnounceCPPSafe(const string& name,
    const string& version,
    const string& host,
    unsigned short port,
    const string& health,
    int thread_num)
{
    s_PrintAnnouncementDetails(name.c_str(), version.c_str(), port,
        health.c_str(), thread_num);
    stringstream healthcheck;
    healthcheck << health << "/port" << port << "/host" << host <<
                   "/version" << version;
    MEASURE_TIME_START
        s_LBOS_CPP_Announce(name, version, host, port, healthcheck.str());
    MEASURE_TIME_FINISH
        /* Print good result */
        s_PrintAnnouncedDetails("<C++ does not show answer from LBOS>",
        "OK", 200, time_elapsed, kSingleThreadNumber);
}


/** Announce using C++ interface and using values from registry. If 
 * announcement finishes with error - try again until success (with invalid 
 * parameters means infinite loop) */
static void s_AnnounceCPPFromRegistry(const string& registry_section)
{
    s_PrintRegistryAnnounceParams(registry_section.c_str(),
                                  kSingleThreadNumber);
    MEASURE_TIME_START
    try {
        s_LBOS_CPP_AnnounceReg(registry_section);
    }
    catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
        s_PrintAnnouncedDetails(ex.what(), ex.GetErrCodeString(),
                                ex.GetErrCode(), time_elapsed,
                                kSingleThreadNumber);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintAnnouncedDetails("<C++ does not show answer from LBOS>",
    "OK", 200, time_elapsed, kSingleThreadNumber);
}


/** Before deannounce */
static void s_PrintDeannouncementDetails(const char* name,
                                         const char* version,
                                         const char* host, 
                                         unsigned short port,
                                         int thread_num)
{
    WRITE_LOG("De-announcing server \"" <<
                    (name ? name : "<NULL>") <<
                    "\" with version " << (version ? version : "<NULL>") <<
                    ", port " << port << ", host \"" <<
                    (host ? host : "<NULL>") << "\", ip " << s_GetMyIP(), 
               thread_num);
}

/** Before deannounce */
static void s_PrintDeannouncedDetails(const char*       lbos_ans, 
                                      const char*       lbos_mess, 
                                      unsigned short    result,
                                      double            time_elapsed,
                                      int               thread_num)
{
    WRITE_LOG("De-announce returned code " << result <<
               " after " << time_elapsed << " seconds, "
               ", LBOS status message: \"" <<
               (lbos_mess ? lbos_mess : "<NULL>") <<
               "\", LBOS answer: \"" <<
               (lbos_ans ? lbos_ans : "<NULL>") << "\"", thread_num);
}

static unsigned short s_DeannounceC(const char*     name,
                                    const char*     version,
                                    const char*     host,
                                    unsigned short  port,
                                    char**          lbos_ans,
                                    char**          lbos_mes,
                                    int             thread_num)
{
    unsigned short result;
    s_PrintDeannouncementDetails(name, version, host, port, thread_num);
    MEASURE_TIME_START
    result = LBOS_Deannounce(name, version, host, port, lbos_ans, lbos_mes);  
    MEASURE_TIME_FINISH
    s_PrintDeannouncedDetails(lbos_ans ? *lbos_ans : NULL,
                              lbos_mes ? *lbos_mes : NULL,
                              result, time_elapsed, thread_num);
    return result;
}

static void s_DeannounceAll(int thread_num = kSingleThreadNumber)
{
    WRITE_LOG("De-announce all", thread_num);
    MEASURE_TIME_START;
        LBOS_DeannounceAll();
    MEASURE_TIME_FINISH;
    WRITE_LOG("De-announce all has finished and took " <<
               time_elapsed << " seconds.", thread_num);
}

static void s_DeannounceCPP(const string& name,
                            const string& version,
                            const string& host,
                            unsigned short port,
                            int thread_num)
{
    s_PrintDeannouncementDetails(name.c_str(), version.c_str(), host.c_str(), 
                                 port, thread_num);
    MEASURE_TIME_START
    try {
        LBOS::Deannounce(name, version, host, port);
    }
    catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
            s_PrintDeannouncedDetails(ex.what(), ex.GetErrCodeString(),
            ex.GetStatusCode(), time_elapsed, thread_num);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintDeannouncedDetails("<C++ does not show answer from LBOS>",
                              "OK", 200, time_elapsed, thread_num);
}


static void s_SelectPort(int&               count_before, 
                         const string&      node_name,
                         unsigned short&    port,
                         int                thread_num)
{
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    WRITE_LOG("Random port is " << port << ". "
        "Count of servers with this port is " <<
        count_before << ".",
        thread_num);
}


/** Check if expected number of specified servers is announced 
 * (usually it is 0 or 1)
 * This function does not care about host (IP) of server.
 */
int s_CountServersWithExpectation(string            service,
                                  unsigned short    port, 
                                  int               expected_count,
                                  int               secs_timeout, 
                                  int          thread_num = kSingleThreadNumber,
                                  string            dtab = "")
{
    WRITE_LOG("Counting number of servers \"" << service <<
                   "\" with dtab \"" << dtab << 
                   "\" and port " << port << ", ip " << s_GetMyIP() << 
                   " via service discovery "
                   "(expecting " << expected_count << " servers found).",
               thread_num);
    const int wait_time = 5000; /* msecs */
    int max_retries = secs_timeout * 1000 / wait_time; /* number of repeats 
                                                          until timeout */
    int retries = 0;
    int servers = 0;
    MEASURE_TIME_START
        while (servers != expected_count && retries < max_retries) {
            servers = 0;
            if (retries > 0) { /* for the first cycle we do not sleep */
                SleepMilliSec(wait_time);
            }
            const SSERV_Info* info;
            CConnNetInfo net_info(service);
            if (dtab.length() > 1) {
                ConnNetInfo_SetUserHeader(*net_info, dtab.c_str());
            }
            CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                *net_info, 0/*skip*/, 0/*n_skip*/,
                0/*external*/, 0/*arg*/, 0/*val*/));
            do {
                info = SERV_GetNextInfoEx(*iter, NULL);
                if (info != NULL && info->port == port)
                    servers++;
            } while (info != NULL);
            retries++;
        }
    MEASURE_TIME_FINISH
        WRITE_LOG("Found " << servers << " servers of service " << service << 
                  ", port " << port << " after " << time_elapsed <<
                  " seconds via service discovery.", thread_num);
    return servers;
}


static int  s_FindAnnouncedServer(string            service,
                                  string            version,
                                  unsigned short    port,
                                  string            host,
                                  int               thread_num)
{
    WRITE_LOG("Trying to find announced server in the storage\"" <<
                service << "\" with version 1.0.0 and port " <<
                port << ", ip " << host,
              thread_num);
    s_PrintAnnouncedServers();


    CLBOSStatus lbos_status(true, true);
    struct SLBOS_AnnounceHandle_Tag*& arr =
        *g_LBOS_UnitTesting_GetAnnouncedServers();
    unsigned int count = g_LBOS_UnitTesting_GetAnnouncedServersNum();
    unsigned int found = 0;
    /* Just iterate and compare */
    unsigned int i = 0;
    CORE_LOCK_READ;
    for (i = 0; i < count; i++) {
        if (strcasecmp(service.c_str(), arr[i].service) == 0
            &&
            strcasecmp(version.c_str(), arr[i].version) == 0
            &&
            strcasecmp(host.c_str(), arr[i].host) == 0
            &&
            arr[i].port == port)
        {
            found++;
        }
    }
    CORE_UNLOCK;
    WRITE_LOG("Found  " << found <<
        " servers in the inner LBOS storage", thread_num);
    return found;
}

/** To run tests of /configuration we need name of service that is not used yet.
*/
static bool s_CheckServiceKnown(string service)
{
    bool exists;
    LBOS::ServiceVersionGet(service, &exists);
    return exists;
}

static string s_GetUnknownService() {
    static string charset = "abcdefghijklmnopqrstuvwxyz"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "1234567890";
    const int length = 10;

    for (;;) {
        string result = "/";
        result.resize(length);
        for (int i = 1; i < length; i++)
            result[i] = charset[rand() % charset.length()];
        if (!s_CheckServiceKnown(result)) {
            return result;
        }
    }
    return ""; /* never reachable */
}

#ifdef NCBI_THREADS
class CHealthcheckThread : public CThread
{
public:
    CHealthcheckThread()
        : m_RunHealthCheck(true)
    {
        m_Busy = false;
    }
    void Stop() {
        m_RunHealthCheck = false;
    }
    /* Check if thread has answered all requests */
    bool IsBusy() {
        return m_Busy;
    }
protected:
    // As it is said in ncbithr.hpp, destructor must be protected
    ~CHealthcheckThread()
    {
    }
private:
    void* Main(void) {
        CListeningSocket listening_sock(PORT);
        CSocket sock;
        listening_sock.Listen(PORT);
        STimeout rw_timeout = { 0, 20000 };
        STimeout c_timeout = { 0, 0 };
        while (m_RunHealthCheck) {
            if (listening_sock.Accept(sock, &rw_timeout) != eIO_Success) {
                m_Busy = false;
                continue;
            }
            m_Busy = true;
            char buf[4096];
            size_t n_read = 0;
            size_t n_written = 0;
            sock.SetTimeout(eIO_ReadWrite, &rw_timeout);
            sock.SetTimeout(eIO_Close, &c_timeout);
            sock.Read(buf, sizeof(buf), &n_read);
            buf[n_read] = '\0';
            string request = buf;
            if (request.length() > 10) {
                request = request.substr(4, NPOS);
                request = request.erase(request.find("HTTP"), NPOS);
                WRITE_LOG("Answered healthcheck for " << request,
                          kSingleThreadNumber);
            }
            if (request == "/health" || request == "") {
                WRITE_LOG("Answered healthcheck for " << request, 
                          kSingleThreadNumber);
            }
            const char healthy_answer[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 4\r\n"
                "Content-Type: text/plain;charset=UTF-8\r\n"
                "\r\n"
                "OK\r\n";
            sock.Write(healthy_answer, sizeof(healthy_answer) - 1, 
                       &n_written);
            sock.Wait(eIO_Read, &c_timeout);
            sock.Close();
        }
        return NULL;
    }

    bool m_RunHealthCheck;
    bool m_Busy;
};
#endif /* NCBI_THREADS */

///////////////////////////////////////////////////////////////////////////////
//////////////               DECLARATIONS            //////////////////////////
///////////////////////////////////////////////////////////////////////////////
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ComposeLBOSAddress
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. All files OK - return composed LBOS address
 * 2. If could not read files, give up this industry and return NULL
 * 3. If could not read files, give up this industry and return NULL         */
void LBOSExists__ShouldReturnLbos();
void RoleFail__ShouldReturnNULL();
void DomainFail__ShouldReturnNULL();
} /* namespace GetLBOSAddress */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ResetIterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Should make capacity of elements in data->cand equal zero
 * 2. Should be able to reset iter N times consequently without crash
 * 3. Should be able to "reset iter, then getnextinfo" N times 
 *    consequently without crash                                             */
void NoConditions__IterContainsZeroCandidates();
void MultipleReset__ShouldNotCrash();
void Multiple_AfterGetNextInfo__ShouldNotCrash();
} /* namespace ResetIterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace CloseIterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Should work immediately after Open
 * 2. Should work immediately after Reset
 * 3. Should work immediately after Open, GetNextInfo
 * 4. Should work immediately after Open, GetNextInfo, Reset                 */
void AfterOpen__ShouldWork();
void AfterReset__ShouldWork();
void AfterGetNextInfo__ShouldWork();
void FullCycle__ShouldWork();
} /* namespace CloseIterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DTab
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
/* 1. Mix of registry DTab and HTTP Dtab: registry goes first */
{
void DTabRegistryAndHttp__RegistryGoesFirst();
void NonStandardVersion__FoundWithDTab();
}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ResolveViaLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if LBOS answered "not found"
 * 3. Should return NULL if LBOS is not reachable
 * 4. Should be able to support up to M IP:port combinations 
      (not checking for repeats) with storage overhead not more than same 
      as size needed (that is, all space consumed is twice as size needed, 
      used and unused space together)
 * 5. Should be able to skip answer of LBOS if it is corrupt or contains 
      not valid data                                                         */
void ServiceExists__ReturnHostIP();
void ServiceDoesNotExist__ReturnNULL();
void NoLBOS__ReturnNULL();
void FakeMassiveInput__ShouldProcess();
void FakeErrorInput__ShouldNotCrash();
} /* namespace ResolveViaLBOS */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetLBOSAddress
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If specific way to find LBOS is specified, try it first. If failed, 
      search LBOS's address in default order
 * 2. If custom host is specified as method but is not provided as value, 
      search LBOS's address in default order
 * 3. Default order is: search LBOS's address first in registry. If
      failed, try 127.0.0.1:8080. If failed, try /etc/ncbi/{role, domain}.   */
void SpecificMethod__FirstInResult();
void CustomHostNotProvided__SkipCustomHost();
void NoConditions__AddressDefOrder();
} /* namespace GetLBOSAddress */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetCandidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Iterate through received LBOS's addresses, if there is no response 
      from current LBOS
 * 2. If one LBOS works, do not try another LBOS
 * 3. If *net_info was provided for Serv_OpenP, the same *net_info should be
      available while getting candidates via LBOS to provide DTABs.          */
void LBOSNoResponse__SkipLBOS();
void LBOSResponds__Finish();
void NetInfoProvided__UseNetInfo();
} /* namespace GetCandidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If no candidates found yet, or reset was just made, get candidates 
      and return first
 * 2. If no candidates found yet, or reset was just made, and unrecoverable
      error while getting candidates, return 0
 * 3. If candidates already found, return next
 * 4. If last candidate was already returned, return 0
 * 5. If data is NULL for some reason, construct new data
 * 6. If iter is NULL, return NULL
 * 7. If SERV_MapperName(*iter) returns name of another mapper, return NULL   */
void EmptyCands__RunGetCandidates();
void ErrorUpdating__ReturnNull();
void HaveCands__ReturnNext();
void LastCandReturned__ReturnNull();
void DataIsNull__ReconstructData();
void IterIsNull__ReturnNull();
void WrongMapper__ReturnNull();
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If iter is NULL, return NULL
 * 2. If *net_info is NULL, construct own *net_info
 * 3. If read from LBOS successful, return s_op
 * 4. If read from LBOS successful and info pointer != NULL, write 
      first element NULL to info
 * 5. If read from LBOS unsuccessful or no such service, return 0            */
void IterIsNull__ReturnNull();
void NetInfoNull__ConstructNetInfo();
void ServerExists__ReturnLbosOperations();
void InfoPointerProvided__WriteNull();
void NoSuchService__ReturnNull();
} /* namespace Open */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GeneralLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If server exists in LBOS, it should be found and s_op should be 
 *    returned by SERV_OpenP()
 * 2. If server does not exist in LBOS, it should not be found and 
 *    SERV_OpenP() should return NULL 
 * 3. If most priority LBOS can be found, it should be used used for 
 *    resolution                                                             */
void ServerExists__ShouldReturnLbosOperations();
void ServerDoesNotExist__ShouldReturnNull();
void LbosExist__ShouldWork();
} /* namespace GeneralLBOS */



// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1. Successfully announced: return SUCCESS
 *  2. Successfully announced: SLBOS_AnnounceHandle deannounce_handle 
 *     contains info needed to later deannounce announced server
 *  3. Successfully announced: char* lbos_answer contains answer of LBOS
 *  4. Successfully announced: information about announcement is saved to 
 *     hidden LBOS mapper's storage
 *  5. Could not find LBOS: return NO_LBOS
 *  6. Could not find LBOS: char* lbos_answer is set to NULL
 *  7. Could not find LBOS: SLBOS_AnnounceHandle deannounce_handle is set to
 *     NULL
 *  8. LBOS returned error: return LBOS_ERROR
 *  9. LBOS returned error: char* lbos_answer contains answer of LBOS
 * 10. LBOS returned error: SLBOS_AnnounceHandle deannounce_handle is set to 
 *     NULL
 * 11. Server announced again (service name, IP and port coincide) and 
 *     announcement in the same zone, replace old info about announced 
 *     server in internal storage with new one.
 * 12. Server announced again and trying to announce in another 
 *     zone - return MULTIZONE_ANNOUNCE_PROHIBITED
 * 13. Was passed incorrect healthcheck URL (NULL or empty not starting with 
 *     "http(s)://"): do not announce and return INVALID_ARGS
 * 14. Was passed incorrect port (zero): do not announce and return 
 *     INVALID_ARGS
 * 15. Was passed incorrect version(NULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 16. Was passed incorrect service nameNULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 17. Real-life test: after announcement server should be visible to 
 *     resolve
 * 18. If was passed "0.0.0.0" as IP, should replace it with local IP or 
 *     hostname
 * 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host 
 *     IP: do not announce and return DNS_RESOLVE_ERROR
 * 20. LBOS is OFF - return kLBOSOff                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   
 * 22. Trying to announce server and providing dead healthcheck URL -
 *     return kLBOSNotFound       
 * 23. Trying to announce server and providing dead healthcheck URL -
 *     server should not be announced                                        
 * 24. Announce server with separate host and healtcheck - should be found in 
 *     %LBOS%/text/service                                                    */
void AllOK__ReturnSuccess(int thread_num);
void AllOK__DeannounceHandleProvided(int thread_num);
void AllOK__LBOSAnswerProvided(int thread_num);
void AllOK__AnnouncedServerSaved(int thread_num);
void NoLBOS__ReturnNoLBOSAndNotFind(int thread_num);
void NoLBOS__LBOSAnswerNull(int thread_num);
void NoLBOS__DeannounceHandleNull(int thread_num);
void LBOSErrorCode__ReturnServerErrorCode(int thread_num);
void LBOSError__LBOSAnswerProvided(int thread_num);
void LBOSError__DeannounceHandleNull(int thread_num);
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num);
void ForeignDomain__NoAnnounce(int thread_num);
void AlreadyAnnouncedInAnotherZone__ReturnMultizoneProhibited(int thread_num);
void IncorrectURL__ReturnInvalidArgs(int thread_num);
void IncorrectPort__ReturnInvalidArgs(int thread_num);
void IncorrectVersion__ReturnInvalidArgs(int thread_num);
void IncorrectServiceName__ReturnInvalidArgs(int thread_num);
void RealLife__VisibleAfterAnnounce(int thread_num);
void ResolveLocalIPError__ReturnDNSError(int thread_num);
void IP0000__ReplaceWithIP(int thread_num);
void LBOSOff__ReturnKLBOSOff(int thread_num);
void LBOSAnnounceCorruptOutput__ReturnServerError(int thread_num);
void HealthcheckDead__ReturnKLBOSSuccess(int thread_num);
void HealthcheckDead__AnnouncementOK(int thread_num);

}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void TestNullOrEmptyField(const char* field_tested);
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return kLBOSSuccess
    2.  Custom section has nothing in config - return kLBOSInvalidArgs
    3.  Section empty or NULL (should use default section and return 
        kLBOSSuccess)
    4.  Service is empty or NULL - return kLBOSInvalidArgs
    5.  Version is empty or NULL - return kLBOSInvalidArgs
    6.  port is empty or NULL - return kLBOSInvalidArgs
    7.  port is out of range - return kLBOSInvalidArgs
    8.  port contains letters - return kLBOSInvalidArgs
    9.  healthcheck is empty or NULL - return kLBOSInvalidArgs
    10. healthcheck does not start with http:// or https:// - return 
        kLBOSInvalidArgs                                                    */
void ParamsGood__ReturnSuccess() ;
void CustomSectionNoVars__ReturnInvalidArgs();
void CustomSectionEmptyOrNullAndDefaultSectionIsOk__ReturnSuccess();
void ServiceEmptyOrNull__ReturnInvalidArgs();
void VersionEmptyOrNull__ReturnInvalidArgs();
void PortEmptyOrNull__ReturnInvalidArgs();
void PortOutOfRange__ReturnInvalidArgs();
void PortContainsLetters__ReturnInvalidArgs();
void HealthchecktEmptyOrNull__ReturnInvalidArgs();
void HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs();
}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully de-announced : return 1
 * 2. Successfully de-announced : if announcement was saved in local storage, 
 *    remove it
 * 3. Could not connect to provided LBOS : fail and return 0
 * 4. Successfully connected to LBOS, but deannounce returned error : return 0
 * 5. Real - life test : after de-announcement server should be invisible 
 *    to resolve                                                            
 * 6. Another domain - do not deannounce 
 * 7. Deannounce without IP specified - deannounce from local host 
 * 8. LBOS is OFF - return kLBOSOff                                         */
void Deannounced__Return1(unsigned short port, int thread_num);
void Deannounced__AnnouncedServerRemoved(int thread_num);
void NoLBOS__Return0(int thread_num);
void LBOSExistsDeannounce404__Return404(int thread_num);
void RealLife__InvisibleAfterDeannounce(int thread_num);
void ForeignDomain__DoNothing(int thread_num);
void NoHostProvided__LocalAddress(int thread_num);
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in LBOS                              */
void AllDeannounced__NoSavedLeft(int thread_num);
}


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


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Configure
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
    /* 1. Set version the check version - should show the version that was just set */
    void SetThenCheck__ShowsSetVersion();
    /* 2. Check version, that set different version, then check version -
    *    should show new version */
    void CheckSetNewCheck__ChangesVersion();
    /* 3. Set version, check that it was set, then delete version - check
    *    that no version exists */
    void DeleteThenCheck__SetExistsFalse();
    /* 4. Announce two servers with different version. First set one version
    *    and discover server with that version. Then set the second version
    *    and discover server with that version. */
    void AnnounceThenChangeVersion__DiscoverAnotherServer();
    /* 5. Announce one server. Discover it. Then delete version. Try to
    *    discover it again, should not find.*/
    void AnnounceThenDeleteVersion__DiscoverFindsNothing();
    /* 6. Set with no service - invalid args */
    void SetNoService__InvalidArgs();
    /* 7. Get with no service - invalid args */
    void GetNoService__InvalidArgs();
    /* 8. Delete with no service - invalid args */
    void DeleteNoService__InvalidArgs();
    /* 9. Set with empty version - OK */
    void SetEmptyVersion__OK();
    /* 10. Set with empty version no service - invalid args */
    void SetNoServiceEmptyVersion__InvalidArgs();
    /* 11. Get, set, delete with service that does not exist, providing
     *     "exists" parameter - this parameter should be false and version 
     *     should be empty */
    void ServiceNotExistsAndBoolProvided__EqualsFalse();
    /* 12. Get, set, delete with service that does exist, providing
     *     "exists" parameter - this parameter should be true and version 
     *     should be filled */
    void ServiceExistsAndBoolProvided__EqualsTrue();
    /* 13. Get, set, delete with service that does not exist, not providing
     *     "exists" parameter -  version should be empty and no crash should 
     *     happen*/
    void ServiceNotExistsAndBoolNotProvided__NoCrash();
    /* 14. Get, set, delete with service that does exist, not providing
     *     "exists" parameter - this parameter should be true and version
     *     should be filled */
    void ServiceExistsAndBoolNotProvided__NoCrash();
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Stability
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Open, get all hosts, reset, get all hosts ... repeat N times
 * 2. (Open, (get all hosts, reset: repeat N times), close: repeat M times)  */
void GetNext_Reset__ShouldNotCrash();
void FullCycle__ShouldNotCrash();
} /* namespace Stability */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Performance
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. (get all hosts, reset) times a second, dependency on number of threads
 * 2. (Open, get all hosts, reset, close) times a second, dependency on 
      number of threads                                                      */
void FullCycle__ShouldNotCrash();
} /* namespace Performance */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace MultiThreading
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
    /*
    */
void TryMultiThread(); /* namespace MultiThreading */
}


///////////////////////////////////////////////////////////////////////////////
//////////////               DEFINITIONS             //////////////////////////
///////////////////////////////////////////////////////////////////////////////
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ComposeLBOSAddress
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSExists__ShouldReturnLbos()
{
    CLBOSStatus lbos_status(true, true);
#ifndef NCBI_COMPILER_MSVC 
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(result),
                             "LBOS address was not constructed appropriately");
    free(result);
#endif
}

/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multithreaded testing.
 */
void RoleFail__ShouldReturnNULL()
{
    CLBOSStatus lbos_status(true, true);
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    size_t lastSlash = min(path.rfind('/'), string::npos); //UNIX
    size_t lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
    path = path.substr(0, max(lastSlash, lastBackSlash));
    string corruptRoleString = path + string("/ncbi_lbos_role");
    const char* corruptRole = corruptRoleString.c_str();
    ofstream roleFile;
    CLBOSRoleDomain role_domain;
    role_domain.setRole(corruptRole);

    /* 1. Empty role */
    roleFile.open(corruptRoleString.data());
    roleFile << "";
    roleFile.close();

    CCObjHolder<char> result(g_LBOS_ComposeLBOSAddress());
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "LBOS address "
                           "construction did not fail appropriately "
                           "on empty role");

    /* 2. Garbage role */
    roleFile.open(corruptRoleString.data());
    roleFile << "I play a thousand of roles";
    roleFile.close();
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "LBOS address "
                           "construction did not fail appropriately "
                           "on garbage role");

    /* 3. No role file (use set of symbols that certainly
     * cannot constitute a file name)*/
    role_domain.setRole("|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "LBOS address "
                           "construction did not fail appropriately "
                           "on bad role file");
}


/* Thread-unsafe because of g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles().
 * Excluded from multi threaded testing.
 */
void DomainFail__ShouldReturnNULL()
{
    CLBOSStatus lbos_status(true, true);
    string path = CNcbiApplication::Instance()->GetProgramExecutablePath();
    size_t lastSlash = min(path.rfind('/'), string::npos); //UNIX
    size_t lastBackSlash = min(path.rfind("\\"), string::npos); //WIN
    path = path.substr(0, max(lastSlash, lastBackSlash));
    string corruptDomainString = path + string("/ncbi_lbos_domain");
    const char* corruptDomain = corruptDomainString.c_str();
    string reg_domain = "";
    bool has_reg_domain = false;
    ncbi::CNcbiRegistry& registry = CNcbiApplication::Instance()->GetConfig();
    if (registry.HasEntry("CONN", "lbos_domain")) {
        has_reg_domain = true;
        reg_domain = registry.Get("CONN", "lbos_domain");
        registry.Unset("CONN", "lbos_domain");
    }

    ofstream domainFile (corruptDomain);
    domainFile << "";
    domainFile.close();
    CLBOSRoleDomain role_domain;

    /* 1. Empty domain file */
    role_domain.setDomain(corruptDomain);
    CCObjHolder<char> result(g_LBOS_ComposeLBOSAddress());
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "LBOS address "
                           "construction did not fail appropriately");

    /* 2. No domain file (use set of symbols that certainly cannot constitute
     * a file name)*/
    role_domain.setDomain("|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "LBOS address "
                           "construction did not fail appropriately "
                           "on bad domain file");
    /* Cleanup */
    if (has_reg_domain) {
        registry.Set("CONN", "lbos_domain", reg_domain);
    }
}
} /* namespace GetLBOSAddress */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ResetIterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void NoConditions__IterContainsZeroCandidates();
void MultipleReset__ShouldNotCrash();
void Multiple_AfterGetNextInfo__ShouldNotCrash();
/*
 * Unit tested: SERV_Reset
 * Conditions: No Conditions
 * Expected result: iter contains zero Candidates
 */
void NoConditions__IterContainsZeroCandidates()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    /*
     * We know that iter is LBOS's.
     */
    SERV_Reset(*iter);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "LBOS not found when should be");
        return;
    }
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);

    NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
                           "Reset did not set n_cand to 0");
    NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
                           "Reset did not set pos_cand "
                           "to 0");
}


/*
 * Unit being tested: SERV_Reset
 * Conditions: Multiple reset
 * Expected result: should not crash
 */
void MultipleReset__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    SLBOS_Data* data;  
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    /*
     * We know that iter is LBOS's. It must have clear info by
     * implementation before GetNextInfo is called, so we can set
     * source of LBOS address now
     */
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "LBOS not found when should be");
        return;
    }
    int i = 0;
    for (i = 0;  i < 15;  ++i) {
        /* If just don't crash here, it is good enough. No assert is
         * necessary, plus this will cause valgrind to swear if not all iter
         * is reset
         */
        SERV_Reset(*iter);
        if (*iter == NULL)
            continue;//If nothing found, and reset does not crash - good enough
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_EQUAL(data->n_cand, 0U);
        NCBITEST_CHECK_EQUAL(data->pos_cand, 0U);
    }
    return;
}


void Multiple_AfterGetNextInfo__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";   
    SLBOS_Data* data;
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "LBOS not found when should be");
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
        SERV_Reset(*iter);
        if (*iter == NULL)
            continue;//If nothing found, and reset does not crash - good enough
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
            "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
            "Reset did not set pos_cand "
            "to 0");

        HOST_INFO host_info; // we leave garbage here for a reason
        SERV_GetNextInfoEx(*iter, &host_info);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE(data->n_cand > 0,
                               "n_cand should be more than 0 after "
                               "GetNextInfo");
        NCBITEST_CHECK_MESSAGE(data->pos_cand > 0,
                               "pos_cand should be more than 0 after "
                               "GetNextInfo");
        if (iter->op == NULL) {
            NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                "Mapper returned NULL when it should "
                "return s_op");
            return;
        }
        NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                               "Name of mapper that returned answer "
                               "is not \"lbos\"");
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
} /* namespace ResetIterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace CloseIterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void AfterOpen__ShouldWork();
void AfterReset__ShouldWork();
void AfterGetNextInfo__ShouldWork();
void FullCycle__ShouldWork();

void AfterOpen__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);    
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    /*SERV_Close(*iter);
    return;*/
}
void AfterReset__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    /*SERV_Reset(*iter);
    SERV_Close(*iter);*/
}

void AfterGetNextInfo__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    SLBOS_Data* data;
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    SERV_GetNextInfo(*iter);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");
//     SERV_Close(*iter);
//     return;
}

void FullCycle__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbostest";
    SLBOS_Data* data;
    size_t i;
    CConnNetInfo net_info(service);
    /*
     * 0. We have to announce what we will be iterating through
     */
    unsigned short port1, port2, port3;
    int count_before;
    CCObjHolder<char> lbos_answer(NULL);
    /* Prepare for test. We need to be sure that there is no previously
     * registered non-deleted service. We count servers with chosen port
     * and check if there is no server already announced */
    lbos_answer = NULL;
    s_SelectPort(count_before, service, port1, kSingleThreadNumber);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port1,
                     (string("http://") + s_GetMyIP() + ":"
                             PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), NULL, kSingleThreadNumber);
    lbos_answer = NULL;

    s_SelectPort(count_before, service, port2, kSingleThreadNumber);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port2,
                     (string("http://") + s_GetMyIP() + ":"
                             PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), NULL, kSingleThreadNumber);
    lbos_answer = NULL;

    s_SelectPort(count_before, service, port3, kSingleThreadNumber);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port3,
                     (string("http://") + s_GetMyIP() + ":"
                             PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), NULL, kSingleThreadNumber);
    lbos_answer = NULL;

    unsigned int servers_found1 =
        s_CountServersWithExpectation(service, port1, 1, kDiscoveryDelaySec,
                                      kSingleThreadNumber),
                 servers_found2 =
        s_CountServersWithExpectation(service, port2, 1, kDiscoveryDelaySec,
                                      kSingleThreadNumber),
                 servers_found3 =
        s_CountServersWithExpectation(service, port3, 1, kDiscoveryDelaySec,
                                      kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(servers_found1 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    NCBITEST_REQUIRE_MESSAGE(servers_found2 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    NCBITEST_REQUIRE_MESSAGE(servers_found3 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    /*
     * 1. We close after first GetNextInfo
     */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "Could not find announced lbostest nodes");
        return;
    }
    SERV_GetNextInfo(*iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");

    /*
     * 2. We close after half hosts checked with GetNextInfo
     */
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "Could not find announced lbostest nodes");
        return;
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    /*                                                            v half    */
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand / 2;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                                             " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                                               "than 0 after GetNextInfo");

    /* 3. We close after all hosts checked with GetNextInfo*/
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "Could not find announced lbostest nodes");
        return;
    }
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");

    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.0.0", "", port1,
                  &lbos_answer.Get(), NULL, kSingleThreadNumber);
    s_DeannounceC(service.c_str(), "1.0.0", "", port2,
                  &lbos_answer.Get(), NULL, kSingleThreadNumber);
    s_DeannounceC(service.c_str(), "1.0.0", "", port3,
                  &lbos_answer.Get(), NULL, kSingleThreadNumber);

    lbos_answer = NULL;
}
} /* namespace CloseIterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DTab
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Mix of registry DTab and HTTP Dtab: registry goes first */
void DTabRegistryAndHttp__RegistryGoesFirst()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
        s_FakeReadWithErrorFromLBOSCheckDTab);
    ConnNetInfo_SetUserHeader(*net_info, 
                              "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    string expected_header = "DTab-local: /lbostest=>/zk#/lbostest/1.0.0; "
                            "/lbostest=>/zk#/lbostest/1.1.0";
    
    NCBITEST_CHECK_MESSAGE(s_LBOS_header.substr(0, expected_header.length()) ==
                                expected_header, 
                           "Header with DTab did not combine as expected");
}

/** 2. Announce server with non-standard version and server with no standard 
    version at all. Both should be found via DTab                            */
void NonStandardVersion__FoundWithDTab() 
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbostest";
    CConnNetInfo net_info(service);
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort(kSingleThreadNumber);
    /*
     * I. Non-standard version
     */ 
    int count_before =
            s_CountServers(service, port,
                           "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort(kSingleThreadNumber);
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    }

    s_AnnounceCSafe(service.c_str(), "1.1.0", "", port,
                    //"http://0.0.0.0:" PORT_STR(PORT) "/health",
                     (string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), NULL, kSingleThreadNumber);

    unsigned int servers_found =
        s_CountServersWithExpectation(service, port, 1, kDiscoveryDelaySec, kSingleThreadNumber,
                                  "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    NCBITEST_CHECK_MESSAGE(servers_found == 1,
                           "Error while searching non-standard server "
                           "version with DTab");
    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.1.0", 
                    NULL, port, NULL, NULL, kSingleThreadNumber);
    lbos_answer = NULL;
    
    /*
     * II. Service with no standard version in ZK config
     */
    service = "/lbostest1";
    count_before = s_CountServers(service, port,
                                  "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort(kSingleThreadNumber);
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    }
    s_AnnounceCSafe(service.c_str(), "1.1.0", "", port,
                  "http://0.0.0.0:" PORT_STR(PORT) "/health",
                  &lbos_answer.Get(), NULL, kSingleThreadNumber);

    servers_found = s_CountServersWithExpectation(service, port, 1, kDiscoveryDelaySec,
                           kSingleThreadNumber,
                           "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    NCBITEST_CHECK_MESSAGE(servers_found == 1,
                           "Error while searching server with no standard"
                           "version with DTab");

    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.1.0", 
        NULL, port, NULL, NULL, kSingleThreadNumber);
    lbos_answer = NULL;
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ResolveViaLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServiceExists__ReturnHostIP();
void ServiceDoesNotExist__ReturnNULL();
void NoLBOS__ReturnNULL();
void FakeMassiveInput__ShouldProcess();
void FakeErrorInput__ShouldNotCrash();

void ServiceExists__ReturnHostIP()
{
    string service = "/lbos";
    CConnNetInfo net_info(service);
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    /*
     * We know that iter is LBOS's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
                      g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                       lbos_addr.c_str(),
                                       service.c_str(),
                                       *net_info));
    size_t i = 0;
    if (*hostports != NULL) {
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
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";
    CConnNetInfo net_info(service);
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    /*
     * We know that iter is LBOS's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                        lbos_addr.c_str(),
                                        service.c_str(),
                                        *net_info));
    NCBITEST_CHECK_MESSAGE(hostports.count() == 0, "Mapper should not find "
                                                   "service, but it somehow "
                                                   "found.");
}


void NoLBOS__ReturnNULL()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);
    /*
     * We know that iter is LBOS's.
     */

    CCObjArrayHolder<SSERV_Info> hostports(
                           g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                                "lbosdevacvancbinlmnih.gov:80",
                                                service.c_str(),
                                                *net_info));
    NCBITEST_CHECK_MESSAGE(hostports.count() == 0, 
                           "Mapper should not find LBOS, but it somehow" 
                           "found.");
}


void FakeMassiveInput__ShouldProcess()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";    
    unsigned int temp_ip;
    unsigned short temp_port;
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info(service);
    /*
     * We know that iter is LBOS's.
     */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
        s_FakeReadDiscovery<200>);
    CCObjArrayHolder<SSERV_Info> hostports(
                     g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info));
    int i = 0;
    NCBITEST_CHECK_MESSAGE(*hostports != NULL,
                           "Problem with fake massive input, "
                           "no servers found. "
                           "Most likely, problem with test.");
    ostringstream temp_host;
    if (*hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            temp_host.str("");
            temp_host << i+1 << "." <<  i+2 << "." <<  i+3 << "." << i+4
                      << ":" << (i+1)*215;
            SOCK_StringToHostPort(temp_host.str().c_str(), 
                                  &temp_ip, 
                                  &temp_port);
            NCBITEST_CHECK_MESSAGE(hostports[i]->host == temp_ip,
                                   "Problem with recognizing IP"
                                   " in massive input");
            NCBITEST_CHECK_MESSAGE(hostports[i]->port == temp_port,
                                   "Problem with recognizing IP "
                                   "in massive input");
        }
        NCBITEST_CHECK_MESSAGE(i == 200, "Mapper should find 200 hosts, but "
                               "did not.");
    }
    /* Return everything back*/
    //g_LBOS_UnitTesting_GetLBOSFuncs()->Read = temp_func_pointer;
}


void FakeErrorInput__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";   
    unsigned int temp_ip;
    unsigned short temp_port;
    s_CallCounter = 0;
    CConnNetInfo net_info(service);
    /*
     * We know that iter is LBOS's.
     */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                    s_FakeReadDiscoveryCorrupt<200>);
    CCObjArrayHolder<SSERV_Info> hostports(
                        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info));
    int i=0, /* iterate test numbers*/
        j=0; /*iterate hostports from ResolveIPPort */
    NCBITEST_CHECK_MESSAGE(*hostports != NULL,
                           "Problem with fake error input, no servers found. "
                           "Most likely, problem with test.");
    if (*hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            /* We need to check which combination is supposed to be failure*/

            stringstream ss;
            ss << std::setfill('0') << std::setw(3) << i + 1 << "." 
               << std::setfill('0') << std::setw(3) << i + 2 << "." 
               << std::setfill('0') << std::setw(3) << i + 3 << "." 
               << std::setfill('0') << std::setw(3) << i + 4;
            if (
                    i < 100 
                    && 
                    (
                        ss.str().find('8') != string::npos 
                        ||
                        ss.str().find('9') != string::npos
                    )
                ) 
            {
                continue;
            }
            ss << ":" << (i + 1) * 215;
            SOCK_StringToHostPort(ss.str().c_str(), &temp_ip, &temp_port);
            NCBITEST_CHECK_MESSAGE(hostports[j]->host == temp_ip,
                                   "Problem with recognizing IP "
                                   "in massive input");
            NCBITEST_CHECK_MESSAGE(hostports[j]->port == temp_port,
                                   "Problem with recognizing IP "
                                   "in massive input");
            j++;
        }
        NCBITEST_CHECK_MESSAGE(i == 80, "Mapper should find 80 hosts, but "
                                        "did not.");
   }
   /* g_LBOS_UnitTesting_GetLBOSFuncs()->Read = temp_func_pointer;*/
}
} /* namespace ResolveViaLBOS */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetLBOSAddress
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void SpecificMethod__FirstInResult();
void CustomHostNotProvided__SkipCustomHost();
void NoConditions__AddressDefOrder();

/* Not thread-safe because of using s_LBOS_funcs */
void SpecificMethod__FirstInResult()
{
    CLBOSStatus lbos_status(true, true);
    string custom_lbos = "lbos.custom.host";
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                     custom_lbos.c_str()));
    /* I. Custom address */
    WRITE_LOG("Part 1. Testing custom LBOS address", kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(string(addresses[0]), custom_lbos);

    /* II. Registry address */
    string registry_lbos =
            CNcbiApplication::Instance()->GetConfig().Get("CONN", "LBOS");
    WRITE_LOG("Part 2. Testing registry LBOS address", kSingleThreadNumber);
    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_Registry, NULL);
    NCBITEST_CHECK_EQUAL(string(addresses[0]), registry_lbos);

#ifndef NCBI_OS_MSWIN
    /* We have to fake last method, because its result is dependent on
     * location */
    /* III. etc/ncbi address (should be 127.0.0.1) */
    WRITE_LOG("Part 3. Testing etc/ncbi/lbosresolver", kSingleThreadNumber);
    size_t buffer_size = 1024;
    size_t size;
    AutoArray<char> buffer(new char[buffer_size]);
    AutoPtr<IReader> reader(CFileReader::New("/etc/ncbi/lbosresolver"));
    ERW_Result result = reader->Read(buffer.get(), buffer_size, &size);
    NCBITEST_REQUIRE_MESSAGE(result == eRW_Success, "Could not read "
                             "/etc/ncbi/lbosresolver, test failed");
    buffer.get()[size] = '\0';
    string lbosresolver(buffer.get() + 7);
    lbosresolver = lbosresolver.erase(lbosresolver.length()-6);
    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_Lbosresolve, NULL);
    NCBITEST_CHECK_EQUAL(string(addresses[0]), lbosresolver);
    
#endif /* #ifndef NCBI_OS_MSWIN */
}

void CustomHostNotProvided__SkipCustomHost()
{
    CLBOSStatus lbos_status(true, true);
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                     NULL));
    /* We check the count of LBOS addresses */
    NCBITEST_CHECK_EQUAL(addresses.count(),  1U);
}

void NoConditions__AddressDefOrder()
{
    CMockFunction<FLBOS_ComposeLBOSAddressMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress,
        s_FakeComposeLBOSAddress);

    CNcbiRegistry& registry = CNcbiApplication::Instance()->GetConfig();
    string lbos = registry.Get("CONN", "lbos");
    /* I. Registry has entry (we suppose that it has by default) */
    CCObjArrayHolder<char> addresses(g_LBOS_GetLBOSAddresses());
    WRITE_LOG("1. Checking LBOS address when registry LBOS is provided",
              kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(string(addresses[0]), lbos);
    NCBITEST_REQUIRE_MESSAGE(addresses[1] ==  NULL,
                             "LBOS address list was not closed after the "
                             "first item");

#ifndef NCBI_OS_MSWIN
    /* II. Registry has no entries - check that LBOS is read from
     *     /etc/ncbi/lbosresolver */
    WRITE_LOG("2. Checking LBOS address when registry LBOS is not provided",
              kSingleThreadNumber);
    size_t buffer_size = 1024;
    size_t size;
    AutoArray<char> buffer(new char[buffer_size]);
    AutoPtr<IReader> reader(CFileReader::New("/etc/ncbi/lbosresolver"));
    ERW_Result result = reader->Read(buffer.get(), buffer_size, &size);
    NCBITEST_REQUIRE_MESSAGE(result == eRW_Success, "Could not read "
                             "/etc/ncbi/lbosresolver, test failed");
    buffer.get()[size] = '\0';
    string lbosresolver(buffer.get() + 7);
    lbosresolver = lbosresolver.erase(lbosresolver.length()-6);
    registry.Unset("CONN", "lbos");
    addresses = g_LBOS_GetLBOSAddresses();
    NCBITEST_CHECK_EQUAL(string(addresses[0]), lbosresolver);
    NCBITEST_REQUIRE_MESSAGE(addresses[1] ==  NULL,
                             "LBOS address list was not closed after the "
                             "first item");

    /* Cleanup */
    registry.Set("CONN", "lbos", lbos);
#endif

#if 0
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_Registry));
    WRITE_LOG("2. Checking registry LBOS address", kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[1] !=  NULL,
                             "LBOS address list ended too soon");
    /* III. /etc/ncbi/lbosresolver */
    string registry_lbos =
            CNcbiApplication::Instance()->GetConfig().Get("CONN", "LBOS");
    NCBITEST_CHECK_EQUAL(string(addresses[1]), registry_lbos);

    /* IV. Default */
    WRITE_LOG("3. Checking localhost LBOS address", kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[2] !=  NULL,
                             "LBOS address list ended too soon");
    NCBITEST_CHECK_EQUAL(string(addresses[2]), "127.0.0.1:8080");

#ifndef NCBI_OS_MSWIN
    WRITE_LOG("4. Checking first etc/ncbi LBOS address", kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[3] !=  NULL,
                             "LBOS address list ended too soon");
    NCBITEST_CHECK_EQUAL(string(addresses[3]), "lbos.foo");

    WRITE_LOG("5. Checking second etc/ncbi LBOS address", kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[4] !=  NULL,
                             "LBOS address list ended too soon");
    NCBITEST_CHECK_EQUAL(string(addresses[4]), "lbos.foo:8080");

    WRITE_LOG("6. Checking last NULL element in LBOS address list",
              kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[5] ==  NULL,
                             "LBOS address list does not end where expected");
#else
    WRITE_LOG("3. Checking last NULL element in LBOS address list",
              kSingleThreadNumber);
    NCBITEST_REQUIRE_MESSAGE(addresses[3] == NULL,
                             "LBOS address list does not end where expected");
#endif
#endif
}
} /* namespace GetLBOSAddress */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetCandidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSNoResponse__ErrorNoLBOS()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info(service);
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort,
                            s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                              SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                              *net_info, 0/*skip*/, 0/*n_skip*/,
                              0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_REQUIRE_MESSAGE(*iter == NULL, "LBOS found when it should not be");
}

void LBOSResponds__Finish()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";    
    CConnNetInfo net_info(service);
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    s_CallCounter = 2;
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, 
        s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = lbos_addr.c_str();
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(*iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service. We expect only one call, which means that counter
     * should increase by 1
     */
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 3,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of alive LBOS");
}

/*Not thread safe because of s_LastHeader*/
void NetInfoProvided__UseNetInfo()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info(service);
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    ConnNetInfo_SetUserHeader(*net_info, "My header fq34facsadf");
    
    s_CallCounter = 2; // to get desired behavior from s_FakeResolveIPPort
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                             lbos_addr.c_str();
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(*iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service
     */
    NCBITEST_CHECK_MESSAGE(s_LastHeader == "My header fq34facsadf\r\n",
                           "s_LBOS_FillCandidates: Incorrect "
                           "transition of header");
}
} /* namespace GetCandidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

void EmptyCands__RunGetCandidates()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;    
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port = kDefaultPort;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);
    CConnNetInfo net_info(service);
    ConnNetInfo_SetUserHeader(*net_info, "My header fq34facsadf");
    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<10>);

    /* If no candidates found yet, get candidates and return first of them. */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_CallCounter = 0;

    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(*iter);
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
//     SERV_Close(*iter);
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//     s_CallCounter = 0;
}


void ErrorUpdating__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";
    const SSERV_Info* info = NULL;
    CConnNetInfo net_info(service);
    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
        s_FakeFillCandidatesWithError);

    /*If no candidates found yet, get candidates, catch error and return NULL*/
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    HOST_INFO hinfo = NULL;
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(info == 0, "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to error in LBOS" );
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in LBOS");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_CallCounter = 0;

    /* Now we first play fair, Open() iter, then Reset() iter, and in the
     * end simulate error */
    mock = s_FakeFillCandidates<10>;
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(*iter);
    mock = s_FakeFillCandidatesWithError;
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(info == 0, "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to error in LBOS "
                           "(info not NULL)" );
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 2,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in LBOS "
                           "(fillCandidates was not called once)");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
    /*ConnNetInfo_Destroy(*net_info);*/
//     SERV_Close(*iter);
//     s_CallCounter = 0;
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
}

void HaveCands__ReturnNext()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;    
    unsigned int host = 0;
    unsigned short port = kDefaultPort;  
    CConnNetInfo net_info(service);   

    CMockFunction<FLBOS_FillCandidatesMethod*> mock (
        g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
        s_FakeFillCandidates<200>);

    /* We will get 200 candidates, iterate 220 times and see how the system
     * behaves */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not "
                           "ask LBOS for candidates");

    int i = 0, found_hosts = 0;
    ostringstream hostport;
    for (i = 0;  i < 220/*200+ returned by s_FakeFillCandidates*/;  i++)
    {
        info = SERV_GetNextInfoEx(*iter, &hinfo);
        if (info != NULL) { /*As we suppose it will be last 20 iterations */
            found_hosts++;
            hostport.str("");
            hostport << i+1 << "." << i+2 << "." << i+3 << "." << i+4 << 
                      ":" << (i+1)*210;
            SOCK_StringToHostPort(hostport.str().c_str(), &host, &port);
            NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                                   "SERV_GetNextInfoEx: fill "
                                   "candidates was called, but "
                                   "it should not be");
            NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                                   "SERV_GetNextInfoEx: mapper "
                                   "error with 'next' returned element");
            NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                                   "SERV_GetNextInfoEx: hinfo is not "
                                   "NULL (always should be NULL)");
        }
    }

    /* The main interesting here is to check if info is null, and that
     * 'fillcandidates()' was not called again internally*/
    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper error with "
                           "'after last' returned element");
    NCBITEST_CHECK_MESSAGE(found_hosts == 200, "Mapper should find 200 "
                                              "hosts, but did not.");

    /* Cleanup*/
//     SERV_Close(*iter);
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//     s_CallCounter = 0;
}

void LastCandReturned__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;
    HOST_INFO hinfo = NULL;    
    string hostport = "127.0.0.1:80";
    unsigned int host = 0;
    unsigned short port = 0;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);    
    CConnNetInfo net_info(service);  

    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<200>);
    /* If no candidates found yet, get candidates and return first of them. */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));

    /*ConnNetInfo_Destroy(*net_info);*/
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");

    info = SERV_GetNextInfoEx(*iter, &hinfo);
    int i = 0;
    for (i = 0;  info != NULL;  i++) {
        info = SERV_GetNextInfoEx(*iter, &hinfo);
    }

    NCBITEST_CHECK_MESSAGE(i == 200, "Mapper should find 200 hosts, but "
                           "did not.");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
//     SERV_Close(*iter);
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//     s_CallCounter = 0;
}

void DataIsNull__ReconstructData()
{
    string service = "/lbos";
    const SSERV_Info* info = NULL;
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port = kDefaultPort;
    CCounterResetter resetter(s_CallCounter);
    CLBOSStatus lbos_status(true, true);
    HOST_INFO hinfo;
    SOCK_StringToHostPort(hostport.c_str(), &host, &port);
    CConnNetInfo net_info;
    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates, 
                             s_FakeFillCandidates<10>);

                                       
    /* We will get iterator, and then delete data from it and run GetNextInfo.
     * The mapper should recover from this kind of error */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All /*types*/,
                              SERV_LOCALHOST /*preferred_host*/, 0/*port*/, 
                              0.0/*preference*/, *net_info, 0/*skip*/, 
                              0/*n_skip*/, 0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /* Now we destroy data */
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);
    g_LBOS_UnitTesting_GetLBOSFuncs()->DestroyData(data);
    iter->data = NULL;
    /* Now let's see how the mapper behaves. Let's check the first element */
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    /*Assert*/
    NCBITEST_CHECK_MESSAGE(s_CallCounter == 2,
                           "SERV_GetNextInfoEx: mapper did "
                           "not ask LBOS for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error "
                           "with first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not "
                           "NULL (always should be NULL)");
}

void IterIsNull__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    const SSERV_Info* info = NULL;
    HOST_INFO host_info;
    info = g_LBOS_UnitTesting_GetLBOSFuncs()->GetNextInfo(NULL, &host_info);

    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to empty iter");
}

void WrongMapper__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;
    CConnNetInfo net_info(service);
    /* We will get iterator, and then change mapper name from it and run
     * GetNextInfo.
     * The mapper should return null */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    const SSERV_VTable* origTable = iter->op;
    const SSERV_VTable fakeTable = {NULL, NULL, NULL, NULL, NULL, "LBSMD"};
    iter->op = &fakeTable;
    /* Now let's see how the mapper behaves. Let's run GetNextInfo()*/
    info = SERV_GetNextInfoEx(*iter, &hinfo);

    NCBITEST_CHECK_MESSAGE(info == NULL,
                           "SERV_GetNextInfoEx: mapper did not "
                           "react correctly to wrong mapper name");

    iter->op = origTable; /* Because we need to clean iter */

    /* Cleanup*/
    //SERV_Close(*iter);
}
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void IterIsNull__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    if (iter.get() == NULL) {
        NCBITEST_CHECK_MESSAGE(iter.get() == NULL,
                               "Problem with memory allocation, "
                               "calloc failed. Not enough RAM?");
        return;
    }
    
    CConnNetInfo net_info(service);
    
    iter->op = SERV_LBOS_Open(NULL, *net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op == NULL,
                           "Mapper returned operations when "
                           "it should return NULL");
    /* Cleanup */
    /*ConnNetInfo_Destroy(*net_info);*/
    /*SERV_Close(*iter);*/
}


void NetInfoNull__ConstructNetInfo()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                                           "LBOS not found when it should be");
        return;
    }
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                            "Mapper returned NULL when it should return s_op");
        return;
    }
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned "
                           "answer is not \"lbos\"");
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
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    if (iter.get() == NULL) {
        NCBITEST_CHECK_MESSAGE(iter.get() == NULL,
                               "Problem with memory allocation, "
                               "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service.c_str();
    CConnNetInfo net_info(service); 

    iter->op = SERV_LBOS_Open(iter.get(), *net_info, NULL);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                               "LBOS not found when it should be");
        return;
    }

    NCBITEST_CHECK_MESSAGE(iter->op != NULL, 
                           "Mapper returned NULL when it should return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
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

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->DestroyData(
                                          static_cast<SLBOS_Data*>(iter->data));
}


void InfoPointerProvided__WriteNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    iter->name = service.c_str();
    SSERV_Info* info;
    CConnNetInfo net_info(service);
    
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, &info);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                               "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned answer "
                           "is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE(info == NULL, "LBOS mapper provided "
            "something in host info, when it should not");

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->DestroyData(
                                          static_cast<SLBOS_Data*>(iter->data));
}

void NoSuchService__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/donotexist";
    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    if (iter.get() == NULL) {
        NCBITEST_CHECK_MESSAGE(iter.get() == NULL,
                               "Problem with memory allocation, "
                               "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service.c_str();    
    CConnNetInfo net_info(service);    
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, NULL);
    NCBITEST_CHECK_MESSAGE(iter->op == NULL,
                           "Mapper returned s_op when it "
                           "should return NULL");
    /* Cleanup */
    /*ConnNetInfo_Destroy(*net_info);*/
}
} /* namespace Open */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GeneralLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServerExists__ShouldReturnLbosOperations()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    CConnNetInfo net_info(service);   
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                           "Mapper returned NULL when it "
                           "should return s_op");
    if (iter->op == NULL) return;
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
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
    CLBOSStatus lbos_status(true, true);
    string service = "/asdf/idonotexist";
    CConnNetInfo net_info(service);
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_CHECK_MESSAGE(*iter == NULL,
                           "Mapper should not find service, but "
                           "it somehow found.");
}

void LbosExist__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
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


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{


/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    unsigned short deannounce_result;
    WRITE_LOG("Testing simple announce test. Should return 200.", thread_num);
    int count_before;

    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count servers with chosen port 
     * and check if there is no server already announced */
    s_SelectPort(count_before, node_name, port, thread_num);
    unsigned short result;
    /*
     * I. Check with 0.0.0.0
     */
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    result = s_AnnounceCSafe(node_name.c_str(),
                             "1.0.0",
                             "",
                             port,
                             "http://0.0.0.0:" PORT_STR(PORT) "/health",
                             &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);

    /* Cleanup */
    deannounce_result = s_DeannounceC(node_name.c_str(), "1.0.0", NULL, 
                                      port, NULL, NULL, thread_num);
    NCBITEST_CHECK_EQUAL(deannounce_result, kLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /*
     * II. Now check with IP
     */
    WRITE_LOG("Part II: real IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(),
                    "1.0.0",
                    "", port,
                    (string("http://") + s_GetMyIP() +
                    ":" PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);

    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", s_GetMyIP().c_str(),
                  port, NULL, NULL, thread_num);
    NCBITEST_CHECK_EQUAL(deannounce_result, kLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
}


/*  3. Successfully announced : char* lbos_answer contains answer of LBOS    */
/* Test is thread-safe. */
void AllOK__LBOSAnswerProvided(int thread_num = -1)
{
    WRITE_LOG("Testing simple announce test. "
             "Announcement function should return answer of LBOS", thread_num);
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  (string("http://") + s_GetMyHost() + ":8080/health").c_str(),
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);

    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "LBOS answer as expected");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                  NULL, NULL, thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP  */
    WRITE_LOG("Part II: real IP", thread_num);
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "LBOS answer as expected");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", s_GetMyIP().c_str(),
                  port, NULL, NULL, thread_num);
                    
    lbos_answer = NULL;
    lbos_status_message = NULL;
}

/* If announced successfully - status message is "OK" */
void AllOK__LBOSStatusMessageIsOK(int thread_num = -1)
{
    WRITE_LOG("Testing simple announce test. Announcement function should "
              "return answer of LBOS", 
              thread_num);
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    "http://0.0.0.0:" PORT_STR(PORT) "/health",
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        string("OK"));
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                  NULL, NULL, thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP  */
    WRITE_LOG("Part II: real IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  (string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health").c_str(),
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", s_GetMyIP().c_str(),
                  port, NULL, NULL, thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden LBOS mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved(int thread_num = -1)
{
    WRITE_LOG("Testing saving of parameters of announced server when "
              "announcement finished successfully", 
              thread_num);
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Announce */
    unsigned short result;
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_MESSAGE(result == kLBOSSuccess,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name, "1.0.0", port, "0.0.0.0", thread_num),
        1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    int deannounce_result = s_DeannounceC(node_name.c_str(), 
                                            "1.0.0",
                                            "", 
                                            port, &lbos_answer.Get(), 
                                            &lbos_status_message.Get(), 
                                            thread_num);
    NCBITEST_CHECK_MESSAGE(deannounce_result == kLBOSSuccess,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP instead of host name */
    WRITE_LOG("Part II: real IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(),
                        "1.0.0",
                        "",
                        port,
                        (string("http://") + s_GetMyIP() +
                                ":" PORT_STR(PORT) "/health").c_str(),
                        &lbos_answer.Get(), &lbos_status_message.Get(), 
                        thread_num);
    NCBITEST_CHECK_MESSAGE(result == kLBOSSuccess,
                           "Announcement function did not return "
                           "SUCCESS as expected");

    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyIP(),
                              thread_num),
        1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    deannounce_result = s_DeannounceC(node_name.c_str(), 
                                      "1.0.0",
                                      "",
                                      port, NULL, NULL, thread_num);
    NCBITEST_CHECK_EQUAL(deannounce_result, kLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
}


/*  5. Could not find LBOS: return NO_LBOS                                   */
/* Test is NOT thread-safe. */
void NoLBOS__ReturnNoLBOSAndNotFind(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found."
                " Should return kLBOSNoLBOS", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    unsigned short result;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()", thread_num);
    
    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    
    result = s_AnnounceC(node_name.c_str(),
                   "1.0.0",
                   "", port,
                   "http://0.0.0.0:" PORT_STR(PORT) "/health",
                   &lbos_answer.Get(), &lbos_status_message.Get(), 
                   thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSNoLBOS);

    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()", thread_num);
}


/*  6. Could not find LBOS : char* lbos_answer is set to NULL                */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSAnswerNull(int thread_num = -1)
{
    WRITE_LOG(
             "Testing behavior of LBOS when no LBOS is found."
             " LBOS answer should be NULL.", thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG(
        "Mocking CONN_Read() with s_FakeReadEmpty()", thread_num);

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                       s_FakeReadEmpty);
    s_AnnounceC(node_name.c_str(),
                "1.0.0",
                "", port,
                "http://0.0.0.0:" PORT_STR(PORT) "/health",
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");

    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()", thread_num);
}


/*  6. Could not find LBOS: char* lbos_status_message is set to NULL         */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSStatusMessageNull(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found. "
              "LBOS message should be NULL.", thread_num);
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()", thread_num);

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                    s_FakeReadEmpty);
    s_AnnounceC(node_name.c_str(),
                "1.0.0",
                "", port,
                "http://0.0.0.0:" PORT_STR(PORT) "/health",
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");

    /* Cleanup*/
    WRITE_LOG(
            "Reverting mock of CONN_Read() with s_FakeReadEmpty()", thread_num);
}


/*  8. LBOS returned error: return kLBOSServerError                          */
/* Test is NOT thread-safe. */
void LBOSError__ReturnServerErrorCode(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when LBOS returns error code (507).", thread_num);
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()", thread_num);

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                   "http://0.0.0.0:" PORT_STR(PORT) "/health",
                   &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);

    /* Check that error code is the same as in mock*/
    NCBITEST_CHECK_MESSAGE(result == 507, 
                           "Announcement did not return "
                           "kLBOSDNSResolveError as expected");
    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()", thread_num);
}


/*  8.5. LBOS returned error: return kLBOSServerError                         */
/* Test is NOT thread-safe. */
void LBOSError__ReturnServerStatusMessage(int thread_num = -1)
{
    CLBOSStatus       lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when LBOS returns error message "
              "(\"LBOS STATUS\").", thread_num);
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                    s_FakeReadAnnouncementWithErrorFromLBOS);
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                "http://0.0.0.0:" PORT_STR(PORT) "/health",
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);

    /* Check that error code is the same as in mock*/
    NCBITEST_CHECK_MESSAGE(strcmp(*lbos_status_message, "LBOS STATUS") == 0,
                           "Announcement did not return "
                           "kLBOSDNSResolveError as expected");
    /* Cleanup*/
    WRITE_LOG("Reverting mock of CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()", thread_num);
}


/*  9. LBOS returned error : char* lbos_answer contains answer of LBOS       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when LBOS returns error code. Should return "
             "exact message from LBOS", thread_num);
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                  "http://0.0.0.0:" PORT_STR(PORT) "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(string(*lbos_answer ? *lbos_answer : "<NULL>"),
                         "Those lbos errors are scaaary");
    /* Cleanup*/
    WRITE_LOG("Reverting mock of CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()", thread_num);
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS "
                 "mapper when server was already announced (info stored in "
                 "internal storage should be replaced. Server node should be "
                 "rewritten, no duplicates.", 
             thread_num);
    unsigned int lbos_addr = 0;
    unsigned short lbos_port = 0;
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    unsigned short result;
    const char* convert_result;
    /*
     * First time
     */
    WRITE_LOG("Part 1. First time announcing server", thread_num);
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_REQUIRE_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                             "Did not get answer after announcement");
    convert_result = 
        SOCK_StringToHostPort(*lbos_answer, &lbos_addr, &lbos_port);
    NCBITEST_CHECK_MESSAGE(convert_result != NULL,
                           "Host:port returned by LBOS is trash");
    /* Count how many servers there are */
    int count_after = 0;
    count_after = s_CountServersWithExpectation(node_name, port, 1, 
                                                kDiscoveryDelaySec, thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    WRITE_LOG("Trying to find the announced server in "
              "%LBOS%/lbos/text/service", thread_num);
    int servers_in_text_service = s_FindAnnouncedServer(node_name, "1.0.0", 
                                                        port, "0.0.0.0", 
                                                        thread_num);
    WRITE_LOG("Found  " << servers_in_text_service << 
              " servers in %LBOS%/lbos/text/service (should be " << 
              count_before + 1 << ")", thread_num);

    NCBITEST_CHECK_EQUAL(servers_in_text_service, count_before + 1);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * Second time
     */
    WRITE_LOG("Part 2. Second time announcing server", thread_num);
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_REQUIRE_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                             "Did not get answer after announcement");
    convert_result = SOCK_StringToHostPort(*lbos_answer,
                                           &lbos_addr,
                                           &lbos_port);
    NCBITEST_CHECK_MESSAGE(convert_result != NULL &&
                           convert_result != *lbos_answer,
                           "LBOS answer could not be parsed to host:port");
    /* Count how many servers there are. */
    count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    WRITE_LOG("Trying to find the announced server in "
              "%LBOS%/lbos/text/service", thread_num);
    servers_in_text_service = s_FindAnnouncedServer(node_name, "1.0.0", 
                                                    port, "0.0.0.0", 
                                                    thread_num);
    WRITE_LOG("Found " << servers_in_text_service << 
              " servers in %LBOS%/lbos/text/service (should be " << 
              count_before + 1 << ")", thread_num);

    NCBITEST_CHECK_EQUAL(servers_in_text_service, count_before + 1);

    /* Cleanup */     
    int deannounce_result = s_DeannounceC(node_name.c_str(), 
                                            "1.0.0",
                                            s_GetMyIP().c_str(), 
                                            port, NULL, NULL, thread_num);
    NCBITEST_CHECK_EQUAL(deannounce_result, kLBOSSuccess);
}


/* 12. Trying to announce in foreign domain - do nothing and 
       return that no LBOS is found (because no LBOS in current 
       domain is found) */
/* Test is NOT thread-safe. */
void ForeignDomain__NoAnnounce(int thread_num = -1)
{
#if 0 /* deprecated */
    /* Test is not run in TeamCity*/
    if (!getenv("TEAMCITY_VERSION")) {
        CLBOSStatus lbos_status(true, true);
        CCObjHolder<char> lbos_answer(NULL);
        CCObjHolder<char> lbos_status_message(NULL);
        unsigned short port = kDefaultPort;
        string node_name = s_GenerateNodeName();
        WRITE_LOG("Testing behavior of LBOS mapper when no LBOS is "
                   "available in the current region (should "
                   "return error code kLBOSNoLBOS).", thread_num);
        WRITE_LOG("Mocking region with \"or-wa\"", thread_num);
        CMockString mock1(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");

        unsigned short result;
        result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                               "http://0.0.0.0:" PORT_STR(PORT) "/health",
                               &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
        NCBITEST_CHECK_EQUAL(result, kLBOSNoLBOS);
        NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                               "LBOS status message is not NULL");
        NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                               "Answer from LBOS was not NULL");
        /* Cleanup*/
        WRITE_LOG("Reverting mock of region with \"or-wa\"", thread_num);
    }
#endif
}


/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect healthcheck URL - should return "
             "kLBOSInvalidArgs", thread_num);
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that equals NULL
     */
    WRITE_LOG("Part I. Healthcheck is <NULL>", thread_num);
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port, NULL,
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup*/
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
     * II. Healthcheck URL that does not start with http or https
     */
    WRITE_LOG("Part II. Healthcheck is \"\" (empty string)", thread_num);
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port, "",
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");

    /*
    * III. Healthcheck URL that does not start with http or https
    */
    WRITE_LOG("Part III. Healthcheck is \""
              "lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health\" (no http://)",
              thread_num);
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port, 
                "lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL,
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");
}


/* 14. Was passed incorrect port: do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect port (zero) - should return "
              "kLBOSInvalidArgs", thread_num);
    unsigned short result;
    /* I. 0 */
    port = 0;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    unsigned short result;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect version - should return "
             "kLBOSInvalidArgs", thread_num);
    /*
     * I. NULL version
     */
    WRITE_LOG("Part I. Version is <NULL>", thread_num);
    result = s_AnnounceC(node_name.c_str(), NULL, "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");

    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /*
     * II. Empty version 
     */
    WRITE_LOG("Part II. Version is \"\" (empty string)", thread_num);
    node_name = s_GenerateNodeName();
    result = s_AnnounceC(node_name.c_str(), "", "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 16. Was passed incorrect service name (NULL or empty): do not 
 *     announce and return INVALID_ARGS                                      */
/* Test is thread-safe. */
void IncorrectServiceName__ReturnInvalidArgs(int thread_num = -1)
{
    unsigned short result;
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect service name - should return "
              "kLBOSInvalidArgs", thread_num);
    /*
     * I. NULL service name
     */
    WRITE_LOG("Part I. Service name is <NULL>", thread_num);
    result = s_AnnounceC(NULL, "1.0.0", "", port, "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
     * II. Empty service name
     */
    WRITE_LOG("Part II. Service name is \"\" (empty string)", thread_num);
    node_name = s_GenerateNodeName();
    port = kDefaultPort;
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    result = s_AnnounceC("", "1.0.0", "", port, "http://0.0.0.0:" PORT_STR(PORT) "/health",
                   &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 17. Real-life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    string node_name = s_GenerateNodeName();
    WRITE_LOG("Real-life test of LBOS: "
              "after announcement, number of servers with specific name, "
              "port and version should increase by 1", thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    unsigned short result;
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                             "http://0.0.0.0:" PORT_STR(PORT) "/health",
                             &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    int count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0",
                  "", port, NULL, NULL, thread_num);
}


/* 18. If was passed "0.0.0.0" as IP, change 0.0.0.0 to real IP */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithIP(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host: "
              "it should be sent as-is to LBOS", thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"1.2.3.4\"", thread_num);
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                             s_FakeGetLocalHostAddress<true, 1, 2, 3, 4>);
    WRITE_LOG("Mocking Announce with fake Anonounce", thread_num);
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    unsigned short result;
    const char* health = "http://0.0.0.0:" PORT_STR(PORT) "/health";
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                         health, &lbos_answer.Get(), &lbos_status_message.Get(),
                         thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSDNSResolveError);
    stringstream healthcheck;
    healthcheck << "http%3A%2F%2F1.2.3.4%3A" PORT_STR(PORT) "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL(s_LBOS_hostport, healthcheck.str().c_str());
    lbos_answer = NULL;
    lbos_status_message = NULL;

    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"251.252.253.147\"", thread_num);
    mock1 = s_FakeGetLocalHostAddress<true, 251, 252, 253, 147>;
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                "http://0.0.0.0:" PORT_STR(PORT) "/health",
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSDNSResolveError);
    healthcheck.str(std::string());
    healthcheck << "http%3A%2F%2F251.252.253.147%3A" PORT_STR(PORT) 
                   "%2Fhealth" << "%2Fport" << port <<
                   "%2Fhost%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL(s_LBOS_hostport, healthcheck.str().c_str());
    /* Cleanup*/
    WRITE_LOG("Reverting mock of SOCK_gethostbyaddr", 
              thread_num);
    WRITE_LOG("Reverting mock of Announce with fake Anonounce", thread_num);
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP - return kLBOSDNSResolveError                                       */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__ReturnDNSError(int thread_num = -1)
{
    CLBOSStatus         lbos_status(true, true);
    CCObjHolder<char>   lbos_answer(NULL);
    CCObjHolder<char>   lbos_status_message(NULL);
    string node_name =  s_GenerateNodeName();
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host, "
              "and running SOCK_gethostbyaddr returns error: "
              "do not care, because we do not substitute 0.0.0.0, "
              "we send it as it is", thread_num);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"0.0.0.0\"", thread_num);
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                              s_FakeGetLocalHostAddress<false,0,0,0,0>);
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSDNSResolveError);
    /* Cleanup*/
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL,
                  thread_num);
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"0.0.0.0\"", 
              thread_num);
}


/* 20. LBOS is OFF - return kLBOSOff                                          */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnKLBOSOff(int thread_num = -1)
{
    CCObjHolder<char> lbos_answer(NULL);
    CLBOSStatus lbos_status(true, false);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = 8080;
    unsigned short result;
    WRITE_LOG("LBOS mapper is OFF (maybe it is not turned ON in registry " 
              "or it could not initialize at start) - return kLBOSOff", 
              thread_num);
    result = s_AnnounceC("lbostest", "1.0.0", "", port,
                            "http://0.0.0.0:" PORT_STR(PORT) "/health",
                            &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSOff);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "Answer from LBOS was not NULL");
}


/*21. Announced successfully, but LBOS return corrupted answer -
      return 454                                                              */
/* Test is NOT thread-safe. */
void LBOSAnnounceCorruptOutput__Return454(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announced successfully, but LBOS returns corrupted answer - "
              "return kLBOSCorruptOutput",
              thread_num);
    WRITE_LOG("Mocking CONN_Read with corrupt output", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSCorruptOutput);
    /* Cleanup */
    WRITE_LOG("Reverting mock of CONN_Read with corrupt output", thread_num);
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return code from LBOS (200). If healthcheck is at non-existent domain - 
      return 400                                                              */
/* Test is thread-safe. */
void HealthcheckDead__ReturnKLBOSSuccess(int thread_num = -1) 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    unsigned short result;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "return code from LBOS(200). If healthcheck is at non - existent "
              "domain - return 400",thread_num);
    /*
     * I. Healthcheck is dead completely 
     */
     WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
                "return  kLBOSBadRequest",thread_num);
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                  "http://badhealth.gov", 
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSBadRequest);
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    /* Cleanup */
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL,
                  thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * II. Healthcheck returns 404
     */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "return  kLBOSSuccess",thread_num);
     result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                          "http://0.0.0.0:4097/healt", //wrong port
                          &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer), 
                           "Answer from LBOS was NULL");
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL,
                  thread_num);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should be announced                                         */
/* Test is thread-safe. */
void HealthcheckDead__AnnouncementOK(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "server should be announced",thread_num);
    
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                "http://0.0.0.0:4097/healt",  //wrong port
                &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    stringstream healthcheck;
    healthcheck << ":4097/healt" << "?port=" << port <<
                   "&host=" << "" << "&version=1.0.0";
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port,
                                              healthcheck.str().c_str()),
                                              "Service was not announced");
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL,
                  thread_num);
}


} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return kLBOSSuccess                                       */
void ParamsGood__ReturnSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    WRITE_LOG("Simple announcement from registry. "
              "Should return kLBOSSuccess", kSingleThreadNumber);
    result = s_AnnounceCRegistry(NULL, 
                                 &lbos_answer.Get(), &lbos_status_message.Get(),
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer), 
                           "Successful announcement did not end up with "
                           "answer from LBOS");
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    //SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    s_DeannounceAll(kSingleThreadNumber);
}

/*  2.  Custom section has nothing in config - return kLBOSInvalidArgs      */
void CustomSectionNoVars__ReturnInvalidArgs() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    WRITE_LOG("Custom section has nothing in config - "
              "return kLBOSInvalidArgs", kSingleThreadNumber);
    result = s_AnnounceCRegistry("EMPTY_SECTION",
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get(), 
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}

/*  3.  Section empty or NULL (should use default section and return 
        kLBOSSuccess, if section is Good)                                   */
void CustomSectionEmptyOrNullAndDefaultSectionIsOk__ReturnSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    WRITE_LOG("Section empty or NULL - should use default section and return "
              "kLBOSSuccess, if section is Good)", kSingleThreadNumber);
    /*
     * I. NULL section
     */
    result = s_AnnounceCRegistry(NULL, &lbos_answer.Get(), &lbos_status_message.Get(),
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer), 
                           "Successful announcement did not end up with "
                           "answer from LBOS");
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceAll(kSingleThreadNumber);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* 
     * II. Empty section 
     */
    result = s_AnnounceCRegistry("", &lbos_answer.Get(), &lbos_status_message.Get(),
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer), 
                           "Successful announcement did not end up with "
                           "answer from LBOS");
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceAll(kSingleThreadNumber);
    lbos_answer = NULL;
}


void TestNullOrEmptyField(const char* field_tested)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short result;
    CCObjHolder<char> lbos_status_message(NULL);
    string null_section = "SECTION_WITHOUT_";
    string empty_section = "SECTION_WITH_EMPTY_";
    string field_name = field_tested;
    /* 
     * I. NULL section 
     */
    WRITE_LOG("Part I. " << field_tested << " is not in section (NULL)", 
              kSingleThreadNumber);
    result = s_AnnounceCRegistry((null_section + field_name).c_str(),
                                       &lbos_answer.Get(), &lbos_status_message.Get(), 
                                       kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    //SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* 
     * II. Empty section 
     */
    WRITE_LOG("Part II. " << field_tested << " is an empty string", 
              kSingleThreadNumber);
    result = s_AnnounceCRegistry((empty_section + field_name).c_str(),
                                   &lbos_answer.Get(), &lbos_status_message.Get(), 
                                   kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}

/*  4.  Service is empty or NULL - return kLBOSInvalidArgs                  */
void ServiceEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return kLBOSInvalidArgs                  */
void VersionEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Version is empty or NULL - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return kLBOSInvalidArgs                     */
void PortEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Port is empty or NULL - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return kLBOSInvalidArgs                      */
void PortOutOfRange__ReturnInvalidArgs() 
{
    WRITE_LOG("Port is out of range - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    /*
     * I. port = 0 
     */
    WRITE_LOG("Part I. Port is 0", kSingleThreadNumber);
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1",
                                  &lbos_answer.Get(), &lbos_status_message.Get(), 
                                  kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * II. port = 100000 
     */
    WRITE_LOG("Part II. Port is 100000", kSingleThreadNumber);
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2",
                                    &lbos_answer.Get(), &lbos_status_message.Get(),
                                    kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * III. port = 65536 
     */
    WRITE_LOG("Part III. Port is 65536", kSingleThreadNumber);
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3",
                                   &lbos_answer.Get(), &lbos_status_message.Get(),
                                   kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}

/*  8.  Port contains letters - return kLBOSInvalidArgs                     */
void PortContainsLetters__ReturnInvalidArgs() 
{
    WRITE_LOG("Port contains letters - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    result = s_AnnounceCRegistry("SECTION_WITH_CORRUPTED_PORT",
                                 &lbos_answer.Get(), &lbos_status_message.Get(),
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;    
}

/*  9.  Healthcheck is empty or NULL - return kLBOSInvalidArgs              */
void HealthchecktEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Healthcheck is empty or NULL - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    TestNullOrEmptyField("HEALTHCHECK");
}

/*  10. Healthcheck does not start with http:// or https:// - return         
        kLBOSInvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs() 
{
    WRITE_LOG("Healthcheck does not start with http:// or https:// - return "      
              "kLBOSInvalidArgs", kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    result = s_AnnounceCRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK",
                                   &lbos_answer.Get(), &lbos_status_message.Get(),
                                   kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;    
}
/*  11. Trying to announce server providing dead healthcheck URL -
        return kLBOSSuccess (previously was kLBOSNotFound)                    */
void HealthcheckDead__ReturnKLBOSSuccess()
{
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "return kLBOSSuccess(previously was kLBOSNotFound)", 
              kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    /* 1. Non-existent domain in healthcheck */
    WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
              "return  kLBOSBadRequest", kSingleThreadNumber);
    result = s_AnnounceCRegistry("SECTION_WITH_HEALTHCHECK_DNS_ERROR",
                                 &lbos_answer.Get(), &lbos_status_message.Get(),
                                 kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSBadRequest);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    NCBITEST_CHECK_EQUAL(*lbos_answer, (char*)NULL);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* 2. Healthcheck is reachable but does not answer */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "return  kLBOSSuccess", kSingleThreadNumber);
     result = s_AnnounceCRegistry("SECTION_WITH_DEAD_HEALTHCHECK",
                                    &lbos_answer.Get(), &lbos_status_message.Get(),
                                    kSingleThreadNumber);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer), 
                           "Answer from LBOS was NULL");
    /* Cleanup */
    s_DeannounceAll(kSingleThreadNumber);
    lbos_answer = NULL;  
    lbos_status_message = NULL;
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully de-announced: return kLBOSSuccess                          */
/*    Test is thread-safe. */
void Deannounced__Return1(unsigned short port, int thread_num = -1)
{
    WRITE_LOG("Successfully de-announced: return kLBOSSuccess",thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_status_message(NULL);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short result;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /* 
     * I. Check with 0.0.0.0
     */
    WRITE_LOG("Part I. 0.0.0.0", thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  "http://0.0.0.0:" PORT_STR(PORT) "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1, 
                                                    kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);

    result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                           &lbos_answer.Get(), &lbos_status_message.Get(), 
                           thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* 
     * II. Now check with IP instead of 0.0.0.0 
     */
    WRITE_LOG("Part II. IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + s_GetMyIP() + 
                    ":" PORT_STR(PORT) "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Count how many servers there are */
    count_after = s_CountServersWithExpectation(node_name, port, 1, 
                                                kDiscoveryDelaySec, thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);

    result = s_DeannounceC(node_name.c_str(), 
                           "1.0.0", s_GetMyIP().c_str(), port,
                           &lbos_answer.Get(), &lbos_status_message.Get(), 
                           thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 2. Successfully de-announced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved(int thread_num = -1)
{
    WRITE_LOG("Successfully de-announced : if announcement was saved in local "
              "storage, remove it",thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    unsigned short result;
    /* 
     * I. Check with hostname
     */
    WRITE_LOG("Part I. Check with hostname", kSingleThreadNumber);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                   (string("http://") + s_GetMyHost() + ":8080/health").c_str(),
                   &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyHost(),
                              thread_num),
       1);
    result = s_DeannounceC(node_name.c_str(), 
                           "1.0.0",
                           s_GetMyHost().c_str(),
                           port, &lbos_answer.Get(), 
                           &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");

    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyHost(),
            thread_num), 
       0);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /* 
     * II. Now check with IP instead of host name 
     */
    WRITE_LOG("Part II. Check with IP", kSingleThreadNumber);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + s_GetMyIP() + ":8080/health").c_str(), 
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(
       s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyIP(), thread_num), 
       1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Deannounce */
    result = s_DeannounceC(node_name.c_str(), "1.0.0",
                           s_GetMyIP().c_str(), port,
                           &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    NCBITEST_CHECK_EQUAL(
       s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyIP(), thread_num), 
       0);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
    * III. Now check with "0.0.0.0"
    */
    WRITE_LOG("Part III. Check with 0.0.0.0", kSingleThreadNumber);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    "http://0.0.0.0:8080/health",
                    &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name, "1.0.0", port, "0.0.0.0", thread_num),
        1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Deannounce */
    result = s_DeannounceC(node_name.c_str(), "1.0.0",
                           "", port,
                           &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    NCBITEST_CHECK_EQUAL(
                s_FindAnnouncedServer(node_name, "1.0.0", port, "0.0.0.0", 
                    thread_num),
                0);
}


/* 3. Could not connect to provided LBOS: fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0(int thread_num = -1)
{
    WRITE_LOG("Could not connect to provided LBOS: fail and return 0",
              thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read with Read_Empty", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                           &lbos_answer.Get(), &lbos_status_message.Get(), 
                           thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSNoLBOS);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
    WRITE_LOG("Reverting mock of CONN_Read with Read_Empty", thread_num);
}


/* 4. Successfully connected to LBOS, but deannounce returned 400: 
 *    return 400                                                             */
/* Test is thread-safe. */
void LBOSExistsDeannounce400__Return400(int thread_num = -1)
{
    WRITE_LOG("Successfully connected to LBOS, but deannounce returned 400: "
              "return 400", thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Currently LBOS does not return any errors */
    /* Here we can try to deannounce something non-existent */
    unsigned short result;
    unsigned short port = kDefaultPort;
    result =  s_DeannounceC("no such service", "no such version",
                            "127.0.0.1", port,
                            &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, 400);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 5. Real - life test : after de-announcement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce(int thread_num = -1)
{
    WRITE_LOG("Real-life test : after de-announcement server should "
              "be invisible to resolve", thread_num);
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    Deannounced__Return1(port);
    int count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
}


/*6. If trying to deannounce in another domain - should return kLBOSNoLBOS */
/* We fake our domain so no address looks like our own domain */
/* Test is NOT thread-safe. */
void ForeignDomain__DoNothing(int thread_num = -1)
{
#if 0 /* deprecated */
    /* Test is not run in TeamCity*/
    if (!getenv("TEAMCITY_VERSION")) {
        WRITE_LOG("Deannounce in another domain - return kLBOSNoLBOS", 
                  thread_num);
        CLBOSStatus lbos_status(true, true);
        CCObjHolder<char> lbos_answer(NULL);
        CCObjHolder<char> lbos_status_message(NULL);
        unsigned short result;
        string node_name = s_GenerateNodeName();
        unsigned short port = kDefaultPort;
        WRITE_LOG("Mocking region with \"or-wa\"", thread_num);
        CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
        result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                               &lbos_answer.Get(), &lbos_status_message.Get(), 
                               thread_num);
        NCBITEST_CHECK_EQUAL(result, kLBOSNoLBOS);
        NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                             "LBOS status message is not NULL");
        NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                               "Answer from LBOS was not NULL");
        /* Cleanup*/
        WRITE_LOG("Reverting mock of region with \"or-wa\"", thread_num);
    }
#endif
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
/*  NOT ON WINDOWS. */
void NoHostProvided__LocalAddress(int thread_num = -1)
{
    WRITE_LOG("Deannounce without IP specified - deannounce from local host", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    unsigned short result;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    stringstream healthcheck;
    healthcheck << ":4097/healt" << "?port=" << port <<
                   "&host=" << "cnn.com" << "&version=" << "1.0.0";
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced                     */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  "http://0.0.0.0:" PORT_STR(PORT) "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    healthcheck.str(std::string());
    healthcheck << ":" PORT_STR(PORT) "/health" << "?port=" << port <<
                   "&host=" << "" << "&version=1.0.0";
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              healthcheck.str().c_str()),
                           "Service was not announced");
    result = 
        s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                               ":" PORT_STR(PORT) "/health"),
                                            false);
    NCBITEST_CHECK_EQUAL(result, kLBOSSuccess);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}


/* 8. LBOS is OFF - return kLBOSOff                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnKLBOSOff(int thread_num = -1)
{
    WRITE_LOG("Deannonce when LBOS mapper is OFF - return kLBOSOff", 
              thread_num);
    CLBOSStatus lbos_status(true, false);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result = 
        s_DeannounceC("lbostest", "1.0.0", "", 8080, 
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSOff);
    NCBITEST_CHECK_MESSAGE(*lbos_status_message == NULL, 
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}
/* 9. Trying to deannounce non-existent service - return kLBOSNotFound      */
/*    Test is thread-safe. */
void NotExists__ReturnKLBOSNotFound(int thread_num = -1)
{
    WRITE_LOG("Deannonce non-existent service - return kLBOSNotFound", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result = 
        s_DeannounceC("notexists", "1.0.0", 
                        "", 8080, 
                        &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
    NCBITEST_CHECK_EQUAL(result, kLBOSNotFound);
    NCBITEST_CHECK_EQUAL(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Not Found");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "Answer from LBOS was not NULL");
}
}
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in LBOS.
      No multithread!                                                         */
void AllDeannounced__NoSavedLeft(int thread_num = -1)
{
    WRITE_LOG("DeannonceAll - should deannounce everyrithing that was announced", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Part I. Announcing", thread_num);
    for (i = 0;  i < 10;  i++) {
        int count_before;
        s_SelectPort(count_before, node_name, port, thread_num);
        ports.push_back(port);
        counts_before.push_back(count_before);
        s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", ports[i],
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get(), thread_num);
        lbos_answer = NULL;
        lbos_status_message = NULL;
    }
    WRITE_LOG("Part II. DeannounceAll", thread_num);
    s_DeannounceAll(kSingleThreadNumber);

    WRITE_LOG("Part III. Checking discovery - should find nothing", thread_num);
    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(
            s_CountServersWithExpectation(
            s_GenerateNodeName(), ports[i], counts_before[i], kDiscoveryDelaySec,
                 thread_num));
        NCBITEST_CHECK_EQUAL(counts_before[i], counts_after[i]);
    }
}
}



// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing simple announce test. Should return 200.", thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /*
    * I. Check with 0.0.0.0
    */
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           "http://0.0.0.0:" PORT_STR(PORT)
                                           "/health",
                                           thread_num));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port, 
                                         thread_num));

    /* Now check with IP instead of host name */
    /*
     * II. Now check with IP
     */
    WRITE_LOG("Part II: real IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    string health = string("http://") + s_GetMyIP() + ":8080/health";
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port, health,
                                             thread_num));
    /* Count how many servers there are */
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(), port,
                                          thread_num));
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden LBOS mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved(int thread_num = -1)
{
    WRITE_LOG("Testing saving of parameters of announced server when "
              "announcement finished successfully", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    WRITE_LOG("Part I : 0.0.0.0", thread_num);
    s_SelectPort(count_before, node_name, port, thread_num);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                         "http://0.0.0.0:" PORT_STR(PORT) "/health", thread_num));
    NCBITEST_CHECK_MESSAGE(s_FindAnnouncedServer(
                               node_name.c_str(), "1.0.0", port, 
                               "0.0.0.0", thread_num) != -1,
                           "Announced server was not found in storage");
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0",
                                         "", port, thread_num));
    /* Now check with IP instead of host name */
    WRITE_LOG("Part II: real IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          string("http://") + s_GetMyIP() + ":8080/health",
                          thread_num));
    NCBITEST_CHECK_MESSAGE(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0",
        port, s_GetMyIP().c_str(), thread_num) != -1,
        "Announced server was not found in storage");
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port,
                                         thread_num));
}


/*  5. Could not find LBOS: throw exception NO_LBOS                          */
/* Test is NOT thread-safe. */
void NoLBOS__ThrowNoLBOSAndNotFind(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found.", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSNoLBOS" <<
                "\", status code \"" << 450 << 
                "\", message \"" << "450\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSNoLBOS, 450> comparator("450\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port,
                          "http://0.0.0.0:" PORT_STR(PORT) "/health", thread_num),
                          CLBOSException, comparator);

    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()", thread_num);
}


/*  8. LBOS returned unknown error: return its code                          */
/* Test is NOT thread-safe. */
void LBOSError__ThrowServerError(int thread_num = -1)
{
    WRITE_LOG("LBOS returned unknown error: "
                " Should throw exception with received code (507)", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSUnknown" <<
                "\", status code \"" << 507 << 
                "\", message \"" << 
                "507 LBOS STATUS Those lbos errors are scaaary\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSUnknown, 507> comparator(
        "507 LBOS STATUS Those lbos errors are scaaary\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_FakeReadAnnouncementWithErrorFromLBOS);
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port,
                          "http://0.0.0.0:" PORT_STR(PORT) "/health",
                          thread_num),
            CLBOSException, comparator);
}


/*  9. LBOS returned error : char* lbos_answer contains answer of LBOS       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided(int thread_num = -1)
{
    WRITE_LOG("LBOS returned unknown error: "
                " Exact message from LBOS should be provided", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSUnknown" <<
                "\", status code \"" << 507 << 
                "\", message \"" << 
                "507 LBOS STATUS Those lbos errors are scaaary\\n" << "\".",
              thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    try {
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num);
    }
    catch(const CLBOSException& ex) {
        /* Checking that message in exception is exactly what LBOS sent*/
        NCBITEST_CHECK_MESSAGE(
            ex.GetErrCode() == CLBOSException::e_LBOSUnknown,
            "LBOS exception contains wrong error type");
        const char* ex_message =
            strstr(ex.what(), "Error: ") + strlen("Error: ");
        string message = "";
        message.append(ex_message);
        NCBITEST_CHECK_EQUAL(
            message, string("507 LBOS STATUS Those lbos errors are scaaary\n"));
        return;
    } 
    catch (...) {
        NCBITEST_CHECK_MESSAGE(false, "Wrong exception was generated");
        return;
    }
    NCBITEST_CHECK_MESSAGE(false, "No exception was generated");
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS "
                 "mapper when server was already announced (info stored in "
                 "internal storage should be replaced. Server node should be "
                 "rewritten, no duplicates.", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    //SleepMilliSec(1500); //ZK is not that fast
    /*
     * First time
     */
    WRITE_LOG("Part 1. First time announcing server", thread_num);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           "http://0.0.0.0:" PORT_STR(PORT)
                                           "/health",
                                           thread_num));
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    NCBITEST_CHECK_EQUAL(s_FindAnnouncedServer(node_name, "1.0.0", port,
                         "0.0.0.0", thread_num), 1);
    /*
     * Second time
     */
    WRITE_LOG("Part 2. Second time announcing server", thread_num);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          "http://0.0.0.0:" PORT_STR(PORT) "/health",
                          thread_num));
    /* Count how many servers there are.  */
    count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(s_FindAnnouncedServer(node_name, "1.0.0", port,
                         "0.0.0.0", thread_num), 1);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    /* Cleanup */     
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port, thread_num));
}


/* 12. Trying to announce in another domain - do nothing                     */
/* Test is NOT thread-safe. */
void ForeignDomain__NoAnnounce(int thread_num = -1)
{
#if 0 /* deprecated */
    /* Test is not run in TeamCity*/
    if (!getenv("TEAMCITY_VERSION")) {
        ExceptionComparator<CLBOSException::e_LBOSNoLBOS, 450> comparator("450\n");
        CLBOSStatus lbos_status(true, true);
        unsigned short port = kDefaultPort;
        string node_name = s_GenerateNodeName();
        WRITE_LOG("Testing behavior of LBOS mapper when no LBOS "
                  "is available in the current region (should "
                  "return error code kLBOSNoLBOS", thread_num);
        WRITE_LOG("Mocking region with \"or-wa\"", thread_num);
        CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
        WRITE_LOG("Expected exception with error code \"" << "e_LBOSNoLBOS" <<
            "\", status code \"" << 450 <<
                    "\", message \"" << "450\\n" << "\".",
                  thread_num);
        BOOST_CHECK_EXCEPTION(
                s_AnnounceCPP(node_name, "1.0.0", "", port,
                              "http://0.0.0.0:" PORT_STR(PORT) "/health", 
                              thread_num),
                              CLBOSException, comparator);
    }
#endif
}


/* 13. Was passed incorrect healthcheck URL (empty, not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect healthcheck URL", thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              thread_num);
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that does not start with http or https
     */
    WRITE_LOG("Part I. Healthcheck is <NULL>", thread_num);
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, "0.0.0.0:8080/health",
                      thread_num), CLBOSException, comparator);
    /*
     * II. Empty healthcheck URL
     */
    WRITE_LOG("Part II. Healthcheck is \"\" (empty string)", thread_num);
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port, "", thread_num),
            CLBOSException, comparator);
}


/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ThrowInvalidArgs(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect port (zero)", thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /* Count how many servers there are before we announce */
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", 0,
                          "http://0.0.0.0:8080/health",
                          thread_num), CLBOSException, comparator);
}


/* 15. Was passed incorrect version(empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ThrowInvalidArgs(int thread_num = -1)
{
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect version - should return "
             "kLBOSInvalidArgs", thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    node_name = s_GenerateNodeName();
    WRITE_LOG("Version is \"\" (empty string)", thread_num);
    /*
     * I. Empty string
     */
    string health = "http://0.0.0.0:" PORT_STR(PORT) "/health";
    stringstream healthcheck;
    healthcheck << health << "/port" << port << "/host" << "" <<
                   "/version" << "";
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "", "", port,
                       healthcheck.str()),
        CLBOSException, comparator);
}


/* 16. Was passed incorrect service name (empty): do not 
 *     announce and return INVALID_ARGS                                      */
/* Test is thread-safe. */
void IncorrectServiceName__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect service name - should return "
              "kLBOSInvalidArgs", thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              thread_num);
    /*
     * I. Empty service name
     */
    WRITE_LOG("Service name is \"\" (empty string)", thread_num);
    node_name = s_GenerateNodeName();
    port = kDefaultPort;
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP("", "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
}


/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    unsigned short port = kDefaultPort;
    string node_name = s_GenerateNodeName();
    WRITE_LOG("Real-life test of LBOS: "
              "after announcement, number of servers with specific name, "
              "port and version should increase by 1", thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                       "http://0.0.0.0:" PORT_STR(PORT) "/health", thread_num));
    int count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port,
                                         thread_num));
}


/* 18. If was passed "0.0.0.0" as IP, we change  0.0.0.0 to real IP           */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithIP(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSDNSResolveError, 451> comparator
                                                                ("451\n");
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                           */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host: "
        "it should be sent as-is to LBOS", thread_num);
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"1.2.3.4\"", thread_num);
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                             s_FakeGetLocalHostAddress<true, 1, 2, 3, 4>);
    WRITE_LOG("Mocking Announce with fake Anonounce", thread_num);
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,     
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
    stringstream healthcheck;
    healthcheck << "http%3A%2F%2F1.2.3.4%3A" PORT_STR(PORT) "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost" << "" << "%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL(s_LBOS_hostport, healthcheck.str().c_str());
    mock1 = s_FakeGetLocalHostAddress<true, 251,252,253,147>;
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, 
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
    healthcheck.str(std::string());
    healthcheck << "http%3A%2F%2F251.252.253.147%3A" PORT_STR(PORT) 
                   "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost" << "" << "%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL(s_LBOS_hostport, healthcheck.str().c_str());
    /* Cleanup*/
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"1.2.3.4\"",
        thread_num);
    WRITE_LOG("Reverting mock of Announce with fake Anonounce", thread_num);
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP - do not announce and return DNS_RESOLVE_ERROR                      */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__ReturnDNSError(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSDNSResolveError, 451> comparator
                                                                ("451\n");
    CLBOSStatus lbos_status(true, true);
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host, "
              "and running SOCK_gethostbyaddr returns error: "
              "do not care, because we do not substitute 0.0.0.0, "
              "we send it as it is", thread_num);
    /* Here we mock SOCK_gethostbyaddrEx to know IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    string node_name     = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"0.0.0.0\"", thread_num);
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                              s_FakeGetLocalHostAddress<false,0,0,0,0>);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
    /* Cleanup*/
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"0.0.0.0\"", 
              thread_num);
}

/* 20. LBOS is OFF - return kLBOSOff                                        */
/* Test is NOT thread-safe. */
void LBOSOff__ThrowKLBOSOff(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSOff, 550> comparator("550\n");
    CLBOSStatus lbos_status(true, false);
    WRITE_LOG("LBOS mapper is OFF (maybe it is not turned ON in registry " 
              "or it could not initialize at start) - return kLBOSOff", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSOff" <<
              "\", status code \"" << 550 <<
                "\", message \"" << "550\\n" << "\".",
              thread_num);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP("lbostest", "1.0.0", "", 8080,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
}


/*21. Announced successfully, but LBOS return corrupted answer -
      return 454 and exact answer of LBOS                                    */
void LBOSAnnounceCorruptOutput__ThrowServerError(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_LBOSCorruptOutput, 454> comparator
                                                      ("454 Corrupt output\n");
    WRITE_LOG("Announced successfully, but LBOS returns corrupted answer.",
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSCorruptOutput" << 
                "\", status code \"" << 454 <<
                "\", message \"" << "454 Corrupt output\\n" << "\".",
              thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read with corrupt output", thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num), CLBOSException, comparator);
    /* Cleanup */
    WRITE_LOG("Reverting mock of CONN_Read with corrupt output", thread_num);
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return kLBOSNotFound                                                  */
void HealthcheckDead__ThrowE_NotFound(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    int count_before;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "return code from LBOS(200). If healthcheck is at non - existent "
              "domain - throw exception",thread_num);
    /*
     * I. Healthcheck is on non-existent domain
     */
     WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
                "return  kLBOSBadRequest",thread_num);
    s_SelectPort(count_before, node_name, port, thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSBadRequest, 400> comparator
                                                      ("400 Bad Request\n");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, "http://badhealth.gov",
                      thread_num), CLBOSException, comparator);
    int count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);

    /*
     * II. Healthcheck returns 404
     */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "should work fine",thread_num);
    s_SelectPort(count_before, node_name, port, thread_num);
//  ExceptionComparator<CLBOSException::e_LBOSNotFound, 404> comparator2
//                                                       ("404 Not Found\n");
    BOOST_CHECK_NO_THROW(                    //  missing 'h'
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,// v
                          "http://0.0.0.0:" PORT_STR(PORT)
                          "/healt", thread_num));
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "cnn.com", port, &lbos_answer.Get(),
                  NULL, thread_num);
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should be announced                                             */
void HealthcheckDead__AnnouncementOK(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "server should be announced", thread_num);
    /* We do not care if announcement throws or not (we check it in other 
       tests), we check if server is announced */
    try {
        s_AnnounceCPP(node_name, "1.0.0", "", port, "http://0.0.0.0:8080/healt",
                      thread_num);
    }
    catch(...)
    {
    }
    stringstream healthcheck;
    healthcheck << ":8080/healt" << "?port=" << port <<
                   "&host=" << "" << "&version=" << "1.0.0";
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port,
                                              healthcheck.str().c_str()), 
                           "Service was not announced");
}


/* 24. Announce server with separate host and healthcheck - should be found in
*     %LBOS%/text/service                                                   */
void SeparateHost__AnnouncementOK(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server with separate host that is not the "
              "same as healtcheck host - return code from LBOS(200).",
              thread_num);
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);

    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "cnn.com", port,
                                           "http://0.0.0.0:" PORT_STR(PORT)
                                           "/health",
                                           thread_num));
    int count_after = s_CountServersWithExpectation(node_name, port, 1,
        kDiscoveryDelaySec,
        thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);

    stringstream healthcheck;
    healthcheck << ":4097/healt" << "?port=" << port <<
                   "&host=" << "cnn.com" << "&version=1.0.0";
    string announced_ip = CLBOSIpCache::HostnameTryFind(node_name, "cnn.com", 
                                                    "1.0.0", port);
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port,
                           healthcheck.str().c_str(), true,
                           announced_ip),
                           "Service was not announced");
    lbos_answer = NULL;
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name.c_str(), "1.0.0",
                                         "cnn.com", port,
                                         thread_num));
}
} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry_CXX /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Default section has all parameters correct in 
        config) - return kLBOSSuccess                                        */
void ParamsGood__ReturnSuccess() 
{
    WRITE_LOG("Simple announcement from registry. "
              "Should work fine", kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPFromRegistry(string()));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}

/*  2.  Custom section has nothing in config - return kLBOSInvalidArgs       */
void CustomSectionNoVars__ThrowInvalidArgs() 
{
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator("452\n");
    WRITE_LOG("Testing custom section that has nothing in config", 
              kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("NON-EXISTENT_SECTION"),
        CLBOSException, comparator);
}

/*  3.  Section empty (should use default section and return 
        kLBOSSuccess, if section is Good)                                    */
void CustomSectionEmptyOrNullAndSectionIsOk__AllOK() 
{
    CLBOSStatus lbos_status(true, true);
    WRITE_LOG("Section empty or NULL - should use default section all work "
              "fine, if default section is Good", kSingleThreadNumber);
    /* 
     * Empty section 
     */
    BOOST_CHECK_NO_THROW(s_AnnounceCPPFromRegistry(""));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}


void TestNullOrEmptyField(const char* field_tested)
{
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    string null_section = "SECTION_WITHOUT_";
    string empty_section = "SECTION_WITH_EMPTY_";
    string field_name = field_tested;
    /* 
     * I. NULL section 
     */
    WRITE_LOG("Part I. " << field_tested << " is not in section (NULL)", 
              kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry((null_section + field_name)),
        CLBOSException, comparator);
    /* 
     * II. Empty section 
     */
    WRITE_LOG("Part II. " << field_tested << " is an empty string", 
              kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry((empty_section + field_name)),
        CLBOSException, comparator);
}

/*  4.  Service is empty or NULL - return kLBOSInvalidArgs                  */
void ServiceEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL", 
              kSingleThreadNumber);
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return kLBOSInvalidArgs                  */
void VersionEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Version is empty or NULL", 
              kSingleThreadNumber);
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return kLBOSInvalidArgs                     */
void PortEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL", 
              kSingleThreadNumber);
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return kLBOSInvalidArgs                      */
void PortOutOfRange__ThrowInvalidArgs() 
{
    WRITE_LOG("Port is out of range - return kLBOSInvalidArgs", 
              kSingleThreadNumber);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    /*
     * I. port = 0 
     */
    WRITE_LOG("Part I. Port is 0", kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1"),
        CLBOSException, comparator);
    /*
     * II. port = 100000 
     */
    WRITE_LOG("Part II. Port is 100000", kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    BOOST_CHECK_EXCEPTION(
       s_AnnounceCPPFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2"),
        CLBOSException, comparator);
    /*
     * III. port = 65536 
     */
    WRITE_LOG("Part III. Port is 65536", kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3"),
        CLBOSException, comparator);
}
/*  8.  Port contains letters - return kLBOSInvalidArgs                     */
void PortContainsLetters__ThrowInvalidArgs() 
{
    WRITE_LOG("Port contains letters", 
              kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator("452\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_CORRUPTED_PORT"),
        CLBOSException, comparator);
}
/*  9.  Healthcheck is empty or NULL - return kLBOSInvalidArgs              */
void HealthchecktEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Healthcheck is empty or NULL", 
              kSingleThreadNumber);
    TestNullOrEmptyField("HEALTHCHECK");
}
/*  10. Healthcheck does not start with http:// or https:// - return         
        kLBOSInvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ThrowInvalidArgs() 
{
    WRITE_LOG("Healthcheck does not start with http:// or https://",
              kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".",
              kSingleThreadNumber);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comparator
                                                                ("452\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK"),
        CLBOSException, comparator);
}
/*  11. Trying to announce server and providing dead healthcheck URL -
        return kLBOSNotFound                                                */
void HealthcheckDead__ThrowE_NotFound() 
{
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "should be OK", 
              kSingleThreadNumber);
    CLBOSStatus lbos_status(true, true);
    unsigned short port = 5001;
    string node_name = s_GenerateNodeName();
    int count_before = s_CountServers(node_name, port);
    /* 1. Non-existent domain in healthcheck */
    WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
              "return  kLBOSBadRequest", kSingleThreadNumber);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".",
              kSingleThreadNumber);
    ExceptionComparator<CLBOSException::e_LBOSBadRequest, 400> comparator
                                                          ("400 Bad Request\n");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_HEALTHCHECK_DNS_ERROR"),
                                   CLBOSException, comparator);
    int count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);


    /* 2. Healthcheck is reachable but does not answer */
    WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
              "return  kLBOSSuccess", kSingleThreadNumber);
    BOOST_CHECK_NO_THROW(
                   s_AnnounceCPPFromRegistry("SECTION_WITH_DEAD_HEALTHCHECK"));
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);

    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll()); 
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully de-announced: return 1                                     */
/*    Test is thread-safe. */
void Deannounced__Return1(unsigned short port, int thread_num = -1)
{
    WRITE_LOG("Simple deannounce test", thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name   = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    /*
    * I. Check with 0.0.0.0
    */
    WRITE_LOG("Part I. 0.0.0.0", thread_num);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          "http://0.0.0.0:" PORT_STR(PORT) "/health",
                          thread_num));
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port,
        thread_num));
    /*
    * II. Now check with IP instead of 0.0.0.0
    */
    WRITE_LOG("Part II. IP", thread_num);
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    string health =
            string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health";
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           health, thread_num));
    /* Count how many servers there are */
    count_after = s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec,
                                                thread_num);
    NCBITEST_CHECK_EQUAL(count_after, count_before + 1);

    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(), port, thread_num));
}


/* 2. Successfully de-announced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved(int thread_num = -1)
{
    WRITE_LOG("Successfully de-announced : if announcement was saved in local "
              "storage, remove it", thread_num);
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    string my_host = s_GetMyHost();
    /*
     * I. Check with hostname
     */
    WRITE_LOG("Part I. Check with hostname", kSingleThreadNumber);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          string("http://") + s_GetMyHost() + (":8080/health"),
                          thread_num));
    /* Check that server is in storage */
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, s_GetMyIP(),
                              thread_num), 1);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", s_GetMyHost(), port, thread_num));
    /* Check that server was removed from storage */
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, s_GetMyIP(),
                              thread_num), 0);

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    s_SelectPort(count_before, node_name, port, thread_num);
    /*
     * II. Now check with IP instead of host name
     */
    WRITE_LOG("Part II. Check with IP", kSingleThreadNumber);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          string("http://") + s_GetMyIP() + ":8080/health",
                          thread_num));
    /* Check that server is in storage */
    NCBITEST_CHECK_NE(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, s_GetMyIP(),
                              thread_num), -1);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(), port, thread_num));
    /* Check that server was removed from storage */
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, s_GetMyIP(),
                              thread_num), 0);
    /*
     * III. Check with 0.0.0.0
     */
    WRITE_LOG("Part III. Check with IP", kSingleThreadNumber);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
        "http://0.0.0.0:8080/health",
        thread_num););
    /* Check that server is in storage */
    NCBITEST_CHECK_NE(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, "0.0.0.0",
                              thread_num), 0);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port, thread_num));
    /* Check that server was removed from storage */
    NCBITEST_CHECK_EQUAL(
        s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port, "0.0.0.0",
                              thread_num), 0);
}


/* 3. Could not connect to provided LBOS : fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0(int thread_num = -1)
{
    WRITE_LOG("Could not connect to provided LBOS", thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSNoLBOS" <<
                "\", status code \"" << 450 <<
                "\", message \"" << "450\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSNoLBOS, 450> comparator("450\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP(node_name, "1.0.0", "", port, thread_num),
        CLBOSException, comparator);
}


/* 4. Successfully connected to LBOS, but deannounce returned error: 
 *    return 0                                                               */
/* Test is thread-safe. */
void LBOSExistsDeannounceError__Return0(int thread_num = -1)
{
    WRITE_LOG("Could not connect to provided LBOS", thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSBadRequest, 400> comparator
                                                       ("400 Bad Request\n");
    CLBOSStatus lbos_status(true, true);
    /* Currently LBOS does not return any errors */
    /* Here we can try to deannounce something non-existent */
    unsigned short port = kDefaultPort;
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("no such service", "no such version", "127.0.0.1", 
                         port, thread_num), 
        CLBOSException, comparator);
}


/* 5. Real-life test: after de-announcement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce(int thread_num = -1)
{
    WRITE_LOG("Real-life test : after de-announcement server should "
              "be invisible to resolve", thread_num);
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    //SleepMilliSec(1500); //ZK is not that fast
    Deannounced__Return1(port);
    //SleepMilliSec(1500); //ZK is not that fast
    int count_after = s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec,
                                                    thread_num);
    NCBITEST_CHECK_EQUAL(count_after - count_before, 0);
}


/* 6. If trying to deannounce in another domain - do not deannounce */
/* We fake our domain so no address looks like our own domain */
/* Test is NOT thread-safe. */
void ForeignDomain__DoNothing(int thread_num = -1)
{
#if 0 /* deprecated */
    /* Test is not run in TeamCity*/
    if (!getenv("TEAMCITY_VERSION")) {
        WRITE_LOG("Deannounce in foreign domain", thread_num);
        ExceptionComparator<CLBOSException::e_LBOSNoLBOS, 450> comparator("450\n");
        WRITE_LOG("Expected exception with error code \"" << 
                    "e_LBOSNoLBOS" <<
                    "\", status code \"" << 450 <<
                    "\", message \"" << "450\\n" << "\".",
                  thread_num);
        CLBOSStatus lbos_status(true, true);
        string node_name = s_GenerateNodeName();
        unsigned short port = kDefaultPort;
        WRITE_LOG("Mocking region with \"or-wa\"", thread_num);
        CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
        BOOST_CHECK_EXCEPTION(
            s_DeannounceCPP(node_name, "1.0.0", "", port, thread_num),
                            CLBOSException, comparator);
        /* Cleanup*/
        WRITE_LOG("Reverting mock of region with \"or-wa\"", thread_num);
    }
#endif
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
void NoHostProvided__LocalAddress(int thread_num = -1)
{
    WRITE_LOG("Deannounce without IP specified - deannounce from local host", 
              thread_num);
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    s_SelectPort(count_before, node_name, port, thread_num);
    try {
        // We get random answers of LBOS in this test, so try-catch
        // Service always get announced, though
        s_AnnounceCPP(node_name, "1.0.0", "",
                      port, "http://0.0.0.0:" PORT_STR(PORT) "/health",
                      thread_num);
    } 
    catch (...) {
    }
    stringstream healthcheck;
    healthcheck << ":" PORT_STR(PORT) "/health" << "?port=" << port <<
                   "&host=" << "" << "&version=" << "1.0.0";
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              healthcheck.str().c_str()),
                           "Service was not announced");
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port, thread_num));
    NCBITEST_CHECK_MESSAGE(!s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                               ":" PORT_STR(PORT) "/health", 
                                               false),
                           "Service was not de-announced");
}

/* 8. LBOS is OFF - return kLBOSOff                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ThrowKLBOSOff(int thread_num = -1)
{
    WRITE_LOG("Deannonce when LBOS mapper is OFF", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSOff" <<
                "\", status code \"" << 550 <<
                "\", message \"" << "550\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSOff, 550> comparator("550\n");
    CLBOSStatus lbos_status(true, false);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("lbostest", "1.0.0", "", 8080, thread_num), 
        CLBOSException, comparator);
}


/* 9. Trying to deannounce non-existent service - throw e_LBOSNotFound           */
/*    Test is thread-safe. */
void NotExists__ThrowE_NotFound(int thread_num = -1)
{
    WRITE_LOG("Deannonce non-existent service", 
              thread_num);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSNotFound" <<
                "\", status code \"" << 404 <<
                "\", message \"" << "404 Not Found\\n" << "\".",
              thread_num);
    ExceptionComparator<CLBOSException::e_LBOSNotFound, 404> comparator
                                                        ("404 Not Found\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("notexists", "1.0.0", "", 8080, thread_num),
        CLBOSException, comparator);
}

}
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in LBOS
      No Multithread!                                                        */
void AllDeannounced__NoSavedLeft(int thread_num = -1)
{
    WRITE_LOG("DeannonceAll - should deannounce everyrithing "
              "that was announced", thread_num);
    CLBOSStatus lbos_status(true, true);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Part I. Announcing", thread_num);
    for (i = 0;  i < 10;  i++) {
        int count_before;
        s_SelectPort(count_before, node_name, port, thread_num);
        ports.push_back(port);
        counts_before.push_back(count_before);
        BOOST_CHECK_NO_THROW(
            s_AnnounceCPPSafe(node_name, "1.0.0", "", ports[i],
                              "http://0.0.0.0:" PORT_STR(PORT) "/health",
                              thread_num));
    }
    WRITE_LOG("Part II. DeannounceAll", thread_num);
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    //SleepMilliSec(10000); //We need LBOS to clear cache

    WRITE_LOG("Part III. Checking discovery - should find nothing", thread_num);
    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(s_CountServers(s_GenerateNodeName(), ports[i]));
        NCBITEST_CHECK_EQUAL(counts_before[i], counts_after[i]);
    }
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Initialization
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
    /** Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
     * initialized should not crash                                          */
    void MultithreadInitialization__ShouldNotCrash()
    {
        #ifdef LBOS_TEST_MT
    CLBOSStatus lbos_status(false, false);
            GeneralLBOS::LbosExist__ShouldWork();
        #endif
    }


    /**  At initialization if no LBOS found, mapper must be turned OFF       */
    void InitializationFail__TurnOff()
    {
        CLBOSStatus lbos_status(false, true);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);

        CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<0>);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter == NULL,
                               "LBOS found when it should not be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 0,
                               "LBOS has not been shut down as it should be");
    }


    /**  At initialization if LBOS found, mapper should be ON                */
    void InitializationSuccess__StayOn()
    {
        CLBOSStatus lbos_status(false, false);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);
        
        CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<1>);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 1,
                               "LBOS is not turned ON as it should be");

    }


    /** If LBOS has not yet been initialized, it should be initialized at
     * SERV_LBOS_Open()                                                      */
    void OpenNotInitialized__ShouldInitialize()
    {
        CMockFunction<FLBOS_InitializeMethod*> mock (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize,
                                s_FakeInitialize);
        CLBOSStatus lbos_status(false, false);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                                 SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                                 NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                                 0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter == NULL,
            "Error: LBOS found when mapper is OFF");
        NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
            "Initialization was not called when it should be");

        /* Cleanup */
//         g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = temp_func_pointer;
//         SERV_Close(*iter);
    }


    /**  If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open()         */
    void OpenWhenTurnedOff__ReturnNull()
    {
        CLBOSStatus lbos_status(true, false);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);
        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                                  SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                                  NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                                  0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter == NULL,
            "SERV_LBOS_Open did not return NULL when it is disabled");

        /* Cleanup */
        *(g_LBOS_UnitTesting_PowerStatus()) = 1;
        *(g_LBOS_UnitTesting_InitStatus()) = 0;
    }


    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_Initialize()                                                  */
    void s_LBOS_Initialize__s_LBOS_InstancesListNotNULL()
    {
        CMockFunction<FLBOS_InitializeMethod*> mock (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize,
                                s_FakeInitializeCheckInstances);
        CLBOSStatus lbos_status(false, true);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_CallCounter == 1,
                      "Fake initialization was not called when it should be");

        /* Cleanup */
//         g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = temp_func_pointer;
//         SERV_Close(*iter);
    }


    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_FillCandidates()                                              */
    void s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL()
    {
        CMockFunction<FLBOS_FillCandidatesMethod*> mock (
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidatesCheckInstances);
        CLBOSStatus lbos_status(false, true);
        string service = "/lbos";
        CCounterResetter resetter(s_CallCounter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));

        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_CallCounter == 2,
            "Fill candidates was not called when it should be");
    }


    /** Template parameter instance_num
     *   LBOS instance with which number (in original order as from
     *   g_LBOS_GetLBOSAddresses()) is emulated to be working
     *   Note: it is 1-based. So first instance is 1, second is 2, etc.
     *   Note: -1 used for all instances OFF.
     *  Template parameter testnum
     *   Number of test. Used for debug output in multithread tests
     *  Template parameter predictable_leader
     *   If test is run in mode which allows to predict first address. It is
     *   either single threaded mode or test_mt with synchronization points
     *   between different tests                                             */
    template<int instance_num, int testnum, bool predictable_first>
    void SwapAddressesTest(CMockFunction<FLBOS_ResolveIPPortMethod*>& mock)
    {
        CCounterResetter resetter(s_CallCounter);
        string service = "/lbos";
        CConnNetInfo net_info(service);
        CORE_LOCK_READ;
        CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
        unsigned int addresses_count = lbos_addresses.count();
        CORE_UNLOCK;
        vector<string> addresses_control_list;
        map<string, int> before_test, 
                         after_test; /*to compare address lists*/
        /* We test that not all instances OFF and sanity test (number of 
         * requested instance is less than total amount of instances )*/
        if (instance_num != -1
                         && static_cast<int>(addresses_count) >= instance_num)
        {
            for (unsigned int i = 0;  i < addresses_count;  i++) {
                addresses_control_list.push_back(string(lbos_addresses[i]));
                before_test[string(addresses_control_list[i])]++;
            }
            mock = s_FakeResolveIPPortSwapAddresses < instance_num > ;
        } else {
            for (unsigned int i = 0; 
                    (*g_LBOS_UnitTesting_InstancesList())[i] != NULL;  i++) {
                addresses_control_list.push_back(string(
                                     (*g_LBOS_UnitTesting_InstancesList())[i]));
                before_test[string(addresses_control_list[i])]++;
            }
            mock = s_FakeResolveIPPortSwapAddresses <-1> ;
        }

        s_CallCounter = 0;

        /* We will check results after open */
        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
            
            SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
            *net_info, 0/*skip*/, 0/*n_skip*/,
            0/*external*/, 0/*arg*/, 0/*val*/));

        /* We launch */
        CORE_LOCK_READ;
        {
            for (unsigned int i = 0;  i < addresses_count;  i++) {
                lbos_addresses.set(i, 
                             strdup((*g_LBOS_UnitTesting_InstancesList())[i]));
            }
        }
        CORE_UNLOCK;

        if (predictable_first) {
            if (instance_num != -1
                          && static_cast<int>(addresses_count) >= instance_num)
            {
                CORE_LOGF(eLOG_Trace, (
                    "Test %d. Expecting `%s', reality '%s'",
                    testnum,
                    addresses_control_list[instance_num - 1].c_str(),
                    lbos_addresses[0]));
                NCBITEST_CHECK_MESSAGE(
                    string(lbos_addresses[0]) ==
                                      addresses_control_list[instance_num - 1],
                    "priority LBOS instance error");
            } else {
                CORE_LOGF(eLOG_Trace, (
                    "Test %d. Expecting `%s', reality '%s'",
                    testnum, addresses_control_list[0].c_str(),
                    lbos_addresses[0]));
                NCBITEST_CHECK_MESSAGE(
                    string(lbos_addresses[0]) == addresses_control_list[0],
                    "priority LBOS instance error");
            }
        }
        /* Actually, there can be duplicates. 
           The problem with this test is that there can be duplicates even 
           in etc/ncbi/{role, domain} and registry. So we count every address*/
        
        for (unsigned int i = 0;  i < addresses_count;  i++) {
            NCBITEST_CHECK_MESSAGE(lbos_addresses[i] != NULL,
                                    "NULL encountered after"
                                    " swapping algorithm");
            after_test[string(lbos_addresses[i])]++;
        }
        map<string, int>::iterator it;
        for (it = before_test.begin();  it != before_test.end();  ++it) {
            NCBITEST_CHECK_MESSAGE(it->second == after_test[it->first],
                                    "Duplicate after swapping algorithm");
        }
    }
    /** @brief
     * s_LBOS_FillCandidates() should switch first and good LBOS addresses,
     * if first is not responding
     *
     *  Our fake fill candidates designed that way that we can tell when to
     *  tell nothing and when to tell good through template parameter. Of
     *  course, we have to hardcode every value, but there are 7 max possible,
     *  not that much       */
    void PrimaryLBOSInactive__SwapAddresses()
    {
        CLBOSStatus lbos_status(true, true);
        // We need to first initialize mapper
        string service = "/lbos";
        /* Initialize LBOS mapper */
        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        /* Save current function pointer. It will be changed inside 
         * test functions */
        CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, 
                            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort);

        /* Pseudo random order */
        SwapAddressesTest<1, 1, false>(mock);
        SwapAddressesTest<2, 2, false>(mock);
        SwapAddressesTest<3, 3, false>(mock);
        SwapAddressesTest<1, 4, false>(mock);
        SwapAddressesTest<7, 5, false>(mock);
        SwapAddressesTest<2, 6, false>(mock);
        SwapAddressesTest<4, 7, false>(mock);
        SwapAddressesTest<1, 8, false>(mock);
        SwapAddressesTest<2, 9, false>(mock);
        SwapAddressesTest<3, 10, false>(mock);
        SwapAddressesTest<1, 11, false>(mock);
        SwapAddressesTest<7, 12, false>(mock);
        SwapAddressesTest<2, 13, false>(mock);
        SwapAddressesTest<4, 14, false>(mock);
        SwapAddressesTest<2, 15, false>(mock);
        SwapAddressesTest<6, 16, false>(mock);
        SwapAddressesTest<3, 17, false>(mock);
        SwapAddressesTest<5, 18, false>(mock);
        SwapAddressesTest<2, 19, false>(mock);
        SwapAddressesTest<1, 20, false>(mock);
        SwapAddressesTest<6, 21, false>(mock);
        SwapAddressesTest<4, 22, false>(mock);
        SwapAddressesTest<2, 23, false>(mock);
        SwapAddressesTest<6, 24, false>(mock);
        SwapAddressesTest<3, 25, false>(mock);
        SwapAddressesTest<5, 26, false>(mock);
        SwapAddressesTest<2, 27, false>(mock);
        SwapAddressesTest<1, 28, false>(mock);
        SwapAddressesTest<6, 29, false>(mock);
        SwapAddressesTest<4, 30, false>(mock);
    }
} /* namespace Initialization */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Configure
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Set version then check version - should show the version that was 
 *    just set.
 *   Test is not for multi-threading                                        */
void SetThenCheck__ShowsSetVersion()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
        
    /* Set version */
    LBOS::ServiceVersionSet(node_name, "1.0.0");

    /* Check version */
    string conf_data = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL(conf_data, "1.0.0");

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 2. Check version, then set different version, then check version -
 *    should show new version.
 *   Test is not for multi-threading                                       */
void CheckSetNewCheck__ChangesVersion()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();

    /* Check version and save it */
    string conf_data = LBOS::ServiceVersionGet(node_name);
    string prev_version = conf_data;

    /* Set different version */
    conf_data = LBOS::ServiceVersionSet(node_name, prev_version + ".0");
    NCBITEST_CHECK_EQUAL(conf_data, prev_version);

    /* Check version */
    conf_data = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL(conf_data, prev_version + ".0");

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 3. Set version, check that it was set, then delete version - check
 *    that no version exists.
 *   Test is not for multi-threading                                       */
void DeleteThenCheck__SetExistsFalse()
{
    bool exists;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    ExceptionComparator<CLBOSException::e_LBOSNotFound, 404> comp("404\n");

    /* Set version */
    LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);

    /* Delete version */
    string conf_data = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL(conf_data, "1.0.0");

    /* Check version */
    LBOS::ServiceVersionGet(node_name, &exists);
    NCBITEST_CHECK_EQUAL(exists, false);

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 4. Announce two servers with different version. First, set one version
 *    and discover server with that version. Then, set the second version
 *    and discover server with that version.
 *   Test is not for multi-threading                                      */
void AnnounceThenChangeVersion__DiscoverAnotherServer()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    unsigned short port1, port2;
    string health = 
        string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health";
    int count_before;
    s_SelectPort(count_before, node_name, port1, kSingleThreadNumber);
    s_AnnounceCPPSafe(node_name, "v1", "", port1, health.c_str(),
                      kSingleThreadNumber);

    s_SelectPort(count_before, node_name, port2, kSingleThreadNumber);
    s_AnnounceCPPSafe(node_name, "v2", "", port2, health.c_str(),
                      kSingleThreadNumber);

    /* Set first version */
    LBOS::ServiceVersionSet(node_name, "v1");
    unsigned int servers_found =
        s_CountServersWithExpectation(node_name, port1, 1, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 1U);
    servers_found =
        s_CountServersWithExpectation(node_name, port2, 0, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 0U);

    /* Set second version and discover  */
    LBOS::ServiceVersionSet(node_name, "v2");
    servers_found =
        s_CountServersWithExpectation(node_name, port1, 0, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 0U);
    servers_found =
        s_CountServersWithExpectation(node_name, port2, 1, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 1U);

    /* Cleanup */
    s_DeannounceCPP(node_name, "v1", "", port1, kSingleThreadNumber);
    s_DeannounceCPP(node_name, "v2", "", port2, kSingleThreadNumber);
    LBOS::ServiceVersionDelete(node_name);

}

/* 5. Announce one server. Discover it. Then delete version. Try to
 *    discover it again, should not find.
 *   Test is not for multi-threading                                      */
void AnnounceThenDeleteVersion__DiscoverFindsNothing()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    unsigned short port;

    /* Set version */
    LBOS::ServiceVersionSet(node_name, "1.0.0");
    
    /* Announce and discover */
    string health = 
                 string("http://") + s_GetMyIP() + ":" PORT_STR(PORT) "/health";
    int count_before;
    s_SelectPort(count_before, node_name, port, kSingleThreadNumber);
    s_AnnounceCPPSafe(node_name, "1.0.0", "", port, health.c_str(),
                      kSingleThreadNumber);
    unsigned int servers_found =
        s_CountServersWithExpectation(node_name, port, 1, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 1U);

    /* Delete version and not discover */
    LBOS::ServiceVersionDelete(node_name);
    servers_found =
        s_CountServersWithExpectation(node_name, port, 0, kDiscoveryDelaySec, 
                                      kSingleThreadNumber, "");
    NCBITEST_CHECK_EQUAL(servers_found, 0U);

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
    s_DeannounceCPP(node_name, "1.0.0", "", port, kSingleThreadNumber);
}

/* 6. Set with no service - invalid args */
void SetNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionSet("", "1.0.0"), 
                          CLBOSException, 
                          comp);
}

/* 7. Get with no service - invalid args */
void GetNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionGet(""), 
                          CLBOSException, 
                          comp);
}

/* 8. Delete with no service - invalid args */
void DeleteNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionDelete(""), 
                          CLBOSException, 
                          comp);
}

/* 9. Set with empty version - OK */
void SetEmptyVersion__OK()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionSet(node_name, ""),
                          CLBOSException, comp);

    /* Check empty version */
    string cur_version = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL(cur_version, "");

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 10. Set with empty version no service - invalid args */
void SetNoServiceEmptyVersion__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::e_LBOSInvalidArgs, 452> comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionSet("", ""), 
                          CLBOSException, 
                          comp);
}

/* 11. Get, set, delete with service that does not exist, providing
*     "exists" parameter - this parameter should be false and version
*     should be empty */
void ServiceNotExistsAndBoolProvided__EqualsFalse()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    bool exists = true;
    string conf_version = "1";

    /* Get version */
    conf_version = LBOS::ServiceVersionGet(node_name, &exists);
    NCBITEST_CHECK_EQUAL(exists, false);
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";
    exists = true;

    /* Delete */
    conf_version = LBOS::ServiceVersionDelete(node_name, &exists);
    NCBITEST_CHECK_EQUAL(exists, false);
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";
    exists = true;

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);
    NCBITEST_CHECK_EQUAL(exists, false);
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";
    exists = true;

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 12. Get, set, delete with service that does exist, providing
*     "exists" parameter - this parameter should be true and version
*     should be filled */
void ServiceExistsAndBoolProvided__EqualsTrue()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    bool exists = false;
    string conf_version = "1";
    LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);

    /* Get version */
    conf_version = LBOS::ServiceVersionGet(node_name, &exists);
    NCBITEST_CHECK_EQUAL(exists, true);
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";
    exists = false;

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);
    NCBITEST_CHECK_EQUAL(exists, true);
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";
    exists = false;

    /* Delete (and also a cleanup) */
    conf_version = LBOS::ServiceVersionDelete(node_name, &exists);
    NCBITEST_CHECK_EQUAL(exists, true);
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";
    exists = false;
}

/* 13. Get, set, delete with service that does not exist, not providing
*     "exists" parameter -  version should be empty and no crash should
*     happen*/
void ServiceNotExistsAndBoolNotProvided__NoCrash()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    string conf_version = "1";

    /* Get version */
    conf_version = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";

    /* Delete */
    conf_version = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0");
    NCBITEST_CHECK_EQUAL(conf_version, "");
    conf_version = "1";

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 14. Get, set, delete with service that does exist, not providing
*     "exists" parameter - this parameter should be true and version
*     should be filled */
void ServiceExistsAndBoolNotProvided__NoCrash()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GetUnknownService();
    string conf_version = "1";
    LBOS::ServiceVersionSet(node_name, "1.0.0");

    /* Get version */
    conf_version = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0");
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";

    /* Delete (and also a cleanup) */
    conf_version = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL(conf_version, "1.0.0");
    conf_version = "1";
}
}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Stability
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void GetNext_Reset__ShouldNotCrash();
void FullCycle__ShouldNotCrash();


void GetNext_Reset__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    int secondsBeforeStop = 10;  /* when to stop this test */
    struct ::timeval start; /**< we will measure time from start
                                   of test as main measure of when
                                   to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    string service = "/lbos";
    
    
    const SSERV_Info* info = NULL;
    int i = 0;

    
    CConnNetInfo net_info(service);
    
    double elapsed = 0.0;

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
        CORE_LOGF(eLOG_Trace, ("Stability test 1: iteration %d, "
                "%0.2f seconds passed", i, elapsed));
        do {
            info = SERV_GetNextInfoEx(*iter, NULL);
        } while (info != NULL);
        SERV_Reset(*iter);
        if (s_GetTimeOfDay(&stop) != 0)
            memset(&stop, 0, sizeof(stop));
        elapsed = s_TimeDiff(&stop, &start);
    }
    CORE_LOGF(eLOG_Trace, ("Stability test 1:  %d iterations\n", i));
}

void FullCycle__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    int secondsBeforeStop = 10; /* when to stop this test */
    double elapsed = 0.0;
    struct timeval start; /**< we will measure time from start of test as main
                                           measure of when to finish */
    struct timeval stop;
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    string service = "/lbos";
    
    
    const SSERV_Info* info = NULL;
    int i = 0;

    
    CConnNetInfo net_info(service);
    

    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
        CORE_LOGF(eLOG_Trace, ("Stability test 2: iteration %d, "
                               "%0.2f seconds passed", i, elapsed));
        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                          
                          SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                          *net_info, 0/*skip*/, 0/*n_skip*/,
                          0/*external*/, 0/*arg*/, 0/*val*/));
        do {
            info = SERV_GetNextInfoEx(*iter, NULL);
        } while (info != NULL);
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
    CLBOSStatus lbos_status(true, true);
    int                 secondsBeforeStop = 10;  /* when to stop this test */
    double              total_elapsed     = 0.0;
    double              cycle_elapsed     = 0.0; /**< we check performance
                                                      every second            */
    struct timeval      start;                   /**< we will measure time from
                                                      start of test as main
                                                      measure of when to
                                                      finish                  */
    struct timeval      cycle_start;             /**< to measure start of
                                                      current cycle           */
    struct timeval      stop;                    /**< To check time at
                                                      the end of each
                                                      iterations              */
    string              service            =  "/lbos";
    const SSERV_Info*   info;
    int                 total_iters        = 0;  /**< Total number of full
                                                      iterations since start of
                                                      test                    */
    int                 cycle_iters        = 0;  /**< Total number of full
                                                      iterations since start
                                                      of one second cycle     */
    int                 total_hosts        = 0;  /**< Total number of found
                                                      hosts since start of
                                                      test                    */
    int                 cycle_hosts        = 0;  /**< number of full
                                                      iterations since start of
                                                      one second cycle        */
    int                 max_iters_per_cycle = 0; /**< Maximum iterations
                                                      per one second cycle    */
    int                 min_iters_per_cycle = INT_MAX; /**< Minimum iterations
                                                      per one second cycle    */
    int                 max_hosts_per_cycle = 0; /**< Maximum hosts found
                                                         per one second cycle */
    int                 min_hosts_per_cycle = INT_MAX; /**< Minimum hosts found
                                                      per one second cycle    */
    /*
     * Basic initialization
     */
    
    CConnNetInfo net_info(service);
    
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
        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                          
                          SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                          *net_info, 0/*skip*/, 0/*n_skip*/,
                          0/*external*/, 0/*arg*/, 0/*val*/));
        int hosts_found = 0;
        while ((info = SERV_GetNextInfoEx(*iter, NULL)) != NULL) {
            ++hosts_found;
            ++total_hosts;
            ++cycle_hosts;;
        }
        CORE_LOGF(eLOG_Trace,  ("Found %d hosts", hosts_found)  );
        s_CallCounter = 0;
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
    X(01,ComposeLBOSAddress::LBOSExists__ShouldReturnLbos)                    \
    X(04,ResetIterator::NoConditions__IterContainsZeroCandidates)             \
    X(05,ResetIterator::MultipleReset__ShouldNotCrash)                        \
    X(06,ResetIterator::Multiple_AfterGetNextInfo__ShouldNotCrash)            \
    X(07,CloseIterator::AfterOpen__ShouldWork)                                \
    X(08,CloseIterator::AfterReset__ShouldWork)                               \
    X(09,CloseIterator::AfterGetNextInfo__ShouldWork)                         \
    X(10,CloseIterator::FullCycle__ShouldWork)                                \
    X(11,ResolveViaLBOS::ServiceExists__ReturnHostIP)                         \
    X(12,ResolveViaLBOS::ServiceDoesNotExist__ReturnNULL)                     \
    X(13,ResolveViaLBOS::NoLBOS__ReturnNULL)                                  \
    X(17,GetLBOSAddress::CustomHostNotProvided__SkipCustomHost)               \
    X(27,GetNextInfo::IterIsNull__ReturnNull)                                 \
    X(28,GetNextInfo::WrongMapper__ReturnNull)                                \
    X(29,Stability::GetNext_Reset__ShouldNotCrash)                            \
    X(30,Stability::FullCycle__ShouldNotCrash)
    
#define X(num,name) CMainLoopThread* thread##num = new CMainLoopThread(name);
    LIST_OF_FUNCS
#undef X

#define X(num,name) thread##num->Run();
    LIST_OF_FUNCS
#undef X

//SleepMilliSec(10000);
#define X(num,name) thread##num->Join(); 
    LIST_OF_FUNCS
#undef X

#undef LIST_OF_FUNCS
}
} /* namespace MultiThreading */

static
unsigned short s_Msb(unsigned short x)
{
    unsigned int y;
    while ((y = x & (x - 1)) != 0)
        x = y;
    return x;
}

static
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

static
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
static
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

static
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

static
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


static
void s_TestFindMethod(ELBOSFindMethod find_method)
{
    string service = "/lbos";
    
    const SSERV_Info* info = NULL;
    struct timeval start;
    int n_found = 0;
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    CConnNetInfo net_info(service);
    
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of LBOS address now
     */
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                            lbos_addr.c_str();
    /*ConnNetInfo_Destroy(*net_info);*/
    if (*iter) {
        g_LBOS_UnitTesting_SetLBOSFindMethod(*iter, find_method);
        HOST_INFO hinfo;
        while ((info = SERV_GetNextInfoEx(*iter, &hinfo)) != 0) {
            struct timeval stop;
            double elapsed;
            char* info_str;
            ++n_found;
            if (s_GetTimeOfDay(&stop) != 0)
                memset(&stop, 0, sizeof(stop));
            elapsed = s_TimeDiff(&stop, &start);
            info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Server #%-2d (%.6fs) `%s' = %s",
                    ++n_found, elapsed, SERV_CurrentName(*iter),
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
                                                    SERV_MapperName(*iter)));
        SERV_Reset(*iter);
        CORE_LOG(eLOG_Trace, "Service mapper has been reset");
        NCBITEST_CHECK_MESSAGE (n_found && (info = SERV_GetNextInfo(*iter)),
                                         "Service not found after reset");
    }
}

struct SAnnouncedServer
{
    unsigned int host;
    unsigned short port;
    string version;
    string service;
};


/** Result of parsing of /service */
static vector<SAnnouncedServer> s_ParseService()
{
    vector<SAnnouncedServer> nodes;
    CConnNetInfo net_info;
    size_t start = 0, end = 0;
    CCObjArrayHolder<char> lbos_addresses(g_LBOS_GetLBOSAddresses());
    string lbos_addr(lbos_addresses[0]);
    /*
    * Deannounce all lbostest servers (they are left if previous
    * launch of test crashed)
    */
    CCObjHolder<char> lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
        UrlReadAll(*net_info, (string("http://") + lbos_addr +
        "/lbos/text/service").c_str(), NULL, NULL));
    if (*lbos_output_orig == NULL)
        lbos_output_orig = strdup("");
    string lbos_output = *lbos_output_orig;
    LOG_POST(Error << "/text/service output: \r\n" << lbos_output);
    vector<string> to_find;
    to_find.push_back("/lbostest");
    to_find.push_back("/lbostest1");

    unsigned int i = 0;
    for (i = 0; i < to_find.size(); ++i) {
        while (start != string::npos) {
            start = lbos_output.find(to_find[i] + "\t", start);
            if (start == string::npos)
                break;
            // We already know service name since we searched for it.
            start = lbos_output.find("\t", start); //skip service name
            start += 1; //skip \t
            // Next, we extract version.
            end = lbos_output.find("\t", start); //version
            string version = lbos_output.substr(start, end - start);
            // Now we extract ip
            start = end + 1; //skip \t
            end = lbos_output.find("\t", start); //skip ip
            string ip = lbos_output.substr(start, end - start);
            /* Make sure that it is IP, not host name */
            ip = CSocketAPI::HostPortToString(CSocketAPI::gethostbyname(ip), 0);
            // Now we extract port
            start = end + 1; //skip "\t"
            end = lbos_output.find("\t", start);
            unsigned short port =
                NStr::StringToInt(lbos_output.substr(start, end - start));
            // We skip healthcheck
            start = end + 1; //skip \t
            end = lbos_output.find("\t", start);
            // Check if service is announced are just hangs until being deleted
            start = end + 1; //skip \t
            end = lbos_output.find("\t", start);
            string is_announced = lbos_output.substr(start, end - start);
            if (is_announced == "true") {
                SAnnouncedServer node;
                node.host = CSocketAPI::gethostbyname(ip);
                node.port = port;
                node.service = to_find[i];
                node.version = version;
                nodes.push_back(node);
            }
        }
        start = 0; // reset search for the next service
    }
    return nodes;
}


#ifdef DEANNOUNCE_ALL_BEFORE_TEST
/** Remove from ZooKeeper only those services which have name specified in
 * special array (this array is defined inside this function) and are
 * based on current host. We do not remove servers that are based on another 
 * host not to interfere with other test applications. And we do not touch 
 * servers with other names that are not related to our test */
static void s_ClearZooKeeper()
{
    vector<SAnnouncedServer> nodes = s_ParseService();
    vector<SAnnouncedServer>::iterator node;
    for (node = nodes.begin(); node != nodes.end(); node++) {
        string host = CSocketAPI::HostPortToString(node->host, 0);
        if (host == s_GetMyIP()) {
            try {
                s_DeannounceCPP(node->service, node->version, host, node->port,
                                kSingleThreadNumber);
            }
            catch (const CLBOSException&) {
            }
        }
    }
}
#endif /* DEANNOUNCE_ALL_BEFORE_TEST */


/** Find server in %LBOS%/text/service */
static
bool s_CheckIfAnnounced(string          service, 
                        string          version, 
                        unsigned short  server_port, 
                        string          health_suffix,
                        bool            expectedAnnounced,
                        string          expected_host)
{   
    int wait_msecs = 500;
    int max_retries = kDiscoveryDelaySec * 1000 / wait_msecs;
    int retries = 0;
    bool announced = !expectedAnnounced;
    expected_host = expected_host == "" ?
        CSocketAPI::HostPortToString(CSocketAPI::GetLocalHostAddress(), 0) :
        CSocketAPI::HostPortToString(CSocketAPI::gethostbyname(expected_host), 0);
    MEASURE_TIME_START
        while (announced != expectedAnnounced && retries < max_retries) {
            announced = false;
            if (retries > 0) { /* for the first cycle we do not sleep */
                SleepMilliSec(wait_msecs);
            }
            vector<SAnnouncedServer> nodes = s_ParseService();
            vector<SAnnouncedServer>::iterator node;
            for (node = nodes.begin();  node != nodes.end();   node++) {
                string host = CSocketAPI::HostPortToString(node->host, 0);
                if (expected_host == host && node->port == server_port &&
                        node->version == version && node->service == service)
                {
                    announced = true;
                    break;
                }
            }
            retries++;
        }
    MEASURE_TIME_FINISH
    LOG_POST(Error << "Server " << service << " with port " << server_port <<
             " and version " << version << " was" <<
             (announced ? "" : " NOT") << " found int /text/service after " <<
             time_elapsed << " seconds (expected to" <<
             (expectedAnnounced ? "" : " NOT") << " find it).");
    return announced;    
}


/** A simple construction that returns "Thread n: " when n is not -1, 
 *  and returns "" (empty string) when n is -1. */
static string s_PrintThreadNum(int n) {
    stringstream ss;
    CTime cl(CTime::eCurrent, CTime::eLocal);

    ss << cl.AsString("h:m:s.l ");
    if (n != -1) {
        ss << "Thread " << n << ": ";
    }
    return ss.str();
}

static void s_PrintAnnouncedServers() {
#if 0
    int count, i;
    stringstream ss;
    struct SLBOS_AnnounceHandle_Tag** arr =
        g_LBOS_UnitTesting_GetAnnouncedServers();

    if (*arr == NULL)
        return "";
    count = g_LBOS_UnitTesting_GetAnnouncedServersNum();

    for (i = 0; i < count; i++) {
        ss << i << ". " << 
            "\t" << (*arr)[i].service << "\t" << (*arr)[i].version << 
            "\t" << (*arr)[i].host << ":" << (*arr)[i].port << endl;
    }
    WRITE_LOG("Announced servers list: \n" <<  ss.str());
#endif
}

#endif /* CONNECT___TEST_NCBI_LBOS__HPP*/
