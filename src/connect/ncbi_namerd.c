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
    We just override the given fields (which were collected for the service in
    question), so there are some standard keys plus some additional ones, which
    are purely for NAMERD.  Note that the namerd API doesn't support a port.
*/
#define DEF_NAMERD_REG_SECTION     "_NAMERD"

#define REG_NAMERD_API_SCHEME      "SCHEME"
#define DEF_NAMERD_API_SCHEME      "http"

#define REG_NAMERD_API_REQ_METHOD  REG_CONN_REQ_METHOD
#define DEF_NAMERD_API_REQ_METHOD  "GET"

#define REG_NAMERD_API_HOST        REG_CONN_HOST
#define DEF_NAMERD_API_HOST        "namerd-api.linkerd.ncbi.nlm.nih.gov"

#define REG_NAMERD_API_PORT        REG_CONN_PORT
#define DEF_NAMERD_API_PORT        0

#define REG_NAMERD_API_PATH        REG_CONN_PATH
#define DEF_NAMERD_API_PATH        "/api/1/resolve"

#define REG_NAMERD_API_ENV         "ENVIRONMENT"
#define DEF_NAMERD_API_ENV         "default"

#define REG_NAMERD_API_ARGS        REG_CONN_ARGS
#define DEF_NAMERD_API_ARGS        "path=/service/"

#define REG_NAMERD_PROXY_HOST      REG_CONN_HTTP_PROXY_HOST
/*  NAMERD_TODO - "temporarily" support plain "linkerd" on Unix only */
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_CYGWIN)
#define DEF_NAMERD_PROXY_HOST      "linkerd"
#else
#define DEF_NAMERD_PROXY_HOST      \
    "pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#endif

#define REG_NAMERD_PROXY_PORT      REG_CONN_HTTP_PROXY_PORT
#define DEF_NAMERD_PROXY_PORT      "4140"

#define REG_NAMERD_DTAB            "DTAB"
#define DEF_NAMERD_DTAB            ""

#define NAMERD_DTAB_ARG            "dtab"


/* Default rate increase 20% if svc runs locally */
#define NAMERD_LOCAL_BONUS         1.2

/* Misc. */
#define DTAB_HTTP_HEADER_TAG       "DTab-Local:"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, NULL/*Feedback*/, NULL/*s_Update*/, s_Reset, s_Close, "NAMERD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SNAMERD_Data {
    SConnNetInfo*  net_info;
    int/*bool*/    done;
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

    return HTTP_CreateConnector(data->net_info, 0/*user-headers*/, 0/*flags*/);
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
            &&  SERV_EqualInfo(info, data->cand[i].info)) {
            /* Replace older version */
            CORE_TRACEF(("Replaced candidate " FMT_SIZE_T ": %p", i, info));
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
    CORE_TRACEF(("Added candidate " FMT_SIZE_T ": %p", data->n_cand, info));
    data->cand[data->n_cand].info   = info;
    data->cand[data->n_cand].status = info->rate;
    data->n_cand++;

    return 1;
}


