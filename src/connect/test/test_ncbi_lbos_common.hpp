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
#include <corelib/request_ctx.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_lbos.hpp>
#include <connect/ncbi_monkey.hpp>
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
    {                                                                         \
        stringstream ss;                                                      \
        int* p_val = s_Tls->GetValue();                                         \
        if (p_val != NULL)                                                    \
        {                                                                     \
            ss << "Thread " << *p_val << ": ";                                \
        }                                                                     \
        ss << M;                                                              \
        if (!(P)) {/* the process is doomed, let's print what was announced */\
            s_PrintPortsLines();                                              \
            s_Print500sCount();                                               \
            s_PrintResolutionErrors();                                        \
        }                                                                     \
        NCBI_ALWAYS_ASSERT(P,ss.str().c_str());                               \
    }
#   undef  NCBITEST_REQUIRE_MESSAGE
#   define NCBITEST_REQUIRE_MESSAGE(P,M)                                      \
        NCBITEST_CHECK_MESSAGE(P,M)           
#   undef  BOOST_CHECK_EXCEPTION
#   define BOOST_CHECK_EXCEPTION(S,E,P)                                       \
     do {                                                                     \
        try {                                                                 \
            S;                                                                \
        }                                                                     \
        catch (const E& ex) {                                                 \
            NCBITEST_CHECK_MESSAGE(P(ex),                                     \
                                  "LBOS exception contains wrong error type") \
            break;                                                            \
        }                                                                     \
        catch (...) {                                                         \
            NCBITEST_CHECK_MESSAGE(false,                                     \
                           "Unexpected exception was thrown")                 \
        }                                                                     \
        NCBITEST_CHECK_MESSAGE(false,                                         \
                       "No exception was thrown")                             \
    } while (false);
    //because assert and exception are the same
#   undef  BOOST_CHECK_NO_THROW
#   define BOOST_CHECK_NO_THROW(S) S            
#   undef  NCBITEST_CHECK_EQUAL
#   define NCBITEST_CHECK_EQUAL(S,E)                                          \
    do {                                                                      \
        stringstream ss, lh_str, rh_str;                                      \
        lh_str << S;                                                          \
        rh_str << E;                                                          \
        ss << " (" << #S << " != " << #E <<  ")"                              \
           << " (" << lh_str.str() << " != " << rh_str.str() <<  ")";         \
        NCBITEST_CHECK_MESSAGE(lh_str.str() == rh_str.str(),                  \
                               ss.str().c_str())                              \
    } while(false);

#   undef  NCBITEST_CHECK_NE
#   define NCBITEST_CHECK_NE(S,E)                                             \
    do {                                                                      \
           stringstream ss;                                                   \
           ss << " (" << S << " == " << E <<  ")";                            \
           NCBITEST_CHECK_MESSAGE(S != E, ss.str().c_str())                   \
    } while(false);
#   undef  NCBITEST_REQUIRE_NE
#   define NCBITEST_REQUIRE_NE(S,E)                                           \
        NCBITEST_CHECK_NE(S,E)            
#else  /* if LBOS_TEST_MT not defined */
//  This header must be included before all Boost.Test headers
#   include <corelib/test_boost.hpp>
#endif /* #ifdef LBOS_TEST_MT */

#if 0

#ifdef LBOS_TEST_MT
#   define NCBITEST_CHECK_MESSAGE(P,M)                                        \
        NCBITEST_CHECK_MESSAGE(P,M)           
#   define NCBITEST_REQUIRE_MESSAGE(P,M)                                      \
        NCBITEST_REQUIRE_MESSAGE(P,M)           
#   define BOOST_CHECK_EXCEPTION(S,E,P)                                       \
        BOOST_CHECK_EXCEPTION(S,E,P)           
#   define BOOST_CHECK_NO_THROW(S)                                            \
        BOOST_CHECK_NO_THROW(S)           
#   define NCBITEST_CHECK_EQUAL(S,E)                                          \
        NCBITEST_CHECK_EQUAL(S,E)           
#   define NCBITEST_CHECK_NE(S,E)                                             \
        NCBITEST_CHECK_NE(S,E)           
#   define NCBITEST_REQUIRE_NE(S,E)                                           \
        NCBITEST_REQUIRE_NE(S,E)           
#else  /* if LBOS_TEST_MT not defined - no thread output */
#   define NCBITEST_CHECK_MESSAGE(P,M)                                        \
        NCBITEST_CHECK_MESSAGE(P,M)
#   define NCBITEST_REQUIRE_MESSAGE(P,M)                                      \
        NCBITEST_REQUIRE_MESSAGE(P,M)
#   define BOOST_CHECK_EXCEPTION(S,E,P)                                       \
        BOOST_CHECK_EXCEPTION(S,E,P)
#   define BOOST_CHECK_NO_THROW(S)                                            \
        BOOST_CHECK_NO_THROW(S)
#   define NCBITEST_CHECK_EQUAL(S,E)                                          \
        NCBITEST_CHECK_EQUAL(S,E)
#   define NCBITEST_CHECK_NE(S,E)                                             \
        NCBITEST_CHECK_NE(S,E)
#   define NCBITEST_REQUIRE_NE(S,E)                                           \
        NCBITEST_REQUIRE_NE(S,E)
#endif /* #ifdef LBOS_TEST_MT */

#endif

/* Boost checks are not thread-safe, so they need to be handled appropriately*/
#define MT_SAFE(E)                                                            \
    {{                                                                        \
        CFastMutexGuard spawn_guard(s_BoostTestLock);                         \
        E;                                                                    \
    }}
#define NCBITEST_CHECK_MESSAGE_MT_SAFE(P,M)                                   \
            MT_SAFE(NCBITEST_CHECK_MESSAGE(P, M))
#define NCBITEST_REQUIRE_MESSAGE_MT_SAFE(P,M)                                 \
            MT_SAFE(NCBITEST_REQUIRE_MESSAGE(P, M))
#define NCBITEST_CHECK_EQUAL_MT_SAFE(S,E)                                     \
            MT_SAFE(NCBITEST_CHECK_EQUAL(S, E))
#define NCBITEST_CHECK_NE_MT_SAFE(S,E)                                        \
            MT_SAFE(NCBITEST_CHECK_NE(S, E))
#define NCBITEST_REQUIRE_NE_MT_SAFE(S,E)                                      \
            MT_SAFE(NCBITEST_REQUIRE_NE(S, E))

#ifdef LBOS_TEST_MT
#define TEST_PASS return
#define EXTRACT_TEST_NAME                                                     \
    size_t first_colon = func_name.find(':') + 2;                             \
    size_t last_colon = func_name.find_last_of(':') - 1;                      \
    func_name = func_name.substr(first_colon, last_colon - first_colon);      
#else
#define TEST_PASS return
#define EXTRACT_TEST_NAME  
#endif 
 
#define CHECK_LBOS_VERSION()                                                  \
do {                                                                          \
    string func_name = __FUNCTION__;                                          \
    EXTRACT_TEST_NAME                                                         \
    CCObjHolder<char> versions_cstr(g_LBOS_RegGet("TESTVERSIONS",             \
                                                  func_name.c_str(),          \
                                                  NULL));                     \
    if (*versions_cstr != NULL) {                                             \
        string versions_str = *versions_cstr;                                 \
        vector<SLBOSVersion> versions_arr =                                   \
            s_ParseVersionsString(versions_str);                              \
        bool active = s_CheckTestVersion(versions_arr);                       \
        if (!active) {                                                        \
            WRITE_LOG("Test " << func_name << " is not active because "       \
                      "LBOS has version \"" <<                                \
                      s_LBOSVersion.major << "." <<                           \
                      s_LBOSVersion.minor << "." << s_LBOSVersion.patch <<    \
                      "\" and test has version string \""                     \
                      << versions_str << "\"");                               \
            TEST_PASS;                                                        \
        }                                                                     \
    } else {                                                                  \
        WRITE_LOG("Test " << func_name << " compatible LBOS versions not "    \
                    "found in config. Allowing to run.");                     \
    }                                                                         \
} while (false)


/* LBOS returns 500 on announcement unexpectedly */
//#define QUICK_AND_DIRTY // define if announcements are repeated until success

/* We might want to clear ZooKeeper from nodes before running tests.
* This is generally not good, because if this test application runs
* on another host at the same moment, it will miss a lot of nodes and
* tests will fail.
*/
#define DEANNOUNCE_ALL_BEFORE_TEST
/*test*/
#include "test_assert.h"


USING_NCBI_SCOPE;


/* Version of LBOS or this test */
struct SLBOSVersion
{
    unsigned short major;
    unsigned short minor;
    unsigned short patch;

    bool operator<(const SLBOSVersion& rhs) 
    {
        if (major != rhs.major)
            return major < rhs.major;
        if (minor != rhs.minor)
            return minor < rhs.minor;
        if (patch != rhs.patch)
            return patch < rhs.patch;
        return false;
    }
    bool operator==(const SLBOSVersion& rhs) 
    {
        return major == rhs.major && minor == rhs.minor && patch == rhs.patch;
    }
    bool operator>(const SLBOSVersion& rhs)
    {
        return !( *this < rhs  ||  *this == rhs );
    }
    bool operator>=(const SLBOSVersion& rhs)
    {
        return (*this > rhs || *this == rhs);
    }
    bool operator<=(const SLBOSVersion& rhs)
    {
        return (*this < rhs || *this == rhs);
    }
};

struct SLBOSResolutionError
{
    int code_line;
    int expected_count;
    int count;
    unsigned short port;
};


struct SServer
{
    unsigned int host;
    unsigned short port;
    string version;
    string service;
};


/* First let's declare some functions that will be
 * used in different test suites. It is convenient
 * that their definitions are at the very end, so that
 * test config is as high as possible */
static CRef<CTls<int>> s_Tls(new CTls<int>);
static void            s_PrintInfo                  (HOST_INFO);
static void            s_TestFindMethod             (ELBOSFindMethod);
static string          s_PrintThreadNum             ();
static void            s_PrintAnnouncedServers      ();

#ifdef NCBI_OS_MSWIN
static int             s_GetTimeOfDay           (struct timeval*);
#else
#   define             s_GetTimeOfDay(tv)        gettimeofday(tv, 0)
#endif
static unsigned short  s_Msb                    (unsigned short);
static const char*     s_OS                     (TNcbiOSType);
static const char*     s_Bits                   (TNcbiCapacity);
/** Count difference between two timestamps, in seconds*/
static double          s_TimeDiff               (const struct   timeval*,
                                                 const struct   timeval*);
static string          s_GenerateNodeName       (void);
static unsigned short  s_GeneratePort           ();
static bool            s_CheckIfAnnounced       (const string&  service,
                                                 const string&  version,
                                                 unsigned short server_port,
                                                 const string&  health_suffix,
                                                 bool         expectedAnnounced
                                                                        = true,
                                                 string         host = "");
static string          s_ReadLBOSVersion        (void);
static bool            s_CheckTestVersion       (vector<SLBOSVersion>
                                                                versions_arr);
static 
vector<SLBOSVersion>   s_ParseVersionsString    (const string& versions);
static void            s_PrintPortsLines        (void);
static void            s_PrintResolutionErrors  (void);
static void            s_Print500sCount         (void);
static vector<SServer> s_GetAnnouncedServers    (bool is_enabled = false, 
                                                 vector<string> to_find 
                                                            = vector<string>());

const int              kThreadsNum                  = 34;
/** When the test is run in single-threaded mode, we set the number of the 
 * main thread to -1 to distinguish between MT and ST                        */
const int              kSingleThreadNumber          = -1;
const int              kMainThreadNumber            = 99;
const int              kHealthThreadNumber          = 100;
/* Seconds to try to find server. We wait maximum of 60 seconds.             */
const int              kDiscoveryDelaySec           = 15; 
/* for tests where port is not necessary (they fail before announcement)     */
const unsigned short   kDefaultPort                 = 5000; 
/** Static variables that are used in mock functions.
 * This is not thread-safe!                                                  */
static int             s_CallCounter                = 0;
/** It is yet impossible on TeamCity, but useful for local tests, where 
 * local LBOS can be easily run                                              */
static string          s_LastHeader;
static SLBOSVersion    s_LBOSVersion                = {0,0,0};
/** To remember on which line of code which port was announced 
 *  (for debugging purposes)                                                 */
static map<int, int>   s_PortsLines;
/* To remember on which line of code we did not find as many servers as 
 * expected */
static vector<SLBOSResolutionError>  s_ResolutionErrors;
/** Remember how many times we got 500 */
static unsigned int    s_500sCount                  = 0;


/* Mutex for thread-unsafe Boost checks */
DEFINE_STATIC_FAST_MUTEX(s_BoostTestLock);
/* Mutex for log output */
DEFINE_STATIC_FAST_MUTEX(s_WriteLogLock);

#define PORT_N 5 /* port for healthcheck */
#define PORT_STR_HELPER(port) #port
#define PORT_STR(port) PORT_STR_HELPER(port)

#ifdef NCBI_OS_MSWIN
#   define LBOSRESOLVER_PATH "C:\\Apps\\Admin_Installs\\etc\\ncbi\\lbosresolver"
#else
#   define LBOSRESOLVER_PATH "/etc/ncbi/lbosresolver"
#endif

/*#define ANNOUNCEMENT_HOST string("iebdev22") */
#define ANNOUNCEMENT_HOST s_GetMyIP() /* The host which is used for healthchecks 
                                         in all test cases. By default, 
                                         ANNOUNCEMENT_HOST is set to s_GetMyIP()
                                         For additional 
                                         tests it can be changed to any host */

/*#define ANNOUNCEMENT_HOST_0000 "iebdev22"*/
#define ANNOUNCEMENT_HOST_0000 "0.0.0.0" 
                                         /* The host which is used as 0.0.0.0.
                                            For additional tests it can be 
                                            changed to some other host 
                                            address. */
const int kPortsNeeded = 9;
static CSafeStatic<vector<unsigned short>> s_ListeningPorts;

#include "test_ncbi_lbos_mocks.hpp"

#define WRITE_LOG(text)                                                       \
{{                                                                            \
    CFastMutexGuard spawn_guard(s_WriteLogLock);                              \
    LOG_POST(s_PrintThreadNum() << "\t" << __FILE__ <<                        \
                                   "\t" << __LINE__ <<                        \
                                   "\t" <<   text);                           \
}}

/* Trash collector for Thread Local Storage that stores thread number */
void TlsCleanup(int* p_value, void* /* data */)
{
    delete p_value;
}

static void s_PrintAnnouncementDetails(const char* name,
                                       const char* version,
                                       const char* host,
                                       const char* port,
                                       const char* health)
{
    WRITE_LOG("Announcing server \"" << (name ? name : "<NULL>") << "\""    <<
                "with version "      << (version ? version : "<NULL>")      <<
                ", port "            << port                                << 
                ", host "            << host                                << 
                ", healthcheck \""   << (health ? health : "<NULL>") << "\"");
}


static void s_PrintAnnouncementDetails(const char*      name,
                                       const char*      version,
                                       const char*      host,
                                       unsigned short   port,
                                       const char*      health)
{
    stringstream port_ss;
    port_ss << port;
    s_PrintAnnouncementDetails(name, version, host, port_ss.str().c_str(), 
                               health);
}


static void s_PrintAnnouncedDetails(const char*       name,
                                    const char*       version,
                                    const char*       host,
                                    const char*       port,
                                    const char*       health,
                                    const char*       lbos_ans, 
                                    const char*       lbos_mes, 
                                    unsigned short    result,
                                    double            time_elapsed)
{
    const char* message = lbos_mes ? lbos_mes : "<NULL>";
    const char* status  = lbos_ans ? lbos_ans : "<NULL>";
    WRITE_LOG("Announcing server \""  << (name ? name : "<NULL>") << "\" "  <<
               "with version "        << (version ? version : "<NULL>")     <<
               ", port "              << port                               <<
               ", host \""            << (host ? host : "<NULL>")           <<
               ", healthcheck \""     << (health ? health : "<NULL>")       << 
               " returned code "      << result                             <<
               " after "              << time_elapsed << " seconds, "       <<
               ", status message: \"" << status  << "\""                    <<
               ", body: \""           << message << "\"");
}


static void s_PrintAnnouncedDetails(const char*       name,
                                    const char*       version,
                                    const char*       host,
                                    unsigned short    port,
                                    const char*       health,
                                    const char*       lbos_ans, 
                                    const char*       lbos_mes, 
                                    unsigned short    result,
                                    double            time_elapsed)
{
    stringstream port_ss;
    port_ss << port;
    s_PrintAnnouncedDetails(name, version, host, port_ss.str().c_str(), health, 
                            lbos_ans, lbos_mes, result, time_elapsed);
}

static void s_GetRegistryAnnouncementParams(const char*     registry_section,
                                                  char**    srvc, 
                                                  char**    vers,
                                                  char**    host, 
                                                  char**    port,
                                                  char**    hlth)
{
    const char* kLBOSAnnouncementSection    = "LBOS_ANNOUNCEMENT";
    const char* kLBOSServiceVariable        = "SERVICE";
    const char* kLBOSVersionVariable        = "VERSION";
    const char* kLBOSHostVariable           = "HOST";
    const char* kLBOSPortVariable           = "PORT";
    const char* kLBOSHealthcheckUrlVariable = "HEALTHCHECK";
    if (g_LBOS_StringIsNullOrEmpty(registry_section)) {
        registry_section = kLBOSAnnouncementSection;
    }
    *srvc = g_LBOS_RegGet(registry_section,
                          kLBOSServiceVariable,
                          NULL);
    *vers = g_LBOS_RegGet(registry_section,
                          kLBOSVersionVariable,
                          NULL);
    *host = g_LBOS_RegGet(registry_section,
                          kLBOSHostVariable,
                          NULL);
    *port = g_LBOS_RegGet(registry_section,
                          kLBOSPortVariable,
                          NULL);
    *hlth = g_LBOS_RegGet(registry_section,
                          kLBOSHealthcheckUrlVariable,
                          NULL);
}


static void s_PrintRegistryAnnouncementDetails(const char* registry_section)
{
    char* srvc_str, *vers_str, *host_str, *port_str, *hlth_str;
    s_GetRegistryAnnouncementParams(registry_section, &srvc_str, &vers_str,  
                                    &host_str, &port_str, &hlth_str);
    CCObjHolder<char> srvc(srvc_str);
    CCObjHolder<char> vers(vers_str);
    CCObjHolder<char> host(host_str);
    CCObjHolder<char> port(port_str);
    CCObjHolder<char> hlth(hlth_str);
    s_PrintAnnouncementDetails(srvc.Get(), vers.Get(), host.Get(), port.Get(),
                               hlth.Get());
}


static void s_PrintRegistryAnnouncedDetails(const char*      registry_section,
                                           const char*       lbos_ans, 
                                           const char*       lbos_mes, 
                                           unsigned short    result,
                                           double            time_elapsed)
{
    char *srvc_str, *vers_str, *host_str, *port_str, *hlth_str;
    s_GetRegistryAnnouncementParams(registry_section, &srvc_str, &vers_str,
                                    &host_str, &port_str, &hlth_str);
    CCObjHolder<char> srvc(srvc_str),
                      vers(vers_str),
                      host(host_str),
                      port(port_str),
                      hlth(hlth_str);
    s_PrintAnnouncedDetails(srvc.Get(), vers.Get(), host.Get(), port.Get(), 
                            hlth.Get(), lbos_ans, lbos_mes, result, 
                            time_elapsed);
}


#define MEASURE_TIME_START                                                    \
    struct timeval      time_start;         /**< to measure start of          \
                                                 announcement */              \
    struct timeval      time_stop;          /**< To check time at             \
                                                 the end of each              \
                                                 iteration*/                  \
    double              time_elapsed = 0.0; /**< difference between start     \
                                                 and end time */              \
    if (s_GetTimeOfDay(&time_start) != 0) { /**  Initialize time of           \
                                                 iteration start*/            \
        memset(&time_start, 0, sizeof(time_start));                           \
    }


#define MEASURE_TIME_FINISH                                                   \
    if (s_GetTimeOfDay(&time_stop) != 0)                                      \
        memset(&time_stop, 0, sizeof(time_stop));                             \
    time_elapsed = s_TimeDiff(&time_stop, &time_start);    


/** Announce using C interface. */
static unsigned short s_AnnounceC(const char*       name,
                                  const char*       version,
                                  const char*       host,
                                  unsigned short    port,
                                  const char*       health,
                                  char**            lbos_ans,
                                  char**            lbos_mes,
                                  bool              safe = false)
{
    unsigned short result;
    MEASURE_TIME_START;
    const char * healthcheck_cstr = NULL; //NULL if health is NULL
    string healthcheck_str; //for c_str() to retain value until end of function
    if (health != NULL) {
        stringstream healthcheck;
        healthcheck << health << "/port" << port << 
                       "/host" << (host ? host : "") <<
                       "/version" << (version ? version : "");
        ;
        healthcheck_cstr = (healthcheck_str = healthcheck.str()).c_str();
    }
    
    s_PrintAnnouncementDetails(name, version, host, port,
                               healthcheck_cstr);

    if (safe) {
        /* If announcement finishes with error - return
         * error */
        result = LBOS_Announce(name, version, host, port, 
                               healthcheck_cstr, lbos_ans, lbos_mes);
    } else {
        /* If announcement finishes with error - try again
         * until success (with invalid parameters means infinite loop) */
        result = s_LBOS_Announce(name, version, host, port, 
                                 healthcheck_cstr, lbos_ans, lbos_mes);
    }
    MEASURE_TIME_FINISH;
    if (result == 500) {
        s_500sCount++;
    }
    s_PrintAnnouncedDetails(name, version, host, port, 
                            healthcheck_cstr, 
                            lbos_ans ? *lbos_ans : NULL,
                            lbos_mes ? *lbos_mes : NULL,
                            result, time_elapsed);
    return result;
}
        

