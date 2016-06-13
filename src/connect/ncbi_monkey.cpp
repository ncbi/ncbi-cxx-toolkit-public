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
DEFINE_STATIC_FAST_MUTEX(s_SeedLogConfigMutex);
DEFINE_STATIC_FAST_MUTEX(s_SingletonMutex);
DEFINE_STATIC_FAST_MUTEX(s_KnownConnMutex);
static CSocket*          s_TimeoutingSock = NULL;
static CSocket*          s_PeerSock = NULL;
const  int               kRandCount = 100;
/* Registry names */
const string             kMonkeyMainSect = "CHAOS_MONKEY";
const string             kEnablField     = "enabled";
const string             kSeedField     = "seed";

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

#   endif /* NCBI_MONKEY_TESTS */
/*/////////////////////////////////////////////////////////////////////////////
//                    STATIC CONVENIENCE FUNCTIONS                           //
/////////////////////////////////////////////////////////////////////////////*/
static vector<string>& s_Monkey_Split(const string   &s, 
                                      char           delim,
                                      vector<string> &elems) 
{
    g_MonkeyMock_SetInterceptedPoll(false);
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
                            CMonkeyException::EErrCode::e_MonkeyUnknown,
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
    return config.Get(kMonkeyMainSect, "config");
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


static void s_GetSocketDestinations(MONKEY_SOCKTYPE sock,
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
    if (fqdn != NULL)
        *fqdn = CSocketAPI::gethostbyaddr   (addr);
    if (IP != NULL)
        *IP   = CSocketAPI::HostPortToString(addr, 0);
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
//                            CMonkeyException                                //
//////////////////////////////////////////////////////////////////////////////*/
const char* CMonkeyException::what() const throw()
{
    return m_Message.c_str();
}


/*//////////////////////////////////////////////////////////////////////////////
//                             CMonkeyRuleBase                                //
//////////////////////////////////////////////////////////////////////////////*/
CMonkeyRuleBase::CMonkeyRuleBase(EMonkeyActionType     action_type,
                                 const vector<string>& params)
    : m_ReturnStatus(-1), m_RepeatType(eMonkey_RepeatNone), m_Delay (0),
      m_RunsSize(0), m_ActionType(action_type)
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


static string s_PrintActionType(EMonkeyActionType action) {
    switch (action)
    {
    case eMonkey_Recv:
        return "recv()";
    case eMonkey_Send:
        return "send()";
    case eMonkey_Poll:
        return "poll()";
    case eMonkey_Connect:
        return "connect()";
    default:
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
            string("Unknown EMonkeyActionType value"));
    }
}


/** Check that the rule should trigger on this run */
bool CMonkeyRuleBase::CheckRun(MONKEY_SOCKTYPE sock)
{
    bool isRun = false;
    /* If "runs" is not set, rule always triggers */
    if (m_Runs.empty()) return true;

    /* In probability mode each item of m_Runs is a probability of each run */
    if (m_RunMode == eMonkey_RunProbability) {
        assert(m_RunPos[sock] <= m_Runs.size());
        if (m_RunPos[sock] == m_Runs.size()) {
            switch (m_RepeatType) {
            case eMonkey_RepeatNone:
                return false;
            case eMonkey_RepeatLast:
                m_RunPos[sock] = m_Runs.size() - 1;
                break;
            case eMonkey_RepeatAgain:
                m_RunPos[sock] = 0;
                break;
            }
        }
        int rand_val = CMonkey::Instance()->GetRand(Key());
        LOG_POST(Note << "[CMonkeyRuleBase::CheckRun]  Checking if the rule " 
                      << "for " << s_PrintActionType(m_ActionType) 
                      << " will be run this time. Random value is " 
                      << rand_val << ", probability threshold is " 
                      << m_Runs[m_RunPos[sock]] * 100);
        isRun = (rand_val % 100) < m_Runs[m_RunPos[sock]] * 100;
        LOG_POST(Note << "[CMonkeyRuleBase::CheckRun]  The rule will be " 
                      << (isRun ? "" : "NOT ") << "run");
    }
    /* In "number of the run" mode each item in m_Runs is a specific number of 
       run when the rule should trigger */
    else {
        if ((m_RunPos[sock] + 1) > *m_Runs.rbegin()) {
            switch (m_RepeatType) {
            case eMonkey_RepeatNone:
                return false;
            case eMonkey_RepeatLast:
                return true;
            case eMonkey_RepeatAgain:
                m_RunPos[sock] = 0;
                break;
            }
        }
        /* If m_Runs has the exact number of current run, the rule triggers */
        isRun = std::binary_search(m_Runs.begin(), 
                                   m_Runs.end(), m_RunPos[sock] + 1);
    }
    m_RunPos[sock]++;
    return isRun;
}


void CMonkeyRuleBase::x_ReadEIOStatus(const string& eIOStatus_str)
{
    string eIOStatus = (eIOStatus_str);
    NStr::ToLower(eIOStatus);
    if (eIOStatus == "eio_closed") {
        m_ReturnStatus = eIO_Closed;
    } else if (eIOStatus == "eio_invalidarg") {
        m_ReturnStatus = eIO_InvalidArg;
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
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
            string("Could not parse 'return_status': ") + eIOStatus_str);
    }
}


