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
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_dispd.h"
#include "ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_service_misc.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if 0/*defined(_DEBUG) && !defined(NDEBUG)*/
#  define SERV_DISPD_DEBUG 1
#endif /*_DEBUG && !NDEBUG*/

/* Lower bound of up-to-date/out-of-date ratio */
#define SERV_DISPD_STALE_RATIO_OK  0.8
/* Default rate increase if svc runs locally */
#define SERV_DISPD_LOCAL_SVC_BONUS 1.2


/* Dispatcher messaging support */
static int               s_MessageIssued = 0;
static FDISP_MessageHook s_MessageHook = 0;


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static void        s_Reset      (SERV_ITER);
    static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
    static int/*bool*/ s_Update     (SERV_ITER, TNCBI_Time, const char*);
    static void        s_Close      (SERV_ITER);

    static const SSERV_VTable s_op = {
        s_Reset, s_GetNextInfo, s_Update, 0/*Penalize*/, s_Close, "DISPD"
    };
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


int g_NCBIConnectRandomSeed = 0;


typedef struct {
    SSERV_Info*   info;
    double        status;
} SDISPD_Node;


typedef struct {
    int/*bool*/   disp_fail;
    SConnNetInfo* net_info;
    SDISPD_Node*  s_node;
    size_t        n_node;
    size_t        n_max_node;
} SDISPD_Data;


static int/*bool*/ s_AddServerInfo(SDISPD_Data* data, SSERV_Info* info)
{
    size_t i;

    /* First check that the new server info updates an existing one */
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

/*ARGSUSED*/
static int/*bool*/ s_ParseHeader(const char* header, void *iter,
                                 int/*ignored*/ server_error)
{
    SERV_Update((SERV_ITER) iter, header);
    return 1/*header parsed okay*/;
}


#ifdef __cplusplus
extern "C" {
    static int s_Adjust(SConnNetInfo*, void*, unsigned int);
}
#endif /* __cplusplus */

/*ARGSUSED*/
/* This callback is only for services called via direct HTTP */
static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         iter,
                            unsigned int  n)
{
    SDISPD_Data* data = (SDISPD_Data*)((SERV_ITER) iter)->data;
    return data->disp_fail ? 0/*failed*/ : 1/*try again*/;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    static const char service[]  = "service";
    static const char address[]  = "address";
    static const char platform[] = "platform";
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    SConnNetInfo *net_info = data->net_info;
    CONNECTOR conn = 0;
    const char *arch;
    unsigned int ip;
    char addr[64];
    char* s;
    CONN c;

    /* Dispatcher CGI arguments (sacrifice some if they all do not fit) */
    if ((arch = CORE_GetPlatform()) != 0 && *arch)
        ConnNetInfo_PreOverrideArg(net_info, platform, arch);
    if (*net_info->client_host && !strchr(net_info->client_host, '.') &&
        (ip = SOCK_gethostbyname(net_info->client_host)) != 0 &&
        SOCK_ntoa(ip, addr, sizeof(addr)) == 0) {
        if ((s = (char*) malloc(strlen(net_info->client_host) +
                                strlen(addr) + 3)) != 0) {
            sprintf(s, "%s(%s)", net_info->client_host, addr);
        } else
            s = net_info->client_host;
    } else
        s = net_info->client_host;
    if (s && *s)
        ConnNetInfo_PreOverrideArg(net_info, address, s);
    if (s != net_info->client_host)
        free(s);
    if (!ConnNetInfo_PreOverrideArg(net_info, service, iter->service)) {
        ConnNetInfo_DeleteArg(net_info, platform);
        if (!ConnNetInfo_PreOverrideArg(net_info, service, iter->service)) {
            ConnNetInfo_DeleteArg(net_info, address);
            if (!ConnNetInfo_PreOverrideArg(net_info, service, iter->service))
                return 0/*failed*/;
        }
    }
    /* Reset request method to be GET ('cause no HTTP body will follow) */
    net_info->req_method = eReqMethod_Get;
    /* Obtain additional header information */
    if ((!(s = SERV_Print(iter))
         || ConnNetInfo_OverrideUserHeader(net_info, s))                     &&
        ConnNetInfo_OverrideUserHeader(net_info, net_info->stateless
                                       ?"Client-Mode: STATELESS_ONLY\r\n"
                                       :"Client-Mode: STATEFUL_CAPABLE\r\n") &&
        ConnNetInfo_OverrideUserHeader(net_info,
                                       "Dispatch-Mode: INFORMATION_ONLY\r\n")){
        ConnNetInfo_OverrideUserHeader
            (net_info, "User-Agent: NCBIServiceDispatcher/"
             DISP_PROTOCOL_VERSION
#ifdef NCBI_CXX_TOOLKIT
             " (C++ Toolkit)"
#else
             " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
             "\r\n");
        data->disp_fail = 0;
        /* All the rest in the net_info structure is fine with us */
        conn = HTTP_CreateConnectorEx(net_info, fHCC_SureFlush, s_ParseHeader,
                                      s_Adjust, iter/*data*/, 0/*cleanup*/);
    }
    if (s) {
        ConnNetInfo_DeleteUserHeader(net_info, s);
        free(s);
    }
    if (!conn || CONN_Create(conn, &c) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[DISPATCHER]  Unable to create aux. %s",
                               conn ? "connection" : "connector"));
        assert(0);
        return 0/*failed*/;
    }
    /* This will also send all the HTTP data, and trigger header callback */
    CONN_Flush(c);
    CONN_Close(c);
    return ((SDISPD_Data*) iter->data)->n_node != 0;
}