static unsigned short s_AnnounceC(const string&    name,
                                  const string&    version,
                                  const string&    host,
                                  unsigned short   port,
                                  const string&    health,
                                  char**           lbos_ans,
                                  char**           lbos_mes,
                                  bool             safe = false)
{
    return s_AnnounceC(name.c_str(), version.c_str(), host.c_str(),
                       port, health.c_str(), lbos_ans, lbos_mes, safe);
}


static unsigned short s_AnnounceCSafe(const char*      name,
                                      const char*      version,
                                      const char*      host,
                                      unsigned short   port,
                                      const char*      health,
                                      char**           lbos_ans,
                                      char**           lbos_mes)
{
    unsigned short result;
    MEASURE_TIME_START;
    const char * healthcheck_cstr = NULL; //NULL if health is NULL
    string healthcheck_str; //for c_str() to retain value until end of function
    if (health != NULL) {
        stringstream healthcheck;
        healthcheck << health << "/port" << port << 
                       "/host" << (host ? host : "") <<
                       "/version" << (version ? version : "");
        ;
        healthcheck_cstr = (healthcheck_str = healthcheck.str()).c_str();
    }
    
    s_PrintAnnouncementDetails(name, version, host, port, 
                               healthcheck_cstr);
    result = s_LBOS_Announce(name, version, host, port, 
                             healthcheck_cstr, lbos_ans, lbos_mes);
    MEASURE_TIME_FINISH;
    s_PrintAnnouncedDetails(name, version, host, port, 
                            healthcheck_cstr, 
                            lbos_ans ? *lbos_ans : NULL,
                            lbos_mes ? *lbos_mes : NULL,
                            result, time_elapsed);
    return result;
}


static unsigned short s_AnnounceCSafe(const string&    name,
                                      const string&    version,
                                      const string&    host,
                                      unsigned short   port,
                                      const string&    health,
                                      char**           lbos_ans,
                                      char**           lbos_mes)
{
    return s_AnnounceCSafe(name.c_str(), version.c_str(), host.c_str(), 
                           port, health.c_str(), lbos_ans, lbos_mes);
}


/** Announce using C interface and using values from registry. If announcement 
 * finishes with error - try again until success (with invalid parameters means
 * infinite loop) */
static unsigned short s_AnnounceCRegistry(const char*   registry_section,
                                          char**        lbos_ans,
                                          char**        lbos_mes)
{
    unsigned short result;
    s_PrintRegistryAnnouncementDetails(registry_section);
    MEASURE_TIME_START
        result = s_LBOS_AnnounceReg(registry_section, lbos_ans, lbos_mes);
    MEASURE_TIME_FINISH
    s_PrintRegistryAnnouncedDetails(registry_section,
                                    lbos_ans ? *lbos_ans : NULL,
                                    lbos_mes ? *lbos_mes : NULL,
                                    result, time_elapsed);
    return result;
}


/** Announce using C++ interface. If annoucement finishes with error - return
 * error */
static void s_AnnounceCPP(const string& name,
                          const string& version,
                          const string& host,
                          unsigned short port,
                          const string& health)
{
    s_PrintAnnouncementDetails(name.c_str(), version.c_str(), host.c_str(), 
                               port, health.c_str());
    stringstream healthcheck;
    healthcheck << health << "/port" << port << "/host" << host <<
                   "/version" << version;
    MEASURE_TIME_START
    try {
        LBOS::Announce(name, version, host, port, healthcheck.str());
    }
    catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
            if (ex.GetErrCode() == 500) {
                s_500sCount++;
            }
            s_PrintAnnouncedDetails(name.c_str(), version.c_str(), 
                                    host.c_str(), port, health.c_str(),   
                                    ex.what(), ex.GetErrCodeString(),  
                                    ex.GetErrCode(), time_elapsed);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintAnnouncedDetails(name.c_str(), version.c_str(), host.c_str(), port,
                            health.c_str(), 
                            "<C++ does not show answer from LBOS on success>",
                            "OK", 200, time_elapsed);
}


/** Announce using C++ interface. If announcement finishes with error - try 
 * again until success (with invalid parameters means infinite loop) */
static void s_AnnounceCPPSafe(const string&     name,
                              const string&     version,
                              const string&     host,
                              unsigned short    port,
                              const string&     health)
{
    s_PrintAnnouncementDetails(name.c_str(), version.c_str(), host.c_str(), 
                               port, health.c_str());
    stringstream healthcheck;
    healthcheck << health << "/port" << port << "/host" << host <<
                   "/version" << version;
    MEASURE_TIME_START
    s_LBOS_CPP_Announce(name, version, host, port, healthcheck.str());
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintAnnouncedDetails(name.c_str(), version.c_str(), host.c_str(), port, 
                            health.c_str(), 
                            "<C++ does not show answer from LBOS on success>",
                            "OK", 200, time_elapsed);
}


/** Announce using C++ interface and using values from registry. If 
 * announcement finishes with error - try again until success (with invalid 
 * parameters meaning infinite loop) */
static void s_AnnounceCPPFromRegistry(const string& registry_section)
{
    s_PrintRegistryAnnouncementDetails(registry_section.c_str());
    MEASURE_TIME_START
    try {
        s_LBOS_CPP_AnnounceReg(registry_section);
    } catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
        if (ex.GetErrCode() == 500) {
            s_500sCount++;
        }
        s_PrintRegistryAnnouncedDetails(registry_section.c_str(), ex.what(), 
                                        ex.GetErrCodeString(),
                                        ex.GetErrCode(), time_elapsed);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintRegistryAnnouncedDetails(registry_section.c_str(), 
                                    "<C++ does not show answer from LBOS>",
                                    "OK", 200, time_elapsed);
}


/** Before deannounce */
static void s_PrintDeannouncementDetails(const char* name,
                                         const char* version,
                                         const char* host, 
                                         unsigned short port)
{
    WRITE_LOG("De-announcing server \"" <<
                    (name ? name : "<NULL>") <<
                    "\" with version " << (version ? version : "<NULL>") <<
                    ", port " << port << ", host \"" <<
                    (host ? host : "<NULL>") << "\", ip " << s_GetMyIP());
}

/** Before deannounce */
static void s_PrintDeannouncedDetails(const char*       name,
                                      const char*       version,
                                      const char*       host, 
                                      unsigned short    port,
                                      const char*       lbos_ans, 
                                      const char*       lbos_mess, 
                                      unsigned short    result,
                                      double            time_elapsed)
{
    WRITE_LOG("De-announce of server \"" <<
              (name ? name : "<NULL>") <<
              "\" with version " << (version ? version : "<NULL>") <<
              ", port " << port << ", host \"" <<
              (host ? host : "<NULL>") << " returned code " << result <<
              " after " << time_elapsed << " seconds, "
              ", LBOS status message: \"" <<
              (lbos_mess ? lbos_mess : "<NULL>") <<
              "\", LBOS answer: \"" <<
              (lbos_ans ? lbos_ans : "<NULL>") << "\"");
}

static unsigned short s_DeannounceC(const char*     name,
                                    const char*     version,
                                    const char*     host,
                                    unsigned short  port,
                                    char**          lbos_ans,
                                    char**          lbos_mes)
{
    unsigned short result;
    s_PrintDeannouncementDetails(name, version, host, port);
    MEASURE_TIME_START
    result = LBOS_Deannounce(name, version, host, port, lbos_ans, lbos_mes);  
    MEASURE_TIME_FINISH
    if (result == 500) {
        s_500sCount++;
    }
    s_PrintDeannouncedDetails(name, version, host, port,
                                lbos_ans ? *lbos_ans : NULL,
                                lbos_mes ? *lbos_mes : NULL,
                                result, time_elapsed);
    return result;
}

static void s_DeannounceAll()
{
    WRITE_LOG("De-announce all");
    MEASURE_TIME_START;
        LBOS_DeannounceAll();
    MEASURE_TIME_FINISH;
    WRITE_LOG("De-announce all has finished and took " <<
               time_elapsed << " seconds.");
}

static void s_DeannounceCPP(const string& name,
                            const string& version,
                            const string& host,
                            unsigned short port)
{
    s_PrintDeannouncementDetails(name.c_str(), version.c_str(), host.c_str(), 
                                 port);
    MEASURE_TIME_START
    try {
        LBOS::Deannounce(name, version, host, port);
    }
    catch (CLBOSException& ex) {
        MEASURE_TIME_FINISH
        if (ex.GetStatusCode() == 500) {
            s_500sCount++;
        }
        s_PrintDeannouncedDetails(name.c_str(), version.c_str(), 
                                    host.c_str(), port, 
                                    ex.what(), ex.GetErrCodeString(),
                                    ex.GetStatusCode(), time_elapsed);
        throw; /* Move the exception down the stack */
    }
    MEASURE_TIME_FINISH
    /* Print good result */
    s_PrintDeannouncedDetails(name.c_str(), version.c_str(), host.c_str(), 
                              port, "<C++ does not show answer from LBOS>",
                              "OK", 200, time_elapsed);
}

// Macro, because we want to know from which line this was called
// (original line will be shown in WRITE_LOG)
#define SELECT_PORT(count_before,node_name,port)                              \
do {                                                                          \
    port = s_GeneratePort();                                                  \
    count_before = s_CountServers(node_name, port);                           \
    while (count_before != 0) {                                               \
        port = s_GeneratePort();                                              \
        count_before = s_CountServers(node_name, port);                       \
    }                                                                         \
    WRITE_LOG("Random port is " << port << ". "                               \
              "Count of servers with this port is " <<                        \
              count_before << ".");                                           \
    s_PortsLines[port] = __LINE__;                                            \
} while (false)


/** Print all ports with which servers were announced, and line of code which
 * initiated each announcement  */
static void s_PrintPortsLines()
{
    /* We will not surround it with mutex because it is not so important */
    static bool already_launched = false;
    if (already_launched) return;
    already_launched = true;

    stringstream ports_lines;
    ports_lines << "Printing port<->line:" << endl;
    auto ports_iter = s_PortsLines.begin();
    for (  ;  ports_iter != s_PortsLines.end()  ;  ports_iter++  ) {
        ports_lines << "Port " << ports_iter->first 
                    << " was announced on line " << ports_iter->second << endl;
    }
    WRITE_LOG(ports_lines.str());
}


/** Print all cases when we did not find as many servers as expected. Write 
 * line of code which started counting, port of server searched, expected count 
 * and real count */
static void s_PrintResolutionErrors()
{
    /* We will not surround it with mutex because it is not so important */
    static bool already_launched = false;
    if (already_launched) return;
    already_launched = true;

    stringstream errors;
    errors << "Printing s_CountServersWithExpectation errors:" << endl;
    auto errors_iter = s_ResolutionErrors.begin();
    for (; errors_iter != s_ResolutionErrors.end(); errors_iter++) {
        errors << "Port " << errors_iter->port << " "
               << "was announced on line " << errors_iter->code_line << ", "
               << "expected to find " << errors_iter->expected_count << ", "
               << "but " << errors_iter->count << " was found instead"
               << endl;
    }
    WRITE_LOG(errors.str());
}


static void s_Print500sCount()
{
    WRITE_LOG("Got " << s_500sCount << " 500's during test");
}


static int  s_FindAnnouncedServer(const string&     service,
                                  const string&     version,
                                  unsigned short    port,
                                  const string&     host)
{
    WRITE_LOG("Trying to find announced server in the storage\"" <<
                service << "\" with version " << version << ", port " <<
                port << ", ip " << host);


    CLBOSStatus lbos_status(true, true);
    struct SLBOS_AnnounceHandle_Tag*& arr =
        *g_LBOS_UnitTesting_GetAnnouncedServers();
    CORE_LOCK_READ;
    unsigned int count = g_LBOS_UnitTesting_GetAnnouncedServersNum();
    unsigned int found = 0;
    /* Just iterate and compare */
    unsigned int i = 0;
    for (i = 0; i < count; i++) {
        if (strcasecmp(service.c_str(), arr[i].service) == 0
            &&
            strcasecmp(version.c_str(), arr[i].version) == 0
            &&
            strcasecmp(host.c_str(), arr[i].host) == 0
            &&
            port == arr[i].port)
        {
            WRITE_LOG("Found a server of \"" <<
                      arr[i].service << "\" with version " << arr[i].version <<
                      ", port " << arr[i].port << ", ip " << arr[i].host
                      << ", #" << i);
            found++;
            break;
        }
    }
    CORE_UNLOCK;
    WRITE_LOG("Found " << found << " servers in the inner LBOS storage");
    if (found > 1) {
        s_PrintAnnouncedServers();
    }
    return found;
}

/** To run tests of /configuration we need name of service that is not used yet.
*/
static bool s_CheckServiceKnown(const string& service)
{
    bool exists;
    LBOS::ServiceVersionGet(service, &exists);
    return exists;
}

/** Special service name for /configure endpoint tests. Telle that it is 
 * safe to delete it and is unique for each host that create them */
static string s_GetUnknownService() {
    static string charset = "abcdefghijklmnopqrstuvwxyz"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "1234567890";
    const int length = 10;
    unsigned int i;
    for (;;) {
        string result = 
            string("/deleteme") + NStr::Replace(s_GetMyIP(),".", "");
        i = result.length();
        result.resize(result.length() + length);
        for ( ; i < result.length(); i++) {
            result[i] = charset[rand() % charset.length()];
        }
        if (!s_CheckServiceKnown(result)) {
            return result;
        }
    }
    return ""; /* never reachable */
}

static void s_CleanDTabs() {
    vector<string>      nodes_to_delete;
    CConnNetInfo        net_info;
    size_t              start           = 0, 
                        end             = 0;
    CCObjHolder<char>   lbos_address    (g_LBOS_GetLBOSAddress());
    string              lbos_addr       (lbos_address.Get());
    CCObjHolder<char>   lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
                                         UrlReadAll(*net_info, 
                                         (string("http://") + lbos_addr +
                                         "/admin/dtab").c_str(), NULL, NULL));
    if (*lbos_output_orig == NULL)
        lbos_output_orig = strdup("");
    string lbos_output = *lbos_output_orig;
    WRITE_LOG("admin/dtab output: \n" << lbos_output);
    string to_find = string("/deleteme") + NStr::Replace(s_GetMyIP(),".", "");    
    while (start != string::npos) {
        start = lbos_output.find(to_find, start);
        if (start == string::npos)
            break;
        // We already know service name since we searched for it.
        end = lbos_output.find("=>", start); //skip service name
        nodes_to_delete.push_back(lbos_output.substr(start, end-start));
        start = end+7; //skip "=>/zk#/" 
    }
    
    vector<string>::iterator it;
    for (it = nodes_to_delete.begin();  it != nodes_to_delete.end();  it++ ) {
        LBOS::ServiceVersionDelete(*it);
    }
}

static string s_PortStr(unsigned int i = 0) 
{
    if (i >= s_ListeningPorts->size()) {
        WRITE_LOG("Error index in s_PortStr(): " << i);
        i = 0;
    }
    return NStr::IntToString((*s_ListeningPorts)[i]);
}


class CHealthcheckThread
#ifdef NCBI_THREADS
        : public CThread
#endif /* NCBI_THREADS */
{
public:
    CHealthcheckThread()
        : m_RunHealthCheck(true)
    {
        m_Busy = false;
        for (unsigned short port = 8080; port < 8110; port++) {
            if ( m_ListeningSockets.size() >= kPortsNeeded ) break;
            CListeningSocket* l_sock = new CListeningSocket(port);
            if (l_sock->GetStatus() == eIO_Success) {
                s_ListeningPorts->push_back(port);
                m_ListeningSockets.push_back(CSocketAPI::SPoll(l_sock, 
                                                               eIO_ReadWrite));
            } else {
                l_sock->Close();
                delete l_sock;
            }
        }
        if (s_ListeningPorts->size() < kPortsNeeded) {
            throw CLBOSException(CDiagCompileInfo(), NULL, 
                                 CLBOSException::EErrCode::e_LBOSUnknown,
                                 "Not enough vacant ports to start listening", 
                                 0);
        }
    }
    void Stop() 
    {
        m_RunHealthCheck = false;
    }
    /* Check if thread has answered all requests */
    bool IsBusy() 
    {
        return m_Busy;
    }

    void AnswerHealthcheck() 
    {
        WRITE_LOG("AnswerHealthcheck() started, m_ListeningSockets has " 
                  << s_ListeningPorts->size() << " open listening sockets"
                  << " and " << m_SocketPool.size() << " open connections");
        /* Keeping number of open sockets under control at all times! */
        int collect_grbg_retries = 0;
        while ((m_SocketPool.size() > 150) && (++collect_grbg_retries < 10))
            CollectGarbage();
        struct timeval  accept_time_stop;
        STimeout        rw_timeout           = { 1, 20000 };
        STimeout        accept_timeout       = { 0, 20000 };
        STimeout        c_timeout            = { 0, 0 };
        int             iters_passed         = 0;
        size_t          n_ready              = 0;
        int             secs_btw_grbg_cllct  = 5;/* collect garbage every 5s */
        int             iters_btw_grbg_cllct = secs_btw_grbg_cllct * 100000 /
                                               (rw_timeout.sec * 100000 + 
                                                rw_timeout.usec);
        
        if (s_GetTimeOfDay(&accept_time_stop) != 0) {
            memset(&accept_time_stop, 0, sizeof(accept_time_stop));
        }
        auto it = m_ListeningSockets.begin();
        CSocketAPI::Poll(m_ListeningSockets, &rw_timeout, &n_ready);
        for (; it != m_ListeningSockets.end(); it++) {
            if (it->m_REvent != eIO_Open && it->m_REvent != eIO_Close) {
                CSocket*   sock  =  new CSocket;
                struct timeval      accept_time_start;
                struct timeval      accept_time_stop;
                if (s_GetTimeOfDay(&accept_time_start) != 0) {
                    memset(&accept_time_start, 0, sizeof(accept_time_start));
                }
                double accept_time_elapsed      = 0.0;
                double last_accept_time_elapsed = 0.0;
                if (static_cast<CListeningSocket*>(it->m_Pollable)->
                            Accept(*sock, &accept_timeout) != eIO_Success) 
                {
                    if (s_GetTimeOfDay(&accept_time_stop) != 0)
                        memset(&accept_time_stop, 0, sizeof(accept_time_stop));
                    accept_time_elapsed      = s_TimeDiff(&accept_time_stop, 
                                                          &accept_time_start);
                    last_accept_time_elapsed = s_TimeDiff(&accept_time_stop, 
                                                        &m_LastSuccAcceptTime);
                    WRITE_LOG("healthcheck vacant after trying accept for " 
                               << accept_time_elapsed << "s, last successful "
                               "accept was " << last_accept_time_elapsed 
                                << "s ago");
                    m_Busy = false;
                    delete sock;
                    return;
                }

                if (s_GetTimeOfDay(&accept_time_stop) != 0)
                    memset(&accept_time_stop, 0, sizeof(accept_time_stop));
                accept_time_elapsed = 
                    s_TimeDiff(&accept_time_stop, &accept_time_start);
                last_accept_time_elapsed = 
                    s_TimeDiff(&accept_time_stop, &m_LastSuccAcceptTime);
                if (s_GetTimeOfDay(&m_LastSuccAcceptTime) != 0) {
                    memset(&m_LastSuccAcceptTime, 0, 
                           sizeof(m_LastSuccAcceptTime));
                }

                CSocket* my_sock;
                m_SocketPool.push_back(
                    CSocketAPI::SPoll(my_sock = sock, eIO_ReadWrite));
                iters_passed++;
                m_Busy = true;
                char buf[4096];
                size_t n_read = 0;
                size_t n_written = 0;
                my_sock->SetTimeout(eIO_ReadWrite, &rw_timeout);
                my_sock->SetTimeout(eIO_Close, &c_timeout);
                my_sock->Read(buf, sizeof(buf), &n_read);
                buf[n_read] = '\0';
                string request = buf;
                if (request.length() > 10) {
                    request = request.substr(4, NPOS);
                    request = request.erase(request.find("HTTP"), NPOS);
                    WRITE_LOG("Answered healthcheck for " << request << 
                               " after trying accept for " 
                               << accept_time_elapsed 
                               << "s, last successful accept was "
                               << last_accept_time_elapsed << "s ago");
                }
                if (request == "/health" || request == "") {
                    WRITE_LOG("Answered healthcheck for " << request << 
                               " after trying accept for " 
                               << accept_time_elapsed 
                               << "s, last successful accept was "
                               << last_accept_time_elapsed << "s ago");
                }
                const char healthy_answer[] =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 4\r\n"
                    "Content-Type: text/plain;charset=UTF-8\r\n"
                    "\r\n"
                    "OK\r\n";
                my_sock->Write(healthy_answer, sizeof(healthy_answer) - 1,
                    &n_written);

                if (iters_passed == iters_btw_grbg_cllct) {
                    iters_passed = 0;
                    CollectGarbage();
                }
            }
        }
    }
        
    
protected:
    // As it is said in ncbithr.hpp, destructor must be protected
    ~CHealthcheckThread()
    {
        WRITE_LOG("~CHealthcheckThread() started");
        for (unsigned int i = 0; i < m_ListeningSockets.size(); ++i) {
            CListeningSocket* l_sock = static_cast<CListeningSocket*>
                (m_ListeningSockets[i].m_Pollable);
            l_sock->Close();
            delete l_sock;
        }
        for (unsigned int i = 0; i < 100 /* random number */; ++i) {
            if (!HasGarbage()) break;
            CollectGarbage();
            SleepMilliSec(20);
        }
        m_ListeningSockets.clear();
        WRITE_LOG("~CHealthcheckThread() ended");
    }

private:
    /* Go through sockets in collection and remove closed ones */
    void CollectGarbage()
    {
        WRITE_LOG("CHealthcheckThread::CollectGarbage() started, size of "
                  "m_SocketPool is " << m_SocketPool.size());
        size_t   n_ready;
        STimeout rw_timeout = { 1, 20000 };
        /* 
         * Divide polls in parts if there are more that 60 sockets 
         */
        size_t polls_size = m_SocketPool.size(), chunk_size = 60, j = 0;
        // do we have more than one chunk?
        if (polls_size > chunk_size) {
            // handle all but the last chunk
            for (; j < polls_size - chunk_size; j += chunk_size) {
                vector<CSocketAPI::SPoll>::iterator begin, end;
                begin = m_SocketPool.begin() + j;
                end = m_SocketPool.begin() + j + chunk_size;
                vector<CSocketAPI::SPoll> polls(begin, end);
                CSocketAPI::Poll(polls, &rw_timeout, &n_ready);
                /* save result of poll */
                for (size_t k = 0; k < chunk_size; ++k) {
                    m_SocketPool[j + k].m_REvent = polls[k].m_REvent;
                }
            }
        }
        // if we still have a part of a chunk left, handle it
        if (polls_size - j > 0) {
            auto begin = m_SocketPool.begin() + j;
            auto end = m_SocketPool.end();
            vector<CSocketAPI::SPoll> polls(begin, end);
            CSocketAPI::Poll(polls, &rw_timeout, &n_ready);
            for (size_t k = 0; k < polls_size - j; ++k) {
                m_SocketPool[j + k].m_REvent = polls[k].m_REvent;
            }
        }
    
        /* We check sockets that have some events */
        unsigned int i;

        WRITE_LOG("m_SocketPool has " << m_SocketPool.size() << " sockets");
        for (i = 0; i < m_SocketPool.size(); ++i) {
            if (m_SocketPool[i].m_REvent == eIO_ReadWrite) {
                /* If this socket has some event */
                CSocket* sock = 
                             static_cast<CSocket*>(m_SocketPool[i].m_Pollable);
                sock->Close();
                delete sock;
                /* Remove item from vector by swap and pop_back */
                swap(m_SocketPool[i], m_SocketPool.back());
                m_SocketPool.pop_back();
            }
        }
        WRITE_LOG("CHealthcheckThread::CollectGarbage() ended, size of "
                  "m_SocketPool is " << m_SocketPool.size());
    }

    bool HasGarbage() {
        return m_SocketPool.size() > 0;
    }
    
    void* Main(void) {
        s_Tls->SetValue(new int, TlsCleanup);
        *s_Tls->GetValue() = kHealthThreadNumber;
#ifdef NCBI_MONKEY
        CMonkey::Instance()->
            RegisterThread(kHealthThreadNumber);
#endif /* NCBI_MONKEY */
        WRITE_LOG("Healthcheck thread started");
        if (s_GetTimeOfDay(&m_LastSuccAcceptTime) != 0) {
            memset(&m_LastSuccAcceptTime, 0, sizeof(m_LastSuccAcceptTime));
        }
        while (m_RunHealthCheck) {
            AnswerHealthcheck();
        }
        return NULL;
    }
    /** Pool of listening sockets */
    vector<CSocketAPI::SPoll> m_ListeningSockets;
    /** Pool of sockets created by accept() */
    vector<CSocketAPI::SPoll> m_SocketPool;
    /** time of last successful accept() */
    struct timeval            m_LastSuccAcceptTime; 
    bool m_RunHealthCheck;
    bool m_Busy;
    map<unsigned short, short> m_ListenPorts;
};