void CMonkeyRuleBase::x_ReadRuns(const string& runs)
{
    /* We get the string already without whitespaces and only have to
       split it on commas*/
    vector<string> runs_list = s_Monkey_Split(runs, ',');
    if (runs_list.size() == 0)
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
            string("\"Runs\" parameter is empty"));
    if (runs_list.size() == 1)
        runs_list.push_back("...");
    m_RunMode = runs_list[0][runs_list[0].length() - 1] == '%'
                ? eMonkey_RunProbability : eMonkey_RunNumber;

    m_RepeatType = eMonkey_RepeatNone;
    if ( *runs_list.rbegin() == "repeat" ) {
        m_RepeatType = eMonkey_RepeatAgain;
    } else if ( *runs_list.rbegin() == "..." ) {
        m_RepeatType = eMonkey_RepeatLast;
    }

    unsigned int end_pos = (m_RepeatType == eMonkey_RepeatNone)
                                    ? runs_list.size() : runs_list.size() - 1;
    for ( unsigned int i = 0;  i < end_pos;  i++ ) {
        string& num = runs_list[i];
        if (m_RunMode == eMonkey_RunProbability) {
            if (*num.rbegin() != '%') {
                throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
                        string("Value is not percentage: ") + num + 
                        string(", values have to be either only percentages or "
                               "only numbers of tries, not mixed"));
            }
            num = num.substr(0, num.length() - 1);
            m_Runs.push_back(NStr::StringToDouble(num) / 100);
        } else {
            if (*num.rbegin() == '%') {
                throw CMonkeyException(
                        CDiagCompileInfo(__FILE__, __LINE__),
                        NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
                        string("Value is percentage: ") + num + 
                        string(", values have to be either only percentages or "
                               "only numbers of tries, not mixed"));
            }
            int val = NStr::StringToInt(num);
            if (m_Runs.size() > 0 && val <= *m_Runs.rbegin()) {
                throw CMonkeyException(
                    CDiagCompileInfo(__FILE__, __LINE__),
                    NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
                    string("\"runs\" should contain values in "
                           "increasing order"));
            }
            m_Runs.push_back(val);
        }
    }
}


int /* EIO_Status or -1 */ CMonkeyRuleBase::GetReturnStatus()
{
    return m_ReturnStatus;
}


unsigned long CMonkeyRuleBase::GetDelay()
{
    return m_Delay;
}


