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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for the HTTP-based network connection
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.12  2001/04/26 20:21:34  lavr
 * Reorganized and and made slightly more effective
 *
 * Revision 6.11  2001/04/24 21:41:47  lavr
 * Reorganized code to allow reconnects in case of broken connections at
 * stage of connection, sending data and receiving HTTP header. Added
 * code to pull incoming data from connection while sending - stall protection.
 *
 * Revision 6.10  2001/03/02 20:08:47  lavr
 * Typo fixed
 *
 * Revision 6.9  2001/01/25 16:53:24  lavr
 * New flag for HTTP_CreateConnectorEx: fHCC_DropUnread
 *
 * Revision 6.8  2001/01/23 23:11:20  lavr
 * Status virtual method implemented
 *
 * Revision 6.7  2001/01/11 16:38:17  lavr
 * free(connector) removed from s_Destroy function
 * (now always called from outside, in METACONN_Remove)
 *
 * Revision 6.6  2001/01/03 22:32:43  lavr
 * Redundant calls to 'adjust_info' removed
 *
 * Revision 6.5  2000/12/29 17:57:16  lavr
 * Adapted for use of new connector structure;
 * parse header callback added; some internal functions renamed.
 *
 * Revision 6.4  2000/11/15 18:52:02  vakatov
 * Call SOCK_Shutdown() after the HTTP request is sent.
 * Use SOCK_Status() instead of SOCK_Eof().
 *
 * Revision 6.3  2000/10/12 21:43:14  vakatov
 * Minor cosmetic fix...
 *
 * Revision 6.2  2000/09/26 22:02:57  lavr
 * HTTP request method added
 *
 * Revision 6.1  2000/04/21 19:41:01  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_buffer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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


/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    SConnNetInfo*      info;        /* generic connection configuration     */
    THCC_Flags         flags;       /* as passed to HTTP_CreateConnector()  */
    FHttpParseHTTPHdr  parse_http_hdr; /* callback to parse HTTP reply hdr  */

    /* for on-the-fly adjustment of the connection info */
    FHttpAdjustInfo    adjust_info;
    void*              adjust_data;
    FHttpAdjustCleanup adjust_cleanup;

    SOCK            sock;        /* socket;  NULL if not in the "READ" mode  */
    BUF             obuf;        /* storage to accumulate output data        */
    BUF             ibuf;        /* storage to accumulate input data         */
    const STimeout* c_timeout;   /* NULL(infinite), default or ptr to next   */
    STimeout        cc_timeout;  /* storage for a (finite) connect timeout   */
    const STimeout* w_timeout;   /* NULL(infinite), default or ptr to next   */
    STimeout        ww_timeout;  /* storage for a (finite) write timeout     */
    ECanConnect     can_connect; /* if still permitted to make a connection  */
    int/*bool*/     first_read;  /* if the 1st attempt to read after connect */
    unsigned int    fail_count;  /* incremented each failure since open      */
} SHttpConnector;


static int/*bool*/ s_Adjust(SHttpConnector* uuu)
{
    /* we're here becuase something is going wrong */
    uuu->fail_count++;
    /* adjust info before another connection attempt */
    if (!uuu->adjust_info ||
        !(*uuu->adjust_info)(uuu->info, uuu->adjust_data, uuu->fail_count))
        return 0/*false*/;
    ConnNetInfo_AdjustForHttpProxy(uuu->info);
    
    if (uuu->info->debug_printout)
        ConnNetInfo_Print(uuu->info, stderr);
    return 1/*true*/;
}


