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
 * Revision 6.10  2001/01/25 17:06:36  lavr
 * s_FreeData now calls ConnNetInfo_Destroy() unconditionally
 *
 * Revision 6.9  2001/01/12 23:51:40  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.8  2001/01/08 23:48:14  lavr
 * (unsigned char) conversion in isspace
 *
 * Revision 6.7  2001/01/08 22:40:23  lavr
 * Further development of service-mapping protocol: stateless/stateful
 * is now separated from firewall/direct mode (see also in few more files)
 *
 * Revision 6.6  2000/12/29 18:05:46  lavr
 * First working revision.
 *
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

#include "ncbi_comm.h"
#include "ncbi_priv.h"
#include "ncbi_servicep_dispd.h"
#include <connect/ncbi_ansi_ext.h>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


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

    ConnNetInfo_Destroy(data->net_info);

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
    size_t i;

    /* First check that the new server info is updating existing one */
    for (i = 0; i < data->n_info; i++) {
        if (SERV_EqualInfo(data->s_info[i], info)) {
            /* Replace older version */
            free(data->s_info[i]);
            data->s_info[i] = info;
            return 1;
        }
    }

    /* Next, add new service to the list */
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


#ifdef __cplusplus
extern "C" {
    static int s_ParseHeader(const char *, void *, int);
}
#endif /* __cplusplus */

static int/*bool*/ s_ParseHeader(const char* header, void *data,
                          int/*bool, ignored*/ server_error)
{
    SERV_ITER iter = (SERV_ITER) data;

    if (header && !s_Update(iter, header))
        return 0;
    return 1;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    static const char service[] = "service=";
    static const char direct[] = "Relay-Mode: DIRECT\r\n";
    static const char firewall[] = "Relay-Mode: FIREWALL\r\n";
    static const char stateless[] = "Client-Mode: STATELESS_ONLY\r\n";
    static const char dispatch_mode[] = "Dispatch-Mode: INFORMATION_ONLY\r\n";
    static const char stateful_capable[] = "Client-Mode: STATEFUL_CAPABLE\r\n";
    SConnNetInfo *net_info = ((SDISPD_Data *)iter->data)->net_info;
    const char *tag1, *tag2;
    size_t buflen;
    CONNECTOR c;
    BUF buf = 0;
    CONN conn;
    char *s;
    
    /* Form service name argument (as CGI argument) */
    if (strlen(iter->service) + sizeof(service) > sizeof(net_info->args))
        return 0/*failed*/;
    strcpy(net_info->args, service);
    strcat(net_info->args, iter->service);
    /* Reset request method to be GET (as no HTTP body will follow) */
    net_info->req_method = eReqMethod_Get;
    /* Obtain additional header information */
    s = SERV_Print(iter);
    if (s) {
        int status = BUF_Write(&buf, s, strlen(s));
        free(s);
        if (!status) {
            BUF_Destroy(buf);
            return 0/*failure*/;
        }
    }
    tag1 = net_info->stateless ? stateless : stateful_capable;
    tag2 = net_info->firewall ? firewall : direct;
    if (!BUF_Write(&buf, tag1, strlen(tag1)) ||
        !BUF_Write(&buf, tag2, strlen(tag2)) ||
        !BUF_Write(&buf, dispatch_mode, sizeof(dispatch_mode)-1)) {
        BUF_Destroy(buf);
        return 0/*failure*/;
    }
    /* Now the entire user header is ready, take it out of the buffer */
    buflen = BUF_Size(buf);
    assert(buflen != 0);
    if ((s = (char *)malloc(buflen + 1)) != 0) {
        if (BUF_Read(buf, s, buflen) != buflen) {
            free(s);
            s = 0;
        } else
            s[buflen] = '\0';
    }
    BUF_Destroy(buf);
    if (!s)
        return 0/*failure*/;
    ConnNetInfo_SetUserHeader(net_info, s);
    assert(strcmp(net_info->http_user_header, s) == 0);
    free(s);
    /* All the rest in the net_info structure is fine with us */
    if (!(c = HTTP_CreateConnectorEx(net_info, 0/*flags*/, s_ParseHeader,
                                     0/*adj.info*/, iter/*data*/, 0/*clnup*/)))
        return 0/*failed*/;
    if (CONN_Create(c, &conn) != eIO_Success)
        return 0/*failed*/;
    CONN_SetTimeout(conn, eIO_Open, &net_info->timeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, &net_info->timeout);
    CONN_SetTimeout(conn, eIO_Close, &net_info->timeout);
    /* This dummy read will send all the HTTP data, we'll get a callback */
    CONN_Read(conn, 0, 0, &buflen, eIO_Plain);
    CONN_Close(conn);
    return ((SDISPD_Data*)(iter->data))->n_info != 0;
}


