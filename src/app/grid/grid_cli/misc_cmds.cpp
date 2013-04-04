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

#include <misc/netstorage/netstorage_impl.hpp>

USING_NCBI_SCOPE;

#define TOKEN_TYPE__NETCACHE_BLOB_KEY "NetCacheBlobKey"
#define TOKEN_TYPE__NETSCHEDULE_JOB_KEY "NetScheduleJobKey"
#define TOKEN_TYPE__NETFILE_ID "NetFileID"

int CGridCommandLineInterfaceApp::Cmd_WhatIs()
{
    CNetCacheKey nc_key;
    CNetScheduleKey ns_key;

    if (CNetCacheKey::ParseBlobKey(m_Opts.id.c_str(),
            m_Opts.id.length(), &nc_key)) {
        printf(m_Opts.output_format == eJSON ?

                "{\n"
                "\t\"type\": \"" TOKEN_TYPE__NETCACHE_BLOB_KEY "\",\n"
                "\t\"key_version\": %u,\n" :

                "type: " TOKEN_TYPE__NETCACHE_BLOB_KEY "\n"
                "key_version: %u\n",

                nc_key.GetVersion());

        PrintBlobMeta(nc_key);

        if (m_Opts.output_format == eHumanReadable)
            printf("\nTo retrieve blob attributes from the server, use\n"
                    GRID_APP_NAME " blobinfo %s\n", m_Opts.id.c_str());
        else if (m_Opts.output_format == eJSON)
            printf("}\n");
    } else if (ns_key.ParseJobKey(m_Opts.id)) {
        if (m_Opts.output_format == eJSON) {
            CJobInfoToJSON job_info_to_json;

            CJsonNode job_info_node(job_info_to_json.GetRootNode());

            job_info_node.SetString("type", TOKEN_TYPE__NETSCHEDULE_JOB_KEY);
            job_info_node.SetNumber("key_version", ns_key.version);

            job_info_to_json.ProcessJobMeta(ns_key);

            g_PrintJSON(stdout, job_info_node);
        } else {
            printf("type: " TOKEN_TYPE__NETSCHEDULE_JOB_KEY "\n"
                    "key_version: %u\n",
                    ns_key.version);

            CPrintJobInfo print_job_info;

            print_job_info.ProcessJobMeta(ns_key);

            printf("\nTo retrieve job attributes from the server, use\n"
                    GRID_APP_NAME " jobinfo %s\n", m_Opts.id.c_str());
        }
    } else {
        try {
            CNetFileID netfile_id(m_Opts.id);

            TNetStorageFlags storage_flags = netfile_id.GetStorageFlags();

            if (m_Opts.output_format == eJSON) {
                puts("{\n\t\"type\": \"" TOKEN_TYPE__NETFILE_ID
                        "\",\n\t\"version\": 1,");

                puts(storage_flags & fNST_Fast ?
                        "\t\"fast\": true," :
                        "\t\"fast\": false,");
                puts(storage_flags & fNST_Persistent ?
                        "\t\"persistent\": true," :
                        "\t\"persistent\": false,");
                puts(storage_flags & fNST_Movable ?
                        "\t\"movable\": true," :
                        "\t\"movable\": false,");
                puts(storage_flags & fNST_Cacheable ?
                        "\t\"cacheable\": true," :
                        "\t\"cacheable\": false,");
            } else {
                printf("type: " TOKEN_TYPE__NETFILE_ID "\nversion: 1");

                const char* sep = "\nstorage_flags: ";

                if (storage_flags & fNST_Fast) {
                    printf("%sfast", sep);
                    sep = " | ";
                }
                if (storage_flags & fNST_Persistent) {
                    printf("%spersistent", sep);
                    sep = " | ";
                }
                if (storage_flags & fNST_Movable) {
                    printf("%smovable", sep);
                    sep = " | ";
                }
                if (storage_flags & fNST_Cacheable)
                    printf("%scacheable", sep);

                puts("");
            }

            TNetFileIDFields fields = netfile_id.GetFields();

            if (fields & fNFID_KeyAndNamespace)
                printf(m_Opts.output_format == eJSON ?

                        "\t\"domain_name\": \"%s\",\n"
                        "\t\"key\": \"%s\"" :

                        "domain_name: %s\n"
                        "key: %s\n",

                        netfile_id.GetDomainName().c_str(),
                        netfile_id.GetKey().c_str());
            else
                printf(m_Opts.output_format == eJSON ?

                        "\t\"timestamp\": %llu,\n"
                        "\t\"random\": %llu" :

                        "timestamp: %llu\n"
                        "random: %llu\n",

                        (unsigned long long) netfile_id.GetTimestamp(),
                        (unsigned long long) netfile_id.GetRandom());

            if (fields & fNFID_NetICache) {
                CNetICacheClient icache_client(netfile_id.GetNetICacheClient());

                printf(m_Opts.output_format == eJSON ?

                        ",\n\t\"ic_service_name\": \"%s\",\n"
                        "\t\"ic_cache_name\": \"%s\",\n"
                        "\t\"ic_server_host\": \"%s\",\n"
                        "\t\"ic_server_port\": %hu,\n"
                        "\t\"ic_server_xsite\": %s" :

                        "ic_service_name: %s\n"
                        "ic_cache_name: %s\n"
                        "ic_server: %s:%hu\n"
                        "ic_server_xsite: %s\n",

                        icache_client.GetService().GetServiceName().c_str(),
                        icache_client.GetCacheName().c_str(),
                        g_NetService_gethostnamebyaddr(
                                netfile_id.GetNetCacheIP()).c_str(),
                        netfile_id.GetNetCachePort(),
                        fields & fNFID_AllowXSiteConn ? "true" : "false");
            }

            if (storage_flags & fNST_Cacheable)
                printf(m_Opts.output_format == eJSON ?
                        ",\n\t\"cache_chunk_size\": %llu" :
                        "cache_chunk_size: %llu\n",
                        (unsigned long long) netfile_id.GetCacheChunkSize());

            if (fields & fNFID_TTL)
                printf(m_Opts.output_format == eJSON ?
                        ",\n\t\"ttl\": %llu" :
                        "ttl: %llu\n",
                        (unsigned long long) netfile_id.GetTTL());

            if (m_Opts.output_format == eJSON)
                puts("\n}");
        }
        catch (CNetStorageException&) {
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
    g_GetUserAndHost(&user, &host);
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
