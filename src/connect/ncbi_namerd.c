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
 * Authors:  Anton Lavrentiev, David McElhany
 *
 * File Description:
 *   Low-level API to resolve an NCBI service name to server meta-addresses
 *   with the use of NAMERD.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_lb.h"
#include "ncbi_namerd.h"
#include "ncbi_priv.h"
#include "parson.h"

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_memory_connector.h>

#include <ctype.h>
#include <stdlib.h>
#include <time.h>


#ifdef _MSC_VER
#define FMT_TIME_T      "%llu"
#else
#define FMT_TIME_T      "%lu"
#endif


#define NCBI_USE_ERRCODE_X   Connect_Namerd


/* NAMERD subcodes for CORE_LOG*X() macros */
enum ENAMERD_Subcodes {
    eNSub_Message         = 0,   /**< not an error */
    eNSub_Alloc           = 1,   /**< memory allocation failed */
    eNSub_BadData         = 2,   /**< bad data was provided */
    eNSub_Connect         = 3,   /**< problem in connect library */
    eNSub_HttpRead        = 4,   /**< failed reading from HTTP conn */
    eNSub_Json            = 5,   /**< a JSON parsing failure */
    eNSub_Libc            = 6,   /**< a standard library failure */
    eNSub_NoService       = 7,   /**< couldn't reach namerd service provider */
    eNSub_TooLong         = 8    /**< data was too long to fit in a buffer */
};


/* Apache is limited to around 4000 byte query strings. -- todo: find reference
 * Note that this is approximate and doesn't need to be precise because
 * it's more than should be required for namerd anyway.
 */
#define MAX_QRY_STR_LEN             4000

/* This is hard-coded in the definition of SConnNetInfo in ncbi_connutil.h */
#define MAX_ARGS_LEN                2048

/* Misc. */
#define DTAB_HDR_FIELD_NAME         "DTab-Local"
#define NIL                         '\0'


/*  Registry entry names and default values for NAMERD "SConnNetInfo" fields.
    Note that these are purely for the NAMERD API; they don't relate to any
    other part of the connect library, returned endpoints, or client code.
    Therefore, they are independent of other connect library macros.
    Also, the namerd API doesn't support using a port so there's no macro for
    that.
 */
#define REG_NAMERD_SECTION          "_NAMERD"

#define REG_NAMERD_PROXY_HOST_KEY   "PROXY_HOST"
#ifdef _MSC_VER
/* NAMERD_TODO - temporary, until Windows machines support plain "linkerd" */
#define REG_NAMERD_PROXY_HOST_DEF   \
    "proxy.linkerd.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#else
#define REG_NAMERD_PROXY_HOST_DEF   "linkerd"
#endif

#define REG_NAMERD_PROXY_PORT_KEY   "PROXY_PORT"
#define REG_NAMERD_PROXY_PORT_DEF   4140

#define REG_NAMERD_API_HOST_KEY     "API_HOST"
#define REG_NAMERD_API_HOST_DEF     "api.namerd.linkerd.ncbi.nlm.nih.gov"

#define REG_NAMERD_API_PATH_KEY     "API_PATH"
#define REG_NAMERD_API_PATH_DEF     "/api/1/resolve"

#define REG_NAMERD_API_ARGS_KEY     "API_ARGS"
#define REG_NAMERD_API_ARGS_DEF     "path=/service/"

#define REG_NAMERD_API_REQ_KEY      "API_REQ_METHOD"
#define REG_NAMERD_API_REQ_DEF      "GET"

#define REG_NAMERD_API_SCHEME_KEY   "API_SCHEME"
#define REG_NAMERD_API_SCHEME_DEF   "http"

#define REG_NAMERD_API_ENV_KEY      "API_ENVIRONMENT"
#define REG_NAMERD_API_ENV_DEF      "default"

#define REG_NAMERD_DTAB_KEY         "DTAB"
#define REG_NAMERD_DTAB_DEF         ""


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         iter,
                            unsigned int  unused);

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static int/*bool*/ s_Update     (SERV_ITER, const char*, int);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, NULL/*Feedback*/, s_Update, s_Reset, s_Close, "NAMERD"
};

static EHTTP_HeaderParse s_ParseHeader(const char* header,
                                       void*       iter,
                                       int         server_error);

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SNAMERD_Data {
    short/*bool*/  eof;  /* no more resolves */
    short/*bool*/  fail; /* no more connects */
    short/*bool*/  done; /* all endpoints have been processed */
    SConnNetInfo*  net_info;
    SLB_Candidate* cand;
    size_t         n_cand;
    size_t         a_cand;
};


/* Extra-verbose tracing to make following nested functions easier. */
#define EXTRA_VERBOSE_CALL_TRACING 1
#ifdef EXTRA_VERBOSE_CALL_TRACING

static int s_nest = 0; /* trace nest level */

#define NEST_PFX    ".........................................................."
#define NEST_PFXX   "**********************************************************"
#define MAX_NEST    ((int)(sizeof(NEST_PFX)-1))

#define PFXI  (s_nest++ > MAX_NEST ? NEST_PFXX : NEST_PFX + MAX_NEST - s_nest+1)
#define PFXO  (--s_nest > MAX_NEST ? NEST_PFXX : NEST_PFX + MAX_NEST - s_nest  )

#define TIN( fmt            )   CORE_TRACEF(("%s[ " fmt, PFXI            ));
#define TIN1(fmt, arg       )   CORE_TRACEF(("%s[ " fmt, PFXI, arg       ));
#define TIN2(fmt, arg1, arg2)   CORE_TRACEF(("%s[ " fmt, PFXI, arg1, arg2));

#define TOUT( fmt            )  CORE_TRACEF(("%s] " fmt, PFXO            ));
#define TOUT1(fmt, arg       )  CORE_TRACEF(("%s] " fmt, PFXO, arg       ));

#else

#define TIN( fmt            )
#define TIN1(fmt, arg       )
#define TIN2(fmt, arg1, arg2)

#define TOUT( fmt            )
#define TOUT1(fmt, arg       )

#endif /* EXTRA_VERBOSE_CALL_TRACING */


