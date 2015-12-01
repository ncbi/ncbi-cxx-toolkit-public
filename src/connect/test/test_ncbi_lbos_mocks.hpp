#ifndef CONNECT___TEST_NCBI_LBOS_MOCKS__HPP
#define CONNECT___TEST_NCBI_LBOS_MOCKS__HPP
/* ===========================================================================
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
 * Authors:  Dmitriy Elisov
 * @file
 * File Description:
 *   Common functions needed for conducting unit tests
 */

#include <corelib/ncbimtx.hpp>
#include "../ncbi_lbosp.h"

/** Get number of servers with specified IP announced in ZK  */
int                  s_CountServers(string         service,
                                    unsigned short port,
                                    string         dtab);

                                                               
template <unsigned int lines>
static EIO_Status    s_FakeReadAnnouncement(CONN           conn,
                                            void*          line,
                                            size_t         size,
                                            size_t*        n_read,
                                            EIO_ReadMethod how);
static
EIO_Status s_FakeReadAnnouncementWithErrorFromLBOS(CONN              conn,
                                                   void*             line,
                                                   size_t            size,
                                                   size_t*           n_read,
                                                   EIO_ReadMethod    how);

static string s_LBOS_hostport;
static string s_LBOS_header;

static
unsigned short s_FakeAnnounceEx(const char*     service,
                               const char*      version,
                               const char*      host,
                               unsigned short   port,
                               const char*      healthcheck_url,
                               char**           LBOS_answer,
                               char**           http_status_message)  
{
    s_LBOS_hostport = healthcheck_url;
    return kLBOSDNSResolveError;
}


/** Reset static counter in the beginning and in the end */
class CCounterResetter
{
public:
    explicit CCounterResetter(int& counter): m_Counter(counter) {
        m_Counter = 0;
    }
    ~CCounterResetter() {
        m_Counter = 0;
    }
private:
    CCounterResetter& operator=(const CCounterResetter&);
    CCounterResetter(const CCounterResetter&);
    int& m_Counter;
};

/** Create iter in the beginning and destroy in the end */
class CServIter 
{
public:
    explicit CServIter(SERV_ITER iter):m_Iter(iter) {}
    ~CServIter() {
        SERV_Close(m_Iter);
    }
    CServIter& operator=(const SERV_ITER iter) {
        if (m_Iter == iter)
            return *this;
        SERV_Close(m_Iter);
        m_Iter = iter;
        return *this;
    }
    SERV_ITER& operator*() {
        return m_Iter;
    }
    SERV_ITER& operator->() {
        return m_Iter;
    }
private:
    CServIter& operator=(const CServIter&);
    CServIter (const CServIter&);
    SERV_ITER m_Iter;
};


/** Create SConnNetInfo in the beginning and destroy in the end */
class CConnNetInfo 
{
public:
    CConnNetInfo() 
    {
        m_NetInfo = ConnNetInfo_Create(NULL);
    }
    explicit CConnNetInfo(const string& service) 
    {
        m_NetInfo = ConnNetInfo_Create(service.c_str());
    }
    ~CConnNetInfo() 
    {
        ConnNetInfo_Destroy(m_NetInfo);
    }
    SConnNetInfo*& operator*() 
    {
        return m_NetInfo;
    }
    SConnNetInfo*& operator->() 
    {
        return m_NetInfo;
    }
private:
    CConnNetInfo& operator=(const CConnNetInfo&);
    CConnNetInfo(const CConnNetInfo&);
    SConnNetInfo* m_NetInfo;
};

/** Replace original function with mock in the beginning, and revert
    changes in the end */
template<class T> 
class CMockFunction
{
public:
    CMockFunction(T& original, T mock)
        : m_OriginalValue(original), m_Original(original)
    {
        original = mock;
    }
    ~CMockFunction() {
        m_Original = m_OriginalValue;
    }
    void Restore() {
        m_Original = m_OriginalValue;
    }
    CMockFunction& operator=(const T mock) {
        m_Original = mock;
        return *this;
    }
private:
    T  m_OriginalValue;
    T& m_Original;
    CMockFunction& operator=(const CMockFunction&);
    CMockFunction(const CMockFunction&);
};


/** Replace original C-string with mock in the beginning, and revert
    changes in the end */
