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
 *   Common functions for lbos mapper tests
 *
 */

/*C++*/
#include <sstream>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_lbos.hpp>
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
#   define NCBITEST_CHECK_MESSAGE(P,M)  NCBI_ALWAYS_ASSERT(P,M)
#   undef  BOOST_CHECK_EXCEPTION
#   define BOOST_CHECK_EXCEPTION(S,E,P) \
     do {                                                                     \
        try {                                                                 \
            S;                                                                \
        }                                                                     \
        catch (const E& ex) {                                                 \
            NCBITEST_CHECK_MESSAGE(                                           \
               P(ex),                                                         \
                "LBOS exception contains wrong error type");                  \
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
#   define NCBITEST_CHECK_EQUAL(S,E) \
     NCBITEST_CHECK_MESSAGE(S == E, #S "is not equal to " #E "as expected")
#else  /* if LBOS_TEST_MT not defined */
    // This header must be included before all Boost.Test headers
#   include <corelib/test_boost.hpp>
#endif /* #ifdef LBOS_TEST_MT */


/*test*/
#include "test_assert.h"


USING_NCBI_SCOPE;


/* First let's declare some functions that will be
 * used in different test suites. It is convenient
 * that their definitions are at the very end, so that
 * test config is as high as possible */
static
void                   s_PrintInfo                   (HOST_INFO);
static
void                   s_TestFindMethod              (ELBOSFindMethod);

/** Return a priori known lbos address */
template <unsigned int lines>
static char*           s_FakeComposeLBOSAddress      (void);
#ifdef NCBI_OS_MSWIN
static int             s_GetTimeOfDay                (struct timeval*);
#else
#   define             s_GetTimeOfDay(tv)             gettimeofday(tv, 0)
#endif
static unsigned short  s_Msb                         (unsigned short);
static const char*     s_OS                          (TNcbiOSType);
static const char*     s_Bits                        (TNcbiCapacity);
/** Count difference between two timestamps, in seconds*/
static double          s_TimeDiff                    (const struct timeval*,
                                                      const struct timeval*);

static string          s_GenerateNodeName            (void);
static unsigned short  s_GeneratePort                (int thread_num);
static const int       kThreadsNum                   = 34;
static bool            s_CheckIfAnnounced         (string         service, 
                                                   string         version,
                                                   unsigned short server_port,
                                                   string health_suffix);
/* Static variables that are used in mock functions.
 * This is not thread-safe! */
static int             s_call_counter                = 0;
/* It is yet impossible on Teamcity, but useful for local tests, where 
   local lbos can be easily run                                              */
static string          s_last_header;


#include "test_ncbi_lbos_mocks.hpp"

class CHealthcheckThread : public CThread
{
public:
    CHealthcheckThread()
    {
    }
    ~CHealthcheckThread()
    {
    }

private:
    void* Main(void) {
        CListeningSocket listening_sock(4096);
        CSocket sock;
        listening_sock.Listen(4096);
        STimeout read_timeout = {3, 0};
        while (true) {
            listening_sock.Accept(sock);
            char buf[4096];
            size_t n_read = 0;
            size_t n_written = 0;
            sock.SetTimeout(eIO_Read, &read_timeout);
            sock.Read(buf, 4096, &n_read);
            const char healthy_answer[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 3\r\n"
                "Content-Type: text/plain;charset=UTF-8\r\n"
                "\r\n"
                "OK\r\n";
            sock.Write(healthy_answer, sizeof(healthy_answer) - 1, 
                       &n_written);
            sock.Wait(eIO_Read, NULL);
            sock.Close();
        }
        return NULL;
    }
};

///////////////////////////////////////////////////////////////////////////////
//////////////               DECLARATIONS            //////////////////////////
///////////////////////////////////////////////////////////////////////////////
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Compose_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. All files OK - return composed lbos address
 * 2. If could not read files, give up this industry and return NULL
 * 3. If could not read files, give up this industry and return NULL         */
void LBOSExists__ShouldReturnLbos();
void RoleFail__ShouldReturnNULL();
void DomainFail__ShouldReturnNULL();
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Reset_iterator
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Should make capacity of elements in data->cand equal zero
 * 2. Should be able to reset iter N times consequently without crash
 * 3. Should be able to "reset iter, then getnextinfo" N times 
 *    consequently without crash                                             */
void NoConditions__IterContainsZeroCandidates();
void MultipleReset__ShouldNotCrash();
void Multiple_AfterGetNextInfo__ShouldNotCrash();
} /* namespace Reset_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Close_iterator
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
} /* namespace Close_iterator */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DTab
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
/* 1. Mix of registry DTab and HTTP Dtab: registry goes first */
{
void DTabRegistryAndHttp__RegistryGoesFirst();
void NonStandardVersion__FoundWithDTab();
}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Resolve_via_LBOS
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if lbos answered "not found"
 * 3. Should return NULL if lbos is not reachable
 * 4. Should be able to support up to M IP:port combinations 
      (not checking for repeats) with storage overhead not more than same 
      as size needed (that is, all space consumed is twice as size needed, 
      used and unused space together)
 * 5. Should be able to skip answer of lbos if it is corrupt or contains 
      not valid data                                                         */
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
/* 1. If specific way to find lbos is specified, try it first. If failed, 
      search lbos's address in default order
 * 2. If custom host is specified as method but is not provided as value, 
      search lbos's address in default order
 * 3. Default order is: search lbos's address first in registry. If
      failed, try 127.0.0.1:8080. If failed, try /etc/ncbi/{role, domain}.   */
void SpecificMethod__FirstInResult();
void CustomHostNotProvided__SkipCustomHost();
void NoConditions__AddressDefOrder();
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_candidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Iterate through received lbos's addresses, if there is no response 
      from current lbos
 * 2. If one lbos works, do not try another lbos
 * 3. If *net_info was provided for Serv_OpenP, the same *net_info should be
      available while getting candidates via lbos to provide DTABs.          */
void LBOSNoResponse__SkipLBOS();
void LBOSResponds__Finish();
void NetInfoProvided__UseNetInfo();
} /* namespace Get_candidates */


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
 * 3. If read from lbos successful, return s_op
 * 4. If read from lbos successful and info pointer != NULL, write 
      first element NULL to info
 * 5. If read from lbos unsuccessful or no such service, return 0            */
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
/* 1. If server exists in lbos, it should be found and s_op should be 
 *    returned by SERV_OpenP()
 * 2. If server does not exist in lbos, it should not be found and 
 *    SERV_OpenP() should return NULL 
 * 3. If most priority lbos can be found, it should be used used for 
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
 *  3. Successfully announced: char* lbos_answer contains answer of lbos
 *  4. Successfully announced: information about announcement is saved to 
 *     hidden lbos mapper's storage
 *  5. Could not find lbos: return NO_LBOS
 *  6. Could not find lbos: char* lbos_answer is set to NULL
 *  7. Could not find lbos: SLBOS_AnnounceHandle deannounce_handle is set to
 *     NULL
 *  8. lbos returned error: return LBOS_ERROR
 *  9. lbos returned error: char* lbos_answer contains answer of lbos
 * 10. lbos returned error: SLBOS_AnnounceHandle deannounce_handle is set to 
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
 * 20. lbos is OFF - return eLBOS_Off                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   
 * 22. Trying to announce server and providing dead healthcheck URL -
 *     return eLBOS_NotFound       
 * 23. Trying to announce server and providing dead healthcheck URL -
 *     server should not be announced                                        */
void AllOK__ReturnSuccess(int thread_num);
void AllOK__DeannounceHandleProvided(int thread_num);
void AllOK__LBOSAnswerProvided(int thread_num);
void AllOK__AnnouncedServerSaved(int thread_num);
void NoLBOS__ReturnNoLBOSAndNotFind(int thread_num);
void NoLBOS__LBOSAnswerNull(int thread_num);
void NoLBOS__DeannounceHandleNull(int thread_num);
void LBOSError__ReturnServerError(int thread_num);
void LBOSError__LBOSAnswerProvided(int thread_num);
void LBOSError__DeannounceHandleNull(int thread_num);
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num);
void AnotherRegion__NoAnnounce(int thread_num);
void AlreadyAnnouncedInAnotherZone__ReturnMultizoneProhibited(int thread_num);
void IncorrectURL__ReturnInvalidArgs(int thread_num);
void IncorrectPort__ReturnInvalidArgs(int thread_num);
void IncorrectVersion__ReturnInvalidArgs(int thread_num);
void IncorrectServiceName__ReturnInvalidArgs(int thread_num);
void RealLife__VisibleAfterAnnounce(int thread_num);
void ResolveLocalIPError__Return_DNS_RESOLVE_ERROR(int thread_num);
void IP0000__ReplaceWithLocalIP(int thread_num);
void LBOSOff__ReturnELBOS_Off(int thread_num);
void LBOSAnnounceCorruptOutput__ReturnServerError(int thread_num);
void HealthcheckDead__ReturnELBOS_NotFound(int thread_num);
void HealthcheckDead__NoAnnouncement(int thread_num);

}

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void TestNullOrEmptyField(const char* field_tested);
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success
    2.  Custom section has nothing in config - return eLBOS_InvalidArgs
    3.  Section empty or NULL (should use default section and return 
        eLBOS_Success)
    4.  Service is empty or NULL - return eLBOS_InvalidArgs
    5.  Version is empty or NULL - return eLBOS_InvalidArgs
    6.  port is empty or NULL - return eLBOS_InvalidArgs
    7.  port is out of range - return eLBOS_InvalidArgs
    8.  port contains letters - return eLBOS_InvalidArgs
    9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs
    10. healthcheck does not start with http:// or https:// - return 
        eLBOS_InvalidArgs                                                    */