/* Some static variables needed only to support testing with mock data.
    Testing with mock data is currently limited to single-threaded tests. */
static int          s_initialized = 0;
static BUF          s_mock_buf = NULL;
static const char*  s_mock_body = NULL;


/* Set up the ability to flexibly use either a file or http connector for
    reading from.  This will enable higher performance normal operation while
    also allowing file input for testing with minimal code impact.
    */
static CONNECTOR s_CreateConnectorHttp(SERV_ITER iter);
static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter);

typedef CONNECTOR (*fCreateConnector)(SERV_ITER iter);
static fCreateConnector s_CreateConnector = s_CreateConnectorHttp;


static CONNECTOR s_CreateConnectorHttp(SERV_ITER iter)
{
    CORE_TRACE("s_CreateConnectorHttp()");
    struct SNAMERD_Data*    data = (struct SNAMERD_Data*) iter->data;

    return HTTP_CreateConnectorEx(data->net_info, fHTTP_Flushable,
                s_ParseHeader, iter/*data*/, s_Adjust, 0/*cleanup*/);
}


static void s_DestroyMockBuf(void)
{
    if (s_mock_buf)
        BUF_Destroy(s_mock_buf);
    s_mock_buf = NULL;
}


static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter)
{
    CORE_TRACE("s_CreateConnectorMemory()");
    assert(s_mock_body);

    /* Reset buffer for each connector */
    s_DestroyMockBuf();

    BUF_Append(&s_mock_buf, (void*)s_mock_body, strlen(s_mock_body));

    return MEMORY_CreateConnectorEx(s_mock_buf, 0);
}


static void s_Quit(void)
{
    s_DestroyMockBuf();
    CORE_LOCK_WRITE;
    s_initialized = 0;
    CORE_UNLOCK;
}


static void s_Init(void)
{
    CORE_LOCK_READ;
    if (s_initialized) {
        CORE_UNLOCK;
        return;
    }
    CORE_UNLOCK;

    if (atexit(s_Quit) == 0) {
        CORE_LOCK_WRITE;
        s_initialized = 1;
        CORE_UNLOCK;
    } else {
        static int once = 0;
        if ( ! once++) {
            CORE_LOG_X(eNSub_Libc, eLOG_Error,
                       "Error registering atexit function.");
        }
    }
}


static EIO_Status s_CONN_Create(SERV_ITER iter, CONNECTOR* c_p, CONN* conn_p)
{
    EIO_Status  status = eIO_Unknown;

    TIN("s_CONN_Create()");

    /* require valid, NULL pointers */
    assert(c_p     &&  ! *c_p   );
    assert(conn_p  &&  ! *conn_p);

    *c_p = s_CreateConnector(iter);
    if (*c_p) {
        status = CONN_Create(*c_p, conn_p);
        if (status != eIO_Success) {
            CORE_LOGF_X(eNSub_Connect, eLOG_Error,
                ("Unable to create connection, status = %s.",
                 IO_StatusStr(status)));
            if ((*c_p)->destroy  &&  (*c_p)->handle)
                (*c_p)->destroy(*c_p);
            *c_p    = NULL;
            *conn_p = NULL;
        }
    } else {
        CORE_LOG_X(eNSub_Connect, eLOG_Error, "Unable to create connector.");
    }

    TOUT("s_CONN_Create()");
    return status;
}


static void s_CONN_Destroy(CONNECTOR* c_p, CONN* conn_p)
{
    assert(conn_p);
    if (*conn_p) CONN_Close(*conn_p);
    *c_p    = NULL;
    *conn_p = NULL;
    s_DestroyMockBuf();
}


/* Update a dtab value by appending another entry. */
static void s_UpdateDtab(char** dest_dtab_p, char* src_dtab, int* success_p)
{
    char* new_dtab = NULL;
    char enc_dtab[MAX_QRY_STR_LEN + 1];
    size_t new_size, src_size, enc_size;

    TIN2("s_UpdateDtab(\"%s\") -- old dtab = \"%s\"", src_dtab,
        *dest_dtab_p ? *dest_dtab_p : "");

    if ( ! *success_p) {
        TOUT("s_UpdateDtab() -- prior no success");
        return;
    }
    if ( ! *src_dtab) {
        TOUT("s_UpdateDtab() -- prior no dtab");
        return;
    }

    /* Ignore header label if it was included. */
    if (strncasecmp(src_dtab, DTAB_HDR_FIELD_NAME ":",
                    sizeof(DTAB_HDR_FIELD_NAME) + 1/*':'*/ - 1/*'\0'*/) == 0)
    {
        /* advance start past name, colon, and any whitespace */
        src_dtab += sizeof(DTAB_HDR_FIELD_NAME) + 1/*':'*/ - 1/*'\0'*/;
        while (*src_dtab == ' '  ||  *src_dtab == '\t') ++src_dtab;
    }

    /* Dtabs passed as query string parameter must be url-encoded, for example:
     *  from:   "/lbsm/bounce=>/service/xyz"
     *  to:     "%2Flbsm%2Fbounce%3D%3E%2Fservice%2Fxyz"
     */
    URL_Encode(src_dtab, strlen(src_dtab), &src_size,
               enc_dtab, MAX_QRY_STR_LEN, &enc_size);
    enc_dtab[enc_size] = NIL; /* not done by URL_Encode() */

    /* If dtab already has an entry then append a semicolon and the new dtab. */
    if (*dest_dtab_p  &&  **dest_dtab_p) {
        new_size = strlen(*dest_dtab_p) + 3/* "%3B" */ + strlen(enc_dtab) + 1;
        new_dtab = (char*)realloc(*dest_dtab_p, new_size);
        if (new_dtab) {
            strcat(new_dtab, "%3B"); /* url-encoded ';' separator */
            strcat(new_dtab, enc_dtab);
        }
    } else {
        /* Dtab didn't exist yet, so just clone it. */
        new_dtab = strdup(enc_dtab);
    }
    if ( ! new_dtab) {
        *success_p = 0;
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical, "Couldn't alloc for dtab.");
        TOUT("s_UpdateDtab() -- bad alloc");
        return;
    }

    /* Update the caller's pointer. */
    *dest_dtab_p = new_dtab;

    TOUT1("s_UpdateDtab() -- new dtab = \"%s\"", new_dtab);
}


