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
 * Revision 6.24  2001/06/25 15:36:38  lavr
 * s_GetNextInfo now takes one additional argument for host environment
 *
 * Revision 6.23  2001/06/20 17:27:49  kans
 * include <time.h> for Mac compiler
 *
 * Revision 6.22  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.21  2001/05/17 15:02:51  lavr
 * Typos corrected
 *
 * Revision 6.20  2001/05/11 15:30:31  lavr
 * Protocol change: REQUEST_FAILED -> DISP_FAILURES
 *
 * Revision 6.19  2001/05/03 16:58:16  lavr
 * FIX: Percent is taken of local bonus coef instead of the value itself
 *
 * Revision 6.18  2001/05/03 16:35:53  lavr
 * Local bonus coefficient modified: meaning of negative value changed
 *
 * Revision 6.17  2001/04/26 20:20:01  lavr
 * Better way of choosing local server with a tiny (e.g. penalized) status
 *
 * Revision 6.16  2001/04/24 21:35:46  lavr
 * Treatment of new bonus coefficient for local servers
 *
 * Revision 6.15  2001/03/21 21:24:11  lavr
 * Type match (int) for %n in scanf
 *
 * Revision 6.14  2001/03/06 23:57:27  lavr
 * SERV_DISPD_LOCAL_SVC_BONUS used for services running locally
 *
 * Revision 6.13  2001/03/05 23:10:46  lavr
 * SERV_ReadInfo takes only one argument now
 *
 * Revision 6.12  2001/03/01 00:33:12  lavr
 * FIXES: Empty update does not generate parse error
 *        Dispathing error is only logged in debug mode; milder severity
 *
 * Revision 6.11  2001/02/09 17:36:48  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 *
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
#if defined(_DEBUG) && !defined(NDEBUG)
#include "ncbi_priv.h"
#endif
#include "ncbi_servicep_dispd.h"
#include <connect/ncbi_ansi_ext.h>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Lower bound of up-to-date/out-of-date ratio */
#define SERV_DISPD_STALE_RATIO_OK  0.8
/* Default rate increase if svc runs locally */
#define SERV_DISPD_LOCAL_SVC_BONUS 1.2


#ifdef __cplusplus
extern "C" {
#endif

    static SSERV_Info* s_GetNextInfo(SERV_ITER iter, char** env);
    static int/*bool*/ s_Update(SERV_ITER iter, const char* text);
    static void s_Close(SERV_ITER iter);

    static const SSERV_VTable s_op = {
        s_GetNextInfo, s_Update, 0, s_Close, "DISPD"
    };

#ifdef __cplusplus
} /* extern "C" */
#endif


typedef struct {
    SSERV_Info* info;
    double      status;
} SDISPD_Node;


typedef struct {
    SConnNetInfo* net_info;
    SDISPD_Node*  s_node;
    size_t        n_node;
    size_t        n_max_node;
} SDISPD_Data;


static void s_FreeData(SDISPD_Data* data)
{
    if (!data)
        return;

    if (data->s_node) {
        size_t i;
        
        for (i = 0; i < data->n_node; i++)
            free(data->s_node[i].info);
        free(data->s_node);
    }

    ConnNetInfo_Destroy(data->net_info);

    free(data);
}


static int/*bool*/ s_AddServerInfo(SDISPD_Data* data, SSERV_Info* info)
{
    size_t i;

    /* First check that the new server info is updating existing one */
    for (i = 0; i < data->n_node; i++) {
        if (SERV_EqualInfo(data->s_node[i].info, info)) {
            /* Replace older version */
            free(data->s_node[i].info);
            data->s_node[i].info = info;
            return 1;
        }
    }

    /* Next, add new service to the list */
    if (data->n_node == data->n_max_node) {
        size_t n = data->n_max_node + 10;
        SDISPD_Node* temp;

        if (data->s_node)
            temp = (SDISPD_Node*) realloc(data->s_node, sizeof(*temp) * n);
        else
            temp = (SDISPD_Node*) malloc(sizeof(*temp) * n);
        if (!temp)
            return 0;
        
        data->s_node = temp;
        data->n_max_node = n;
    }

    data->s_node[data->n_node++].info = info;
    return 1;
}


#ifdef __cplusplus
extern "C" {
    static int s_ParseHeader(const char*, void*, int);
}
#endif /* __cplusplus */