class CMockString
{
public:
    CMockString(char*& original, const char* mock)
        : m_OriginalValue(original), m_Original(original) 
    {
        original = strdup(mock);
    }
    ~CMockString() {
        free(m_Original);
        m_Original = m_OriginalValue;
    }
    CMockString& operator=(const char* mock) {
        free(m_Original);
        m_Original = strdup(mock);
        return *this;
    }
private:
    char*  m_OriginalValue;
    char*& m_Original;
    CMockString& operator=(const CMockString&);
    CMockString(const CMockString&);
};


/** Set LBOS status to needed values, then revert */
class CLBOSStatus
{
public:
    CLBOSStatus(bool init_status, bool power_status)
        : m_OriginalInitStatus(*g_LBOS_UnitTesting_InitStatus() != 0),
          m_OriginalPowerStatus(*g_LBOS_UnitTesting_PowerStatus() != 0)
    {
        *g_LBOS_UnitTesting_InitStatus() = init_status;
        *g_LBOS_UnitTesting_PowerStatus() = power_status;
    }
    ~CLBOSStatus() {
        *g_LBOS_UnitTesting_InitStatus() = m_OriginalInitStatus;
        *g_LBOS_UnitTesting_PowerStatus() = m_OriginalPowerStatus;
    }

private:
    CLBOSStatus& operator=(const CLBOSStatus&);
    CLBOSStatus(const CLBOSStatus&);
    bool m_OriginalInitStatus;
    bool m_OriginalPowerStatus;
};


/** Hold a non-const C-object (malloc, free). 
 *  Assignment free()'s current object */
template <class T>
class CCObjHolder
{
public:
    CCObjHolder(T* obj):m_Obj(obj)
    {
    }
    CCObjHolder& operator=(T* obj) {
        if (m_Obj == obj) 
            return *this;
        free(m_Obj);
        m_Obj = obj;
        return *this;
    }
    ~CCObjHolder() {
        free(m_Obj);
    }
    T*& operator*() {
        return m_Obj;
    }
    T*& Get() {
        return m_Obj;
    }
    T*& operator->() {
        return m_Obj;
    }
private:
    CCObjHolder& operator=(const CCObjHolder&);
    CCObjHolder(const CCObjHolder&);
    T* m_Obj;
};


/** Hold non-const C-style (malloc, free) array terminating with NULL element. 
 *  Assignment free()'s current C-array */
template<class T>
class CCObjArrayHolder
{
public:
    CCObjArrayHolder(T** arr) :m_Arr(arr)
    {
    }
    CCObjArrayHolder& operator=(T** arr) {
        if (m_Arr == arr)
            return *this;
        if (m_Arr != NULL) {
            for (size_t i = 0; m_Arr[i] != NULL; i++) {
                free(m_Arr[i]);
            }
            free(m_Arr);
        }
        m_Arr = arr;
        return *this;
    }
    ~CCObjArrayHolder() {
        if (m_Arr != NULL) {
            for (size_t i = 0; m_Arr[i] != NULL; i++) {
                free(m_Arr[i]);
            }
            free(m_Arr);
        }
    }
    T**& operator*() {
        return m_Arr;
    }
    T*& operator[] (unsigned int i) {
        return m_Arr[i];
    }
    void set(unsigned int i, T* val) {
        free(m_Arr[i]);
        m_Arr[i] = val;
    }
    size_t count() {
        size_t i = 0;
        if (m_Arr != NULL) {
            for (i = 0; m_Arr[i] != NULL; i++) continue;
        }
        return i;
    }
private:
    CCObjArrayHolder& operator=(const CCObjArrayHolder&);
    CCObjArrayHolder(const CCObjArrayHolder&);
    T** m_Arr;
};