static int/*bool*/ s_Update(SERV_ITER iter, TNCBI_Time now, const char* text)
{
    static const char server_info[] = "Server-Info-";
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    size_t len = strlen(text);

    if (len >= sizeof(server_info) &&
        strncasecmp(text, server_info, sizeof(server_info) - 1) == 0) {
        const char* p = text + sizeof(server_info) - 1;
        SSERV_Info* info;
        unsigned int d1;
        int d2;

        if (sscanf(p, "%u: %n", &d1, &d2) < 1)
            return 0/*not updated*/;
        if ((info = SERV_ReadInfo(p + d2)) != 0) {
            assert(info->rate != 0.0);
            info->time += now; /* expiration time now */
            if (s_AddServerInfo(data, info))
                return 1/*updated*/;
            free(info);
        }
    } else if (len >= sizeof(HTTP_DISP_FAILURES) &&
               strncasecmp(text, HTTP_DISP_FAILURES,
                           sizeof(HTTP_DISP_FAILURES) - 1) == 0) {
#if defined(_DEBUG) && !defined(NDEBUG)
        const char* p = text + sizeof(HTTP_DISP_FAILURES) - 1;
        while (*p && isspace((unsigned char)(*p)))
            p++;
        if (data->net_info->debug_printout)
            CORE_LOGF(eLOG_Warning, ("[DISPATCHER]  %s", p));
#endif /*_DEBUG && !NDEBUG*/
        data->disp_fail = 1;
        return 1/*updated*/;
    } else if (len >= sizeof(HTTP_DISP_MESSAGE) &&
               strncasecmp(text, HTTP_DISP_MESSAGE,
                           sizeof(HTTP_DISP_MESSAGE) - 1) == 0) {
        const char* p = text + sizeof(HTTP_DISP_MESSAGE) - 1;
        while (*p && isspace((unsigned char)(*p)))
            p++;
        if (s_MessageHook) {
            if (s_MessageIssued <= 0) {
                s_MessageIssued = 1;
                s_MessageHook(p);
            }
        } else {
            s_MessageIssued = -1;
            CORE_LOGF(eLOG_Warning, ("[DISPATCHER]  %s", p));
        }
    }

    return 0/*not updated*/;
}


