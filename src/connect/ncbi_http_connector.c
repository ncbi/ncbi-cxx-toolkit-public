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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for the HTTP-based network connection
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2000/09/26 22:02:57  lavr
 * HTTP request method added
 *
 * Revision 6.1  2000/04/21 19:41:01  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

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
    SConnNetInfo* info;        /* generic connection configuration */
    char*         user_header; /* user-defined part of the HTTP header */
    THCC_Flags    flags;       /* as passed to HTTP_CreateConnector() */

    /* for on-the-fly adjustment of the connection info */
    FHttpAdjustInfo    adjust_info;
    void*              adjust_data;
    FHttpAdjustCleanup adjust_cleanup;

    SOCK            sock;        /* socket;  NULL if not in the "READ" mode */
    BUF             buf;         /* storage to accumulate output data */
    const STimeout* c_timeout;   /* NULL(infinite) or points to "cc_timeout" */
    STimeout        cc_timeout;  /* storage for a (finite) connect timeout */
    const STimeout* w_timeout;   /* NULL(infinite) or points to "ww_timeout" */
    STimeout        ww_timeout;  /* storage for a (finite) write timeout */
    ECanConnect     can_connect; /* if still permitted to make a connection */
    int/*bool*/     first_read;  /* if the 1st attempt to read after connect */
} SHttpConnector;


/* Reset the accumulated output data and close socket
 */
static void s_Close(SHttpConnector* uuu)
{
    assert(uuu->sock);
    BUF_Read(uuu->buf, 0, BUF_Size(uuu->buf));
    SOCK_Close(uuu->sock);
    uuu->sock = 0;
}


/* Flush the connector's accumulated output data.
 * NOTE:  on both success and any error, all internally stored accumulated
 *        output data will be discarded(BUF_Size(uuu->buf) --> zero)!
 */
static EIO_Status s_FlushData(SHttpConnector* uuu)
{
    assert(uuu->sock);
    if ( !BUF_Size(uuu->buf) )
        return eIO_Success;

    SOCK_SetTimeout(uuu->sock, eIO_Write, uuu->w_timeout);
    do {
        char       buf[4096];
        size_t     n_written;
        size_t     size = BUF_Peek(uuu->buf, buf, sizeof(buf));
        EIO_Status status = SOCK_Write(uuu->sock, buf, size, &n_written);

        /* on any error, just discard all data and fail */
        if (status != eIO_Success) {
            BUF_Read(uuu->buf, 0, BUF_Size(uuu->buf));
            return status;
        }
    
        /* on success, discard the succesfully written data and continue */
        BUF_Read(uuu->buf, 0, n_written);
    } while ( BUF_Size(uuu->buf) );

    return eIO_Success;
}


/* Adjust the "uuu->info" with "uuu->adjust_info()";
 * connect to the "port:host" specified in "uuu->info", then
 * compose and form relevant HTTP header and
 * flush the accumulated output data("uuu->buf") after the HTTP header.
 * On error, all accumulated output data will be lost, and the connector
 * socket will be NULL.
 */
static EIO_Status s_ConnectAndSend(SHttpConnector* uuu)
{
    EIO_Status status;
    unsigned int i;
    unsigned int max_try = uuu->info->max_try ? uuu->info->max_try : 1;
    assert(!uuu->sock);

    /* the re-try loop... */
    for (i = 0;  i < max_try  &&  !uuu->sock;  i++) {
        /* adjust the connection info */
        if ( uuu->adjust_info ) {
            uuu->adjust_info(uuu->info, uuu->adjust_data, i);
            if ( uuu->info->debug_printout )
                ConnNetInfo_Print(uuu->info, stderr);
        }

        /* connect & send HTTP header */
        uuu->sock = URL_Connect
            (uuu->info->host, uuu->info->port, uuu->info->path,
             uuu->info->args, uuu->info->req_method,
             BUF_Size(uuu->buf), uuu->c_timeout, uuu->w_timeout,
             uuu->user_header,
             (int/*bool*/) (uuu->flags & fHCC_UrlEncodeArgs));
    }

    /* ? error */
    if ( !uuu->sock ) {
        BUF_Read(uuu->buf, 0, BUF_Size(uuu->buf));
        return eIO_Unknown;
    }

    /* flush the accumulated output data */
    if ((status = s_FlushData(uuu)) != eIO_Success) {
        if (status == eIO_Timeout)
            status = eIO_Closed;  /* fake it to avoid somebody to re-try */
        s_Close(uuu);
        return status;
    }

    uuu->first_read = 1/*true*/;
    return eIO_Success;
}