static TReqMethod s_GetReqMethod()
{
    char val[20];

    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_REQ_KEY, val, sizeof(val)-1,
        REG_NAMERD_API_REQ_DEF))
    {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for request method.");
        return eReqMethod_Any;
    }
    if ( ! *val) return eReqMethod_Any;
    if (strcasecmp(val, "GET") == 0) return eReqMethod_Get;
    if (strcasecmp(val, "POST") == 0) return eReqMethod_Post;
    if (strcasecmp(val, "GET11") == 0) return eReqMethod_Get11;
    if (strcasecmp(val, "POST11") == 0) return eReqMethod_Post11;

    return eReqMethod_Any;
}


static EBURLScheme s_GetScheme()
{
    char val[12];

    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_SCHEME_KEY, val, sizeof(val)-1,
        REG_NAMERD_API_SCHEME_DEF))
    {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme.");
        return eURL_Unspec;
    }
    if ( ! *val) return eURL_Unspec;
    if (strcasecmp(val, "http") == 0) return eURL_Http;
    if (strcasecmp(val, "https") == 0) return eURL_Https;

    return eURL_Unspec;
}


static unsigned short s_GetProxyPort()
{
    char val[24];
    char reg_def_port[24];
    unsigned short port;

    sprintf(reg_def_port, "%hu", REG_NAMERD_PROXY_PORT_DEF);
    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_PROXY_PORT_KEY, val, sizeof(val)-1,
        reg_def_port))
    {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for proxy port.");
        return 0;
    }
    if ( ! *val)
        return 0;
    if (sscanf(val, "%hu", &port) == 1)
        return port;

    return 0;
}


static void s_RemoveCand(struct SNAMERD_Data* data, size_t n, int free_info)
{
    CORE_TRACE("s_RemoveCand() Removing endpoint.");
    assert(n >= 0  &&  n < data->n_cand);
    if (free_info)
        free((void*) data->cand[n].info);
    if (n < --data->n_cand) {
        memmove(data->cand + n, data->cand + n + 1,
                (data->n_cand - n) * sizeof(*data->cand));
    }
    if (data->n_cand == 0)
        data->done = 1;
}


static void s_RemoveStandby(struct SNAMERD_Data* data)
{
    double  max_rate = 0.0;
    int     all_standby = 1, i;

    /*  basic logic:
        if any endpoints have rate >= LBSM_STANDBY_THRESHOLD
            discard all endpoints with rate < LBSM_STANDBY_THRESHOLD
        else
            discard all endpoints with rate < highest rate
    */

    for (i = 0;  i < (int)data->n_cand;  ++i) {

        if (max_rate < data->cand[i].info->rate)
            max_rate = data->cand[i].info->rate;

        if (data->cand[i].info->rate >= LBSM_STANDBY_THRESHOLD)
            all_standby = 0;
    }

    for (i = (int)data->n_cand - 1;  i >= 0;  --i) {
        if (data->cand[i].info->rate <
                (all_standby ? max_rate : LBSM_STANDBY_THRESHOLD))
            s_RemoveCand(data, (size_t)i, 1);
    }
}


static int/*bool*/ s_AddServerInfo(struct SNAMERD_Data* data, SSERV_Info* info)
{
    const char* name = SERV_NameOfInfo(info);
    size_t i;

    /* First check that the new server info updates an existing one */
    for (i = 0;  i < data->n_cand;  ++i) {
        if (strcasecmp(name, SERV_NameOfInfo(data->cand[i].info)) == 0
            &&  SERV_EqualInfo(info, data->cand[i].info))
        {
            /* Replace older version */
            CORE_TRACE("Replaced older version.");
            free((void*) data->cand[i].info);
            data->cand[i].info   = info;
            data->cand[i].status = info->rate;

            return 1;
        }
    }

    /* Grow candidates container at capacity - trigger growth when there's no
        longer room for a new entry. */
    if (data->a_cand == 0  ||  data->n_cand >= data->a_cand) {
        size_t n = data->a_cand + 10;
        SLB_Candidate* temp =
            (SLB_Candidate*)realloc(data->cand, n * sizeof(*temp));
        if ( ! temp) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                "Failed to reallocate memory for new candidates.");
            return 0;
        }
        data->cand = temp;
        data->a_cand = n;
    }

    /* Add the new service to the list */
#if defined(_DEBUG)  &&  ! defined(NDEBUG)
    {{
        char hostbuf[40];
        SOCK_ntoa(info->host, hostbuf, sizeof(hostbuf));
        CORE_TRACEF(("Added candidate %s:%hu with svc_type '%s'.",
            hostbuf, info->port, SERV_TypeStr(info->type)));
    }}
#endif
    data->cand[data->n_cand].info   = info;
    data->cand[data->n_cand].status = info->rate;
    data->n_cand++;
    data->done = 0;

    return 1;
}


/*  Parse the "addrs[i].meta.expires" JSON from the namerd API, and adjust
    it according to the local timezone/DST to get the UTC epoch time.
    This function is not meant to be a generic ISO-8601 parser.  It expects
    the namerd API to return times in the format: "2017-03-29T23:02:55Z"
    Unfortunately, strptime is not supported at all on Windows, and doesn't
    support the "%z" format on Unix, so some custom parsing is required.
    */
