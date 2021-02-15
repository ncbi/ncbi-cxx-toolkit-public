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
#include "ncbi_linkerd.h"
#include "ncbi_namerd.h"
#include "ncbi_once.h"
#include "parson.h"

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_server_info.h>

#include <ctype.h>
#include <stdlib.h>
#include <time.h>


#ifdef _MSC_VER
#define FMT_SIZE_T      "%llu"
#define FMT_TIME_T      "%llu"
#else
#define FMT_SIZE_T      "%zu"
#define FMT_TIME_T      "%lu"
#endif


#define NCBI_USE_ERRCODE_X   Connect_NamerdLinkerd


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
    eNSub_TooLong         = 8,   /**< data was too long to fit in a buffer */
    eNSub_Logic           = 9    /**< logic error */
    /*eNSub_Unused        = 10   //   reserved*/
};


/*  Registry entry names and default values for NAMERD "SConnNetInfo" fields.
    Note that these are purely for the NAMERD API; they don't relate to any
    other part of the connect library, returned endpoints, or client code.
    Therefore, they are independent of other connect library macros.
    Also, the namerd API doesn't support using a port so there's no macro for
    that.
 */
#define DEF_NAMERD_REG_SECTION     "_NAMERD"

#define REG_NAMERD_API_SCHEME      "API_SCHEME"
#define DEF_NAMERD_API_SCHEME      "http"

#define REG_NAMERD_API_REQ_METHOD  "API_REQ_METHOD"
#define DEF_NAMERD_API_REQ_METHOD  "GET"

#define REG_NAMERD_API_HOST        "API_HOST"
#define DEF_NAMERD_API_HOST        "namerd-api.linkerd.ncbi.nlm.nih.gov"

#define REG_NAMERD_API_PATH        "API_PATH"
#define DEF_NAMERD_API_PATH        "/api/1/resolve"

#define REG_NAMERD_API_ENV         "API_ENVIRONMENT"
#define DEF_NAMERD_API_ENV         "default"

#define REG_NAMERD_API_ARGS        "API_ARGS"
#define DEF_NAMERD_API_ARGS        "path=/service/"

#define REG_NAMERD_PROXY_HOST      "PROXY_HOST"
/*  NAMERD_TODO - "temporarily" support plain "linkerd" on Unix only */
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_CYGWIN)
#define DEF_NAMERD_PROXY_HOST      "linkerd"
#else
#define DEF_NAMERD_PROXY_HOST      \
    "pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#endif

#define REG_NAMERD_PROXY_PORT      "PROXY_PORT"
#define DEF_NAMERD_PROXY_PORT      "4140"

#define REG_NAMERD_DTAB            "DTAB"
#define DEF_NAMERD_DTAB            ""


/* Default rate increase 20% if svc runs locally */
#define NAMERD_LOCAL_BONUS         1.2


/* Apache is limited to around 4000 byte query strings. -- todo: find reference
 * Note that this is approximate and doesn't need to be precise because
 * it's more than should be required for namerd anyway.
 */
#define MAX_QUERY_STRING_LEN       4000


/* Misc. */
#define DTAB_HTTP_HEADER_TAG       "DTab-Local:"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         iter,
                            unsigned int  unused);

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, NULL/*Feedback*/, NULL/*s_Update*/, s_Reset, s_Close, "NAMERD"
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


/* Some static variables needed only to support testing with mock data.
 * Testing with mock data is currently limited to single-threaded tests. */
static const char*  s_mock_body = 0;


/* Set up the ability to flexibly use arbitrary connector for reading from.
 * This will allow different input for testing with minimal code impact.
 */
static CONNECTOR s_CreateConnectorHttp  (SERV_ITER iter);
static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter);

typedef CONNECTOR (*FCreateConnector)(SERV_ITER iter);
static FCreateConnector s_CreateConnector = s_CreateConnectorHttp;


static CONNECTOR s_CreateConnectorHttp(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACE("s_CreateConnectorHttp()");

    return HTTP_CreateConnectorEx(data->net_info, 0/*flags*/,
                s_ParseHeader, iter/*data*/, s_Adjust, 0/*cleanup*/);
}


static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter)
{
    BUF buf;

    CORE_TRACE("s_CreateConnectorMemory()");
    if ( ! s_mock_body) {
        CORE_LOG_X(eNSub_Logic, eLOG_Critical,
                   "Unexpected NULL 's_mock_body' pointer.");
        return 0;
    }

    buf = 0;
    BUF_Append(&buf, s_mock_body, strlen(s_mock_body));
    return MEMORY_CreateConnectorEx(buf, 1/*own it!*/);
}