void ParamsGood__ReturnSuccess() ;
void CustomSectionNoVars__ReturnInvalidArgs();
void CustomSectionEmptyOrNullAndSectionIsOk__ReturnSuccess();
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
/* 1. Successfully deannounced : return 1
 * 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it
 * 3. Could not connect to provided lbos : fail and return 0
 * 4. Successfully connected to lbos, but deannounce returned error : return 0
 * 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                            
 * 6. Another domain - do not deannounce 
 * 7. Deannounce without IP specified - deannounce from local host 
 * 8. lbos is OFF - return eLBOS_Off                                         */
void Deannounced__Return1(unsigned short port, int thread_num);
void Deannounced__AnnouncedServerRemoved(int thread_num);
void NoLBOS__Return0(int thread_num);
void LBOSExistsDeannounceError__Return0(int thread_num);
void RealLife__InvisibleAfterDeannounce(int thread_num);
void AnotherDomain__DoNothing(int thread_num);
void NoHostProvided__LocalAddress(int thread_num);
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in lbos                              */
void AllDeannounced__NoSavedLeft(int thread_num);
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Initialization
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
   /* 1. Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
    *    initialized should not crash
    * 2. At initialization if no lbos found, mapper must turn OFF
    * 3. At initialization if lbos found, mapper should be ON
    * 4. If lbos has not yet been initialized, it should be initialized
    *    at SERV_LBOS_Open()
    * 5. If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()
    * 6. s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_
    *    Initialize()
    * 7. s_LBOS_InstancesList MUST not be NULL at beginning of
    *    s_LBOS_FillCandidates()
    * 8. s_LBOS_FillCandidates() should switch first and good lbos
    *    addresses, if first is not responding
    */
    /**  Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
     *   initialized should not crash                                        */
    void MultithreadInitialization__ShouldNotCrash();
    /**  At initialization if no lbos found, mapper must turn OFF            */
    void InitializationFail__TurnOff();
    /**  At initialization if lbos found, mapper should be ON                */
    void InitializationSuccess__StayOn();
    /**  If lbos has not yet been initialized, it should be initialized at
     *  SERV_LBOS_Open().                                                    */
    void OpenNotInitialized__ShouldInitialize();
    /**  If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open().        */
    void OpenWhenTurnedOff__ReturnNull();
    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *  s_LBOS_Initialize()                                                  */
    void s_LBOS_Initialize__s_LBOS_InstancesListNotNULL();
    /**  s_LBOS_InstancesList MUST not be NULL at beginning of
     *   s_LBOS_FillCandidates()                                             */
    void s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL();
    /**  s_LBOS_FillCandidates() should switch first and good lbos addresses,
     *   if first is not responding                                          */
    void PrimaryLBOSInactive__SwapAddresses();
} /* namespace LBOSMapperInit */


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
namespace Compose_LBOS_address
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSExists__ShouldReturnLbos()
{
    CLBOSStatus lbos_status(true, true);
#ifndef NCBI_COMPILER_MSVC 
    char* result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(result),
                             "lbos address was not constructed appropriately");
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
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "lbos address "
                           "construction did not fail appropriately "
                           "on empty role");

    /* 2. Garbage role */
    roleFile.open(corruptRoleString.data());
    roleFile << "I play a thousand of roles";
    roleFile.close();
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "lbos address "
                           "construction did not fail appropriately "
                           "on garbage role");

    /* 3. No role file (use set of symbols that certainly
     * cannot constitute a file name)*/
    role_domain.setRole("|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "lbos address "
                           "construction did not fail appropriately "
                           "on bad role file");
/*

    free(*g_LBOS_UnitTesting_CurrentRole());
    *g_LBOS_UnitTesting_CurrentRole() = NULL;
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("/etc/ncbi/role", NULL);*/
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
    ofstream domainFile (corruptDomain);
    domainFile << "";
    domainFile.close();
    CLBOSRoleDomain role_domain;

    /* 1. Empty domain file */
    role_domain.setDomain(corruptDomain);
    CCObjHolder<char> result(g_LBOS_ComposeLBOSAddress());
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "lbos address "
                           "construction did not fail appropriately");

    /* 2. No domain file (use set of symbols that certainly cannot constitute
     * a file name)*/
    role_domain.setDomain("|*%&&*^");
    result = g_LBOS_ComposeLBOSAddress();
    NCBITEST_CHECK_MESSAGE(g_LBOS_StringIsNullOrEmpty(*result), "lbos address "
                           "construction did not fail appropriately "
                           "on bad domain file");

    /* Revert changes */
    /*free(*g_LBOS_UnitTesting_CurrentDomain());
    *g_LBOS_UnitTesting_CurrentDomain() = NULL;
    g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, "/etc/ncbi/domain");*/
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
     * We know that iter is lbos's.
     */
    SERV_Reset(*iter);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "lbos not found when should be");
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
     * We know that iter is lbos's. It must have clear info by
     * implementation before GetNextInfo is called, so we can set
     * source of lbos address now
     */
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "lbos not found when should be");
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
        NCBITEST_CHECK_MESSAGE(data->n_cand == 0,
            "Reset did not set n_cand to 0");
        NCBITEST_CHECK_MESSAGE(data->pos_cand == 0,
            "Reset did not set pos_cand to 0");
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
        NCBITEST_CHECK_MESSAGE(*iter != NULL, "lbos not found when should be");
        return;
    }
    /*
     * We know that iter is lbos's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of lbos address now
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
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CConnNetInfo net_info(service);    
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "lbos not found when it should be");
        return;
    }
    /*
     * We know that iter is lbos's.
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
                               "lbos not found when it should be");
        return;
    }
    /*
     * We know that iter is lbos's.
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
            "lbos not found when it should be");
        return;
    }
    /*
     * We know that iter is lbos's.
     */
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
//     SERV_Close(*iter);
//     return;
}

void FullCycle__ShouldWork()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    SLBOS_Data* data;
    size_t i;
    CConnNetInfo net_info(service);

    /*
     * 1. We close after first GetNextInfo
     */
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "lbos not found when it should be");
        return;
    }
    SERV_GetNextInfo(*iter);
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
//     SERV_Reset(*iter);
//     SERV_Close(*iter);

    /*
     * 2. We close after half hosts checked with GetNextInfo
     */
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    data = static_cast<SLBOS_Data*>(iter->data);
    /*                                                            v half    */
    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand / 2;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                                             " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                                               "than 0 after GetNExtInfo");
//     SERV_Reset(*iter);
//     SERV_Close(*iter);

    /* 3. We close after all hosts checked with GetNextInfo*/
    iter = SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);

    for (i = 0;  i < static_cast<SLBOS_Data*>(iter->data)->n_cand;  ++i) {
        SERV_GetNextInfo(*iter);
    }
    data = static_cast<SLBOS_Data*>(iter->data);
    NCBITEST_CHECK_MESSAGE(data->n_cand > 0, "n_cand should be more than 0"
                           " after GetNExtInfo");
    NCBITEST_CHECK_MESSAGE(data->pos_cand > 0, "pos_cand should be more "
                           "than 0 after GetNExtInfo");
//     SERV_Reset(*iter);
//     SERV_Close(*iter);

    /* Cleanup */
    /*ConnNetInfo_Destroy(*net_info);*/
}
} /* namespace Close_iterator */


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
                              "DTab-local: /lbostest=>/zk#/lbostest/1.0.0");
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    string expected_header = "DTab-local:  /lbostest=>/zk#/lbostest/1.0.0;"
                            "/lbostest=>/zk#/lbostest/1.0.0";
    
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
    const SSERV_Info* info = NULL;
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort(-1);
    /*
     * I. Non-standard version
     */ 
    int count_before =
            s_CountServers(service, port,
                           "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort(-1);
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    }
    LBOS_Announce(service.c_str(), "1.1.0", port,
                  "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                  &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    ConnNetInfo_SetUserHeader(*net_info, 
                              "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "lbos not found when it should be");
        return;
    }
    unsigned int servers_found = 0;
    do {
        info = SERV_GetNextInfoEx(*iter, NULL);
        if (info == NULL)
            break;
        if (info->port == port)
            servers_found++;
    } while (true);
    NCBITEST_CHECK_MESSAGE(servers_found == 1,
                           "Error while searching non-standard server "
                           "version with DTab");
    /* Cleanup */
    LBOS_Deannounce(service.c_str(), "1.1.0", 
                    "lbos.dev.be-md.ncbi.nlm.nih.gov", port);
    lbos_answer = NULL;
    
    /*
     * II. Service with no standard version in ZK config
     */
    service = "/lbostest1";
    count_before = s_CountServers(service, port,
                                  "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    while (count_before != 0) {
        port = s_GeneratePort(-1);
        count_before =
                s_CountServers(service, port,
                               "DTab-local: /lbostest=>/zk#/lbostest/1.1.0");
    }
    LBOS_Announce(service.c_str(), "1.1.0", port,
                  "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                  &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    ConnNetInfo_SetUserHeader(*net_info, 
                              "DTab-local: /lbostest1=>/zk#/lbostest1/1.1.0");
    iter = SERV_OpenP(service.c_str(), fSERV_All,
                   SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                   *net_info, 0/*skip*/, 0/*n_skip*/,
                   0/*external*/, 0/*arg*/, 0/*val*/);
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "lbos not found when it should be");
        return;
    }
    servers_found = 0;
    do {
        info = SERV_GetNextInfoEx(*iter, NULL);
        if (info == NULL)
            break;
        if (info->port == port)
            servers_found++;
    } while (info != NULL);
    NCBITEST_CHECK_MESSAGE(servers_found == 1,
                           "Error while searching non-standard server "
                           "version with DTab");

    /* Cleanup */
    LBOS_Deannounce(service.c_str(), "1.1.0", 
                    "lbos.dev.be-md.ncbi.nlm.nih.gov", port);
    lbos_answer = NULL;
}
}


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
    string service = "/lbos";
    CConnNetInfo net_info(service);
    /*
     * We know that iter is lbos's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
                      g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                       "lbos.dev.be-md.ncbi.nlm.nih.gov:8080",
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
    /*
     * We know that iter is lbos's.
     */
    CCObjArrayHolder<SSERV_Info> hostports(
            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov:8080",
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
     * We know that iter is lbos's.
     */

    CCObjArrayHolder<SSERV_Info> hostports(
                           g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort(
                                                "lbosdevacvancbinlmnih.gov:80",
                                                service.c_str(),
                                                *net_info));
    NCBITEST_CHECK_MESSAGE(hostports.count() == 0, "Mapper should not find lbos, but "
                                   "it somehow found.");
}