static TNCBI_Time s_ParseExpires(time_t tt_now, const char* expires)
{
    int     tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec;
    char    tm_zulu;

    assert(expires);

    if (sscanf(expires, "%d-%d-%dT%d:%d:%d%c", &tm_year, &tm_mon, &tm_mday,
            &tm_hour, &tm_min, &tm_sec, &tm_zulu) != 7  ||
        tm_year < 2017  ||
        (tm_mon < 0  || tm_mon > 11)  ||
        (tm_mday < 1  || tm_mday > 31)  ||
        (tm_hour < 0  || tm_hour > 23)  ||
        (tm_min < 0  || tm_min > 59)  ||
        (tm_sec < 0  || tm_sec > 60)  ||
        tm_zulu != 'Z')
    {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
            ("Unexpected JSON {\"addrs[i].meta.expires\"} "
             "value '%s'.", expires));
        return 0;
    }
    tm_year -= 1900;    /* years since 1900 */
    tm_mon  -= 1;       /* months since January: 0-11 */

    /* Get the UTC epoch time for the expires value. */
    struct tm   tm_expires;
    tm_expires.tm_year  = tm_year;
    tm_expires.tm_mon   = tm_mon;
    tm_expires.tm_mday  = tm_mday;
    tm_expires.tm_hour  = tm_hour;
    tm_expires.tm_min   = tm_min;
    tm_expires.tm_sec   = tm_sec;
    tm_expires.tm_isdst = 0;
    tm_expires.tm_wday  = 0;
    tm_expires.tm_yday  = 0;

    /* Adjust for time diff between local and UTC, which should
        correspond to 3600 x (number of time zones from UTC),
        i.e. diff between current TZ (UTC-12 to UTC+14) and UTC. */
    double      tdiff;
    struct tm   tm_now;
    CORE_LOCK_WRITE;
    tm_now = *gmtime(&tt_now);
    CORE_UNLOCK;
    tdiff = difftime(mktime(&tm_expires), mktime(&tm_now));
    if (tdiff < -14.0 * 3600.0  ||  tdiff > 12.0 * 3600.0) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
            ("Unexpected JSON {\"addrs[i].meta.expires\"} "
             "value '%s' - tdiff = %lf", expires, tdiff));
        return 0;
    }
    TNCBI_Time  timeval = (TNCBI_Time)((double)tt_now + tdiff);
    /*CORE_TRACEF(
        ("expires: %s   tdiff %lf   now " FMT_TIME_T "   info->time %u",
         expires, tdiff, tt_now, timeval));*/

    return timeval;
}


/*  Parsing the response requires having the entire response in
    one buffer.  Therefore, realloc as necessary.

    If the realloc'd size significantly exceeds the needed size, be nice and
    realloc it back down to match the needed size.
    */
static EIO_Status s_ReadFullResponse(CONN conn, char** bufp,
    SConnNetInfo* net_info)
{
    static size_t   buf_len_steps[] = {10100, 300100, 10100100};
                    /* just try a limited number of realloc sizes */
    int             max_steps = sizeof(buf_len_steps)/sizeof(buf_len_steps[0]);
    size_t          waste_threshold = 50100;
    size_t          total_read = 0;
    size_t          buf_len, current_size, current_read;
    char*           new_buf;
    int             num_steps;

    TIN("s_ReadFullResponse()");

    assert(bufp);
    assert(net_info);

    for (num_steps = 0;  num_steps < max_steps;  ++num_steps) {

        /* Expand the buffer to the next step size. */
        buf_len = buf_len_steps[num_steps];
        new_buf = (char*)realloc(*bufp, buf_len);
        if ( ! new_buf) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                "Failed to allocate memory for response buffer.");
            if (*bufp) {
                free(*bufp);
                *bufp = NULL;
            }
            TOUT("s_ReadFullResponse() -- bad alloc");
            return eIO_Unknown;
        }
        *bufp = new_buf;

        /* Read the next block of data. */
        current_size = buf_len - total_read - 1/* zero-terminate */;
        EIO_Status status = CONN_Read(conn, *bufp + total_read, current_size,
                                      &current_read, eIO_ReadPlain);
        total_read += current_read;
        (*bufp)[total_read] = NIL; /* zero-terminate */

        /* Handle problems. */
        if (status != eIO_Success) {
            if (status == eIO_Closed) {
                CORE_LOG_X(eNSub_HttpRead, eLOG_Error,
                    "Connection unexpectedly closed.");
            } else {
                CORE_LOGF_X(eNSub_HttpRead, eLOG_Error,
                    ("Read error: %s", IO_StatusStr(status)));
            }
            free(*bufp);
            *bufp = NULL;
            TOUT("s_ReadFullResponse() -- wait");
            return status;
        }

        /* We've successfully fetched all data. */
        if (current_read < current_size) {
            break;
        }

        /* If (status == eIO_Success  &&  current_read == current_size)
           Then no problem, but we can't tell if we've gotten all the data
            (in the rare case that it exactly matches the bufsize), or if we
            need to fetch more data.  Therefore, just iterate again. */
    }

    /* Reduce the buffer size if there's a lot of wasted space. */
    if (buf_len - total_read > waste_threshold) {
        new_buf = (char*)realloc(*bufp, total_read + 1/* zero-terminate */);
        if (new_buf) {
            *bufp = new_buf;
        }
    }

    CORE_TRACEF(("Got response: %s", *bufp));

    TOUT("s_ReadFullResponse()");
    return eIO_Success;
}


static int/*bool*/ s_ParseResponse(SERV_ITER iter, CONN conn)
{
    struct SNAMERD_Data*    data = (struct SNAMERD_Data*) iter->data;
    SConnNetInfo*           net_info = data->net_info;
    x_JSON_Value*           root_value = NULL;
    char*                   response = NULL;
    int/*bool*/             retval = 0;

    TIN("s_ParseResponse()");

    if (eIO_Success == s_ReadFullResponse(conn, &response, net_info)) {
        x_JSON_Object *root_obj;
        x_JSON_Array  *address_arr;
        const char    *success_type;
        size_t         it;

        /* root object */
        root_value = x_json_parse_string(response);
        if ( ! root_value) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                "Response couldn't be parsed as JSON.");
            goto out;
        }
        if (x_json_value_get_type(root_value) != JSONObject) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                "Response root name is not a JSON object.");
            goto out;
        }
        root_obj = x_json_value_get_object(root_value);
        if ( ! root_obj) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                "Couldn't get JSON root object.");
            goto out;
        }
#if defined(_DEBUG)  &&  ! defined(NDEBUG)
        char json[9999];
        if (x_json_serialize_to_buffer_pretty(
                root_value, json, sizeof(json)-1) == JSONSuccess)
            CORE_TRACEF(("Got JSON:\n%s", json));
        else
            CORE_LOG_X(eNSub_Json, eLOG_Error, "Couldn't serialize JSON");
