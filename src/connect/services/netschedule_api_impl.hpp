#ifndef CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP
#define CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule client API implementation details.
 *
 */

#include "netservice_api_impl.hpp"
#include "netschedule_api_getjob.hpp"

#include <corelib/request_ctx.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/services/error_codes.hpp>

#include <list>
#include <map>
#include <vector>
#include <algorithm>


BEGIN_NCBI_SCOPE

namespace grid {
namespace netschedule {
namespace limits {

struct SClientNode
{
    static string Name() { return "client node ID"; }
    static bool IsValidValue(const string&) { return false; }
    static bool IsValidChar(char c)
    {
        return isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c == '@' || c == '|';
    }
};

struct SClientSession
{
    static string Name() { return "client session ID"; }
    static bool IsValidValue(const string&) { return false; }
    static bool IsValidChar(char c)
    {
        return isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c == '@' || c == '|';
    }
};

struct SQueueName
{
    static string Name() { return "queue name"; }
    static bool IsValidValue(const string& s)
    {
        if (s.empty()) {
            NCBI_THROW(CConfigException, eParameterMissing, "Queue name cannot be empty.");
        }
        if (s.front() == '_') {
            NCBI_THROW(CConfigException, eInvalidParameter, "Queue name cannot start with underscore character.");
        }

        return false;
    }
    static bool IsValidChar(char c) { return isalnum(c) || c == '_' || c == '-'; }
};

struct SJobGroup
{
    static string Name() { return "job group name"; }
    static bool IsValidValue(const string& s) { return s == "-"; }
    static bool IsValidChar(char c) { return isalnum(c) || c == '_' || c == '.'; }
};

struct SAffinity
{
    static string Name() { return "affinity token"; }
    static bool IsValidValue(const string& s) { return s == "-"; }
    static bool IsValidChar(char c) { return isalnum(c) || c == '_' || c == '.'; }
};

struct SAuthToken 
{
    static string Name() { return "security token"; }
    static bool IsValidValue(const string&) { return false; }
    static bool IsValidChar(char c) { return isalnum(c) || c == '_' || c == '.'; }
};

void ThrowIllegalChar(const string&, const string&, char);

template <class TValue>
void Check(const string& value)
{
    if (TValue::IsValidValue(value)) return;

    auto it = find_if_not(value.begin(), value.end(), TValue::IsValidChar);

    if (it != value.end()) {
        ThrowIllegalChar(TValue::Name(), value, *it);
    }
}

}
}
}

using namespace grid::netschedule;


////////////////////////////////////////////////////////////////////////////////
//

struct SNetScheduleServerProperties : public INetServerProperties
{
    SNetScheduleServerProperties() :
        affs_synced(false)
    {
    }

    string ns_node;
    string ns_session;

    // Warning:
    // Version is not set until we execute a command on a server.
    // Therefore, if that command is version dependent,
    // old version of the command will be sent to the server at first.
    CVersionInfo version;

    bool affs_synced;
};

class CNetScheduleConfigLoader
{
public:
    CNetScheduleConfigLoader(CSynRegistry& registry, SRegSynonyms& sections, bool ns_conf = true);

    bool operator()(SNetScheduleAPIImpl* impl);

private:
    bool Transform(const string& prefix, string& name) const;

    CSynRegistry& m_Registry;
    const SRegSynonyms& m_Sections;
    const bool m_NsConf;
    enum { eOff, eImplicit, eExplicit } m_Mode;
};

struct SNetScheduleSharedData;

class CNetScheduleServerListener : public INetServerConnectionListener
{
public:
    CNetScheduleServerListener(bool non_wn, SNetScheduleSharedData* shared_data) :
        m_NonWn(non_wn),
        m_SharedData(shared_data)
    {
        _ASSERT(shared_data);
    }

    void SetAuthString(const string& auth) { m_Auth = auth; }
    string& Scope() { return m_Scope; }

    TPropCreator GetPropCreator() const override;
    INetServerConnectionListener* Clone() override;

