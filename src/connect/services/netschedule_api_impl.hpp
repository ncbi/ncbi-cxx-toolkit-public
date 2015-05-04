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
#include "timeline.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/error_codes.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////
//

#define SERVER_PARAMS_ASK_MAX_COUNT 100

bool g_ParseGetJobResponse(CNetScheduleJob& job, const string& response);

inline string g_MakeBaseCmd(const string& cmd_name, const string& job_key)
{
    string cmd(cmd_name);
    cmd += ' ';
    cmd += job_key;
    return cmd;
}

struct SNetScheduleServerProperties : public INetServerProperties
{
    SNetScheduleServerProperties() :
        affs_synced(false)
    {
    }

    CFastMutex m_Mutex;

    string ns_node;
    string ns_session;

    bool affs_synced;
};

// A namespace-like helper class to load configuration from NetSchedule server
class CNetScheduleConfigLoader
{
public:
    enum EParseMode {
        eDefault,
        eGetQueueName
    };

    static bool Use(CConfig* config, const string& section)
    {
        return config && config->GetBool(section, "load_config_from_ns",
                CConfig::eErr_NoThrow, false);
    }

    static CConfig* Get(const CTempString* literals, SNetScheduleAPIImpl* impl,
            string* section, EParseMode mode = eDefault);

private:
    typedef CNetScheduleAPI::TQueueParams TParams;

    static CConfig* Parse(const TParams& params, const CTempString& prefix,
            EParseMode mode);
};

class CNetScheduleServerListener : public INetServerConnectionListener
{
public:
    CNetScheduleServerListener() :
        m_ClientType(CNetScheduleAPI::eCT_Auto),
        m_WorkerNodeCompatMode(false)
    {
    }

    void SetAuthString(SNetScheduleAPIImpl* impl);

    bool NeedToSubmitAffinities(SNetServerImpl* server_impl);
    void SetAffinitiesSynced(SNetServerImpl* server_impl, bool affs_synced);

    CRef<SNetScheduleServerProperties> x_GetServerProperties(
            SNetServerImpl* server_impl);

    virtual CRef<INetServerProperties> AllocServerProperties();

    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection& connection);
    virtual void OnError(const string& err_msg, CNetServer& server);
    virtual void OnWarning(const string& warn_msg, CNetServer& server);

    string m_Auth;
    CRef<INetEventHandler> m_EventHandler;

    CFastMutex m_ServerByNodeMutex;
    typedef map<string, SNetServerInPool*> TServerByNode;
    TServerByNode m_ServerByNode;

    // Make sure the worker node does not attempt to submit its
    // preferred affinities from two threads.
    CFastMutex m_AffinitySubmissionMutex;

    CNetScheduleAPI::EClientType m_ClientType;
    bool m_WorkerNodeCompatMode;
};

// Node IDs of the servers that are ready
// (i.e. the servers have sent notifications).
typedef set<string> TReadyServers;

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
    // A set of NetSchedule node IDs that are ready.
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

    unsigned short GetPort() const {return m_UDPPort;}

    const string& GetMessage() const {return m_Message;}

    void PrintPortNumber();

    void CmdAppendPortAndTimeout(string* cmd, const CDeadline* deadline);

    SNetScheduleAPIImpl* m_API;

    CDatagramSocket m_UDPSocket;
    unsigned short m_UDPPort;

    char m_Buffer[1024];
    string m_Message;

    bool m_StopThread;

    SServerNotifications m_GetNotifications;
    SServerNotifications m_ReadNotifications;
};

struct SNetScheduleAPIImpl : public CObject
{
    SNetScheduleAPIImpl(CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name);

    // Special constructor for CNetScheduleAPI::GetServer().
    SNetScheduleAPIImpl(SNetServerInPool* server, SNetScheduleAPIImpl* parent);

    CNetScheduleServerListener* GetListener()
    {
        return static_cast<CNetScheduleServerListener*>(
                m_Service->m_Listener.GetPointer());
    }

    string x_ExecOnce(const string& cmd_name, const CNetScheduleJob& job)
    {
        string cmd(g_MakeBaseCmd(cmd_name, job.job_id));
        g_AppendClientIPSessionIDHitID(cmd);

        CNetServer::SExecResult exec_result;

        GetServer(job)->ConnectAndExec(cmd, false, exec_result);

        return exec_result.response;
    }

    CNetScheduleAPI::EJobStatus GetJobStatus(const string& cmd,
            const CNetScheduleJob& job, time_t* job_exptime,
            ENetScheduleQueuePauseMode* pause_mode);

    const CNetScheduleAPI::SServerParams& GetServerParams();

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

    bool GetServerByNode(const string& ns_node, CNetServer* server);

    static void VerifyJobGroupAlphabet(const string& job_group)
    {
        g_VerifyAlphabet(job_group, "job group name", eCC_BASE64_PI);
    }

    static void VerifyAuthTokenAlphabet(const string& auth_token)
    {
        g_VerifyAlphabet(auth_token, "security token", eCC_BASE64_PI);
    }

