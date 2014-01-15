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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: NetScheduler threaded server
 *
 */

#include <ncbi_pch.hpp>

#include "ns_server.hpp"
#include "ns_ini_params.hpp"
#include "queue_database.hpp"


USING_NCBI_SCOPE;


CNetScheduleServer* CNetScheduleServer::sm_netschedule_server = 0;

//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server implementation
CNetScheduleServer::CNetScheduleServer()
    : m_BackgroundHost(this),
      m_RequestExecutor(this),
      m_Port(0),
      m_HostNetAddr(0),
      m_Shutdown(false),
      m_SigNum(0),
      m_InactivityTimeout(default_network_timeout),
      m_QueueDB(0),
      m_StartTime(GetFastLocalTime()),
      m_LogFlag(default_is_log),
      m_LogBatchEachJobFlag(default_log_batch_each_job),
      m_LogNotificationThreadFlag(default_log_notification_thread),
      m_LogCleaningThreadFlag(default_log_cleaning_thread),
      m_LogExecutionWatcherThreadFlag(default_log_execution_watcher_thread),
      m_LogStatisticsThreadFlag(default_log_statistics_thread),
      m_RefuseSubmits(false),
      m_UseHostname(default_use_hostname),
      m_DeleteBatchSize(default_del_batch_size),
      m_MarkdelBatchSize(default_markdel_batch_size),
      m_ScanBatchSize(default_scan_batch_size),
      m_PurgeTimeout(default_purge_timeout),
      m_StatInterval(default_stat_interval),
      m_MaxAffinities(default_max_affinities),
      m_NodeID("not_initialized"),
      m_SessionID("s" + x_GenerateGUID())
{
    m_CurrentSubmitsCounter.Set(kSubmitCounterInitialValue);
    sm_netschedule_server = this;
}


CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}


void CNetScheduleServer::AddDefaultListener(IServer_ConnectionFactory* factory)
{
    // port must be set before listening
    _ASSERT(m_Port);
    AddListener(factory, m_Port);
}


static void s_AddSeparator(string &  what)
{
    if (what.empty())
        what += "\"server_changes\" {";
    else
        what += ", ";
}

