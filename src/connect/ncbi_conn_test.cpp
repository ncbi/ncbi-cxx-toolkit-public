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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   NCBI connectivity test suite.
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_comm.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <corelib/ncbiutil.hpp>
#include <corelib/stream_utils.hpp>
#include <connect/ncbi_conn_test.hpp>
#include <connect/ncbi_socket.hpp>
#include <iterator>
#include <stdlib.h>

#define _STR(x)           #x
#define STRINGIFY(x)  _STR(x)

#define HELP_EMAIL    (m_Email.empty()                                    \
                       ? string("NCBI Help Desk <info@ncbi.nlm.nih.gov>") \
                       : m_Email)

#define NCBI_FWD_BEMD  "130.14.29.112"
#define NCBI_FWD_STVA  "165.112.7.12"


BEGIN_NCBI_SCOPE


static const SIZE_TYPE kParIndent = 4;

static const char kTest[] = "test";
static const char kCanceled[] = "Check canceled";

static const char kFWSign[] =
    "NCBI Firewall Daemon:  Invalid ticket.  Connection closed.";


inline static bool operator > (const STimeout* t1, const STimeout& t2)
{
    if (!t1)
        return true;
    return
        t1->sec + t1->usec / (double) kMicroSecondsPerSecond >
        t2.sec  + t2.usec  / (double) kMicroSecondsPerSecond;
}


inline static bool x_Large(const STimeout* t)
{
    return t > g_NcbiDefConnTimeout;
}


CConnTest::CConnTest(const STimeout* timeout,
                     CNcbiOstream* output, SIZE_TYPE width)
    : m_Output(output), m_Width(width),
      m_HttpProxy(false), m_Stateless(false), m_Firewall(false), m_End(false)
{
    SetTimeout(timeout);
}


void CConnTest::SetTimeout(const STimeout* timeout)
{
    if (timeout) {
        x_Timeout = timeout == kDefaultTimeout ? g_NcbiDefConnTimeout : *timeout;
        m_Timeout = &x_Timeout;
    } else
        m_Timeout = kInfiniteTimeout/*0*/;
}


EIO_Status CConnTest::Execute(EStage& stage, string* reason)
{
    typedef EIO_Status (CConnTest::*FCheck)(string* reason);
    FCheck check[] = {
        NULL,
        &CConnTest::HttpOkay,
        &CConnTest::DispatcherOkay,
        &CConnTest::ServiceOkay,
        &CConnTest::GetFWConnections,
        &CConnTest::CheckFWConnections,
        &CConnTest::StatefulOkay,
        &CConnTest::x_CheckTrap  // Guaranteed to fail
    };

    // Reset everything
    m_HttpProxy = m_Stateless = m_Firewall = m_End = false;
    m_Fwd.clear();
    if (reason)
        reason->clear();
    m_CheckPoint.clear();

    int s = eHttp;
    EIO_Status status;
    do {
        if ((status = (this->*check[s])(reason)) != eIO_Success) {
            stage = EStage(s);
            break;
        }
    } while (EStage(s++) < stage);

    if (status != eIO_Success  &&  status != eIO_Interrupt)
        ExtraCheckOnFailure();
    return status;
}


string CConnTest::x_TimeoutMsg(void)
{
    if (!m_Timeout)
        return kEmptyStr;
    char tmo[40];
    int n = ::sprintf(tmo, "%u", m_Timeout->sec);
    if (m_Timeout->usec)
        ::sprintf(tmo + n, ".%06u", m_Timeout->usec);
    string result("Make sure the specified timeout value of ");
    result += tmo;
    result += "s is adequate for your network throughput\n";
    return result;
}


EIO_Status CConnTest::ConnStatus(bool failure, CConn_IOStream* io)
{
    EIO_Status status;
    string type = io ? io->GetType()        : kEmptyStr;
    string text = io ? io->GetDescription() : kEmptyStr;
    m_CheckPoint = (type
                    + (!type.empty()  &&  !text.empty() ? "; " : "") +
                    text);
    if (!failure)
        return eIO_Success;
    if (!io)
        return eIO_Unknown;
    if (!io->GetCONN())
        return eIO_Closed;
    if ((status = io->Status())         != eIO_Success  ||
        (status = io->Status(eIO_Open)) != eIO_Success) {
        return status;
    }
    EIO_Status r_status = io->Status(eIO_Read);
    EIO_Status w_status = io->Status(eIO_Write);
    status = r_status > w_status ? r_status : w_status;
    return status == eIO_Success ? eIO_Unknown : status;
}


static SConnNetInfo* ConnNetInfo_Create(const char*    svc_name,
                                        EDebugPrintout dbg_printout)
{
    SConnNetInfo* net_info = ::ConnNetInfo_Create(svc_name);
    if (net_info  &&  (EDebugPrintout) net_info->debug_printout < dbg_printout)
        net_info->debug_printout = dbg_printout;
    return net_info;
}


static inline bool x_IsFatalError(int error)
{
    return error == 400  ||  error == 403  ||  error == 404 ? true : false;
}


struct SAuxData {
    const ICanceled* m_Canceled;
    bool             m_Redirect;
    bool             m_Failed;
    void*            m_Data;

    SAuxData(const ICanceled* canceled, void* data)
        : m_Canceled(canceled), m_Redirect(false),
          m_Failed(false), m_Data(data)
    { }
};


