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


struct SHostInfoTag;  /*forward declaration of an opaque private structure*/
typedef struct SHostInfoTag* HOST_INFO; /*handle for the user code use*/


/* Return official host address or 0 if unknown.
 */
extern NCBI_XCONNECT_EXPORT
unsigned int HINFO_HostAddr(const HOST_INFO host_info);


/* Return CPU count or -1 if an error occurred.
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_CpuCount(const HOST_INFO host_info);


/* Return number of actual CPU units, 0 if the number cannot
 * be determined, or -1 if an error occurred.
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_CpuUnits(const HOST_INFO host_info);


/* Return CPU clock rate (in MHz) or 0 if an error occurred.
 */
extern NCBI_XCONNECT_EXPORT
double HINFO_CpuClock(const HOST_INFO host_info);


/* Return task count or -1 if an error occurred.
 */
extern NCBI_XCONNECT_EXPORT
int HINFO_TaskCount(const HOST_INFO host_info);


/* Return non-zero on success and store memory usage (in MB
 * at the provided array "memusage" in the following layout:
 * index 0 = total RAM, MB;
 * index 1 = discardable RAM (cached), MB;
 * index 2 = free RAM, MB;
 * index 3 = total swap, MB;
 * index 4 = free swap, MB.
 * Return 0 if an error occurred.
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_Memusage(const HOST_INFO host_info, double memusage[5]);


typedef struct {
    unsigned int       arch;    /* Architecture ID, 0=unknown */
    unsigned int       ostype;  /* OS type ID,      0=unknown */
    struct {
        unsigned short major;
        unsigned short minor;
        unsigned short patch;
    } kernel;                   /* Kernel/OS version #, if available         */
    unsigned short     bits;    /* Platform bitness, 32/64/0=unknown         */
    size_t             pgsize;  /* Hardware page size in bytes, if available */
    TNCBI_Time         bootup;  /* System boot time, time_t-compatible       */
    TNCBI_Time         startup; /* LBSMD start time, time_t-compatible       */
    struct {
        unsigned short major;
        unsigned short minor;
        unsigned short patch;
    } daemon;                   /* LBSMD daemon version                      */
    unsigned short     svcpack; /* Kernel service pack (Hi=major, Lo=minor)  */
} SHINFO_Params;

extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_MachineParams(const HOST_INFO host_info, SHINFO_Params* p);


/* Return non-zero on success and store load averages in the
 * provided array "lavg", with the standard load average for last
 * minute stored at index [0], and instant load average
 * (aka BLAST) stored at index [1].  Return 0 on error.
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_LoadAverage(const HOST_INFO host_info, double lavg[2]);


/* Return non-zero on success and store host status coefficients in
 * the provided array "status", with status based on the standard
 * load average stored at index [0], and that based on instant load
 * average stored at index [1]. Status may return as 0 if the host
 * does not provide such information.  Return 0 on error.
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ HINFO_Status(const HOST_INFO host_info, double status[2]);


/* Obtain and return host environment.  The host environment is the
 * sequence of lines (separated by \n), all having the form "name=value",
 * which are provided to load-balancing service mapping daemon (LBSMD)
 * in the configuration file on that host.  Return 0 if the host
 * environment cannot be obtained.  If completed successfully, the
 * host environment remains valid until the handle 'host_info' deleted
 * in the application program.
 */
extern NCBI_XCONNECT_EXPORT
const char* HINFO_Environment(const HOST_INFO host_info);


/* Obtain affinity argument and value that has keyed the service
 * selection (if affinities have been used at all).  NULL gets returned
 * as argument if no affinity has been found (in this case the value
 * will be returned NULL as well).  Otherwise, NULL gets returned as
 * the value if there was no particular value matched but the argument
 * played alone; "" is the value has been used empty, or any other
 * substring from the host environment that keyed the selection decision.
 */
extern NCBI_XCONNECT_EXPORT
const char* HINFO_AffinityArgument(const HOST_INFO host_info);

extern NCBI_XCONNECT_EXPORT
const char* HINFO_AffinityArgvalue(const HOST_INFO host_info);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_HOST_INFO__H */
