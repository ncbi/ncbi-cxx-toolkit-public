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

#include "util.hpp"
#include "grid_cli.hpp"

USING_NCBI_SCOPE;

CGridCommandLineInterfaceApp::EAPIClass
        CGridCommandLineInterfaceApp::SetUp_AdminCmd(
                CGridCommandLineInterfaceApp::EAdminCmdSeverity cmd_severity)
{
    const char* applicable_error;

    switch (IsOptionAccepted(eNetCache, OPTION_N(0)) |
            IsOptionAccepted(eNetSchedule, OPTION_N(1)) |
            IsOptionAccepted(eWorkerNode, OPTION_N(2))) {
    case OPTION_N(0): // eNetCache
        SetUp_NetCacheCmd(eNetCacheAdmin, cmd_severity);
        return eNetCacheAdmin;

    case OPTION_N(1): // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin, cmd_severity);
        return eNetScheduleAdmin;

    case OPTION_N(2): // eWorkerNode
        SetUp_NetScheduleCmd(eWorkerNodeAdmin, cmd_severity);
        return eWorkerNodeAdmin;

    case OPTION_N(0) | OPTION_N(1): // eNetCache or eNetSchedule
        applicable_error = "either '--" NETCACHE_OPTION "' or '--"
                NETSCHEDULE_OPTION "' must be explicitly specified.";
        break;
    case OPTION_N(0) | OPTION_N(2): // eNetCache or eWorkerNode
        applicable_error = "either '--" NETCACHE_OPTION "' or '--"
                WORKER_NODE_OPTION "' must be explicitly specified.";
        break;
    case OPTION_N(1) | OPTION_N(2): // eNetSchedule or eWorkerNode
        applicable_error = "either '--" NETSCHEDULE_OPTION "' or '--"
                WORKER_NODE_OPTION "' must be explicitly specified.";
        break;
    default: // All three are accepted.
        applicable_error = "exactly one of '--" NETCACHE_OPTION "', '--"
                 NETSCHEDULE_OPTION "', or '--" WORKER_NODE_OPTION
                 "' must be explicitly specified.";
    }

    switch (IsOptionExplicitlySet(eNetCache, OPTION_N(0)) |
            IsOptionExplicitlySet(eNetSchedule, OPTION_N(1)) |
            IsOptionExplicitlySet(eWorkerNode, OPTION_N(2))) {
    case OPTION_N(0): // eNetCache
        SetUp_NetCacheCmd(eNetCacheAdmin, cmd_severity);
        return eNetCacheAdmin;

    case OPTION_N(1): // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin, cmd_severity);
        return eNetScheduleAdmin;

    case OPTION_N(2): // eWorkerNode
        SetUp_NetScheduleCmd(eWorkerNodeAdmin, cmd_severity);
        return eWorkerNodeAdmin;

    default: // None or a combination of options
        NCBI_THROW(CArgException, eNoValue, applicable_error);
    }
}

int CGridCommandLineInterfaceApp::Cmd_ServerInfo()
{
    CNetService service;

    EAPIClass api_class = SetUp_AdminCmd(eReadOnlyAdminCmd);

    switch (api_class) {
    case eNetCacheAdmin:
        service = m_NetCacheAPI.GetService();
        break;

    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        service = m_NetScheduleAPI.GetService();
        break;

    default:
        return 2;
    }

    if (api_class == eNetScheduleAdmin && IsOptionExplicitlySet(eQueue)) {
        CNetScheduleAPI::SServerParams params =
            m_NetScheduleAPI.GetServerParams();
        printf("max_input_size: %lu\n"
            "max_output_size: %lu\n"
            "fast_status_support: %s\n",
            (unsigned long) params.max_input_size,
            (unsigned long) params.max_output_size,
            params.fast_status ? "yes" : "no");
    } else {
        bool print_server_address = service.IsLoadBalanced();

        for (CNetServiceIterator it = service.Iterate(); it; ++it) {
            if (print_server_address)
                printf("[%s]\n", (*it).GetServerAddress().c_str());

            CNetServerInfo server_info((*it).GetServerInfo());

            string attr_name, attr_value;

            while (server_info.GetNextAttribute(attr_name, attr_value))
                printf("%s: %s\n", attr_name.c_str(), attr_value.c_str());

            if (print_server_address)
                printf("\n");
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Stats()
{
    switch (SetUp_AdminCmd(eReadOnlyAdminCmd)) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintStat(NcbiCout, m_Opts.aggregation_interval,
                !IsOptionSet(ePreviousInterval) ?
                        CNetCacheAdmin::eReturnCurrentPeriod :
                        CNetCacheAdmin::eReturnCompletePeriod);
        return 0;

    case eNetScheduleAdmin:
        return PrintNetScheduleStats();

    case eWorkerNodeAdmin:
        m_NetScheduleAdmin.PrintServerStatistics(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Health()
{
    if (SetUp_AdminCmd(eReadOnlyAdminCmd) != eNetCacheAdmin)
        return 2;
    m_NetCacheAdmin.PrintHealth(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetConf()
{
    switch (SetUp_AdminCmd(eReadOnlyAdminCmd)) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintConfig(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        m_NetScheduleAdmin.PrintConf(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Reconf()
{
    switch (SetUp_AdminCmd(eSevereAdminCmd)) {
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

int CGridCommandLineInterfaceApp::Cmd_Drain()
{
    switch (SetUp_AdminCmd(eSevereAdminCmd)) {
    case eNetScheduleAdmin:
        m_NetScheduleAdmin.SwitchToDrainMode(m_Opts.on_off_switch);
        return 0;

    default:
        return 2;
    }
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Shutdown()
{
    switch (SetUp_AdminCmd(eSevereAdminCmd)) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.ShutdownServer();
        return 0;

    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        switch (IsOptionSet(eNow, OPTION_N(0)) |
                IsOptionSet(eDie, OPTION_N(1)) |
                IsOptionSet(eDrain, OPTION_N(2))) {
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

        case OPTION_N(2): // eDrain is set.
            m_NetScheduleAdmin.ShutdownServer(CNetScheduleAdmin::eDrain);
            return 0;

        default: // A combination of the above options.
            fprintf(stderr, "Options '--" NOW_OPTION "', '--" DIE_OPTION
                "', and '--" DRAIN_OPTION "' are mutually exclusive.\n");
        }
        /* FALL THROUGH */

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Exec()
{
    CNetService service;

    switch (SetUp_AdminCmd(eSevereAdminCmd)) {
    case eNetCacheAdmin:
        service = m_NetCacheAPI.GetService();
        break;

    case eNetScheduleAdmin:
        service = m_NetScheduleAPI.GetService();
        break;

    default:
        return 2;
    }

    if (m_Opts.output_format == eRaw)
        service.PrintCmdOutput(m_Opts.command, NcbiCout,
                IsOptionSet(eMultiline) ? CNetService::eMultilineOutput :
                CNetService::eSingleLineOutput);
    else // Output format is eJSON.
        PrintJSON(stdout, ExecToJson(service,
                m_Opts.command, IsOptionSet(eMultiline)));

    return 0;
}