/* Update a dtab value by appending another entry. */
static void s_UpdateDtab(char** dest_dtab_p, char* src_dtab, int* success_p)
{
    char* new_dtab = NULL;
    char enc_dtab[MAX_QUERY_STRING_LEN + 1];
    size_t new_size, src_size, enc_size;

    CORE_TRACEF(("Entering s_UpdateDtab(\"%s\") -- old dtab = \"%s\"", src_dtab,
        *dest_dtab_p ? *dest_dtab_p : ""));

    if ( ! *success_p) {
        CORE_TRACE("Leaving s_UpdateDtab() -- prior no success");
        return;
    }
    if ( ! *src_dtab) {
        CORE_TRACE("Leaving s_UpdateDtab() -- prior no dtab");
        return;
    }

    /* Ignore header label if it was included. */
    if (strncasecmp(src_dtab, DTAB_HTTP_HEADER_TAG,
                    sizeof(DTAB_HTTP_HEADER_TAG) - 1) == 0)
    {
        /* advance start past name, colon, and any whitespace */
        src_dtab += sizeof(DTAB_HTTP_HEADER_TAG) - 1;
        while (*src_dtab == ' '  ||  *src_dtab == '\t') ++src_dtab;
    }

    /* Dtabs passed as query string parameter must be url-encoded, for example:
     *  from:   "/lbsm/bounce=>/service/xyz"
     *  to:     "%2Flbsm%2Fbounce%3D%3E%2Fservice%2Fxyz"
     */
    URL_Encode(src_dtab, strlen(src_dtab), &src_size,
               enc_dtab, sizeof(enc_dtab) - 1, &enc_size);
    enc_dtab[enc_size] = '\0'; /* not done by URL_Encode() */

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
        CORE_TRACE("Leaving s_UpdateDtab() -- bad alloc");
        return;
    }

    /* Update the caller's pointer. */
    *dest_dtab_p = new_dtab;

    CORE_TRACEF(("Leaving s_UpdateDtab() -- new dtab = \"%s\"", new_dtab));
}


static TReqMethod s_GetReqMethod(void)
{
    char val[20];

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_REQ_METHOD,
                                       val, sizeof(val),
                                       DEF_NAMERD_API_REQ_METHOD)) {
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


static EBURLScheme s_GetScheme(void)
{
    char val[12];

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_SCHEME,
                                       val, sizeof(val),
                                       DEF_NAMERD_API_SCHEME)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme.");
        return eURL_Unspec;
    }
    if ( ! *val) return eURL_Unspec;
    if (strcasecmp(val, "http") == 0) return eURL_Http;
    if (strcasecmp(val, "https") == 0) return eURL_Https;

    return eURL_Unspec;
}


static int s_GetHttpProxy(SConnNetInfo* net_info)
{
    char port_str[24];

    if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port
        &&  net_info->http_proxy_only) {
        return 1/*success*/;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_PROXY_HOST,
                                       net_info->http_proxy_host,
                                       sizeof(net_info->http_proxy_host),
                                       DEF_NAMERD_PROXY_HOST)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't get default http proxy host.");
        return 0/*failure*/;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_PROXY_PORT,
                                       port_str, sizeof(port_str),
                                       DEF_NAMERD_PROXY_PORT)
         ||  sscanf(port_str, "%hu", &net_info->http_proxy_port) != 1) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't get default http proxy port.");
        return 0/*failure*/;
    }

    return 1/*success*/;
}


static void s_RemoveCand(struct SNAMERD_Data* data, size_t n, int free_info)
{
    CORE_TRACEF(("s_RemoveCand() Removing info " FMT_SIZE_T ": %p",
                 n, data->cand[n].info));

    if (n >= data->n_cand) {
        CORE_LOGF_X(eNSub_Logic, eLOG_Critical,
                   ("Unexpected: n(" FMT_SIZE_T ") >= data->n_cand("
                    FMT_SIZE_T ")", n, data->n_cand));
        return;
    }
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
    int     all_standby = 1;
    size_t  i;

    /*  basic logic:
        if any endpoints have rate >= LBSM_STANDBY_THRESHOLD
            discard all endpoints with rate < LBSM_STANDBY_THRESHOLD
        else
            discard all endpoints with rate < highest rate //FIXME: WHY???
    */

    for (i = 0;  i < data->n_cand;  ++i) {

        if (max_rate < data->cand[i].info->rate)
            max_rate = data->cand[i].info->rate;

        if (data->cand[i].info->rate >= LBSM_STANDBY_THRESHOLD)
            all_standby = 0;
    }

    /* Loop from highest index to lowest to ensure well-defined behavior when
       candidates are removed and to avoid memmove in s_RemoveCand(). */
    for (i = data->n_cand;  i > 0;  ) {
        if (data->cand[--i].info->rate
            < (all_standby ? max_rate : LBSM_STANDBY_THRESHOLD)) {
            s_RemoveCand(data, i, 1);
        }
    }
}


