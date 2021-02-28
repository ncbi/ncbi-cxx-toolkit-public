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
#include "ncbi_priv.h"
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


/* NAMERD subcodes [1-10] for CORE_LOG*X() macros */
enum ENAMERD_Subcodes {
    eNSub_Message         = 0,   /**< not an error */
    eNSub_Alloc           = 1,   /**< memory allocation failed */
    eNSub_BadData         = 2,   /**< bad data was provided */
    eNSub_TooLong         = 3,   /**< data was too long to fit in a buffer */
    eNSub_Connect         = 4,   /**< problem in connect library */
    eNSub_Json            = 5    /**< a JSON parsing failure */
};


/*  Registry entry names and default values for NAMERD "SConnNetInfo" fields.
    We just override the given fields (which were collected for the service in
    question), so there are some standard keys plus some additional ones, which
    are purely for NAMERD.  Note that the namerd API doesn't support a port.
*/
#define DEF_NAMERD_REG_SECTION       "_NAMERD"

#define REG_NAMERD_API_SCHEME        "SCHEME"
#define DEF_NAMERD_API_SCHEME        "http"

#define REG_NAMERD_API_REQ_METHOD    REG_CONN_REQ_METHOD
#define DEF_NAMERD_API_REQ_METHOD    "GET"

#define REG_NAMERD_API_HTTP_VERSION  REG_CONN_HTTP_VERSION
#define DEF_NAMERD_API_HTTP_VERSION  0

#define REG_NAMERD_API_HOST          REG_CONN_HOST
#define DEF_NAMERD_API_HOST          "namerd-api.linkerd.ncbi.nlm.nih.gov"

#define REG_NAMERD_API_PORT          REG_CONN_PORT
#define DEF_NAMERD_API_PORT          0

#define REG_NAMERD_API_PATH          REG_CONN_PATH
#define DEF_NAMERD_API_PATH          "/api/1/resolve"

#define REG_NAMERD_API_ENV           "ENV"
#define DEF_NAMERD_API_ENV           "default"

#define REG_NAMERD_API_ARGS          REG_CONN_ARGS
#define DEF_NAMERD_API_ARGS          "path=/service/"

#define REG_NAMERD_PROXY_HOST        REG_CONN_HTTP_PROXY_HOST
/*  NAMERD_TODO - "temporarily" support plain "linkerd" on Unix only */
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_CYGWIN)
#define DEF_NAMERD_PROXY_HOST        "linkerd"
#else
#define DEF_NAMERD_PROXY_HOST        \
    "pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#endif

#define REG_NAMERD_PROXY_PORT        REG_CONN_HTTP_PROXY_PORT
#define DEF_NAMERD_PROXY_PORT        "4140"

#define REG_NAMERD_DTAB              "DTAB"
#define DEF_NAMERD_DTAB              ""

#define NAMERD_DTAB_ARG              "dtab"


/* Default rate increase 20% if svc runs locally */
#define NAMERD_LOCAL_BONUS           1.2

/* Misc */
#define DTAB_HTTP_HEADER_TAG         "DTab-Local:"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable kNamerdOp = {
    s_GetNextInfo, 0/*Feedback*/, 0/*s_Update*/, s_Reset, s_Close, "NAMERD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SNAMERD_Data {
    SConnNetInfo*  net_info;
    unsigned       resolved:1;
    unsigned       standby:1;
    TSERV_TypeOnly types;
    SLB_Candidate* cand;
    size_t         n_cand;
    size_t         a_cand;
};


/* Some static variables needed only to support testing with mock data.
 * Testing with mock data is currently limited to single-threaded tests. */
static const char* s_mock_body = 0;


/* Set up the ability to flexibly use arbitrary connector for reading from.
 * This will allow different input for testing with minimal code impact. */
static CONNECTOR s_CreateConnectorHttp  (SERV_ITER iter);
static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter);

typedef CONNECTOR (*FCreateConnector)(SERV_ITER iter);
static FCreateConnector s_CreateConnector = s_CreateConnectorHttp;


static CONNECTOR s_CreateConnectorHttp(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACEF(("NAMERD::s_CreateConnectorHttp(\"%s\")", iter->name));

    return HTTP_CreateConnector(data->net_info, 0/*user-headers*/, 0/*flags*/);
}


static CONNECTOR s_CreateConnectorMemory(SERV_ITER iter)
{
    BUF buf = 0;
    CORE_TRACEF(("NAMERD::s_CreateConnectorMemory(\"%s\")", iter->name));
    BUF_Append(&buf, s_mock_body, strlen(s_mock_body));
    return MEMORY_CreateConnectorEx(buf, 1/*own it!*/);
}


static void s_RemoveServerInfo(struct SNAMERD_Data* data, size_t n, int del
#ifdef _DEBUG
                               , const char* name
#endif /*_DEBUG*/
                               )
{
    CORE_TRACEF(("[%s]  %s server info " FMT_SIZE_T ": \"%s\" %p",
                 name, del ? "Deleting" : "Releasing", n,
                 SERV_NameOfInfo(data->cand[n].info), data->cand[n].info));

    assert(n < data->n_cand  &&  data->cand[n].info);
    if (del)
        free((void*) data->cand[n].info);
    if (n < --data->n_cand) {
        memmove(data->cand + n, data->cand + n + 1,
                (data->n_cand - n) * sizeof(*data->cand));
    }
}