    void OnConnected(CNetServerConnection& connection) override;

private:
    void OnErrorImpl(const string& err_msg, CNetServer& server) override;
    void OnWarningImpl(const string& warn_msg, CNetServer& server) override;

    string m_Auth;
    const bool m_NonWn;
    string m_Scope;
    CRef<SNetScheduleSharedData> m_SharedData;
};

struct SNetScheduleSharedData : CObject
{
    CFastMutex m_ServerByNodeMutex;
    typedef map<string, SNetServerInPool*> TServerByNode;
    TServerByNode m_ServerByNode;

    // Make sure the worker node does not attempt to submit its
    // preferred affinities from two threads.
    CFastMutex m_AffinitySubmissionMutex;
};

// Structure that governs NetSchedule server notifications.
struct SServerNotifications
{
    SServerNotifications() :
        m_NotificationSemaphore(0, 1),
        m_Interrupted(false)
    {
    }

    bool Wait(const CDeadline& deadline)
    {
        return m_NotificationSemaphore.TryWait(deadline.GetRemainingTime());
    }

    void InterruptWait();

    void RegisterServer(const string& ns_node);

    bool GetNextNotification(string* ns_node);

private:
    void x_ClearInterruptFlag()
    {
        if (m_Interrupted) {
            m_Interrupted = false;
            m_NotificationSemaphore.TryWait();
        }
    }
    // Semaphore that the worker node or the job reader can wait on.
    // If count=0 then m_ReadyServers is empty and m_Interrupted=false;
    // if count=1 then at least one server is ready or m_Interrupted=true.
    CSemaphore m_NotificationSemaphore;
    // Protection against concurrent access to m_ReadyServers.
    CFastMutex m_Mutex;
    // A set of NetSchedule node IDs of the servers that are ready.
    // (i.e. the servers have sent notifications).
    typedef set<string> TReadyServers;
    TReadyServers m_ReadyServers;
    // This flag is set when the wait must be interrupted.
    bool m_Interrupted;
};

struct SNetScheduleNotificationThread : public CThread
{
    SNetScheduleNotificationThread(SNetScheduleAPIImpl* ns_api);

    enum ENotificationType {
        eNT_GetNotification,
        eNT_ReadNotification,
        eNT_Unknown,
    };
    ENotificationType CheckNotification(string* ns_node);

    virtual void* Main();

    unsigned short GetPort() const { return m_Receiver.port; }

    const string& GetMessage() const { return m_Receiver.message; }

    void CmdAppendPortAndTimeout(string* cmd, unsigned remaining_seconds);

    SNetScheduleAPIImpl* m_API;

    SNetScheduleNotificationReceiver m_Receiver;

    bool m_StopThread;

    SServerNotifications m_GetNotifications;
    SServerNotifications m_ReadNotifications;
};

struct SNetScheduleAPIImpl : public CObject
{
private:
    enum EMode {
        fWnCompatible       = (0 << 0),
        fNonWnCompatible    = (1 << 0),
        fConfigLoading      = (1 << 1),
        fWorkerNode         = fWnCompatible,
        fNetSchedule        = fNonWnCompatible,
    };
    typedef int TMode;

    static TMode GetMode(bool wn, bool try_config)
    {
        if (wn) return fWorkerNode;
        if (try_config) return fNetSchedule | fConfigLoading;
        return fNetSchedule;
    }

public:
    SNetScheduleAPIImpl(CSynRegistryBuilder registry_builder, const string& section,
        const string& service_name = kEmptyStr, const string& client_name = kEmptyStr,
        const string& queue_name = kEmptyStr, bool wn = false, bool try_config = true);

    // Special constructor for CNetScheduleAPI::GetServer().
    SNetScheduleAPIImpl(SNetServerInPool* server, SNetScheduleAPIImpl* parent);

    ~SNetScheduleAPIImpl();

    CNetScheduleServerListener* GetListener()
    {
        return static_cast<CNetScheduleServerListener*>(
                m_Service->m_Listener.GetPointer());
    }

    CNetScheduleAPI::EJobStatus GetJobStatus(string cmd,
            const CNetScheduleJob& job, time_t* job_exptime,
            ENetScheduleQueuePauseMode* pause_mode);

