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
 * Authors:  David McElhany
 *
 * File Description:
 *   Standard test for NAMERD service resolution facility.
 *
 */


#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_namerd.h"
#include "../ncbi_servicep.h"
#include "../parson.h"

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_service.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* ---------------------------------------------------------------------------
    Stub out the whole program for Windows DLL configurations.
    The problem is that the program uses PARSON, which does not have the
    dllexport/dllimport specifiers, resulting in link errors when built
    with DLL on Windows.
    */
#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)
int main(int argc, const char *argv[])
{
    printf("NCBI_UNITTEST_DISABLED - This program is disabled for WinDLL.\n");
    return 1;
}
#else

#ifdef _MSC_VER
#define unsetenv(n)     _putenv_s(n,"")
#define setenv(n,v,w)   _putenv_s(n,v)
#define FMT_SIZE_T      "%llu"
#define FMT_TIME_T      "%llu"
#else
#define FMT_SIZE_T      "%zu"
#define FMT_TIME_T      "%lu"
#endif


#define LEN_TYPE    12
#define LEN_HOST    15
#define LEN_XTRA    255
#define LEN_LOC     3
#define LEN_PRIV    3
#define LEN_STFL    3

#define MAX_HITS    40

#define MAX_TESTS   400




/*  ----------------------------------------------------------------------------
    Local Types
*/

typedef struct
{
    char            type[LEN_TYPE+1];   /* service type (eg HTTP, STANDALONE) */
    char            host[LEN_HOST+1];   /* host (eg 130.14.22.49) */
    unsigned short  port;
    char            xtra[LEN_XTRA+1];   /* meta.extra (e.g. cgi path) */
    char            loc [LEN_LOC+1];    /* local ("L") */
    char            priv[LEN_PRIV+1];   /* private ("H") */
    char            stfl[LEN_STFL+1];   /* stateful */
    int             match;              /* expected hit matches actual */
} SExpHit;

typedef enum {
    fMatch_None         = 0,
    fMatch_Type         = 0x01,
    fMatch_Host         = 0x02,
    fMatch_Port         = 0x04,
    fMatch_Extra        = 0x08,
    fMatch_Loc          = 0x10,
    fMatch_Priv         = 0x20,
    fMatch_Stateful     = 0x40,
    fMatch_All          = -1,
    fMatch_Default      = fMatch_All & ~fMatch_Extra,
    fMatch_NoHostPort   = fMatch_Default & ~fMatch_Host & ~fMatch_Port
} EMatch;

typedef struct
{
    const char*     name;
    const char*     result;
    int             live;
} STestResult;




/*  ----------------------------------------------------------------------------
    Static Variables
*/

static SExpHit      s_hits_exp[MAX_HITS];
static SExpHit      s_hits_got[MAX_HITS];
static int          s_n_hits_exp, s_n_hits_got;
static const char*  s_user_header = "";
static const char*  s_json_file = "test_ncbi_namerd_tests.json";
static STestResult  s_results[MAX_TESTS];




/*  ----------------------------------------------------------------------------
    Static Function Declarations
*/

static int check_match(EMatch flags, int idx_exp, int idx_got);

static int is_env_set(const char *name);

static int log_match_diffs(int idx_exp, int idx_got);

static int run_a_test(size_t test_idx, int live, const char *svc,
                      const char *hdr, int check_for_match, int exp_err,
                      const char *mock_body, int repop, int reset);

static int run_tests(int live, const char *test_nums);

static int test_service(int live, const char *svc);




/*  ----------------------------------------------------------------------------
    Static Functions
*/

/* Replace strings like: \"expires\":\"__________+00:00:27_\"
   with    strings like: \"expires\":\"2017-04-01T11:04:52Z\"

   Note that in the input string, the time is an offset [+-]hh:mm:ss from now.
   For simplicity, the input and adjusted time strings will be equal length,
   so the max offset is +/- 99:99:99.

   Also, note that the input mock data must be minified.  To support free-form
   JSON would require more complex parsing.
   */
