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
*   Chaos Monkey - a library that is hooked with ncbi_socket.c and introduces 
*   problems in network connectivity - losses, bad data, delays.
*   difficulties
*
*/

#include <ncbi_pch.hpp>

#include "ncbi_priv.h"

#ifdef NCBI_MONKEY
#   include <connect/ncbi_connutil.h>
#   include <corelib/ncbi_system.hpp>
#   include <corelib/ncbireg.hpp>
#   include <corelib/ncbithr.hpp>
#   include <corelib/env_reg.hpp>
#   include <corelib/ncbiapp.hpp>
#   include <corelib/ncbimisc.hpp>
#   include "ncbi_monkeyp.hpp"
#   include <list>
#include <vector>

/* OS-dependent way to set socket errors */
#   ifdef NCBI_OS_MSWIN
#       define MONKEY_SET_SOCKET_ERROR(error) WSASetLastError(error)
#       define MONKEY_ENOPROTOOPT WSAENOPROTOOPT 
#   else
#       define MONKEY_SET_SOCKET_ERROR(error) errno = error
#       define MONKEY_ENOPROTOOPT ENOPROTOOPT 
#   endif /* NCBI_OS_MSWIN */

/* Include OS-specific headers */
#   ifdef NCBI_OS_MSWIN
#       include <regex>
#   else
#       include <arpa/inet.h>
#       include <sys/types.h>
#       include <sys/socket.h>
#       include <sys/un.h>
#   endif /* NCBI_OS_MSWIN */


BEGIN_NCBI_SCOPE

DEFINE_STATIC_FAST_MUTEX(s_ConfigMutex);
DEFINE_STATIC_FAST_MUTEX(s_NetworkDataCacheMutex);
DEFINE_STATIC_FAST_MUTEX(s_SeedLogConfigMutex);
DEFINE_STATIC_FAST_MUTEX(s_SingletonMutex);
DEFINE_STATIC_FAST_MUTEX(s_KnownConnMutex);
static CSocket*          s_TimeoutingSock = NULL;
static CSocket*          s_PeerSock = NULL;
const  int               kRandCount = 100;
/* Registry names */
const string             kMonkeyMainSect = "CHAOS_MONKEY";
const string             kEnablField     = "enabled";
const string             kSeedField      = "seed";

#define PARAM_TWICE_EXCEPTION(param)                                        \
    throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),            \
                           NULL, CMonkeyException::e_MonkeyInvalidArgs,     \
                           string("Parameter \"" param "\" is set "         \
                           "twice in [") + GetSection() + "]" +             \
                           s_RuleTypeString(GetActionType()));


/*/////////////////////////////////////////////////////////////////////////////
//                              MOCK DEFINITIONS                             //
/////////////////////////////////////////////////////////////////////////////*/
#   ifdef NCBI_MONKEY_TESTS
#       undef DECLARE_MONKEY_MOCK
/* Declare a static variable and global getter&setter for it */
#       define DECLARE_MONKEY_MOCK(ty,name,def_val)                            \
            static ty s_Monkey_ ## name = def_val;                             \
            ty g_MonkeyMock_Get ## name() {                                    \
                return s_Monkey_ ## name;                                      \
            }                                                                  \
            void g_MonkeyMock_Set ## name(const ty& val) {                     \
                s_Monkey_ ## name = val;                                       \
            }

/* This macro contains the list of variables to be mocked. Needed mocks will
 * be created with DECLARE_MONKEY_MOCK
 */
MONKEY_MOCK_MACRO()

string CMonkeyMocks::MainMonkeySection = "CHAOS_MONKEY";


void CMonkeyMocks::Reset()
{
    MainMonkeySection = "CHAOS_MONKEY";
}

string CMonkeyMocks::GetMainMonkeySection()
{
    return MainMonkeySection;
}
void CMonkeyMocks::SetMainMonkeySection(string section)
{
    MainMonkeySection = section;
}


/*//////////////////////////////////////////////////////////////////////////////
//                            CMonkeyException                                //
//////////////////////////////////////////////////////////////////////////////*/
const char* CMonkeyException::what() const throw()
{

    return CException::GetMsg().c_str();
}


#   endif /* NCBI_MONKEY_TESTS */
/*/////////////////////////////////////////////////////////////////////////////
//                    STATIC CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
static vector<string>& s_Monkey_Split(const string   &s, 
                                      char           delim,
                                      vector<string> &elems) 
{
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


/* Trash collector for Thread Local Storage */
template<class T>
static void s_TlsCleanup(T* p_value, void* /* data */)
{
    delete p_value;
}


static void s_TimeoutingSocketInit(void)
{
    unsigned short i = 8080;
    CListeningSocket* server_socket;

    for (;  i < 8100;  i++) {
        /* Initialize a timeouting socket */
        try {
            server_socket = new CListeningSocket(i);
        } catch (CException) {
            continue;
        }
        STimeout         accept_timeout = { 0, 20000 };
        STimeout         connect_timeout = { 1, 20000 };
        if (server_socket->GetStatus() != eIO_Success) {
            server_socket->Close();
            delete server_socket;
            continue;
        }
        try {
            s_TimeoutingSock = new CSocket(CSocketAPI::gethostbyaddr(0), i,
                                           &connect_timeout);
        } catch (CException) {
            continue;
        }
        s_PeerSock       = new CSocket;
        if (server_socket->Accept(*s_PeerSock, &accept_timeout) == eIO_Success) {
            server_socket->Close();
            delete server_socket;
            char buf[1024];
            size_t n_read;
            s_TimeoutingSock->SetTimeout(eIO_Read, &accept_timeout);
            s_TimeoutingSock->SetTimeout(eIO_Write, &accept_timeout);
            s_TimeoutingSock->Read((void*)buf, 1024, &n_read);
            return;
        } else {
            s_TimeoutingSock->Close();
            delete s_TimeoutingSock;
            s_PeerSock->Close();
            delete s_PeerSock;
            server_socket->Close();
            delete server_socket;
            continue;
        }
        /* If we got here, everything works fine */
    }
    throw CMonkeyException(CDiagCompileInfo(), NULL,
                            CMonkeyException::e_MonkeyUnknown,
                            "Could not create a peer socket for the "
                            "timeouting socket. Tried ports 8080-8100",
                            ncbi::EDiagSev::eDiagSevMin);
}


static void s_TimeoutingSocketDestroy(void)
{
    if (s_TimeoutingSock != NULL) {
        s_TimeoutingSock->Close();
        delete s_TimeoutingSock;
        s_TimeoutingSock = NULL;
    }
    if (s_PeerSock != NULL) {
        s_PeerSock->Close();
        delete s_PeerSock;
        s_PeerSock = NULL;
    }
}


static string s_GetMonkeySection()
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    return config.Get(CMonkeyMocks::GetMainMonkeySection(), "config");
}


static vector<string> s_Monkey_Split(const string &s, char delim) {
    vector<string> elems;
    s_Monkey_Split(s, delim, elems);
    return elems;
}


static void s_MONKEY_GenRandomString(char *s, const size_t len) {
    static const char alphabet[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        " !@#$%^&*()-=_+/,.{}[];':`~<>"
        "\\\n\"";

    for (size_t i = 0; i < len - 1; ++i) {
        s[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
    }

    s[len-1] = 0;
}


void CMonkey::x_GetSocketDestinations(MONKEY_SOCKTYPE sock,
                                             string*         fqdn,
                                             string*         IP,
                                             unsigned short* my_port,
                                             unsigned short* peer_port)
{
    struct sockaddr_in sock_addr;
#ifdef NCBI_OS_MSWIN
    int len_inet = sizeof(sock_addr);
#else
    socklen_t len_inet = sizeof(sock_addr);
#endif
    if (my_port != NULL) {
        getsockname(sock, (struct sockaddr*)&sock_addr, &len_inet);
        *my_port = ntohs(sock_addr.sin_port);
    }
    /* If we need peer information for at least one value */
    if (peer_port || fqdn || IP)
        getpeername(sock, (struct sockaddr*)&sock_addr, &len_inet);
    if (peer_port != NULL) 
        *peer_port = ntohs(sock_addr.sin_port);
#ifndef NCBI_OS_MSWIN
    uint32_t addr = sock_addr.sin_addr.s_addr;
#else
    u_long addr = sock_addr.sin_addr.S_un.S_addr;
#endif
    SFqdnIp&& net_data = x_GetFqdnIp(addr);
    if (fqdn != NULL)
        *fqdn = net_data.fqdn;
    if (IP != NULL)
        *IP = net_data.ip;
}


#if 0
#   ifdef NCBI_OS_MSWIN
static
int s_MONKEY_GetTimeOfDay(struct timeval* tv)
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

#   else

#       define s_MONKEY_GetTimeOfDay(tv)  gettimeofday(tv, 0)

#   endif

static
double s_MONKEY_TimeDiff(const struct timeval* end,
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
#endif


/*//////////////////////////////////////////////////////////////////////////////
//                             CMonkeySeedKey                                 //
//////////////////////////////////////////////////////////////////////////////*/
const CMonkeySeedKey& CMonkeySeedAccessor::Key()
{
    static CMonkeySeedKey key;
    return key;
}


/*//////////////////////////////////////////////////////////////////////////////
//                             CMonkeyRuleBase                                //
//////////////////////////////////////////////////////////////////////////////*/
static string s_RuleTypeString(EMonkeyActionType type)
{
    switch (type) {
    case eMonkey_Connect:
        return "connect";
    case eMonkey_Poll:
        return "poll";
    case eMonkey_Recv:
        return "read";
    case eMonkey_Send:
        return "write";
    default:
        throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                               NULL, CMonkeyException::e_MonkeyInvalidArgs,
                               string("Unknown EMonkeyActionType value"));
    }
}


static string s_SocketCallString(EMonkeyActionType action) 
{
    switch (action) {
    case eMonkey_Recv:
        return "recv()";
    case eMonkey_Send:
        return "send()";
    case eMonkey_Poll:
        return "poll()";
    case eMonkey_Connect:
        return "connect()";
    default:
        throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                               NULL, CMonkeyException::e_MonkeyInvalidArgs,
                               string("Unknown EMonkeyActionType value"));
    }
}


static string s_EIOStatusString(EIO_Status status)
{
    switch (status) {
    case eIO_Timeout:
        return "eIO_Timeout";
    case eIO_Closed:
        return "eIO_Closed";
    case eIO_Unknown:
        return "eIO_Unknown";
    case eIO_Interrupt:
        return "eIO_Interrupt";
    default:
        break;
    }
}


CMonkeyRuleBase::CMonkeyRuleBase(EMonkeyActionType     action_type,
                                 string                section,
                                 const vector<string>& params)
    : m_ReturnStatus(-1), m_RepeatType(eMonkey_RepeatNone), m_Delay (0),
      m_RunsSize(0), m_ActionType(action_type), m_Section(section)
{
    /** If there are no-interception runs before repeating the cycle,
    * we know that from m_RunsSize */
    for (unsigned int i = 0; i < params.size(); i++) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name = name_value[0];
        string value = name_value[1];
        if (name == "return_status") {
            x_ReadEIOStatus(value);
        } else if (name == "runs") {
            x_ReadRuns(value);
        } else if (name == "delay") {
            m_Delay = NStr::StringToULong(value);
        }
    }
}


void CMonkeyRuleBase::AddSocket(MONKEY_SOCKTYPE sock)
{
    /* Element has to exist */
    m_RunPos[sock];
}

/** Check that the rule should trigger on this run */
bool CMonkeyRuleBase::CheckRun(MONKEY_SOCKTYPE sock,
                               unsigned short  rule_probability,
                               unsigned short  probability_left) const
{
    bool isRun = false;
    
    int rand_val = CMonkey::Instance()->GetRand(Key()) % 100;
    isRun = (rand_val) < rule_probability * 100 / probability_left;
    LOG_POST(Note << "[CMonkeyRuleBase::CheckRun]  Checking if the rule " 
        << "for " << s_SocketCallString(m_ActionType)
                  << " will be run this time. Random value is " 
                  << rand_val << ", probability threshold is "
                  << rule_probability * 100 / probability_left);
    LOG_POST(Note << "[CMonkeyRuleBase::CheckRun]  The rule will be " 
                  << (isRun ? "" : "NOT ") << "run");
    return isRun;
}


unsigned short CMonkeyRuleBase::GetProbability(MONKEY_SOCKTYPE sock) const
{
    if (m_RunPos.find(sock) == m_RunPos.end()) {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::e_MonkeyInvalidArgs,
            "The socket provided has not been registered with current rule");
    }
    /* If 'runs' not set, the rule always engages */
    if (m_RunsSize == 0) {
        return 100;
    }
    if (m_RunMode == eMonkey_RunProbability) {
        return static_cast<unsigned short>(m_Runs.at(m_RunPos.at(sock)) * 100);
    } else {
        /* If current run is more than the maximum run in the rule, we have to 
         * start over, or stop*/
        if ((double)(m_RunPos.at(sock) + 1) > m_Runs.back()) {
            switch (m_RepeatType) {
            case eMonkey_RepeatNone:
                return 0;
            case eMonkey_RepeatLast:
                return 100;
            }
        }
        /* If m_Runs has the exact number of current run, the rule triggers */
        return std::binary_search(m_Runs.begin(),
                                  m_Runs.end(),
                                  m_RunPos.at(sock) + 1) ? 100 : 0;
    }
    /* If "runs" is not set, rule always triggers */
    if (m_Runs.empty()) return 100;

}


void CMonkeyRuleBase::x_ReadEIOStatus(const string& eIOStatus_in)
{
    /* Check that 'runs' has not been set before */
    if (m_ReturnStatus != -1) {
        PARAM_TWICE_EXCEPTION("return_status");
    }
    string eIOStatus = eIOStatus_in;
    NStr::ToLower(eIOStatus);
    if (eIOStatus == "eio_closed") {
        m_ReturnStatus = eIO_Closed;
    } else if (eIOStatus == "eio_invalidarg") {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::e_MonkeyInvalidArgs,
            string("Unsupported 'return_status': ") + eIOStatus_in);
    } else if (eIOStatus == "eio_interrupt") {
        m_ReturnStatus = eIO_Interrupt;
    } else if (eIOStatus == "eio_success") {
        m_ReturnStatus = eIO_Success;
    } else if (eIOStatus == "eio_timeout") {
        m_ReturnStatus = eIO_Timeout;
    } else if (eIOStatus == "eio_unknown") {
        m_ReturnStatus = eIO_Unknown;
    } else if (eIOStatus == "eio_notsupported") {
        m_ReturnStatus = eIO_NotSupported;
    } else {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::e_MonkeyInvalidArgs,
            string("Could not parse 'return_status': ") + eIOStatus_in);
    }
}


void CMonkeyRuleBase::x_ReadRuns(const string& runs)
{
    /* Check that 'runs' has not been set before */
    if (m_RunsSize != 0) {
        PARAM_TWICE_EXCEPTION("runs");
    }
    /* We get the string already without whitespaces and only have to
       split it on commas*/
    vector<string> runs_list = s_Monkey_Split(runs, ',');
    if (runs_list.size() == 0)
        throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                               NULL, CMonkeyException::e_MonkeyInvalidArgs,
                               string("Parameter \"runs\" is empty in [")
                               + m_Section + "]" +
                               s_RuleTypeString(m_ActionType));
    m_RunMode = runs_list[0][runs_list[0].length() - 1] == '%'
                ? eMonkey_RunProbability : eMonkey_RunNumber;

    if (runs_list.size() == 1 && m_RunMode == eMonkey_RunProbability)
        runs_list.push_back("...");
    m_RepeatType = eMonkey_RepeatNone;
    if ( *runs_list.rbegin() == "repeat" ) {
        m_RepeatType = eMonkey_RepeatAgain;
    } else if ( *runs_list.rbegin() == "..." ) {
        m_RepeatType = eMonkey_RepeatLast;
    }

    ERunFormat run_format = runs_list[0].find_first_of(':') != string::npos ? 
                                                          eMonkey_RunRanges : 
                                                          eMonkey_RunSequence;

    unsigned int end_pos = (m_RepeatType == eMonkey_RepeatNone)
                           ? runs_list.size() : runs_list.size() - 1;

    for ( unsigned int i = 0;  i < end_pos;  i++ ) {
        string& run = runs_list[i];
        double prob;
        if (m_RunMode == eMonkey_RunProbability) {
            if (*run.rbegin() != '%') {
                throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::e_MonkeyInvalidArgs,
                        "Value is not percentage: " + run +
                        ", values have to be either only percentages or "
                               "only numbers of tries, not mixed");
            }
            switch (run_format) {
            default:
                assert(0);
                /* In release build - fall through to eMonkey_RunSequence */
            case eMonkey_RunSequence:
                prob = NStr::StringToDouble(run.substr(0, run.length() - 1));
                m_Runs.push_back(prob / 100);
                break;
            case eMonkey_RunRanges:
                size_t prob_start = run.find_first_of(':');
                size_t step = NStr::StringToSizet(run.substr(0, prob_start));
                size_t last_step = m_Runs.size();
                if (last_step == 0 && step != 1) {
                    throw CMonkeyException(
                            CDiagCompileInfo(__FILE__, __LINE__),
                            NULL, CMonkeyException::e_MonkeyInvalidArgs,
                            "In the string of runs: " + runs + " the first "
                            "element MUST set value for the first run");
                }
                prob = NStr::StringToDouble(run.substr(prob_start + 1,
                                            run.length() - prob_start - 2));
                for (size_t j = last_step+1; j > 0 && j < step; j++) {
                    m_Runs.push_back(*m_Runs.rbegin());
                }
                m_Runs.push_back(prob / 100);
                break;
            }
        } else {
            assert(run_format == eMonkey_RunSequence);
            if (*run.rbegin() == '%') {
                throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::e_MonkeyInvalidArgs,
                        string("Value is percentage: ") + run +
                        string(", values have to be either only percentages or "
                               "only numbers of tries, not mixed"));
            }
            int val = NStr::StringToInt(run);
            if (m_Runs.size() > 0 && val <= *m_Runs.rbegin()) {
                throw CMonkeyException(
                    CDiagCompileInfo(__FILE__, __LINE__),
                    NULL, CMonkeyException::e_MonkeyInvalidArgs,
                    string("\"runs\" should contain values in "
                           "increasing order"));
            }
            m_Runs.push_back(val);
        }
    }
    m_RunsSize = m_Runs.size();
}


int /* EIO_Status or -1 */ CMonkeyRuleBase::GetReturnStatus() const
{
    return m_ReturnStatus;
}


unsigned long CMonkeyRuleBase::GetDelay() const
{
    return m_Delay;
}


std::string CMonkeyRuleBase::GetSection(void) const
{
    return m_Section;
}


EMonkeyActionType CMonkeyRuleBase::GetActionType(void) const
{
    return m_ActionType;
}


void CMonkeyRuleBase::IterateRun(MONKEY_SOCKTYPE sock)
{
    if (m_Runs.empty()) 
        return;
    m_RunPos[sock]++;
    /* In probability mode each item of m_Runs is a probability of each run */
    if (m_RunMode == eMonkey_RunProbability) {
        assert(m_RunPos[sock] <= m_Runs.size());
        if (m_RunPos[sock] == m_Runs.size()) {
            switch (m_RepeatType) {
            case eMonkey_RepeatNone: case eMonkey_RepeatLast:
                m_RunPos[sock]--;
            case eMonkey_RepeatAgain:
                m_RunPos[sock] = 0;
                break;
            }
        }
        return;
    }
    /* In "number of the run" mode each item in m_Runs is a specific number of
    run when the rule should trigger */
    else if ((m_RunPos.at(sock) + 1) > *m_Runs.rbegin()) {
        switch (m_RepeatType) {
        case eMonkey_RepeatAgain:
            m_RunPos[sock] = 0;
            break;
        }
    }
    return;
}


/*//////////////////////////////////////////////////////////////////////////////
//                              CMonkeyRWRuleBase                             //
//////////////////////////////////////////////////////////////////////////////*/
CMonkeyRWRuleBase::CMonkeyRWRuleBase(EMonkeyActionType           action_type,
                                     string                      section,
                                     const vector<string>&       params)
    : CMonkeyRuleBase(action_type, section, params)
{
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyWriteRule
//////////////////////////////////////////////////////////////////////////
CMonkeyWriteRule::CMonkeyWriteRule(string section, const vector<string>& params) 
    : CMonkeyRWRuleBase(eMonkey_Send, section, params)
{
}


MONKEY_RETTYPE CMonkeyWriteRule::Run(MONKEY_SOCKTYPE        sock,
                                     const MONKEY_DATATYPE  data,
                                     MONKEY_LENTYPE         size,
                                     int                    flags,
                                     SOCK*                  sock_ptr)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedSend(true);
#endif /* NCBI_MONKEY_TESTS */
    int return_status = GetReturnStatus();
    LOG_POST(Error << "[CMonkeyReadRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED send(). " << 
                      (return_status != -1  
                       ? s_EIOStatusString((EIO_Status)return_status) 
                       : string()));
    if (return_status != eIO_Success && return_status != -1) {
        switch (return_status) {
        case eIO_Timeout:
            *sock_ptr = s_TimeoutingSock->GetSOCK();
            MONKEY_SET_SOCKET_ERROR(SOCK_EWOULDBLOCK);
            return -1;
        case eIO_Closed:
            MONKEY_SET_SOCKET_ERROR(SOCK_ENOTCONN);
            return -1;
        case eIO_Unknown:
            MONKEY_SET_SOCKET_ERROR(MONKEY_ENOPROTOOPT);
            return -1;
        case eIO_Interrupt:
            MONKEY_SET_SOCKET_ERROR(SOCK_EINTR);
            return -1;
        default:
            break;
        }
    }

    return send(sock, (const char*)data, size, flags);
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyReadRule
//////////////////////////////////////////////////////////////////////////
CMonkeyReadRule::CMonkeyReadRule(string section, const vector<string>& params)
    : CMonkeyRWRuleBase(eMonkey_Recv, section, params)
{
    STimeout r_timeout = { 1, 0 };
}


MONKEY_RETTYPE CMonkeyReadRule::Run(MONKEY_SOCKTYPE sock,
                                    MONKEY_DATATYPE buf,
                                    MONKEY_LENTYPE  size,
                                    int             flags,
                                    SOCK*           sock_ptr)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedRecv(true);
#endif /* NCBI_MONKEY_TESTS */
    int return_status = GetReturnStatus();
    LOG_POST(Error << "[CMonkeyReadRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED recv(). " << 
                      (return_status != -1  
                       ? s_EIOStatusString((EIO_Status)return_status) 
                       : string()));
    if (return_status != eIO_Success && return_status != -1) {
        switch (return_status) {
        case eIO_Timeout:
            *sock_ptr = s_TimeoutingSock->GetSOCK();
            MONKEY_SET_SOCKET_ERROR(SOCK_EWOULDBLOCK);
            return -1;
        case eIO_Closed:
            MONKEY_SET_SOCKET_ERROR(SOCK_ENOTCONN);
            return -1;
        case eIO_Unknown:
            MONKEY_SET_SOCKET_ERROR(MONKEY_ENOPROTOOPT);
            return -1;
        case eIO_Interrupt:
            MONKEY_SET_SOCKET_ERROR(SOCK_EINTR);
        default:
            break;
        }
    }

    if (size == 0) 
        return 0;

    /* So we decided to override */
    MONKEY_RETTYPE bytes_read = recv(sock, buf, size, flags);
    
    return bytes_read;
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyConnectRule
//////////////////////////////////////////////////////////////////////////

CMonkeyConnectRule::CMonkeyConnectRule(string                section,
                                       const vector<string>& params)
    : CMonkeyRuleBase(eMonkey_Connect, section, params)
{
    bool allow_set = false;
    for (unsigned int i = 0; i < params.size(); i++) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name = name_value[0];
        string value = name_value[1];
        if (name == "allow") {
            if (allow_set) {
                throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                       NULL, 
                                       CMonkeyException::e_MonkeyInvalidArgs,
                                       string("Parameter \"allow\" is set "
                                       "twice in [") + GetSection() + "]" +
                                       s_RuleTypeString(GetActionType()));
            }
            m_Allow = ConnNetInfo_Boolean(value.c_str()) == 1;
            allow_set = true;
        }
    }
    if (!allow_set)
        throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                               NULL, CMonkeyException::e_MonkeyInvalidArgs,
                               string("Parameter \"allow\" not set in [")
                               + GetSection() + "]" +
                               s_RuleTypeString(GetActionType()));
}


int CMonkeyConnectRule::Run(MONKEY_SOCKTYPE        sock,
                            const struct sockaddr* name,
                            MONKEY_SOCKLENTYPE     namelen)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedConnect(true);
#endif /* NCBI_MONKEY_TESTS */
    int return_status = GetReturnStatus();
    LOG_POST(Error << "[CMonkeyReadRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED connect(). " << 
                      (return_status != -1  
                       ? s_EIOStatusString((EIO_Status)return_status) 
                       : string()));
    if (return_status != eIO_Success && return_status != -1) {
        switch (return_status) {
        case eIO_Timeout:
            MONKEY_SET_SOCKET_ERROR(SOCK_EWOULDBLOCK);
            return -1;
        case eIO_Closed:
            MONKEY_SET_SOCKET_ERROR(SOCK_ECONNREFUSED);
            return -1;
        case eIO_Unknown:
            MONKEY_SET_SOCKET_ERROR(MONKEY_ENOPROTOOPT);
            return -1;
        case eIO_Interrupt:
            MONKEY_SET_SOCKET_ERROR(SOCK_EINTR);
            return -1;
        default:
            break;
        }
    }
    if (GetDelay() > 0) {
        SleepMilliSec(GetDelay(), EInterruptOnSignal::eInterruptOnSignal);
    }
    if (m_Allow) {
        return connect(sock, name, namelen);
    }
    else {
        struct sockaddr_in addr;
        /* Connect to a non-existing host */
        addr.sin_family = AF_INET;
        addr.sin_port = 65511;
        /* 3232235631 is 192.168.0.111 - does not exist at NCBI, which is 
           enough for the goal of test */
#ifdef NCBI_OS_MSWIN
        addr.sin_addr.S_un.S_addr = htonl(3232235631);
#else
        addr.sin_addr.s_addr = htonl(3232235631);
#endif /* NCBI_OS_MSWIN */
        return connect(sock, (struct sockaddr*)&addr, namelen);
    }
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyPollRule
//////////////////////////////////////////////////////////////////////////
CMonkeyPollRule::CMonkeyPollRule(string section, const vector<string>& params)
    : CMonkeyRuleBase(eMonkey_Poll, section, params)
{
    bool ignore_set = false;
    for ( unsigned int i = 0;  i < params.size();  i++ ) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name  = name_value[0];
        string value = name_value[1];
        if (name == "ignore") {
            if (ignore_set) {
                throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                       NULL,
                                       CMonkeyException::e_MonkeyInvalidArgs,
                                       string("Parameter \"ignore\" is set "
                                       "twice in [") + GetSection() + "]" +
                                       s_RuleTypeString(GetActionType()));
            }
            m_Ignore = ConnNetInfo_Boolean(value.c_str()) == 1;
            ignore_set = true;
        }
    }
    if (!ignore_set) {
        throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                               NULL, CMonkeyException::e_MonkeyInvalidArgs,
                               string("Parameter \"ignore\" not set in [")
                               + GetSection() + "]" +
                               s_RuleTypeString(GetActionType()));
    }
}


bool CMonkeyPollRule::Run(size_t*     n,
                          SOCK*       sock,
                          EIO_Status* return_status)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedPoll(true);
#endif /* NCBI_MONKEY_TESTS */
    LOG_POST(Error << "[CMonkeyReadRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED poll(). " << 
                      (m_Ignore ? "Ignoring poll" : "NOT ignoring poll"));
    if (m_Ignore) {
        return true;
    }
    return false;
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyPlan
//////////////////////////////////////////////////////////////////////////
CMonkeyPlan::CMonkeyPlan(const string& section)
    : m_Probability(100), m_HostRegex(".*"), m_Name(section)
{
    /* Plan settings */
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    string probability = config.Get(section, "probability");
    if (probability != "") {
        if (probability.find('%') != string::npos) {
            probability = probability.substr(0, probability.length() - 1);
            m_Probability = static_cast<unsigned short>(
                NStr::StringToUInt(probability));
        } else {
            m_Probability = static_cast<unsigned short>(
                NStr::StringToDouble(probability) * 100);
        }
    }
    m_HostRegex = NStr::Replace(NStr::Replace(
            config.Get(section, "host_match"), " ",  ""),
                                               "\t", "");

    x_ReadPortRange(config.Get(section, "port_match"));

    /* Write rules */
    x_LoadRules(section, "write",   m_WriteRules);
    /* Read rules */
    x_LoadRules(section, "read",    m_ReadRules);
    /* Connect rules */
    x_LoadRules(section, "connect", m_ConnectRules);
    /* Poll rules */
    x_LoadRules(section, "poll",    m_PollRules);
}


template<class Rule_Ty>
void CMonkeyPlan::x_LoadRules(const string&    section,
                              const string&    rule_type_str,
                              vector<Rule_Ty>& container)
{
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    bool multi_rule = false;
    string rule_str = config.Get(section, rule_type_str);
    if (rule_str == "") {
        /* Try check if there are multiple rules defined */
        if ((rule_str = config.Get(section, rule_type_str + "1")) != "")
            multi_rule = true;
    }
    if ( rule_str == "" )
        return;
    /* If a rule was read - parse it */
    string temp_conf = NStr::Replace(rule_str, string(" "), string(""));
    vector<string> params = s_Monkey_Split(temp_conf, ';');
    container.push_back(Rule_Ty(section, params));
    if ( multi_rule ) {
        int rule_num = 2;
        string val_name = rule_type_str + NStr::IntToString(rule_num++);
        while ( (rule_str = config.Get(section, val_name)) != "" ) {
            temp_conf = NStr::Replace(rule_str, string(" "), string(""));
            params = s_Monkey_Split(temp_conf, ';');
            container.push_back(Rule_Ty(section, params));
            val_name = rule_type_str+ NStr::IntToString(rule_num++);
        }
    }
}

#ifndef NCBI_OS_MSWIN
/** Regex-like check (might use some refactoring) */
static bool s_MatchRegex(const string& to_match, const string& regex)
{
    /* Convert "match regex" task to "haystack and needle" task. We check 
     * whether there is a wildcard in the beginning and/or in the end of
     * regex (other cases are not supported). And then check if to_match 
     * starts with, end with or contains the needle */

    /* First - compatibility check. Functionality is limited. */
    /* Check for special characters and combinations that cannot be processed.
    * Method - remove combinations of characters that are supported and check
    * the rest */
    string exception_message = string("Pattern") + regex + " "
        "cannot be processed because regular expressions "
        "are not fully supported. You can use .* in the "
        "beginning and in the end of a pattern and | to separate "
        "patterns. Stop mark is set with \\\\.\n"
        "Example:\n"
        "host_match = .*nlm\\\\.nih.*|.*gov|10\\\\.55.*\n";
        string filtered = NStr::Replace(NStr::Replace(regex, "\\.", ""),
                                                             ".*" , "");
    if (filtered.find_first_of("[]()+^?{}$.*\\") != string::npos) {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL,
            CMonkeyException::e_MonkeyInvalidArgs,
            exception_message);
    }
    size_t pos = 0;
    size_t last_find = 0;
    bool start_wildcard = false, end_wildcard = false;
    while ((last_find = NStr::Find(regex, ".*", pos)) != NPOS &&
        pos < regex.length()) {
        pos = last_find + 1;
        if (last_find == 0) {
            start_wildcard = true;
        }
        else if (last_find == regex.length() - strlen(".*")) {
            end_wildcard = true;
        }
        else {
            throw CMonkeyException(
                CDiagCompileInfo(__FILE__, __LINE__),
                NULL,
                CMonkeyException::e_MonkeyInvalidArgs,
                exception_message);
        }
    }
    /*
     * Run the "Needle in a haystack" task 
     */
    string needle = NStr::Replace(NStr::Replace(regex, ".*", ""), "\\.", ".");
    if (start_wildcard && !end_wildcard) {
        if (!NStr::StartsWith(to_match, needle)) {
            return false;
        }
    }
    else if (!start_wildcard && end_wildcard) {
        if (!NStr::EndsWith(to_match, needle)) {
            return false;
        }
    }
    else if (start_wildcard && end_wildcard) {
        if (NStr::Find(to_match, needle) == NPOS) {
            return false;
        }
    }
    return true;
}
#endif /* NCBI_OS_MSWIN */


/* Compares supplied parameters to */
bool CMonkeyPlan::Match(const string&  sock_host,
                        unsigned short sock_port, 
                        const string&  sock_url,
                        unsigned short probability_left)
{
    /* Check that regex works and that at least one of hostname and IP 
     * is not empty*/
    if ( !m_HostRegex.empty()  &&
         (sock_host.length() + sock_url.length() > 0) ) {
#ifdef NCBI_OS_MSWIN
        regex reg(m_HostRegex);
        smatch find;
        if (!regex_match(sock_host, find, reg) && 
            !regex_match(sock_url,  find, reg)) {
            LOG_POST(Note << "[CMonkeyPlan::Match]  Plan " << m_Name << " was "
                          << "NOT matched. Reason: "
                          << "[host regex mismatch, host=" << sock_host
                          << ", regex = \"" << m_HostRegex << "\"]");
            return false;
        }
#else
    /*
     * Regex is not supported - work with simple simulation - only .* in
     * beginning and end of pattern and | to separate patterns
     */
        vector<string> patterns = s_Monkey_Split(m_HostRegex, '|');
        auto it = patterns.begin();
        bool match_found = false;
        for (  ;  it != patterns.end();  it++) {
            if (s_MatchRegex(sock_url, *it) || s_MatchRegex(sock_host, *it)) {
                match_found = true;
                break;
            }
        }
        if (!match_found) {
            LOG_POST(Note << "[CMonkeyPlan::Match]  Plan " << m_Name << " was "
                          << "NOT matched. Reason: "
                          << "[host regex mismatch, host=" << sock_host
                          << ", regex = \"" << m_HostRegex << "\"]");
            return false;
        }
#endif /* NCBI_OS_MSWIN */
    }
    /* If port match pattern is not set, any port is good */
    bool port_match = m_PortRanges.empty();
    auto port_iter = m_PortRanges.begin();
    for (; port_iter != m_PortRanges.end(); port_iter++) {
        if (sock_port >= *port_iter && sock_port <= *++port_iter) {
            port_match = true;
            break;
        }
    }
    int rand_val = CMonkey::Instance()->GetRand(Key()) % 100;
    bool prob_match = rand_val < m_Probability * 100 / probability_left;
    LOG_POST(Note << "[CMonkeyPlan::Match]  Checking if plan " << m_Name
                  << " will be matched. Random value is "
                  << rand_val << ", plan probability is "
                  << m_Probability << "%, probability left is " 
                  << probability_left << "%, so probability threshold is "
                  << m_Probability * 100 / probability_left << "%");
    bool match = port_match && prob_match;
    LOG_POST(Note << "[CMonkeyPlan::Match]  Plan " << m_Name << " was "
                  << (match ? "" : "NOT ") << "matched"
                  << (!match ? ". Reason: " : "")
                  << (port_match ? "" : "[invalid port range] ")
                  << (prob_match ? "" : "[probability threshold]"));
    return port_match ? (rand_val < m_Probability * 100 / probability_left) 
                                  : false;
}


bool CMonkeyPlan::WriteRule(MONKEY_SOCKTYPE        sock,
                            const MONKEY_DATATYPE  data,
                            MONKEY_LENTYPE         size,
                            int                    flags,
                            MONKEY_RETTYPE*        bytes_written,
                            SOCK*                  sock_ptr)
{
    short probability_left = 100;
    int res = -1;
    for (unsigned int i = 0;  i < m_WriteRules.size();  i++) {
        m_WriteRules[i].AddSocket(sock);
        unsigned short rule_prob = m_WriteRules[i].GetProbability(sock);
        if (m_WriteRules[i].CheckRun(sock, rule_prob, probability_left)) {
            *bytes_written = m_WriteRules[i].Run(sock, data, size, flags, 
                                                 sock_ptr);
            res = 1;
            break;
        }
        /* If this rule did not engage, we go to the next rule, and
           remember to normalize probability of next rule */
        probability_left -= rule_prob;
        if (probability_left <= 0) {
            stringstream ss;
            ss << "Probability below zero for write rule in plan " << m_Name
                << ". Check config!";
            throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::e_MonkeyInvalidArgs, 
                        ss.str());
        }
    }
    // If no rules engaged, return 0
    if (res == -1)
        res = 0;

    // Iterate run in all rules
    for (unsigned int i = 0;  i < m_WriteRules.size();  i++) {
        m_WriteRules[i].IterateRun(sock);
    }

    return res == 0 ? false : true;
}