extern "C" {

static EHTTP_HeaderParse s_AnyHeader(const char* /*header*/,
                                     void* data, int server_error)
{
    SAuxData* auxdata = reinterpret_cast<SAuxData*>(data);
    _ASSERT(auxdata);
    if (x_IsFatalError(server_error))
        auxdata->m_Failed = true;
    return eHTTP_HeaderContinue;
}


static EHTTP_HeaderParse s_GoodHeader(const char* /*header*/,
                                      void* data, int server_error)
{
    SAuxData* auxdata = reinterpret_cast<SAuxData*>(data);
    _ASSERT(auxdata);
    if (server_error)
        auxdata->m_Failed = server_error != 301  &&  server_error != 302;
    return eHTTP_HeaderSuccess;
}


static EHTTP_HeaderParse s_SvcHeader(const char* header,
                                     void* data, int server_error)
{
    SAuxData* auxdata = reinterpret_cast<SAuxData*>(data);
    _ASSERT(auxdata);
    if (x_IsFatalError(server_error))
        auxdata->m_Failed = true;
    *((int*) auxdata->m_Data) =
        !server_error  &&  NStr::FindNoCase(header, "\nService: ") != NPOS
        ? 1
        : 2;
    return eHTTP_HeaderSuccess;
}


static int/*bool*/ s_Adjust(SConnNetInfo*, void* data, unsigned int count)
{
    SAuxData* auxdata = reinterpret_cast<SAuxData*>(data);
    _ASSERT(auxdata);
    if (!count/*redirect*/)
        auxdata->m_Redirect = true;
    return auxdata->m_Failed
        ||  (auxdata->m_Canceled  &&  auxdata->m_Canceled->IsCanceled())
        ? 0/*false*/ : 1/*true*/;
}


static void s_Cleanup(void* data)
{
    SAuxData* auxdata = reinterpret_cast<SAuxData*>(data);
    _ASSERT(auxdata);
    delete auxdata;
}

} // extern "C"


template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


EIO_Status CConnTest::ExtraCheckOnFailure(void)
{
    static const STimeout kTimeout   = { 5,                           0 };
    static const STimeout kTimeSlice = { 0, kMicroSecondsPerSecond / 10 };
    static const struct {
        EURLScheme scheme;
        const char*  host;
        const char* vhost;
    } kTests[] = {
        // 0. NCBI default
        { eURL_Http,
          0,                            0                      }, // NCBI
        // 1. External server(s)
        { eURL_Https,
          "www.google.com",             0                      },
        // 2. NCBI servers, explicitly
        { eURL_Https,
          "www.be-md.ncbi.nlm.nih.gov", "www.ncbi.nlm.nih.gov" }, // NCBI main
        { eURL_Https,
          "www.st-va.ncbi.nlm.nih.gov", "www.ncbi.nlm.nih.gov" }  // NCBI colo
    };

    m_CheckPoint.clear();
    PreCheck(eNone, 0/*main*/, "Failback HTTP access check");

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(0, eDebugPrintout_Data));
    if (!net_info.get()) {
        PostCheck(eNone, 0/*main*/,
                  eIO_Unknown, "Unable to create network info structure");
        return eIO_Unknown;
    }

    net_info->req_method = eReqMethod_Head;
    net_info->timeout    = &kTimeout;
    net_info->max_try    = 0;
    m_Timeout = 0;

    CDeadline deadline(kTimeout.sec, kTimeout.usec * (kNanoSecondsPerSecond /
                                                      kMicroSecondsPerSecond));
    time_t           sec;
    unsigned int nanosec;
    deadline.GetExpirationTime(&sec, &nanosec);
    ::sprintf(net_info->path, "/NcbiTest%08lX%08lX",
              (unsigned long) sec, (unsigned long) nanosec);

    size_t n;
    vector< AutoPtr<CConn_HttpStream> > http;
    for (n = 0;  n < sizeof(kTests) / sizeof(kTests[0]);  ++n) {
        char user_header[80];
        net_info->scheme  = kTests[n].scheme;
        const char* host  = kTests[n].host;
        const char* vhost = kTests[n].vhost;
        if (host) {
            _ASSERT(::strlen(host) < sizeof(net_info->host) - 1);
            unsigned int ip = vhost ? CSocketAPI::gethostbyname(host) : 0;
            ::strcpy(net_info->host, ip ? CSocketAPI::ntoa(ip).c_str() : host);
        }
        if (vhost) {
            _ASSERT(::strlen(vhost) + 6 < sizeof(user_header) - 1);
            ::sprintf(user_header, "Host: %s", vhost);
        } else
            *user_header = '\0';
        SAuxData* auxdata = new SAuxData(m_Canceled, 0);
        http.push_back(new CConn_HttpStream(net_info.get(), 
                                            user_header, s_AnyHeader,
                                            auxdata, s_Adjust, s_Cleanup));
        http.back()->SetCanceledCallback(m_Canceled);
    }
    net_info.reset();

    EIO_Status status = eIO_Success;
    do {
        if (!http.size())
            break;
        ERASE_ITERATE(vector< AutoPtr<CConn_HttpStream> >, h, http) {
            CONN conn = (*h)->GetCONN();
            if (!conn) {
                VECTOR_ERASE(h, http);
                if (status == eIO_Success)
                    status  = eIO_Unknown;
                continue;
            }
            EIO_Status readst = CONN_Wait(conn, eIO_Read, &kTimeSlice);
            if (readst > eIO_Timeout) {
                if (readst == eIO_Interrupt) {
                    status  = eIO_Interrupt;
                    break;
                }
                if (status < readst  &&  (*h)->GetStatusCode() != 404)
                    status = readst;
                VECTOR_ERASE(h, http);
                continue;
            }
        }
    } while (status != eIO_Interrupt  &&  !deadline.IsExpired());

    if (status == eIO_Success  &&  http.size() == n)
        status  = eIO_Timeout;

    PostCheck(eNone, 0/*main*/, status, kEmptyStr);

    return status;
}


