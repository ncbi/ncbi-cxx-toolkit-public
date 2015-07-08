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
#include <corelib/resource_info.hpp>

#include "nst_server.hpp"
#include "nst_service_thread.hpp"
#include "nst_config.hpp"



USING_NCBI_SCOPE;


CNetStorageServer *  CNetStorageServer::sm_netstorage_server = NULL;



CNetStorageServer::CNetStorageServer()
   : m_Port(0),
     m_HostNetAddr(CSocketAPI::gethostbyname(kEmptyStr)),
     m_Shutdown(false),
     m_SigNum(0),
     m_Log(default_log),
     m_LogTiming(default_log_timing),
     m_LogTimingNSTAPI(default_log_timing_nst_api),
     m_LogTimingClientSocket(default_log_timing_client_socket),
     m_SessionID("s" + x_GenerateGUID()),
     m_NetworkTimeout(0),
     m_StartTime(CNSTPreciseTime::Current()),
     m_AnybodyCanReconfigure(false),
     m_NeedDecryptCacheReset(false),
     m_LastDecryptCacheReset(0.0)
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


CJsonNode
CNetStorageServer::SetParameters(
                        const SNetStorageServerParameters &  params,
                        bool                                 reconfigure)
{
    if (!reconfigure) {
        CServer::SetParameters(params);

        m_Port = params.port;
        m_Log = params.log;
        m_LogTiming = params.log_timing;
        m_LogTimingNSTAPI = params.log_timing_nst_api;
        m_LogTimingClientSocket = params.log_timing_client_socket;
        m_NetworkTimeout = params.network_timeout;
        m_AdminClientNames = x_GetAdminClientNames(params.admin_client_names);
        return CJsonNode::NewNullNode();
    }

    // Here: it is reconfiguration.
    // Need to consider:
    // - log
    // - admin names
    // - max connections
    // - network timeout

    set<string>     new_admins = x_GetAdminClientNames(
                                        params.admin_client_names);
    vector<string>  added;
    vector<string>  deleted;

    {
        CFastMutexGuard     guard(m_AdminClientNamesLock);

        // Compare the old and the new sets of administrators
        set_difference(new_admins.begin(), new_admins.end(),
                       m_AdminClientNames.begin(), m_AdminClientNames.end(),
                       inserter(added, added.begin()));
        set_difference(m_AdminClientNames.begin(), m_AdminClientNames.end(),
                       new_admins.begin(), new_admins.end(),
                       inserter(deleted, deleted.begin()));

        m_AdminClientNames = new_admins;
    }

    SServer_Parameters      current_params;
    CServer::GetParameters(&current_params);

    if (m_Log == params.log &&
        m_LogTiming == params.log_timing &&
        m_LogTimingNSTAPI == params.log_timing_nst_api &&
        m_LogTimingClientSocket == params.log_timing_client_socket &&
        m_NetworkTimeout == params.network_timeout &&
        current_params.max_connections == params.max_connections &&
        added.empty() &&
        deleted.empty())
        return CJsonNode::NewNullNode();

    // Here: there is a difference
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (m_Log != params.log) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(m_Log));
        values.SetByKey("New", CJsonNode::NewBooleanNode(params.log));
        diff.SetByKey("log", values);

        m_Log = params.log;
    }
    if (m_LogTiming != params.log_timing) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(m_LogTiming));
        values.SetByKey("New", CJsonNode::NewBooleanNode(params.log_timing));
        diff.SetByKey("log_timing", values);

        m_LogTiming = params.log_timing;
    }
    if (m_LogTimingNSTAPI != params.log_timing_nst_api) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(m_LogTimingNSTAPI));
        values.SetByKey("New", CJsonNode::NewBooleanNode(
                                            params.log_timing_nst_api));
        diff.SetByKey("log_timing_nst_api", values);

        m_LogTimingNSTAPI = params.log_timing_nst_api;
    }
    if (m_LogTimingClientSocket != params.log_timing_client_socket) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(
                                            m_LogTimingClientSocket));
        values.SetByKey("New", CJsonNode::NewBooleanNode(
                                            params.log_timing_client_socket));
        diff.SetByKey("log_timing_client_socket", values);

        m_LogTimingClientSocket = params.log_timing_client_socket;
    }

    if (m_NetworkTimeout != params.network_timeout) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewIntegerNode(m_NetworkTimeout));
        values.SetByKey("New", CJsonNode::NewIntegerNode(
                                                    params.network_timeout));
        diff.SetByKey("network_timeout", values);

        m_NetworkTimeout = params.network_timeout;
    }

    if (current_params.max_connections != params.max_connections) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewIntegerNode(
                                            current_params.max_connections));
        values.SetByKey("New", CJsonNode::NewIntegerNode(
                                            params.max_connections));
        diff.SetByKey("max_connections", values);

        current_params.max_connections = params.max_connections;
        CServer::SetParameters(current_params);
    }


    if (!added.empty() || !deleted.empty())
        diff.SetByKey("admin_client_name", x_diffInJson(added, deleted));

    return diff;
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


