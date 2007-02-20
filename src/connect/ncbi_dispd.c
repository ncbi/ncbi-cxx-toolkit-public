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
#include "ncbi_lb.h"
#include "ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* Lower bound of up-to-date/out-of-date ratio */
#define SERV_DISPD_STALE_RATIO_OK  0.8
/* Default rate increase 20% if svc runs locally */
#define SERV_DISPD_LOCAL_SVC_BONUS 1.2


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static void        s_Reset      (SERV_ITER);
    static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
    static int/*bool*/ s_Update     (SERV_ITER, const char*, int);
    static void        s_Close      (SERV_ITER);

    static const SSERV_VTable s_op = {
        s_Reset, s_GetNextInfo, s_Update, 0/*Penalize*/, s_Close, "DISPD"
    };
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SDISPD_Data {
    int/*bool*/    disp_fail;
    SConnNetInfo*  net_info;
    SLB_Candidate* cand;
    size_t         n_cand;
    size_t         a_cand;
};


static int/*bool*/ s_AddServerInfo(struct SDISPD_Data* data, SSERV_Info* info)
{
    size_t i;
    const char* name = SERV_NameOfInfo(info);
    /* First check that the new server info updates an existing one */
    for (i = 0; i < data->n_cand; i++) {
        if (strcasecmp(name, SERV_NameOfInfo(data->cand[i].info))
            &&  SERV_EqualInfo(info, data->cand[i].info)) {
            /* Replace older version */
            free((void*) data->cand[i].info);
            data->cand[i].info = info;
            return 1;
        }
    }
    /* Next, add new service to the list */
    if (data->n_cand == data->a_cand) {
        size_t n = data->a_cand + 10;
        SLB_Candidate* temp = (SLB_Candidate*)
            (data->cand
             ? realloc(data->cand, n * sizeof(*temp))
             : malloc (            n * sizeof(*temp)));
        if (!temp)
            return 0;
        data->cand = temp;
        data->a_cand = n;
    }
    data->cand[data->n_cand++].info = info;
    return 1;
}


#ifdef __cplusplus
extern "C" {
    static int s_ParseHeader(const char*, void*, int);
}
#endif /* __cplusplus */