static CHealthcheckThread* s_HealthcheckThread;



/** Check if expected number of specified servers is announced
 * (usually it is 0 or 1)
 * This function does not care about host (IP) of server.
 */
int s_CountServersWithExpectation(const string&     service,
                                  unsigned short    port,
                                  int               expected_count,
                                  size_t            code_line,
                                  int               secs_timeout,
                                  const string&     dtab = "")
{
    CConnNetInfo net_info;
    const int wait_time = 1000; /* msecs */
    int max_retries = secs_timeout * 1000 / wait_time; /* number of repeats
                                                          until timeout */
    int retries = 0;
    int servers = 0;
    MEASURE_TIME_START
    while (servers != expected_count && retries < max_retries) {
        WRITE_LOG("Counting number of servers \"" << service <<
                  "\" with dtab \"" << dtab <<
                  "\" and port " << port << ", ip " << ANNOUNCEMENT_HOST <<
                  " via service discovery "
                  "(expecting " << expected_count << 
                  " servers found). Retry #" << retries);
        servers = 0;
        /* Check that the server is actually announced and enabled */
        auto announced_srvrs = s_GetAnnouncedServers(false, { service }),
             enabled_srvrs   = s_GetAnnouncedServers(true,  { service });
        bool serv_enabled = false, serv_announced = false;
        for (unsigned short i = 0; i < announced_srvrs.size(); ++i) {
            if (announced_srvrs[i].port == port &&
                announced_srvrs[i].service == service) {
                serv_announced = true;
                break;
            }
        }
        for (unsigned short i = 0; i < enabled_srvrs.size(); ++i) {
            if (enabled_srvrs[i].port == port &&
                enabled_srvrs[i].service == service) {
                serv_enabled = true;
                break;
            }
        }
        if (retries > 0) { /* for the first cycle we do not sleep */
#ifdef NCBI_THREADS
            SleepMilliSec(wait_time);
#else
            /* Answer healthcheck a few times */
            for (int i = 0;  i < 10;  i++) {
                s_HealthcheckThread->AnswerHealthcheck();
            };
#endif /* NCBI_THREADS */
        }
        const SSERV_Info* info;

        if (dtab.length() > 1) {
            ConnNetInfo_SetUserHeader(*net_info, dtab.c_str());
        } else {
            ConnNetInfo_SetUserHeader(*net_info, "DTab-Local: ");
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
        MEASURE_TIME_FINISH
        WRITE_LOG("Found " << servers << " servers of service " 
                  << service << ", port " << port << " after " 
                  << time_elapsed << " seconds via service discovery.\n"
                  << "This service is " << (serv_announced ? "" : "NOT ") 
                  << "announced and is " << (serv_enabled ? "" : "NOT ")
                  << "enabled.");
        retries++;
    }
    /* If we did not find the expected amount of servers, there will be an error 
     * (just if to think logically).
     * To easier find what went wrong - save line of code that called 
     * s_CountServersWithExpectation(), port, count and expected count */
    if (servers != expected_count) {
        SLBOSResolutionError err;
        err.code_line = code_line;
        err.count = servers;
        err.expected_count = expected_count;
        err.port = port;
        s_ResolutionErrors.push_back(err);
    }
    return servers;
}

///////////////////////////////////////////////////////////////////////////////
//////////////               DECLARATIONS            //////////////////////////
///////////////////////////////////////////////////////////////////////////////
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
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
 * 6 .If SERV_MapperName(*iter) returns name of another mapper, return NULL   */
void EmptyCands__RunGetCandidates();
void ErrorUpdating__ReturnNull();
void HaveCands__ReturnNext();
void LastCandReturned__ReturnNull();
void DataIsNull__ReconstructData();
void WrongMapper__ReturnNull();
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If *net_info is NULL, construct own *net_info
 * 2. If read from LBOS successful, return s_op
 * 3. If read from LBOS successful and info pointer != NULL, write 
      first element NULL to info
 * 4. If read from LBOS unsuccessful or no such service, return 0            */
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
 * 20. LBOS is OFF - return eLBOSOff                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   
 * 22. Trying to announce server and providing dead healthcheck URL -
 *     return eLBOSNotFound       
 * 23. Trying to announce server and providing dead healthcheck URL -
 *     server should not be announced                                        
 * 24. Announce server with separate host and healtcheck - should be found in 
 *     %LBOS%/text/service                                                    */
void AllOK__ReturnSuccess();
void AllOK__DeannounceHandleProvided();
void AllOK__LBOSAnswerProvided();
void AllOK__AnnouncedServerSaved();
void NoLBOS__ReturnNoLBOSAndNotFind();
void NoLBOS__LBOSAnswerNull();
void NoLBOS__DeannounceHandleNull();
void LBOSErrorCode__ReturnServerErrorCode();
void LBOSError__LBOSAnswerProvided();
void LBOSError__DeannounceHandleNull();
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage();
void ForeignDomain__NoAnnounce();
void AlreadyAnnouncedInAnotherZone__ReturnMultizoneProhibited();
void IncorrectURL__ReturnInvalidArgs();
void IncorrectPort__ReturnInvalidArgs();
void IncorrectVersion__ReturnInvalidArgs();
void IncorrectServiceName__ReturnInvalidArgs();
void RealLife__VisibleAfterAnnounce();
void ResolveLocalIPError__ReturnDNSError();
void IP0000__ReplaceWithIP();
void LBOSOff__ReturnKLBOSOff();
void LBOSAnnounceCorruptOutput__ReturnServerError();
void HealthcheckDead__ReturnKLBOSSuccess();
void HealthcheckDead__AnnouncementOK();

}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void TestNullOrEmptyField(const char* field_tested);
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOSSuccess
    2.  Custom section has nothing in config - return eLBOSInvalidArgs
    3.  Section empty or NULL (should use default section and return 
        eLBOSSuccess)
    4.  Service is empty or NULL - return eLBOSInvalidArgs
    5.  Version is empty or NULL - return eLBOSInvalidArgs
    6.  port is empty or NULL - return eLBOSInvalidArgs
    7.  port is out of range - return eLBOSInvalidArgs
    8.  port contains letters - return eLBOSInvalidArgs
    9.  healthcheck is empty or NULL - return eLBOSInvalidArgs
    10. healthcheck does not start with http:// or https:// - return 
        eLBOSInvalidArgs                                                    */
void ParamsGood__ReturnSuccess();
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
 * 8. LBOS is OFF - return eLBOSOff                                         */
void Deannounced__Return1(unsigned short port);
void Deannounced__AnnouncedServerRemoved();
void NoLBOS__Return0();
void LBOSExistsDeannounce404__Return404();
void RealLife__InvisibleAfterDeannounce();
void ForeignDomain__DoNothing();
void NoHostProvided__LocalAddress();
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in LBOS                              */
void AllDeannounced__NoSavedLeft();
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

namespace ExtraResolveData
{
struct SHostCheck
{
    unsigned int host;
    bool check;
};

static void s_FakeInputTest(int servers_num)
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";    
    unsigned int temp_ip;
    unsigned short temp_port;
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info;
    /*
     * We know that iter is LBOS's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
                     g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info));
    int i = 0;
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*hostports != NULL,
                                   "Problem with fake massive input, "
                                   "no servers found. ");
    ostringstream temp_host;
    map<unsigned short, SHostCheck> hosts;
    for (i = 0;  i < 200;  i++) {
        temp_host.str("");
        temp_host << i+1 << "." <<  i+2 << "." <<  i+3 << "." << i+4
                    << ":" << (i+1)*215;
        SOCK_StringToHostPort(temp_host.str().c_str(), 
                                &temp_ip, 
                                &temp_port);
        hosts[temp_port] = SHostCheck{ temp_ip, false };
    }
    if (*hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            SHostCheck& host_check = hosts[hostports[i]->port];
            NCBITEST_CHECK_MESSAGE_MT_SAFE(
                host_check.host == hostports[i]->host,
                "Problem with recognizing port or IP in massive input");
            if (host_check.host == hostports[i]->host) {
                host_check.check = true;
            }
        }
        i = 0;
        auto iter = hosts.begin();
        for (; iter != hosts.end(); iter++) {
            if (iter->second.check)
                i++;
        }
        stringstream ss;
        ss << "Mapper should find " << servers_num << " hosts, but found " << i;
        NCBITEST_CHECK_MESSAGE_MT_SAFE(i == servers_num, ss.str().c_str());
    }
}
void ExtraData__DoesNotCrash()
{
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_FakeReadDiscoveryExtra<200>);
    s_FakeInputTest(200);
}

void EmptyExtra__DoesNotCrash()
{
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
        s_FakeReadDiscoveryEmptyExtra<200>);
    s_FakeInputTest(200);
}

void EmptyServInfoPart__Skip()
{
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
        s_FakeReadDiscoveryEmptyServInfoPart<200>);
    s_FakeInputTest(0);
}
} /* namespace ExtraResolveData */

namespace ExceptionCodes
{
void CheckCodes()
{    
    CLBOSException::EErrCode code;
    code = CLBOSException::s_HTTPCodeToEnum(400);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSBadRequest);
    code = CLBOSException::s_HTTPCodeToEnum(404);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSNotFound);
    code = CLBOSException::s_HTTPCodeToEnum(450);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSNoLBOS);
    code = CLBOSException::s_HTTPCodeToEnum(451);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSDNSResolveError);
    code = CLBOSException::s_HTTPCodeToEnum(452);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSInvalidArgs);
    code = CLBOSException::s_HTTPCodeToEnum(453);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSMemAllocError);
    code = CLBOSException::s_HTTPCodeToEnum(454);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSCorruptOutput);
    code = CLBOSException::s_HTTPCodeToEnum(500);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code,
                                 CLBOSException::EErrCode::e_LBOSServerError);
    code = CLBOSException::s_HTTPCodeToEnum(550);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSOff);
    /* Some unknown */
    code = CLBOSException::s_HTTPCodeToEnum(200);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSUnknown);
    code = CLBOSException::s_HTTPCodeToEnum(204);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSUnknown);
    code = CLBOSException::s_HTTPCodeToEnum(401);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSUnknown);
    code = CLBOSException::s_HTTPCodeToEnum(503);
    NCBITEST_CHECK_EQUAL_MT_SAFE(code, CLBOSException::EErrCode::e_LBOSUnknown);
}


/** Error codes - only for internal errors (that are not returned by 
 *  LBOS itself) */
void CheckErrorCodeStrings()
{
    string error_string;

    /* 400 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSBadRequest, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "");

    /* 404 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSNotFound, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "");
    
    /* 500 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSServerError, "",
                                  eLBOSServerError).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "");

    /* 450 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSNoLBOS, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "LBOS was not found");

    /* 451 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::
                                  e_LBOSDNSResolveError, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "DNS error. Possibly, cannot "
                                 "get IP of current machine or resolve "
                                 "provided hostname for the server");
    /* 452 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSInvalidArgs, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, 
                                 "Invalid arguments were provided. No "
                                 "request to LBOS was sent");

    /* 453 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSMemAllocError, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "Memory allocation error happened "
                                               "while performing request");

    /* 454 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSCorruptOutput, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "Failed to parse LBOS output.");

    /* 550 */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSOff, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, 
                                 "LBOS functionality is turned OFF. Check "
                                 "config file or connection to LBOS.");
    /* unknown */
    error_string = CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
                                  CLBOSException::EErrCode::e_LBOSUnknown, "",
                                  eLBOSBadRequest).GetErrCodeString();
    NCBITEST_CHECK_EQUAL_MT_SAFE(error_string, "Unknown LBOS error code");
}
}


namespace IPCache
{
/** Announce with host empty - resolving works, gets host from healthcheck,
 *  resolves host to IP and  saves result to cache. We compare real 
 *  IP with what was saved in cache
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void HostInHealthcheck__TryFindReturnsHostIP()
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announce host - resolving works and saves resolution result");
    /* Prepare for test. We need to be sure that there is no previously
    * registered non-deleted service. We count server with chosen port
    * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    /* Pre-cleanup */
    CLBOSIpCache::HostnameDelete(node_name, s_GetMyHost(), "1.0.0", port);
    
    /* Test - announce, then check TryFind */
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           string("http://") + s_GetMyHost() + 
                                           ":" + s_PortStr(PORT_N) + 
                                           "/health"));
    string IP = CLBOSIpCache::HostnameTryFind(node_name, 
                                              s_GetMyHost(), "1.0.0", port);
    string my_ip = s_GetMyIP();
    NCBITEST_CHECK_EQUAL_MT_SAFE(my_ip, IP);

    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));
}


/** Announce with host difficult from the one in healthcheck - resolving works, 
 *  resolves host to IP and saves result to cache. We compare real IP 
 *  with what was saved in cache
 * @attention 
 *  No multithread, test uses private methods that are not thread-safe by 
 *  themselves */
void HostSeparate__TryFindReturnsHostkIP()
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announce host - resolving works and saves resolution result");
    /* Prepare for test. We need to be sure that there is no previously
    * registered non-deleted service. We count server with chosen port
    * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    /* Pre-cleanup */
    CLBOSIpCache::HostnameDelete(node_name, s_GetMyHost(), "1.0.0", port);

    /* Test - announce, then check TryFind */
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", s_GetMyHost(),
                                           port, string("http://") + "iebdev11" 
                                           ":" + s_PortStr(PORT_N) +  "/health"));
    string IP = CLBOSIpCache::HostnameTryFind(node_name,
                                              s_GetMyHost(), "1.0.0", port);
    string my_ip = s_GetMyIP();
    NCBITEST_CHECK_EQUAL_MT_SAFE(my_ip, IP);

    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));
}


/** Do not announce host - HostnameTryFind returns the same hostname.
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void NoHost__TryFindReturnsTheSame()
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announce host - resolving works and saves resolution result");
    /* Prepare for test. We need to be sure that there is no previously
    * registered non-deleted service. We count server with chosen port
    * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    /* Pre-cleanup */
    CLBOSIpCache::HostnameDelete(node_name, "cnn.com", "1.0.0", port);
    
    /* Test - check TryFind without announcing */
    string IP = CLBOSIpCache::HostnameTryFind(node_name, 
                                              s_GetMyHost(), "1.0.0", port);
    string my_host = s_GetMyHost();
    NCBITEST_CHECK_EQUAL_MT_SAFE(my_host, IP);
}


/** Common part for all tests that check if resolution works as meant to be
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
static void s_LBOSIPCacheTest(const string& host, 
                              const string& expected_result = "N/A")
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing Resolve with host " << host << 
              ", expected result: " << expected_result);
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    string resolved_ip = CLBOSIpCache::HostnameResolve(node_name, host,
                                                       "1.0.0", port);
    string IP = CLBOSIpCache::HostnameTryFind(node_name, host, "1.0.0", port);

    NCBITEST_CHECK_EQUAL_MT_SAFE(resolved_ip, IP);

    /* If result is predictable */
    if (expected_result != "N/A")
        NCBITEST_CHECK_EQUAL_MT_SAFE(expected_result, IP);

    /* Cleanup */
    CLBOSIpCache::HostnameDelete(node_name, host, "1.0.0", port);
}    


/** Resolve hostname that can be resolved to multiple options - 
 *  the resolved IP gets saved and then is returned again and again for 
 *  the same service name, version and port
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void ResolveHost__TryFindReturnsIP()
{
    s_LBOSIPCacheTest("cnn.com");
}


/** Resolve and cache an IP address - it must be resolved to itself
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void ResolveIP__TryFindReturnsIP()
{
    s_LBOSIPCacheTest("130.14.25.27", "130.14.25.27");
}


/** Resolve an empty string - get "Unknown error".
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void ResolveEmpty__Error()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSUnknown, 400> 
    comparator("Internal error in LBOS Client IP Cache. Please contact developer\n");
    BOOST_CHECK_EXCEPTION(s_LBOSIPCacheTest("", s_GetMyIP()),
                         CLBOSException, comparator);
}


/** Actually, this behavior is not used anywhere, but this is the contract,
 *  and must be tested so it works if ever needed
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void Resolve0000__Return0000()
{
    s_LBOSIPCacheTest("0.0.0.0", "0.0.0.0");
}


/** Real-life test. Announce a host and deannounce it: cache MUST forget
 *  deannounced host
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void DeannounceHost__TryFindDoesNotFind()
{
    string host = s_GetMyHost();
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announce host - resolving works and saves resolution result");
    /* Prepare for test. We need to be sure that there is no previously
    * registered non-deleted service. We count server with chosen port
    * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    /* Pre-cleanup */
    CLBOSIpCache::HostnameDelete(node_name, host, "1.0.0", port);

    /* Test - announce, then check TryFind */
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", host,
                                           port, string("http://") + "iebdev11" 
                                           ":" + s_PortStr(PORT_N) +  "/health"));
    /* Deannounce */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", host,
                                         port));

    /* Check after deannounce */
    string IP = CLBOSIpCache::HostnameTryFind(node_name,
                                              host, "1.0.0", port);
    NCBITEST_CHECK_EQUAL_MT_SAFE(host, IP);
}


/** Test that for the second time rsolution result is taken from cache. Tested 
 *  with google.com which is very often resolved to different IPs
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void ResolveTwice__SecondTimeNoOp()
{
    string host = "google.com";
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Resolving the same name twice - on the second run function "
              "should return already saved result");
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    string resolved_ip = CLBOSIpCache::HostnameResolve(node_name, host,
                                                       "1.0.0", port);
    int i;
    stringstream sstream;
    string IP;
    BOOST_CHECK_NO_THROW(
        for (i = 0; i < 100; i++) {
            IP = CLBOSIpCache::HostnameResolve(node_name, host,
                                                      "1.0.0", port);    
            if (resolved_ip != IP)
                break; 
        }
    );
    sstream << "Resolve does return consistent answers (iteration " << i << ")";
    NCBITEST_CHECK_MESSAGE_MT_SAFE(resolved_ip == IP, sstream.str().c_str());
    /* Cleanup */
    CLBOSIpCache::HostnameDelete(node_name, host, "1.0.0", port);
}


/** Add host to cache and then remove it multiple times - no crashes should 
 *  happen. The value MUST be deleted and not found.
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void DeleteTwice__SecondTimeNoOp()
{
    string host = s_GetMyHost();
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Resolving the same name twice - on the second run function "
              "should return already saved result");
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    string resolved_ip = CLBOSIpCache::HostnameResolve(node_name, host,
                                                       "1.0.0", port);
    int i;
    stringstream sstream;
    string IP;
    BOOST_CHECK_NO_THROW(
        for (i = 0; i < 100; i++) {
            CLBOSIpCache::HostnameDelete(node_name, host, "1.0.0", port);
            IP = CLBOSIpCache::HostnameTryFind(node_name, host,
                                                      "1.0.0", port);    
            if (s_GetMyHost() != IP)
                break;
        }
    );
    sstream << "IP is stored in the cache even after it was deleted" <<
               "(iteration " << i << ")";
    string my_host = s_GetMyHost();
    NCBITEST_CHECK_MESSAGE_MT_SAFE(my_host == IP, sstream.str().c_str());
}


/** Finding a resolved hostname multiple times - just a stress test that can 
 *  show leaking memory
 * @attention
 *  No multithread, test uses private methods that are not thread-safe by
 *  themselves */
