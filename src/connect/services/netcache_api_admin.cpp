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


void CNetCacheAdmin::ShutdownServer() const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetSrvConnectorHolder conn = m_API->x_GetConnector();
    m_API->SendCmdWaitResponse(conn, "SHUTDOWN");  
}

void CNetCacheAdmin::Logging(bool on_off) const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    string cmd = "LOG ";
    cmd += on_off ? "ON" : "OFF";
    CNetSrvConnectorHolder conn = m_API->x_GetConnector();
    m_API->SendCmdWaitResponse(conn, cmd);  
}

void CNetCacheAdmin::PrintConfig(INetServiceAPI::ISink& sink) const
{
    m_API->x_CollectStreamOutput("GETCONF", sink, INetServiceAPI::ePrintServerOut);
}

void CNetCacheAdmin::PrintStat(INetServiceAPI::ISink& sink) const
{
    m_API->x_CollectStreamOutput("GETSTAT", sink, INetServiceAPI::ePrintServerOut);
}

void CNetCacheAdmin::GetServerVersion(INetServiceAPI::ISink& sink) const
{
    m_API->x_CollectStreamOutput("VERSION", sink, INetServiceAPI::eSendCmdWaitResponse);
}

void CNetCacheAdmin::DropStat() const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetSrvConnectorHolder conn = m_API->x_GetConnector();
    m_API->SendCmdWaitResponse(conn, "DROPSTAT");  
}

void CNetCacheAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetCacheException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    CNetSrvConnectorHolder conn = m_API->x_GetConnector();
    conn->WriteStr("MONI\r\n");
    conn->Telnet(out);
    conn->Disconnect();
}


END_NCBI_SCOPE