static void adjust_mock_times(const char* mock_body, char** mock_body_adj_p)
{
    const char* cp;
    char        sign;
    int         hh, mm, ss;
    size_t      spec_len = strlen("\"expires\":\"__________+00:00:27_\"");
    size_t      prfx_len = strlen("\"expires\":\"");
    size_t      notz_len = strlen("2017-04-01T11:04:52");

    for (cp = mock_body;  cp  &&  *cp;  cp += spec_len) {
        /* Get next "expires" offset, if any. */
        const char* exp = strstr(cp, "\"expires\":\"__________");
        if ( ! exp) break;
        if (strlen(exp) < spec_len) break;
        if (sscanf(exp, "\"expires\":\"__________%c%2d:%2d:%2d_\"",
            &sign, &hh, &mm, &ss) != 4) break;
        if (sign != '-'  &&  sign != '+') break;
        if (hh < 0  ||  mm < 0  ||  ss < 0) break;
        ss += 60 * mm + 3600 * hh;

        /* Get the current UTC epoch time. */
        time_t      tt_now = time(0);
        struct tm   tm_now;
        CORE_LOCK_WRITE;
        tm_now = *gmtime(&tt_now);
        CORE_UNLOCK;

        /* Make the adjustment. */
        struct tm   tm_adj = tm_now;
        tm_adj.tm_sec += ss;
        mktime(&tm_adj); /* normalize */

        /* Put the adjusted time. */
        if ( ! *mock_body_adj_p)
            *mock_body_adj_p = strdup(mock_body);
        char* cp2 = *mock_body_adj_p + (exp - mock_body) + prfx_len;
        if (strftime(cp2, notz_len + 1, "%Y-%m-%dT%H:%M:%S", &tm_adj)
            != notz_len)
        {
            CORE_LOG(eLOG_Fatal, "Unexpected result from strftime(mock_body)");
        }
        cp2[notz_len] = 'Z'; /* undo strftime's overwriting of 'Z' by NIL */
    }
}


static int check_match(EMatch flags, int idx_exp, int idx_got)
{
    if ((flags & fMatch_Type) &&
        strcmp(s_hits_exp[idx_exp].type, s_hits_got[idx_got].type) != 0)
    {
        return 0;
    }

    if ((flags & fMatch_Host) &&
        strcmp(s_hits_exp[idx_exp].host, s_hits_got[idx_got].host) != 0)
    {
        /* always consider localhost a match */
        if (strcmp("127.0.0.1", s_hits_got[idx_got].host) != 0)
        {
            return 0;
        }
    }

    if ((flags & fMatch_Port) &&
        s_hits_exp[idx_exp].port != s_hits_got[idx_got].port)
    {
        return 0;
    }

    if ((flags & fMatch_Extra) &&
        strcmp(s_hits_exp[idx_exp].xtra, s_hits_got[idx_got].xtra) != 0)
    {
        return 0;
    }

    if ((flags & fMatch_Loc) &&
        strcmp(s_hits_exp[idx_exp].loc, s_hits_got[idx_got].loc) != 0)
    {
        return 0;
    }

    if ((flags & fMatch_Priv) &&
        strcmp(s_hits_exp[idx_exp].priv, s_hits_got[idx_got].priv) != 0)
    {
        return 0;
    }

    if ((flags & fMatch_Stateful) &&
        strcmp(s_hits_exp[idx_exp].stfl, s_hits_got[idx_got].stfl) != 0)
    {
        return 0;
    }

    return 1;
}


/* This is just to check if a Boolean value is set in the environment. */
static int is_env_set(const char *name)
{
    char        regbuf[10]; /* true/false, 1/0, on/off, yes/no */
    const char* reg;

    reg = CORE_REG_GET("", name, regbuf, sizeof(regbuf)-1, "");
    return (reg  &&  *reg) ? 1 : 0;
}


static int log_match_diffs(int idx_exp, int idx_got)
{
    if (strcmp(s_hits_exp[idx_exp].type, s_hits_got[idx_got].type) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "        type:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].type ? s_hits_exp[idx_exp].type : "(nothing)",
           *s_hits_got[idx_got].type ? s_hits_got[idx_got].type : "(nothing)"));
    }

    if (strcmp(s_hits_exp[idx_exp].host, s_hits_got[idx_got].host) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "        host:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].host ? s_hits_exp[idx_exp].host : "(nothing)",
           *s_hits_got[idx_got].host ? s_hits_got[idx_got].host : "(nothing)"));
    }

    if (s_hits_exp[idx_exp].port != s_hits_got[idx_got].port)
    {
        CORE_LOGF(eLOG_Note, (
            "        port:   expected: %-20hu      got: %-20hu",
            s_hits_exp[idx_exp].port, s_hits_got[idx_got].port));
    }

    if (strcmp(s_hits_exp[idx_exp].xtra, s_hits_got[idx_got].xtra) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "        xtra:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].xtra ? s_hits_exp[idx_exp].xtra : "(nothing)",
           *s_hits_got[idx_got].xtra ? s_hits_got[idx_got].xtra : "(nothing)"));
    }

    if (strcmp(s_hits_exp[idx_exp].loc, s_hits_got[idx_got].loc) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "         loc:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].loc ? s_hits_exp[idx_exp].loc : "(nothing)",
           *s_hits_got[idx_got].loc ? s_hits_got[idx_got].loc : "(nothing)"));
    }

    if (strcmp(s_hits_exp[idx_exp].priv, s_hits_got[idx_got].priv) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "        priv:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].priv ? s_hits_exp[idx_exp].priv : "(nothing)",
           *s_hits_got[idx_got].priv ? s_hits_got[idx_got].priv : "(nothing)"));
    }

    if (strcmp(s_hits_exp[idx_exp].stfl, s_hits_got[idx_got].stfl) != 0)
    {
        CORE_LOGF(eLOG_Note, (
            "        stfl:   expected: %-20s      got: %-20s",
           *s_hits_exp[idx_exp].stfl ? s_hits_exp[idx_exp].stfl : "(nothing)",
           *s_hits_got[idx_got].stfl ? s_hits_got[idx_got].stfl : "(nothing)"));
    }

    return 1;
}