static void s_ProcessForStandby(struct SNAMERD_Data* data
#ifdef _DEBUG
                                , const char* name
#endif /*_DEBUG*/
                                )
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
       candidates are removed and to avoid memmove in s_RemoveServerInfo() */
    for (i = data->n_cand;  i > 0;  ) {
        if (data->cand[--i].info->rate
            < (all_standby ? max_rate : LBSM_STANDBY_THRESHOLD)) {
            s_RemoveServerInfo(data, i, 1/*del*/
#ifdef _DEBUG
                               , name
#endif /*_DEBUG*/
                               );
        }
    }
}


static int/*bool*/ s_AddServerInfo(struct SNAMERD_Data* data, SSERV_Info* info
#ifdef _DEBUG
                                   , const char* name
#endif /*_DEBUG*/
                                   )
{
    const char* infoname = SERV_NameOfInfo(info);
    size_t n;

    /* First check if the new server info updates an existing one */
    for (n = 0;  n < data->n_cand;  ++n) {
        if (strcasecmp(infoname, SERV_NameOfInfo(data->cand[n].info)) == 0
            &&  SERV_EqualInfo(info, data->cand[n].info)) {
            /* Replace older version */
            CORE_TRACEF(("[%s]  Replacing server info " FMT_SIZE_T
                         ": \"%s\" %p", name, n, infoname, info));
            free((void*) data->cand[n].info);
            data->cand[n].info   = info;
            data->cand[n].status = info->rate;
            return 1/*success*/;
        }
    }

    /* Grow candidates container at capacity - trigger growth when there's no
       longer room for a new entry */
    if (data->n_cand == data->a_cand) {
        SLB_Candidate* temp;
        n = data->a_cand + 10;
        temp = (SLB_Candidate*)(data->cand
                                ? realloc(data->cand, n * sizeof(*temp))
                                : malloc (            n * sizeof(*temp)));
        if ( ! temp)
            return 0/*failure*/;
        data->cand = temp;
        data->a_cand = n;
    }

    /* Add the new service to the array */
    CORE_TRACEF(("[%s]  Adding server info " FMT_SIZE_T ": \"%s\" %p",
                 name, data->n_cand, infoname, info));
    data->cand[data->n_cand].info   = info;
    data->cand[data->n_cand].status = info->rate;
    data->n_cand++;
    return 1/*success*/;
}


/* Parse the "addrs[i].meta.expires" JSON from the namerd API, and adjust
   it according to the local timezone/DST to get the UTC epoch time.
   This function is not meant to be a generic ISO-8601 parser.  It expects
   the namerd API to return times in the format: "2017-03-29T23:02:55Z"
   Unfortunately, strptime is not supported at all on Windows, and doesn't
   support the "%z" format on Unix, so some custom parsing is required.
*/
static TNCBI_Time x_ParseExpires(const char* expires, time_t utc,
                                 const char* name, size_t i)
{
    struct tm tm_exp, tm_now;
    time_t exp, now;
    char   exp_zulu;
    double tzdiff;
    int    n;

    assert(utc);
    if ( ! expires  ||  ! *expires) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unable to get JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value", name, i));
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
        /* Get the UTC epoch time for the expires value */
        ||  (tm_exp.tm_year -= 1900,  /* years since 1900 */
             tm_exp.tm_mon--,         /* months since January: 0-11 */
             (exp = mktime(&tm_exp)) == (time_t)(-1))) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unexpected JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value \"%s\"", name, i, expires));
        return 0/*failure*/;
    }

    CORE_LOCK_WRITE;
    tm_now = *gmtime(&utc);
    CORE_UNLOCK;
    verify((now = mktime(&tm_now)) != (time_t)(-1));

    /* Adjust for time diff between local and UTC, which should
       correspond to 3600 x (number of time zones from UTC),
       i.e. diff between current TZ (UTC-12 to UTC+14) and UTC */
    tzdiff = difftime(utc, now);
    assert(-12.0 * 3600.0 <= tzdiff  &&  tzdiff <= 14.0 * 3600.0);
    exp += tzdiff;
    if (exp < utc) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unexpected JSON {\"addrs[" FMT_SIZE_T
                     "].meta.expires\"} value expired: " FMT_TIME_T " vs. "
                     FMT_TIME_T " now", name, i, exp, utc));
        return 0/*failure*/;
    }
    return (TNCBI_Time) exp;
}


/* Parsing the response requires having the entire response in one buffer.
*/
static const char* s_ReadFullResponse(CONN conn, const char* name)
{
    char*      response = 0;
    BUF        buf = 0;
    EIO_Status status;
    size_t     len;

    CORE_TRACEF(("Enter NAMERD::s_ReadFullResponse(\"%s\")", name));

    do {
        char block[2000];
        status = CONN_Read(conn, block, sizeof(block), &len, eIO_ReadPlain);
        if (!len)
            assert(status != eIO_Success);
        else if (!BUF_Write(&buf, block, len)) {
            CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                        ("[%s]  Failed to store response", name));
            status = eIO_Unknown;
            break;
        }
    } while (status == eIO_Success);
    if ((len = BUF_Size(buf)) > 0  &&  (response = (char*) malloc(len + 1))) {
        /* read all in */
        verify(BUF_Read(buf, response, len) == len  &&  !BUF_Size(buf));
        response[len] = '\0';
        CORE_TRACEF(("[%s]  Got response:\n%s", name, response));
    } else if (len) {
        CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to retrieve response", name));
    }
    BUF_Destroy(buf);

    CORE_TRACEF(("Leave NAMERD::s_ReadFullResponse(\"%s\"): " FMT_SIZE_T
                 ", %s", name, len, IO_StatusStr(status)));
    return response;
}