static int/*bool*/ s_AddServerInfo(struct SNAMERD_Data* data, SSERV_Info* info)
{
    const char* name = SERV_NameOfInfo(info);
    size_t i;

    /* First check if the new server info updates an existing one */
    for (i = 0;  i < data->n_cand;  ++i) {
        if (strcasecmp(name, SERV_NameOfInfo(data->cand[i].info)) == 0
            &&  SERV_EqualInfo(info, data->cand[i].info))
        {
            /* Replace older version */
            CORE_TRACE("Replaced older candidate version.");
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
            (SLB_Candidate*) realloc(data->cand, n * sizeof(*temp));
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
        CORE_TRACEF(("Added candidate " FMT_SIZE_T
            ":   %s:%hu with server type '%s'.",
            data->n_cand, hostbuf, info->port, SERV_TypeStr(info->type)));
    }}
#endif
    data->cand[data->n_cand].info   = info;
    data->cand[data->n_cand].status = info->rate;
    data->n_cand++;
    data->done = 0;

    return 1;
}


/* Parse the "addrs[i].meta.expires" JSON from the namerd API, and adjust
   it according to the local timezone/DST to get the UTC epoch time.
   This function is not meant to be a generic ISO-8601 parser.  It expects
   the namerd API to return times in the format: "2017-03-29T23:02:55Z"
   Unfortunately, strptime is not supported at all on Windows, and doesn't
   support the "%z" format on Unix, so some custom parsing is required.
*/
static TNCBI_Time s_ParseExpires(time_t tt_now, const char* expires)
{
    int     exp_year, exp_mon, exp_mday, exp_hour, exp_min, exp_sec;
    char    exp_zulu;

    if ( ! expires) {
        CORE_LOG_X(eNSub_Logic, eLOG_Critical,
                   "Unexpected NULL 'expires' pointer.");
        return 0;
    }

    if (sscanf(expires, "%d-%d-%dT%d:%d:%d%c", &exp_year, &exp_mon, &exp_mday,
               &exp_hour, &exp_min, &exp_sec, &exp_zulu) != 7  ||
        (exp_year < 2017  ||  exp_year > 9999)  ||
        (exp_mon  < 1     ||  exp_mon  > 12)    ||
        (exp_mday < 1     ||  exp_mday > 31)    ||
        (exp_hour < 0     ||  exp_hour > 23)    ||
        (exp_min  < 0     ||  exp_min  > 59)    ||
        (exp_sec  < 0     ||  exp_sec  > 60)    ||  /* 60 for leap second */
        exp_zulu != 'Z')
    {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Unexpected JSON {\"addrs[i].meta.expires\"} "
                     "value '%s'.", expires));
        return 0;
    }

    /* Get the UTC epoch time for the expires value. */
    struct tm   tm_expires;
    tm_expires.tm_year  = exp_year - 1900;    /* years since 1900 */
    tm_expires.tm_mon   = exp_mon - 1;        /* months since January: 0-11 */
    tm_expires.tm_mday  = exp_mday;
    tm_expires.tm_hour  = exp_hour;
    tm_expires.tm_min   = exp_min;
    tm_expires.tm_sec   = exp_sec;
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
        int     now_year = tm_now.tm_year + 1900;
        int     now_mon  = tm_now.tm_mon + 1;
        int     now_mday = tm_now.tm_mday;
        int     now_hour = tm_now.tm_hour;
        int     now_min  = tm_now.tm_min;
        int     now_sec  = tm_now.tm_sec;
        char    now_str[40]; /* need 21 for 'yyyy-mm-ddThh:mm:ssZ' */
        double  td_diff, td_hour, td_min, td_sec;
        const char* td_sign;

        if ((now_year < 2017  ||  now_year > 9999)  ||
            (now_mon  < 1     ||  now_mon  > 12)    ||
            (now_mday < 1     ||  now_mday > 31)    ||
            (now_hour < 0     ||  now_hour > 23)    ||
            (now_min  < 0     ||  now_min  > 59)    ||
            (now_sec  < 0     ||  now_sec  > 60)) { /* 60 for leap second */
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Invalid 'struct tm' for current time"
                         " (adjusted to UTC): { "
                         "tm_year = %d, tm_mon = %d, tm_mday = %d, "
                         "tm_hour = %d, tm_min = %d, tm_sec = %d }.",
                         tm_now.tm_year + 1900, tm_now.tm_mon + 1,
                         tm_now.tm_mday, tm_now.tm_hour,
                         tm_now.tm_min, tm_now.tm_sec));
            return 0;
        }
        if (tdiff < 0.0) {
            td_sign = "-";
            td_diff = -tdiff;
        } else {
            td_sign = "";
            td_diff = tdiff;
        }
        td_hour  = td_diff / 3600.0;
        td_diff -= td_hour * 3600.0;
        td_min   = td_diff / 60.0;
        td_sec   = td_diff - td_min * 60.0;
        sprintf(now_str, "%d-%02d-%02dT%02d:%02d:%02dZ",
                now_year, now_mon, now_mday, now_hour, now_min, now_sec);
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Unexpected JSON {\"addrs[i].meta.expires\"}"
                     " value '%s' - excessive difference"
                     " (%.0lfs = %s%.0lf:%.0lf:%.0lf) "
                     "from current time '%s'", expires, tdiff,
                     td_sign, td_hour, td_min, td_sec, now_str));
        return 0;
    }
    TNCBI_Time  timeval = (TNCBI_Time)((double) tt_now + tdiff);
    /*CORE_TRACEF(("expires=%s  tdiff=%lf  now=" FMT_TIME_T "  info->time=%u",
      expires, tdiff, tt_now, timeval));*/

    return timeval;
}


