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
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <algorithm>

#define __STR(x)  #x
#define _STR(x)   __STR(x)

#define NCBI_HELP_DESK  "NCBI Help Desk info@ncbi.nlm.nih.gov"
#define NCBI_FW_URL                                                     \
    "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/NETWORK/firewall.html#Settings"


BEGIN_NCBI_SCOPE


static const char kTest[] = "test";
static const char kCanceled[] = "Check canceled";

static const char kFWSign[] =
    "NCBI Firewall Daemon:  Invalid ticket.  Connection closed.";


const STimeout CConnTest::kTimeout = { 30, 0 };


class CIOGuard
{
    friend class CConnTest;

protected:
    CIOGuard(CConn_IOStream*& ptr, CConn_IOStream& stream)
        : m_Ptr(ptr) { m_Ptr = &stream; }

    ~CIOGuard() { CORE_LOCK_WRITE; m_Ptr = 0; CORE_UNLOCK; }

private:
    CConn_IOStream*& m_Ptr;
};


inline bool operator > (const STimeout* t1, const STimeout& t2)
{
    if (!t1)
        return true;
    return t1->sec + t1->usec/1000000.0 > t2.sec + t2.usec/1000000.0;
}


CConnTest::CConnTest(const STimeout* timeout,
                     CNcbiOstream* output, SIZE_TYPE width)
    : m_Output(output), m_Width(width),
      m_HttpProxy(false), m_Stateless(false), m_Firewall(false),
      m_End(false), m_IO(0), m_Canceled(false)
{
    SetTimeout(timeout);
}


void CConnTest::SetTimeout(const STimeout* timeout)
{
    if (timeout) {
        m_TimeoutStorage = timeout == kDefaultTimeout ? kTimeout : *timeout;
        m_Timeout        = &m_TimeoutStorage;
    } else
        m_Timeout        = kInfiniteTimeout/*0*/;
}


void CConnTest::Cancel(void)
{
    m_Canceled = true;
    if (m_IO) {
        CORE_LOCK_READ;
        if (m_IO)
            m_IO->Cancel();
        CORE_UNLOCK;
    }
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
    m_HttpProxy = m_Stateless = m_Firewall = m_End = false;
    _ASSERT(!m_IO);
    m_Fwd.clear();
    if (reason)
        reason->clear();
    m_CheckPoint.clear();

    int s = eHttp;
    EIO_Status status;
    do {
        if (m_Canceled) {
            if ( reason )
                *reason = kCanceled;
            status = eIO_Interrupt;
            break;
        }
        if ((status = (this->*check[s])(reason)) != eIO_Success) {
            stage = EStage(s);
            break;
        }
    } while (EStage(s++) < stage);
    _ASSERT(!m_IO);
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
    string result("Make sure the specified timeout value ");
    result += tmo;
    result += "s is adequate for your network throughput\n";
    return result;
}


EIO_Status CConnTest::ConnStatus(bool failure, CConn_IOStream* io)
{
    string type = io ? io->GetType()        : kEmptyStr;
    string text = io ? io->GetDescription() : kEmptyStr;
    m_CheckPoint = (type
                    + (!type.empty()  &&  !text.empty() ? "; " : "") +
                    text);
    if (m_Canceled)
        return eIO_Interrupt;
    if (!failure)
        return eIO_Success;
    if (!io)
        return eIO_Unknown;
    if (!io->GetCONN())
        return eIO_Closed;
    EIO_Status status = io->Status();
    if (status != eIO_Success)
        return status;
    EIO_Status r_status = io->Status(eIO_Read);
    EIO_Status w_status = io->Status(eIO_Write);
    status = r_status > w_status ? r_status : w_status;
    return status == eIO_Success ? eIO_Unknown : status;
}