/* Send the accumulated output data(if any) to server, then close socket
 */
static void s_FlushAndClose(SHttpConnector* uuu)
{
    if ( uuu->sock ) {
        assert(!BUF_Size(uuu->buf));
        SOCK_Close(uuu->sock);
        uuu->sock = 0;
    } else if ((uuu->flags & fHCC_SureFlush)  ||  BUF_Size(uuu->buf)) {
        if (uuu->can_connect != eCC_None  &&
            s_ConnectAndSend(uuu) == eIO_Success) {
            s_Close(uuu);
        }
    }
}



/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/


#ifdef __cplusplus
extern "C" {
    static const char* s_VT_GetType(void* connector);
    static EIO_Status  s_VT_Connect(void*           connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Wait(void*           connector,
                                 EIO_Event       event,
                                 const STimeout* timeout);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status  s_VT_WaitAsync(void*                   connector,
                                      FConnectorAsyncHandler  func,
                                      SConnectorAsyncHandler* data);
#  endif
    static EIO_Status  s_VT_Write(void*           connector,
                                  const void*     buf,
                                  size_t          size,
                                  size_t*         n_written,
                                  const STimeout* timeout);
    static EIO_Status  s_VT_Flush(void*           connector,
                                  const STimeout* timeout);
    static EIO_Status  s_VT_Read(void*           connector,
                                 void*           buf,
                                 size_t          size,
                                 size_t*         n_read,
                                 const STimeout* timeout);
    static EIO_Status  s_VT_Close(CONNECTOR       connector,
                                  const STimeout* timeout);
} /* extern "C" */
#endif /* __cplusplus */



static const char* s_VT_GetType
(void* connector)
{
    return "HTTP";
}


static EIO_Status s_VT_Connect
(void*           connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector;

    /* flush the accumulated output data and close current socket */
    s_FlushAndClose(uuu);

    /* reset the auto-reconnect feature */
    uuu->can_connect = (uuu->flags & fHCC_AutoReconnect)
        ? eCC_Unlimited : eCC_Once;

    /* NOTE: the real connect will be performed on the first "READ", or
     *"CLOSE", or on the "WAIT" on read -- see in "s_ConnectAndSend()";
     * here we just store the timeout value -- for the future connect */
    if ( timeout ) {
        uuu->cc_timeout = *timeout;
        uuu->c_timeout  = &uuu->cc_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->c_timeout = 0;
        uuu->w_timeout = 0;
    }

    return eIO_Success;
}


static EIO_Status s_VT_Wait
(void*           connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector;
    if (uuu->can_connect == eCC_None)
        return eIO_Closed; /* no more i/o permitted */

    if (event == eIO_Read) {
        if ( !uuu->sock ) {
            EIO_Status status = s_ConnectAndSend(uuu);
            if (status != eIO_Success)
                return status;
        }
        return SOCK_Wait(uuu->sock, eIO_Read, timeout);
    }

    if (event == eIO_Write) {
        return (uuu->sock  &&  uuu->can_connect == eCC_Once) ?
            eIO_Closed : eIO_Success;
    }

    return eIO_InvalidArg;
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


static EIO_Status s_VT_Write
(void*           connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector;
    if (uuu->can_connect == eCC_None)
        return eIO_Closed; /* no more connects permitted */

    /* if trying to "WRITE" after "READ"... */
    if ( uuu->sock ) {
        if (uuu->can_connect == eCC_Once) {
            uuu->can_connect = eCC_None;
            return eIO_Closed; /* no more connects permitted */
        }
        /* close the socket, and thus switch to the "WRITE" mode */
        SOCK_Close(uuu->sock);
        uuu->sock = 0;
    }

    /* accumulate all output in the memory buffer */
    if (uuu->flags & fHCC_UrlEncodeOutput) {
        /* with URL-encoding */
        size_t dst_size = 3 * size;
        void* dst = malloc(dst_size);
        size_t dst_written;
        URL_Encode(buf, size, n_written, dst, dst_size, &dst_written);
        assert(*n_written == size);
        if ( !BUF_Write(&uuu->buf, dst, dst_written) ) {
            free(dst);
            return eIO_Unknown;
        }
        free(dst);
    }
    else {
        /* "as is" (without URL-encoding) */
        if ( !BUF_Write(&uuu->buf, buf, size) )
            return eIO_Unknown;
        *n_written = size;
    }

    /* store the write timeout */
    if ( timeout ) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->w_timeout = 0;
    }

    return eIO_Success;
}


static EIO_Status s_VT_Flush
(void*           connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector;

    /* The real flush will be performed on the first "READ"(or "CLOSE"),
     * or on the "WAIT".
     * We just store the write timeout here, that's all...
     */
    if ( timeout ) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->w_timeout = 0;
    }

    return eIO_Success;
}