void FakeMassiveInput__ShouldProcess()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/service/doesnotexist";    
    unsigned int temp_ip;
    unsigned short temp_port;
    CCounterResetter resetter(s_call_counter);
    CConnNetInfo net_info(service);
    /*
     * We know that iter is lbos's.
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
    s_call_counter = 0;
    CConnNetInfo net_info(service);
    /*
     * We know that iter is lbos's.
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
    CLBOSStatus lbos_status(true, true);
    string custom_lbos = "lbos.custom.host";
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                     custom_lbos.c_str()));
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], custom_lbos.c_str()) == 0,
                           "Custom specified lbos address method error");

    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_Registry, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0],
                                  "lbos.dev.be-md.ncbi.nlm.nih.gov:8080") == 0,
                           "Registry specified lbos address method error");

    addresses = g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_127001, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "127.0.0.1:8080") == 0,
                           "Localhost specified lbos address method error");

#ifndef NCBI_OS_MSWIN
    /* We have to fake last method, because its result is dependent on
     * location */
    CMockFunction<FLBOS_ComposeLBOSAddressMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress,  
        s_FakeComposeLBOSAddress);
    
    addresses =
              g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_EtcNcbiDomain, NULL);
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[0], "lbos.foo") == 0,
                           "/etc/ncbi{role, domain} specified lbos "
                           "address method error");
    NCBITEST_CHECK_MESSAGE(strcmp(addresses[1], "lbos.foo:8080") == 0,
        "/etc/ncbi{role, domain} specified lbos "
        "address method error");
#endif /* #ifndef NCBI_OS_MSWIN */
}

void CustomHostNotProvided__SkipCustomHost()
{
    CLBOSStatus lbos_status(true, true);
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                     NULL));
    /* We check that there are only 2 lbos addresses */
#ifdef NCBI_OS_MSWIN //in this case, no /etc/ncbi{role, domain} hosts
    NCBITEST_CHECK_MESSAGE(addresses.count() == 2, "Custom host not specified, but "
                           "lbos still provides it");
#else
    NCBITEST_CHECK_MESSAGE(addresses.count() == 4, "Custom host not specified, but "
        "lbos still provides it");
#endif
}

void NoConditions__AddressDefOrder()
{
    CMockFunction<FLBOS_ComposeLBOSAddressMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ComposeLBOSAddress,
        s_FakeComposeLBOSAddress);
    CCObjArrayHolder<char> addresses(
                           g_LBOS_GetLBOSAddressesEx(eLBOSFindMethod_CustomHost,
                                                     "lbos.custom.host"));
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

}
} /* namespace Get_LBOS_address */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Get_candidates
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
void LBOSNoResponse__SkipLBOS()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CCounterResetter resetter(s_call_counter);
    CConnNetInfo net_info(service); 
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort,
                            s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                              SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                              *net_info, 0/*skip*/, 0/*n_skip*/,
                              0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                                           "lbos not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov:8080";
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(*iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service
     */
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of dead lbos");
}

void LBOSResponds__Finish()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_call_counter);
    string service = "/lbos";    
    CConnNetInfo net_info(service);
    s_call_counter = 2;
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, 
        s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "lbos not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov:8080";
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(*iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service. We expect only one call, which means that counter
     * should increase by 1
     */
    NCBITEST_CHECK_MESSAGE(s_call_counter == 3,
                           "s_LBOS_FillCandidates: Incorrect "
                           "processing of alive lbos");
}

/*Not thread safe because of s_last_header*/
void NetInfoProvided__UseNetInfo()
{
    CLBOSStatus lbos_status(true, true);
    string service = "/lbos";
    CCounterResetter resetter(s_call_counter);
    CConnNetInfo net_info(service);
    ConnNetInfo_SetUserHeader(*net_info, "My header fq34facsadf");
    
    s_call_counter = 2; // to get desired behavior from s_FakeResolveIPPort
    CMockFunction<FLBOS_ResolveIPPortMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->ResolveIPPort, s_FakeResolveIPPort);

    CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,                      
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      *net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/));
    if (*iter == NULL) {
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
            "lbos not found when it should be");
        return;
    }
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                             "lbos.dev.be-md.ncbi.nlm.nih.gov";
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    SERV_GetNextInfoEx(*iter, &hinfo);

    /* We do not care about results, we care how many IPs algorithm tried
     * to resolve service
     */
    NCBITEST_CHECK_MESSAGE(s_last_header == "My header fq34facsadf\r\n", 
                           "s_LBOS_FillCandidates: Incorrect "
                           "transition of header");
}
} /* namespace Get_candidates */


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace GetNextInfo
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{

void EmptyCands__RunGetCandidates()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_call_counter);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;    
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port;
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
            "lbos not found when it should be");
        return;
    }
    /*ConnNetInfo_Destroy(*net_info);*/
    HOST_INFO hinfo;
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "lbos for candidates");
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
    SERV_Reset(*iter);
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "lbos for candidates");
    NCBITEST_CHECK_MESSAGE(info->port == port && info->host == host,
                           "SERV_GetNextInfoEx: mapper error with "
                           "first returned element");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
//     SERV_Close(*iter);
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//     s_call_counter = 0;
}


void ErrorUpdating__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_call_counter);
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
                           "react correctly to error in lbos" );
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in lbos");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup */
    s_call_counter = 0;

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
                           "react correctly to error in lbos "
                           "(info not NULL)" );
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "SERV_GetNextInfoEx:mapper did not "
                           "react correctly to error in lbos "
                           "(fillCandidates was not called once)");
    NCBITEST_CHECK_MESSAGE(hinfo == NULL,
                           "SERV_GetNextInfoEx: hinfo is not NULL "
                           "(always should be NULL)");

    /* Cleanup*/
    /*ConnNetInfo_Destroy(*net_info);*/
//     SERV_Close(*iter);
//     s_call_counter = 0;
//     g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
}

void HaveCands__ReturnNext()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_call_counter);
    string service = "/lbos";    
    const SSERV_Info* info = NULL;    
    unsigned int host = 0;
    unsigned short port;  
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
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not "
                           "ask lbos for candidates");

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
//     s_call_counter = 0;
}

void LastCandReturned__ReturnNull()
{
    CLBOSStatus lbos_status(true, true);
    CCounterResetter resetter(s_call_counter);
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
    NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
                           "SERV_GetNextInfoEx: mapper did not ask "
                           "lbos for candidates");

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
//     s_call_counter = 0;
}

void DataIsNull__ReconstructData()
{
    string service = "/lbos";
    const SSERV_Info* info = NULL;
    string hostport = "1.2.3.4:210";
    unsigned int host = 0;
    unsigned short port;
    CCounterResetter resetter(s_call_counter);
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
            "lbos not found when it should be");
        return;
    }
    /* Now we destroy data */
    SLBOS_Data* data = static_cast<SLBOS_Data*>(iter->data);
    g_LBOS_UnitTesting_GetLBOSFuncs()->DestroyData(data);
    iter->data = NULL;
    /* Now let's see how the mapper behaves. Let's check the first element */
    info = SERV_GetNextInfoEx(*iter, &hinfo);
    /*Assert*/
    NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
                           "SERV_GetNextInfoEx: mapper did "
                           "not ask lbos for candidates");
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
            "lbos not found when it should be");
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
                                           "lbos not found when it should be");
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
                               "lbos not found when it should be");
        return;
    }

    NCBITEST_CHECK_MESSAGE(iter->op != NULL, 
                           "Mapper returned NULL when it should return s_op");
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned "
                           "answer is not \"lbos\"");
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
                               "lbos not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned answer "
                           "is not \"lbos\"");
    NCBITEST_CHECK_MESSAGE(info == NULL, "lbos mapper provided "
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
            "lbos not found when it should be");
        return;
    }
    NCBITEST_CHECK_MESSAGE(iter->op != NULL,
                           "Mapper returned NULL when it "
                           "should return s_op");
    if (iter->op == NULL) return;
    NCBITEST_CHECK_MESSAGE(strcmp(iter->op->mapper, "lbos") == 0,
                           "Name of mapper that returned "
                           "answer is not \"lbos\"");
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
static int  s_FindAnnouncedServer(string            service,
                                  string            version,
                                  unsigned short    port,
                                  string            host)
{
    CLBOSStatus lbos_status(true, true);
    struct SLBOS_AnnounceHandle_Tag*& arr = 
                                     *g_LBOS_UnitTesting_GetAnnouncedServers();
    unsigned int count = g_LBOS_UnitTesting_GetAnnouncedServersNum();
    unsigned int found = 0;
    /* Just iterate and compare */
    unsigned int i = 0;
    for (i = 0;  i < count; i++) {
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
    return found;
}


/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    /* Cleanup */
    ELBOS_Result deannounce_result = LBOS_Deannounce(node_name.c_str(),
                                            "1.0.0",
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                             port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
    lbos_answer = NULL;

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    result = LBOS_Announce(node_name.c_str(),
                            "1.0.0",
                            port,
                            "http://130.14.25.27:8080/health",
                            &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    /* Cleanup */
    deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                        "1.0.0",
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                        port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
    lbos_answer = NULL;
}


/*  3. Successfully announced : char* lbos_answer contains answer of lbos    */
/* Test is thread-safe. */
void AllOK__LBOSAnswerProvided(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    /* Cleanup */
    LBOS_Deannounce(node_name.c_str(), 
                    "1.0.0",
                    "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                    port);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "lbos answer as expected");
    lbos_answer = NULL;

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://130.14.25.27:8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                           "Announcement function did not return "
                           "lbos answer as expected");
    lbos_answer = NULL;
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    /* Cleanup */
    LBOS_Deannounce(node_name.c_str(), 
                    "1.0.0",
                    "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                    port);
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden lbos mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    result = 
        LBOS_Announce(node_name.c_str(), "1.0.0", port,
                      "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                      &*lbos_answer);
    lbos_answer = NULL;
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(
            node_name.c_str(), "1.0.0", port, 
            "lbos.dev.be-md.ncbi.nlm.nih.gov") != -1,
        "Announced server was not found in storage");
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    /* Cleanup */
    int deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                            "1.0.0",
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                            port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    LBOS_Announce(node_name.c_str(),
                  "1.0.0",
                  port,
                  "http://130.14.25.27:8080/health",
                  &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_FindAnnouncedServer(
                           node_name.c_str(), 
                           "1.0.0", port, "130.14.25.27") != -1,
                           "Announced server was not found in storage");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                          "1.0.0",
                                          "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                          port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
}


/*  5. Could not find lbos: return NO_LBOS                                   */
/* Test is NOT thread-safe. */
void NoLBOS__ReturnNoLBOSAndNotFind(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    ELBOS_Result result;
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CCObjHolder<char> lbos_answer(NULL);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                           ":8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_NoLBOS,
                           "Announcement did not return "
                           "eLBOS_NoLBOS as expected");
}


/*  6. Could not find lbos : char* lbos_answer is set to NULL                */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSAnswerNull(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CCObjHolder<char> lbos_answer(NULL);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                       s_FakeReadEmpty);
    LBOS_Announce(node_name.c_str(),
                  "1.0.0",
                  port,
                  "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                  &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "lbos answer was not NULL as should be in case "
                           "when lbos not found");
}


/*  8. lbos returned error: return eLBOS_ServerError                         */
/* Test is NOT thread-safe. */
void LBOSError__ReturnServerError(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_ServerError, 
                           "Announcement did not return "
                           "eLBOS_DNSResolveError as expected");
}