    const CNetScheduleAPI::SServerParams& GetServerParams() { return m_ServerParamsSync(m_Service, m_Queue); }

    typedef CNetScheduleAPI::TQueueParams TQueueParams;
    void GetQueueParams(const string& queue_name, TQueueParams& queue_params);
    void GetQueueParams(TQueueParams& queue_params);

    CNetServer GetServer(const string& job_key)
    {
        CNetScheduleKey nskey(job_key, m_CompoundIDPool);
        return m_Service.GetServer(nskey.host, nskey.port);
    }

    CNetServer GetServer(const CNetScheduleJob& job)
    {
        return job.server != NULL ? job.server : GetServer(job.job_id);
    }

    template <class TJob>
    string ExecOnJobServer(const TJob& job, const string& cmd, ESwitch roe = eDefault)
    {
        auto server = GetServer(job);
        auto retry_on_exception = (roe == eDefault) ? m_RetryOnException : (roe == eOn);
        return server->ConnectAndExec(cmd, false, retry_on_exception).response;
    }

    bool GetServerByNode(const string& ns_node, CNetServer* server);

    void AllocNotificationThread();
    void StartNotificationThread();

    // Unregister client-listener. After this call, the
    // server will not try to send any notification messages or
    // maintain job affinity for the client.
    void x_ClearNode();

    void UpdateAuthString();
    void UseOldStyleAuth();
    void SetAuthParam(const string& param_name, const string& param_value);
    CCompoundIDPool GetCompoundIDPool() { return m_CompoundIDPool; }
    void Init(CSynRegistry& registry, SRegSynonyms& sections);
    void InitAffinities(CSynRegistry& registry, const SRegSynonyms& sections);
    string MakeAuthString();

private:
    const TMode m_Mode;

public:
    CNetScheduleAPI::EClientType m_ClientType = CNetScheduleAPI::eCT_Auto;
    CRef<SNetScheduleSharedData> m_SharedData;
    bool m_RetryOnException;
    CNetService m_Service;

    string m_Queue;
    string m_ProgramVersion;
    string m_ClientNode;
    string m_ClientSession;

    typedef map<string, string> TAuthParams;
    TAuthParams m_AuthParams;

    struct SServerParamsSync
    {
        const CNetScheduleAPI::SServerParams& operator()(CNetService& service, const string& queue);

    private:
        CFastMutex m_FastMutex;
        CNetScheduleAPI::SServerParams m_ServerParams;
        long m_AskCount = 0;
        constexpr static long kAskMaxCount = 100;
    } m_ServerParamsSync;

    CNetScheduleExecutor::EJobAffinityPreference m_AffinityPreference = CNetScheduleExecutor::eAnyJob;
    list<string> m_AffinityList;

    CNetScheduleGetJob::TAffinityLadder m_AffinityLadder;

    string m_JobGroup;
    unsigned m_JobTtl = 0;

    bool m_UseEmbeddedStorage;

    CCompoundIDPool m_CompoundIDPool;

    CFastMutex m_NotificationThreadMutex;
    CRef<SNetScheduleNotificationThread> m_NotificationThread;
    CAtomicCounter_WithAutoInit m_NotificationThreadStartStopCounter;
};


struct SNetScheduleSubmitterImpl : public CObject
{
    SNetScheduleSubmitterImpl(CNetScheduleAPI::TInstance ns_api_impl);

    string SubmitJobImpl(CNetScheduleNewJob& job, unsigned short udp_port,
            unsigned wait_time, CNetServer* server = NULL);

    void FinalizeRead(const char* cmd_start,
        const string& job_id,
        const string& auth_token,
        const string& error_message);

    CNetScheduleAPI::EJobStatus SubmitJobAndWait(CNetScheduleJob& job,
            unsigned wait_time, time_t* job_exptime = NULL);

    void AppendClientIPSessionIDHitID(string& cmd, const string& job_group);

    CNetScheduleAPI m_API;
};

inline SNetScheduleSubmitterImpl::SNetScheduleSubmitterImpl(
    CNetScheduleAPI::TInstance ns_api_impl) :
    m_API(ns_api_impl)
{
}