EIO_Status CConnTest::HttpOkay(string* reason)
{
    PreCheck(eHttp, 0/*main*/,
             "Checking whether NCBI is HTTP(S) accessible");

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(0, m_DebugPrintout));
    if (!net_info.get()) {
        PostCheck(eNone, 0/*main*/,
                  eIO_Unknown, "Unable to create network info structure");
        return eIO_Unknown;
    }

    net_info->scheme = eURL_Http;
    if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port)
        m_HttpProxy = true;
    // Make sure there are no extras
    ConnNetInfo_SetUserHeader(net_info.get(), 0);
    ConnNetInfo_SetArgs(net_info.get(), 0);

    SAuxData* auxdata = new SAuxData(m_Canceled, 0);
    CConn_HttpStream http("/Service/index.html",
                          net_info.get(), kEmptyStr/*user_hdr*/, s_GoodHeader,
                          auxdata, s_Adjust, s_Cleanup, fHTTP_AdjustOnRedirect,
                          m_Timeout);
    http.SetCanceledCallback(m_Canceled);
    string temp;
    http >> temp;
    EIO_Status status = ConnStatus(temp.empty(), &http);

    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK";
    else {
        if (status == eIO_Timeout)
            temp = x_TimeoutMsg();
        else
            temp.clear();
        if (auxdata->m_Redirect) {
            // Redirect to https:// is expected
            temp += "Your HTTP connection appears to work but it does not seem"
                " to be able to upgrade to secure HTTP (HTTPS), most likely"
                " because of a malfunctioning HTTP proxy server in the way\n";
        } else {
            bool host = NStr::CompareNocase(net_info->host, DEF_CONN_HOST) !=0;
            bool port = net_info->port != 0;
            if (host  ||  port) {
                int n = 0;
                temp += "Make sure that ";
                if (host) {
                    n++;
                    temp += "[" DEF_CONN_REG_SECTION "]" REG_CONN_HOST "=\"";
                    temp += net_info->host;
                    temp += port ? "\"" : "\" and ";
                }
                if (port) {
                    n++;
                    temp += "[" DEF_CONN_REG_SECTION "]" REG_CONN_PORT "=\"";
                    temp += NStr::UIntToString(net_info->port);
                    temp += '"';
                }
                _ASSERT(n);
                temp += n > 1 ? " are" : " is";
                temp += " redefined correctly\n";
            }
        }
        bool auth = true;
        if (m_HttpProxy) {
            temp += "Make sure that the HTTP proxy server \'";
            temp += net_info->http_proxy_host;
            temp += ':';
            temp += NStr::UIntToString(net_info->http_proxy_port);
            temp += "' specified with ";
            if (net_info->http_proxy_only) {
                temp += "environment variable $http_proxy or $HTTP_PROXY";
                auth = false;
            } else
                temp += "[" DEF_CONN_REG_SECTION "]HTTP_PROXY_{HOST|PORT}";
            temp += " is correct";
        } else {
            if (net_info->http_proxy_host[0]  ||  net_info->http_proxy_port) {
                temp += "Note that your HTTP proxy seems to have been"
                    " specified only partially, and thus cannot be used: the ";
                if (net_info->http_proxy_port) {
                    temp += "host part is missing (for port :"
                        + NStr::UIntToString(net_info->http_proxy_port);
                } else {
                    temp += "port part is missing (for host \'"
                        + string(net_info->http_proxy_host) + '\'';
                }
                temp += ")\n";
            }
            temp += "If your network access requires the use of an HTTP proxy"
                " server, please contact your network administrator, and set"
                " [" DEF_CONN_REG_SECTION "]{HTTP_PROXY_{HOST|PORT} (both must"
                " be set) in your configuration accordingly";
        }
        if (auth) {
            temp += "; and if your proxy server requires authorization, please"
                " check that appropriate ["
                DEF_CONN_REG_SECTION "]HTTP_PROXY_{USER|PASS} have been set\n";
        } else
            temp += '\n';
        if (*net_info->user  ||  *net_info->pass) {
            temp += "Make sure there are no stray values for ["
                DEF_CONN_REG_SECTION "]{" REG_CONN_USER "|" REG_CONN_PASS
                "} in your configuration -- NCBI services neither require"
                " nor use them\n";
        }
        if (status == eIO_NotSupported)
            temp += "Your application may also need to have SSL support on\n";
    }
    net_info.reset();

    PostCheck(eHttp, 0/*main*/, status, temp);

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::DispatcherOkay(string* reason)
{
    PreCheck(eDispatcher, 0/*main*/,
             "Checking whether NCBI dispatcher is okay");

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(0, m_DebugPrintout));
    if (ConnNetInfo_SetupStandardArgs(net_info.get(), kTest))
        net_info->scheme = eURL_Https;

    int okay = 0;
    SAuxData* auxdata = new SAuxData(m_Canceled, &okay);
    CConn_HttpStream http(net_info.get(), kEmptyStr/*user_hdr*/, s_SvcHeader,
                          auxdata, s_Adjust, s_Cleanup, 0/*flags*/, m_Timeout);
    http.SetCanceledCallback(m_Canceled);
    char buf[1024];
    http.read(buf, sizeof(buf));
    CTempString str(buf, (size_t) http.gcount());
    EIO_Status status = ConnStatus
        (okay != 1  ||
         NStr::FindNoCase(str, "NCBI Dispatcher Test Page") == NPOS  ||
         NStr::FindNoCase(str, "Welcome") == NPOS, &http);

    string temp;
    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK";
    else {
        if (status != eIO_Timeout) {
            if (okay) {
                temp = "Make sure there are no stray values for ["
                    DEF_CONN_REG_SECTION "]{" REG_CONN_HOST "|"
                    REG_CONN_PORT "|" REG_CONN_PATH "} in your"
                    " configuration\n";
            }
            if (okay == 1) {
                temp += "Service response was not recognized; please contact "
                    + HELP_EMAIL + '\n';
            }
        } else
            temp += x_TimeoutMsg();
        if (!(okay & 1)) {
            temp += "Check with your network administrator that your network"
                " neither filters out nor blocks non-standard HTTP headers\n";
        }
        if (net_info.get()  &&  status == eIO_NotSupported)
            temp += "NCBI network dispatcher must be accessed via HTTPS\n";
    }
    net_info.reset();

    PostCheck(eDispatcher, 0/*main*/, status, temp);

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::ServiceOkay(string* reason)
{
    static const char kService[] = "bounce";

    PreCheck(eStatelessService, 0/*main*/,
             "Checking whether NCBI services operational");

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(kService,
                                                      m_DebugPrintout));
    if (net_info.get())
        net_info->lb_disable = 1/*no local LB to use even if available*/;

    CConn_ServiceStream svc(kService, fSERV_Stateless, net_info.get(),
                            0/*extra*/, m_Timeout);
    svc.SetCanceledCallback(m_Canceled);

    svc << kTest << NcbiEndl;
    string temp;
    svc >> temp;
    bool responded = temp.size() > 0 ? true : false;
    EIO_Status status = ConnStatus(NStr::Compare(temp, kTest) != 0, &svc);

    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK";
    else {
        char* str = net_info.get() ? SERV_ServiceName(kService) : 0;
        if (str  &&  NStr::CompareNocase(str, kService) == 0) {
            free(str);
            str = 0;
        }
        string mapper;
        SERV_ITER iter = SERV_OpenSimple(kService);
        const char* x_mapper = SERV_MapperName(iter);
        if (x_mapper)
            mapper = x_mapper;
        if (!iter  ||  !SERV_GetNextInfo(iter)) {
            // Service not found
            SERV_Close(iter);
            iter = SERV_OpenSimple(kTest);
            if (!iter  ||  !SERV_GetNextInfo(iter)
                ||  NStr::CompareNocase(SERV_MapperName(iter), "DISPD") != 0) {
                // Make sure there will be a mapper error printed
                SERV_Close(iter);
                temp.clear();
                iter = 0;
            } else {
                // kTest service can be located but not kService
                temp = str ? "Substituted service" : "Service";
                temp += " cannot be located";
            }
        } else {
            temp = responded ? "Unrecognized" : "No";
            temp += " response from ";
            temp += str ? "substituted service" : "service";
        }
        if (!temp.empty()) {
            if (str) {
                temp += "; please remove [";
                string upper(kService);
                temp += NStr::ToUpper(upper);
                temp += "]" REG_CONN_SERVICE_NAME "=\"";
                temp += str;
                temp += "\" from your configuration\n";
            } else if (status != eIO_Timeout  ||  x_Large(m_Timeout))
                temp += "; please contact " + HELP_EMAIL + '\n';
        }
        if (status != eIO_Timeout) {
            x_mapper = SERV_MapperName(iter);
            if (!x_mapper  ||  NStr::CompareNocase(x_mapper, "DISPD") != 0) {
                temp += "Network dispatcher is not enabled as a service"
                    " locator;  please review your configuration to purge any"
                    " occurrences of [" DEF_CONN_REG_SECTION "]"
                    REG_CONN_DISPD_DISABLE " from your settings\n";
                if (!mapper.empty()  &&  mapper != (x_mapper ? x_mapper : "")){
                    temp += "It appears that \"" + mapper + "\" is currently"
                        " set as your default service locator\n";
                }
            }
        } else
            temp += x_TimeoutMsg();
        SERV_Close(iter);
        if (str)
            free(str);
    }
    net_info.reset();

    PostCheck(eStatelessService, 0/*main*/, status, temp);

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::x_GetFirewallConfiguration(const SConnNetInfo* net_info)
{
    static const char kFWDUrl[] = "/IEB/ToolBox/NETWORK/fwd_check.cgi";

    char fwdurl[128];
    if (!ConnNetInfo_GetValueInternal(0, "FWD_URL",
                                      fwdurl, sizeof(fwdurl), kFWDUrl)) {
        return eIO_InvalidArg;
    }
    SAuxData* auxdata = new SAuxData(m_Canceled, 0);
    CConn_HttpStream fwdcgi(fwdurl, net_info, kEmptyStr/*ushdr*/, s_GoodHeader,
                            auxdata, s_Adjust, s_Cleanup, 0/*flg*/, m_Timeout);
    fwdcgi.SetCanceledCallback(m_Canceled);
    fwdcgi << "selftest" << NcbiEndl;

    char line[256];
    bool responded = false;
    while (fwdcgi.getline(line, sizeof(line))) {
        responded = true;
        CTempString hostport, state;
        if (!NStr::SplitInTwo(line, "\t", hostport, state))
            continue;
        bool fb;
        if (NStr::Compare(state, 0, 3, "FB-") == 0) {
            state = state.substr(3);
            fb = true;
        } else
            fb = false;
        bool okay;
        if      (NStr::CompareNocase(state, 0, 2, "OK")   == 0)
            okay = true;
        else if (NStr::CompareNocase(state, 0, 4, "FAIL") == 0)
            okay = false;
        else
            continue;
        CFWConnPoint cp;
        if (!CSocketAPI::StringToHostPort(hostport, &cp.host, &cp.port))
            continue;
        if (!fb  &&
            (( m_Firewall  &&  !(CONN_FWD_PORT_MIN <= cp.port
                                 &&  cp.port <= CONN_FWD_PORT_MAX))  ||
             (!m_Firewall  &&  !(4444 <= cp.port  &&  cp.port <= 4544)))) {
            fb = true;
        }
        if ( fb  &&  net_info  &&  net_info->firewall == eFWMode_Firewall)
            continue;
        if (!fb  &&  net_info  &&  net_info->firewall == eFWMode_Fallback)
            continue;
        cp.status = okay ? eIO_Success : eIO_NotSupported;
        if (!fb)
            m_Fwd.push_back(cp);
        else
            m_FwdFB.push_back(cp);
    }

    return ConnStatus(!responded || (fwdcgi.fail() && !fwdcgi.eof()), &fwdcgi);
}


