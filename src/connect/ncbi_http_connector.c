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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for the HTTP-based network connection
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_base64.h>
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_HTTP


#define HTTP_SOAK_READ_SIZE  16384


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/


/* Connection states, see table below
 */
enum EConnState {
    eCS_NotInitiated =   0,
    eCS_WriteRequest =   1,
    eCS_FlushRequest =   2,
    eCS_ReadHeader   =   3,
    eCS_ReadBody     =   4,
    eCS_DoneBody     =   5,  /* NB: |eCS_ReadBody */
    eCS_Discard      =   7,  /* NB: |eCS_DoneBody */
    eCS_Eom          = 0xF   /* NB: |eCS_Discard  */
};
typedef unsigned EBConnState;   /* packed EConnState */


/* Whether the connector is allowed to connect
 * NB:  In order to be able to connect, fCC_Once must be set.
 */
enum ECanConnect {
    fCC_None,      /* 0   */
    fCC_Once,      /*   1 */
    fCC_Unlimited  /* 0|2 */
};
typedef unsigned EBCanConnect;  /* packed ECanConnect */

typedef unsigned EBSwitch;      /* packed ESwitch     */

typedef enum {
    eEM_Drop,      /*   0 */
    eEM_Wait,      /* 1   */
    eEM_Read,      /*   2 */
    eEM_Flush      /* 1|2 */
} EExtractMode;


typedef enum {
    eRetry_None              = 0,  /* 0   */
    eRetry_Unused            = 1,  /* yet */
    eRetry_Redirect          = 2,  /* 2   */
    eRetry_Redirect303       = 3,  /* 2|1 */
    eRetry_Authenticate      = 4,  /* 4   */
    eRetry_ProxyAuthenticate = 5   /* 4|1 */
} ERetry;

typedef struct {
    ERetry      mode;
    const char* data;
} SRetry;


/* All internal data necessary to perform the (re)connect and I/O.
 *
 *  The following connection states are defined:                    |  sock?
 * --------------+--------------------------------------------------+---------
 *  NotInitiated | HTTP request needs to be (re-)initiated          | 
 *  WriteRequest | HTTP request body is being written               | != NULL
 *  FlushRequest | HTTP request is being completed and flushed out  | != NULL
 *   ReadHeader  | HTTP response header is being read               | != NULL
 *    ReadBody   | HTTP response body is being read                 | != NULL
 *    DoneBody   | HTTP body is all read out of the connection      | != NULL
 *    Discard    | HTTP data (if any) to be discarded               | != NULL
 *      Eom      | HTTP message has been completed (end of message) |
 * --------------+--------------------------------------------------+---------
 */
typedef struct {
    SConnNetInfo*     net_info;       /* network configuration parameters    */
    FHTTP_ParseHeader parse_header;   /* callback to parse HTTP reply header */
    void*             user_data;      /* user data handle for callbacks (CB) */
    FHTTP_Adjust      adjust;         /* on-the-fly net_info adjustment CB   */
    FHTTP_Cleanup     cleanup;        /* cleanup callback                    */

    THTTP_Flags       flags;          /* as passed to constructor            */
    EBSwitch          unsafe_redir:2; /* if unsafe redirects are allowed     */
    EBSwitch          error_header:2; /* only err.HTTP header on SOME debug  */
    EBCanConnect      can_connect:2;  /* whether more connections permitted  */
    EBConnState       conn_state:4;   /* connection state per table above    */
    unsigned          auth_done:1;    /* website authorization sent          */
    unsigned    proxy_auth_done:1;    /* proxy authorization sent            */
    unsigned          skip_host:1;    /* do *not* add the "Host:" header tag */
    unsigned          keepalive:1;    /* keep-alive connection               */
    unsigned          chunked:1;      /* if writing/reading chunked, HTTP/1.1*/
    unsigned          entity:1;       /* if there's entity payload (body)/1.1*/
    unsigned          reused:1;       /* if connection was re-used           */
    unsigned          retry:1;        /* if the request is being re-tried    */
    unsigned          reserved:14;
    unsigned char     unused[3];
    unsigned char     minor_fault;    /* incr each minor failure since majo  */
    unsigned short    major_fault;    /* incr each major failure since open  */
    unsigned short    http_code;      /* last HTTP response code             */

    SOCK              sock;           /* socket;  NULL if not connected      */
    const STimeout*   o_timeout;      /* NULL(infinite), dflt or ptr to next */
    STimeout          oo_timeout;     /* storage for (finite) open timeout   */
    const STimeout*   w_timeout;      /* NULL(infinite), dflt or ptr to next */
    STimeout          ww_timeout;     /* storage for a (finite) write tmo    */

    BUF               http;           /* storage for HTTP reply              */
    BUF               r_buf;          /* storage to accumulate input data    */
    BUF               w_buf;          /* storage to accumulate output data   */
    size_t            w_len;          /* pending message body size           */

    TNCBI_BigCount    expected;       /* expected to receive before end/EOF  */
    TNCBI_BigCount    received;       /* actually received so far            */
} SHttpConnector;


static const char     kHttpHostTag[] = "Host: ";
static const STimeout kZeroTimeout   = {0, 0};


/* NCBI messaging support */
static int                   s_MessageIssued = 0;
static FHTTP_NcbiMessageHook s_MessageHook   = 0;


static int/*bool*/ x_UnsafeRedirectOK(SHttpConnector* uuu)
{
    if (uuu->unsafe_redir == eDefault) {
        if (!(uuu->flags & fHTTP_UnsafeRedirects)) {
            char val[32];
            ConnNetInfo_GetValue(0, "HTTP_UNSAFE_REDIRECTS",
                                 val, sizeof(val), 0);
            uuu->unsafe_redir = ConnNetInfo_Boolean(val) ? eOn : eOff;
        } else
            uuu->unsafe_redir = eOn;
    }
    return uuu->unsafe_redir == eOn ? 1/*true*/ : 0/*false*/;
}


typedef enum {
    /* Negative code: NOP;  positive: Error */
    eHTTP_AuthMissing = -3,  /* No auth info available  */
    eHTTP_AuthUnsafe  = -2,  /* Auth would be unsafe    */
    eHTTP_AuthDone    = -1,  /* Auth already sent       */
    eHTTP_AuthOK      =  0,  /* Success                 */
    eHTTP_AuthError   =  1,  /* Auth can't/doesn't work */
    eHTTP_AuthIllegal =  3,  /* Auth won't work now     */
    eHTTP_AuthNotSupp =  2,  /* Unknown auth parameters */
    eHTTP_AuthConnect =  4   /* Auth with CONNECT       */
} EHTTP_Auth;


