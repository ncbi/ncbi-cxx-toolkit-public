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
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_HTTP


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* Whether the connector is allowed to connect
 */
typedef enum {
    eCC_None,
    eCC_Once,
    eCC_Unlimited
} ECanConnect;

typedef unsigned       EBCanConnect;
typedef unsigned short TBHCC_Flags;

typedef enum {
    eRM_Regular    = 0,
    eRM_DropUnread = 1,
    eRM_WaitCalled = 2
} EReadMode;


/* All internal data necessary to perform the (re)connect and I/O
 *
 * The following states are defined:
 *  "sock"  | "read_header" | State description
 * ---------+---------------+--------------------------------------------------
 *   NULL   |  <whatever>   | User "WRITE" mode: accumulate data in buffer
 * non-NULL |   non-zero    | HTTP header is being read
 * non-NULL |     zero      | HTTP body is being read (user "READ" mode)
 * ---------+---------------+--------------------------------------------------
 */
typedef struct {
    SConnNetInfo*        net_info;        /* network configuration parameters*/
    FHttpParseHTTPHeader parse_http_hdr;  /* callback to parse HTTP reply hdr*/
    FHttpAdjustNetInfo   adjust_net_info; /* for on-the-fly net_info adjust  */
    FHttpAdjustCleanup   adjust_cleanup;  /* supplemental user data...       */
    void*                adjust_data;     /* ...and cleanup routine          */

    TBHCC_Flags          flags;           /* as passed to constructor        */
    unsigned             error_header:1;  /* only err.HTTP header on SOME dbg*/
    EBCanConnect         can_connect:2;   /* whether more conns permitted    */
    unsigned             read_header:1;   /* whether reading header          */
    unsigned             shut_down:1;     /* whether shut down for write     */
    unsigned             auth_sent:1;     /* authenticate sent               */
    unsigned             reserved:7;      /* MBZ                             */
    unsigned             minor_fault:3;   /* incr each min failure since maj */
    unsigned short       major_fault;     /* incr each maj failure since open*/
    unsigned short       code;            /* last response code              */

    SOCK                 sock;         /* socket;  NULL if not in "READ" mode*/
    const STimeout*      o_timeout;    /* NULL(infinite), dflt or ptr to next*/
    STimeout             oo_timeout;   /* storage for (finite) open timeout  */
    const STimeout*      w_timeout;    /* NULL(infinite), dflt or ptr to next*/
    STimeout             ww_timeout;   /* storage for a (finite) write tmo   */

    BUF                  http;         /* storage for HTTP reply header      */
    BUF                  r_buf;        /* storage to accumulate input data   */
    BUF                  w_buf;        /* storage to accumulate output data  */
    size_t               w_len;        /* pending message body size          */

    size_t               expected;     /* expected to receive until EOF      */
    size_t               received;     /* actually received so far           */
} SHttpConnector;


/* NCBI messaging support */
static int                   s_MessageIssued = 0;
static FHTTP_NcbiMessageHook s_MessageHook   = 0;


typedef enum {
    eRetry_None = 0,
    eRetry_Redirect,
    eRetry_Authenticate,
    eRetry_ProxyAuthenticate
} ERetry;


typedef struct {
    ERetry      mode;
    const char* data;
} SRetry;


/* Try to fix connection parameters (called for an unconnected connector) */
static EIO_Status s_Adjust(SHttpConnector* uuu,
                           const SRetry*   retry,
                           EReadMode       read_mode)
{
    EIO_Status status;

    assert(!uuu->sock  &&  uuu->can_connect != eCC_None);

    if (retry  &&  retry->mode  &&  ++uuu->minor_fault < 3) {
        int fail;
        /* adjust info before yet another connection attempt */
        switch (retry->mode) {
        case eRetry_Redirect:
            if (retry->data  &&  retry->data[0] != '?') {
                if (uuu->net_info->req_method == eReqMethod_Get
                    ||  (uuu->flags & fHCC_InsecureRedirect)
                    ||  !uuu->w_len) {
                    int secure = uuu->net_info->scheme == eURL_Https ? 1 : 0;
                    *uuu->net_info->args = '\0'/*arguments not inherited*/;
                    fail = !ConnNetInfo_ParseURL(uuu->net_info, retry->data);
                    if (!fail  &&  secure
                        &&  uuu->net_info->scheme != eURL_Https
                        &&  !(uuu->flags & fHCC_InsecureRedirect)) {
                        fail = -1;
                    }
                } else
                    fail = -1;
            } else
                fail = 1;
            if (fail) {
                CORE_LOGF_X(2, eLOG_Error,
                            ("[HTTP]  %s redirect to %s%s%s",
                             fail < 0 ? "Prohibited" : "Cannot",
                             retry->data ? "\""        : "<",
                             retry->data ? retry->data : "NULL",
                             retry->data ? "\""        : ">"));
                status = eIO_Closed;
            } else
                status = eIO_Success;
            break;
        case eRetry_Authenticate:
        case eRetry_ProxyAuthenticate:
            fail = uuu->auth_sent ? 1 : -1;
            CORE_LOGF_X(3, eLOG_Error,
                        ("[HTTP]  %s %s %c%s%c",
                         retry->mode == eRetry_Authenticate
                         ? "Authorization" : "Proxy authorization",
                         fail < 0 ? "not yet implemented" : "failed",
                         "(<"[!retry->data],
                         retry->data ? retry->data : "NULL",
                         ")>"[!retry->data]));
            status = fail < 0 ? eIO_NotSupported : eIO_Closed;
            break;
        default:
            status = eIO_Unknown;
            assert(0);
            break;
        }
        if (status != eIO_Success) {
            uuu->can_connect = eCC_None;
            return status;
        }
    } else {
        const char* msg;
        uuu->minor_fault = 0;
        /* we're here because something is going wrong */
        if (++uuu->major_fault >= uuu->net_info->max_try) {
            msg = read_mode != eRM_DropUnread  &&  uuu->major_fault > 1
                ? "[HTTP]  Too many failed attempts (%d), giving up" : "";
        } else if (!uuu->adjust_net_info
                   ||  uuu->adjust_net_info(uuu->net_info,
                                            uuu->adjust_data,
                                            uuu->major_fault) == 0) {
            msg = read_mode != eRM_DropUnread  &&  uuu->major_fault > 1
                ? "[HTTP]  Retry attempts (%d) exhausted, giving up" : "";
        } else
            msg = 0;
        if (msg) {
            if (*msg) {
                CORE_LOGF_X(1, eLOG_Error,
                            (msg, uuu->major_fault));
            }
            uuu->can_connect = eCC_None;
            return eIO_Closed;
        }
    }

    ConnNetInfo_AdjustForHttpProxy(uuu->net_info);
    return eIO_Success;
}


