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
#define HTTP_CONNECTION_INFO   "Connection-Info:"
#define HTTP_NCBI_SID          "NCBI-SID:"
#define HTTP_DISP_FAILURES     "Dispatcher-Failures:"
#define HTTP_DISP_MESSAGES     "Dispatcher-Messages:"
#define HTTP_NCBI_MESSAGE      "NCBI-Message:"
#define LBSM_DEFAULT_TIME      30     /* Default expiration time, in seconds */
#define LBSM_DEFAULT_RATE      1000.0 /* For SLBSM_Service::info::rate       */
#define DISPATCHER_CFGPATH     "/etc/lbsmd/"
#define DISPATCHER_CFGFILE     "servrc.cfg"
#define DISP_PROTOCOL_VERSION  "1.1"
#define DISPD_MESSAGE_FILE     ".dispd.msg"
#define CONN_FWD_PORT_MIN      5860
#define CONN_FWD_PORT_MAX      5870

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int           ticket_t;


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONNECT___NCBI_COMM__H */