static EHTTP_Auth x_Authenticate(SHttpConnector* uuu,
                                 ERetry          auth,
                                 int/*bool*/     retry)
{
    static const char kProxyAuthorization[] = "Proxy-Authorization: Basic ";
    static const char kAuthorization[]      = "Authorization: Basic ";
    char buf[80 + (CONN_USER_LEN + CONN_PASS_LEN)*3], *s;
    size_t taglen, userlen, passlen, len, n;
    const char *tag, *user, *pass;

    switch (auth) {
    case eRetry_Authenticate:
        if (uuu->auth_done)
            return eHTTP_AuthDone;
        tag    = kAuthorization;
        taglen = sizeof(kAuthorization) - 1;
        user   = uuu->net_info->user;
        pass   = uuu->net_info->pass;
        break;
    case eRetry_ProxyAuthenticate:
        if (uuu->proxy_auth_done)
            return eHTTP_AuthDone;
        if (!uuu->net_info->http_proxy_host[0]  ||
            !uuu->net_info->http_proxy_port) {
            return retry ? eHTTP_AuthError : eHTTP_AuthMissing;
        }
        tag    = kProxyAuthorization;
        taglen = sizeof(kProxyAuthorization) - 1;
        user   = uuu->net_info->http_proxy_user;
        pass   = uuu->net_info->http_proxy_pass;
        break;
    default:
        assert(0);
        return eHTTP_AuthError;
    }
    assert(tag  &&  user  &&  pass);
    if (!*user)
        return eHTTP_AuthMissing;
    if (retry  &&  uuu->entity)
        return eHTTP_AuthIllegal;
    if (auth == eRetry_Authenticate) {
        if (uuu->net_info->scheme != eURL_Https
            &&  !x_UnsafeRedirectOK(uuu)) {
            return eHTTP_AuthUnsafe;
        }
        uuu->auth_done = 1/*true*/;
    } else
        uuu->proxy_auth_done = 1/*true*/;
    userlen = strlen(user);
    passlen = strlen(pass);
    s = buf + sizeof(buf) - passlen;
    if (pass[0] == '['  &&  pass[passlen - 1] == ']') {
        if (BASE64_Decode(pass + 1, passlen - 2, &len, s, passlen, &n)
            &&  len == passlen - 2) {
            len = n;
        } else
            len = 0;
    } else
        len = 0;
    s -= userlen;
    memcpy(--s, user, userlen);
    s[userlen++] = ':';
    if (!len) {
        memcpy(s + userlen, pass, passlen);
        len  = userlen + passlen;
    } else
        len += userlen;
    /* usable room */
    userlen = (size_t)(s - buf);
    assert(userlen > taglen);
    userlen -= taglen + 1;
    passlen = 0;
    BASE64_Encode(s, len, &n, buf + taglen, userlen, &passlen, &passlen/*0*/);
    if (len != n  ||  buf[taglen + passlen])
        return eHTTP_AuthError;
    memcpy(buf, tag, taglen);
    return (EHTTP_Auth) !ConnNetInfo_OverrideUserHeader(uuu->net_info, buf);
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_SameScheme(EBURLScheme scheme1, EBURLScheme scheme2)
{
    if (!scheme1)
        scheme1 = eURL_Http;
    if (!scheme2)
        scheme2 = eURL_Http;
    return scheme1 == scheme2 ? 1/*true*/ : 0/*false*/;
}


static int/*bool*/ x_SameHost(const char* host1, const char* host2)
{
    char buf1[CONN_HOST_LEN+1], buf2[CONN_HOST_LEN+1];
    unsigned int ip1, ip2;
 
    if (strcasecmp(host1, host2) == 0)
        return 1/*true*/;
    if (!(ip1 = SOCK_gethostbyname(host1))  ||  ip1 == (unsigned int)(-1))
        return 0/*false*/;
    if (!(ip2 = SOCK_gethostbyname(host2))  ||  ip2 == (unsigned int)(-1))
        return 0/*false*/;
    if (ip1 == ip2)
        return 1/*true*/;
    SOCK_gethostbyaddr(ip1, buf1, sizeof(buf1));
    SOCK_gethostbyaddr(ip2, buf2, sizeof(buf2));
    return *buf1  &&  strcasecmp(buf1, buf2) == 0 ? 1/*true*/ : 0/*false*/;
}


static unsigned short x_PortForScheme(unsigned port, EBURLScheme scheme)
{
    if (port)
        return port;
    switch (scheme) {
    case eURL_Http:
        break;
    case eURL_Https:
        return CONN_PORT_HTTPS;
    default:
        assert(!scheme);
        break;
    }
    return CONN_PORT_HTTP;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_SamePort(unsigned short port1, EBURLScheme scheme1,
                              unsigned short port2, EBURLScheme scheme2)
{
    return x_PortForScheme(port1, scheme1) == x_PortForScheme(port2, scheme2)
        ? 1/*true*/ : 0/*false*/;
}


static int/*bool*/ s_CallAdjust(SHttpConnector* uuu, unsigned int arg)
{
    int retval;
    SConnNetInfo* net_info = ConnNetInfo_CloneInternal(uuu->net_info);
    if (!net_info)
        return 0/*failure*/;
    retval = uuu->adjust(uuu->net_info, uuu->user_data, arg);
    if (retval/*advisory of no change if < 0 but we don't trust it :-)*/) {
        int same_host = -1/*undef*/;
        if (uuu->sock) {
            int close = 0/*false*/;
            if (!x_SameHost(uuu->net_info->http_proxy_host,
                                 net_info->http_proxy_host)
                ||  (uuu->net_info->http_proxy_host[0]  &&
                     uuu->net_info->http_proxy_port !=
                          net_info->http_proxy_port)){
                close = 1/*true*/;
            } else if (net_info->http_proxy_host[0]  &&
                       net_info->http_proxy_port) {
                if (net_info->scheme == eURL_Https) {
                    if (uuu->net_info->scheme != eURL_Https)
                        close = 1/*true*/;
                    else if (!(same_host = !strcasecmp(uuu->net_info->host,
                                                            net_info->host)) ||
                             !x_SamePort(uuu->net_info->port,
                                         uuu->net_info->scheme,
                                              net_info->port,
                                              net_info->scheme)) {
                        close = 1/*true*/;
                    }
                } else if (!net_info->http_proxy_only)
                    close = 1/*true*/;
                /* connection reused with HTTP and w/CONNECT: HTTP -> HTTPS */
            } else if (!x_SameScheme(uuu->net_info->scheme,
                                          net_info->scheme)             ||
                       !(same_host = !strcasecmp(uuu->net_info->host,
                                                      net_info->host))  ||
                       !x_SamePort(uuu->net_info->port,
                                   uuu->net_info->scheme,
                                        net_info->port,
                                   net_info->scheme)) {
                close = 1/*true*/;
            }
            if (close) {
                SOCK_Destroy(uuu->sock);
                uuu->sock = 0;
            }
        }
        if (!same_host  ||  uuu->net_info->port != net_info->port
            ||  (same_host < 0
                 &&  strcasecmp(uuu->net_info->host,
                                     net_info->host) != 0)) {
            /* drop the flag on host / port replaced */
            uuu->skip_host = 0/*false*/;
        }
    }
    ConnNetInfo_Destroy(net_info);
    return retval;
}


/* NB: treatment of 'host_from' and 'host_to' is not symmetrical */
static int/*bool*/ x_RedirectOK(EBURLScheme    scheme_to,
                                const char*      host_to,
                                unsigned short   port_to,
                                EBURLScheme    scheme_from,
                                const char*      host_from,
                                unsigned short   port_from)
{
    char buf1[CONN_HOST_LEN+1], buf2[CONN_HOST_LEN+1];
    unsigned int ip1, ip2;
    if ((port_to | port_from)
        &&  !x_SamePort(port_to, scheme_to, port_from, scheme_from)) {
        return 0/*false*/;
    }
    if (strcasecmp(host_to, host_from) == 0)
        return 1/*true*/;
    if (!SOCK_isipEx(host_from, 1/*full-quad*/))
        return 0/*false*/;
    if ((ip1 = SOCK_gethostbyname(host_from)) == (unsigned int)(-1))
        ip1 = 0;
    if (!ip1  ||  !SOCK_gethostbyaddr(ip1, buf1, sizeof(buf1)))
        strncpy0(buf1, host_from, sizeof(buf1) - 1);
    else if (strcasecmp(buf1, host_to) == 0)
        return 1/*true*/;
    if ((ip2 = SOCK_gethostbyname(host_to)) == (unsigned int)(-1))
        ip2 = 0;
    if (ip1/*&&  ip2*/  &&  ip1 == ip2)
        return 1/*true*/;
    if (!ip2  ||  !SOCK_gethostbyaddr(ip2, buf2, sizeof(buf2)))
        strncpy0(buf2, host_to, sizeof(buf2) - 1);
    return strcasecmp(buf1, buf2) == 0 ? 1/*true*/ : 0/*false*/;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsWriteThru(const SHttpConnector* uuu)
{
    return !uuu->net_info->http_version  ||  !(uuu->flags & fHTTP_WriteThru)
        ? 0/*false*/ : 1/*true*/;
}


typedef enum {
    eHTTP_RedirectInvalid = -2,
    eHTTP_RedirectUnsafe  = -1,
    eHTTP_RedirectOK      =  0,
    eHTTP_RedirectError   =  1,
    eHTTP_RedirectTunnel  =  2
} EHTTP_Redirect;


static EHTTP_Redirect x_Redirect(SHttpConnector* uuu, const SRetry* retry)
{
    EBURLScheme    scheme = uuu->net_info->scheme;
    EReqMethod     req_method = (EReqMethod) uuu->net_info->req_method;
    char           host[sizeof(uuu->net_info->host)];
    unsigned short port = uuu->net_info->port;
    int/*bool*/    unsafe;

    strcpy(host, uuu->net_info->host);
    if (req_method == eReqMethod_Any)
        req_method  = BUF_Size(uuu->w_buf) ? eReqMethod_Post : eReqMethod_Get;
    ConnNetInfo_SetArgs(uuu->net_info, "");  /*arguments not inherited*/
 
    if (!ConnNetInfo_ParseURL(uuu->net_info, retry->data))
        return eHTTP_RedirectError;

    unsafe = scheme == eURL_Https  &&  uuu->net_info->scheme != eURL_Https
        ? 1/*true*/ : 0/*false*/;

    if (req_method == eReqMethod_Put   ||
        req_method == eReqMethod_Post  ||
        req_method == eReqMethod_Delete) {
        if (uuu->net_info->req_method == eReqMethod_Post
            &&  retry->mode == eRetry_Redirect303) {
            uuu->net_info->req_method  = eReqMethod_Get;
            BUF_Erase(uuu->w_buf);
        } else {
            if (x_IsWriteThru(uuu)  &&  BUF_Size(uuu->w_buf))
                return eHTTP_RedirectInvalid;
            if (!x_RedirectOK(uuu->net_info->scheme,
                              uuu->net_info->host,
                              uuu->net_info->port,
                                             scheme,
                                             host,
                                             port)) {
                unsafe = 1/*true*/;
            }
        }
    }

    if (unsafe  &&  !x_UnsafeRedirectOK(uuu))
        return eHTTP_RedirectUnsafe;

    if ((uuu->flags & fHTTP_AdjustOnRedirect)  &&  uuu->adjust) {
        if (!s_CallAdjust(uuu, 0))
            return eHTTP_RedirectError;
    } else if (port  !=   uuu->net_info->port  ||
               strcasecmp(uuu->net_info->host, host) != 0) {
        /* drop the flag on host / port replaced */
        uuu->skip_host = 0/*false*/;
    }

    return eHTTP_RedirectOK;
}


/* Try to fix connection parameters (called for an unconnected connector) */
static EIO_Status s_Adjust(SHttpConnector* uuu,
                           const SRetry*   retry,
                           EExtractMode    extract)
{
    EIO_Status status;
    const char* msg;

    assert(!retry  ||  !retry->data  ||  *retry->data);
    assert(!uuu->sock  &&  uuu->can_connect != fCC_None);

    uuu->retry = 0;
    if (uuu->reused) {
        assert(!uuu->sock  &&  (!retry  ||  !retry->data));
        if (uuu->entity)
            return eIO_Unknown;
        uuu->retry = 1;
        return eIO_Success;
    }
    if (!retry  ||  !retry->mode  ||  uuu->minor_fault > 5) {
        uuu->minor_fault = 0;
        uuu->major_fault++;
    } else
        uuu->minor_fault++;

    if (uuu->major_fault >= uuu->net_info->max_try) {
        msg = extract != eEM_Drop  &&  uuu->major_fault > 1
            ? "[HTTP%s%s]  Too many failed attempts (%hu), giving up" : "";
    } else if (retry  &&  retry->mode) {
        int secure = uuu->net_info->scheme == eURL_Https ? 1/*T*/ : 0/*F*/;
        char*  url = ConnNetInfo_URL(uuu->net_info);
        int   fail = 0;
        switch (retry->mode) {
        case eRetry_Redirect:
            if (uuu->entity/*FIXME*/)
                fail = eHTTP_RedirectInvalid;
            /*FALLTHRU*/
        case eRetry_Redirect303:
            if (!fail) {
                if (uuu->net_info->req_method == eReqMethod_Connect)
                    fail = eHTTP_RedirectTunnel;
                else if (!retry->data  ||  *retry->data == '?')
                    fail = eHTTP_RedirectError;
                else
                    fail = x_Redirect(uuu, retry);
            }
            if (fail) {
                const char* reason;
                switch (fail) {
                case eHTTP_RedirectInvalid:
                    reason = "Invalid";
                    status = eIO_Unknown;
                    break;
                case eHTTP_RedirectUnsafe:
                    reason = "Prohibited";
                    status = eIO_NotSupported;
                    break;
                case eHTTP_RedirectError:
                    reason = "Cannot";
                    status = eIO_Unknown;
                    break;
                case eHTTP_RedirectTunnel:
                    reason = "Spurious tunnel";
                    status = eIO_InvalidArg;
                    break;
                default:
                    reason = "Unknown failure of";
                    status = eIO_Unknown;
                    assert(0);
                    break;
                }
                assert(status != eIO_Success);
                CORE_LOGF_X(2, eLOG_Error,
                            ("[HTTP%s%s]  %s %s%s to %s%s%s",
                             url ? "; " : "",
                             url ? url  : "",
                             reason,
                             fail == eHTTP_RedirectUnsafe  &&  secure
                             ? "insecure " : "",
                             fail > eHTTP_RedirectError
                             ||  retry->mode != eRetry_Redirect303
                             ? "redirect" : "submission",
                             retry->data ? "\""        : "<",
                             retry->data ? retry->data : "NULL",
                             retry->data ? "\""        : ">"));
            } else {
                CORE_LOGF_X(17, eLOG_Trace,
                            ("[HTTP%s%s]  %s \"%s\"",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Redirect303
                             ? "Finishing submission with" : "Redirecting to",
                             retry->data));
                status = eIO_Success;
            }
            break;
        case eRetry_Authenticate:
            if (uuu->net_info->req_method == eReqMethod_Connect)
                fail = eHTTP_AuthConnect;
            /*FALLTHRU*/
        case eRetry_ProxyAuthenticate:
            if (!fail) {
                if (!retry->data
                    ||  strncasecmp(retry->data, "basic",
                                    strcspn(retry->data, " \t")) != 0) {
                    fail = eHTTP_AuthNotSupp;
                } else
                    fail = x_Authenticate(uuu, retry->mode, 1/*retry*/);
            }
            if (fail) {
                const char* reason;
                switch (fail) {
                case eHTTP_AuthConnect:
                    reason = "not allowed with CONNECT";
                    status = eIO_Unknown;
                    break;
                case eHTTP_AuthNotSupp:
                    reason = "not implemented";
                    status = eIO_NotSupported;
                    break;
                case eHTTP_AuthIllegal:
                    reason = "cannot be done at this point";
                    status = eIO_InvalidArg;
                    break;
                case eHTTP_AuthDone:
                case eHTTP_AuthError:
                    reason = "failed";
                    status = eIO_Unknown;
                    break;
                case eHTTP_AuthUnsafe:
                    reason = "prohibited";
                    status = eIO_NotSupported;
                    break;
                case eHTTP_AuthMissing:
                    reason = "required";
                    status = eIO_Unknown;
                    break;
                default:
                    reason = "unknown failure";
                    status = eIO_NotSupported;
                    assert(0);
                    break;
                }
                assert(status != eIO_Success);
                CORE_LOGF_X(3, eLOG_Error,
                            ("[HTTP%s%s]  %s %s %c%s%c",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Authenticate
                             ? "Authorization" : "Proxy authorization",
                             reason,
                             "(["[!retry->data],
                             retry->data ? retry->data : "NULL",
                             ")]"[!retry->data]));
            } else {
                CORE_LOGF_X(18, eLOG_Trace,
                            ("[HTTP%s%s]  Authorizing%s",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Authenticate
                             ? "" : " proxy"));
                status = eIO_Success;
            }
            break;
        default:
            CORE_LOGF_X(4, eLOG_Critical,
                        ("[HTTP%s%s]  Unknown retry mode #%u",
                         url ? "; " : "",
                         url ? url  : "", (unsigned int) retry->mode));
            status = eIO_InvalidArg;
            assert(0);
            break;
        }
        if (url)
            free(url);
        if (status != eIO_Success)
            uuu->can_connect = fCC_None;
        return status;
    } else if (uuu->adjust  &&  !s_CallAdjust(uuu, uuu->major_fault)) {
        msg = extract != eEM_Drop  &&  uuu->major_fault > 1
            ? "[HTTP%s%s]  Retry attempts (%hu) exhausted, giving up" : "";
    } else
        return eIO_Success;

    assert(msg);
    if (*msg) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(1, eLOG_Error,
                    (msg,
                     url ? "; " : "",
                     url ? url  : "", uuu->major_fault));
        if (url)
            free(url);
    }
    uuu->can_connect = fCC_None;
    return eIO_Unknown;
}


static char* x_HostPort(size_t reserve, const char* host, unsigned short xport)
{
    size_t hostlen = strlen(host), portlen;
    char*  hostport, port[16];
 
    if (!xport) {
        portlen = 1;
        port[0] = '\0';
    } else
        portlen = (size_t) sprintf(port, ":%hu", xport) + 1;
    hostport = (char*) malloc(reserve + hostlen + portlen);
    if (hostport) {
        memcpy(hostport + reserve, host, hostlen);
        hostlen        += reserve;
        memcpy(hostport + hostlen, port, portlen);
    }
    return hostport;
}


static int/*bool*/ x_SetHttpHostTag(SConnNetInfo* net_info)
{
    char*          tag;
    int/*bool*/    retval;
    unsigned short port = net_info->port;

    if (port  &&  port == x_PortForScheme(0, net_info->scheme))
        port = 0;
    tag = x_HostPort(sizeof(kHttpHostTag)-1, net_info->host, port);
    if (tag) {
        memcpy(tag, kHttpHostTag, sizeof(kHttpHostTag)-1);
        retval = ConnNetInfo_OverrideUserHeader(net_info, tag);
        free(tag);
    } else
        retval = 0/*failure*/;
    return retval;
}


static void x_SetRequestIDs(SConnNetInfo* net_info)
{
    char* id = CORE_GetNcbiRequestID(eNcbiRequestID_SID);
    assert(!id  ||  *id);
    if (id) {
        char*  tag;
        size_t len = strlen(id);
        if (!(tag = (char*) realloc(id, ++len + sizeof(HTTP_NCBI_SID)))) {
            ConnNetInfo_DeleteUserHeader(net_info, HTTP_NCBI_SID);
            free(id);
        } else {
            memmove(tag + sizeof(HTTP_NCBI_SID), tag, len);
            memcpy (tag,         HTTP_NCBI_SID, sizeof(HTTP_NCBI_SID) - 1);
            tag[sizeof(HTTP_NCBI_SID) - 1] = ' ';
            ConnNetInfo_OverrideUserHeader(net_info, tag);
            free(tag);
        }
    }
    id = CORE_GetNcbiRequestID(eNcbiRequestID_HitID);
    assert(!id  ||  *id);
    if (id) {
        char*  tag;
        size_t len = strlen(id);
        if (!(tag = (char*) realloc(id, ++len + sizeof(HTTP_NCBI_PHID)))) {
            ConnNetInfo_DeleteUserHeader(net_info, HTTP_NCBI_PHID);
            free(id);
        } else {
            memmove(tag + sizeof(HTTP_NCBI_PHID), tag, len);
            memcpy (tag,         HTTP_NCBI_PHID, sizeof(HTTP_NCBI_PHID) - 1);
            tag[sizeof(HTTP_NCBI_PHID) - 1] = ' ';
            ConnNetInfo_OverrideUserHeader(net_info, tag);
            free(tag);
        }
    }
}


/*ARGSUSED*/
static int s_TunnelAdjust(SConnNetInfo* net_info, void* data, unsigned int arg)
{
    SHttpConnector* uuu = (SHttpConnector*) data;
    uuu->minor_fault = 0;
    uuu->major_fault++;
    return -1/*noop*/;
}


/* Connect to the HTTP server, specified by uuu->net_info's "port:host".
 * Return eIO_Success only if socket connection has succeeded and uuu->sock
 * is non-zero.  If unsuccessful, try to adjust uuu->net_info with s_Adjust(),
 * and then re-try the connection attempt.
 */
static EIO_Status s_Connect(SHttpConnector* uuu,
                            const STimeout* timeout,
                            EExtractMode    extract)
{
    EIO_Status status;
    SOCK sock;

    assert(uuu->conn_state == eCS_NotInitiated || uuu->conn_state == eCS_Eom);

    uuu->http_code = 0;
    if (!(uuu->can_connect & fCC_Once)) {
        if (extract == eEM_Read  &&  uuu->net_info->max_try
            &&  uuu->can_connect == fCC_None) {
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(5, eLOG_Critical,
                        ("[HTTP%s%s]  Connector is no longer usable",
                         url ? "; " : "",
                         url ? url  : ""));
            if (url)
                free(url);
        }
        return eIO_Unknown;
    }

    if (uuu->conn_state == eCS_Eom) {
        if (uuu->adjust) {
            int retval = s_CallAdjust(uuu, (unsigned int)(-1));
            if (!retval)
                return eIO_Unknown;
            if (retval < 0)
                return eIO_Closed;
        } else
            return eIO_Closed;
        uuu->conn_state  = eCS_NotInitiated;
    }

    uuu->entity = 0;
    uuu->chunked = 0;
    /* the re-try loop... */
    for (;;) {
        TSOCK_Flags flags
            = (uuu->net_info->debug_printout == eDebugPrintout_Data
               ? fSOCK_KeepAlive | fSOCK_LogOn
               : fSOCK_KeepAlive | fSOCK_LogDefault);
        sock = uuu->sock;
        uuu->sock = 0;
        uuu->reused = sock ? 1/*true*/ : 0/*false*/;
        if ((!sock  ||  !SOCK_IsSecure(sock))
            &&  uuu->net_info->req_method != eReqMethod_Connect
            &&  uuu->net_info->scheme == eURL_Https
            &&  uuu->net_info->http_proxy_host[0]
            &&  uuu->net_info->http_proxy_port
            && !uuu->net_info->http_proxy_only) {
            SConnNetInfo* net_info = ConnNetInfo_Clone(uuu->net_info);
            uuu->reused = 0/*false*/;
            if (!net_info) {
                status = eIO_Unknown;
                break;
            }
            net_info->scheme    = eURL_Http;
            net_info->user[0]   = '\0';
            net_info->pass[0]   = '\0';
            if (net_info->port == 0)
                net_info->port  = CONN_PORT_HTTPS;
            net_info->firewall  = 0/*false*/;
            ConnNetInfo_DeleteUserHeader(net_info, kHttpHostTag);
            status = HTTP_CreateTunnelEx(net_info, fHTTP_NoUpread,
                                         0, 0, uuu, s_TunnelAdjust, &sock);
            assert((status == eIO_Success) ^ !sock);
            ConnNetInfo_Destroy(net_info);
        } else
            status  = eIO_Success;
        if (status == eIO_Success) {
            EReqMethod     req_method = (EReqMethod) uuu->net_info->req_method;
            int/*bool*/    reset_user_header;
            char*          http_user_header;
            const char*    host;
            unsigned short port;
            const char*    path;
            const char*    args;
            char*          temp;
            size_t         len;

            /* RFC7230 now requires Host: for CONNECT just as well */
            if (!uuu->skip_host  &&  !x_SetHttpHostTag(uuu->net_info)) {
                status = eIO_Unknown;
                break;
            }
            if (x_IsWriteThru(uuu)) {
                assert(req_method != eReqMethod_Connect);
                if (req_method == eReqMethod_Any) {
                    req_method  = BUF_Size(uuu->w_buf)
                        ? eReqMethod_Post
                        : eReqMethod_Get;
                }
                if (req_method != eReqMethod_Head  &&
                    req_method != eReqMethod_Get) {
                    if (!ConnNetInfo_OverrideUserHeader
                        (uuu->net_info, "Transfer-Encoding: chunked")) {
                        status = eIO_Unknown;
                        break;
                    }
                    uuu->chunked = 1;
                }
                len = (size_t)(-1L);
            } else
                len = BUF_Size(uuu->w_buf);
            if (req_method == eReqMethod_Connect
                ||  (uuu->net_info->scheme != eURL_Https
                     &&  uuu->net_info->http_proxy_host[0]
                     &&  uuu->net_info->http_proxy_port)) {
                if (uuu->net_info->http_push_auth
                    &&  x_Authenticate(uuu, eRetry_ProxyAuthenticate, 0) > 0) {
                    status = eIO_Unknown;
                    break;
                }
                host = uuu->net_info->http_proxy_host;
                port = uuu->net_info->http_proxy_port;
                path = ConnNetInfo_URL(uuu->net_info);
                if (!path) {
                    status = eIO_Unknown;
                    break;
                }
                if (req_method == eReqMethod_Connect) {
                    /* Tunnel (RFC2817) */
                    assert(!uuu->net_info->http_proxy_only);
                    assert(!uuu->net_info->http_version);
                    if (!len) {
                        args = 0;
                    } else if (!(temp = (char*) malloc(len))
                               ||  BUF_Peek(uuu->w_buf, temp, len) != len) {
                        if (temp)
                            free(temp);
                        status = eIO_Unknown;
                        free((void*) path);
                        break;
                    } else
                        args = temp;
                } else {
                    /* Proxied HTTP */
                    assert(uuu->net_info->scheme == eURL_Http);
                    if (uuu->flags & fHCC_UrlEncodeArgs) {
                        /* args added to path not valid(unencoded), remove */
                        if ((temp = (char*) strchr(path, '?')) != 0)
                            *temp = '\0';
                        args = ConnNetInfo_GetArgs(uuu->net_info);
                        if (args  &&  (!*args  ||  *args == '#'))
                            args = 0;
                    } else
                        args = 0;
                }
            } else {
                /* Direct HTTP[S] or tunneled HTTPS */
                if (uuu->net_info->http_push_auth
                    &&  (uuu->net_info->scheme == eURL_Https
                         ||  x_UnsafeRedirectOK(uuu))
                    &&  x_Authenticate(uuu, eRetry_Authenticate, 0) > 0) {
                    status = eIO_Unknown;
                    break;
                }
                host = uuu->net_info->host;
                port = uuu->net_info->port;
                args = ConnNetInfo_GetArgs(uuu->net_info);
                if (*args == '#'  ||  !(uuu->flags & fHCC_UrlEncodeArgs)) {
                    path = uuu->net_info->path;
                    args = 0;
                } else if (!(path = strndup(uuu->net_info->path, (size_t)
                                            (args - uuu->net_info->path) -
                                            !(args == uuu->net_info->path)
                                            /*'?'*/))) {
                    status = eIO_Unknown;
                    break;
                } else if (!*args)
                    args = 0;
            }

            /* encode args (obsolete feature) */
            if (req_method != eReqMethod_Connect  &&  args) {
                size_t args_len = strcspn(args, "#");
                size_t size = args_len * 3;
                size_t rd_len, wr_len;
                assert((uuu->flags & fHCC_UrlEncodeArgs)  &&  args_len > 0);
                if (!(temp = (char*) malloc(size + strlen(args+args_len) +1))){
                    int error = errno;
                    temp = ConnNetInfo_URL(uuu->net_info);
                    CORE_LOGF_ERRNO_X(20, eLOG_Error, error,
                                      ("[HTTP%s%s]  Out of memory encoding"
                                       " URL arguments (%lu bytes)",
                                       temp ? "; " : "",
                                       temp ? temp : "",
                                       (unsigned long)(size + strlen
                                                       (args + args_len) +1)));
                    if (path != uuu->net_info->path)
                        free((void*) path);
                    status = eIO_Unknown;
                    break;
                }
                URL_Encode(args, args_len, &rd_len, temp, size, &wr_len);
                assert(rd_len == args_len);
                assert(wr_len <= size);
                strcpy(temp + wr_len, args + args_len);
                args = temp;
            }

            /* NCBI request IDs */
            if (!(uuu->flags & fHTTP_NoAutomagicSID))
                x_SetRequestIDs(uuu->net_info);

            /* identify the connector in the User-Agent: header tag */
            http_user_header = (uuu->net_info->http_user_header
                                ? strdup(uuu->net_info->http_user_header)
                                : 0);
            if (!uuu->net_info->http_user_header == !http_user_header) {
                ConnNetInfo_ExtendUserHeader
                    (uuu->net_info, "User-Agent: NCBIHttpConnector"
#ifdef NCBI_CXX_TOOLKIT
                     " (CXX Toolkit)"
#else
                     " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
                     );
                if (!uuu->net_info->http_version
                    &&  req_method != eReqMethod_Connect) {
                    ConnNetInfo_ExtendUserHeader(uuu->net_info,
                                                 "Connection: keep-alive");
                }
                reset_user_header = 1;
            } else
                reset_user_header = 0;

            if (uuu->net_info->debug_printout) {
                ConnNetInfo_Log(uuu->net_info,
                                uuu->retry ? eLOG_Trace : eLOG_Note,
                                CORE_GetLOG());
            }
            uuu->retry = 0;

            /* connect & send HTTP header */
            if (uuu->net_info->scheme == eURL_Https)
                flags |= fSOCK_Secure;
            if (!(uuu->flags & fHTTP_NoUpread))
                flags |= fSOCK_ReadOnWrite;

            status = URL_ConnectEx(host, port, path, args,
                                   req_method | (uuu->net_info->http_version
                                                 ? eReqMethod_v1
                                                 : 0), len,
                                   uuu->o_timeout, timeout,
                                   uuu->net_info->http_user_header,
                                   uuu->net_info->credentials,
                                   flags, &sock);
            /* FIXME: remember if "sock" reused here; and not cause major fault
             * if failed later;  but simply re-try w/o calling adjust. */

            if (reset_user_header) {
                ConnNetInfo_SetUserHeader(uuu->net_info, 0);
                uuu->net_info->http_user_header = http_user_header;
            }

            if (path != uuu->net_info->path)
                free((void*) path);
            if (args)
                free((void*) args);

            if (sock) {
                assert(status == eIO_Success);
                uuu->w_len = req_method != eReqMethod_Connect
                    ? BUF_Size(uuu->w_buf) : 0;
                break;
            }
        } else
            assert(!sock);

        assert(status != eIO_Success);
        if (status == eIO_Closed)
            status  = eIO_Unknown;
        /* connection failed, try another server */
        if (s_Adjust(uuu, 0, extract) != eIO_Success)
            break;
    }

    if (status == eIO_Success) {
        uuu->conn_state = eCS_WriteRequest;
        uuu->sock = sock;
        assert(uuu->sock);
    } else {
        if (sock) {
            SOCK_Abort(sock);
            SOCK_Destroy(sock);
        }
        assert(!uuu->sock);
    }
    return status;
}


/* Unconditionally drop the connection w/o any wait */
static void s_DropConnection(SHttpConnector* uuu, enum EConnState state)
{
    assert(uuu->sock);
    if (!(uuu->conn_state & eCS_ReadBody)  ||  uuu->conn_state == eCS_Discard)
        SOCK_Abort(uuu->sock);
    else
        SOCK_SetTimeout(uuu->sock, eIO_Close, &kZeroTimeout);
    SOCK_Close(uuu->sock);
    uuu->sock = 0;
    uuu->retry = 0;
    uuu->conn_state = state;
}


typedef struct {
    SOCK       sock;
    EIO_Status status;
} XBUF_PeekCBCtx;


static size_t x_WriteBuf(void* data, const void* buf, size_t size)
{
    XBUF_PeekCBCtx* ctx = (XBUF_PeekCBCtx*) data;
    size_t written;

    assert(buf  &&  size);
    assert(ctx->status == eIO_Success);
    ctx->status = SOCK_Write(ctx->sock, buf, size,
                             &written, eIO_WritePlain);
    return written;
}


/* Connect to the server specified by uuu->net_info, then compose and form
 * relevant HTTP header, and flush the accumulated output data(uuu->w_buf)
 * after the HTTP header.
 * If connection/write unsuccessful, retry to reconnect and send the data
 * again until permitted by s_Adjust().
 */
static EIO_Status s_ConnectAndSend(SHttpConnector* uuu,
                                   const STimeout* timeout,
                                   EExtractMode    extract)
{
    EIO_Status status = eIO_Success;

    for (;;) {
        char   what[80];
        int    error;
        size_t off;
        char*  url;

        assert(status == eIO_Success);

        if ((uuu->conn_state == eCS_NotInitiated || uuu->conn_state == eCS_Eom)
            &&  (status = s_Connect(uuu, timeout, extract)) != eIO_Success) {
            break;
        }

        if (uuu->w_len) {
            XBUF_PeekCBCtx ctx;
            ctx.sock   = uuu->sock;
            ctx.status = eIO_Success/*NB:==status*/;
            off = BUF_Size(uuu->w_buf) - uuu->w_len;
            assert(uuu->conn_state < eCS_ReadHeader);
            SOCK_SetTimeout(ctx.sock, eIO_Write, uuu->w_timeout);
            uuu->w_len -= BUF_PeekAtCB(uuu->w_buf, off,
                                       x_WriteBuf, &ctx, uuu->w_len);
            status = ctx.status;
        } else
            off = 0;

        if (status == eIO_Success) {
            if (uuu->w_len)
                continue;
            if (uuu->conn_state  > eCS_FlushRequest)
                break;
            if (!uuu->chunked)
                uuu->conn_state  = eCS_FlushRequest;
            if (uuu->conn_state == eCS_FlushRequest) {
                /* "flush" */
                status = SOCK_Write(uuu->sock, 0, 0, 0, eIO_WritePlain);
                if (status == eIO_Success) {
                    uuu->conn_state = eCS_ReadHeader;
                    uuu->keepalive = uuu->net_info->http_version;
                    uuu->expected = (TNCBI_BigCount)(-1L);
                    uuu->received = 0;
                    uuu->chunked = 0;
                    BUF_Erase(uuu->http);
                    break;
                }
            } else
                break;
        }

        if (status == eIO_Timeout
            &&  (extract == eEM_Wait
                 ||  (timeout  &&  !(timeout->sec | timeout->usec)))) {
            break;
        }

        error = errno;
        if (uuu->w_len  &&  uuu->conn_state == eCS_WriteRequest) {
            if (!x_IsWriteThru(uuu)) {
                sprintf(what, "write request body at offset %lu",
                        (unsigned long) off);
            } else
                strcpy(what, "write request body");
        } else {
            if (uuu->w_len)
                strcpy(what, "finalize request body");
            else
                strcpy(what, "finalize request");
        }

        url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_ERRNO_X(6, uuu->reused ? eLOG_Trace : eLOG_Error, error,
                          ("[HTTP%s%s]  Cannot %s (%s)",
                           url ? "; " : "",
                           url ? url  : "", what, IO_StatusStr(status)));
        if (url)
            free(url);

        /* write failed; close and try to use another server, if possible */
        s_DropConnection(uuu, eCS_NotInitiated);
        assert(status != eIO_Success);
        if ((status = s_Adjust(uuu, 0, extract)) != eIO_Success)
            break;
    }

    return status;
}


static int/*bool*/ x_IsValidParam(const char* param, size_t paramlen)
{
    const char* e = (const char*) memchr(param, '=', paramlen);
    size_t len;
    if (!e  ||  e == param)
        return 0/*false*/;
    if ((len = (size_t)(++e - param)) >= paramlen)
        return 0/*false*/;
    assert(!isspace((unsigned char)(*param)));
    if (strcspn(param, " \t") < len)
        return 0/*false*/;
    if (*e == '\''  ||  *e == '"') {
        /* a quoted string */
        assert(len < paramlen);
        len = paramlen - len;
        if (!(e = (const char*) memchr(e + 1, *e, --len)))
            return 0/*false*/;
        e++/*skip the quote*/;
    } else
        e += strcspn(e, " \t");
    if (e != param + paramlen  &&  e + strspn(e, " \t") != param + paramlen)
        return 0/*false*/;
    return 1/*true*/;
}


static int/*bool*/ x_IsValidChallenge(const char* text, size_t len)
{
    /* Challenge must contain a scheme name token and a non-empty param
     * list (comma-separated pairs token={token|quoted_string}), with at
     * least one parameter being named "realm=".
     */
    size_t word = strcspn(text, " \t");
    int retval = 0/*false*/;
    if (word < len) {
        /* 1st word is always the scheme name */
        const char* param = text + word;
        for (param += strspn(param, " \t");  param < text + len;
             param += strspn(param, ", \t")) {
            size_t paramlen = (size_t)(text + len - param);
            const char* c = (const char*) memchr(param, ',', paramlen);
            if (c)
                paramlen = (size_t)(c - param);
            if (!x_IsValidParam(param, paramlen))
                return 0/*false*/;
            if (paramlen > 6  &&  strncasecmp(param, "realm=", 6) == 0)
                retval = 1/*true, but keep scanning*/;
            param += c ? ++paramlen : paramlen;
        }
    }
    return retval;
}


static size_t x_PushbackBuf(void* data, const void* buf, size_t size)
{
    XBUF_PeekCBCtx* ctx = (XBUF_PeekCBCtx*) data;

    assert(buf  &&  size);
    assert(ctx->status == eIO_Success);
    ctx->status = SOCK_Pushback(ctx->sock, buf, size);
    return ctx->status != eIO_Success ? 0 : size;
}


static int/*bool*/ x_Pushback(SOCK sock, BUF buf)
{
    size_t size = BUF_Size(buf);
    XBUF_PeekCBCtx ctx;
    ctx.sock   = sock;
    ctx.status = eIO_Success;
    return !(size ^ BUF_PeekAtCB(buf, 0, x_PushbackBuf, &ctx, size));
}


static EIO_Status x_ReadChunkHead(SHttpConnector* uuu, int/*bool*/ first)
{
    int            n;
    BUF            buf;
    char*          str;
    size_t         size;
    TNCBI_BigCount chunk;
    EIO_Status     status;

    buf = 0;
    str = 0;
    size = 0;
    for (;;) {
        size_t off;
        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &buf, &off);
        if (status != eIO_Success  ||  (size += off) != BUF_Size(buf))
            break;
        if (size > 2) {
            if (!(str = (char*) malloc(size + 1)))
                break;
            verify(BUF_Peek(buf, str, size) == size);
            if (!first  &&  (str[0] != '\r'  ||  str[1] != '\n')) {
                free(str);
                str = 0;
                break;
            }
            assert(str[size - 1] == '\n');
            str[size] = '\0';
            break;
        }
    }

    if (!str
        ||  sscanf(str, "%" NCBI_BIGCOUNT_FORMAT_SPEC_HEX "%n", &chunk, &n) < 1
        ||  (!isspace((unsigned char) str[n])  &&  str[n] != ';')) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(23, eLOG_Error,
                    ("[HTTP%s%s]  Cannot read chunk size%s%.*s",
                     url ? "; " : "",
                     url ? url  : "",
                     str ? ": " : "",
                     str ? (int)(size - (first ? 2 : 4)) : 0,
                     str ?       str  + (first ? 0 : 2)  : ""));
        if (url)
            free(url);
        if (str)
            free(str);
        if (status == eIO_Closed  ||  !x_Pushback(uuu->sock, buf))
            status  = eIO_Unknown;
        BUF_Destroy(buf);
        return status ? status : eIO_Unknown;
    }

    free(str);
    BUF_Destroy(buf);
    uuu->received = 0;
    uuu->expected = chunk;
    return eIO_Success;
}