static int/*bool*/ s_ParseResponse(SERV_ITER iter, CONN conn)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;
    size_t               found = 0;
    x_JSON_Value*        root = 0;
    const char*          response;
    x_JSON_Array*        addrs;
    const char*          type;
    size_t               i, n;
    x_JSON_Object*       top;

    CORE_TRACEF(("Enter NAMERD::s_ParseResponse(\"%s\")", iter->name));

    if (!(response = s_ReadFullResponse(conn, iter->name)))
        goto out;

    /* root object */
    root = x_json_parse_string(response);
    if ( ! root) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Response failed to parse as JSON", iter->name));
        goto out;
    }
    if (x_json_value_get_type(root) != JSONObject) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Response is not a JSON object", iter->name));
        goto out;
    }
    top = x_json_value_get_object(root);
    if ( ! top) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unable to get JSON root object", iter->name));
        goto out;
    }

#if defined(_DEBUG)  &&  ! defined(NDEBUG)
    {{
        char json[9999];
        if (x_json_serialize_to_buffer_pretty(root, json, sizeof(json)-1)
            == JSONSuccess) {
            json[sizeof(json)-1] = '\0';
            CORE_TRACEF(("[%s]  Got JSON:\n%s", iter->name, json));
        } else {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Failed to serialize JSON", iter->name));
        }
    }}