void TryFindTwice__SecondTimeNoOp()
{
    string host = "google.com";
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Resolving the same name twice - on the second run function "
        "should return already saved result");
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    string resolved_ip = CLBOSIpCache::HostnameResolve(node_name, host,
                                                       "1.0.0", port);
    int i;
    stringstream sstream;
    string IP;
    BOOST_CHECK_NO_THROW(
        for (i = 0; i < 100; i++) {
            IP = CLBOSIpCache::HostnameTryFind(node_name, host,
                                               "1.0.0", port);
            if (resolved_ip != IP)
                break; 
        }
    );
    sstream << "TryFind does not return consistent answers (iteration " << i  
            << "), original value: " << resolved_ip << ", outlying value: " 
            << IP;
    NCBITEST_CHECK_MESSAGE_MT_SAFE(resolved_ip == IP, sstream.str().c_str());
    /* Cleanup */
    CLBOSIpCache::HostnameDelete(node_name, host, "1.0.0", port);
}

} /* namespace IPCache */


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
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    /*
     * We know that iter is LBOS's.
     */
    SERV_Reset(*iter);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,  
                                       ": LBOS not found when should be");
        return;
    }
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand == 0,
                                   "Reset did not set n_cand to 0");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand == 0,
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
    CConnNetInfo net_info;
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL, "LBOS not found when should be");
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
        NCBITEST_CHECK_EQUAL_MT_SAFE(data->n_cand, 0U);
        NCBITEST_CHECK_EQUAL_MT_SAFE(data->pos_cand, 0U);
    }
    return;
}


void Multiple_AfterGetNextInfo__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";   
    SLBOS_Data* data;
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL, 
                                       "LBOS not found when should be");
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand == 0,
                                       "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand == 0,
            "Reset did not set pos_cand "
            "to 0");

        HOST_INFO host_info; // we leave garbage here for a reason
        SERV_GetNextInfoEx(*iter, &host_info);
        data = static_cast<SLBOS_Data*>(iter->data);
        NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand > 0,
                               "n_cand should be more than 0 after "
                               "GetNextInfo");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand > 0,
                               "pos_cand should be more than 0 after "
                               "GetNextInfo");
        if (iter->op == NULL) {
            NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL,
                "Mapper returned NULL when it should "
                "return s_op");
            return;
        }
        NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(iter->op->mapper, "lbos") == 0,
                               "Name of mapper that returned answer "
                               "is not \"lbos\"");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Close != NULL,
                               "Close operation pointer "
                               "is null");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Feedback != NULL,
                               "Feedback operation pointer is null");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->GetNextInfo != NULL,
                               "GetNextInfo operation pointer is null");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Reset != NULL,
                               "Reset operation pointer is null");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(host_info == NULL,
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
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    SERV_GetNextInfo(*iter);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*
     * We know that iter is LBOS's.
     */
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");
}

void FullCycle__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbostest";
    SLBOS_Data* data;
    size_t i;
    CConnNetInfo net_info;
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
    SELECT_PORT(count_before, service, port1);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port1,
                     (string("http://") + ANNOUNCEMENT_HOST + ":"
                             + s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), NULL);
    lbos_answer = NULL;

    SELECT_PORT(count_before, service, port2);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port2,
                     (string("http://") + ANNOUNCEMENT_HOST + ":"
                             + s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), NULL);
    lbos_answer = NULL;

    SELECT_PORT(count_before, service, port3);
    s_AnnounceCSafe(service.c_str(), "1.0.0", "", port3,
                     (string("http://") + ANNOUNCEMENT_HOST + ":"
                             + s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), NULL);
    lbos_answer = NULL;

    unsigned int servers_found1 =
        s_CountServersWithExpectation(service, port1, 1, __LINE__, 
                                      kDiscoveryDelaySec),
                 servers_found2 =
        s_CountServersWithExpectation(service, port2, 1, __LINE__, 
                                      kDiscoveryDelaySec),
                 servers_found3 =
        s_CountServersWithExpectation(service, port3, 1, __LINE__,
                                      kDiscoveryDelaySec);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(servers_found1 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(servers_found2 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(servers_found3 == 1, "lbostest was announced, "
                                                   "but cannot find it");
    /*
     * 1. We close after first GetNextInfo
     */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(*iter != NULL,
                           "Could not find announced lbostest nodes");
    SERV_GetNextInfo(*iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");

    /*
     * 2. We close after half hosts checked with GetNextInfo
     */
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
            "Could not find announced lbostest nodes");
        return;
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    /*                                                            v half    */
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand / 2;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand > 0, "n_cand should be more than 0"
                                             " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand > 0, "pos_cand should be more "
                                               "than 0 after GetNextInfo");

    /* 3. We close after all hosts checked with GetNextInfo*/
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
            "Could not find announced lbostest nodes");
        return;
    }
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNextInfo");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNextInfo");

    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.0.0", "", port1,
                  &lbos_answer.Get(), NULL);
    s_DeannounceC(service.c_str(), "1.0.0", "", port2,
                  &lbos_answer.Get(), NULL);
    s_DeannounceC(service.c_str(), "1.0.0", "", port3,
                  &lbos_answer.Get(), NULL);

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
    CConnNetInfo net_info;
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
    
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_LBOS_header.substr(0, expected_header.length()) ==
                                expected_header, 
                           "Header with DTab did not combine as expected");
}

/** 2. Announce server with non-standard version and server with no standard 
    version at all. Both should be found via DTab                            */
void NonStandardVersion__FoundWithDTab()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbostest";
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort();
    /*
     * I. Non-standard version
     */ 
    int count_before =
            s_CountServers(service, port,
                           "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort();
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    }

    s_AnnounceCSafe(service.c_str(), "1.1.0", "", port,
                     (string("http://") + ANNOUNCEMENT_HOST + ":" + 
                     s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), NULL);

    unsigned int servers_found =
        s_CountServersWithExpectation(service, port, 1, __LINE__, 
                                      kDiscoveryDelaySec, "DTab-local: "
                                      "/lbostest=>/zk#/lbostest/1.1.0");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(servers_found == 1,
                           "Error while searching non-standard server "
                           "version with DTab");
    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.1.0", 
                    NULL, port, NULL, NULL);
    lbos_answer = NULL;
    
    /*
     * II. Service with no standard version in ZK config
     */
    service = "/lbostest1";
    count_before = s_CountServers(service, port,
                                  "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort();
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    }
    s_AnnounceCSafe(service.c_str(), "1.1.0", "", port,
                    ("http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  
                    "/health").c_str(),
                    &lbos_answer.Get(), NULL);
    servers_found = 
        s_CountServersWithExpectation(service, port, 1, __LINE__, 
                                      kDiscoveryDelaySec, "DTab-local: "
                                      "/lbostest1=>/zk#/lbostest1/1.1.0");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(servers_found == 1,
                           "Error while searching server with no standard"
                           "version with DTab");

    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.1.0", 
        NULL, port, NULL, NULL);
    lbos_answer = NULL;
}

/* 3. Mix of registry DTab and HTTP Dtab and request Dtab: registry goes first,
 *    then goes ConnNetInfo DTab, then goes RequestContext */
void DTabRegistryAndHttpAndRequestContext__RegistryGoesFirst()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                        s_FakeReadWithErrorFromLBOSCheckDTab);
    ConnNetInfo_SetUserHeader(*net_info,
                              "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    CDiagContext::GetRequestContext().SetDtab("DTab-local: /lbostest=>/zk#/lbostest/1.2.0");
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    string expected_header = "DTab-local: /lbostest=>/zk#/lbostest/1.0.0; "
                              "/lbostest=>/zk#/lbostest/1.1.0 ; "
                              "/lbostest=>/zk#/lbostest/1.2.0";

    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_LBOS_header.substr(0, expected_header.length()) ==
                           expected_header,
                           "Header with DTab did not combine as expected");
}

/** 4. Announce server with non-standard version and server with no standard 
       version at all. Both should be found via Request Context DTab         */
void NonStandardVersion__FoundWithRequestContextDTab()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbostest";
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort();
    /*
     * I. Non-standard version
     */ 
    int count_before =
            s_CountServers(service, port,
                           "DTab-local: /lbostest=>/zk#/lbostest/1.2.0");
    while (count_before != 0) {
        port = s_GeneratePort();
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest=>/zk#/lbostest/1.2.0");
    }

    s_AnnounceCSafe(service.c_str(), "1.2.0", "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) + "/health").c_str(),
                    &lbos_answer.Get(), NULL);

    CDiagContext::GetRequestContext().SetDtab("DTab-local: /lbostest=>"
                                              "/zk#/lbostest/1.2.0");
    unsigned int servers_found =
        s_CountServersWithExpectation(service, port, 1, __LINE__, 
                                      kDiscoveryDelaySec, "DTab-local: "
                                      "/lbostest=>/zk#/lbostest/1.1.0");
    CDiagContext::GetRequestContext().SetDtab("");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(servers_found == 1,
                           "Error while searching non-standard server "
                           "version with DTab");
    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.2.0", 
                    NULL, port, NULL, NULL);
    lbos_answer = NULL;
    
    /*
     * II. Service with no standard version in ZK config
     */
    service = "/lbostest1";
    count_before = s_CountServers(service, port,
                                  "DTab-local: /lbostest1=>/zk#/lbostest1/1.2.0");
    while (count_before != 0) {
        port = s_GeneratePort();
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest1=>/zk#/lbostest1/1.2.0");
    }
    s_AnnounceCSafe(service.c_str(), "1.2.0", "", port,
                  "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health",
                  &lbos_answer.Get(), NULL);

    CDiagContext::GetRequestContext().SetDtab("DTab-local: /lbostest1=>/zk#/lbostest1/1.2.0");
    servers_found = 
        s_CountServersWithExpectation(service, port, 1, __LINE__,
                                      kDiscoveryDelaySec, "DTab-local: "
                                      "/lbostest1=>/zk#/lbostest1/1.1.0");
    CDiagContext::GetRequestContext().SetDtab("");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(servers_found == 1,
                           "Error while searching server with no standard"
                           "version with DTab");

    /* Cleanup */
    s_DeannounceC(service.c_str(), "1.2.0", 
        NULL, port, NULL, NULL);
    lbos_answer = NULL;
}


} /* namespace DTab */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace ResolveViaLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServiceExists__ReturnHostIP();
void LegacyService__ReturnHostIP();
void ServiceDoesNotExist__ReturnNULL();
void NoLBOS__ReturnNULL();
void FakeMassiveInput__ShouldProcess();
void FakeErrorInput__ShouldNotCrash();

void ServiceExists__ReturnHostIP()
{
    string service = "/lbos";
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
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
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->host > 0, 
                                           "Problem with getting host for "
                                           "server");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->port > 0, 
                                           "Problem with getting port for "
                                           "server");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->port < 65535, 
                                           "Problem with getting port for "
                                           "server");
        }
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(i > 0, 
                                   "Problem with searching for service");
}


/** Look for accn2gi and check if a valid IP is returned */
void LegacyService__ReturnHostIP()
{
    string service = "accn2gi";
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    /* Announce the legacy service */

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
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->host > 0, 
                                           "Problem with getting host for "
                                           "server");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->port > 0, 
                                           "Problem with getting port for "
                                           "server");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports[i]->port < 65535, 
                                           "Problem with getting port for "
                                           "server");
        }
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(i > 0, "Problem with searching for service");
}


void ServiceDoesNotExist__ReturnNULL()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    /*
     * We know that iter is LBOS's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                        lbos_addr.c_str(),
                                        service.c_str(),
                                        *net_info));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports.count() == 0, "Mapper should not find "
                                                   "service, but it somehow "
                                                   "found.");
}


void NoLBOS__ReturnNULL()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info;
    /*
     * We know that iter is LBOS's.
     */

    CCObjArrayHolder<SSERV_Info> hostports(
                           g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                                "lbosdevacvancbinlmnih.gov:80",
                                                service.c_str(),
                                                *net_info));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hostports.count() == 0,
                           "Mapper should not find LBOS, but it somehow" 
                           "found.");
}


struct SHostCheck
{
    unsigned int host;
    bool check;
};


void FakeMassiveInput__ShouldProcess()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";    
    unsigned int temp_ip;
    unsigned short temp_port;
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info;
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*hostports != NULL,
                           "Problem with fake massive input, "
                           "no servers found. "
                           "Most likely, problem with test.");
    ostringstream temp_host;
    map<unsigned short, SHostCheck> hosts;
    for (i = 0;  i < 200;  i++) {
        temp_host.str("");
        temp_host << i+1 << "." <<  i+2 << "." <<  i+3 << "." << i+4
                    << ":" << (i+1)*215;
        SOCK_StringToHostPort(temp_host.str().c_str(), 
                                &temp_ip, 
                                &temp_port);
        hosts[temp_port] = SHostCheck{ temp_ip, false };
    }
    if (*hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            SHostCheck& host_check = hosts[hostports[i]->port];
            NCBITEST_CHECK_MESSAGE_MT_SAFE(
                host_check.host == hostports[i]->host,
                "Problem with recognizing port or IP in massive input");
            if (host_check.host == hostports[i]->host) {
                host_check.check = true;
            }
        }
        i = 0;
        auto iter = hosts.begin();
        for (; iter != hosts.end(); iter++) {
            if (iter->second.check)
                i++;
        }
        stringstream ss;
        ss << "Mapper should find 200 hosts, but found " << i;
        NCBITEST_CHECK_MESSAGE_MT_SAFE(i == 200, ss.str().c_str());
    }
}


/** Check that LBOS client shuffles input from LBOS. Check occurence of different
 * combinations when there are just 3 elements in array*/
void FakeMassiveInput__ShouldShuffle()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";    
    unsigned int temp_ip;
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info;
    size_t n_retries = 30000;
    /*
     * We know that iter is LBOS's.
     */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                    s_FakeReadDiscovery<3>);
    CCObjArrayHolder<SSERV_Info> hostports(
                     g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*hostports != NULL,
                                   "Problem with fake massive input, "
                                   "no servers found. "
                                   "Most likely, problem with test.");
    /* First, prepare examples for each group */
    unsigned short port1, port2, port3; /* we compare only ports, for speed */
    /* Combinations:
     * 1) 1, 2, 3
     * 2) 1, 3, 2
     * 3) 2, 1, 3
     * 4) 2, 3, 1
     * 5) 3, 1, 2
     * 6) 3, 2, 1
     */
    size_t n[6]; /* For 6 different combinations we count occurrence*/
    memset(n, 0, sizeof(n));
    SOCK_StringToHostPort("1.2.3.4:215", &temp_ip, &port1);
    SOCK_StringToHostPort("2.3.4.5:430", &temp_ip, &port2);
    SOCK_StringToHostPort("3.4.5.6:645", &temp_ip, &port3);
    /* we do 30 thousand retries */
    for ( size_t retries = 0;  retries < n_retries;  retries++ ) {
        hostports =
            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info);
        NCBITEST_CHECK_MESSAGE(*hostports != NULL, "ResolveIPPort return NULL "
                               "instead of SSERV_Info array");
        if (hostports[0]->port == port1) {
            if (hostports[1]->port == port2) {
                n[0]++;
            } else if (hostports[1]->port == port3) {
                n[1]++;
            }
        } else if (hostports[0]->port == port2) {
            if (hostports[1]->port == port1) {
                n[2]++;
            } else if (hostports[1]->port == port3) {
                n[3]++;
            }
        } else if (hostports[0]->port == port3) {
            if (hostports[1]->port == port1) {
                n[4]++;
            } else if (hostports[1]->port == port2) {
                n[5]++;
            }
        }
    }
    /** Not any combination should occur more than with n_retries/50 deviation  
     * from average (n_retries/6)  */
    for (size_t j = 0; j < 6; j++) {
        ostringstream message;
        message << "Combination " << j << " has " << n[j] << " occurrences (too"
                   " much deviation from average of " << n_retries / 6 << ")";
        NCBITEST_CHECK_MESSAGE(n[j] >= n_retries / 6 - n_retries / 50, 
                               message.str().c_str());
        NCBITEST_CHECK_MESSAGE(n[j] <= n_retries / 6 + n_retries / 50, 
                               message.str().c_str());
    }
}


void FakeErrorInput__ShouldNotCrash()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";   
    unsigned int temp_ip;
    unsigned short temp_port;
    s_CallCounter = 0;
    CConnNetInfo net_info;
    /*
     * We know that iter is LBOS's.
     */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                    s_FakeReadDiscoveryCorrupt<200>);
    CCObjArrayHolder<SSERV_Info> hostports(
                        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                            "lbosdevacvancbinlmnih.gov", "/lbos", *net_info));
    int i=0; /* iterate test numbers*/
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*hostports != NULL,
                           "Problem with fake error input, no servers found. "
                           "Most likely, problem with test.");
    map<unsigned short, SHostCheck> hosts;
    for (i = 0;  i < 200;  i++) {
        stringstream ss;
        ss << 0 << i + 1 << "." 
           << 0 << i + 2 << "." 
           << 0 << i + 3 << "." 
           << 0 << i + 4;
        if ( ss.str().find('8') != string::npos ||
             ss.str().find('9') != string::npos ) 
        {
            WRITE_LOG(ss.str() << " Has 8 or 9, skipping");
            continue;
        }
        ss << ":" << (i + 1) * 215;
        SOCK_StringToHostPort(ss.str().c_str(), &temp_ip, &temp_port);
        hosts[temp_port] = SHostCheck{ temp_ip, false };
    }
    if (*hostports) {
        for (i = 0;  hostports[i] != NULL;  i++) {
            SHostCheck& host_check = hosts[hostports[i]->port];
            NCBITEST_CHECK_MESSAGE_MT_SAFE(
                host_check.host == hostports[i]->host,
                "Problem with recognizing port or IP in massive input");
            if (host_check.host == hostports[i]->host) {
                host_check.check = true;
            }
        }
        i = 0;
        auto iter = hosts.begin();
        for (; iter != hosts.end(); iter++) {
            if (iter->second.check)
                i++;
        }
        stringstream ss;
        ss << "Mapper should find 80 hosts, but found " << i;
        NCBITEST_CHECK_MESSAGE_MT_SAFE(i == 80, ss.str().c_str());
    }
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
    CCObjHolder<char> addresses(
                           g_LBOS_GetLBOSAddressEx(eLBOSFindMethod_CustomHost,
                                                     custom_lbos.c_str()));
    /* I. Custom address */
    WRITE_LOG("Part 1. Testing custom LBOS address");
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(addresses.Get()), custom_lbos);

    /* II. Registry address */
    string registry_lbos =
            CNcbiApplication::Instance()->GetConfig().Get("CONN", "LBOS");
    WRITE_LOG("Part 2. Testing registry LBOS address");
    addresses = g_LBOS_GetLBOSAddressEx(eLBOSFindMethod_Registry, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(addresses.Get()), registry_lbos);

    /* We have to fake last method, because its result is dependent on
     * location */
    /* III. etc/ncbi address (should be 127.0.0.1) */
    WRITE_LOG("Part 3. Testing " LBOSRESOLVER_PATH);
    size_t buffer_size = 1024;
    size_t size;
    AutoArray<char> buffer(new char[buffer_size]);
    AutoPtr<IReader> reader(CFileReader::New(LBOSRESOLVER_PATH));
    ERW_Result result = reader->Read(buffer.get(), buffer_size, &size);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(result == eRW_Success, "Could not read "
                                     LBOSRESOLVER_PATH ", test failed");
    buffer.get()[size] = '\0';
    string lbosresolver(buffer.get() + 7);
    lbosresolver = NStr::Replace(lbosresolver, "\n", "");
    lbosresolver = NStr::Replace(lbosresolver, "\r", "");
    lbosresolver = lbosresolver.erase(lbosresolver.length() - 5);
    addresses = g_LBOS_GetLBOSAddressEx(eLBOSFindMethod_Lbosresolve, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(addresses.Get()), lbosresolver);
}

void CustomHostNotProvided__SkipCustomHost()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> addresses(
                           g_LBOS_GetLBOSAddressEx(eLBOSFindMethod_CustomHost,
                                                     NULL));
    /* We check the count of LBOS addresses */
    NCBITEST_CHECK_MESSAGE_MT_SAFE(addresses.Get() !=  NULL,
                                   "Custom host was not processed in "
                                   "g_LBOS_GetLBOSAddressEx");
}

void NoConditions__AddressDefOrder()
{
    CNcbiRegistry& registry = CNcbiApplication::Instance()->GetConfig();
    string lbos = registry.Get("CONN", "lbos");
    /* I. Registry has entry (we suppose that it has by default) */
    CCObjHolder<char> addresses(g_LBOS_GetLBOSAddress());
    WRITE_LOG("1. Checking LBOS address when registry LBOS is provided");
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(addresses.Get()), lbos);

    /* II. Registry has no entries - check that LBOS is read from
     *     LBOSRESOLVER_PATH */
    WRITE_LOG("2. Checking LBOS address when registry LBOS is not provided");
    size_t buffer_size = 1024;
    size_t size;
    AutoArray<char> buffer(new char[buffer_size]);
    AutoPtr<IReader> reader(CFileReader::New(LBOSRESOLVER_PATH));
    ERW_Result result = reader->Read(buffer.get(), buffer_size, &size);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(result == eRW_Success, "Could not read "
                                     LBOSRESOLVER_PATH ", test failed");
    buffer.get()[size] = '\0';
    string lbosresolver(buffer.get() + 7);
    lbosresolver = NStr::Replace(lbosresolver, "\n", "");
    lbosresolver = NStr::Replace(lbosresolver, "\r", "");
    lbosresolver = lbosresolver.erase(lbosresolver.length()-5);
    registry.Unset("CONN", "lbos");
    addresses = g_LBOS_GetLBOSAddress();
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(addresses.Get()), lbosresolver);

    /* Cleanup */
    registry.Set("CONN", "lbos", lbos);
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
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort,
                            s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                              SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                              *net_info, 0/*skip*/, 0/*n_skip*/,
                              0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(*iter == NULL,
                                     "LBOS found when it should not be");
}