static EIO_Status x_ReadChunkTail(SHttpConnector* uuu)
{
    EIO_Status status;
    BUF buf = 0;
    do {
        size_t n;
        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &buf, &n);
        if (n == 2) {
            /*last trailer*/
            BUF_Destroy(buf);
            uuu->conn_state = eCS_Discard;
            return eIO_Closed;
        }
    } while (status == eIO_Success);
    if (status == eIO_Closed) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(25, eLOG_Error,
                    ("[HTTP%s%s]  Cannot read chunk tail",
                     url ? "; " : "",
                     url ? url  : ""));
        if (url)
            free(url);
        status  = eIO_Unknown;
    } else if (!x_Pushback(uuu->sock, buf))
        status  = eIO_Unknown;
    BUF_Destroy(buf);
    return status;
}


/* If "size"==0, then "buf" is a pointer to a BUF to receive the current body.
 * Otherwise, "buf" is a non-NULL destination buffer.
 */
static EIO_Status s_ReadData(SHttpConnector* uuu,
                             void* buf, size_t size,
                             size_t* n_read, EIO_ReadMethod how)
{
    BUF*       xxx;
    EIO_Status status;

    assert(buf  &&  n_read  &&  !*n_read  &&  uuu->conn_state == eCS_ReadBody);

    if (!uuu->chunked  ||  uuu->expected > uuu->received) {
        if (!size) {
            size = uuu->expected != (TNCBI_BigCount)(-1L)
                ? uuu->expected - uuu->received
                : HTTP_SOAK_READ_SIZE;
            if (!size) {
                assert(!uuu->chunked);
                uuu->conn_state = eCS_DoneBody;
                return eIO_Closed;
            }
            if (size > HTTP_SOAK_READ_SIZE)
                size = HTTP_SOAK_READ_SIZE;
            xxx = (BUF*) buf;
            if (!(buf = (void*) malloc(size))) {
                int error = errno;
                char* url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_ERRNO_X(24, eLOG_Error, error,
                                  ("[HTTP%s%s]  Cannot allocate response chunk"
                                   " (%lu byte%s)",
                                   url ? "; " : "",
                                   url ? url  : "",
                                   (unsigned long) size, &"s"[size == 1]));
                if (url)
                    free(url);
                return eIO_Unknown;
            }
        } else if (uuu->chunked) {
            assert(uuu->expected != (TNCBI_BigCount)(-1L));
            if (size > uuu->expected - uuu->received)
                size = uuu->expected - uuu->received;
            xxx = 0;
        } else if (uuu->expected == uuu->received) {
            uuu->conn_state = eCS_DoneBody;
            return eIO_Closed;
        } else
            xxx = 0;
        status = SOCK_Read(uuu->sock, buf, size, n_read, how);
        if (xxx  &&  !BUF_AppendEx(xxx, buf, size, buf, *n_read)) {
            int error = errno;
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_ERRNO_X(25, eLOG_Error, error,
                              ("[HTTP%s%s]  Cannot collect response body",
                               url ? "; " : "",
                               url ? url  : ""));
            if (url)
                free(url);
            free(buf);
            status = eIO_Unknown;
        }
        if (status == eIO_Closed)
            uuu->conn_state = eCS_Eom;
        return status;
    }

    if ((status = x_ReadChunkHead(uuu, !uuu->expected)) != eIO_Success)
        return status;

    if (uuu->expected) {
        assert(uuu->expected != (TNCBI_BigCount)(-1L)  &&  !uuu->received);
        if (size > uuu->expected)
            size = uuu->expected;
        return s_ReadData(uuu, buf, size, n_read, how);
    }

    uuu->conn_state = eCS_DoneBody;
    return x_ReadChunkTail(uuu);
}


