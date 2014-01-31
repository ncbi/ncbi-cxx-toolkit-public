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
 *   with the use of NCBI network dispatcher (DISPD).
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_dispd.h"
#include "ncbi_lb.h"
#include "ncbi_priv.h"
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef   fabs
#  undef fabs
#endif /*fabs*/
#define  fabs(v)  ((v) < 0.0 ? -(v) : (v))

#define NCBI_USE_ERRCODE_X   Connect_Dispd

/* Lower bound of up-to-date/out-of-date ratio */
#define DISPD_STALE_RATIO_OK  0.8
/* Default rate increase 20% if svc runs locally */
#define DISPD_LOCAL_BONUS     1.2


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
    static int/*bool*/ s_Update     (SERV_ITER, const char*, int);
    static void        s_Reset      (SERV_ITER);
    static void        s_Close      (SERV_ITER);

    static const SSERV_VTable s_op = {
        s_GetNextInfo, 0/*Feedback*/, s_Update, s_Reset, s_Close, "DISPD"
    };
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SDISPD_Data {
    short/*bool*/  eof;  /* no more resolves */
    short/*bool*/  fail; /* no more connects */
    SConnNetInfo*  net_info;
    SLB_Candidate* cand;
    size_t         n_cand;
    size_t         a_cand;
    size_t         n_skip;
};


static int/*bool*/ s_AddServerInfo(struct SDISPD_Data* data, SSERV_Info* info)
{
    size_t i;
    const char* name = SERV_NameOfInfo(info);
    /* First check that the new server info updates an existing one */
    for (i = 0; i < data->n_cand; i++) {
        if (strcasecmp(name, SERV_NameOfInfo(data->cand[i].info)) == 0
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
    static EHTTP_HeaderParse s_ParseHeader(const char*, void*, int);
}
#endif /*__cplusplus*/

static EHTTP_HeaderParse s_ParseHeader(const char* header,
                                       void*       iter,
                                       int         server_error)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*)((SERV_ITER) iter)->data;
    int code = 0/*success code if any*/;
    if (server_error) {
        if (server_error == 400 || server_error == 403 || server_error == 404)
            data->fail = 1/*true*/;
    } else if (sscanf(header, "%*s %d", &code) < 1) {
        data->eof = 1/*true*/;
        return eHTTP_HeaderError;
    }
    /* check for empty document */
    if (!SERV_Update((SERV_ITER) iter, header, server_error)  ||  code == 204)
        data->eof = 1/*true*/;
    return eHTTP_HeaderSuccess;
}


#ifdef __cplusplus
extern "C" {
    static int s_Adjust(SConnNetInfo*, void*, unsigned int);
}
#endif /*__cplusplus*/

/*ARGSUSED*/
static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         iter,
                            unsigned int  unused)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*)((SERV_ITER) iter)->data;
    return data->fail ? 0/*no more tries*/ : 1/*may try again*/;
}


static void s_Resolve(SERV_ITER iter)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    SConnNetInfo* net_info = data->net_info;
    EIO_Status status = eIO_Success;
    CONNECTOR c = 0;
    CONN conn;
    char* s;

    assert(!(data->eof | data->fail));
    assert(!!net_info->stateless == !!iter->stateless);
    /* Obtain additional header information */
    if ((!(s = SERV_Print(iter, 0, 0))
         ||  ConnNetInfo_OverrideUserHeader(net_info, s))
        &&
        ConnNetInfo_OverrideUserHeader(net_info,
                                       iter->ok_down  &&  iter->ok_suppressed
                                       ? "Dispatch-Mode: PROMISCUOUS\r\n"
                                       : iter->ok_down
                                       ? "Dispatch-Mode: OK_DOWN\r\n"
                                       : iter->ok_suppressed
                                       ? "Dispatch-Mode: OK_SUPPRESSED\r\n"
                                       : "Dispatch-Mode: INFORMATION_ONLY\r\n")
        &&
        ConnNetInfo_OverrideUserHeader(net_info, iter->reverse_dns
                                       ? "Client-Mode: REVERSE_DNS\r\n"
                                       : !net_info->stateless
                                       ? "Client-Mode: STATEFUL_CAPABLE\r\n"
                                       : "Client-Mode: STATELESS_ONLY\r\n")) {
        c = HTTP_CreateConnectorEx(net_info, fHTTP_Flushable, s_ParseHeader,
                                   iter/*data*/, s_Adjust, 0/*cleanup*/);
    }
    if (s) {
        ConnNetInfo_DeleteUserHeader(net_info, s);
        free(s);
    }
    if (c  &&  (status = CONN_Create(c, &conn)) == eIO_Success) {
        /* Send all the HTTP data... */
        CONN_Flush(conn);
        /* ...then trigger the header callback */
        CONN_Close(conn);
    } else {
        CORE_LOGF_X(1, eLOG_Error,
                    ("%s%s%sUnable to create auxiliary HTTP %s: %s",
                     &"["[!*iter->name], iter->name, *iter->name ? "]  " : "",
                     c              ? "connection" : "connector",
                     IO_StatusStr(c ? status       : eIO_Unknown)));
        if (c  &&  c->destroy)
            c->destroy(c);
        assert(0);
    }
}


