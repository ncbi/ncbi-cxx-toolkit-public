/*  $Id$
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

/* If the connector is allowed to connect
 */
typedef enum {
    eCC_None,
    eCC_Once,
    eCC_Unlimited
} ECanConnect;

typedef unsigned EBCanConnect;
typedef unsigned TBHCC_Flags;

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

    TBHCC_Flags          flags:10;        /* as passed to constructor        */
    unsigned             reserved:1;
    unsigned             error_header:1;  /* only err.HTTP header on SOME dbg*/
    EBCanConnect         can_connect:2;   /* whether more conns permitted    */
    unsigned             read_header:1;   /* whether reading header          */
    unsigned             shut_down:1;     /* whether shut down for write     */
    unsigned short       failure_count;   /* incr each failure since open    */

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


/* Try to fix connection parameters (called for an unconnected connector) */
static int/*bool*/ s_Adjust(SHttpConnector* uuu,
                            char**          redirect,
                            EReadMode       read_mode)
{
    assert(!uuu->sock  &&  uuu->can_connect != eCC_None);
    /* we're here because something is going wrong */
    if (++uuu->failure_count >= uuu->net_info->max_try) {
        if (*redirect) {
            free(*redirect);
            *redirect = 0;
        }
        if (read_mode != eRM_DropUnread  &&  uuu->failure_count > 1) {
            CORE_LOGF_X(1, eLOG_Error,
                        ("[HTTP]  Too many failed attempts (%d),"
                         " giving up", uuu->failure_count));
        }
        uuu->can_connect = eCC_None;
        return 0/*failure*/;
    }
    /* adjust info before yet another connection attempt */
    if (*redirect) {
        int status;
        assert(**redirect);
        if (**redirect != '?') {
            *uuu->net_info->args = '\0'; /*arguments are not inherited*/
            status = ConnNetInfo_ParseURL(uuu->net_info, *redirect);
        } else
            status = 0/*failed*/;
        if (!status) {
            CORE_LOGF_X(2, eLOG_Error,
                        ("[HTTP]  Unable to redirect to \"%s\"", *redirect));
        }
        free(*redirect);
        *redirect = 0;
        if (!status) {
            uuu->can_connect = eCC_None;
            return 0/*failure*/;
        }
    } else if (!uuu->adjust_net_info
               ||  uuu->adjust_net_info(uuu->net_info,
                                        uuu->adjust_data,
                                        uuu->failure_count) == 0) {
        if (read_mode != eRM_DropUnread  &&  uuu->failure_count > 1) {
            CORE_LOGF_X(3, eLOG_Error,
                        ("[HTTP]  Retry attempts (%d) exhausted,"
                         " giving up", uuu->failure_count));
        }
        uuu->can_connect = eCC_None;
        return 0/*failure*/;
    }

    ConnNetInfo_AdjustForHttpProxy(uuu->net_info);
    return 1/*success*/;
}


