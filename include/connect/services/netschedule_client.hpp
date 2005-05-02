#ifndef CONN___NETSCHEDULE_CLIENT__HPP
#define CONN___NETSCHEDULE_CLIENT__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule client API.
 *
 */

/// @file netschedule_client.hpp
/// NetSchedule client specs. 
///

#include <connect/connect_export.h>
#include <connect/ncbi_types.h>
#include <connect/services/netservice_client.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager.hpp>

BEGIN_NCBI_SCOPE


class CSocket;


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Meaningful information encoded in the NetSchedule key
///
/// @sa CNetSchedule_ParseBlobKey
///
struct CNetSchedule_Key
{
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< Job id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};


/// Client API for NCBI NetSchedule server
///
/// This API is logically divided into two sections:
/// Job Submitter API and Worker Node API.
///
///
/// @sa CNetServiceException, CNetScheduleException
///

class NCBI_XCONNECT_EXPORT CNetScheduleClient : protected CNetServiceClient
{
public:
    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// job key 
    ///
    /// @param client_name
    ///    Name of the client program(project)
    /// @param queue_name
    ///    Name of the job queue
    ///
    CNetScheduleClient(const string& client_name,
                       const string& queue_name);

    CNetScheduleClient(const string&  host,
                       unsigned short port,
                       const string&  client_name,
                       const string&  queue_name);

    /// Contruction based on existing TCP/IP socket
    /// @param sock
    ///    Connected socket to the primary server. 
    ///    CNetScheduleClient does not take socket ownership 
    ///    and does not change communication parameters (like timeout)
    ///
    CNetScheduleClient(CSocket*      sock,
                       const string& client_name,
                       const string& queue_name);

    virtual ~CNetScheduleClient();

    /// Enable or disable request rate control (on by default)
    ///
    /// On some systems if we start sending too many requests at a time
    /// we will be running out of TCP/IP ports. Request control rate
    /// automatically introduce delays if client start sending too many
    /// requests
    ///
    /// Important!
    /// Rate control unit is shared between different instances of
    /// CNetScheduleClient and NOT thread syncronized. Disable it if
    /// you need competing threads all use CNetScheduleClient 
    /// (even separate instances).
    ///
    void ActivateRequestRateControl(bool on_off);

    /// Return TRUE if request rate control is ON
    bool RequestRateControl() const { return m_RequestRateControl; }


    /// Set program version (like: MyProgram v. 1.2.3)
    ///
    /// Program version is passed to NetSchedule queue so queue
    /// controls versions and does not allow obsolete clients
    /// to connect and submit or execute jobs
    ///
    void SetProgramVersion(const string& pv) { m_ProgramVersion = pv; }

    /// Get program version string
    const string& GetProgramVersion() const { return m_ProgramVersion; }

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



    /// Submit job to server
    ///
    /// @param input
    ///    Input data. Arbitrary string (cannot exceed 1K). This string
    ///    encodes input data for the job. It is suggested to use NetCache
    ///    to keep the actual data and pass NetCache key as job input.
    /// @param progress_msg
    ///    Initial progress message (in most cases a NetCache key for 
    ///    message exchange)
    ///
    /// @return job key
    virtual
    string SubmitJob(const string& input, 
                     const string& progress_msg = kEmptyStr);

    /// Submit job to server and wait for the result.
    /// This function should be used if we expect that job execution
    /// infrastructure is capable of finishing job in the specified 
    /// time frame. This method can save a lot of roundtrips with the 
    /// netschedule server (comparing to series of GetStatus calls).
    ///
    /// @param input
    ///    Input data. Arbitrary string (cannot exceed 1K). This string
    ///    encodes input data for the job. It is suggested to use NetCache
    ///    to keep the actual data and pass NetCache key as job input.
    ///
    /// @param job_key
    ///    Job key
    ///
    /// @param ret_code
    ///    Job return code (if job finished in the specified timeout.
    ///
    /// @param output
    ///    Job result data. Empty, if job not finished in the specified
    ///    timeout (wait_time).
    /// @param wait_time
    ///    Time in seconds function waits for the job to finish.
    ///    If job does not finish in the output parameter will hold the empty
    ///    string.
    ///
    /// @param udp_port
    ///    UDP port to listen for queue notifications. Try to avoid many 
    ///    client programs (or threads) listening on the same port. Message
    ///    is going to be delivered to just only one listener.
    ///
    /// @return job status
    ///
    virtual
    EJobStatus SubmitJobAndWait(const string&  input,
                                string*        job_key,
                                int*           ret_code,
                                string*        output,
                                string*        err_msg,
                                unsigned       wait_time,
                                unsigned short udp_port);


    /// Cancel job
    ///
    /// @param job_key
    ///    Job identification string
    virtual
    void CancelJob(const string& job_key);



    /// Get a pending job. 
    /// When function returns TRUE and job_key job receives running status,
    /// client(worker node) becomes responsible for execution or returning 
    /// the job. If there are no jobs in the queue function returns FALSE 
    /// immediately and you have to repeat the call (after a delay).
    /// Consider WaitJob method as an alternative.
    ///
    /// @param job_key
    ///     Job key
    /// @param input
    ///     Job input data (NetCache key). 
    /// @param udp_port
    ///     Used to instruct server that specified client does NOT
    ///     listen to notifications (opposed to WaitJob)
    /// @return
    ///     TRUE if job has been returned from the queue.
    ///     FALSE means queue is empty or for some reason scheduler
    ///     decided not to grant the job (node is overloaded).
    ///     In this case worker node should pause and come again later
    ///     for a new job.
    ///
    /// @sa WaitJob
    ///
    virtual
    bool GetJob(string* job_key, 
                string* input, 
                unsigned short udp_port = 0);

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
    /// @param wait_time
    ///    Time in seconds function waits for new jobs to come.
    ///    If there are no jobs in the period of time, function retuns FALSE
    ///    Do not choose too long waiting time because it increases chances of
    ///    UDP notification loss. (60-320 seconds should be a reasonable value)
    ///
    /// @param udp_port
    ///    UDP port to listen for queue notifications. Try to avoid many 
    ///    client programs (or threads) listening on the same port. Message
    ///    is going to be delivered to just only one listener.
    ///
    /// @sa GetJob
    ///
    virtual
    bool WaitJob(string*        job_key, 
                 string*        input, 
                 unsigned       wait_time,
                 unsigned short udp_port);


    /// Put job result (job should be received by GetJob() or WaitJob())
    /// 
    /// @param job_key
    ///     Job key
    /// @param ret_code
    ///     Job return code
    /// @param output
    ///     Job output data (NetCache key). 
    ///
    virtual
    void PutResult(const string& job_key, 
                   int           ret_code, 
                   const string& output);

    /// Put job interim (progress) message
    /// 
    /// @param job_key
    ///     Job key
    /// @param progress_msg
    ///     Progress message (NetCache key)
    /// @sa GetProgressMsg
    ///
    virtual
    void PutProgressMsg(const string& job_key, 
                        const string& progress_msg);

    /// Get progress message
    ///
    /// @param job_key
    ///     Job key
    /// @return Interim (progress) message (NetCache key)
    /// @sa GetProgressMsg
    ///
    virtual
    string GetProgressMsg(const string& job_key);


    /// Submit job failure diagnostics. This method indicates that
    /// job failed because of some fatal, unrecoverable error.
    /// 
    /// @param job_key
    ///     Job key
    /// @param err_msg
    ///     Short diagnostic message
    ///
    virtual
    void PutFailure(const string& job_key, 
                    const string& err_msg);


    /// Request of current job status
    /// eJobNotFound is returned if job status cannot be found 
    /// (job record timed out)
    ///
    /// @param ret_code
    ///    Job return code for successfully finished jobs
    /// @param output
    ///    Job output data (NetCache key).
    /// @param err_msg
    ///    Error message (if job failed)
    /// @param input
    ///    When job is done input contains the job input string
    ///    (if it is a reference to an external BLOB it can be deleted,
    ///     since it is no longer needed)
    /// @param progress_msg
    ///    While job is running on the backend, worker node can 
    ///    submit progress messages
    ///
    virtual
    EJobStatus GetStatus(const string& job_key, 
                         int*          ret_code,
                         string*       output,
                         string*       err_msg = 0,
                         string*       input   = 0);