bool CMonkeyPlan::ReadRule(MONKEY_SOCKTYPE        sock,
                           MONKEY_DATATYPE        buf,
                           MONKEY_LENTYPE         size,
                           int                    flags,
                           MONKEY_RETTYPE*        bytes_read,
                           SOCK*                  sock_ptr)
{
    short probability_left = 100;
    int res = -1;
    for (unsigned int i = 0;  i < m_ReadRules.size();  i++) {
        m_ReadRules[i].AddSocket(sock);
        unsigned short rule_prob = m_ReadRules[i].GetProbability(sock);
        if (m_ReadRules[i].CheckRun(sock, rule_prob, probability_left)) {
            *bytes_read = m_ReadRules[i].Run(sock, buf, size, flags, sock_ptr);
            res = 1;
            break;
        }
        /* If this rule did not engage, we go to the next rule, and
           and remember to normalize probability of next rule */
        probability_left -= rule_prob;
        if (probability_left <= 0) {
            stringstream ss;
            ss << "Probability below zero for write rule in plan " << m_Name
               << ". Check config!";
            throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                   NULL, CMonkeyException::e_MonkeyInvalidArgs,
                                   ss.str());
        }
    }

    // If no rules engaged, return 0
    if (res == -1)
        res = 0;

    // Iterate run in all rules
    for (unsigned int i = 0;  i < m_ReadRules.size();  i++) {
        m_ReadRules[i].IterateRun(sock);
    }

    return res == 0 ? false : true;
}


bool CMonkeyPlan::ConnectRule(MONKEY_SOCKTYPE        sock,
                              const struct sockaddr* name,
                              MONKEY_SOCKLENTYPE     namelen,
                              int*                   result)
{
    short probability_left = 100;
    /* We are fine with losing precision. Just need a unique ID*/
    MONKEY_SOCKTYPE x_sock = CMonkey::Instance()->GetSockBySocketid(sock);
    int res = -1;
    for (unsigned int i = 0; i < m_ConnectRules.size(); i++) {
        m_ConnectRules[i].AddSocket(x_sock);
        unsigned short rule_prob = 
            m_ConnectRules[i].GetProbability(x_sock);
        /* Check if the rule will trigger on this run. If not - we go to the 
           next rule in plan */
        if (m_ConnectRules[i].CheckRun(x_sock, rule_prob, probability_left)) {
            /* 'result' is the result of connect() launched in the rule. It can
               even be result of original connect() with real parameters.
               Or, it can be an error code of a failed fake connect() */
            *result = m_ConnectRules[i].Run(sock, name, namelen);
            res = 1;
            break;
        }
        /* If this rule did not engage, we go to the next rule, and
           and remember to normalize probability of next rule */
        probability_left -= rule_prob;
        if (probability_left <= 0) {
            stringstream ss;
            ss << "Probability below zero for write rule in plan " << m_Name
                << ". Check config!";
            throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                   NULL, CMonkeyException::e_MonkeyInvalidArgs,
                                   ss.str());
        }
    }

    // If no rules engaged, return 0
    if (res == -1)
        res = 0;

    // Iterate run in all rules
    for (unsigned int i = 0;  i < m_ConnectRules.size();  i++) {
        m_ConnectRules[i].IterateRun(x_sock);
    }

    return res == 0 ? false : true;
}


bool CMonkeyPlan::PollRule(size_t*     n,
                           SOCK*       sock,
                           EIO_Status* return_status)
{
    short probability_left = 100;
    int res = -1;
    for (unsigned int i = 0;  i < m_PollRules.size();  i++) {
        m_PollRules[i].AddSocket((*sock)->sock);
        unsigned short rule_prob = m_PollRules[i].GetProbability((*sock)->sock);
        if (m_PollRules[i].CheckRun((*sock)->sock, rule_prob, probability_left))
        {
            res = m_PollRules[i].Run(n, sock, return_status) ? 1 : 0;
            break;
        }
        LOG_POST(Error << "[CMonkeyPlan::PollRule]  CHAOS MONKEY NOT "
                          "ENGAGED!!! poll() passed");
        /* If this rule did not engage, we go to the next rule, and
           and remember to normalize probability of next rule */
        probability_left -= rule_prob;
        if (probability_left <= 0) {
            stringstream ss;
            ss << "Probability below zero for write rule in plan " << m_Name
                << ". Check config!";
            throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::e_MonkeyInvalidArgs, 
                        ss.str());
        }
    }

    // If no rules triggered, return 0
    if (res == -1) {
        res = 0;
        *return_status = EIO_Status::eIO_Success;
    }

    // Iterate run in all rules
    for (unsigned int i = 0;  i < m_PollRules.size();  i++) {
        m_PollRules[i].IterateRun((*sock)->sock);
    }

    return res == 0 ? false : true;
}


unsigned short CMonkeyPlan::GetProbabilty(void)
{
    return m_Probability;
}


void CMonkeyPlan::x_ReadPortRange(const string& conf)
{
    vector<string> ranges = s_Monkey_Split(conf, ',');
    unsigned short from, to;
    auto it = ranges.begin();
    for (  ;  it != ranges.end();  it++  )  {
        if (it->find('-') != string::npos) {
            vector<string> ports = s_Monkey_Split(*it, '-');
            from = static_cast<unsigned short>(NStr::StringToUInt(ports[0]));
            to   = static_cast<unsigned short>(NStr::StringToUInt(ports[1]));
        } else {
            from = to = static_cast<unsigned short>(NStr::StringToUInt(*it));
        }
        /* From*/
        m_PortRanges.push_back(from);
        /* Until */
        m_PortRanges.push_back(to);
    }
}


//////////////////////////////////////////////////////////////////////////
// CMonkey
//////////////////////////////////////////////////////////////////////////
CMonkey*          CMonkey::sm_Instance   = NULL;
FMonkeyHookSwitch CMonkey::sm_HookSwitch = NULL;


CMonkey::CMonkey() : m_Probability(100), m_Enabled(false)
{
    if (sm_HookSwitch == NULL) {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::e_MonkeyInvalidArgs,
            "Launch CONNECT_Init() before initializing CMonkey instance");
    }
    m_TlsToken = new CTls<int>;
    m_TlsRandList = new CTls<vector<int> >;
    m_TlsRandListPos = new CTls<int>;
    srand((unsigned int)time(NULL));
    m_Seed = rand();
    LOG_POST(Note << "[CMonkey::CMonkey()]  Chaos Monkey seed is: " << m_Seed);
    ReloadConfig();
}


CMonkey* CMonkey::Instance()
{
    CFastMutexGuard spawn_guard(s_SingletonMutex);

    if (sm_Instance == NULL) {
        sm_Instance = new CMonkey;
    }
    return sm_Instance;
}


bool CMonkey::IsEnabled()
{
    return m_Enabled;
}


