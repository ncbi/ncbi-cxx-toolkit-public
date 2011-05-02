#ifndef NETSCHEDULE_QUEUE_COLL__HPP
#define NETSCHEDULE_QUEUE_COLL__HPP


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
 * File Description:
 *   NetSchedule queue collection and database.
 *
 */


/// @file queue_coll.hpp
/// NetSchedule queue collection and database.
///
/// @internal


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <connect/services/netschedule_api.hpp>
#include "ns_util.hpp"
#include "ns_format.hpp"
#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "notif_thread.hpp"
#include "ns_queue.hpp"
#include "queue_vc.hpp"
#include "background_host.hpp"
#include "worker_node.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;
class CNetScheduleServer;


/// Queue database manager
///
/// @internal
///
class CQueueIterator;
class CQueueConstIterator;
class CQueueCollection
{
public:
    typedef map<string, CRef<CQueue> > TQueueMap;
    typedef CQueueIterator iterator;
    typedef CQueueConstIterator const_iterator;

public:
    CQueueCollection(const CQueueDataBase& db);
    ~CQueueCollection();

    void Close();

    CRef<CQueue> GetQueue(const string& qname) const;
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    CQueue& AddQueue(const string& name, CQueue* queue);
    // Remove queue from collection, notifying every interested party
    // through week reference mechanism.
    bool RemoveQueue(const string& name);

    // Iteration over queue map
    CQueueIterator begin();
    CQueueIterator end();
    CQueueConstIterator begin() const;
    CQueueConstIterator end() const;

    size_t GetSize() const { return m_QMap.size(); }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);
private:
    friend class CQueueIterator;
    friend class CQueueConstIterator;
    const CQueueDataBase&  m_QueueDataBase;
    TQueueMap              m_QMap;
    mutable CRWLock        m_Lock;
};



class CQueueIterator
{
public:
    CQueueIterator(CQueueCollection::TQueueMap::iterator iter,
                   CRWLock* lock)
    : m_Iter(iter), m_Lock(lock)
    {
        if (m_Lock) m_Lock->ReadLock();
    }
    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueIterator(const CQueueIterator& rhs)
    : m_Iter(rhs.m_Iter),
        m_Lock(rhs.m_Lock)
    {
        // Linear on lock
        if (m_Lock) rhs.m_Lock = 0;
    }
    ~CQueueIterator()
    {
        if (m_Lock) m_Lock->Unlock();
    }
    CQueue& operator*()
    {
        return *m_Iter->second;
    }
    const string GetName() const 
    {
        return m_Iter->first;
    }
    void operator++()
    {
        ++m_Iter;
    }
    bool operator==(const CQueueIterator& rhs) {
        return m_Iter == rhs.m_Iter;
    }
    bool operator!=(const CQueueIterator& rhs) {
        return m_Iter != rhs.m_Iter;
    }
private:
    CQueueCollection::TQueueMap::iterator m_Iter;
    mutable CRWLock*                      m_Lock;
};


class CQueueConstIterator
{
public:
    CQueueConstIterator(CQueueCollection::TQueueMap::const_iterator iter,
        CRWLock* lock)
        : m_Iter(iter), m_Lock(lock)
    {
        if (m_Lock) m_Lock->ReadLock();
    }
    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueConstIterator(const CQueueConstIterator& rhs)
        : m_Iter(rhs.m_Iter),
        m_Lock(rhs.m_Lock)
    {
        // Linear on lock
        if (m_Lock) rhs.m_Lock = 0;
    }
    ~CQueueConstIterator()
    {
        if (m_Lock) m_Lock->Unlock();
    }
    const CQueue& operator*() const
    {
        return *m_Iter->second;
    }
    const string GetName() const 
    {
        return m_Iter->first;
    }
    void operator++()
    {
        ++m_Iter;
    }
    bool operator==(const CQueueConstIterator& rhs) {
        return m_Iter == rhs.m_Iter;
    }
    bool operator!=(const CQueueConstIterator& rhs) {
        return m_Iter != rhs.m_Iter;
    }
private:
    CQueueCollection::TQueueMap::const_iterator m_Iter;
    mutable CRWLock*                      m_Lock;
};


struct SNSDBEnvironmentParams
{
    string    db_storage_ver;      ///< Version of DB schema, should match
                                   ///  NETSCHEDULED_STORAGE_VERSION
    string    db_path;
    string    db_log_path;
    unsigned  max_queues;          ///< Number of pre-allocated queues
    unsigned  cache_ram_size;      ///< Size of database cache
    unsigned  mutex_max;           ///< Number of mutexes
    unsigned  max_locks;           ///< Number of locks
    unsigned  max_lockers;         ///< Number of lockers
    unsigned  max_lockobjects;     ///< Number of lock objects
    ///    Size of in-memory LOG (when 0 log is put to disk)
    ///    In memory LOG is not durable, put it to memory
    ///    only if you need better performance
    unsigned  log_mem_size;
    unsigned  max_trans;           ///< Maximum number of active transactions
    unsigned  checkpoint_kb;
    unsigned  checkpoint_min;
    bool      sync_transactions;
    bool      direct_db;
    bool      direct_log;
    bool      private_env;

