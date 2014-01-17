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
 *   (client and server) of firewall daemon and service dispatcher.
 *
 */

#define NCBID_WEBPATH          "/Service/ncbid.cgi"
#define NCBI_DISP_VERSION      "1.2"
#define HTTP_CONNECTION_INFO   "Connection-Info:"
#define HTTP_DISP_FAILURES     "Dispatcher-Failures:"
#define HTTP_DISP_MESSAGES     "Dispatcher-Messages:"
#define HTTP_NCBI_MESSAGE      "NCBI-Message:"
#define HTTP_NCBI_SID          "NCBI-SID:"
#define LBSM_DEFAULT_TIME      30     /* Default expiration time, in seconds */
#define LBSM_DEFAULT_RATE      1000.0 /* For SLBSM_Service::info::rate       */
#define LBSM_STANDBY_THRESHOLD 0.01
#define DISPATCHER_CFGPATH     "/etc/lbsmd/"
#define DISPATCHER_CFGFILE     "servrc.cfg"
#define DISPATCHER_MSGFILE     ".dispd.msg"
#define CONN_FWD_PORT_MIN      5860
#define CONN_FWD_PORT_MAX      5870

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int           ticket_t;


/* This structure is assumed packed */
typedef struct {
    unsigned int   host;   /* must be in network byte order                  */
    unsigned short port;   /* see note about byte flag byte order below      */
    unsigned short flag;   /* FWDaemon control information, see below        */
    ticket_t       ticket; /* connection ticket (raw binary data, n.b.o.)    */
    unsigned int   client; /* expected host to call back (nbo, logging only) */
    char           text[1];/* name requested (for statistics purposes only)  */
} SFWDRequestReply;


/* Maximal accepted request/reply size */
#define FWD_MAX_RR_SIZE 128


/*
 * Currently, bit 0 (if set) of FWDaemon control information (flag) is used to
 * indicate that the client is a true firewall client.  If the bit is clear,
 * it means that the client is a relay client (and should use a secondary
 * -not an official firewall- port of the daemon, if available).
 * Non-zero bit 0 in response indicates that the true firewall mode (via DMZ)
 * is available (acknowledged when requested) and is being used by FWDaemon.
 *
 * Byte order for port and flag fields:
 * When FWDaemon is contacted via INET socket, these two fields must be
 * in network byte order.
 * When FWDaemon is contacted via UNIX socket, these two fields are assumed
 * to be in host byte order, unless 0xF000 is ORed with input "flag" value
 * and both fields are then converted (or not) into network byte order:  in
 * this case the byte order can be auto-detected, and the values returned
 * in both fields in the response are going to use that very same byte order.
 * NOTE:  0xF000 can also be used with INET socket, but conversion to and
 *        from network byte order is still mandatory.
 * NOTE:  0xF000 is ORed in reply flag field only if it has been present
 *        in the request.
 * NOTE:  This is a transitional interface;  future revisions will
 *        require both flag and port to always be in network byte order.
 */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONNECT___NCBI_COMM__H */