#endif /*_DEBUG && !NDEBUG*/

    /* top-level {"type" : "bound"} expected for successful lookup */
    type = x_json_object_get_string(top, "type");
    if ( ! type) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unable to get JSON {\"type\"} value", iter->name));
        goto out;
    }
    if (strcmp(type, "bound") != 0) {
        CORE_TRACEF(("[%s]  Service unknown: \"%s\"", iter->name, type));
        goto out;
    }

    /* top-level {"addrs" : []} contains endpoint data */
    addrs = x_json_object_get_array(top, "addrs");
    if ( ! addrs) {
        CORE_LOGF_X(eNSub_Json, eLOG_Error,
                    ("[%s]  Unable to get JSON {\"addrs\"} array",iter->name));
        goto out;
    }

    /* Note: top-level {"meta" : {}} not currently used */

    /* Iterate through addresses, adding to "candidates" */
    n = x_json_array_get_count(addrs);
    for (i = 0;  i < n;  ++i) {
        const char*    host, *extra, *mime, *mode, *local,
                       *privat, *stateful, *secure;
        x_JSON_Object* address;
        char*          infostr;
        ESERV_Type     atype;
        SSERV_Info*    info;
        double         rate;
        int            port;
        TNCBI_Time     time;
        size_t         size;

        /*  JSON|SSERV_Info|variable mapping to format string:
            meta.secure|mode|secure ----------------------------------+
            meta.expires|time|time ---------------------------------+ |
            meta.stateful|mode|stateful -----------------------+    | |
            meta.rate|rate|rate --------------------------+    |    | |
            meta.mode|site|privat -------------------+    |    |    | |
            meta.mode|site|local ------------------+ |    |    |    | |
            meta.contentType|mime_*|mime ---+      | |    |    |    | |
            meta.extra|extra|extra -----+   |      | |    |    |    | |
            port|port|port -----------+ |   |      | |    |    |    | |
            ip|host|host ----------+  | |   |      | |    |    |    | |
            meta.serviceType       |  | |   |      | |    |    |    | |  */
        /*        |type|type ---+  |  | |   |      | |    |    |    | |  */
        static const        /*  [] [] [][__][__]   [][]   [___][]   [][] */
            char kDescrFmt[] = "%s %s:%u%s%s%s%s L=%s%s R=%.*lf%s T=%u%s";
        /*  NOTE: Some fields must not be included in certain situations
            because SERV_ReadInfoEx() does not expect them in those
            situations, and if they are present then SERV_ReadInfoEx()
            will prematurely terminate processing the descriptor.
            Specifically:
            do not include      when
            --------------      ---------------------
            P=                  type=DNS
            S=                  type=DNS or type=HTTP
            $=                  type=DNS
        */

        /* get a handle on the object for this iteration */
        address = x_json_array_get_object(addrs, i);
        if ( ! address) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Unable to get JSON {\"addrs[" FMT_SIZE_T
                         "]\"} object", iter->name, i));
            continue;
        }

        /* SSERV_Info.host <=== addrs[i].ip */
        host = x_json_object_get_string(address, "ip");
        if ( ! host  ||  ! *host) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Unable to get JSON {\"addrs[" FMT_SIZE_T
                         "].ip\"} value", iter->name, i));
            continue;
        }

        /* SSERV_Info.port <=== addrs[i].port */
        /* Unfortunately the x_json_object_get_number() function does
           not distinguish failure from a legitimate zero value :-/
           Therefore, first explicitly check for the value. */
        if ( ! x_json_object_has_value_of_type(address, "port", JSONNumber)) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Unable to get JSON {\"addrs[" FMT_SIZE_T
                         "].port\"} type", iter->name, i));
            continue;
        }
        port = (int) x_json_object_get_number(address, "port");
        if (port <= 0  ||  port > 65535) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Invalid JSON {\"addrs[" FMT_SIZE_T
                         "].port\"} value %d", iter->name, i, port));
            continue;
        }

        /* SSERV_Info.type <=== addrs[i].meta.serviceType */
        type = x_json_object_dotget_string(address, "meta.serviceType");
        if ( ! type  ||  ! *type) {
            atype = SERV_GetImplicitServerTypeInternal(iter->name);
            type  = SERV_TypeStr(atype);
        } else if ( ! (extra = SERV_ReadType(type, &atype))  ||  *extra) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Unrecognized {\"addrs[" FMT_SIZE_T
                         "].meta.serviceType\"} value \"%s\"", iter->name, i,
                         type));
            continue;
        }
        if (!*type  ||
            !(atype
              & (fSERV_Ncbid | fSERV_Standalone | fSERV_Http | fSERV_Dns))) {
            CORE_LOGF_X(eNSub_Json, eLOG_Error,
                        ("[%s]  Bogus {\"addrs[" FMT_SIZE_T
                         "].meta.serviceType\"} value \"%s\" (%u)", iter->name,
                         i, type, atype));
            continue;
        }

        CORE_TRACEF(("[%s]  Parsing for %s:%d '%s'",
                     iter->name, host, port, type));
        ++found;

        /* SSERV_Info.mode <=== addrs[i].meta.secure */
        if (x_json_object_dothas_value_of_type(address, "meta.secure",
                                               JSONBoolean)) {
            int sec = x_json_object_dotget_boolean(address, "meta.secure");
            if (sec == 0) {
                secure = "";
            } else if (sec != 1) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("[%s]  Invalid JSON {\"addrs[" FMT_SIZE_T
                             "].meta.secure\"} value %d", iter->name, i, sec));
                continue;
            } else if (atype == fSERV_Dns) {
                CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                            ("[%s]  Bogus JSON {\"addrs[" FMT_SIZE_T
                             "].meta.secure\"} value for '%s' server"
                             " type ignored", iter->name, i, type));
                secure = "";
            } else
                secure = " $=Yes";
        } else
            secure = "";

        /* SSERV_Info.mode <=== addrs[i].meta.stateful */
        if (x_json_object_dothas_value_of_type(address, "meta.stateful",
                                               JSONBoolean)) {
            int st = x_json_object_dotget_boolean(address, "meta.stateful");
            if (st == 0) {
                stateful = "";
            } else if (st != 1) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("[%s]  Invalid JSON {\"addrs[" FMT_SIZE_T
                             "].meta.stateful\"} value %d", iter->name, i,st));
                continue;
            } else if ((atype & fSERV_Http)  ||  atype == fSERV_Dns) {
                CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                            ("[%s]  Bogus JSON {\"addrs[" FMT_SIZE_T
                             "].meta.stateful\"} value for '%s' server"
                             " type ignored", iter->name, i, type));
                stateful = "";
            } else
                stateful = " S=Yes";
        } else
            stateful = "";

        /* SSERV_Info.site <=== addrs[i].meta.mode */
        privat = "";
        local  = "No";
        if (x_json_object_dothas_value_of_type(address, "meta.mode",
                                               JSONString)) {
            mode = x_json_object_dotget_string(address, "meta.mode");
            if ( ! mode  ||  ! *mode) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("[%s]  Unable to get JSON {\"addrs[" FMT_SIZE_T
                             "].meta.mode\"} value", iter->name, i));
                continue;
            }
            if (strcmp(mode, "L") == 0)
                local = "Yes";
            else if (strcmp(mode, "P") == 0)
                privat = " P=Yes";
            else if (strcmp(mode, "H") == 0)
                local = "Yes", privat = " P=Yes";
            else {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("[%s]  Unrecognized JSON {\"addrs[" FMT_SIZE_T
                             "].meta.mode\"} value \"%s\"", iter->name, i,
                             mode));
                continue;
            }
            if (*privat  &&  atype == fSERV_Dns) {
                CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                            ("[%s]  Bogus JSON {\"addrs[" FMT_SIZE_T
                             "].meta.mode\"} private value for '%s' server"
                             " type ignored", iter->name, i, type));
                privat = "";
            }
        }

        /* SSERV_Info.rate <=== addrs[i].meta.rate */
        if (x_json_object_dothas_value_of_type(address, "meta.rate",
                                               JSONNumber)) {
            rate = x_json_object_dotget_number(address, "meta.rate");
            /* verify magnitude */
            if (rate < SERV_MINIMAL_RATE  ||  SERV_MAXIMAL_RATE < rate) {
                CORE_LOGF_X(eNSub_Json, eLOG_Error,
                            ("[%s]  Unexpected JSON {\"addrs[" FMT_SIZE_T
                             "].meta.rate\"} value %lf", iter->name, i, rate));
                continue;
            }
        } else
            rate = LBSM_DEFAULT_RATE;

        /* SSERV_Info.time <=== addrs[i].meta.expires */
        if (x_json_object_dothas_value_of_type(address, "meta.expires",
                                               JSONString)) {
            time = x_ParseExpires(x_json_object_dotget_string
                                  (address, "meta.expires"),
                                  (time_t) iter->time, iter->name, i);
            if ( ! time)
                continue;
        } else
            time = iter->time + LBSM_DEFAULT_TIME;

        /* SSERV_Info.mime_t
           SSERV_Info.mime_s
           SSERV_Info.mime_e <=== addrs[i].meta.contentType */
        mime = x_json_object_dotget_string(address, "meta.contentType");
        if (mime  &&  !*mime)
            mime = 0;
        if (mime  &&  atype == fSERV_Dns) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                        ("[%s]  Bogus JSON {\"addrs[" FMT_SIZE_T
                         "].meta.contentType\"} value \"%s\" for '%s' server"
                         " type ignored", iter->name, i, mime, type));
            mime = 0;
        }

        /* SSERV_Info.extra <=== addrs[i].meta.extra */
        extra = x_json_object_dotget_string(address, "meta.extra");
        if ( ! extra  ||  ! *extra) {
            if (atype & fSERV_Http) {
                CORE_LOGF_X(eNSub_Json, eLOG_Trace,
                            ("[%s]  Empty path in {\"addrs[" FMT_SIZE_T
                             "].meta.extra\"} for an HTTP type server",
                             iter->name, i));
                extra = "/";
            } else if (atype == fSERV_Ncbid) {
                CORE_LOGF_X(eNSub_Json, eLOG_Trace,
                            ("[%s]  Empty args in {\"addrs[" FMT_SIZE_T
                             "].meta.extra\"} for an NCBID type server",
                             iter->name, i));
                extra = "''";
            } else
                extra = "";
        }

        /* Make sure the server type matches an allowed type */
        if ((!data->types  &&    atype == fSERV_Dns)  ||
            ( data->types  &&  !(atype & data->types))) {
            CORE_TRACEF(("Ignoring server info %s:%d with mismatching server"
                         " type '%s'(0x%04X) - allowed types = 0x%04X",
                         host, port, type, atype, data->types));
            continue;
        }
        /* Make sure no stateful JSON for a stateless SERV_ITER */
        if (*stateful  &&  (iter->types & fSERV_Stateless)) {
            CORE_TRACEF(("Ignoring stateful server info %s:%d '%s' in"
                         " stateless search", host, port, type));
            continue;
        }
        /* Make sure no local/private servers in external search */
        if (iter->external  &&  (*privat  ||  *local != 'N')) {
            CORE_TRACEF(("Ignoring %s server info %s:%d '%s' in"
                         " external search", *privat ? "private" : "local",
                         host, port, type));
            continue;
        }
        /* Make sure no regular entries if in standby mode already */
        if (rate >= LBSM_STANDBY_THRESHOLD  &&  data->standby) {
            CORE_TRACEF(("Ignoring regular server info %s:%d '%s' with rate"
                         " %.2lf in standby search", host, port, type, rate));
            continue;
        }

        /* Prepare server descriptor */
        size = sizeof(kDescrFmt) + strlen(type) + strlen(host) + 5/*port*/
            + strlen(extra) + (mime  &&  *mime ? 3 + strlen(mime) : 0)
            + strlen(local) + strlen(privat) + 10/*rate*/
            + strlen(stateful) + 10/*time*/ + 40/*slack room*/;
        if (!(infostr = (char*) malloc(size))) {
            CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                        ("[%s]  Failed to allocate for server descriptor",
                         iter->name));
            goto out;
        }
        verify((size_t)
               sprintf(infostr, kDescrFmt, type, host, port,
                       &" "[!(extra  &&  *extra)], extra ? extra : "",
                       mime  &&  *mime ? " C=" : "", mime ? mime : "",
                       local, privat, rate < LBSM_STANDBY_THRESHOLD ? 3 : 2,
                       /* 3-digit precision for standby; else 2-digits */
                       rate, stateful, time, secure) < size);

        /* Parse the descriptor into SSERV_Info */
        CORE_TRACEF(("[%s]  NAMERD parsing server descriptor: \"%s\"",
                     iter->name, infostr));
        info = SERV_ReadInfoEx(infostr, iter->reverse_dns
                               ? iter->name : "", 0/*false*/);
        if ( ! info) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                        ("[%s]  Failed to parse server descriptor \"%s\"",
                         iter->name, infostr));
            free(infostr);
            continue;
        }
        free(infostr);
        CORE_TRACEF(("Created server info: \"%s\" %p", iter->name, info));

        /*FIXME: the skip array*/

        /* Add new info to collection */
        if (!s_AddServerInfo(data, info
#ifdef _DEBUG
                             , iter->name
#endif /*_DEBUG*/
                             )) {
            CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                        ("[%s]  Failed to add server info", iter->name));
            CORE_TRACEF(("Freeing server info: \"%s\" %p", iter->name, info));
            free(info); /* not freed by failed s_AddServerInfo() */
            goto out;
        }
    }

    /* Post process for standby's */
    s_ProcessForStandby(data
#ifdef _DEBUG
                        , iter->name
#endif /*_DEBUG*/
                        );

 out:
    if (root)
        x_json_value_free(root);
    if (response)
        free((void*) response);
    CORE_TRACEF(("Leave NAMERD::s_ParseResponse(\"%s\"): " FMT_SIZE_T "/"
                 FMT_SIZE_T " found, " FMT_SIZE_T " available", iter->name,
                 found, n, data->n_cand));
    return found ? 1/*success*/ : 0/*failure*/;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    int/*bool*/ retval;
    EIO_Status  status;
    CONN        conn;
    CONNECTOR   c;

    CORE_TRACEF(("Enter NAMERD::s_Resolve(\"%s\")", iter->name));

    /* Create connector and connection, and fetch and parse the response */
    if (!(c = s_CreateConnector(iter))
        ||  (status = CONN_Create(c, &conn)) != eIO_Success) {
        char what[80];
        if (c)
            sprintf(what, "connection: %s", IO_StatusStr(status));
        else
            strcpy (what, "connector");
        CORE_LOGF_X(eNSub_Connect, eLOG_Error,
                    ("[%s]  Failed to create %s", iter->name, what));
        if (c)
            c->destroy(c);
        retval = 0/*failure*/;
    } else {
        retval = s_ParseResponse(iter, conn);

        CONN_Close(conn);
    }

    ((struct SNAMERD_Data*) iter->data)->resolved = 1/*true*/;
    CORE_TRACEF(("Leave NAMERD::s_Resolve(\"%s\"): %s", iter->name,
                 retval ? "success" : "failure"));
    return retval;
}


