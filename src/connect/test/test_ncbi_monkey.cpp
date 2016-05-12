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
 *   Tests for Chaos Monkey. It connects to itself and should experience 
 *   difficulties
 *
 */

#include <ncbi_pch.hpp>    
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_boost.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_monkey.hpp>
#include "../ncbi_monkeyp.hpp"
#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#else
#  include <sys/time.h>
#endif
#include "test_assert.h"  /* This header must go last */

#ifdef NCBI_MONKEY

#define WRITE_LOG(text)                                                       \
{{                                                                            \
    CFastMutexGuard spawn_guard(s_WriteLogLock);                              \
    LOG_POST(Error << s_PrintThreadNum() << "\t" << __FILE__ <<               \
                                            "\t" << __LINE__ <<               \
                                            "\t" <<   text);                  \
}}


USING_NCBI_SCOPE;


/* Mutex for log output */
DEFINE_STATIC_FAST_MUTEX(s_WriteLogLock);
const int kPortsNeeded = 9;
static CSafeStatic<vector<unsigned short>> s_ListeningPorts;
static CRef<CTls<int>> tls(new CTls<int>);
const int              kSingleThreadNumber = -1;
const int              kMainThreadNumber = 99;
const int              kHealthThreadNumber = 100;
static const STimeout  kRWTimeout = { 10, 500000 };

#ifdef NCBI_OS_MSWIN
static
int s_GetTimeOfDay(struct timeval* tv)
{
    FILETIME         systime;
    unsigned __int64 sysusec;

    if (!tv)
        return -1;

    GetSystemTimeAsFileTime(&systime);

    sysusec = systime.dwHighDateTime;
    sysusec <<= 32;
    sysusec |= systime.dwLowDateTime;
    sysusec += 5;
    sysusec /= 10;

    tv->tv_usec = (long)(sysusec % 1000000);
    tv->tv_sec = (long)(sysusec / 1000000 - 11644473600Ui64);

    return 0;
}

#else

#  define s_GetTimeOfDay(tv)  gettimeofday(tv, 0)

#endif

/** A simple construction that returns "Thread n: " when n is not -1,
*  and returns "" (empty string) when n is -1. */
static string s_PrintThreadNum() {
    stringstream ss;
    CTime cl(CTime::eCurrent, CTime::eLocal);

    ss << cl.AsString("h:m:s.l ");
    int* p_val = tls->GetValue();
    if (p_val != NULL) {
        if (*p_val == kMainThreadNumber) {
            ss << "Main thread: ";
        }
        else if (*p_val == kHealthThreadNumber) {
            ss << "H/check thread: ";
        }
        else
            ss << "Thread " << *p_val << ": ";
    }
    return ss.str();
}

/* Trash collector for Thread Local Storage that stores thread number */
void TlsCleanup(int* p_value, void* /* data */)
{
    delete p_value;
}


static CONN s_ConnectURL(SConnNetInfo* net_info, const char* url, 
                         const char* user_data)
{
    THTTP_Flags         flags        = fHTTP_AutoReconnect | fHTTP_Flushable;
    CONN                conn;
    CONNECTOR           connector;

    CORE_LOGF(eLOG_Note, ("Parsing URL \"%s\"", url));
    if (!ConnNetInfo_ParseURL(net_info, url)) {
        CORE_LOG(eLOG_Warning, "Cannot parse URL");
        return NULL;
    }
    CORE_LOGF(eLOG_Note, ("Creating HTTP%s connector",
                          &"S"[net_info->scheme != eURL_Https]));
    if (!(connector = HTTP_CreateConnector(net_info, user_data, flags))) 
    {
        CORE_LOG(eLOG_Warning, "Cannot create HTTP connector");
        return NULL;
    }
    CORE_LOG(eLOG_Note, "Creating connection");
    if (CONN_Create(connector, &conn) != eIO_Success) {
        CORE_LOG(eLOG_Warning, "Cannot create connection");
        return NULL;
    }
    CONN_SetTimeout(conn, eIO_Open,      &kRWTimeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, &kRWTimeout);
    return conn;
}


static unsigned short s_Port(unsigned int i = 0)
{
    if (i >= s_ListeningPorts->size()) {
        WRITE_LOG("Error index in s_PortStr(): " << i);
        i = 0;
    }
    return (*s_ListeningPorts)[i];
}

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


