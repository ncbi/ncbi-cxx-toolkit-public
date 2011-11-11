#ifndef CONNECT___HTTP_CONNECTOR__H
#define CONNECT___HTTP_CONNECTOR__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for the HTTP-based network connection
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector ("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include <connect/ncbi_connutil.h>

#ifndef NCBI_DEPRECATED
#  define NCBI_HTTP_CONNECTOR_DEPRECATED
#else
#  define NCBI_HTTP_CONNECTOR_DEPRECATED NCBI_DEPRECATED
#endif


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to hit the specified URL using HTTP
 * with either POST / GET or CONNECT method.
 * Use the configuration values recorded in "net_info". If "net_info" is NULL,
 * then use the default info (created by "ConnNetInfo_Create(0)").
 *
 * If "net_info" does not explicitly specify an HTTP request method (i.e.
 * it has it as "eReqMethod_Any"), then the actual method sent to the HTTP
 * server depends on whether any data have been written to the connection
 * with CONN_Write():  the presense of pending data will cause POST request
 * (with Content-Length tag provided automatically and reflecting the total
 * pending data size), and GET request method will result in the absence
 * of any data.  An explicit value for the request method will cause the
 * specified request to be used regardless of pending data, and flagging
 * an error if any data will have to be sent with GET (per the standard).
 *
 * In order to workaround some HTTP communication features, this code does:
 *  1) Accumulate all output data in an internal memory buffer until the
 *     first "Read" (or "Peek", or "Close", or "Wait" on read) is attempted
 *     (also see fHTTP_Flushable flag below).
 *  2) On the first "Read" (or "Peek", or "Close", or "Wait" on read),
 *     compose and send the whole HTTP request as:
 *        {POST|GET} <net_info->path>?<net_info->args> HTTP/1.0\r\n
 *        <user_header\r\n>
 *        Content-Length: <accumulated_data_length>\r\n
 *        \r\n
 *        <accumulated_data>
 *     NOTE:
 *       if <user_header> is neither a NULL pointer nor an empty string, then:
 *       - it must NOT contain any "empty lines":  "\r\n\r\n";
 *       - it may be terminated by a single "\r\n";
 *       - it gets inserted to the HTTP header "as is", without any automatic
 *         checking and / or encoding;
 *       - the "user_header" specified in the arguments overrides any user
 *         header that can be provided via the "net_info" argument, see
 *         ConnNetInfo_OverrideUserHeader() from <connect/ncbi_connutil.h>.
 *     NOTE:
 *       Data may depart to the server side earlier if Flush()'ed in
 *       fHTTP_Flushable connector, see below in "flags".
 *  3) After the request has been sent, then the response data from the peer
 *     (usually, a CGI program) can be actually read out.
 *  4) On any "Write" operation, which follows data reading, the connection
 *     to the peer is read out until EOF (the data stored internally) then
 *     forcedly closed (the peer CGI process will presumably die if it has not
 *     done so yet on its own), and data to be written again get stored in the
 *     buffer until next "Read" etc, see item 1).  The following "Read" will
 *     first see the leftovers (if any) of data stored previously, then the
 *     new data generated in response to the latest request.  The behavior
 *     can be changed by the fHTTP_DropUnread flag, see below.
 *
 *  *) If "fHTTP_AutoReconnect" is set in "flags", then the connector makes
 *     an automatic reconnect to the same URL with just the same parameters
 *     for each micro-session steps (1,2,3) repeated.
 *
 *     If "fHTTP_AutoReconnect" is not set then only a single
 *     "Write ... Write Read ... Read" micro-session is allowed, any
 *     following "Write" attempt fails with an error "eIO_Closed".
 *
 *  Other flags:
 *
 *  fHTTP_Flushable --
 *       by default all data written to the connection are kept until read
 *       begins (even though Flush() might have been called in between the
 *       writes);  with this flag set, Flush() will result the data to be
 *       actually sent to the server side, so the following write will form a
 *       new request, and not get added to the previous one;  also this flag
 *       assures that the connector sends at least an HTTP header on "CLOSE"
 *       and re-"CONNECT", even if no data for HTTP body have been written
 *  fHTTP_KeepHeader --
 *       do not strip HTTP header (i.e. everything up to the first "\r\n\r\n",
 *       including the "\r\n\r\n") from the CGI script's response (including
 *       any server error, which then is made available for reading as well)
 *       *NOTE* this flag disables automatic authorization, redirection, etc.
 *  fHTTP_UrlDecodeInput --
 *       assume the response body single-part, URL-encoded;  perform the
 *       URL-decoding on read, and deliver decoded data to the user
 *  fHTTP_NoUpread --
 *       do *not* do internal reading into temporary buffer while sending data
 *       to HTTP server;  by default any send operation tries to extract data
 *       as they are coming back from the server in order to prevent stalling
 *       due to data clogging in the connection
 *  fHTTP_DropUnread --
 *       do not collect incoming data in "Read" mode before switching into
 *       "Write" mode for preparing next request;  by default all data sent by
 *       the server get stored even if not all of it had been requested prior
 *       to a "Write" following a "Read" was issued (stream emulation)
 *  fHTTP_NoAutoRetry --
 *       do not attempt any auto-retries in case of failing connections
 *       (this flag effectively means having SConnNetInfo::max_try set to 1)
 *  fHTTP_InsecureRedirect --
 *       for security reasons the following redirects comprise security risk
 *       and, thus, are prohibited:  switching from https to http, and
 *       re-posting data (regardless of the transport, either http or https);
 *       this flag allows such redirects (if needed) to be honored
 *
 * NOTE: the URL encoding/decoding (in the "fHTTP_Url_*" cases and
 *       "net_info->args") is performed by URL_Encode() and URL_Decode()
 *       -- see "ncbi_connutil.[ch]".
 *
 * @sa
 *  SConnNetInfo, ConnNetInfo_OverrideUserHeader, URL_Encode, URL_Decode
 */