/*  9. lbos returned error : char* lbos_answer contains answer of lbos       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    LBOS_Announce(node_name.c_str(),
                  "1.0.0",
                  port,
                  "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                  &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(strcmp
                           (*lbos_answer, "Those lbos errors are scaaary") == 0, 
                           "Message from lbos did not coincide with what "
                           "was expected");
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    unsigned int lbos_addr = 0;
    unsigned short lbos_port = 0;
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    const char* convert_result;
    /*
     * First time
     */
    result = 
        LBOS_Announce(node_name.c_str(), "1.0.0", port,
                      "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                      &*lbos_answer);
    if (g_LBOS_StringIsNullOrEmpty(*lbos_answer)) {
        NCBITEST_CHECK_MESSAGE(!g_LBOS_StringIsNullOrEmpty(*lbos_answer),
                                "Did not gert answer after announcement");
        return;
    }
    convert_result = 
        SOCK_StringToHostPort(*lbos_answer, &lbos_addr, &lbos_port);
    NCBITEST_CHECK_MESSAGE(convert_result != NULL,
                             "Host:port conversion unsuccessful");
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    NCBITEST_CHECK_EQUAL(s_FindAnnouncedServer(node_name, "1.0.0", port,
                                     "lbos.dev.be-md.ncbi.nlm.nih.gov"), 1);
    lbos_answer = NULL;
    /*
     * Second time
     */
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    convert_result = SOCK_StringToHostPort(*lbos_answer,
                                           &lbos_addr,
                                           &lbos_port);
    /* Count how many servers there are. Actually, lbos has some lag, so 
     * we wait a bit */
    SleepMilliSec(1500);
    count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(s_FindAnnouncedServer(node_name, "1.0.0", port,
                           "lbos.dev.be-md.ncbi.nlm.nih.gov") == 1,
                           "Wrong number of stored announced servers! "
                           "Should be 1");
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(convert_result != NULL &&
                           convert_result != *lbos_answer,
                           "lbos answer could not be parsed to host:port");
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    /* Cleanup */
     
    int deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                            "1.0.0",
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                            port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
}


/* 12. Trying to announce in another domain - do nothing                     */
/* Test is NOT thread-safe. */
void AnotherRegion__NoAnnounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort(thread_num);
    string node_name = s_GenerateNodeName();
    CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(), 
                           "1.0.0", 
                           port, 
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_NoLBOS,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /* Cleanup */
    /*free(lbos_answer);*/
}


/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that equals NULL
     */
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           NULL,
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /*
     * II. Healthcheck URL that does not start with http or https
     */
    port = s_GeneratePort(thread_num);
    node_name = s_GenerateNodeName();
    lbos_answer = NULL;
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
//     /* Cleanup */
//     free(lbos_answer);
//     free(node_name);
}


/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    /* Count how many servers there are before we announce */
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                             "1.0.0",
                             0,
                             "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                             &*lbos_answer);
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /* Cleanup */
//     free(lbos_answer);
//     free(node_name);
}


/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    ELBOS_Result result;
    /*
     * I. NULL version 
     */
    result = LBOS_Announce(node_name.c_str(),
                           NULL,
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                           ":8080/health",
                           &*lbos_answer);
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /*
     * II. Empty version 
     */
    lbos_answer = NULL;
    node_name = s_GenerateNodeName();
    result = LBOS_Announce(node_name.c_str(),
                             "",
                             port,
                             "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                             &*lbos_answer);
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /* Cleanup */
//     free(lbos_answer);
//     free(node_name);
}


/* 16. Was passed incorrect service name (NULL or empty): do not 
 *     announce and return INVALID_ARGS                                      */
/* Test is thread-safe. */
void IncorrectServiceName__ReturnInvalidArgs(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /*
     * I. NULL service name
     */
    ELBOS_Result result;
    result = LBOS_Announce(NULL,
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
    /*
     * II. Empty service name
     */
    lbos_answer = NULL;
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    result = LBOS_Announce("",
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement result did not match expected "
                           "eLBOS_InvalidArgs");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL,
                           "Announcement did not return NULL lbos "
                           "answer as it should");
}


/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    unsigned short port = s_GeneratePort(thread_num);
    string node_name = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                               "1.0.0",
                               port,
                               "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                                &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    /* Cleanup */
    LBOS_Deannounce(node_name.c_str(), "1.0.0",
                   "lbos.dev.be-md.ncbi.nlm.nih.gov", port);
}


/* 18. If was passed "0.0.0.0" as IP, should replace it with local IP or
 *     hostname                                                              */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithLocalIP(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    CMockFunction<FLBOS_SOCKGetHostByAddrExMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->GetHostByAddr,
                             s_FakeGetHostByAddrEx<true, 1,2,3,4>);
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    ELBOS_Result result = 
        LBOS_Announce(node_name.c_str(), "1.0.0", port, 
                      "http://0.0.0.0:8080/health", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_DNSResolveError, 
                           "Self announcement did not finish with expected "
                           "result eLBOS_DNSResolveError. Maybe mock "
                           "was not called.");
    NCBITEST_CHECK_MESSAGE(
        s_LBOS_hostport == "http%3A%2F%2F1.2.3.4%3A8080%2Fhealth", 
        "0.0.0.0 was not replaced with current machine IP");
    
    mock1 = s_FakeGetHostByAddrEx<true, 251,252,253,147>;
    result = LBOS_Announce(node_name.c_str(), "1.0.0", port, 
                           "http://0.0.0.0:8080/health", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_DNSResolveError, 
                           "Self announcement did not finish with expected "
                           "result eLBOS_DNSResolveError. Maybe mock "
                           "was not called.");
    NCBITEST_CHECK_MESSAGE(s_LBOS_hostport == 
                           "http%3A%2F%2F251.252.253.147%3A8080%2Fhealth", 
                           "0.0.0.0 was not replaced with current machine IP");
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP : do not announce and return DNS_RESOLVE_ERROR                     */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__Return_DNS_RESOLVE_ERROR(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to know IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    CCObjHolder<char> lbos_answer(NULL);
    string node_name     = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_SOCKGetHostByAddrExMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->GetHostByAddr,
                              s_FakeGetHostByAddrEx<false,0,0,0,0>);
    ELBOS_Result result = LBOS_Announce(node_name.c_str(),
                                        "1.0.0",
                                        port,
                                        "http://0.0.0.0:8080/health",
                                        &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_DNSResolveError, 
                           "Announcement did not finish with "
                           "eLBOS_DNSResolveError as expected");
}

/* 20. lbos is OFF - return eLBOS_Off                                        */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnELBOS_Off(int thread_num = -1)
{
    CCObjHolder<char> lbos_answer(NULL);
    CLBOSStatus lbos_status(true, false);
    ELBOS_Result result = LBOS_Announce("lbostest",
                                        "1.0.0",
                                        8080,
                                        "http://0.0.0.0:8080/health",
                                        &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Off, 
                           "Announcement did not finish with "
                           "eLBOS_Off as expected");
    NCBITEST_CHECK_MESSAGE(*lbos_answer == NULL, 
                           "lbos_answer is not NULL when no lbos");
}



/*21. Announced successfully, but LBOS return corrupted answer -
      return SERVER_ERROR                                                    */