static int run_a_test(size_t test_idx, int live, const char *svc,
                      const char *hdr, int check_for_match, int exp_err,
                      const char *mock_body_in, int repop, int reset)
{
    const SSERV_Info    *info = NULL;
    SConnNetInfo        *net_info;
    SERV_ITER           iter;
    const char          *mock_body = NULL;
    char                *mock_body_adj = NULL;
    int                 n_matches_perfect = 0, n_matches_near = 0;
    int                 success = 0, errors = 0;
    int                 retval = -1;

    s_n_hits_got = 0;

    /* Adjust mock data for current time, if necessary. */
    adjust_mock_times(mock_body_in, &mock_body_adj);
    mock_body = mock_body_adj ? mock_body_adj : mock_body_in;

    /* Select the HTTP data source (live or mock). */
    s_results[test_idx].live = live;
    if ( ! s_results[test_idx].live  &&
        ( ! mock_body  ||  ! *mock_body))
    {
        CORE_TRACE("Mock HTTP data source unavailable.");
        s_results[test_idx].live = 1;
    }
    if (s_results[test_idx].live) {
        CORE_TRACE("Using a live HTTP data source.");
        SERV_NAMERD_SetConnectorSource(NULL); /* use live HTTP */
    } else {
        CORE_TRACE("Using a mock HTTP data source.");
        if ( ! SERV_NAMERD_SetConnectorSource(mock_body)) {
            CORE_LOG(eLOG_Error, "Unable to create mock HTTP data source.");
            retval = 1;
            goto out;
        }
    }

    /* Set up the server iterator. */
    net_info = ConnNetInfo_Create(svc);
    if (*hdr)  ConnNetInfo_SetUserHeader(net_info, hdr);
    iter = SERV_OpenP(svc, fSERV_All |
                      (strpbrk(svc, "?*[") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);

    /* Fetch the server hits from namerd. */
    if (iter) {
        for (; s_n_hits_got < MAX_HITS  &&  (info = SERV_GetNextInfo(iter));
             ++s_n_hits_got)
        {
            if (info->type & fSERV_Http) {
                CORE_LOGF(eLOG_Note, ("    HTTP extra (path): %s",
                                      SERV_HTTP_PATH(&info->u.http)));
            }
            strcpy(s_hits_got[s_n_hits_got].type, SERV_TypeStr(info->type));
            strcpy(s_hits_got[s_n_hits_got].xtra,
                (info->type & fSERV_Http) ? SERV_HTTP_PATH(&info->u.http) : "");
            strcpy(s_hits_got[s_n_hits_got].loc ,
                (info->site & fSERV_Local   ) ? "yes" : "no");
            strcpy(s_hits_got[s_n_hits_got].priv,
                (info->site & fSERV_Private ) ? "yes" : "no");
            strcpy(s_hits_got[s_n_hits_got].stfl,
                (info->mode & fSERV_Stateful) ? "yes" : "no");

            SOCK_ntoa(info->host, s_hits_got[s_n_hits_got].host, LEN_HOST);

            s_hits_got[s_n_hits_got].port = info->port;

            s_hits_got[s_n_hits_got].match = 0;

            char    *info_str;
            info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, (
                "    Found server %d: "
                "(type, host:post, local, private, stateful, extra) = "
                "(%s, %s:%hu, %s, %s, %s, '%s')",
                s_n_hits_got,                   s_hits_got[s_n_hits_got].type,
                s_hits_got[s_n_hits_got].host,  s_hits_got[s_n_hits_got].port,
                s_hits_got[s_n_hits_got].loc,   s_hits_got[s_n_hits_got].priv,
                s_hits_got[s_n_hits_got].stfl,  s_hits_got[s_n_hits_got].xtra));
            if (info_str)
                free(info_str);
        }

        /* Make sure endpoint data can be repopulated and reset. */
#if 0
        if (repop  &&  s_n_hits_got) {
            /* repopulate */
            CORE_LOG(eLOG_Trace, "Repopulating the service mapper.");
            if ( ! info  &&  ! SERV_GetNextInfo(iter)) {
                /* THIS IS A LOGIC ERROR:  if the iterator has reached its end
                 * repopulating it might end up with all duplicates (to skip)
                 * and result in no new entries!  Which is NOT an error. */
                CORE_LOG(eLOG_Error, "Unable to repopulate endpoint data.");
                errors = 1;
            }
        }
#endif
        if (reset  &&  s_n_hits_got) {
            /* reset */
            CORE_LOG(eLOG_Trace, "Resetting the service mapper.");
            SERV_Reset(iter);
            if ( ! SERV_GetNextInfo(iter)) {
                CORE_LOG(eLOG_Error, "No services found after reset.");
                errors = 1;
            }
        }

        SERV_Close(iter);
    } else {
        errors = 1;
    }

    /* Search for matches unless this is a standalone run. */
    if (check_for_match) {
        /* Search for perfect matches first (order is unknown). */
        int it_exp, it_got;
        for (it_got=0; it_got < s_n_hits_got; ++it_got) {
            for (it_exp=0; it_exp < s_n_hits_exp; ++it_exp) {
                if (s_hits_exp[it_exp].match) continue;

                /*if (check_match(fMatch_Default, it_exp, it_got)) {*/
                if (check_match(fMatch_All, it_exp, it_got)) {
                    CORE_LOGF(eLOG_Note, (
                        "    Found server %d perfectly matched expected server "
                        "%d.", it_got, it_exp));
                    s_hits_exp[it_exp].match = 1;
                    s_hits_got[it_got].match = 1;
                    ++n_matches_perfect;
                    break;
                }
            }
        }
        /* If not all found, search again but exclude host:port from match. */
        for (it_got=0; it_got < s_n_hits_got; ++it_got) {
            if (s_hits_got[it_got].match) continue;
            for (it_exp=0; it_exp < s_n_hits_exp; ++it_exp) {
                if (s_hits_exp[it_exp].match) continue;

                if (check_match(fMatch_NoHostPort, it_exp, it_got)) {
                    CORE_LOGF(eLOG_Note, (
                       "    Found server %d nearly matched expected server %d.",
                        it_got, it_exp));
                    s_hits_exp[it_exp].match = 1;
                    s_hits_got[it_got].match = 1;
                    ++n_matches_near;
                    log_match_diffs(it_exp, it_got);
                    break;
                }
            }
        }
        /* List any non-matching servers. */
        for (it_exp=0; it_exp < s_n_hits_exp; ++it_exp) {
            if ( ! s_hits_exp[it_exp].match)
                CORE_LOGF(eLOG_Note, (
                    "    Expected server %d didn't match any found servers.",
                    it_exp));
        }
        for (it_got=0; it_got < s_n_hits_got; ++it_got) {
            if ( ! s_hits_got[it_got].match)
                CORE_LOGF(eLOG_Note, (
                    "    Found server %d didn't match any expected servers.",
                    it_got));
        }

        CORE_LOGF(n_matches_perfect + n_matches_near == s_n_hits_got ?
                  eLOG_Note : eLOG_Error,
                  ("Expected %d servers; found %d (%d perfect matches, %d near "
                   "matches, and %d non-matches).",
                  s_n_hits_exp, s_n_hits_got, n_matches_perfect, n_matches_near,
                  s_n_hits_got - n_matches_perfect - n_matches_near));

        if (!errors  &&
            s_n_hits_got == s_n_hits_exp  &&
            s_n_hits_got == n_matches_perfect + n_matches_near)
        {
            success = 1;
        }
        retval = (success != exp_err ? 1 : 0);
        CORE_LOGF(eLOG_Note, ("Test result:  %s.",
            retval ?
            (success ? "PASS" : "PASS (with expected error)") :
            (success ? "FAIL (success when error expected)" : "FAIL")));
    }

out:
    if (mock_body_adj)
        free(mock_body_adj);
    return retval == -1 ? (success != exp_err ? 1 : 0) : retval;
}


