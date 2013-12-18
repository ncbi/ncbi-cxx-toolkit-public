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


/** HTTP connector flags.
 *
 * @var fHTTP_Flushable
 *       by default all data written to the connection are kept until read
 *       begins (even though CONN_Flush() might have been called in between the
 *       writes);  with this flag set, CONN_Flush() will result the data to be
 *       actually sent to the server side, so the following write will form a
 *       new request, and not get added to the previous one;  also this flag
 *       assures that the connector sends at least an HTTP header on "CLOSE"
 *       and re-"CONNECT", even if no data for HTTP body have been written.
 *
 * @var fHTTP_KeepHeader
 *       Do not strip HTTP header (i.e. everything up to the first "\r\n\r\n",
 *       including the "\r\n\r\n") from the CGI script's response (including
 *       any server error, which then is made available for reading as well)
 *       *NOTE* this flag disables automatic authorization and redirection.
 *
 * @var fHTTP_UrlDecodeInput
 *       Assume the response body as single-part, URL-encoded;  perform the
 *       URL-decoding on read, and deliver decoded data to the user.
 *
 * @var fHTTP_NoUpread
 *       Do *not* do internal reading into temporary buffer while sending data
 *       to HTTP server;  by default any send operation tries to extract data
 *       as they are coming back from the server in order to prevent stalling
 *       due to data clogging the connection.
 *
 * @var fHTTP_DropUnread
 *       Do not collect incoming data in "Read" mode before switching into
 *       "Write" mode for preparing next request;  by default all data sent by
 *       the server get stored even if not all of it had been requested prior
 *       to a "Write" following a "Read" was issued (stream emulation).
 *
 * @var fHTTP_NoAutoRetry
 *       Do not attempt any auto-retries in case of failing connections
 *       (this flag effectively means having SConnNetInfo::max_try set to 1).

 * @var fHTTP_InsecureRedirect
 *       For security reasons the following redirects comprise security risk,
 *       and thus, are prohibited:  switching from https to http, and/or
 *       re-POSTing data (regardless of the transport, either http or https);
 *       this flag allows such redirects (when encountered) to be honored.
 *
 * @note
 *  URL encoding/decoding (in the "fHTTP_Url*" cases and "net_info->args")
 *  is performed by URL_Encode() and URL_Decode() -- see "ncbi_connutil.[ch]".
 *
 * @sa
 *  SConnNetInfo, ConnNetInfo_OverrideUserHeader, URL_Encode, URL_Decode
 */
enum EHTTP_Flag {
    fHTTP_AutoReconnect   = 0x1,  /**< See HTTP_CreateConnectorEx()          */
    fHTTP_Flushable       = 0x2,  /**< Connector will really flush on Flush()*/
    fHTTP_KeepHeader      = 0x4,  /**< Keep HTTP header (see limitations)    */
    /*fHTTP_UrlEncodeArgs = 0x8,       URL-encode "info->args" (w/o fragment)*/
    fHTTP_UrlDecodeInput  = 0x10, /**< URL-decode response body              */
    fHTTP_UrlEncodeOutput = 0x20, /**< URL-encode all output data            */
    fHTTP_UrlCodec        = 0x30, /**< fHTTP_UrlDecodeInput | ...EncodeOutput*/
    fHTTP_NoUpread        = 0x40, /**< Do not use SOCK_SetReadOnWrite()      */
    fHTTP_DropUnread      = 0x80, /**< Each microsession drops unread data   */
    fHTTP_NoAutoRetry     = 0x100,/**< No auto-retries allowed               */
    fHTTP_NoAutomagicSID  = 0x200,/**< Do not add NCBI SID automagically     */
    fHTTP_InsecureRedirect= 0x400,/**< Any redirect will be honored          */
    /*fHTTP_ReservedFlag  = 0x800,     Do not use                            */
};
typedef unsigned int THTTP_Flags; /**< Bitwise OR of EHTTP_Flag              */
NCBI_HTTP_CONNECTOR_DEPRECATED
/** DEPRECATED, do not use! */
typedef enum {
    fHCC_AutoReconnect    = fHTTP_AutoReconnect,
    fHCC_SureFlush        = fHTTP_Flushable,
    fHCC_KeepHeader       = fHTTP_KeepHeader,
    fHCC_UrlDecodeInput   = fHTTP_UrlDecodeInput,
    fHCC_UrlEncodeOutput  = fHTTP_UrlEncodeOutput,
    fHCC_UrlCodec         = fHTTP_UrlCodec,
    fHCC_UrlEncodeArgs    = 0x8,  /**< NB: Error-prone semantics, don't use! */
    fHCC_DropUnread       = fHTTP_DropUnread,
    fHCC_NoUpread         = fHTTP_NoUpread,
    fHCC_Flushable        = fHTTP_Flushable,
    fHCC_NoAutoRetry      = fHTTP_NoAutoRetry
} EHCC_Flag;
NCBI_HTTP_CONNECTOR_DEPRECATED
typedef unsigned int THCC_Flags;  /**< bitwise OR of EHCC_Flag, deprecated   */