EIO_Status CConnTest::HttpOkay(string* reason)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    if (net_info) {
        if (net_info->http_proxy_port)
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
    CIOGuard guard(m_IO, http);
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
        if (NStr::CompareNocase(host, DEF_CONN_HOST) != 0  ||  !port.empty()) {
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
            temp += ':';
            temp += NStr::UIntToString(net_info->http_proxy_port);
            temp += "' specified with [CONN]HTTP_PROXY_{HOST|PORT}"
                " is correct";
        } else {
            temp += "If your network access requires the use of an HTTP proxy"
                " server, please contact your network administrator and set"
                " [CONN]HTTP_PROXY_{HOST|PORT} in your configuration"
                " accordingly";
        }
        temp += "; and if your proxy server requires authorization, please"
            " check that appropriate [CONN]HTTP_PROXY_{USER|PASS} have been"
            " specified\n";
        if (net_info  &&  (*net_info->user  ||  *net_info->pass)) {
            temp += "Make sure there are no stray [CONN]{USER|PASS} appear in"
                " your configuration -- NCBI services neither require nor use"
                " them\n";
        }
    }

    PostCheck(eHttp, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


extern "C" {
static int s_ParseHeader(const char* header, void* data, int server_error)
{
    _ASSERT(data);
    *((int*) data) =
        !server_error  &&  NStr::FindNoCase(header, "\nService: ") != NPOS
        ? 1
        : 2;
    return 1/*always success*/;
}
}


EIO_Status CConnTest::DispatcherOkay(string* reason)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(kTest);
    ConnNetInfo_SetupStandardArgs(net_info, kTest);

    PreCheck(eDispatcher, 0/*main*/,
             "Checking whether NCBI dispatcher is okay");

    int okay = 0;
    CConn_HttpStream http(net_info, kEmptyStr/*user_header*/,
                          s_ParseHeader, &okay, 0/*adjust*/, 0/*cleanup*/,
                          0/*flags*/, m_Timeout);
    CIOGuard guard(m_IO, http);
    char buf[1024];
    http.read(buf, sizeof(buf));
    CTempString str(buf, http.gcount());
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
                temp = "Make sure there are no stray [CONN]{HOST|PORT|PATH}"
                    " settings on the way in your configuration\n";
            }
            if (okay == 1) {
                temp += "Service response was not recognized; please contact "
                    NCBI_HELP_DESK "\n";
            }
        } else
            temp += x_TimeoutMsg();
        if (!(okay & 1)) {
            temp += "Check with your network administrator that your network"
                " neither filters out nor blocks non-standard HTTP headers\n";
        }
    }

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
    CIOGuard guard(m_IO, svc);
    svc << kTest << NcbiEndl;
    string temp;
    svc >> temp;
    bool responded = temp.size() > 0;
    EIO_Status status = ConnStatus(NStr::Compare(temp, kTest) != 0, &svc);

    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK";
    else {
        char* str = net_info ? SERV_ServiceName(kService) : 0;
        if (str  &&  NStr::CompareNocase(str, kService) == 0) {
            free(str);
            str = 0;
        }
        SERV_ITER iter = SERV_OpenSimple(kService);
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
                temp += "]CONN_SERVICE_NAME=\"";
                temp += str;
                temp += "\" from your configuration\n";
            } else if (status != eIO_Timeout  ||  m_Timeout > kTimeout)
                temp += "; please contact " NCBI_HELP_DESK "\n";
        }
        if (status != eIO_Timeout) {
            const char* mapper = SERV_MapperName(iter);
            if (!mapper  ||  NStr::CompareNocase(mapper, "DISPD") != 0) {
                temp += "Network dispatcher is not enabled as a service"
                    " locator;  please review your configuration to purge any"
                    " occurrences of [CONN]DISPD_DISABLE off your settings\n";
            }
        } else
            temp += x_TimeoutMsg();
        SERV_Close(iter);
        if (str)
            free(str);
    }

    PostCheck(eStatelessService, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);
    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::x_GetFirewallConfiguration(const SConnNetInfo* net_info)
{
    static const char kFWDUrl[] =
        "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/NETWORK/fwd_check.cgi";

    char fwdurl[128];
    if (!ConnNetInfo_GetValue(0, "FWD_URL", fwdurl, sizeof(fwdurl), kFWDUrl))
        return eIO_InvalidArg;
    CConn_HttpStream fwdcgi(fwdurl, net_info, kEmptyStr/*user hdr*/,
                            0/*flags*/, m_Timeout);
    CIOGuard guard(m_IO, fwdcgi);
    fwdcgi << "selftest" << NcbiEndl;

    char line[256];
    bool responded = false;
    while (!m_Canceled  &&  fwdcgi.getline(line, sizeof(line))) {
        responded = true;
        CTempString hostport, state;
        if (!NStr::SplitInTwo(line, "\t", hostport, state, NStr::eMergeDelims))
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
        cp.okay = okay;
        if (!fb)
            m_Fwd.push_back(cp);
        else
            m_FwdFB.push_back(cp);
    }

    return ConnStatus(!responded || (fwdcgi.fail() && !fwdcgi.eof()), &fwdcgi);
}


