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
 *
 */

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_host_info.h>


/* Revision 6.220 */
#define SERV_CLIENT_REVISION_MAJOR  6
#define SERV_CLIENT_REVISION_MINOR  220


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


/* Create an iterator for sequential server lookup.
 * Connection information 'net_info' can be a NULL pointer, which means not to
 * make any network connections (only LOCAL/LBSMD mappers will be consulted).
 * If 'net_info' is not NULL, LOCAL/LBSMD are consulted first, and then DISPD
 * is consulted last (using the connection information provided) but only if
 * mapping with LOCAL/LBSMD (if any occurred) has failed.  Registry
 * section [CONN], keys LOCAL_DISABLE, LBSMD_DISABLE, and DISPD_DISABLE
 * (which can be overridden from the environment variables CONN_LOCAL_DISABLE,
 * CONN_LBSMD_DISABLE, and CONN_DISPD_DISABLE, respectively) can be used to
 * skip respective service mappers.  This scheme permits to use any
 * combination of the service mappers (local/lbsmd/network-based).
 * Note that if 'net_info' is not NULL then non-zero value of
 * 'net_info->stateless' forces 'types' to have the 'fSERV_StatelessOnly'
 * bit set implicitly.
 * NB: 'nbo' in comments denotes parameters coming in network byte order;
 *     'hbo' stands for 'host byte order'.
 */

/* Allocate an iterator and consult either local database (if present),
 * or network database, using all default communication parameters
 * found both in registry and environment variables (here the implicit
 * parameter 'net_info', which has to be explicitly passed in the two
 * subsequent variations of this call, is filled out internally by
 * ConnNetInfo_Create(service), and then automatically used).
 * NOTE that no preferred host (0) is set in the resultant iterator.
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenSimple
(const char*          service        /* service name                         */
 );

/* Special values for 'preferred_host' parameter */
#define SERV_LOCALHOST ((unsigned int)(~0UL))
#define SERV_ANYHOST   0             /* default, may be used as just 0       */

/* Special "type" bit values that may be combined with server types */
typedef enum {
    /* Allows to get even dead services (but not the off ones!)
     * NB: all preference params are ignored */
    fSERV_IncludeSuppressed = 0x40000000,
    fSERV_IncludeDead       = 0x20000000,
    fSERV_Promiscuous       = 0x60000000,
    /* Do reverse DNS translation of the would-be resulting info */
    fSERV_ReverseDns        = 0x01000000,
    /* Only stateless servers should be returned */
    fSERV_Stateless         = 0x00100000,
    fSERV_All               = 0x0000FFFF,
    fSERV_Any               = 0
} ESERV_SpecialType;
typedef unsigned int TSERV_Type;     /* Bitwise OR of ESERV_[Special]Type    */

/* Simplified (uncluttered) type to use in 'skip' parameter below */
typedef const SSERV_Info* SSERV_InfoCPtr;

extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenEx
(const char*          service,       /* service name                         */
 TSERV_Type           types,         /* mask of type(s) of servers requested */
 unsigned int         preferred_host,/* preferred host to use service on, nbo*/
 const SConnNetInfo*  net_info,      /* connection information               */
 const SSERV_InfoCPtr skip[],        /* array of servers NOT to select       */
 size_t               n_skip         /* number of servers in preceding array */
 );

extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_Open
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
);


/* Get the next server meta-address, optionally accompanied by host
 * environment, specified in LBSMD configuration file on that host.
 * Return 0 if no more servers have been found for the service requested
 * (the pointer to 'host_info' remains untouched in this case).
 * Only when completing successfully, i.e. returning a non-NULL info,
 * this function can also provide the host information as follows: if
 * 'host_info' parameter is passed as a non-NULL pointer, then a copy of the
 * host information is allocated, and the pointer is stored at *host_info.
 * Using this information, various host parameters like load, host
 * environment, number of CPUs can be retrieved (see ncbi_host_info.h).
 * Resulting DNS server info (only if coming out for the first time) may
 * contain 0 in the host field to denote that the name is known but
 * is currently down.
 * NOTE:  Application program should NOT destroy the returned server info:
 *        it will be freed automatically upon iterator destruction.
 *        On the other hand, returned host information has to be
 *        explicitly free()'d when no longer needed.
 * NOTE:  Returned server info is valid only until either of the two events:
 *        1) SERV_GetNextInfo[Ex] is called for the same iterator again;
 *        2) iterator closed (SERV_Close() called).
 */
