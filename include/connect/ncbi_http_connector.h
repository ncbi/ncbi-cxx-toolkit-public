#ifndef NCBI_HTTP_CONNECTOR__H
#define NCBI_HTTP_CONNECTOR__H

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
 * Revision 6.6  2001/05/23 21:52:00  lavr
 * +fHCC_NoUpread
 *
 * Revision 6.5  2001/01/25 16:53:22  lavr
 * New flag for HTTP_CreateConnectorEx: fHCC_DropUnread
 *
 * Revision 6.4  2000/12/29 17:41:44  lavr
 * Pretty printed; HTTP_CreateConnectorEx constructor interface changed
 *
 * Revision 6.3  2000/10/03 21:20:34  lavr
 * Request method changed from POST to {GET|POST}
 *
 * Revision 6.2  2000/09/26 22:02:55  lavr
 * HTTP request method added
 *
 * Revision 6.1  2000/04/21 19:40:58  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_connector.h>
#include <connect/ncbi_connutil.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to hit the specified URL using HTTP
 * with either POST or GET method.
 * Use the configuration values recorded in "info". If "info" is NULL, then
 * use the default info (created by "ConnNetInfo_Create(0)").
 *
 * In order to workaround some HTTP communication features, this code will:
 *  1) Accumulate all output data in an internal memory buffer until the
 *     first "Read" (or "Peek", or "Close", or "Wait" on read) is attempted.
 *  2) On the first "Read" (or "Peek", or "Close", or "Wait" on read), compose
 *     and send the whole HTTP request as:
 *        {POST|GET} <info->path>?<info->args> HTTP/1.0\r\n
 *        <user_header\r\n>
 *        Content-Length: <accumulated_data_length>\r\n
 *        \r\n
 *        <accumulated_data>
 *     NOTE:
 *       if <user->header> is not a NULL or empty string, then:
 *       - it must NOT contain "empty lines":  '\n\r\n';
 *       - it must be terminated by a single '\r\n';
 *       - it gets inserted to the HTTP header "as is", without any
 *         automatic checking or encoding.
 *  3) Now you can "Read" the reply data sent to you by the peer CGI program.
 *  4) Then, if you "Write" again then the connection to the peer CGI program
 *     will be forcibly closed, and you cannot communicate with it anymore
 *     (the peer CGI process will die).
 *
 *  *) But if "fHCC_AutoReconnect" is set in "flags", then the connector will
 *     make an automatic reconnect to the same CGI script with just the
 *     same parameters, and you can repeat the (1,2,3) micro-session with
 *     another instance of your peer CGI program.
 *
 *     If "fHCC_AutoReconnect" is not set then only one
 *     "Write ... Write Read ... Read" micro-session is allowed, and the next
 *     try to "Write" will fail with error status "eIO_Closed".
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
    fHCC_DropUnread       = 0x40, /* Each microsession drops yet unread data */
    fHCC_NoUpread         = 0x80  /* Do not use SOCK_ReadWhileWrite() at all */
} EHCC_Flags;
typedef int THCC_Flags;  /* binary OR of "EHttpCreateConnectorFlags"         */

extern CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* info,
 const char*         user_header,
 THCC_Flags          flags
 );


/* An extended version of URL_CreateConnector() to change the URL of the
 * server CGI "on-the-fly":
 *  -- "adjust_info()" will be invoked each time before starting a
 *      new "HTTP micro-session" making a hit;  it will be passed "info"
 *      stored in the connector, and the # of previous unsuccessful
 *      attempts to start the current HTTP micro-session.
 *  -- "adjust_cleanup()" will be called when the connector is destroyed.
 */

typedef int/*bool*/ (*FHttpParseHTTPHdr)
(const char* http_header,
 void*       adjust_data,
 int/*bool*/ server_error
 );

typedef int/*bool*/ (*FHttpAdjustInfo)
(SConnNetInfo* info,
 void*         adjust_data,
 unsigned int  n_failed
 );

typedef void (*FHttpAdjustCleanup)
(void* adjust_data
 );

extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* info,
 THCC_Flags          flags,
 FHttpParseHTTPHdr   parse_http_hdr, /* can be NULL, then no addtl. parsing  */
 FHttpAdjustInfo     adjust_info,    /* can be NULL, then no adjustments     */
 void*               adjust_data,    /* for "adjust_info" & "adjust_cleanup" */
 FHttpAdjustCleanup  adjust_cleanup  /* can be NULL                          */
);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_HTTP_CONNECTOR__H */
