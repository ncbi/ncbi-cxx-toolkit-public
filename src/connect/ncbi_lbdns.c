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
 *   Low-level API to resolve an NCBI service name to server meta-addresses
 *   with the use of DNS.
 *
 */

#include "ncbi_config.h"
#include "ncbi_lbdns.h"

#ifdef NCBI_OS_UNIX

#include "ncbi_priv.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM  /* errors: 31 and on */


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LBDNS"
};
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLBDNS_Data {
    const char*    host;     /* LB DNS server host */
    unsigned short port;     /* LB DNS server port */
    const char*    domain;   /* Domain name to use */
    size_t         a_info;   /* Allocated pointers */
    size_t         n_info;   /* Used pointers      */
    SSERV_Info*    info[1];  /* "a_info" pointers  */
};



static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    return 0/*false*/;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    SSERV_Info* info;

    if (!data->n_info)
        return 0;
    info = data->info[0];
    assert(info);
    if (--data->n_info) {
        memmove(data->info, data->info + 1,
                data->n_info * sizeof(data->info[0]));
    }
    if (host_info)
        *host_info = 0;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    if (data  &&  data->n_info) {
        size_t n;
        for (n = 0;  n < data->n_info;  ++n) {
            assert(data->info[n]);
            free(data->info[n]);
        }
        data->n_info = 0;
    }
}


static void s_Close(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    iter->data = 0;
    assert(!data->n_info); /*s_Reset() had to be called before*/
    if (data->host)
        free((void*) data->host);
    if (data->domain)
        free((void*) data->domain);
    free(data);
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    const char val[NS_MAXDNAME];
    struct SLBDNS_Data* data;

    /* Can process fSERV_Any (meaning fSERV_Standalone), and explicit
     * fSERV_Standalone and/or fSERV_Dns only */
    if (iter->types != fSERV_Any
        &&  !(iter->types & (fSERV_Standalone | fSERV_Dns))) {
        return 0;
    }
    if (!(data = (struct SLBDNS_Data*) calloc(1, sizeof(*data)
                                              + sizeof(data->info) * 31))) {
        return 0;
    }
    data->a_info = 32;
    iter->data = data;

    if (!s_Resolve(iter)) {
        s_Reset(iter);
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    return &s_op;
}


#else

#include "ncbi_once.h"


/*ARGSUSED*/
const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    /* NB: This should never be called on a non-UNIX platform */
    static void* /*bool*/ s_Once = 0/*false*/;
    if (CORE_Once(&s_Once))
        CORE_LOG(eLOG_Critical, "LB DNS only available on UNIX platform(s)");
    return 0;
}


#endif /*NCBI_OS_UNIX*/
