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
 *   with the use of NCBI network dispatcher (DISPD).
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2000/10/05 22:43:30  lavr
 * Another dummy revision: still in development
 *
 * Revision 6.3  2000/10/05 22:34:23  lavr
 * Temporary (dummy) revision for compilation to go
 *
 * Revision 6.2  2000/05/22 16:53:12  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:43:59  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "ncbi_servicep_dispd.h"

const SSERV_VTable *SERV_DISPD_Open(SERV_ITER iter, SConnNetInfo *net_info)
{
    return 0;
}

#if 0

#ifdef __cplusplus
extern "C" {
#endif

    static SSERV_Info* s_GetNextInfo(SERV_ITER iter);
    static void s_Close(SERV_ITER iter);

    static const SSERV_VTable s_op = { s_GetNextInfo, s_Close };

#ifdef __cplusplus
} /* extern "C" */
#endif

struct SDISPD_DataTag {
    struct SConnNetInfo* net_info;
    struct SSERV_Info*   s_info;
    unsigned             n_info;
    CONN                 conn;
};


const SSERV_VTable *SERV_DISPD_Open(SERV_ITER iter, SConnNetInfo *net_info)
{
    CONNECTOR c;
    CONN conn;

    if (!net_info && !(net_info = ConnNetInfo_Create(iter->service)))
        return 0;

    if (!(c = SOCK_CreateConnector(info->host, info->port, info->max_try)) ||
        CONN_Create(c, &conn) != eIO_Success) {
        ConnNetInfo_Destroy(&net_info);
        return 0;
    }

    if (!(iter->data = calloc(sizeof(struct SDISPD_DataTag)))) {
        ConnNetInfo_Destroy(&net_info);
        CONN_Close(conn);
        return 0;
    }

    data = (struct SDISPD_DataTag)(iter->data);
    data->net_info = net_info;
    data->conn = conn;

    CONN_SetTimeout(conn, eIO_Open, &net_info->timeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, &net_info->timeout);

    return s_op;
}

static void s_Close(SERV_ITER iter)
{
    if (iter->data) {
        struct SDISPD_DataTag *data = (struct SDISPD_DataTag *)iter->data;

        if (data->net_info)
            ConnNetInfo_Destroy(&data->net_info);
        if (data->info)
            free(data->info);
        if (data->conn)
            CONN_Close(conn);
        free(iter->data);
        iter->data = 0;
    }
}

static int/*bool*/ s_Connect(struct SDISPD_DataTag *data)
{
    
}

static SSERV_Info* s_GetNext(SERV_ITER iter)
{
    struct SDISPD_DataTag data = (struct SDISPD_DataTag *)iter->data;
    unsigned n;

    if (!data)
        return 0;

    if (!data->n_info && !s_Connect(data))
        return 0;

    
    memmove(&data->info[0], &data->info[1], sizeof(SSERV_Info));

}
#endif