static int/*bool*/ s_IsUpdateNeeded(struct SNAMERD_Data* data, TNCBI_Time now
#ifdef _DEBUG
                                    , const char* name
#endif /*_DEBUG*/
                                    )
{
    int/*bool*/ expired = 0/*false*/;
    size_t i;

    /* Loop from highest index to lowest to ensure well-defined behavior when
       candidates are removed and to avoid memmove() in s_RemoveCand() */
    for (i = data->n_cand;  i > 0;  ) {
        const SSERV_Info* info = data->cand[--i].info;
        if (info->time < now) {
            CORE_TRACEF(("NAMERD expired server info (%u < %u): \"%s\" %p",
                         info->time, now, name, info));
            s_RemoveServerInfo(data, i, 1/*del*/
#ifdef _DEBUG
                               , name
#endif /*_DEBUG*/
                               );
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

    CORE_TRACEF(("Enter NAMERD::s_GetNextInfo(\"%s\")", iter->name));

    assert(data);

    if ((!data->n_cand  &&  !data->resolved)
        ||  s_IsUpdateNeeded(data, iter->time
#ifdef _DEBUG
                             , iter->name
#endif /*_DEBUG*/
                             )) {
        (void) s_Resolve(iter);
        assert(data->resolved);
    }
    if (!data->n_cand  &&  data->resolved) {
        CORE_TRACEF(("Leave NAMERD::s_GetNextInfo(\"%s\"): EOF", iter->name));
        return 0;
    }

    /* Pick a randomized candidate */
    n = LB_Select(iter, data, s_GetCandidate, NAMERD_LOCAL_BONUS);
    assert(n < data->n_cand);
    info       = (SSERV_Info*) data->cand[n].info;
    info->rate =               data->cand[n].status;

    /* Remove returned info */
    s_RemoveServerInfo(data, n, 0/*keep*/
#ifdef _DEBUG
                       , iter->name
#endif /*_DEBUG*/
                       );

    if (host_info)
        *host_info = 0;

    CORE_TRACEF(("Leave NAMERD::s_GetNextInfo(\"%s\"): \"%s\" %p", iter->name,
                 SERV_NameOfInfo(info), info));
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACEF(("Enter NAMERD::s_Reset(\"%s\"): " FMT_SIZE_T,
                 iter->name, data->n_cand));

    if (data->cand) {
        size_t i;
        assert(data->a_cand  &&  data->n_cand <= data->a_cand);
        for (i = 0;  i < data->n_cand;  ++i) {
            CORE_TRACEF(("Freeing server info " FMT_SIZE_T ": \"%s\" %p",
                         i, iter->name, data->cand[i].info));
            free((void*) data->cand[i].info);
        }
        data->n_cand = 0;
    }
    data->resolved = 0/*false*/;
    data->standby = 0/*false*/;

    CORE_TRACEF(("Leave NAMERD::s_Reset(\"%s\")", iter->name));
}


static void s_Close(SERV_ITER iter)
{
    struct SNAMERD_Data* data = (struct SNAMERD_Data*) iter->data;

    CORE_TRACEF(("Enter NAMERD::s_Close(\"%s\")", iter->name));
    iter->data = 0;

    assert(data  &&  !data->n_cand); /*s_Reset() had to be called before*/

    if (data->cand)
        free(data->cand);
    ConnNetInfo_Destroy(data->net_info);
    free(data);

    CORE_TRACEF(("Leave NAMERD::s_Close(\"%s\")", iter->name));
}


/* Update a dtab value by appending another entry */
static int/*bool*/ x_UpdateDtabInArgs(SConnNetInfo* net_info,
                                      const char*   dtab,
                                      const char*   name)
{
    size_t dtablen, arglen, bufsize, dtab_in, dtab_out;
    const char* arg;
    char* buf;

    CORE_TRACEF(("Enter NAMERD::x_UpdateDtabInArgs(\"%s\"): \"%s\"",
                 name, dtab));

    /* Trim off all surrounding whitespace */
    while (*dtab  &&  isspace((unsigned char)(*dtab)))
        ++dtab;
    dtablen = strlen(dtab);
    while (dtablen  &&  isspace((unsigned char) dtab[dtablen - 1]))
        --dtablen;
    assert(!*dtab == !dtablen);

    if (!*dtab) {
        CORE_TRACEF(("Leave NAMERD::x_UpdateDtabInArgs(\"%s\"): nothing to do",
                     name));
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
        CORE_TRACEF(("Found NAMERD::x_UpdateDtabInArgs(\"%s\") existing dtab"
                     " \"%.*s\"", name, (int) arglen, arg));
    } else
        arglen = 0;

    /* Prepare new argument value, appending the new dtab, if necessary */
    bufsize = (arglen ? arglen + 3/*"%3B"*/ : 0) + dtablen * 3 + 1/*'\0'*/;
    if (!(buf = (char*) malloc(bufsize))) {
        CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to %s service dtab %s\"%s\"", name,
                     arglen ? "extend" : "allocate for", dtab,
                     arglen ? "with "  : ""));
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
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Failed to set service dtab \"%s\"",
                     name, buf));
        free(buf);
        return 0/*failure*/;
    }

    CORE_TRACEF(("Leave NAMERD::x_UpdateDtabInArgs(\"%s\"): new dtab \"%s\"",
                 name, buf));
    free(buf);
    return 1/*success*/;
}