/* Unconditionally drop the connection; timeout may specify time allowance */
static void s_DropConnection(SHttpConnector* uuu, const STimeout* timeout)
{
    size_t http_size = BUF_Size(uuu->http);
    assert(uuu->sock);
    if (http_size  &&  BUF_Read(uuu->http, 0, http_size) != http_size) {
        CORE_LOG_X(4, eLOG_Error, "[HTTP]  Cannot discard HTTP header buffer");
        assert(0);
    }
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
    assert(!uuu->sock);
    if (uuu->can_connect == eCC_None) {
        CORE_LOG_X(5, eLOG_Error, "[HTTP]  Connector is no longer usable");
        return eIO_Closed;
    }

    /* the re-try loop... */
    for (;;) {
        int/*bool*/ reset_user_header = 0;
        char*       http_user_header = 0;
        char*       null = 0;

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
        /* connect & send HTTP header */
        uuu->sock = URL_Connect
            (uuu->net_info->host,       uuu->net_info->port,
             uuu->net_info->path,       uuu->net_info->args,
             uuu->net_info->req_method, uuu->w_len,
             uuu->o_timeout,            uuu->w_timeout,
             uuu->net_info->http_user_header,
             (int/*bool*/) (uuu->flags & fHCC_UrlEncodeArgs),
             uuu->net_info->debug_printout == eDebugPrintout_Data
             ? eOn : (uuu->net_info->debug_printout == eDebugPrintout_None
                      ? eOff : eDefault));
        if (reset_user_header) {
            ConnNetInfo_SetUserHeader(uuu->net_info, 0);
            uuu->net_info->http_user_header = http_user_header;
        }

        if (uuu->sock) {
            if (!(uuu->flags & fHCC_NoUpread))
                SOCK_SetReadOnWrite(uuu->sock, eOn);
            return eIO_Success;
        }

        /* connection failed, no socket was created */
        if (!s_Adjust(uuu, &null, read_mode))
            break;
    }

    return eIO_Closed;
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
        char*  null = 0;

        if (!uuu->sock) {
            if ((status = s_Connect(uuu, read_mode)) != eIO_Success)
                break;
            assert(uuu->sock);
            uuu->read_header = 1/*true*/;
            uuu->shut_down = 0/*false*/;
            uuu->expected = 0;
            uuu->received = 0;
        } else
            status = eIO_Success;

        if (uuu->w_len) {
            size_t off = BUF_Size(uuu->w_buf) - uuu->w_len;

            SOCK_SetTimeout(uuu->sock, eIO_Write, uuu->w_timeout);
            do {
                char   buf[4096];
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

        if (status == eIO_Timeout &&  uuu->w_timeout  &&
            !uuu->w_timeout->sec  &&  !uuu->w_timeout->usec) {
            break;
        }

        CORE_LOGF_X(6, eLOG_Error,
                    ("[HTTP]  Error writing body at offset %lu (%s)",
                     (unsigned long) (BUF_Size(uuu->w_buf) - uuu->w_len),
                     IO_StatusStr(status)));

        /* write failed; close and try to use another server */
        SOCK_Abort(uuu->sock);
        s_DropConnection(uuu, 0/*no wait*/);
        if (!s_Adjust(uuu, &null, read_mode)) {
            uuu->can_connect = eCC_None;
            status = eIO_Closed;
            break;
        }
    }

    return status;
}


/* Parse HTTP header */
static EIO_Status s_ReadHeader(SHttpConnector* uuu,
                               char**          redirect,
                               EReadMode       read_mode)
{
    EIO_Status  status = eIO_Success;
    int/*bool*/ moved = 0/*false*/;
    int         server_error = 0;
    int         http_status = 0;
    char*       header;
    size_t      size;

    assert(uuu->sock && uuu->read_header);
    *redirect = 0;

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
                else if (tmo->sec | tmo->usec)
                    level = eLOG_Warning;
                else if (read_mode != eRM_WaitCalled)
                    level = eLOG_Trace;
                else
                    return status;
            } else
                level = eLOG_Error;
            CORE_LOGF_X(8, level, ("[HTTP]  Error reading header (%s)",
                                   IO_StatusStr(status)));
            return status;
        }
    }
    assert(header  &&  status == eIO_Success);
    /* the entire header has been read */
    uuu->read_header = 0/*false*/;

    if (BUF_Read(uuu->http, 0, size) != size) {
        CORE_LOG_X(9, eLOG_Error, "[HTTP]  Cannot discard HTTP header buffer");
        status = eIO_Unknown;
        assert(0);
    }

    /* HTTP status must come on the first line of the reply */
    if (sscanf(header, " HTTP/%*d.%*d %d ", &http_status) != 1)
        http_status = -1;
    if (http_status < 200  ||  299 < http_status) {
        server_error = http_status;
        if (http_status == 301  ||  http_status == 302)
            moved = 1;
        else if (http_status < 0  ||  http_status == 403 || http_status == 404)
            uuu->net_info->max_try = 0;
    }

    if ((server_error  ||  !uuu->error_header)
        &&  uuu->net_info->debug_printout == eDebugPrintout_Some) {
        /* HTTP header gets printed as part of data logging when
           uuu->net_info->debug_printout == eDebugPrintout_Data. */
        const char* header_header;
        if (!server_error)
            header_header = "HTTP header";
        else if (uuu->flags & fHCC_KeepHeader)
            header_header = "HTTP header (error)";
        else if (moved)
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
                            s_MessageIssued = 1;
                            s_MessageHook(message);
                        }
                    } else {
                        s_MessageIssued = -1;
                        CORE_LOGF_X(10, eLOG_Warning,
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

    if (moved) {
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
                assert(s);
                do {
                    if (!isspace((unsigned char) s[-1]))
                        break;
                } while (--s > location);
                if (s != location) {
                    size_t len = (size_t)(s - location);
                    memmove(header, location, len);
                    header[len] = '\0';
                    *redirect = header;
                }
                break;
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
    if (!*redirect)
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
    char* redirect = 0;
    EIO_Status status;

    for (;;) {
        status = s_ConnectAndSend(uuu, read_mode);
        if (!uuu->sock) {
            assert(status != eIO_Success);
            break;
        }
        if (status != eIO_Success) {
            if (status != eIO_Timeout ||
                status == SOCK_Status(uuu->sock, eIO_Read)/*pending*/)
                break;
        }

        /* set timeout */
        SOCK_SetTimeout(uuu->sock, eIO_Read, timeout);

        if (!uuu->read_header)
            break;

        if ((status = s_ReadHeader(uuu, &redirect, read_mode)) == eIO_Success){
            size_t w_size = BUF_Size(uuu->w_buf);
            assert(!uuu->read_header);
            /* pending output data no longer needed */
            if (BUF_Read(uuu->w_buf, 0, w_size) != w_size) {
                CORE_LOG_X(15, eLOG_Error,
                           "[HTTP]  Cannot discard output buffer");
                assert(0);
            }
            break;
        }
        /* if polling then bail out with eIO_Timeout */
        if(status == eIO_Timeout && timeout && !(timeout->sec | timeout->usec))
            break;

        /* HTTP header read error; disconnect and try to use another server */
        SOCK_Abort(uuu->sock);
        s_DropConnection(uuu, 0/*no wait*/);
        if (!s_Adjust(uuu, &redirect, read_mode)) {
            uuu->can_connect = eCC_None;
            break;
        }
        assert(redirect == 0);
    }
    assert(redirect == 0);
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

    if (read_mode == eRM_DropUnread) {
        size_t r_size = BUF_Size(uuu->r_buf);
        if (r_size  &&  BUF_Read(uuu->r_buf, 0, r_size) != r_size) {
            CORE_LOG_X(17, eLOG_Error, "[HTTP]  Cannot drop input buffer");
            assert(0);
        }
    } else if ((status = s_PreRead(uuu, timeout, read_mode)) == eIO_Success) {
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
        uuu->can_connect = eCC_None;

    return status;
}


/* Send the accumulated output data(if any) to server, then close socket.
 * Regardless of the flush, clear both input and output buffer.
 * This function is only called to either re-open or close the connector.
 */
static void s_FlushAndDisconnect(SHttpConnector* uuu,
                                 int/*bool*/     close,
                                 const STimeout* timeout)
{
    size_t w_size;

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

    if (close  &&  uuu->can_connect != eCC_None  &&  !uuu->sock
        &&  ((uuu->flags & fHCC_SureFlush)  ||  BUF_Size(uuu->w_buf))) {
        /* "WRITE" mode and data (or just flag) pending */
        s_PreRead(uuu, timeout, eRM_DropUnread);
    }
    s_Disconnect(uuu, timeout, eRM_DropUnread);
    assert(!uuu->sock);

    /* clear pending output data, if any */
    w_size = BUF_Size(uuu->w_buf);
    if (w_size  &&  BUF_Read(uuu->w_buf, 0, w_size) != w_size) {
        CORE_LOG_X(18, eLOG_Error, "[HTTP]  Cannot drop output buffer");
        assert(0);
    }
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
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(CONNECTOR     connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
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
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    size_t len = 7/*"http://"*/ + strlen(uuu->net_info->host) +
        (uuu->net_info->port == 80 ? 0 : 6/*:port*/) +
        strlen(uuu->net_info->path) +
        (*uuu->net_info->args ? 2 + strlen(uuu->net_info->args) : 1);
    char* buf = (char*) malloc(len);
    if (buf) {
        len = sprintf(buf, "http://%s", uuu->net_info->host);
        if (uuu->net_info->port != 80)
            len += sprintf(&buf[len], ":%hu", uuu->net_info->port);
        sprintf(&buf[len], "%s%s%s", uuu->net_info->path,
                *uuu->net_info->args ? "&" : "", uuu->net_info->args);
    }
    return buf;
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* NOTE: the real connect will be performed on the first "READ", or
     * "CLOSE", or on "WAIT" on read -- see in "s_ConnectAndSend()";
     * we just close underlying socket and prepare to open it later */
    s_FlushAndDisconnect(uuu, 0/*open*/, timeout);

    /* reset the auto-reconnect feature */
    uuu->can_connect = uuu->flags & fHCC_AutoReconnect
        ? eCC_Unlimited : eCC_Once;
    uuu->failure_count = 0;

    return eIO_Success;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    switch (event) {
    case eIO_Read:
        if (uuu->can_connect == eCC_None)
            return eIO_Closed;
        if (!uuu->sock  ||  uuu->read_header) {
            EIO_Status status = s_PreRead(uuu, timeout, eRM_WaitCalled);
            if (status != eIO_Success  ||  BUF_Size(uuu->r_buf))
                return status;
            assert(uuu->sock);
        }
        return SOCK_Wait(uuu->sock, eIO_Read, timeout);
    case eIO_Write:
        /* Return 'Closed' if no more writes are allowed (and now - reading) */
        return uuu->can_connect == eCC_None
            ||  (uuu->sock  &&  uuu->can_connect == eCC_Once)
            ? eIO_Closed : eIO_Success;
    default:
        assert(0);
        return eIO_InvalidArg;
    }
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
    if (uuu->flags & fHCC_UrlEncodeOutput) {
        /* with URL-encoding */
        size_t dst_size = 3 * size;
        void* dst = malloc(dst_size);
        size_t dst_written;
        URL_Encode(buf, size, n_written, dst, dst_size, &dst_written);
        assert(*n_written == size);
        if (!BUF_Write(&uuu->w_buf, dst, dst_written)) {
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
    s_FlushAndDisconnect((SHttpConnector*) connector->handle,
                         1/*close*/, timeout);
    return eIO_Success;
}


#ifdef IMPLEMENTED__CONN_WaitAsync
static EIO_Status s_VT_WaitAsync
(void*                   connector,
 FConnectorAsyncHandler  func,
 SConnectorAsyncHandler* data)
{
    return eIO_NotSupported;
}
#endif


static void s_Setup(SMetaConnector *meta, CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, descr,      s_VT_Descr,     connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      s_VT_Flush,     connector);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    CONN_SET_DEFAULT_TIMEOUT(meta, uuu->net_info->timeout);
}


static void s_Destroy(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    if (uuu->adjust_cleanup)
        uuu->adjust_cleanup(uuu->adjust_data);
    ConnNetInfo_Destroy(uuu->net_info);
    BUF_Destroy(uuu->http);
    BUF_Destroy(uuu->r_buf);
    BUF_Destroy(uuu->w_buf);
    free(uuu);
    connector->handle = 0;
    free(connector);
}


/* NB: HTTP tag name misspelled as "Referer" as per the standard. */
static void s_AddReferer(SConnNetInfo* net_info)
{
    const char* s;
    char* referer;
    if (!net_info  ||  !net_info->http_referer  ||  !*net_info->http_referer)
        return;
    if ((s = net_info->http_user_header) != 0) {
        int/*bool*/ first;
        for (first = 1;  *s;  first = 0) {
            if (strncasecmp(s++, "\nReferer: " + first, 10 - first) == 0)
                return;
            if (!(s = strchr(s, '\n')))
                break;
        }
    }
    if (!(referer = (char*) malloc(strlen(net_info->http_referer) + 12)))
        return;
    sprintf(referer, "Referer: %s\r\n", net_info->http_referer);
    ConnNetInfo_ExtendUserHeader(net_info, referer);
    free(referer);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/

extern CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* info,
 const char*         user_header,
 THCC_Flags          flags)
{
    SConnNetInfo *net_info;
    CONNECTOR connector;

    net_info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(0);
    if (user_header)
        ConnNetInfo_OverrideUserHeader(net_info, user_header);
    connector = HTTP_CreateConnectorEx(net_info, flags, 0, 0, 0, 0);
    ConnNetInfo_Destroy(net_info);
    return connector;
}


extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo*  net_info,
 THCC_Flags           flags,
 FHttpParseHTTPHeader parse_http_hdr,
 FHttpAdjustNetInfo   adjust_net_info,
 void*                adjust_data,
 FHttpAdjustCleanup   adjust_cleanup)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector    ));
    SHttpConnector* uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector));

    /* initialize internal data structure */
    uuu->net_info        = net_info ?
        ConnNetInfo_Clone(net_info) : ConnNetInfo_Create(0);
    ConnNetInfo_AdjustForHttpProxy(uuu->net_info);
    s_AddReferer(uuu->net_info);

    uuu->parse_http_hdr  = parse_http_hdr;
    uuu->adjust_net_info = adjust_net_info;
    uuu->adjust_cleanup  = adjust_cleanup;
    uuu->adjust_data     = adjust_data;

    uuu->flags           = flags;
    uuu->can_connect     = eCC_Once;         /* will be properly set at open */
    uuu->error_header    = getenv("HTTP_ERROR_HEADER_ONLY") ? 1 : 0;

    uuu->sock            = 0;
    uuu->o_timeout       = kDefaultTimeout;  /* deliberately bad values --   */
    uuu->w_timeout       = kDefaultTimeout;  /* will be reset prior to use   */
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


extern void HTTP_SetNcbiMessageHook(FHTTP_NcbiMessageHook hook)
{
    if (hook) {
        if (hook != s_MessageHook)
            s_MessageIssued = s_MessageIssued ? -1 : -2;
    } else if (s_MessageIssued < -1)
        s_MessageIssued = 0;
    s_MessageHook = hook;
}
