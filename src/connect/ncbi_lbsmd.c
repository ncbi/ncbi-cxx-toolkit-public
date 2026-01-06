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
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of NCBI Load-Balancing Service Mapper (LBSMD).
 *
 */

#include "ncbi_lbsm.h"
#include "ncbi_lbsmd.h"
#include "ncbi_priv.h"


#ifdef NCBI_OS_UNIX

#include "ncbi_ansi_ext.h"
#include "ncbi_lb.h"
#include "ncbi_lbsm_ipc.h"
#include "ncbi_version.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM


#define MAX_IP_ADDR_LEN    16 /* sizeof("255.255.255.255") */

/* Default rate increase 10% if svc runs locally */
#define LBSMD_LOCAL_BONUS  1.1


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static int/*bool*/ s_Feedback   (SERV_ITER, double, TNCBI_Time);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable kLbsmdOp = {
    s_GetNextInfo, s_Feedback, 0/*Update*/, 0/*Reset*/, s_Close, "LBSMD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


volatile HEAP g_LBSM_Heap = 0;  /* make it accessible from the daemon code */


static ESwitch s_LBSM_FastHeapAccess = eOff;


typedef enum {
    eHOST_BestMatch, /* arg matches the provided environment */
    eHOST_GoodMatch,
    eHOST_FairMatch,
    eHOST_PoorMatch,
    eHOST_NoMatch,   /* arg does not match the environment   */
    eHOST_BadMatch,  /* the entry must be skipped            */
    eHOST_InitMatch  /* no information                       */
} EHOST_Match;


#if 0
static unsigned int x_HeapRevup(const SLBSM_Version* v)
{
    if (v->major > LBSM_HEAP_VERSION_MAJ_CRC32
        ||  (v->major == LBSM_HEAP_VERSION_MAJ_CRC32
             &&  v->minor > LBSM_HEAP_VERSION_MIN_CRC32)) {
        return v->start;
    }
    return v->entry.head.size > 32 ? v->start: 0;
}
#endif


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static const char* s_HostEnv(const SLBSM_Host* h)
{
    return h->env ? (const char*) h + h->env : 0;
}


static unsigned int s_Localhost(const SLBSM_Version* v)
{
    if (v->entry.head.size >= 32/*FIXME: remove this condition*/
        &&  v->local  &&  v->local != (unsigned int)(-1)) {
        return v->local;
    }
    return 0;
}


unsigned int LBSMD_GetLocalHostAddress(const void* p)
{
    unsigned int localhost;
    const SLBSM_Version* v = (const SLBSM_Version*) p;
    if (!v  ||  !(v->entry.head.flag & 1)
        ||  v->entry.head.size != sizeof(*v)
        ||  !(v->entry.type != eLBSM_Version)
        ||  !(localhost = s_Localhost(v))) {
        localhost = SOCK_GetLocalHostAddress(eDefault);
    }
    return localhost;
}


static unsigned int s_GetLocalHostAddress(HEAP heap)
{
    const SLBSM_Version* v = (const SLBSM_Version*) HEAP_Base(heap);
    assert(v->entry.type == eLBSM_Version);
    return LBSMD_GetLocalHostAddress(v);
}


static EHOST_Match s_Match(const char* env,
                           const char* arg, size_t arglen,
                           const char* val, size_t vallen,
                           const char** a, char** v)
{
    int/*bool*/ wildcard = 0/*false*/;
    int/*bool*/ noval = 0/*false*/;
    int/*bool*/ only = 0/*false*/;
    const char* c = env;

    assert(arg  &&  arglen);
    assert(a  &&  !*a  &&  v  &&  !*v);
    /* Note 1:  val == NULL implies vallen == 0, and means there was
     *          no argument in the query;
     * Note 2:  val != NULL does not imply vallen != 0, but means there
     *          was (perhaps, empty [if vallen == 0]) argument in the query.
     */
    while (c) {
        const char* p = strchr(c == env ? c : ++c, '=');
        const char* q = c;
        if (!p)
            break;
        c = strchr(q, '\n');
        if ((size_t)(p - q) != arglen  ||  strncasecmp(q, arg, arglen) != 0)
            continue;
        /* arg matches */
        *a = arg;
        if (memchr(p + 1, '!', (c ? (size_t)(c - p) : strlen(p)) - 1))
            only = 1/*true*/;
        for (q = p+1/*=*/ + strspn(p+1, " \t!"); ; q = p + strspn(p, " \t!")) {
            int/*bool*/ no = 0/*false*/;
            size_t len;
            if (*q != '\0'  &&  *q != '\n')
                len = strcspn(q, " \t!\n");
            else if (*p == '=')
                len = 0;
            else
                break;
            if (len  &&  *q == '~') {
                no = 1/*true*/;
                len--;
                q++;
            }
            if (len == 1  &&  *q == '*') {
                if (!no)
                    wildcard = 1/*true*/;
            } else if (len == 1  &&  *q == '-') {
                if (!val) {
                    if (no)
                        return eHOST_BadMatch;
                    *v = strndup("-", 1);
                    return eHOST_BestMatch;
                }
                if (no)
                    wildcard = 1/*true*/;
                else
                    noval = 1/*true*/;
            } else {
                size_t vlen = len;
                if (vlen == 2  &&  q[0] == '"'  &&  q[1] == '"')
                    vlen  = 0;
                if (val  &&  vlen == vallen  &&  !strncasecmp(q, val, vlen)) {
                    if (no)
                        return eHOST_BadMatch;
                    *v = strndup(q, vlen);
                    return eHOST_BestMatch;
                }
                if (no)
                    wildcard = 1/*true*/;
            }
            p = q + len;
        }
    }
    /* Neither best match nor mismatch found */
    if (val) {
        if (wildcard) {
            *v = strndup("*", 1);
            return eHOST_GoodMatch;
        }
        if (only)
            return eHOST_BadMatch;
        if (!*a)
            return eHOST_FairMatch;
        if (noval) {
            *v = strndup("", 0);
            return eHOST_PoorMatch;
        }
        return eHOST_NoMatch;
    }
    if (!*a)
        return eHOST_GoodMatch;
    if (only)
        return eHOST_BadMatch;
    if (wildcard) {
        *v = strndup("*", 1);
        return eHOST_FairMatch;
    }
    assert(!noval);
    return eHOST_PoorMatch;
}


struct SLBSM_Candidate {
    SLB_Candidate        cand;
    const SLBSM_Host*    host;
    const SLBSM_Service* svc;
    const char*          arg;
    const char*          val;
};


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static int s_SortStandbys(const void* p1, const void* p2);
#ifdef __cplusplus
}
#endif /*__cplusplus*/