/** Same as HTTP_CreateConnector(net_info, flags, 0, 0, 0, 0)
 * with the passed "user_header" overriding the value provided in
 * "net_info->http_user_header".
 * @sa
 *  HTTP_CreateConnectorEx, ConnNetInfo_OverrideUserHeader
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags
 );


/** The extended version HTTP_CreateConnectorEx() is able to track the HTTP
 * response chain and also change the URL of the server "on-the-fly":
 * - FHTTP_ParseHeader() gets called every time a new HTTP response header is
 *   received from the server, and only if fHTTP_KeepHeader is NOT set.
 *   Return code from the parser adjusts the existing server error condition
 *   (if any) as the following:
 *   + eHTTP_HeaderError:    unconditionally flag a server error;
 *   + eHTTP_HeaderSuccess:  header parse successful, retain existing condition
 *                           (note that in case of an already existing server
 *                           error condition the response body can be logged
 *                           but will not be made available for the user code
 *                           to read, and eIO_Unknown will result on read);
 *   + eHTTP_HeaderContinue: if there was already a server error condition,
 *                           the response body will be made available for the
 *                           user code to read (but only if HTTP connector
 *                           cannot post-process the request such as for
 *                           redirects, authorization etc);  otherwise, this
 *                           code has the same effect as eHTTP_HeaderSuccess;
 *   + eHTTP_HeaderComplete: flag this request as processed completely, and do
 *                           not do any post-processing (such as redirects,
 *                           authorization etc), yet make the response body (if
 *                           any, and regardless of whether there was a server
 *                           error or not) available for reading.
 * - FHTTP_Adjust() gets invoked every time before starting a new "HTTP
 *   micro-session" making a hit when a previous hit has failed;  it is passed
 *   "net_info" as stored within the connector, and the number of previously
 *   unsuccessful consecutive attempts (in the least significant word) since
 *   the connection was opened;  a zero (false) return value ends the retries.
 * - FHTTP_Cleanup() gets called when the connector is about to be destroyed;
 *   "user_data" is guaranteed not to be referenced anymore (so this is a good
 *   place to clean up "user_data" if necessary).
 * @sa
 *   SConnNetInfo::max_try
 */
typedef enum {
    eHTTP_HeaderError    = 0,  /**< Parse failed, treat as a server error */
    eHTTP_HeaderSuccess  = 1,  /**< Parse succeeded, retain server status */
    eHTTP_HeaderContinue = 2,  /**< Parse succeeded, continue with body   */
    eHTTP_HeaderComplete = 3   /**< Parse succeeded, no more processing   */
} EHTTP_HeaderParse;
typedef EHTTP_HeaderParse (*FHTTP_ParseHeader)
(const char*         http_header,   /**< HTTP header to parse                */
 void*               user_data,     /**< supplemental user data              */
 int                 server_error   /**< != 0 if HTTP error (NOT 2xx code)   */
 );

typedef int/*bool*/ (*FHTTP_Adjust)
(SConnNetInfo*       net_info,      /**< net_info to adjust (in place)       */
 void*               user_data,     /**< supplemental user data              */
 unsigned int        failure_count  /**< low word: # of failures since open  */
 );

typedef void        (*FHTTP_Cleanup)
(void*               user_data      /**< supplemental user data              */
 );


