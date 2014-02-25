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
 * @file
 *   Top-level API to resolve NCBI service names into server meta-addresses.
 *
 */

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_host_info.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */

/** Revision 6.270 */
#define SERV_CLIENT_REVISION_MAJOR  6
#define SERV_CLIENT_REVISION_MINOR  270

/** Special values for the "preferred_host" parameter.
 * @sa
 *  SERV_OpenEx, SERV_GetInfoEx
 */
#define SERV_LOCALHOST  ((unsigned int) ~0UL)
#define SERV_ANYHOST    0


#ifdef __cplusplus
extern "C" {
#endif


/* Fwdecl of an opaque type */
struct SSERV_IterTag;
/** Iterator through the servers */
typedef struct SSERV_IterTag* SERV_ITER;


/** Simplified (uncluttered) SSERV_Info pointer type */
typedef const SSERV_Info* SSERV_InfoCPtr;


/** Special "type" bit values that may be combined with server types.
 * @note MSBs should be maintained compatible with EMGHBN_Option.
 * @sa
 *  ESERV_Type, ESERV_OpenEx, SERV_GetInfoEx
 */
enum ESERV_TypeSpecial {
    fSERV_Any               = 0,
    fSERV_All               = 0x0000FFFF,
    fSERV_Stateless         = 0x00100000,  /**< Stateless servers only    */
    fSERV_Reserved          = 0x00200000,  /**< Reserved, MBZ             */
    fSERV_ReverseDns        = 0x00800000,  /**< LB-DNS translation        */
    /* The following allow to get currently inactive service instances    */
    fSERV_IncludeDown       = 0x08000000,
    fSERV_IncludeStandby    = 0x10000000,
    fSERV_IncludeReserved   = 0x20000000,  /**< @note Not yet implemented */
    fSERV_IncludePenalized  = 0x40000000,
    fSERV_IncludeSuppressed = 0x70000000,
    fSERV_Promiscuous       = 0x78000000
};
typedef unsigned int   TSERV_Type;      /**<Bitwise OR of ESERV_Type[Special]*/
typedef unsigned short TSERV_TypeOnly;  /**<Server type only, w/o specials   */


/** Create an iterator for sequential server lookup.
 * @note 'nbo' in comments denotes parameters coming in network byte order;
 *       'hbo' stands for 'host byte order'.
 * @param service
 *  A service name, may not be NULL or empty.
 * @param types
 *  A bitset of type(s) of servers requested.
 * @param preferred_host
 *  Preferred host to use the service at, nbo.
 * @param net_info
 *  Connection information (NULL prevents use of the network dispatching).
 * @note If "net_info" is NULL, only LOCAL/LBSMD mappers will be consulted.
 *       If "net_info" is not NULL, LOCAL/LBSMD are consulted first,
 *       and then DISPD is consulted last (using the connection information
 *       provided) but only if mapping with LOCAL/LBSMD (if any occurred) has
 *       failed.
 * @note The registry section [CONN], keys LOCAL_DISABLE, LBSMD_DISABLE, and
 *       DISPD_DISABLE (which can be overridden by the environment variables
 *       CONN_LOCAL_DISABLE, CONN_LBSMD_DISABLE, and CONN_DISPD_DISABLE,
 *       respectively) can be used to skip the corresponding service mappers.
 *       This scheme permits to use any combination of the service mappers
 *       (local/lbsmd/network-based).
 * @note If "net_info" is not NULL then a non-zero value of
 *       "net_info->stateless" forces "types" to get the "fSERV_StatelessOnly"
 *       bit set implicitly.
 * @param skip
 *  An array of servers NOT to select: contains server-info elements that are
 *  not to return from the search (whose server-infos match the would-be
 *  result).
 * @note However, special additional rules apply to the "skip" elements when
 *       the fSERV_ReverseDns bit is set in the "types":  the result-to-be is
 *       not considered if either
 *    1. There is an fSERV_Dns type entry found in "skip" that matches
 *       host[:port] (any port if 0); or
 *    2. The reverse lookup of the host:port turns up an fSERV_Dns type server
 *       whose name matches an fSERV_Dns type server from "skip".
 * @param n_skip
 *  Number of entries in the "skip" array.
 * @return
 *  Non-NULL iterator, or NULL if the service does not exist.
 * @note
 *  Non-NULL iterator does not guarantee the service operational, it merely
 *  acknowledges the service existence.
 * @sa
 *  SERV_GetNextInfoEx, SERV_Reset, SERV_Close, ConnNetInfo_Create
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenEx
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info,
 SSERV_InfoCPtr       skip[], 
 size_t               n_skip
 );

/** Same as "SERV_OpenEx(., ., ., ., 0, 0)" -- i.e. w/o the "skip" array.
 * @sa
 *  SERV_OpenEx
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_Open
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
);


/** Allocate an iterator and consult either local databases (if any present),
 * or network database, using all default communication parameters found both
 * in the registry and the environment variables (as if having an implicit
 * parameter "net_info" created with "ConnNetInfo_Create(service)").
 * @note No preferred host is set in the target iterator.
 * @param service
 *  A service name (may not be NULL or empty).
 * @return
 *  Non-NULL iterator, or NULL if the service does not exist.
 * @note
 *  Non-NULL iterator does not guarantee the service operational, it merely
 *  acknowledges the service existence.
 * @sa
 *  SERV_GetNextInfoEx, SERV_OpenEx, SERV_Reset, SERV_Close, ConnNetInfo_Create
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenSimple
(const char*          service
 );


/** Get the next server meta-address, optionally accompanied by the host
 * parameters made available by the LB daemon (LBSMD).
 * @param iter
 *  An iterator handle obtained via a "SERV_Open*" call.
 * @note NULL is accepted, and results in NULL returned.
 * @param host_info
 *  An optional pointer to store host info at (may be NULL).
 * @return
 *  NULL if no more servers have been found for the service requested (the
 *  pointer to "host_info" remains untouched in this case);  otherwise, a
 *  non-NULL pointer to the server meta address.
 * @note The returned server-info is valid only until either of the two events:
 *       1. SERV_GetNextInfo[Ex]() is called with this iterator again; or
 *       2. The iterator reset / closed (SERV_Reset() / SERV_Close() called).
 * @note Application program should NOT destroy the returned server-info as it
 *       is managed automatically by the iterator.
 * @note Resulting DNS-type server-info (only if coming out for the first time)
 *       may contain 0 in the host field to denote that the name exists but the
 *       service is currently not serving (down / unavailable).
 * @note Only when completing successfully, i.e. returning a non-NULL info,
 *       this call can also provide the host information as follows: if
 *       "host_info" parameter is passed as a non-NULL pointer, then a copy of
 *       the host information is allocated, and the handle is stored at
 *       "*host_info".  Using this handle, various host parameters like load,
 *       host environment, number of CPUs can be retrieved
 *       (see ncbi_host_info.h).  The returned host information handle has to
 *       be explicitly free()'d when no longer needed.
 * @sa
 *  SERV_Reset, SERV_Close, SERV_GetInfoEx, ncbi_host_info.h
 */
extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfoEx
(SERV_ITER            iter,
 HOST_INFO*           host_info
 );

/** Same as "SERV_GetNextInfoEx(., 0)" -- i.e. w/o host information.
 * @sa
 *  SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfo
(SERV_ITER            iter
 );


/** A "fast track" routine equivalent to creating of an iterator as with
 * SERV_OpenEx(), and then taking an info as with SERV_GetNextInfoEx().
 * However, this call is optimized for an application, which only needs
 * a single entry (the first one), and which is not interested in iterating
 * over all available instances.  Both the returned server-info and host
 * information have to be explicitly free()'d by the application when no
 * longer needed.
 * @param service
 *  A service name (may not be NULL or empty).
 * @param types
 *  A bitset of type(s) of servers requested.
 * @param preferred_host
 *  Preferred host to use the service at, nbo.
 * @param net_info
 *  Connection information (NULL disables network dispatching).
 * @param skip[]
 *  An array of servers NOT to select, see SERV_OpenEx() for notes.
 * @param n_skip
 *  Number of entries in the "skip" array.
 * @param host_info
 *  An optional pointer to store host info at (may be NULL).
 * @note The host information is provided only (if at all) if this call returns
 *       a non-NULL result, see SERV_OpenEx() for notes.
 * @return
 *  First matching server-info (non-NULL), or NULL if no instances found.
 * @sa
 *  SERV_GetInfo, SERV_OpenEx
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoEx
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info,
 SSERV_InfoCPtr       skip[],
 size_t               n_skip,
 HOST_INFO*           host_info
 );

/** Same as "SERV_GetInfoEx(., ., ., ., 0, 0, 0)" -- i.e. w/o the "skip" array,
 * and w/o "host_info".
 * @sa
 *  SERV_GetInfoEx
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfo
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
 );


/** Penalize the server returned last from SERV_GetNextInfo[Ex]().
 * @param iter
 *  An iterator handle obtained via a "SERV_Open*" call.
 * @param fine
 *  A fine value in the range [0=min..100=max] (%%), inclusive.
 * @return
 *  Return 0 if failed, non-zero if successful.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Penalize
(SERV_ITER            iter,
 double               fine
 );


/** Rerate the server returned last from SERV_GetNextInfo[Ex]().
 * @note This is an experimental API.
 * @param iter
 *  An iterator handle obtained via a "SERV_Open*" call.
 * @param rate
 *  A new rate value, or 0.0 to turn the server off, or
 *  fabs(rate) >= LBSM_RERATE_DEFAULT to revert to the default.
 * @return
 *  Return 0 if failed, non-zero if successful.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Rerate
(SERV_ITER            iter,
 double               rate
 );


/** Reset the iterator to the state as if it has just been opened.
 * @warning Invalidates all previosuly issued server descriptors (SSERV_Info*).
 * @param iter
 *  An iterator handle obtained via a "SERV_Open*" call.
 * @note NULL is accepted, and causes no actions.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx, SERV_Close
 */
extern NCBI_XCONNECT_EXPORT void SERV_Reset
(SERV_ITER            iter           /* handle obtained via 'SERV_Open*' call*/
 );


/** Deallocate the iterator.  Must be called to finish the lookup process.
 * @warning Invalidates all previosuly issued server descriptors (SSERV_Info*).
 * @param iter
 *  An iterator handle obtained via a "SERV_Open*" call.
 * @note NULL is accepted, and causes no actions.
 * @sa
 *  SERV_OpenEx, SERV_Reset
 */
extern NCBI_XCONNECT_EXPORT void SERV_Close
(SERV_ITER            iter
 );


/** Obtain port number that corresponds to the named (standalone) service
 * declared at the specified host (per the LB configuration information).
 * @param name
 *  Service name (of type fSERV_Standalone) to look up.
 * @param host
 *  Host address (or SERV_LOCALHOST, or 0, same) to look the service up at.
 * @return
 *  The port number or 0 on error (no suitable service found).
 * @note The call returns the first match, and does not check whether an
 *       application is already running at the returned port (i.e. regardless
 *       of whether or not the service is currently up).
 * @sa
 *  ESERV_Type, SERV_OpenEx, LSOCK_CreateEx
 */
extern NCBI_XCONNECT_EXPORT unsigned short SERV_ServerPort
(const char*          name,
 unsigned int         host
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SERVICE__H */