    /// Transfer job to the "Returned" status. It will be
    /// re-executed after a while. 
    ///
    /// Node may decide to return the job if it cannot process it right
    /// now (does not have resources, being asked to shutdown, etc.)
    ///
    virtual
    void ReturnJob(const string& job_key);

    /// Delete job
    virtual
    void DropJob(const string& job_key);

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
    virtual
    void SetRunTimeout(const string& job_key, unsigned time_to_run);

    /// Return version string
    virtual
    string ServerVersion();

    /// Printable status type 
    static
    string StatusToString(EJobStatus status);

    /// Return Queue name
    const string& GetQueueName() const { return m_Queue; }

    /// Return Client name
    using CNetServiceClient::GetClientName;

    /// Shutdown level
    ///
    enum EShutdownLevel {
        eNoShutdown = 0,   ///< No Shutdown was requested
        eNormalShutdown,   ///< Normal shutdown was requested
        eShutdownImmidiate ///< Urgent shutdown was requested
    };

protected:

    /// Shutdown the server daemon.
    ///
    void ShutdownServer();

    /// Kill all jobs in the queue.
    ///
    void DropQueue();

    void PrintStatistics(CNcbiOstream & out);

    void Monitor(CNcbiOstream & out);

    /// Turn server-side logging on(off)
    ///
    void Logging(bool on_off);

protected:
    virtual 
    void CheckConnect(const string& key);
    bool IsError(const char* str);

    /// Adds trailing: "client\r\n queue\r\n COMMAND"
    void MakeCommandPacket(string* out_str, 
                           const string& cmd_str);

    /// check string for "OK:" prefix, throws an exception if "ERR:"
    void TrimPrefix(string* str);

    /// Check if server returned "OK:"
    void CheckOK(string* str);

    void CommandInitiate(const string& command, 
                         const string& job_key,
                         string*       answer);

    void ParseGetJobResponse(string*        job_key, 
                             string*        input, 
                             const string&  response);

    void WaitQueueNotification(unsigned       wait_time,
                               unsigned short udp_port);

    void WaitJobNotification(unsigned       wait_time,
                             unsigned short udp_port,
                             unsigned       job_id);

    /// Get job with wait notification (no immediate wait)
    bool GetJobWaitNotify(string*    job_key, 
                          string*    input, 
                          unsigned   wait_time,
                          unsigned short udp_port);

private:
    CNetScheduleClient(const CNetScheduleClient&);
    CNetScheduleClient& operator=(const CNetScheduleClient&);

protected:
    string            m_Queue;
    bool              m_RequestRateControl;
    CNetSchedule_Key  m_JobKey;
    string            m_ProgramVersion;

private:
    string         m_Tmp; ///< Temporary string
};





/// Client API for NetSchedule server.
///
/// The same API as provided by CNetScheduleClient, 
/// only integrated with NCBI load balancer.
///
/// Rebalancing is based on a combination of rebalance parameters.
/// When rebalance parameter is not zero and parameter criteria has been 
/// satisfied client connects to NCBI load balancing service (could be a 
/// daemon or a network instance) and obtains the most available server.
/// 
/// The intended use case for this client is long living programs like
/// services or fast CGIs or any other program running a lot of NetSchedule
/// requests.
///
/// @sa CNetScheduleClient
///

class NCBI_XCONNECT_EXPORT CNetScheduleClient_LB : public CNetScheduleClient
{
public:
    typedef CNetScheduleClient  TParent;


    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// job key or received from NCBI load balancing service.
    ///
    /// @param client_name
    ///    Name of the client program(project)
    /// @param lb_service_name
    ///    Service name as listed in NCBI load balancer
    /// @param queue_name
    ///    Name of the job queue
    ///
    /// @param rebalance_time
    ///    Number of seconds after which client is rebalanced
    /// @param rebalance_requests
    ///    Number of requests before rebalancing. 
    ///    0 value means this criteria will not be evaluated.
    ///
    CNetScheduleClient_LB(const string& client_name,
                          const string& lb_service_name,
                          const string& queue_name,
                          unsigned int  rebalance_time     = 10,
                          unsigned int  rebalance_requests = 0);