static int/*bool*/ s_ParseHeader(const char* header, void *data,
                          int/*bool, ignored*/ server_error)
{
    SERV_ITER iter = (SERV_ITER) data;

    if (header)
        s_Update(iter, header);
    return 1/*header parsed okay*/;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    static const char service[] = "service=";
    static const char direct[] = "Relay-Mode: DIRECT\r\n";
    static const char firewall[] = "Relay-Mode: FIREWALL\r\n";
    static const char stateless[] = "Client-Mode: STATELESS_ONLY\r\n";
    static const char dispatch_mode[] = "Dispatch-Mode: INFORMATION_ONLY\r\n";
    static const char stateful_capable[] = "Client-Mode: STATEFUL_CAPABLE\r\n";
    SConnNetInfo *net_info = ((SDISPD_Data*) iter->data)->net_info;
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
    if ((s = (char*) malloc(buflen + 1)) != 0) {
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
    /* This dummy read will send all the HTTP data, we'll get a callback */
    CONN_Read(conn, 0, 0, &buflen, eIO_Plain);
    CONN_Close(conn);
    return ((SDISPD_Data*) iter->data)->n_node != 0;
}


static int/*bool*/ s_Update(SERV_ITER iter, const char* text)
{
    static const char server_info[] = "Server-Info-";
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    char* buf = (char*) malloc(strlen(text) + 1);
    TNCBI_Time t = (TNCBI_Time) time(0);
    char *b, *c;

    if (!buf)
        return 0/*failure*/;
    strcpy(buf, text);
    for (b = buf; (c = strchr(b, '\n')) != 0; b = c + 1) {
        SSERV_Info* info;
        unsigned int d1;
        char* p;
        int d2;

        *c = 0;
        if (strncasecmp(b, server_info,
                        sizeof(server_info) - 1) == 0) {
            b += sizeof(server_info) - 1;
            if ((p = strchr(b, '\r')) != 0)
                *p = 0;
            if (sscanf(b, "%u: %n", &d1, &d2) < 1)
                continue;
            if (!(info = SERV_ReadInfo(b + d2)))
                continue;
            info->time += t;        /* Expiration time now */
            if (!s_AddServerInfo(data, info))
                continue;
        } else if (strncasecmp(b, HTTP_DISP_FAILURES,
                               sizeof(HTTP_DISP_FAILURES) - 1) == 0) {
            b += sizeof(HTTP_DISP_FAILURES) - 1;
#if defined(_DEBUG) && !defined(NDEBUG)
            while (*b && isspace((unsigned char)(*b)))
                b++;
            if (!(p = strchr(b, '\r')))
                p = c;
            else
                *p = 0;
            assert(b <= p);
            if (data->net_info->debug_printout)
                CORE_LOGF(eLOG_Warning, ("[DISPATCHER] %s", b));
#endif
        }
    }
    free(buf);
    
    return 1/*success*/;
}


static int/*bool*/ s_IsUpdateNeeded(SDISPD_Data *data)
{
    double status = 0.0, total = 0.0;
    TNCBI_Time t = (TNCBI_Time) time(0);
    size_t i = 0;

    while (i < data->n_node) {
        SSERV_Info* info = data->s_node[i].info;

        total += info->rate;
        if (info->time < t) {
            if (i < --data->n_node)
                memmove(data->s_node + i, data->s_node + i + 1,
                        (data->n_node - i)*sizeof(*data->s_node));
            free(info);
        } else {
            status += info->rate;
            i++;
        }
    }
    return total != 0.0 ? (status/total < SERV_DISPD_STALE_RATIO_OK) : 1;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, char** env)
{
    double total = 0.0, point = -1.0, access = 0.0, p = 0.0, status;
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    SSERV_Info* info;
    size_t i;
    
    if (!data)
        return 0;
    
    if (s_IsUpdateNeeded(data) && !s_Resolve(iter))
        return 0;
    assert(data->n_node != 0);
    
    for (i = 0; i < data->n_node; i++) {
        info = data->s_node[i].info;
        status = info->rate;
        assert(status != 0.0);
        if (info->host == iter->preferred_host) {
            if (info->coef <= 0.0) {
                status *=SERV_DISPD_LOCAL_SVC_BONUS;
                if (info->coef < 0.0 && access < status) {
                    access = status;
                    point  = total; /* Latch this local server */
                    p      = -info->coef;
                }
            } else
                status *= info->coef;
        }
        total                 += status;
        data->s_node[i].status = total;
    }

    /* We will take pre-chosen local server only if its status is not less
       than p% of the average rest status; otherwise, we ignore the server,
       and apply the general procedure by seeding a random point. */
    if (point < 0.0 || access*(data->n_node - 1) < p*0.01*(total - access))
        point = (total * rand()) / (double) RAND_MAX;
    for (i = 0; i < data->n_node; i++) {
        if (point < data->s_node[i].status)
            break;
    }
    assert(i < data->n_node);
    
    info = data->s_node[i].info;
    info->rate = data->s_node[i].status - (i ? data->s_node[i-1].status : 0.0);
    if (i < --data->n_node) {
        memmove(data->s_node + i, data->s_node + i + 1,
                (data->n_node - i)*sizeof(*data->s_node));
    }
    if (env)
        *env = 0;

    return info;
}


static void s_Close(SERV_ITER iter)
{
    if (iter->data) {
        s_FreeData((SDISPD_Data*) iter->data);
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
    if (iter->type & fSERV_StatelessOnly)
        data->net_info->stateless = 1/*true*/;
    data->n_node = data->n_max_node = 0;
    data->s_node = 0;
    iter->data = data;

    if (!s_Resolve(iter)) {
        iter->data = 0;
        s_FreeData(data);
        return 0;
    }
    
    return &s_op;
}