static int s_SortStandbys(const void* p1, const void* p2)
{
    const struct SLBSM_Candidate* c1 = (const struct SLBSM_Candidate*) p1;
    const struct SLBSM_Candidate* c2 = (const struct SLBSM_Candidate*) p2;
    if (!c1->cand.status  ||  !c2->cand.status) {
        if (c1->cand.status)
            return -1;
        if (c2->cand.status)
            return  1;
    }
    if (c1->cand.status < 0.0  ||  c2->cand.status < 0.0) {
        if (c1->cand.status > 0.0)
            return -1;
        if (c2->cand.status > 0.0)
            return  1;
    }
    assert(c1->svc->info.rate * c2->svc->info.rate >= 0.0); /* same sign */
    /* randomization is done by allocation of services on the heap */
    return (int) copysign(1.0,
                          fabs(c2->svc->info.rate) - fabs(c1->svc->info.rate));
}


struct SLBSM_Data {
    struct SLBSM_Candidate* cand;
    size_t                  n_cand;
};


static SLB_Candidate* s_GetCandidate(void* user_data, size_t n)
{
    struct SLBSM_Data* data = (struct SLBSM_Data*) user_data;
    return n < data->n_cand ? &data->cand[n].cand : 0;
}


static SSERV_Info* s_FakeDnsReturn(SERV_ITER       iter,
                                   HOST_INFO*      host_info,
                                   int/*tristate*/ sign,
                                   TNCBI_Time      time)
{
    SSERV_Info* info;

    if (iter->last  ||  iter->n_skip)
        return 0;

    if ((info = SERV_CreateDnsInfo(0/*host*/)) != 0) {
        info->time = time != NCBI_TIME_INFINITE ? time + iter->time : time;
        info->rate = sign ? copysign(LBSM_DEFAULT_RATE, sign) : 0.0;
        if (host_info)
            *host_info = 0;
    }
    return info;
}