/** Create new CONNECTOR structure to hit the specified URL using HTTP with
 * either POST / GET (or ANY) method.  Use the configuration values stored in
 * "net_info".  If "net_info" is NULL, then use the default info as created by
 * ConnNetInfo_Create(0).
 *
 * If "net_info" does not explicitly specify an HTTP request method (i.e. it
 * has it as "eReqMethod_Any"), then the actual method sent to the HTTP server
 * depends on whether any data have been written to the connection with
 * CONN_Write():  the presense of pending data will cause a POST request (with
 * a "Content-Length:" tag supplied automatically and reflecting the total
 * pending data size), and GET request method will result in the absence of any
 * data.  An explicit value for the request method will cause the specified
 * request to be used regardless of pending data, and will flag an error if any
 * data will have to be sent with a GET (per the standard).
 *
 * In order to workaround some HTTP communication features, this code does:
 *  1. Accumulate all output data in an internal memory buffer until the
 *     first CONN_Read() (including peek) or CONN_Wait(on read) is attempted
 *     (also see fHTTP_Flushable flag below).
 *  2. On the first CONN_Read() or CONN_Wait(on read), compose and send the
 *     whole HTTP request as:
 *        @verbatim
 *        {POST|GET} <net_info->path>?<net_info->args> HTTP/1.0\r\n
 *        <user_header\r\n>
 *        Content-Length: <accumulated_data_length>\r\n
 *        \r\n
 *        <accumulated_data>
 *        @endverbatim
 *     @note
 *       If <user_header> is neither a NULL pointer nor an empty string, then:
 *       - it must NOT contain any "empty lines":  "\r\n\r\n";
 *       - multiple tags must be separated by "\r\n" (*not* just "\n");
 *       - it should be terminated by a single "\r\n" (will be added, if not);
 *       - it gets inserted to the HTTP header "as is", without any automatic
 *         checking and / or encoding (except for the trailing "\r\n");
 *       - the "user_header" specified in the arguments overrides any user
 *         header that can be provided via the "net_info" argument, see
 *         ConnNetInfo_OverrideUserHeader() from <connect/ncbi_connutil.h>.
 *     @note
 *       Data may depart to the server side earlier if CONN_Flush()'ed in a
 *       fHTTP_Flushable connector, see "flags".
 *  3. After the request has been sent, then the response data from the peer
 *     (usually, a CGI program) can be actually read out.
 *  4. On a CONN_Write() operation, which follows data reading, the connection
 *     to the peer is read out until EOF (the data stored internally) then
 *     forcedly closed (the peer CGI process will presumably die if it has not
 *     done so yet on its own), and data to be written again get stored in the
 *     buffer until next "Read" etc, see item 1).  The subsequent read will
 *     first see the leftovers (if any) of data stored previously, then the
 *     new data generated in response to the latest request.  The behavior can
 *     be changed by the fHTTP_DropUnread flag.
 *
 *  @note
 *     If "fHTTP_AutoReconnect" is set in "flags", then the connector makes an
 *     automatic reconnect to the same URL with just the same parameters for
 *     each micro-session steps (1,2,3) repeated.
 *  @note
 *     If "fHTTP_AutoReconnect" is not set then only a single
 *     "Write ... Write Read ... Read" micro-session is allowed, and any
 *     following write attempt fails with "eIO_Closed".
 *
 * @sa
 *  EHTTP_Flag
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,  /**< may be NULL, then no addtl. parsing */
 void*               user_data,     /**< user data for HTTP CBs (callbacks)  */
 FHTTP_Adjust        adjust,        /**< may be NULL                         */
 FHTTP_Cleanup       cleanup        /**< may be NULL                         */
 );


/** Create a tunnel to "net_info->host:net_info->port" via an HTTP proxy server
 * located at "net_info->http_proxy_host:net_info->http_proxy_port".  Return
 * the tunnel as a socket via the last parameter.  For compatibility with
 * future API extensions, please make sure *sock is NULL when making the call.
 * If "net_info->scheme == eURL_Https", the returned socket is secure.
 * @note
 *  "net_info" can be passed as NULL to be constructed from the environment.
 * @note
 *  Some HTTP proxies do not process "data" correctly (e.g. Squid 3) when sent
 *  along with the tunnel creation request (despite the standard specifically
 *  allows such use), so they may require separate SOCK I/O calls to write the
 *  data to the tunnel.
 * @return
 *  eIO_Success if the tunnel has been successfully created;
 *  otherwise, return an error code and set "*sock" to NULL upon return.
 * @sa
 *  THTTP_Flags, SOCK_CreateEx, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status HTTP_CreateTunnelEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 const void*         data,    /**< initial data block to send via the tunnel */
 size_t              size,    /**< size of the initial data block            */
 SOCK*               sock
 );


/** Same as HTTP_CreateTunnelEx(net_info, flags, 0, 0, sock) */
extern NCBI_XCONNECT_EXPORT EIO_Status HTTP_CreateTunnel
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 SOCK*               sock
 );


typedef void (*FHTTP_NcbiMessageHook)(const char* message);

/** Set message hook procedure for messages originating from NCBI via HTTP.
 * Any hook will be called not more than once.  Until no hook is installed,
 * and exactly one message is caught, a warning will be generated in
 * the standard log file upon acceptance of every message.
 */
extern NCBI_XCONNECT_EXPORT void HTTP_SetNcbiMessageHook
(FHTTP_NcbiMessageHook  /**< New hook to be installed, NULL to reset */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___HTTP_CONNECTOR__H */