/* Parse the "addrs[i].meta.expires" JSON from the namerd API, and adjust
   it according to the local timezone/DST to get the UTC epoch time.
   This function is not meant to be a generic ISO-8601 parser.  It expects
   the namerd API to return times in the format: "2017-03-29T23:02:55Z"
   Unfortunately, strptime is not supported at all on Windows, and doesn't
   support the "%z" format on Unix, so some custom parsing is required.
*/
static TNCBI_Time x_ParseExpires(const char* expires, time_t utc, size_t i)
{
    struct tm tm_exp, tm_now;
    time_t exp, now;
    char   exp_zulu;
    double tzdiff;
    int    n;

    if ( ! expires  ||  ! *expires) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Couldn't get JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value.", i));
        return 0;
    }

    memset(&tm_exp, 0, sizeof(tm_exp));
    if (sscanf(expires, "%d-%d-%dT%d:%d:%d%c%n",
               &tm_exp.tm_year, &tm_exp.tm_mon, &tm_exp.tm_mday,
               &tm_exp.tm_hour, &tm_exp.tm_min, &tm_exp.tm_sec,
               &exp_zulu, &n) < 7  ||  expires[n]
        ||  tm_exp.tm_year < 2017  ||  tm_exp.tm_year > 9999
        ||  tm_exp.tm_mon  < 1     ||  tm_exp.tm_mon  > 12
        ||  tm_exp.tm_mday < 1     ||  tm_exp.tm_mday > 31
        ||  tm_exp.tm_hour < 0     ||  tm_exp.tm_hour > 23
        ||  tm_exp.tm_min  < 0     ||  tm_exp.tm_min  > 59
        ||  tm_exp.tm_sec  < 0     ||  tm_exp.tm_sec  > 60/* 60 for leap sec */
        ||  exp_zulu != 'Z'
        /* Get the UTC epoch time for the expires value. */
        ||  (tm_exp.tm_year -= 1900,  /* years since 1900 */
             tm_exp.tm_mon--,         /* months since January: 0-11 */
             (exp = mktime(&tm_exp)) == (time_t)(-1))) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Unexpected JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value '%s'.", i, expires));
        return 0;
    }

    CORE_LOCK_WRITE;
    tm_now = *gmtime(&utc);
    CORE_UNLOCK;
    verify((now = mktime(&tm_now)) != (time_t)(-1));

    /* Adjust for time diff between local and UTC, which should
       correspond to 3600 x (number of time zones from UTC),
       i.e. diff between current TZ (UTC-12 to UTC+14) and UTC. */
    tzdiff = difftime(utc, now);
    assert(-12.0 * 3600.0 <= tzdiff  &&  tzdiff <= 14.0 * 3600.0);
    exp += tzdiff;
    if (exp < utc) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("Unexpected JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value expired: "
                     FMT_TIME_T " vs. " FMT_TIME_T " now",
                     i, exp, utc));
        return 0;
    }
    return (TNCBI_Time) exp;
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
        char block[2000];
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
        verify(BUF_Read(buf, response, len) == len  &&  !BUF_Size(buf));
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
    int/*bool*/          retval = 0;
    x_JSON_Value*        root = 0;
    const char*          response;
    x_JSON_Array*        addrs;
    const char*          type;
    size_t               i, n;
    x_JSON_Object*       top;

    CORE_TRACE("Entering s_ParseResponse()");

    if (!(response = s_ReadFullResponse(conn)))
        goto out;

    /* root object */
    root = x_json_parse_string(response);
    if ( ! root) {
        CORE_LOG_X(eNSub_Json, eLOG_Error,
                   "Response couldn't be parsed as JSON.");
        goto out;
    }
    if (x_json_value_get_type(root) != JSONObject) {
        CORE_LOG_X(eNSub_Json, eLOG_Error,
                   "Response is not a JSON object.");
        goto out;
    }
    top = x_json_value_get_object(root);
    if ( ! top) {
        CORE_LOG_X(eNSub_Json, eLOG_Error,
                   "Couldn't get JSON root object.");
        goto out;
    }

#if defined(_DEBUG)  &&  ! defined(NDEBUG)
    {{
        char json[9999];
        if (x_json_serialize_to_buffer_pretty(root, json, sizeof(json)-1)
            == JSONSuccess) {
            json[sizeof(json)-1] = '\0';
            CORE_TRACEF(("Got JSON:\n%s", json));
        } else
            CORE_LOG_X(eNSub_Json, eLOG_Error, "Couldn't serialize JSON");
    }}