static EIO_Status s_VT_Read
(void*           connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector;
    if (uuu->can_connect == eCC_None)
        return eIO_Closed; /* no more connects permitted */

    if ( !uuu->sock ) {
        /* not in the "READ" mode yet... so "CONNECT" and "WRITE" first */
        EIO_Status status = s_ConnectAndSend(uuu);
        if (status != eIO_Success)
            return status;
        assert(uuu->sock);
    }

    /* if it is a "fake" read -- just to connect and flush the output data */
    if ( !size )
        return eIO_Success;

    /* set timeout */
    SOCK_SetTimeout(uuu->sock, eIO_Read, timeout);

    /* first read::  skip HTTP header (and check reply status) */
    if ( uuu->first_read ) {
        uuu->first_read = 0/*false*/;
        if ( !(uuu->flags & fHCC_KeepHeader) ) {
            int/*bool*/ server_error = 0/*false*/;
            EIO_Status  status;
            BUF         buf = 0;

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
                    http_status < 200  ||  299 < http_status)
                    server_error = 1/*true*/;
            }

            /* skip HTTP header */
            if (status == eIO_Success) {
                if ( uuu->info->debug_printout ) {
                    char   data[256];
                    size_t n_read;

                    /* skip & printout the HTTP header */
                    status = SOCK_StripToPattern(uuu->sock, "\r\n\r\n", 4,
                                                 &buf, 0);
                    fprintf(stderr, "\
\n\n----- [BEGIN] HTTP Header(%ld bytes follow after \\n) -----\n",
                            (long) BUF_Size(buf));
                    while ((n_read = BUF_Read(buf, data, sizeof(data))) > 0)
                        fwrite(data, 1, n_read, stderr);
                    fprintf(stderr, "\
\n----- [END] HTTP Header -----\n\n");
                    fflush(stderr);

                    /* skip & printout the content, if server err detected */
                    if ( server_error ) {
                        fprintf(stderr, "\
\n\n----- [BEGIN] Detected a server error -----\n");
                        for (;;) {
                            EIO_Status status =
                                SOCK_Read(uuu->sock, data, sizeof(data),
                                          &n_read, eIO_Plain);
                            if (status != eIO_Success)
                                break;
                            fwrite(data, 1, n_read, stderr);
                        }
                        fprintf(stderr, "\
\n----- [END] Detected a server error -----\n\n");
                        fflush(stderr);
                    }
                } else if ( !server_error ) {
                    /* skip HTTP header */
                    status = SOCK_StripToPattern(uuu->sock, "\r\n\r\n",4, 0,0);
                }
            }

            buf = BUF_Destroy(buf);
            if ( server_error )
                return eIO_Unknown;
            if (status != eIO_Success)
                return status;
        }
    }

    /* just read, with no URL-decoding */
    if ( !(uuu->flags & fHCC_UrlDecodeInput) ) {
        return SOCK_Read(uuu->sock, buf, size, n_read, eIO_Plain);
    }

    /* read and URL-decode */
    {{
        EIO_Status status;
        size_t     n_peeked, n_decoded;
        size_t     peek_size = 3 * size;
        void*      peek_buf = malloc(peek_size);

        /* peek the data */
        status= SOCK_Read(uuu->sock, peek_buf, peek_size, &n_peeked, eIO_Peek);
        if (status != eIO_Success) {
            free(peek_buf);
            return status;
        }

        /* decode, then discard the successfully decoded data from the input */
        if ( URL_Decode(peek_buf, n_peeked, &n_decoded, buf, size, n_read) ) {
            if ( n_decoded ) {
                size_t x_read;
                SOCK_Read(uuu->sock, peek_buf, n_decoded, &x_read, eIO_Plain);
                assert(x_read == n_decoded);
                status = eIO_Success;
            } else if ( SOCK_Eof(uuu->sock) ) {
                /* we are at EOF, and the remaining data cannot be decoded */
                status = eIO_Unknown;
            } 
        } else {
            status = eIO_Unknown;
        }

        free(peek_buf);
        return status;
    }}
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    s_FlushAndClose(uuu);
    ConnNetInfo_Destroy(&uuu->info);
    if ( uuu->user_header )
        free(uuu->user_header);
    if ( uuu->adjust_cleanup )
        uuu->adjust_cleanup(uuu->adjust_data);
    BUF_Destroy(uuu->buf);
    free(uuu);
    free(connector);
    return eIO_Success;
}



