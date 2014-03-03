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


CJsonNode
CNetStorageServer::SetParameters(
                        const SNetStorageServerParameters &  params,
                        bool                                 reconfigure)
{
    if (!reconfigure) {
        CServer::SetParameters(params);

        m_Port = params.port;
        m_Log = params.log;
        m_NetworkTimeout = params.network_timeout;
        m_AdminClientNames = x_GetAdminClientNames(params.admin_client_names);
        return CJsonNode::NewNullNode();
    }

    // Here: it is reconfiguration. Need to consider log and admin names only.

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

    if (m_Log == params.log && added.empty() && deleted.empty())
        return CJsonNode::NewNullNode();

    // Here: there is a difference
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (m_Log != params.log) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("old", CJsonNode::NewBooleanNode(m_Log));
        values.SetByKey("old", CJsonNode::NewBooleanNode(params.log));
        diff.SetByKey("log", values);

        m_Log = params.log;
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


bool  CNetStorageServer::NeedMetadata(const string &  service) const
{
    set<string>::const_iterator     found;
    CFastMutexGuard                 guard(m_MetadataServicesLock);

    found = m_MetadataServices.find(service);
    return found != m_MetadataServices.end();
}


CJsonNode
CNetStorageServer::ReadMetadataConfiguration(const IRegistry &  reg)
{
    const string    section = "metadata_conf";
    set<string>     new_services;
    list<string>    entries;

    reg.EnumerateEntries(section, &entries);
    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string      entry = *k;

        if (!NStr::StartsWith(entry, "service_name_", NStr::eCase))
            continue;

        string      value = reg.GetString(section, entry, "");
        if (!value.empty())
            new_services.insert(value);
    }

    vector<string>      added;
    vector<string>      deleted;
    {
        CFastMutexGuard     guard(m_MetadataServicesLock);

        // Compare the new and the old set of services
        set_difference(new_services.begin(), new_services.end(),
                       m_MetadataServices.begin(), m_MetadataServices.end(),
                       inserter(added, added.begin()));
        set_difference(m_MetadataServices.begin(), m_MetadataServices.end(),
                       new_services.begin(), new_services.end(),
                       inserter(deleted, deleted.begin()));

        m_MetadataServices = new_services;
    }

    if (added.empty() && deleted.empty())
        return CJsonNode::NewNullNode();

    CJsonNode       diff = CJsonNode::NewObjectNode();
    diff.SetByKey("services", x_diffInJson(added, deleted));
    return diff;
}


CJsonNode
CNetStorageServer::serializeMetadataInfo(void) const
{
    CJsonNode                   services = CJsonNode::NewArrayNode();
    set<string>::const_iterator k;
    CFastMutexGuard             guard(m_MetadataServicesLock);

    for (k = m_MetadataServices.begin(); k != m_MetadataServices.end(); ++k)
        services.AppendString(*k);
    return services;
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
        diff.SetByKey("added", add_node);
    }

    if (!deleted.empty()) {
        CJsonNode       del_node = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = deleted.begin();
                k != deleted.end(); ++k)
            del_node.AppendString(*k);
        diff.SetByKey("deleted", del_node);
    }

    return diff;
}

