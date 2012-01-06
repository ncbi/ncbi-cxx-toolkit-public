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

#include "grid_cli.hpp"

#include <string.h>
#include <ctype.h>

USING_NCBI_SCOPE;

CGridCommandLineInterfaceApp::EAPIClass
    CGridCommandLineInterfaceApp::SetUp_AdminCmd()
{
    switch (IsOptionSet(eNetCache, OPTION_N(0)) |
            IsOptionSet(eNetSchedule, OPTION_N(1)) |
            IsOptionSet(eWorkerNode, OPTION_N(2))) {
    case 0:
        fprintf(stderr, "This command requires either "
            "'--nc' or '--ns' option to be specified.\n");
        return eUnknownAPI;

    case OPTION_N(0): // eNetCache
        SetUp_NetCacheCmd(eNetCacheAdmin);
        return eNetCacheAdmin;

    case OPTION_N(1): // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin);
        return eNetScheduleAdmin;

    case OPTION_N(2): // eWorkerNode
        SetUp_NetScheduleCmd(eWorkerNodeAdmin);
        return eWorkerNodeAdmin;

    default: // A combination of options
        fprintf(stderr, "Options '--nc', '--ns', and '--wn' "
            "are mutually exclusive.\n");
        return eUnknownAPI;
    }
}

int CGridCommandLineInterfaceApp::Cmd_WhatIs()
{
    CNetCacheKey nc_key;
    if (CNetCacheKey::ParseBlobKey(m_Opts.id.c_str(),
            m_Opts.id.length(), &nc_key)) {
        printf("Type: NetCache blob ID, version %u\n", nc_key.GetVersion());

        PrintBlobMeta(nc_key);

        printf("\nTo retrieve blob attributes from the server, use\n"
            PROGRAM_NAME " blobinfo %s\n", m_Opts.id.c_str());
    } else {
        try {
            CNetScheduleKey ns_key(m_Opts.id);
            printf("Type: NetSchedule job ID, version %u\n", ns_key.version);

            PrintJobMeta(ns_key);

            printf("\nTo retrieve job attributes from the server, use\n"
                PROGRAM_NAME " jobinfo %s\n", m_Opts.id.c_str());
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

static void PrintVersionParts(const char* version)
{
    const char* prev_part_end = version;

    for (;;) {
        switch (*version) {
        case 'v':
        case 'V':
            if (memcmp(version + 1, "ersion", 6) == 0) {
                const char* version_number = version + 7;
                while (isspace(*version_number) || *version_number == ':')
                    ++version_number;
                const char* version_number_end = version_number;
                while (isdigit(*version_number_end) ||
                        *version_number_end == '.')
                    ++version_number_end;
                printf("%.*sversion: %.*s\n",
                    int(version - prev_part_end), prev_part_end,
                    int(version_number_end - version_number), version_number);
                prev_part_end = version_number_end;
                while (isspace(*prev_part_end))
                    ++prev_part_end;
            }
            break;
        case 'b':
        case 'B':
            if (memcmp(version + 1, "uil", 3) == 0 &&
                    (version[4] == 'd' || version[4] == 't')) {
                const char* build = version + 5;
                while (isspace(*build) || *build == ':')
                    ++build;
                printf("Buil%c: %s\n", version[4], build);
                if (version != prev_part_end) {
                    while (isspace(version[-1]))
                        --version;
                    printf("Details: %.*s\n",
                        int(version - prev_part_end), prev_part_end);
                }
                return;
            }
            break;
        case '\0':
            return;
        }
        ++version;
    }
}

int CGridCommandLineInterfaceApp::Cmd_ServerInfo()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        PrintVersionParts(m_NetCacheAdmin.GetServerVersion().c_str());
        return 0;

    case eNetScheduleAdmin:
        PrintVersionParts(m_NetScheduleAdmin.GetServerVersion().c_str());
        if (IsOptionSet(eQueue)) {
            CNetScheduleAPI::SServerParams params =
                m_NetScheduleAPI.GetServerParams();
            printf("Maximum input size: %lu\n"
                "Maximum output size: %lu\n"
                "Fast status support: %s\n",
                (unsigned long) params.max_input_size,
                (unsigned long) params.max_output_size,
                params.fast_status ? "yes" : "no");
        }
        return 0;

    case eWorkerNodeAdmin:
        PrintVersionParts(m_NetScheduleAdmin.GetServerVersion().c_str());
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Stats()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintStat(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        if (IsOptionSet(eClientInfo))
            m_NetScheduleAdmin.PrintServerStatistics(NcbiCout,
                CNetScheduleAdmin::eStatisticsClients);
        else if (IsOptionSet(eActiveJobCount))
            printf("%u\n", m_NetScheduleAdmin.CountActiveJobs());
        else if (IsOptionSet(eJobsByAffinity)) {
            CNetScheduleAdmin::TAffinityMap affinity_map;

            m_NetScheduleAdmin.AffinitySnapshot(affinity_map);

            ITERATE(CNetScheduleAdmin::TAffinityMap, it, affinity_map) {
                printf("%s: %u", it->first.c_str(), it->second.first);
                if (it->second.second.empty())
                    printf("\n");
                else {
                    const char* sep = " [";
                    ITERATE(CNetScheduleAdmin::TWorkerNodeList, wn,
                            it->second.second) {
                        printf("%s%s", sep, wn->c_str());
                        sep = ", ";
                    }
                    printf("]\n");
                }
            }
        } else if (IsOptionSet(eJobsByStatus)) {
            CNetScheduleAdmin::TStatusMap st_map;
            m_NetScheduleAdmin.StatusSnapshot(st_map, m_Opts.affinity);
            ITERATE(CNetScheduleAdmin::TStatusMap, it, st_map) {
                if (it->second > 0)
                    printf("%s: %u\n",
                        CNetScheduleAPI::StatusToString(it->first).c_str(),
                        it->second);
            }
        } else
            m_NetScheduleAdmin.PrintServerStatistics(NcbiCout,
                IsOptionSet(eBrief) ? CNetScheduleAdmin::eStatisticsBrief :
                    CNetScheduleAdmin::eStatisticsAll);
        return 0;

    case eWorkerNodeAdmin:
        m_NetScheduleAdmin.PrintServerStatistics(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Health()
{
    if (SetUp_AdminCmd() != eNetCacheAdmin)
        return 2;
    m_NetCacheAdmin.PrintHealth(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetConf()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintConfig(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        if (IsOptionSet(eQueue))
            m_NetScheduleAdmin.PrintQueueConf(NcbiCout);
        else
            m_NetScheduleAdmin.PrintConf(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Reconf()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.ReloadServerConfig();
        return 0;

    case eNetScheduleAdmin:
        m_NetScheduleAdmin.ReloadServerConfig();
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Shutdown()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.ShutdownServer();
        return 0;

    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        switch (IsOptionSet(eNow, OPTION_N(0)) |
                IsOptionSet(eDie, OPTION_N(1))) {
        case 0: // No additional options.
            m_NetScheduleAdmin.ShutdownServer();
            return 0;

        case OPTION_N(0): // eNow is set.
            m_NetScheduleAdmin.ShutdownServer(
                CNetScheduleAdmin::eShutdownImmediate);
            return 0;

        case OPTION_N(1): // eDie is set.
            m_NetScheduleAdmin.ShutdownServer(CNetScheduleAdmin::eDie);
            return 0;

        default: // Both eNow and eDie are set
            fprintf(stderr, "Options '--" NOW_OPTION
                "' and '--" DIE_OPTION "' are mutually exclusive.\n");
        }
        /* FALL THROUGH */

    default:
        return 2;
    }
}
