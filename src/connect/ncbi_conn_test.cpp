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
#include <corelib/ncbiutil.hpp>
#include <corelib/stream_utils.hpp>
#include <connect/ncbi_conn_test.hpp>
#include <connect/ncbi_socket.hpp>
#include "ncbi_comm.h"
#include "ncbi_servicep.h"

#ifndef   MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN 255
#endif // MAXHOSTNAMELEN

#define __STR(x)  #x
#define _STR(x) __STR(x)

#define NCBI_HELP_DESK  "NCBI Help Desk info@ncbi.nlm.nih.gov"
#define NCBI_FW_URL                                                     \
    "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/NETWORK/firewall.html#Settings"


BEGIN_NCBI_SCOPE


static const char kTest[] = "test";


const STimeout CConnTest::kTimeout = {30, 0};


inline bool operator > (const STimeout* t1, const STimeout& t2)
{
    if (!t1)
        return true;
    return t1->sec + t1->usec/1000000.0 > t2.sec + t2.usec/1000000.0;
}


CConnTest::CConnTest(const STimeout* timeout,
                     CNcbiOstream* out, SIZE_TYPE linelen)
    : m_Linelen(linelen), m_Out(out),
      m_HttpProxy(false), m_Stateless(false), m_Firewall(false),
      m_FWProxy(false),
      m_FwdStatus(eIO_NotSupported), m_FwdFallbackStatus(eIO_NotSupported),
      m_End(false)
{
    memset(&m_TimeoutValue, 0, sizeof(m_TimeoutValue));
    if (timeout) {
        m_TimeoutValue = timeout != kDefaultTimeout ? *timeout : kTimeout;
        m_Timeout      = &m_TimeoutValue;
    } else
        m_Timeout      = kInfiniteTimeout/*0*/;
}


EIO_Status CConnTest::Execute(EStage& stage, string* reason)
{
    typedef EIO_Status (CConnTest::*FCheck)(string* reason);
    FCheck check[] = {
        &CConnTest::HttpOkay,
        &CConnTest::DispatcherOkay,
        &CConnTest::ServiceOkay,
        &CConnTest::GetFWConnections,
        &CConnTest::CheckFWConnections,
        &CConnTest::StatefulOkay,
        &CConnTest::x_CheckTrap  // Guaranteed to fail
    };

    // Reset everything
    m_HttpProxy = m_Stateless = m_Firewall
        = m_FWProxy = m_End = false;
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
    return status;
}


string CConnTest::x_TimeoutMsg(void)
{
    if (!m_Timeout)
        return kEmptyStr;
    char tmo[40];
    ::sprintf(tmo, "%u.%06u", m_Timeout->sec, m_Timeout->usec);
    string result("Make sure the specified timeout value ");
    result += tmo;
    result += "s is adequate for your network throughput\n";
    return result;
}


EIO_Status CConnTest::ConnStatus(bool failure, CConn_IOStream& io)
{
    CONN conn = io.GetCONN();
    const char* ctype = conn ? CONN_GetType(conn)     : 0;
    const char* descr = conn ? CONN_Description(conn) : 0;
    m_CheckPoint =
        string(ctype            ? ctype : kEmptyStr) +
        string(ctype  &&  descr ? "/"   : kEmptyStr) +
        string(descr            ? descr : kEmptyStr);
    if (!failure)
        return eIO_Success;
    EIO_Status status = io.Status();
    if (status == eIO_Success  &&  conn) {
        EIO_Status r_status = CONN_Status(conn, eIO_Read);
        EIO_Status w_status = CONN_Status(conn, eIO_Write);
        status = r_status > w_status ? r_status : w_status;
    }
    return status == eIO_Success ? eIO_Unknown : status;
}


