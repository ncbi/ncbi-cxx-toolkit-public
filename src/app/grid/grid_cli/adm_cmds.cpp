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

bool CGridCommandLineInterfaceApp::SetUp_AdminCmd()
{
    if (!IsOptionSet(eNetCache)) {
        fprintf(stderr, "This command requires "
            "'--nc' option to be specified.\n");
        return false;
    }

    SetUp_NetCacheCmd(eNetCacheAdmin);
    return true;
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
        fprintf(stderr, "Unable to recognize the specified token.\n");
        return 3;
    }
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Version()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.GetServerVersion(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Stats()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.PrintStat(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Health()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.PrintHealth(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetConf()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.PrintConfig(NcbiCout);
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Reconf()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.ReloadServerConfig();
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Shutdown()
{
    if (!SetUp_AdminCmd())
        return 2;
    m_NetCacheAdmin.ShutdownServer();
    return 0;
}