static int/*bool*/ s_VerifyChecksum(const HEAP   heap,
                                    unsigned int count,
                                    unsigned int cksum)
{ 
    const char* base       = (const char*) HEAP_Base(heap);
    const SLBSM_Version* v = (const SLBSM_Version*) base;
    if (v->count == count  &&  v->cksum == cksum) {
#if defined(_DEBUG)  &&  !defined(NDEBUG)
        static const unsigned char kZero[sizeof(v->cksum)] = { 0 };
        const char* stop = (const char*) &v->cksum;
        size_t       len = (size_t)(stop - base);
        unsigned int (*update)(unsigned int sum, const void* ptr, size_t len);

        assert(v == LBSM_GetVersion(heap));
        if (v->major < LBSM_HEAP_VERSION_MAJ_CRC32
            ||  (v->major == LBSM_HEAP_VERSION_MAJ_CRC32  &&
                 v->minor <= LBSM_HEAP_VERSION_MIN_CRC32)) {
            update = UTIL_CRC32_Update;
            cksum = 0;
        } else {
            update = UTIL_Adler32_Update;
            cksum = 1;
        }
        cksum = (*update)(cksum, base,  len);
        cksum = (*update)(cksum, kZero, sizeof(kZero));
        len   = HEAP_Size(heap) - len - sizeof(kZero);
        cksum = (*update)(cksum, stop + sizeof(kZero), len);
        return v->cksum == cksum ? 1/*success*/ : 0/*failure*/;
#else
        return 1/*success*/;
#endif /*_DEBUG && !NDEBUG*/
    }
    return 0/*failure*/;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_IfDaemon(void)
{
    if (g_LBSM_Heap) {
        CORE_LOCK_READ;
        if (g_LBSM_Heap  &&  !HEAP_Serial(g_LBSM_Heap)) {
            CORE_UNLOCK;
            return 1/*true*/;
        }
        CORE_UNLOCK;
    }
    return 0/*false*/;
}


/*
 * HEAP caching protocol (client side only).
 *
 * In order to make heap accesses as efficient as possible:
 *
 * If s_LBSM_FastHeapAccess is non-zero, each iterator has its own (cached)
 * copy of the LBSM heap, and keeps re-using it (w/o expiration checks) until
 * closed.
 *
 * If s_LBSM_FastHeapAccess is zero, g_LBSM_Heap (which then may expire and get
 * renewed in-between searches) is a cached copy and is used for service
 * look-ups.
 *
 * Serial numbers of original (attached) heaps are positive, while those of
 * copied heaps (including the global g_LBSM_Heap) are negative (negated
 * original).
 *
 * If the serial number of g_LBSM_Heap is 0, it means that the heap is
 * controlled by and used from within the daemon (LBSMD), and the heap caching
 * is not performed.
 */

static HEAP s_GetHeapCopy(TNCBI_Time now)
{
    enum {
        eNone     = 0,
        eAgain    = 1,
        eFallback = 2
    } retry   = eNone;
    HEAP heap = 0;
    HEAP lbsm;

    if (s_IfDaemon())
        return g_LBSM_Heap;

    for (;;) {
        const volatile SLBSM_Version* v;
        unsigned int count, cksum;
        const SLBSM_Version* c;
        void* base;
        int serial;

        CORE_LOCK_WRITE;

        if (g_LBSM_Heap) {
            c = LBSM_GetVersion(g_LBSM_Heap);
            assert((void*) c == HEAP_Base(g_LBSM_Heap));
            if (c->entry.good < now) {
#ifdef LBSM_DEBUG
                CORE_LOGF(eLOG_Trace,
                          ("Cached LBSM heap[%p, %p, %d] expired", g_LBSM_Heap,
                           HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
#endif /*LBSM_DEBUG*/
                HEAP_Destroy(g_LBSM_Heap);
                g_LBSM_Heap = 0;
            }
#ifdef LBSM_DEBUG
            else {
                CORE_LOGF(eLOG_Trace,
                          ("Cached LBSM heap[%p, %p, %d] valid", g_LBSM_Heap,
                           HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
            }
#endif /*LBSM_DEBUG*/
        } else
            c = 0/*dummy for compiler not to complain*/;
        
        if (!(lbsm = LBSM_Shmem_Attach(retry == eFallback ? 1/*T*/ : 0/*F*/))){
            /* an error has already been posted */
            break;
        }
        if ((serial = HEAP_Serial(lbsm)) <= 0) {
            CORE_LOGF_X(26, eLOG_Error,
                        ("Bad serial (%d) from LBSM heap attach", serial));
            break;
        }
        base = HEAP_Base(lbsm);

        if (!(v = LBSM_GetVersion(lbsm))  ||  (void*) v != base
            ||  (v->major < LBSM_HEAP_VERSION_MAJ
                 ||  (v->major == LBSM_HEAP_VERSION_MAJ
                      &&  v->minor < LBSM_HEAP_VERSION_MIN))) {
            if (v  &&  (void*) v == base) {
                CORE_LOGF_X(27, eLOG_Error,
                            ("LBSM heap[%d] version mismatch"
                             " (current=%hu.%hu, expected=%u.%u+)",
                             serial, v->major, v->minor,
                             LBSM_HEAP_VERSION_MAJ, LBSM_HEAP_VERSION_MIN));
            } else {
                CORE_LOGF_X(28, eLOG_Error,
                            ("LBSM heap[%d] version information %s",
                             serial, v ? "misplaced" : "missing"));
            }
            break;
        }
        if (v->entry.good < now) {
            int/*bool*/ again = !retry  &&  serial > 1? 1/*true*/ : 0/*false*/;
            CORE_LOGF_X(29, again ? eLOG_Warning : eLOG_Error,
                        ("LBSM heap[%d] out-of-date"
                         " (current=%lu, expiry=%lu, age=%lu, #%u)%s", serial,
                         (unsigned long) now, (unsigned long) v->entry.good,
                         (unsigned long) now -(unsigned long) v->entry.good,
                         v->count, again ? ", re-trying" : ""));
            if (again) {
                LBSM_Shmem_Detach(lbsm);
                retry = eFallback;
                CORE_UNLOCK;
                continue;
            }
            if (g_LBSM_Heap) {
#ifdef LBSM_DEBUG
                CORE_LOGF(eLOG_Trace,
                          ("Cached LBSM heap[%p, %p, %d] dropped", g_LBSM_Heap,
                           HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
#endif /*LBSM_DEBUG*/
                HEAP_Destroy(g_LBSM_Heap);
                g_LBSM_Heap = 0;
            }
            break;
        }

        if (g_LBSM_Heap) {
            assert(c);
            if (c->count == v->count  &&  c->cksum == v->cksum) {
#ifdef LBSM_DEBUG
                CORE_LOGF(eLOG_Trace,
                          ("Cached LBSM heap[%p, %p, %d] used", g_LBSM_Heap,
                           HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
#endif /*LBSM_DEBUG*/
                heap = g_LBSM_Heap;
                break;
            }
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace,
                      ("Cached LBSM heap[%p, %p, %d] stale", g_LBSM_Heap,
                       HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
#endif /*LBSM_DEBUG*/
            HEAP_Destroy(g_LBSM_Heap);
            g_LBSM_Heap = 0;
        }

        if (!(heap = HEAP_Copy(lbsm, 0, -serial))) {
            CORE_LOGF_ERRNO_X(3, eLOG_Error, errno,
                              ("Unable to copy LBSM heap[%d]", serial));
            break;
        }

        count = v->count;
        cksum = v->cksum;
        LBSM_Shmem_Detach(lbsm);

        if (s_VerifyChecksum(heap, count, cksum)) {
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace,
                      ("Cached LBSM heap[%p, %p, %d] renewed",
                       heap, HEAP_Base(heap), HEAP_Serial(heap)));
#endif /*LBSM_DEBUG*/
            g_LBSM_Heap = heap;
            lbsm = 0;
            break;
        }

        CORE_LOGF_X(4, retry ? eLOG_Error : eLOG_Warning,
                    ("LBSM heap[%p, %p, %d]%s checksum failure%s",
                     (void*) heap, HEAP_Base(heap), HEAP_Serial(heap),
                     retry == eAgain ? " persistent" : "",
                     retry           ? ""            : ", re-trying"));
        verify(HEAP_Destroy(heap) == 0);
        heap = 0;
        if (retry) {
            lbsm = 0;
            break;
        }

        retry = eAgain;
        CORE_UNLOCK;
    }

    if (lbsm) {
        assert(heap != lbsm);
        LBSM_Shmem_Detach(lbsm);
    }
    if (heap) {
        assert(heap == g_LBSM_Heap);
        verify(HEAP_AddRef(g_LBSM_Heap) > 1);
    }

    CORE_UNLOCK;

    return heap;
}


static const SLBSM_Host* s_LookupHost(HEAP heap, const SERV_ITER iter,
                                      const SLBSM_Service* svc)
{
    unsigned int addr =
        svc->info.host ? svc->info.host : s_GetLocalHostAddress(heap);
    const SLBSM_Host* host = LBSM_LookupHost(heap, addr, &svc->entry);
    if (!host  ||  host->entry.good < iter->time) {
        if (svc->info.rate > 0.0) {
            char buf[40];
            if (SOCK_ntoa(addr, buf, sizeof(buf)) != 0)
                strcpy(buf, "(unknown)");
            CORE_LOGF_X(30, eLOG_Error,
                        ("Dynamic %s server `%s' on [%s] w/%s host entry",
                         SERV_TypeStr(svc->info.type),
                         (const char*) svc + svc->name,
                         buf, host ? "outdated" : "o"));
        }
        return 0;
    }
    return host;
}


/*
 * Entries as stored in LBSMD heap:
 * R==0 T==0  Off      (Entry inaccessible and *must* be ignored)
 * R!=0 T >0  Active   (Standby: 0<|R|<LBSM_STANDBY_THRESHOLD)
 * R!=0 T==0  Reserved
 * R==0 T!=0  Down
 * Suppressed have their "fine" field at 100%.
 * The sign on R for non-down services denotes if the service is "static"
 * (when < 0) or "dynamic" (when > 0).
 */
static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    const TSERV_TypeOnly types
        = iter->types & (TSERV_TypeOnly)(~(fSERV_Stateless | fSERV_Firewall));
    size_t i, n, idx[eHOST_NoMatch], n_cand, a_cand;
    EHOST_Match best_match, match;
    struct SLBSM_Candidate* cand;
    const SLBSM_Service* svc;
    TNCBI_Time dns_info_time;
    const SLBSM_Host* host;
    const char* env, *a;
    SSERV_Info* info;
    const char* name;
    double status;
    int standby;
    HEAP heap;
    char* v;

    heap = (HEAP)(iter->data != iter ? iter->data : 0);
    if (heap) {
#ifdef LBSM_DEBUG
        CORE_LOGF(eLOG_Trace,
                  ("LBSM heap[%p, %p, %d] for \"%s\" detected",
                   heap, HEAP_Base(heap), HEAP_Serial(heap), iter->name));
#endif /*LBSM_DEBUG*/
        /*noop*/;
    } else if (!(heap = s_GetHeapCopy(iter->time))) {
        return iter->external  ||  iter->data == iter  ||  !(types & fSERV_Dns)
            ? 0 : s_FakeDnsReturn(iter, host_info, 0, NCBI_TIME_INFINITE);
    }

    best_match = eHOST_InitMatch;
    memset(idx, 0, sizeof(idx));
    standby = -1/*unassigned*/;
    dns_info_time = 0/*none*/;
    n = n_cand = a_cand = 0;
    a = v = 0;
    cand = 0;
    svc = 0;

    name = *iter->name ? iter->name : 0;
    assert(name  ||  iter->ismask); /*NB: ismask ignored for NULL*/
    while ((svc = LBSM_LookupService(heap, name, iter->ismask, svc))) {
        assert(svc->entry.type == eLBSM_Service);
        if (svc->entry.good < iter->time)
            continue; /* expired entry */

        if (!svc->info.rate  &&  !svc->info.time)
            continue; /* off */

        if (types != fSERV_Any  &&  !(types & svc->info.type))
            continue; /* type doesn't match */

        if (iter->external 
            &&  (svc->info.site & (fSERV_Local | fSERV_Private))) {
            continue; /* external mapping requested; local/private server */
        }

        if (!iter->ok_private  &&  (svc->info.site & fSERV_Private)) {
            assert(svc->info.type != fSERV_Dns);
            if (svc->info.host  &&
                svc->info.host != s_GetLocalHostAddress(heap)) {
                continue;
            }
        }

        if ((iter->types & fSERV_Stateless)&&(svc->info.mode & fSERV_Stateful))
            continue;

        if (svc->info.type == fSERV_Dns) {
            if (types == fSERV_Any)
                continue; /* DNS entries have to be requested explicitly */
            if (!iter->ismask) {
                if (dns_info_time < svc->info.time)
                    dns_info_time = svc->info.time;
            }
        } else {
            if (!iter->ismask  &&  iter->reverse_dns) {
                if (dns_info_time < svc->info.time)
                    dns_info_time = svc->info.time;
            }
        }

        if (svc->info.rate > 0.0  ||  host_info) {
            if (!(host = s_LookupHost(heap, iter, svc))
                &&  svc->info.rate > 0.0) {
                continue; /* no host information for non-static server */
            }
        } else
            host = 0;

        for (n = 0;  n < iter->n_skip;  ++n) {
            const SSERV_Info* skip = iter->skip[n];
            const char* s = SERV_NameOfInfo(skip);
            if (*s) {
                assert(iter->ismask  ||  iter->reverse_dns);
                if (strcasecmp(s, (const char*) svc + svc->name) == 0
                    &&  ((skip->type == fSERV_Dns  &&  !skip->host)
                         ||  SERV_EqualInfo(skip, &svc->info))) {
                    break;
                }
            } else if (SERV_EqualInfo(skip, &svc->info))
                break;
            if (skip->type == fSERV_Firewall
                &&  skip->u.firewall.type == svc->info.type) {
                break;
            }
            if (iter->reverse_dns  &&  skip->type == fSERV_Dns
                &&  skip->host == svc->info.host
                &&  (!skip->port  ||  skip->port == svc->info.port)) {
                break;
            }
        }
        /*FIXME*//*CORE_LOG(eLOG_Note, (char*) svc + svc->name);*/
        if (n >= iter->n_skip) {
            status = LBSM_CalculateStatus(svc->info.rate, svc->fine,
                                          (ESERV_Algo) svc->info.algo,
                                          &host->sys.load);
            if (status <= 0.0) {
                if (!svc->info.rate) {
                    if (!iter->ok_down)
                        continue; /* not operational */
                    status = 0.0;
                } else
                    status = copysign(svc->info.rate, -1.0);
            }
        } else
            status = 0.0; /* dummy assignment to keep no-init warning off */

        if (v) {
            free(v);
            v = 0;
        }
        a = env = 0;
        if (iter->pref < 0.0  &&  iter->host
            &&  (iter->host != svc->info.host
                 ||  (iter->port  &&  iter->port != svc->info.port))) {
            /* not a suitable fixed latching */
            match = eHOST_BadMatch;
        } else if (iter->arglen) {
            assert(iter->arg);
            if (!host)
                host = s_LookupHost(heap, iter, svc);
            env = host ? s_HostEnv(host) : 0;
            match = s_Match(env,
                            iter->arg, iter->arglen,
                            iter->val, iter->vallen, &a, &v);
            assert(!a  ||  a == iter->arg);
        } else
            match = eHOST_GoodMatch;

        if (best_match > match)
            best_match = match;

        if (match > eHOST_NoMatch) {
            assert(!v);
            continue;
        }

        if (svc->info.rate) {
            /* NB: server is _not_ down, but it may have been suppressed */
            if (fabs(svc->info.rate) < LBSM_STANDBY_THRESHOLD) {
                if (!standby) {
                    if (!iter->ok_suppressed)
                        continue;
                    /* this has to be given out as a suppressed one */
                    status = copysign(svc->info.rate, -1.0);
                } else if (standby < 0)
                    standby = 1;
            } else if (standby) {
                standby = 0/*cancel*/;
                if (!iter->ok_suppressed) {
                    memset(idx, 0, sizeof(idx));
                    for (i = 0;  i < n_cand;  ++i) {
                        if (cand[i].val)
                            free((void*) cand[i].val);
                    }
                    n_cand = 0;
                } else for (i = 0;  i < n_cand;  ++i)
                    cand[i].cand.status = copysign(cand[i].svc->info.rate,-1.);
            }
        }

        if (n < iter->n_skip)
            continue; /* excluded/seen;  NB: dummy assignment goes off here */

        if (!iter->ok_suppressed  &&  status < 0.0)
            continue;

#ifdef NCBI_LB_DEBUG
        if (iter->arglen) {
            char* s = SERV_WriteInfo(&svc->info);
            const char* m;
            assert(s);
            switch (match) {
            case eHOST_BestMatch:
                m = "Best match";
                break;
            case eHOST_GoodMatch:
                m = "Good match";
                break;
            case eHOST_FairMatch:
                m = "Fair match";
                break;
            case eHOST_PoorMatch:
                m = "Poor match";
                break;
            case eHOST_NoMatch:
                m = "No match";
                break;
            default:
                assert(0);
                m = "?";
                break;
            }
            assert(!a  || *a);
            assert(!v  ||  a);
            CORE_LOGF(eLOG_Note, ("%s%s%s%s: %s%s%s%s%s%s", s,
                                  env ? " <" : "", env ? env : "",
                                  env ? ">"  : "", m,
                                  a   ? ", arg="             : "",
                                  a   ? a                    : "",
                                  v   ? ", val="             : "",
                                  v   ? (*v ? v : "\"\"")    : "",
                                  standby > 0 ? ", standby"  : ""));
            free(s);
        }
#endif /*NCBI_LB_DEBUG*/

        /* This server should be taken into consideration */
        if (n_cand == a_cand) {
            struct SLBSM_Candidate* temp;
            n = a_cand + 10;
            temp = (struct SLBSM_Candidate*)
                (cand
                 ? realloc(cand, n * sizeof(*temp))
                 : malloc (      n * sizeof(*temp)));
            if (!temp)
                break;
            cand = temp;
            a_cand = n;
        }

        if (match < eHOST_NoMatch) {
            assert((size_t) match < sizeof(idx)/sizeof(idx[0]));
            n = idx[match];
            if (n < n_cand)
                memmove(&cand[n + 1], &cand[n], sizeof(cand[0])*(n_cand - n));
            for (i = match;  i < sizeof(idx)/sizeof(idx[0]);  ++i)
                idx[i]++;
        } else
            n = n_cand;
        cand[n].cand.info   = &svc->info;
        cand[n].cand.status = status;
        cand[n].host        = host;
        cand[n].svc         = svc;
        cand[n].arg         = a;
        cand[n].val         = v;
        a = v = 0;
        n_cand++;
    }
    if (v)
        free(v);

    if (best_match < eHOST_NoMatch) {
        assert(!best_match  ||  !idx[best_match - 1]);
        for (n = idx[best_match];  n < n_cand;  ++n) {
            if (cand[n].val)
                free((void*) cand[n].val);
        }
        n_cand = idx[best_match];
    }
    if (n_cand) {
        assert(cand);
        do {
            if (standby <= 0) {
                struct SLBSM_Data data;
                data.cand   = cand;
                data.n_cand = n_cand;
                n = LB_Select(iter, &data, s_GetCandidate, LBSMD_LOCAL_BONUS);
            } else {
                qsort(cand, n_cand, sizeof(*cand), s_SortStandbys);
                status = cand[0].cand.status;
                for (n = 1;  n < n_cand;  ++n) {
                    if (status != cand[n].cand.status)
                        break;
                }
                n = (size_t) rand() % n;
            }
            svc = cand[n].svc;
            /* FIXME: Do not convert secure services because DNS entry
             * (if found) cannot correctly reflect that property! */
            if (iter->reverse_dns  &&  svc->info.type != fSERV_Dns) {
                svc = 0;
                dns_info_time = 0/*none*/;
                while ((svc = LBSM_LookupService(heap, 0/*all*/, 0, svc)) !=0){
                    if (svc->info.type != fSERV_Dns  ||  !svc->info.time  ||
                        svc->info.host != cand[n].svc->info.host          ||
                        svc->info.port != cand[n].svc->info.port) {
                        continue;
                    }
                    if (!iter->ismask) {
                        if (dns_info_time < svc->info.time)
                            dns_info_time = svc->info.time;
                    }
                    if (iter->external
                        &&  (svc->info.site & (fSERV_Local | fSERV_Private))) {
                        continue;/* external mapping requested; local server */
                    }
                    status = LBSM_CalculateStatus(!svc->info.rate ? 0.0
                                                  : -LBSM_DEFAULT_RATE,
                                                  svc->fine, eSERV_Regular,
                                                  NULL);
                    if (status > 0.0)
                        break;
                    if ((!svc->info.rate  &&  iter->ok_down)  ||
                        ( svc->info.rate  &&  iter->ok_suppressed)) {
                        cand[n].cand.status = !svc->info.rate ? 0.0
                            : copysign(svc->info.rate, -1.0);
                        break;
                    }
                }
                if (!svc  &&  !dns_info_time)
                    svc = cand[n].svc;
            }
            if (svc)
                break;
            if (cand[n].val)
                free((void*) cand[n].val);
            if (n < --n_cand)
                memmove(cand + n, cand + n + 1, (n_cand - n) * sizeof(*cand));
        } while (n_cand);
    } else
        svc = 0;

    if (svc) {
        /* FIXME: do not return |rate| less that LBSM_STANDBY_THRESHOLD for
         * non-standby, return standbys with their natural values */ 
        name = (iter->ismask  ||  iter->reverse_dns ?
                (const char*) svc + svc->name : "");
        if ((info = SERV_CopyInfoEx(&svc->info, name)) != 0) {
            info->rate = cand[n].cand.status;
            if (info->time != NCBI_TIME_INFINITE)
                info->time  = cand[n].svc->entry.good;
            if (host_info) {
                if ((host = cand[n].host) != 0) {
                    *host_info =
                        HINFO_Create(host->addr, &host->sys, sizeof(host->sys),
                                     s_HostEnv(host), cand[n].arg,cand[n].val);
                } else
                    *host_info = 0;
            }
        }
    } else {
        info = !n_cand  &&  dns_info_time
            ? s_FakeDnsReturn(iter, host_info,
                              best_match == eHOST_InitMatch ?  0/*down*/ :
                              best_match != eHOST_BadMatch  ? -1/*busy*/ : 1,
                              dns_info_time)
            : 0;
    }

    for (n = 0;  n < n_cand;  ++n) {
        if (cand[n].val)
            free((void*) cand[n].val);
    }
    if (cand)
        free(cand);

    if (HEAP_Serial(heap)) {
        if (!s_LBSM_FastHeapAccess) {
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace,
                      ("LBSM heap[%p, %p, %d] for \"%s\" released", heap,
                       HEAP_Base(heap), HEAP_Serial(heap), iter->name));
#endif /*LBSM_DEBUG*/
            CORE_LOCK_WRITE;
            HEAP_Detach(heap);
            CORE_UNLOCK;
            heap = 0;
        }
#ifdef LBSM_DEBUG
        else {
            CORE_LOGF(eLOG_Trace,
                      ("LBSM heap[%p, %p, %d] for \"%s\" retained", heap,
                       HEAP_Base(heap), HEAP_Serial(heap), iter->name));
        }
#endif /*LBSM_DEBUG*/
    } else
        heap = 0;
    iter->data = heap;

    return info;
}


static int/*bool*/ s_Feedback(SERV_ITER iter, double rate, TNCBI_Time fine)
{
    SSERV_InfoCPtr last = iter->last;
    assert(last);
    return LBSM_SubmitPenaltyOrRerate(SERV_CurrentName(iter), last->type,
                                      rate, fine, last->host, last->port, 0);
}


static void s_Close(SERV_ITER iter)
{
    if (iter->data) {
#ifdef LBSM_DEBUG
        CORE_LOGF(eLOG_Trace,
                  ("LBSM heap[%p, %p, %d] for \"%s\" discarded",
                   iter->data, HEAP_Base((HEAP) iter->data),
                   HEAP_Serial((HEAP) iter->data), iter->name));
#endif /*LBSM_DEBUG*/
        CORE_LOCK_WRITE;
        HEAP_Detach((HEAP) iter->data);
        CORE_UNLOCK;
        iter->data = 0;
    }
    if (!s_LBSM_FastHeapAccess)
        LBSM_UnLBSMD(-1);
}


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static void s_Fini(void);
#ifdef __cplusplus
}
#endif /*__cplusplus*/


static void s_Fini(void)
{
    if (g_LBSM_Heap) {
        CORE_LOCK_WRITE;
        if (g_LBSM_Heap) {
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace,
                      ("Cached LBSM heap[%p, %p, %d] destroyed", g_LBSM_Heap,
                       HEAP_Base(g_LBSM_Heap), HEAP_Serial(g_LBSM_Heap)));
#endif /*LBSM_DEBUG*/
            HEAP_Destroy(g_LBSM_Heap);
            g_LBSM_Heap = 0;
        }
        CORE_UNLOCK;
    }
    LBSM_UnLBSMD(-1);
}


static int s_LBSMD(void)
{
    static volatile int s_Inited = 0;
    /* Daemon is running if LBSM_LBSMD() returns 1:  lock exists but read
     * operation failed with errno == EAGAIN:  the lock is taken (!=0). */
    if (LBSM_LBSMD(0) <= 0  ||  errno != EAGAIN)
        return 0/*false*/;
    if (!s_Inited) {
        CORE_LOCK_WRITE;
        if (!s_Inited  &&  atexit(s_Fini) == 0)
            s_Inited = 1;
        CORE_UNLOCK;
    }
    return 1/*true*/;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER    iter,
                                    SSERV_Info** info,
                                    HOST_INFO*   host_info,
                                    int/*bool*/  no_dispd_follows)
{
    SSERV_Info* tmp;

    assert(iter  &&  !iter->data  &&  !iter->op);
    assert(!strchr(iter->name, '/'));

    if (!s_IfDaemon()  &&  !s_LBSMD())
        return 0;

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed  = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }
    if (!no_dispd_follows)
        iter->data  = iter;
    tmp = s_GetNextInfo(iter, host_info);
    if (iter->data == iter)
        iter->data  = 0;
    if (!no_dispd_follows  &&  !tmp) {
        s_Close(iter);
        return 0;
    }
    if (info)
        *info = tmp ? tmp : (SSERV_Info*)(-1L);
    else if (tmp)
        free(tmp);
    return &kLbsmdOp;
}


ESwitch LBSMD_FastHeapAccess(ESwitch OnOff)
{
    ESwitch old = s_LBSM_FastHeapAccess;
    if (OnOff != eDefault)
        s_LBSM_FastHeapAccess = OnOff;
    return old;
}


/*NB: May not be used from within the daemon!*/
HEAP LBSMD_GetHeapCopy(TNCBI_Time now)
{
    return s_GetHeapCopy(now);
}


/*NB: May not be used from within the daemon!*/
const char* LBSMD_GetConfig(void)
{
    const char* s = 0;
    HEAP heap;

    if (s_LBSMD()) {
        if ((heap = s_GetHeapCopy((TNCBI_Time) time(0))) != 0) {
            if ((s = LBSM_GetConfig(heap)) != 0)
                s = strdup(s);
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace,
                      ("LBSM heap[%p, %p, %d] released",
                       heap, HEAP_Base(heap), HEAP_Serial(heap)));
#endif /*LBSM_DEBUG*/
            CORE_LOCK_WRITE;
            HEAP_Detach(heap);
            CORE_UNLOCK;
        }
    }
    if (!s_LBSM_FastHeapAccess) {
        /* As a documented side effect, clean up cached copy of LBSM heap */
        s_Fini();
    }
    return s;
}


/*NB: May not be used from within the daemon!*/
const char* LBSMD_GetHostParameter(unsigned int addr,
                                   const char*  name)
{
    size_t namelen = name  &&  *name ? strlen(name) : 0;
    const SLBSM_Host* host;
    const char* env;
    HEAP heap;

    if (!namelen  ||  !s_LBSMD())
        return 0;
    if (!(heap = s_GetHeapCopy((TNCBI_Time) time(0))))
        return 0;
    if (addr == SERV_ANYHOST  ||  addr == SERV_LOCALHOST)
        addr  = s_GetLocalHostAddress(heap);
    if ((host = LBSM_LookupHost(heap, addr, 0)) != 0
        &&  (env = s_HostEnv(host)) != 0) {
        const char* e;
        for (e = env;  *e;  e = env) {
            const char* p;
            size_t len;
            if (!(env = strchr(e, '\n'))) {
                len = strlen(e);
                env = e + len;
            } else
                len = (size_t)(env++ - e);
            if (!(p = (const char*) memchr(e, '=', len)))
                continue;
            len = (size_t)(p - e);
            if (len != namelen)
                continue;
            if (strncasecmp(e, name, namelen) != 0)
                continue;
            len = (size_t)(env - ++p);
            return strndup(p, len);
        }
    }
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace,
              ("LBSM heap[%p, %p, %d] released",
               heap, HEAP_Base(heap), HEAP_Serial(heap)));
#endif /*LBSM_DEBUG*/
    CORE_LOCK_WRITE;
    HEAP_Detach(heap);
    CORE_UNLOCK;
    return 0;
}


/*ARGSUSED*/
static const SLBSM_Sysinfo* s_GetSysinfo(const HOST_INFO hinfo,
                                         int/*bool*/     warn)
{
    const SLBSM_Sysinfo* si =
        (const SLBSM_Sysinfo*)((const char*) hinfo + sizeof(*hinfo));
    assert(hinfo);
#if defined(_DEBUG)  &&  !defined(NDEBUG)  &&  defined(NETDAEMONS_VERSION_INT)
    if (si->data.version < NETDAEMONS_VERSION_INT  &&  warn) {
        static int s_Warn = 0;
        if (s_Warn < 20) {
            char addr[40];
            if (SOCK_ntoa(hinfo->addr, addr, sizeof(addr)) != 0)
                strcpy(addr, "(unknown)");
            CORE_LOGF(s_Warn++ < 5 ? eLOG_Warning : eLOG_Trace,
                      ("HINFO may be incorrect for obsolete LBSMD at %s"
                       " (detected=%hu.%hu.%hu, expected=%s+)", addr,
                       NETDAEMONS_MAJOR_OF(si->data.version),
                       NETDAEMONS_MINOR_OF(si->data.version),
                       NETDAEMONS_PATCH_OF(si->data.version),
                       NETDAEMONS_VERSION_STR));
        }
    }
#endif /*_DEBUG && !NDEBUG && NETDAEMONS_VERSION_INT*/
    return si;
}


int LBSM_HINFO_CpuCount(const HOST_INFO hinfo)
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    return si->data.nrproc & 0xFF;
}


int LBSM_HINFO_CpuUnits(const HOST_INFO hinfo)
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    return si->data.nrproc >> 8;
}