static int/*bool*/ s_IsUpdateNeeded(SDISPD_Data *data)
{
    double status = 0.0, total = 0.0;

    if (data->n_node) {
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
    }

    return total == 0.0 ? 1 : (status/total < SERV_DISPD_STALE_RATIO_OK);
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    double total = 0.0, point = 0.0, access = 0.0, p = 0.0, status;
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

        if (iter->preferred_host == info->host  ||
            (!iter->preferred_host  &&
             info->locl  &&  info->coef < 0.0)) {
            if (iter->preference  ||  info->coef <= 0.0) {
                status *= SERV_DISPD_LOCAL_SVC_BONUS;
                if (access < status &&
                    (iter->preference  ||  info->coef < 0.0)) {
                    access =  status;
                    point  =  total + status; /* Latch this local server */
                    p      = -info->coef;
                    assert(point > 0.0);
                }
            } else
                status *= info->coef;
        }
        total                 += status;
        data->s_node[i].status = total;
    }

    if (point > 0.0  &&  iter->preference) {
        if (total != access) {
            p = SERV_Preference(iter->preference, access/total, data->n_node);
#ifdef SERV_DISPD_DEBUG
            CORE_LOGF(eLOG_Note, ("(P = %lf, A = %lf, T = %lf, N = %d)"
                                  " -> Pref = %lf", iter->preference,
                                  access, total, (int) data->n_node, p));
#endif /*SERV_DISPD_DEBUG*/
            status = total*p;
            p = total*(1.0 - p)/(total - access);
            for (i = 0; i < data->n_node; i++) {
                data->s_node[i].status *= p;
                if (p*point <= data->s_node[i].status) 
                    data->s_node[i].status += status - p*access;
            }
#ifdef SERV_DISPD_DEBUG
            for (i = 0; i < data->n_node; i++) {
                char addr[16];
                SOCK_ntoa(data->s_node[i].info->host, addr, sizeof(addr));
                status = data->s_node[i].status -
                    (i ? data->s_node[i-1].status : 0.0);
                CORE_LOGF(eLOG_Note, ("%s %lf %.2lf%%", addr,
                                      status, status/total*100.0));
            }
#endif /*SERV_DISPD_DEBUG*/
        }
        point = -1.0;
    }

    /* We take pre-chosen local server only if its status is not less than
       p% of the average remaining status; otherwise, we ignore the server,
       and apply the generic procedure by seeding a random point. */
    if (point <= 0.0 || access*(data->n_node - 1) < p*0.01*(total - access))
        point = (total * rand()) / (double) RAND_MAX;
    for (i = 0; i < data->n_node; i++) {
        if (point <= data->s_node[i].status)
            break;
    }
    assert(i < data->n_node);

    info = data->s_node[i].info;
    info->rate = data->s_node[i].status - (i ? data->s_node[i-1].status : 0.0);
    if (i < --data->n_node) {
        memmove(data->s_node + i, data->s_node + i + 1,
                (data->n_node - i)*sizeof(*data->s_node));
    }
    if (host_info)
        *host_info = 0;

    return info;
}


static void s_Reset(SERV_ITER iter)
{
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    if (data && data->s_node) {
        size_t i;
        assert(data->n_max_node);
        for (i = 0; i < data->n_node; i++)
            free(data->s_node[i].info);
        data->n_node = 0;
    }
}