/* Parsing the response requires having the entire response in one buffer.
*/
static const char* s_ReadFullResponse(CONN conn)
{
    char*      response = 0;
    BUF        buf = 0;
    EIO_Status status;
    size_t     len;

    CORE_TRACE("Entering s_ReadFullResponse()");

    do {
        char block[5000];
        status = CONN_Read(conn, block, sizeof(block), &len, eIO_ReadPlain);
        if (!len)
            assert(status != eIO_Success);
        else if (!BUF_Write(&buf, block, len)) {
            CORE_TRACE("Leaving s_ReadFullResponse() -- bad alloc");
            status = eIO_Unknown;
            break;
        }
    } while (status == eIO_Success);
    if ((len = BUF_Size(buf)) > 0  &&  (response = (char*) malloc(len + 1))) {
        /* read all in */
        verify(BUF_Read(buf, response, len) == len);
        response[len] = '\0';
        CORE_TRACEF(("Got response: %s", response));
    }
    BUF_Destroy(buf);

    CORE_TRACEF(("Leaving s_ReadFullResponse(): " FMT_SIZE_T ", %s",
                 len, IO_StatusStr(status)));
    return response;
}


static int/*bool*/ s_ParseResponse(SERV_ITER iter, CONN conn)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;
    x_JSON_Value*        root_value = 0;
    const char*          success_type;
    int/*bool*/          retval = 0;
    x_JSON_Object*       root_obj;
    x_JSON_Array*        address_arr;
    const char*          response;
    size_t               it;

    CORE_TRACE("Entering s_ParseResponse()");

    if (!(response = s_ReadFullResponse(conn)))
        goto out;

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
    if (x_json_serialize_to_buffer_pretty(root_value, json, sizeof(json)-1)
        == JSONSuccess) {
        CORE_TRACEF(("Got JSON:\n%s", json));
    } else
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
        const char      *host, *extra, *srv_type, *mode;
        const char      *cpfx, *ctype;
        const char      *local;
        const char      *priv;
        const char      *stateful;
        char            *server_descriptor;
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
            meta.serviceType        |  |  |  |      | |    | |    |
               type|srv_type ----+  |  |  |  |      | |    | |    |
                                 [] [] [] [] [__]   [][]   [][]   [] */
        const char* descr_fmt = "%s %s:%u %s %s%s L=%s%s R=%s%s T=%u";
        /*  NOTE: Some fields must not be included in certain situations
            because SERV_ReadInfoEx() does not expect them in those
            situations, and if they are present then SERV_ReadInfoEx()
            will prematurely terminate processing the descriptor.
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
        if ( ! x_json_object_has_value_of_type
             ((const x_JSON_Object*) address,
              (const char*) "port", JSONNumber)) {
            CORE_LOG_X(eNSub_Json, eLOG_Error,
                       "Couldn't get JSON {\"addrs.port\"} name.");
            goto out;
        }
        port = (int) x_json_object_get_number(address, "port");
        if (port <= 0  ||  port > 65535) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Invalid JSON {\"addrs[i].port\"} value (%d).",
                         port));
            goto out;
        }

        ESERV_Type  type;
        /* SSERV_Info.type <=== addrs[i].meta.serviceType */
        srv_type = x_json_object_dotget_string(address, "meta.serviceType");
        if ( ! srv_type  ||   ! *srv_type) {
            type = SERV_GetImplicitServerType(iter->name);
            srv_type = SERV_TypeStr(type);
        } else if ( ! SERV_ReadType(srv_type, &type)) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Unrecognized server type '%s'.", srv_type));
            goto out;
        }
        /* Make sure the server type matches an allowed type. */
        if (iter->types != fSERV_Any  &&  !(iter->types & type)) {
            CORE_TRACEF(("Ignoring endpoint %s:%d with unallowed server"
                         " type '%s' - allowed types = 0x%lx.",
                         host, port, srv_type, (unsigned long) iter->types));
            continue;
        }
        CORE_TRACEF(("Parsed endpoint %s:%d with allowed server type '%s'.",
                     host, port, srv_type));

        /* SSERV_Info.mode <=== addrs[i].meta.stateful */
        if (x_json_object_dothas_value_of_type(address, "meta.stateful",
                                               JSONBoolean)) {
            int st = x_json_object_dotget_boolean(address, "meta.stateful");
            if (st == 0) {
                stateful = "";
            } else if (st == 1) {
                stateful = " S=YES";
            } else {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("Invalid JSON {\"addrs[i].meta.stateful\"}"
                             " value (%d).", st));
                goto out;
            }
        } else {
            stateful = "";
        }

        /* Make sure the JSON is stateless for a stateless SERV_ITER. */
        if ((iter->types & fSERV_Stateless)  &&  stateful[0] != '\0') {
            CORE_TRACEF(("Ignoring stateful endpoint %s:%d in stateless"
                         " search.", host, port));
            continue;
        }

        /* SSERV_Info.site <=== addrs[i].meta.mode */
        local = "NO";
        priv = "";
        if (x_json_object_dothas_value_of_type(address, "meta.mode",
                                               JSONString)) {
            mode = x_json_object_dotget_string(address, "meta.mode");
            if ( ! mode  ||  ! *mode) {
                CORE_LOG_X(eNSub_Json, eLOG_Error,
                           "Couldn't get JSON {\"addrs[i].meta.mode\"}"
                           " value.");
                goto out;
            }
            if (strcmp(mode, "L") == 0) {
                local = "YES";
            } else if (strcmp(mode, "H") == 0) {
                local = "YES";
                priv = " P=YES";
            } else {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("Unexpected JSON {\"addrs[i].meta.mode\"}"
                             " value '%s'.", mode));
                goto out;
            }
        }

        /* SSERV_Info.rate <=== addrs[i].meta.rate */
        if (x_json_object_dothas_value_of_type(address, "meta.rate",
                                               JSONNumber)) {
            rate = x_json_object_dotget_number(address, "meta.rate");
        } else {
            rate = LBSM_DEFAULT_RATE;
        }
        /* verify magnitude */
        if (rate < SERV_MINIMAL_RATE  ||  rate > SERV_MAXIMAL_RATE) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Unexpected JSON {\"addrs[i].meta.rate\"}"
                         " value '%lf'.", rate));
            goto out;
        }
        /* format with 3-digit precsion for standby; else 2-digits */
        char rate_str[12];
        sprintf(rate_str, "%.*lf",
                (rate < LBSM_STANDBY_THRESHOLD ? 3 : 2), rate);

        /* SSERV_Info.time <=== addrs[i].meta.expires */
        TNCBI_Time  timeval = iter->time + LBSM_DEFAULT_TIME;
        if (x_json_object_dothas_value_of_type(address, "meta.expires",
                                               JSONString)) {
            timeval = s_ParseExpires(iter->time,
                                     x_json_object_dotget_string
                                     (address, "meta.expires"));
            if ( ! timeval)
                goto out;
        } else {
            static void* s_Once = 0;
            if (CORE_Once(&s_Once)) {
                CORE_LOGF_X(eNSub_Json, eLOG_Trace,
                            ("Missing JSON {\"addrs[i].meta.expires\"} - "
                             "using current time (%u) + default expiration"
                             " (%u) = %u",
                             iter->time, LBSM_DEFAULT_TIME, timeval));
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
            if (type & fSERV_Http) {
                CORE_LOG_X(eNSub_Json, eLOG_Trace,
                           "Namerd API did not return a path in meta.extra "
                           "JSON for an HTTP type server");
                extra = "/";
            } else if (type == fSERV_Ncbid) {
                CORE_LOG_X(eNSub_Json, eLOG_Trace,
                           "Namerd API did not return args in meta.extra "
                           "JSON for an NCBID type server");
                extra = "''";
            } else {
                extra = "";
            }
        }

        /* Prepare descriptor */
        size_t length;
        length = strlen(descr_fmt) + strlen(srv_type) + strlen(host) +
            5 /*length of port*/ + strlen(extra) + strlen(cpfx) +
            strlen(ctype) + strlen(local) + strlen(priv) +
            strlen(rate_str) + strlen(stateful) +
            20/*length of timeval*/;
        server_descriptor = (char*)malloc(sizeof(char) * length);
        if ( ! server_descriptor) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't alloc for server descriptor.");
            goto out;
        }
        sprintf(server_descriptor, descr_fmt, srv_type, host,
                port, extra, cpfx, ctype, local, priv, rate_str, stateful,
                timeval);

        /* Parse descriptor into SSERV_Info */
        CORE_TRACEF(("Parsing server descriptor: '%s'", server_descriptor));
        SSERV_Info* info = SERV_ReadInfoEx(server_descriptor,
                                           iter->reverse_dns
                                           ? iter->name : "", 0/*false*/);

        if ( ! info) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                        ("Unable to add server info with descriptor '%s'.",
                         server_descriptor));
            free(server_descriptor);
            continue;
        }
        free(server_descriptor);
        CORE_TRACEF(("Created info: %p", info));

        /* Add new info to collection */
        if (s_AddServerInfo(data, info)) {
            retval = 1; /* at least one info added */
        } else {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Unable to add server info.");
            CORE_TRACEF(("Freeing info: %p", info));
            free(info); /* not freed by failed s_AddServerInfo() */
            goto out;
        }
    }

    /* Filter out standby endpoints */
    s_RemoveStandby(data);