static EIO_Status s_Read(SHttpConnector* uuu, void* buf,
                         size_t size, size_t* n_read)
{
    assert(uuu->sock);
    
    /* just read, with no URL-decoding */
    if (!(uuu->flags & fHCC_UrlDecodeInput)) {
        return SOCK_Read(uuu->sock, buf, size, n_read, eIO_Plain);
    }
    
    /* read and URL-decode */
    {{
        EIO_Status status;
        size_t     n_peeked, n_decoded;
        size_t     peek_size = 3 * size;
        void*      peek_buf  = malloc(peek_size);
        
        /* peek the data */
        status= SOCK_Read(uuu->sock, peek_buf, peek_size, &n_peeked, eIO_Peek);
        if (status != eIO_Success) {
            *n_read = 0;
            free(peek_buf);
            return status;
        }
        
        /* decode, then discard the successfully decoded data from the input */
        if (URL_Decode(peek_buf, n_peeked, &n_decoded, buf, size, n_read)) {
            if (n_decoded) {
                size_t x_read;
                SOCK_Read(uuu->sock, peek_buf, n_decoded, &x_read, eIO_Plain);
                assert(x_read == n_decoded);
                status = eIO_Success;
            } else if (SOCK_Status(uuu->sock, eIO_Read) == eIO_Closed) {
                /* we are at EOF, and the remaining data cannot be decoded */
                status = eIO_Unknown;
            } 
        } else
            status = eIO_Unknown;
        
        if (status != eIO_Success)
            CORE_LOG(eLOG_Error, "[HTTP_SockRead]  Cannot URL-decode data");
        
        free(peek_buf);
        return status;
    }}
}


/* Reset/upread input data and close socket.
 */
static EIO_Status s_Disconnect(SHttpConnector* uuu, int/*bool*/ drop_unread)
{
    EIO_Status status = eIO_Success;
    assert(uuu->sock && !uuu->first_read);

    if (!drop_unread) {
        char   buf[4096];
        size_t x_read;
        
        do {
            status = s_Read(uuu, buf, sizeof(buf), &x_read);
            if (!BUF_Write(&uuu->ibuf, buf, x_read))
                status = eIO_Unknown;
        } while (status == eIO_Success);
        if (status == eIO_Closed)
            status = eIO_Success;
    } else
        BUF_Read(uuu->ibuf, 0, BUF_Size(uuu->ibuf));
    SOCK_Close(uuu->sock);
    uuu->sock = 0;
    if (uuu->can_connect == eCC_Once)
        uuu->can_connect = eCC_None;
    return status;
}


/* Connect to the HTTP server, specified by "uuu->info" "port:host".
 * If unsuccessful, try to adjust the "uuu->info" with "uuu->adjust_info()"
 * and then re-try the connection attempt.
 */
static EIO_Status s_Connect(SHttpConnector* uuu)
{
    assert(!uuu->sock);
    if (uuu->can_connect == eCC_None)
        return eIO_Closed;

    /* the re-try loop... */
    while (uuu->fail_count < uuu->info->max_try) {
        /* connect & send HTTP header */
        uuu->sock = URL_Connect
            (uuu->info->host, uuu->info->port, uuu->info->path,
             uuu->info->args, uuu->info->req_method,
             BUF_Size(uuu->obuf),
             uuu->c_timeout == CONN_DEFAULT_TIMEOUT
             ? uuu->info->timeout : uuu->c_timeout,
             uuu->w_timeout == CONN_DEFAULT_TIMEOUT
             ? uuu->info->timeout : uuu->w_timeout,
             uuu->info->http_user_header,
             (int/*bool*/) (uuu->flags & fHCC_UrlEncodeArgs),
             uuu->info->debug_printout == eDebugPrintout_Data ? eOn :
             uuu->info->debug_printout == eDebugPrintout_None ? eOff :
             eDefault);

        if (uuu->sock)
            return eIO_Success;

        if (!s_Adjust(uuu))
            break;
    }

    return eIO_Unknown;
}


/* Connect to the server specified by "uuu->info", then compose and form
 * relevant HTTP header, and flush the accumulated output data("uuu->obuf")
 * after the HTTP header. On error, all accumulated output data will be lost,
 * and the connector socket will be NULL.
 */