/* Test is NOT thread-safe. */
void LBOSAnnounceCorruptOutput__ReturnServerError(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                           "1.0.0",
                           port,
                           "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                ":8080/health",
                           &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_ServerError,
                           "Announcement did not return "
                           "eLBOS_DNSResolveError as expected");
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return eLBOS_NotFound                                                  */
/* Test is thread-safe. */
void HealthcheckDead__ReturnELBOS_NotFound(int thread_num = -1) 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    ELBOS_Result result;
    /*
     * I. Healthcheck is dead completely 
     */
    result = LBOS_Announce(node_name.c_str(), "1.0.0", port, 
                           "http://badhealth.gov", &*lbos_answer);
    NCBITEST_CHECK_EQUAL(result, eLBOS_NotFound);
    /*
     * II. Healthcheck returns 404
     */
    result =                                             //       missing 'h'
        LBOS_Announce(node_name.c_str(), "1.0.0", port,  //            v
                      "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/healt", 
                      &*lbos_answer);
    NCBITEST_CHECK_EQUAL(result, eLBOS_NotFound);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should not be announced                                         */
/* Test is thread-safe. */
void HealthcheckDead__NoAnnouncement(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    LBOS_Announce(node_name.c_str(), "1.0.0", port, 
                  "http://badhealth.gov:4096/health", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(!s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              ":4096/health"), 
                           "Service was announced");
}

} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success                                       */
void ParamsGood__ReturnSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    result = LBOS_AnnounceFromRegistry(NULL, &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
}
/*  2.  Custom section has nothing in config - return eLBOS_InvalidArgs      */
void CustomSectionNoVars__ReturnInvalidArgs() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    result = LBOS_AnnounceFromRegistry("EMPTY_SECTION", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                               "Announcement function did not return "
                               "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
}
/*  3.  Section empty or NULL (should use default section and return 
        eLBOS_Success, if section is Good)                                   */
void CustomSectionEmptyOrNullAndSectionIsOk__ReturnSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    /* 
     * I. NULL section 
     */
    result = LBOS_AnnounceFromRegistry(NULL, &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
    /* 
     * II. Empty section 
     */
    result = LBOS_AnnounceFromRegistry("", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;
}


void TestNullOrEmptyField(const char* field_tested)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    string null_section = "SECTION_WITHOUT_";
    string empty_section = "SECTION_WITH_EMPTY_";
    string field_name = field_tested;
    /* 
     * I. NULL section 
     */
    result = LBOS_AnnounceFromRegistry((null_section + field_name).c_str(),
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                               "Announcement function did not return "
                               "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
    /* 
     * II. Empty section 
     */
    result = LBOS_AnnounceFromRegistry((empty_section + field_name).c_str(), 
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement function did not return "
                           "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;
}

/*  4.  Service is empty or NULL - return eLBOS_InvalidArgs                  */
void ServiceEmptyOrNull__ReturnInvalidArgs()
{
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return eLBOS_InvalidArgs                  */
void VersionEmptyOrNull__ReturnInvalidArgs()
{
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return eLBOS_InvalidArgs                     */
void PortEmptyOrNull__ReturnInvalidArgs()
{
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return eLBOS_InvalidArgs                      */
void PortOutOfRange__ReturnInvalidArgs() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    /*
     * I. port = 0 
     */
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1",
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                               "Announcement function did not return "
                               "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
    /*
     * II. port = 100000 
     */
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2",
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                               "Announcement function did not return "
                               "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
    /*
     * III. port = 65536 
     */
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3",
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                               "Announcement function did not return "
                               "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
}
/*  8.  Port contains letters - return eLBOS_InvalidArgs                     */
void PortContainsLetters__ReturnInvalidArgs() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_CORRUPTED_PORT",
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement function did not return "
                           "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
}
/*  9.  Healthcheck is empty or NULL - return eLBOS_InvalidArgs              */
void HealthchecktEmptyOrNull__ReturnInvalidArgs()
{
    TestNullOrEmptyField("HEALTHCHECK");
}
/*  10. Healthcheck does not start with http:// or https:// - return         
        eLBOS_InvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK",
                                   &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_InvalidArgs,
                           "Announcement function did not return "
                           "eLBOS_InvalidArgs as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;    
}
/*  11. Trying to announce server and providing dead healthcheck URL -
        return eLBOS_NotFound                                                */
void HealthcheckDead__ReturnELBOS_NotFound() 
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    ELBOS_Result result;
    result = LBOS_AnnounceFromRegistry("SECTION_WITH_DEAD_HEALTHCHECK",
                                       &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_NotFound,
                           "Announcement function did not return "
                           "eLBOS_NotFound as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    LBOS_DeannounceAll();
    lbos_answer = NULL;  
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully deannounced: return 1                                     */
/*    Test is thread-safe. */
void Deannounced__Return1(unsigned short port, int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    string node_name   = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    if (port == 0) {
        port = s_GeneratePort(thread_num);
        while (count_before != 0) {
            port = s_GeneratePort(thread_num);
            count_before = s_CountServers(node_name, port);
        }
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    result = LBOS_Announce(node_name.c_str(),
                             "1.0.0",
                             port,
                             "http://lbos.dev.be-md.ncbi.nlm.nih.gov"
                                                                ":8080/health",
                             &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                            "Number of announced servers did not "
                            "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                            "Announcement function did not return "
                            "SUCCESS as expected");
    /* Cleanup */
    LBOS_Deannounce(node_name.c_str(), "1.0.0",
                        "lbos.dev.be-md.ncbi.nlm.nih.gov", port);
    lbos_answer = NULL;
    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    result = LBOS_Announce(node_name.c_str(),
                             "1.0.0",
                             port,
                             "http://130.14.25.27:8080/health",
                             &*lbos_answer);

    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");

    int deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                                "1.0.0", "130.14.25.27", port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannounce did not work as expected");
}


/* 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    CCObjHolder<char> lbos_answer(NULL);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    ELBOS_Result result;
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    result = 
        LBOS_Announce(node_name.c_str(), "1.0.0", port,
                      "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                      &*lbos_answer);
    SleepMilliSec(1500); //ZK is not that fast
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                               "Announcement function did not return "
                               "SUCCESS as expected");
    /* Cleanup */
    int deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                            "1.0.0",
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                             port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(
            node_name.c_str(), "1.0.0", port, 
            "lbos.dev.be-md.ncbi.nlm.nih.gov") == -1,
        "Deannounced server is still found in storage");
    lbos_answer = NULL;

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    result = 
        LBOS_Announce(node_name.c_str(), "1.0.0", port,
                      "http://130.14.25.27:8080/health", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Success,
                           "Announcement function did not return "
                           "SUCCESS as expected");
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(
            node_name.c_str(), "1.0.0", port, "130.14.25.27") != -1,
        "Deannouncement function did not return SUCCESS as expected");
    /* Cleanup */
    deannounce_result = 
        LBOS_Deannounce(node_name.c_str(), "1.0.0",
                        "lbos.dev.be-md.ncbi.nlm.nih.gov", port);
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(
           node_name.c_str(), "1.0.0", port, 
           "lbos.dev.be-md.ncbi.nlm.nih.gov") == -1,
        "Deannounced server is still found in storage");
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "Deannouncement function did not return "
                           "SUCCESS as expected");
}


/* 3. Could not connect to provided lbos : fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    int deannounce_result;
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                        "1.0.0", 
                                        "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                        port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_NoLBOS, 
                           "deannounce_result does not equal zero as expected "
                           "when lbos is not found");
}


/* 4. Successfully connected to lbos, but deannounce returned error: 
 *    return 0                                                               */
/* Test is thread-safe. */
void LBOSExistsDeannounceError__Return0(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Currently lbos does not return any errors */
    /* Here we can try to deannounce something non-existent */
    int deannounce_result;
    unsigned short port = s_GeneratePort(thread_num);
    deannounce_result =  LBOS_Deannounce("no such service", 
                                          "no such version",
                                          "127.0.0.1", 
                                          port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_ServerError,
                           "LBOS_Deannounce did not return 0 as expected on "
                           "lbos error");
}


/* 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    Deannounced__Return1(port);
    SleepMilliSec(1500); //ZK is not that fast
    int count_after  = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 0,
                           "Number of announced servers should not change "
                           "after consecutive service announcement and "
                           "deannouncement");
}


/*6. If trying to deannounce in another domain - do not deannounce */
/* We fake our domain so no address looks like own domain */
/* Test is NOT thread-safe. */
void AnotherDomain__DoNothing(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    int deannounce_result;
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
    deannounce_result = LBOS_Deannounce(node_name.c_str(), 
                                        "1.0.0", 
                                        "lbos.dev.be-md.ncbi."
                                        "nlm.nih.gov", 
                                        port);
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_NoLBOS, 
                           "deannounce_result does not equal zero as expected"
                           "when lbos is not found");
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
void NoHostProvided__LocalAddress(int thread_num = -1)
{

    CLBOSStatus lbos_status(true, true);
    int deannounce_result;
    CCObjHolder<char> lbos_answer(NULL);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced                     */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    LBOS_Announce(node_name.c_str(), "1.0.0", port, 
        "http://0.0.0.0:4096/health", &*lbos_answer);
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              ":4096/health"), 
                           "Service was not announced");
    deannounce_result = 
        LBOS_Deannounce(node_name.c_str(), "1.0.0", NULL, port);
    NCBITEST_CHECK_MESSAGE(!s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                               ":4096/health"), 
                           "Service was not deannounced");
    NCBITEST_CHECK_MESSAGE(deannounce_result == eLBOS_Success,
                           "deannounce_result does not equal eLBOS_Success "
                           "as expected");
}