static int/*bool*/ x_ErrorHeaderOnly(SHttpConnector* uuu)
{
    if (uuu->error_header == eDefault) {
        char val[32];
        ConnNetInfo_GetValueInternal(0, "HTTP_ERROR_HEADER_ONLY",
                                     val, sizeof(val), 0);
        uuu->error_header = ConnNetInfo_Boolean(val) ? eOn : eOff;
    }
    return uuu->error_header == eOn ? 1/*true*/ : 0/*false*/;
}


/* Read and parse HTTP header */
static EIO_Status s_ReadHeader(SHttpConnector* uuu,
                               SRetry*         retry,
                               EExtractMode    extract)
{
    enum EHTTP_Tag {
        eHTTP_NcbiMsg          = 1 << 0,
        eHTTP_NcbiSid          = 1 << 1,
        eHTTP_Location         = 1 << 2,
        eHTTP_Connection       = 1 << 3,
        eHTTP_Authenticate     = 1 << 4,
        eHTTP_ContentLength    = 1 << 5,
        eHTTP_TransferEncoding = 1 << 6
    };
    typedef unsigned short THTTP_Tags;  /* Bitwise-OR of EHTTP_Tag */
    char*             url = 0, *hdr, *s;
    EHTTP_HeaderParse header_parse;
    int               http_code;
    size_t            size, n;
    EIO_Status        status;
    int               fatal;
    THTTP_Tags        tags;

    assert(uuu->sock  &&  uuu->conn_state == eCS_ReadHeader);
    memset(retry, 0, sizeof(*retry));
    /*retry->mode = eRetry_None;
      retry->data = 0;*/

    /* line by line HTTP header input */
    for (;;) {
        /* do we have full header yet? */
        if ((size = BUF_Size(uuu->http)) >= 4) {
            if (!(hdr = (char*) malloc(size + 1))) {
                int error = errno;
                assert(!url);
                url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_ERRNO_X(7, eLOG_Error, error,
                                  ("[HTTP%s%s]  Cannot allocate header"
                                   " (%lu bytes)",
                                   url ? "; " : "",
                                   url ? url  : "",
                                   (unsigned long) size));
                if (url)
                    free(url);
                uuu->reused = 0;
                return eIO_Unknown;
            }
            verify(BUF_Peek(uuu->http, hdr, size) == size);
            if (memcmp(&hdr[size - 4], "\r\n\r\n", 4) == 0) {
                /* full header captured */
                hdr[size] = '\0';
                break;
            }
            free(hdr);
        }

        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &uuu->http, &n);

        if (status != eIO_Success  ||  size + n != BUF_Size(uuu->http)) {
            ELOG_Level level;
            if (status == eIO_Timeout) {
                const STimeout* tmo = SOCK_GetTimeout(uuu->sock, eIO_Read);
                if (!tmo)
                    level = eLOG_Error;
                else if (extract == eEM_Wait)
                    return status;
                else if (tmo->sec | tmo->usec)
                    level = eLOG_Warning;
                else
                    level = eLOG_Trace;
            } else
                level = status  &&  uuu->reused ? eLOG_Trace : eLOG_Error;
            assert(!url);
            url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(8, level,
                        ("[HTTP%s%s]  Cannot %s header (%s)",
                         url ? "; " : "",
                         url ? url  : "",
                         status != eIO_Success ? "read" : "scan",
                         IO_StatusStr(status != eIO_Success
                                      ? status : eIO_Unknown)));
            if (url)
                free(url);
            if (status != eIO_Success)
                return status;
            uuu->reused = 0;
            return eIO_Unknown;
        }
    }
    /* the entire header has been read in */
    uuu->conn_state = eCS_ReadBody;
    BUF_Erase(uuu->http);
    uuu->reused = 0;
    assert(hdr);

    /* HTTP status must come on the first line of the response */
    fatal = 0/*false*/;
    if (sscanf(hdr, "HTTP/%*d.%*d %d ", &http_code) != 1  ||  !http_code)
        http_code = -1;
    uuu->http_code = (unsigned short) http_code;
    if (http_code < 200  ||  299 < http_code) {
        if      (http_code == 304)
            /*void*/;
        else if (http_code == 301  ||  http_code == 302  ||  http_code == 307)
            retry->mode = eRetry_Redirect;
        else if (http_code == 303)
            retry->mode = eRetry_Redirect303;
        else if (http_code == 401)
            retry->mode = eRetry_Authenticate;
        else if (http_code == 407)
            retry->mode = eRetry_ProxyAuthenticate;
        else if (http_code <=   0  ||
                 http_code == 400  ||  http_code == 403  ||
                 http_code == 404  ||  http_code == 405  ||
                 http_code == 406  ||  http_code == 410) {
            fatal = 1/*true*/;
        }
    } else
        http_code = 0/*no server error*/;

    if (uuu->net_info->debug_printout == eDebugPrintout_Some
        &&  (http_code  ||  !x_ErrorHeaderOnly(uuu))) {
        /* HTTP header gets printed as part of data logging when
           uuu->net_info->debug_printout == eDebugPrintout_Data. */
        const char* header_header;
        if (!http_code  ||  http_code == 304)
            header_header = "HTTP header";
        else if (fatal)
            header_header = "HTTP header (fatal)";
        else if (uuu->flags & fHTTP_KeepHeader)
            header_header = "HTTP header (error)";
        else if (retry->mode == eRetry_Redirect)
            header_header = "HTTP header (redirect)";
        else if (retry->mode == eRetry_Redirect303)
            header_header = "HTTP header (see other)";
        else if (retry->mode & eRetry_Authenticate)
            header_header = "HTTP header (authentication)";
        else if (retry->mode)
            header_header = 0, assert(0);
        else
            header_header = "HTTP header (retriable server error)";
        assert(!url);
        url = ConnNetInfo_URL(uuu->net_info);
        CORE_DATAF_X(9, fatal ? eLOG_Error : eLOG_Note, hdr, size,
                     ("[HTTP%s%s]  %s",
                      url ? "; " : "",
                      url ? url  : "", header_header));
    }

    if (!(uuu->flags & fHTTP_KeepHeader)) {
        if (fatal) {
            assert(http_code);
            if (!uuu->net_info->debug_printout  &&  !uuu->parse_header) {
                char text[40];
                if (!url)
                    url = ConnNetInfo_URL(uuu->net_info);
                if (http_code > 0)
                    sprintf(text, "%d", http_code);
                else
                    strcpy(text, "occurred");
                CORE_LOGF_X(22, eLOG_Error,
                            ("[HTTP%s%s]  Fatal error %s",
                             url ? "; " : "",
                             url ? url  : "", text));
            }
            if (!uuu->adjust)
                uuu->net_info->max_try = 0;
        }

        header_parse = uuu->parse_header
            ? uuu->parse_header(hdr, uuu->user_data, http_code)
            : eHTTP_HeaderSuccess;

        if (header_parse == eHTTP_HeaderError) {
            retry->mode = eRetry_None;
            if (uuu->net_info->debug_printout == eDebugPrintout_Some
                &&  !http_code/*i.e. was okay*/  &&  x_ErrorHeaderOnly(uuu)) {
                if (!url)
                    url = ConnNetInfo_URL(uuu->net_info);
                CORE_DATAF_X(9, eLOG_Note, hdr, size,
                             ("[HTTP%s%s]  HTTP header (parse error)",
                              url ? "; " : "",
                              url ? url  : ""));
            }
        } else if (header_parse == eHTTP_HeaderComplete) {
            /* i.e. stop processing */
            retry->mode = eRetry_None;
            http_code = 0/*fake success*/;
        }
    } else {
        header_parse = eHTTP_HeaderSuccess;
        retry->mode = eRetry_None;
    }

    tags = (THTTP_Tags)
        (eHTTP_NcbiMsg | eHTTP_Connection
         | (retry->mode & eRetry_Redirect     ? eHTTP_Location     :
            retry->mode & eRetry_Authenticate ? eHTTP_Authenticate : 0)
         | (uuu->flags & fHTTP_NoAutomagicSID ? 0 : eHTTP_NcbiSid));
    if (uuu->http_code / 100 != 1
        &&  uuu->http_code != 204  &&  uuu->http_code != 304) {
        tags |= eHTTP_ContentLength | eHTTP_TransferEncoding;
    }

    /* NB: the loop may clobber "hdr" unless fHTTP_KeepHeader is set */
    for (s = strchr(hdr, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
        if (tags & eHTTP_NcbiMsg) {
            /* parse NCBI message */
            static const char kNcbiMsgTag[] = "\n" HTTP_NCBI_MESSAGE;
            if (strncasecmp(s, kNcbiMsgTag, sizeof(kNcbiMsgTag) - 1) == 0) {
                char* msg = s + sizeof(kNcbiMsgTag) - 1, *e;
                while (*msg  &&  isspace((unsigned char)(*msg)))
                    ++msg;
                if (!(e = strchr(msg, '\r'))  &&  !(e = strchr(msg, '\n')))
                    break;
                if (e > msg) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > msg);
                n = (size_t)(e - msg);
                if (n) {
                    char c = msg[n];
                    msg[n] = '\0';
                    if (s_MessageHook) {
                        if (s_MessageIssued <= 0) {
                            s_MessageIssued  = 1;
                            s_MessageHook(msg);
                        }
                    } else {
                        s_MessageIssued = -1;
                        CORE_LOGF_X(10, eLOG_Critical,
                                    ("[NCBI-MESSAGE]  %s", msg));
                    }
                    msg[n] = c;
                }
                tags &= (THTTP_Tags)(~eHTTP_NcbiMsg);
                continue;
            }
        }
        if (tags & eHTTP_NcbiSid) {
            /* parse NCBI SID */
            static const char kNcbiSidTag[] = "\n" HTTP_NCBI_SID;
            if (strncasecmp(s, kNcbiSidTag, sizeof(kNcbiSidTag) - 1) == 0) {
                char* sid = s + sizeof(kNcbiSidTag) - 1, *e;
                while (*sid  &&  isspace((unsigned char)(*sid)))
                    ++sid;
                if (!(e = strchr(sid, '\r'))  &&  !(e = strchr(sid, '\n')))
                    break;
                if (e > sid) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > sid);
                n = (size_t)(e - sid);
                if (n) {
                    char c = sid[n];
                    sid[n] = '\0';
                    ConnNetInfo_OverrideUserHeader(uuu->net_info, s + 1);
                    sid[n] = c;
                }
                tags &= (THTTP_Tags)(~eHTTP_NcbiSid);
                continue;
            }
        }
        if (tags & eHTTP_Location) {
            /* parse "Location" pointer */
            static const char kLocationTag[] = "\nLocation:";
            assert(http_code);
            if (strncasecmp(s, kLocationTag, sizeof(kLocationTag) - 1) == 0) {
                char* loc = s + sizeof(kLocationTag) - 1, *e;
                while (*loc  &&  isspace((unsigned char)(*loc)))
                    ++loc;
                if (!(e = strchr(loc, '\r'))  &&  !(e = strchr(loc, '\n')))
                    break;
                if (e > loc) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > loc);
                n = (size_t)(e - loc);
                if (n) {
                    memmove(hdr, loc, n);
                    hdr[n] = '\0';
                    retry->data = hdr;
                }
                tags &= (THTTP_Tags)(~eHTTP_Location);
                continue;
            }
        }
        if (tags & eHTTP_Connection) {
            /* parse "Connection" tag */
            static const char kConnectionTag[] = "\nConnection:";
            if (strncasecmp(s, kConnectionTag, sizeof(kConnectionTag)-1) == 0){
                char* con = s + sizeof(kConnectionTag) - 1, *e;
                while (*con  &&  isspace((unsigned char)(*con)))
                    ++con;
                if (!(e = strchr(con, '\r'))  &&  !(e = strchr(con, '\n')))
                    break;
                if (e > con) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > con);
                while ((n = (size_t)(e - con)) > 0) {
                    const char* c = (const char*) memchr(con, ',', n);
                    size_t m;
                    if (c > con) {
                        do {
                            if (!isspace((unsigned char) c[-1]))
                                break;
                        } while (--c > con);
                        m = (size_t)(c - con);
                    } else
                        m = n;
                    if (m == 5  &&  strncasecmp(con, "close", 5) == 0) {
                        uuu->keepalive = 0;
                        break;
                    }
                    if (m == 10  &&  strncasecmp(con, "keep-alive", 10) == 0) {
                        uuu->keepalive = 1;
                        break;
                    }
                    if (m == n)
                        break;
                    con += m + 1;
                    while (con < e  &&  isspace((unsigned char)(*con)))
                        ++con;
                }
                tags &= (THTTP_Tags)(~eHTTP_Connection);
                continue;
            }
        }
        if (tags & eHTTP_Authenticate) {
            /* parse "Authenticate"/"Proxy-Authenticate" tags */
            static const char kAuthenticateTag[] = "-Authenticate:";
            n = retry->mode == eRetry_Authenticate ? 4 : 6;
            assert(http_code);
            if (((retry->mode == eRetry_Authenticate
                  &&  strncasecmp(s, "\nWWW",   n) == 0)  ||
                 (retry->mode == eRetry_ProxyAuthenticate
                  &&  strncasecmp(s, "\nProxy", n) == 0))  &&
                strncasecmp(s + n,
                            kAuthenticateTag, sizeof(kAuthenticateTag)-1)==0) {
                char* txt = s + n + sizeof(kAuthenticateTag) - 1, *e;
                while (*txt  &&  isspace((unsigned char)(*txt)))
                    ++txt;
                if (!(e = strchr(txt, '\r'))  &&  !(e = strchr(txt, '\n')))
                    break;
                if (e > txt) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > txt);
                n = (size_t)(e - txt);
                if (n  &&  x_IsValidChallenge(txt, n)) {
                    memmove(hdr, txt, n);
                    hdr[n] = '\0';
                    retry->data = hdr;
                }
                tags &= (THTTP_Tags)(~eHTTP_Authenticate);
                continue;
            }
        }
        if (tags & eHTTP_ContentLength) {
            /* parse "Content-Length" for non-HEAD/CONNECT */
            static const char kContentLenTag[] = "\nContent-Length:";
            if (strncasecmp(s, kContentLenTag, sizeof(kContentLenTag)-1) == 0){
                if (!uuu->chunked
                    &&  uuu->net_info->req_method != eReqMethod_Head
                    &&  uuu->net_info->req_method != eReqMethod_Connect) {
                    const char* len = s + sizeof(kContentLenTag) - 1, *e;
                    int tmp;
                    while (*len  &&  isspace((unsigned char)(*len)))
                        ++len;
                    if (!(e = strchr(len, '\r'))  &&  !(e = strchr(len, '\n')))
                        break;
                    if (e > len) do {
                        if (!isspace((unsigned char) e[-1]))
                            break;
                    } while (--e > len);
                    if (e == len
                        ||  sscanf(len, "%" NCBI_BIGCOUNT_FORMAT_SPEC "%n",
                                   &uuu->expected, &tmp) < 1
                        ||  len + tmp != e) {
                        uuu->expected = (TNCBI_BigCount)(-1L)/*no checks*/;
                    }
                }
                tags &= (THTTP_Tags)(~eHTTP_ContentLength);
                continue;
            }
        }
        if (tags & eHTTP_TransferEncoding) {
            static const char kTransferEncodingTag[] = "\nTransfer-Encoding:";
            if (strncasecmp(s, kTransferEncodingTag,
                            sizeof(kTransferEncodingTag)-1) == 0) {
                const char* te = s + sizeof(kTransferEncodingTag) - 1, *e;
                while (*te  &&  isspace((unsigned char)(*te)))
                    ++te;
                if (!(e = strchr(te, '\r'))  &&  !(e = strchr(te, '\n')))
                    break;
                if (e > te) do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while(--e > te);
                n = (size_t)(e - te);
                if (n == 7  &&  strncasecmp(&e[-7], "chunked", 7) == 0
                    &&  (isspace((unsigned char) e[-8])
                         ||  e[-8] == ':'  ||  e[-8] == ',')) {
                    uuu->chunked = 1;
                }
                uuu->expected = 0;
                tags &= (THTTP_Tags)
                    (~(eHTTP_ContentLength | eHTTP_TransferEncoding));
                if (!uuu->net_info->http_version) {
                    if (!url)
                        url = ConnNetInfo_URL(uuu->net_info);
                    CORE_LOGF_X(26, eLOG_Warning,
                                ("[HTTP%s%s]  Chunked transfer encoding"
                                 " with HTTP/1.0",
                                 url ? "; " : "",
                                 url ? url  : ""));
                }
                continue;
            }
        }
        if (!tags)
            break;
    }
    if (uuu->keepalive  &&  uuu->expected == (TNCBI_BigCount)(-1L)) {
        if (uuu->net_info->http_version
            ||  uuu->net_info->req_method != eReqMethod_Head) {
            uuu->keepalive = 0;
        } else
            uuu->expected = 0;
    }

    if (uuu->flags & fHTTP_KeepHeader) {
        assert(retry->mode == eRetry_None);
        if (!BUF_AppendEx(&uuu->r_buf, hdr, 0, hdr, size)) {
            int error = errno;
            if (!url)
                url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_ERRNO_X(11, eLOG_Error, error,
                              ("[HTTP%s%s]  Cannot save HTTP header",
                               url ? "; " : "",
                               url ? url  : ""));
            free(hdr);
        }
        if (url)
            free(url);
        return eIO_Success;
    }

    if (!retry->data)
        free(hdr);

    if (!http_code  ||  http_code == 304) {
        if (url)
            free(url);
        return header_parse == eHTTP_HeaderError ? eIO_Unknown : eIO_Success;
    }
    assert(header_parse != eHTTP_HeaderComplete);

    if (uuu->net_info->debug_printout
        ||  header_parse == eHTTP_HeaderContinue) {
        if (http_code > 0/*real error, w/only a very short body expected*/)
            SOCK_SetTimeout(uuu->sock, eIO_Read, &g_NcbiDefConnTimeout);
        do {
            n = 0;
            status = s_ReadData(uuu, &uuu->http, 0, &n, eIO_ReadPlain);
            uuu->received += n;
        } while (status == eIO_Success);
        if (header_parse == eHTTP_HeaderContinue  &&  status == eIO_Closed) {
            if (url)
                free(url);
            return retry->mode ? status/*eIO_Closed*/ : eIO_Success;
        }
    } else
        status = eIO_Success/*NB: irrelevant*/;

    if (uuu->net_info->debug_printout == eDebugPrintout_Some
        ||  header_parse == eHTTP_HeaderContinue) {
        const char* err = status != eIO_Closed ? IO_StatusStr(status) : 0;
        assert(status != eIO_Success);
        assert(!err  ||  *err);
        if (!url)
            url = ConnNetInfo_URL(uuu->net_info);
        if (header_parse == eHTTP_HeaderContinue) {
            assert(err/*status != eIO_Closed*/);
            CORE_LOGF_X(19, eLOG_Warning,
                        ("[HTTP%s%s]  Server error message incomplete (%s)",
                         url ? "; " : "",
                         url ? url  : "", err));
        } else if (!(size = BUF_Size(uuu->http))) {
            CORE_LOGF_X(12, err  &&  !(uuu->flags & fHTTP_SuppressMessages)
                        ? eLOG_Warning : eLOG_Trace,
                        ("[HTTP%s%s]  No error message received from server"
                         "%s%s%s",
                         url ? "; " : "",
                         url ? url  : "",
                         err ? " (" : "",
                         err ? err  : "", &")"[!err]));
        } else if ((s = (char*) malloc(size)) != 0) {
            n = BUF_Read(uuu->http, s, size);
            if (n != size) {
                CORE_LOGF_X(13, eLOG_Error,
                            ("[HTTP%s%s]  Cannot extract server error message"
                             " from buffer entirely (%lu/%lu)",
                             url ? "; " : "",
                             url ? url  : "",
                             (unsigned long) n, (unsigned long) size));
            }
            if (n) {
                CORE_DATAF_X(14, eLOG_Note, s, n,
                             ("[HTTP%s%s]  Server error message%s%s%s",
                              url ? "; " : "",
                              url ? url  : "",
                              err ? " (" : "",
                              err ? err  : "", &")"[!err]));
            }
            free(s);
        } else {
            CORE_LOGF_X(15, eLOG_Error,
                        ("[HTTP%s%s]  Cannot allocate server error message"
                         " (%lu byte%s)",
                         url ? "; " : "",
                         url ? url  : "",
                         (unsigned long) size, &"s"[size == 1]));
        }
    }

    if (url)
        free(url);
    if (header_parse != eHTTP_HeaderContinue)
        BUF_Erase(uuu->http);
    return eIO_Unknown;
}