out:
    if (response)
        free((void*) response);
    if (root_value)
        x_json_value_free(root_value);
    CORE_TRACEF(("Leaving s_ParseResponse(): %d", retval));
    return retval;
}


/* Parse buffer and extract DTab-Local header, if present. */
static char* s_GetDtabHeaderFromBuf(const char* buf)
{
    char* start = (char*)buf;
    char* end;
    char* dup_hdr;

    CORE_TRACEF(("Entering s_GetDtabHeaderFromBuf(\"%s\")", buf ? buf : ""));

    if (start  &&  strncasecmp(start, DTAB_HTTP_HEADER_TAG,
                       sizeof(DTAB_HTTP_HEADER_TAG) - 1) == 0)
    {
        /* advance start past name, colon, and any whitespace */
        start += sizeof(DTAB_HTTP_HEADER_TAG) - 1;
        while (*start == ' '  ||  *start == '\t') ++start;

        /* advance end to \0 or eol (excluding eol) */
        end = start;
        while (*end  &&  *end != '\r'  &&  *end != '\n') ++end;

        /* clone the header value */
        dup_hdr = strndup(start, (size_t)(end - start));
        if ( ! dup_hdr) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't alloc for dtab header value.");
            CORE_TRACE("Leaving s_GetDtabHeaderFromBuf() -- bad alloc");
            return NULL;
        }
        CORE_TRACEF((
            "Leaving s_GetDtabHeaderFromBuf() -- got dtab header \"%s\"",
            dup_hdr));
        return dup_hdr;
    }

    CORE_TRACE("Leaving s_GetDtabHeaderFromBuf()");
    return NULL;
}