EIO_Status CConnTest::GetFWConnections(string* reason)
{
    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(0, m_DebugPrintout));
    if (ConnNetInfo_SetupStandardArgs(net_info.get(), 0/*w/o service*/)) {
        const char* user_header;
        net_info->scheme     = eURL_Https;
        net_info->req_method = eReqMethod_Post;
        if (net_info->firewall) {
            user_header = "NCBI-RELAY: FALSE";
            m_Firewall = true;
        } else
            user_header = "NCBI-RELAY: TRUE";
        if (net_info->stateless)
            m_Stateless = true;
        ConnNetInfo_OverrideUserHeader(net_info.get(), user_header);
    }

    string temp(m_Firewall ? "FIREWALL" : "RELAY (legacy)");
    temp += " connection mode has been detected for stateful services\n";
    if (m_Firewall) {
        temp += "This mode requires your firewall to be configured in such a"
            " way that it allows outbound connections to the port range ["
            STRINGIFY(CONN_FWD_PORT_MIN) ".." STRINGIFY(CONN_FWD_PORT_MAX)
            "] (inclusive) at the two fixed NCBI addresses, "
            NCBI_FWD_BEMD " and " NCBI_FWD_STVA ".\n"
            "To set that up correctly, please have your network administrator"
            " read the following (if they have not already done so):"
            " " CONN_FWD_URL "\n";
    } else {
        temp += "This is an obsolescent mode that requires keeping a wide port"
            " range [4444..4544] (inclusive) open to let through connections"
            " to the entire NCBI site (130.14.xxx.xxx/165.112.xxx.xxx) -- this"
            " mode was designed for unrestricted networks when firewall port"
            " blocking had not been an issue\n";
    }
    if (m_Firewall) {
        _ASSERT(net_info.get());
        switch (net_info->firewall) {
        case eFWMode_Adaptive:
            temp += "Also, there are usually a few additional ports such as "
                STRINGIFY(CONN_PORT_SSH) " and " STRINGIFY(CONN_PORT_HTTPS)
                " at " NCBI_FWD_BEMD ", which can be used if connections to"
                " the ports in the range described above, have failed\n";
            break;
        case eFWMode_Firewall:
            temp += "Furthermore, your configuration explicitly forbids to use"
                " any fallback firewall ports that may exist to improve"
                " reliability of connection experience\n";
            break;
        case eFWMode_Fallback:
            temp += "There are usually a few backup connection ports such as "
                STRINGIFY(CONN_PORT_SSH) " and " STRINGIFY(CONN_PORT_HTTPS)
                " at " NCBI_FWD_BEMD ", which can be used as a failover if"
                " connections to the port range above fail.  However, your "
                " configuration explicitly requests that only those fallback"
                " firewall ports (if any exist) are to be used for"
                " connections:  this also implies that no conventional ports"
                " from the default range will be used\n";
            break;
        default:
            temp += "Internal program error, please report!\n";
            _ASSERT(0);
            break;
        }
    } else {
        temp += "This mode may not be reliable if your site has a restraining"
            " firewall imposing a fine-grained control over which hosts and"
            " ports the outbound connections are allowed to use\n";
    }
    if (m_HttpProxy  &&  net_info.get()  &&  !net_info->http_proxy_only) {
        temp += "Connections to the aforementioned ports will be made via an"
            " HTTP proxy at '";
        temp += net_info->http_proxy_host;
        temp += ':';
        temp += NStr::UIntToString(net_info->http_proxy_port);
        temp += "'";
        if (net_info->http_proxy_leak) { 
            temp += ".  If that is unsuccessful, a link bypassing the proxy"
                " will then be attempted";
        }
    }
    temp += '\n';

    PreCheck(eFirewallConnPoints, 0/*main*/, temp);

    PreCheck(eFirewallConnPoints, 1/*sub*/,
             "Obtaining current NCBI " +
             string(m_Firewall ? "firewall settings" : "service entries"));

    EIO_Status status = x_GetFirewallConfiguration(net_info.get());

    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success) {
        bool empty;
        if (!m_Fwd.empty()
            ||  (!m_FwdFB.empty()
                 &&  m_Firewall  &&  (net_info->firewall & eFWMode_Adaptive))){
            empty = m_Fwd.empty()  &&  !m_FwdFB.empty();
            temp = "OK: ";
            if (!empty) {
                temp += NStr::UInt8ToString(m_Fwd.size());
                stable_sort(m_Fwd.begin(),   m_Fwd.end());
            }
            size_t up = m_FwdFB.size();
            if (up) {
                stable_sort(m_FwdFB.begin(), m_FwdFB.end());
                if (!empty)
                    temp += " + ";
                temp += NStr::UInt8ToString(up);
                size_t down = 0;
                ITERATE(vector<CConnTest::CFWConnPoint>, cp, m_FwdFB) {
                    if (cp->status != eIO_Success)
                        ++down;
                }
                if (down) {
                    temp += " - " + NStr::UInt8ToString(down);
                    up -= down;
                }
            }
            up += m_Fwd.size();
            if (up) {
                temp += up == 1 ? " port" : " ports";
                empty = false;
            } else
                empty = true;
        } else
            empty = true;
        if (empty) {
            status = eIO_Unknown;
            temp = "No connection ports found, please contact " + HELP_EMAIL;
        }
    } else if (status == eIO_Timeout) {
        temp = x_TimeoutMsg();
        if (x_Large(m_Timeout))
            temp += "You may want to contact " + HELP_EMAIL;
    } else
        temp = "Please contact " + HELP_EMAIL;

    PostCheck(eFirewallConnPoints, 1/*sub*/, status, temp);

    net_info.reset();

    if (status == eIO_Success) {
        PreCheck(eFirewallConnPoints, 2/*sub*/,
                 "Verifying configuration for consistency");

        temp.resize(2);
        bool firewall = true;
        // Only check primary ports here
        ITERATE(vector<CConnTest::CFWConnPoint>, cp, m_Fwd) {
            if (cp->port < CONN_FWD_PORT_MIN  ||  CONN_FWD_PORT_MAX < cp->port)
                firewall = false;
            if (cp->status != eIO_Success) {
                temp = CSocketAPI::HostPortToString(cp->host, cp->port)
                    + " is not operational, ";
                if (!m_Firewall  ||  m_FwdFB.empty()) {
                    status = cp->status;
                    temp += "please contact " + HELP_EMAIL;
                } else {
                    temp += "trying to use fallback port(s) only";
                    temp.insert(0, "OK\n");
                    m_Fwd.clear();
                }
                break;
            }
        }
        if (status == eIO_Success) {
            if (!m_Firewall  &&  !m_FwdFB.empty()) {
                status = eIO_Unknown;
                temp = "Fallback port(s) found in non-firewall mode,"
                    " please contact " + HELP_EMAIL;
            } else if (m_Firewall != firewall) {
                status = eIO_Unknown;
                temp  = "Firewall ";
                temp += firewall ? "wrongly" : "not";
                temp += " acknowledged, please contact " + HELP_EMAIL;
            }
        }

        PostCheck(eFirewallConnPoints, 2/*sub*/, status, temp);
    }

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::CheckFWConnections(string* reason)
{
    static const STimeout kZeroTmo = { 0, 0 };

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(0, m_DebugPrintout));

    string temp("Checking individual connection points..\n");
    if (net_info.get()) {
        if (net_info->firewall != eFWMode_Fallback) {
            temp += "NOTE that even though that not the entire port range may"
                " currently be utilized and checked, in order for NCBI"
                " services to work correctly and seamlessly, your network must"
                " support all ports in the range as documented above\n";
        }
        if (net_info->firewall & eFWMode_Adaptive) {
            temp += net_info->firewall == eFWMode_Fallback
                ? "Fallback" : "Also, adaptive",
            temp += " firewall mode allows that some (but not all) fallback"
                " firewall ports may fail to operate.  Only those ports found"
                " in working order will be used to access NCBI services\n";
        }
        net_info->path[0] = '\0';
        net_info->timeout = m_Timeout;
    }

    SERV_InitFirewallPorts();

    PreCheck(eFirewallConnections, 0/*main*/, temp);

    vector<CFWConnPoint>* fwd[] = { &m_Fwd, &m_FwdFB };

    size_t n;
    bool url = false;
    unsigned int m = 0;
    EIO_Status status = net_info.get() ? eIO_Unknown : eIO_InvalidArg;
    for (n = 0;  net_info.get()  &&  n < sizeof(fwd) / sizeof(fwd[0]);  ++n) {
        if (fwd[n]->empty())
            continue;
        status = eIO_Success;

        typedef pair<AutoPtr<CConn_IOStream>, CFWConnPoint*> CFWCheck;
        vector<CFWCheck> fwck;

        // Spawn connections for all non-failing CPs
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, *fwd[n]) {
            if (m_Canceled.NotNull()  &&  m_Canceled->IsCanceled()) {
                status = eIO_Interrupt;
                break;
            }
            AutoPtr<CConn_IOStream> fw;
            if (cp->status != eIO_Success)
                cp->status  = eIO_Unknown;
            else if (status == eIO_Success  ||  n) {
                SOCK_ntoa(cp->host, net_info->host, sizeof(net_info->host));
                net_info->port = cp->port;
                fw.reset(new CConn_SocketStream(*net_info.get(),
                                                "\r\n"/*data*/, 2/*size*/,
                                                fSOCK_LogDefault, &kZeroTmo));
                if (!fw->good()
                    /* NB: eIO_Success assures there's an underlying CONN */
                    ||  fw->SetTimeout(eIO_Close, &kZeroTmo) != eIO_Success) {
                    EIO_Status st = fw->Status();
                    if (st == eIO_Success)
                        st  = eIO_InvalidArg;
                    cp->status = st;
                    if (!n)
                        status = st;
                } else
                    fw->SetCanceledCallback(m_Canceled);
            }
            fwck.push_back(make_pair(fw, &*cp));
        }

        // Check results randomly but let modify status no more than once
        unsigned int valid = 0;
        do {
            NON_CONST_ITERATE(vector<CFWCheck>, ck, fwck) {
                CConn_IOStream* fw = ck->first.get();
                CFWConnPoint*   cp = ck->second;
                if (!fw  ||  cp->status != eIO_Success)
                    continue;
                CONN conn = fw->GetCONN();
                if (status != eIO_Success  &&  !n) {
                    size_t drain;
                    CONN_SetTimeout(conn, eIO_Read, &kZeroTmo);
                    CONN_Read(conn, 0, 1<<20, &drain, eIO_ReadPlain);
                    continue;
                }
                EIO_Status st = CONN_Wait(conn, eIO_Read, &kZeroTmo);
                if (st == eIO_Timeout)
                    continue;
                if (st != eIO_Success) {
                    cp->status = st;
                    if (!n)
                        status = st;
                    // NB: ck->first retained
                    continue;
                }
                char line[sizeof(kFWSign) + 2/*"\r\0"*/];
                if (!fw->getline(line, sizeof(line)))
                    cp->status = ConnStatus(true, fw);
                else if (NStr::strncasecmp(line,kFWSign,sizeof(kFWSign)-1)!=0)
                    cp->status = eIO_NotSupported;
                else
                    ++valid;
                if (cp->status != eIO_Success  &&  !n)
                    status = cp->status;
                // NB: ck->first deleted
                ck->first.reset();
            }
            if (status != eIO_Success)
                break;
            vector<CSocketAPI::SPoll>  poll;
            ITERATE(vector<CFWCheck>, ck, fwck) {
                CConn_IOStream* fw = ck->first.get();
                if (!fw  ||  ck->second->status != eIO_Success)
                    continue;
                poll.push_back(CSocketAPI::SPoll(&fw->GetSocket(), eIO_Read));
            }
            if (!poll.size())
                break;
            status = CSocketAPI::Poll(poll, net_info->timeout);
            if (status != eIO_Timeout)
                continue;
            ITERATE(vector<CFWCheck>, ck, fwck)
                ck->second->status = eIO_Timeout;
            if (n) {
                status = eIO_Success;
                break;
            }
        } while (status == eIO_Success);

        // Report results:
        // when status != eIO_Success report only first entry that failed;
        // when status == eIO_Success report all entries (failed or not).
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, *fwd[n]) {
            if (status == eIO_Interrupt)
                cp->status = eIO_Interrupt;

            if (status != eIO_Success  &&  cp->status == eIO_Success)
                continue;

            PreCheck(eFirewallConnections, ++m, "Connectivity at "
                     + CSocketAPI::HostPortToString(cp->host, cp->port));

            size_t k;
            switch (cp->status) {
            case eIO_Success:
                if (n  &&  (net_info->firewall & eFWMode_Adaptive)
                    &&  !SERV_AddFirewallPort(cp->port)) {
                    m_CheckPoint = "Invalid firewall port";
                    cp->status = eIO_Unknown;
                    if (valid)
                        valid--;
                }
                break;
            case eIO_Timeout:
                m_CheckPoint = "Connection timed out";
                break;
            case eIO_Closed:
#ifdef NCBI_COMPILER_WORKSHOP
                k = 0;
                distance(fwd[n]->begin(), cp, k);
#else
                k = distance(fwd[n]->begin(), cp);
#endif // NCBI_COMPILER_WORKSHOP
                if (!fwck[k].first.get())
                    m_CheckPoint = "Connection closed";
                else
                    m_CheckPoint = "Connection refused";
                break;
            case eIO_Unknown:
                m_CheckPoint = "Unknown error occurred";
                break;
            case eIO_Interrupt:
                m_CheckPoint = "Connection interrupted";
                break;
            case eIO_InvalidArg:
                m_CheckPoint = "Cannot open connection";
                break;
            case eIO_NotSupported:
                m_CheckPoint = "Unrecognized response received";
                break;
            default:
                m_CheckPoint = "Internal program error, please report!";
                break;
            }
            if (status == eIO_Interrupt)
                temp = kCanceled;
            else if (status == eIO_Success)
                temp = cp->status == eIO_Success ? "OK" : kEmptyStr;
            else if  (n  ||  m_FwdFB.empty()) {
                if (status == eIO_Timeout)
                    temp = x_TimeoutMsg();
                else
                    temp.clear();
                if (m_HttpProxy  &&  !(net_info->http_proxy_only |
                                       net_info->http_proxy_leak)) {
                    temp += "Your HTTP proxy '";
                    temp += net_info->http_proxy_host;
                    temp += ':';
                    temp += NStr::UIntToString(net_info->http_proxy_port);
                    temp += "' may not allow connections relayed to ";
                    temp += n ? "fallback" : "non-conventional";
                    temp += " ports; please get in touch with your network"
                        " administrator";
                    if (!url) {
                        temp += " and let them read: " CONN_FWD_URL;
                        url = true;
                    }
                    temp += '\n';
                }
                temp += "The network port required for this connection to"
                    " succeed may be blocked/diverted at your firewall;";
                if (m_Firewall) {
                    temp += " please contact your network administrator";
                    if (!url) {
                        temp += " and have them read the following: "
                            CONN_FWD_URL;
                        url = true;
                    }
                    temp += '\n';
                } else {
                    temp += " try setting [" DEF_CONN_REG_SECTION "]"
                        REG_CONN_FIREWALL "=1 in your configuration to use a"
                        " more narrow port range";
                    if (!url) {
                        temp += " per: " CONN_FWD_URL;
                        url = true;
                    }
                    temp += '\n';
                }
                if (!m_Stateless) {
                    temp +=  "Not all NCBI stateful services require to work"
                        " over dedicated (persistent) connections; some can"
                        " still work (at the cost of degraded performance)"
                        " over a stateless carrier such as HTTP;  try setting"
                        " [" DEF_CONN_REG_SECTION "]" REG_CONN_STATELESS "=1"
                        " in your configuration\n";
                }
            } else
                temp = "Now checking fallback port(s)...";

            PostCheck(eFirewallConnections, m,
                      status == eIO_Success  ||  n ? cp->status : status,
                      temp);

            if (status != eIO_Success)
                break;
        }
        if (status == eIO_Interrupt)
            break;
        if (status == eIO_Success  &&  n  &&  !valid)
            status  = eIO_NotSupported;
        if (status == eIO_Success  ||  m_FwdFB.empty())
            break;
    }

    if (status != eIO_Success  ||  n) {
        bool note = !url;
        if (status != eIO_Success) {
            temp = m_Firewall ? "Firewall port" : "Service entry";
            temp += " check FAILED";
        } else {
            _ASSERT(net_info);
            temp = "Firewall port check PASSED";
            if (!m_Fwd.empty()  &&  net_info->firewall != eFWMode_Fallback) {
                temp += " only with fallback port(s)";
                m_Fwd.clear();
            } else
                note = false;
        }
        if (note  &&  status != eIO_Interrupt) {
            temp += "\nYou may want to read this link for more information: "
                CONN_FWD_URL;
        }
    } else {
        temp = "All " + string(m_Firewall
                               ? "firewall port(s)"
                               : "service entry point(s)") + " checked OK";
    }

    PostCheck(eFirewallConnections, 0/*main*/, status, temp);

    net_info.reset();
    if (reason)
        reason->swap(temp);
    return status;
}