static EIO_Status s_ConnectAndSend(SHttpConnector* uuu)
{
    EIO_Status status;
    
    for (;;) {
        size_t size;

        if ((status = s_Connect(uuu)) != eIO_Success) {
            /* error: discard all pending output data */
            BUF_Read(uuu->obuf, 0, BUF_Size(uuu->obuf));
            break;
        }
        assert(uuu->sock);
        uuu->first_read = 1/*true*/;

        if ((size = BUF_Size(uuu->obuf)) != 0) {
            /* flush the accumulated output data */
            size_t off = 0;

            SOCK_SetTimeout(uuu->sock, eIO_Write,
                            uuu->w_timeout == CONN_DEFAULT_TIMEOUT 
                            ? uuu->info->timeout : uuu->w_timeout);
            do {
                char       buf[4096];
                size_t     n_written;
                size_t     size = BUF_PeekAt(uuu->obuf, off, buf, sizeof(buf));
                STimeout   poll_tmo = {0, 0};
                
                status = SOCK_Write(uuu->sock, buf, size, &n_written);
                if (status != eIO_Success)
                    break;
                off += n_written;
                if (SOCK_Wait(uuu->sock, eIO_Read, &poll_tmo) == eIO_Success) {
                    size_t n;
                    /* Pull incoming data from socket - stall protection */
                    SOCK_SetTimeout(uuu->sock, eIO_Read, &poll_tmo);
                    SOCK_Read(uuu->sock, 0, 2000000000/*!*/, &n, eIO_Peek);
                }
            } while (off < size);
        }
        /* shutdown the socket for writing */
#if !defined(NCBI_OS_MSWIN)
        /* (on MS-Win, socket shutdown on writing apparently messes up (?!)
         *  with the later reading, especially when reading a lot of data.) */
        SOCK_Shutdown(uuu->sock, eIO_Write);
#endif
        if (status == eIO_Success)
            break;
        
        /* write failed, close socket and try to use another server */
        SOCK_Close(uuu->sock);
        uuu->sock = 0;
        if (!s_Adjust(uuu))
            break;
    }

    return status;
}


/* Parse HTTP header */
static EIO_Status s_ReadHeader(SHttpConnector* uuu)
{
    int/*bool*/ server_error = 0/*false*/;
    EIO_Status  status;
    BUF         buf = 0;

    if (uuu->flags & fHCC_KeepHeader)
        return eIO_Success;
    
    /* check status (assume the reply status is in the first line) */
    status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &buf, 0);
    if (status == eIO_Success) {
        char str[64];
        int http_v1, http_v2, http_status = 0;
        size_t n_peek = BUF_Peek(buf, str, sizeof(str)-1);
        assert(2 <= n_peek  &&  n_peek < sizeof(str));
        str[n_peek] = '\0';
        if (sscanf(str, " HTTP/%d.%d %d ",
                   &http_v1, &http_v2, &http_status) != 3  ||
            http_status < 200  ||  299 < http_status) {
            server_error = 1/*true*/;
        }
        
        /* skip HTTP header */
        if (uuu->info->debug_printout || uuu->parse_http_hdr) {
            char data[256], *header;
            size_t hdrsize, n_read;
            
            /* skip & printout the HTTP header */
            status = SOCK_StripToPattern(uuu->sock, "\r\n\r\n", 4,
                                         &buf, 0);
            hdrsize = BUF_Size(buf);
            if ((header = (char*) malloc(hdrsize + 1)) != 0) {
                if (BUF_Read(buf, header, hdrsize) != hdrsize) {
                    free(header);
                    header = 0;
                } else
                    header[hdrsize] = '\0';
            }
            
            if (uuu->info->debug_printout) {
                fprintf(stderr, "\n\
----- [BEGIN] HTTP Header(%ld bytes followed by \\n%s) -----\n",
                        (long) hdrsize,
                        header ? "" : " {allocation failed}");
                if (!header) {
                    while ((n_read = BUF_Read(buf, data, sizeof(data))) > 0)
                        fwrite(data, 1, n_read, stderr);
                } else
                    fwrite(header, 1, hdrsize, stderr);
                fprintf(stderr, "\
----- [END] HTTP Header -----\n\n");
                fflush(stderr);
            }
            
            if (uuu->parse_http_hdr) {
                if (!(*uuu->parse_http_hdr)
                    (header, uuu->adjust_data, server_error))
                    server_error = 1;
            }
            
            if (header)
                free(header);
            
            /* skip & printout the content, if server err detected */
            if (server_error && uuu->info->debug_printout) {
                fprintf(stderr, "\n\
----- [BEGIN] Detected a server error -----\n");
                for (;;) {
                    EIO_Status status =
                        SOCK_Read(uuu->sock, data, sizeof(data),
                                  &n_read, eIO_Plain);
                    if (status != eIO_Success)
                        break;
                    fwrite(data, 1, n_read, stderr);
                }
                fprintf(stderr, "\n\
----- [END] Detected a server error -----\n\n");
                fflush(stderr);
            }
        } else if (!server_error) {
            /* skip HTTP header */
            status = SOCK_StripToPattern(uuu->sock, "\r\n\r\n", 4, 0, 0);
        }
    }
    
    BUF_Destroy(buf);
    if (server_error)
        return eIO_Unknown;
    return status;
}


