#ifndef CONNECT___NCBI_SERVICE__H
#define CONNECT___NCBI_SERVICE__H

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
 *   More elaborate documentation could be found in:
 *   http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/libs/conn.html#ref_ServiceAPI
 *
 */

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_host_info.h>


/* Revision 6.100 */
#define SERV_CLIENT_REVISION_MAJOR  6
#define SERV_CLIENT_REVISION_MINOR  101


/** @addtogroup ServiceSupport
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Iterator through the servers
 */
struct SSERV_IterTag;
typedef struct SSERV_IterTag* SERV_ITER;


/* Create an iterator for the iterative server lookup.
 * Connection information 'net_info' can be a NULL pointer, which means
 * not to make any network connections (only LBSMD will be consulted). If
 * 'net_info' is not NULL, LBSMD is consulted first (unless 'net_info->lb_disable'
 * is non-zero, meaning to skip LBSMD), and then DISPD is consulted (using
 * the information provided) but only if mapping with LBSMD (if any)
 * has failed. This scheme permits to use any combination of service mappers.
 * Note that if 'info' is not NULL then non-zero value of 'info->stateless'
 * forces 'types' to have 'fSERV_StatelessOnly' set.
 * NB: 'nbo' in comments denotes parameters coming in network byte order.
 */

/* Allocate an iterator and consult either local database (if present),
 * or network database, using all default communication parameters
 * found in registry and environment variables (implicit parameter
 * 'info' found in two subsequent variations of this call is filled out
 * internally by ConnNetInfo_Create(service) and then automatically used).
 * NOTE that no preferred host (0) is set in the resultant iterator.
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenSimple
(const char*         service        /* service name                          */
 );


/* Special values for preferred_host parameter */
#define SERV_LOCALHOST  ((unsigned int)(~0UL))
#define SERV_ANYHOST    0           /* default, may be used as just 0 in code*/

/* Can be combined in types to get even dead services (not off ones!) */
#define fSERV_Promiscuous 0x80000000
/* Do reverse DNS translation of the resulting info */
#define fSERV_ReverseDns  0x40000000


extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenEx
(const char*         service,       /* service name                          */
 TSERV_Type          types,         /* mask of type(s) of servers requested  */
 unsigned int        preferred_host,/* preferred host to use service on, nbo */
 const SConnNetInfo* net_info,      /* connection information                */
 const SSERV_Info*   const skip[],  /* array of servers NOT to select        */
 size_t              n_skip         /* number of servers in preceding array  */
 );

#define SERV_Open(service, types, preferred_host, net_info) \
        SERV_OpenEx(service, types, preferred_host, net_info, 0, 0)


/* Get the next server meta-address, optionally accompanied by host
 * environment, specified in LBSMD configuration file on that host.
 * Return 0 if no more servers were found for the service requested
 * (parameter 'host_info' remains untouched in this case).
 * Only when completing successfully, i.e. returning non-NULL info,
 * this function can also provide the host information as follows: if
 * 'host_info' parameter is passed as a non-NULL pointer, then a copy of the
 * host information is allocated, and pointer to it is stored in 'host_info'.
 * Using this information, various host parameters like load, host
 * environment, number of CPUs can be retrieved (see ncbi_host_info.h).
 * NOTE:  Application program should NOT destroy returned server info:
 *        it will be freed automatically upon iterator destruction.
 *        On the other hand, host information has to be explicitly free()'d
 *        when no longer needed.
 * NOTE:  Returned server info is valid only until either of the two events:
 *        1) SERV_GetNextInfo[Ex] is called for the same iterator again;
 *        2) iterator closed (SERV_Close() called).
 */
extern NCBI_XCONNECT_EXPORT const SSERV_Info* SERV_GetNextInfoEx
(SERV_ITER           iter,          /* handle obtained via 'SERV_Open*' call */
 HOST_INFO*          host_info      /* ptr to store host info to [may be 0]  */
 );

#define SERV_GetNextInfo(iter)  SERV_GetNextInfoEx(iter, 0)


/* This is a 'fast track' routine equivalent to creation of an iterator
 * as with SERV_OpenEx() and then taking an info as with SERV_GetNextInfoEx().
 * However, this call is optimized for an application, which only needs
 * a single entry (the first one), and which is not interested in iterating
 * over all available entries. Both returned server info and env have to be
 * explicitly free()'d by the application when no longer needed.
 * Note that env is only supplied if the function returns a non-NULL result.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoEx
(const char*         service,       /* service name                          */
 TSERV_Type          types,         /* mask of type(s) of servers requested  */
 unsigned int        preferred_host,/* preferred host to use service on, nbo */
 const SConnNetInfo* net_info,      /* connection information                */
 const SSERV_Info*   const skip[],  /* array of servers NOT to select        */
 size_t              n_skip,        /* number of servers in preceding array  */
 HOST_INFO*          host_info      /* ptr to store host info to [may be 0]  */
 );


#define SERV_GetInfo(service, types, preferred_host, net_info) \
    SERV_GetInfoEx(service, types, preferred_host, net_info, 0, 0, 0)


/* Penalize server returned last from SERV_GetNextInfo[Ex]().
 * Return 0 if failed, 1 if successful.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Penalize
(SERV_ITER           iter,          /* handle obtained via 'SERV_Open*' call */
 double              fine           /* fine in a range [0=min..100=max] (%%) */
 );