/* Prepare connector for reading.  Open socket if necessary and make an initial
 * connect and send, re-trying if possible until success.
 * Return codes:
 *   eIO_Success = success, connector is ready for reading (uuu->sock != NULL);
 *   eIO_Timeout = maybe (check uuu->sock) connected and no data available yet;
 *   other code  = error, not connected (uuu->sock == NULL).
 * NB: On error, uuu->r_buf may be updated to contain new pending data!
 */
static EIO_Status s_PreRead(SHttpConnector* uuu,
                            const STimeout* timeout,
                            EExtractMode    extract)
{
    EIO_Status status;

    for (;;) {
        EIO_Status adjust;
        SRetry     retry;

        if ((status = s_ConnectAndSend(uuu, timeout, extract)) != eIO_Success)
            break;
        assert(!uuu->w_len);

        if (uuu->conn_state == eCS_WriteRequest) {
            assert(x_IsWriteThru(uuu)  &&  uuu->chunked);
            BUF_Erase(uuu->w_buf);
            if (!BUF_Write(&uuu->w_buf, "0\r\n\r\n", 5)) {
                status = eIO_Unknown;
                break;
            }
            uuu->w_len = 5;
            uuu->conn_state  = eCS_FlushRequest;

            status = s_ConnectAndSend(uuu, timeout, extract);
            if (status != eIO_Success)
                break;
            assert(!uuu->w_len);
        }
        assert(uuu->conn_state > eCS_FlushRequest);

        if (extract == eEM_Flush)
            return eIO_Success;

        if (extract == eEM_Wait
            &&  (uuu->conn_state & eCS_DoneBody) == eCS_DoneBody) {
            return eIO_Closed;
        }

        /* set read timeout before leaving (s_Read() expects it set) */
        SOCK_SetTimeout(uuu->sock, eIO_Read, timeout);

        if (uuu->conn_state & eCS_ReadBody)
            return eIO_Success;

        assert(uuu->sock  &&  uuu->conn_state == eCS_ReadHeader);
        if ((status = s_ReadHeader(uuu, &retry, extract)) == eIO_Success) {
            assert((uuu->conn_state & eCS_ReadBody)  &&  !retry.mode);
            /* pending output data no longer needed */
            BUF_Erase(uuu->w_buf);
            break;
        }

        assert(status != eIO_Timeout  ||  !retry.mode);
        /* if polling then bail out with eIO_Timeout */
        if (status == eIO_Timeout
            &&  (extract == eEM_Wait
                 ||  (timeout  &&  !(timeout->sec | timeout->usec)))) {
            assert(!retry.data);
            return eIO_Timeout;
        }

        /* HTTP header read error; disconnect and retry */
        assert(status != eIO_Success);
        s_DropConnection(uuu, eCS_NotInitiated);
        adjust = s_Adjust(uuu, &retry, extract);
        if (retry.data)
            free((void*) retry.data);
        if (adjust != eIO_Success) {
            if (adjust != eIO_Closed)
                status  = adjust;
            break;
        }
    }

    if (BUF_Size(uuu->http)  &&  !BUF_Splice(&uuu->r_buf, uuu->http))
        BUF_Erase(uuu->http);
    assert(!BUF_Size(uuu->http));
    return status;
}