/** Hold non-const C-string. Assignment free()'s current C-string */
class CLBOSRoleDomain
{
public:
    CLBOSRoleDomain()
        : m_Role(*g_LBOS_UnitTesting_CurrentRole()),
          m_Domain(*g_LBOS_UnitTesting_CurrentDomain()),
          m_MockRoleFile(NULL),
          m_MockDomainFile(NULL)
    {    }
    ~CLBOSRoleDomain()
    {    
        free(*g_LBOS_UnitTesting_CurrentRole());
        free(*g_LBOS_UnitTesting_CurrentDomain());
        free(m_MockRoleFile);
        free(m_MockDomainFile);
        *g_LBOS_UnitTesting_CurrentRole()   = NULL;
        *g_LBOS_UnitTesting_CurrentDomain() = NULL;
        g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles("/etc/ncbi/role",
                                                     "/etc/ncbi/domain");
    }
    void setRole(string roleFile) {
        free(*g_LBOS_UnitTesting_CurrentRole());
        *g_LBOS_UnitTesting_CurrentRole() = NULL;
        free(m_MockRoleFile);
        m_MockRoleFile = strdup(roleFile.c_str());
        g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(m_MockRoleFile, NULL);
    }
    void setDomain(string domainFile) {
        free(*g_LBOS_UnitTesting_CurrentDomain());
        *g_LBOS_UnitTesting_CurrentDomain() = NULL;
        free(m_MockDomainFile);
        m_MockDomainFile = strdup(domainFile.c_str());
        g_LBOS_UnitTesting_SetLBOSRoleAndDomainFiles(NULL, m_MockDomainFile);
    }
private:
    char* m_Role;            /* pointer to static variable       */
    char* m_Domain;          /* pointer to static variable       */
    char* m_MockRoleFile;    /* current value of static variable */
    char* m_MockDomainFile;  /* current value of static variable */
};


static
EIO_Status s_FakeReadAnnouncementWithErrorFromLBOS(CONN             conn,
                                                   void*            line,
                                                   size_t           size,
                                                   size_t*          n_read,
                                                   EIO_ReadMethod   how)
{
    static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
        ->http_response_code = 507;
    static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
        ->http_status_mesage = strdup("LBOS STATUS");
    const char* error = "Those lbos errors are scaaary";
    strncpy(static_cast<char*>(line), error, size - 1);
    *n_read = strlen(error);
    return eIO_Closed;
}


static
EIO_Status s_FakeReadAnnouncementSuccessWithCorruptOutputFromLBOS
(CONN           conn,
 void*          line,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
        ->http_response_code = 200;
    const char* error = "Corrupt output";
    strncpy(static_cast<char*>(line), error, size - 1);
    *n_read = strlen(error);
    return eIO_Closed;
}


static
EIO_Status s_FakeReadWithErrorFromLBOSCheckDTab(CONN conn,
                                                void* line,
                                                size_t size,
                                                size_t* n_read,
                                                EIO_ReadMethod how)
{
    SLBOS_UserData * user_data = 
        static_cast<SLBOS_UserData*>(CONN_GetUserData(conn));
    user_data->http_response_code = 400;
    s_LBOS_header = 
        string(static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))->header);
    return eIO_Closed;
}


template <unsigned int lines>
static
EIO_Status s_FakeReadAnnouncement(CONN           conn,
                                  void*          line,
                                  size_t         size,
                                  size_t*        n_read,
                                  EIO_ReadMethod how)
{
    static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
        ->http_response_code = 200;
    int localscope_lines = lines; /* static analysis and compiler react 
                                strangely to template parameter in equations */
    if (s_CallCounter++ < localscope_lines) {
        char* buf = "1.2.3.4:5";
        strncat(static_cast<char*>(line), buf, size-1);
        *n_read = strlen(buf);
        return eIO_Success;
    } else {
        *n_read = 0;
        return eIO_Closed;
    }
}


template<CLBOSException::EErrCode kErrCode, unsigned short kStatusCode>
class ExceptionComparator
{
public:
    ExceptionComparator (string expected_message = "") 
        : m_ExpectedMessage(expected_message) 
    {
    }

    bool operator()(const CLBOSException& ex)
    {
        CLBOSException::EErrCode err_code = kErrCode; /* for debug */
        if (ex.GetErrCode() != err_code) {
            ERR_POST("Exception has code " << ex.GetErrCode() << " and " <<
                      err_code << " is expected");
            return false;
        }
        unsigned short status_code = kStatusCode; /* for debug */
        if (ex.GetStatusCode() != status_code) {
            ERR_POST("Exception has status code " << ex.GetStatusCode() << 
                     " and " << status_code << " is expected");
            return false;
        }
        const char* ex_message = ex.what();
        ex_message = strstr(ex_message, "Error: ") + strlen("Error: ");
        string message;
        message.append(ex_message);
        if (message != m_ExpectedMessage) {
            ERR_POST("Exception has message \"" << ex.GetStatusCode() << 
                     "\" and \"" << m_ExpectedMessage << "\" is expected");
            return false;
        }
        return true;
    }
private:
    string m_ExpectedMessage;
};


