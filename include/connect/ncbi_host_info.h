#ifndef CONNECT___NCBI_HOST_INFO__H
#define CONNECT___NCBI_HOST_INFO__H

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
 * @file
 *   NCBI host info getters
 *
 *   Host information handle becomes available from SERV_Get[Next]InfoEx()
 *   calls of the service mapper (ncbi_service.c) and remains valid until
 *   destructed by passing into free(). All API functions declared below
 *   accept NULL as 'host_info' parameter, and as the result return a failure
 *   status as described individually for each API call.
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Fwdecl of an opaque type */
struct SHostInfoTag;
/** Handle for the user code use */
typedef struct SHostInfoTag* HOST_INFO;


/** Get the official host address.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  Official host address, or 0 if unknown.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
unsigned int HINFO_HostAddr(const HOST_INFO host_info);


/** Get CPU count (number of logical cores, hyper-threaded included).
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  CPU count, or -1 if an error occurred.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_CpuCount(const HOST_INFO host_info);


/** Get physical CPU count (number of physical cores, not packages).
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  The number of physical CPU units, 0 if the number cannot be determined,
 *  or -1 if an error occurred.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_CpuUnits(const HOST_INFO host_info);


/** Get CPU clock rate.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  CPU clock rate (in MHz), or 0 if an error occurred.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
double HINFO_CpuClock(const HOST_INFO host_info);


/** Get task count.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  Task count, or -1 if an error occurred.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_TaskCount(const HOST_INFO host_info);


/** Get memory usage data.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @param memusage
 *  Memory usage in MB (filled in upon return):
 *  - [0] = total RAM;
 *  - [1] = discardable RAM (cached);
 *  - [2] = free RAM;
 *  - [3] = total swap;
 *  - [4] = free swap.
 * @return
 *  Non-zero on success and store memory usage (MB, in the provided array
 *  "memusage"), or 0 if an error occurred.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_Memusage(const HOST_INFO host_info, double memusage[5]);


typedef enum {
    eArch_Unknown
} ENcbiArch;
typedef unsigned short TNcbiArch;


typedef enum {
    fCapacity_Unknown/* = 0 */,
    fCapacity_32     /* = 1 */, /**< 32 bits only                            */
    fCapacity_64     /* = 2 */, /**< 64 bits, but 32-bit backward compatible */
    fCapacity_32_64  /* = 2|1*/ /**< 32 bits, but 64-bit forward compatible  */
} ENcbiCapacity;
typedef unsigned short TNcbiCapacity;


/*NB: High bit determines the OS family */
typedef enum {
    fOS_Unknown,
    fOS_IRIX          = 8,
    fOS_Solaris       = 16,
    fOS_BSD           = 32,
    fOS_Darwin        = 36,
    fOS_Windows       = 64,
    fOS_WindowsServer = 96,
    fOS_Linux         = 128
} ENcbiOSType;
typedef unsigned short TNcbiOSType;


/** Host parameters */
typedef struct {
    TNcbiArch          arch;    /**< Architecture ID, see enum,    0=unknown */
    TNcbiCapacity      bits;    /**< Platform bitness, 32/64/32+64/0=unknown */
    TNcbiOSType        ostype;  /**< OS type ID, see enum,         0=unknown */
    struct {
        unsigned short major;
        unsigned short minor;
        unsigned short patch;
    } kernel;                   /**< Kernel/OS version #, if available       */
    size_t             pgsize;  /**< Hardware page size in bytes, if known   */
    TNCBI_Time         bootup;  /**< System boot time, time_t-compatible     */
    TNCBI_Time         startup; /**< LBSMD start time, time_t-compatible     */
    struct {
        unsigned short major;
        unsigned short minor;
        unsigned short patch;
    } daemon;                   /**< LBSMD daemon version                    */
    unsigned short     svcpack; /**< Kernel service pack (Hi=major, Lo=minor)*/
} SHINFO_Params;

/** Get host parameters.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @param p
 *  Host parameters to fill in upon return.
 * @return
 *  Non-zero on success, 0 on error.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_MachineParams(const HOST_INFO host_info, SHINFO_Params* p);


/** Obtain host load averages.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @param lavg
 *  Load averages to fill in upon return:
 *  - [0] = The standard 1-minute load average;
 *  - [1] = Instant (a.k.a. Blast) load average (averaged over runnable count).
 * @return
 *  Non-zero on success and store load averages in the provided array "lavg",
 *  or 0 on error.
 * @sa
 *  HINFO_Status, SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_LoadAverage(const HOST_INFO host_info, double lavg[2]);


/** Obtain LB host availability status.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @param status
 *  Status values to fill in upon return:
 *  - [0] = status based on the standard load average;
 *  - [1] = status based on the instant load average.
 * @return
 *  Non-zero on success and store host status values in the provided array
 *  "status", or 0 on error.
 * @note  Status may get returned as 0.0 if either the host does not provide
 *        such information, or if the host is overloaded (unavailable).
 * @sa
 *  HINFO_LoadAverage, SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_Status(const HOST_INFO host_info, double status[2]);


/** Obtain and return LB host environment.
 * LB host environment is a sequence of lines (separated by \\n), all having
 * form of "name=value", which is provided to and stored by the Load-Balancing
 * and Service Mapping Daemon (LBSMD) in the configuration file on that host.
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  NULL if the host environment cannot be obtained (or does not exist);
 *  otherwise, a non-NULL pointer to a '\0'-terminated string that contains
 *  the environment, which remains valid until the handle "host_info" gets
 *  free()'d by the application.
 * @sa
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
const char* HINFO_Environment(const HOST_INFO host_info);


/** Obtain the affinity argument that has keyed the service selection (if
 * argument affinities have been used at all).
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  NULL if no affinity has been found/used (in this case
 *  HINFO_AffinityArgvalue() would also return NULL);  otherwise, a non-NULL
 *  pointer to a '\0'-terminated string that remains valid until the handle
 *  "host_info" gets free()'d by the application.
 * @sa
 *  HINFO_AffinityArgvalue, SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
const char* HINFO_AffinityArgument(const HOST_INFO host_info);

/** Obtain the affinity argument's value that has keyed the service selection
 * (if argument affinities have been used at all).
 * @param host_info
 *  HOST_INFO as returned by the SERV API.
 * @return
 *  NULL if there was no particular value matched but the argument (as returned
 *  by HINFO_AffinityArgument()) played alone;  "" if the value has been used
 *  empty, or any other substring from the host environment that keyed the
 *  selection decision.  The non-NULL pointer remains valid until the handle
 *  "host_info" gets free()'d by the application.
 * @sa
 *  HINFO_AffinityArgument, HINFO_Environment,
 *  SERV_GetInfoEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT
const char* HINFO_AffinityArgvalue(const HOST_INFO host_info);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_HOST_INFO__H */
