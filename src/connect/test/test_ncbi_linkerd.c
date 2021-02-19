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
 *   Standard test for LINKERD service resolution facility.
 *
 */


#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_linkerd.h"
#include "../ncbi_servicep.h"
#include "../parson.h"

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_service.h>
#include <connect/ncbi_tls.h>

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
#else
#define FMT_SIZE_T      "%zu"
#endif

#define LEN_HOST        77
#define LEN_HDR         99
#define MAX_TESTS       100

#define NIL             '\0'


/*  ----------------------------------------------------------------------------
    Local Types
*/

typedef struct
{
    char            host[LEN_HOST+1];
    char            hdr [LEN_HDR +1];
    unsigned short  port;
} SEndpoint;

typedef enum {
    fMatch_None     = 0,
    fMatch_Host     = 0x01,
    fMatch_Port     = 0x02,
    fMatch_Hdr      = 0x04,
    fMatch_All      = -1,
    fMatch_Default  = fMatch_All
} EMatch;

typedef struct
{
    const char*     name;
    const char*     result;
} STestResult;




/*  ----------------------------------------------------------------------------
    Static Variables
*/

static SEndpoint    s_end_exp;
static SEndpoint    s_end_got;
static const char*  s_json_file = "test_ncbi_linkerd_tests.json";
static STestResult  s_results[MAX_TESTS];




/*  ----------------------------------------------------------------------------
    Static Function Declarations
*/

static int check_match(EMatch flags);

static int run_a_test(size_t test_idx, const char *svc, const char *sch,
                      const char *user, const char *pass, const char *path,
                      const char *args, int exp_err, int exp_warn, int repop,
                      int reset);

static int run_tests(const char *test_nums);




/*  ----------------------------------------------------------------------------
    Static Functions
*/

static int check_match(EMatch flags)
{
    if ((flags & fMatch_Host)  &&  strcmp(s_end_exp.host, s_end_got.host) != 0) {
        /* always consider localhost a match */
        if (strcmp("127.0.0.1", s_end_got.host) != 0)
            return 0;
    }

    if ((flags & fMatch_Hdr)  &&  strcmp(s_end_exp.hdr, s_end_got.hdr) != 0)
        return 0;

    if ((flags & fMatch_Port)  &&  s_end_exp.port != s_end_got.port)
        return 0;

    return 1;
}