static char* x_GetDtabFromHeader(const char* header
#ifdef _DEBUG
                                 , const char* name
#endif /*_DEBUG*/
                                 )
{
    const char* line;
    size_t linelen;

    CORE_TRACEF(("Enter NAMERD::x_GetDtabFromHeader(\"%s\"): %s%s%s", name,
                 &"\""[!header], header ? header : "NULL", &"\""[!header]));

    for (line = header;  line  &&  *line;  line += linelen) {
        const char* end = strchr(line, '\n');
        linelen = end ? (size_t)(end - line) + 1 : strlen(line);
        if (!(end = (const char*) memchr(line, ':', linelen)))
            continue;
        if ((size_t)(end - line) != sizeof(DTAB_HTTP_HEADER_TAG)-2/*":\0"*/)
            continue;
        if (strncasecmp(line, DTAB_HTTP_HEADER_TAG,
                        sizeof(DTAB_HTTP_HEADER_TAG)-2/*":\0"*/) == 0) {
            line    += sizeof(DTAB_HTTP_HEADER_TAG)-1/*"\0"*/;
            linelen -= sizeof(DTAB_HTTP_HEADER_TAG)-1/*"\0"*/;
            CORE_TRACEF(("Leave NAMERD::x_GetDtabFromHeader(\"%s\"): "
                         DTAB_HTTP_HEADER_TAG " \"%.*s\"", name,
                         (int) linelen, line));
            return strndup(line, linelen);
        }
    }

    CORE_TRACEF(("Leave NAMERD::x_GetDtabFromHeader(\"%s\"): "
                 DTAB_HTTP_HEADER_TAG " not found", name));
    return (char*)(-1L);
}