// Returs a string which describes what was changed
string CNetScheduleServer::SetNSParameters(const SNS_Parameters &  params,
                                           bool                    limited)
{
    string      what_changed = "";
    bool        new_val;

    if (m_LogFlag != params.is_log) {
        s_AddSeparator(what_changed);
        what_changed += "\"log\" [" + NStr::BoolToString(m_LogFlag) +
                        ", " + NStr::BoolToString(params.is_log) + "]";
    }
    m_LogFlag = params.is_log;

    new_val = (params.log_batch_each_job && m_LogFlag);
    if (m_LogBatchEachJobFlag != new_val) {
        s_AddSeparator(what_changed);
        what_changed += "\"log_batch_each_job\" [" +
                        NStr::BoolToString(m_LogBatchEachJobFlag) +
                        ", " + NStr::BoolToString(new_val) + "]";
    }
    m_LogBatchEachJobFlag = new_val;

    new_val = (params.log_notification_thread && m_LogFlag);
    if (m_LogNotificationThreadFlag != new_val) {
        s_AddSeparator(what_changed);
        what_changed += "\"log_notification_thread\" [" +
                        NStr::BoolToString(m_LogNotificationThreadFlag) +
                        ", " + NStr::BoolToString(new_val) + "]";
    }
    m_LogNotificationThreadFlag = new_val;

    new_val = (params.log_cleaning_thread && m_LogFlag);
    if (m_LogCleaningThreadFlag != new_val) {
        s_AddSeparator(what_changed);
        what_changed += "\"log_cleaning_thread\" [" +
                        NStr::BoolToString(m_LogCleaningThreadFlag) +
                        ", " + NStr::BoolToString(new_val) + "]";
    }
    m_LogCleaningThreadFlag = new_val;

    new_val = (params.log_execution_watcher_thread&& m_LogFlag);
    if (m_LogExecutionWatcherThreadFlag != new_val) {
        s_AddSeparator(what_changed);
        what_changed += "\"log_execution_watcher_thread\" [" +
                        NStr::BoolToString(m_LogExecutionWatcherThreadFlag) +
                        ", " + NStr::BoolToString(new_val) + "]";
    }
    m_LogExecutionWatcherThreadFlag = new_val;

    new_val = (params.log_statistics_thread && m_LogFlag);
    if (m_LogStatisticsThreadFlag != new_val) {
        s_AddSeparator(what_changed);
        what_changed += "\"log_statistics_thread\" [" +
                        NStr::BoolToString(m_LogStatisticsThreadFlag) +
                        ", " + NStr::BoolToString(new_val) + "]";
    }
    m_LogStatisticsThreadFlag = new_val;

    if (m_StatInterval != params.stat_interval) {
        s_AddSeparator(what_changed);
        what_changed += "\"stat_interval\" [" +
                        NStr::NumericToString(m_StatInterval) +
                        ", " + NStr::NumericToString(params.stat_interval) + "]";
    }
    m_StatInterval = params.stat_interval;


    string  accepted_hosts = m_AdminHosts.SetHosts(params.admin_hosts);
    if (!accepted_hosts.empty()) {
        s_AddSeparator(what_changed);
        what_changed += "\"admin_host\" [" +
                        accepted_hosts + "]";
    }

    string  accepted_admins = x_SetAdminClientNames(params.admin_client_names);
    if (!accepted_admins.empty()) {
        s_AddSeparator(what_changed);
        what_changed += "\"admin_client_name\" [" +
                        accepted_admins + "]";
    }

    if (!what_changed.empty())
        what_changed += "}";

    if (limited)
        return what_changed;


    CServer::SetParameters(params);
    m_Port = params.port;
    m_HostNetAddr = CSocketAPI::gethostbyname(kEmptyStr);
    m_UseHostname = params.use_hostname;
    if (m_UseHostname)
        m_Host = CSocketAPI::gethostname();
    else
        m_Host = CSocketAPI::ntoa(m_HostNetAddr);

    m_InactivityTimeout = params.network_timeout;

    // Purge related parameters
    m_DeleteBatchSize = params.del_batch_size;
    m_MarkdelBatchSize = params.markdel_batch_size;
    m_ScanBatchSize = params.scan_batch_size;
    m_PurgeTimeout = params.purge_timeout;
    m_MaxAffinities = params.max_affinities;

    m_AffinityHighMarkPercentage = params.affinity_high_mark_percentage;
    m_AffinityLowMarkPercentage = params.affinity_low_mark_percentage;
    m_AffinityHighRemoval = params.affinity_high_removal;
    m_AffinityLowRemoval = params.affinity_low_removal;
    m_AffinityDirtPercentage = params.affinity_dirt_percentage;
    return "";
}


// Return: difference
// Note: it is always called after the configuration is validated so
//       no messages should be logged
string CNetScheduleServer::ReadServicesConfig(const CNcbiRegistry &  reg)
{
    string                  diff;
    const string            section = "service_to_queue";
    map< string, string >   new_services;

    // Read the new list -- new alerts if so
    list<string>            entries;
    reg.EnumerateEntries(section, &entries);


    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string      service_name = *k;
        string      qname = reg.Get("service_to_queue", service_name);
        if (qname.empty())
            continue;

        // Check that the queue name has been provided
        if (!m_QueueDB->QueueExists(qname))
            continue;

        // Config line is fine, memorize it
        NStr::ToUpper(service_name);
        new_services[service_name] = qname;
    }

    // Compare with the old list -- combine report string
    string      new_items;
    string      deleted_items;
    string      changed_items;

    for (map< string, string>::const_iterator  k = new_services.begin();
         k != new_services.end(); ++k) {
        map< string, string>::const_iterator    found =
                                                    m_Services.find(k->first);
        if (found == m_Services.end()) {
            if (!new_items.empty())
                new_items += ", ";
            new_items += "\"" + k->first + "\"";
            continue;
        }
        if (found->second != k->second) {
            if (!changed_items.empty())
                changed_items += ", ";
            changed_items += "\"" + k->first + "\" [\"" +
                             found->second + "\", \"" + k->second + "\"]";
        }
    }

    for (map< string, string>::const_iterator  k = m_Services.begin();
         k != m_Services.end(); ++k) {
        map< string, string>::const_iterator    found =
                                                    new_services.find(k->first);
        if (found == new_services.end()) {
            if (!deleted_items.empty())
                deleted_items += ", ";
            deleted_items += "\"" + k->first + "\"";
        }
    }


    if (!new_items.empty())
        diff = "\"services_added\" [" + new_items + "]";

    if (!deleted_items.empty()) {
        if (!diff.empty())
            diff += ", ";
        diff += "\"services_deleted\" [" + deleted_items + "]";
    }

    if (!changed_items.empty()) {
        if (!diff.empty())
            diff += ", ";
        diff += "\"services_changed\" {" + changed_items + "}";
    }

    // Set the current as the new one
    CFastMutexGuard     guard(m_ServicesLock);
    m_Services = new_services;
    return diff;
}


bool CNetScheduleServer::ShutdownRequested(void)
{
    return m_Shutdown;
}


void CNetScheduleServer::SetQueueDB(CQueueDataBase* qdb)
{
    delete m_QueueDB;
    m_QueueDB = qdb;
}


void CNetScheduleServer::SetShutdownFlag(int signum)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_SigNum = signum;
    }
}


// Queue handling
unsigned int  CNetScheduleServer::Configure(const IRegistry &  reg,
                                            string &           diff)
{
    return m_QueueDB->Configure(reg, diff);
}


unsigned CNetScheduleServer::CountActiveJobs() const
{
    return m_QueueDB->CountActiveJobs();
}


CRef<CQueue> CNetScheduleServer::OpenQueue(const std::string& name)
{
    return m_QueueDB->OpenQueue(name);
}


void CNetScheduleServer::CreateDynamicQueue(const CNSClientId &  client,
                                            const string &  qname,
                                            const string &  qclass,
                                            const string &  description)
{
    m_QueueDB->CreateDynamicQueue(client, qname, qclass, description);
}


void CNetScheduleServer::DeleteDynamicQueue(const CNSClientId &  client,
                                            const std::string& qname)
{
    m_QueueDB->DeleteDynamicQueue(client, qname);
}


SQueueParameters
CNetScheduleServer::QueueInfo(const string &  qname) const
{
    return m_QueueDB->QueueInfo(qname);
}


std::string CNetScheduleServer::GetQueueNames(const string &  sep) const
{
    return m_QueueDB->GetQueueNames(sep);
}


void CNetScheduleServer::PrintMutexStat(CNcbiOstream& out)
{
    m_QueueDB->PrintMutexStat(out);
}


void CNetScheduleServer::PrintLockStat(CNcbiOstream& out)
{
    m_QueueDB->PrintLockStat(out);
}


void CNetScheduleServer::PrintMemStat(CNcbiOstream& out)
{
    m_QueueDB->PrintMemStat(out);
}


string CNetScheduleServer::PrintTransitionCounters(void)
{
    return m_QueueDB->PrintTransitionCounters();
}


string CNetScheduleServer::PrintJobsStat(void)
{
    return m_QueueDB->PrintJobsStat();
}


string CNetScheduleServer::GetQueueClassesInfo(void) const
{
    return m_QueueDB->GetQueueClassesInfo();
}


string CNetScheduleServer::GetQueueClassesConfig(void) const
{
    return m_QueueDB->GetQueueClassesConfig();
}


string CNetScheduleServer::GetQueueInfo(void) const
{
    return m_QueueDB->GetQueueInfo();
}


string CNetScheduleServer::GetQueueConfig(void) const
{
    return m_QueueDB->GetQueueConfig();
}


string CNetScheduleServer::GetLinkedSectionConfig(void) const
{
    return m_QueueDB->GetLinkedSectionConfig();
}


string CNetScheduleServer::GetServiceToQueueSectionConfig(void) const
{
    string                                  output;
    map< string, string >::const_iterator   k;
    CFastMutexGuard                         guard(m_ServicesLock);

    if (m_Services.empty())
        return output;

    output = "\n[service_to_queue]";
    for (k = m_Services.begin(); k != m_Services.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += k->first + "=\"" + k->second + "\"";
    }

    return output;
}