/* 8. lbos is OFF - return eLBOS_Off                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnELBOS_Off(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, false);
    ELBOS_Result result = LBOS_Deannounce("lbostest",
                                           "1.0.0",
                                           "lbos.dev.be-md.ncbi.nlm.nih.gov",
                                           8080);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_Off,
                           "Dennouncement did not finish with "
                           "eLBOS_Off as expected");
}
/* 9. Trying to deannounce non-existent service - return eLBOS_NotFound      */
/*    Test is thread-safe. */
void NotExists__ReturnELBOS_NotFound(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    ELBOS_Result result = 
        LBOS_Deannounce("notexists", "1.0.0", 
                        "lbos.dev.be-md.ncbi.nlm.nih.gov", 8080);
    NCBITEST_CHECK_MESSAGE(result == eLBOS_NotFound,
        "Dennouncement did not finish with "
        "eLBOS_Off as expected");
}
}
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in lbos                              */
void AllDeannounced__NoSavedLeft(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port;
    for (i = 0;  i < 10;  i++) {
        CCObjHolder<char> lbos_answer(NULL);
        port = s_GeneratePort(thread_num);
        int count_before = s_CountServers(node_name, port);
        while (count_before != 0) {
            port = s_GeneratePort(thread_num);
            count_before = s_CountServers(node_name, port);
        }
        ports.push_back(port);
        counts_before.push_back(count_before);
        LBOS_Announce(node_name.c_str(), "1.0.0", ports[i],
                      "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health",
                      &*lbos_answer);
    }
    LBOS_DeannounceAll();
    SleepMilliSec(10000); //We need lbos to clear cache

    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(s_CountServers(s_GenerateNodeName(), ports[i]));
        NCBITEST_CHECK_MESSAGE(counts_before[i] == counts_after[i], 
                               "Service was not deannounced as supposed");
    }
}
}



// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Announcement_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
static int  s_FindAnnouncedServer(string            service,
                                  string            version,
                                  unsigned short    port,
                                  string            host)
{
    CLBOSStatus lbos_status(true, true);
    struct SLBOS_AnnounceHandle_Tag*& arr = 
                                     *g_LBOS_UnitTesting_GetAnnouncedServers();
    unsigned int count = g_LBOS_UnitTesting_GetAnnouncedServersNum();
    unsigned int found = 0;
    /* Just iterate and compare */
    unsigned int i = 0;
    for (i = 0;  i < count;  i++) {
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
    return found;
}


/*  1. Successfully announced : return SUCCESS                               */
/* Test is thread-safe. */
void AllOK__ReturnSuccess(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string lbos_answer;
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    BOOST_CHECK_NO_THROW(LBOS::Announce(node_name,
                    "1.0.0",
                    port,
                    "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::Deannounce(node_name, 
                                    "1.0.0",
                                    "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                    port));

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    BOOST_CHECK_NO_THROW(LBOS::Announce(node_name,
                                    "1.0.0",
                                    port,
                                    "http://130.14.25.27:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::Deannounce(node_name, 
                                    "1.0.0",
                                    "lbos.dev.be-md.ncbi.nlm.nih.gov", 
                                    port));
}


/*  4. Successfully announced: information about announcement is saved to
 *     hidden lbos mapper's storage                                          */
/* Test is thread-safe. */
void AllOK__AnnouncedServerSaved(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port,
                       "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_FindAnnouncedServer(
                     node_name.c_str(), 
                    "1.0.0", port, "lbos.dev.be-md.ncbi.nlm.nih.gov") != -1,
                           "Announced server was not found in storage");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0",
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port));

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port, 
            "http://130.14.25.27:8080/health"));
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(node_name.c_str(), "1.0.0", 
            port, "130.14.25.27") != -1,
        "Announced server was not found in storage");
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0",
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
}


/*  5. Could not find lbos: return NO_LBOS                                   */
/* Test is NOT thread-safe. */
void NoLBOS__ThrowNoLBOSAndNotFind(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NoLBOS> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/*  6. Could not find lbos : char* lbos_answer is set to NULL                */
/* Test is NOT thread-safe. */
void NoLBOS__LBOSAnswerNull(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NoLBOS> comparator(
                                                        "No LBOS was found\n");
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                       s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, 
        comparator);
}


/*  8. lbos returned error: return eLBOS_ServerError                         */
/* Test is NOT thread-safe. */
void LBOSError__ThrowServerError(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_ServerError> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_FakeReadAnnouncementWithErrorFromLBOS);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/*  9. lbos returned error : char* lbos_answer contains answer of lbos       */
/* Test is NOT thread-safe. */
void LBOSError__LBOSAnswerProvided(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                      g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                                      s_FakeReadAnnouncementWithErrorFromLBOS);
    try {
        LBOS::Announce(node_name,
                  "1.0.0",
                  port,
                  "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health");
    }
    catch(const CLBOSException& ex) {
        NCBITEST_CHECK_MESSAGE(
            ex.GetErrCode() == CLBOSException::e_ServerError,
            "LBOS exception contains wrong error type");
        const char* ex_message =
            strstr(ex.what(), " Error: ") + strlen(" Error: ");
        string message = "";
        message.append(ex_message);
        NCBITEST_CHECK_MESSAGE(
            message == "LBOS returned error: Those lbos errors "
                        "are scaaary\n", 
            "Message from lbos did not coincide with what was expected");
        return;
    } 
    catch(...) {
    }
    NCBITEST_CHECK_MESSAGE(false, "No exception was generated");
}


/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
/* Test is thread-safe. */
void AlreadyAnnouncedInTheSameZone__ReplaceInStorage(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    /*
     * First time
     */
    BOOST_CHECK_NO_THROW(LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    NCBITEST_CHECK_MESSAGE(s_FindAnnouncedServer(node_name, "1.0.0", port,
                           "lbos.dev.be-md.ncbi.nlm.nih.gov") == 1,
                           "Wrong number of stored announced servers! "
                           "Should be 1");
    /*
     * Second time
     */
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    /* Count how many servers there are. Actually, lbos has some lag, so 
     * we wait a bit */
    SleepMilliSec(1500);
    count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(s_FindAnnouncedServer(node_name, "1.0.0", port,
                           "lbos.dev.be-md.ncbi.nlm.nih.gov") == 1,
                           "Wrong number of stored announced servers! "
                           "Should be 1");
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    /* Cleanup */     
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0",
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
}


/* 12. Trying to announce in another domain - do nothing                     */
/* Test is NOT thread-safe. */
void AnotherRegion__NoAnnounce(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NoLBOS> comparator;
    CLBOSStatus lbos_status(true, true);
    unsigned short port = s_GeneratePort(thread_num);
    string node_name = s_GenerateNodeName();
    CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port, 
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/* 13. Was passed incorrect healthcheck URL (empty, not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
/* Test is thread-safe. */
void IncorrectURL__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Count how many servers there are before we announce */
    /*
     * I. Healthcheck URL that does not start with http or https
     */
    port = s_GeneratePort(thread_num);
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port, 
                       "lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
    /*
     * II. Empty healthcheck URL
     */
    port = s_GeneratePort(thread_num);
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port, ""),
        CLBOSException, comparator);
}


/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
/* Test is thread-safe. */
void IncorrectPort__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    /* Count how many servers there are before we announce */
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", 0,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/* 15. Was passed incorrect version(empty) : do not announce and
 *     return INVALID_ARGS                                                   */
/* Test is thread-safe. */
void IncorrectVersion__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    node_name = s_GenerateNodeName();
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/* 16. Was passed incorrect service name (empty): do not 
 *     announce and return INVALID_ARGS                                      */
/* Test is thread-safe. */
void IncorrectServiceName__ThrowInvalidArgs(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /*
     * I. Empty service name
     */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    /* As the call is not supposed to go through mapper to network,
     * we do not need any mocks*/
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce("", "1.0.0", port, 
                       "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
/* Test is thread-safe. */
void RealLife__VisibleAfterAnnounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    unsigned short port = s_GeneratePort(thread_num);
    string node_name = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port, 
                       "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::Deannounce(node_name, "1.0.0",
                   "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
}


/* 18. If was passed "0.0.0.0" as IP, should replace it with local IP or
 *     hostname                                                              */
/* Test is NOT thread-safe. */
void IP0000__ReplaceWithLocalIP(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_DNSResolveError> comparator;
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to specify IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to BOOST_CHECK_NO_THROWbe sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    CMockFunction<FLBOS_SOCKGetHostByAddrExMethod*> mock1(
                             g_LBOS_UnitTesting_GetLBOSFuncs()->GetHostByAddr,
                             s_FakeGetHostByAddrEx<true, 1,2,3,4>);
    CMockFunction<FLBOS_AnnounceMethod*> mock2 (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->AnnounceEx,
                                s_FakeAnnounceEx);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
                       "http://0.0.0.0:8080/health"),
        CLBOSException, comparator);
    NCBITEST_CHECK_MESSAGE(
        s_LBOS_hostport == "http%3A%2F%2F1.2.3.4%3A8080%2Fhealth", 
        "0.0.0.0 was not replaced with current machine IP");    
    mock1 = s_FakeGetHostByAddrEx<true, 251,252,253,147>;
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port, 
                       "http://0.0.0.0:8080/health"), 
        CLBOSException, comparator);
    NCBITEST_CHECK_MESSAGE(
        s_LBOS_hostport == "http%3A%2F%2F251.252.253.147%3A8080%2Fhealth", 
        "0.0.0.0 was not replaced with current machine IP");
}


/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP : do not announce and return DNS_RESOLVE_ERROR                     */
/* Test is NOT thread-safe. */
void ResolveLocalIPError__Throw_DNS_RESOLVE_ERROR(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_DNSResolveError> comparator;
    CLBOSStatus lbos_status(true, true);
    /* Here we mock SOCK_gethostbyaddrEx to know IP address that we want to
     * expect in place of "0.0.0.0"                                          */
    string node_name     = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_SOCKGetHostByAddrExMethod*> mock(
                              g_LBOS_UnitTesting_GetLBOSFuncs()->GetHostByAddr,
                              s_FakeGetHostByAddrEx<false,0,0,0,0>);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port, 
            "http://0.0.0.0:8080/health"),
        CLBOSException, comparator);
}