/* mostly error-handling-free for the moment  :-/  */
static int run_tests(int live, const char *test_nums)
{
    x_JSON_Value    *root_value = NULL;
    x_JSON_Object   *root_obj;
    x_JSON_Array    *test_arr;
    const char      *test_num = test_nums;
    size_t          n_tests = 0, n_pass = 0, n_fail = 0, n_skip = 0;
    size_t          n_comm_unset, n_comm_set;
    int             all_enabled = 0;

    CORE_LOG(eLOG_Note,
        "============================================================");
    CORE_LOGF(eLOG_Note, ("Test file: %s", s_json_file));

    root_value = x_json_parse_file(s_json_file);
    root_obj   = x_json_value_get_object(root_value);

    if (x_json_object_has_value_of_type(root_obj, "all_enabled", JSONNumber)  &&
        (int)x_json_object_get_number(root_obj, "all_enabled") != 0)
    {
        all_enabled = 1;
    }

    test_arr = x_json_object_get_array(root_obj, "tests");
    n_tests = x_json_array_get_count(test_arr);
    size_t it;
    for (it = 0; it < n_tests; ++it) {
        x_JSON_Object   *test_obj;
        x_JSON_Array    *hit_arr;
        const char      *svc, *hdr;
        int             err = 0;

        test_obj = x_json_array_get_object(test_arr, it);
        s_results[it].name = x_json_object_get_string(test_obj, "alias");

        /* Skip tests not in user-supplied test numbers list. */
        if (test_nums  &&  *test_nums) {
            size_t next_test;
            if ( ! test_num)
                break;
            if (sscanf(test_num, FMT_SIZE_T, &next_test) == 1) {
                if (it+1 != next_test) {
                    s_results[it].result = "-";
                    continue;
                } else {
                    test_num = strchr(test_num, ',');
                    if (test_num  &&  *test_num)
                        test_num++;
                }
            } else {
                CORE_LOGF(eLOG_Error, ("Invalid test numbers list: %s",
                    test_num));
                return 0;
            }
        }

        CORE_LOG(eLOG_Note,
            "============================================================");

        /* Skip disabled tests. */
        if ( ! all_enabled) {
            if (x_json_object_has_value_of_type(test_obj, "disabled",
                JSONNumber))
            {
                if ((int)x_json_object_get_number(test_obj, "disabled") == 1) {
                    ++n_skip;
                    CORE_LOGF(eLOG_Note, ("Skipping test " FMT_SIZE_T ": %s",
                        it+1, x_json_object_get_string(test_obj, "alias")));
                    s_results[it].result = "skip";
                    continue;
                }
            }
        }

        CORE_LOGF(eLOG_Note, ("Running test " FMT_SIZE_T ": %s", it+1,
            s_results[it].name));

        svc = x_json_object_get_string(test_obj, "service");
        if (x_json_object_has_value_of_type(test_obj, "http_user_header",
                JSONString))
            hdr = x_json_object_get_string(test_obj, "http_user_header");
        else
            hdr = "";

        if (x_json_object_has_value_of_type(test_obj, "expect_error",
            JSONString))
        {
            if (strcmp(x_json_object_get_string(test_obj, "expect_error"),
                        "yes") == 0)
            {
                err = 1;
            }
        }

        if (x_json_object_has_value_of_type(test_obj, "url", JSONString))
            CORE_LOGF(eLOG_Note, ("    url:              %s",
                x_json_object_get_string(test_obj, "url")));
        CORE_LOGF(eLOG_Note, ("    service:          %s", svc));
        CORE_LOGF(eLOG_Note, ("    header:           %s",
            *hdr ? hdr : "(none)"));
        CORE_LOGF(eLOG_Note, ("    expected error:   %s", err ? "yes" : "no"));

        /* Redo common unset/set - they could have been changed by a prior
            test. */
        if (x_json_object_dothas_value_of_type(root_obj, "common.env_unset",
            JSONArray))
        {
            x_JSON_Array    *comm_unset_arr;

            comm_unset_arr = x_json_object_dotget_array(root_obj,
                                "common.env_unset");
            n_comm_unset   = x_json_array_get_count(comm_unset_arr);

            for (size_t it2 = 0; it2 < n_comm_unset; ++it2) {
                const char  *name = x_json_array_get_string(
                                    comm_unset_arr, it2);

                CORE_LOGF(eLOG_Note, ("    Unsetting common var: %s", name));
                unsetenv(name);
                if (getenv(name) != NULL) {
                    CORE_LOGF(eLOG_Critical,
                        ("Unable to unset common env var %s.", name));
                    return 0;
                }
            }
        }
        if (x_json_object_dothas_value_of_type(root_obj, "common.env_set",
            JSONArray))
        {
            x_JSON_Array    *comm_set_arr;

            comm_set_arr   = x_json_object_dotget_array(root_obj,
                                "common.env_set");
            n_comm_set     = x_json_array_get_count(comm_set_arr);

            for (size_t it2 = 0; it2 < n_comm_set; ++it2) {
                x_JSON_Object   *env_obj = x_json_array_get_object(comm_set_arr,
                                            it2);
                const char      *name = x_json_object_get_name(env_obj, 0);
                const char      *val = x_json_value_get_string(
                                        x_json_object_get_value_at(env_obj, 0));

                CORE_LOGF(eLOG_Note, ("    Setting common var: %s=%s", name,
                          val));
                setenv(name, val, 1);
                if (strcmp(val, getenv(name)) != 0) {
                    CORE_LOGF(eLOG_Critical,
                        ("Unable to set common env var %s to '%s'.",
                         name, val));
                    return 0;
                }
            }
        }

        /* Per-test unset/set */
        if (x_json_object_has_value_of_type(test_obj, "env_unset", JSONArray)) {
            x_JSON_Array    *unset_arr;
            size_t          n_unset;

            unset_arr = x_json_object_get_array(test_obj, "env_unset");
            n_unset = x_json_array_get_count(unset_arr);
            CORE_LOGF(eLOG_Note, (
                "    Unset per-test environment variables: " FMT_SIZE_T,
                n_unset));
            if (n_unset) {
                for (size_t it2 = 0; it2 < n_unset; ++it2) {
                    const char  *name = x_json_array_get_string(unset_arr, it2);

                    CORE_LOGF(eLOG_Note, ("        %s", name));
                    unsetenv(name);
                    if (getenv(name) != NULL) {
                        CORE_LOGF(eLOG_Critical,
                            ("Unable to unset per-test env var %s.", name));
                        return 0;
                    }
                }
            }
        }
        if (x_json_object_has_value_of_type(test_obj, "env_set", JSONArray)) {
            x_JSON_Array    *set_arr;
            size_t          n_set;

            set_arr = x_json_object_get_array(test_obj, "env_set");
            n_set = x_json_array_get_count(set_arr);
            CORE_LOGF(eLOG_Note, (
                "    Set per-test environment variables: " FMT_SIZE_T,
                n_set));
            if (n_set) {
                for (size_t it2 = 0; it2 < n_set; ++it2) {
                    x_JSON_Object   *env_obj = x_json_array_get_object(set_arr,
                                                it2);
                    const char      *name = x_json_object_get_name(env_obj, 0);
                    const char      *val = x_json_value_get_string(
                                        x_json_object_get_value_at(env_obj, 0));

                    CORE_LOGF(eLOG_Note, ("        %s=%s", name, val));
                    setenv(name, val, 1);
                    if (strcmp(val, getenv(name)) != 0) {
                        CORE_LOGF(eLOG_Critical,
                            ("Unable to set per-test env var %s to '%s'.",
                             name, val));
                        return 0;
                    }
                }
            }
        }

        hit_arr = x_json_object_get_array(test_obj, "expect_hits");
        s_n_hits_exp = (int)x_json_array_get_count(hit_arr);
        if (s_n_hits_exp >= MAX_HITS) {
            CORE_LOG(eLOG_Critical, "Too many expected hits.");
            return 0;
        }
        CORE_LOGF(eLOG_Note, ("    Expected hits: %d", s_n_hits_exp));
        int it2;
        for (it2 = 0; it2 < s_n_hits_exp; ++it2) {
            x_JSON_Object   *hit_obj = x_json_array_get_object(hit_arr, it2);
            const char      *type = "HTTP", *loc = "no", *priv = "no";
            const char      *xtra = "", *stfl = "no";

            if (x_json_object_has_value_of_type(hit_obj, "type", JSONString))
                type = x_json_object_get_string(hit_obj, "type");
            if (x_json_object_has_value_of_type(hit_obj, "xtra", JSONString))
                xtra = x_json_object_get_string(hit_obj, "xtra");
            if (x_json_object_has_value_of_type(hit_obj, "loc",  JSONString))
                loc  = x_json_object_get_string(hit_obj, "loc" );
            if (x_json_object_has_value_of_type(hit_obj, "priv", JSONString))
                priv = x_json_object_get_string(hit_obj, "priv");
            if (x_json_object_has_value_of_type(hit_obj, "stfl", JSONString))
                stfl = x_json_object_get_string(hit_obj, "stfl");

            if (strlen(type) > LEN_TYPE) {
                CORE_LOGF(eLOG_Critical, ("type string '%s' too long.", type));
                return 0;
            }
            if (strlen(xtra) > LEN_XTRA) {
                CORE_LOGF(eLOG_Critical, ("xtra string '%s' too long.", xtra));
                return 0;
            }
            if (strlen(loc) > LEN_LOC) {
                CORE_LOGF(eLOG_Critical, ("loc string '%s' too long.", loc));
                return 0;
            }
            if (strlen(priv) > LEN_PRIV) {
                CORE_LOGF(eLOG_Critical, ("priv string '%s' too long.", priv));
                return 0;
            }
            if (strlen(stfl) > LEN_STFL) {
                CORE_LOGF(eLOG_Critical, ("stfl string '%s' too long.", stfl));
                return 0;
            }

            strcpy(s_hits_exp[it2].type, type);
            strcpy(s_hits_exp[it2].xtra, xtra);
            strcpy(s_hits_exp[it2].loc , loc );
            strcpy(s_hits_exp[it2].priv, priv);
            strcpy(s_hits_exp[it2].stfl, stfl);

            if ( ! x_json_object_has_value_of_type(hit_obj, "host",
                JSONString))
            {
                CORE_LOG(eLOG_Critical, "hit object missing host.");
                return 0;
            }
            if (strlen(x_json_object_get_string(hit_obj, "host")) > LEN_HOST)
            {
                CORE_LOG(eLOG_Critical, "host string too long.");
                return 0;
            }
            strcpy(s_hits_exp[it2].host, x_json_object_get_string(hit_obj,
                "host"));

            if ( ! (strcmp(s_hits_exp[it2].loc , "no" ) == 0  ||
                    strcmp(s_hits_exp[it2].loc , "yes") == 0))
            {
                CORE_LOG(eLOG_Critical, "hits loc must be 'no' or 'yes'.");
                return 0;
            }
            if ( ! (strcmp(s_hits_exp[it2].priv , "no" ) == 0  ||
                    strcmp(s_hits_exp[it2].priv , "yes") == 0))
            {
                CORE_LOG(eLOG_Critical, "hits priv must be 'no' or 'yes'.");
                return 0;
            }
            if ( ! (strcmp(s_hits_exp[it2].stfl , "no" ) == 0  ||
                    strcmp(s_hits_exp[it2].stfl , "yes") == 0))
            {
                CORE_LOG(eLOG_Critical, "hits stfl must be 'no' or 'yes'.");
                return 0;
            }

            if ( ! x_json_object_has_value_of_type(hit_obj, "port",
                JSONNumber))
            {
                CORE_LOG(eLOG_Critical, "JSON hit missing a port.");
                return 0;
            }
            double dbl_port = x_json_object_get_number(hit_obj, "port");
            if (dbl_port < 1.0  ||  dbl_port > 65535.0)
            {
                CORE_LOGF(eLOG_Critical, ("Invalid JSON hit port, %lf.",
                    dbl_port));
                return 0;
            }
            s_hits_exp[it2].port = (unsigned short)dbl_port;

            s_hits_exp[it2].match = 0;

            CORE_LOGF(eLOG_Note, (
                "        Expected server %d: "
                "(type, host:post, local, private, stateful, extra) = "
                "(%s, %s:%hu, %s, %s, %s, '%s')",
                it2,                    s_hits_exp[it2].type,
                s_hits_exp[it2].host,   s_hits_exp[it2].port,
                s_hits_exp[it2].loc,    s_hits_exp[it2].priv,
                s_hits_exp[it2].stfl,   s_hits_exp[it2].xtra));
        }

        const char *mock_body = NULL;
        if (x_json_object_has_value_of_type(test_obj, "mock_body", JSONString))
        {
            mock_body = x_json_object_get_string(test_obj, "mock_body");
            if (mock_body  &&  *mock_body)
                CORE_LOG(eLOG_Note, "    Mock HTTP body found.");
            else
                CORE_LOG(eLOG_Note, "    Empty mock HTTP body found.");
        }

        int repop = 0, reset = 0;
        if (x_json_object_has_value_of_type(
                test_obj, "iter-repop", JSONString)  &&
            strcmp(x_json_object_get_string(
                test_obj, "iter-repop"), "yes") == 0)
        {
            repop = 1;
        }
        if (x_json_object_has_value_of_type(
                test_obj, "iter-reset", JSONString)  &&
            strcmp(x_json_object_get_string(
                test_obj, "iter-reset"), "yes") == 0)
        {
            reset = 1;
        }

        /* Run the test */
        if (run_a_test(it, live, svc, hdr, 1, err, mock_body, repop, reset))
        {
            ++n_pass;
            s_results[it].result = "ok";
        } else {
            ++n_fail;
            s_results[it].result = "FAIL";
        }
    }

    CORE_LOG (eLOG_Note,
        "============================================================");
    CORE_LOGF(eLOG_Note,
        (FMT_SIZE_T " tests:  " FMT_SIZE_T " passed, "
         FMT_SIZE_T " failed, " FMT_SIZE_T " skipped",
         n_tests, n_pass, n_fail, n_skip));
    CORE_LOG (eLOG_Note,
        "============================================================");
    CORE_LOG (eLOG_Note,
        "Test  Source  Result  Description");
    CORE_LOG (eLOG_Note,
        "----  ------  ------  ----------------------------------------------");
    for (it = 0; it < n_tests; ++it) {
        if ( ! s_results[it].result  ||  ! *s_results[it].result  ||
              *s_results[it].result == '-')
        {
          continue;
        }
        char tests_buf[6];
        if (it+1 > 9999)
            strcpy (tests_buf, "####");
        else
            sprintf(tests_buf, FMT_SIZE_T, it+1);
        CORE_LOGF(eLOG_Note, ("%4s  %6s  %6s  %s",
            tests_buf, s_results[it].live ? "live" : "mock",
            s_results[it].result, s_results[it].name));
    }

    if (root_value)
        x_json_value_free(root_value);

    return (n_tests > 0  &&  n_pass == n_tests - n_skip) ? 1 : 0;
}