#endif

    /* top-level {"type" : "bound"} expected for successful lookup */
    type = x_json_object_get_string(top, "type");
    if ( ! type) {
        CORE_LOG_X(eNSub_Json, eLOG_Error,
                   "Couldn't get JSON {\"type\"} value.");
        goto out;
    }
    if (strcmp(type, "bound") != 0) {
        CORE_TRACEF(("Service \"%s\" appears to be unknown: %s.",
                     iter->name, type));
        goto out;
    }

    /* top-level {"addrs" : []} contains endpoint data */
    addrs = x_json_object_get_array(top, "addrs");
    if ( ! addrs) {
        CORE_LOG_X(eNSub_Json, eLOG_Error,
                   "Couldn't get JSON {\"addrs\"} array.");
        goto out;
    }

    /* Note: top-level {"meta" : {}} not currently used */

    /* Iterate through addresses, adding to "candidates". */
    n = x_json_array_get_count(addrs);
    for (i = 0;  i < n;  ++i) {
        const char*    host, *extra, *mime, *mode, *local, *private, *stateful;
        char*          server_descr;
        x_JSON_Object* address;
        ESERV_Type     atype;
        double         rate;
        int            port;
        TNCBI_Time     time;
        size_t         size;

        /*  JSON|SSERV_Info|variable mapping to format string:
            meta.expires|time|time ---------------------------------+
            meta.stateful|mode|stateful -----------------------+    |
            meta.rate|rate|rate --------------------------+    |    |
            meta.mode|site|private ------------------+    |    |    |
            meta.mode|site|local ------------------+ |    |    |    |
            meta.contentType|mime_*|mime ---+      | |    |    |    |
            meta.extra|extra|extra -----+   |      | |    |    |    |
            port|port|port -----------+ |   |      | |    |    |    |
            ip|host|host ----------+  | |   |      | |    |    |    |
            meta.serviceType       |  | |   |      | |    |    |    |
                  |type|type ---+  |  | |   |      | |    |    |    |  */
        static const         /* [] [] [][__][__]   [][]   [___][]   [] */
            char kDescrFmt[] = "%s %s:%u%s%s%s%s L=%s%s R=%.*lf%s T=%u";
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
        address = x_json_array_get_object(addrs, i);
        if ( ! address) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Couldn't get JSON {\"addrs[" FMT_SIZE_T "]\"}"
                         " object.", i));
            goto out;
        }

        /* SSERV_Info.host <=== addrs[i].ip */
        host = x_json_object_get_string(address, "ip");
        if ( ! host  ||  ! *host) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Couldn't get JSON {\"addrs[" FMT_SIZE_T "].ip\"}"
                         " value.", i));
            goto out;
        }

        /* SSERV_Info.port <=== addrs[i].port */
        /* Unfortunately the x_json_object_get_number() function does
           not distinguish failure from a legitimate zero value :-/
           Therefore, first explicitly check for the value. */
        if ( ! x_json_object_has_value_of_type(address, "port", JSONNumber)) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Couldn't get JSON {\"addrs[" FMT_SIZE_T " ].port\"}"
                         " type", i));
            goto out;
        }
        port = (int) x_json_object_get_number(address, "port");
        if (port <= 0  ||  port > 65535) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Invalid JSON {\"addrs[" FMT_SIZE_T "].port\"}"
                         " value: %d.", i, port));
            goto out;
        }

        /* SSERV_Info.type <=== addrs[i].meta.serviceType */
        type = x_json_object_dotget_string(address, "meta.serviceType");
        if ( ! type  ||  ! *type) {
            atype = SERV_GetImplicitServerType(iter->name);
            type  = SERV_TypeStr(atype);
        } else if ( ! SERV_ReadType(type, &atype)) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("Unrecognized {\"addrs[" FMT_SIZE_T
                         "].meta.serviceType\"} value: '%s'.", i, type));
            goto out;
        }

        CORE_TRACEF(("Parsed endpoint %s:%d '%s'", host, port, type));

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
                            ("Invalid JSON {\"addrs[" FMT_SIZE_T
                             "].meta.stateful\"} value (%d).", i, st));
                goto out;
            }
        } else
            stateful = "";

        /* SSERV_Info.site <=== addrs[i].meta.mode */
        local   = "NO";
        private = "";
        if (x_json_object_dothas_value_of_type(address, "meta.mode",
                                               JSONString)) {
            mode = x_json_object_dotget_string(address, "meta.mode");
            if ( ! mode  ||  ! *mode) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("Couldn't get JSON {\"addrs[" FMT_SIZE_T
                             "].meta.mode\"} value.", i));
                goto out;
            }
            if (strcmp(mode, "L") == 0)
                local = "YES";
            else if (strcmp(mode, "P") == 0)
                private = " P=YES";
            else if (strcmp(mode, "H") == 0)
                local = "YES", private = " P=YES";
            else {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("Unrecognized JSON {\"addrs[" FMT_SIZE_T
                             "].meta.mode\"} value '%s'.", i, mode));
                goto out;
            }
        }

        /* SSERV_Info.rate <=== addrs[i].meta.rate */
        if (x_json_object_dothas_value_of_type(address, "meta.rate",
                                               JSONNumber)) {
            rate = x_json_object_dotget_number(address, "meta.rate");
            /* verify magnitude */
            if (rate < SERV_MINIMAL_RATE  ||  rate > SERV_MAXIMAL_RATE) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("Unexpected JSON {\"addrs[" FMT_SIZE_T
                             "].meta.rate\"} value '%lf'.", i, rate));
                goto out;
            }
        } else
            rate = LBSM_DEFAULT_RATE;

        /* SSERV_Info.time <=== addrs[i].meta.expires */
        if (x_json_object_dothas_value_of_type(address, "meta.expires",
                                               JSONString)) {
            time = x_ParseExpires(x_json_object_dotget_string
                                  (address, "meta.expires"),
                                  (time_t) iter->time, i);
            if ( ! time)
                goto out;
        } else {
            static void* s_Once = 0;
            time = iter->time + LBSM_DEFAULT_TIME;
            if (CORE_Once(&s_Once)) {
                CORE_LOGF_X(eNSub_Json, eLOG_Trace,
                            ("Missing JSON {\"addrs[" FMT_SIZE_T
                             "].meta.expires\"} - using default expiration",
                             i));
            }
        }

        /* SSERV_Info.mime_t
           SSERV_Info.mime_s
           SSERV_Info.mime_e <=== addrs[i].meta.contentType */
        mime = x_json_object_dotget_string(address, "meta.contentType");

        /* SSERV_Info.extra <=== addrs[i].meta.extra */
        extra = x_json_object_dotget_string(address, "meta.extra");
        if ( ! extra  ||  ! *extra) {
            if (atype & fSERV_Http) {
                CORE_LOG_X(eNSub_Json, eLOG_Trace,
                           "Namerd API did not return a path in meta.extra "
                           "JSON for an HTTP type server");
                extra = "/";
            } else if (atype == fSERV_Ncbid) {
                CORE_LOG_X(eNSub_Json, eLOG_Trace,
                           "Namerd API did not return args in meta.extra "
                           "JSON for an NCBID type server");
                extra = "''";
            } else
                extra = "";
        }

        retval = 1; /* at least one info found */

        /* Make sure the server type matches an allowed type. */
        /* FIXME:  not quite */
        if (iter->types != fSERV_Any  &&  !(iter->types & atype)) {
            CORE_TRACEF(("Ignoring endpoint %s:%d with unallowed server"
                         " type '%s'(0x%04X) - allowed types = 0x%04X",
                         host, port, type, atype, iter->types));
            continue;
        }

        /* Make sure the JSON is stateless for a stateless SERV_ITER. */
        if ((iter->types & fSERV_Stateless)  &&  *stateful) {
            CORE_TRACEF(("Ignoring stateful endpoint %s:%d '%s' in stateless"
                         " search.", host, port, type));
            continue;
        }

        /* Prepare descriptor */
        size = sizeof(kDescrFmt) + strlen(type) + strlen(host) + 5/*port*/
            + strlen(extra) + (mime  &&  *mime ? 3 + strlen(mime) : 0)
            + strlen(local) + strlen(private) + 10/*rate*/
            + strlen(stateful) + 10/*time*/ + 40/*slack room*/;
        if (!(server_descr = (char*) malloc(size))) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't alloc for server descriptor.");
            goto out;
        }
        verify(sprintf(server_descr, kDescrFmt, type, host, port,
                       &" "[!(extra  &&  *extra)], extra ? extra : "",
                       mime  &&  *mime ? " C=" : "", mime ? mime : "",
                       local, private, rate < LBSM_STANDBY_THRESHOLD ? 3 : 2,
                       /* 3-digit precsion for standby; else 2-digits */
                       rate, stateful, time) < size);

        /* Parse descriptor into SSERV_Info */
        CORE_TRACEF(("Parsing server descriptor: '%s'", server_descr));
        SSERV_Info* info = SERV_ReadInfoEx(server_descr, iter->reverse_dns
                                           ? iter->name : "", 0/*false*/);
        if ( ! info) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                        ("Unable to add server info with descriptor '%s'.",
                         server_descr));
            free(server_descr);
            continue;
        }
        free(server_descr);
        CORE_TRACEF(("Created info: %p", info));

        /* Add new info to collection */
        if (!s_AddServerInfo(data, info)) {
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
    if (root)
        x_json_value_free(root);
    if (response)
        free((void*) response);
    CORE_TRACEF(("Leaving s_ParseResponse(): %d", retval));
    return retval;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    int/*bool*/ retval;
    EIO_Status  status;
    CONN        conn;
    CONNECTOR   c;

    CORE_TRACE("Entering s_Resolve()");

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

    CORE_TRACEF(("Leaving s_Resolve(): %d", retval));
    return retval;
}