/* NB: Sets the EOM read state */
static void x_Close(SHttpConnector* uuu)
{
    /* since this is merely an acknowledgement, it will be "instant" */
    SOCK_SetTimeout(uuu->sock, eIO_Close, &kZeroTimeout);
    SOCK_Destroy(uuu->sock);
    uuu->sock = 0;
    uuu->conn_state = eCS_Eom;
}


/* Read non-header data from connection */
static EIO_Status s_Read(SHttpConnector* uuu, void* buf,
                         size_t size, size_t* n_read)
{
    EIO_Status status = eIO_Success;

    assert(uuu->conn_state & eCS_ReadBody);
    assert(uuu->net_info->req_method != eReqMethod_Connect);

    if ((uuu->conn_state & eCS_DoneBody) == eCS_DoneBody) {
        if (uuu->conn_state == eCS_Eom)
            return eIO_Closed;
        if (uuu->chunked) {
            if (uuu->conn_state != eCS_Discard)
                status = x_ReadChunkTail(uuu);
            if (uuu->conn_state == eCS_Discard) {
                if (uuu->keepalive)
                    uuu->conn_state = eCS_Eom;
                else
                    x_Close(uuu);
            } else if (status != eIO_Closed)
                x_Close(uuu);
        } else
            uuu->conn_state = eCS_Eom;
        return status ? status : eIO_Closed;
    }
    assert(uuu->sock  &&  size  &&  n_read  &&  !*n_read);
    assert(uuu->received <= uuu->expected);

    if (uuu->net_info->req_method == eReqMethod_Head
        ||  uuu->http_code / 100 == 1
        ||  uuu->http_code == 204
        ||  uuu->http_code == 304) {
        uuu->conn_state = eCS_Discard;
        status = eIO_Closed;
    } else if (!uuu->net_info->http_version
               &&  (uuu->flags & fHCC_UrlDecodeInput)) {
        /* read and URL-decode */
        size_t         n_peeked, n_decoded;
        TNCBI_BigCount remain    = uuu->expected - uuu->received;
        size_t         peek_size = size > remain ? (size_t)(remain + 1) : size;
        void*          peek_buf  = malloc(peek_size *= 3);

        /* peek the data */
        status= SOCK_Read(uuu->sock,peek_buf,peek_size,&n_peeked,eIO_ReadPeek);
        if (status == eIO_Success) {
            if (URL_DecodeEx(peek_buf,n_peeked,&n_decoded,buf,size,n_read,"")){
                /* decode, then discard successfully decoded data from input */
                if (n_decoded) {
                    SOCK_Read(uuu->sock,0,n_decoded,&n_peeked,eIO_ReadPersist);
                    assert(n_peeked == n_decoded);
                    uuu->received += n_decoded;
                } else {
                    assert(!*n_read);
                    if (size) {
                        /* if at EOF and remaining data cannot be decoded */
                        status = SOCK_Status(uuu->sock, eIO_Read);
                        if (status == eIO_Closed)
                            status  = eIO_Unknown;
                    }
                }
            } else
                status  = eIO_Unknown;
            if (status != eIO_Success) {
                char* url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_X(16, eLOG_Error,
                            ("[HTTP%s%s]  Cannot URL-decode data: %s",
                             url ? "; " : "",
                             url ? url  : "", IO_StatusStr(status)));
                if (url)
                    free(url);
            }
        } else
            assert(!n_peeked);
        if (peek_buf)
            free(peek_buf);
    } else {
        /* just read, with no URL-decoding */
        status = s_ReadData(uuu, buf, size, n_read, eIO_ReadPlain);
        uuu->received += *n_read;
    }

    if (status == eIO_Closed) {
        if (!uuu->keepalive)
            x_Close(uuu);
        else if (uuu->conn_state == eCS_Discard)
            uuu->conn_state = eCS_Eom;
    }

    if (uuu->expected != (TNCBI_BigCount)(-1L)) {
        const char* how = 0;
        if (uuu->received < uuu->expected) {
            if (status == eIO_Closed) {
                status  = eIO_Unknown;
                how = "premature EOM in";
            }
        } else if (uuu->expected < uuu->received) {
            if (!uuu->net_info->http_version
                &&  (uuu->flags & fHCC_UrlDecodeInput)) {
                assert(*n_read);
                --(*n_read);
            } else {
                TNCBI_BigCount excess = uuu->received - uuu->expected;
                assert(*n_read >= excess);
                *n_read -= (size_t) excess;
            }
            uuu->conn_state = eCS_Discard;
            status  = eIO_Unknown;
            how = "too much";
        } else if (!uuu->chunked  &&  uuu->keepalive)
            uuu->conn_state = eCS_DoneBody;
        if (how) {
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(21, eLOG_Warning,
                        ("[HTTP%s%s]  Got %s data (received "
                         "%" NCBI_BIGCOUNT_FORMAT_SPEC " vs. "
                         "%" NCBI_BIGCOUNT_FORMAT_SPEC " expected)",
                         url ? "; " : "",
                         url ? url  : "", how,
                         uuu->received,
                         uuu->expected != (TNCBI_BigCount)(-1L) ?
                         uuu->expected : 0));
            if (url)
                free(url);
        }
    }
    return status;
}


