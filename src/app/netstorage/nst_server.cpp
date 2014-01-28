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
 * Authors:  Denis Vakatov
 *
 * File Description: NetStorage threaded server
 *
 */

#include <ncbi_pch.hpp>

#include "nst_server.hpp"



USING_NCBI_SCOPE;


CNetStorageServer *  CNetStorageServer::sm_netstorage_server = NULL;



CNetStorageServer::CNetStorageServer()
   : m_Port(0),
     m_HostNetAddr(CSocketAPI::gethostbyname(kEmptyStr)),
     m_Shutdown(false),
     m_SigNum(0),
     m_Log(true),
     m_SessionID("s" + x_GenerateGUID()),
     m_NetworkTimeout(0),
     m_StartTime(CNSTPreciseTime::Current())
{
    sm_netstorage_server = this;
}


CNetStorageServer::~CNetStorageServer()
{}


void
CNetStorageServer::AddDefaultListener(IServer_ConnectionFactory *  factory)
{
    AddListener(factory, m_Port);
}


void
CNetStorageServer::SetParameters(const SNetStorageServerParameters &  params)
{
    CServer::SetParameters(params);

    m_Port = params.port;
    m_Log = params.log;
    m_NetworkTimeout = params.network_timeout;
    x_SetAdminClientNames(params.admin_client_names);
}


bool CNetStorageServer::ShutdownRequested(void)
{
    return m_Shutdown;
}


void CNetStorageServer::SetShutdownFlag(int signum)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_SigNum = signum;
    }
}


CNetStorageServer *  CNetStorageServer::GetInstance(void)
{
    return sm_netstorage_server;
}


void CNetStorageServer::Exit()
{}


string  CNetStorageServer::x_GenerateGUID(void) const
{
    // Naive implementation of the unique identifier.
    Int8        pid = CProcess::GetCurrentPid();
    Int8        current_time = time(0);

    return NStr::NumericToString((pid << 32) | current_time);
}


bool CNetStorageServer::IsAdminClientName(const string &  name) const
{
    for (vector<string>::const_iterator  k(m_AdminClientNames.begin());
         k != m_AdminClientNames.end(); ++k)
        if (*k == name)
            return true;
    return false;
}


void CNetStorageServer::x_SetAdminClientNames(const string &  client_names)
{
    m_AdminClientNames.clear();
    NStr::Tokenize(client_names, ";, ", m_AdminClientNames,
                   NStr::eMergeDelims);
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(const string &  id)
{
    return m_Alerts.Acknowledge(id);
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(EAlertType  alert_type)
{
    return m_Alerts.Acknowledge(alert_type);
}


void CNetStorageServer::RegisterAlert(EAlertType  alert_type)
{
    m_Alerts.Register(alert_type);
}

