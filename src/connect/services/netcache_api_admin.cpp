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

static string s_MkCmd(const char* cmd)
{
    string result(cmd);
    SNetCacheAPIImpl::AppendClientIPSessionID(&result);
    return result;
}

void CNetCacheAdmin::ShutdownServer()
{
    m_Impl->m_API->m_Service->RequireStandAloneServerSpec().
        ExecWithRetry(s_MkCmd("SHUTDOWN"));
}

void CNetCacheAdmin::PrintConfig(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput(s_MkCmd("GETCONF"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::PrintStat(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput(s_MkCmd("GETSTAT"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::PrintHealth(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput(s_MkCmd("HEALTH"),
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::GetServerVersion(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput(s_MkCmd("VERSION"),
        output_stream, CNetService::eSingleLineOutput);
}

void CNetCacheAdmin::Monitor(CNcbiOstream& out) const
{
    m_Impl->m_API->m_Service->Monitor(out, s_MkCmd("MONI"));
}


END_NCBI_SCOPE