static int run_a_test(size_t test_idx, const char *svc, const char *sch,
                      const char *user, const char *pass, const char *path,
                      const char *args, int exp_err, int exp_warn, int repop,
                      int reset)
{
    const SSERV_Info    *info = NULL;
    SConnNetInfo        *net_info;
    SERV_ITER           iter;
    int                 success = 0, errors = 0;
    int                 retval = -1;

    /* Set up the net_info */
    if ( ! svc) {
        CORE_LOG(eLOG_Critical, "Unexpected empty service name.");
        return 0;
    }
    net_info = ConnNetInfo_Create(svc);
    if (sch) {
        if      (strcasecmp(sch, "http" ) == 0) net_info->scheme = eURL_Http;
        else if (strcasecmp(sch, "https") == 0) net_info->scheme = eURL_Https;
        else {
            CORE_LOG(eLOG_Critical, "Unexpected non-http(s) scheme.");
            CORE_LOG(eLOG_Note, "Test result:  FAIL.");
            return 0;
        }
    }
    if (user  &&  strlen(user) >= sizeof(net_info->user)) {
        CORE_LOG(eLOG_Critical, "Unexpected too-long user.");
        CORE_LOG(eLOG_Note, "Test result:  FAIL.");
        return 0;
    }
    if (pass  &&  strlen(pass) >= sizeof(net_info->pass)) {
        CORE_LOG(eLOG_Critical, "Unexpected too-long password.");
        CORE_LOG(eLOG_Note, "Test result:  FAIL.");
        return 0;
    }
    if ((path ? strlen(path) : 0) + 1 +
        (args ? strlen(args) : 0) >= sizeof(net_info->path))
    {
        CORE_LOG(eLOG_Critical, "Unexpected too-long path / args.");
        CORE_LOG(eLOG_Note, "Test result:  FAIL.");
        return 0;
    }
    strcpy(net_info->user, user ? user : "");
    strcpy(net_info->pass, pass ? pass : "");
    ConnNetInfo_SetPath(net_info, path ? path : "");
    ConnNetInfo_SetArgs(net_info, args ? args : "");

    /* Set up the server iterator */
    iter = SERV_OpenP(svc, fSERV_All,
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);

    /* Fetch the linkerd info */
    if ( ! iter) {
        CORE_LOG(eLOG_Error, "SERV_OpenP failed.");
        errors = 1;
    } else {
        info = SERV_GetNextInfo(iter);
        if ( ! info) {
            CORE_LOG(eLOG_Error, "SERV_GetNextInfo failed.");
            errors = 1;
        } else {
            SOCK_ntoa(info->host, s_end_got.host, LEN_HOST);
            s_end_got.port = info->port;
            /* s_end_got.hdr is not currently set because it isn't strictly
                needed for comparison (plus it would be very ugly code) */

            char    *info_str;
            info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("    Found server:   %s",
                                  info_str ? info_str : "?"));
            if (info_str)
                free(info_str);
            /* linkerd must return only 1 hit */
            info = SERV_GetNextInfo(iter);
            if (info) {
                CORE_LOG(eLOG_Error, "Linkerd unexpectedly returned a hit.");
                errors = 1;
            } else {
#if 0
                /* Make sure endpoint data can be repopulated and reset */
                /* THIS IS A LOGIC ERROR:  if the iterator has reached its end
                 * repopulating it might end up with all duplicates (to skip)
                 * and result in no new entries!  Which is NOT an error. */
                if (repop) {
                    /* repopulate */
                    CORE_LOG(eLOG_Trace, "Repopulating the service mapper.");
                    if ( ! SERV_GetNextInfo(iter)) {
                        CORE_LOG(eLOG_Error, "Unable to repopulate endpoint data.");
                        errors = 1;
                    }
                }
#else
                if (SERV_GetNextInfo(iter)  ||  SERV_GetNextInfo(iter)) {
                    CORE_LOG(eLOG_Error, "Server entry after EOF.");
                    errors = 1;
                }
#endif
                if (reset) {
                    /* reset */
                    CORE_LOG(eLOG_Trace, "Resetting the service mapper.");
                    SERV_Reset(iter);
                    if ( ! SERV_GetNextInfo(iter)) {
                        CORE_LOG(eLOG_Error, "No services found after reset.");
                        errors = 1;
                    }
                }
            }

            SERV_Close(iter);
        }
    }

    /* Check for match */
    /* Note that the host IP may change, and the header isn't parsed,
        so currently only the port is checked - but that is a
        reserved port, so it should be sufficient. */
    if (check_match(fMatch_Port)) {
        CORE_LOG(eLOG_Note, "    Found server matched expected server.");
        if ( ! errors)
            success = 1;
    } else {
        CORE_LOG(eLOG_Note, "    Found server didn't match expected server.");
    }

    retval = (success != exp_err ? 1 : 0);
    CORE_LOGF(eLOG_Note, ("Test result:  %s.",
        retval ?
        (success ? "PASS" : "PASS (with expected error)") :
        (success ? "FAIL (success when error expected)" : "FAIL")));

    return retval;
}


/* wrapper around x_json_object_dotget_string() to support platform-specific
   overrides */
static const char * p_json_object_dotget_string(x_JSON_Object *obj,
                                                const char *path)
{
#define PLATFORM_BUFLEN 200
    char        platform_buf[PLATFORM_BUFLEN+1];
    const char  *platform = NULL;
    const char  *val;

#if defined(NCBI_OS_MSWIN)
    platform = "win";
#elif defined(NCBI_OS_UNIX)
    platform = "nix";
#endif
    if (platform) {
        if (strlen(path) + strlen(platform) + 1 > PLATFORM_BUFLEN) {
            strncpy(platform_buf, path, PLATFORM_BUFLEN);
            platform_buf[PLATFORM_BUFLEN] = NIL;
            CORE_LOGF(eLOG_Error, ("Overlong path: '%s'...", platform_buf));
        }
        sprintf(platform_buf, "%s_%s", path, platform);
        val = x_json_object_dotget_string(obj, platform_buf);
        if (val)
            return val;
    }
    return x_json_object_dotget_string(obj, path);
#undef PLATFORM_BUFLEN
}


