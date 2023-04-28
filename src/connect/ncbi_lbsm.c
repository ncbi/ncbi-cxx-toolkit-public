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
 *   LBSM client-server data exchange API
 *
 *   UNIX only !!!
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_lbsm.h"
#include "ncbi_priv.h"
#include <connect/ncbi_socket_unix.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM


#ifdef   max
#  undef max
#endif /*max*/
#define  max(a, b)  ((a) < (b) ? (b) : (a))

#ifdef   min
#  undef min
#endif /*min*/
#define  min(a, b)  ((a) > (b) ? (b) : (a))


const SLBSM_Version* LBSM_GetVersion(HEAP heap)
{
    const SLBSM_Entry* e = (const SLBSM_Entry*) HEAP_Next(heap, 0);
    if (!e  ||  e->type != eLBSM_Version)
        return 0;
    assert((void*) e == (void*) HEAP_Base(heap));
    return (const SLBSM_Version*) e;
}


const char* LBSM_GetConfig(HEAP heap)
{
    const SLBSM_Entry* e = 0;
    while (likely((e = (const SLBSM_Entry*) HEAP_Next(heap, &e->head)) != 0)) {
        if (unlikely(e->type == eLBSM_Config)) {
            const SLBSM_Config* c = (const SLBSM_Config*) e;
            return c->name;
        }
    }
    return 0;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_NameMatch(int/*bool*/ mask,
                               const char* name,
                               const char* test)
{
    return ((!mask  &&  strcasecmp      (name, test) == 0)  ||
            ( mask  &&  UTIL_MatchesMask(name, test)) ? 1/*T*/ : 0/*F*/);
}


const SLBSM_Service* LBSM_LookupServiceEx(HEAP                heap,
                                          const char*         name,
                                          int/*bool*/         mask,
                                          const SLBSM_Entry** prev)
{
    const SLBSM_Entry* e = *prev;
    while (likely((e = (const SLBSM_Entry*) HEAP_Next(heap, &e->head)) != 0)) {
        if (likely(e->type == eLBSM_Service  ||  e->type == eLBSM_Pending)) {
            const SLBSM_Service* s = (const SLBSM_Service*) e;
            assert(s->info.host);
            if (!name  ||  x_NameMatch(mask, (const char*) s + s->name, name))
                return s;
        }
        *prev = e;
    }
    return 0;
}


const SLBSM_Service* LBSM_LookupService(HEAP                 heap,
                                        const char*          name,
                                        int/*bool*/          mask,
                                        const SLBSM_Service* hint)
{
    const SLBSM_Service* svc;
    const SLBSM_Entry*   prev = &hint->entry;
    if (likely(hint)  &&  unlikely(prev->type != eLBSM_Service)) {
        CORE_LOG_X(1, eLOG_Error, "Invalid preceding entry in service lookup");
        return 0;
    }
    while (likely((svc = LBSM_LookupServiceEx(heap, name, mask, &prev)) != 0)){
        if (likely(svc->entry.type == eLBSM_Service))
            break;
        assert(svc->entry.type == eLBSM_Pending);
        prev = &svc->entry;
    }
    return svc;
}


/* NB: "hint" may point to a freed and a merged-in entry */
const SLBSM_Host* LBSM_LookupHost(HEAP               heap,
                                  unsigned int       addr,
                                  const SLBSM_Entry* hint)
{
    int/*bool*/ wrap = hint ? 1/*true*/ : 0/*false*/;
    const SLBSM_Entry* e = hint;
    while (likely((e = (const SLBSM_Entry*)HEAP_Next(heap,&e->head)) != hint)){
        if (unlikely(!e)) {
            assert(hint);
            if (!wrap)
                break;
            wrap = 0/*false*/;
            continue;
        }
        if (e->type == eLBSM_Host) {
            const SLBSM_Host* h = (const SLBSM_Host*) e;
            assert(h->addr);
            if (unlikely(!addr  ||  h->addr == addr))
                return h;
        }
    }
    return 0;
}


double LBSM_CalculateStatus(double rate, double fine, ESERV_Algo algo,
                            const SLBSM_HostLoad* load)
{
    double status;

    if (!rate)
        return 0.0;
    if (unlikely(rate < LBSM_STANDBY_THRESHOLD))
        status = rate < 0.0 ? -LBSM_DEFAULT_RATE : LBSM_DEFAULT_RATE;
    else
        status = algo & eSERV_Blast ? load->statusBLAST : load->status;
    status *= rate / LBSM_DEFAULT_RATE;
    /* accurately apply fine: avoid imperfections with 100% */
    status *= (100. - (fine < 0. ? 0. : fine > 100. ? 100. : fine)) / 100.0;
    return fabs(status); /*paranoid but safe*/
}


int/*bool*/ LBSM_SubmitPenaltyOrRerate(const char*    name,
                                       ESERV_Type     type,
                                       double         rate,
                                       TNCBI_Time     fine,
                                       unsigned int   host,
                                       unsigned short port,
                                       const char*    path)
{
    const char* type_name = type ? SERV_TypeStr(type) : "ANY";
    struct sigaction sa, osa;
    int len, retval;
    char value[40];
    char addr[80];
    char* msg;

    if (!name  ||  !*name  ||  !*type_name
        ||  !SOCK_HostPortToString(host, port, addr, sizeof(addr))) {
        errno = EINVAL;
        return 0/*failure*/;
    }
    if (!path  ||  !*path)
        path = LBSM_DEFAULT_FEEDFILE;

    if (!(msg = (char*) malloc(20
                               + strlen(name)
                               + strlen(type_name)
                               + strlen(addr)
                               + sizeof(value)))) {
        return 0/*failure*/;
    }
    if (fine)
        NCBI_simple_ftoa(value, min(max(0.0, rate), 100.0), 0);
    else if (LBSM_RERATE_RESERVE <= rate)
        strcpy(value, "RESERVE");
    else if  (rate <= LBSM_RERATE_DEFAULT)
        strcpy(value, "DEFAULT");
    else {
        if (fabs(rate) < SERV_MINIMAL_RATE / 2.0)
            rate = 0.0;
        else if (rate > 0.0)
            rate = max(rate,  SERV_MINIMAL_RATE);
        else if (rate < 0.0)
            rate = min(rate, -SERV_MINIMAL_RATE);
        NCBI_simple_ftoa(value, !rate ? 0.0/*NB: avoid -0.0*/ :
                         min(max(-SERV_MAXIMAL_RATE, rate), SERV_MAXIMAL_RATE),
                         3/*decimal places*/);
    }
    retval = 0;  /*assume worst, a failure*/
    len = sprintf(msg, "%u %s %s%s %s %s\n", (unsigned int) geteuid(),
                  name, fine ? "" : "RERATE ", type_name, addr, value);
    if (len > 0) {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &sa, &osa) == 0) {
            SOCK cmd;
            SOCK_CreateUNIX(path, 0, &cmd, msg, (size_t)len, fSOCK_LogDefault);
            if (cmd  &&  SOCK_Close(cmd) == eIO_Success)
                retval = 1/*success*/;
            sigaction(SIGPIPE, &osa, 0);
        }
    }
    free(msg);
    return retval;
}