/*//////////////////////////////////////////////////////////////////////////////
//                              CMonkeyRWRuleBase                             //
//////////////////////////////////////////////////////////////////////////////*/
CMonkeyRWRuleBase::CMonkeyRWRuleBase(EMonkeyActionType           action_type, 
                                     const vector<string>& params)
    : CMonkeyRuleBase(action_type, params), m_Text(""), m_TextLength(0), 
      m_Garbage(false), m_FillType(eMonkey_FillRepeat)
{
    for ( unsigned int i = 0;  i < params.size();  i++ ) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name = name_value[0];
        string value = name_value[1];
        if (name == "text") {
            if (GetReturnStatus() != eIO_Success && GetReturnStatus() != -1) {
                throw CMonkeyException(
                    CDiagCompileInfo(__FILE__, __LINE__),
                    NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
                    string("Return error status is set in Rule, cannot "
                           "set 'text' parameter"));
            }
            if (value == "garbage") {
                m_Garbage = true;
            }
            /*  The text should start and finish with ' or "  */
            else if ( value[0] == value[value.length() - 1] 
                  && (value[0] == '\'' || value[0] == '\"') )
            {
                m_Garbage = false;
                m_Text = value.substr(1, value.length() - 2);
            } else {
                throw CMonkeyException(
                    CDiagCompileInfo(__FILE__, __LINE__),
                    NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
                    string("Could not parse 'text' for Rule: ") + value);
            }
        } else if (name == "text_length") {
            m_TextLength = NStr::StringToInt(value);
        } else if (name == "fill") {
            if (value == "last_letter") {
                m_FillType = eMonkey_FillLastLetter;
            } else if (name == "repeat") {
                m_FillType = eMonkey_FillRepeat;
            }
        } 
    }
    /* Checking that everything was set */
    if (GetReturnStatus() == eIO_Success && m_Text == "") {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
            string("Parameter 'text' not set to a non-empty value for rule, "
                   "but 'return_status' set to eIO_Success requires 'text' to "
                   "be set"));
    }
    if (GetReturnStatus() == -1 && m_Text == "") {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
            string("Parameter 'text' not set to a non-empty value for rule, "
                   "but 'return_status' not set requires 'text' to "
                   "be set"));
    }
}


void CMonkeyRWRuleBase::x_ReadFill(const string& fill_str)
{
    string fill = fill_str;
    if (NStr::ToLower(fill) == "repeat") {
        m_FillType = eMonkey_FillRepeat;
    }
    if (NStr::ToLower(fill) == "last_letter") {
        m_FillType = eMonkey_FillLastLetter;
    }
}


string CMonkeyRWRuleBase::GetText()
{
    return m_Text;
}


size_t CMonkeyRWRuleBase::GetTextLength()
{
    return m_TextLength;
}


bool CMonkeyRWRuleBase::GetGarbage()
{
    return m_Garbage;
}


CMonkeyRWRuleBase::EFillType CMonkeyRWRuleBase::GetFillType()
{
    return m_FillType;
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyWriteRule
//////////////////////////////////////////////////////////////////////////
CMonkeyWriteRule::CMonkeyWriteRule(const vector<string>& params) 
    : CMonkeyRWRuleBase(eMonkey_Send, params)
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
    LOG_POST(Error << "[CMonkeyWriteRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED send()");
    int return_status = GetReturnStatus();
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
        default:
            break;
        }
    }

    /* We cannot write more than the user asked */
    size_t max_size;
    if (GetTextLength() == 0) {
        max_size = min(GetTextLength(), (size_t)size);
    }
    else {
        max_size = size;
    }
    /* Fill data */
    void* new_data = (char*)malloc(max_size * sizeof(char));
    if (GetGarbage()) {
        s_MONKEY_GenRandomString((char*)new_data, max_size);
    } else {
        string text = GetText();
        if (GetFillType() == CMonkeyRWRuleBase::eMonkey_FillRepeat) {
            text += text;
            text = text.substr(0, max_size);
        }
        if (GetFillType() == CMonkeyRWRuleBase::eMonkey_FillLastLetter)
            text.insert(text.length() - 1, max_size - text.length(),
                        text[text.length()-1]);
        memcpy(new_data, text.c_str(), max_size);
    }
    return send(sock, (const char*)new_data, size, flags);
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyReadRule
//////////////////////////////////////////////////////////////////////////
CMonkeyReadRule::CMonkeyReadRule(const vector<string>& params)
    : CMonkeyRWRuleBase(eMonkey_Recv, params)
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
    LOG_POST(Error << "[CMonkeyReadRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED recv()");
    int return_status = GetReturnStatus();
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
        default:
            break;
        }
    }

    if (size == 0) 
        return 0;

    /* So we decided to override */
    MONKEY_RETTYPE bytes_read = recv(sock, buf, size, flags);

    /* We cannot resize the buffer since it can be a local array, so we have to 
     * decrease monkey text length instead */
    size_t max_size = min(GetTextLength() - 1, static_cast<size_t>(size) - 1);
    /* Replace data */
    if (GetGarbage()) {
        s_MONKEY_GenRandomString((char*)buf, max_size);
    } else {
        string text = GetText();
        if (GetFillType() == CMonkeyRWRuleBase::eMonkey_FillRepeat) {
            text += text;
            text = text.substr(0, GetTextLength());
        }
        if (GetFillType() == CMonkeyRWRuleBase::eMonkey_FillLastLetter)
            text.insert(text.length()-1, GetTextLength() - text.length(), 
                        text[text.length()-1]);
        memcpy(buf, text.c_str(), GetTextLength());
    }

    return bytes_read;
}