double LBSM_HINFO_CpuClock(const HOST_INFO hinfo)
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    assert(si->data.hzfreq >= 0.0);
    return si->data.hzfreq / 0.128;
}


int LBSM_HINFO_TaskCount(const HOST_INFO hinfo)
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    return (int) si->load.nrtask;
}


int/*bool*/ LBSM_HINFO_Memusage(const HOST_INFO hinfo,
                                double memusage[5])
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    double pgsizemb = si->data.pgsize / 1024.0;
    memusage[0] = si->load.ram_total  * pgsizemb;
    memusage[1] = si->load.ram_cache  * pgsizemb;
    memusage[2] = si->load.ram_free   * pgsizemb;
    memusage[3] = si->load.swap_total * pgsizemb;
    memusage[4] = si->load.swap_free  * pgsizemb;
    return 1/*success*/;
}


int/*bool*/ LBSM_HINFO_MachineParams(const HOST_INFO hinfo,
                                     SHINFO_Params* p)
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    unsigned short div   = (si->data.kernel & LBSM_KERNELID_SVPKBIT
                            ? LBSM_KERNELID_SVPKDIV : 1);
    unsigned short major = (unsigned short)((si->data.kernel >> 24) & 0xFF);
    unsigned short minor = (unsigned short)((si->data.kernel >> 16) & 0xFF);
    p->kernel.patch      =                   si->data.kernel        & 0x7FFF;
    p->bits              =  si->data.machine >> 14;
    p->arch              = (si->data.machine >> 8) & 0x3F;
    p->ostype            =  si->data.machine       & 0xFF;
    p->pgsize            = (size_t) si->data.pgsize << 10;
    p->bootup            =  si->data.sys_uptime;
    p->startup           =  si->data.start_time;
    p->daemon.major      = NETDAEMONS_MAJOR_OF(si->data.version);
    p->daemon.minor      = NETDAEMONS_MINOR_OF(si->data.version);
    p->daemon.patch      = NETDAEMONS_PATCH_OF(si->data.version);
    p->kernel.major      = major / div;
    p->kernel.minor      = minor / div;
    p->svcpack           = (unsigned short)(((major % div) << 8) |
                                             (minor % div));
    return 1/*success*/;
}