static int/*bool*/ s_IsUpdateNeeded(TNCBI_Time now, struct SNAMERD_Data *data)
{
    int/*bool*/ expired = 0/*false*/;
    size_t i;

    /* Loop from highest index to lowest to ensure well-defined behavior when
       candidates are removed and to avoid memmove in s_RemoveCand(). */
    for (i = data->n_cand;  i > 0;  ) {
        const SSERV_Info* info = data->cand[--i].info;
        if (info->time < now) {
            CORE_TRACEF(("Endpoint expired (%u < %u): %p",
                         info->time, now, info));
            s_RemoveCand(data, i, 1);
            expired = 1/*true*/;
        }
    }

    return expired;
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

    if (!data->n_cand  &&  data->done) {
        CORE_TRACE("Leaving s_GetNextInfo() -- end of candidates");
        return 0;
    }

    if (!data->n_cand  ||  s_IsUpdateNeeded(iter->time, data)) {
        (void) s_Resolve(iter);
        if (!data->n_cand) {
            data->done = 1/*true*/;
            CORE_TRACE("Leaving s_GetNextInfo() -- resolved no candidates");
            return 0;
        }
    }

    /* Pick a randomized candidate. */
    n = LB_Select(iter, data, s_GetCandidate, NAMERD_LOCAL_BONUS);
    assert(n < data->n_cand);
    info       = (SSERV_Info*) data->cand[n].info;
    info->rate =               data->cand[n].status;

    /* Remove returned info */
    s_RemoveCand(data, n, 0);
    if (!data->n_cand)
        data->done = 1/*true*/;

    if (host_info)
        *host_info = NULL;

    CORE_TRACEF(("Leaving s_GetNextInfo(): %p", info));
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
    data->done = 0/*false*/;

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

 
/* Update a dtab value by appending another entry. */
static int/*bool*/ x_UpdateDtabInArgs(SConnNetInfo* net_info, const char* dtab)
{
    size_t dtablen, arglen, bufsize, dtab_in, dtab_out;
    const char* arg;
    char* buf;

    CORE_TRACEF(("Entering x_UpdateDtabInArgs(\"%s\")", dtab));

    /* Trim off all surrounding whitespace */
    while (*dtab  &&  isspace((unsigned char)(*dtab)))
        ++dtab;
    dtablen = strlen(dtab);
    while (dtablen  &&  isspace((unsigned char) dtab[dtablen - 1]))
        --dtablen;
    assert(!*dtab == !dtablen);

    if (!*dtab) {
        CORE_TRACE("Leaving x_UpdateDtabInArgs() -- nothing to do");
        return 1/*success*/;
    }

    /* Find any existing dtab in the args */
    arg = ConnNetInfo_GetArgs(net_info);
    assert(arg);
    for ( ;  *arg  &&  *arg != '#';  arg += arglen + !(arg[arglen] != '&')) {
        arglen = strcspn(arg, "&#");
        if (arglen < sizeof(NAMERD_DTAB_ARG))
            continue;
        if (strncasecmp(arg, NAMERD_DTAB_ARG "=", sizeof(NAMERD_DTAB_ARG)) !=0)
            continue;
        arg    += sizeof(NAMERD_DTAB_ARG);
        arglen -= sizeof(NAMERD_DTAB_ARG);
        if (arglen)
            break;
    }
    if (*arg  &&  *arg != '#') {
        assert(arglen);
        CORE_TRACEF(("x_UpdateDtabInArgs() existing dtab \"%.*s\"",
                     (int) arglen, arg));
    } else
        arglen = 0;

    /* Prepare new argument value, appending the new dtab, if necessary */
    bufsize = (arglen ? arglen + 3/*"%3B"*/ : 0) + dtablen * 3 + 1/*'\0'*/;
    if (!(buf = (char*) malloc(bufsize))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical, "Couldn't alloc for dtab.");
        CORE_TRACE("Leaving x_UpdateDtabInArgs() -- bad alloc");
        return 0/*failure*/;
    }
    if (arglen) {
        memcpy(buf,            arg, arglen);
        memcpy(buf + arglen, "%3B", 3); /* url-encoded ';' separator */
        arglen += 3;
    }

    /* Dtabs passed as query string parameter must be url-encoded, for example:
     *  from:   "/lbsm/bounce=>/service/xyz"
     *  to:     "%2Flbsm%2Fbounce%3D%3E%2Fservice%2Fxyz"
     */
    URL_Encode(dtab,         dtablen,          &dtab_in,
               buf + arglen, bufsize - arglen, &dtab_out);
    assert(dtablen == dtab_in  &&  dtab_out < bufsize - arglen);
    buf[arglen + dtab_out] = '\0'; /* not done by URL_Encode() */

    if (!ConnNetInfo_PostOverrideArg(net_info, NAMERD_DTAB_ARG, buf)) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Couldn't set dtab in args \"%s\"", buf));
        CORE_TRACE("Leaving x_UpdateDtabInArgs() -- value too long");
        free(buf);
        return 0/*failure*/;
    }

    CORE_TRACEF(("Leaving x_UpdateDtabInArgs() -- new dtab \"%s\"", buf));
    free(buf);
    return 1/*success*/;
}


