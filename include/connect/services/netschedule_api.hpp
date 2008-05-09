#ifndef CONN___NETSCHEDULE_API__HPP
#define CONN___NETSCHEDULE_API__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   NetSchedule client API.
 *
 */


/// @file netschedule_client.hpp
/// NetSchedule client specs.
///

#include <connect/services/netservice_api.hpp>
#include <corelib/plugin_manager.hpp>

#include <connect/services/netschedule_key.hpp>
#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/netschedule_api_const.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */

template<typename T> struct ToStr { static string Convert(T t); };

template<> struct ToStr<string> {
    static string Convert(const string& val) {
        return '\"' + NStr::PrintableString(val) + '\"';
    }
};
template<> struct ToStr<unsigned int> {
    static string Convert(unsigned int val) {
        return NStr::UIntToString(val);
    }
};

template<> struct ToStr<int> {
    static string Convert(int val) {
        return NStr::IntToString(val);
    }
};

class CNetScheduleSubmitter;
class CNetScheduleExecuter;
class CNetScheduleAdmin;

struct CNetScheduleJob;


/// Client API for NCBI NetSchedule server
///
/// This API is logically divided into two sections:
/// Job Submitter API and Worker Node API.
///
///
/// @sa CNetServiceException, CNetScheduleException
///

class NCBI_XCONNECT_EXPORT CNetScheduleAPI : public CNetServiceAPI_Base
{
public:

    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// job key
    ///
    /// @param service_name
    ///    Name of the servcie to connect to (format: LB service name or host:port)
    /// @param client_name
    ///    Name of the client program(project)
    /// @param queue_name
    ///    Name of the job queue
    ///
    CNetScheduleAPI(const string& service_name,
                    const string& client_name,
                    const string& queue_name);

    virtual ~CNetScheduleAPI();

    /// Set program version (like: MyProgram v. 1.2.3)
    ///
    /// Program version is passed to NetSchedule queue so queue
    /// controls versions and does not allow obsolete clients
    /// to connect and submit or execute jobs
    ///
    void SetProgramVersion(const string& pv) { m_ProgramVersion = pv; }

    /// Get program version string
    const string& GetProgramVersion() const { return m_ProgramVersion; }

    /// Return Queue name
    const string& GetQueueName() const { return m_Queue; }
    // -----------------------------------------------------------------

    /// Job status codes
    ///
    enum EJobStatus
    {
        eJobNotFound = -1,  ///< No such job
        ePending     = 0,   ///< Waiting for execution
        eRunning,           ///< Running on a worker node
        eReturned,          ///< Returned back to the queue(to be rescheduled)
        eCanceled,          ///< Explicitly canceled
        eFailed,            ///< Failed to run (execution timeout)
        eDone,              ///< Job is ready (computed successfully)

        eLastStatus         ///< Fake status (do not use)
    };

    /// Printable status type
    static
    string StatusToString(EJobStatus status);

    /// Parse status string into enumerator value
    ///
    /// Acceptable string values:
    ///   Pending, Running, Returned, Canceled, Failed, Done
    ///   Pend, Run, Return, Cancel, Fail
    ///
    /// @return eJobNotFound if string cannot be parsed
    static
    EJobStatus StringToStatus(const string& status_str);

    /// Job masks
    ///
    enum EJobMask {
        eEmptyMask    = 0x0,
        eExclusiveJob = 0x1,  ///< Exlcusive job
        eUsersMask    = (1 << 16) ///< User's masks start from here
    };
    typedef unsigned TJobMask;

    /// Key-value pair, value can be empty
    typedef pair<string,string> TJobTag;
    typedef vector<TJobTag>     TJobTags;


    CNetScheduleSubmitter GetSubmitter();
    CNetScheduleExecuter  GetExecuter();
    CNetScheduleAdmin     GetAdmin();

    struct SServerParams {
        size_t max_input_size;
        size_t max_output_size;
        bool   fast_status;
    };

