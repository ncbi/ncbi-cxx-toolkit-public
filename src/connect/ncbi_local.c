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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of local registry.
 *
 */

#include "ncbi_local.h"
#include "ncbi_lb.h"
#include "ncbi_priv.h"
#include <string.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static void        s_Reset      (SERV_ITER);
    static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
    static void        s_Close      (SERV_ITER);

    static const SSERV_VTable s_op = {
        s_Reset, s_GetNextInfo, 0/*Update*/, 0/*Penalize*/, s_Close, "LOCAL"
    };
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLOCAL_Data {
    SLB_Candidate* cand;
    size_t       n_cand;
    size_t       a_cand;
};


static SLB_Candidate* s_GetCandidate(void* user_data, size_t n)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) user_data;
    return n < data->n_cand ? &data->cand[n] : 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    SSERV_Info* info;
    size_t n;

    assert(data);
    if (!data->n_cand)
        return 0;

    for (n = 0; n < data->n_cand; n++)
        data->cand[n].status = data->cand[n].info->rate;
    n = LB_Select(iter, data, s_GetCandidate, 1.0);
    info = (SSERV_Info*) data->cand[n].info;
    info->rate = data->cand[n].status;
    if (n < --data->n_cand) {
        memmove(data->cand + n, data->cand + n + 1,
                (data->n_cand - n)*sizeof(*data->cand));
    }

    if (host_info)
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
}


static void s_Close(SERV_ITER iter)
{
    struct SLOCAL_Data* data = (struct SLOCAL_Data*) iter->data;
    assert(!data->n_cand); /* s_Reset() had to be called before */
    if (data->cand)
        free(data->cand);
    free(data);
    iter->data = 0;
}


static int/*bool*/ s_LoadServices(SERV_ITER iter)
{
    return 0/*false*/;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

/*ARGSUSED*/
const SSERV_VTable* SERV_LOCAL_Open(SERV_ITER iter,
                                    SSERV_Info** info, HOST_INFO* u/*unused*/)
{
    struct SLOCAL_Data* data;

    return 0/*for now*/;

    if (!iter->ismask  &&  strpbrk(iter->name, "?*"))
        return 0/*failed to start unallowed wildcard search*/;

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

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    return &s_op;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2006/03/28 18:27:32  lavr
 * Initial revision (not yet working)
 *
 * ==========================================================================
 */
