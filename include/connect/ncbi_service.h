#ifndef NCBI_SERVICE__H
#define NCBI_SERVICE__H

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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Top-level API to resolve NCBI service name to the server meta-address.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.18  2001/06/25 15:32:06  lavr
 * Added function: SERV_GetNextInfoEx
 * SERV_Open and SERV_GetNextInfo made macros for faster access
 *
 * Revision 6.17  2001/06/11 22:14:44  lavr
 * Include files adjusted
 *
 * Revision 6.16  2001/06/04 17:00:10  lavr
 * Include files adjusted
 *
 * Revision 6.15  2001/04/26 14:18:20  lavr
 * SERV_MapperName moved to a private header
 *
 * Revision 6.14  2001/04/24 21:15:35  lavr
 * Added functions: SERV_MapperName(), SERV_Penalize().
 *
 * Revision 6.13  2001/03/02 20:05:56  lavr
 * SERV_LOCALHOST addad; SERV_OpenSimple() made more documented
 *
 * Revision 6.12  2001/02/09 17:32:52  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 *
 * Revision 6.11  2001/01/08 22:48:00  lavr
 * Double return 0 in GetNextInfo removed:
 * 0 now indicates an error unconditionally
 *
 * Revision 6.10  2000/12/29 17:40:30  lavr
 * Pretty printed; Double 0 return added to SERV_GetNextInfo
 *
 * Revision 6.9  2000/12/06 22:17:02  lavr
 * Binary host addresses are now explicitly stated to be in network byte
 * order, whereas binary port addresses now use native (host) representation
 *
 * Revision 6.8  2000/10/20 17:03:49  lavr
 * Added 'const' to SConnNetInfo in 'SERV_OpenEx'
 *
 * Revision 6.7  2000/10/05 22:40:06  lavr
 * Additional parameter 'info' in a call to 'Open' for service mapper
 *
 * Revision 6.6  2000/10/05 21:26:30  lavr
 * Log message edited
 *
 * Revision 6.5  2000/10/05 21:10:11  lavr
 * Parameters to 'Open' and 'OpenEx' changed
 *
 * Revision 6.4  2000/05/31 23:12:17  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.3  2000/05/22 16:53:07  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.2  2000/05/12 18:29:22  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_server_info.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Iterator through the servers
 */
struct SSERV_IterTag;
typedef struct SSERV_IterTag* SERV_ITER;


/* Create iterator for the iterative server lookup.
 * Connection information 'info' can be a NULL pointer, which means
 * not to make any network connections (only LBSMD will be consulted).
 * If 'info' is not NULL, LBSMD is consulted first (unless 'info->lb_disable'
 * is non-zero, meaning to skip LBSMD), and then DISPD is consulted
 * (using information provided) but only if mapping with LBSMD (if any)
 * failed. This scheme permits to use any combination of service mappers.
 * Note that if 'info' is not NULL then non-zero value of 'info->stateless'
 * forces 'types' to have 'fSERV_StatelessOnly' set.
 * NB: 'nbo' in comments denotes parameters coming in network byte order.
 */

/* Open iterator and consult either local database (if present),
 * or network database, using all default communication parameters
 * found in registry and environment variables (implicit parameter
 * 'info', found in two subsequent variations of this call, is filled out
 * by ConnNetInfo_Create(service) and then used automatically).
 */
SERV_ITER SERV_OpenSimple
(const char*         service        /* service name                          */
 );


/* Special values for preferred_host parameter */
#define SERV_LOCALHOST  ((unsigned int)(~0UL))
#define SERV_ANYHOST    0           /* default, may be used as just 0 in code*/


SERV_ITER SERV_OpenEx
(const char*         service,       /* service name                          */
 TSERV_Type          types,         /* mask of type(s) of servers requested  */
 unsigned int        preferred_host,/* preferred host to use service on, nbo */
 const SConnNetInfo* info,          /* connection information                */
 const SSERV_Info    *const skip[], /* array of servers NOT to select        */
 size_t              n_skip         /* number of servers in preceding array  */
 );

#define SERV_Open(service, types, preferred_host, info) \
        SERV_OpenEx(service, types, preferred_host, info, 0, 0)


/* Get the next server meta-address, optionally accompanied by a host
 * environment, specified in LBSMD configuration file on that host.
 * Return 0 if no more servers were found for the service requested.
 * Only when completing successfully, i.e. returning non-NULL info,
 * this function can also provide the host environment as follows:
 * when 'env' parameter is passed as a non-NULL pointer, then a copy of
 * the host environment is allocated, and pointer to it is stored in '*env'.
 * NULL value stored if no environment is available for the host, which
 * is referred by returned server info. Otherwise, *env remains untouched.
 * NOTE that the application program should NOT destroy returned server info:
 * it will be freed automatically upon iterator destruction. On the other hand,
 * environment has to be explicitly free()'d, when no longer needed.
 */
const SSERV_Info* SERV_GetNextInfoEx
(SERV_ITER           iter,          /* handle obtained via 'SERV_Open*' call */
 char**              env            /* ptr to copy of host envir to store in */
 );

#define SERV_GetNextInfo(iter)  SERV_GetNextInfoEx(iter, 0)


/* Penalize server returned last from SERV_GetNextInfo.
 * Return 0 if failed, 1 if successful.
 */
int/*bool*/ SERV_Penalize
(SERV_ITER           iter,          /* handle obtained via 'SERV_Open*' call */
 double              penalty        /* penalty in a range [0..100] (percents)*/
 );


/* Deallocate iterator.
 * Must be called to finish lookup process.
 */
void SERV_Close
(SERV_ITER           iter           /* handle obtained via 'SERV_Open*' call */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICE__H */