    const SServerParams& GetServerParams();

    /// Get job's details
    /// @param job
    ///
    EJobStatus GetJobDetails(CNetScheduleJob& job);

    void GetProgressMsg(CNetScheduleJob& job);

    virtual void ProcessServerError(string& response, ETrimErr trim_err) const;

private:
    friend class CNetScheduleSubmitter;
    friend class CNetScheduleExecuter;
    friend class CNetScheduleAdmin;

    CNetScheduleAPI(const CNetScheduleAPI&);
    CNetScheduleAPI& operator=(const CNetScheduleAPI&);

    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key)
    {
        return SendCmdWaitResponse(x_GetConnection(job_key), cmd + ' ' + job_key);
    }
    template<typename Arg1>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key, Arg1 arg1)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1);
        return SendCmdWaitResponse(x_GetConnection(job_key), tmp);
    }
    template<typename Arg1, typename Arg2>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key,
                                    Arg1 arg1, Arg2 arg2)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1) + ' ' + ToStr<Arg2>::Convert(arg2);
        return SendCmdWaitResponse(x_GetConnection(job_key), tmp);
    }
    template<typename Arg1, typename Arg2, typename Arg3>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key,
                                    Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1) + ' '
            + ToStr<Arg2>::Convert(arg2) + ' ' + ToStr<Arg3>::Convert(arg3);
        return SendCmdWaitResponse(x_GetConnection(job_key), tmp);
    }

    CNetServerConnection x_GetConnection(const string& job_key = kEmptyStr);

    virtual void x_SendAuthetication(CNetServerConnection& conn) const;

    EJobStatus x_GetJobStatus(const string& job_key, bool submitter);

private:
    static CNetScheduleExceptionMap sm_ExceptionMap;
    string            m_Queue;
    string            m_ProgramVersion;

    mutable auto_ptr<SServerParams> m_ServerParams;
    mutable long m_ServerParamsAskCount;
    mutable CFastMutex m_ServerParamsMutex;
};


/////////////////////////////////////////////////////////////////////////////////////
////
/// Job description
///
struct CNetScheduleJob
{
    CNetScheduleJob(const string& _input = kEmptyStr, const string& _affinity = kEmptyStr,
                    CNetScheduleAPI::TJobMask _mask = CNetScheduleAPI::eEmptyMask)
        : input(_input), affinity(_affinity), mask(_mask), ret_code(0) {}

    void Reset()
    {
        input.erase();
        affinity.erase();
        mask = CNetScheduleAPI::eEmptyMask;
        tags.clear();
        job_id.erase();
        ret_code = 0;
        output.erase();
        error_msg.erase();
        progress_msg.erase();
    }
    // input parameters

    ///    Input data. Arbitrary string (cannot exceed 1K). This string
    ///    encodes input data for the job. It is suggested to use NetCache
    ///    to keep the actual data and pass NetCache key as job input.
    string    input;

    string    affinity;
    CNetScheduleAPI::TJobMask  mask;
    CNetScheduleAPI::TJobTags  tags;

    // output and error

    ///    Job key
    string    job_id;
    ///    Job return code.
    int       ret_code;
    ///    Job result data.  Arbitrary string (cannot exceed 1K).
    string    output;
    string    error_msg;
    string    progress_msg;
};

class NCBI_XCONNECT_EXPORT CNetScheduleSubmitter
{
public:

    /// Submit job.
    /// @note on success job.job_id will be set.
    string SubmitJob(CNetScheduleJob& job) const;

    /// Submit job batch.
    /// Method automatically splits the submission into reasonable sized
    /// transactions, so there is no limit on the input batch size.
    ///
    /// Every job in the job list receives job id
    ///
    void SubmitJobBatch(vector<CNetScheduleJob>& jobs) const;