class CResponseThread
#ifdef NCBI_THREADS
    : public CThread
#endif /* NCBI_THREADS */
{
public:
    CResponseThread()
        : m_RunResponse(true)
    {
        m_Busy = false;
        for (unsigned short port = 8080; port < 8110; port++) {
            if (m_ListeningSockets.size() >= kPortsNeeded) break;
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
            throw CMonkeyException(CDiagCompileInfo(), NULL,
                                   CMonkeyException::EErrCode::e_MonkeyUnknown,
                                   "Not enough vacant ports to start listening", 
                                   ncbi::EDiagSev::eDiagSevMin);
        }
    }
    void Stop()
    {
        m_RunResponse = false;
    }
    /* Check if thread has answered all requests */
    bool IsBusy()
    {
        return m_Busy;
    }

    void AnswerResponse()
    {
        WRITE_LOG("AnswerResponse() started, m_ListeningSockets has " <<
                  s_ListeningPorts->size() << " open listening sockets");
        struct timeval accept_time_stop;
        STimeout       rw_timeout     = { 1, 20000 };
        STimeout       accept_timeout = { 0, 20000 };
        STimeout       c_timeout      = { 0, 0 };
        size_t         n_ready        = 0;
        int            iters_passed   = 0;
        int            secs_btw_grbg_cllct  = 5;/* collect garbage every 5s */
        int            iters_btw_grbg_cllct = secs_btw_grbg_cllct * 100000 /
                                              (rw_timeout.sec * 100000 +
                                               rw_timeout.usec);

        if (s_GetTimeOfDay(&accept_time_stop) != 0) {
            memset(&accept_time_stop, 0, sizeof(accept_time_stop));
        }
        auto it = m_ListeningSockets.begin();
        CSocketAPI::Poll(m_ListeningSockets, &rw_timeout, &n_ready);
        for (; it != m_ListeningSockets.end(); it++) {
            if (it->m_REvent != eIO_Open && it->m_REvent != eIO_Close) {
                CSocket*   sock = new CSocket;
                struct timeval      accept_time_start;
                struct timeval      accept_time_stop;
                if (s_GetTimeOfDay(&accept_time_start) != 0) {
                    memset(&accept_time_start, 0, sizeof(accept_time_start));
                }
                double accept_time_elapsed = 0.0;
                double last_accept_time_elapsed = 0.0;
                if (static_cast<CListeningSocket*>(it->m_Pollable)->
                    Accept(*sock, &accept_timeout) != eIO_Success) {
                    if (s_GetTimeOfDay(&accept_time_stop) != 0)
                        memset(&accept_time_stop, 0, sizeof(accept_time_stop));
                    accept_time_elapsed = s_TimeDiff(&accept_time_stop,
                                                     &accept_time_start);
                    last_accept_time_elapsed = s_TimeDiff(&accept_time_stop,
                                                         &m_LastSuccAcceptTime);
                    WRITE_LOG("response vacant after trying accept for " <<
                              accept_time_elapsed << "s, last successful "
                              "accept was " << last_accept_time_elapsed <<
                              "s ago");
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
                if (request.length() > 0) {
                    WRITE_LOG("Get message \"" << request << "\""
                              " after trying accept for " << 
                              accept_time_elapsed << "s, "
                              "last successful accept was " <<
                              last_accept_time_elapsed << "s ago");
                }
                m_LastRequest = request;
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

    string GetLastRequest() {
        string temp = m_LastRequest;
        m_LastRequest = "";
        return temp;
    }

protected:
    // As it is said in ncbithr.hpp, destructor must be protected
    ~CResponseThread()
    {
        WRITE_LOG("~CResponseThread() started");
        for (unsigned int i = 0; i < m_ListeningSockets.size(); ++i) {
            CListeningSocket* l_sock = static_cast<CListeningSocket*>
                (m_ListeningSockets[i].m_Pollable);
            l_sock->Close();
            delete l_sock;
        }
        for (unsigned int i = 0; i < 100 /* random number */; ++i) {
            if (!HasGarbage()) break;
            CollectGarbage();
        }
        m_ListeningSockets.clear();
        WRITE_LOG("~CResponseThread() ended");
    }

private:
    /* Go through sockets in collection and remove closed ones */
    void CollectGarbage()
    {
        WRITE_LOG("CResponseThread::CollectGarbage() started, size of "
            "m_SocketPool is " << m_SocketPool.size());
        size_t   n_ready;
        STimeout rw_timeout = { 1, 20000 };
        CSocketAPI::Poll(m_SocketPool, &rw_timeout, &n_ready);
        /* We check sockets that have some events */
        unsigned int i;

        WRITE_LOG("m_SocketPool has " << m_SocketPool.size() << " sockets");
        for (i = 0; i < m_SocketPool.size(); ++i) {
            if (m_SocketPool[i].m_REvent | eIO_ReadWrite) {
                /* If this socket has some event */
                CSocket* sock =
                    static_cast<CSocket*>(m_SocketPool[i].m_Pollable);
                char buf[4096];
                size_t n_read = 0;
                sock->Read(buf, sizeof(buf), &n_read);
                sock->Close();
                delete sock;
                /* Remove item from vector by swap and pop_back */
                swap(m_SocketPool[i], m_SocketPool.back());
                m_SocketPool.pop_back();
            }
        }
        WRITE_LOG("CResponseThread::CollectGarbage() ended, size of "
            "m_SocketPool is " << m_SocketPool.size());
    }

    bool HasGarbage() {
        return m_SocketPool.size() > 0;
    }

    void* Main(void) {
        tls->SetValue(new int, TlsCleanup);
        *tls->GetValue() = kHealthThreadNumber;
        WRITE_LOG("Response thread started");
        if (s_GetTimeOfDay(&m_LastSuccAcceptTime) != 0) {
            memset(&m_LastSuccAcceptTime, 0, sizeof(m_LastSuccAcceptTime));
        }
        while (m_RunResponse) {
            AnswerResponse();
        }
        return NULL;
    }
    /** Pool of listening sockets */
    vector<CSocketAPI::SPoll> m_ListeningSockets;
    /** Pool of sockets created by accept() */
    vector<CSocketAPI::SPoll> m_SocketPool;
    /** time of last successful accept() */
    struct timeval            m_LastSuccAcceptTime;
    bool m_RunResponse;
    string m_LastRequest;
    bool m_Busy;
    map<unsigned short, short> m_ListenPorts;
};

static CResponseThread* s_ResponseThread;

template<CMonkeyException::EErrCode kErrCode>
class CExceptionComparator
{
public:
    CExceptionComparator(string expected_message = "")
        : m_ExpectedMessage(expected_message)
    {
    }

    bool operator()(const CMonkeyException& ex)
    {
        CMonkeyException::EErrCode err_code = kErrCode; /* for debug */
        if (ex.GetErrCode() != err_code) {
            ERR_POST("Exception has code " << ex.GetErrCode() << " and " <<
                err_code << " is expected");
            return false;
        }
        const char* ex_message = ex.what();
        ex_message = strstr(ex_message, "Error: ") + strlen("Error: ");
        string message;
        message.append(ex_message);
        if (message != m_ExpectedMessage) {
            WRITE_LOG("Exception has message \"" << message <<
                      "\" and \"" << m_ExpectedMessage << "\" is expected");
            return false;
        }
        return true;
    }
private:
    string m_ExpectedMessage;
};

NCBITEST_INIT_TREE()
{}


NCBITEST_INIT_CMDLINE(args)
{
    args->AddOptionalPositional("lbos", "Primary address to LBOS",
                                CArgDescriptions::eString);
}


NCBITEST_AUTO_FINI()
{
#ifdef NCBI_THREADS
    s_ResponseThread->Stop(); // Stop listening on the socket
    s_ResponseThread->Join();
#endif
}

/* We might want to clear ZooKeeper from nodes before running tests.
 * This is generally not good, because if this test application runs
 * on another host at the same moment, it will miss a lot of nodes and
 * tests will fail.
 */

NCBITEST_AUTO_INIT()
{
    tls->SetValue(new int, TlsCleanup);
    *tls->GetValue() = kMainThreadNumber;
#ifdef NCBI_OS_MSWIN
    srand(NULL);
#else
    srand(time(NULL));
#endif
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "lbos mapper Unit Test");
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(&config);
    s_ResponseThread = new CResponseThread;
#ifdef NCBI_THREADS
    s_ResponseThread->Run();
#endif
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Miscellaneous )/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/** Check that no interceptions happened for whatever reason */
static void s_CheckNoInterceptions()
{
    g_Monkey_Foo();
    EIO_Status status = eIO_Timeout;
    SOCK sock;

    g_MonkeyMock_SetInterceptedRecv(false);
    g_MonkeyMock_SetInterceptedSend(false);
    g_MonkeyMock_SetInterceptedConnect(false);
    g_MonkeyMock_SetInterceptedPoll(false);

    /* Cause a chance for Monkey to intercept poll() - if Monkey works */
    /* Doing nothing for this, poll() is part of CResponse::AnswerResponse()  */


    /* Cause a chance for Monkey to intercept connect() - if Monkey works */
    status = SOCK_Create("msdev101", s_Port(0), &kRWTimeout, &sock);
    NCBITEST_CHECK_EQUAL(status, eIO_Success);


    /* Cause a chance for Monkey to intercept send() and recv() - if
    * Monkey works*/
    char* data = "I AM DATA";
    size_t n_written;
    SOCK_Write(sock, data, strlen(data), &n_written,
               EIO_WriteMethod::eIO_WritePersist);

    /* Wait for response thread to receive the message */
    int retries = 0;
    bool message_received = false;
    string last_request;
    while ((last_request = s_ResponseThread->GetLastRequest()) == "" && 
        retries++ < 200) {
        SleepMilliSec(20);
    }
    NCBITEST_REQUIRE_EQUAL(last_request, string(data));

    /* Check that no interception happened */
    NCBITEST_CHECK_EQUAL(g_MonkeyMock_GetInterceptedRecv(),    false);
    NCBITEST_CHECK_EQUAL(g_MonkeyMock_GetInterceptedSend(),    false);
    NCBITEST_CHECK_EQUAL(g_MonkeyMock_GetInterceptedPoll(),    false);
    NCBITEST_CHECK_EQUAL(g_MonkeyMock_GetInterceptedConnect(), false);
}


/* Chaos Monkey not enabled - no calls should be intercepted!
 * Preparation:
 * 1) [CHAOS_MONKEY]enabled set to false
 * 2) [CHAOS_MONKEY_PLAN1] has all rules and they are set to
 *    engage for any connection, so if Monkey works, rules should engage.
 * Test:
 * Chaos Monkey should not engage
*/
BOOST_AUTO_TEST_CASE(ChaosMonkeyDisabled__DoesNotIntercept)
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    config.Set("CHAOS_MONKEY", "enabled", "no");
    CMonkey::Instance()->ReloadConfig();

    s_CheckNoInterceptions();

    /* Cleanup */
    config.Set("CHAOS_MONKEY", "enabled", "yes");
}


/* No Monkey section at all - no calls should be intercepted! 
 * Preparation:
 * 1) Chaos Monkey is set to believe that its section name is [doesnotexist]
 * 2) The config file contains [doesnotexist_PLAN1], but does not contain 
 *    [doesnotexist]. [doesnotexist_PLAN1] has all rules and they are set to 
 *    engage for any connection, so if Monkey works, rules should engage.
 * Test:
 * Chaos Monkey should not engage
 */
BOOST_AUTO_TEST_CASE(NoChaosMonkeySection__DoesNotIntercept)
{
    CMonkey::Instance()->ReloadConfig("doesnotexist");
    s_CheckNoInterceptions();
}


/** Check cases when rules end with semicolon and end without semicolon. 
 * Make both types of rules and run them. Rules should be parsed OK and then 
 * run. We check only final behavior. */
BOOST_AUTO_TEST_CASE(SemicolonLastInLine__Ok)
{
    CMonkey::Instance()->ReloadConfig("SEMICOLON_TEST");

    EIO_Status status = eIO_Timeout;
    SOCK sock;

    g_MonkeyMock_SetInterceptedRecv   (false);
    g_MonkeyMock_SetInterceptedSend   (false);
    g_MonkeyMock_SetInterceptedConnect(false);
    g_MonkeyMock_SetInterceptedPoll   (false);

    /* Cause a chance for Monkey to intercept poll() - if Monkey works */
    /* Doing nothing for this, poll() is part of CResponse::AnswerResponse()  */


    /* Cause a chance for Monkey to intercept connect() - if Monkey works */
    unsigned short connect_retries = 0;
    while (status != eIO_Success && connect_retries < 20) {
        status = SOCK_Create("msdev101", s_Port(0), &kRWTimeout, &sock);
        connect_retries++;
    }
    NCBITEST_CHECK_EQUAL(status, eIO_Success);
    /* we should connect from the 3rd try*/
    NCBITEST_CHECK_EQUAL(connect_retries, 3); 

    /* Cause a chance for Monkey to intercept send() and recv() - if
    *  Monkey works */
    char*   data          = "I AM DATA";
    size_t  n_written;
    /* First time - should be 'MONKEY WRITES_TEXT1' */
    vector<string> results;
    results.push_back("I AM DATA");
    results.push_back("MONKEY_WRITES_TEXT2");
    results.push_back("MONKEY_WRITES_TEXT1");
    results.push_back("MONKEY_READS_TEXT2");
    results.push_back("MONKEY_READS_TEXT1");
    for (unsigned int i = 0; i < results.size(); ++i) {
        SOCK_Write(sock, data, strlen(data), &n_written,
                   EIO_WriteMethod::eIO_WritePersist);
        int  retries          = 0;
        bool message_received = false;
        while (s_ResponseThread->GetLastRequest() == "" && retries++ < 20) {
            SleepSec(1);
        }
        NCBITEST_REQUIRE_EQUAL(s_ResponseThread->GetLastRequest(), 
                               string(data));
    }
}


/** Check that if some parameter occurs twice for a rule then an exception 
 * must be thrown */
BOOST_AUTO_TEST_CASE(DuplicateParam__Throw)
{
    CExceptionComparator<CMonkeyException::e_MonkeyInvalidArgs> comparator(
                    "Parameter \"runs\" appears twice in "
                    "[DUPLPARAM_TEST_WRITE_PLAN1]write");
    BOOST_CHECK_EXCEPTION(
        CMonkey::Instance()->ReloadConfig("DUPLPARAM_TEST_WRITE"), 
        CMonkeyException,
        comparator);

    comparator = CExceptionComparator<CMonkeyException::e_MonkeyInvalidArgs>(
                    "Parameter \"runs\" appears twice in "
                    "[DUPLPARAM_TEST_READ_PLAN1]read");
    BOOST_CHECK_EXCEPTION(
        CMonkey::Instance()->ReloadConfig("DUPLPARAM_TEST_READ"), 
        CMonkeyException,
        comparator);
    
    comparator = CExceptionComparator<CMonkeyException::e_MonkeyInvalidArgs>(
                    "Parameter \"allow\" appears twice in "
                    "[DUPLPARAM_TEST_CONNECT_PLAN1]connect");
    BOOST_CHECK_EXCEPTION(
        CMonkey::Instance()->ReloadConfig("DUPLPARAM_TEST_CONNECT"), 
        CMonkeyException,
        comparator);

    comparator = CExceptionComparator<CMonkeyException::e_MonkeyInvalidArgs>(
                    "Parameter \"ignore\" appears twice in "
                    "[DUPLPARAM_TEST_POLL_PLAN1]poll");
    BOOST_CHECK_EXCEPTION(
        CMonkey::Instance()->ReloadConfig("DUPLPARAM_TEST_POLL"),
        CMonkeyException,
        comparator);
}


/** Check that if probability is set in various valid formats - it should be 
 * parsed.
 * Create 1k different connections and then check ratio of intercepted 
 * connections to all connections
 */
BOOST_AUTO_TEST_CASE(ProbabilityGood__Ok)
{
    vector<double> probabilities;
    probabilities.push_back(0.33);
    probabilities.push_back(0);
    probabilities.push_back(0);
    probabilities.push_back(1);
    probabilities.push_back(0);
    probabilities.push_back(0);
    probabilities.push_back(0.385);
    probabilities.push_back(1);
    for (unsigned int i = 0;  i <= 8;  ++i) {
        stringstream ss;
        unsigned short repeats = 1000;
        unsigned short interceptions = 0;
        SOCK sock;
        EIO_Status status;
        ss << "PROBABILITYVALID" << i;
        BOOST_CHECK_NO_THROW(CMonkey::Instance()->ReloadConfig(ss.str()));

        for (unsigned short k = 0;  k < repeats;  k++) {
            /* Cause a chance for Monkey to intercept connect() - if Monkey works */
            unsigned short connect_retries = 0;
            while (status != eIO_Success && connect_retries < 20) {
                status = SOCK_Create("msdev101", s_Port(0), &kRWTimeout, &sock);
                connect_retries++;
            }
            NCBITEST_REQUIRE_EQUAL(status, eIO_Success);
            char* data = "I AM DATA";
            size_t n_written;
            SOCK_Write(sock, data, strlen(data), &n_written,
                       EIO_WriteMethod::eIO_WritePersist);

            /* Wait for response thread to receive the message */
            int retries = 0;
            bool message_received = false;
            string last_request;
            while ((last_request = s_ResponseThread->GetLastRequest()) == "" &&
                retries++ < 20) {
                SleepSec(1);
            }
            if (last_request != "I AM DATA") {

            }
            NCBITEST_REQUIRE_EQUAL(s_ResponseThread->GetLastRequest(), 
                                   string(data));
        }
        double ratio = (double)interceptions / repeats;
        bool lower_bound = probabilities[i] * 0.9 <= ratio;
        bool upper_bound = probabilities[i] * 1.1 >= ratio;
        stringstream ss1;
        ss1 << "Error with Monkey main probability: ratio is " << ratio 
            << "(" << interceptions << "/" << repeats << ")" 
            << "with expected " << probabilities[i] << " [" << ss.str() << "]";
        NCBITEST_REQUIRE_MESSAGE(lower_bound && upper_bound, ss1.str().c_str());
    }
}


/** Check that if probability is set in various invalid formats - an exception 
 * must be thrown */
BOOST_AUTO_TEST_CASE(ProbabilityInvalid__Throw)
{
    for (unsigned int i = 0;  i <= 8;  ++i) {
        stringstream ss;
        ss << "PROBABILITYINVALID" << i;        
        CExceptionComparator<CMonkeyException::e_MonkeyInvalidArgs> comparator(
                        "Parameter \"runs\" appears twice in "
                        "[DUPLPARAM_TEST_WRITE_PLAN1]write");
        BOOST_CHECK_EXCEPTION(
            CMonkey::Instance()->ReloadConfig("DUPLPARAM_TEST_WRITE"),
            CMonkeyException, 
            comparator);
    }
}




/** Check that 'text' parameter can contain a semicolon and be parsed
* correctly */
BOOST_AUTO_TEST_CASE(TextParamContainsSemicolon__Ok)
{

}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( SelfTest ) /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* We know that response thread listens on a specific port. We connect to it 10 
 * times and should experience both success and failure */
BOOST_AUTO_TEST_CASE(TryWrite__NotWrite)
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    char user_data[1] = { 0 };
    SConnNetInfo* net_info = ConnNetInfo_Create(NULL);
    char* url = "http://127.0.0.1:8085/hi";
    CONN conn = s_ConnectURL(net_info, url, user_data);
    SOCK sock;
    EIO_Status status = eIO_Timeout;
    unsigned short retries = 0;
    while (status != eIO_Success) {
        status = SOCK_Create("msdev101", 8085, &kRWTimeout, &sock);
        retries++;
    }
    WRITE_LOG("Connected from " << retries << " tries.");
    status = SOCK_Status(sock, EIO_Event::eIO_Open);
    char* data = "I AM DATA";
    size_t n_written;
    SOCK_Write(sock, data, strlen(data), &n_written, 
               EIO_WriteMethod::eIO_WritePersist);

    SOCK_Write(sock, data, strlen(data), &n_written,
        EIO_WriteMethod::eIO_WritePersist);

    SOCK_Write(sock, data, strlen(data), &n_written,
        EIO_WriteMethod::eIO_WritePersist);
    SleepSec(2);
}
BOOST_AUTO_TEST_SUITE_END()



#endif /* NCBI_MONKEY */