static inline unsigned int ud(time_t one, time_t two)
{
    return (unsigned int)(one > two ? one - two : two - one);
}


static inline size_t rnd(size_t minimal, size_t maximal)
{
    return minimal <= maximal ? minimal + rand() % (maximal - minimal + 1) : 0;
}


EIO_Status CConnTest::StatefulOkay(string* reason)
{
    static const char kEcho[] = "ECHO";

    PreCheck(eStatefulService, 0/*main*/,
             "Checking reachability of a stateful service");

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(kEcho, m_DebugPrintout));

    CTime  time(CTime::eCurrent, CTime::eLocal);
    time_t seed = time.GetTimeT();

    char send[(1 << 10) + (8 + 1)];
    char recv[sizeof(send)];
    sprintf(send, "%08X", (unsigned int) seed);
    size_t size = rnd(2, (sizeof(send) - (8 + 1)) >> 3);

    size_t i;
    for (i = 1;  i < size;  ++i)
        sprintf(send + (i << 3), "%08X", (unsigned int) rand());
    memset(&send[size <<= 3], 0, 8);
    send[size += 8] = '\0';

    CConn_ServiceStream echo(kEcho, fSERV_Any, net_info.get(),
                             0/*xtra*/, m_Timeout);
    echo.SetCanceledCallback(m_Canceled);

    streamsize n = 0;
    bool iofail = !echo.write(send, size)  ||  !echo.flush()
        ||  !(n = CStreamUtils::Readsome(echo, recv, size));
    if (!iofail) {
        if (n < (streamsize) size) {
            if (!echo.read(recv + n, (streamsize) size - n))
                iofail = true;
            else
                n += echo.gcount();
        }
        if (n == (streamsize) size) {
            for (i = 8;  i < size;  ++i) {
                if (send[i] != recv[i])
                    break;
            }
            if (i >= size) {
                time_t now = (time_t) NStr::StringToNumeric<unsigned int>
                    (CTempString(recv,8), NStr::fConvErr_NoThrow, 16/*radix*/);
                time.SetTimeT(now);
            } else
                iofail = true;
        } else
            iofail = true;
    }
    EIO_Status status = ConnStatus(iofail, &echo);
    recv[n] = '\0';

    string temp;
    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK (RTT "+NStr::NumericToString(ud(seed,time.GetTimeT()))+')';
    else {
        char* str = SERV_ServiceName(kEcho);
        if (str  &&  NStr::CompareNocase(str, kEcho) != 0) {
            temp = n ? "Unrecognized" : "No";
            temp += " response received from substituted service;"
                " please remove [";
            string upper(kEcho);
            temp += NStr::ToUpper(upper);
            temp += "]" REG_CONN_SERVICE_NAME "=\"";
            temp += str;
            temp += "\" from your configuration\n";
            free(str); // NB: still, str != NULL
        } else if (str) {
            free(str);
            str = 0;
        }
        if (iofail) {
            if (status == eIO_Timeout) {
                if (!str) {
                    temp = n ? "Unrecognized" : "No";
                    temp += " response received from service. ";
                }
                temp += x_TimeoutMsg();
            }
            if (m_Stateless  ||  (net_info.get()  &&  net_info->stateless)) {
                temp += "STATELESS mode forced by your configuration may be"
                    " preventing this stateful service from operating"
                    " properly; try to remove [";
                if (!m_Stateless) {
                    string upper(kEcho);
                    temp += NStr::ToUpper(upper);
                    temp += "]"
                        DEF_CONN_REG_SECTION "_" REG_CONN_STATELESS "\n";
                } else
                    temp += DEF_CONN_REG_SECTION "]" REG_CONN_STATELESS "\n";
            } else if (!str) {
                SERV_ITER iter = 0;
                if (status != eIO_Timeout
                    &&  (!(iter = SERV_OpenSimple(kEcho))
                         ||  !SERV_GetNextInfo(iter))) {
                    temp += "The service is currently unavailable;"
                        " you may want to contact " + HELP_EMAIL + '\n';
                } else if (m_Fwd.empty()  &&  net_info.get()
                           &&  net_info->firewall != eFWMode_Fallback) {
                    temp += "The most likely reason for the failure is that"
                        " your firewall is still blocking ports as reported"
                        " above\n";
                } else if (status != eIO_Timeout  ||  x_Large(m_Timeout))
                    temp += "Please contact " + HELP_EMAIL + '\n';
                SERV_Close(iter);
            }
        } else if (!str) {
            if (n  &&  NStr::strncasecmp(recv, kFWSign, (size_t) n) == 0) {
                temp += "NCBI Firewall";
                if (!net_info->firewall)
                    temp += " (Connection Relay)";
                temp += " Daemon reports negotitation error";
                if (net_info.get()  &&  m_HttpProxy
                    &&  !(net_info->http_proxy_only |
                          net_info->http_proxy_leak)) {
                    temp += ", which usually means that an intermediate HTTP"
                        " proxy '";
                    temp += net_info->http_proxy_host;
                    temp += ':';
                    temp += NStr::UIntToString(net_info->http_proxy_port);
                    temp += "' may be buggy.";
                } else
                    temp += '.';
                temp += " Please contact your network administrator\n";
            } else {
                temp += n ? "Unrecognized" : "No";
                temp += " response from service;"
                    " please contact " + HELP_EMAIL + '\n';
            }
        }
    }
    net_info.reset();

    PostCheck(eStatefulService, 0/*main*/, status, temp);

    if (reason)
        reason->swap(temp);
    return status;
}