/* Send the accumulated output data(if any) to server, then close socket.
 * Regardless of the flush, clear both input and output buffer.
 * This function is only called to either re-open or close connector.
 */
static void s_FlushAndDisconnect(SHttpConnector* uuu)
{
    if (uuu->sock) {
        /* In "READ" mode no pending output data must be in the buffer */
        assert(BUF_Size(uuu->obuf) == 0);
        s_Disconnect(uuu, 1/*drop_unread*/);
    } else if ((uuu->flags & fHCC_SureFlush) || BUF_Size(uuu->obuf)) {
        /* "WRITE" mode and data (or just flag) pending */
        if (uuu->can_connect != eCC_None) {
            for (;;) {
                if (s_ConnectAndSend(uuu) != eIO_Success) {
                    if (!s_Adjust(uuu))
                        break;    
                } else if (s_ReadHeader(uuu) == eIO_Success) {
                    s_Disconnect(uuu, 1/*drop_unread*/);
                    break;
                } else {
                    SOCK_Close(uuu->sock);
                    uuu->sock = 0;
                    if (!s_Adjust(uuu))
                        break;
                }
            }
        }
        /* Discard all data */
        BUF_Read(uuu->ibuf, 0, BUF_Size(uuu->ibuf));
        BUF_Read(uuu->obuf, 0, BUF_Size(uuu->obuf));
    }
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType(CONNECTOR       connector);
    static EIO_Status  s_VT_Open   (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Wait   (CONNECTOR       connector,
                                    EIO_Event       event,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Write  (CONNECTOR       connector,
                                    const void*     buf,
                                    size_t          size,
                                    size_t*         n_written,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Flush  (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Read   (CONNECTOR       connector,
                                    void*           buf,
                                    size_t          size,
                                    size_t*         n_read,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Status (CONNECTOR       connector,
                                    EIO_Event       dir);
    static EIO_Status  s_VT_Close  (CONNECTOR       connector,
                                    const STimeout* timeout);

    static void        s_Setup     (SMetaConnector *meta,
                                    CONNECTOR connector);
    static void        s_Destroy   (CONNECTOR connector);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status  s_VT_WaitAsync(CONNECTOR     connector,
                                      FConnectorAsyncHandler  func,
                                      SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "HTTP";
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* flush, then drop all data and close current socket (if any) */
    s_FlushAndDisconnect(uuu);
    assert(!uuu->sock);

    /* reset the auto-reconnect feature */
    uuu->can_connect = uuu->flags & fHCC_AutoReconnect
        ? eCC_Unlimited : eCC_Once;
    uuu->fail_count = 0;
    
    /* NOTE: the real connect will be performed on the first "READ", or
     *"CLOSE", or on the "WAIT" on read -- see in "s_ConnectAndSend()";
     * here we just store the timeout value -- for the future connect */
    if (timeout && timeout != CONN_DEFAULT_TIMEOUT) {
        uuu->cc_timeout = *timeout;
        uuu->c_timeout  = &uuu->cc_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->c_timeout  = timeout;
        uuu->w_timeout  = timeout;
    }

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
        if (!uuu->sock) {
            EIO_Status status = s_ConnectAndSend(uuu);
            if (status != eIO_Success)
                return status;
        }
        return SOCK_Wait(uuu->sock, eIO_Read, timeout == CONN_DEFAULT_TIMEOUT
                         ? uuu->info->timeout : timeout);
    case eIO_Write:
        /* Return 'Closed' if no more writes are allowed (and now - reading) */
        return uuu->can_connect == eCC_None ||
            (uuu->sock != 0 && uuu->can_connect == eCC_Once)
            ? eIO_Closed : eIO_Success;
    default:
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
     * and thus switch to "WRITE" mode */
    if (uuu->sock) {
        EIO_Status status = s_Disconnect(uuu, uuu->flags & fHCC_DropUnread);
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
        if (!BUF_Write(&uuu->obuf, dst, dst_written)) {
            free(dst);
            return eIO_Unknown;
        }
        free(dst);
    }
    else {
        /* "as is" (without URL-encoding) */
        if (!BUF_Write(&uuu->obuf, buf, size))
            return eIO_Unknown;
        *n_written = size;
    }

    /* store the write timeout */
    if (timeout && timeout != CONN_DEFAULT_TIMEOUT) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->w_timeout  = timeout;
    }

    return eIO_Success;
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* The real flush will be performed on the first "READ" (or "CLOSE"),
     * or on "WAIT". We just store write timeout here, that's all...
     */
    if (timeout && timeout != CONN_DEFAULT_TIMEOUT) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->w_timeout  = timeout;
    }

    return eIO_Success;
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;
    size_t x_read;
    
    for (;;) {
        if (!uuu->sock) {
            /* not in "READ" mode yet... so "CONNECT" and "WRITE" first */
            if ((status = s_ConnectAndSend(uuu)) != eIO_Success)
                return status;
            assert(uuu->sock);
        }

        /* set timeout */
        SOCK_SetTimeout(uuu->sock, eIO_Read,
                        timeout == CONN_DEFAULT_TIMEOUT
                        ? uuu->info->timeout : timeout);

        if (!uuu->first_read)
            break;
        /* first read:  parse and skip HTTP header */
        uuu->first_read = 0;
        if ((status = s_ReadHeader(uuu)) == eIO_Success) {
            /* reply was okay - discard output data */
            BUF_Read(uuu->obuf, 0, BUF_Size(uuu->obuf));
            break;
        }

        SOCK_Close(uuu->sock);
        uuu->sock = 0;
        if (!s_Adjust(uuu))
            return status;
    }

    x_read = BUF_Read(uuu->ibuf, buf, size);
    if (x_read < size) {
        status = s_Read(uuu, (char*) buf + x_read, size - x_read, n_read);
        *n_read += x_read;
    } else {
        status = eIO_Success;
        *n_read = x_read;
    }

    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    return uuu->sock ? SOCK_Status(uuu->sock, dir) : eIO_Success;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
#if 0
    if (timeout && timeout != CONN_DEFAULT_TIMEOUT) {
        uuu->cc_timeout = *timeout;
        uuu->c_timeout  = &uuu->cc_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->c_timeout  = timeout;
        uuu->w_timeout  = timeout;
    }
#endif
    s_FlushAndDisconnect((SHttpConnector*) connector->handle);
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
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
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
}


static void s_Destroy(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    ConnNetInfo_Destroy(uuu->info);
    if (uuu->adjust_cleanup)
        (*uuu->adjust_cleanup)(uuu->adjust_data);
    BUF_Destroy(uuu->ibuf);
    BUF_Destroy(uuu->obuf);
    free(uuu);
    free(connector);
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
        ConnNetInfo_SetUserHeader(net_info, user_header);
    connector = HTTP_CreateConnectorEx(net_info, flags, 0, 0, 0, 0);
    ConnNetInfo_Destroy(net_info);
    return connector;
}


extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* info,
 THCC_Flags          flags,
 FHttpParseHTTPHdr   parse_http_hdr,
 FHttpAdjustInfo     adjust_info,
 void*               adjust_data,
 FHttpAdjustCleanup  adjust_cleanup)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector    ));
    SHttpConnector* uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector));

    /* initialize internal data structures */
    uuu->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(0);
    ConnNetInfo_AdjustForHttpProxy(uuu->info);
    if (uuu->info->debug_printout) {
        ConnNetInfo_Print(uuu->info, stderr);
    }

    uuu->flags           = flags;
    if (flags & fHCC_UrlDecodeInput)
        uuu->flags      &= ~fHCC_KeepHeader;

    uuu->parse_http_hdr  = parse_http_hdr;
    
    uuu->adjust_info     = adjust_info;
    uuu->adjust_data     = adjust_data;
    uuu->adjust_cleanup  = adjust_cleanup;
    
    uuu->sock = 0;
    uuu->obuf = 0;
    uuu->ibuf = 0;
    uuu->c_timeout = CONN_DEFAULT_TIMEOUT;
    uuu->w_timeout = CONN_DEFAULT_TIMEOUT;

    /* initialize connector structure */
    ccc->handle  = uuu;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}
