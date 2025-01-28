#ifndef CONNECT_IMPL___NCBI_SERVICEP__H
#define CONNECT_IMPL___NCBI_SERVICEP__H

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
 *   Quasy public service API.
 *
 */
 
#include <connect/ncbi_service.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Modified "fast track" routine for obtaining a server info in one-shot.
 * Please see <connect/ncbi_service.h> for explanations [SERV_GetInfoEx()].
 *
 * CAUTION: Unlike the 'service' parameter, for performance reasons 'arg'
 *          and 'val' are not copied into the internal iterator structure
 *          but the original pointers to them get stored -- take this into
 *          account while dealing with dynamically allocated strings in the
 *          slow iterative version of the call below -- the pointers must
 *          remain valid as long as the iterator stays open (i.e. until
 *          SERV_Close() gets called).
 *
 * NOTE: Preference 0.0 does not prohibit the preferred_host to be selected;
 *       nor preference 100.0 ultimately opts for the preferred_host;  rather,
 *       the preference is considered as an estimate for the selection
 *       probability when all other conditions for favoring the host are
 *       optimal, i.e. preference 0.0 actually means not to favor the preferred
 *       host at all, while 100.0 means to opt for that as much as possible.
 *
 * NOTE: Preference < 0.0 is a special value that means to latch the preferred
 *       host[:port] if the service exists out there, regardless of the load
 *       (but taking into account the server disposition [working/non-working]
 *       only: servers, which are down, don't get returned unless requested).
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoP
(const char*          service,       /* service name (may not be a mask here)*/
 TSERV_Type           types,         /* mask of type(s) of servers requested */
 unsigned int         preferred_host,/* preferred host to use service on, nbo*/
 unsigned short       preferred_port,/* preferred port to use service on, hbo*/
 double               preference,    /* [0,100] preference in %, or -1(latch)*/
 const SConnNetInfo*  net_info,      /* for network dispatcher (0 to disable)*/
 SSERV_InfoCPtr       skip[],        /* array of servers NOT to select       */
 size_t               n_skip,        /* number of servers in preceding array */
 int/*bool*/          external,      /* whether mapping is not local to NCBI */
 const char*          arg,           /* environment variable name to search  */
 const char*          val,           /* environment variable value to match  */
 HOST_INFO*           hinfo          /* host information to return on match  */
 );


/* Same as the above but creates an iterator to get the servers one by one.
 * CAUTION:  Special requirement for "skip" infos in case of a wildcard
 * service is that they _must_ be created having a name (perhaps, empty "")
 * attached, like if done by SERV_ReadInfoEx() or SERV_CopyInfoEx().
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenP
(const char*          service,       /* service name (here: can be a mask!)  */
 TSERV_Type           types,
 unsigned int         preferred_host,
 unsigned short       preferred_port,
 double               preference,
 const SConnNetInfo*  net_info,
 SSERV_InfoCPtr       skip[],        /* must be "named" if service is a mask!*/
 size_t               n_skip,
 int/*bool*/          external,
 const char*          arg,
 const char*          val
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /*CONNECT_IMPL___NCBI_SERVICEP__H*/