static char* x_GetDtabFromHeader(const char* header)
{
    const char* line;
    size_t linelen;

    CORE_TRACEF(("Entering x_GetDtabFromHeader(%s%s%s)",
                 &"\""[!header], header ? header : "NULL", &"\""[!header]));
    
    for (line = header;  line  &&  *line;  line += linelen) {
        const char* end = strchr(line, '\n');
        linelen = end ? (size_t)(end - line) + 1 : strlen(line);
        if (!(end = memchr(line, ':', linelen)))
            continue;
        if ((size_t)(end - line) != sizeof(DTAB_HTTP_HEADER_TAG)-2/*":\0"*/)
            continue;
        if (strncasecmp(line, DTAB_HTTP_HEADER_TAG,
                        sizeof(DTAB_HTTP_HEADER_TAG)-2/*":\0"*/) == 0) {
            line    += sizeof(DTAB_HTTP_HEADER_TAG)-1/*"\0"*/;
            linelen -= sizeof(DTAB_HTTP_HEADER_TAG)-1/*"\0"*/;
            CORE_TRACEF(("Leaving x_GetDtabFromHeader(): "
                         DTAB_HTTP_HEADER_TAG " \"%.*s\"",
                         (int) linelen, line));
            return strndup(line, linelen);
        }
    }

    CORE_TRACE("Leaving x_GetDtabFromHeader() -- " DTAB_HTTP_HEADER_TAG
               " not found");
    return (char*)(-1L);
}