/** Check how many specified servers are announced 
 * (usually it is 0 or 1)
 * This function does not care about host (IP) of server.
 */
int s_CountServers(string service, unsigned short port, string dtab = "")
{
    int servers = 0;
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
    return servers;
}



template <unsigned int lines>
static 
EIO_Status           s_FakeReadDiscoveryCorrupt   (CONN, void*,
                                                   size_t, size_t*,
                                                   EIO_ReadMethod);


template<size_t count>
static void          s_FakeFillCandidates            (SLBOS_Data*  data,
                                                      const char*  service);
static void          s_FakeFillCandidatesWithError   (SLBOS_Data*,
                                                      const char*);
static SSERV_Info**  s_FakeResolveIPPort             (const char*,
                                                      const char*,
                                                      SConnNetInfo*);
static void          s_FakeInitialize                (void);
static void          s_FakeInitializeCheckInstances  (void);
static void          s_FakeFillCandidatesCheckInstances
                                                     (SLBOS_Data*  data,
                                                      const char*  service);
template<int instance_number>
static SSERV_Info**  s_FakeResolveIPPortSwapAddresses(const char*  lbos_address,
                                                      const char*  serviceName,
                                                      SConnNetInfo*net_info);

/* 0 - error, 1 - success */
template <bool success, int a1, int a2, int a3, int a4>
static unsigned int s_FakeGetLocalHostAddress        (ESwitch reget);


template<int response_code>
static
EHTTP_HeaderParse    s_LBOS_FakeParseHeader(const char*     header,
                                            void* /*int**/  response,
                                            int             server_error)
{
    int* response_output = static_cast<int*>(response);
    int code = response_code; 
    /* check for empty document */
    if (response_output != NULL) {
        *response_output = code;
    return eHTTP_HeaderSuccess;
    } else {
        return eHTTP_HeaderError;
    }

}

static string s_GenerateNodeName(void)
{
    return string("/lbostest");
}

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);

/** Given number of threads, it divides port range in 34 pieces (as we know,
 * there can be maximum of 34 threads) and gives ports only from corresponding
 * piece of range. If -1 is provided as thread_num, then full range is used
 * for port generation                                                       */
static unsigned short s_GeneratePort(int thread_num = -1)
{
    static int cur = static_cast<int>(time(0));

    {{
        CFastMutexGuard spawn_guard(s_GlobalLock);
        cur = cur * 1049 + 3571;
    }}
    unsigned short port;
    if (thread_num == -1) {
        port = abs(cur % 65534) + 1;
    } else {
        port = abs(cur % (65534 / kThreadsNum)) + 
                  (65534 / kThreadsNum) * thread_num + 1;
    }
    CORE_LOGF(eLOG_Trace, ("Port %hu for thread %d", port, thread_num));
    return port;
}




#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
/** Accepts requests on specified socket.
    Returns result - if managed or not to answer */