//////////////////////////////////////////////////////////////////////////
// CMonkeyConnectRule
//////////////////////////////////////////////////////////////////////////

CMonkeyConnectRule::CMonkeyConnectRule(const vector<string>& params)
    : CMonkeyRuleBase(eMonkey_Connect, params)
{
    for (unsigned int i = 0; i < params.size(); i++) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name = name_value[0];
        string value = name_value[1];
        if (name == "allow") {
            m_Allow = ConnNetInfo_Boolean(value.c_str()) == 1;
        }
    }
}


int CMonkeyConnectRule::Run(MONKEY_SOCKTYPE        sock,
                            const struct sockaddr* name,
                            MONKEY_SOCKLENTYPE     namelen)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedConnect(true);
#endif /* NCBI_MONKEY_TESTS */
    LOG_POST(Error << "[CMonkeyConnectRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED connect()");
    int return_status = GetReturnStatus();
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
CMonkeyPollRule::CMonkeyPollRule(const vector<string>& params) 
    : CMonkeyRuleBase(eMonkey_Poll, params)
{
#ifdef NCBI_MONKEY_TESTS
    g_MonkeyMock_SetInterceptedPoll(true);
#endif /* NCBI_MONKEY_TESTS */
    for ( unsigned int i = 0;  i < params.size();  i++ ) {
        vector<string> name_value = s_Monkey_Split(params[i], '=');
        string name  = name_value[0];
        string value = name_value[1];
        if (name == "ignore") {
            m_Ignore = ConnNetInfo_Boolean(value.c_str()) == 1;
        }
    }
}