/* Unconditionally drop the connection; timeout may specify time allowance */
static void s_DropConnection(SHttpConnector* uuu, const STimeout* timeout)
{
    assert(uuu->sock);
    BUF_Erase(uuu->http);
    if (uuu->read_header)
        SOCK_Abort(uuu->sock);
    else
        SOCK_SetTimeout(uuu->sock, eIO_Close, timeout);
    SOCK_Close(uuu->sock);
    uuu->sock = 0;
}


/* Connect to the HTTP server, specified by uuu->net_info's "port:host".
 * Return eIO_Success only if socket connection has succeeded and uuu->sock
 * is non-zero. If unsuccessful, try to adjust uuu->net_info by s_Adjust(),
 * and then re-try the connection attempt.
 */
static EIO_Status s_Connect(SHttpConnector* uuu,
                            EReadMode       read_mode)
{
    EIO_Status status;

    assert(!uuu->sock);
    if (uuu->can_connect == eCC_None) {
        if (read_mode == eRM_Regular)
            CORE_LOG_X(5, eLOG_Error, "[HTTP]  Connector no longer usable");
        return eIO_Closed;
    }

    /* the re-try loop... */
    for (;;) {
        int/*bool*/ reset_user_header = 0;
        char*       http_user_header = 0;
        TSOCK_Flags flags;

        uuu->w_len = BUF_Size(uuu->w_buf);
        if (uuu->net_info->http_user_header)
            http_user_header = strdup(uuu->net_info->http_user_header);
        if (!uuu->net_info->http_user_header == !http_user_header) {
            ConnNetInfo_ExtendUserHeader
                (uuu->net_info, "User-Agent: NCBIHttpConnector"
#ifdef NCBI_CXX_TOOLKIT
                 " (C++ Toolkit)"
#else
                 " (C Toolkit)"
#endif
                 "\r\n");
            reset_user_header = 1;
        }
        if (uuu->net_info->debug_printout)
            ConnNetInfo_Log(uuu->net_info, CORE_GetLOG());
        flags = (uuu->net_info->debug_printout == eDebugPrintout_Data
                 ? fSOCK_LogOn : fSOCK_LogDefault);
        if (uuu->net_info->scheme == eURL_Https)
            flags |= fSOCK_Secure;
        if (!(uuu->flags & fHCC_NoUpread))
            flags |= fSOCK_ReadOnWrite;
        /* connect & send HTTP header */
        if ((status = URL_ConnectEx
             (uuu->net_info->host,       uuu->net_info->port,
              uuu->net_info->path,       uuu->net_info->args,
              uuu->net_info->req_method, uuu->w_len,
              uuu->o_timeout,            uuu->w_timeout,
              uuu->net_info->http_user_header,
              (int/*bool*/)(uuu->flags & fHCC_UrlEncodeArgs),
              flags, &uuu->sock)) != eIO_Success) {
            uuu->sock = 0;
        }
        if (reset_user_header) {
            ConnNetInfo_SetUserHeader(uuu->net_info, 0);
            uuu->net_info->http_user_header = http_user_header;
        }
        if (uuu->sock)
            break/*success*/;

        /* connection failed, no socket was created */
        if (s_Adjust(uuu, 0, read_mode) != eIO_Success)
            break;
    }

    return status;
}