    virtual ~CNetScheduleClient_LB();

    /// Add service address. This function turns off load balancing 
    /// service discovery. 
    /// Services list should be supplied by AddServiceAddress
    ///
    void AddServiceAddress(const string&  hostname,
                           unsigned short port);

    /// Set max number of retries when queue returns (recoverable) error
    void SetMaxRetry(unsigned max_retry) { m_MaxRetry = max_retry; }

    /// Get max number of retries when queue returns (recoverable) error
    unsigned GetMaxRetry() const { return m_MaxRetry; }


    virtual
    string SubmitJob(const string& input, 
                     const string& progress_msg = kEmptyStr);


    virtual
    bool GetJob(string*        job_key, 
                string*        input, 
                unsigned short udp_port = 0);

    bool WaitJob(string*        job_key, 
                 string*        input, 
                 unsigned       wait_time,
                 unsigned short udp_port);

protected:
    virtual 
    void CheckConnect(const string& key);

    bool NeedRebalance(time_t curr) const;

protected:

    struct SServiceAddress
    {
        unsigned int     host;           ///< host address in network bo
        unsigned short   port;           ///< port (host bo)
        time_t           conn_fail_time; ///< conn failure timestamp

        SServiceAddress() : host(0), port(0) {}
        SServiceAddress(unsigned int host_addr, unsigned short port_num)
            : host(host_addr), port(port_num), conn_fail_time(0)
        {}
    };

    typedef vector<SServiceAddress>  TServiceList;

private:

    void x_GetServerList(const string& service_name);

    bool x_TryGetJob(SServiceAddress& sa,
                     string* job_key, 
                     string* input, 
                     unsigned short udp_port);
    bool x_GetJobWaitNotify(SServiceAddress& sa,
                            string*    job_key, 
                            string*    input, 
                            unsigned   wait_time,
                            unsigned short udp_port);
private:
    CNetScheduleClient_LB(const CNetScheduleClient_LB&);
    CNetScheduleClient_LB& operator=(const CNetScheduleClient_LB&);
private:
    string        m_LB_ServiceName;

    unsigned int  m_RebalanceTime;
    unsigned int  m_RebalanceRequests;

    time_t        m_LastRebalanceTime;
    unsigned int  m_Requests;
    bool          m_StickToHost;
    bool          m_LB_ServiceDiscovery;

    TServiceList  m_ServList;
    unsigned      m_ServListCurr;    ///< Current connected service

    unsigned      m_ConnFailPenalty; ///< Conn fail penalty in seconds
    unsigned      m_MaxRetry;        ///< N. re-attempts to submit(get) job
};


NCBI_DECLARE_INTERFACE_VERSION(CNetScheduleClient,  "xnetschedule", 1, 1, 0);


/// NetSchedule internal exception
///
class CNetScheduleException : public CNetServiceException
{
public:
    enum EErrCode {
        eAuthenticationError,
        eKeyFormatError,
        eInvalidJobStatus,
        eUnknownQueue,
        eTooManyPendingJobs,
        eDataTooLong,
        eInvalidClientOrVersion
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError:    return "eAuthenticationError";
        case eKeyFormatError:         return "eKeyFormatError";
        case eInvalidJobStatus:       return "eInvalidJobStatus";
        case eUnknownQueue:           return "eUnknownQueue";
        case eTooManyPendingJobs:     return "eTooManyPendingJobs";
        case eDataTooLong:            return "eDataTooLong";
        case eInvalidClientOrVersion: return "eInvalidClientOrVersion";
        default:                      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetScheduleException, CNetServiceException);
};