/* 20. lbos is OFF - return eLBOS_Off                                        */
/* Test is NOT thread-safe. */
void LBOSOff__ThrowELBOS_Off(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_Off> comparator;
    CLBOSStatus lbos_status(true, false);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce("lbostest", "1.0.0", 8080, 
        "http://0.0.0.0:8080/health"),
        CLBOSException, comparator);
}


/*21. Announced successfully, but LBOS return corrupted answer -
      return SERVER_ERROR                                                    */
void LBOSAnnounceCorruptOutput__ThrowServerError(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_ServerError> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                    g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
                    s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS);
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"),
        CLBOSException, comparator);
}


/*22. Trying to announce server and providing dead healthcheck URL - 
      return eLBOS_NotFound                                                  */
void HealthcheckDead__ThrowE_NotFound(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NotFound> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /*
     * I. Healthcheck is dead completely 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://badhealth.gov"),
        CLBOSException, comparator);
    /*
     * II. Healthcheck returns 404
     */
    BOOST_CHECK_EXCEPTION(                        //  missing 'h'
        LBOS::Announce(node_name, "1.0.0", port, //      v
        "http://lbos.dev.be-md.cnbi.nlm.nih.gov:8080/healt"),
        CLBOSException, comparator);
}


/*23. Trying to announce server and providing dead healthcheck URL -
      server should not be announced                                         */
void HealthcheckDead__NoAnnouncement(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_ServerError> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    try {
        LBOS::Announce(node_name, "1.0.0", port,
            "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health");
    }
    catch(...)
    {
    }
    NCBITEST_CHECK_MESSAGE(!s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              ":4096/health"), 
                           "Service was announced");
}
} /* namespace Announcement */

// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace AnnouncementRegistry_CXX /* These tests are NOT for multithreading */
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/*  1.  All parameters good (Default section has all parameters correct in 
        config) - return eLBOS_Success                                       */
void ParamsGood__ReturnSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_NO_THROW(LBOS::AnnounceFromRegistry(string()));
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}
/*  2.  Custom section has nothing in config - return eLBOS_InvalidArgs      */
void CustomSectionNoVars__ThrowInvalidArgs() 
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("NON-EXISTENT_SECTION"), 
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}
/*  3.  Section empty (should use default section and return 
        eLBOS_Success, if section is Good)                                   */
void CustomSectionEmptyOrNullAndSectionIsOk__ThrowSuccess() 
{
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_NO_THROW(LBOS::AnnounceFromRegistry(""));
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}


void TestNullOrEmptyField(const char* field_tested)
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    string null_section = "SECTION_WITHOUT_";
    string empty_section = "SECTION_WITH_EMPTY_";
    string field_name = field_tested;
    /* 
     * I. NULL section 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry((null_section + field_name)),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    /* 
     * II. Empty section 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry((empty_section + field_name)), 
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}

/*  4.  Service is empty or NULL - return eLBOS_InvalidArgs                  */
void ServiceEmptyOrNull__ThrowInvalidArgs()
{
    TestNullOrEmptyField("SERVICE");
}

/*  5.  Version is empty or NULL - return eLBOS_InvalidArgs                  */
void VersionEmptyOrNull__ThrowInvalidArgs()
{
    TestNullOrEmptyField("VERSION");
}

/*  6.  Port is empty or NULL - return eLBOS_InvalidArgs                     */
void PortEmptyOrNull__ThrowInvalidArgs()
{
    TestNullOrEmptyField("PORT");
}

/*  7.  Port is out of range - return eLBOS_InvalidArgs                      */
void PortOutOfRange__ThrowInvalidArgs() 
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    /*
     * I. port = 0 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE1"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    /*
     * II. port = 100000 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE2"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    /*
     * III. port = 65536 
     */
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_PORT_OUT_OF_RANGE3"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}
/*  8.  Port contains letters - return eLBOS_InvalidArgs                     */
void PortContainsLetters__ThrowInvalidArgs() 
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_CORRUPTED_PORT"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}
/*  9.  Healthcheck is empty or NULL - return eLBOS_InvalidArgs              */
void HealthchecktEmptyOrNull__ThrowInvalidArgs()
{
    TestNullOrEmptyField("HEALTHCHECK");
}
/*  10. Healthcheck does not start with http:// or https:// - return         
        eLBOS_InvalidArgs                                                    */ 
void HealthcheckDoesNotStartWithHttp__ThrowInvalidArgs() 
{
    ExceptionComparator<CLBOSException::e_InvalidArgs> comparator;
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_CORRUPTED_HEALTHCHECK"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
}
/*  11. Trying to announce server and providing dead healthcheck URL -
        return eLBOS_NotFound                                                */
void HealthcheckDead__ThrowE_NotFound() 
{
    ExceptionComparator<CLBOSException::e_NotFound> comparator;
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        LBOS::AnnounceFromRegistry("SECTION_WITH_DEAD_HEALTHCHECK"),
        CLBOSException, comparator);
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll()); 
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Deannouncement_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. Successfully deannounced: return 1                                     */
/*    Test is thread-safe. */
void Deannounced__Return1(unsigned short port, int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name   = s_GenerateNodeName();
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    if (port == 0) {
        port = s_GeneratePort(thread_num);
        while (count_before != 0) {
            port = s_GeneratePort(thread_num);
            count_before = s_CountServers(node_name, port);
        }
    }
    SleepMilliSec(1500); //ZK is not that fast
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port,
                       "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    int count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                            "Number of announced servers did not "
                            "increase by 1 after announcement");
    /* Cleanup */
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0",
                         "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    BOOST_CHECK_NO_THROW(LBOS::Announce(node_name,
                                 "1.0.0",
                                 port,
                                 "http://130.14.25.27:8080/health"));

    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    count_after = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 1,
                           "Number of announced servers did not "
                           "increase by 1 after announcement");

    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0", "130.14.25.27", port));
}


/* 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it                                                              */
/* Test is thread-safe. */
void Deannounced__AnnouncedServerRemoved(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    /* We use lbos /health url as healthcheck (yes, it 
     * is hack) */
    BOOST_CHECK_NO_THROW(LBOS::Announce(        
        node_name, "1.0.0", port,
        "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Cleanup */
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0", 
                         "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(node_name.c_str(), 
            "1.0.0", port, "lbos.dev.be-md.ncbi.nlm.nih.gov") == -1,
        "Deannounced server is still found in storage");

    /* Now check with IP instead of host name */
    node_name = s_GenerateNodeName();
    port = s_GeneratePort(thread_num);
    count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    BOOST_CHECK_NO_THROW(
        LBOS::Announce(node_name, "1.0.0", port,
            "http://130.14.25.27:8080/health"));
    SleepMilliSec(1500); //ZK is not that fast
    /* Count how many servers there are */
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(node_name.c_str(), "1.0.0", 
                                               port, "130.14.25.27") != -1,
        "Deannouncement function did not return SUCCESS as expected");
    /* Cleanup */
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0", 
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port));
    NCBITEST_CHECK_MESSAGE(
        g_LBOS_UnitTesting_FindAnnouncedServer(node_name.c_str(), "1.0.0", 
            port, "lbos.dev.be-md.ncbi.nlm.nih.gov") == -1,
        "Deannounced server is still found in storage");
}


/* 3. Could not connect to provided lbos : fail and return 0                 */
/* Test is NOT thread-safe. */
void NoLBOS__Return0(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NoLBOS> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockFunction<FLBOS_ConnReadMethod*> mock(
                                       g_LBOS_UnitTesting_GetLBOSFuncs()->Read, 
                                       s_FakeReadEmpty);
    BOOST_CHECK_EXCEPTION(
        LBOS::Deannounce(node_name, "1.0.0", 
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port),
        CLBOSException, comparator);
}


/* 4. Successfully connected to lbos, but deannounce returned error: 
 *    return 0                                                               */
/* Test is thread-safe. */
void LBOSExistsDeannounceError__Return0(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_ServerError> comparator;
    CLBOSStatus lbos_status(true, true);
    /* Currently lbos does not return any errors */
    /* Here we can try to deannounce something non-existent */
    unsigned short port = s_GeneratePort(thread_num);
    BOOST_CHECK_EXCEPTION(
        LBOS::Deannounce("no such service", "no such version", "127.0.0.1", 
                         port), 
        CLBOSException, comparator);
}


/* 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                             */
/* Test is thread-safe. */
void RealLife__InvisibleAfterDeannounce(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* It is best to take test Deannounced__Return1() and just check number
     * of servers after the test */
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    SleepMilliSec(1500); //ZK is not that fast
    Deannounced__Return1(port);
    SleepMilliSec(1500); //ZK is not that fast
    int count_after  = s_CountServers(node_name, port);
    NCBITEST_CHECK_MESSAGE(count_after - count_before == 0,
                           "Number of announced servers should not change "
                           "after consecutive service announcement and "
                           "deannouncement");
}


/*6. If trying to deannounce in another domain - do not deannounce */
/* We fake our domain so no address looks like own domain */
/* Test is NOT thread-safe. */
void AnotherDomain__DoNothing(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NoLBOS> comparator;
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    CMockString mock(*g_LBOS_UnitTesting_CurrentDomain(), "or-wa");
    BOOST_CHECK_EXCEPTION(
        LBOS::Deannounce(node_name, "1.0.0",
            "lbos.dev.be-md.ncbi.nlm.nih.gov", port),
        CLBOSException, comparator);
}


