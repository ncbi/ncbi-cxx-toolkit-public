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
 * Revision 6.5  2000/10/20 17:36:05  lavr
 * Partially working dispd dispatcher client (service mapping works)
 * Checkin for backup purposes; working code '#if 0'-ed out
 *
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
#include <stdlib.h>
#include <string.h>

#if 1

const SSERV_VTable *SERV_DISPD_Open(SERV_ITER iter,
                                    const SConnNetInfo *net_info)
{
    return 0;
}

#else

#ifdef __cplusplus
extern "C" {
#endif

    static SSERV_Info* s_GetNextInfo(SERV_ITER iter);
    static int/*bool*/ s_Update(SERV_ITER iter, const char *text);
    static void s_Close(SERV_ITER iter);

    static const SSERV_VTable s_op = { s_GetNextInfo, s_Update, s_Close };

#ifdef __cplusplus
} /* extern "C" */
#endif


typedef struct SDISPD_DataTag {
    SConnNetInfo* net_info;
    SSERV_Info**  s_info;
    size_t        n_info;
    size_t        n_max_info;  
} SDISPD_Data;


static void s_FreeData(SDISPD_Data *data)
{
    if (!data)
        return;

    if (data->net_info)
        ConnNetInfo_Destroy(&(data->net_info));

    if (data->s_info) {
        size_t i;

        for (i = 0; i < data->n_info; i++)
            free(data->s_info[i]);
        free(data->s_info);
    }

    free(data);
}


static int/*bool*/ s_AddServerInfo(SDISPD_Data *data, SSERV_Info *info)
{
    if (data->n_info == data->n_max_info) {
        size_t n = data->n_max_info + 10;
        SSERV_Info** temp;

        if (data->s_info)
            temp = (SSERV_Info**) realloc(data->s_info, sizeof(*temp) * n);
        else
            temp = (SSERV_Info**) malloc(sizeof(*temp) * n);
        if (!temp)
            return 0;

        data->s_info = temp;
        data->n_max_info = n;
    }

    data->s_info[data->n_info++] = info;
    return 1;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    const static char client_mode[] = "Client-Mode: DISPATCH_ONLY\r\n";
    const static char accepted_types[] = "Accepted-Server-Types:";
    const static char service[] = "service=";
    SConnNetInfo *net_info = ((SDISPD_Data *)iter->data)->net_info;
    TSERV_Type type = (iter->type & ~fSERV_StatelessOnly), t;
    char service_arg[128];
    char buffer[128];
    size_t buflen;
    SOCK s;
    int/*bool*/ server_error = 0/*false*/;
    EIO_Status  status;
    BUF         buf = 0;
    
    /* Form service name argument (as CGI argument) */
    if (strlen(iter->service) + sizeof(service) > sizeof(service_arg))
        return 0/*failed*/;
    strcpy(service_arg, service);
    strcat(service_arg, iter->service);
    /* Form accepted server types (as customized header) */
    strcpy(buffer, accepted_types);
    buflen = sizeof(accepted_types) - 1;
    for (t = 1; t; t <<= 1) {
        if (type & t) {
            const char *name = SERV_TypeStr(t);
            size_t namelen = strlen(name);
            
            if (namelen) {
                if (buflen + namelen < sizeof(buffer) - 1) {
                    buffer[buflen++] = ' ';
                    strcpy(&buffer[buflen], name);
                    buflen += namelen;
                }
            } else
                break;
        }
    }
    assert(buflen < sizeof(buffer));
    if (buffer[buflen - 1] != ':' && buflen < sizeof(buffer) - 2)
        strcpy(&buffer[buflen - 1], "\r\n");
    else
        buffer[0] = '\0';
    /* Append customized header with client mode information */
    buflen = strlen(buffer);
    assert(buflen < sizeof(buffer));
    if (buflen + sizeof(client_mode)-1 < sizeof(buffer))
        strcpy(&buffer[buflen], client_mode);

    /* Now connect to the mapper with everything above */
    if (!(s = URL_Connect(net_info->host, net_info->port,
                          net_info->path, service_arg,
                          eReqMethodGet, 0/* request_length */,
                          &net_info->timeout, &net_info->timeout,
                          *buffer ? buffer : 0, 0/*no arg encoding*/))) {
        return 0/* failed */;
    }

    /* set timeout */
    SOCK_SetTimeout(s, eIO_Read, &net_info->timeout);

    /* check status (assume the reply status is in the first line) */
    if ((status = SOCK_StripToPattern(s, "\r\n", 2, &buf, 0)) == eIO_Success) {
        int http_v1, http_v2, http_status = 0;
        buflen = BUF_Peek(buf, buffer, sizeof(buffer)-1);
        assert(2 <= buflen   &&  buflen < sizeof(buffer));
        buffer[buflen] = '\0';
        if (sscanf(buffer, " HTTP/%d.%d %d ",
                   &http_v1, &http_v2, &http_status) != 3  ||
            http_status < 200  ||  299 < http_status)
            server_error = 1/*true*/;
    }

    if (!server_error) {
        /* Grab the entire HTTP header */
        status = SOCK_StripToPattern(s, "\r\n\r\n", 4, &buf, 0);
        if (status == eIO_Success) {
            size_t header_size = BUF_Size(buf);
            char *header = malloc(header_size + 1);

            if ((header_size = BUF_Read(buf, header, header_size)) != 0) {
                header[header_size] = '\0';
                if (!s_Update(iter, header))
                    status = eIO_Unknown;
            }
            free(header);
        }
    }

    BUF_Destroy(buf);

    if (server_error || status != eIO_Success)
        return 0/* fail */;

    return 1/* success */;
}


const SSERV_VTable *SERV_DISPD_Open(SERV_ITER iter,
                                    const SConnNetInfo *net_info)
{
    SDISPD_Data *data;

    if (!(data = calloc(1, sizeof(*data))))
        return 0;
    data->net_info = ConnNetInfo_Clone(net_info);
    iter->data = data;

    if (!s_Resolve(iter)) {
        iter->data = 0;
        s_FreeData(data);
        return 0;
    }
    
    return &s_op;
}


static void s_Close(SERV_ITER iter)
{
    s_FreeData(iter->data);
    iter->data = 0;
}


static int/*bool*/ s_Update(SERV_ITER iter, const char *text)
{
    const char server_info[] = "Server-Info-";
    char *buf = malloc(strlen(text) + 1);
    char *b = buf, *c;
    SSERV_Info *info;
    int n = 0;

    strcpy(buf, text);
    for (b = buf; (c = strchr(b, '\n')) != 0; b = c + 1) {
        unsigned d1;
        size_t d2;
        char *p;

        *c = '\0';
        if (strncasecmp(b, server_info, sizeof(server_info) - 1) != 0)
            continue;
        b += sizeof(server_info) - 1;
        if ((p = strchr(b, '\r')) != 0)
            *p = '\0';
        if (sscanf(b, "%u: %n", &d1, &d2) < 1)
            break;
        if (!(info = SERV_ReadInfo(b + d2, 0)))
            break;
        if (!s_AddServerInfo((SDISPD_Data *)iter->data, info))
            break;
        n++;
    }
    free(buf);

    return n != 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter)
{
    SDISPD_Data *data = (SDISPD_Data *)iter->data;
    SSERV_Info *info;

    if (!data)
        return 0;

    if (!data->n_info && !s_Resolve(iter))
        return 0;

    assert(data->n_info > 0);

    info = data->s_info[0];
    memmove(&data->s_info[0], &data->s_info[1],
            (--data->n_info)*sizeof(*data->s_info));

    return info;
}

#endif
