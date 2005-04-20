#ifndef CONNECT___NCBI_COMM__H
#define CONNECT___NCBI_COMM__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Common part of internal communication protocol used by both sides
 *   (client and server) of firewall daemon and service dispatcher.
 *
 */


#define NCBID_WEBPATH           "/Service/ncbid.cgi"
#define HTTP_CONNECTION_INFO    "Connection-Info:"
#define HTTP_DISP_FAILURES      "Dispatcher-Failures:"
#define HTTP_DISP_MESSAGE       "NCBI-Message:"
#define DISPATCHER_CFGPATH      "/var/etc/lbsmd/"
#define DISPATCHER_CFGFILE      "servrc.cfg"
#define DISP_PROTOCOL_VERSION   "1.0"
#define DISPD_MESSAGE_FILE      ".dispd.msg"


#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int ticket_t;


#ifdef __cplusplus
} /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.10  2005/04/20 18:13:10  lavr
 * extern "C" limited to code only (not preprocessor macros)
 *
 * Revision 6.9  2004/08/02 16:52:42  lavr
 * +DISPD_MESSAGE_FILE
 *
 * Revision 6.8  2003/08/11 19:06:23  lavr
 * +HTTP_DISP_MESSAGE
 *
 * Revision 6.7  2002/12/10 22:11:14  lavr
 * Add DISP_PROTOCOL_VERSION
 *
 * Revision 6.6  2002/10/11 19:44:51  lavr
 * NCBID_NAME changed into NCBID_WEBAPTH; DISPATCHER_CFGPATH added
 *
 * Revision 6.5  2002/09/19 18:07:57  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.4  2001/08/20 21:59:05  lavr
 * New macro: DISPATCHER_CFGFILE
 *
 * Revision 6.3  2001/05/11 15:28:46  lavr
 * Protocol change: REQUEST_FAILED -> DISP_FAILURES
 *
 * Revision 6.2  2001/01/08 22:34:45  lavr
 * Request-Failed added to protocol
 *
 * Revision 6.1  2000/12/29 18:20:26  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_COMM__H */
