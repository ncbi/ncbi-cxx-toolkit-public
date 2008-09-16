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
#include <corelib/ncbistr.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/services/netcache_api_expt.hpp>



BEGIN_NCBI_SCOPE


void CNetCacheAdmin::ShutdownServer()
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetServerConnection conn = m_API->x_GetConnection();
    m_API->SendCmdWaitResponse(conn, m_API->x_MakeCommand("SHUTDOWN"));
}

void CNetCacheAdmin::Logging(bool on_off) const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    string cmd = "LOG ";
    cmd += on_off ? "ON" : "OFF";
    CNetServerConnection conn = m_API->x_GetConnection();
    m_API->SendCmdWaitResponse(conn, m_API->x_MakeCommand(cmd));
}

void CNetCacheAdmin::PrintConfig(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput(m_API->x_MakeCommand("GETCONF"),
                                 sink, CNetServiceAPI_Base::ePrintServerOut);
}

void CNetCacheAdmin::PrintStat(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput(m_API->x_MakeCommand("GETSTAT"),
                                 sink, CNetServiceAPI_Base::ePrintServerOut);
}

void CNetCacheAdmin::GetServerVersion(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput(m_API->x_MakeCommand("VERSION"),
                                 sink, CNetServiceAPI_Base::eSendCmdWaitResponse);
}

void CNetCacheAdmin::DropStat() const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetServerConnection conn = m_API->x_GetConnection();
    m_API->SendCmdWaitResponse(conn, m_API->x_MakeCommand("DROPSTAT"));
}

void CNetCacheAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetServerConnection conn = m_API->x_GetConnection();
    conn.WriteStr(m_API->x_MakeCommand("MONI") + "\r\n");
    conn.Telnet(&out, NULL);
}


END_NCBI_SCOPE