int LBSM_HINFO_PortUsage(const HOST_INFO hinfo,
                         SHINFO_PortUsage ports[], size_t count)
{
    size_t n;
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    for (n = 0;  n < sizeof(si->load.port)/sizeof(si->load.port[0]);  ++n) {
        if (!si->load.port[n])
            break;
        if (n < count) {
            ports[n].port = si->load.port[n];
            ports[n].used = si->load.used[n] / 2.0;
        }
    }
    return (int) n;
}



int/*bool*/ LBSM_HINFO_LoadAverage(const HOST_INFO hinfo,
                                   double lavg[2])
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 1/*warn*/);
    lavg[0] = si->load.avg;
    lavg[1] = si->load.avgBLAST;
    return 1/*success*/;
}


int/*bool*/ LBSM_HINFO_Status(const HOST_INFO hinfo,
                              double status[2])
{
    const SLBSM_Sysinfo* si = s_GetSysinfo(hinfo, 0/*nowarn*/);
    status[0] = si->load.status;
    status[1] = si->load.statusBLAST;
    return 1/*success*/;
}


#else


/*ARGSUSED*/
int LBSM_LBSMD(int/*bool*/ check_n_lock)
{
    return -1/*failure*/;
}


/*ARGSUSED*/
const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER    iter,
                                    SSERV_Info** info,
                                    HOST_INFO*   host_info,
                                    int/*bool*/  no_dispd_follows)
{
    assert(iter  &&  !iter->data  &&  !iter->op);
    return 0/*failure*/;
}