/* Reset the iterator to the state as if it'd just been opened.
 * Caution: All previously obtained with this iterator pointers (if any)
 * to server descriptors (SSERV_Info*) become invalid.
 */
extern NCBI_XCONNECT_EXPORT void SERV_Reset
(SERV_ITER           iter           /* handle obtained via 'SERV_Open*' call */
 );


/* Deallocate the iterator. Must be called to finish lookup process.
 */
extern NCBI_XCONNECT_EXPORT void SERV_Close
(SERV_ITER           iter           /* handle obtained via 'SERV_Open*' call */
 );


/* Set message hook procedure for messages originating from NCBI network
 * dispatcher (if used).  Any hook will be called not more than once.
 * If no hook is installed, warning message will be generated in the
 * standard log file, not more than once.
 */

typedef void (*FDISP_MessageHook)(const char* message);

extern NCBI_XCONNECT_EXPORT void DISP_SetMessageHook(FDISP_MessageHook);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.36  2005/03/05 21:04:11  lavr
 * +fSERV_ReverseDns
 *
 * Revision 6.35  2005/01/31 17:08:35  lavr
 * info -> net_info where appropriate for consistency
 *
 * Revision 6.34  2004/08/19 15:26:54  lavr
 * +fSERV_Promiscuous
 *
 * Revision 6.33  2004/06/14 16:36:13  lavr
 * Client minor version number incremented
 *
 * Revision 6.32  2004/01/30 14:37:33  lavr
 * Client revision made independent of CVS revisions
 *
 * Revision 6.31  2003/08/11 19:05:54  lavr
 * +DISP_SetMessageHook()
 *
 * Revision 6.30  2003/06/12 13:20:59  lavr
 * Added notes for SERV_GetNextInfoEx()
 *
 * Revision 6.29  2003/04/09 19:05:50  siyan
 * Added doxygen support
 *
 * Revision 6.28  2003/01/08 01:59:33  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.27  2002/10/28 20:12:20  lavr
 * +ncbi_host_info.h
 *
 * Revision 6.26  2002/09/19 18:05:00  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.25  2002/05/06 19:08:35  lavr
 * Removed mention of IP substitutions (mistakenly left behind a while ago)
 *
 * Revision 6.24  2002/04/13 06:35:10  lavr
 * Fast track routine SERV_GetInfoEx(), many syscalls optimizations
 *
 * Revision 6.23  2001/09/26 16:55:10  lavr
 * Revision log updated; in-line documentation corrected
 *
 * Revision 6.22  2001/09/24 20:22:59  lavr
 * Added function: SERV_Reset()
 *
 * Revision 6.21  2001/09/10 21:18:35  lavr
 * SERV_GetNextInfoEx(): Description of IP address subst in host environment
 *
 * Revision 6.20  2001/08/20 21:57:49  lavr
 * Parameter change for clarity: info -> net_info if type is SConnNetInfo
 *
 * Revision 6.19  2001/07/18 17:39:56  lavr
 * Comment added for SERV_OpenSimple() to note about absence of preferred host
 *
 * Revision 6.18  2001/06/25 15:32:06  lavr
 * Added function: SERV_GetNextInfoEx()
 * Both SERV_Open() and SERV_GetNextInfo() made macros for faster access
 *
 * Revision 6.17  2001/06/11 22:14:44  lavr
 * Include files adjusted
 *
 * Revision 6.16  2001/06/04 17:00:10  lavr
 * Include files adjusted
 *
 * Revision 6.15  2001/04/26 14:18:20  lavr
 * SERV_MapperName() moved to a private header
 *
 * Revision 6.14  2001/04/24 21:15:35  lavr
 * Added functions: SERV_MapperName(), SERV_Penalize()
 *
 * Revision 6.13  2001/03/02 20:05:56  lavr
 * SERV_LOCALHOST added; SERV_OpenSimple() made more documented
 *
 * Revision 6.12  2001/02/09 17:32:52  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 *
 * Revision 6.11  2001/01/08 22:48:00  lavr
 * Double return 0 in GetNextInfo() removed:
 * 0 now indicates an error unconditionally
 *
 * Revision 6.10  2000/12/29 17:40:30  lavr
 * Pretty printed; Double 0 return added to SERV_GetNextInfo()
 *
 * Revision 6.9  2000/12/06 22:17:02  lavr
 * Binary host addresses are now explicitly stated to be in network byte
 * order, whereas binary port addresses now use native (host) representation
 *
 * Revision 6.8  2000/10/20 17:03:49  lavr
 * Added 'const' to SConnNetInfo in SERV_OpenEx()
 *
 * Revision 6.7  2000/10/05 22:40:06  lavr
 * Additional parameter 'info' in a call to SERV_Open()
 *
 * Revision 6.6  2000/10/05 21:26:30  lavr
 * Log message edited
 *
 * Revision 6.5  2000/10/05 21:10:11  lavr
 * Parameters to SERV_Open() and SERV_OpenEx() changed
 *
 * Revision 6.4  2000/05/31 23:12:17  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.3  2000/05/22 16:53:07  lavr
 * Rename service_info -> server_info everywhere
 * (including file names) as the latter name is more relevant
 *
 * Revision 6.2  2000/05/12 18:29:22  lavr
 * First working revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_SERVICE__H */