    /// Submit job to server and wait for the result.
    /// This function should be used if we expect that job execution
    /// infrastructure is capable of finishing job in the specified
    /// time frame. This method can save a lot of roundtrips with the
    /// netschedule server (comparing to series of GetStatus calls).
    ///
    /// @param job
    ///    NetSchedule job description structure
    /// @param wait_time
    ///    Time in seconds function waits for the job to finish.
    ///    If job does not finish in the output parameter will hold the empty
    ///    string.
    /// @param udp_port
    ///    UDP port to listen for queue notifications. Try to avoid many
    ///    client programs (or threads) listening on the same port. Message
    ///    is going to be delivered to just only one listener.
    ///
    /// @return job status
    ///
    /// @note the result fields of the job description structure will be set
    ///    if the job is fininshed during specified time.
    ///
    CNetScheduleAPI:: EJobStatus SubmitJobAndWait(CNetScheduleJob& job,
                                                  unsigned       wait_time,
                                                  unsigned short udp_port);

    /// Cancel job
    ///
    /// @param job_key
    ///    Job key
    void CancelJob(const string& job_key) const;

    /// Get progress message
    ///
    /// @param job
    ///    NetSchedule job description structure. The message is taken from
    ///    progress_msg filed
    ///
    /// @sa PutProgressMsg
    ///
    void GetProgressMsg(CNetScheduleJob& job);

    /// Request of current job status
    /// eJobNotFound is returned if job status cannot be found
    /// (job record timed out)
    ///
    CNetScheduleAPI::EJobStatus GetJobStatus(const string& job_key);

    CNetScheduleAPI::EJobStatus GetJobDetails(CNetScheduleJob& job);

    const CNetScheduleAPI::SServerParams& GetServerParams();


private:
    friend class CNetScheduleAPI;

    CNetScheduleSubmitter(CNetScheduleAPI* api) : m_API(api) {}

    string SubmitJobImpl(CNetScheduleJob& job,
        unsigned short udp_port, unsigned wait_time) const;

    CNetScheduleAPI* m_API;
};