/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/


extern CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* info,
 const char*         user_header,
 THCC_Flags          flags)
{
    return HTTP_CreateConnectorEx(info, user_header, flags, 0, 0, 0);
}



extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* info,
 const char*         user_header,
 THCC_Flags          flags,
 FHttpAdjustInfo     adjust_info,
 void*               adjust_data,
 FHttpAdjustCleanup  adjust_cleanup)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector    ));
    SHttpConnector* uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector));

    /* initialize internal data structures */
    uuu->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(0);
    ConnNetInfo_AdjustForHttpProxy(uuu->info);
    if ( uuu->info->debug_printout ) {
        ConnNetInfo_Print(uuu->info, stderr);
    }

    uuu->user_header = user_header ? 
        strcpy((char*) malloc(strlen(user_header) + 1), user_header) : 0;

    uuu->flags = flags;
    if (flags & fHCC_UrlDecodeInput) {
        uuu->flags &= ~fHCC_KeepHeader;
    }

    uuu->adjust_info    = adjust_info;
    uuu->adjust_data    = adjust_data;
    uuu->adjust_cleanup = adjust_cleanup;

    uuu->sock = 0;
    uuu->buf  = 0;
    uuu->c_timeout = 0;
    uuu->w_timeout = 0;
    uuu->can_connect = (flags & fHCC_AutoReconnect) ? eCC_Unlimited : eCC_Once;


    /* initialize handle */
    ccc->handle = uuu;

    /* initialize virtual table */ 
    ccc->vtable.get_type    = s_VT_GetType;
    ccc->vtable.connect     = s_VT_Connect;
    ccc->vtable.wait        = s_VT_Wait;
#ifdef IMPLEMENTED__CONN_WaitAsync
    ccc->vtable.wait_async  = s_VT_WaitAsync;
#endif
    ccc->vtable.write       = s_VT_Write;
    ccc->vtable.flush       = s_VT_Flush;
    ccc->vtable.read        = s_VT_Read;
    ccc->vtable.close       = s_VT_Close;

    /* done */
    return ccc;
}