static int/*bool*/ s_Update(SERV_ITER iter, const char* text, int code)
{
    static const char server_info[] = "Server-Info-";
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    int/*bool*/ failure;

    if (strncasecmp(text, server_info, sizeof(server_info) - 1) == 0
        &&  isdigit((unsigned char) text[sizeof(server_info) - 1])) {
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
                                sizeof(HTTP_DISP_MESSAGES) - 1) == 0)  &&
               isspace((unsigned char) text[sizeof(HTTP_DISP_FAILURES) - 1])) {
        assert(sizeof(HTTP_DISP_FAILURES) == sizeof(HTTP_DISP_MESSAGES));
#if defined(_DEBUG) && !defined(NDEBUG)
        if (data->net_info->debug_printout) {
            text += sizeof(HTTP_DISP_FAILURES) - 1;
            while (*text  &&  isspace((unsigned char)(*text)))
                text++;
            CORE_LOGF_X(2, failure ? eLOG_Warning : eLOG_Note,
                        ("[%s]  %s", data->net_info->svc, text));
        }
#endif /*_DEBUG && !NDEBUG*/
        if (failure) {
            if (code)
                data->fail = 1;
            return 1/*updated*/;
        }
        /* NB: a mere message does not constitute an update */
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

            total += fabs(info->rate);
            if (info->time < now) {
                if (i < --data->n_cand) {
                    memmove(data->cand + i, data->cand + i + 1,
                            sizeof(*data->cand)*(data->n_cand - i));
                }
                free((void*) info);
            } else {
                status += fabs(info->rate);
                i++;
            }
        }
    }

    return total == 0.0 ? 1 : status/total < DISPD_STALE_RATIO_OK;
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
    if (!data->fail  &&  iter->n_skip < data->n_skip)
        data->eof = 0/*false*/;
    data->n_skip = iter->n_skip;

    if (s_IsUpdateNeeded(iter->time, data)) {
        if (!(data->eof | data->fail))
            s_Resolve(iter);
        if (!data->n_cand)
            return 0;
    }

    for (n = 0; n < data->n_cand; n++)
        data->cand[n].status = data->cand[n].info->rate;
    n = LB_Select(iter, data, s_GetCandidate, DISPD_LOCAL_BONUS);
    info = (SSERV_Info*) data->cand[n].info;
    info->rate = data->cand[n].status;
    if (n < --data->n_cand) {
        memmove(data->cand + n, data->cand + n + 1,
                (data->n_cand - n) * sizeof(*data->cand));
    }

    if (host_info)
        *host_info = 0;
    data->n_skip++;

    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    if (data) {
        data->eof = data->fail = 0/*false*/;
        if (data->cand) {
            size_t i;
            assert(data->a_cand);
            for (i = 0; i < data->n_cand; i++)
                free((void*) data->cand[i].info);
            data->n_cand = 0;
        }
        data->n_skip = iter->n_skip;
    }
}


static void s_Close(SERV_ITER iter)
{
    struct SDISPD_Data* data = (struct SDISPD_Data*) iter->data;
    assert(!data->n_cand); /*s_Reset() had to be called before*/
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

    if (!(data = (struct SDISPD_Data*) calloc(1, sizeof(*data))))
        return 0;
    iter->data = data;

    assert(net_info); /*must be called with non-NULL*/
    data->net_info = ConnNetInfo_Clone(net_info);
    if (!ConnNetInfo_SetupStandardArgs(data->net_info, iter->name)) {
        s_Close(iter);
        return 0;
    }

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed  = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }

    /* Reset request method to be GET ('cause there's no HTTP body to send) */
    data->net_info->req_method = eReqMethod_Get;
    if (iter->stateless)
        data->net_info->stateless = 1/*true*/;
    if ((iter->types & fSERV_Firewall)  &&  !data->net_info->firewall)
        data->net_info->firewall = eFWMode_Adaptive;
    ConnNetInfo_ExtendUserHeader(data->net_info,
                                 "User-Agent: NCBIServiceDispatcher/"
                                 NCBI_DISP_VERSION
#ifdef NCBI_CXX_TOOLKIT
                                 " (CXX Toolkit)"
#else
                                 " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
                                 "\r\n");
    data->n_skip = iter->n_skip;

    iter->op = &s_op; /*SERV_Update() [from HTTP callback] expects*/
    s_Resolve(iter);
    iter->op = 0;

    if (!data->n_cand  &&  (data->fail
                            ||  !(data->net_info->stateless  &&
                                  data->net_info->firewall))) {
        s_Reset(iter);
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    return &s_op;
}