void CMonkey::ReloadConfig(const string& config)
{
    assert(sm_HookSwitch != NULL);
    CFastMutexGuard spawn_guard(s_ConfigMutex);
    string          rules;
    string          monkey_section = config.empty() ? s_GetMonkeySection() :
                                                      config;
    list<string>    sections;
    m_Plans.clear();
    CNcbiRegistry&  reg = CNcbiApplication::Instance()->GetConfig();
    if (ConnNetInfo_Boolean(reg.Get(kMonkeyMainSect, kEnablField).c_str()) != 1)
    {
        LOG_POST(Note << "[CMonkey::ReloadConfig]  Chaos Monkey is disabled "
                      << "in [" << kMonkeyMainSect << "]");
        m_Enabled = false;
        return;
    }
    string seed = reg.Get(kMonkeyMainSect, kSeedField).c_str();
    if (seed != "") {
        LOG_POST(Note << "[CMonkey::ReloadConfig]  Chaos Monkey seed is set "
                      << "to " << seed << " in config");
        SetSeed(NStr::StringToInt(seed));
    }
    reg.EnumerateSections(&sections);
    /* If the section does not exist */
    if (find(sections.begin(), sections.end(), monkey_section)
        == sections.end()) {
        LOG_POST(Error << "[CMonkey::ReloadConfig]  Config section [" <<
                          monkey_section << "] does not exist, Chaos Monkey "
                          "is NOT active!");
        m_Enabled = false;
        return;
    }
    if (ConnNetInfo_Boolean(reg.Get(monkey_section, kEnablField).c_str()) != 1)
    {
        LOG_POST(Note << "[CMonkey::ReloadConfig]  Chaos Monkey is disabled "
                      << "in [" << monkey_section << "]");
        m_Enabled = false;
        return;
    }
    m_Enabled = true;
    LOG_POST(Error << "[CMonkey::ReloadConfig]  Chaos Monkey is active!");
    string prob_str = reg.Get(monkey_section, "probability");
    if (prob_str != "") {
        prob_str = NStr::Replace(prob_str, " ", "");
        int prob;
        try {
            if (*prob_str.rbegin() == '%') {
                string prob_tmp = prob_str.substr(0, prob_str.length() - 1);
                prob = NStr::StringToInt(prob_tmp);
            }
            else {
                prob = (int)(NStr::StringToDouble(prob_str) * 100);
            }
            if (prob < 0 || prob > 100) {
                throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                       NULL,
                                       CMonkeyException::e_MonkeyInvalidArgs,
                                       "Parameter \"probability\"=" + prob_str
                                       + " for section [" + monkey_section
                                       + "] is not a valid value "
                                       "(valid range is 0%-100% or 0.0-1.0)");
            }
            m_Probability = (unsigned short)prob;
        }
        catch (const CStringException&) {
            throw CMonkeyException(CDiagCompileInfo(__FILE__, __LINE__),
                                   NULL, CMonkeyException::e_MonkeyInvalidArgs,
                                   "Parameter \"probability\"=" + prob_str
                                   + " for section [" + monkey_section
                                   + "] could not be parsed");
        }
    }
    // Disable hooks while Monkey initializes
    sm_HookSwitch(eMonkeyHookSwitch_Disabled);
    unsigned short total_probability = 0;
    for (int i = 1;  ;  ++i) {
        string section = monkey_section + "_PLAN" + NStr::IntToString(i);
        if (find(sections.begin(), sections.end(), section) ==
            sections.end()) {
            break;
        }
        m_Plans.push_back(CMonkeyPlan(section));
        total_probability += (*m_Plans.rbegin()).GetProbabilty();
    }
    /* Check that sum of probabilities for plans is less than 100%, otherwise -
     * throw an exception with an easy-to-read message */
    if (total_probability > 100) {
        stringstream ss;
        ss << "Total probability for plans in configuration " <<
            monkey_section << " exceeds 100%. Please check that summary " <<
            "probability for plans stays under 100% (where the remaining "
            "percents go to connections that are not intercepted by any plan)."
            "\nTurning Chaos Monkey off";
        LOG_POST(Critical << ss.str());
        m_Enabled = false;
    }
    if (m_Enabled) {
        s_TimeoutingSocketInit();
    }
    else {
        s_TimeoutingSocketDestroy();
    }
    sm_HookSwitch(m_Enabled ? eMonkeyHookSwitch_Enabled 
                            : eMonkeyHookSwitch_Disabled);
}


MONKEY_RETTYPE CMonkey::Send(MONKEY_SOCKTYPE        sock,
                             const MONKEY_DATATYPE  data,
                             MONKEY_LENTYPE         size,
                             int                    flags,
                             SOCK*                  sock_ptr)
{
    string host_fqdn, host_IP;
    unsigned short peer_port;
    if (m_Enabled) {
        x_GetSocketDestinations(sock, &host_fqdn, &host_IP, NULL, &peer_port);
        LOG_POST(Note << "[CMonkey::Send]  For connection with port " 
                      << peer_port << ", host " << host_IP
                      << " and hostname " << host_fqdn << ".");
        CMonkeyPlan* sock_plan = x_FindPlan(sock, host_fqdn,host_IP,peer_port);

        if (sock_plan != NULL) {
            MONKEY_RETTYPE bytes_written;
            /* Plan may decide to leave connection untouched */
            if (sock_plan->WriteRule(sock, data, size, flags, &bytes_written, 
                                     sock_ptr) )
                return bytes_written;
        }
    }
    return send(sock, (const char*)data, size, flags);
}


MONKEY_RETTYPE CMonkey::Recv(MONKEY_SOCKTYPE        sock,
                             MONKEY_DATATYPE        buf,
                             MONKEY_LENTYPE         size,
                             int                    flags,
                             SOCK*                  sock_ptr)
{
    string host_fqdn, host_IP;
    unsigned short peer_port;
    if (m_Enabled) {
        x_GetSocketDestinations(sock, &host_fqdn, &host_IP, NULL, &peer_port);
        LOG_POST(Note << "[CMonkey::Recv]  For connection with port " 
                      << peer_port << ", host " << host_IP
                      << " and hostname " << host_fqdn << ".");

        CMonkeyPlan* sock_plan = x_FindPlan(sock, host_fqdn, host_IP,peer_port);

        if (sock_plan != NULL) {
            MONKEY_RETTYPE bytes_read;
            if (sock_plan->ReadRule(sock, buf, size, flags, &bytes_read, 
                                    sock_ptr))
                return bytes_read;
        }
    }
    return recv(sock, buf, size, flags);
}


int CMonkey::Connect(MONKEY_SOCKTYPE        sock,
                     const struct sockaddr* name,
                     MONKEY_SOCKLENTYPE     namelen)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } addr;
    if (m_Enabled) {
        addr.sa = *name;
        unsigned int host = addr.in.sin_addr.s_addr;
        string host_fqdn, host_IP;
        unsigned short peer_port = ntohs(addr.in.sin_port);
        SFqdnIp&&    net_data = x_GetFqdnIp(host);
        LOG_POST(Note << "[CMonkey::Connect]  For connection with port " 
                      << peer_port << ", host " << host_IP
                      << " and hostname " << host_fqdn << ".");
        CMonkeyPlan* sock_plan = x_FindPlan(CMonkey::GetSockBySocketid(sock),
                                            net_data.fqdn, net_data.ip, 
                                            peer_port);
        if (sock_plan != NULL) {
            int result;
            if ( sock_plan->ConnectRule(sock, name, namelen, &result) )
                return result;
        }
    }
    return connect(sock, name, namelen);
}