extern NCBI_XCONNECT_EXPORT const SSERV_Info* SERV_GetNextInfoEx
(SERV_ITER            iter,          /* handle obtained via 'SERV_Open*' call*/
 HOST_INFO*           host_info      /* ptr to store host info at [may be 0] */
 );

extern NCBI_XCONNECT_EXPORT const SSERV_Info* SERV_GetNextInfo
(SERV_ITER            iter
 );


/* This is a "fast track" routine equivalent to creation of an iterator
 * as with SERV_OpenEx() and then taking an info as with SERV_GetNextInfoEx().
 * However, this call is optimized for an application, which only needs
 * a single entry (the first one), and which is not interested in iterating
 * over all available entries.  Both returned server info and environment have
 * to be explicitly free()'d by the application when no longer needed.
 * Note that the host environment is provided only (if at all) if the function
 * returns a non-NULL result.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoEx
(const char*          service,       /* service name                         */
 TSERV_Type           types,         /* mask of type(s) of servers requested */
 unsigned int         preferred_host,/* preferred host to use service on, nbo*/
 const SConnNetInfo*  net_info,      /* connection information               */
 const SSERV_InfoCPtr skip[],        /* array of servers NOT to select       */
 size_t               n_skip,        /* number of servers in preceding array */
 HOST_INFO*           host_info      /* ptr to store host info at [may be 0] */
 );

extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfo
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
 );


/* Penalize the server returned last from SERV_GetNextInfo[Ex]().
 * Return 0 if failed, 1 if successful.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Penalize
(SERV_ITER            iter,          /* handle obtained via 'SERV_Open*' call*/
 double               fine           /* fine from range [0=min..100=max] (%%)*/
 );


/* Reset the iterator to the state as if it has just been opened.
 * CAUTION:  All pointers to server descriptors (SSERV_Info*), if any
 * previously obtained via this iterator, are rendered invalid by this call.
 */
extern NCBI_XCONNECT_EXPORT void SERV_Reset
(SERV_ITER            iter           /* handle obtained via 'SERV_Open*' call*/
 );


/* Deallocate the iterator.  Must be called to finish the lookup process.
 */
extern NCBI_XCONNECT_EXPORT void SERV_Close
(SERV_ITER            iter           /* handle obtained via 'SERV_Open*' call*/
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.49  2006/07/06 18:48:40  lavr
 * Some fixes in comments
 *
 * Revision 6.48  2006/06/07 20:23:07  lavr
 * Bump up minor client version number to 220 (full now is 6.220)
 *
 * Revision 6.47  2006/04/20 19:27:18  lavr
 * More comments for SERV_Open*() family of calls
 *
 * Revision 6.46  2006/03/06 20:24:44  lavr
 * Comments
 *
 * Revision 6.45  2006/01/03 19:57:59  lavr
 * Bump client revision to 6.210
 *
 * Revision 6.44  2005/12/23 18:07:34  lavr
 * Special service flags moved here, values reassigned
 *
 * Revision 6.43  2005/12/14 22:03:53  lavr
 * ESERV_SpecialType reinstated public
 *
 * Revision 6.42  2005/12/14 21:17:09  lavr
 * ESERV_SpecialType made private (moved to ncbi_servicep.h in source tree)
 * SSERV_InfoCPtr introduced;  some API usage comments extended/corrected
 *
 * Revision 6.41  2005/10/03 15:55:22  lavr
 * Change ESERV_SpecialType::fSERV_Promiscuous to fit "signed int"
 *
 * Revision 6.40  2005/09/02 14:28:18  lavr
 * Bump client revision number to 6.200 for use with patched PubFetch
 *
 * Revision 6.39  2005/07/06 18:54:44  lavr
 * +enum ESERV_SpecialType to hold special server type bits (instead of macros)
 *
 * Revision 6.38  2005/07/06 18:27:58  lavr
 * Eliminate macro calls
 *
 * Revision 6.37  2005/05/04 16:13:57  lavr
 * DISP_SetMessageHook() moved to <connect/ncbi_service_misc.h>
 *
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