struct SNetScheduleExecutorImpl : public CObject
{
    SNetScheduleExecutorImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl),
        m_AffinityPreference(ns_api_impl->m_AffinityPreference),
        m_JobGroup(ns_api_impl->m_JobGroup)
    {
        copy(ns_api_impl->m_AffinityList.begin(),
             ns_api_impl->m_AffinityList.end(),
             inserter(m_PreferredAffinities, m_PreferredAffinities.end()));
    }

    void ClaimNewPreferredAffinity(CNetServer orig_server,
        const string& affinity);
    string MkSETAFFCmd();
    bool ExecGET(SNetServerImpl* server,
            const string& get_cmd, CNetScheduleJob& job);
    bool x_GetJobWithAffinityLadder(SNetServerImpl* server,
            const CDeadline& timeout,
            const string& prio_aff_list,
            bool any_affinity,
            CNetScheduleJob& job);

    void ReturnJob(const CNetScheduleJob& job, bool blacklist = true);

    enum EChangeAffAction {
        eAddAffs,
        eDeleteAffs
    };
    int AppendAffinityTokens(string& cmd,
            const vector<string>* affs, EChangeAffAction action);

    CNetScheduleAPI m_API;

    CNetScheduleExecutor::EJobAffinityPreference m_AffinityPreference;

    CNetScheduleNotificationHandler m_NotificationHandler;

    CFastMutex m_PreferredAffMutex;
    set<string> m_PreferredAffinities;

    string m_JobGroup;
    ESwitch retry_on_exception = eOn;
};

class CNetScheduleGETCmdListener : public INetServerExecListener
{
public:
    CNetScheduleGETCmdListener(SNetScheduleExecutorImpl* executor) :
        m_Executor(executor)
    {
    }

    virtual void OnExec(CNetServerConnection::TInstance conn_impl,
            const string& cmd);

    SNetScheduleExecutorImpl* m_Executor;
};

const unsigned s_Timeout = 10;

struct SNetScheduleJobReaderImpl : public CObject
{
    SNetScheduleJobReaderImpl(CNetScheduleAPI::TInstance ns_api_impl,
            const string& group, const string& affinity) :
        m_Impl(ns_api_impl, group, affinity),
        m_Timeline(m_Impl)
    {
    }

    void x_StartNotificationThread()
    {
        m_Impl.m_API->StartNotificationThread();
    }

    CNetScheduleJobReader::EReadNextJobResult ReadNextJob(
        CNetScheduleJob* job,
        CNetScheduleAPI::EJobStatus* job_status,
        const CTimeout* timeout);
    void InterruptReading();

private:
    class CImpl : public CNetScheduleGetJob
    {
    public:
        CImpl(CNetScheduleAPI::TInstance ns_api_impl,
                const string& group, const string& affinity) :
            m_API(ns_api_impl),
            m_Timeout(s_Timeout),
            m_JobGroup(group),
            m_Affinity(affinity),
            m_MoreJobs(false)
        {
            limits::Check<limits::SJobGroup>(group);
            limits::Check<limits::SAffinity>(affinity);
        }

        EState CheckState();
        CNetServer ReadNotifications();
        CNetServer WaitForNotifications(const CDeadline& deadline);
        bool MoreJobs(const SEntry& entry);
        bool CheckEntry(
                SEntry& entry,
                const string& prio_aff_list,
                bool any_affinity,
                CNetScheduleJob& job,
                CNetScheduleAPI::EJobStatus* job_status);
        void ReturnJob(CNetScheduleJob& job);

        CNetScheduleAPI m_API;
        const unsigned m_Timeout;
        string m_JobGroup;
        string m_Affinity;

    private:
        bool m_MoreJobs;
    };

    CImpl m_Impl;
    CNetScheduleGetJobImpl<CImpl> m_Timeline;
};

struct SNetScheduleAdminImpl : public CObject
{
    SNetScheduleAdminImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl)
    {
    }

    CNetScheduleAPI m_API;
};

END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP */
