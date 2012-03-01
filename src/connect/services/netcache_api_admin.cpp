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
 * Author:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>

#include "netcache_api_impl.hpp"

#include <connect/services/netcache_admin.hpp>
#include <connect/services/netcache_api_expt.hpp>

BEGIN_NCBI_SCOPE

void CNetCacheAdmin::ShutdownServer()
{
    string cmd(m_Impl->m_API->MakeCmd("SHUTDOWN"));

    m_Impl->m_API->m_Service->RequireStandAloneServerSpec(cmd).
        ExecWithRetry(cmd);
}

void CNetCacheAdmin::ReloadServerConfig()
{
    string cmd(m_Impl->m_API->MakeCmd("RECONF"));

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}

void CNetCacheAdmin::Reinitialize(const string& cache_name)
{
    string cmd(m_Impl->m_API->MakeCmd(cache_name.empty() ? "REINIT" :
        string("IC(" + cache_name + ") REINIT").c_str()));

    m_Impl->m_API->m_Service->RequireStandAloneServerSpec(cmd).
        ExecWithRetry(cmd);
}

void CNetCacheAdmin::PrintConfig(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput(m_Impl->m_API->MakeCmd("GETCONF"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::PrintStat(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput(m_Impl->m_API->MakeCmd("GETSTAT"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::PrintHealth(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput(m_Impl->m_API->MakeCmd("HEALTH"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::GetServerVersion(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput(m_Impl->m_API->MakeCmd("VERSION"),
        output_stream, CNetService::eSingleLineOutput);
}

string CNetCacheAdmin::GetServerVersion()
{
    string cmd(m_Impl->m_API->MakeCmd("VERSION"));

    return m_Impl->m_API->m_Service->RequireStandAloneServerSpec(cmd).
        ExecWithRetry(cmd).response;
}


END_NCBI_SCOPE