/// Parse job key string into a CNetSchedule_Key structure
extern NCBI_XCONNECT_EXPORT
void CNetSchedule_ParseJobKey(CNetSchedule_Key* key, const string& key_str);

/// Parse job key, extract job id
extern NCBI_XCONNECT_EXPORT
unsigned CNetSchedule_GetJobId(const string&  key_str);


/// Generate job key string
///
/// Please note that "id" is an integer issued by the scheduling server.
/// Clients should not use this function with custom ids. 
/// Otherwise it may disrupt the communication protocol.
///
extern NCBI_XCONNECT_EXPORT
void CNetSchedule_GenerateJobKey(string*        key, 
                                  unsigned       id, 
                                  const string&  host, 
                                  unsigned short port);

/// @internal
#define NETSCHEDULE_JOBMASK "JSID_01_%u_%s_%u"


/// @internal
const unsigned int kNetScheduleMaxDataSize = 512;

/// @internal
const unsigned int kNetScheduleMaxErrSize = 1024;

extern NCBI_XCONNECT_EXPORT const char* kNetScheduleDriverName;


//extern "C" {

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetschedule(
     CPluginManager<CNetScheduleClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetScheduleClient>::EEntryPointRequest method);


//} // extern C


/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2005/05/02 14:40:20  kuznets
 * +Monitor()
 *
 * Revision 1.26  2005/04/20 15:42:09  kuznets
 * Added progress message to SubmitJob()
 *
 * Revision 1.25  2005/04/19 19:33:17  kuznets
 * Added methods to submit and receive progress messages
 *
 * Revision 1.24  2005/04/18 18:32:01  kuznets
 * Comments correction
 *
 * Revision 1.23  2005/04/11 13:50:10  kuznets
 * +Logging()
 *
 * Revision 1.22  2005/04/06 12:37:07  kuznets
 * Added support of client version control
 *
 * Revision 1.21  2005/04/01 14:21:27  kuznets
 * Added penalty to unavailable service
 *
 * Revision 1.20  2005/03/28 15:31:37  didenko
 * Made destructors virtual
 *
 * Revision 1.19  2005/03/23 21:22:54  didenko
 * Renamed EShutdownLevel values names
 *
 * Revision 1.18  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.17  2005/03/22 18:54:18  kuznets
 * Changed project tree layout
 *
 * Revision 1.16  2005/03/22 16:09:05  didenko
 * + enum EShutdownLevel
 *
 * Revision 1.15  2005/03/21 16:53:31  didenko
 * + creating from PluginManager
 *
 * Revision 1.14  2005/03/21 13:05:25  kuznets
 * +PrintStatistics
 *
 * Revision 1.13  2005/03/17 20:36:05  kuznets
 * +PutFailure()
 *
 * Revision 1.12  2005/03/17 17:18:00  kuznets
 * Implemented load-balanced client
 *
 * Revision 1.11  2005/03/15 20:12:58  kuznets
 * +SubmitJobAndWait()
 *
 * Revision 1.10  2005/03/15 14:48:14  kuznets
 * +DropJob()
 *
 * Revision 1.9  2005/03/10 14:17:36  kuznets
 * +SetRunTimeout()
 *
 * Revision 1.8  2005/03/07 17:29:35  kuznets
 * Added load-balanced client
 *
 * Revision 1.7  2005/03/04 12:04:31  kuznets
 * Implemented WaitJob() method
 *
 * Revision 1.6  2005/02/28 18:39:33  kuznets
 * +ReturnJob()
 *
 * Revision 1.5  2005/02/28 12:17:58  kuznets
 * Added new job status (Returned), + DropQueue()
 *
 * Revision 1.4  2005/02/22 16:14:00  kuznets
 * Reduced size of input data
 *
 * Revision 1.3  2005/02/10 20:00:54  kuznets
 * +GetJob(), +PutResult()
 *
 * Revision 1.2  2005/02/09 18:58:04  kuznets
 * Implemented job submission part of the API
 *
 * Revision 1.1  2005/02/07 13:02:32  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETSCHEDULE_CLIENT__HPP */