inline string CNetScheduleSubmitter::SubmitJob(CNetScheduleJob& job) const
{
    return SubmitJobImpl(job, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////
////
class NCBI_XCONNECT_EXPORT CNetScheduleExecuter
{
public:
    /// Get a pending job.
    /// When function returns TRUE and job_key job receives running status,
    /// client(worker node) becomes responsible for execution or returning
    /// the job. If there are no jobs in the queue function returns FALSE
    /// immediately and you have to repeat the call (after a delay).
    /// Consider WaitJob method as an alternative.
    ///
    /// @param job
    ///     NetSchedule job description structure
    /// @param udp_port
    ///     Used to instruct server that specified client does NOT
    ///     listen to notifications (opposed to WaitJob)
    ///
    /// @return
    ///     TRUE if job has been returned from the queue and its input
    ///     fields are set.
    ///     FALSE means queue is empty or for some reason scheduler
    ///     decided not to grant the job (node is overloaded).
    ///     In this case worker node should pause and come again later
    ///     for a new job.
    ///
    /// @sa WaitJob
    ///
    bool GetJob(CNetScheduleJob& job,
                unsigned short udp_port = 0) const;

    /// Notification wait mode
    enum EWaitMode {
        eWaitNotification,   ///< Wait for notification
        eNoWaitNotification  ///< Register for notofication but do not wait
    };

    /// Wait for a job to come.
    /// Variant of GetJob method. The difference is that if there no
    /// pending jobs, method waits for a notification from the server.
    ///
    /// NetSchedule server sends UDP packets with queue notification
    /// information. This is unreliable protocol, some notification may be
    /// lost. WaitJob internally makes an attempt to connect the server using
    /// reliable statefull TCP/IP, so even if some UDP notifications are
    /// lost jobs will be still delivered (with a delay).
    ///
    /// When new job arrives to the queue server may not send the notification
    /// to all clients immediately, it depends on specific queue notification
    /// timeout
    ///
    /// @param job
    ///     NetSchedule job description structure
    /// @param wait_time
    ///    Time in seconds function waits for new jobs to come.
    ///    If there are no jobs in the period of time, function retuns FALSE
    ///    Do not choose too long waiting time because it increases chances of
    ///    UDP notification loss. (60-320 seconds should be a reasonable value)
    /// @param udp_port
    ///    UDP port to listen for queue notifications. Try to avoid many
    ///    client programs (or threads) listening on the same port. Message
    ///    is going to be delivered to just only one listener.
    ///
    /// @param wait_mode
    ///    Notification wait mode. Function either waits for the message in
    ///    this call, or returns control (eNoWaitNotification).
    ///    In the second case caller should call WaitNotification to listen
    ///    for server signals.
    ///
    /// @sa GetJob, WaitNotification
    ///
    bool WaitJob(CNetScheduleJob& job,
                 unsigned       wait_time,
                 unsigned short udp_port,
                 EWaitMode      wait_mode = eWaitNotification) const;


    /// Put job result (job should be received by GetJob() or WaitJob())
    ///
    /// @param job
    ///     NetSchedule job description structure. its ret_code
    ///     and output fields should be set
    ///
    void PutResult(const CNetScheduleJob& job) const;

    /// Put job result, get new job from the queue
    /// If this is the first call and there is no previous job
    /// (done_job.job_id is empty) this is equivalent to GetJob
    ///
    /// @sa PutResult, GetJob
    bool PutResultGetJob(const CNetScheduleJob& done_job, CNetScheduleJob& new_job) const;

    /// Put job interim (progress) message
    ///
    /// @param job
    ///     NetSchedule job description structure. its progerss_msg
    ///     field should be set
    ///
    /// @sa GetProgressMsg
    ///
    void PutProgressMsg(const CNetScheduleJob& job) const;

    /// Get progress message
    ///
    /// @param job
    ///    NetSchedule job description structure. The message is taken from
    ///    progress_msg filed
    ///
    /// @sa PutProgressMsg
    ///
    void GetProgressMsg(CNetScheduleJob& job);


    /// Submit job failure diagnostics. This method indicates that
    /// job failed because of some fatal, unrecoverable error.
    ///
    /// @param job
    ///     NetSchedule job description structure. its error_msg
    ///     and optionaly ret_code and output fields should be set
    ///
    void PutFailure(const CNetScheduleJob& job) const;

    /// Request of current job status
    /// eJobNotFound is returned if job status cannot be found
    /// (job record timed out)
    ///
    CNetScheduleAPI::EJobStatus GetJobStatus(const string& job_key);

    /// Transfer job to the "Returned" status. It will be
    /// re-executed after a while.
    ///
    /// Node may decide to return the job if it cannot process it right
    /// now (does not have resources, being asked to shutdown, etc.)
    ///
    void ReturnJob(const string& job_key) const;

    /// Set job execution timeout
    ///
    /// When node picks up the job for execution it may evaluate what time it
    /// takes for computation and report it to the queue. If job does not
    /// finish in the specified timeframe (because of a failure)
    /// it is going to be rescheduled
    ///
    /// Default value for the run timeout specified in the queue settings on
    /// the server side.
    ///
    /// @param time_to_run
    ///    Time in seconds to finish the job. 0 means "queue default value".
    ///
    void SetRunTimeout(const string& job_key, unsigned time_to_run) const;

    /// Increment job execution timeout
    ///
    /// When node picks up the job for execution it may peridically
    /// communicate to the server that job is still alive and
    /// prolong job execution timeout, so job server does not try to
    /// reschedule.
    ///
    ///
    /// @param runtime_inc
    ///    Estimated time in seconds(from the current moment) to
    ///    finish the job.
    ///
    /// @sa SetRunTimeout
    void JobDelayExpiration(const string& job_key, unsigned runtime_inc) const;


    /// Register client-listener
    void RegisterClient(unsigned short udp_port) const;

    const CNetScheduleAPI::SServerParams& GetServerParams() const;


    /// Unregister client-listener. After this call
    /// server will not try to send any notification messages or maintain
    /// job affinity for the client.
    void UnRegisterClient(unsigned short udp_port) const;

    static bool WaitNotification(const string&  queue_name,
                                 unsigned       wait_time,
                                 unsigned short udp_port);

    /// Return Queue name
    const string& GetQueueName() const;
    const string& GetClientName() const;
    const string& GetServiceName() const;

private:
    friend class CNetScheduleAPI;
    CNetScheduleExecuter(CNetScheduleAPI* api) : m_API(api) {}

    CNetScheduleAPI* m_API;

    bool GetJobImpl(const string& cmd, CNetScheduleJob& job) const;

    void x_RegUnregClient(const string& cmd, unsigned short udp_port) const;
};

/////////////////////////////////////////////////////////////////////////////////////
////

class NCBI_XCONNECT_EXPORT CNetScheduleAdmin
{
public:

    /// Status map, shows number of jobs in each status
    typedef map<CNetScheduleAPI::EJobStatus, unsigned> TStatusMap;

    /// Returns statuses for a given affnity token
    /// @param status_map
    ///    Status map (status to job count)
    /// @param affinity_token
    ///    Affinity token (optional)
    void StatusSnapshot(TStatusMap& status_map,
                        const string& affinity_token) const;


    /// Delete job
    void DropJob(const string& job_key);

    enum ECreateQueueFlags {
        eIgnoreDuplicateName = 0,
        eErrorOnDublicateName
    };

    /// Create queue of given queue class
    /// @param qname
    ///    Name of the queue to create
    /// @param qclass
    ///    Parameter set described in config file in qclass_* section
    void CreateQueue(const string& qname, const string& qclass,
                     const string& comment = kEmptyStr, ECreateQueueFlags flags = eIgnoreDuplicateName) const;

    /// Delete queue
    /// Applicable only to queues, created through CreateQueue method
    /// @param qname
    ///    Name of the queue to delete.
    void DeleteQueue(const string& qname) const;


    /// Shutdown level
    ///
    enum EShutdownLevel {
        eNoShutdown = 0,    ///< No Shutdown was requested
        eNormalShutdown,    ///< Normal shutdown was requested
        eShutdownImmediate, ///< Urgent shutdown was requested
        eDie                ///< Somethig wrong has happend, so server should kill itself
    };

    /// Shutdown the server daemon.
    ///
    void ShutdownServer(EShutdownLevel level = eNormalShutdown) const;

    /// Kill all jobs in the queue.
    ///
    void DropQueue() const;

    void DumpJob(CNcbiOstream& out, const string& job_key) const;

    /// Reschedule a job
    ///
    void ForceReschedule(const string& job_key) const;

    /// Turn server-side logging on(off)
    ///
    void Logging(bool on_off) const;

    void ReloadServerConfig() const;

    ///////////////////////////////////???????????????///////////////////
    /// Print version string
    void GetServerVersion(CNetServiceAPI_Base::ISink& sink) const;

    enum EStatisticsOptions
    {
        eStatisticsAll,
        eStaticticsBrief
    };

    void GetServerStatistics(CNetServiceAPI_Base::ISink& sink, EStatisticsOptions opt = eStaticticsBrief) const;

    void DumpQueue(CNetServiceAPI_Base::ISink& sink) const;

    void PrintQueue(CNetServiceAPI_Base::ISink& sink, CNetScheduleAPI::EJobStatus status) const;

    void Monitor(CNcbiOstream& out) const;

    /// ";" delimited list of server queues
    void GetQueueList(CNetServiceAPI_Base::ISink& sink) const;
    ///////////////////////////////////???????????????///////////////////

    /// Query by tags
    unsigned long Count(const string& query) const;
    void Cancel(const string& query) const;
    void Drop(const string& query) const;
    void Query(const string& query, const vector<string>& fields, CNcbiOstream& os) const;
    void Select(const string& select_stmt, CNcbiOstream& os) const;

    template <typename TBVector>
    void RetrieveKeys(const string& query, CNetScheduleKeys<TBVector>& ids) const;

    typedef map<pair<string,unsigned int>, string> TIDsMap;
private:
    friend class CNetScheduleAPI;
    CNetScheduleAdmin(CNetScheduleAPI* api) : m_API(api) {}

    TIDsMap x_QueueIDs(const string& queury) const;

    CNetScheduleAPI* m_API;
};

/////////////////////////////////////////////////////////


inline CNetScheduleSubmitter CNetScheduleAPI::GetSubmitter()
{
    DiscoverLowPriorityServers(eOff);
    return CNetScheduleSubmitter(this);
}
inline CNetScheduleExecuter CNetScheduleAPI::GetExecuter()
{
    DiscoverLowPriorityServers(eOn);
    return CNetScheduleExecuter(this);
}
inline CNetScheduleAdmin CNetScheduleAPI::GetAdmin()
{
    DiscoverLowPriorityServers(eOff);
    return CNetScheduleAdmin(this);
}

inline
CNetServerConnection CNetScheduleAPI::x_GetConnection(const string& job_key)
{
    if (job_key.empty())
        return GetBest();

    CNetScheduleKey nskey(job_key);
    return GetSpecific(nskey.host, nskey.port);
}


inline CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::GetJobStatus(const string& job_key)
{
    return m_API->x_GetJobStatus(job_key, true);
}

inline CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::GetJobDetails(CNetScheduleJob& job)
{
    return m_API->GetJobDetails(job);
}

inline void CNetScheduleSubmitter::GetProgressMsg(CNetScheduleJob& job)
{
    m_API->GetProgressMsg(job);
}
inline
const CNetScheduleAPI::SServerParams& CNetScheduleSubmitter::GetServerParams()
{
    return m_API->GetServerParams();
}


/////////////////////////////////////////////////////////////////////////
inline CNetScheduleAPI::EJobStatus
CNetScheduleExecuter::GetJobStatus(const string& job_key)
{
    return m_API->x_GetJobStatus(job_key, false);
}
inline void CNetScheduleExecuter::GetProgressMsg(CNetScheduleJob& job)
{
    m_API->GetProgressMsg(job);
}

inline const string& CNetScheduleExecuter::GetQueueName() const
{
    return m_API->GetQueueName();
}
inline const string& CNetScheduleExecuter::GetClientName() const
{
    return m_API->GetClientName();
}
inline const string& CNetScheduleExecuter::GetServiceName() const
{
    return m_API->GetServiceName();
}

inline
const CNetScheduleAPI::SServerParams& CNetScheduleExecuter::GetServerParams() const
{
    return m_API->GetServerParams();
}

template <typename BVAlloc>
void CNetScheduleAdmin::RetrieveKeys(const string& query,
                                     CNetScheduleKeys<BVAlloc>& ids) const
{
    TIDsMap  inter_ids = x_QueueIDs(query);
    ids.x_Clear();
    ITERATE(TIDsMap, it, inter_ids) {
        ids.x_Add(it->first, it->second);
    }
}

///////////////////////////////////////////////////////////////////////

NCBI_DECLARE_INTERFACE_VERSION(CNetScheduleAPI,  "xnetschedule_api", 1,0, 0);


/// @internal
extern NCBI_XCONNECT_EXPORT const char* kNetScheduleAPIDriverName;


//extern "C" {

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetscheduleapi(
     CPluginManager<CNetScheduleAPI>::TDriverInfoList&   info_list,
     CPluginManager<CNetScheduleAPI>::EEntryPointRequest method);


//} // extern C


/* @} */


END_NCBI_SCOPE


#endif  /* CONN___NETSCHEDULE_API__HPP */