void CConnTest::PreCheck(EStage/*stage*/, unsigned int/*step*/,
                         const string& title)
{
    m_End = false;

    if (!m_Output)
        return;

    list<string> stmt;
    NStr::Split(title, "\n", stmt,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    SIZE_TYPE size = stmt.size();
    *m_Output << NcbiEndl << stmt.front() << '.';
    stmt.pop_front();
    if (size > 1) {
        ERASE_ITERATE(list<string>, str, stmt) {
            if (str->empty())
                stmt.erase(str);
        }
        if (!stmt.empty()) {
            *m_Output << NcbiEndl;
            NON_CONST_ITERATE(list<string>, str, stmt) {
                NStr::TruncateSpacesInPlace(*str);
                if (!NStr::EndsWith(*str, ".")  &&  !NStr::EndsWith(*str, "!"))
                    str->append(1, '.');
                list<string> par;
                NStr::Justify(*str, m_Width, par,
                              kEmptyStr, string(kParIndent, ' '));
                ITERATE(list<string>, line, par) {
                    *m_Output << NcbiEndl << *line;
                }
            }
        }
        *m_Output << NcbiEndl;
    } else
        *m_Output << ".." << NcbiFlush;
}


void CConnTest::PostCheck(EStage/*stage*/, unsigned int/*step*/,
                          EIO_Status status, const string& reason)
{
    bool end = m_End;
    m_End = true;

    if (!m_Output)
        return;

    list<string> stmt;
    NStr::Split(reason, "\n", stmt,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    ERASE_ITERATE(list<string>, str, stmt) {
        if (str->empty())
            stmt.erase(str);
    }

    if (status == eIO_Success) {
        if (reason.empty()) {
            *m_Output << NcbiEndl;
            return;
        }
        *m_Output << "\n\t"[!end] << (stmt.empty() ? reason : stmt.front())
                  << '!' << NcbiEndl;
        if (!stmt.empty())
            stmt.pop_front();
        if (stmt.empty())
            return;
    } else if (!end) {
        *m_Output << "\tFAILED (" << IO_StatusStr(status) << ')';
        const string& where = GetCheckPoint();
        if (!where.empty())
            *m_Output << ':' << NcbiEndl << string(kParIndent, ' ') << where;
        if (!stmt.empty())
            *m_Output << NcbiEndl;
    }

    unsigned int n = 0;
    NON_CONST_ITERATE(list<string>, str, stmt) {
        NStr::TruncateSpacesInPlace(*str);
        if (!NStr::EndsWith(*str, ".")  &&  !NStr::EndsWith(*str, "!"))
            str->append(1, '.');
        string pfx1, pfx;
        if (status == eIO_Success  ||  !end) {
            pfx.assign(kParIndent, ' ');
            if (status != eIO_Success  &&  stmt.size() > 1) {
                char buf[40];
                pfx1.assign(buf, ::sprintf(buf, "%2d. ", ++n));
            } else
                pfx1.assign(pfx);
        }
        list<string> par;
        NStr::Justify(*str, m_Width, par, pfx, pfx1);
        ITERATE(list<string>, line, par) {
            *m_Output << NcbiEndl << *line;
        }
    }
    *m_Output << NcbiEndl;
}


EIO_Status CConnTest::x_CheckTrap(string* reason)
{
    m_CheckPoint.clear();

    PreCheck(EStage(0), 0, "Runaway check");
    PostCheck(EStage(0), 0, eIO_NotSupported, "Check usage");

    if (reason)
        reason->clear();
    return eIO_NotSupported;
}


bool CConnTest::IsNcbiInhouseClient(void)
{
    static const STimeout kFast = { 2, 0 };
    CConn_HttpStream http("https:///Service/getenv.cgi",
                          fHTTP_KeepHeader | fHTTP_NoAutoRetry, &kFast);
    char line[256];
    if (!http  ||  !http.getline(line, sizeof(line)))
        return false;
    int code;
    return !(::sscanf(line, "HTTP/%*d.%*d %d ", &code) < 1  ||  code != 200);
}


END_NCBI_SCOPE