void LBOSResponds__Finish()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_CallCounter);
    string service = "/lbos";    
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    s_CallCounter = 2;
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, 
        s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 3,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of alive LBOS");
}

/*Not thread safe because of s_LastHeader*/
void NetInfoProvided__UseNetInfo()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CCounterResetter resetter(s_CallCounter);
    CConnNetInfo net_info;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    ConnNetInfo_SetUserHeader(*net_info, "My header fq34facsadf");
    
    s_CallCounter = 2; // to get desired behavior from s_FakeResolveIPPort
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_LastHeader == "My header fq34facsadf\r\n",
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
    CConnNetInfo net_info;
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
            "LBOS not found when it should be");
        return;
    }
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_CallCounter = 0;

    /* If reset was just made, get candidates and return first of them.
     * We do not care about results, we care how many times algorithm tried
     * to resolve service  */
    SERV_Reset(*iter);
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
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
    CConnNetInfo net_info;
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info == 0, 
                                   "SERV_GetNextInfoEx: mapper did not "
                                   "react correctly to error in LBOS" );
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1, 
                                   "SERV_GetNextInfoEx:mapper did not "
                                   "react correctly to error in LBOS");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL, 
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info == 0, 
                                   "SERV_GetNextInfoEx: mapper did not "
                                   "react correctly to error in LBOS "
                                   "(info not NULL)" );
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 2,
                                   "SERV_GetNextInfoEx:mapper did not "
                                   "react correctly to error in LBOS "
                                   "(fillCandidates was not called once)");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
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
    CConnNetInfo net_info;

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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
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
            NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
                                           "SERV_GetNextInfoEx: fill "
                                           "candidates was called, but "
                                           "it should not be");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(info->port == port && 
                                           info->host == host,
                                           "SERV_GetNextInfoEx: mapper "
                                           "error with 'next' returned "
                                           "element");
            NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
                                           "SERV_GetNextInfoEx: hinfo is not "
                                           "NULL (always should be NULL)");
        }
    }

    /* The main interesting here is to check if info is null, and that
     * 'fillcandidates()' was not called again internally*/
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info == NULL,
                                   "SERV_GetNextInfoEx: mapper error with "
                                   "'after last' returned element");
    stringstream ss;
    ss << "Mapper should find 200 hosts, but found " << i;
    NCBITEST_CHECK_MESSAGE_MT_SAFE(found_hosts == 200, ss.str().c_str());
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
    CConnNetInfo net_info;

    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<200>);
    /* If no candidates found yet, get candidates and return first of them. */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));

    /*ConnNetInfo_Destroy(*net_info);*/
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "LBOS for candidates");

    info = SERV_GetNextInfoEx(*iter, &hinfo);
    int i = 0;
    for (i = 0;  info != NULL;  i++) {
        info = SERV_GetNextInfoEx(*iter, &hinfo);
    }

    stringstream ss;
    ss << "Mapper should find 200 hosts, but found " << i;
    NCBITEST_CHECK_MESSAGE_MT_SAFE(i == 200, ss.str().c_str());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
                                   "SERV_GetNextInfoEx: hinfo is not NULL "
                                   "(always should be NULL)");
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
    NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 2,
                                   "SERV_GetNextInfoEx: mapper did "
                                   "not ask LBOS for candidates");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info->port == port && info->host == host,
                                   "SERV_GetNextInfoEx: mapper error "
                                   "with first returned element");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(hinfo == NULL,
                                   "SERV_GetNextInfoEx: hinfo is not "
                                   "NULL (always should be NULL)");
}


void WrongMapper__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;
    CConnNetInfo net_info;
    /* We will get iterator, and then change mapper name from it and run
     * GetNextInfo.
     * The mapper should return null */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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

    NCBITEST_CHECK_MESSAGE_MT_SAFE(info == NULL,
                                   "SERV_GetNextInfoEx: mapper did not "
                                   "react correctly to wrong mapper name");

    iter->op = origTable; /* Because we need to clean iter */
}
} /* namespace GetNextInfo */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Open
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

void NetInfoNull__ConstructNetInfo()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
                                           "LBOS not found when it should be");
        return;
    }
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL,
                            "Mapper returned NULL when it should return s_op");
        return;
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned "
                           "answer is not \"lbos\"");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Close != NULL, "Close "
                                                  "operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Feedback != NULL,
                                         "Feedback operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->GetNextInfo != NULL,
                                      "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Reset != NULL,
                                            "Reset operation pointer is null");
}


void ServerExists__ReturnLbosOperations()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    if (iter.get() == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter.get() == NULL,
                               "Problem with memory allocation, "
                               "calloc failed. Not enough RAM?");
        return;
    }
    iter->name = service.c_str();
    iter->ismask = 0;
    iter->arg = NULL;
    iter->val = NULL;
    CConnNetInfo net_info;

    iter->op = SERV_LBOS_Open(iter.get(), *net_info, NULL);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL,
                               "LBOS not found when it should be");
        return;
    }

    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL, 
                                   "Mapper returned NULL when it should "
                                   "return s_op");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(iter->op->mapper, "lbos") == 0,
                                   "Name of mapper that returned "
                                   "answer is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Close != NULL,
                                   "Close operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Feedback != NULL,
                                   "Feedback operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->GetNextInfo != NULL,
                                   "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Reset != NULL,
                                   "Reset operation pointer is null");

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->
        DestroyData(static_cast<SLBOS_Data*>(iter->data));
}


void InfoPointerProvided__WriteNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    iter->name = service.c_str();
    iter->ismask = 0;
    iter->arg = NULL;
    iter->val = NULL;
    SSERV_Info* info;
    CConnNetInfo net_info;
    
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, &info);
    if (iter->op == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL,
                                       "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(iter->op->mapper, "lbos") == 0,
                                   "Name of mapper that returned answer "
                                   "is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(info == NULL, "LBOS mapper provided "
                                                 "something in host info, "
                                                 "when it should not");

    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->
        DestroyData(static_cast<SLBOS_Data*>(iter->data));
}

void NoSuchService__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/donotexist";
    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(iter.get() != NULL,
                                     "Problem with memory allocation, "
                                     "calloc failed. Not enough RAM?");
    iter->name = service.c_str();
    iter->ismask = 0;
    iter->arg = NULL;
    iter->val = NULL;
    CConnNetInfo net_info;
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, NULL);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op == NULL,
                                   "Mapper returned s_op when it "
                                   "should return NULL");
}

void Dbaf__AppendDBNameToServiceName()
{
    CLBOSStatus lbos_status(true, true);
    CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<1>);
    string service = "dbinfo";    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    iter->name = service.c_str();
    iter->ismask = 0;
    iter->arg = "dbaf";
    iter->val = "pubmed";
    SSERV_Info* info;
    CConnNetInfo net_info;
    
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, &info);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(iter->op != NULL,
                                     "LBOS not found when it should be");
    NCBITEST_CHECK_EQUAL_MT_SAFE(s_LBOS_servicename, string("dbinfo/pubmed"));
    /* Cleanup */
    g_LBOS_UnitTesting_GetLBOSFuncs()->
        DestroyData(static_cast<SLBOS_Data*>(iter->data));
}

void NameIsMask__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    
    auto_ptr<SSERV_IterTag> iter(new SSERV_IterTag);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(iter.get() != NULL,
                                     "Problem with memory allocation, "
                                     "calloc failed. Not enough RAM?");
    iter->name = service.c_str();
    iter->ismask = 1;
    iter->arg = NULL;
    iter->val = NULL;
    CConnNetInfo net_info;
    iter->op = SERV_LBOS_Open(iter.get(), *net_info, NULL);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op == NULL,
                                   "Mapper returned s_op when it "
                                   "should return NULL");
}

} /* namespace Open */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GeneralLBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void ServerExists__ServOpenPReturnsLbosOperations()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";    
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
                                       "LBOS not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op != NULL,
                           "Mapper returned NULL when it "
                           "should return s_op");
    if (iter->op == NULL) return;
    NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned "
                           "answer is not \"LBOS\"");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Close != NULL,
                           "Close operation pointer "
                           "is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Feedback != NULL,
                           "Feedback operation "
                           "pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->GetNextInfo != NULL,
                           "GetNextInfo operation pointer is null");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(iter->op->Reset != NULL,
                           "Reset operation pointer "
                           "is null");
}


void LbosExist__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    ELBOSFindMethod find_methods_arr[] = {
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


void ServerDoesNotExist__ShouldReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/asdf/idonotexist";
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
                                   "Mapper should not find service, but "
                                   "it somehow found.");
}


void DbafUnknownDB__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "dbinfo";
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, "dbaf"/*arg*/, "mypubmed"/*val*/));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
                                   "Mapper should not find service, but "
                                   "it somehow found.");
}


void DbafKnownDB__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "dbinfo";
    CConnNetInfo net_info;
    const SSERV_Info* info = NULL;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, "dbaf"/*arg*/, "pubmed"/*val*/));
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(*iter != NULL,
                                   "LBOS did not find dbinfo/pubmed");
    int servers = -1;
    do {
        servers++;
        info = SERV_GetNextInfoEx(*iter, NULL);
    } while (info != NULL);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(servers > 0,
                                   "Mapper should find services dbinfo/pubmed, "
                                   "but nothing was found.");
}

void NameIsMask__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos*";
    CConnNetInfo net_info;
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
                                   "Mapper should not find service, but "
                                   "it somehow found.");
}

} /* namespace GeneralLBOS */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{


/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess()
{
#undef PORT_N
#define PORT_N 1
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    unsigned short deannounce_result;
    WRITE_LOG("Testing simple announce test. Should return 200.");
    int count_before;

    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count servers with chosen port 
     * and check if there is no server already announced */
    SELECT_PORT(count_before, node_name, port);
    unsigned short result;
    /*
     * I. Check with 0.0.0.0
     */
    WRITE_LOG("Part I : 0.0.0.0");
    result = s_AnnounceCSafe(node_name.c_str(),
                             "1.0.0",
                             "",
                             port,
                             "http://" ANNOUNCEMENT_HOST_0000 ":" +
                             s_PortStr(PORT_N) +  "/health",
                             &lbos_answer.Get(), &lbos_status_message.Get());
    
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);

    /* Cleanup */
    deannounce_result = s_DeannounceC(node_name.c_str(), "1.0.0", NULL, 
                                      port, NULL, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(deannounce_result, eLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /*
     * II. Now check with IP
     */
    WRITE_LOG("Part II: real IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(),
                    "1.0.0",
                    "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + ":" +
                    s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);

    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", ANNOUNCEMENT_HOST.c_str(),
                  port, NULL, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(deannounce_result, eLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
#undef PORT_N
#define PORT s_PortStr()
}


/*  3. Successfully announced : char* lbos_answer contains answer of LBOS    */
/* Test is thread-safe. */
void AllOK__LBOSAnswerProvided()
{
#undef PORT_N
#define PORT_N 2
    WRITE_LOG("Testing simple announce test. "
             "Announcement function should return answer of LBOS");
    WRITE_LOG("Part I : 0.0.0.0");
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
    SELECT_PORT(count_before, node_name, port);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  (string("http://") + s_GetMyHost() + ":" + s_PortStr(PORT_N) +  "/health").c_str(),
                  &lbos_answer.Get(), &lbos_status_message.Get());

    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "LBOS answer as expected");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                  NULL, NULL);
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP  */
    WRITE_LOG("Part II: real IP");
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) + "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "LBOS answer as expected");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", ANNOUNCEMENT_HOST.c_str(),
                  port, NULL, NULL);
                    
    lbos_answer = NULL;
    lbos_status_message = NULL;
}

/* If announced successfully - status message is "OK" */
void AllOK__LBOSStatusMessageIsOK()
{
    WRITE_LOG("Testing simple announce test. Announcement function should "
              "return answer of LBOS");
    WRITE_LOG("Part I : 0.0.0.0");
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
    SELECT_PORT(count_before, node_name, port);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://" ANNOUNCEMENT_HOST_0000 ":") + 
                    s_PortStr(PORT_N) + "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        string("OK"));
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                  NULL, NULL);
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP  */
    WRITE_LOG("Part II: real IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0", ANNOUNCEMENT_HOST.c_str(),
                  port, NULL, NULL);
    lbos_answer = NULL;
    lbos_status_message = NULL;
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden LBOS mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved()
{
#undef PORT_N
#define PORT_N 3
    WRITE_LOG("Testing saving of parameters of announced server when "
              "announcement finished successfully");
    WRITE_LOG("Part I : 0.0.0.0");
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
    SELECT_PORT(count_before, node_name, port);
    /* Announce */
    unsigned short result;
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                             "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                             s_PortStr(PORT_N) +  "/health",
                             &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(result == eLBOSSuccess,
                                   "Announcement function did not return "
                                   "SUCCESS as expected");
    int find_result = s_FindAnnouncedServer(node_name, "1.0.0", port,
                                            "0.0.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    int deannounce_result = s_DeannounceC(node_name.c_str(), 
                                          "1.0.0", "", port,
                                          &lbos_answer.Get(),
                                          &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(deannounce_result == eLBOSSuccess,
                                   "Deannouncement function did not return "
                                   "SUCCESS as expected");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Now check with IP instead of host name */
    WRITE_LOG("Part II: real IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    /* Announce */
    s_AnnounceCSafe(node_name.c_str(),
                    "1.0.0",
                    "",
                    port,
                    (string("http://") + ANNOUNCEMENT_HOST +
                            ":" + s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(result == eLBOSSuccess,
                                   "Announcement function did not return "
                                   "SUCCESS as expected");
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port,
                                        ANNOUNCEMENT_HOST);
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    deannounce_result = s_DeannounceC(node_name.c_str(), 
                                      "1.0.0",
                                      "",
                                      port, NULL, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(deannounce_result, eLBOSSuccess);
    lbos_answer = NULL;
    lbos_status_message = NULL;
#undef PORT_N
#define PORT_N 0
}


/*  5. Could not find LBOS: return NO_LBOS                                   */
/* Test is NOT thread-safe. */
void NoLBOS__ReturnNoLBOSAndNotFind()
{
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found."
                " Should return eLBOSNoLBOS");
    CLBOSStatus lbos_status(true, true);
    unsigned short result;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()");
    
    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    
    result = s_AnnounceC(node_name,
                         "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSNoLBOS);

    /* Cleanup*/
    WRITE_LOG("Reverting mock of CONN_Read() with s_FakeReadEmpty()");
}


/*  6. Could not find LBOS : char* lbos_answer is set to NULL                */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSAnswerNull()
{
    WRITE_LOG(
             "Testing behavior of LBOS when no LBOS is found."
             " LBOS answer should be NULL.");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG(
        "Mocking CONN_Read() with s_FakeReadEmpty()");

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                       s_FakeReadEmpty);
    s_AnnounceC(node_name.c_str(),
                "1.0.0",
                "", port,
                "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                s_PortStr(PORT_N) +  "/health",
                &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");

    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()");
}


/*  6. Could not find LBOS: char* lbos_status_message is set to NULL         */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSStatusMessageNull()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found. "
              "LBOS message should be NULL.");
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()");

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                    s_FakeReadEmpty);
    s_AnnounceC(node_name.c_str(),
                "1.0.0",
                "", port,
                "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                s_PortStr(PORT_N) +  "/health",
                &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");

    /* Cleanup*/
    WRITE_LOG(
            "Reverting mock of CONN_Read() with s_FakeReadEmpty()");
}


/*  8. LBOS returned error: return eLBOSServerError                          */
/* Test is NOT thread-safe. */
void LBOSError__ReturnServerErrorCode()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when LBOS returns error code (507).");
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()");

    /* Announce */
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                   "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health",
                   &lbos_answer.Get(), &lbos_status_message.Get());

    /* Check that error code is the same as in mock*/
    NCBITEST_CHECK_MESSAGE_MT_SAFE(result == 507,
                           "Announcement did not return "
                           "eLBOSDNSResolveError as expected");
    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()");
}


/*  8.5. LBOS returned error: return eLBOSServerError                         */
/* Test is NOT thread-safe. */
void LBOSError__ReturnServerStatusMessage()
{
    CLBOSStatus       lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when LBOS returns error message "
              "(\"LBOS STATUS\").");
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                    s_FakeReadAnnouncementWithErrorFromLBOS);
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health",
                &lbos_answer.Get(), &lbos_status_message.Get());

    /* Check that error code is the same as in mock*/
    NCBITEST_CHECK_MESSAGE_MT_SAFE(strcmp(*lbos_status_message, "LBOS STATUS") == 0,
                           "Announcement did not return "
                           "eLBOSDNSResolveError as expected");
    /* Cleanup*/
    WRITE_LOG("Reverting mock of CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()");
}


/*  9. LBOS returned error : char* lbos_answer contains answer of LBOS       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when LBOS returns error code. Should return "
             "exact message from LBOS");
    WRITE_LOG("Mocking CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                  "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                  s_PortStr(PORT_N) +  "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(string(*lbos_answer ? *lbos_answer : "<NULL>"),
                         "Those lbos errors are scaaary");
    /* Cleanup*/
    WRITE_LOG("Reverting mock of CONN_Read() with "
              "s_FakeReadAnnouncementWithErrorFromLBOS()");
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage()
{
#undef PORT_N
#define PORT_N 0 
    WRITE_LOG("Testing behavior of LBOS "
                 "mapper when server was already announced (info stored in "
                 "internal storage should be replaced. Server node should be "
                 "rewritten, no duplicates.");
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
    SELECT_PORT(count_before, node_name, port);
    unsigned short result;
    const char* convert_result;
    /*
     * First time
     */
    WRITE_LOG("Part 1. First time announcing server");
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                      s_PortStr(PORT_N) +  "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                             "Did not get answer after announcement");
    convert_result = 
            SOCK_StringToHostPort(*lbos_answer, &lbos_addr, &lbos_port);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(convert_result != NULL,
                           "Host:port returned by LBOS is trash");
    /* Count how many servers there are */
    int count_after = 0;
    count_after = s_CountServersWithExpectation(node_name, port, 1,
                                                __LINE__, kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    WRITE_LOG("Trying to find the announced server in "
              "%LBOS%/lbos/text/service");
    int servers_in_text_service = s_FindAnnouncedServer(node_name, "1.0.0", 
                                                        port, "0.0.0.0");
    WRITE_LOG("Found  " << servers_in_text_service << 
              " servers in %LBOS%/lbos/text/service (should be " << 
              count_before + 1 << ")");

    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_in_text_service, count_before + 1);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * Second time
     */
    WRITE_LOG("Part 2. Second time announcing server");
    result = s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                      s_PortStr(PORT_N) +  "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_REQUIRE_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                             "Did not get answer after announcement");
    convert_result = SOCK_StringToHostPort(*lbos_answer,
                                           &lbos_addr,
                                           &lbos_port);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(convert_result != NULL &&
                           convert_result != *lbos_answer,
                           "LBOS answer could not be parsed to host:port");
    /* Count how many servers there are. */
    count_after = s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    WRITE_LOG("Trying to find the announced server in "
              "%LBOS%/lbos/text/service");
    servers_in_text_service = s_FindAnnouncedServer(node_name, "1.0.0", 
                                                    port, "0.0.0.0");
    WRITE_LOG("Found " << servers_in_text_service << 
              " servers in %LBOS%/lbos/text/service (should be " << 
              count_before + 1 << ")");

    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_in_text_service, count_before + 1);

    /* Cleanup */     
    int deannounce_result = s_DeannounceC(node_name.c_str(), 
                                            "1.0.0",
                                            ANNOUNCEMENT_HOST.c_str(),
                                            port, NULL, NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(deannounce_result, eLBOSSuccess);
#undef PORT_N
#define PORT_N 0 
}


/* 12. Trying to announce in foreign domain - do nothing and 
       return that no LBOS is found (because no LBOS in current 
       domain is found) */
/* Test is NOT thread-safe. */
void ForeignDomain__NoAnnounce()
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
                   "return error code eLBOSNoLBOS).");
        WRITE_LOG("Mocking region with \"or-wa\"");
        CMockString mock1(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");

        unsigned short result;
        result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                               "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health",
                               &lbos_answer.Get(), &lbos_status_message.Get());
        NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSNoLBOS);
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                               "LBOS status message is not NULL");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                               "Answer from LBOS was not NULL");
        /* Cleanup*/
        WRITE_LOG("Reverting mock of region with \"or-wa\"");
    }
#endif
}


/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ReturnInvalidArgs()
{
#undef PORT_N
#define PORT_N 4
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect healthcheck URL - should return "
             "eLBOSInvalidArgs");
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that equals NULL
     */
    WRITE_LOG("Part I. Healthcheck is <NULL>");
    unsigned short result;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port, NULL,
                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");
    /* Cleanup*/
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
     * II. Healthcheck URL that does not start with http or https
     */
    WRITE_LOG("Part II. Healthcheck is \"\" (empty string)");
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port, "",
                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");

    /*
    * III. Healthcheck URL that does not start with http or https
    */
    WRITE_LOG("Part III. Healthcheck is \""
              "lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health\" (no http://)");
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port, 
                "lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
        "Answer from LBOS was not NULL");
#undef PORT_N
#define PORT_N 0
}


/* 14. Was passed incorrect port: do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ReturnInvalidArgs()
{
#undef PORT_N
#define PORT_N 5
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect port (zero) - should return "
              "eLBOSInvalidArgs");
    unsigned short result;
    /* I. 0 */
    port = 0;
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                    "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "Answer from LBOS was not NULL");
#undef PORT_N
#define PORT_N 0
}