/* Long but very linear */
static int/*bool*/ x_SetupConnectionParams(const SERV_ITER iter)
{
    SConnNetInfo* net_info = ((struct SNAMERD_Data*) iter->data)->net_info;
    char   buf[CONN_PATH_LEN + 1];
    size_t len, argslen, namelen;
    char*  dtab;
    int    n;

    /* Scheme */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_SCHEME,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_SCHEME)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD scheme", iter->name));
        return 0/*failed*/;
    }
    if (!*buf)
        net_info->scheme = eURL_Unspec;
    else if (strcasecmp(buf, "http") == 0)
        net_info->scheme = eURL_Http;
    else if (strcasecmp(buf, "https") == 0)
        net_info->scheme = eURL_Https;
    else {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("[%s]  Unrecognized NAMERD scheme \"%s\"",
                     iter->name, buf));
        return 0/*failed*/;
    }

    /* Request method */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_REQ_METHOD,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_REQ_METHOD)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD request method", iter->name));
        return 0/*failed*/;
    }
    if (!*buf  ||  strcasecmp(buf, "ANY") == 0)
        net_info->req_method = eReqMethod_Any;
    else if (strcasecmp(buf, "GET") == 0)
        net_info->req_method = eReqMethod_Get;
    else if (strcasecmp(buf, "POST") == 0)
        net_info->req_method = eReqMethod_Post;
    else {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("[%s]  Unrecognized NAMERD request method \"%s\"",
                     iter->name, strupr(buf)));
        return 0/*failed*/;
    }

    /* HTTP version */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_HTTP_VERSION,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_HTTP_VERSION)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD HTTP version", iter->name));
        return 0/*failed*/;
    }
    net_info->http_version = *buf  &&  atoi(buf) == 1 ? 1 : 0;

    /* Host */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_HOST,
                                       net_info->host, sizeof(net_info->host),
                                       DEF_NAMERD_API_HOST)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD host", iter->name));
        return 0/*failed*/;
    }
    if (!net_info->host[0]
        ||  NCBI_HasSpaces(net_info->host, strlen(net_info->host))) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("[%s]  %s NAMERD host \"%s\"", iter->name,
                     net_info->host[0] ? "Bad" : "Empty", net_info->host));
        return 0/*failed*/;
    }

    /* Port */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_PORT,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_PORT)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to check NAMERD port", iter->name));
        return 0/*failed*/;
    }
    /* namerd doesn't support port -- make sure none given */
    if (*buf  &&  (sscanf(buf, "%hu%n", &net_info->port, &n) < 1
                   ||  buf[n]  ||  net_info->port != DEF_NAMERD_API_PORT)) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("[%s]  Bad NAMERD port \"%s\"", iter->name, buf));
        return 0/*failed*/;
    } else
        net_info->port = DEF_NAMERD_API_PORT;

    /* Path */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_PATH,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_PATH)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD path", iter->name));
        return 0/*failed*/;
    }
    if (!ConnNetInfo_SetPath(net_info, buf)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Failed to set NAMERD path \"%s\"", iter->name,
                     buf));
        return 0/*failed*/;
    }

    /* Env */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_ENV,
                                       buf + 1, sizeof(buf) - 1,
                                       DEF_NAMERD_API_ENV)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD env", iter->name));
        return 0/*failed*/;
    }
    if (buf[1]) {
        *buf = '/';
        if (!ConnNetInfo_AddPath(net_info, buf)) {
            CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                        ("[%s]  Failed to set NAMERD env \"%s\"", iter->name,
                         buf));
            return 0/*failed*/;
        }
    }

    /* Args */
    if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                       REG_NAMERD_API_ARGS,
                                       buf, sizeof(buf),
                                       DEF_NAMERD_API_ARGS)) {
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s] Unable to get NAMERD args", iter->name));
        return 0/*failed*/;
    }
    argslen = strlen(buf);
    namelen = strlen(iter->name);
    len = argslen + namelen;
    if (iter->arglen) {
        len += 1 + iter->arglen;
        if (iter->val)
            len += 1 + iter->vallen;
    }
    if (len < sizeof(buf)) {
        len = argslen,
        memcpy(buf + len, iter->name, namelen);
        len += namelen;
        if (iter->vallen) {
            buf[len++] = '/';
            memcpy(buf + len, iter->arg, iter->arglen);
            len += iter->arglen;
            if (iter->val) {
                buf[len++] = '/';
                memcpy(buf + len, iter->val, iter->vallen);
                len += iter->vallen;
            }
        }
        buf[len] = '\0';
    }
    if (len >= sizeof(buf)  ||  !ConnNetInfo_PreOverrideArg(net_info, buf, 0)){
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s] Failed to set NAMERD args \"%s\"", iter->name,
                     buf));
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
            CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                        ("[%s]  Unable to get NAMERD http proxy host",
                         iter->name));
            return 0/*failed*/;
        }
        if (!net_info->http_proxy_host[0]
            ||  NCBI_HasSpaces(net_info->http_proxy_host,
                               strlen(net_info->http_proxy_host))) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                        ("[%s]  %s NAMERD http proxy host \"%s\"", iter->name,
                         net_info->http_proxy_host[0] ? "Bad" : "Empty",
                         net_info->http_proxy_host));
            return 0/*failed*/;
        }
        if ( ! ConnNetInfo_GetValueService(DEF_NAMERD_REG_SECTION,
                                           REG_NAMERD_PROXY_PORT,
                                           buf, sizeof(buf),
                                           DEF_NAMERD_PROXY_PORT)) {
            CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                        ("[%s]  Unable to get NAMERD http proxy port",
                         iter->name));
            return 0/*failed*/;
        }
        if (!*buf  ||  sscanf(buf, "%hu%n", &net_info->http_proxy_port, &n) < 1
            ||  buf[n]  ||  !net_info->http_proxy_port) {
            CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                        ("[%s]  %s NAMERD http proxy port \"%s\"", iter->name,
                         *buf ? "Bad" : "Empty", buf));
            return 0/*failed*/;
        }
        net_info->http_proxy_only = 1;
    }

    /* Lastly, DTABs */
    /*  If net_info->http_user_header contains a DTab-Local: tag, that value
     *  must be moved to net_info->args, which in turn populates the "dtab"
     *  argument in the HTTP query string.  It must not be in a header because
     *  if it was, it would apply to the namerd service itself, not the
     *  requested service.  Instead, the namerd service uses the dtab argument
     *  for resolution. */
    /* 1: service DTAB (either service-specific or global) */
    dtab = x_GetDtabFromHeader(net_info->http_user_header
#ifdef _DEBUG
                               , iter->name
#endif /*_DEBUG*/
                               );
    if (!dtab) {
        CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                    ("[%s]  Unable to get service dtab from http header",
                     iter->name));
        return 0/*failed*/;
    }
    if (dtab  &&  dtab != (const char*)(-1L)) {
        if (!x_UpdateDtabInArgs(net_info, dtab, iter->name))
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
        CORE_LOGF_X(eNSub_TooLong, eLOG_Error,
                    ("[%s]  Unable to get NAMERD dtab", iter->name));
        return 0/*failed*/;
    }
    memcpy(buf, DTAB_HTTP_HEADER_TAG " ", sizeof(DTAB_HTTP_HEADER_TAG));
    /* note that it also clears remnants of a service DTAB if "buf" is empty */
    if (!ConnNetInfo_OverrideUserHeader(net_info, buf)) {
        CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to set NAMERD dtab in http header",
                     iter->name));
        return 0/*failed*/;
    }

    return 1/*succeeded*/;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern const SSERV_VTable* SERV_NAMERD_Open(SERV_ITER           iter,
                                            const SConnNetInfo* net_info,
                                            SSERV_Info**        info)
{
    struct SNAMERD_Data* data;
    TSERV_TypeOnly types;

    assert(iter  &&  net_info  &&  !iter->data  &&  !iter->op);
    if (iter->ismask)
        return 0/*LINKERD doesn't support masks*/;
    assert(iter->name  &&  *iter->name);

    CORE_TRACEF(("Enter SERV_NAMERD_Open(\"%s\")", iter->name));

    /* Prohibit catalog-prefixed services, e.g. "/lbsm/<svc>" */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eNSub_BadData, eLOG_Error,
                    ("[%s]  Invalid NAMERD service name", iter->name));
        return 0;
    }

    types = iter->types & ~(fSERV_Stateless | fSERV_Firewall);
    if (iter->reverse_dns  &&  (!types  ||  (types & fSERV_Standalone))) {
        CORE_LOGF_X(eNSub_BadData, eLOG_Warning,
                    ("[%s]  NAMERD does not support Reverse-DNS service"
                     " name resolutions, use at your own risk!", iter->name));
    }

    if ( ! (data = (struct SNAMERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOGF_X(eNSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to allocate for SNAMERD_Data", iter->name));
        return 0;
    }
    iter->data = data;
    data->types = types;

    data->net_info = ConnNetInfo_Clone(net_info);
    if ( ! ConnNetInfo_SetupStandardArgs(data->net_info, iter->name)) {
        CORE_LOGF_X(data->net_info ? eNSub_TooLong : eNSub_Alloc,
                    data->net_info ? eLOG_Error    : eLOG_Critical,
                    ("[%s]  Failed to set up net_info", iter->name));
        s_Close(iter);
        return 0;
    }
    if (iter->types & fSERV_Stateless)
        data->net_info->stateless = 1/*true*/;
    if ((iter->types & fSERV_Firewall)  &&  !data->net_info->firewall)
        data->net_info->firewall = eFWMode_Adaptive;
    if (!x_SetupConnectionParams(iter)) {
        s_Close(iter);
        return 0;
    }

    ConnNetInfo_ExtendUserHeader(data->net_info,
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
        CORE_LOGF_X(eNSub_Message, eLOG_Trace,
                    ("SERV_NAMERD_Open(\"%s%s%s%s%s\"): Service not found",
                     iter->name,
                     &"/"[!iter->arglen],
                     iter->arg ? iter->arg : "",
                     &"/"[!iter->arglen || !iter->val],
                     iter->val ? iter->val : ""));
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    CORE_TRACEF(("Leave SERV_NAMERD_Open(\"%s\"): success", iter->name));
    return &kNamerdOp;
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