/* Long but very linear */
static int/*bool*/ x_SetupConnectionParams(SConnNetInfo* net_info,
                                           const char* name)
{
    char buf[CONN_PATH_LEN];
    size_t argslen, namelen;
    char* dtab;
    int n;

    /* Scheme */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_SCHEME,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_SCHEME)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme.");
        return 0/*failed*/;
    }
    switch (strlen(buf)) {
    case 0:
        net_info->scheme = eURL_Unspec;
        break;
    case 4:
        if (strcasecmp(buf, "http") == 0) {
            net_info->scheme = eURL_Http;
            break;
        }
        /*FALLTHRU*/
    case 5:
        if (strcasecmp(buf, "https") == 0) {
            net_info->scheme = eURL_Https;
            break;
        }
        /*FALLTHRU*/
    default:
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Unrecognized scheme \"%s\"", buf));
        return 0/*failed*/;
    }

    /* Request method */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_REQ_METHOD,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_REQ_METHOD)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for request method.");
        return 0/*failed*/;
    }
    switch (strlen(buf)) {
    case 0:
        net_info->req_method = eReqMethod_Any;
        break;
    case 3:
        if (strcasecmp(buf, "ANY") == 0) {
            net_info->req_method = eReqMethod_Any;
            break;
        }
        if (strcasecmp(buf, "GET") == 0) {
            net_info->req_method = eReqMethod_Get;
            break;
        }
        /*FALLTHRU*/
    case 4:
        if (strcasecmp(buf, "POST") == 0) {
            net_info->req_method = eReqMethod_Post;
            break;
        }
        /*FALLTHRU*/
    case 5:
        if (strcasecmp(buf, "ANY11") == 0) {
            net_info->req_method = eReqMethod_Any11;
            break;
        }
        if (strcasecmp(buf, "GET11") == 0) {
            net_info->req_method = eReqMethod_Get11;
            break;
        }
        /*FALLTHRU*/
    case 6:
        if (strcasecmp(buf, "POST11") == 0) {
            net_info->req_method = eReqMethod_Post11;
            break;
        }
        /*FALLTHRU*/
    default:
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Unrecognized request method \"%s\"", strupr(buf)));
        return 0/*failed*/;
    }

    /* Host */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_HOST,
                                       net_info->host, sizeof(net_info->host),
                                       DEF_NAMERD_API_HOST)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for host.");
        return 0/*failed*/;
    }
    if (NCBI_HasSpaces(net_info->host, strlen(net_info->host))) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Bad host \"%s\"", net_info->host));
        return 0/*failed*/;
    }

    /* Port */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_PORT,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_PORT)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for port.");
        return 0/*failed*/;
    }
    /* namerd doesn't support a port -- make sure none given */
    if (*buf  &&  (sscanf(buf, "%hu%n", &net_info->port, &n) < 1
                   ||  net_info->port != DEF_NAMERD_API_PORT  ||  buf[n])) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Bad port \"%s\"", buf));
        return 0/*failed*/;
    } else
        net_info->port = DEF_NAMERD_API_PORT;

    /* Path */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_PATH,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_PATH)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for path.");
        return 0/*failed*/;
    }
    if (!ConnNetInfo_SetPath(net_info, buf)) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Bad path \"%s\"", buf));
        return 0/*failed*/;
    }

    /* Env */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_ENV,
                                       buf + 1, sizeof(buf) - 1,
                                       DEF_NAMERD_API_ENV)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for env.");
        return 0/*failed*/;
    }
    if (buf[1]) {
        *buf = '/';
        if (!ConnNetInfo_AddPath(net_info, buf)) {
            CORE_LOGF_X(eNSub_Alloc, eLOG_Error,
                        ("Couldn't set env \"%s\"", buf));
            return 0/*failed*/;
        }
    }

    /* Args */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_ARGS,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_ARGS)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for args.");
        return 0/*failed*/;
    }  
    if ((argslen = strlen(buf)) + (namelen = strlen(name)) < sizeof(buf))
        memcpy(buf + argslen, name, namelen + 1);
    if (argslen + namelen >= sizeof(buf)
        ||  !ConnNetInfo_PreOverrideArg(net_info, buf, 0)) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("Args too long \"%s\"", buf));
        return 0/*failed*/;
    }

    /* Proxy: $http_proxy not overridden */
    if (!net_info->http_proxy_host[0]  ||
        !net_info->http_proxy_port     ||
        !net_info->http_proxy_only) {
        if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                           REG_NAMERD_PROXY_HOST,
                                           net_info->http_proxy_host,
                                           sizeof(net_info->http_proxy_host),
                                           DEF_NAMERD_PROXY_HOST)) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't get http proxy host.");
            return 0/*failed*/;
        }
        if (!net_info->http_proxy_host[0]
            ||  NCBI_HasSpaces(net_info->http_proxy_host,
                               strlen(net_info->http_proxy_host))) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                        ("%s proxy host \"%s\"",
                         net_info->http_proxy_host[0] ? "Bad" : "Empty",
                         net_info->http_proxy_host));
            return 0/*failed*/;
        }
        if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                           REG_NAMERD_PROXY_PORT,
                                           buf, sizeof(buf),
                                           DEF_NAMERD_PROXY_PORT)) {
            CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                       "Couldn't get http proxy port.");
            return 0/*failed*/;
        }
        if (!*buf  ||  sscanf(buf, "%hu%n", &net_info->http_proxy_port, &n) < 1
            ||  !net_info->http_proxy_port  ||  buf[n]) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                        ("%s proxy port \"%s\"", *buf ? "Bad" : "Empty", buf));
            return 0/*failed*/;
        }
        net_info->http_proxy_only = 0;  /* just in case */
    }

    /* Lastly, DTABs */
    /*  If net_info->http_user_header contains a DTab-Local: header, that value
     *  must be moved to net_info->args, which in turn populates the "dtab"
     *  argument in the HTTP query string.  It must not be in a header because
     *  if it was, it would apply to the namerd service itself, not the
     *  requested service.  Instead, the namerd service uses the dtab argument
     *  for resolution. */
    /* 1: service DTAB (either service-specific or global) */
    if (!(dtab = x_GetDtabFromHeader(net_info->http_user_header))) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't get dtab from http header.");
        return 0/*failed*/;
    }
    if (dtab  &&  dtab != (const char*)(-1L)) {
        if (!x_UpdateDtabInArgs(net_info, dtab))
            return 0/*failed*/;
        free(dtab);
    }
    /* 2: NAMERD DTAB */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_DTAB,
                                       buf + sizeof(DTAB_HTTP_HEADER_TAG),
                                       sizeof(buf)
                                       - sizeof(DTAB_HTTP_HEADER_TAG),
                                       DEF_NAMERD_DTAB)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Critical,
                   "Couldn't get NAMERD dtab from registry.");
        return 0/*failed*/;
    }
    memcpy(buf, DTAB_HTTP_HEADER_TAG " ", sizeof(DTAB_HTTP_HEADER_TAG));
    /* note that it also clears remnants of a service DTAB if "buf" is empty */
    if (!ConnNetInfo_OverrideUserHeader(net_info, buf)) {
        CORE_LOG_X(eNSub_Alloc, eLOG_Error,
                   "Couldn't set NAMERD dtab in http header.");
        return 0/*failed*/;
    }

    return 1/*succeeded*/;
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
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, net_info clone");
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

    if (!x_SetupConnectionParams(net_info, iter->name)) {
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- fail, connection params");
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

    if (!s_Resolve(iter)) {
        CORE_TRACE("Leaving SERV_NAMERD_Open() -- service not found");
        goto out;
    }
    if (data->n_cand)
        data->done = 1/*true*/;

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