#endif

        /* top-level {"type" : "bound"} expected for successful connection */
        success_type = x_json_object_get_string(root_obj, "type");
        if ( ! success_type) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                "Couldn't get JSON {\"type\"} value.");
            goto out;
        }
        if (strcmp(success_type, "bound") != 0) {
            CORE_LOGF_X(eNSub_NoService, eLOG_Error,
                ("Service \"%s\" appears to be unknown.", iter->name));
            goto out;
        }

        /* top-level {"addrs" : []} contains endpoint data */
        address_arr = x_json_object_get_array(root_obj, "addrs");
        if ( ! address_arr) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                "Couldn't get JSON {\"addrs\"} array.");
            goto out;
        }

        /* Note: top-level {"meta" : {}} not currently used */

        /* Iterate through addresses, adding to "candidates". */
        for (it = 0;  it < x_json_array_get_count(address_arr);  ++it) {
            x_JSON_Object   *address;
            const char      *host, *extra, *svc_type, *mode;
            const char      *cpfx, *ctype;
            const char      *local;
            const char      *priv;
            const char      *stateful;
            char            *server_description;
            double          rate;
            int             port;

            /*  JSON|SSERV_Info|variable mapping to format string:
                meta.expires|time|time -------------------------------+
                meta.stateful|mode|stateful ---------------------+    |
                meta.rate|rate|rate ---------------------------+ |    |
                meta.mode|site|priv ----------------------+    | |    |
                meta.mode|site|local -------------------+ |    | |    |
                meta.contentType|mime_*|cpfx,ctype      | |    | |    |
                meta.extra|extra|extra -------+  |      | |    | |    |
                port|port|port ------------+  |  |      | |    | |    |
                ip|host|host -----------+  |  |  |      | |    | |    |
                meta.serviceType|       |  |  |  |      | |    | |    |
                    type|svc_type ---+  |  |  |  |      | |    | |    |
                                     [] [] [] [] [__]   [][]   [][]   [] */
            const char* descr_fmt = "%s %s:%u %s %s%s L=%s%s R=%s%s T=%u";
            /*  NOTE: Some fields must not be included in certain situations
                because SERV_ReadInfoEx() does not expect them in those
                situations, and if they are present then SERV_ReadInfoEx()
                will prematurely terminate processing the description.
                Specifically:
                    do not include      when
                    --------------      ---------------------
                    P=                  type=DNS
                    S=                  type=DNS or type=HTTP
                */

            /* get a handle on the object for this iteration */
            address = x_json_array_get_object(address_arr, it);
            if ( ! address) {
                CORE_LOG_X(eNSub_Json, eLOG_Error,
                    "Couldn't get JSON {\"addrs[i]\"} object.");
                goto out;
            }

            /* SSERV_Info.host <=== addrs[i].ip */
            host = x_json_object_get_string(address, "ip");
            if ( ! host  ||   ! *host) {
                CORE_LOG_X(eNSub_Json, eLOG_Error,
                    "Couldn't get JSON {\"addrs[i].ip\"} value.");
                goto out;
            }

            /* SSERV_Info.port <=== addrs[i].port */
            /* Unfortunately the x_json_object_get_number() function does
                not distinguish failure from a legitimate zero value.  :-/
                Therefore, first explicitly check for the value. */
            if ( ! x_json_object_has_value_of_type(
                    (const x_JSON_Object *)address,
                    (const char *)"port", JSONNumber))
            {
                CORE_LOG_X(eNSub_Json, eLOG_Error,
                    "Couldn't get JSON {\"addrs.port\"} name.");
                goto out;
            }
            port = (int)x_json_object_get_number(address, "port");
            if (port <= 0  ||  port > 65535) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Invalid JSON {\"addrs[i].port\"} value (%d).", port));
                goto out;
            }

            /* SSERV_Info.type <=== addrs[i].meta.serviceType */
            svc_type = x_json_object_dotget_string(address, "meta.serviceType");
            if ( ! svc_type  ||   ! *svc_type)
                svc_type = "HTTP";

            /* Make sure the service type matches an allowed type. */
            ESERV_Type  type;
            if ( ! SERV_ReadType(svc_type, &type)  ||
                 ! (iter->types & type))
            {
                CORE_TRACEF((
                    "Ignoring endpoint %s:%d with unallowed svc_type '%s'.",
                    host, port, svc_type));
                continue;
            }
            CORE_TRACEF((
                "Parsed endpoint %s:%d with allowed svc_type '%s'.",
                host, port, svc_type));

            /* SSERV_Info.mode <=== addrs[i].meta.stateful */
            if (x_json_object_dothas_value_of_type(address, "meta.stateful",
                JSONBoolean))
            {
                int st = x_json_object_dotget_boolean(address, "meta.stateful");
                if (st == 0) {
                    stateful = "";
                } else if (st == 1) {
                    stateful = " S=YES";
                } else {
                    CORE_LOGF_X(eNSub_Json, eLOG_Error,
                       ("Invalid JSON {\"addrs[i].meta.stateful\"} value (%d).",
                         st));
                    goto out;
                }
            } else {
                stateful = "";
            }

            /* SSERV_Info.site <=== addrs[i].meta.mode */
            local = "NO";
            priv = "";
            if (x_json_object_dothas_value_of_type(address, "meta.mode",
                JSONString))
            {
                mode = x_json_object_dotget_string(address, "meta.mode");
                if ( ! mode  ||   ! *mode) {
                    CORE_LOG_X(eNSub_Json, eLOG_Error,
                        "Couldn't get JSON {\"addrs[i].meta.mode\"} value.");
                    goto out;
                }
                if (strcmp(mode, "L") == 0) {
                    local = "YES";
                } else if (strcmp(mode, "H") == 0) {
                    local = "YES";
                    priv = " P=YES";
                } else {
                    CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Unexpected JSON {\"addrs[i].meta.mode\"} value '%s'.",
                         mode));
                    goto out;
                }
            }

            /* SSERV_Info.rate <=== addrs[i].meta.rate */
            if (x_json_object_dothas_value_of_type(address, "meta.rate",
                JSONNumber))
            {
                rate = x_json_object_dotget_number(address, "meta.rate");
            } else {
                rate = 0.0;
            }
            /* verify magnitude */
            if (rate < 0.0  ||  rate > 10000.0) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Unexpected JSON {\"addrs[i].meta.rate\"} value '%lf'.",
                     rate));
                goto out;
            }
            /* format with 3-digit precsion for standby; else 2-digits */
            char rate_str[12];
            sprintf(rate_str, "%.*lf",
                (rate < LBSM_STANDBY_THRESHOLD ? 3 : 2), rate);

            /* SSERV_Info.time <=== addrs[i].meta.expires */
            time_t      tt_now = time(0);
            TNCBI_Time  timeval = (TNCBI_Time)(tt_now + LBSM_DEFAULT_TIME);
            if (x_json_object_dothas_value_of_type(address, "meta.expires",
                JSONString))
            {
                timeval = s_ParseExpires(tt_now,
                    x_json_object_dotget_string(address, "meta.expires"));
                if ( ! timeval)
                    goto out;
            } else {
                static int once = 0;
                if ( ! once++) {
                    CORE_LOGF_X(eNSub_Json, eLOG_Warning,
                        ("Missing JSON {\"addrs[i].meta.expires\"} - "
                         "using current time (" FMT_TIME_T
                         ") + default expiration "
                         "(%u) = %u", tt_now, LBSM_DEFAULT_TIME, timeval));
                }
            }

            /* SSERV_Info.mime_t
               SSERV_Info.mime_s
               SSERV_Info.mime_e <=== addrs[i].meta.contentType */
            ctype = x_json_object_dotget_string(address, "meta.contentType");
            cpfx  = (ctype ? " C=" : "");
            ctype = (ctype ? ctype : "");

            /* SSERV_Info.extra <=== addrs[i].meta.extra */
            extra = x_json_object_dotget_string(address, "meta.extra");
            if ( ! extra  ||  ! *extra) {
                if (strcmp(svc_type, "HTTP") == 0) {
                    CORE_LOG_X(eNSub_Json, eLOG_Error,
                            "Namerd API did not return a path in meta.extra "
                            "JSON for an HTTP type service");
                    extra = "/ERROR/namerd/API/did/not/return/a/path/in"
                            "/meta.extra/JSON/for/an/HTTP/type/service";
                } else {
                    extra = "";
                }
            }

            /* Prepare description */
            size_t length;
            length = strlen(descr_fmt) + strlen(svc_type) + strlen(host) +
                     5 /*length of port*/ + strlen(extra) + strlen(cpfx) +
                     strlen(ctype) + strlen(local) + strlen(priv) +
                     strlen(rate_str) + strlen(stateful) +
                     20/*length of timeval*/;
            server_description = (char*)malloc(sizeof(char) * length);
            if ( ! server_description) {
                CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                    "Couldn't alloc for server description.");
                goto out;
            }
            sprintf(server_description, descr_fmt, svc_type, host,
                    port, extra, cpfx, ctype, local, priv, rate_str, stateful,
                    timeval);

            /* Parse description into SSERV_Info */
            CORE_TRACEF(
                ("Parsing server description: '%s'", server_description));
            SSERV_Info* info = SERV_ReadInfoEx(server_description,
                iter->name, 0);
            /*CORE_TRACEF(
                ("Parsed server description; expires=%u", info->time));*/

            if ( ! info) {
                CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                    ("Unable to add server info with description '%s'.",
                     server_description));
                free(server_description);
                continue;
            }
            free(server_description);

            /* Add new info to collection */
            if (s_AddServerInfo(data, info)) {
                retval = 1; /* at least one info added */
                /* Note: successful s_AddServerInfo() frees 'info' */
            } else {
                CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                    "Unable to add server info.");
                free(info); /* not freed by failed s_AddServerInfo() */
                goto out;
            }
        }

        /* Filter out standby endpoints */
        s_RemoveStandby(data);
    }