/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ReturnInvalidArgs()
{
#undef PORT_N
#define PORT_N 6
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    unsigned short result;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect version - should return "
             "eLBOSInvalidArgs");
    /*
     * I. NULL version
     */
    WRITE_LOG("Part I. Version is <NULL>");
    result = s_AnnounceC(node_name.c_str(), NULL, "", port,
                         ("http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health").c_str(),
                         &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");

    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /*
     * II. Empty version 
     */
    WRITE_LOG("Part II. Version is \"\" (empty string)");
    node_name = s_GenerateNodeName();
    result = s_AnnounceC(node_name.c_str(), "", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
        "Answer from LBOS was not NULL");
#undef PORT_N
#define PORT_N 0
}


/* 16. Was passed incorrect service name (NULL or empty): do not 
 *     announce and return INVALID_ARGS                                      */
/* Test is thread-safe. */
void IncorrectServiceName__ReturnInvalidArgs()
{
#undef PORT_N
#define PORT_N 7
    unsigned short result;
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect service name - should return "
              "eLBOSInvalidArgs");
    /*
     * I. NULL service name
     */
    WRITE_LOG("Part I. Service name is <NULL>");
    result = s_AnnounceC(NULL, "1.0.0", "", port,
                         ("http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) + "/health").c_str(),
                         &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
     * II. Empty service name
     */
    WRITE_LOG("Part II. Service name is \"\" (empty string)");
    node_name = s_GenerateNodeName();
    port = kDefaultPort;
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    result = s_AnnounceC("", "1.0.0", "", port, 
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "Answer from LBOS was not NULL");
#undef PORT_N
#define PORT_N 0
}


/* 17. Real-life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce()
{
#undef PORT_N
#define PORT_N 7
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    string node_name = s_GenerateNodeName();
    WRITE_LOG("Real-life test of LBOS: "
              "after announcement, number of servers with specific name, "
              "port and version should increase by 1");
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    unsigned short result;
    result = s_AnnounceCSafe(node_name, "1.0.0", "", port,
                             "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                             s_PortStr(PORT_N) +  "/health",
                             &lbos_answer.Get(), &lbos_status_message.Get());
    int count_after = s_CountServersWithExpectation(node_name, port, 1, 
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    /* Cleanup */
    s_DeannounceC(node_name.c_str(), "1.0.0",
                  "", port, NULL, NULL);
#undef PORT_N
#define PORT_N 0
}


/* 18. If was passed "0.0.0.0" as IP, change 0.0.0.0 to real IP */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithIP()
{
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host: "
              "it should be sent as-is to LBOS");
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"1.2.3.4\"");
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                             s_FakeGetLocalHostAddress<true, 1, 2, 3, 4>);
    WRITE_LOG("Mocking Announce with fake Anonounce");
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    unsigned short result;
    string health = "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) + 
                    "/health";
    result = s_AnnounceC(node_name, "1.0.0", "", port,
                         health, &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSDNSResolveError);
    stringstream healthcheck;
    healthcheck << "http%3A%2F%2F1.2.3.4%3A" + s_PortStr(PORT_N) +  "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL_MT_SAFE(s_LBOS_hostport, healthcheck.str().c_str());
    lbos_answer = NULL;
    lbos_status_message = NULL;

    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"251.252.253.147\"");
    mock1 = s_FakeGetLocalHostAddress<true, 251, 252, 253, 147>;
    s_AnnounceC(node_name, "1.0.0", "", port,
                "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                s_PortStr(PORT_N) +  "/health",
                &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSDNSResolveError);
    healthcheck.str(std::string());
    healthcheck << "http%3A%2F%2F251.252.253.147%3A" + s_PortStr(PORT_N) +  
                   "%2Fhealth" << "%2Fport" << port <<
                   "%2Fhost%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL_MT_SAFE(s_LBOS_hostport, healthcheck.str().c_str());
    /* Cleanup*/
    WRITE_LOG("Reverting mock of SOCK_gethostbyaddr");
    WRITE_LOG("Reverting mock of Announce with fake Anonounce");
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP - return eLBOSDNSResolveError                                       */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__ReturnDNSError()
{
    CLBOSStatus         lbos_status(true, true);
    CCObjHolder<char>   lbos_answer(NULL);
    CCObjHolder<char>   lbos_status_message(NULL);
    string node_name =  s_GenerateNodeName();
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host, "
              "and running SOCK_gethostbyaddr returns error: "
              "do not care, because we do not substitute 0.0.0.0, "
              "we send it as it is");
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"0.0.0.0\"");
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                              s_FakeGetLocalHostAddress<false,0,0,0,0>);
    unsigned short result;
    result = s_AnnounceC(node_name, "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSDNSResolveError);
    /* Cleanup*/
    lbos_answer = NULL;
    result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                           &lbos_answer.Get(), NULL);
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSDNSResolveError);
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"0.0.0.0\"");
}


/* 20. LBOS is OFF - return eLBOSOff                                          */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnKLBOSOff()
{
    CCObjHolder<char> lbos_answer(NULL);
    CLBOSStatus lbos_status(true, false);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = 8080;
    unsigned short result;
    WRITE_LOG("LBOS mapper is OFF (maybe it is not turned ON in registry " 
              "or it could not initialize at start) - return eLBOSOff");
    result = s_AnnounceC("lbostest", "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSOff);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "Answer from LBOS was not NULL");
}


/*21. Announced successfully, but LBOS return corrupted answer -
      return 454                                                              */
/* Test is NOT thread-safe. */
void LBOSAnnounceCorruptOutput__Return454()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Announced successfully, but LBOS returns corrupted answer - "
              "return eLBOSCorruptOutput");
    WRITE_LOG("Mocking CONN_Read with corrupt output");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    unsigned short result;
    result = s_AnnounceC(node_name, "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health",
                         &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSCorruptOutput);
    /* Cleanup */
    WRITE_LOG("Reverting mock of CONN_Read with corrupt output");
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return code from LBOS (200). If healthcheck is at non-existent domain - 
      return 400                                                              */
/* Test is thread-safe. */
void HealthcheckDead__ReturnKLBOSSuccess()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    unsigned short result;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "return code from LBOS(200). If healthcheck is at non - existent "
              "domain - return 400");
    /*
     * I. Healthcheck is dead completely 
     */
     WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
                "return  eLBOSBadRequest");
    result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                  "http://badhealth.gov", 
                  &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSBadRequest);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    /* Cleanup */
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * II. Healthcheck returns 404
     */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "return  eLBOSSuccess");
     result = s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                          "http://0.0.0.0:4097/healt", //wrong port
                          &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Answer from LBOS was NULL");
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should be announced                                         */
/* Test is thread-safe. */
void HealthcheckDead__AnnouncementOK()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "server should be announced");
    
    s_AnnounceC(node_name.c_str(), "1.0.0", "", port,
                "http://0.0.0.0:4097/healt",  //wrong port
                &lbos_answer.Get(), &lbos_status_message.Get());
    stringstream healthcheck;
    healthcheck << ":4097/healt" << "?port=" << port <<
                   "&host=" << "" << "&version=1.0.0";
    bool was_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                            healthcheck.str().c_str());
    NCBITEST_CHECK_EQUAL_MT_SAFE(was_announced, true);
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "", port, &lbos_answer.Get(), NULL);
}


} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOSSuccess                                       */
void ParamsGood__ReturnSuccess() 
{
    CLBOSStatus         lbos_status(true, true);
    CCObjHolder<char>   lbos_answer(NULL);
    CCObjHolder<char>   lbos_status_message(NULL);
    unsigned short      result;
    WRITE_LOG("Simple announcement from registry. "
              "Should return eLBOSSuccess");
    result = s_AnnounceCRegistry(NULL, &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                                   "Successful announcement did not end up "
                                   "with answer from LBOS");
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceAll();
}

/*  2.  Custom section has nothing in config - return eLBOSInvalidArgs      */
void CustomSectionNoVars__ReturnInvalidArgs()
{
    CLBOSStatus         lbos_status(true, true);
    CCObjHolder<char>   lbos_answer(NULL);
    CCObjHolder<char>   lbos_status_message(NULL);
    unsigned short      result;
    WRITE_LOG("Custom section has nothing in config - "
              "return eLBOSInvalidArgs");
    result = s_AnnounceCRegistry("EMPTY_SECTION",
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
}

/*  3.  Section empty or NULL (should use default section and return 
        eLBOSSuccess, if section is Good)                                   */
void CustomSectionEmptyOrNullAndDefaultSectionIsOk__ReturnSuccess()
{
    CLBOSStatus         lbos_status(true, true);
    CCObjHolder<char>   lbos_answer(NULL);
    CCObjHolder<char>   lbos_status_message(NULL);
    unsigned short      result;
    WRITE_LOG("Section empty or NULL - should use default section and return "
              "eLBOSSuccess, if section is Good)");
    /*
     * I. NULL section
     */
    result = s_AnnounceCRegistry(NULL, &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                                   "Successful announcement did not end up "
                                   "with answer from LBOS");
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceAll();
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* 
     * II. Empty section 
     */
    result = s_AnnounceCRegistry("", &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Successful announcement did not end up with "
                           "answer from LBOS");
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    /* Cleanup */
    s_DeannounceAll();
    lbos_answer = NULL;
}


void TestNullOrEmptyField(const char* field_tested)
{
    CLBOSStatus         lbos_status             (true, true);
    CCObjHolder<char>   lbos_answer             (NULL);
    unsigned short      result;
    CCObjHolder<char>   lbos_status_message     (NULL);
    string              null_section            = "SECTION_WITHOUT_";
    string              empty_section           = "SECTION_WITH_EMPTY_";
    string              field_name              = field_tested;
    /* 
     * I. NULL section 
     */
    WRITE_LOG("Part I. " << field_tested << " is not in section (NULL)");
    result = s_AnnounceCRegistry((null_section + field_name).c_str(),
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* 
     * II. Empty section 
     */
    WRITE_LOG("Part II. " << field_tested << " is an empty string");
    result = s_AnnounceCRegistry((empty_section + field_name).c_str(),
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
}

/*  4.  Service is empty or NULL - return eLBOSInvalidArgs                  */
void ServiceEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL - return eLBOSInvalidArgs");
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return eLBOSInvalidArgs                  */
void VersionEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Version is empty or NULL - return eLBOSInvalidArgs");
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return eLBOSInvalidArgs                     */
void PortEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Port is empty or NULL - return eLBOSInvalidArgs");
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return eLBOSInvalidArgs                      */
void PortOutOfRange__ReturnInvalidArgs()
{
    WRITE_LOG("Port is out of range - return eLBOSInvalidArgs");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    /*
     * I. port = 0 
     */
    WRITE_LOG("Part I. Port is 0");
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1",
                                  &lbos_answer.Get(), 
                                  &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * II. port = 100000 
     */
    WRITE_LOG("Part II. Port is 100000");
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2",
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /*
     * III. port = 65536 
     */
    WRITE_LOG("Part III. Port is 65536");
    result = s_AnnounceCRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3",
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
}

/*  8.  Port contains letters - return eLBOSInvalidArgs                     */
void PortContainsLetters__ReturnInvalidArgs()
{
    WRITE_LOG("Port contains letters - return eLBOSInvalidArgs");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    result = s_AnnounceCRegistry("SECTION_WITH_CORRUPTED_PORT",
                                 &lbos_answer.Get(), 
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;    
}

/*  9.  Healthcheck is empty or NULL - return eLBOSInvalidArgs              */
void HealthchecktEmptyOrNull__ReturnInvalidArgs()
{
    WRITE_LOG("Healthcheck is empty or NULL - return eLBOSInvalidArgs");
    TestNullOrEmptyField("HEALTHCHECK");
}

/*  10. Healthcheck does not start with http:// or https:// - return         
        eLBOSInvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs()
{
    WRITE_LOG("Healthcheck does not start with http:// or https:// - return "      
              "eLBOSInvalidArgs");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    result = s_AnnounceCRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK",
                                 &lbos_answer.Get(),
                                 &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSInvalidArgs);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                                   "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;    
}
/*  11. Trying to announce server providing dead healthcheck URL -
        return eLBOSSuccess (previously was eLBOSNotFound)                    */
void HealthcheckDead__ReturnKLBOSSuccess()
{
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "return eLBOSSuccess(previously was eLBOSNotFound)");
    CLBOSStatus         lbos_status         (true, true);
    CCObjHolder<char>   lbos_answer         (NULL);
    CCObjHolder<char>   lbos_status_message (NULL);
    unsigned short      result;
    /* 1. Non-existent domain in healthcheck */
    WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
              "return  eLBOSBadRequest");
    result = s_AnnounceCRegistry("SECTION_WITH_HEALTHCHECK_DNS_ERROR",
                                 &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSBadRequest);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    NCBITEST_CHECK_EQUAL_MT_SAFE(*lbos_answer, (char*)NULL);
    /* Cleanup */
    lbos_answer         = NULL;
    lbos_status_message = NULL;

    /* 2. Healthcheck is reachable but does not answer */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "return  eLBOSSuccess");
     result = s_AnnounceCRegistry("SECTION_WITH_DEAD_HEALTHCHECK",
                                    &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Answer from LBOS was NULL");
    /* Cleanup */
    s_DeannounceAll();
    lbos_answer         = NULL;  
    lbos_status_message = NULL;
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully de-announced: return eLBOSSuccess                          */
/*    Test is thread-safe. */
void Deannounced__Return1(unsigned short port)
{
#undef PORT_N
#define PORT_N 8
    WRITE_LOG("Successfully de-announced: return eLBOSSuccess");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_status_message(NULL);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short result;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    /* 
     * I. Check with 0.0.0.0
     */
    WRITE_LOG("Part I. 0.0.0.0");
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                  s_PortStr(PORT_N) +  "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get());
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1, 
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);

    result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                           &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* 
     * II. Now check with IP instead of 0.0.0.0 
     */
    WRITE_LOG("Part II. IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + 
                    ":" + s_PortStr(PORT_N) +  "/health").c_str(),
                    &lbos_answer.Get(), &lbos_status_message.Get());
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /* Count how many servers there are */
    count_after = s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);

    result = s_DeannounceC(node_name.c_str(), 
                           "1.0.0", s_GetMyIP().c_str(), port,
                           &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
#undef PORT_N
#define PORT_N 0
}


/* 2. Successfully de-announced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved()
{
#undef PORT_N
#define PORT_N 9
    WRITE_LOG("Successfully de-announced : if announcement was saved in local "
              "storage, remove it");
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
    SELECT_PORT(count_before, node_name, port);
    unsigned short result;
    /* 
     * I. Check with hostname
     */
    WRITE_LOG("Part I. Check with hostname");
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                   (string("http://") + s_GetMyHost() + ":8080/health").c_str(),
                   &lbos_answer.Get(), &lbos_status_message.Get());
    lbos_answer = NULL;
    lbos_status_message = NULL;
    int find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyHost());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    result = s_DeannounceC(node_name.c_str(), 
                           "1.0.0",
                           s_GetMyHost().c_str(),
                           port, &lbos_answer.Get(), 
                           &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyHost());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);
    lbos_answer = NULL;
    lbos_status_message = NULL;
    
    /* 
     * II. Now check with IP instead of host name 
     */
    WRITE_LOG("Part II. Check with IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    (string("http://") + ANNOUNCEMENT_HOST + 
                    ":8080/health").c_str(), 
                    &lbos_answer.Get(), &lbos_status_message.Get());
    /* Count how many servers there are */
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyIP());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Deannounce */
    result = s_DeannounceC(node_name.c_str(), "1.0.0",
                           s_GetMyIP().c_str(), port,
                           &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, s_GetMyIP());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;

    /*
    * III. Now check with "0.0.0.0"
    */
    WRITE_LOG("Part III. Check with 0.0.0.0");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                    "http://0.0.0.0:8080/health",
                    &lbos_answer.Get(), &lbos_status_message.Get());
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, "0.0.0.0");
    /* Count how many servers there are */
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    /* Cleanup */
    lbos_answer = NULL;
    lbos_status_message = NULL;
    /* Deannounce */
    result = s_DeannounceC(node_name.c_str(), "1.0.0",
                           "", port,
                           &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port, "0.0.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);
#undef PORT_N
#define PORT_N 0
}


/* 3. Could not connect to provided LBOS: fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0()
{
    WRITE_LOG("Could not connect to provided LBOS: fail and return 0");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read with Read_Empty");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                           &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSNoLBOS);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                           "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
    WRITE_LOG("Reverting mock of CONN_Read with Read_Empty");
}


/* 4. Successfully connected to LBOS, but deannounce returned 400: 
 *    return 400                                                             */
/* Test is thread-safe. */
void LBOSExistsDeannounce400__Return400()
{
    WRITE_LOG("Test: 1) Successfully connected to LBOS,\n "
              "2) deannounce returned 400:\n"
              "return 400");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* Currently LBOS does not return any errors */
    /* Here we can try to deannounce something non-existent */
    unsigned short result;
    unsigned short port = kDefaultPort;
    result =  s_DeannounceC("no such service", "no such version",
                            "127.0.0.1", port,
                            &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, 400);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Bad Request");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
}


/* 5. Real - life test : after de-announcement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce()
{
    WRITE_LOG("Real-life test : after de-announcement server should "
              "be invisible to resolve");
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    Deannounced__Return1(port);
    int count_after = s_CountServersWithExpectation(node_name, port, 0,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
}


/*6. If trying to deannounce in another domain - should return eLBOSNoLBOS */
/* We fake our domain so no address looks like our own domain */
/* Test is NOT thread-safe. */
void ForeignDomain__DoNothing()
{
#if 0 /* deprecated */
    /* Test is not run in TeamCity*/
    if (!getenv("TEAMCITY_VERSION")) {
        WRITE_LOG("Deannounce in another domain - return eLBOSNoLBOS");
        CLBOSStatus lbos_status(true, true);
        CCObjHolder<char> lbos_answer(NULL);
        CCObjHolder<char> lbos_status_message(NULL);
        unsigned short result;
        string node_name = s_GenerateNodeName();
        unsigned short port = kDefaultPort;
        WRITE_LOG("Mocking region with \"or-wa\"");
        CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
        result = s_DeannounceC(node_name.c_str(), "1.0.0", "", port, 
                               &lbos_answer.Get(), &lbos_status_message.Get());
        NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSNoLBOS);
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                             "LBOS status message is not NULL");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                               "Answer from LBOS was not NULL");
        /* Cleanup*/
        WRITE_LOG("Reverting mock of region with \"or-wa\"");
    }
#endif
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
/*  NOT ON WINDOWS. */
void NoHostProvided__LocalAddress()
{
    WRITE_LOG("Deannounce without IP specified - deannounce from local host");
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
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", port,
                  "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                  s_PortStr(PORT_N) +  "/health",
                  &lbos_answer.Get(), &lbos_status_message.Get());
    lbos_answer = NULL;
    lbos_status_message = NULL;
    healthcheck.str(std::string());
    healthcheck << ":" + s_PortStr(PORT_N) +  "/health" << "?port=" << port <<
                   "&host=" << "" << "&version=1.0.0";
    bool is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                           healthcheck.str().c_str(),
                                           true);
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, true);
    result = 
        s_DeannounceC(node_name.c_str(), "1.0.0", NULL, port, 
                      &lbos_answer.Get(), &lbos_status_message.Get());
    is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                      ":" + s_PortStr(PORT_N) +  "/health",
                                      false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSSuccess);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), "OK");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                                   "Answer from LBOS was not NULL");
}


/* 8. LBOS is OFF - return eLBOSOff                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnKLBOSOff()
{
    WRITE_LOG("Deannonce when LBOS mapper is OFF - return eLBOSOff");
    CLBOSStatus lbos_status(true, false);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result = 
        s_DeannounceC("lbostest", "1.0.0", "", 8080, 
                      &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSOff);
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_status_message == NULL,
                         "LBOS status message is not NULL");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
                           "Answer from LBOS was not NULL");
}
/* 9. Trying to deannounce non-existent service - return eLBOSNotFound      */
/*    Test is thread-safe. */
void NotExists__ReturnKLBOSNotFound()
{
    WRITE_LOG("Deannonce non-existent service - return eLBOSNotFound");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short result = 
        s_DeannounceC("notexists", "1.0.0", 
                        "", 8080, 
                        &lbos_answer.Get(), &lbos_status_message.Get());
    NCBITEST_CHECK_EQUAL_MT_SAFE(result, eLBOSNotFound);
    NCBITEST_CHECK_EQUAL_MT_SAFE(
        string(*lbos_status_message ? *lbos_status_message : "<NULL>"), 
        "Not Found");
    NCBITEST_CHECK_MESSAGE_MT_SAFE(*lbos_answer == NULL,
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
void AllDeannounced__NoSavedLeft()
{
    WRITE_LOG("DeannonceAll - should deannounce everyrithing that was announced");
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    CCObjHolder<char> lbos_status_message(NULL);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Part I. Announcing");
    for (i = 0;  i < 10;  i++) {
        int count_before;
        SELECT_PORT(count_before, node_name, port);
        ports.push_back(port);
        counts_before.push_back(count_before);
        s_AnnounceCSafe(node_name.c_str(), "1.0.0", "", ports[i],
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                      s_PortStr(PORT_N) +  "/health",
                      &lbos_answer.Get(), &lbos_status_message.Get());
        lbos_answer = NULL;
        lbos_status_message = NULL;
    }
    WRITE_LOG("Part II. DeannounceAll");
    s_DeannounceAll();

    WRITE_LOG("Part III. Checking discovery - should find nothing");
    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(
            s_CountServersWithExpectation(
                    s_GenerateNodeName(), ports[i], counts_before[i], __LINE__,
                    kDiscoveryDelaySec));
        NCBITEST_CHECK_EQUAL_MT_SAFE(counts_before[i], counts_after[i]);
    }
}
}



// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess()
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing simple announce test. Should return 200.");
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    /*
    * I. Check with 0.0.0.0
    */
    WRITE_LOG("Part I : 0.0.0.0");
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           "http://" ANNOUNCEMENT_HOST_0000 ":" 
                                           + s_PortStr(PORT_N) + 
                                           "/health"));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));

    /* Now check with IP instead of host name */
    /*
     * II. Now check with IP
     */
    WRITE_LOG("Part II: real IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    string health = string("http://") + ANNOUNCEMENT_HOST + ":8080/health";
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           health));
    /* Count how many servers there are */
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(),
                                         port));
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden LBOS mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved()
{
    WRITE_LOG("Testing saving of parameters of announced server when "
              "announcement finished successfully");
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    WRITE_LOG("Part I : 0.0.0.0");
    SELECT_PORT(count_before, node_name, port);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                         "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                         s_PortStr(PORT_N) +  "/health"));
    int find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                            "0.0.0.0");
    NCBITEST_CHECK_NE_MT_SAFE(find_result, -1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0",
                                         "", port));
    /* Now check with IP instead of host name */
    WRITE_LOG("Part II: real IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          string("http://") + ANNOUNCEMENT_HOST + 
                          ":8080/health"));
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0",
                                        port, s_GetMyIP().c_str());
    NCBITEST_CHECK_NE_MT_SAFE(find_result, -1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));
}


/*  5. Could not find LBOS: throw exception NO_LBOS                          */
/* Test is NOT thread-safe. */
void NoLBOS__ThrowNoLBOSAndNotFind()
{
    WRITE_LOG("Testing behavior of LBOS when no LBOS is found.");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSNoLBOS" <<
                "\", status code \"" << 450 << 
                "\", message \"" << "450\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSNoLBOS, 450> comparator("450\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read() with s_FakeReadEmpty()");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port,
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                          s_PortStr(PORT_N) +  "/health"),
                          CLBOSException, comparator);

    /* Cleanup*/
    WRITE_LOG(
        "Reverting mock of CONN_Read() with s_FakeReadEmpty()");
}


/*  8. LBOS returned unknown error: return its code                          */
/* Test is NOT thread-safe. */
void LBOSError__ThrowServerError()
{
    WRITE_LOG("LBOS returned unknown error: "
                " Should throw exception with received code (507)");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSUnknown" <<
                "\", status code \"" << 507 << 
                "\", message \"" << 
                "507 LBOS STATUS Those lbos errors are scaaary\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSUnknown, 507> comparator(
        "507 LBOS STATUS Those lbos errors are scaaary\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_FakeReadAnnouncementWithErrorFromLBOS);
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port,
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                          s_PortStr(PORT_N) +  "/health"),
            CLBOSException, comparator);
}


/*  9. LBOS returned error : char* lbos_answer contains answer of LBOS       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided()
{
    WRITE_LOG("LBOS returned unknown error: "
                " Exact message from LBOS should be provided");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSUnknown" <<
                "\", status code \"" << 507 << 
                "\", message \"" << 
                "507 LBOS STATUS Those lbos errors are scaaary\\n" << "\".");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    try {
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                      s_PortStr(PORT_N) +  "/health");
    }
    catch(const CLBOSException& ex) {
        /* Checking that message in exception is exactly what LBOS sent*/
        NCBITEST_CHECK_MESSAGE_MT_SAFE(
            ex.GetErrCode() == CLBOSException::EErrCode::e_LBOSUnknown,
            "LBOS exception contains wrong error type");
        const char* ex_message =
            strstr(ex.what(), "Error: ") + strlen("Error: ");
        string message = "";
        message.append(ex_message);
        NCBITEST_CHECK_EQUAL_MT_SAFE(
            message, string("507 LBOS STATUS Those lbos errors are scaaary\n"));
        return;
    } 
    catch (...) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(false, "Wrong exception was generated");
        return;
    }
    NCBITEST_CHECK_MESSAGE_MT_SAFE(false, "No exception was generated");
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage()
{
    WRITE_LOG("Testing behavior of LBOS "
                 "mapper when server was already announced (info stored in "
                 "internal storage should be replaced. Server node should be "
                 "rewritten, no duplicates.");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    //SleepMilliSec(1500); //ZK is not that fast
    /*
     * First time
     */
    WRITE_LOG("Part 1. First time announcing server");
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           "http://" ANNOUNCEMENT_HOST_0000 ":" 
                                           + s_PortStr(PORT_N) + "/health"));
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    int find_result = s_FindAnnouncedServer(node_name, "1.0.0", port,
                                            "0.0.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    /*
     * Second time
     */
    WRITE_LOG("Part 2. Second time announcing server");
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                          s_PortStr(PORT_N) +  "/health"));
    /* Count how many servers there are.  */
    count_after = s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                                kDiscoveryDelaySec);
    find_result = s_FindAnnouncedServer(node_name, "1.0.0", port,
                                        "0.0.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    /* Cleanup */     
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port));
}


/* 13. Was passed incorrect healthcheck URL (empty, not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ThrowInvalidArgs()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                           comparator ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect healthcheck URL");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
              "\", message \"" << "452\\n" << "\".");
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that does not start with http or https
     */
    WRITE_LOG("Part I. Healthcheck is <NULL>");
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, "0.0.0.0:8080/health"),
                      CLBOSException, comparator);
    /*
     * II. Empty healthcheck URL
     */
    WRITE_LOG("Part II. Healthcheck is \"\" (empty string)");
    port = kDefaultPort;
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", port, ""),
            CLBOSException, comparator);
}


/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ThrowInvalidArgs()
{
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect port (zero)");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                           comparator ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    /* Count how many servers there are before we announce */
    BOOST_CHECK_EXCEPTION(
            s_AnnounceCPP(node_name, "1.0.0", "", 0,
                          "http://0.0.0.0:8080/health"), CLBOSException, comparator);
}


/* 15. Was passed incorrect version(empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ThrowInvalidArgs()
{
    WRITE_LOG("Testing behavior of LBOS "
             "mapper when passed incorrect version - should return "
             "eLBOSInvalidArgs");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                           comparator ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    node_name = s_GenerateNodeName();
    WRITE_LOG("Version is \"\" (empty string)");
    /*
     * I. Empty string
     */
    string health = "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                    s_PortStr(PORT_N) +  "/health";
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
void IncorrectServiceName__ThrowInvalidArgs()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                           comparator ("452\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Testing behavior of LBOS "
              "mapper when passed incorrect service name - should return "
              "eLBOSInvalidArgs");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSInvalidArgs" <<
              "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    /*
     * I. Empty service name
     */
    WRITE_LOG("Service name is \"\" (empty string)");
    node_name = s_GenerateNodeName();
    port = kDefaultPort;
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP("", "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +
                      "/health"), CLBOSException, comparator);
}


/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce()
{
    CLBOSStatus lbos_status(true, true);
    unsigned short port = kDefaultPort;
    string node_name = s_GenerateNodeName();
    WRITE_LOG("Real-life test of LBOS: "
              "after announcement, number of servers with specific name, "
              "port and version should increase by 1");
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                          s_PortStr(PORT_N) +  "/health"));
    int count_after = s_CountServersWithExpectation(node_name, port, 1,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));
}


/* 18. If was passed "0.0.0.0" as IP, we change  0.0.0.0 to real IP           */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithIP()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSDNSResolveError, 451> 
                                                           comparator ("451\n");
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                           */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host: "
        "it should be sent as-is to LBOS");
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"1.2.3.4\"");
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                             s_FakeGetLocalHostAddress<true, 1, 2, 3, 4>);
    WRITE_LOG("Mocking Announce with fake Anonounce");
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,     
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +
                      "/health"), CLBOSException, comparator);
    stringstream healthcheck;
    healthcheck << "http%3A%2F%2F1.2.3.4%3A" + s_PortStr(PORT_N) +  "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost" << "" << "%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL_MT_SAFE(s_LBOS_hostport, healthcheck.str().c_str());
    mock1 = s_FakeGetLocalHostAddress<true, 251,252,253,147>;
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, 
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) + 
                      "/health"), CLBOSException, comparator);
    healthcheck.str(std::string());
    healthcheck << "http%3A%2F%2F251.252.253.147%3A" + s_PortStr(PORT_N) +  
                   "%2Fhealth" << 
                   "%2Fport" << port <<
                   "%2Fhost" << "" << "%2Fversion1.0.0";
    NCBITEST_CHECK_EQUAL_MT_SAFE(s_LBOS_hostport, healthcheck.str().c_str());
    /* Cleanup*/
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"1.2.3.4\"");
    WRITE_LOG("Reverting mock of Announce with fake Anonounce");
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP - do not announce and return DNS_RESOLVE_ERROR                      */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__ReturnDNSError()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSDNSResolveError, 451>
                                                            comparator("451\n");
    CLBOSStatus lbos_status(true, true);
    WRITE_LOG("If healthcheck has 0.0.0.0 specified as host, "
              "and running SOCK_gethostbyaddr returns error: "
              "do not care, because we do not substitute 0.0.0.0, "
              "we send it as it is");
    /* Here we mock SOCK_gethostbyaddrEx to know IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    string node_name     = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking SOCK_gethostbyaddr with \"0.0.0.0\"");
    CMockFunction<FLBOS_SOCKGetLocalHostAddressMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->LocalHostAddr,
                              s_FakeGetLocalHostAddress<false,0,0,0,0>);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) + 
                      "/health"), CLBOSException, comparator);
    /* Cleanup*/
    WRITE_LOG("Reverting mock og SOCK_gethostbyaddr with \"0.0.0.0\"");
}

/* 20. LBOS is OFF - return eLBOSOff                                        */
/* Test is NOT thread-safe. */
void LBOSOff__ThrowKLBOSOff()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSOff, 550> 
                                                            comparator("550\n");
    CLBOSStatus lbos_status(true, false);
    WRITE_LOG("LBOS mapper is OFF (maybe it is not turned ON in registry " 
              "or it could not initialize at start) - return eLBOSOff");
    WRITE_LOG("Expected exception with error code \"" << "e_LBOSOff" <<
              "\", status code \"" << 550 <<
                "\", message \"" << "550\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP("lbostest", "1.0.0", "", 8080,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) + 
                      "/health"), 
                      CLBOSException, comparator);
}


/*21. Announced successfully, but LBOS return corrupted answer -
      return 454 and exact answer of LBOS                                    */
void LBOSAnnounceCorruptOutput__ThrowServerError()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSCorruptOutput, 454>
                                            comparator ("454 Corrupt output\n");
    WRITE_LOG("Announced successfully, but LBOS returns corrupted answer.");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSCorruptOutput" << 
                "\", status code \"" << 454 <<
                "\", message \"" << "454 Corrupt output\\n" << "\".");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Mocking CONN_Read with corrupt output");
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port,
                      "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +
                      "/health"), CLBOSException, comparator);
    /* Cleanup */
    WRITE_LOG("Reverting mock of CONN_Read with corrupt output");
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return eLBOSNotFound                                                  */
void HealthcheckDead__ThrowE_NotFound()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    int count_before;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "return code from LBOS(200). If healthcheck is at non - existent "
              "domain - throw exception");
    /*
     * I. Healthcheck is on non-existent domain
     */
     WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
                "return  eLBOSBadRequest");
    SELECT_PORT(count_before, node_name, port);
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSBadRequest, 400> 
                                                comparator("400 Bad Request\n");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPP(node_name, "1.0.0", "", port, "http://badhealth.gov"), 
                      CLBOSException, comparator);
    int count_after = s_CountServersWithExpectation(node_name, port, 0,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);

    /*
     * II. Healthcheck returns 404
     */
     WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
                "should work fine");
    SELECT_PORT(count_before, node_name, port);
    BOOST_CHECK_NO_THROW(                    //  missing 'h'
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,// v
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + 
                          s_PortStr(PORT_N) + "/healt"));
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
    lbos_answer = NULL;
    s_DeannounceC(node_name.c_str(), "1.0.0", "cnn.com", port, 
                  &lbos_answer.Get(), NULL);
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should be announced                                             */
void HealthcheckDead__AnnouncementOK()
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - " 
              "server should be announced");
    /* We do not care if announcement throws or not (we check it in other 
       tests), we check if server is announced */
    try {
        s_AnnounceCPP(node_name, "1.0.0", "", port, "http://0.0.0.0:8080/healt");
    }
    catch(...)
    {
    }
    stringstream healthcheck;
    healthcheck << ":8080/healt" << "?port=" << port <<
                   "&host=" << "" << "&version=" << "1.0.0";
    bool is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                           healthcheck.str().c_str());
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, true);
}


/* 24. Announce server with separate host and healthcheck - should be found in
*     %LBOS%/text/service                                                   */
void SeparateHost__AnnouncementOK()
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    CCObjHolder<char> lbos_status_message(NULL);
    unsigned short port = kDefaultPort;
    WRITE_LOG("Trying to announce server with separate host that is not the "
              "same as healtcheck host - return code from LBOS(200).");
    int count_before;
    SELECT_PORT(count_before, node_name, port);

    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "cnn.com", port,
                                           "http://" ANNOUNCEMENT_HOST_0000 ":" 
                                           + s_PortStr(PORT_N) + 
                                           "/health"));
    int count_after = s_CountServersWithExpectation(node_name, port, 1,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);

    stringstream healthcheck;
    healthcheck << ":4097/healt" << "?port=" << port <<
                   "&host=" << "cnn.com" << "&version=1.0.0";
    string announced_ip = CLBOSIpCache::HostnameTryFind(node_name, "cnn.com", 
                                                    "1.0.0", port);
    bool is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                           healthcheck.str().c_str(),
                                           true, announced_ip);
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, true);
    lbos_answer = NULL;
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name.c_str(), "1.0.0",
                                         "cnn.com", port));
}
} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry_CXX /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Default section has all parameters correct in 
        config) - return eLBOSSuccess                                        */
void ParamsGood__ReturnSuccess() 
{
    WRITE_LOG("Simple announcement from registry. "
              "Should work fine");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_NO_THROW(s_AnnounceCPPFromRegistry(string()));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}

/*  2.  Custom section has nothing in config - return eLBOSInvalidArgs       */
void CustomSectionNoVars__ThrowInvalidArgs()
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                            comparator("452\n");
    WRITE_LOG("Testing custom section that has nothing in config");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("NON-EXISTENT_SECTION"),
        CLBOSException, comparator);
}

/*  3.  Section empty (should use default section and return 
        eLBOSSuccess, if section is Good)                                    */
void CustomSectionEmptyOrNullAndSectionIsOk__AllOK()
{
    CLBOSStatus lbos_status(true, true);
    WRITE_LOG("Section empty or NULL - should use default section all work "
              "fine, if default section is Good");
    /* 
     * Empty section 
     */
    BOOST_CHECK_NO_THROW(s_AnnounceCPPFromRegistry(""));
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}


void TestNullOrEmptyField(const char* field_tested)
{
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                            comparator("452\n");
    CLBOSStatus lbos_status(true, true);
    string null_section = "SECTION_WITHOUT_";
    string empty_section = "SECTION_WITH_EMPTY_";
    string field_name = field_tested;
    /* 
     * I. NULL section 
     */
    WRITE_LOG("Part I. " << field_tested << " is not in section (NULL)");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry((null_section + field_name)),
        CLBOSException, comparator);
    /* 
     * II. Empty section 
     */
    WRITE_LOG("Part II. " << field_tested << " is an empty string");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry((empty_section + field_name)),
        CLBOSException, comparator);
}

/*  4.  Service is empty or NULL - return eLBOSInvalidArgs                  */
void ServiceEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL");
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return eLBOSInvalidArgs                  */
void VersionEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Version is empty or NULL");
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return eLBOSInvalidArgs                     */
void PortEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Service is empty or NULL");
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return eLBOSInvalidArgs                      */
void PortOutOfRange__ThrowInvalidArgs()
{
    WRITE_LOG("Port is out of range - return eLBOSInvalidArgs");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                            comparator("452\n");
    /*
     * I. port = 0 
     */
    WRITE_LOG("Part I. Port is 0");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1"),
        CLBOSException, comparator);
    /*
     * II. port = 100000 
     */
    WRITE_LOG("Part II. Port is 100000");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
       s_AnnounceCPPFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2"),
        CLBOSException, comparator);
    /*
     * III. port = 65536 
     */
    WRITE_LOG("Part III. Port is 65536");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3"),
        CLBOSException, comparator);
}
/*  8.  Port contains letters - return eLBOSInvalidArgs                     */
void PortContainsLetters__ThrowInvalidArgs()
{
    WRITE_LOG("Port contains letters");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                            comparator("452\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_CORRUPTED_PORT"),
        CLBOSException, comparator);
}
/*  9.  Healthcheck is empty or NULL - return eLBOSInvalidArgs              */
void HealthchecktEmptyOrNull__ThrowInvalidArgs()
{
    WRITE_LOG("Healthcheck is empty or NULL");
    TestNullOrEmptyField("HEALTHCHECK");
}
/*  10. Healthcheck does not start with http:// or https:// - return         
        eLBOSInvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ThrowInvalidArgs()
{
    WRITE_LOG("Healthcheck does not start with http:// or https://");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSInvalidArgs" <<
                "\", status code \"" << 452 <<
                "\", message \"" << "452\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                            comparator("452\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK"),
        CLBOSException, comparator);
}
/*  11. Trying to announce server and providing dead healthcheck URL -
        return eLBOSNotFound                                                */
void HealthcheckDead__ThrowE_NotFound()
{
    WRITE_LOG("Trying to announce server providing dead healthcheck URL - "
              "should be OK");
    CLBOSStatus lbos_status(true, true);
    unsigned short port = 5001;
    string node_name = s_GenerateNodeName();
    int count_before;
    count_before = s_CountServers(node_name, port);
    /* 1. Non-existent domain in healthcheck */
    WRITE_LOG("Part I. Healthcheck is \"http://badhealth.gov\" - "
              "return  eLBOSBadRequest");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSBadRequest, 400> 
                                                comparator("400 Bad Request\n");
    BOOST_CHECK_EXCEPTION(
        s_AnnounceCPPFromRegistry("SECTION_WITH_HEALTHCHECK_DNS_ERROR"),
                                   CLBOSException, comparator);
    int count_after = s_CountServersWithExpectation(node_name, port, 0, 
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);


    /* 2. Healthcheck is reachable but does not answer */
    WRITE_LOG("Part II. Healthcheck is \"http:/0.0.0.0:4097/healt\" - "
              "return  eLBOSSuccess");
    BOOST_CHECK_NO_THROW(
                   s_AnnounceCPPFromRegistry("SECTION_WITH_DEAD_HEALTHCHECK"));
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    count_after = s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);

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
void Deannounced__Return1(unsigned short port)
{
    WRITE_LOG("Simple deannounce test");
    CLBOSStatus lbos_status(true, true);
    string node_name   = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    /*
    * I. Check with 0.0.0.0
    */
    WRITE_LOG("Part I. 0.0.0.0");
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health"));
    /* Count how many servers there are */
    int count_after = s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);
    /* Cleanup */
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", "", port));
    /*
    * II. Now check with IP instead of 0.0.0.0
    */
    WRITE_LOG("Part II. IP");
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    string health = string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) +  "/health";
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           health));
    /* Count how many servers there are */
    count_after = s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                                kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after, count_before + 1);

    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(), port));
}


/* 2. Successfully de-announced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved()
{
    WRITE_LOG("Successfully de-announced : if announcement was saved in local "
              "storage, remove it");
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    string my_host = s_GetMyHost();
    /*
     * I. Check with hostname
     */
    WRITE_LOG("Part I. Check with hostname");
    BOOST_CHECK_NO_THROW(
        s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                          string("http://") + s_GetMyHost() + (":8080/health")));
    /* Check that server is in storage */
    int find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                            s_GetMyIP());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 1);
    BOOST_CHECK_NO_THROW(s_DeannounceCPP(node_name, "1.0.0", s_GetMyHost(),
                                         port));
    /* Check that server was removed from storage */
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                        s_GetMyIP());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    SELECT_PORT(count_before, node_name, port);
    /*
     * II. Now check with IP instead of host name
     */
    WRITE_LOG("Part II. Check with IP");
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           string("http://") + 
                                           ANNOUNCEMENT_HOST +
                                           ":8080/health"));
    /* Check that server is in storage */
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                        s_GetMyIP());
    NCBITEST_CHECK_NE_MT_SAFE(find_result, -1);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", s_GetMyIP(), port));
    /* Check that server was removed from storage */
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                        s_GetMyIP());
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);
    /*
     * III. Check with 0.0.0.0
     */
    WRITE_LOG("Part III. Check with IP");
    BOOST_CHECK_NO_THROW(s_AnnounceCPPSafe(node_name, "1.0.0", "", port,
                                           "http://0.0.0.0:8080/health"););
    /* Check that server is in storage */
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                        "0.0.0.0");
    NCBITEST_CHECK_NE_MT_SAFE(find_result, 0);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port));
    /* Check that server was removed from storage */
    find_result = s_FindAnnouncedServer(node_name.c_str(), "1.0.0", port,
                                        "0.0.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(find_result, 0);
}


/* 3. Could not connect to provided LBOS : fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0()
{
    WRITE_LOG("Could not connect to provided LBOS");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSNoLBOS" <<
                "\", status code \"" << 450 <<
                "\", message \"" << "450\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSNoLBOS, 450> 
                                                            comparator("450\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP(node_name, "1.0.0", "", port),
        CLBOSException, comparator);
}


/* 4. Successfully connected to LBOS, but deannounce returned error: 
 *    return 0                                                               */
/* Test is thread-safe. */
void LBOSExistsDeannounceError__Return0()
{
    WRITE_LOG("Could not connect to provided LBOS");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSBadRequest" <<
                "\", status code \"" << 400 <<
                "\", message \"" << "400 Bad Request\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSBadRequest, 400> 
                                                comparator("400 Bad Request\n");
    CLBOSStatus lbos_status(true, true);
    /* Currently LBOS does not return any errors */
    /* Here we can try to deannounce something non-existent */
    unsigned short port = kDefaultPort;
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("no such service", "no such version", "127.0.0.1", 
                         port), 
        CLBOSException, comparator);
}