static int test_service(int live, const char *svc)
{
    /* Set environment to force testing of NAMERD (unless it's already set). */
    if ( ! is_env_set("CONN_LBSMD_DISABLE"))
        CORE_REG_SET("", "CONN_LBSMD_DISABLE", "1", eREG_Transient);

    if ( ! is_env_set("CONN_DISPD_DISABLE"))
        CORE_REG_SET("", "CONN_DISPD_DISABLE", "1", eREG_Transient);

    if ( ! is_env_set("CONN_NAMERD_ENABLE"))
        CORE_REG_SET("", "CONN_NAMERD_ENABLE", "1", eREG_Transient);

    /* Run the test */
    return run_a_test(0, 1 ,svc, s_user_header, 0, 0, NULL, 0, 0) ? 0 : 1;
}




/*  ----------------------------------------------------------------------------
    Main
*/

int main(int argc, const char *argv[])
{
    const char  *retmsg = "";
    const char  *svc = "";
    const char  *test_nums = "";
    int         retval = 1;
    int         live = 0;

    int i;
    for (i=1; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0  &&  i < argc-1) {
            ++i;
            s_json_file = argv[i];
        } else if (strcmp(argv[i], "-l") == 0) {
            live = 1;
        } else if (strcmp(argv[i], "-n") == 0  &&  i < argc-1) {
            ++i;
            test_nums = argv[i];
            retmsg = "Not all tests run.";
        } else if (strcmp(argv[i], "-s") == 0  &&  i < argc-1) {
            ++i;
            svc = argv[i];
            retmsg = "Run for a given service instead of standard tests.";
        } else if (strcmp(argv[i], "-u") == 0  &&  i < argc-1) {
            ++i;
            s_user_header = argv[i];
            retmsg = "User header overridden.";
        } else {
            fprintf(stderr, "USAGE:  %s [OPTIONS...]\n", argv[0]);
            fprintf(stderr,
                "    [-h]       help\n"
                "    [-f ARG]   test file\n"
                "    [-l]       live test data (default is mock data)\n"
                "    [-n ARG]   comma-separated test selections (eg 1,2,5)\n"
                "    [-s ARG]   service\n"
                "    [-u ARG]   user header\n");
            goto out;
        }
    }

    CORE_SetLOGFormatFlags(fLOG_None | fLOG_Level | fLOG_OmitNoteLevel);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (*svc) {
        test_service(live, svc);
    } else {
        if ( ! run_tests(live, test_nums)  &&  ! *retmsg)
            retmsg = "Not all tests passed.";
    }

out:
    CORE_LOG(eLOG_Note, "");
    if (strcmp(retmsg, "") == 0) {
        /* The only successful condition is a run of all tests. */
        CORE_LOG(eLOG_Note, "SUCCESS - All tests passed.");
        retval = 0;
    } else {
        CORE_LOGF(eLOG_Note, ("FAIL - %s", retmsg));
    }
    CORE_SetLOG(0);
    return retval;
}


/* END -----------------------------------------------------------------------
    Stub out the program for Windows DLL configurations.
    */
#endif