out:
    if (response)   free(response);
    if (root_value) x_json_value_free(root_value);
    TOUT("s_ParseResponse()");
    return retval;
}


/* Parse buffer and extract DTab-Local header, if present. */
static char* s_GetDtabHeaderFromBuf(const char* buf)
{
    char* start = (char*)buf;
    char* end;
    char* dup_hdr;

    TIN1("s_GetDtabHeaderFromBuf(\"%s\")", buf ? buf : "");

    if (start  &&  strncasecmp(start, DTAB_HDR_FIELD_NAME ":",
                       sizeof(DTAB_HDR_FIELD_NAME) + 1/*':'*/ - 1/*'\0'*/) == 0)
    {
        /* advance start past name, colon, and any whitespace */
        start += sizeof(DTAB_HDR_FIELD_NAME) + 1;
        while (*start == ' '  ||  *start == '\t') ++start;

        /* advance end to \0 or eol (excluding eol) */
        end = start;
        while (*end  &&  *end != '\r'  &&  *end != '\n') ++end;

        /* clone the header value */
        dup_hdr = (char*)malloc(end - start + 1);
        if ( ! dup_hdr) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't alloc for dtab header value.");
            TOUT("s_GetDtabHeaderFromBuf() -- bad alloc");
            return NULL;
        }
        memcpy(dup_hdr, start, end - start);
        dup_hdr[end - start] = NIL;
        TOUT1("s_GetDtabHeaderFromBuf() -- got dtab header \"%s\"", dup_hdr);
        return dup_hdr;
    }

    TOUT("s_GetDtabHeaderFromBuf()");
    return NULL;
}


/* If there's a dtab in the user header, add it to the caller's dtab. */
static void s_UpdateDtabFromUserHeader(char** dtab_p, int* success_p,
                                       SConnNetInfo* net_info)
{
    char* dtab = NULL;

    TIN2("s_UpdateDtabFromUserHeader(\"%s\") -- success=%d",
        net_info->http_user_header ? net_info->http_user_header : "",
        *success_p);

    if ( ! *success_p) {
        TOUT("s_UpdateDtabFromUserHeader() -- prior no success");
        return;
    }

    dtab = s_GetDtabHeaderFromBuf(net_info->http_user_header);
    if (dtab) {
        /*ConnNetInfo_DeleteUserHeader(net_info, DTAB_HDR_FIELD_NAME);*/
        s_UpdateDtab(dtab_p, dtab, success_p);
        free(dtab);
    }

    TOUT("s_UpdateDtabFromUserHeader()");
}