/* Connect to the server specified by uuu->net_info, then compose and form
 * relevant HTTP header, and flush the accumulated output data(uuu->w_buf)
 * after the HTTP header. If connection/write unsuccessful, retry to reconnect
 * and send the data again until permitted by s_Adjust().
 */
static EIO_Status s_ConnectAndSend(SHttpConnector* uuu,
                                   EReadMode       read_mode)
{
    EIO_Status status;

    for (;;) {
        size_t body_size;
        char   buf[4096];

        if (!uuu->sock) {
            if ((status = s_Connect(uuu, read_mode)) != eIO_Success)
                break;
            assert(uuu->sock);
            uuu->read_header = 1/*true*/;
            uuu->shut_down = 0/*false*/;
            uuu->expected = 0;
            uuu->received = 0;
            uuu->code = 0;
        } else
            status = eIO_Success;

        if (uuu->w_len) {
            size_t off = BUF_Size(uuu->w_buf) - uuu->w_len;

            SOCK_SetTimeout(uuu->sock, eIO_Write, uuu->w_timeout);
            do {
                size_t n_written;
                size_t n_write = BUF_PeekAt(uuu->w_buf, off, buf, sizeof(buf));
                status = SOCK_Write(uuu->sock, buf, n_write,
                                    &n_written, eIO_WritePlain);
                if (status != eIO_Success)
                    break;
                uuu->w_len -= n_written;
                off        += n_written;
            } while (uuu->w_len);
        } else if (!uuu->shut_down)
            status = SOCK_Write(uuu->sock, 0, 0, 0, eIO_WritePlain);

        if (status == eIO_Success) {
            assert(uuu->w_len == 0);
            if (!uuu->shut_down) {
                /* 10/07/03: While this call here is perfectly legal, it could
                 * cause connection severed by a buggy CISCO load-balancer. */
                /* 10/28/03: CISCO's beta patch for their LB shows that the
                 * problem has been fixed; no more 2'30" drops in connections
                 * that shut down for write.  We still leave this commented
                 * out to allow unpatched clients to work seamlessly... */ 
                /*SOCK_Shutdown(uuu->sock, eIO_Write);*/
                uuu->shut_down = 1;
            }
            break;
        }

        if (status == eIO_Timeout
            &&  (read_mode == eRM_WaitCalled
                 ||  (uuu->w_timeout
                      &&  !(uuu->w_timeout->sec | uuu->w_timeout->usec)))) {
            break;
        }
        if ((body_size = BUF_Size(uuu->w_buf))) {
            sprintf(buf, "writing request body at offset %lu",
                    (unsigned long)(body_size - uuu->w_len));
        } else
            strcpy(buf, "flushing request header");
        CORE_LOGF_X(6, eLOG_Error,
                    ("[HTTP]  Error %s (%s)", buf, IO_StatusStr(status)));

        /* write failed; close and try to use another server */
        s_DropConnection(uuu, 0/*no wait*/);
        if ((status = s_Adjust(uuu, 0, read_mode)) != eIO_Success)
            break/*closed*/;
    }

    return status;
}


static int/*bool*/ s_IsValidParam(const char* param, size_t parlen)
{
    const char* e = (const char*) memchr(param, '=', parlen);
    size_t toklen;
    if (!e  ||  e == param)
        return 0/*false*/;
    if ((toklen = (size_t)(++e - param)) >= parlen)
        return 0/*false*/;
    assert(!isspace((unsigned char)(*param)));
    if (strcspn(param, " \t") < toklen)
        return 0/*false*/;
    if (*e == '\''  ||  *e == '"') {
        /* a quoted string */
        toklen = parlen - toklen;
        if (!(e = (const char*) memchr(e + 1, *e, --toklen)))
            return 0/*false*/;
        e++/*skip the quote*/;
    } else
        e += strcspn(e, " \t");
    if (e != param + parlen  &&  e + strspn(e, " \t") != param + parlen)
        return 0/*false*/;
    return 1/*true*/;
}


static int/*bool*/ s_IsValidAuth(const char* challenge, size_t len)
{
    /* Challenge must contain a scheme name token and a non-empty param
     * list (comma-separated pairs token={token|quoted_string}), with at
     * least one parameter being named "realm=".
     */
    size_t word = strcspn(challenge, " \t");
    int retval = 0/*false*/;
    if (word < len) {
        /* 1st word is always the scheme name */
        const char* param = challenge + word;
        for (param += strspn(param, " \t");  param < challenge + len;
             param += strspn(param, ", \t")) {
            size_t parlen = (size_t)(challenge + len - param);
            const char* c = (const char*) memchr(param, ',', parlen);
            if (c)
                parlen = (size_t)(c - param);
            if (!s_IsValidParam(param, parlen))
                return 0/*false*/;
            if (parlen > 6  &&  strncasecmp(param, "realm=", 6) == 0)
                retval = 1/*true, but keep scanning*/;
            param += c ? ++parlen : parlen;
        }
    }
    return retval;
}


