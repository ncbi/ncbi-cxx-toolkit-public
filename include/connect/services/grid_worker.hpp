#ifndef CONNECT_SERVICES__GRID_WORKER_HPP
#define CONNECT_SERVICES__GRID_WORKER_HPP


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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Interfaces.
 *
 */

/// @file grid_worker.hpp
/// NetSchedule Framework specs. 
///

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <connect/connect_export.h>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


class CWorkerNodeJobContext;

/// Worker Node Job interface.
/// 
/// This interface is a worker node workhorse. 
/// Job is executed by method Do of this interface.
/// Worker node application may be configured to execute several jobs at once
/// using threads, so implementation must be thread safe.
///
/// @sa CWorkerNodeJobContext
///
class IWorkerNodeJob
{
public:
    virtual ~IWorkerNodeJob() {}
    /// Execute the job.
    ///
    /// Job is considered successfull if method calls 
    /// CommitJob (see CWorkerNodeJobContext).
    /// If method does not call CommitJob job is considered unresolved 
    /// and returned back to the queue.
    /// Method can throw an exception (derived from std::exception),
    /// in this case job is considered failed (error message will be
    /// redirected to the netschedule queue)
    ///
    /// @param context
    ///   Context where a job can get all requered information
    ///   like input/output steams, the job key etc.
    ///
    /// @return
    ///   Job exit code
    ///
    virtual int Do(CWorkerNodeJobContext& context) = 0;
};

class CGridWorkerNode;
class CWorkerNodeRequest;

/// Worker Node job context
///
/// Context in which a job is runnig, gives access to input and output
/// storage and some job control parameters.
///
/// @sa IWorkerNodeJob
///
class NCBI_XCONNECT_EXPORT CWorkerNodeJobContext
{
public:
    
    ~CWorkerNodeJobContext();

    /// Get a job key
    const string& GetJobKey() const        { return m_JobKey; }

    /// Get a job input string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an input data for a job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with 
    ///    an input data for the job
    ///
    const string& GetJobInput() const      { return m_JobInput; }

    /// Set a job's output. This string will be sent to
    /// the queue when job is done.
    /// 
    /// This method can be used to send a short data back to the client.
    /// To send a large result use GetOStream method. Don't call this 
    /// method after GetOStream method is called.
    ///
    void          SetJobOutput(const string& output) { m_JobOutput = output; }

    /// Get a stream with input data for a job. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined 
    /// using GetInputBlobSize
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    ///
    size_t        GetInputBlobSize() const { return m_InputBlobSize; }

    /// Get a stream where a job can write its result
    ///
    CNcbiOstream& GetOStream();

    /// Confirm that a job is done and result is ready to be sent 
    /// back to the client. 
    ///
    /// This method should be called at the end of IWorkerNodeJob::Do 
    /// method. If this method is not called the job's result will NOT be sent 
    /// to the queue (job is returned back to the NetSchedule queue 
    /// and re-executed in a while)
    ///
    void CommitJob()                       { m_JobStatus = true; }

    /// Check if node application shutdown was requested.
    ///
    /// If job takes a long time it should periodically call 
    /// this method during the execution and check if node shutdown was 
    /// requested and gracefully finish its work.
    ///
    /// Shutdown level eNormal means job can finish the job and exit,
    /// eImmidiate means node should exit immediately 
    /// (unfinished jobs are returned back to the queue)
    /// 
    CNetScheduleClient::EShutdownLevel GetShutdownLevel(void) const;

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

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
    void SetJobRunTimeout(unsigned time_to_run);

private:    
    friend class CWorkerNodeRequest;  
    void Reset();
    CGridWorkerNode& GetWorkerNode() { return m_WorkerNode; }
    bool IsJobCommited() const       { return m_JobStatus; }

    /// Only a CGridWorkerNode can create an instance of this class
    friend class CGridWorkerNode;
    CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                          const string&    job_key,
                          const string&    job_input);

    CGridWorkerNode&     m_WorkerNode;
    string               m_JobKey;
    string               m_JobInput;
    string               m_JobOutput;
    bool                 m_JobStatus;
    size_t               m_InputBlobSize;

    INetScheduleStorage* m_Reader;
    INetScheduleStorage* m_Writer;
    CNetScheduleClient*  m_Reporter;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CWorkerNodeJobContext(const CWorkerNodeJobContext&);
    CWorkerNodeJobContext& operator=(const CWorkerNodeJobContext&);
};