/* Reset/readout input data and close socket */
static EIO_Status s_Disconnect(SHttpConnector* uuu,
                               const STimeout* timeout,
                               EExtractMode    extract)
{
    EIO_Status status = eIO_Success;

    assert(!(extract & eEM_Wait));  /* here it's only either drop or read */

    BUF_Erase(uuu->http);
    if (extract == eEM_Drop)
        BUF_Erase(uuu->r_buf);
    else if (uuu->conn_state != eCS_Eom
             &&  (status = s_PreRead(uuu, timeout, extract)) == eIO_Success) {
        char* x_buf = 0;
        do {
            if (x_buf  ||  (x_buf = (char*) malloc(HTTP_SOAK_READ_SIZE)) != 0){
                size_t x_read = 0;
                status = s_Read(uuu, x_buf, HTTP_SOAK_READ_SIZE, &x_read);
                if (x_read < HTTP_SOAK_READ_SIZE / 2) {
                    if (!BUF_Write(&uuu->r_buf, x_buf, x_read))
                        status = eIO_Unknown;
                } else {
                    if (!BUF_AppendEx(&uuu->r_buf,
                                      x_buf, HTTP_SOAK_READ_SIZE,
                                      x_buf, x_read)) {
                        status = eIO_Unknown;
                    } else
                        x_buf = 0;
                }
            } else
                status = eIO_Unknown;
        } while (status == eIO_Success);
        if (x_buf)
            free(x_buf);
        if (status == eIO_Closed)
            status  = eIO_Success;
    }

    /* s_PreRead() might have dropped the connection already */
    if (uuu->sock  &&  (extract == eEM_Drop  ||  !uuu->keepalive))
        s_DropConnection(uuu, eCS_Eom);
    uuu->can_connect &= ~fCC_Once;
    return status;
}


static void s_OpenHttpConnector(SHttpConnector* uuu,
                                const STimeout* timeout)
{
    /* NOTE: the real connect will be performed on the first "READ", or
     * "FLUSH/CLOSE", or on "WAIT" on read -- see in "s_ConnectAndSend" */
    assert(!uuu->sock);

    /* store timeouts for later use */
    if (timeout) {
        uuu->oo_timeout = *timeout;
        uuu->o_timeout  = &uuu->oo_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->o_timeout  = kInfiniteTimeout;
        uuu->w_timeout  = kInfiniteTimeout;
    }

    /* reset the auto-reconnect/state/re-try/auth features */
    uuu->can_connect     = (uuu->flags & fHTTP_AutoReconnect
                            ? fCC_Unlimited | fCC_Once : fCC_Once);
    uuu->conn_state      = eCS_NotInitiated;
    uuu->major_fault     = 0;
    uuu->minor_fault     = 0;
    uuu->auth_done       = 0;
    uuu->proxy_auth_done = 0;
    uuu->retry           = 0;
}