/* If there's a dtab in the registry, add it to the caller's dtab. */
static void s_UpdateDtabFromRegistry(char** dtab_p, int* success_p,
                                     const char* service)
{
    char val[MAX_QRY_STR_LEN + 1];

    TIN2("s_UpdateDtabFromRegistry(\"%s\") -- success=%d",
        service ? service : "", *success_p);

    if ( ! *success_p) {
        TOUT("s_UpdateDtabFromRegistry() -- prior no success");
        return;
    }

    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_DTAB_KEY, val, MAX_QRY_STR_LEN,
        REG_NAMERD_DTAB_DEF))
    {
        *success_p = 0;
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for dtab from registry.");
        TOUT("s_UpdateDtabFromRegistry() -- bad alloc");
        return;
    }

    s_UpdateDtab(dtab_p, val, success_p);

    TOUT("s_UpdateDtabFromRegistry()");
}


/*  If net_info->http_user_header contains a DTab-Local header, that value
 *  must be moved to net_info->args, which in turn populates the "dtab"
 *  parameter in the HTTP query string.  It must not be in a header because
 *  if it was, it would apply to the namerd service itself, not the requested
 *  service.  Instead, the namerd service uses the dtab param for resolution.
 *
 *  Thus, this function builds the dtab param from (highest priority first):
 *      *   CONN_DTAB environment variable
 *      *   DTab-Local header in net_info->http_user_header
 */
static int/*bool*/ s_ProcessDtab(SConnNetInfo* net_info)
{
#define  DTAB_ARGS_SEP  "&dtab="
    int/*bool*/ success = 1;
    char* dtab = NULL;

    TIN("s_ProcessDtab()");

    /* Dtab precedence (highest first): registry > user_header */
    s_UpdateDtabFromRegistry(&dtab, &success, net_info->svc);
    s_UpdateDtabFromUserHeader(&dtab, &success, net_info);

    if (success  &&  dtab) {
        if (strlen(net_info->args) + strlen(DTAB_ARGS_SEP) +
            strlen(dtab) < MAX_ARGS_LEN)
        {
            strcat(net_info->args, DTAB_ARGS_SEP);
            strcat(net_info->args, dtab);
        } else {
            CORE_LOG_X(eNSub_TooLong, eLOG_Error, "Dtab too long.");
            success = 0;
        }
    }

    if (dtab)   free(dtab);

    TOUT("s_ProcessDtab()");
    return success;
#undef  DTAB_ARGS_SEP
}


static EHTTP_HeaderParse s_ParseHeader(const char* header,
                                       void*       iter,
                                       int         server_error)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*)((SERV_ITER) iter)->data;
    int code = 0/*success code if any*/;

    TIN1("s_ParseHeader(\"%s\")", header);

    if (server_error == 400  ||  server_error == 403  ||  server_error == 404) {
        data->fail = 1/*true*/;
    } else if (sscanf(header, "%*s %d", &code) < 1) {
        data->eof = 1/*true*/;
        TOUT("s_ParseHeader() -- eof=true");
        return eHTTP_HeaderError;
    }

    /* check for empty document */
    if (code == 204)
        data->eof = 1/*true*/;

    TOUT("s_ParseHeader()");
    return eHTTP_HeaderSuccess;
}


static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         iter,
                            unsigned int  unused)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*)((SERV_ITER) iter)->data;
    return data->fail ? 0/*no more tries*/ : 1/*may try again*/;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    struct SNAMERD_Data*    data = (struct SNAMERD_Data*) iter->data;
    SConnNetInfo*           net_info = data->net_info;
    CONNECTOR               c = NULL;
    CONN                    conn = NULL;
    int/*bool*/             retval = 0;

    TIN("s_Resolve()");
    assert( ! (data->eof | data->fail));
    assert( !! net_info->stateless ==  !! iter->stateless);

    /* Handle DTAB, if present. */
    s_ProcessDtab(net_info);

    /* Create connector and connection, and fetch and parse the response. */
    if (s_CONN_Create(iter, &c, &conn) == eIO_Success) {
        retval = s_ParseResponse(iter, conn);
    }
    s_CONN_Destroy(&c, &conn);

    TOUT("s_Resolve()");
    return retval;
}


static int/*bool*/ s_IsUpdateNeeded(TNCBI_Time now, struct SNAMERD_Data *data)
{
    /* Note: Because the namerd API does not supply rate data for many
        of the services it resolves, a rate-based algorithm can't be used.
        It could end up in an infinite loop because the total could be zero,
        thus causing immediate repeated updates of the same server, forever.
        Therefore, use a simpler algorithm that is just:
            An update is needed if:
            *   there are no candidates; or
            *   any candidates are expired.
        */
    int any_expired = 0, i;

    for (i = (int)data->n_cand - 1;  i >= 0;  --i) {
        const SSERV_Info* info = data->cand[i].info;
        if (info->time < now) {
            TNCBI_Time  tnow = (TNCBI_Time)time(0);
            CORE_TRACE("Endpoint expired:");
            CORE_TRACEF((
                "    info->time (%u) < iter->time (%u)   now (%u)",
                info->time, now, tnow));
            CORE_TRACEF((
                "    iter->time - info->time (%d)     now - iter->time (%d)",
                now - info->time, tnow - now));
            any_expired = 1;
            if ((size_t)i < --data->n_cand)
                s_RemoveCand(data, (size_t)i, 1);
        }
    }

    return any_expired  ||  data->n_cand == 0;
}


