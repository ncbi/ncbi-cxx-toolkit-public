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
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of local registry.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_lb.h"
#include "ncbi_local.h"
#include "ncbi_priv.h"
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
    static void        s_Reset      (SERV_ITER);
    static void        s_Close      (SERV_ITER);

    static const SSERV_VTable s_op = {
        s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LOCAL"
    };
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLOCAL_Data {
    SLB_Candidate* cand;
    size_t       i_cand;
    size_t       n_cand;
    size_t       a_cand;
    int/*bool*/  reset;
};


static int/*bool*/ s_AddService(const SSERV_Info* info,
                                struct SLOCAL_Data* data)
{
    SLB_Candidate* temp;
    size_t n;

    if (data->a_cand <= data->n_cand) {
        n = data->a_cand + 10;
        temp = (SLB_Candidate*)(data->cand
                                ? realloc(data->cand,n * sizeof(*data->cand))
                                : malloc (           n * sizeof(*data->cand)));
        if (!temp)
            return 0/*false*/;
        data->a_cand = n;
        data->cand   = temp;
    }

    n = rand() % ++data->n_cand;
    if (n < data->n_cand - 1) {
        temp = data->cand + n++;
        memmove(temp + 1, temp, (data->n_cand - n) * sizeof(*data->cand));
    }
    data->cand[n].info = info;
    return 1/*true*/;
}


static int/*bool*/ s_LoadSingleService(const char* name, SERV_ITER iter)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    const TSERV_Type types = iter->types & ~fSERV_Firewall;
    char key[sizeof(REG_CONN_LOCAL_SERVER) + 10];
    int/*bool*/ ok = 0/*failed*/;
    SSERV_Info* info;
    int n;

    info = 0;
    strcpy(key, REG_CONN_LOCAL_SERVER "_");
    for (n = 0;  n <= 100;  n++) {
        const char* svc;
        char buf[1024];

        if (info) {
            free((void*) info);
            info = 0;
        }
        sprintf(key + sizeof(REG_CONN_LOCAL_SERVER), "%d", n);
        if (!(svc = ConnNetInfo_GetValue(name, key, buf, sizeof(buf), 0)))
            continue;
        if (!(info = SERV_ReadInfoEx
              (svc, iter->ismask  ||  iter->reverse_dns ? name : "", 0))) {
            continue;
        }
        if (iter->external  &&  info->locl)
            continue;  /* external mapping for local server not allowed */
        if (!info->host  ||  (info->locl & 0x10)) {
            unsigned int localhost = SOCK_GetLocalHostAddress(eDefault);
            if (!info->host)
                info->host = localhost;
            if ((info->locl & 0x10)  &&  info->host != localhost)
                continue;  /* private server */
        }
        if (!iter->reverse_dns  &&  info->type != fSERV_Dns) {
            if (types != fSERV_Any  &&  !(types & info->type))
                continue;  /* type doesn't match */
            if (types == fSERV_Any  &&  info->type == fSERV_Dns)
                continue;  /* DNS entries have to be req'd explicitly */
            if (iter->stateless && info->sful && !(info->type & fSERV_Http))
                continue;  /* skip stateful only servers */
        }
        if (!info->rate)
            info->rate = LBSM_DEFAULT_RATE;
        if (!info->time)
            info->time = LBSM_DEFAULT_TIME;

        if (!s_AddService(info, data))
            break;

        info = 0;
        ok = 1/*succeeded*/;
    }
    if (info)
        free((void*) info);

    return ok/*whatever*/;
}


static int/*bool*/ s_LoadServices(SERV_ITER iter)
{
    int/*bool*/ ok = 0/*false*/;
    char services[1024];
    const char* c;
    char* s;

    if (!iter->ismask) {
        ok = s_LoadSingleService(iter->name, iter);
        if (!ok  ||  !iter->reverse_dns)
            return ok;
    }
    if (!(c = ConnNetInfo_GetValue(0, REG_CONN_LOCAL_SERVICES,
                                   services, sizeof(services), 0))  ||  !*c) {
        return ok;
    }

    s = services;
    ok = 0/*false*/;
    for (s += strspn(s, " \t");  *s;  s += strspn(s, " \t")) {
        size_t len = strcspn(s, " \t");
        assert(len);
        if (s[len])
            s[len++] = '\0';
        if (!(c = SERV_ServiceName(s)))
            break;
        if ((iter->reverse_dns
             ||  (iter->ismask
                  &&  (!*iter->name  ||  UTIL_MatchesMask(c, iter->name))))
            &&  s_LoadSingleService(c, iter)) {
            ok = 1/*succeeded*/;
        }
        free((void*) c);
        s += len;
    }

    return ok/*whatever*/;
}


static int s_Sort(const void* p1, const void* p2)
{
    const SLB_Candidate* c1 = (const SLB_Candidate*) p1;
    const SLB_Candidate* c2 = (const SLB_Candidate*) p2;
    if (c1->info->type == fSERV_Dns  ||  c2->info->type == fSERV_Dns) {
        if (c1->info->type != fSERV_Dns)
            return -1;
        if (c2->info->type != fSERV_Dns)
            return  1;
    }
    if ((int) c1->info->type < (int) c2->info->type)
        return -1;
    if ((int) c1->info->type > (int) c2->info->type)
        return  1;
    return 0;
}


static SLB_Candidate* s_GetCandidate(void* user_data, size_t i)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) user_data;
    return i < data->i_cand ? &data->cand[i] : 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    const TSERV_Type types = iter->types & ~fSERV_Firewall;
    int/*bool*/ dns_info_seen = 0/*false*/;
    SSERV_Info* info;
    size_t i, n;

    assert(data);
    if (data->reset) {
        data->reset = 0/*false*/;
        if (!s_LoadServices(iter))
            return 0;
        if (data->n_cand > 1)
            qsort(data->cand, data->n_cand, sizeof(*data->cand), s_Sort);
    }

    i = 0;
    data->i_cand = 0;
    while (i < data->n_cand) {
        /* NB all servers have been loaded in accordance with iter->external */
        info = (SSERV_Info*) data->cand[i].info;
        if (info->rate > 0.0  ||  iter->ok_down) {
            const char* c = SERV_NameOfInfo(info);
            for (n = 0;  n < iter->n_skip;  n++) {
                const SSERV_Info* skip = iter->skip[n];
                const char* s = SERV_NameOfInfo(skip);
                if (*s) {
                    assert(iter->ismask  ||  iter->reverse_dns);
                    if (strcasecmp(s, c) == 0
                        &&  ((skip->type == fSERV_Dns  &&  !skip->host)  ||
                             SERV_EqualInfo(skip, info))) {
                        break;
                    }
                } else if (SERV_EqualInfo(skip, info))
                    break;
                if (iter->reverse_dns  &&  skip->type == fSERV_Dns
                    &&  skip->host == info->host
                    &&  (!skip->port  ||  skip->port == info->port)) {
                    break;
                }
            }
        } else
            n = 0;
        if (!iter->ismask) {
            if (types == fSERV_Any) {
                if (iter->reverse_dns  &&  info->type != fSERV_Dns)
                    dns_info_seen = 1/*true*/;
            } else if ((types & info->type)  &&  info->type == fSERV_Dns)
                dns_info_seen = 1/*true*/;
        }
        if (n < iter->n_skip) {
            if (i < --data->n_cand) {
                memmove(data->cand + i, data->cand + i + 1,
                        (data->n_cand - i) * sizeof(*data->cand));
            }
            free(info);
        } else {
            if (types != fSERV_Any  &&  !(types & info->type))
                break;
            if (types == fSERV_Any  &&  info->type == fSERV_Dns)
                break;
            data->i_cand++;
            data->cand[i].status = info->rate < 0.0 ? 0.0 : info->rate;
            if (iter->ok_down)
                break;
            i++;
        }
    }

    if (data->i_cand) {
        n = LB_Select(iter, data, s_GetCandidate, 1.0);
        info = (SSERV_Info*) data->cand[n].info;
        if (iter->reverse_dns  &&  info->type != fSERV_Dns) {
            dns_info_seen = 0/*false*/;
            for (i = 0;  i < data->n_cand;  i++) {
                SSERV_Info* temp = (SSERV_Info*) data->cand[i].info;
                if (temp->type != fSERV_Dns   ||
                    temp->host != info->host  ||  temp->port != info->port) {
                    continue;
                }
                if (!iter->ismask)
                    dns_info_seen = 1/*true*/;
                if (iter->external  &&  temp->locl)
                    continue; /* external mapping req'd; local server */
                assert(!(temp->locl & 0x10)); /* no private DNS */
                if (temp->rate > 0.0  ||  iter->ok_down) {
                    data->cand[i].status = data->cand[n].status;
                    info = temp;
                    n = i;
                    break;
                }
            }
            if (i >= data->n_cand  &&  dns_info_seen)
                info = 0;
        }

        if (info) {
            info->rate  = data->cand[n].status;
            info->time += iter->time;
            if (n < --data->n_cand) {
                memmove(data->cand + n, data->cand + n + 1,
                        (data->n_cand - n) * sizeof(*data->cand));
            }
        }
    } else if (iter->last  ||  iter->n_skip  ||  !dns_info_seen) {
        info = 0;
    } else if ((info = SERV_CreateDnsInfo(0)) != 0)
        info->time = NCBI_TIME_INFINITE;

    if (info  &&  host_info)
        *host_info = 0;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    if (data  &&  data->cand) {
        size_t i;
        assert(data->a_cand);
        for (i = 0; i < data->n_cand; i++)
            free((void*) data->cand[i].info);
        data->n_cand = 0;
    }
    data->reset = 1/*true*/;
}


static void s_Close(SERV_ITER iter)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    assert(!data->n_cand  &&  data->reset); /* s_Reset() has been called */
    if (data->cand) {
        assert(data->a_cand);
        data->a_cand = 0;
        free(data->cand);
        data->cand = 0;
    }
    free(data);
    iter->data = 0;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

/*ARGSUSED*/
const SSERV_VTable* SERV_LOCAL_Open(SERV_ITER iter,
                                    SSERV_Info** info, HOST_INFO* u/*unused*/)
{
    struct SLOCAL_Data* data;

    if (!(data = (struct SLOCAL_Data*) calloc(1, sizeof(*data))))
        return 0;

    iter->data = data;

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }

    if (!s_LoadServices(iter)) {
        s_Reset(iter);
        s_Close(iter);
        return 0;
    }
    if (data->n_cand > 1)
        qsort(data->cand, data->n_cand, sizeof(*data->cand), s_Sort);

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    return &s_op;
}