/* Parse HTTP header */
static EIO_Status s_ReadHeader(SHttpConnector* uuu,
                               SRetry*         retry,
                               EReadMode       read_mode)
{
    int        server_error = 0;
    int        http_status;
    EIO_Status status;
    char*      header;
    size_t     size;

    assert(uuu->sock  &&  uuu->read_header);
    retry->mode = eRetry_None;
    retry->data = 0;

    /* line by line HTTP header input */
    for (;;) {
        /* do we have full header yet? */
        size = BUF_Size(uuu->http);
        if (!(header = (char*) malloc(size + 1))) {
            CORE_LOGF_X(7, eLOG_Error,
                        ("[HTTP]  Cannot allocate header, %lu bytes",
                         (unsigned long) size));
            return eIO_Unknown;
        }
        verify(BUF_Peek(uuu->http, header, size) == size);
        header[size] = '\0';
        if (size >= 4  &&  strcmp(&header[size - 4], "\r\n\r\n") == 0)
            break/*full header captured*/;
        free(header);

        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &uuu->http, 0);
        if (status != eIO_Success) {
            ELOG_Level level;
            if (status == eIO_Timeout) {
                const STimeout* tmo = SOCK_GetTimeout(uuu->sock, eIO_Read);
                if (!tmo)
                    level = eLOG_Error;
                else if (read_mode == eRM_WaitCalled)
                    return status;
                else if (tmo->sec | tmo->usec)
                    level = eLOG_Warning;
                else
                    level = eLOG_Trace;
            } else
                level = eLOG_Error;
            CORE_LOGF_X(8, level, ("[HTTP]  Error reading header (%s)",
                                   IO_StatusStr(status)));
            return status;
        }
    }
    /* the entire header has been read */
    uuu->read_header = 0/*false*/;
    status = eIO_Success;
    BUF_Erase(uuu->http);

    /* HTTP status must come on the first line of the response */
    if (sscanf(header, "HTTP/%*d.%*d %d ", &http_status) != 1 || !http_status)
        http_status = -1;
    if (http_status < 200  ||  299 < http_status) {
        server_error = http_status;
        if (http_status == 301  ||  http_status == 302  ||  http_status == 307)
            retry->mode = eRetry_Redirect;
        else if (http_status == 401)
            retry->mode = eRetry_Authenticate;
        else if (http_status == 407)
            retry->mode = eRetry_ProxyAuthenticate;
        else if (http_status < 0  ||  http_status == 403 || http_status == 404)
            uuu->net_info->max_try = 0;
    }
    uuu->code = http_status < 0 ? -1 : http_status;

    if ((server_error  ||  !uuu->error_header)
        &&  uuu->net_info->debug_printout == eDebugPrintout_Some) {
        /* HTTP header gets printed as part of data logging when
           uuu->net_info->debug_printout == eDebugPrintout_Data. */
        const char* header_header;
        if (!server_error)
            header_header = "HTTP header";
        else if (uuu->flags & fHCC_KeepHeader)
            header_header = "HTTP header (error)";
        else if (uuu->code == 301  ||  uuu->code == 302  ||  uuu->code == 307)
            header_header = "HTTP header (moved)";
        else if (!uuu->net_info->max_try)
            header_header = "HTTP header (unrecoverable error)";
        else
            header_header = "HTTP header (server error, can retry)";
        CORE_DATA_X(19, header, size, header_header);
    }

    {{
        /* parsing "NCBI-Message" tag */
        static const char kNcbiMessageTag[] = "\n" HTTP_NCBI_MESSAGE " ";
        const char* s;
        for (s = strchr(header, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
            if (strncasecmp(s, kNcbiMessageTag, sizeof(kNcbiMessageTag)-1)==0){
                const char* message = s + sizeof(kNcbiMessageTag) - 1;
                while (*message  &&  isspace((unsigned char)(*message)))
                    message++;
                if (!(s = strchr(message, '\r')))
                    s = strchr(message, '\n');
                assert(s);
                do {
                    if (!isspace((unsigned char) s[-1]))
                        break;
                } while (--s > message);
                if (message != s) {
                    if (s_MessageHook) {
                        if (s_MessageIssued <= 0) {
                            s_MessageIssued  = 1;
                            s_MessageHook(message);
                        }
                    } else {
                        s_MessageIssued = -1;
                        CORE_LOGF_X(10, eLOG_Critical,
                                    ("[NCBI-MESSAGE]  %.*s",
                                     (int)(s - message), message));
                    }
                }
                break;
            }
        }
    }}

    if (uuu->flags & fHCC_KeepHeader) {
        if (!BUF_Write(&uuu->r_buf, header, size)) {
            CORE_LOG_X(11, eLOG_Error, "[HTTP]  Cannot keep HTTP header");
            status = eIO_Unknown;
        }
        free(header);
        return status;
    }

    if (uuu->parse_http_hdr
        &&  !(*uuu->parse_http_hdr)(header, uuu->adjust_data, server_error)) {
        server_error = 1/*fake, but still boolean true*/;
    }

    if (retry->mode == eRetry_Redirect) {
        /* parsing "Location" pointer */
        static const char kLocationTag[] = "\nLocation: ";
        char* s;
        for (s = strchr(header, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
            if (strncasecmp(s, kLocationTag, sizeof(kLocationTag) - 1) == 0) {
                char* location = s + sizeof(kLocationTag) - 1;
                while (*location  &&  isspace((unsigned char)(*location)))
                    location++;
                if (!(s = strchr(location, '\r')))
                    s = strchr(location, '\n');
                if (!s)
                    break;
                do {
                    if (!isspace((unsigned char) s[-1]))
                        break;
                } while (--s > location);
                if (s != location) {
                    size_t len = (size_t)(s - location);
                    memmove(header, location, len);
                    header[len] = '\0';
                    retry->data = header;
                }
                break;
            }
        }
    } else if (retry->mode != eRetry_None) {
        /* parsing "Authenticate" tags */
        static const char kAuthenticateTag[] = "-Authenticate: ";
        char* s;
        for (s = strchr(header, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
            if (strncasecmp(s + (retry->mode == eRetry_Authenticate ? 4 : 6),
                            kAuthenticateTag, sizeof(kAuthenticateTag)-1)==0){
                if ((retry->mode == eRetry_Authenticate
                     &&  strncasecmp(s, "\nWWW",   4) == 0)  ||
                    (retry->mode == eRetry_ProxyAuthenticate
                     &&  strncasecmp(s, "\nProxy", 6) == 0)) {
                    char* challenge = s + sizeof(kAuthenticateTag) - 1, *e;
                    challenge += retry->mode == eRetry_Authenticate ? 4 : 6;
                    while (*challenge && isspace((unsigned char)(*challenge)))
                        challenge++;
                    if (!(e = strchr(challenge, '\r')))
                        e = strchr(challenge, '\n');
                    else
                        s = e;
                    if (!e)
                        break;
                    do {
                        if (!isspace((unsigned char) e[-1]))
                            break;
                    } while (--e > challenge);
                    if (e != challenge) {
                        size_t len = (size_t)(e - challenge);
                        if (s_IsValidAuth(challenge, len)) {
                            memmove(header, challenge, len);
                            header[len] = '\0';
                            retry->data = header;
                            break;
                        }
                    }
                }
            }
        }
    } else if (!server_error) {
        static const char kContentLengthTag[] = "\nContent-Length: ";
        const char* s;
        for (s = strchr(header, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
            if (!strncasecmp(s,kContentLengthTag,sizeof(kContentLengthTag)-1)){
                const char* expected = s + sizeof(kContentLengthTag) - 1;
                while (*expected  &&  isspace((unsigned char)(*expected)))
                    expected++;
                if (!(s = strchr(expected, '\r')))
                    s = strchr(expected, '\n');
                assert(s);
                do {
                    if (!isspace((unsigned char) s[-1]))
                        break;
                } while (--s > expected);
                if (s != expected) {
                    char* e;
                    errno = 0;
                    uuu->expected = (size_t) strtol(expected, &e, 10);
                    if (errno  ||  e != s)
                        uuu->expected = 0;
                    else if (!uuu->expected)
                        uuu->expected = (size_t)(-1L);
                }
                break;
            }
        }
    }
    if (!retry->data)
        free(header);

    /* skip & printout the content, if server error was flagged */
    if (server_error && uuu->net_info->debug_printout == eDebugPrintout_Some) {
        BUF   buf = 0;
        char* body;

        SOCK_SetTimeout(uuu->sock, eIO_Read, 0);
        /* read until EOF */
        SOCK_StripToPattern(uuu->sock, 0, 0, &buf, 0);
        if (!(size = BUF_Size(buf))) {
            CORE_LOG_X(12, eLOG_Trace,
                       "[HTTP]  No HTTP body received with this error");
        } else if ((body = (char*) malloc(size)) != 0) {
            size_t n = BUF_Read(buf, body, size);
            if (n != size) {
                CORE_LOGF_X(13, eLOG_Error,
                            ("[HTTP]  Cannot read server error "
                             "body from buffer (%lu out of %lu)",
                             (unsigned long) n, (unsigned long) size));
            }
            CORE_DATA_X(20, body, n, "Server error body");
            free(body);
        } else {
            CORE_LOGF_X(14, eLOG_Error,
                        ("[HTTP]  Cannot allocate server error "
                         "body, %lu bytes", (unsigned long) size));
        }
        BUF_Destroy(buf);
    }

    return server_error ? eIO_Unknown : status;
}


/* Prepare connector for reading. Open socket if necessary and
 * make initial connect and send, re-trying if possible until success.
 * Return codes:
 *   eIO_Success = success, connector is ready for reading (uuu->sock != NULL);
 *   eIO_Timeout = maybe (check uuu->sock) connected and no data yet available;
 *   other code  = error, not connected (uuu->sock == NULL).
 */
static EIO_Status s_PreRead(SHttpConnector* uuu,
                            const STimeout* timeout,
                            EReadMode       read_mode)
{
    EIO_Status status;

    for (;;) {
        EIO_Status adjust_status;
        SRetry     retry;

        status = s_ConnectAndSend(uuu, read_mode);
        if (!uuu->sock) {
            assert(status != eIO_Success);
            break;
        }
        if (status != eIO_Success) {
            if (status != eIO_Timeout  ||
                status == SOCK_Status(uuu->sock, eIO_Read)/*pending*/)
                break;
        }

        /* set timeout */
        verify(SOCK_SetTimeout(uuu->sock, eIO_Read, timeout) == eIO_Success);

        if (!uuu->read_header)
            break;

        if ((status = s_ReadHeader(uuu, &retry, read_mode)) == eIO_Success) {
            /* pending output data no longer needed */
            assert(!uuu->read_header  &&  !retry.mode);
            BUF_Erase(uuu->w_buf);
            break;
        }
        assert(status != eIO_Timeout  ||  !retry.mode);
        /* if polling then bail out with eIO_Timeout */
        if (status == eIO_Timeout
            &&  (read_mode == eRM_WaitCalled
                 ||  (timeout  &&  !(timeout->sec | timeout->usec)))) {
            assert(!retry.data);
            break;
        }

        /* HTTP header read error; disconnect and try to use another server */
        s_DropConnection(uuu, 0/*no wait*/);
        adjust_status = s_Adjust(uuu, retry.mode ? &retry : 0, read_mode);
        if (retry.data)
            free((void*) retry.data);
        if (adjust_status != eIO_Success) {
            if (adjust_status != eIO_Closed)
                status = adjust_status;
            break;
        }
    }

    return status;
}


/* Read non-header data from connection */
static EIO_Status s_Read(SHttpConnector* uuu, void* buf,
                         size_t size, size_t* n_read)
{
    EIO_Status status;

    assert(uuu->sock);
    if (uuu->flags & fHCC_UrlDecodeInput) {
        /* read and URL-decode */
        size_t     n_peeked, n_decoded;
        size_t     peek_size = 3 * size;
        void*      peek_buf  = malloc(peek_size);

        /* peek the data */
        status= SOCK_Read(uuu->sock,peek_buf,peek_size,&n_peeked,eIO_ReadPeek);
        if (status != eIO_Success) {
            assert(!n_peeked);
            *n_read = 0;
        } else {
            if (URL_Decode(peek_buf,n_peeked,&n_decoded,buf,size,n_read)) {
                /* decode, then discard successfully decoded data from input */
                if (n_decoded) {
                    SOCK_Read(uuu->sock,0,n_decoded,&n_peeked,eIO_ReadPersist);
                    assert(n_peeked == n_decoded);
                    uuu->received += n_decoded;
                    status = eIO_Success;
                } else if (SOCK_Status(uuu->sock, eIO_Read) == eIO_Closed) {
                    /* we are at EOF, and remaining data cannot be decoded */
                    status = eIO_Unknown;
                }
            } else
                status = eIO_Unknown;

            if (status != eIO_Success)
                CORE_LOG_X(16, eLOG_Error, "[HTTP]  Cannot URL-decode data");
        }
        free(peek_buf);
    } else {
        /* just read, with no URL-decoding */
        status = SOCK_Read(uuu->sock, buf, size, n_read, eIO_ReadPlain);
        uuu->received += *n_read;
    }

    if (uuu->expected) {
        if (uuu->received > uuu->expected)
            return eIO_Unknown/*received too much*/;
        if (uuu->expected != (size_t)(-1L)) {
            if (status == eIO_Closed  &&  uuu->expected > uuu->received)
                return eIO_Unknown/*received too little*/;
        } else if (uuu->received)
            return eIO_Unknown/*received too much*/;
    }
    return status;
}


/* Reset/readout input data and close socket */
static EIO_Status s_Disconnect(SHttpConnector* uuu,
                               const STimeout* timeout,
                               EReadMode       read_mode)
{
    EIO_Status status = eIO_Success;

    if (read_mode == eRM_DropUnread)
        BUF_Erase(uuu->r_buf);
    else if ((status = s_PreRead(uuu, timeout, read_mode)) == eIO_Success) {
        do {
            char   buf[4096];
            size_t x_read;
            status = s_Read(uuu, buf, sizeof(buf), &x_read);
            if (!BUF_Write(&uuu->r_buf, buf, x_read))
                status = eIO_Unknown;
        } while (status == eIO_Success);
        if (status == eIO_Closed)
            status = eIO_Success;
    }

    if (uuu->sock) /* s_PreRead() might have dropped the connection already */
        s_DropConnection(uuu, timeout);
    if (uuu->can_connect == eCC_Once)
        uuu->can_connect =  eCC_None;

    return status;
}


/* Send the accumulated output data(if any) to server, then close the socket.
 * Regardless of the flush, clear both input and output buffers.
 */
static void s_FlushAndDisconnect(SHttpConnector* uuu,
                                 const STimeout* timeout)
{
    if (uuu->can_connect != eCC_None  &&  !uuu->sock
        &&  ((uuu->flags & fHCC_SureFlush)  ||  BUF_Size(uuu->w_buf))) {
        /* "WRITE" mode and data (or just flag) is still pending */
        s_PreRead(uuu, timeout, eRM_DropUnread);
    }
    s_Disconnect(uuu, timeout, eRM_DropUnread);
    assert(!uuu->sock);

    /* clear pending output data, if any */
    BUF_Erase(uuu->w_buf);
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

    static void        s_Setup      (SMetaConnector *meta,
                                     CONNECTOR connector);
    static void        s_Destroy    (CONNECTOR connector);
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
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* NOTE: the real connect will be performed on the first "READ", or
     * "CLOSE", or on "WAIT" on read -- see in "s_ConnectAndSend()" */

    assert(!uuu->sock);

    /* store timeouts for later use */
    if (timeout) {
        uuu->oo_timeout = *timeout;
        uuu->o_timeout  = &uuu->oo_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->o_timeout  = timeout;
        uuu->w_timeout  = timeout;
    }

    /* reset the auto-reconnect feature */
    uuu->can_connect = (uuu->flags & fHCC_AutoReconnect
                        ? eCC_Unlimited : eCC_Once);
    uuu->major_fault = 0;
    uuu->minor_fault = 0;
    uuu->auth_sent   = 0;

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
        if (uuu->can_connect == eCC_None)
            return eIO_Closed;
        if ((status = s_PreRead(uuu, timeout, eRM_WaitCalled)) != eIO_Success)
            return status;
        assert(uuu->sock);
        return SOCK_Wait(uuu->sock, eIO_Read, timeout);
    case eIO_Write:
        /* Return 'Closed' if no more writes are allowed (and now - reading) */
        return uuu->can_connect == eCC_None
            ||  (uuu->sock  &&  uuu->can_connect == eCC_Once)
            ? eIO_Closed : eIO_Success;
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

    /* if trying to "WRITE" after "READ" then close the socket,
     * and so switch to "WRITE" mode */
    if (uuu->sock) {
        EIO_Status status = s_Disconnect(uuu, timeout,
                                         uuu->flags & fHCC_DropUnread
                                         ? eRM_DropUnread : eRM_Regular);
        if (status != eIO_Success)
            return status;
    }
    if (uuu->can_connect == eCC_None)
        return eIO_Closed; /* no more connects permitted */

    /* accumulate all output in the memory buffer */
    if (size  &&  (uuu->flags & fHCC_UrlEncodeOutput)) {
        /* with URL-encoding */
        size_t dst_size = 3 * size;
        void*  dst = malloc(dst_size);
        URL_Encode(buf, size, n_written, dst, dst_size, &dst_size);
        if (*n_written != size  ||  !BUF_Write(&uuu->w_buf, dst, dst_size)) {
            if (dst)
                free(dst);
            return eIO_Unknown;
        }
        free(dst);
    } else {
        /* "as is" (without URL-encoding) */
        if (!BUF_Write(&uuu->w_buf, buf, size))
            return eIO_Unknown;
        *n_written = size;
    }

    /* store the write timeout */
    if (timeout) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else
        uuu->w_timeout  = timeout;
    return eIO_Success;
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    assert(connector->meta);
    if (!(uuu->flags & fHCC_Flushable)  ||  uuu->sock) {
        /* The real flush will be performed on the first "READ" (or "CLOSE"),
         * or on "WAIT". Here, we just store the write timeout, that's all...
         * ADDENDUM: fHCC_Flushable connectors are able to actually flush data.
         */
        if (timeout) {
            uuu->ww_timeout = *timeout;
            uuu->w_timeout  = &uuu->ww_timeout;
        } else
            uuu->w_timeout  = timeout;
        
        return eIO_Success;
    }
    if (!connector->meta->wait)
        return eIO_NotSupported;

    return connector->meta->wait(connector->meta->c_wait, eIO_Read, timeout);
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status = s_PreRead(uuu, timeout, eRM_Regular);
    size_t x_read = BUF_Read(uuu->r_buf, buf, size);

    *n_read = x_read;
    if (x_read < size) {
        if (status != eIO_Success)
            return status;
        status = s_Read(uuu, (char*) buf + x_read, size - x_read, n_read);
        *n_read += x_read;
    } else
        status = eIO_Success;

    return *n_read ? eIO_Success : status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    return uuu->sock ? SOCK_Status(uuu->sock, dir) :
        (uuu->can_connect == eCC_None ? eIO_Closed : eIO_Success);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    s_FlushAndDisconnect((SHttpConnector*) connector->handle, timeout);
    return eIO_Success;
}


static void s_Setup(SMetaConnector *meta, CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

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


static void s_Destroy(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    connector->handle = 0;

    if (uuu->adjust_cleanup)
        uuu->adjust_cleanup(uuu->adjust_data);
    ConnNetInfo_Destroy(uuu->net_info);
    BUF_Destroy(uuu->http);
    BUF_Destroy(uuu->r_buf);
    BUF_Destroy(uuu->w_buf);
    free(uuu);
    free(connector);
}


/* NB: per the standard, HTTP tag name misspelled as "Referer" */
static void s_AddRefererStripCAF(SConnNetInfo* net_info)
{
    const char* s;
    char* referer;
    if (!net_info)
        return;
    if ((s = net_info->http_user_header) != 0) {
        int/*bool*/ found = 0/*false*/;
        int/*bool*/ first = 1/*true*/;
        while (*s) {
            if (strncasecmp(s, "\nReferer: " + first, 10 - first) == 0) {
                found = 1/*true*/;
#ifdef HAVE_LIBCONNEXT
            } else if (strncasecmp(s, "\nCAF" + first, 4 - first) == 0
                       &&  (s[4 - first] == '-'  ||  s[4 - first] == ':')) {
                size_t off = (size_t)(s - net_info->http_user_header);
                ConnNetInfo_DeleteUserHeader(net_info, s + !first);
                if (!(s = net_info->http_user_header)  ||  !*(s += off))
                    break;
                continue;
#else
                break;
#endif /*HAVE_LIBCONNEXT*/
            }
            if (!(s = strchr(++s, '\n')))
                break;
            first = 0/*false*/;
        }
        if (found)
            return;
    }
    if (!net_info->http_referer  ||  !*net_info->http_referer  ||
        !(referer = (char*) malloc(strlen(net_info->http_referer) + 12))) {
        return;
    }
    sprintf(referer, "Referer: %s\r\n", net_info->http_referer);
    ConnNetInfo_ExtendUserHeader(net_info, referer);
    free(referer);
}


static CONNECTOR s_CreateConnector
(const SConnNetInfo*  net_info,
 const char*          user_header,
 THCC_Flags           flags,
 FHttpParseHTTPHeader parse_http_hdr,
 FHttpAdjustNetInfo   adjust_net_info,
 void*                adjust_data,
 FHttpAdjustCleanup   adjust_cleanup)
{
    char            value[32];
    CONNECTOR       ccc;
    SHttpConnector* uuu;
    SConnNetInfo*   xxx;
    char*           fff;

    xxx = net_info ? ConnNetInfo_Clone(net_info) : ConnNetInfo_Create(0);
    if (!xxx)
        return 0;
    if ((flags & fHCC_NoAutoRetry)  ||  !xxx->max_try)
        xxx->max_try = 1;

    if ((fff = strchr(xxx->args, '#')) != 0)
        *fff = '\0';
    if ((xxx->scheme != eURL_Unspec  && 
         xxx->scheme != eURL_Https   &&
         xxx->scheme != eURL_Http)   ||
        !ConnNetInfo_AdjustForHttpProxy(xxx)) {
        ConnNetInfo_Destroy(xxx);
        return 0;
    }
    if (xxx->scheme == eURL_Unspec)
        xxx->scheme = eURL_Http;

    if (!(ccc = (SConnector    *) malloc(sizeof(SConnector    )))  ||
        !(uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector)))) {
        if (ccc)
            free(ccc);
        ConnNetInfo_Destroy(xxx);
        return 0;
    }
    if (user_header)
        ConnNetInfo_OverrideUserHeader(xxx, user_header);
    s_AddRefererStripCAF(xxx);

    ConnNetInfo_GetValue(0, "HTTP_INSECURE_REDIRECT", value, sizeof(value),"");
    if (ConnNetInfo_Boolean(value))
        flags |= fHCC_InsecureRedirect;

    /* initialize internal data structure */
    uuu->net_info        = xxx;

    uuu->parse_http_hdr  = parse_http_hdr;
    uuu->adjust_net_info = adjust_net_info;
    uuu->adjust_cleanup  = adjust_cleanup;
    uuu->adjust_data     = adjust_data;

    uuu->flags           = flags;
    uuu->reserved        = 0;
    uuu->can_connect     = eCC_None;         /* will be properly set at open*/

    ConnNetInfo_GetValue(0, "HTTP_ERROR_HEADER_ONLY", value, sizeof(value),"");
    uuu->error_header    = ConnNetInfo_Boolean(value);

    uuu->sock            = 0;
    uuu->o_timeout       = kDefaultTimeout;  /* deliberately bad values --  */
    uuu->w_timeout       = kDefaultTimeout;  /* must be reset prior to use  */
    uuu->http            = 0;
    uuu->r_buf           = 0;
    uuu->w_buf           = 0;
    /* there are some unintialized fields left -- they are initted later */

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
 THCC_Flags          flags)
{
    return s_CreateConnector(net_info, user_header, flags, 0, 0, 0, 0);
}


extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo*  net_info,
 THCC_Flags           flags,
 FHttpParseHTTPHeader parse_http_hdr,
 FHttpAdjustNetInfo   adjust_net_info,
 void*                adjust_data,
 FHttpAdjustCleanup   adjust_cleanup)
{
    return s_CreateConnector(net_info, 0, flags, parse_http_hdr,
                             adjust_net_info, adjust_data, adjust_cleanup);
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