/* If there's a dtab in the user header, add it to the caller's dtab. */
/* FIXME: User header can be a collection of lines, separated by "[\r]\n",
 * not just a single tag... */
static void s_UpdateDtabFromUserHeader(char** dtab_p, int* success_p,
                                       SConnNetInfo* net_info)
{
    char* dtab = NULL;

    CORE_TRACEF(("Entering s_UpdateDtabFromUserHeader(\"%s\") -- success=%d",
        net_info->http_user_header ? net_info->http_user_header : "",
        *success_p));

    if ( ! *success_p) {
        CORE_TRACE("Leaving s_UpdateDtabFromUserHeader() -- prior no success");
        return;
    }

    dtab = s_GetDtabHeaderFromBuf(net_info->http_user_header);
    if (dtab) {
        /*ConnNetInfo_DeleteUserHeader(net_info, DTAB_HTTP_HEADER_TAG);*/
        s_UpdateDtab(dtab_p, dtab, success_p);
        free(dtab);
    }

    CORE_TRACE("Leaving s_UpdateDtabFromUserHeader()");
}


/* If there's a dtab in the registry, add it to the caller's dtab. */
static void s_UpdateDtabFromRegistry(char** dtab_p, int* success_p,
                                     const char* service)
{
    char val[MAX_QUERY_STRING_LEN + 1];

    CORE_TRACEF(("Entering s_UpdateDtabFromRegistry(\"%s\") -- success=%d",
                 service ? service : "", *success_p));

    if ( ! *success_p) {
        CORE_TRACE("Leaving s_UpdateDtabFromRegistry() -- prior no success");
        return;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_DTAB,
                                       val, sizeof(val),
                                       DEF_NAMERD_DTAB)) {
        *success_p = 0;
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for dtab from registry.");
        CORE_TRACE("Leaving s_UpdateDtabFromRegistry() -- bad alloc");
        return;
    }

    s_UpdateDtab(dtab_p, val, success_p);

    CORE_TRACE("Leaving s_UpdateDtabFromRegistry()");
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
static int/*bool*/ s_ProcessDtab(SConnNetInfo* net_info, SERV_ITER iter)
{
#define  DTAB_ARGS_SEP  "dtab"
    int/*bool*/ success = 1;
    char* dtab = NULL;

    CORE_TRACE("Entering s_ProcessDtab()");

    /* Dtab precedence (highest first): registry > user_header */
    s_UpdateDtabFromRegistry(&dtab, &success, iter->name);
    s_UpdateDtabFromUserHeader(&dtab, &success, net_info);

    if (success  &&  dtab
        &&  !ConnNetInfo_AppendArg(net_info, DTAB_ARGS_SEP, dtab)) {
        CORE_LOG_X(eNSub_TooLong, eLOG_Error, "Dtab too long.");
        success = 0;
    }

    if (dtab)
        free(dtab);

    CORE_TRACE("Leaving s_ProcessDtab()");
    return success;
#undef  DTAB_ARGS_SEP
}