/* 5. Real-life test: after de-announcement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce()
{
    WRITE_LOG("Real-life test : after de-announcement server should "
              "be invisible to resolve");
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    //SleepMilliSec(1500); //ZK is not that fast
    Deannounced__Return1(port);
    //SleepMilliSec(1500); //ZK is not that fast
    int count_after = s_CountServersWithExpectation(node_name, port, 0,
                                                    __LINE__,
                                                    kDiscoveryDelaySec);
    NCBITEST_CHECK_EQUAL_MT_SAFE(count_after - count_before, 0);
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
void NoHostProvided__LocalAddress()
{
    WRITE_LOG("Deannounce without IP specified - deannounce from local host");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    try {
        // We get random answers of LBOS in this test, so try-catch
        // Service always get announced, though
        s_AnnounceCPP(node_name, "1.0.0", "",
                      port, "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health");
    } 
    catch (...) {
    }
    stringstream healthcheck;
    healthcheck << ":" + s_PortStr(PORT_N) +  "/health" << "?port=" << port <<
                   "&host=" << "" << "&version=" << "1.0.0";
    bool is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                           healthcheck.str().c_str(),
                                           true);
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, true);
    BOOST_CHECK_NO_THROW(
        s_DeannounceCPP(node_name, "1.0.0", "", port));
    is_announced = s_CheckIfAnnounced(node_name, "1.0.0", port,
                                      ":" + s_PortStr(PORT_N) +  "/health",
                                      false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(is_announced, false);
}

/* 8. LBOS is OFF - return eLBOSOff                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ThrowKLBOSOff()
{
    WRITE_LOG("Deannonce when LBOS mapper is OFF");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSOff" <<
                "\", status code \"" << 550 <<
                "\", message \"" << "550\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSOff, 550> 
                                                            comparator("550\n");
    CLBOSStatus lbos_status(true, false);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("lbostest", "1.0.0", "", 8080), 
        CLBOSException, comparator);
}


/* 9. Trying to deannounce non-existent service - throw e_LBOSNotFound           */
/*    Test is thread-safe. */
void NotExists__ThrowE_NotFound()
{
    WRITE_LOG("Deannonce non-existent service");
    WRITE_LOG("Expected exception with error code \"" << 
                "e_LBOSNotFound" <<
                "\", status code \"" << 404 <<
                "\", message \"" << "404 Not Found\\n" << "\".");
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSNotFound, 404> 
                                                  comparator("404 Not Found\n");
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        s_DeannounceCPP("notexists", "1.0.0", "", 8080),
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
void AllDeannounced__NoSavedLeft()
{
    WRITE_LOG("DeannonceAll - should deannounce everyrithing "
              "that was announced");
    CLBOSStatus lbos_status(true, true);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port = kDefaultPort;
    WRITE_LOG("Part I. Announcing");
    for (i = 0;  i < 10;  i++) {
        int count_before;
        SELECT_PORT(count_before, node_name, port);
        ports.push_back(port);
        counts_before.push_back(count_before);
        BOOST_CHECK_NO_THROW(
            s_AnnounceCPPSafe(node_name, "1.0.0", "", ports[i],
                              "http://" ANNOUNCEMENT_HOST_0000 ":" + s_PortStr(PORT_N) +  "/health"));
    }
    WRITE_LOG("Part II. DeannounceAll");
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    //SleepMilliSec(10000); //We need LBOS to clear cache

    WRITE_LOG("Part III. Checking discovery - should find nothing");
    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(s_CountServers(s_GenerateNodeName(), ports[i]));
        NCBITEST_CHECK_EQUAL_MT_SAFE(counts_before[i], counts_after[i]);
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
                               "LBOS found when it should not be");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*(g_LBOS_UnitTesting_PowerStatus()) == 0,
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*(g_LBOS_UnitTesting_PowerStatus()) == 1,
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
            "Error: LBOS found when mapper is OFF");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter == NULL,
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
                               "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 1,
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

        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
                                       "LBOS not found when it should be");
        NCBITEST_CHECK_MESSAGE_MT_SAFE(s_CallCounter == 2,
                                       "Fill candidates was not called when "
                                       "it should be");
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
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_data, "1.0.0");

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
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_data, prev_version);

    /* Check version */
    conf_data = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_data, prev_version + ".0");

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
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSNotFound, 404> 
                                                                  comp("404\n");

    /* Set version */
    LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);

    /* Delete version */
    string conf_data = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_data, "1.0.0");

    /* Check version */
    LBOS::ServiceVersionGet(node_name, &exists);
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, false);

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
    string health = string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) + "/health";
    int count_before;
    SELECT_PORT(count_before, node_name, port1);
    s_AnnounceCPPSafe(node_name, "v1", "", port1, health.c_str());

    SELECT_PORT(count_before, node_name, port2);
    s_AnnounceCPPSafe(node_name, "v2", "", port2, health.c_str());

    /* Set first version */
    LBOS::ServiceVersionSet(node_name, "v1");
    unsigned int servers_found =
        s_CountServersWithExpectation(node_name, port1, 1, __LINE__,
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 1U);
    servers_found =
        s_CountServersWithExpectation(node_name, port2, 0, __LINE__, 
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 0U);

    /* Set second version and discover  */
    LBOS::ServiceVersionSet(node_name, "v2");
    servers_found =
        s_CountServersWithExpectation(node_name, port1, 0, __LINE__,
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 0U);
    servers_found =
        s_CountServersWithExpectation(node_name, port2, 1, __LINE__,
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 1U);

    /* Cleanup */
    s_DeannounceCPP(node_name, "v1", "", port1);
    s_DeannounceCPP(node_name, "v2", "", port2);
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
    string health = string("http://") + ANNOUNCEMENT_HOST + ":" + 
                    s_PortStr(PORT_N) + "/health";
    int count_before;
    SELECT_PORT(count_before, node_name, port);
    s_AnnounceCPPSafe(node_name, "1.0.0", "", port, health.c_str());
    unsigned int servers_found =
        s_CountServersWithExpectation(node_name, port, 1, __LINE__,
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 1U);

    /* Delete version and not discover */
    LBOS::ServiceVersionDelete(node_name);
    servers_found =
        s_CountServersWithExpectation(node_name, port, 0, __LINE__,
                                      kDiscoveryDelaySec, "");
    NCBITEST_CHECK_EQUAL_MT_SAFE(servers_found, 0U);

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
    s_DeannounceCPP(node_name, "1.0.0", "", port);
}

/* 6. Set with no service - invalid args */
void SetNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                                  comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionSet("", "1.0.0"),
                          CLBOSException, 
                          comp);
}

/* 7. Get with no service - invalid args */
void GetNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                                  comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionGet(""),
                          CLBOSException, 
                          comp);
}

/* 8. Delete with no service - invalid args */
void DeleteNoService__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                                  comp("452\n");

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
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                                  comp("452\n");

    /* Set version */
    BOOST_CHECK_EXCEPTION(LBOS::ServiceVersionSet(node_name, ""),
                          CLBOSException, comp);

    /* Check empty version */
    string cur_version = LBOS::ServiceVersionGet(node_name);
    NCBITEST_CHECK_EQUAL_MT_SAFE(cur_version, "");

    /* Cleanup */
    LBOS::ServiceVersionDelete(node_name);
}

/* 10. Set with empty version no service - invalid args */
void SetNoServiceEmptyVersion__InvalidArgs()
{
    CLBOSStatus lbos_status(true, true);
    ExceptionComparator<CLBOSException::EErrCode::e_LBOSInvalidArgs, 452> 
                                                                  comp("452\n");

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
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
    conf_version = "1";
    exists = true;

    /* Delete */
    conf_version = LBOS::ServiceVersionDelete(node_name, &exists);
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
    conf_version = "1";
    exists = true;

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, false);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
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
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, true);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
    conf_version = "1";
    exists = false;

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0", &exists);
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, true);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
    conf_version = "1";
    exists = false;

    /* Delete (and also a cleanup) */
    conf_version = LBOS::ServiceVersionDelete(node_name, &exists);
    NCBITEST_CHECK_EQUAL_MT_SAFE(exists, true);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
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
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
    conf_version = "1";

    /* Delete */
    conf_version = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
    conf_version = "1";

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "");
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
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
    conf_version = "1";

    /* Set version */
    conf_version = LBOS::ServiceVersionSet(node_name, "1.0.0");
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
    conf_version = "1";

    /* Delete (and also a cleanup) */
    conf_version = LBOS::ServiceVersionDelete(node_name);
    NCBITEST_CHECK_EQUAL_MT_SAFE(conf_version, "1.0.0");
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
    WRITE_LOG("Stability test 1:  only reset() and get_next(), iterator is "
              "not closed");
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
    CConnNetInfo net_info;
    double elapsed = 0.0;

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
        do {
            info = SERV_GetNextInfoEx(*iter, NULL);
        } while (info != NULL);
        SERV_Reset(*iter);
        if (s_GetTimeOfDay(&stop) != 0)
            memset(&stop, 0, sizeof(stop));
        elapsed = s_TimeDiff(&stop, &start);
    }
}

void FullCycle__ShouldNotCrash()
{
    WRITE_LOG("Stability test 2:  full cycle with close(), open() and "
              "get_next()");
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
    CConnNetInfo net_info;
    for (i = 0;  elapsed < secondsBeforeStop;  ++i) {
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
}
} /* namespace Stability */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Performance
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void FullCycle__ShouldNotCrash();

void FullCycle__ShouldNotCrash()
{
    WRITE_LOG("Performance test:  full cycle with close(), open() and "
                                  "get_next()");
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
    
    CConnNetInfo net_info;
    
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
    WRITE_LOG("Performance test:\n"
              "Iterations:\n"
              "\t   Min: " << min_iters_per_cycle << " iters/sec\n"
              "\t   Max: " << max_iters_per_cycle << " iters/sec\n"
              "\t   Avg: " << static_cast<double>(total_iters)/total_elapsed <<
              " iters/sec\n"
              "Found hosts:\n"
              "\t   Min: " << min_hosts_per_cycle << " hosts/sec\n"
              "\t   Max: " << max_hosts_per_cycle << " hosts/sec\n"
              "\t   Avg: " << static_cast<double>(total_hosts)/total_elapsed <<
              " hosts/sec\n");
}
} /* namespace Performance */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace MultiThreading
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static void s_Stability_GetNextReset_ShouldNotCrash( ) {
    Stability::GetNext_Reset__ShouldNotCrash();
}
static void s_Stability_FullCycle_ShouldNotCrash( ) {
    Stability::FullCycle__ShouldNotCrash();
}
static void s_Performance_FullCycle_ShouldNotCrash( ) {
    Performance::FullCycle__ShouldNotCrash();
}

class CMainLoopThread : public CThread
{
public:
    CMainLoopThread(void (*testFunc)(), int idx) 
        : m_TestFunc(testFunc), m_ThreadIdx(idx)
    {
    }
    ~CMainLoopThread()
    {
    }

private:
    void* Main(void) {
        s_Tls->SetValue(new int, TlsCleanup);
        *s_Tls->GetValue() = m_ThreadIdx;
#ifdef NCBI_MONKEY
        CMonkey::Instance()->RegisterThread(m_ThreadIdx);
#endif /* NCBI_MONKEY */
        m_TestFunc();
        return NULL;
    }
    void (*m_TestFunc)();
    int m_ThreadIdx;
};
CMainLoopThread* thread1;
CMainLoopThread* thread2;
CMainLoopThread* thread3;


void TryMultiThread()
{
#define LIST_OF_FUNCS                                                         \
    X(2, ResetIterator::NoConditions__IterContainsZeroCandidates)             \
    X(3, ResetIterator::MultipleReset__ShouldNotCrash)                        \
    X(4, ResetIterator::Multiple_AfterGetNextInfo__ShouldNotCrash)            \
    X(5, CloseIterator::AfterOpen__ShouldWork)                                \
    X(6, CloseIterator::AfterReset__ShouldWork)                               \
    X(7, CloseIterator::AfterGetNextInfo__ShouldWork)                         \
    X(8, CloseIterator::FullCycle__ShouldWork)                                \
    X(9, ResolveViaLBOS::ServiceExists__ReturnHostIP)                         \
    X(10,ResolveViaLBOS::ServiceDoesNotExist__ReturnNULL)                     \
    X(11,ResolveViaLBOS::NoLBOS__ReturnNULL)                                  \
    X(12,GetLBOSAddress::CustomHostNotProvided__SkipCustomHost)               \
    X(13,GetNextInfo::WrongMapper__ReturnNull)                                \
    X(14,MultiThreading::s_Stability_GetNextReset_ShouldNotCrash)             \
    X(15,MultiThreading::s_Stability_FullCycle_ShouldNotCrash)                \
    X(16,MultiThreading::s_Performance_FullCycle_ShouldNotCrash)              \
    X(18,IPCache::HostSeparate__TryFindReturnsHostkIP)                        \
    X(22,IPCache::ResolveEmpty__Error)                                        \
    X(23,IPCache::Resolve0000__Return0000)                                    \
    X(24,IPCache::DeannounceHost__TryFindDoesNotFind)                         \
    X(25,IPCache::ResolveTwice__SecondTimeNoOp)                               \
    X(26,IPCache::DeleteTwice__SecondTimeNoOp)                                \
    X(27,IPCache::TryFindTwice__SecondTimeNoOp)
    
#define X(num,name) CMainLoopThread* thread##num = new CMainLoopThread(name, num);
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
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    CConnNetInfo net_info;
    
    if (s_GetTimeOfDay(&start) != 0) {
        memset(&start, 0, sizeof(start));
    }
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE_MT_SAFE(*iter != NULL,
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
        NCBITEST_CHECK_MESSAGE_MT_SAFE (n_found && (info = SERV_GetNextInfo(*iter)),
                                         "Service not found after reset");
    }
}


/** Result of parsing of /service.
 * is_enabled tell if only enabled servers must be returned */
static vector<SServer> s_GetAnnouncedServers(bool is_enabled, 
                                             vector<string> to_find)
{
    vector<SServer> nodes;
    CConnNetInfo net_info;
    size_t start = 0, end = 0;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    /*
    * Deannounce all lbostest servers (they are left if previous
    * launch of test crashed)
    */
    CCObjHolder<char> lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
        UrlReadAll(*net_info, (string("http://") + lbos_addr +
        "/lbos/watches?format=text").c_str(), NULL, NULL));
    if (*lbos_output_orig == NULL)
        lbos_output_orig = strdup("");
    string lbos_output = *lbos_output_orig;
    to_find.push_back("/lbostest");
    to_find.push_back("/lbostest1");

    unsigned int i = 0;
    for (i = 0; i < to_find.size(); ++i) {
        WRITE_LOG(Error << "i = " << i);
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
            if (!is_enabled || is_announced == "true") {
                SServer node;
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
    vector<SServer> nodes = s_GetAnnouncedServers(true);
    vector<SServer>::iterator node;
    for (node = nodes.begin(); node != nodes.end(); node++) {
        string host = CSocketAPI::HostPortToString(node->host, 0);
        if (host == s_GetMyIP()) {
            try {
                s_DeannounceCPP(node->service, node->version, host, node->port);
            }
            catch (const CLBOSException&) {
            }
        }
    }
}
#endif /* DEANNOUNCE_ALL_BEFORE_TEST */


/** Find server in %LBOS%/text/service */
static
bool s_CheckIfAnnounced(const string&   service, 
                        const string&   version, 
                        unsigned short  server_port, 
                        const string&   health_suffix,
                        bool            expectedAnnounced,
                        string          expected_host)
{   
    WRITE_LOG("Searching for server " << service << ", port " << server_port <<
             " and version " << version << " (expected that it does" <<
             (expectedAnnounced ? "" : " NOT") << " appear in /text/service");
    int wait_msecs = 500;
    int max_retries = kDiscoveryDelaySec * 1000 / wait_msecs;
    int retries = 0;
    bool announced = !expectedAnnounced;
    expected_host = expected_host == "" 
        ?
      CSocketAPI::HostPortToString(CSocketAPI::GetLocalHostAddress(), 0) 
        :
      CSocketAPI::HostPortToString(CSocketAPI::gethostbyname(expected_host),0);
    MEASURE_TIME_START
        while (announced != expectedAnnounced && retries < max_retries) {
            announced = false;
            if (retries > 0) { /* for the first cycle we do not sleep */
                SleepMilliSec(wait_msecs);
            }
            WRITE_LOG("Searching for server " << service << 
                      ", port " << server_port << 
                      " and version " << version << 
                      " (expected that it does" <<
                      (expectedAnnounced ? "" : " NOT") << 
                      " appear in /text/service " << ". Retry #" << retries);
            vector<SServer> nodes = s_GetAnnouncedServers(true);
            vector<SServer>::iterator node;
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
    WRITE_LOG("Server " << service << " with port " << server_port <<
             " and version " << version << " was" <<
             (announced ? "" : " NOT") << " found int /text/service after " <<
             time_elapsed << " seconds (expected to" <<
             (expectedAnnounced ? "" : " NOT") << " find it).");
    return announced;    
}


/** A simple construction that returns "Thread n: " when n is not -1, 
 *  and returns "" (empty string) when n is -1. */
static string s_PrintThreadNum() {
    stringstream ss;
    CTime cl(CTime::eCurrent, CTime::eLocal);

    ss << cl.AsString("h:m:s.l ");
    int* p_val = s_Tls->GetValue();
    if (p_val != NULL) {
        if (*p_val == kMainThreadNumber ) {
            ss << "Main thread: ";
        } else if (*p_val == kHealthThreadNumber ) {
            ss << "H/check thread: ";
        } else
        ss << "Thread " << *p_val << ": ";
    }
    return ss.str();
}

static void s_PrintAnnouncedServers() {
    CORE_LOCK_READ;
    int count, i;
    stringstream ss;
    struct SLBOS_AnnounceHandle_Tag** arr =
        g_LBOS_UnitTesting_GetAnnouncedServers();

    if (*arr == NULL)
        return;
    count = g_LBOS_UnitTesting_GetAnnouncedServersNum();

    for (i = 0; i < count; i++) {
        ss << i << ". " << 
            "\t" << (*arr)[i].service << "\t" << (*arr)[i].version << 
            "\t" << (*arr)[i].host << ":" << (*arr)[i].port << endl;
    }
    WRITE_LOG("Announced servers list: \n" << ss.str());
    CORE_UNLOCK;
}


/** Get string like "x.x.x" and convert it to SLBOSVersion */
static SLBOSVersion s_ParseVersionString(string version) 
{
    SLBOSVersion version_struct;
    /* Now we parse version into major.minor.patch */
    size_t start, end;
    start = 0;
    end = version.find(".", start);
    if (start == string::npos)
        return {0,0,0};
    version_struct.major = NStr::StringToInt(version.substr(start, end - start));
    start = end + 1; // skip "."
    end = version.find(".", start);
    version_struct.minor = NStr::StringToInt(version.substr(start, end - start));
    start = end + 1; // skip "."
    end = version.find(".", start);
    version_struct.patch = NStr::StringToInt(version.substr(start, end - start));
    return version_struct;
}

/** Get string like "x.x.x, x.x.x, x.x.x,..." and convert it to 
 * vector<SLBOSVersion> */
static vector<SLBOSVersion> s_ParseVersionsString(const string& versions)
{
    vector<SLBOSVersion> versions_arr;
    /* Now we parse version into major.minor.patch */
    size_t start = 0, end = 0;
    // if there is something to parse
    for( ; versions.substr(start).length() > 0  &&  end != string::npos; ) {
        end = versions.find(",", start);
        string version_str = versions.substr(start, end-start);
        versions_arr.push_back(s_ParseVersionString(version_str));
        start = end + 2; //skip ", "
    }
    return versions_arr;
}

/**  Read version of LBOS. 
 *  @note 
 *   Should be run only once during runtime. Multiple runs can cause undefined
 *   memory leaks 
 */
static string s_ReadLBOSVersion()
{
    CConnNetInfo net_info;
    size_t version_start = 0, version_end = 0;
    CCObjHolder<char> lbos_address(g_LBOS_GetLBOSAddress());
    string lbos_addr(lbos_address.Get());
    CCObjHolder<char> lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
        UrlReadAll(*net_info, (string("http://") + lbos_addr +
        "/admin/server_info").c_str(), NULL, NULL));
    if (*lbos_output_orig == NULL) // for string constructor not to throw
        lbos_output_orig = strdup("");
    string lbos_output = *lbos_output_orig;
    WRITE_LOG("/admin/server_info output: \r\n" << lbos_output);

    /* In the output of LBOS we search for "\"version\" : \""  */
    string version_tag = "\"version\" : \"";
    version_start = lbos_output.find(version_tag) + version_tag.length();
    if (version_start == string::npos)
        return "";
    version_end = lbos_output.find("\"", version_start);
    string version = lbos_output.substr(version_start, 
                                        version_end - version_start);
    s_LBOSVersion = s_ParseVersionString(version);
    return version;
}


/** Check if LBOS has version compatible with current test. 
 * @param[in] versions_arr 
 *  Array of versions that goes "from, till, from, till, ...". If array ends 
 *  with "from" element, it means that there is no maximum LBOS version for 
 *  the test. Details: if LBOS version equals one of "from" versions, 
 *  test is enabled, if LBOS version equals one of "till" versions - test is 
 *  disabled. If versions_arr has no elements, test is enabled. Elements MUST 
 *  be ascending, e.g. "1.0.0, 1.0.2, 0.0.1, 0.0.2" is invalid input
 */
bool s_CheckTestVersion(vector<SLBOSVersion> versions_arr)
{
    if (versions_arr.size() == 0)
        return true;
    bool active = false;
    bool from_till = false; /* false - from version, true - till version */
    for (size_t i = 0; i < versions_arr.size(); i++) {
        if (from_till) { /* till version */
            if (versions_arr[i] <= s_LBOSVersion) {
                active = false;
            } else {
                break;
            }
        } else { /* from version */
            if (versions_arr[i] <= s_LBOSVersion) {
                active = true;
            } else {
                break;
            }
        }
        from_till = !from_till;
    }
    return active;
}

#endif /* CONNECT___TEST_NCBI_LBOS__HPP*/