static 
bool s_AnswerHealthcheck(CListeningSocket& listening_sock, short int port) {
    CSocket sock;
    STimeout sock_timeout = { 1, 500 };
    if (listening_sock.Accept(sock, &sock_timeout) != eIO_Success) {
        return false;
    }
    char buf[4096];
    size_t n_read = 0;
    size_t n_written = 0;
    sock.SetTimeout(eIO_ReadWrite, &sock_timeout);
    sock.SetTimeout(eIO_Close, &sock_timeout);
    sock.Read(buf, sizeof(buf), &n_read);
    const char healthy_answer[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 4\r\n"
        "Content-Type: text/plain;charset=UTF-8\r\n"
        "\r\n"
        "OK\r\n";
    sock.Write(healthy_answer, sizeof(healthy_answer) - 1, &n_written);
    sock.Wait(eIO_Read, &sock_timeout);
    sock.Close();
    return true;
}
#endif /* NCBI_THREADS */


#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
/** If single-threaded mode, instead of just read, make first flush,
 * then answer healthcheck, and then get answer from LBOS (hopefully,
 * OK)
 *  If multi-threaded mode, then just read, and healthcheck will be answered
 *  in the special thread. */
static
EIO_Status s_RealReadAnnounce(CONN              conn,
                              void*             line,
                              size_t            size,
                              size_t*           n_read,
                              EIO_ReadMethod    how)
{
    EIO_Status status = eIO_Timeout;
    size_t total_read = 0;
    CListeningSocket listening_sock(4096);
    CSocket sock;
    listening_sock.Listen(4096);
    CONN_Flush(conn); //Send request for announcement
    s_AnswerHealthcheck(listening_sock, 4096); // answer the healthcheck
    do { // Get answer from LBOS after it received response for healthcheck
        status = CONN_Read(conn, (char*) line + total_read,
                           size - total_read, n_read, how);
        total_read += *n_read;
    } while (total_read < size && status == eIO_Success);
    *n_read = total_read;
    return status;
}
#endif /* NCBI_THREADS */


static
unsigned short s_LBOS_Announce(const char*             service,
                               const char*             version,
                               const char*             host,
                               unsigned short&         port,
                               const char*             healthcheck_url,
                               /* lbos_answer is never NULL  */
                               char**                  lbos_answer,
                               char**                  http_status_message) 
{ 

    int announce_result = 0;
#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, s_RealReadAnnounce);
#endif /* NCBI_THREADS */
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
    do {
#endif
        announce_result =
            LBOS_Announce(service, version, host, port, healthcheck_url,
                          lbos_answer, http_status_message);
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
        if (announce_result != 200) {
            port++;
        }
    } while (announce_result != 200);
#endif
    return announce_result;
}

static
unsigned short s_LBOS_AnnounceReg(const char*             registry_section,
                                  /* lbos_answer is never NULL  */
                                  char**                  lbos_answer,
                                  char**                  http_status_message) 
{ 

    int announce_result = 0;
#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read, s_RealReadAnnounce);
#endif /* NCBI_THREADS */
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
    do {
#endif
        announce_result =
            LBOS_AnnounceFromRegistry(registry_section, lbos_answer, 
                                      http_status_message); 
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
        if (announce_result != 200) {
            port++;
        }
    } while (announce_result != 200);
#endif
    return announce_result;
}

static
void s_LBOS_CPP_AnnounceReg(const string& registry_section)
{

#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_RealReadAnnounce);
#endif /* NCBI_THREADS */

#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
    bool success;
    do {
        try {
            success = true;
#endif
            LBOS::AnnounceFromRegistry(registry_section);
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                          success (remove this hack when LBOS is fixed) */
        }
        catch(...) {
            success = false;
        }
    } while (!success);
#endif
}

static
void s_LBOS_CPP_Announce(const string&   service,
                         const string&   version,
                         const string&   host,
                         unsigned short& port,
                         const string&   healthcheck_url)
{

#ifndef NCBI_THREADS // If we have to answer healthchech in the same thread
    CMockFunction<FLBOS_ConnReadMethod*> mock(
        g_LBOS_UnitTesting_GetLBOSFuncs()->Read,
        s_RealReadAnnounce);
#endif /* NCBI_THREADS */

#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
        success (remove this hack when LBOS is fixed) */
    bool success;
    do {
        try {
            success = true;
#endif
            LBOS::Announce(service, version, host, port,
                healthcheck_url);
#ifdef QUICK_AND_DIRTY /* If we announce many times on different ports until
                success (remove this hack when LBOS is fixed) */
        }
        catch (...) {
            success = false;
        }
    } while (!success);
#endif
}

/* Emulate a lot of records to be sure that algorithm can take so much.
 * The IP number start with zero (octal format), which makes some of
 * IPs with digits more than 7 invalid */
template <unsigned int lines> 
static
EIO_Status s_FakeReadDiscoveryCorrupt(CONN              conn,
                                      void*             line,
                                      size_t            size,
                                      size_t*           n_read,
                                      EIO_ReadMethod    how)
{
    static unsigned int bytesSent = 0;
    /* Generate string */
    static string buf = "";
    if (buf == "") {
        ostringstream oss;
        unsigned int local_lines = lines;
        for (unsigned int i = 0; i < local_lines; i++) {
            oss << "STANDALONE 0" << i + 1 << ".0" <<
                i + 2 << ".0" <<
                i + 3 << ".0" <<
                i + 4 << ":" << (i + 1) * 215 <<
                " Regular R=1000.0 L=yes T=30\n";
        }
        buf = oss.str();
    }
    unsigned int len = buf.length();
    if (bytesSent < len) {
        static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
            ->http_response_code = 200;
        int length_before = strlen(static_cast<char*>(line));
        strncat(static_cast<char*>(line), buf.c_str() + bytesSent, size - 1);
        int length_after = strlen(static_cast<char*>(line));
        *n_read = length_after - length_before + 1;
        bytesSent += *n_read - 1;
        return eIO_Success;
    } else {
        bytesSent = 0;
        *n_read = 0;
        return eIO_Closed;
    }
}



/* Emulate a lot of records to be sure that algorithm can take so much */
template <unsigned int lines>
static
EIO_Status s_FakeReadDiscovery(CONN            conn,
                               void*           line,
                               size_t          size,
                               size_t*         n_read,
                               EIO_ReadMethod  how)
{
    static unsigned int bytesSent = 0;
    /* Generate string */
    static string buf = "";
    if (buf == "") {
        stringstream oss;
        unsigned int localscope_lines = lines;
        for (unsigned int i = 0; i < localscope_lines; i++) {
            oss << "STANDALONE " << i + 1 << "." <<
                i + 2 << "." <<
                i + 3 << "." <<
                i + 4 << ":" << (i + 1) * 215 <<
                " Regular R=1000.0 L=yes T=30\n";
        }
        buf = oss.str();
    }
    unsigned int len = buf.length();
    if (bytesSent < len) {
        static_cast<SLBOS_UserData*>(CONN_GetUserData(conn))
            ->http_response_code = 200;
        int length_before = strlen(static_cast<char*>(line));
        strncat(static_cast<char*>(line), buf.c_str() + bytesSent, size-1);
        int length_after = strlen(static_cast<char*>(line));
        *n_read = length_after - length_before + 1;
        bytesSent += *n_read-1;
        return eIO_Success;
    } else {
        bytesSent = 0;
        *n_read = 0;
        return eIO_Closed;
    }
}


static
EIO_Status s_FakeReadEmpty(CONN conn,
                           void* line,
                           size_t size,
                           size_t* n_read,
                           EIO_ReadMethod how)
{
    *n_read = 0;
    return eIO_Timeout;
}


template<size_t count>
static void s_FakeFillCandidates (SLBOS_Data* data,
                           const char* service)
{
    s_CallCounter++;
    size_t localscope_count = count; /* for debugging */
    unsigned int host = 0;
    unsigned short int port = 0;
    data->a_cand = count+1;
    data->n_cand = count;
    data->pos_cand = 0;
    SLBOS_Candidate* realloc_result = static_cast<SLBOS_Candidate*>
        (realloc(data->cand, sizeof(SLBOS_Candidate)*data->a_cand));

    NCBITEST_CHECK_MESSAGE(realloc_result != NULL,
                           "Problem with memory allocation, "
                           "calloc failed. Not enough RAM?");
    if (realloc_result == NULL) return;
    data->cand = realloc_result;
    ostringstream hostport;
    for (unsigned int i = 0;  i < localscope_count;  i++) {
        hostport.str("");
        hostport << i + 1 << "." << i + 2 << "." << i + 3 << "." << i + 4
                 << ":" << (i + 1) * 210;
        SOCK_StringToHostPort(hostport.str().c_str(), &host, &port);
        data->cand[i].info = static_cast<SSERV_Info*>(
                calloc(sizeof(SSERV_Info), 1));
        NCBITEST_CHECK_MESSAGE(data->cand[i].info != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (data->cand[i].info == NULL) return;
        data->cand[i].info->host = host;
        data->cand[i].info->port = port;
    }
}


static void s_FakeFillCandidatesCheckInstances(SLBOS_Data* data,
    const char* service)
{
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_InstancesList() != NULL,
        "s_LBOS_InstancesList is empty at a critical place");
    s_FakeFillCandidates<2>(data, service);
}


static void s_FakeFillCandidatesWithError (SLBOS_Data* data, const char* service)
{
    s_CallCounter++;
    return;
}


/** This version works only on specific call number to emulate working and 
 * non-working lbos instances.
 * template @param instance_number
 *  Number of "good" instance.
 *  -1 for all instances OFF                                                 */
template<int instance_number>
static SSERV_Info** s_FakeResolveIPPortSwapAddresses(const char*   lbos_address,
                                              const char*   serviceName,
                                              SConnNetInfo* net_info)
{
    int localscope_instance_number = instance_number; /*to debug */
    s_CallCounter++;
    CCObjArrayHolder<char> original_list(g_LBOS_GetLBOSAddresses());
    NCBITEST_CHECK_MESSAGE(*original_list != NULL,
        "Problem with g_LBOS_GetLBOSAddresses, test failed.");
    if (*original_list == NULL || localscope_instance_number == -1) return NULL;
    SSERV_Info** hostports = NULL;
    if (strcmp(lbos_address, original_list[localscope_instance_number-1]) == 0) 
    {
        if (net_info->http_user_header) {
            s_LastHeader = net_info->http_user_header;
        }
        hostports = static_cast<SSERV_Info**>(calloc(sizeof(SSERV_Info*), 2));
        NCBITEST_CHECK_MESSAGE(hostports != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (hostports == NULL) return NULL;
        hostports[0] = static_cast<SSERV_Info*>(calloc(sizeof(SSERV_Info), 2));
        NCBITEST_CHECK_MESSAGE(hostports[0] != NULL,
                               "Problem with memory allocation, "
                               "calloc failed. Not enough RAM?");
        if (hostports[0] == NULL) return NULL;
        unsigned int host = 0;
        unsigned short int port = 0;
        SOCK_StringToHostPort("127.0.0.1:80", &host, &port);
        hostports[0]->host = host;
        hostports[0]->port = port;
        hostports[1] = NULL;
    }
    return hostports;
}


template <bool success, int a1, int a2, int a3, int a4>
static unsigned int s_FakeGetLocalHostAddress(ESwitch reget)
{
    stringstream ss;
    int localscope_success = success;
    unsigned int host;
    ss << a1 << "." << a2 << "." << a3 << "." << a4;
    unsigned short port;
    if (localscope_success) {
        /* We know for sure that buffer is large enough */
        SOCK_StringToHostPort(ss.str().c_str(), &host, &port);
        return host;
    }
    return 0;
}

static string s_GetMyHost() {
    //char hostname[200];
    string host = CSocketAPI::gethostbyaddr(0);
    //SOCK_gethostname(hostname, 200);
    return host; /* will be converted to string */
}

static string s_GetMyIP() {
    unsigned int local_addr_int = SOCK_GetLocalHostAddress(eOn);
    char local_addr_cstr[25]; 
    SOCK_HostPortToString(local_addr_int, 1, local_addr_cstr, 25);
    string local_addr_str(local_addr_cstr);
    local_addr_str.resize(local_addr_str.find(':')); /* remove port */
    return local_addr_str; 
}

static void s_FakeInitialize()
{
    s_CallCounter++;
    return;
}


static void s_FakeInitializeCheckInstances()
{
    s_CallCounter++;
    NCBITEST_CHECK_MESSAGE(g_LBOS_UnitTesting_InstancesList() != NULL,
        "s_LBOS_InstancesList is empty at a critical place");
    return;
}



static SSERV_Info** s_FakeResolveIPPort (const char*   lbos_address,
                                  const char*   serviceName,
                                  SConnNetInfo* net_info)
{
    s_CallCounter++;
    if (net_info->http_user_header) {
        s_LastHeader = net_info->http_user_header;
    }
    if (s_CallCounter < 2) {
        return NULL;
    } else {
        SSERV_Info** hostports = static_cast<SSERV_Info**>(
                calloc(sizeof(SSERV_Info*), 2));
        NCBITEST_CHECK_MESSAGE(hostports != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (hostports == NULL) return NULL;
        hostports[0] = static_cast<SSERV_Info*>(calloc(sizeof(SSERV_Info), 1));
        NCBITEST_CHECK_MESSAGE(hostports[0] != NULL,
            "Problem with memory allocation, "
            "calloc failed. Not enough RAM?");
        if (hostports[0] == NULL) return NULL;
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
 * will return specified lbos address and just check that the function is
 * called. Original function is thoroughly tested in another test module.*/
static char* s_FakeComposeLBOSAddress()
{
    return strdup("lbos.foo");
}


#endif /* #ifndef CONNECT___TEST_NCBI_LBOS_MOCKS__HPP */