EIO_Status CConnTest::GetFWConnections(string* reason)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    if (net_info) {
        const char* user_header;
        net_info->req_method = eReqMethod_Post;
        if (net_info->firewall) {
            user_header = "NCBI-RELAY: FALSE";
            m_Firewall = true;
        } else
            user_header = "NCBI-RELAY: TRUE";
        if (net_info->stateless)
            m_Stateless = true;
        ConnNetInfo_OverrideUserHeader(net_info, user_header);
        ConnNetInfo_SetupStandardArgs(net_info, 0/*w/o service*/);
    }

    string temp(m_Firewall ? "FIREWALL" : "RELAY (legacy)");
    temp += " connection mode has been detected for stateful services\n";
    if (m_Firewall) {
        temp += "This mode requires your firewall to be configured in such a"
            " way that it allows outbound connections to the port range ["
            _STR(CONN_FWD_PORT_MIN) ".." _STR(CONN_FWD_PORT_MAX)
            "] (inclusive) at the two fixed NCBI hosts, 130.14.29.112"
            " and 165.112.7.12\n"
            "To set that up correctly, please have your network administrator"
            " read the following (if they have not already done so):"
            " " NCBI_FW_URL "\n";
    } else {
        temp += "This is an obsolescent mode that requires keeping a wide port"
            " range [4444..4544] (inclusive) open to let through connections"
            " to any NCBI host (130.14.2x.xxx/165.112.xx.xxx) -- this mode was"
            " designed for unrestricted networks when firewall port blocking"
            " was not an issue\n";
    }
    if (m_HttpProxy) {
        temp += "The first attempt to establish connections to the"
            " aforementioned ports will be made with an HTTP proxy '";
        temp += net_info->http_proxy_host;
        temp += ':';
        temp += NStr::UIntToString(net_info->http_proxy_port);
        temp += "'.  If that is unsuccessful, a link bypassing the proxy will"
            " then be attempted";
        if (m_Firewall  &&  *net_info->proxy_host) {
            temp += ": your configuration specifies that instead of connecting"
                " directly to NCBI, a forwarding non-transparent proxy host '";
            temp += net_info->proxy_host;
            temp += "' should be used for such links";
        }
        temp += '\n';
    }
    if (m_Firewall) {
        temp += "There are also fallback connection ports such as 22 and 443"
            " at 130.14.29.112.  They will be used if connections to the ports"
            " in the range described above have failed";
        if (m_HttpProxy  &&  *net_info->proxy_host) {
            temp += ", and will be tried with the HTTP proxy first, then ";
            temp += *net_info->proxy_host ? "via the forwarder" : "directly";
            temp += " if failed\n";
        }
    } else {
        temp += "This mode may not be reliable if your site has a restrictive"
            " firewall imposing fine-grained control over which hosts and"
            " ports the outbound connections are allowed to use\n";
    }

    PreCheck(eFirewallConnPoints, 0/*main*/, temp);

    PreCheck(eFirewallConnPoints, 1/*sub*/,
             "Obtaining current NCBI " +
             string(m_Firewall ? "firewall settings" : "service entries"));

    EIO_Status status = x_GetFirewallConfiguration(net_info);

    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success) {
        if (!m_Fwd.empty()) {
            stable_sort(m_Fwd.begin(),   m_Fwd.end());
            temp = "OK: ";
            temp += NStr::UInt8ToString(m_Fwd.size());
            if (!m_FwdFB.empty()) {
                stable_sort(m_FwdFB.begin(), m_FwdFB.end());
                temp += " + ";
                temp += NStr::UInt8ToString(m_FwdFB.size());
            }
            temp += m_Fwd.size() + m_FwdFB.size() == 1 ? " port" : " ports";
        } else {
            status = eIO_Unknown;
            temp = "No connection ports found, please contact " NCBI_HELP_DESK;
        }
    } else if (status == eIO_Timeout) {
        temp = x_TimeoutMsg();
        if (m_Timeout > kTimeout)
            temp += "You may want to contact " NCBI_HELP_DESK;
    } else
        temp = "Please contact " NCBI_HELP_DESK;

    PostCheck(eFirewallConnPoints, 1/*sub*/, status, temp);

    ConnNetInfo_Destroy(net_info);

    if (status == eIO_Success) {
        PreCheck(eFirewallConnPoints, 2/*sub*/,
                 "Verifying configuration for consistency");

        bool firewall = true;
        ITERATE(vector<CConnTest::CFWConnPoint>, cp, m_Fwd) {
            if (m_Canceled) {
                status = eIO_Interrupt;
                temp = kCanceled;
                break;
            }
            if (cp->port < CONN_FWD_PORT_MIN  ||  CONN_FWD_PORT_MAX < cp->port)
                firewall = false;
            if (!cp->okay) {
                status = eIO_NotSupported;
                temp = CSocketAPI::HostPortToString(cp->host, cp->port);
                temp += " is not operational, please contact " NCBI_HELP_DESK;
                break;
            }
        }
        if (status == eIO_Success) {
            if (m_Firewall != firewall) {
                status = eIO_Unknown;
                temp = "Configuration mismatch: firewall ";
                temp += firewall ? "wrongly" : "not";
                temp += " acknowledged, please contact " NCBI_HELP_DESK;
            } else
                temp.resize(2);
        }

        PostCheck(eFirewallConnPoints, 2/*sub*/, status, temp);
    }

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::CheckFWConnections(string* reason)
{
    static const STimeout kZeroTimeout = { 0, 0 };

    SConnNetInfo* net_info = ConnNetInfo_Create(0);

    TSOCK_Flags flags =
        (net_info  &&  net_info->debug_printout == eDebugPrintout_Data
         ? fSOCK_LogOn : fSOCK_LogDefault);

    PreCheck(eFirewallConnections, 0/*main*/,
             "Checking individual connection points..\n"
             "NOTE that even though that not the entire port range may"
             " currently be utilized and checked, in order for NCBI services"
             " to work correctly and seamlessly, your network must support all"
             " ports in the range as documented above\n");

    vector<CFWConnPoint>* fwd[] = { &m_Fwd, &m_FwdFB };

    size_t n;
    string temp;
    bool url = false;
    unsigned int m = 0;
    EIO_Status status = net_info ? eIO_Unknown : eIO_InvalidArg;
    for (n = 0;  net_info  &&  n < sizeof(fwd) / sizeof(fwd[0]);  n++) {
        if (fwd[n]->empty())
            continue;
        status = eIO_Success;

        typedef pair<CConn_SocketStream*,CFWConnPoint*> CFWCheck;
        vector<CFWCheck> v;

        // Spawn connections for all CPs
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, *fwd[n]) {
            SOCK_ntoa(cp->host, net_info->host, sizeof(net_info->host));
            net_info->port = cp->port;
            if (m_Canceled) {
                status = eIO_Interrupt;
                break;
            }
            CConn_SocketStream* fw = new CConn_SocketStream(*net_info,
                                                            "\r\n"/*data*/,
                                                            2/*size*/, flags);
            CIOGuard guard(m_IO, *fw);
            if (!fw->good()  ||  !fw->GetCONN()) {
                // Not constructed properly (very unlikely)
                status = eIO_InvalidArg;
                cp->okay = false;
                m_IO = 0;
                delete fw;
                fw = 0;
            }
            v.push_back(make_pair(fw, &*cp));
        }

        // Check results in random order but allow to modify status only once
        random_shuffle(v.begin(), v.end());
        ITERATE(vector<CFWCheck>, ck, v) {
            CConn_IOStream* is = ck->first;
            if (!is) {
                _ASSERT(status != eIO_Success);
                _ASSERT(!ck->second->okay);
                continue;
            }
            CIOGuard guard(m_IO, *is);
            if (m_Canceled)
                status = eIO_Interrupt;
            else if (status != eIO_Success) {
                size_t unused;
                is->SetTimeout(eIO_Read, &kZeroTimeout);
                CONN_Read(is->GetCONN(), 0, 1<<20, &unused, eIO_ReadPlain);
            } else {
                CONN conn = is->GetCONN();
                CFWConnPoint* cp = ck->second;
                status = CONN_Wait(conn, eIO_Read, &kZeroTimeout);
                if (status == eIO_Timeout  ||  status == eIO_Success) {
                    char line[sizeof(kFWSign) + 2/*"\r\0"*/];
                    if (!is->getline(line, sizeof(line)))
                        *line = '\0';
                    else if (NStr::strncasecmp(line,kFWSign,sizeof(kFWSign)-1))
                        cp->okay = false;
                    status = ConnStatus(!*line  ||  !cp->okay, is);
                    if (!cp->okay)
                        status = eIO_NotSupported;
                    else if (status != eIO_Success)
                        cp->okay = false;
                } else {
                    // Connection refused
                    ConnStatus(true, 0);
                    cp->okay = false;
                }
            }
            delete is;
        }

        // Report results (only the first failed one;  all otherwise)
        NON_CONST_ITERATE(vector<CFWConnPoint>, cp, *fwd[n]) {
            if (status == eIO_Interrupt)
                cp->okay = false;
            if (status != eIO_Success  &&  cp->okay)
                continue;
            PreCheck(eFirewallConnections, ++m, "Connectivity at "
                     + CSocketAPI::HostPortToString(cp->host, cp->port));
            if (!cp->okay) {
                temp.clear();
                _ASSERT(status != eIO_Success);
                if (status == eIO_Interrupt)
                    temp = kCanceled;
                else if  (n  ||  m_FwdFB.empty()) {
                    if (status == eIO_Timeout)
                        temp = x_TimeoutMsg();
                    if (m_HttpProxy) {
                        temp += "Your HTTP proxy '";
                        temp += net_info->http_proxy_host;
                        temp += ':';
                        temp += NStr::UIntToString(net_info->http_proxy_port);
                        temp += "' may not allow connections relayed to"
                            " non-conventional ports; please see your network"
                            " administrator";
                        if (!url) {
                            temp += " and let them read: " NCBI_FW_URL;
                            url = true;
                        }
                        temp += '\n';
                    }
                    if (m_Firewall  &&  *net_info->proxy_host) {
                        temp += "Your non-transparent proxy '";
                        temp += net_info->proxy_host;
                        temp += "' may not be forwarding connections properly,"
                            " please check with your network administrator"
                            " that the proxy has been configured correctly";
                        if (!url) {
                            temp += ": " NCBI_FW_URL;
                            url = true;
                        }
                        temp += '\n';
                    }
                    temp += "The network port required for this connection to"
                        " succeed may be blocked/diverted at your firewall;";
                    if (m_Firewall) {
                        temp += " please see your network administrator";
                        if (!url) {
                            temp += " and have them read the following: "
                                NCBI_FW_URL;
                            url = true;
                        }
                        temp += '\n';
                    } else {
                        temp += " try setting [CONN]FIREWALL=1 in your"
                            " configuration to use a more narrow"
                            " port range";
                        if (!url) {
                            temp += " per: " NCBI_FW_URL;
                            url = true;
                        }
                        temp += '\n';
                    }
                    if (!m_Stateless) {
                        temp +=  "Not all NCBI stateful services require to"
                            " work over dedicated (persistent) connections;"
                            " some can work (at the cost of degraded"
                            " performance) over a stateless carrier such as"
                            " HTTP;  try setting [CONN]STATELESS=1 in your"
                            " configuration\n";
                    }
                } else
                    temp = "Now checking fallback port(s)..";
                switch (status) {
                case eIO_Timeout:
                    m_CheckPoint = "Connection timed out";
                    break;
                case eIO_Closed:
                    if (m_CheckPoint.empty())
                        m_CheckPoint = "Connection refused";
                    else
                        m_CheckPoint = "Connection closed";
                    break;
                case eIO_Unknown:
                    m_CheckPoint = "Unknown error occurred";
                    break;
                case eIO_Interrupt:
                    m_CheckPoint = "Connection interrupted";
                    break;
                case eIO_NotSupported:
                    m_CheckPoint = "Unrecognized response received";
                    break;
                default:
                    m_CheckPoint = "Internal program error, please report";
                    break;
                }
            } else
                temp = "OK";
            PostCheck(eFirewallConnections, m, status, temp);
            if (!cp->okay)
                break;
        }
        if (status == eIO_Success  ||  m_FwdFB.empty())
            break;
    }

    if (status != eIO_Success  ||  n) {
        if (status != eIO_Success) {
            temp = m_Firewall ? "Firewall port" : "Service entry";
            temp += " check FAILED";
        } else
            temp = "Firewall port check PASSED only with fallback port(s)";
        if (!url) {
            temp += "; you may want to read this link for more information: "
                NCBI_FW_URL;
        }
    } else {
        temp = "All " + string(m_Firewall
                               ? "firewall port(s)"
                               : "service entry point(s)") + " checked OK";
    }

    PostCheck(eFirewallConnections, 0/*main*/, status, temp);

    ConnNetInfo_Destroy(net_info);

    if (reason)
        reason->swap(temp);
    return status;
}