extern const char* LBSMD_GetConfig(void)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
extern ESwitch LBSMD_FastHeapAccess(ESwitch sw)
{
    return eDefault/*not implemented*/;
}


/*ARGSUSED*/
extern HEAP LBSMD_GetHeapCopy(TNCBI_Time time)
{
    return 0/*no heap*/;
}


/*ARGSUSED*/
extern const char* LBSMD_GetHostParameter(unsigned int host,
                                          const char*  name)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
unsigned int LBSMD_GetLocalHostAddress(const void* v)
{
    return SOCK_GetLocalHostAddress(eDefault);
}


/*ARGSUSED*/
int LBSM_HINFO_CpuCount(const HOST_INFO hinfo)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
int LBSM_HINFO_CpuUnits(const HOST_INFO hinfo)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
double LBSM_HINFO_CpuClock(const HOST_INFO hinfo)
{
    return 0.0/*no info*/;
}


/*ARGSUSED*/
int LBSM_HINFO_TaskCount(const HOST_INFO hinfo)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_Memusage(const HOST_INFO hinfo,
                                double memusage[5])
{
    return 0/*no info*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_MachineParams(const HOST_INFO hinfo,
                                     SHINFO_Params* p)
{
    return 0/*no info*/;
}


/*ARGSUSED*/
int LBSM_HINFO_PortUsage(const HOST_INFO hinfo,
                         SHINFO_PortUsage ports[], size_t count)
{
    return 0/*no ports*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_LoadAverage(const HOST_INFO hinfo,
                                   double lavg[2])
{
    return 0/*no info*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_Status(const HOST_INFO hinfo,
                              double status[2])
{
    return 0/*no info*/;
}


#endif /*!NCBI_OS_UNIX*/


double LBSM_CalculateStatus(double rate, double fine, ESERV_Algo algo,
                            const SLBSM_HostLoad* load)
{
    double status;

    if (!rate)
        return 0.0;
    if (unlikely(rate < LBSM_STANDBY_THRESHOLD))
        status = rate < 0.0 ? -LBSM_DEFAULT_RATE : LBSM_DEFAULT_RATE;
    else {
        assert(load);
        status = algo & eSERV_Blast ? load->statusBLAST : load->status;
    }
    status *= rate / LBSM_DEFAULT_RATE;
    /* accurately apply fine: avoid imperfections with 100% */
    status *= (100. - (fine < 0. ? 0. : fine > 100. ? 100. : fine)) / 100.0;
    return fabs(status); /*paranoid but safe*/
}