static int/*bool*/ s_Update(SERV_ITER iter, const char* text, int code)
{
    /*struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;*/
    int retval = 0;

    TIN2("s_Update(\"%s\", %d)", text ? text : "", code);

    TOUT1("s_Update() -- %supdated", retval ? "" : "not ");
    return retval;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;
    SSERV_Info* info;
    size_t n;

    TIN("s_GetNextInfo()");
    assert(data);

    if (data->n_cand < 1  &&  data->done) {
        data->done = 0;
        TOUT("s_GetNextInfo() -- no cand");
        return NULL;
    }

    if (s_IsUpdateNeeded(iter->time, data)) {
        if ( ! (data->eof | data->fail))
            s_Resolve(iter);
    }

    /* No need to load-balance - that's already been done by the namerd service.
     * Just take the first one.
     */
    n = 0;
    info       = (SSERV_Info*) data->cand[n].info;
    info->rate =               data->cand[n].status;

    /* Remove returned info */
    s_RemoveCand(data, n, 0);

    if (host_info)
        *host_info = NULL;

    TOUT("s_GetNextInfo()");
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    TIN("s_Reset()");

    if (data) {
        data->eof = data->fail = data->done = 0/*false*/;
        if (data->cand) {
            size_t i;
            assert(data->n_cand <= data->a_cand);
            for (i = 0;  i < data->n_cand;  ++i)
                free((void*) data->cand[i].info);
            data->n_cand = 0;
        }
    }

    TOUT("s_Reset()");
}


static void s_Close(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    TIN("s_Close()");

    /* Make sure s_Reset() has been called. */
    assert(data);
    if (data->n_cand)
        s_Reset(iter);
    assert( ! data->n_cand); /* s_Reset() clears candidates count */

    if (data->cand)
        free(data->cand);

    ConnNetInfo_Destroy(data->net_info);
    free(data);
    iter->data = NULL;

    TOUT("s_Close()");
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern const SSERV_VTable* SERV_NAMERD_Open(SERV_ITER           iter,
                                            const SConnNetInfo* net_info,
                                            SSERV_Info**        info)
{
    struct SNAMERD_Data*    data;
    char                    namerd_env[32];

    TIN1("SERV_NAMERD_Open(\"%s\")", iter->name);

    s_Init();

    assert(iter);
    assert(net_info);

    /* Check that service name is provided - otherwise there is nothing to
     * search for. */
    if ( ! iter->name) {
        CORE_LOG_X(eNSub_BadData, eLOG_Error,
            "\"iter->name\" is NULL, not able to continue SERV_NAMERD_Open");
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }
    assert(iter->name);
    assert(*iter->name);

    /* Prohibit catalog-prefixed services (e.g. "/lbsm/<svc>)". */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
            ("Invalid service name \"%s\" - must not begin with '/'.",
             iter->name));
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }

    /* Check that iter is not a mask. */
    if (iter->ismask) {
        CORE_LOG_X(eNSub_BadData, eLOG_Error,
            "NAMERD doesn't support masks.");
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }

    if ( ! (data = (struct SNAMERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
            "Could not allocate for SNAMERD_Data.");
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }
    iter->data = data;

    if ( ! (data->net_info = ConnNetInfo_Clone(net_info))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical, "Couldn't clone net_info.");
        s_Close(iter);
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }

    if ( ! ConnNetInfo_SetupStandardArgs(data->net_info, iter->name)) {
        CORE_LOG_X(eNSub_BadData, eLOG_Critical,
            "Couldn't set up standard args.");
        s_Close(iter);
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed  = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }

    data->net_info->http_proxy_port = s_GetProxyPort();
    data->net_info->req_method      = s_GetReqMethod();
    data->net_info->scheme          = s_GetScheme();
    data->net_info->port            = 0; /* namerd doesn't support a port */
    data->net_info->host[0]         = NIL;
    data->net_info->path[0]         = NIL;
    data->net_info->args[0]         = NIL;

    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_PROXY_HOST_KEY, data->net_info->http_proxy_host,
        sizeof(data->net_info->http_proxy_host) - 1,
        REG_NAMERD_PROXY_HOST_DEF))
    {
        data->net_info->http_proxy_host[0] = NIL;
    }
    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_HOST_KEY, data->net_info->host, sizeof(net_info->host)-1,
        REG_NAMERD_API_HOST_DEF))
    {
        data->net_info->host[0] = NIL;
    }
    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_PATH_KEY, data->net_info->path, sizeof(net_info->path)-1,
        REG_NAMERD_API_PATH_DEF))
    {
        data->net_info->path[0] = NIL;
    }
    if (ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_ENV_KEY, namerd_env, sizeof(namerd_env)-1,
        REG_NAMERD_API_ENV_DEF))
    {
        if (strlen(data->net_info->path) + strlen(namerd_env) <
            sizeof(net_info->path) - 2) /* -2 for '/' separator and NIL */
        {
            strcat(data->net_info->path, "/");
            strcat(data->net_info->path, namerd_env);
        }
    }
    if ( ! ConnNetInfo_GetValue(REG_NAMERD_SECTION,
        REG_NAMERD_API_ARGS_KEY, data->net_info->args, sizeof(net_info->args)-1,
        REG_NAMERD_API_ARGS_DEF))
    {
        data->net_info->args[0] = NIL;
    } else {
        strcat(data->net_info->args, iter->name);
    }

    if (iter->stateless)
        data->net_info->stateless = 1/*true*/;
    if ((iter->types & fSERV_Firewall)  &&   ! data->net_info->firewall)
        data->net_info->firewall = eFWMode_Adaptive;

    iter->op = &s_op; /*SERV_Update() [from HTTP callback] expects*/
    s_Resolve(iter);
    iter->op = NULL;

    if ( ! data->n_cand  &&  (data->fail  ||
                             ! (data->net_info->stateless  &&
                              data->net_info->firewall)))
    {
        s_Close(iter);
        TOUT("SERV_NAMERD_Open() -- fail");
        return NULL;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)   *info = NULL;

    TOUT("SERV_NAMERD_Open()");
    return &s_op;
}


extern int SERV_NAMERD_SetConnectorSource(const char* mock_body)
{
    s_Init();

    if ( ! mock_body  ||  ! *mock_body) {
        s_CreateConnector = s_CreateConnectorHttp;
        return 1;
    }
    s_mock_body = mock_body;
    s_CreateConnector = s_CreateConnectorMemory;
    return 1;
}