/* mostly error-handling-free for the moment  :-/  */
static int run_tests(const char *test_nums)
{
    x_JSON_Value    *root_value = NULL;
    x_JSON_Object   *root_obj;
    x_JSON_Array    *test_arr;
    const char      *test_num = test_nums;
    size_t          n_tests = 0, n_pass = 0, n_fail = 0, n_skip = 0;
    size_t          n_comm_unset, n_comm_set;
    int             all_enabled = 0;
    const char      *cmn_svc  = NULL, *cmn_sch  = NULL;
    const char      *cmn_user = NULL, *cmn_pass = NULL;
    const char      *cmn_path = NULL, *cmn_args = NULL;
    const char      *cmn_exp_host = NULL, *cmn_exp_hdr  = NULL;
    unsigned short   cmn_exp_port = 0;


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

    cmn_svc  = p_json_object_dotget_string(root_obj, "common.service");
    cmn_sch  = p_json_object_dotget_string(root_obj, "common.scheme");
    cmn_user = p_json_object_dotget_string(root_obj, "common.user");
    cmn_pass = p_json_object_dotget_string(root_obj, "common.pass");
    cmn_path = p_json_object_dotget_string(root_obj, "common.path");
    cmn_args = p_json_object_dotget_string(root_obj, "common.args");

    cmn_exp_host = p_json_object_dotget_string(root_obj, "common.expect.host");
    cmn_exp_hdr  = p_json_object_dotget_string(root_obj,
                        "common.expect.host_hdr");
    cmn_exp_port = (unsigned short)x_json_object_dotget_number(root_obj,
                        "common.expect.port");

    test_arr = x_json_object_get_array(root_obj, "tests");
    n_tests = x_json_array_get_count(test_arr);
    if ( ! (n_tests == x_json_array_get_count(test_arr))) {
        CORE_LOG(eLOG_Critical, "JSON array count mismatch.");
        return 0;
    }
    size_t it;
    for (it = 0; it < n_tests; ++it) {
        x_JSON_Object   *test_obj;
        const char      *svc  = NULL, *sch  = NULL;
        const char      *user = NULL, *pass = NULL;
        const char      *path = NULL, *args = NULL;
        const char      *exp_host = NULL, *exp_hdr  = NULL;
        unsigned short   exp_port = 0;
        int             err = 0, warn = 0;

        test_obj = x_json_array_get_object(test_arr, it);
        s_results[it].name = x_json_object_get_string(test_obj, "title");

        /* Skip tests not in user-supplied test numbers list */
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

        /* Skip disabled tests */
        if ( ! all_enabled) {
            if (x_json_object_has_value_of_type(test_obj, "disabled",
                JSONNumber))
            {
                if ((int)x_json_object_get_number(test_obj, "disabled") == 1) {
                    ++n_skip;
                    CORE_LOGF(eLOG_Note, ("Skipping test " FMT_SIZE_T ": %s",
                        it+1, x_json_object_get_string(test_obj, "title")));
                    s_results[it].result = "skip";
                    continue;
                }
            }
        }

        CORE_LOGF(eLOG_Note, ("Running test " FMT_SIZE_T ": %s", it+1,
            s_results[it].name));

        svc  = x_json_object_get_string(test_obj, "service");
        sch  = x_json_object_get_string(test_obj, "scheme");
        user = x_json_object_get_string(test_obj, "user");
        pass = x_json_object_get_string(test_obj, "pass");
        path = x_json_object_get_string(test_obj, "path");
        args = x_json_object_get_string(test_obj, "args");

        exp_host = p_json_object_dotget_string(test_obj, "expect.host");
        exp_hdr  = p_json_object_dotget_string(test_obj, "expect.hdr");
        exp_port = (unsigned short)x_json_object_dotget_number(test_obj,
                        "expect.port");

        if ( ! svc ) svc  = cmn_svc;
        if ( ! sch ) sch  = cmn_sch;
        if ( ! user) user = cmn_user;
        if ( ! pass) pass = cmn_pass;
        if ( ! path) path = cmn_path;
        if ( ! args) path = cmn_args;

        if ( ! exp_host) exp_host = cmn_exp_host;
        if ( ! exp_hdr ) exp_hdr  = cmn_exp_hdr;
        if ( ! exp_port) exp_port = cmn_exp_port;

        if (x_json_object_has_value_of_type(test_obj, "expect_error",
            JSONString))
        {
            if (strcmp(x_json_object_get_string(test_obj, "expect_error"),
                       "yes") == 0)
            {
                err = 1;
            }
        }

        if (x_json_object_has_value_of_type(test_obj, "expect_warn",
            JSONString))
        {
            if (strcmp(x_json_object_get_string(test_obj, "expect_warn"),
                       "yes") == 0)
            {
                /* LINKERD_TODO - compare exp warn to got warn;
                    add to determination of success */
                warn = 1;
            }
        }

        const char *msg_svc      = svc      ? svc      : "(not set)";
        const char *msg_sch      = sch      ? sch      : "(not set)";
        const char *msg_user     = user     ? user     : "(not set)";
        const char *msg_pass     = pass     ? pass     : "(not set)";
        const char *msg_path     = path     ? path     : "(not set)";
        const char *msg_args     = args     ? args     : "(not set)";
        const char *msg_exp_host = exp_host ? exp_host : "(not set)";
        const char *msg_exp_hdr  = exp_hdr  ? exp_hdr  : "(not set)";
        const char *msg_err      = err      ? "yes"    : "no";
        const char *msg_warn     = warn     ? "yes"    : "no";
        CORE_LOGF(eLOG_Note, ("    service:          %s",  msg_svc     ));
        CORE_LOGF(eLOG_Note, ("    scheme:           %s",  msg_sch     ));
        CORE_LOGF(eLOG_Note, ("    user:             %s",  msg_user    ));
        CORE_LOGF(eLOG_Note, ("    password:         %s",  msg_pass    ));
        CORE_LOGF(eLOG_Note, ("    path:             %s",  msg_path    ));
        CORE_LOGF(eLOG_Note, ("    args:             %s",  msg_args    ));
        CORE_LOGF(eLOG_Note, ("    expected host:    %s",  msg_exp_host));
        CORE_LOGF(eLOG_Note, ("    expected header:  %s",  msg_exp_hdr ));
        CORE_LOGF(eLOG_Note, ("    expected port:    %hu", exp_port    ));
        CORE_LOGF(eLOG_Note, ("    expected error:   %s",  msg_err     ));
        CORE_LOGF(eLOG_Note, ("    expected warning: %s",  msg_warn    ));

        /* Redo common unset/set - they could have been changed by a prior
            test */
        if (x_json_object_dothas_value_of_type(root_obj, "common.env_unset",
            JSONArray))
        {
            x_JSON_Array    *comm_unset_arr;

            comm_unset_arr = x_json_object_dotget_array(root_obj,
                                "common.env_unset");
            n_comm_unset   = x_json_array_get_count(comm_unset_arr);

            size_t it2;
            for (it2 = 0; it2 < n_comm_unset; ++it2) {
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

            size_t it2;
            for (it2 = 0; it2 < n_comm_set; ++it2) {
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
                size_t it2;
                for (it2 = 0; it2 < n_unset; ++it2) {
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
                size_t it2;
                for (it2 = 0; it2 < n_set; ++it2) {
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

        if (strlen(exp_host) > LEN_HOST) {
            CORE_LOG(eLOG_Critical, "Too-long exp_host.");
            return 0;
        }
        if (strlen(exp_hdr) > LEN_HDR) {
            CORE_LOG(eLOG_Critical, "Too-long exp_hdr.");
            return 0;
        }
        strcpy(s_end_exp.host, exp_host);
        strcpy(s_end_exp.hdr , exp_hdr);
        s_end_exp.port = exp_port;

        CORE_LOGF(eLOG_Note, (
            "        Expected server:   %s:%hu %s",
            s_end_exp.host, s_end_exp.port, s_end_exp.hdr));

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
        if (run_a_test(it, svc, sch, user, pass, path, args, err, warn, repop,
                       reset))
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
        "Test  Result  Description");
    CORE_LOG (eLOG_Note,
        "----  ------  ----------------------------------------------");
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
        CORE_LOGF(eLOG_Note, ("%4s  %6s  %s",
            tests_buf, s_results[it].result, s_results[it].name));
    }

    if (root_value)
        x_json_value_free(root_value);

    return (n_tests > 0  &&  n_pass == n_tests - n_skip) ? 1 : 0;
}




/*  ----------------------------------------------------------------------------
    Main
*/

int main(int argc, const char *argv[])
{
    const char  *retmsg = "";
    const char  *test_nums = "";
    int         retval = 1;

    int i;
    for (i=1; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0  &&  i < argc-1) {
            ++i;
            s_json_file = argv[i];
        } else if (strcmp(argv[i], "-n") == 0  &&  i < argc-1) {
            ++i;
            test_nums = argv[i];
            retmsg = "Not all tests run.";
        } else {
            fprintf(stderr, "USAGE:  %s [OPTIONS...]\n", argv[0]);
            fprintf(stderr,
                "    [-h]       help\n"
                "    [-f ARG]   test file\n"
                "    [-n ARG]   comma-separated test selections (eg 1,2,5)\n");
            goto out;
        }
    }

    CORE_SetLOGFormatFlags(fLOG_None | fLOG_Level | fLOG_OmitNoteLevel);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    SOCK_SetupSSL(NcbiSetupTls);

    if ( ! run_tests(test_nums)  &&  ! *retmsg)
        retmsg = "Not all tests passed.";

out:
    CORE_LOG(eLOG_Note, "");
    if (strcmp(retmsg, "") == 0) {
        /* The only successful condition is a run of all tests */
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
