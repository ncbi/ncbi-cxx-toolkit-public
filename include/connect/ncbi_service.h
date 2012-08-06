#ifndef CONNECT___NCBI_SERVICE__H
#define CONNECT___NCBI_SERVICE__H

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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Top-level API to resolve NCBI service name to the server meta-address.
 *
 */

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_host_info.h>


/* Revision 6.260 */
#define SERV_CLIENT_REVISION_MAJOR  6
#define SERV_CLIENT_REVISION_MINOR  260


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

/* Special "type" bit values that may be combined with server types.
 * NB:  Also, MSBs should be kept compatible with EMGHBN_Option */
enum ESpecialType {
    fSERV_Any               = 0,
    fSERV_All               = 0x0000FFFF,
    /* Only stateless servers should be returned */
    fSERV_Stateless         = 0x00100000,
    fSERV_Reserved          = 0x00200000, /* Reserved, MBZ */
    /* Do reverse DNS translation of the would-be resulting info */
    fSERV_ReverseDns        = 0x00800000,
    /* Allows to get even down services (but not the off ones!)
     * NB: most flex preference params are ignored */
    fSERV_IncludeReserved   = 0x10000000, /* w/local LBSMD only */
    fSERV_IncludeDown       = 0x20000000,
    fSERV_IncludeSuppressed = 0x40000000,
    fSERV_Promiscuous       = 0x60000000
};
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
extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfoEx
(SERV_ITER            iter,          /* handle obtained via 'SERV_Open*' call*/
 HOST_INFO*           host_info      /* ptr to store host info at [may be 0] */
 );

extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfo
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


/* Rerate the server returned last from SERV_GetNextInfo[Ex]().
 * Return 0 if failed, 1 if successful.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Rerate
(SERV_ITER            iter,          /* handle obtained via 'SERV_Open*' call*/
 double               rate           /* new rate, 0 to off, <0 to set default*/
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


/* Obtain port number that corresponds to the named (standalone) service
 * declared on the specified host (per LB configuration information).
 * @param name
 *   service name (of type fSERV_Standalone) to look up
 * @param host
 *   host address (or SERV_LOCALHOST, or 0, same) to look the service up on
 * @return
 *   the port number or 0 on error (no suitable service found)
 * Note that the call returns the first match, and does not check
 * whether an application is already running on the returned port
 * (i.e. regradless of whether or not the service is currently up).
 * @sa
 *   SERV_Open, LSOCK_CreateEx
 */
extern NCBI_XCONNECT_EXPORT unsigned short SERV_ServerPort
(const char*  name,
 unsigned int host
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SERVICE__H */