static void s_Close(SERV_ITER iter)
{
    SDISPD_Data* data = (SDISPD_Data*) iter->data;
    assert(data->n_node == 0); /* s_Reset() had to be called before */
    if (data->s_node)
        free(data->s_node);
    ConnNetInfo_Destroy(data->net_info);
    free(data);
    iter->data = 0;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

/*ARGSUSED*/
const SSERV_VTable* SERV_DISPD_Open(SERV_ITER iter,
                                    const SConnNetInfo* net_info,
                                    SSERV_Info** info, HOST_INFO* u/*unused*/)
{
    SDISPD_Data* data;

    if (!(data = (SDISPD_Data*) calloc(1, sizeof(*data))))
        return 0;
    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDENT;
        srand(g_NCBI_ConnectRandomSeed);
    }
    data->net_info = ConnNetInfo_Clone(net_info); /*called with non-NULL*/
    if (iter->types & fSERV_StatelessOnly)
        data->net_info->stateless = 1/*true*/;
    if (iter->types & fSERV_Firewall)
        data->net_info->firewall = 1/*true*/;
    iter->data = data;

    iter->op = &s_op; /* SERV_Update() - from HTTP callback - expects this */
    if (!s_Resolve(iter)) {
        iter->op = 0;
        s_Reset(iter);
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo if info is needed */
    if (info)
        *info = 0;
    return &s_op;
}


extern void DISP_SetMessageHook(FDISP_MessageHook hook)
{
    if (hook) {
        if (hook != s_MessageHook)
            s_MessageIssued = s_MessageIssued ? -1 : -2;
    } else if (s_MessageIssued < -1)
        s_MessageIssued = 0;
    s_MessageHook = hook;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.67  2005/05/04 16:14:22  lavr
 * +<connect/ncbi_service_misc.h>
 *
 * Revision 6.66  2005/05/02 16:04:09  lavr
 * Use global random seed
 *
 * Revision 6.65  2005/04/25 18:46:13  lavr
 * Made parallel with ncbi_lbsmd.c
 *
 * Revision 6.64  2005/03/16 20:15:25  lavr
 * Allow local B services to have a preference
 *
 * Revision 6.63  2005/01/27 19:00:05  lavr
 * Explicit cast of malloc()ed memory
 *
 * Revision 6.62  2004/08/19 15:48:15  lavr
 * SERV_ITER::type renamed into SERV_ITER::types to reflect its bitmask nature
 *
 * Revision 6.61  2003/10/14 14:40:07  lavr
 * Fix to avoid resolving empty client's host name
 *
 * Revision 6.60  2003/10/10 19:33:24  lavr
 * Do not generate address CGI parameter if host address is unknown
 *
 * Revision 6.59  2003/08/11 19:07:03  lavr
 * +DISP_SetMessageHook() and implementation of message delivery
 *
 * Revision 6.58  2003/05/31 05:14:38  lavr
 * Add ARGSUSED where args are meant to be unused
 *
 * Revision 6.57  2003/05/22 20:31:40  lavr
 * Comment change
 *
 * Revision 6.56  2003/05/14 15:43:31  lavr
 * Add host address in dispatcher's CGI query
 *
 * Revision 6.55  2003/02/13 21:38:22  lavr
 * Comply with new SERV_Preference() prototype
 *
 * Revision 6.54  2003/02/06 17:35:36  lavr
 * Move reset of disp_fail to correct place in s_Resolve()
 *
 * Revision 6.53  2003/02/04 22:02:44  lavr
 * Introduce adjustment routine and disp_fail member to avoid MAX_TRY retrying
 *
 * Revision 6.52  2003/01/31 21:17:37  lavr
 * Implementation of perference for preferred host
 *
 * Revision 6.51  2002/12/10 22:11:50  lavr
 * Stamp HTTP packets with "User-Agent:" header tag and DISP_PROTOCOL_VERSION
 *
 * Revision 6.50  2002/11/19 19:21:40  lavr
 * Use client_host from net_info instead of obtaining it explicitly
 *
 * Revision 6.49  2002/11/01 20:14:07  lavr
 * Expand hostname buffers to hold up to 256 chars
 *
 * Revision 6.48  2002/10/28 20:12:56  lavr
 * Module renamed and host info API included
 *
 * Revision 6.47  2002/10/28 15:46:21  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.46  2002/10/21 18:32:35  lavr
 * Append service arguments "address" and "platform" in dispatcher requests
 *
 * Revision 6.45  2002/10/11 19:55:20  lavr
 * Append dispatcher request query with address and platform information
 * (as the old dispatcher used to do). Also, take advantage of various new
 * ConnNetInfo_*UserHeader() routines when preparing aux HTTP request.
 *
 * Revision 6.44  2002/09/24 15:08:50  lavr
 * Change non-zero rate assertion into more readable (info->rate != 0.0)
 *
 * Revision 6.43  2002/09/18 16:31:38  lavr
 * Temporary fix for precision loss removed & replaced with assert()
 *
 * Revision 6.42  2002/09/06 17:45:40  lavr
 * Include <connect/ncbi_priv.h> unconditionally (reported by J.Kans)
 *
 * Revision 6.41  2002/09/06 15:44:19  lavr
 * Use fHCC_SureFlush and CONN_Flush() instead of dummy read
 *
 * Revision 6.40  2002/08/12 15:13:50  lavr
 * Temporary fix for precision loss in transmission of SERV_Info as text
 *
 * Revision 6.39  2002/08/07 16:33:43  lavr
 * Changed EIO_ReadMethod enums accordingly; log moved to end
 *
 * Revision 6.38  2002/05/07 15:31:50  lavr
 * +#include <stdio.h>: noticed by J.Kans
 *
 * Revision 6.37  2002/05/06 19:18:12  lavr
 * Few changes to comply with the rest of API
 *
 * Revision 6.36  2002/04/13 06:40:05  lavr
 * Few tweaks to reduce the number of syscalls made
 *
 * Revision 6.35  2002/03/11 22:01:47  lavr
 * Threshold for choosing a local server explained better
 *
 * Revision 6.34  2001/12/04 15:57:05  lavr
 * Change log correction
 *
 * Revision 6.33  2001/10/01 19:53:39  lavr
 * -s_FreeData(), -s_ResetData() - do everything in s_Close()/s_Reset() instead
 *
 * Revision 6.32  2001/09/29 19:33:04  lavr
 * BUGFIX: SERV_Update() requires VT bound (was not the case in constructor)
 *
 * Revision 6.31  2001/09/29 18:41:03  lavr
 * "Server-Keyed-Info:" removed from protocol
 *
 * Revision 6.30  2001/09/28 20:52:16  lavr
 * Update VT method revised as now called on a per-line basis
 *
 * Revision 6.29  2001/09/24 20:30:01  lavr
 * Reset() VT method added and utilized
 *
 * Revision 6.28  2001/09/10 21:23:53  lavr
 * "Relay-Mode:" tag eliminated from the dispatcher protocol
 *
 * Revision 6.27  2001/07/24 18:02:02  lavr
 * Seed random generator at Open()
 *
 * Revision 6.26  2001/07/18 17:41:25  lavr
 * BUGFIX: In code for selecting services by preferred host
 *
 * Revision 6.25  2001/07/03 20:49:44  lavr
 * RAND_MAX included in the interval search
 *
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