string CNetScheduleServer::ResolveService(const string &  service) const
{
    map< string, string >::const_iterator   k;
    CFastMutexGuard                         guard(m_ServicesLock);

    for (k = m_Services.begin(); k != m_Services.end(); ++k) {
        if (NStr::CompareNocase(k->first, service) == 0)
            return k->second;
    }
    return "";
}


bool CNetScheduleServer::AdminHostValid(unsigned host) const
{
    return m_AdminHosts.IsAllowed(host);
}


bool CNetScheduleServer::IsAdminClientName(const string &  name) const
{
    CReadLockGuard      guard(m_AdminClientsLock);

    for (vector<string>::const_iterator  k(m_AdminClientNames.begin());
         k != m_AdminClientNames.end(); ++k)
        if (*k == name)
            return true;
    return false;
}

string CNetScheduleServer::GetAdminClientNames(void) const
{
    string              ret;
    CReadLockGuard      guard(m_AdminClientsLock);

    for (vector<string>::const_iterator  k(m_AdminClientNames.begin());
         k != m_AdminClientNames.end(); ++k) {
        if (!ret.empty())
            ret += ", ";
        ret += *k;
    }
    return ret;
}


string CNetScheduleServer::GetAlerts(void) const
{
    return m_Alerts.GetURLEncoded();
}


enum EAlertAckResult CNetScheduleServer::AcknowledgeAlert(const string &  id)
{
    return m_Alerts.Acknowledge(id);
}


enum EAlertAckResult CNetScheduleServer::AcknowledgeAlert(EAlertType  alert_type)
{
    return m_Alerts.Acknowledge(alert_type);
}


void CNetScheduleServer::RegisterAlert(EAlertType  alert_type)
{
    m_Alerts.Register(alert_type);
}


// The method is called after the database is created or loaded.
// This guarantees that the directory is there.
// The file with an identifier could be read or created safely.
// Returns: true if everything is fine.
void CNetScheduleServer::InitNodeID(const string &  db_path)
{
    CFile   node_id_file(CFile::MakePath(
                            CDirEntry::AddTrailingPathSeparator(db_path),
                            "NODE_ID"));

    if (node_id_file.Exists()) {
        // File exists, read the ID from it
        CFileIO     f;
        char        buffer[64];

        f.Open(node_id_file.GetPath(), CFileIO_Base::eOpen,
                                       CFileIO_Base::eRead);
        size_t      n = f.Read(buffer, sizeof(buffer));

        m_NodeID = string(buffer, n);
        NStr::TruncateSpacesInPlace(m_NodeID, NStr::eTrunc_End);
        f.Close();
    } else {
        // No file, need to be created
        m_NodeID = "n" + x_GenerateGUID();

        CFileIO     f;
        f.Open(node_id_file.GetPath(), CFileIO_Base::eCreate,
                                       CFileIO_Base::eReadWrite);
        f.Write(m_NodeID.data(), m_NodeID.size());
        f.Close();
    }
}


CNetScheduleServer*  CNetScheduleServer::GetInstance(void)
{
    return sm_netschedule_server;
}


void CNetScheduleServer::Exit()
{
    m_QueueDB->Close(IsDrainShutdown());
}


string  CNetScheduleServer::x_GenerateGUID(void) const
{
    // Naive implementation of the unique identifier.
    Int8        pid = CProcess::GetCurrentPid();
    Int8        current_time = time(0);

    return NStr::NumericToString((pid << 32) | current_time);
}


string CNetScheduleServer::x_SetAdminClientNames(const string &  client_names)
{
    CWriteLockGuard     guard(m_AdminClientsLock);
    vector<string>      old_admins = m_AdminClientNames;

    m_AdminClientNames.clear();
    NStr::Tokenize(client_names, ";, \n\r", m_AdminClientNames,
                   NStr::eMergeDelims);
    sort(m_AdminClientNames.begin(), m_AdminClientNames.end());

    if (old_admins != m_AdminClientNames) {
        string      accepted_names;
        for (vector<string>::const_iterator  k = m_AdminClientNames.begin();
             k != m_AdminClientNames.end(); ++k) {
            if (!accepted_names.empty())
                accepted_names += ", ";
            accepted_names += *k;
        }

        if (accepted_names.empty())
            accepted_names = "none";
        return accepted_names;
    }

    return "";
}