EIO_Status CConnTest::HttpOkay(string* reason)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    if (net_info) {
        if (*net_info->http_proxy_host)
            m_HttpProxy = true;
        // Make sure there are no extras
        ConnNetInfo_SetUserHeader(net_info, 0);
        net_info->args[0] = '\0';
    }

    PreCheck(eHttp, 0/*main*/,
             "Checking whether NCBI is HTTP accessible");

    string host(net_info ? net_info->host : DEF_CONN_HOST);
    string port(net_info  &&  net_info->port
                ? ':' + NStr::UIntToString(net_info->port)
                : kEmptyStr);
    CConn_HttpStream http("http://" + host + port + "/Service/index.html",
                          net_info, kEmptyStr/*user_header*/,
                          0/*flags*/, m_Timeout);
    string temp;
    http >> temp;
    EIO_Status status = ConnStatus(temp.empty(), http);

    if (status != eIO_Success) {
        temp.clear();
        if (status == eIO_Timeout)
            temp += x_TimeoutMsg();
        if (host != DEF_CONN_HOST  ||  !port.empty()) {
            int n = 0;
            temp += "Make sure that ";
            if (host != DEF_CONN_HOST) {
                n++;
                temp += "[CONN]HOST=\"";
                temp += host;
                temp += port.empty() ? "\"" : "\" and ";
            }
            if (!port.empty()) {
                n++;
                temp += "[CONN]PORT=\"";
                temp += port.c_str() + 1;
                temp += '"';
            }
            _ASSERT(n);
            temp += n > 1 ? " are" : " is";
            temp += " redefined correctly\n";
        }
        if (m_HttpProxy) {
            temp += "Make sure that the HTTP proxy server \'";
            temp += net_info->http_proxy_host;
            if (net_info->http_proxy_port) {
                temp += ':';
                temp += NStr::UIntToString(net_info->http_proxy_port);
            }
            temp += "' specified with [CONN]HTTP_PROXY_{HOST|PORT}"
                " is correct";
        } else {
            temp += "If your network configuration requires the use of an"
                " HTTP proxy server, please contact your network administrator"
                " and set [CONN]HTTP_PROXY_{HOST|PORT} accordingly";
        }
        temp += "; and if your proxy server requires authorization, please"
            " check that appropriate [CONN]HTTP_PROXY_{USER|PASS} are used\n";
        if (net_info  &&  (*net_info->user  ||  *net_info->pass)) {
            temp += "Make sure there are no stray [CONN]{USER|PASS}"
                " specified in your configuration -- NCBI services"
                " do not require them\n";
        }
    } else
        temp = "OK";

    PostCheck(eHttp, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


extern "C" {
    static int s_ParseHttpHeader(const char*, void*, int);
}
static int s_ParseHttpHeader(const char* header, void* data, int server_error)
{
    _ASSERT(data);
    *((int*) data) =
        !server_error  &&  NStr::FindNoCase(header, "\nService: ") != NPOS
        ? 1
        : 2;
    return 1/*always success*/;
}


EIO_Status CConnTest::DispatcherOkay(string* reason)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(kTest);
    ConnNetInfo_SetupStandardArgs(net_info, kTest);

    PreCheck(eDispatcher, 0/*main*/,
             "Checking whether NCBI dispatcher is okay");

    int okay = 0;
    CConn_HttpStream http(net_info, kEmptyStr/*user_header*/,
                          s_ParseHttpHeader, 0, &okay, 0,
                          0/*flags*/, m_Timeout);
    char buf[1024];
    http.read(buf, sizeof(buf));
    CTempString str(buf, http.gcount());
    EIO_Status status = ConnStatus
        (okay != 1  ||
         NStr::FindNoCase(str, "NCBI Dispatcher Test Page") == NPOS  ||
         NStr::FindNoCase(str, "Welcome") == NPOS, http);

    string temp;
    if (status != eIO_Success) {
        if (status != eIO_Timeout) {
            if (okay) {
                temp += "Make sure there are no stray [CONN]{HOST|PORT|PATH}"
                    " configuration settings on the way\n";
            }
            if (okay == 1) {
                temp += "Service response was not recognized; please contact "
                    NCBI_HELP_DESK "\n";
            }
        } else
            temp += x_TimeoutMsg();
        if (!(okay & 1)) {
            temp += "Check with your network administrator that your"
                " network neither filters out nor blocks non-standard"
                " HTTP headers\n";
        }
    } else
        temp = "OK";

    PostCheck(eDispatcher, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::ServiceOkay(string* reason)
{
    static const char kService[] = "bounce";

    SConnNetInfo* net_info = ConnNetInfo_Create(kService);
    if (net_info)
        net_info->lb_disable = 1/*no local LB to use even if available*/;

    PreCheck(eStatelessService, 0/*main*/,
             "Checking whether NCBI services operational");

    CConn_ServiceStream svc(kService, fSERV_Stateless, net_info,
                            0/*params*/, m_Timeout);
    svc << kTest << NcbiEndl;
    string temp;
    svc >> temp;
    bool responded = temp.size() > 0;
    EIO_Status status = ConnStatus(NStr::CompareCase(temp, kTest) != 0, svc);

    if (status != eIO_Success) {
        char* str = net_info ? SERV_ServiceName(kService) : 0;
        if (str  &&  NStr::strcasecmp(str, kService) == 0) {
            free(str);
            str = 0;
        }
        SERV_ITER iter = SERV_OpenSimple(kService);
        if (!iter  ||  !SERV_GetNextInfo(iter)) {
            // Service not found
            SERV_Close(iter);
            iter = SERV_OpenSimple(kTest);
            if (!iter  ||  !SERV_GetNextInfo(iter)  ||
                NStr::strcasecmp(SERV_MapperName(iter), "DISPD") != 0) {
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
            temp = responded ? "No" : "Unrecognized";
            temp += " response from ";
            temp += str ? "substituted service" : "service";
        }
        if (!temp.empty()) {
            if (str) {
                temp += "; please remove [";
                string upper(kService);
                temp += NStr::ToUpper(upper);
                temp += "]CONN_SERVICE_NAME=";
                temp += str;
                temp += " from your configuration\n";
            } else if (status != eIO_Timeout  ||  m_Timeout > kTimeout)
                temp += "; please contact " NCBI_HELP_DESK "\n";
        }
        if (status != eIO_Timeout) {
            const char* mapper = SERV_MapperName(iter);
            if (!mapper  ||  NStr::strcasecmp(mapper, "DISPD") != 0) {
                temp += "Network dispatcher is not enabled as a service"
                    " locator; please review your configuration to purge any"
                    " occurrences of [CONN]DISPD_DISABLE off your settings\n";
            }
        } else
            temp += x_TimeoutMsg();
        SERV_Close(iter);
        if (str)
            free(str);
    } else
        temp = "OK";

    PostCheck(eStatelessService, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::GetFWConnections(string* reason)
{
    const char* user_header = "NCBI-RELAY: UNDEF";
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    if (net_info) {
        if (net_info->firewall) {
            user_header = "NCBI-RELAY: FALSE";
            m_Firewall = true;
        } else
            user_header = "NCBI-RELAY: TRUE";
        if (net_info->stateless)
            m_Stateless = true;
        ConnNetInfo_SetupStandardArgs(net_info, 0/*w/o service*/);
    }

    static const char* kFWExplanation[] = {
        "This is an obsolescent mode that requires to keep a wide port range"
        " [4444..4544] (inclusive) open to let through connections to any"
        " NCBI host (130.14.2x.xxx/165.112.xx.xxx) -- this mode was designed"
        " for unrestricted networks when firewall port blocking was not an"
        " issue\n"
        "You may not be able to use this mode if your site has a firewall"
        " that controls to which hosts and ports the outbound connections"
        " are allowed to go\n",

        "This mode requires to have your firewall configured such a way"
        " that it allows outbound connections to the port range ["
        _STR(CONN_FWD_PORT_MIN) ".." _STR(CONN_FWD_PORT_MAX) "]"
        " at the two fixed NCBI hosts, 130.14.29.112 and 165.112.7.12\n"
        "In order to configure that correctly, please have your network"
        " administrator read the following (if they have not yet done so): "
        NCBI_FW_URL "\n"
        "There are also fallback connection ports 22 and 443 at"
        " 130.14.29.112. They will be tried if connections to the ports in"
        " the range described above failed"
    };
    string temp = m_Firewall ? "FIREWALL" : "RELAY (legacy)";
    temp += " connection mode has been detected for stateful services\n";
    PreCheck(eFirewallConnPoints, 0/*main*/,
             temp + kFWExplanation[m_Firewall]);

    PreCheck(eFirewallConnPoints, 1/*sub*/,
             "Obtaining current NCBI " +
             string(m_Firewall ? "firewall settings" : "service entries"));

    CConn_HttpStream script("http://www.ncbi.nlm.nih.gov/IEB/ToolBox/NETWORK"
                            "/fwd_check.cgi", net_info, user_header,
                            0/*flags*/, m_Timeout);

    x_ObtainFirewallSettings(script);

    // Check the conventional group
    m_FwdStatus = ConnStatus
        ((script.fail()  &&  !script.eof())  ||  m_Fwd.empty(), script);

    if (m_FwdStatus == eIO_Timeout) {
        temp = x_TimeoutMsg();
        if (m_Timeout > kTimeout)
            temp += "You may want to contact " NCBI_HELP_DESK;
    } else if (m_FwdStatus == eIO_Success) {
        temp = "OK: " +
               NStr::UInt8ToString(m_Fwd.size()) +
               " port";
        temp += &"s"[m_Fwd.size() == 1];
        stable_sort(m_Fwd.begin(), m_Fwd.end());
    } else
        temp = "Please contact " NCBI_HELP_DESK;

    PostCheck(eFirewallConnPoints, 1/*sub*/, m_FwdStatus, temp);

    bool firewall = true;
    if (m_FwdStatus == eIO_Success) {
        PreCheck(eFirewallConnPoints, 2/*sub*/,
                 "Verifying configuration for consistency");

        ITERATE(vector<CConnTest::CFWConnPoint>, cp, m_Fwd) {
            if (cp->port < CONN_FWD_PORT_MIN  ||  CONN_FWD_PORT_MAX < cp->port)
                firewall = false;
            if (!cp->okay) {
                temp  = CSocketAPI::HostPortToString(cp->host, cp->port);
                temp += " is not operational, please contact " NCBI_HELP_DESK;
                m_FwdStatus = eIO_NotSupported;
                break;
            }
        }
        if (m_FwdStatus == eIO_Success) {
            if (m_Firewall != firewall) {
                temp  = "Configuration mismatch: firewall ";
                temp += firewall ? "wrongly" : "not";
                temp += " acknowledged, please contact " NCBI_HELP_DESK;
                m_FwdStatus = eIO_NotSupported;
            } else
                temp.resize(2);
        }

        PostCheck(eFirewallConnPoints, 2/*sub*/, m_FwdStatus, temp);
    }

    // Check the fallback group if necessary
    if (!m_FwdFallback.empty()) {
        PreCheck(eFirewallConnPoints, 3/*sub*/,
                 "Obtaining fallback ports");
        m_FwdFallbackStatus = ConnStatus(script.fail()  &&  !script.eof(),
                                         script);
        if (m_FwdFallbackStatus == eIO_Timeout) {
            temp = x_TimeoutMsg();
            if (m_Timeout > kTimeout)
                temp += "You may want to contact " NCBI_HELP_DESK;
        } else if (m_FwdFallbackStatus == eIO_Success) {
            temp = "OK: " +
                NStr::UInt8ToString(m_FwdFallback.size()) +
                " port";
            temp += &"s"[m_Fwd.size() == 1];
            stable_sort(m_FwdFallback.begin(), m_FwdFallback.end());
        } else
            temp = "Please contact " NCBI_HELP_DESK;
        PostCheck(eFirewallConnPoints, 3/*sub*/, m_FwdFallbackStatus, temp);

        if (m_FwdFallbackStatus == eIO_Success) {
            PreCheck(eFirewallConnPoints, 4/*sub*/,
                     "Verifying fallback ports operational status");
            ITERATE(vector<CConnTest::CFWConnPoint>, cp, m_FwdFallback) {
                if (!cp->okay) {
                    temp = CSocketAPI::HostPortToString(cp->host, cp->port);
                    temp += " is not operational, please contact "
                            NCBI_HELP_DESK;
                    m_FwdFallbackStatus = eIO_NotSupported;
                    break;
                }
            }
            if (m_FwdFallbackStatus == eIO_Success)
                temp = "OK";
            PostCheck(eFirewallConnPoints, 4/*sub*/, m_FwdFallbackStatus, temp);
        }
    }

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    if (m_FwdStatus == eIO_Success || m_FwdFallbackStatus == eIO_Success)
        return eIO_Success;
    return m_FwdStatus;
}


EIO_Status CConnTest::CheckFWConnections(string* reason)
{
    TSOCK_Flags flags;
    char val[MAXHOSTNAMELEN + 1];
    ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                         DEF_CONN_DEBUG_PRINTOUT);
    if (*val  &&  (NStr::strcasecmp(val, "all")  == 0  ||
                   NStr::strcasecmp(val, "data") == 0)) {
        flags = fSOCK_LogOn;
    } else
        flags = fSOCK_LogDefault;

    ConnNetInfo_GetValue(0, REG_CONN_PROXY_HOST, val, sizeof(val),
                         DEF_CONN_PROXY_HOST);
    if (*val)
        m_FWProxy = true;

    if (m_FwdStatus == eIO_Success) {
        PreCheck(eFirewallConnections, 0/*main*/,
                 "Checking individual connection points\n"
                 "NOTE that even though that not the entire port range can"
                 " be currently utilized and checked, in order for NCBI services"
                 " to work correctly, your network must support every port"
                 " in the range as documented above\n");

        unsigned int n = 0;
        _ASSERT(m_Fwd.size());
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, m_Fwd) {
            _ASSERT(cp->okay);
            PreCheck(eFirewallConnections, ++n,
                     "Checking connectivity at "
                     + CSocketAPI::HostPortToString(cp->host, cp->port));

            string temp;
            m_FwdStatus = x_CheckFWConnection(*cp, val, flags, temp);

            PostCheck(eFirewallConnections, n, m_FwdStatus, temp);

            if (m_FwdStatus != eIO_Success) {
                if (reason)
                    reason->swap(temp);
                break;
            }
        }

        string summary;
        if (m_FwdStatus != eIO_Success)
            summary = "Firewall port check FAILED";
        else
            summary = "All firewall ports checked OK";

        PostCheck(eFirewallConnections, 0/*main*/, m_FwdStatus, summary);
    }

    if (m_FwdFallbackStatus == eIO_Success) {
        PreCheck(eFirewallConnections, 0/*main*/,
                 "Checking individual fallback connection points\n"
                 "NOTE that fallback connection points will be used "
                 "only if the standard connection points do not work\n");
        unsigned int n = 0;
        _ASSERT(m_FwdFallback.size());
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, m_FwdFallback) {
            PreCheck(eFirewallConnections, ++n,
                     "Checking connectivity at "
                     + CSocketAPI::HostPortToString(cp->host, cp->port));
            string temp;
            m_FwdFallbackStatus = x_CheckFWConnection(*cp, val, flags, temp);
            PostCheck(eFirewallConnections, n, m_FwdFallbackStatus, temp);

            if (m_FwdFallbackStatus != eIO_Success) {
                if (reason)
                    reason->swap(temp);
                break;
            }
        }

        string summary;
        if (m_FwdFallbackStatus != eIO_Success)
            summary = "Fallback firewall port check FAILED";
        else
            summary = "All fallback firewall ports checked OK";

        PostCheck(eFirewallConnections, 0/*main*/, m_FwdFallbackStatus, summary);
    }

    if (m_FwdStatus == eIO_Success || m_FwdFallbackStatus == eIO_Success)
        return eIO_Success;
    return m_FwdStatus;
}


EIO_Status CConnTest::StatefulOkay(string* reason)
{
    _ASSERT(m_FwdStatus == eIO_Success || m_FwdFallbackStatus == eIO_Success);

    static const char kId2Init[] =
        "0\2000\200\242\200\240\200\5\0\0\0\0\0\0\0\0\0";
    static const char kId2[] = "ID2";
    char ry[80];

    SConnNetInfo* net_info = ConnNetInfo_Create(kId2);

    PreCheck(eStatefulService, 0/*main*/,
             "Checking reachability of a stateful service");

    CConn_ServiceStream id2(kId2, fSERV_Any, net_info, 0/*params*/, m_Timeout);

    streamsize n = 0;
    bool iofail = !id2.write(kId2Init, sizeof(kId2Init) - 1)  ||  !id2.flush()
        ||  !(n = CStreamUtils::Readsome(id2, ry, sizeof(ry)));
    EIO_Status status = ConnStatus
        (iofail  ||  n < 4  ||  memcmp(ry, "0\200\240\200", 4) != 0, id2);

    string temp;
    if (status != eIO_Success) {
        char* str = SERV_ServiceName(kId2);
        if (str  &&  NStr::strcasecmp(str, kId2) != 0) {
            temp += n ? "Unrecognized" : "No";
            temp += " response has been received from substituted service;"
                " please remove [";
            string upper(kId2);
            temp += NStr::ToUpper(upper);
            temp += "]CONN_SERVICE_NAME=";
            temp += str;
            temp += " from your configuration\n";
            free(str);
        } else if (str) {
            free(str);
            str = 0;
        }
        if (iofail) {
            if (m_Stateless  ||  (net_info  &&  net_info->stateless)) {
                temp += "STATELESS mode forced by your configuration may be"
                    " preventing this stateful service from operating"
                    " properly; try to remove [";
                if (!m_Stateless) {
                    string upper(kId2);
                    temp += NStr::ToUpper(upper);
                    temp += "]CONN_STATELESS\n";
                } else
                    temp += "CONN]STATELESS\n";
            } else if (!str) {
                SERV_ITER iter = SERV_OpenSimple(kId2);
                if (!iter  ||  !SERV_GetNextInfo(iter)) {
                    temp += "The service is currently unavailable;"
                        " you may want top contact " NCBI_HELP_DESK "\n";
                } else if (m_FwdStatus != eIO_Success) {
                    temp += "The most likely reason for the failure is that"
                        " your ";
                    temp += m_FWProxy ? "proxy" : "firewall";
                    temp += " is still blocking ports as reported above\n";
                } else if (status != eIO_Timeout  ||  m_Timeout > kTimeout)
                    temp += "Please contact " NCBI_HELP_DESK "\n";
                SERV_Close(iter);
            }
            if (status == eIO_Timeout)
                temp += x_TimeoutMsg();
        } else if (!str) {
            temp += "Unrecognized response from service"
                "; please contact " NCBI_HELP_DESK "\n";
        }
    } else
        temp = "OK";

    PostCheck(eStatefulService, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


void CConnTest::PreCheck(EStage/*stage*/, unsigned int/*step*/,
                         const string& title)
{
    m_End = false;

    if (!m_Out)
        return;

    list<string> stmt;
    NStr::Split(title, "\n", stmt);
    SIZE_TYPE size = stmt.size();
    *m_Out << NcbiEndl << stmt.front() << '.';
    stmt.pop_front();
    if (size > 1) {
        ERASE_ITERATE(list<string>, str, stmt) {
            if (str->empty())
                stmt.erase(str);
        }
        if (stmt.size()) {
            *m_Out << NcbiEndl;
            NON_CONST_ITERATE(list<string>, str, stmt) {
                NStr::TruncateSpacesInPlace(*str);
                str->append(1, '.');
                list<string> par;
                NStr::Justify(*str, m_Linelen, par, kEmptyStr, string(4, ' '));
                ITERATE(list<string>, line, par) {
                    *m_Out << NcbiEndl << *line;
                }
            }
        }
        *m_Out << NcbiEndl;
    } else
        *m_Out << ".." << NcbiFlush;
}


void CConnTest::PostCheck(EStage/*stage*/, unsigned int/*step*/,
                          EIO_Status status, const string& reason)
{
    bool end = m_End;
    m_End = true;

    if (!m_Out)
        return;

    if (status == eIO_Success) {
        *m_Out << "\n\t"[!end] << reason << '!' << NcbiEndl;
        return;
    }

    list<string> stmt;
    NStr::Split(reason, "\n", stmt);
    ERASE_ITERATE(list<string>, str, stmt) {
        if (str->empty())
            stmt.erase(str);
    }

    if (!end  ||  !stmt.size()) {
        *m_Out << "\n\t"[!end] << "FAILED (" << IO_StatusStr(status) << ')';
        if (stmt.size()) {
            const string& where = GetCheckPoint();
            if (!where.empty())
                *m_Out << ':' << NcbiEndl << string(4, ' ') << where;
            *m_Out << NcbiEndl;
        }
    }

    unsigned int n = 0;
    NON_CONST_ITERATE(list<string>, str, stmt) {
        NStr::TruncateSpacesInPlace(*str);
        str->append(1, '.');
        string pfx1, pfx;
        if (!end) {
            pfx.assign(4, ' ');
            if (stmt.size() > 1) {
                char buf[40];
                pfx1.assign(buf, ::sprintf(buf, "%2d. ", ++n));
            } else
                pfx1.assign(pfx);
        }
        list<string> par;
        NStr::Justify(*str, m_Linelen, par, pfx, pfx1);
        ITERATE(list<string>, line, par) {
            *m_Out << NcbiEndl << *line;
        }
    }
    *m_Out << NcbiEndl;
}


EIO_Status CConnTest::x_CheckTrap(string* reason)
{
    EIO_Status status = eIO_NotSupported;
    string temp("Runaway check");
    m_CheckPoint.clear();

    PreCheck(EStage(0), 0, temp);
    PostCheck(EStage(0), 0, status, "Check usage");

    if (reason)
        reason->clear();
    return eIO_NotSupported;
}


EIO_Status CConnTest::x_CheckFWConnection(CFWConnPoint& cp,
                                          char*         val,
                                          TSOCK_Flags   flags,
                                          string&       reason)
{
    const char kFWSign[] =
        "NCBI Firewall Daemon:  Invalid ticket.  Connection closed.";

    CConn_SocketStream fw(m_FWProxy ? val : CSocketAPI::ntoa(cp.host),
                          cp.port, "\r\n"/*data*/, 2/*size*/,
                          flags, 1/*attempts*/, m_Timeout);
    char line[sizeof(kFWSign) + 2/*\r+EOS*/];
    if (!fw.getline(line, sizeof(line)))
        *line = '\0';
    EIO_Status status = ConnStatus
        (NStr::strncasecmp(line, kFWSign, sizeof(kFWSign) - 1) != 0, fw);
    if (status != eIO_Success) {
        cp.okay = false;
        if (status == eIO_Timeout)
            reason += x_TimeoutMsg();
        if (m_FWProxy) {
            reason += "Non-transparent proxy server '";
            reason += val;
            reason += "' may not be forwarding connections properly,"
                      " please check with your network administrator that the"
                      " proxy has been configured correctly: " NCBI_FW_URL "\n";
            *line = '\0';
        }
        if (*line) {
            reason += "There is an unknown server responding at this"
                      " connection point; please contact " NCBI_HELP_DESK "\n";
        } else {
            if (m_Firewall) {
                reason += "The network port required for this connection"
                          " to succeed is being blocked at your firewall;"
                          " see your network administrator and have them read"
                          " the following: " NCBI_FW_URL "\n";
            } else {
                reason += "The network port required for this connection"
                          " to succeed is being blocked at your firewall;"
                          " try setting [CONN]FIREWALL=1 in your configuration"
                          " file to use more narrow port range\n";
            }
            if (!m_Stateless) {
                reason +=  "Not all NCBI stateful services require to work"
                           " over dedicated (persistent) connections;  some can"
                           " work (at the cost of degraded performance) over a"
                           " stateless carrier such as HTTP;  try setting"
                           " [CONN]STATELESS=1 in your configuration\n";
            }
        }
    } else
        reason = "OK";

    return status;
}


void CConnTest::x_ObtainFirewallSettings(CConn_HttpStream& script)
{
    script << "selftest" << NcbiEndl;
    char line[256];
    while (script.getline(line, sizeof(line))) {
        string hostport, state;
        if (!NStr::SplitInTwo(line, "\t", hostport, state))
            continue;
        if (!m_Firewall && NStr::CompareNocase(state, 0, 3, "FB-") == 0)
            continue;
        CFWConnPoint cp;
        if (NStr::CompareNocase(state, 0, 2, "OK") == 0 ||
            NStr::CompareNocase(state, 0, 5, "FB-OK") == 0)
            cp.okay = true;
        else if (NStr::CompareNocase(state, 0, 4, "FAIL") == 0 ||
                 NStr::CompareNocase(state, 0, 7, "FB-FAIL") == 0)
            cp.okay = false;
        else
            continue;
        if (CSocketAPI::StringToHostPort(hostport, &cp.host, &cp.port) == NPOS)
            continue;
        if (NStr::CompareNocase(state, 0, 3, "FB-") == 0)
            m_FwdFallback.push_back(cp);
        else
            m_Fwd.push_back(cp);
    }
}


END_NCBI_SCOPE