static EHTTP_HeaderParse s_ParseHeader(const char* header,
                                       void*       iter,
                                       int         server_error)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*)((SERV_ITER) iter)->data;
    int code = 0/*success code if any*/;

    CORE_TRACEF(("Entering s_ParseHeader(\"%s\")", header));

    if (server_error == 400  ||  server_error == 403  ||  server_error == 404) {
        data->fail = 1/*true*/;
    } else if (sscanf(header, "%*s %d", &code) < 1) {
        data->eof = 1/*true*/;
        CORE_TRACE("Leaving s_ParseHeader() -- eof=true");
        return eHTTP_HeaderError;
    }

    /* check for empty document */
    if (code == 204)
        data->eof = 1/*true*/;

    CORE_TRACE("Leaving s_ParseHeader()");
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
    struct SNAMERD_Data*  data = (struct SNAMERD_Data*) iter->data;
    int/*bool*/           retval;
    EIO_Status            status;
    CONN                  conn;
    CONNECTOR             c;

    CORE_TRACE("Entering s_Resolve()");

    if (data->eof | data->fail) {
        CORE_LOG_X(eNSub_Logic, eLOG_Critical,
                   "Unexpected non-zero 'data->eof | data->fail'.");
        return 0;
    }

    /* Handle DTAB, if present. */
    s_ProcessDtab(data->net_info, iter);

    /* Create connector and connection, and fetch and parse the response. */
    if (!(c = s_CreateConnector(iter))
        ||  (status = CONN_Create(c, &conn)) != eIO_Success) {
        char what[80];
        if (c)
            sprintf(what, "connection: %s", IO_StatusStr(status));
        else
            strcpy (what, "connector");
        CORE_LOGF_X(eNSub_Connect, eLOG_Error,
                    ("Unable to create %s", what));
        if (c)
            c->destroy(c);
        return 0;
    }

    retval = s_ParseResponse(iter, conn);

    CONN_Close(conn);

    CORE_TRACE("Leaving s_Resolve()");
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
    int     any_expired = 0;
    size_t  i;

    /* Loop from highest index to lowest to ensure well-defined behavior when
        candidates are removed and to avoid memmove in s_RemoveCand(). */
    for (i = data->n_cand;  i > 0;  ) {
        --i; /* decrement here to avoid unsigned underflow */
        const SSERV_Info* info = data->cand[i].info;
        if (info->time < now) {
#if defined(_DEBUG)  &&  ! defined(NDEBUG)
            TNCBI_Time  tnow = (TNCBI_Time) time(0);
            CORE_TRACE("Endpoint expired:");
            CORE_TRACEF((
                "    info->time (%u) < iter->time (%u)   now (%u)",
                info->time, now, tnow));
            CORE_TRACEF((
                "    iter->time - info->time (%d)     now - iter->time (%d)",
                now - info->time, tnow - now));
#endif
            any_expired = 1;
            s_RemoveCand(data, i, 1);
        }
    }

    return any_expired  ||  data->n_cand == 0;
}


static SLB_Candidate* s_GetCandidate(void* user_data, size_t n)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) user_data;
    return n < data->n_cand ? &data->cand[n] : 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;
    SSERV_Info* info;
    size_t n;

    CORE_TRACE("Entering s_GetNextInfo()");

    assert(data);

    if (data->n_cand < 1  &&  data->done) {
        data->done = 0;
        CORE_TRACE("Leaving s_GetNextInfo() -- end of candidates");
        return 0;
    }

    if (s_IsUpdateNeeded(iter->time, data)) {
        if ( ! (data->eof | data->fail)) {
            s_Resolve(iter);
            if (data->n_cand < 1) {
                data->done = 1;
                CORE_TRACE("Leaving s_GetNextInfo() -- resolved no candidates");
                return 0;
            }
        }
    }

    /* Pick a randomized candidate. */
    n = LB_Select(iter, data, s_GetCandidate, NAMERD_LOCAL_BONUS);
    info       = (SSERV_Info*) data->cand[n].info;
    info->rate =               data->cand[n].status;

    /* Remove returned info */
    s_RemoveCand(data, n, 0);

    if (host_info)
        *host_info = NULL;

    CORE_TRACE("Leaving s_GetNextInfo()");
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACE("Entering s_Reset()");

    if (data->cand) {
        size_t i;
        assert(data->a_cand  &&  data->n_cand <= data->a_cand);
        for (i = 0;  i < data->n_cand;  ++i) {
            CORE_TRACEF(("Freeing available info " FMT_SIZE_T ": %p",
                         i, data->cand[i].info));
            free((void*) data->cand[i].info);
        }
        data->n_cand = 0;
    }

    CORE_TRACE("Leaving s_Reset()");
}


