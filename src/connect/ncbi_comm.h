#ifndef CONNECT___NCBI_COMM__H
#define CONNECT___NCBI_COMM__H

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
 *   Common part of internal communication protocol used by both sides
 *   (client and server) of firewall daemon and service dispatchers.
 *
 */

#define NCBID_WEBPATH           "/Service/ncbid.cgi"
#define NCBI_DISP_VERSION       "1.2"
#define HTTP_CONNECTION_INFO    "Connection-Info:"
#define HTTP_DISP_FAILURES      "Dispatcher-Failures:"
#define HTTP_DISP_MESSAGES      "Dispatcher-Messages:"
#define HTTP_NCBI_MESSAGE       "NCBI-Message:"
#define HTTP_NCBI_SID           "NCBI-SID:"
#define HTTP_NCBI_PHID          "NCBI-PHID:"
#define LBSM_DEFAULT_TIME       30      /* Default expiration time, seconds */
#define LBSM_DEFAULT_RATE       1000.0  /* For SLBSM_Service::info::rate    */
#define LBSM_STANDBY_THRESHOLD  0.01
#define DISPATCHER_CFGPATH      "/etc/lbsmd/"
#define DISPATCHER_CFGFILE      "servrc.cfg"
#define DISPATCHER_MSGFILE      ".dispd.msg"
#define CONN_FWD_PORT_MIN       5860
#define CONN_FWD_PORT_MAX       5870
#define CONN_FWD_BASE                                   \
    "https://www.ncbi.nlm.nih.gov/IEB/ToolBox/NETWORK"
#define CONN_FWD_LINK           CONN_FWD_BASE "/dispatcher.html#Firewalling"
#define CONN_FWD_URL            CONN_FWD_BASE "/firewall.html#Settings"
#define NCBI_EXTERNAL           "NCBI-External"
#define SERVNSD_TXT_RR_PORT     "_PORT="


#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int  ticket_t;


/** FWDaemon request / reply.
 *  Assumed packed, all intergal fields are in network byte order.
 *
 *  A client (identified by its IP in the "origin" field, or "0" for current
 *  host, or "-1" for unknown host) requests to connect to "host:port", and to
 *  send an optional (when non-zero) "ticket" as the very first data in that
 *  connection.
 *  The client may also (optionally) identify the connection with a variable
 *  size "text" (like a service name) that must be '\0'-terminated unless it
 *  extends to the maximal request size, FWD_RR_MAX_SIZE.  In case if no such
 *  information can or should be provided, the request may skip transmitting
 *  the "text" field altogether, or can put a single '\0' in that field.
 *  @note that in order to be processed correctly, if the "text" field is to be
 *  sent, it _must_ be sent in a single transaction (syscall) with the rest of
 *  the request.
 *  Bit 0 (FWD_RR_FIREWALL, if set) of the FWDaemon control ("flag") is used to
 *  indicate that the client is a true firewall client.  If the bit is clear,
 *  it means that the client is a relay client (and hence, should use a
 *  secondary, _not an official firewall_, port of the daemon, if available).
 *
 *  In a successful reply, FWDaemon sends back a "host:port" pair for the
 *  client to re-connect to, and to send a new (non-zero) "ticket" as the very
 *  first data of that connection, so that the client can reach the endpoint
 *  requested.  If FWD_RR_KEEPALIVE was requested in "flag", then "ticket" can
 *  be returned as 0 to indicate that the client _must_ keep reusing the
 *  existing connection to this FWDaemon in order to talk to the endpoint.
 *  FWDaemon identifies itself in the "origin" field.
 *  Non-zero bit 0 in "flag" of a successful reply indicates that the true
 *  firewall mode (via DMZ) is available (acknowledged when requested), and is
 *  being used by FWDaemon.  The "text" field contains no useful information
 *  (it may not be present at all if the "ticket" returned non-zero, i.e. the
 *  re-connect is required;  otherwise, it is always '\0'-terminated unless it
 *  extends to the maximal reply size, FWD_RR_MAX_SIZE).
 *
 *  An error is signified by either a short reply (shorter than up to "text" --
 *  have to be discarded, and not considered to have any valid fields), or by
 *  "port" returned 0, or by the "flag" field testing non-zero with the
 *  FWD_RR_ERRORMASK mask.  In the latter two cases of a full (i.e. not
 *  short) failure reply received:
 *  1. If "flag" does not have any bits set within FWD_RR_ERRORMASK, then:
 *      if "flag" has some bits set in FWD_RR_REJECTMASK, then the client was
 *      "rejected";  otherwise, the error is "unknown" (the "text" field, if
 *      received and non-empty, may contain an optional error message in either
 *      of these cases);
 *  2. If "flag" has some bits set within FWD_RR_ERRORMASK, then:
 *      if first 4 bytes of the reply contain "NCBI", then the entire reply is
 *      an error message (up to FWD_RR_MAX_SIZE or '\0', whichever comes first)
 *      and all the remaining fields of the reply should be considered invalid;
 *      else if the "text" field is present and non-empty, then it contains an
 *      error message;  otherwise, the error is "unspecified".
 *
 * @sa
 *   FWDaemon_Request
 */

/** FWdaemon request / reply codes (the "flag" field, see above) */
#define FWD_RR_FIREWALL    1  /**< FIREWALL mode client, else RELAY          */
#define FWD_RR_KEEPALIVE   2  /**< Try to reuse the connection               */

/** FWDaemon FWD_RR_REJECTMASK codes (the "flag" field, see above) */
#define FWD_RR_BADREQUEST  1  /**< Bad request    (e.g. port 0 and no svc)   */
#define FWD_RR_USEDIRECT   2  /**< Use directly   (e.g. via direct connect)  */
#define FWD_RR_NOFORWARD   3  /**< Bad forwarding (e.g. non-local endpoint)  */
#define FWD_RR_NOTFOUND    4  /**< Service not found                         */
#define FWD_RR_CANTCONN    5  /**< Cannot connect to server                  */
#define FWD_RR_REFUSED     6  /**< Refused (e.g. due to abuse)               */

#define FWD_RR_ERRORMASK   0xF0F0
#define FWD_RR_REJECTMASK  0x0F0F

typedef struct {
    unsigned int   host;      /**< Host to connect to                        */
    unsigned short port;      /**< Port to connect to (if 0, use service)    */
    unsigned short flag;      /**< FWDaemon control flag                     */
    ticket_t       ticket;    /**< Connection ticket                         */
    unsigned int   origin;    /**< Host requesting / replying                */
    char           text[1];   /**< Service name / error message / status     */
} SFWDRequestReply;


/** Maximal accepted request/reply size */
#define FWD_RR_MAX_SIZE  128
#define FWD_MAX_RR_SIZE  FWD_RR_MAX_SIZE


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONNECT___NCBI_COMM__H */