enum EHTTP_Flag {
    fHTTP_AutoReconnect   = 0x1,  /* see (*) above                           */
    fHTTP_Flushable       = 0x2,  /* connector will really flush on Flush()  */
    fHTTP_KeepHeader      = 0x4,  /* dont strip HTTP header from CGI response*/
    fHTTP_UrlEncodeArgs   = 0x8,  /* URL-encode "info->args" (w/o fragment)  */
    fHTTP_UrlDecodeInput  = 0x10, /* URL-decode contents of the response body*/
    fHTTP_UrlEncodeOutput = 0x20, /* URL-encode all output data              */
    fHTTP_UrlCodec        = 0x30, /* fHTTP_UrlDecodeInput | ...EncodeOutput  */
    fHTTP_NoUpread        = 0x40, /* do not use SOCK_SetReadOnWrite() at all */
    fHTTP_DropUnread      = 0x80, /* each microsession drops yet unread data */
    fHTTP_NoAutoRetry     = 0x100,/* no auto-retries allowed                 */
    fHTTP_NoAutomagicSID  = 0x200,/* do not add NCBI SID automagically       */
    fHTTP_InsecureRedirect= 0x400,/* any redirect will be honored            */
    /*fHTTP_ReservedFlag  = 0x800,   do not use                              */
    fHTTP_DetachableTunnel= 0x1000/* SOCK_Close() won't close the OS handle  */
};
typedef unsigned int THTTP_Flags;  /* bitwise OR of "EHTTP_Flag"             */
NCBI_HTTP_CONNECTOR_DEPRECATED
enum EHCC_Flag {
    /* DEPRECATED, do not use! */
    fHCC_AutoReconnect    = fHTTP_AutoReconnect,
    fHCC_SureFlush        = fHTTP_Flushable,
    fHCC_KeepHeader       = fHTTP_KeepHeader,
    fHCC_UrlDecodeInput   = fHTTP_UrlDecodeInput,
    fHCC_UrlEncodeOutput  = fHTTP_UrlEncodeOutput,
    fHCC_UrlCodec         = fHTTP_UrlCodec,
    fHCC_UrlEncodeArgs    = fHTTP_UrlEncodeArgs,
    fHCC_DropUnread       = fHTTP_DropUnread,
    fHCC_NoUpread         = fHTTP_NoUpread,
    fHCC_Flushable        = fHTTP_Flushable,
    fHCC_NoAutoRetry      = fHTTP_NoAutoRetry
};
NCBI_HTTP_CONNECTOR_DEPRECATED
typedef unsigned int THCC_Flags;   /* bitwise OR of "EHCC_Flag", deprecated */


extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags
 );


/* An extended version of HTTP_CreateConnector() is able to change
 * the URL of the server "on-the-fly":
 *  - "FHTTP_ParseHeader()" is called every time a new HTTP response header
 *    is received from the server, and only if fHTTP_KeepHeader is NOT set;
 *    a zero (false) return value is equivalent of having an error from
 *    the HTTP server itself;
 *  - "FHTTP_Adjust()" is invoked every time before starting a new
 *    "HTTP micro-session" making a hit when previous hit has failed;
 *    it is passed "net_info" stored in the connector, and the number of
 *    previously unsuccessful attempts since the connection was opened;
 *    a zero (false) return value ends retry attempts;
 *  - "FHTTP_Cleanup()" is called when the connector is about to be destroyed;
 *    "user_data" is guaranteed not to be referenced anymore (so it is a good
 *    place to clean up "user_data" if necessary).
 */

typedef int/*bool*/ (*FHTTP_ParseHeader)
(const char*         http_header,   /* HTTP header to parse, '\0'-terminated */
 void*               user_data,     /* supplemental user data                */
 int                 server_error   /* != 0 if HTTP error                    */
 );

typedef int/*bool*/ (*FHTTP_Adjust)
(SConnNetInfo*       net_info,      /* net_info to adjust (in place)         */
 void*               user_data,     /* supplemental user data                */
 unsigned int        failure_count  /* how many failures since open          */
 );

typedef void        (*FHTTP_Cleanup)
(void*               user_data      /* supplemental user data                */
 );

extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,  /* may be NULL, then no addtl. parsing   */
 void*               user_data,     /* user data for HTTP callbacks (CBs)    */
 FHTTP_Adjust        adjust,        /* may be NULL                           */
 FHTTP_Cleanup       cleanup        /* may be NULL                           */
 );


/* Create a tunnel to "net_info->host:net_info->port" via an HTTP proxy
 * server located at "net_info->http_proxy_host:net_info->http_proxy_port".
 * Return the tunnel as a socket via the last parameter.  For compatibility
 * with future API extensions, please make sure *sock == NULL when the call
 * is made.
 * @return
 *  eIO_Success if the tunnel has been successfully created;
 *  otherwise, return an error code and set *sock to NULL upon return.
 * @sa
 *  ESOCK_Flags, SOCK_CreateEx, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status HTTP_CreateTunnelEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 const void*         init_data,
 size_t              init_size,
 SOCK*               sock
 );


/* Same as HTTP_CreateTunnelEx(net_info, flags, 0, 0, sock) */
extern NCBI_XCONNECT_EXPORT EIO_Status HTTP_CreateTunnel
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 SOCK*               sock
 );


/* Set message hook procedure for messages originating from NCBI via HTTP.
 * Any hook will be called not more than once.  Until no hook is installed,
 * and exactly one message is caught, a warning will be generated in
 * the standard log file upon each message acceptance.
 */

typedef void (*FHTTP_NcbiMessageHook)(const char* message);

extern NCBI_XCONNECT_EXPORT void HTTP_SetNcbiMessageHook
(FHTTP_NcbiMessageHook            /* New hook to be installed, NULL to reset */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___HTTP_CONNECTOR__H */
