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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Miscellaneous commands of the grid_cli application
 *                   (the ones that are not specific to NetCache, NetSchedule,
 *                   or worker nodes).
 *
 */

#include <ncbi_pch.hpp>

#include "ns_cmd_impl.hpp"
#include "util.hpp"

USING_NCBI_SCOPE;

int CGridCommandLineInterfaceApp::Cmd_WhatIs()
{
    CNetCacheKey nc_key;
    if (CNetCacheKey::ParseBlobKey(m_Opts.id.c_str(),
            m_Opts.id.length(), &nc_key)) {
        printf("type: NetCacheBlobKey\n"
                "key_version: %u\n",
                nc_key.GetVersion());

        PrintBlobMeta(nc_key);

        printf("\nTo retrieve blob attributes from the server, use\n"
            GRID_APP_NAME " blobinfo %s\n", m_Opts.id.c_str());
    } else {
        try {
            CNetScheduleKey ns_key(m_Opts.id);
            printf("type: NetScheduleJobKey\n"
                    "key_version: %u\n",
                    ns_key.version);

            CPrintJobInfo print_job_info;

            print_job_info.ProcessJobMeta(ns_key);

            printf("\nTo retrieve job attributes from the server, use\n"
                GRID_APP_NAME " jobinfo %s\n", m_Opts.id.c_str());
        }
        catch (CNetScheduleException& e) {
            if (e.GetErrCode() != CNetScheduleException::eKeyFormatError)
                throw;
            fprintf(stderr, "Unable to recognize the specified token.\n");
            return 3;
        }
    }
    return 0;
}

static void AppendLoginTokenField(string* login_token,
        const char* field, const string& field_value)
{
    if (!field_value.empty()) {
        unsigned underscore_count = !login_token->empty();
        const char* underscore = strchr(field_value.c_str(), '_');
        while (underscore != NULL) {
            ++underscore_count;
            underscore = strchr(underscore + 1, '_');
        }

        login_token->append(underscore_count, '_');

        *login_token += field;
        *login_token += '_';
        *login_token += field_value;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Login()
{
    if (m_Opts.app_uid.empty()) {
        fputs("Application UID cannot be empty.\n", stderr);
        return 2;
    }
    if (m_Opts.app_uid == DEFAULT_APP_UID) {
        fputs("Application UID '" DEFAULT_APP_UID
                "' is reserved and cannot be used.\n", stderr);
        return 2;
    }

    string login_token;

    AppendLoginTokenField(&login_token,
            LOGIN_TOKEN_APP_UID_FIELD, m_Opts.app_uid);

    if (IsOptionSet(eAuth))
        AppendLoginTokenField(&login_token,
                LOGIN_TOKEN_AUTH_FIELD, m_Opts.auth);

    string user, host;
    GetUserAndHost(&user, &host);
    AppendLoginTokenField(&login_token, LOGIN_TOKEN_USER_FIELD, user);
    AppendLoginTokenField(&login_token, LOGIN_TOKEN_HOST_FIELD, host);

    if (IsOptionSet(eNetCache))
        AppendLoginTokenField(&login_token,
                LOGIN_TOKEN_NETCACHE_FIELD, m_Opts.nc_service);
    if (IsOptionSet(eCache))
        AppendLoginTokenField(&login_token,
                LOGIN_TOKEN_ICACHE_NAME_FIELD, m_Opts.cache_name);
    if (IsOptionSet(eEnableMirroring))
        AppendLoginTokenField(&login_token, LOGIN_TOKEN_ENABLE_MIRRORING, "y");

    if (IsOptionSet(eNetSchedule))
        AppendLoginTokenField(&login_token,
                LOGIN_TOKEN_NETSCHEDULE_FIELD, m_Opts.ns_service);
    if (IsOptionSet(eQueue))
        AppendLoginTokenField(&login_token,
                LOGIN_TOKEN_QUEUE_FIELD, m_Opts.queue);

    AppendLoginTokenField(&login_token, LOGIN_TOKEN_SESSION_PID_FIELD,
            NStr::NumericToString(CProcess::GetCurrentPid()));

    AppendLoginTokenField(&login_token, LOGIN_TOKEN_SESSION_TIMESTAMP_FIELD,
            NStr::NumericToString(GetFastLocalTime().GetTimeT()));

    AppendLoginTokenField(&login_token, LOGIN_TOKEN_SESSION_UID_FIELD,
            GetDiagContext().GetStringUID());

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    if (IsOptionSet(eAllowXSiteConn))
        AppendLoginTokenField(&login_token, LOGIN_TOKEN_ALLOW_XSITE_CONN, "y");
#endif

    puts(login_token.c_str());

    return 0;
}