static int/*bool*/ s_ParseHeader(const char* header,
                                 void*       iter,
                                 int         server_error)
{
    SERV_Update((SERV_ITER) iter, header, server_error);
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
    struct SDISPD_Data* data = (struct SDISPD_Data*)((SERV_ITER) iter)->data;
    return data->disp_fail ? 0/*failed*/ : 1/*try again*/;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    SConnNetInfo* net_info = data->net_info;
    CONNECTOR conn = 0;
    char* s;
    CONN c;

    assert(!!net_info->stateless == !!iter->stateless);
    /* Obtain additional header information */
    if ((!(s = SERV_Print(iter, 0))
         || ConnNetInfo_OverrideUserHeader(net_info, s))
        &&
        ConnNetInfo_OverrideUserHeader(net_info,
                                       iter->ok_dead  &&  iter->ok_suppressed
                                       ? "Dispatch-Mode: PROMISCUOUS\r\n"
                                       : iter->ok_dead
                                       ? "Dispatch-Mode: OK_DEAD\r\n"
                                       : iter->ok_suppressed
                                       ? "Dispatch-Mode: OK_SUPPRESSED\r\n"
                                       : "Dispatch-Mode: INFORMATION_ONLY\r\n")
        &&
        ConnNetInfo_OverrideUserHeader(net_info, iter->reverse_dns
                                       ? "Client-Mode: REVERSE_DNS\r\n"
                                       : !net_info->stateless
                                       ? "Client-Mode: STATEFUL_CAPABLE\r\n"
                                       : "Client-Mode: STATELESS_ONLY\r\n")) {
        /* all the rest in the net_info structure should be already fine */
        data->disp_fail = 0;
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
    return data->n_cand != 0  ||
        (!data->disp_fail  &&  net_info->stateless  &&  net_info->firewall);
}


static int/*bool*/ s_Update(SERV_ITER iter, const char* text, int code)
{
    static const char server_info[] = "Server-Info-";
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    int/*bool*/ failure;

    if (code == 400)
        data->disp_fail = 1;
    if (strncasecmp(text, server_info, sizeof(server_info) - 1) == 0) {
        const char* name;
        SSERV_Info* info;
        unsigned int d1;
        char* s;
        int d2;

        text += sizeof(server_info) - 1;
        if (sscanf(text, "%u: %n", &d1, &d2) < 1  ||  d1 < 1)
            return 0/*not updated*/;
        if (iter->ismask  ||  iter->reverse_dns) {
            char* c;
            if (!(s = strdup(text + d2)))
                return 0/*not updated*/;
            name = s;
            while (*name  &&  isspace((unsigned char)(*name)))
                name++;
            if (!*name) {
                free(s);
                return 0/*not updated*/;
            }
            for (c = s + (name - s);  *c;  c++) {
                if (isspace((unsigned char)(*c)))
                    break;
            }
            *c++ = '\0';
            d2 += (int)(c - s);
        } else {
            s = 0;
            name = "";
        }
        info = SERV_ReadInfoEx(text + d2, name);
        if (s)
            free(s);
        if (info) {
            if (info->time != NCBI_TIME_INFINITE)
                info->time += iter->time; /* expiration time now */
            if (s_AddServerInfo(data, info))
                return 1/*updated*/;
            free(info);
        }
    } else if (((failure = strncasecmp(text, HTTP_DISP_FAILURES,
                                       sizeof(HTTP_DISP_FAILURES) - 1) == 0)
                ||  strncasecmp(text, HTTP_DISP_MESSAGES,
                                sizeof(HTTP_DISP_MESSAGES) - 1) == 0)) {
        assert(sizeof(HTTP_DISP_FAILURES) == sizeof(HTTP_DISP_MESSAGES));
#if defined(_DEBUG) && !defined(NDEBUG)
        if (data->net_info->debug_printout) {
            text += sizeof(HTTP_DISP_FAILURES) - 1;
            while (*text  &&  isspace((unsigned char)(*text)))
                text++;
            CORE_LOGF(eLOG_Warning, ("[DISPATCHER %s]  %s",
                                     failure ? "FAILURE" : "MESSAGE", text));
        }
#endif /*_DEBUG && !NDEBUG*/
        if (failure)
            data->disp_fail = 1;
        return 1/*updated*/;
    }

    return 0/*not updated*/;
}


static int/*bool*/ s_IsUpdateNeeded(TNCBI_Time now, struct SDISPD_Data *data)
{
    double status = 0.0, total = 0.0;

    if (data->n_cand) {
        size_t i = 0;
        while (i < data->n_cand) {
            const SSERV_Info* info = data->cand[i].info;

            total += info->rate;
            if (info->time < now) {
                if (i < --data->n_cand) {
                    memmove(data->cand + i, data->cand + i + 1,
                            sizeof(*data->cand)*(data->n_cand - i));
                }
                free((void*) info);
            } else {
                status += info->rate;
                i++;
            }
        }
    }

    return total == 0.0 ? 1 : status/total < SERV_DISPD_STALE_RATIO_OK;
}


static SLB_Candidate* s_GetCandidate(void* user_data, size_t n)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) user_data;
    return n < data->n_cand ? &data->cand[n] : 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    SSERV_Info* info;
    size_t n;

    assert(data);
    if (s_IsUpdateNeeded(iter->time, data)  &&
        (!s_Resolve(iter)  ||  !data->n_cand)) {
        return 0;
    }

    for (n = 0; n < data->n_cand; n++)
        data->cand[n].status = data->cand[n].info->rate;
    n = LB_Select(iter, data, s_GetCandidate, SERV_DISPD_LOCAL_SVC_BONUS);
    info = (SSERV_Info*) data->cand[n].info;
    info->rate = data->cand[n].status;
    if (n < --data->n_cand) {
        memmove(data->cand + n, data->cand + n + 1,
                (data->n_cand - n) * sizeof(*data->cand));
    }

    if (host_info)
        *host_info = 0;

    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
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
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    assert(!data->n_cand); /* s_Reset() had to be called before */
    if (data->cand)
        free(data->cand);
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
    struct SDISPD_Data* data;

    if (!iter->ismask  &&  strpbrk(iter->name, "?*"))
        return 0/*failed to start unallowed wildcard search*/;

    if (!(data = (struct SDISPD_Data*) calloc(1, sizeof(*data))))
        return 0;

    assert(net_info); /*must called with non-NULL*/
    if ((data->net_info = ConnNetInfo_Clone(net_info)) != 0)
        data->net_info->service = iter->name; /* SetupStandardArgs() expects */
    if (!ConnNetInfo_SetupStandardArgs(data->net_info)) {
        ConnNetInfo_Destroy(data->net_info);
        free(data);
        return 0;
    }
    /* Reset request method to be GET ('cause no HTTP body is ever used) */
    data->net_info->req_method = eReqMethod_Get;
    if (iter->stateless)
        data->net_info->stateless = 1/*true*/;
    if (iter->type & fSERV_Firewall)
        data->net_info->firewall = 1/*true*/;
    ConnNetInfo_ExtendUserHeader(data->net_info,
                                 "User-Agent: NCBIServiceDispatcher/"
                                 DISP_PROTOCOL_VERSION
#ifdef NCBI_CXX_TOOLKIT
                                 " (C++ Toolkit)"
#else
                                 " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
                                 "\r\n");
    iter->data = data;
    iter->op = &s_op; /* SERV_Update() - from HTTP callback - expects this */

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }

    if (!s_Resolve(iter)) {
        iter->op = 0;
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
 * Revision 6.79  2006/04/05 14:59:32  lavr
 * Small fixup
 *
 * Revision 6.78  2006/03/05 17:38:12  lavr
 * Adjust for SERV_ITER to now carry time (s_Update)
 *
 * Revision 6.77  2006/02/21 14:57:59  lavr
 * Dispatcher to not require server-infos for stateless firewalled client
 *
 * Revision 6.76  2006/02/01 17:13:30  lavr
 * Remove spurious definition of g_NCBI_ConnectRandomSeed (ncbi_priv.c has it)
 *
 * Revision 6.75  2006/01/17 20:25:32  lavr
 * Handling of NCBI messages moved to HTTP connector
 * Dispatcher messages handled separately (FAILURES vs MESSAGES)
 *
 * Revision 6.74  2006/01/11 20:26:29  lavr
 * Take advantage of ConnNetInfo_SetupStandardArgs(); other related changes
 *
 * Revision 6.73  2006/01/11 16:29:17  lavr
 * Take advantage of UTIL_ClientAddress(), some other minor improvements
 *
 * Revision 6.72  2005/12/23 18:18:17  lavr
 * Rework to use newer dispatcher protocol to closely match the functionality
 * of local (LBSM shmem based) service locator.  Factoring out invariant
 * initializations/tuneups; better detection of sufficiency of client addr.
 *
 * Revision 6.71  2005/12/16 16:00:16  lavr
 * Take advantage of new generic LB API
 *
 * Revision 6.70  2005/12/14 21:33:15  lavr
 * Use new SERV_ReadInfoEx() prototype
 *
 * Revision 6.69  2005/12/08 03:52:28  lavr
 * Do not override User-Agent but extend it
 *
 * Revision 6.68  2005/07/11 18:23:14  lavr
 * Break grounds for receiving wildcard matches via HTTP header tags
 *
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