static void s_Close(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACE("Entering s_Close()");
    iter->data = 0;

    assert(data  &&  !data->n_cand); /*s_Reset() had to be called before*/

    if (data->cand)
        free(data->cand);
    ConnNetInfo_Destroy(data->net_info);
    free(data);

    CORE_TRACE("Leaving s_Close()");
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern const SSERV_VTable* SERV_NAMERD_Open(SERV_ITER           iter,
                                            const SConnNetInfo* x_net_info,
                                            SSERV_Info**        info)
{
    struct SNAMERD_Data* data;
    SConnNetInfo*        net_info;
    char                 buf[(CONN_PATH_LEN+1)/2];

    assert(iter  &&  x_net_info  &&  !iter->data  &&  !iter->op);
    if (iter->ismask)
        return 0/*LINKERD doesn't support masks*/;
    assert(iter->name  &&  *iter->name);

    CORE_TRACEF(("Entering SERV_NAMERD_Open(\"%s\")", iter->name));

    /* Prohibit catalog-prefixed services, e.g. "/lbsm/<svc>" */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
            ("Invalid service name \"%s\" - must not begin with '/'.",
             iter->name));
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, catalog prefix");
        return 0;
    }

    if ( ! (data = (struct SNAMERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
            "Could not allocate for SNAMERD_Data.");
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, bad alloc");
        return 0;
    }
    iter->data = data;

    if ( ! (net_info = ConnNetInfo_Clone(x_net_info))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical, "Couldn't clone net_info.");
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, no net_info clone");
        goto out;
    }
    data->net_info = net_info;

    if (iter->types & fSERV_Stateless)
        net_info->stateless = 1/*true*/;
    if ((iter->types & fSERV_Firewall)  &&  !net_info->firewall)
        net_info->firewall = eFWMode_Adaptive;

    if ( ! ConnNetInfo_SetupStandardArgs(net_info, iter->name)) {
        CORE_LOG_X(eNSub_BadData, eLOG_Critical,
            "Couldn't set up standard args.");
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, standard args");
        goto out;
    }

    net_info->scheme      = s_GetScheme();
    net_info->req_method  = s_GetReqMethod();

    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_HOST,
                                       net_info->host, sizeof(net_info->host),
                                       DEF_NAMERD_API_HOST)) {
        net_info->host[0] = '\0';
    }
    net_info->port        = 0; /* namerd doesn't support a port */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_PATH,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_PATH)) {
        ConnNetInfo_SetPath(net_info, "");
    } else
        ConnNetInfo_SetPath(net_info, buf);
    if (ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                    REG_NAMERD_API_ENV,
                                    buf + 1, sizeof(buf) - 1,
                                    DEF_NAMERD_API_ENV)  &&  buf[1]) {
        *buf = '/';
        ConnNetInfo_AddPath(net_info, buf);
    }

    if (ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                    REG_NAMERD_API_ARGS,
                                    buf, sizeof(buf),
                                    DEF_NAMERD_API_ARGS)) {
        size_t argslen = strlen(buf);
        size_t namelen = strlen(iter->name);
        if (argslen + namelen < sizeof(buf)) {
            memcpy(buf + argslen, iter->name, namelen + 1);
            ConnNetInfo_PreOverrideArg(net_info, buf, 0);
        }
    }

    if ( ! s_GetHttpProxy(net_info)) {
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, HTTP proxy");
        goto out;
    }
    ConnNetInfo_ExtendUserHeader(net_info,
                                 "User-Agent: NCBINamerdMapper"
#ifdef NCBI_CXX_TOOLKIT
                                 " (CXX Toolkit)"
#else
                                 " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
                                 "\r\n");

    if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed  = iter->time ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
    }

    s_Resolve(iter);

    if (!data->n_cand) {
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- service not found");
        goto out;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    CORE_TRACE("Leaving SERV_NAMERD_Open()");
    return &s_op;

 out:
    s_Reset(iter);
    s_Close(iter);
    return 0;
}


extern int SERV_NAMERD_SetConnectorSource(const char* mock_body)
{
    if ( ! mock_body  ||  ! *mock_body) {
        s_CreateConnector = s_CreateConnectorHttp;
        s_mock_body = 0;
    } else {
        s_mock_body = mock_body;
        s_CreateConnector = s_CreateConnectorMemory;
    }
    return 1;
}