// Resets the decrypt keys cache if necessary
void CNetStorageServer::ResetDecryptCacheIfNeed(void)
{
    const CNSTPreciseTime   k_ResetInterval(100.0);

    if (m_NeedDecryptCacheReset) {
        CNSTPreciseTime     current = CNSTPreciseTime::Current();
        if ((current - m_LastDecryptCacheReset) >= k_ResetInterval) {
            CNcbiEncrypt::Reload();
            m_LastDecryptCacheReset = current;
        }
    }
}


void CNetStorageServer::RegisterNetStorageAPIDecryptError(
                                                const string &  message)
{
    m_NeedDecryptCacheReset = true;
    RegisterAlert(eDecryptInNetStorageAPI, message);
}


void CNetStorageServer::ReportNetStorageAPIDecryptSuccess(void)
{
    m_NeedDecryptCacheReset = false;
}


string  CNetStorageServer::x_GenerateGUID(void) const
{
    // Naive implementation of the unique identifier.
    Int8        pid = CProcess::GetCurrentPid();
    Int8        current_time = time(0);

    return NStr::NumericToString((pid << 32) | current_time);
}


bool CNetStorageServer::IsAdminClientName(const string &  name) const
{
    set<string>::const_iterator     found;
    CFastMutexGuard                 guard(m_AdminClientNamesLock);

    found = m_AdminClientNames.find(name);
    return found != m_AdminClientNames.end();
}


set<string>
CNetStorageServer::x_GetAdminClientNames(const string &  client_names)
{
    vector<string>      admins;
    NStr::Tokenize(client_names, ";, ", admins, NStr::eMergeDelims);
    return set<string>(admins.begin(), admins.end());
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(const string &  id,
                                                         const string &  user)
{
    return m_Alerts.Acknowledge(id, user);
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(EAlertType  alert_type,
                                                         const string &  user)
{
    return m_Alerts.Acknowledge(alert_type, user);
}


void CNetStorageServer::RegisterAlert(EAlertType  alert_type,
                                      const string &  message)
{
    m_Alerts.Register(alert_type, message);
}


CJsonNode CNetStorageServer::SerializeAlerts(void) const
{
    return m_Alerts.Serialize();
}


CNSTDatabase &  CNetStorageServer::GetDb(void)
{
    if (m_Db.get())
        return *m_Db;

    m_Db.reset(new CNSTDatabase(this));
    return *m_Db;
}


bool  CNetStorageServer::InMetadataServices(const string &  service) const
{
    return m_MetadataServiceRegistry.IsKnown(service);
}


bool  CNetStorageServer::GetServiceTTL(const string &            service,
                                       TNSTDBValue<CTimeSpan> &  ttl) const
{
    return m_MetadataServiceRegistry.GetTTL(service, ttl);
}


bool
CNetStorageServer::GetServiceProlongOnRead(const string &  service,
                                           CTimeSpan &  prolong_on_read) const
{
    return m_MetadataServiceRegistry.GetProlongOnRead(service, prolong_on_read);
}


bool
CNetStorageServer::GetServiceProlongOnWrite(const string &  service,
                                            CTimeSpan &  prolong_on_write) const
{
    return m_MetadataServiceRegistry.GetProlongOnWrite(service,
                                                       prolong_on_write);
}


bool
CNetStorageServer::GetServiceProperties(const string &  service,
                              CNSTServiceProperties &  service_props) const
{
    return m_MetadataServiceRegistry.GetServiceProperties(service,
                                                          service_props);
}


CJsonNode
CNetStorageServer::ReadMetadataConfiguration(const IRegistry &  reg)
{
    return m_MetadataServiceRegistry.ReadConfiguration(reg);
}


CJsonNode
CNetStorageServer::SerializeMetadataInfo(void) const
{
    return m_MetadataServiceRegistry.Serialize();
}


void
CNetStorageServer::RunServiceThread(void)
{
    m_ServiceThread.Reset(new CNetStorageServiceThread(*this));
    m_ServiceThread->Run();
}


void
CNetStorageServer::StopServiceThread(void)
{
    if (!m_ServiceThread.Empty()) {
        m_ServiceThread->RequestStop();
        m_ServiceThread->Join();
        m_ServiceThread.Reset(0);
    }
}


CJsonNode
CNetStorageServer::x_diffInJson(const vector<string> &  added,
                                const vector<string> &  deleted) const
{
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (!added.empty()) {
        CJsonNode       add_node = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = added.begin();
                k != added.end(); ++k)
            add_node.AppendString(*k);
        diff.SetByKey("Added", add_node);
    }

    if (!deleted.empty()) {
        CJsonNode       del_node = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = deleted.begin();
                k != deleted.end(); ++k)
            del_node.AppendString(*k);
        diff.SetByKey("Deleted", del_node);
    }

    return diff;
}