/// Worker Node Job Factory interface
///
/// @sa IWorkerNodeJob
///
class IWorkerNodeJobFactory
{
public:
    /// Create a job
    ///
    virtual IWorkerNodeJob* CreateInstance(void) = 0;

    /// Get the job version
    ///
    virtual string GetJobVersion(void) const = 0;
};


/// Grid Worker Node
/// 
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads (thread pool is used).
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:

    /// Construct a worker node using class factories
    ///
    CGridWorkerNode(IWorkerNodeJobFactory& job_creator,
                    INetScheduleStorageFactory& ns_storage_creator,
                    INetScheduleClientFactory& ns_client_creator);

    virtual ~CGridWorkerNode();

    /// Set a UDP port to listen to input jobs
    ///
    void SetListeningPort(unsigned int udp_port) { m_UdpPort = udp_port; }

    /// Set the maximum threads running simultaneously
    ///
    void SetMaxThreads(unsigned int max_threads) 
                      { m_MaxThreads = max_threads; }

    void SetInitThreads(unsigned int init_threads) 
                       { m_InitThreads = init_threads; }

    void SetNSTimeout(unsigned int timeout) { m_NSTimeout = timeout; }
    void SetThreadsPoolTimeout(unsigned int timeout)
                              { m_ThreadsPoolTimeout = timeout; }

    /// Start jobs execution.
    ///
    void Start();

    /// Request node shutdown
    void RequestShutdown(CNetScheduleClient::EShutdownLevel level) 
                        { m_ShutdownLevel = level; }


    /// Check if shutdown was requested.
    ///
    CNetScheduleClient::EShutdownLevel GetShutdownLevel(void) 
                        { return m_ShutdownLevel; }

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Get a job version
    ///
    string GetJobVersion() const
    {
        CMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.GetJobVersion();
    }

private:
    IWorkerNodeJobFactory&       m_JobFactory;
    INetScheduleStorageFactory&  m_NSStorageFactory;
    INetScheduleClientFactory&   m_NSClientFactory;

    auto_ptr<CNetScheduleClient> m_NSReadClient;
    auto_ptr<CStdPoolOfThreads>  m_ThreadsPool;
    unsigned int                 m_UdpPort;
    unsigned int                 m_MaxThreads;
    unsigned int                 m_InitThreads;
    unsigned int                 m_NSTimeout;
    unsigned int                 m_ThreadsPoolTimeout;
    CMutex                       m_NSClientFactoryMutex;
    mutable CMutex               m_JobFactoryMutex;
    CMutex                       m_StorageFactoryMutex;
    volatile CNetScheduleClient::EShutdownLevel m_ShutdownLevel;

    friend class CWorkerNodeRequest;
    IWorkerNodeJob* CreateJob()
    {
        CMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.CreateInstance();
    }
    INetScheduleStorage* CreateStorage()
    {
        CMutexGuard guard(m_StorageFactoryMutex);
        return  m_NSStorageFactory.CreateInstance();
    }
    CNetScheduleClient* CreateClient()
    {
        CMutexGuard guard(m_NSClientFactoryMutex);
        auto_ptr<CNetScheduleClient> 
            ns_client(m_NSClientFactory.CreateInstance());
        ns_client->SetProgramVersion(m_JobFactory.GetJobVersion());
        return ns_client.release();
    }

    CGridWorkerNode(const CGridWorkerNode&);
    CGridWorkerNode& operator=(const CGridWorkerNode&);

};

/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/04/07 16:46:28  didenko
 * + Program Version checking
 *
 * Revision 1.7  2005/03/28 16:49:00  didenko
 * Added virtual desturctors to all new interfaces to prevent memory leaks
 *
 * Revision 1.6  2005/03/25 16:24:58  didenko
 * Classes restructure
 *
 * Revision 1.5  2005/03/23 21:26:04  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.4  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.3  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.2  2005/03/22 20:35:56  didenko
 * Make it compile under CGG
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //CONNECT_SERVICES__GRID_WOKER_HPP