bool CMonkeyPollRule::Run(size_t*     n,
                          SOCK*       sock,
                          EIO_Status* return_status)
{
    LOG_POST(Error << "[CMonkeyPollRule::Run]  CHAOS MONKEY ENGAGE!!! "
                      "INTERCEPTED poll()");
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
    container.push_back(Rule_Ty(params));
    if ( multi_rule ) {
        int rule_num = 2;
        string val_name = rule_type_str + NStr::IntToString(rule_num++);
        while ( (rule_str = config.Get(section, val_name)) != "" ) {
            temp_conf = NStr::Replace(rule_str, string(" "), string(""));
            params = s_Monkey_Split(temp_conf, ';');
            container.push_back(Rule_Ty(params));
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
            CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
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
                CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
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
                        const string&  sock_url)
{
    /* Regex test just in case */
    static int regex_works = 1; /* 0 for not working, 1 for working */
#if 0
    if (regex_works == -1) {
        regex  test_regexp("test_regex"); /* should not match "test" */
        string test_string = "test.*";
        smatch test_find;
        if (!regex_match(test_string, test_find, test_regexp)) {
            regex_works = 0; /* we learn that regex implementation is fake */
        }
        regex_works = 1;
    }
#endif /* 0 */

    /* Check that regex works and that at least one of hostname and IP 
     * is not empty*/
    if ( regex_works && !m_HostRegex.empty() &&
            (sock_host.length() + sock_url.length() > 0) ) {
#ifdef NCBI_OS_MSWIN
        regex reg(m_HostRegex);
        smatch find;
        if (!regex_match(sock_host, find, reg) && 
            !regex_match(sock_url,  find, reg)) {
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
    int rand_val = CMonkey::Instance()->GetRand(Key());
    LOG_POST(Note << "[CMonkeyPlan::Match]  Checking if plan " << m_Name  
                  << " will be matched. Random value is " 
                  << rand_val << ", probability threshold is " 
                  << m_Probability);
    LOG_POST(Note << "[CMonkeyPlan::Match]  Plan " << m_Name << " was " 
                  << ((port_match && ((rand_val % 100) < m_Probability)) ? 
                      "" : "NOT ") << "matched");
    return port_match ? ((rand_val % 100) < m_Probability) : false;
}


bool CMonkeyPlan::WriteRule(MONKEY_SOCKTYPE        sock,
                            const MONKEY_DATATYPE  data,
                            MONKEY_LENTYPE         size,
                            int                    flags,
                            MONKEY_RETTYPE*        bytes_written,
                            SOCK*                  sock_ptr)
{
    for (unsigned int i = 0;  i < m_WriteRules.size();  i++) {
        if (m_WriteRules[i].CheckRun(sock)) {
            *bytes_written = m_WriteRules[i].Run(sock, data, size, flags, 
                                                 sock_ptr);
            return true;
        }
        // If this rule did not engage, we go to the next rule
    }
    // If no rules engaged, return 0
    return false;
}


bool CMonkeyPlan::ReadRule(MONKEY_SOCKTYPE        sock,
                           MONKEY_DATATYPE        buf,
                           MONKEY_LENTYPE         size,
                           int                    flags,
                           MONKEY_RETTYPE*        bytes_read,
                           SOCK*                  sock_ptr)
{
    for (unsigned int i = 0;  i < m_ReadRules.size();  i++) {
        if (m_ReadRules[i].CheckRun(sock)) {
            *bytes_read = m_ReadRules[i].Run(sock, buf, size, flags, sock_ptr);
            return true;
        }
        // If this rule did not engage, we go to the next rule
    }
    return false;
}


bool CMonkeyPlan::ConnectRule(MONKEY_SOCKTYPE        sock,
                              const struct sockaddr* name,
                              MONKEY_SOCKLENTYPE     namelen,
                              int*                   result)
{
    for (unsigned int i = 0;  i < m_ConnectRules.size();  i++) {
        /* Check if the rule will trigger on this run. If not - we go to the 
           next rule in plan */
        if (m_ConnectRules[i].CheckRun(sock)) {
            /* 'result' is the result of connect() launched in the rule. It can
               even be result of original connect() with real parameters.
               Or, it can be an error code of a failed fake connect() */
            *result = m_ConnectRules[i].Run(sock, name, namelen);
            return true;
        }
        // If this rule did not engage, we go to the next rule
    }
    // If no rules triggered
    return false;
}


bool CMonkeyPlan::PollRule(size_t*     n,
                           SOCK*       sock,
                           EIO_Status* return_status)
{
    for (unsigned int i = 0;  i < m_PollRules.size();  i++) {
        if (m_PollRules[i].CheckRun((*sock)->sock)) {
            return m_PollRules[i].Run(n, sock, return_status);
        }
        LOG_POST(Error << "[CMonkeyPlan::PollRule]  CHAOS MONKEY NOT "
                          "ENGAGED!!! poll() passed");
        // If this rule did not trigger, we go to the next rule
    }
    // If no rules triggered, return 0
    *return_status = EIO_Status::eIO_Success;
    return false;
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


CMonkey::CMonkey() : m_Probability(1.0), m_Enabled(false)
{
    if (sm_HookSwitch == NULL) {
        throw CMonkeyException(
            CDiagCompileInfo(__FILE__, __LINE__),
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
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
    assert(sm_HookSwitch == NULL);
    CFastMutexGuard spawn_guard(s_ConfigMutex);
    string          rules;
    string          monkey_section = config.empty() ? s_GetMonkeySection() : 
                                                      config;
    list<string>    sections;
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
    string probability = reg.Get(monkey_section, "probability");
    if (probability != "") {
        probability = NStr::Replace(probability, " ", "");
        auto arr = s_Monkey_Split(probability, '=');
        if (*probability.rbegin() != '%') {
            m_Probability = NStr::StringToDouble(probability) * 100;
        } else {
            probability = probability.substr(0, probability.length() - 1);
            m_Probability = NStr::StringToDouble(probability);
        }
    }
    /* Disable hooks while Monkey initializes */
    sm_HookSwitch(eMonkeyHookSwitch_Disabled);
    for (int i = 1;  ;  ++i) {
        string section = monkey_section + "_PLAN" + NStr::IntToString(i);
        if (find(sections.begin(), sections.end(), section) ==
            sections.end()) {
            break;
        }
        m_Plans.push_back(CMonkeyPlan(section));
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
    s_GetSocketDestinations(sock, &host_fqdn, &host_IP, NULL, &peer_port);
    CMonkeyPlan* sock_plan = x_FindPlan(sock, host_fqdn, host_IP, peer_port);

    if (sock_plan != NULL) {
        MONKEY_RETTYPE bytes_written;
        /* Plan may decide to leave connection untouched */
        if ( sock_plan->WriteRule(sock, data, size, flags, &bytes_written, 
                                  sock_ptr) )
            return bytes_written;
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
    s_GetSocketDestinations(sock, &host_fqdn, &host_IP, NULL, &peer_port);

    CMonkeyPlan* sock_plan = x_FindPlan(sock, host_fqdn, host_IP, peer_port);

    if (sock_plan != NULL) {
        MONKEY_RETTYPE bytes_read;
        if (sock_plan->ReadRule(sock, buf, size, flags, &bytes_read, sock_ptr))
            return bytes_read;
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
    addr.sa = *name;
    unsigned int host = addr.in.sin_addr.s_addr;
    string host_fqdn, host_IP;
    unsigned short peer_port = ntohs(addr.in.sin_port);
    host_fqdn = CSocketAPI::gethostbyaddr(host);
    host_IP = CSocketAPI::HostPortToString(host, 0);

    CMonkeyPlan* sock_plan = x_FindPlan(sock, host_fqdn, host_IP, peer_port);
    if (sock_plan != NULL) {
        int result;
        if ( sock_plan->ConnectRule(sock, name, namelen, &result) )
            return result;
    }
    return connect(sock, name, namelen);
}


bool CMonkey::Poll(size_t*      n,
                   SSOCK_Poll** polls,
                   EIO_Status*  return_status)
{
    size_t polls_iter       = 0;
    SSOCK_Poll* new_polls   = 
        static_cast<SSOCK_Poll*>(calloc(*n, sizeof(SSOCK_Poll)));
    int polls_count         = 0;
    while (polls_iter < *n) {
        SOCK&        sock      = (*polls)[polls_iter].sock;
        string       host_fqdn = CSocketAPI::gethostbyaddr(sock->host);
        string       host_IP   = CSocketAPI::HostPortToString(sock->host, 0);
        CMonkeyPlan* sock_plan = x_FindPlan(sock->id, "", "", sock->myport);
        if (sock_plan == NULL ||
            (sock_plan != NULL && !sock_plan->PollRule(n, &sock, return_status)) 
            ) {
            new_polls[polls_count++] = (*polls)[polls_iter];
        }
        polls_iter++;
    }
    *polls = new_polls;
    *n = polls_count;
    return false;
}


void CMonkey::Close(MONKEY_SOCKTYPE sock)
{
    auto sock_plan = m_KnownSockets.find(sock);
    if (sock_plan != m_KnownSockets.end()) {
        m_KnownSockets.erase(sock_plan);
    }
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
    int rand_val = CMonkey::Instance()->GetRand(Key());
    LOG_POST(Note << "[CMonkey::x_FindPlan]  Checking if connection will be "
                  << "intercepted by Chaos Monkey. Random value is " 
                  << rand_val << ", probability threshold is " 
                  << m_Probability);
    LOG_POST(Note << "[CMonkey::x_FindPlan]  The connection will be "
                  << ((rand_val % 100 >= m_Probability) ? "NOT " : "") 
                  << "processed.");
    if (rand_val % 100 >= m_Probability) {
        return NULL;
    }
    /* Now we can find a plan */
    bool match_found = false;
    for (unsigned int i = 0; i < m_Plans.size() && !match_found; i++) {
        /* Match includes probability of plan*/
        if (m_Plans[i].Match(host_IP, port, hostname)) {
            // 3. If found plan - use it and assign to this socket
            m_KnownSockets[sock] = &m_Plans[i];
            return m_KnownSockets[sock];
        }
    }
    /* If no plan triggered, then this socket will be always ignored */
    m_KnownSockets[sock] = NULL;
    return NULL;
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
            NULL, CMonkeyException::EErrCode::e_MonkeyInvalidArgs,
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

