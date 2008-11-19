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
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    m_Impl->m_API->x_GetConnection().Exec(
        m_Impl->m_API->x_MakeCommand("SHUTDOWN"));
}

void CNetCacheAdmin::Logging(bool on_off) const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    m_Impl->m_API->x_GetConnection().Exec(
        m_Impl->m_API->x_MakeCommand(on_off ? "LOG ON" : "LOG OFF"));
}

void CNetCacheAdmin::PrintConfig(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput(
        m_Impl->m_API->x_MakeCommand("GETCONF"),
        output_stream, eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::PrintStat(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput(
        m_Impl->m_API->x_MakeCommand("GETSTAT"),
        output_stream, eMultilineOutput_NetCacheStyle);
}

void CNetCacheAdmin::GetServerVersion(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput(
        m_Impl->m_API->x_MakeCommand("VERSION"),
        output_stream, eSingleLineOutput);
}

void CNetCacheAdmin::DropStat() const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    m_Impl->m_API->x_GetConnection().Exec(
        m_Impl->m_API->x_MakeCommand("DROPSTAT"));
}

void CNetCacheAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetServerConnection conn = m_Impl->m_API->x_GetConnection();
    conn->WriteLine(m_Impl->m_API->x_MakeCommand("MONI"));
    conn.Telnet(&out, NULL);
}


END_NCBI_SCOPE
