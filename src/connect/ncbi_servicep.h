#ifndef CONNECT___NCBI_SERVICEP__H
#define CONNECT___NCBI_SERVICEP__H

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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Private API to define server iterator.
 *
 */

#include "ncbi_comm.h"
#include "ncbi_server_infop.h"
#include <connect/ncbi_service.h>


/* CAUTION:  There may be other states not classified by the macros below! */
#define SERV_IsDown(i)        ((i)->time  &&  (i)->time != NCBI_TIME_INFINITE \
                               &&  (i)->rate == 0.0)
#define SERV_IsActive(i)      ((i)->time  &&  (i)->time != NCBI_TIME_INFINITE \
                               &&  (i)->rate >  0.0)
#define SERV_IsStandby(i)     ((i)->time  &&  (i)->time != NCBI_TIME_INFINITE \
                               &&  (i)->rate != 0.0                           \
                               &&  fabs((i)->rate) < LBSM_STANDBY_THRESHOLD)
#define SERV_IsSuppressed(i)  ((i)->time  &&  (i)->time != NCBI_TIME_INFINITE \
                               &&  (i)->rate <  0.0)


#ifdef __cplusplus
extern "C" {
#endif


/* Table of iterator's "virtual functions"
 */
typedef struct {
    SSERV_Info* (*GetNextInfo)(SERV_ITER iter, HOST_INFO* host_info);
    int/*bool*/ (*Feedback)   (SERV_ITER iter, double value, int/*bool*/ fine);
    int/*bool*/ (*Update)     (SERV_ITER iter, const char* text, int code);
    void        (*Reset)      (SERV_ITER iter);
    void        (*Close)      (SERV_ITER iter);
    const char* mapper;
} SSERV_VTable;


/* enough to hold all types, currently */
typedef unsigned char TBSERV_TypeOnly;


/* Iterator structure, fields "packed" to consume minimal space.
 */
struct SSERV_IterTag {
    const char*         name; /* requested service name, private storage     */
    double              pref; /* preference [0..100]%% as a decimal fraction */
    TNCBI_Time          time; /* time of call                                */
    unsigned int        host; /* preferred host to select, network b.o.      */
    unsigned short      port; /* preferred port to select, host b.o.         */
    TBSERV_TypeOnly    types; /* requested server types only, packed         */
    unsigned        ismask:1; /* whether the name is to be treated as a mask */
    unsigned       ok_down:1; /* ..as taken..                                */
    unsigned    ok_standby:1; /*         ..from..                            */
    unsigned   ok_reserved:1; /*             ..types..                       */
    unsigned ok_suppressed:1; /*                  ..passed..                 */
    unsigned   reverse_dns:1; /*                        ..to..               */
    unsigned     stateless:1; /*                          ..SERV_*() calls.. */
    unsigned      external:1; /* whether this is an external request         */
    unsigned int   localhost; /* local host address if known                 */
    size_t            n_skip; /* actual number of servers in the array       */
    size_t            a_skip; /* number of allocated slots in the array      */
    SSERV_InfoCPtr*     skip; /* servers to skip (always w/names)            */
    SSERV_InfoCPtr      last; /* last server info taken out, points into skip*/
    const char*          arg; /* argument to match;  the original pointer!   */
    const char*          val; /* value to match;     the original pointer!   */
    size_t            arglen; /* NB: == 0 for the NULL "arg" pointer above   */
    size_t            vallen; /* NB: == 0 for the NULL "val" pointer above   */
    void*               data; /* private opaque data field of the mapper     */
    const SSERV_VTable*   op; /* table of virtual functions                  */
};


/* Control whether to skip using registry/environment when opening iterators,
 * and doing fast track lookups.  Dangerous!  Default is eOff.
 */
extern NCBI_XCONNECT_EXPORT ESwitch SERV_DoFastOpens(ESwitch on);


/* Modified "fast track" routine for obtaining of a server info in one-shot.
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


/* Return the service name, which the iterator is currently at.
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_CurrentName(SERV_ITER iter);


/* Private interface:  update mapper information from the given text
 * (<CR><LF> separated lines, usually as taken from HTTP header), and
 * by optionally (if non-zero) using the HTTP error code provided.
 */
int/*bool*/ SERV_Update
(SERV_ITER   iter,
 const char* text,
 int         code
 );


/* Private interface:  print and return an HTTP-compliant header portion
 * (<CR><LF> separated lines, including the last line) out of information
 * contained in the iterator;  to be used in mapping requests to DISPD.
 * Also, if "net_info" is non-NULL and "net_info->http_referer" is NULL,
 * then fill out "net_info->http_referer" accordingly.  "but_last" controls
 * whether the currently taken info appears as the info to skip over (by the
 * dispatcher) ["but_last"==0], or is just being used ["but_last"==1].
 * Return value must be 'free()'d.
 */
char* SERV_Print
(SERV_ITER     iter,
 SConnNetInfo* net_info,
 int/*bool*/   but_last
 );


/* Same as SERV_Penalize() but can specify penalty hold time.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_PenalizeEx
(SERV_ITER            iter,          /* handle obtained via 'SERV_Open*' call*/
 double               fine,          /* fine from range [0=min..100=max] (%%)*/
 TNCBI_Time           time           /* for how long to keep the penalty, sec*/
 );


/* Get name of underlying service mapper.
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_MapperName(SERV_ITER iter);


/* Get the final service name, using CONN_SERVICE_NAME_service environment
 * variable, then (if not found) registry section [service] and a key
 * CONN_SERVICE_NAME.  Return the resultant name (perhaps, an exact copy of
 * "service" if no override name has been found in environment/registry), which
 * is to be 'free()'d by the caller when no longer needed.
 * Return NULL on error.
 * NOTE:  This procedure can detect simple cyclical redefinitions, and
 * is limited to a search depth of 8.
 */
char* SERV_ServiceName(const char* service);


void SERV_InitFirewallMode(void);


int/*bool*/ SERV_AddFirewallPort
(unsigned short port
);


int/*bool*/ SERV_IsFirewallPort
(unsigned short port
);

void SERV_PrintFirewallPorts
(char*   buf,
 size_t  bufsize,
 EFWMode mode
);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_SERVICEP__H */