/* 7. Deannounce without IP specified - deannounce from local host           */
/* Test is NOT thread-safe. */
void NoHostProvided__LocalAddress(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    string node_name = s_GenerateNodeName();
    unsigned short port = s_GeneratePort(thread_num);
    /* Prepare for test. We need to be sure that there is no previously 
     * registered non-deleted service. We count server with chosen port 
     * and check if there is no server already announced */
    int count_before = s_CountServers(node_name, port);
    while (count_before != 0) {
        port = s_GeneratePort(thread_num);
        count_before = s_CountServers(node_name, port);
    }
    try {
        // We get random answers of lbos in this test, so try-catch
        // Service always get announced, though
        LBOS::Announce(node_name, "1.0.0", port, "http://0.0.0.0:4096/health");
    } 
    catch (...) {
    }
    NCBITEST_CHECK_MESSAGE(s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                              ":4096/health"),
                           "Service was not announced");
    BOOST_CHECK_NO_THROW(
        LBOS::Deannounce(node_name, "1.0.0", "", port));
    NCBITEST_CHECK_MESSAGE(!s_CheckIfAnnounced(node_name, "1.0.0", port, 
                                               ":4096/health"),
                           "Service was not deannounced");
}

/* 8. lbos is OFF - return eLBOS_Off                                         */
/* Test is NOT thread-safe. */
void LBOSOff__ReturnELBOS_Off(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_Off> comparator;
    CLBOSStatus lbos_status(true, false);
    BOOST_CHECK_EXCEPTION(
        LBOS::Deannounce("lbostest", "1.0.0", 
                          "lbos.dev.be-md.ncbi.nlm.nih.gov", 8080), 
        CLBOSException, comparator);
}


/* 9. Trying to deannounce non-existent service - throw e_NotFound           */
/*    Test is thread-safe. */
void NotExists__ThrowE_NotFound(int thread_num = -1)
{
    ExceptionComparator<CLBOSException::e_NotFound> comparator;
    CLBOSStatus lbos_status(true, true);
    BOOST_CHECK_EXCEPTION(
        LBOS::Deannounce("notexists", "1.0.0",
        "lbos.dev.be-md.ncbi.nlm.nih.gov", 8080),
        CLBOSException, comparator);
}

}
// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace DeannouncementAll_CXX
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
/* 1. If function was called and no servers were announced after call, no 
      announced servers should be found in lbos                              */
void AllDeannounced__NoSavedLeft(int thread_num = -1)
{
    CLBOSStatus lbos_status(true, true);
    /* First, announce some random servers */
    vector<unsigned short> ports, counts_before, counts_after;
    unsigned int i = 0;
    string node_name = s_GenerateNodeName();
    unsigned short port;
    for (i = 0;  i < 10;  i++) {
        port = s_GeneratePort(thread_num);
        int count_before = s_CountServers(node_name, port);
        while (count_before != 0) {
            port = s_GeneratePort(thread_num);
            count_before = s_CountServers(node_name, port);
        }
        ports.push_back(port);
        counts_before.push_back(count_before);
        BOOST_CHECK_NO_THROW(
            LBOS::Announce(node_name, "1.0.0", ports[i],
                "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080/health"));
    }
    BOOST_CHECK_NO_THROW(LBOS::DeannounceAll());
    SleepMilliSec(10000); //We need lbos to clear cache

    for (i = 0;  i < ports.size();  i++) {
        counts_after.push_back(s_CountServers(s_GenerateNodeName(), ports[i]));
        NCBITEST_CHECK_MESSAGE(counts_before[i] == counts_after[i], 
                               "Service was not deannounced as supposed");
    }
}
}


// /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
namespace Initialization
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
{
    /** Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
     * initialized should not crash                                          */
    void MultithreadInitialization__ShouldNotCrash()
    {
        #ifdef LBOS_TEST_MT
    CLBOSStatus lbos_status(false, false);
            GeneralLBOS::LbosExist__ShouldWork();
        #endif
    }


    /**  At initialization if no lbos found, mapper must be turned OFF       */
    void InitializationFail__TurnOff()
    {
        CLBOSStatus lbos_status(false, true);
        string service = "/lbos";
        CCounterResetter resetter(s_call_counter);

        CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<0>);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter == NULL,
                               "lbos found when it should not be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 0,
                               "lbos has not been shut down as it should be");

        /* Cleanup */
//         g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//         SERV_Close(*iter);
    }


    /**  At initialization if lbos found, mapper should be ON                */
    void InitializationSuccess__StayOn()
    {
        CLBOSStatus lbos_status(false, false);
        string service = "/lbos";
        CCounterResetter resetter(s_call_counter);
        
        CMockFunction<FLBOS_FillCandidatesMethod*> mock(
                            g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates,
                            s_FakeFillCandidates<1>);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "lbos not found when it should be");
        NCBITEST_CHECK_MESSAGE(*(g_LBOS_UnitTesting_PowerStatus()) == 1,
                               "lbos is not turned ON as it should be");

        /* Cleanup */
//         g_LBOS_UnitTesting_GetLBOSFuncs()->FillCandidates = temp_func_pointer;
//         SERV_Close(*iter);
    }


    /** If lbos has not yet been initialized, it should be initialized at
     * SERV_LBOS_Open()                                                      */
    void OpenNotInitialized__ShouldInitialize()
    {
        CMockFunction<FLBOS_InitializeMethod*> mock (
                                g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize,
                                s_FakeInitialize);
        CLBOSStatus lbos_status(false, false);
        string service = "/lbos";
        CCounterResetter resetter(s_call_counter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                                 SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                                 NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                                 0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter == NULL,
            "Error: lbos found when mapper is OFF");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
            "Initialization was not called when it should be");

        /* Cleanup */
//         g_LBOS_UnitTesting_GetLBOSFuncs()->Initialize = temp_func_pointer;
//         SERV_Close(*iter);
    }


    /**  If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()         */
    void OpenWhenTurnedOff__ReturnNull()
    {
        CLBOSStatus lbos_status(true, false);
        string service = "/lbos";
        CCounterResetter resetter(s_call_counter);
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
        CCounterResetter resetter(s_call_counter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));
        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "lbos not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 1,
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
        CCounterResetter resetter(s_call_counter);

        CServIter iter(SERV_OpenP(service.c_str(), fSERV_All,            
                       SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                       NULL/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                       0/*external*/, 0/*arg*/, 0/*val*/));

        NCBITEST_CHECK_MESSAGE(*iter != NULL,
                               "lbos not found when it should be");
        NCBITEST_CHECK_MESSAGE(s_call_counter == 2,
            "Fill candidates was not called when it should be");
    }


    /** Template parameter instance_num
     *   lbos instance with which number (in original order as from
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
        CCounterResetter resetter(s_call_counter);
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

        s_call_counter = 0;

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
                    "priority lbos instance error");
            } else {
                CORE_LOGF(eLOG_Trace, (
                    "Test %d. Expecting `%s', reality '%s'",
                    testnum, addresses_control_list[0].c_str(),
                    lbos_addresses[0]));
                NCBITEST_CHECK_MESSAGE(
                    string(lbos_addresses[0]) == addresses_control_list[0],                    
                    "priority lbos instance error");
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
     * s_LBOS_FillCandidates() should switch first and good lbos addresses,
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
        /* Initialize lbos mapper */
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
    string              service            =  "/lbos";
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
    X(01,Compose_LBOS_address::LBOSExists__ShouldReturnLbos)                  \
    X(04,Reset_iterator::NoConditions__IterContainsZeroCandidates)            \
    X(05,Reset_iterator::MultipleReset__ShouldNotCrash)                       \
    X(06,Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash)           \
    X(07,Close_iterator::AfterOpen__ShouldWork)                               \
    X(08,Close_iterator::AfterReset__ShouldWork)                              \
    X(09,Close_iterator::AfterGetNextInfo__ShouldWork)                        \
    X(10,Close_iterator::FullCycle__ShouldWork)                               \
    X(11,Resolve_via_LBOS::ServiceExists__ReturnHostIP)                       \
    X(12,Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL)                   \
    X(13,Resolve_via_LBOS::NoLBOS__ReturnNULL)                                \
    X(17,Get_LBOS_address::CustomHostNotProvided__SkipCustomHost)             \
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

SleepMilliSec(10000);
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
            "lbos not found when it should be");
        return;
    }
    /*
     * We know that iter is lbos's. It must have clear info by implementation
     * before GetNextInfo is called, so we can set source of lbos address now
     */
    static_cast<SLBOS_Data*>(iter->data)->lbos_addr = 
                                            "lbos.dev.be-md.ncbi.nlm.nih.gov";
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


static
bool s_CheckIfAnnounced(string service, 
                        string version, 
                        unsigned short server_port, 
                        string health_suffix) 
{
    
    CConnNetInfo net_info;
    char server_hostport[1024], health_hostport[1024];
    unsigned int host = SOCK_GetLocalHostAddress(eOn);
    SOCK_HostPortToString(host, server_port, server_hostport, 1024);
    SOCK_gethostbyaddrEx(0, health_hostport, 1023 - 1, eOn);
    CCObjHolder<char> lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
            UrlReadAll(*net_info, "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080"
                                 "/lbos/text/service", NULL));
    if (*lbos_output_orig == NULL) 
        lbos_output_orig = strdup("");
    string lbos_output(*lbos_output_orig);
    stringstream ss;
    ss << service << "\t" << version << "\t" <<  server_hostport <<
          "\thttp://" << 
#ifdef NCBI_COMPILER_MSVC
          _strlwr(health_hostport) 
#else 
          strlwr(health_hostport)
#endif
          << health_suffix << "\ttrue";
    /*ConnNetInfo_Destroy(*net_info);*/
    string to_find = ss.str();
    return (lbos_output.find(to_find) != string::npos);
}



#endif /* CONNECT___TEST_NCBI_LBOS__HPP*/