    bool Read(const IRegistry& reg, const string& sname);

    unsigned GetNumParams() const;
    string GetParamName(unsigned n) const;
    string GetParamValue(unsigned n) const;
};

/// Top level queue database.
/// (Thread-Safe, synchronized.)
///
/// @internal
///
class CQueueDataBase
{
public:
    CQueueDataBase(CNetScheduleServer* server);
    ~CQueueDataBase();

    /// @param params
    ///    Parameters of DB environment
    /// @param reinit
    ///    Whether to clean out database
    /// @return whether it was successful
    bool Open(const SNSDBEnvironmentParams& params, bool reinit);

    // Read queue information from registry and configure queues
    // accordingly.
    // returns minimum run timeout, necessary for watcher thread
    unsigned Configure(const IRegistry& reg);

    // Count Pending and Running jobs in all queues
    unsigned CountActiveJobs() const;

    CRef<CQueue> OpenQueue(const string& name);

    typedef CQueue::TQueueKind TQueueKind;
    void MountQueue(const string& qname,
                    const string& qclass,
                    TQueueKind    kind,
                    const SQueueParameters& params,
                    SQueueDbBlock* queue_db_block);

    void CreateQueue(const string& qname, const string& qclass,
                     const string& comment = "");
    void DeleteQueue(const string& qname);
    void QueueInfo(const string& qname, int& kind,
                   string* qclass, string* comment);
    string GetQueueNames(const string& sep) const;

    void UpdateQueueParameters(const string& qname,
                               const SQueueParameters& params);

    void Close(void);
    bool QueueExists(const string& qname) const
                { return m_QueueCollection.QueueExists(qname); }

    /// Remove old jobs
    void Purge(void);
    void StopPurge(void);
    void RunPurgeThread(void);
    void StopPurgeThread(void);

    /// Notify all listeners
    void NotifyListeners(void);
    void RunNotifThread(void);
    void StopNotifThread(void);

    void CheckExecutionTimeout(void);
    void RunExecutionWatcherThread(unsigned run_delay);
    void StopExecutionWatcherThread(void);


    void SetUdpPort(unsigned short port) { m_UdpPort = port; }
    unsigned short GetUdpPort(void) const { return m_UdpPort; }

    /// Force transaction checkpoint
    void TransactionCheckPoint(bool clean_log=false);

    // BerkeleyDB-specific statistics
    void PrintMutexStat(CNcbiOstream& out) {
        m_Env->PrintMutexStat(out);
    }
    void PrintLockStat(CNcbiOstream& out) {
        m_Env->PrintLockStat(out);
    }
    void PrintMemStat(CNcbiOstream& out) {
        m_Env->PrintMemStat(out);
    }


private:
    // No copy
    CQueueDataBase(const CQueueDataBase&);
    CQueueDataBase& operator=(const CQueueDataBase&);

protected:
    /// get next job id (counter increment)
    unsigned GetNextId();

    /// Returns first id for the batch
    unsigned GetNextIdBatch(unsigned count);

private:
    int x_AllocateQueue(const string& qname, const string& qclass,
                        int kind, const string& comment);

    unsigned x_PurgeUnconditional(unsigned batch_size);
    void x_OptimizeStatusMatrix(void);
    bool x_CheckStopPurge(void);
    void x_CleanParamMap(void);

    CBackgroundHost&                m_Host;
    CRequestExecutor&               m_Executor;
    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    mutable CFastMutex              m_ConfigureLock;
    typedef map<string, SQueueParameters*> TQueueParamMap;
    TQueueParamMap                  m_QueueParamMap;
    SQueueDescriptionDB             m_QueueDescriptionDB;
    CQueueCollection                m_QueueCollection;
    
    // Pre-allocated Berkeley DB blocks
    CQueueDbBlockArray              m_QueueDbBlockArray;

    CRef<CJobQueueCleanerThread>    m_PurgeThread;

    bool                 m_StopPurge;         ///< Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned             m_FreeStatusMemCnt;  ///< Free memory counter
    time_t               m_LastFreeMem;       ///< time of the last memory opt
    unsigned short       m_UdpPort;           ///< UDP notification port

    CRef<CJobNotificationThread>             m_NotifThread;
    CRef<CJobQueueExecutionWatcherThread>    m_ExeWatchThread;
    CNetScheduleServer*                      m_Server;

}; // CQueueDataBase


END_NCBI_SCOPE

#endif /* NETSCHEDULE_QUEUE_COLL__HPP */