    static void VerifyAffinityAlphabet(const string& affinity)
    {
        g_VerifyAlphabet(affinity, "affinity token", eCC_BASE64_PI);
    }

    static void VerifyQueueNameAlphabet(const string& queue_name);

    void AllocNotificationThread();
    void StartNotificationThread();
    void StopNotificationThread();

    // Unregister client-listener. After this call, the
    // server will not try to send any notification messages or
    // maintain job affinity for the client.
    void x_ClearNode();

    CNetService m_Service;

    string m_Queue;
    string m_ProgramVersion;
    string m_ClientNode;
    string m_ClientSession;

    typedef map<string, string> TAuthParams;
    TAuthParams m_AuthParams;

    auto_ptr<CNetScheduleAPI::SServerParams> m_ServerParams;
    long m_ServerParamsAskCount;
    CFastMutex m_FastMutex;

    CNetScheduleExecutor::EJobAffinityPreference m_AffinityPreference;
    list<string> m_AffinityList;
    list<string> m_AffinityLadder;

    string m_JobGroup;

    bool m_UseEmbeddedStorage;

    CCompoundIDPool m_CompoundIDPool;

    CFastMutex m_NotificationThreadMutex;
    CRef<SNetScheduleNotificationThread> m_NotificationThread;
    CAtomicCounter_WithAutoInit m_NotificationThreadStartStopCounter;
};


struct SNetScheduleSubmitterImpl : public CObject
{
    SNetScheduleSubmitterImpl(CNetScheduleAPI::TInstance ns_api_impl);

    string SubmitJobImpl(CNetScheduleJob& job, unsigned short udp_port,
            unsigned wait_time, CNetServer* server = NULL);

    void FinalizeRead(const char* cmd_start,
        const char* cmd_name,
        const string& job_id,
        const string& auth_token,
        const string& error_message);

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
        m_JobGroup(ns_api_impl->m_JobGroup),
        m_WorkerNodeMode(false)
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
    bool x_GetJobWithAffinityList(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job,
            CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
            const string& affinity_list);
    bool x_GetJobWithAffinityLadder(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job);

    void ExecWithOrWithoutRetry(const CNetScheduleJob& job, const string& cmd);

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

    // True when this object is used by a real
    // worker node application (CGridWorkerNode).
    bool m_WorkerNodeMode;
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

struct SNotificationTimelineEntry : public CWorkerNodeTimelineEntry
{
    typedef CRef<SNotificationTimelineEntry> TRef;

    SServerAddress m_ServerAddress;

    unsigned m_DiscoveryIteration;

    SNotificationTimelineEntry(const SServerAddress& server_address,
            unsigned discovery_iteration) :
        m_ServerAddress(server_address),
        m_DiscoveryIteration(discovery_iteration)
    {
    }

    struct SLess
    {
        bool operator ()(const SNotificationTimelineEntry* left,
                const SNotificationTimelineEntry* right) const
        {
            return left->m_ServerAddress < right->m_ServerAddress;
        }
    };

    bool IsDiscoveryAction() const {return m_DiscoveryIteration == 0;}
};

class CNotificationTimeline
{
public:
    typedef CWorkerNodeTimeline<SNotificationTimelineEntry,
            SNotificationTimelineEntry::TRef> TNotificationTimeline;
    TNotificationTimeline m_ImmediateActions, m_Timeline;

protected:
    CNotificationTimeline();
    ~CNotificationTimeline();

    SNotificationTimelineEntry* x_GetTimelineEntry(SNetServerImpl* server_impl);

    unsigned m_DiscoveryIteration;
    SNotificationTimelineEntry::TRef m_DiscoveryAction;

private:
    typedef set<SNotificationTimelineEntry*,
            SNotificationTimelineEntry::SLess> TTimelineEntries;
    TTimelineEntries m_TimelineEntryByAddress;
};

struct SNetScheduleJobReaderImpl : public CObject, public CNotificationTimeline
{
    SNetScheduleJobReaderImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl)
    {
    }

    CNetScheduleAPI m_API;

    CAtomicCounter_WithAutoInit m_NotificationThreadIsRunning;

    void x_StartNotificationThread()
    {
        if (m_NotificationThreadIsRunning.Get() == 0 &&
                m_NotificationThreadIsRunning.Add(1) == 1)
            m_API->StartNotificationThread();
    }

    string m_JobGroup;
    string m_Affinity;

    bool x_ReadJob(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job,
            CNetScheduleAPI::EJobStatus* job_status,
            bool* no_more_jobs);
    bool x_PerformTimelineAction(TNotificationTimeline& timeline,
            CNetScheduleJob& job,
            CNetScheduleAPI::EJobStatus* job_status,
            bool* no_more_jobs);
    void x_ProcessReadJobNotifications();

    virtual ~SNetScheduleJobReaderImpl();
};

struct SNetScheduleAdminImpl : public CObject
{
    SNetScheduleAdminImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl)
    {
    }

    typedef map<pair<string, unsigned int>, string> TIDsMap;

    CNetScheduleAPI m_API;
};

END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP */
