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
 *   Top-level API to resolve NCBI service name to the server meta-address.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2000/05/22 16:53:11  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.2  2000/05/12 21:42:59  lavr
 * Cleaned up for the C++ compilation, etc.
 *
 * Revision 6.1  2000/05/12 18:50:20  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include "ncbi_servicep.h"
#include "ncbi_servicep_lbsmd.h"
#include "ncbi_servicep_dispd.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


SERV_ITER SERV_OpenSimple(const char* service)
{
    return SERV_OpenEx(service, 0, 0, 0, 0);
}


SERV_ITER SERV_Open(const char* service, TSERV_Type type,
                    unsigned int preferred_host)
{
    return SERV_OpenEx(service, type, preferred_host, 0, 0);
}


static int/*bool*/ s_AddSkipInfo(SERV_ITER iter, SSERV_Info *info)
{
    if (iter->n_skip == iter->n_max_skip) {
        SSERV_Info** temp;
        size_t n = iter->n_max_skip + 10;

        if (iter->skip)
            temp = (SSERV_Info**) realloc(iter->skip, sizeof(*temp) * n);
        else
            temp = (SSERV_Info**) malloc(sizeof(*temp) * n);
        if (!temp)
            return 0;

        iter->skip = temp;
        iter->n_max_skip = n;
    }

    iter->skip[iter->n_skip++] = info;
    return 1;
}


SERV_ITER SERV_OpenEx(const char* service, TSERV_Type type,
                      unsigned int preferred_host,
                      const SSERV_Info *skip[], size_t n_skip)
{
    size_t i;
    SERV_ITER iter;
    const SSERV_VTable *op;

    if (!(iter = (SERV_ITER)malloc(sizeof(*iter) + strlen(service) + 1)))
        return 0;

    iter->service = (char *)iter + sizeof(*iter);
    strcpy(iter->service, service);
    iter->type = type;
    iter->preferred_host = preferred_host;
    iter->skip = 0;
    iter->n_skip = iter->n_max_skip = 0;

    for (i = 0; i < n_skip; i++) {
        size_t infolen = SERV_SizeOfInfo(skip[i]);
        SSERV_Info* info = (SSERV_Info*) malloc(infolen);

        if (!info) {
            SERV_Close(iter);
            return 0;
        }
        memcpy(info, skip[i], infolen);
        if (!s_AddSkipInfo(iter, info)) {
            free(info);
            SERV_Close(iter);
        }
    }
    assert(n_skip == iter->n_skip);

    if (!(op = SERV_LBSMD_Open(iter)) && !(op = SERV_DISPD_Open(iter))) {
        SERV_Close(iter);
        return 0;
    }
    
    iter->op = op;
    return iter;
}


const SSERV_Info *SERV_GetNextInfo(SERV_ITER iter)
{
    SSERV_Info *info = 0;

    if (iter && iter->op && iter->op->GetNextInfo &&
        (info = (*iter->op->GetNextInfo)(iter)) != 0 &&
        !s_AddSkipInfo(iter, info)) {
        free(info);
        info = 0;
    }
    return info;
}


void SERV_Close(SERV_ITER iter)
{
    size_t i;
    if ( !iter )
        return;

    if (iter->op && iter->op->Close)
        (*iter->op->Close)(iter);
    for (i = 0; i < iter->n_skip; i++)
        free(iter->skip[i]);
    if (iter->skip)
        free(iter->skip);
    free(iter);
}