EIO_Status CConnTest::StatefulOkay(string* reason)
{
    static const char kId2Init[] =
        "0\2000\200\242\200\240\200\5\0\0\0\0\0\0\0\0\0";
    static const char kId2[] = "ID2";
    char ry[sizeof(kFWSign)];

    SConnNetInfo* net_info = ConnNetInfo_Create(kId2);

    PreCheck(eStatefulService, 0/*main*/,
             "Checking reachability of a stateful service");

    CConn_ServiceStream id2(kId2, fSERV_Any, net_info, 0/*params*/, m_Timeout);
    CIOGuard guard(m_IO, id2);

    streamsize n = 0;
    bool iofail = !id2.write(kId2Init, sizeof(kId2Init) - 1)  ||  !id2.flush()
        ||  !(n = CStreamUtils::Readsome(id2, ry, sizeof(ry)-1));
    EIO_Status status = ConnStatus
        (iofail  ||  n < 4  ||  memcmp(ry, "0\200\240\200", 4) != 0, &id2);
    ry[n] = '\0';

    string temp;
    if (status == eIO_Interrupt)
        temp = kCanceled;
    else if (status == eIO_Success)
        temp = "OK";
    else {
        char* str = SERV_ServiceName(kId2);
        if (str  &&  NStr::CompareNocase(str, kId2) != 0) {
            temp = n ? "Unrecognized" : "No";
            temp += " response received from substituted service;"
                " please remove [";
            string upper(kId2);
            temp += NStr::ToUpper(upper);
            temp += "]CONN_SERVICE_NAME=\"";
            temp += str;
            temp += "\" from your configuration\n";
            free(str); // NB: still, str != NULL
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
                        " you may want to contact " NCBI_HELP_DESK "\n";
                } else if (m_Fwd.empty()) {
                    temp += "The most likely reason for the failure is that"
                        " your ";
                    temp += (net_info  &&  *net_info->proxy_host
                             ? "forwarding proxy" : "firewall");
                    temp += " is still blocking ports as reported above\n";
                } else if (status != eIO_Timeout  ||  m_Timeout > kTimeout)
                    temp += "Please contact " NCBI_HELP_DESK "\n";
                SERV_Close(iter);
            }
            if (status == eIO_Timeout)
                temp += x_TimeoutMsg();
        } else if (!str) {
            if (n  &&  net_info  &&  net_info->http_proxy_port
                &&  NStr::strncasecmp(ry, kFWSign, n) == 0) {
                temp += "NCBI Firewall";
                if (!net_info->firewall)
                    temp += " (Connection Relay)";
                temp += " Daemon reports negotitation error, which usually"
                    " means that an intermediate HTTP proxy '";
                temp += net_info->http_proxy_host;
                temp += ':';
                temp += NStr::UIntToString(net_info->http_proxy_port);
                if ((m_Firewall  ||  net_info->firewall)
                    &&  *net_info->proxy_host) {
                    temp += "' and/or connection forwarder '";
                    temp += net_info->proxy_host;
                }
                temp += "' may be buggy."
                    " Please see your network administrator\n";
            } else {
                temp += n ? "Unrecognized" : "No";
                temp += " response from service;"
                    " please contact " NCBI_HELP_DESK "\n";
            }
        }
    }

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

    if (!m_Output)
        return;

    list<string> stmt;
    NStr::Split(title, "\n", stmt);
    SIZE_TYPE size = stmt.size();
    *m_Output << NcbiEndl << stmt.front() << '.';
    stmt.pop_front();
    if (size > 1) {
        ERASE_ITERATE(list<string>, str, stmt) {
            if (str->empty())
                stmt.erase(str);
        }
        if (stmt.size()) {
            *m_Output << NcbiEndl;
            NON_CONST_ITERATE(list<string>, str, stmt) {
                NStr::TruncateSpacesInPlace(*str);
                str->append(1, '.');
                list<string> par;
                NStr::Justify(*str, m_Width, par, kEmptyStr, string(4, ' '));
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

    if (status == eIO_Success) {
        *m_Output << "\n\t"[!end] << reason << '!' << NcbiEndl;
        return;
    }

    list<string> stmt;
    NStr::Split(reason, "\n", stmt);
    ERASE_ITERATE(list<string>, str, stmt) {
        if (str->empty())
            stmt.erase(str);
    }

    if (!end  ||  !stmt.size()) {
        *m_Output << "\n\t"[!end] << "FAILED (" << IO_StatusStr(status) << ')';
        if (stmt.size()) {
            const string& where = GetCheckPoint();
            if (!where.empty()) {
                *m_Output << ':' << NcbiEndl << string(4, ' ') << where;
            }
            *m_Output << NcbiEndl;
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
    CConn_HttpStream http("http://www.ncbi.nlm.nih.gov/Service/getenv.cgi",
                          fHCC_KeepHeader | fHCC_NoAutoRetry, &kFast);
    char line[1024];
    if (!http  ||  !http.getline(line, sizeof(line)))
        return false;
    int code;
    return !(::sscanf(line, "HTTP/%*d.%*d %d ", &code) < 1  ||  code != 200);
}


END_NCBI_SCOPE
