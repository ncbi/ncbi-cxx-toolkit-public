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
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include <connect/ncbi_connutil.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to hit the specified URL using HTTP
 * with either POST or GET method.
 * Use the configuration values recorded in "net_info". If "net_info" is NULL,
 * then use the default info (created by "ConnNetInfo_Create(0)").
 *
 * In order to workaround some HTTP communication features, this code does:
 *  1) Accumulate all output data in an internal memory buffer until the
 *     first "Read" (or "Peek", or "Close", or "Wait" on read) is attempted
 *     (also see fHCC_Flushable flag below).
 *  2) On the first "Read" (or "Peek", or "Close", or "Wait" on read), compose
 *     and send the whole HTTP request as:
 *        {POST|GET} <info->path>?<info->args> HTTP/1.0\r\n
 *        <user_header\r\n>
 *        Content-Length: <accumulated_data_length>\r\n
 *        \r\n
 *        <accumulated_data>
 *     NOTE:
 *       if <user->header> is neither a NULL pointer nor an empty string, then:
 *       - it must NOT contain "empty lines":  '\r\n\r\n';
 *       - it must be terminated by a single '\r\n';
 *       - it gets inserted to the HTTP header "as is", without any
 *         automatic checking or encoding.
 *     NOTE:
 *       Data may depart to server side earlier if Flush()'ed in
 *       fHCC_Flushable connector, see below in "flags".
 *     After the request has been sent, reply data from the peer
 *     CGI program are then can be actually read out.
 *  4) On any "Write" operation. which follows data reading, the connection
 *     to the peer CGI program is forcedly closed (the peer CGI process will
 *     presumably die if has not done yet so), and data to be written are
 *     again are stored in the buffer until next "Read" etc, see item 1).
 *
 *  *) If "fHCC_AutoReconnect" is set in "flags", then the connector makes
 *     an automatic reconnect to the same CGI program with just the
 *     same parameters, and micro-session steps (1,2,3) are repeated with
 *     another instance of the CGI program.
 *
 *     If "fHCC_AutoReconnect" is not set then only one
 *     "Write ... Write Read ... Read" micro-session is allowed, any
 *     following "Write" attempt fails with error status "eIO_Closed".
 *
 *  Other flags:
 *
 *  fHCC_SureFlush --
 *       make the connector to send at least the HTTP header on "CLOSE" and
 *       re-"CONNECT", even if no data was written
 *  fHCC_KeepHeader --
 *       do not strip HTTP header (i.e. everything up to the first "\r\n\r\n",
 *       including the "\r\n\r\n") from the CGI script's response
 *  fHCC_UrlDecodeInput --
 *       strip the HTTP header from the input data;  assume the input
 *       data are single-part, URL-encoded;  perform the URL-decoding on read
 *       NOTE:  this flag disables the "fHCC_KeepHeader" flag
 *  fHCC_DropUnread --
 *       do not collect incoming data in "Read" mode before switching into
 *       "Write" mode for storing output data in buffer; by default all
 *       data sent by the CGI program are stored even if not all requested
 *       before "Write" following "Read" was issued (stream emulation)
 *  fHCC_NoUpread --
 *       do not do internal reading into temporary buffer while sending
 *       data to CGI program; by default any send operation tries to
 *       extract data(if any) coming back from the CGI program in order to
 *       prevent connection blocking
 *  fHCC_Flushable --
 *       usually all data written to the connection are kept until
 *       read begins (even though Flush() might have been called in between
 *       the writes).  With this flag set, Flush() will result the data
 *       to be actually sent to server side, so the following write will form
 *       new request, and not get added to the previous one.
 *  fHCC_InsecureRedirect --
 *       for security reasons the following redirects comprise security risk
 *       and, thus, are prohibited:  switching from https to http, and
 *       re-posting data (regradless of the transport, either http or https).
 *       This flag allows such redirects (if needed) to be honored.
 *
 * NOTE: the URL encoding/decoding (in the "fHCC_Url_*" cases and "info->args")
 *       is performed by URL_Encode() and URL_Decode() -- "ncbi_connutil.[ch]".
 */

typedef enum {
    fHCC_AutoReconnect    = 0x1,  /* see (*) above                           */
    fHCC_SureFlush        = 0x2,  /* always send HTTP request on CLOSE/RECONN*/
    fHCC_KeepHeader       = 0x4,  /* dont strip HTTP header from CGI response*/
    fHCC_UrlDecodeInput   = 0x8,  /* strip HTTP header, URL-decode content   */
    fHCC_UrlEncodeOutput  = 0x10, /* URL-encode all output data              */
    fHCC_UrlCodec         = 0x18, /* fHCC_UrlDecodeInput | ...EncodeOutput   */
    fHCC_UrlEncodeArgs    = 0x20, /* URL-encode "info->args"                 */
    fHCC_DropUnread       = 0x40, /* each microsession drops yet unread data */
    fHCC_NoUpread         = 0x80, /* do not use SOCK_ReadWhileWrite() at all */
    fHCC_Flushable        = 0x100,/* connector will really flush on Flush()  */
    fHCC_InsecureRedirect = 0x200 /* any redirect will be honored            */
} EHCC_Flags;
typedef unsigned int THCC_Flags;  /* bitwise OR of "EHCC_Flags"              */

extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THCC_Flags          flags
 );


/* An extended version of HTTP_CreateConnector() to change the URL of the
 * server CGI "on-the-fly":
 *  -- "parse_http_hdr()" is called each time the HTTP header received
 *      from HTTP server and only if fHCC_KeepHeader is NOT set; false return
 *      value is equivalent of having an error from the HTTP server itself.
 *  -- "adjust_net_info()" is invoked each time before starting a
 *      new "HTTP micro-session" making a hit if the prior hit has failed;
 *      it is passed "net_info" stored in the connector, and the number of
 *      previously unsuccessful attempts since the connection was opened;
 *      false return value terminates retry attempts.
 *  -- "adjust_cleanup()" is called when the connector is being destroyed.
 */

typedef int/*bool*/ (*FHttpParseHTTPHeader)
(const char* http_header,           /* HTTP header to parse, '\0'-terminated */
 void*       adjust_data,           /* supplemental user data                */
 int/*bool*/ server_error           /* true if HTTP server reported an error */
 );

typedef int/*bool*/ (*FHttpAdjustNetInfo)
(SConnNetInfo* net_info,            /* net_info to adjust (in place)         */
 void*         adjust_data,         /* supplemental user data                */
 unsigned int  failure_count        /* how many failures since open          */
 );

typedef void (*FHttpAdjustCleanup)
(void* adjust_data                  /* supplemental user data for cleanup    */
 );

extern NCBI_XCONNECT_EXPORT CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo*  net_info,
 THCC_Flags           flags,
 FHttpParseHTTPHeader parse_http_hdr, /* may be NULL, then no addtl. parsing */
 FHttpAdjustNetInfo   adjust_net_info,/* may be NULL, then no adjustments    */
 void*                adjust_data,    /* for "adjust_info" & "adjust_cleanup"*/
 FHttpAdjustCleanup   adjust_cleanup  /* may be NULL                         */
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