bool CMonkey::Poll(size_t*      n,
                   SSOCK_Poll** polls,
                   EIO_Status*  return_status)
{
    if (m_Enabled) {
        size_t polls_iter       = 0;
        SSOCK_Poll* new_polls   = 
            static_cast<SSOCK_Poll*>(calloc(*n, sizeof(SSOCK_Poll)));
        int polls_count         = 0;
        while (polls_iter < *n) {
            SOCK&        sock      = (*polls)[polls_iter].sock;
            LOG_POST(Note << "[CMonkey::Poll]  For connection with port " 
                          << sock->myport << ", host <empty> and "
                             "hostname <empty>.");
            if (sock != nullptr)
            {
                CMonkeyPlan* sock_plan = x_FindPlan(sock->id, "", "", 
                                                    sock->myport);
                if (sock_plan == NULL ||
                    (sock_plan != NULL && !sock_plan->PollRule(n, &sock,
                                                               return_status)) )
                {
                    new_polls[polls_count++] = (*polls)[polls_iter];
                }
            } else { /* something unreadable by Monkey, but still valid */
                new_polls[polls_count++] = (*polls)[polls_iter];
            }
            polls_iter++;
        }
        *polls = new_polls;
        *n = polls_count;
    }
    return false;
}


void CMonkey::Close(MONKEY_SOCKTYPE sock)
{
    auto sock_plan = m_KnownSockets.find(sock);
    if (sock_plan != m_KnownSockets.end()) {
        m_KnownSockets.erase(sock_plan);
    }
    auto sock_iter = m_SocketMemory.begin();
    for (  ;  sock_iter != m_SocketMemory.end()  ;  )
    {
        if (sock_iter->first == sock)
            m_SocketMemory.erase(sock_iter++);
        else
            ++sock_iter;
    }
}


void CMonkey::SockHasSocket(SOCK sock, MONKEY_SOCKTYPE socket)
{
    m_SocketMemory[socket] = sock;
}


MONKEY_SOCKTYPE CMonkey::GetSockBySocketid(MONKEY_SOCKTYPE socket)
{
    union SSockSocket
    {
        SOCK sock;
        MONKEY_SOCKTYPE socket;
    };
    SSockSocket sock_socket;
    sock_socket.sock = m_SocketMemory[socket];
    return sock_socket.socket;
}


void CMonkey::MonkeyHookSwitchSet(FMonkeyHookSwitch hook_switch_func)
{
    sm_HookSwitch = hook_switch_func;
}


/** Return plan for the socket, new or already assigned one. If the socket
 * is ignored by Chaos Monkey, NULL is returned */
CMonkeyPlan* CMonkey::x_FindPlan(MONKEY_SOCKTYPE sock,  const string& hostname, 
                                 const string& host_IP, unsigned short port)
{
    CFastMutexGuard spawn_guard(s_KnownConnMutex);
    auto sock_plan = m_KnownSockets.find(sock);
    if (sock_plan != m_KnownSockets.end()) {
        return sock_plan->second;
    }
    /* Plan was not found. First roll the dice to know if Monkey will process 
     * current socket */
    int rand_val = CMonkey::Instance()->GetRand(Key()) % 100;
    LOG_POST(Note << "[CMonkey::x_FindPlan]  Checking if connection "
                     "with port " << port << ", host " << host_IP
                  << " and hostname " << hostname << " will be "
                  << "intercepted by Chaos Monkey. Random value is " 
                  << rand_val << ", probability threshold is " 
                  << m_Probability);
    LOG_POST(Note << "[CMonkey::x_FindPlan]  The connection will "
                  << ((rand_val >= m_Probability) ? "NOT " : "") 
                  << "be processed.");
    if (rand_val >= m_Probability) {
        return NULL;
    }
    /* Now we can find a plan */
    bool match_found = false;
    unsigned short probability_left = 100; /* If we tried to match a plan with
                                              n% probability with no luck,
                                              next plan only has (100-n)% 
                                              fraction of connections to match
                                              against, so if its probability to
                                              all connections is m%, its 
                                              probability to connections left 
                                              is m/(100-n)*100%. To count this,
                                              we need probability_left */
    for (unsigned int i = 0; i < m_Plans.size() && !match_found; i++) {
        /* Match includes probability of plan*/
        if (m_Plans[i].Match(host_IP, port, hostname, probability_left)) {
            // 3. If found plan - use it and assign to this socket
            m_KnownSockets[sock] = &m_Plans[i];
            return m_KnownSockets[sock];
        }
        probability_left -= m_Plans[i].GetProbabilty();
    }
    /* If no plan triggered, then this socket will be always ignored */
    m_KnownSockets[sock] = NULL;
    return NULL;
}


CMonkey::SFqdnIp CMonkey::x_GetFqdnIp(unsigned int host)
{
    CFastMutexGuard guard(s_NetworkDataCacheMutex);
    auto iter = m_NetworkDataCache.find(host);
    if (iter != m_NetworkDataCache.end())
        return iter->second;
    string       host_fqdn = CSocketAPI::gethostbyaddr(host);
    string       host_IP = CSocketAPI::HostPortToString(host, 0);
    return m_NetworkDataCache[host] = SFqdnIp(host_fqdn, host_IP);
}


bool CMonkey::RegisterThread(int token)
{
    if (!m_Enabled) {
        ERR_POST(Error << "[CMonkey::RegisterThread]  Chaos Monkey is "
                       << "disabled, the thread with token "
                       << token << " was not registered");
        return false;
    }
    CFastMutexGuard guard(s_SeedLogConfigMutex);
    LOG_POST(Note << "[CMonkey::RegisterThread]  Registering thread with "
                  << "token " << token);

    stringstream ss;
    ss << "[CMonkey::RegisterThread]  Token " << token 
       << " has been already registered in CMonkey and cannot be used again";
    if (m_RegisteredTokens.find(token) != m_RegisteredTokens.end()) {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::e_MonkeyInvalidArgs,
            ss.str());
    }
    m_RegisteredTokens.insert(token);

    /* Remember token */
    m_TlsToken->SetValue(new int, s_TlsCleanup<int>);
    *m_TlsToken->GetValue() = token;

    vector<int>* rand_list = new vector<int>();
    srand(m_Seed + token);
    stringstream rand_list_str;
    for (unsigned int i = 0; i < kRandCount; ++i) {
        rand_list->push_back(rand());
        rand_list_str << *rand_list->rbegin() << " ";
    }
    LOG_POST(Note << "[CMonkey::RegisterThread]  Random list for token "
                  << token << " is " << rand_list_str.str());
    m_TlsRandList->SetValue(rand_list, s_TlsCleanup< vector<int> >);

    m_TlsRandListPos->SetValue(new int, s_TlsCleanup<int>);
    *m_TlsRandListPos->GetValue() = 0;

    return true;
}


int CMonkey::GetSeed()
{
    /* save m_seedlog to file */
    return m_Seed;
}


void CMonkey::SetSeed(int seed)
{
    /* load m_seedlog from file */
    m_Seed = seed;
    LOG_POST(Info << "[CMonkey::SetSeed]  Chaos Monkey seed was manually "
                     "changed to: " << m_Seed);
}


int CMonkey::GetRand(const CMonkeySeedKey& /* key */)
{
    if (m_TlsToken->GetValue() == NULL) {
        return rand();
    }
    int& list_pos = *m_TlsRandListPos->GetValue();
    if (++list_pos == kRandCount) {
        list_pos = 0;
    }
    int next_list_pos = (list_pos + 1 == kRandCount) ? 0 : list_pos + 1;
    LOG_POST(Note << "[CMonkey::GetRand]  Getting random value "
                  << (*m_TlsRandList->GetValue())[list_pos]
                  << " for thread " << *m_TlsToken->GetValue()
                  << ". Next random value is "
                  << (*m_TlsRandList->GetValue())[next_list_pos]);
    return (*m_TlsRandList->GetValue())[list_pos];
}


END_NCBI_SCOPE

#endif /* #ifdef NCBI_MONKEY */

