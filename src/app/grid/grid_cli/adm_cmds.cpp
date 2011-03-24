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

USING_NCBI_SCOPE;

CGridCommandLineInterfaceApp::EAPIClass
    CGridCommandLineInterfaceApp::SetUp_AdminCmd()
{
    switch ((IsOptionSet(eNetCache) << 0) |
            (IsOptionSet(eNetSchedule) << 1)) {
    case 0:
        fprintf(stderr, "This command requires either "
            "'--nc' or '--ns' option to be specified.\n");
        return eUnknownAPI;

    case 1: // eNetCache
        SetUp_NetCacheCmd(eNetCacheAdmin);
        return eNetCacheAdmin;

    case 2: // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin);
        return eNetScheduleAdmin;

    default: // A combination of options
        fprintf(stderr, "Options '--nc' and '--ns' are mutually exclusive.\n");
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

int CGridCommandLineInterfaceApp::Cmd_Version()
{
    switch (SetUp_AdminCmd()) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.GetServerVersion(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        m_NetScheduleAdmin.PrintServerVersion(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Stats()
{
    if (!SetUp_AdminCmd() != eNetCacheAdmin)
        return 2;
    m_NetCacheAdmin.PrintStat(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Health()
{
    if (!SetUp_AdminCmd() != eNetCacheAdmin)
        return 2;
    m_NetCacheAdmin.PrintHealth(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetConf()
{
    if (!SetUp_AdminCmd() != eNetCacheAdmin)
        return 2;
    m_NetCacheAdmin.PrintConfig(NcbiCout);
    return 0;
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
        switch ((IsOptionSet(eNow) << 1) | IsOptionSet(eDie)) {
        case 0: // No additional options.
            m_NetScheduleAdmin.ShutdownServer();
            return 0;

        case 1: // eDie is set.
            m_NetScheduleAdmin.ShutdownServer(CNetScheduleAdmin::eDie);
            return 0;

        case 1 << 1: // eNow is set.
            m_NetScheduleAdmin.ShutdownServer(
                CNetScheduleAdmin::eShutdownImmediate);
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