static int/*bool*/ s_Update(SERV_ITER iter, const char *text)
{
    static const char server_info[] = "Server-Info-";
    SDISPD_Data* data = (SDISPD_Data *)iter->data;
    char *buf = (char *)malloc(strlen(text) + 1);
    time_t t = time(0);
    char *b = buf, *c;
    SSERV_Info *info;
    int n = -1;

    if (!buf)
        return 0;
    strcpy(buf, text);
    for (b = buf; (c = strchr(b, '\n')) != 0; b = c + 1) {
        unsigned d1;
        size_t d2;
        char *p;

        *c = '\0';
        if (strncasecmp(b, server_info,
                        sizeof(server_info) - 1) == 0) {
            if (n < 0)
                n = 0;
            b += sizeof(server_info) - 1;
            if ((p = strchr(b, '\r')) != 0)
                *p = 0;
            if (sscanf(b, "%u: %n", &d1, &d2) < 1)
                continue;
            if (!(info = SERV_ReadInfo(b + d2, 0)))
                continue;
            info->time += t;        /* Set 'expiration time' */
            if (!s_AddServerInfo(data, info))
                continue;
            n++;
        } else if (strncasecmp(b, HTTP_REQUEST_FAILED,
                               sizeof(HTTP_REQUEST_FAILED) - 1) == 0) {
            n = 0;
            b += sizeof(HTTP_REQUEST_FAILED) - 1;
            while (*b && isspace((unsigned char)(*b)))
                b++;
            if (!(p = strchr(b, '\r')))
                p = c;
            else
                *p = '\0';
            if (b >= p) {
                CORE_LOG(eLOG_Error, "[UNKNOWN DISPATCHER ERROR]\n");
            } else {
                CORE_LOGF(eLOG_Error, ("[DISPATCHER ERROR] = %.*s\n",
                                       (int)(p - b), b));
            }
            break;
        }
    }
    free(buf);

    return n != 0;
}


static int/*bool*/ s_IsUpdateNeeded(SDISPD_Data *data)
{
    double status = 0.0, total = 0.0;
    time_t t = time(0);
    size_t i = 0;

    while (i < data->n_info) {
        SSERV_Info* info = data->s_info[i];

        total += info->rate;
        if (info->time < t) {
            if (i < --data->n_info)
                memmove(data->s_info + i, data->s_info + i + 1,
                        (data->n_info - i)*sizeof(*data->s_info));
            free(info);
        } else {
            status += info->rate;
            i++;
        }
    }
    return total != 0.0 ? (status/total < SERV_DISPD_STALE_RATIO_OK) : 1;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter)
{
    SDISPD_Data *data = (SDISPD_Data *)iter->data;
    double status, point;
    SSERV_Info* info = 0;
    size_t i;

    if (!data)
        return 0;
    
    if (s_IsUpdateNeeded(data) && !s_Resolve(iter))
        return 0;
    assert(data->n_info != 0);
    
    status = 0.0;
    for (i = 0; i < data->n_info; i++)
        status += data->s_info[i]->rate;
    
    point = (status * rand()) / (double)RAND_MAX;
    status = 0.0;
    for (i = 0; i < data->n_info; i++) {
        status += data->s_info[i]->rate;
        if (point < status)
            break;
    }
    assert(i < data->n_info);
    
    info = data->s_info[i];
    if (i < --data->n_info) {
        memmove(data->s_info + i, data->s_info + i + 1,
                (data->n_info - i)*sizeof(*data->s_info));
    }
    
    return info;
}


static void s_Close(SERV_ITER iter)
{
    if (iter->data) {
        s_FreeData((SDISPD_Data *)iter->data);
        iter->data = 0;
    }
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_DISPD_Open(SERV_ITER iter,
                                    const SConnNetInfo* net_info)
{
    SDISPD_Data* data;

    if (!(data = (SDISPD_Data*) malloc(sizeof(*data))))
        return 0;
    data->net_info = ConnNetInfo_Clone(net_info);
    data->n_info = data->n_max_info = 0;
    data->s_info = 0;
    iter->data = data;

    if (!s_Resolve(iter)) {
        iter->data = 0;
        s_FreeData(data);
        return 0;
    }
    
    return &s_op;
}