static void s_DestroyHttpConnector(SHttpConnector* uuu)
{
    assert(!uuu->sock);
    if (uuu->cleanup)
        uuu->cleanup(uuu->user_data);
    ConnNetInfo_Destroy(uuu->net_info);
    BUF_Destroy(uuu->r_buf);
    BUF_Destroy(uuu->w_buf);
    BUF_Destroy(uuu->http);
    free(uuu);
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static char*       s_VT_Descr   (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Wait    (CONNECTOR       connector,
                                     EIO_Event       event,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Write   (CONNECTOR       connector,
                                     const void*     buf,
                                     size_t          size,
                                     size_t*         n_written,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Flush   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "HTTP";
}


static char* s_VT_Descr
(CONNECTOR connector)
{
    return ConnNetInfo_URL(((SHttpConnector*) connector->handle)->net_info);
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    s_OpenHttpConnector((SHttpConnector*) connector->handle, timeout);
    return eIO_Success;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;

    assert(event == eIO_Read  ||  event == eIO_Write);
    switch (event) {
    case eIO_Read:
        if (BUF_Size(uuu->r_buf))
            return eIO_Success;
        if (uuu->can_connect == fCC_None)
            return eIO_Closed;
        status = s_PreRead(uuu, timeout, eEM_Wait);
        if (BUF_Size(uuu->r_buf))
            return eIO_Success;
        if (status != eIO_Success)
            return status;
        assert(uuu->sock);
        status = SOCK_Status(uuu->sock, eIO_Read);
        if (status != eIO_Success)
            return status;
        return SOCK_Wait(uuu->sock, eIO_Read, timeout);
    case eIO_Write:
        if (uuu->can_connect == fCC_None)
            return eIO_Closed;
        if (!x_IsWriteThru(uuu)) {
            return uuu->sock  &&  uuu->can_connect == fCC_Once
                ? eIO_Closed : eIO_Success;
        }
        if (!uuu->sock  &&  !BUF_Size(uuu->w_buf))
            return eIO_Success;
        if (!uuu->sock  ||  uuu->conn_state < eCS_ReadHeader) {
            status = s_ConnectAndSend(uuu, timeout, eEM_Flush);
            if (status != eIO_Success)
                return status;
        } else
            return uuu->can_connect == fCC_Once ? eIO_Closed : eIO_Success;
        assert(uuu->sock);
        assert(uuu->conn_state < eCS_ReadHeader  &&  !uuu->w_len);
        return uuu->conn_state < eCS_FlushRequest
            ? SOCK_Wait(uuu->sock, eIO_Write, timeout)
            : eIO_Success;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;

    assert(!*n_written);

    /* store the write timeout */
    if (timeout) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else
        uuu->w_timeout  = kInfiniteTimeout;

    /* if trying to write after a request then close the socket first */
    if (uuu->conn_state > eCS_WriteRequest) {
        status = s_Disconnect(uuu, timeout,
                              uuu->flags & fHTTP_DropUnread
                              ? eEM_Drop : eEM_Read);
        if (status != eIO_Success)
            return status;
        uuu->conn_state = eCS_NotInitiated;
    }
    if (uuu->can_connect == fCC_None)
        return eIO_Closed; /* no more connects permitted */
    uuu->can_connect |= fCC_Once;

    /* check if writing is at all legitimate */
    if (size  &&  (uuu->net_info->req_method == eReqMethod_Head  ||
                   uuu->net_info->req_method == eReqMethod_Get)) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(24, eLOG_Error,
                    ("[HTTP%s%s]  Illegal write (%lu byte%s) with %s",
                     url ? "; " : "",
                     url ? url  : "",
                     (unsigned long) size, &"s"[size == 1],
                     uuu->net_info->req_method == eReqMethod_Get
                     ? "GET" : "HEAD"));
        if (url)
            free(url);
        return eIO_Closed;
    }

    /* write-through with HTTP/1.1 */
    if (x_IsWriteThru(uuu)) {
        if (BUF_Size(uuu->w_buf)) {
            status = s_ConnectAndSend(uuu, timeout, eEM_Flush);
            if (status != eIO_Success)
                return status;
        }
        assert(!uuu->sock  ||  (uuu->conn_state == eCS_WriteRequest
                                &&  !uuu->w_len));
        if (size) {
            char prefix[80];
            int  n = sprintf(prefix, "%" NCBI_BIGCOUNT_FORMAT_SPEC_HEX "\r\n",
                             (TNCBI_BigCount) size);
            BUF_Erase(uuu->w_buf);
            if (!BUF_Write(&uuu->w_buf, prefix, (size_t) n)  ||
                !BUF_Write(&uuu->w_buf, buf,    size)        ||
                !BUF_Write(&uuu->w_buf, "\r\n", 2)) {
                BUF_Erase(uuu->w_buf);
                return eIO_Unknown;
            }
            *n_written = size;
            size += (size_t) n + 2;
            uuu->w_len = size;
            uuu->entity = 1;
        }
        return eIO_Success;
    }

    /* accumulate all output in a memory buffer */
    if (size  &&  !uuu->net_info->http_version
        &&  (uuu->flags & fHCC_UrlEncodeOutput)) {
        /* with URL-encoding */
        size_t dst_size = 3 * size;
        void*  dst = malloc(dst_size);
        URL_Encode(buf, size, n_written, dst, dst_size, &dst_size);
        if (!*n_written
            ||  !BUF_AppendEx(&uuu->w_buf, dst, 0, dst, dst_size)) {
            if (dst)
                free(dst);
            return eIO_Unknown;
        }
    } else {
        /* "as is" (without URL-encoding) */
        if (!BUF_Write(&uuu->w_buf, buf, size))
            return eIO_Unknown;
        *n_written = size;
    }
    return eIO_Success;
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;

    if (!uuu->sock  &&  uuu->can_connect == fCC_None)
        return eIO_Closed;

    if (timeout) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else
        uuu->w_timeout  = timeout;

    if (!(uuu->flags & fHTTP_Flushable))
        return eIO_Success;

    if (uuu->conn_state & eCS_ReadBody)
        return eIO_Success;
    if (uuu->sock
        &&  !(x_IsWriteThru(uuu)  &&  uuu->conn_state < eCS_ReadHeader)) {
        return eIO_Success;
    }
    status = x_IsWriteThru(uuu)
        ? s_ConnectAndSend(uuu, timeout, eEM_Flush)
        : s_PreRead       (uuu, timeout, eEM_Flush);
    return BUF_Size(uuu->r_buf) ? eIO_Success : status;
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SHttpConnector*  uuu = (SHttpConnector*) connector->handle;
    EExtractMode extract = BUF_Size(uuu->r_buf) ? eEM_Flush : eEM_Read;
    EIO_Status    status = uuu->can_connect & fCC_Once
        ? s_PreRead(uuu, timeout, extract) : eIO_Unknown;
    size_t        x_read = BUF_Read(uuu->r_buf, buf, size);

    assert(n_read  &&  !*n_read);
    if (x_read < size  &&  extract == eEM_Read  &&  status == eIO_Success) {
        status   = s_Read(uuu, (char*) buf + x_read, size - x_read, n_read);
        *n_read += x_read;
    } else
        *n_read  = x_read;
    return extract == eEM_Read ? status : eIO_Success;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    return uuu->sock ? SOCK_Status(uuu->sock, dir) :
        (uuu->can_connect == fCC_None ? eIO_Closed : eIO_Success);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* Send the accumulated output data(if any) to server, then close the
     * socket.  Regardless of the flush, clear both input and output buffers.
     */
    if ((uuu->can_connect & fCC_Once)
        &&  ((!uuu->sock  &&  BUF_Size(uuu->w_buf))
             ||  (uuu->flags & fHTTP_Flushable))) {
        /* "WRITE" mode and data (or just flag) is still pending */
        s_PreRead(uuu, timeout, eEM_Drop);
    }
    s_Disconnect(uuu, timeout, eEM_Drop);
    assert(!uuu->sock);

    /* clear pending output data, if any */
    BUF_Erase(uuu->w_buf);
    return eIO_Success;
}


static void s_Setup
(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;

    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, descr,    s_VT_Descr,   connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    s_VT_Flush,   connector);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    CONN_SET_DEFAULT_TIMEOUT(meta, uuu->net_info->timeout);
}


static void s_Destroy
(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    connector->handle = 0;

    s_DestroyHttpConnector(uuu);
    free(connector);
}


/* NB: per the standard, the HTTP tag name is mis-spelled as "Referer" */
static int/*bool*/ x_FixupUserHeader(SConnNetInfo* net_info,
                                     int  /*bool*/ has_ref,
                                     int* /*bool*/ has_sid)
{
    int/*bool*/ has_host = 0/*false*/;
    const char* s;

    if ((s = net_info->http_user_header) != 0) {
        int/*bool*/ first = 1/*true*/;
        while (*s) {
            if (has_ref < 0/*unset*/
                &&  strncasecmp(s, &"\nReferer:"[first], 9 - !!first) == 0) {
                has_ref = 1/*true*/;
            } else if (strncasecmp(s, &"\nHost:"[first], 6 - !!first) == 0) {
                has_host = 1/*true*/;
#ifdef HAVE_LIBCONNEXT
            } else if (strncasecmp(s, &"\nCAF"[first], 4 - !!first) == 0
                       &&  (s[4 - first] == '-'  ||  s[4 - !!first] == ':')) {
                char* caftag = strndup(s + !first, strcspn(s + !first, " \t"));
                if (caftag) {
                    size_t cafoff = (size_t)(s - net_info->http_user_header);
                    ConnNetInfo_DeleteUserHeader(net_info, caftag);
                    free(caftag);
                    if (!(s = net_info->http_user_header)  ||  !*(s += cafoff))
                        break;
                    continue;
                }
#endif /*HAVE_LIBCONNEXT*/
            } else if (!*has_sid
                       &&  strncasecmp(s, &"\n" HTTP_NCBI_SID[first],
                                       sizeof(HTTP_NCBI_SID) - !!first) == 0) {
                *has_sid = 1/*true*/;
            }
            if (!(s = strchr(++s, '\n')))
                break;
            first = 0/*false*/;
        }
    }
    s = CORE_GetAppName();
    if (s  &&  *s) {
        char buf[128];
        sprintf(buf, "User-Agent: %.80s", s);
        ConnNetInfo_ExtendUserHeader(net_info, buf);
    }
    if ((s = net_info->http_referer) != 0) {
        char* ref;
        if (has_ref <= 0  &&  *s) {
            size_t len = strlen(s);
            if ((ref = (char*) realloc((char*) s, 10 + len)) != 0) {
                memmove(ref + 9, ref, len + 1);
                memcpy(ref, "Referer: ", 9);
                ConnNetInfo_AppendUserHeader(net_info, ref);
            } else
                ref = (char*) s;
        } else
            ref = (char*) s;
        net_info->http_referer = 0;
        assert(ref);
        free(ref);
    }
    if (net_info->external)
        ConnNetInfo_OverrideUserHeader(net_info, NCBI_EXTERNAL ": Y");
    return has_host;
}


static EIO_Status s_CreateHttpConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 int/*bool*/         tunnel,
 THTTP_Flags         flags,
 void*               user_data,
 FHTTP_Adjust        adjust,
 SHttpConnector**    http)
{
    SConnNetInfo*   xxx;
    SHttpConnector* uuu;
    int/*bool*/     ref;
    int/*bool*/     sid;

    *http = 0;
    xxx = (net_info
           ? ConnNetInfo_Clone(net_info)
           : ConnNetInfo_CreateInternal(0));
    if (!xxx)
        return eIO_Unknown;

    if (xxx->req_method >=  eReqMethod_v1) {
        xxx->req_method &= ~eReqMethod_v1;
        xxx->http_version = 1;
    }
    if (xxx->http_version  &&  (flags & fHTTP_PushAuth))
        xxx->http_push_auth = 1;
    if (!tunnel) {
        if (xxx->req_method == eReqMethod_Connect
            ||  (xxx->scheme != eURL_Unspec  && 
                 xxx->scheme != eURL_Https   &&
                 xxx->scheme != eURL_Http)) {
            ConnNetInfo_Destroy(xxx);
            return eIO_InvalidArg;
        }
        if (xxx->scheme == eURL_Unspec)
            xxx->scheme  = eURL_Http;
        ConnNetInfo_SetFrag(xxx, "");
    }

    if (user_header  &&  *user_header
        &&  !ConnNetInfo_OverrideUserHeader(xxx, user_header)) {
        ConnNetInfo_Destroy(xxx);
        return eIO_Unknown;
    }

    if (tunnel) {
        if (!xxx->http_proxy_host[0]  ||  !xxx->http_proxy_port
            ||  xxx->http_proxy_only) {
            ConnNetInfo_Destroy(xxx);
            return eIO_InvalidArg;
        }
        xxx->req_method = eReqMethod_Connect;
        xxx->http_version = 0;
        xxx->path[0] = '\0';
        if (xxx->http_referer) {
            free((void*) xxx->http_referer);
            xxx->http_referer = 0;
        }
        ConnNetInfo_DeleteUserHeader(xxx, "Referer:");
        ref =  0/*false*/;
    } else
        ref = -1/*unset*/;

    if (!(uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector)))) {
        ConnNetInfo_Destroy(xxx);
        return eIO_Unknown;
    }

    if (xxx->max_try < 1  ||  (flags & fHTTP_NoAutoRetry))
        xxx->max_try = 1;

    /* initialize internal data structure */
    uuu->net_info     = xxx;

    uuu->parse_header = 0;
    uuu->user_data    = user_data;
    uuu->adjust       = adjust;
    uuu->cleanup      = 0;

    sid = flags & fHTTP_NoAutomagicSID ? 1 : tunnel;
    uuu->skip_host    = x_FixupUserHeader(xxx, ref, &sid) ? 1 : 0;
    if (sid)
        flags |= fHTTP_NoAutomagicSID;
    uuu->flags        = flags;

    uuu->unsafe_redir = flags & fHTTP_UnsafeRedirects ? eOn : eOff;
    uuu->error_header = eDefault;
    uuu->can_connect  = fCC_None;         /* will be properly set at open    */

    uuu->reserved = 0;
    memset(uuu->unused, 0, sizeof(uuu->unused));

    uuu->sock         = 0;
    uuu->o_timeout    = kDefaultTimeout;  /* deliberately bad values here... */
    uuu->w_timeout    = kDefaultTimeout;  /* ...must be reset prior to use   */
    uuu->http         = 0;
    uuu->r_buf        = 0;
    uuu->w_buf        = 0;
    uuu->w_len        = 0;

    if (tunnel)
        s_OpenHttpConnector(uuu, xxx->timeout);
    /* else there are some unintialized fields left -- they are inited later */

    *http = uuu;
    return eIO_Success;
}


static CONNECTOR s_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,
 void*               user_data,
 FHTTP_Adjust        adjust,
 FHTTP_Cleanup       cleanup)
{
    SHttpConnector* uuu;
    CONNECTOR       ccc;

    if (s_CreateHttpConnector(net_info, user_header, 0/*regular*/,
                              flags, user_data, adjust, &uuu) != eIO_Success) {
        assert(!uuu);
        return 0;
    }
    assert(uuu);

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector)))) {
        s_DestroyHttpConnector(uuu);
        return 0;
    }

    /* initialize additional internal data structure */
    uuu->parse_header = parse_header;
    uuu->cleanup      = cleanup;

    /* enable an override from outside */
    if (!uuu->unsafe_redir)
        uuu->unsafe_redir = eDefault;

    /* initialize connector structure */
    ccc->handle  = uuu;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/

extern CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags)
{
    return s_CreateConnector(net_info, user_header, flags, 0, 0, 0, 0);
}


extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,
 void*               user_data,
 FHTTP_Adjust        adjust,
 FHTTP_Cleanup       cleanup)
{
    return s_CreateConnector(net_info, 0/*user_header*/, flags,
                             parse_header, user_data, adjust, cleanup);
}


extern EIO_Status HTTP_CreateTunnelEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 const void*         init_data,
 size_t              init_size,
 void*               user_data,
 FHTTP_Adjust        adjust,
 SOCK*               sock)
{
    unsigned short  http_code;
    EIO_Status      status;
    SHttpConnector* uuu;

    if (!sock)
        return eIO_InvalidArg;

    status = s_CreateHttpConnector(net_info, 0/*user_header*/, 1/*tunnel*/,
                                   flags | fHTTP_DropUnread,
                                   user_data, adjust, &uuu);
    if (status != eIO_Success) {
        assert(!uuu);
        return status;
    }
    uuu->sock = *sock;
    *sock = 0;
    assert(uuu  &&  !BUF_Size(uuu->w_buf));
    if (!init_size  ||  BUF_Prepend(&uuu->w_buf, init_data, init_size)) {
        status = s_PreRead(uuu, uuu->net_info->timeout, eEM_Wait);
        if (status == eIO_Success) {
            assert(uuu->conn_state == eCS_ReadBody);
            assert(uuu->http_code / 100 == 2);
            assert(uuu->sock);
            *sock = uuu->sock;
            uuu->sock = 0;
            http_code = 0;
        } else
            http_code = uuu->http_code;
    } else {
        http_code = 0;
        status = eIO_Unknown;
    }
    if (uuu->sock)
        s_DropConnection(uuu, eCS_Eom);
    s_DestroyHttpConnector(uuu);
    switch (http_code) {
    case 503:
        return eIO_NotSupported;
    case 426:
    case 404:
        return eIO_InvalidArg;
    case 403:
        return eIO_Closed;
    default:
        break;
    }
    return status;
}


extern EIO_Status HTTP_CreateTunnel
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 SOCK*               sock)
{
    return HTTP_CreateTunnelEx(net_info, flags, 0, 0, 0, 0, sock);
}


extern void HTTP_SetNcbiMessageHook(FHTTP_NcbiMessageHook hook)
{
    if (hook) {
        if (hook != s_MessageHook)
            s_MessageIssued = s_MessageIssued ? -1 : -2;
    } else if (s_MessageIssued < -1)
        s_MessageIssued = 0;
    s_MessageHook = hook;
}
