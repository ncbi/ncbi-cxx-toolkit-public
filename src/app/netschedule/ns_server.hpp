#ifndef NETSCHEDULE_SERVER__HPP
#define NETSCHEDULE_SERVER__HPP

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

#include <string>
#include <connect/server.hpp>

#include "ns_server_misc.hpp"
#include "ns_server_params.hpp"
#include "ns_queue.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;


//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CServer
{
public:
    CNetScheduleServer();
    virtual ~CNetScheduleServer();

    void AddDefaultListener(IServer_ConnectionFactory* factory);
    void SetNSParameters(const SNS_Parameters &  new_params,
                         bool                    log_only);

    virtual bool ShutdownRequested(void);

    void SetQueueDB(CQueueDataBase* qdb);

    void SetShutdownFlag(int signum = 0);

    const bool &  IsLog() const                       { return m_LogFlag; }
    const bool &  IsLogBatchEachJob() const           { return m_LogBatchEachJobFlag; }
    const bool &  IsLogNotificationThread() const     { return m_LogNotificationThreadFlag; }
    const bool &  IsLogCleaningThread() const         { return m_LogCleaningThreadFlag; }
    const bool &  IsLogExecutionWatcherThread() const { return m_LogExecutionWatcherThreadFlag; }
    const bool &  IsLogStatisticsThread() const       { return m_LogStatisticsThreadFlag; }

    unsigned GetCommandNumber();

    // Queue handling
    unsigned Configure(const IRegistry& reg);
    unsigned CountActiveJobs() const;
    CRef<CQueue> OpenQueue(const std::string& name);
    void CreateQueue(const std::string& qname,
                     const std::string& qclass,
                     const std::string& comment);
    void DeleteQueue(const std::string& qname);
    void QueueInfo(const std::string& qname,
                   int&               kind,
                   std::string*       qclass,
                   std::string*       comment);
    std::string GetQueueNames(const std::string& sep) const;
    void PrintMutexStat(CNcbiOstream& out);
    void PrintLockStat(CNcbiOstream& out);
    void PrintMemStat(CNcbiOstream& out);


    unsigned GetInactivityTimeout(void) const   { return m_InactivityTimeout; }
    std::string& GetHost()                      { return m_Host; }
    unsigned GetPort() const                    { return m_Port; }
    unsigned GetDeleteBatchSize(void) const     { return m_DeleteBatchSize; }
    unsigned GetScanBatchSize(void) const       { return m_ScanBatchSize; }
    double   GetPurgeTimeout(void) const        { return m_PurgeTimeout; }
    unsigned GetHostNetAddr() const             { return m_HostNetAddr; }
    const CTime& GetStartTime(void) const       { return m_StartTime; }
    CBackgroundHost&  GetBackgroundHost()       { return m_BackgroundHost; }
    CRequestExecutor& GetRequestExecutor()      { return m_RequestExecutor; }

    bool AdminHostValid(unsigned host) const;

    static CNetScheduleServer*  GetInstance(void);

protected:
    virtual void Exit();

private:
    /// API for background threads
    CNetScheduleBackgroundHost                  m_BackgroundHost;
    CNetScheduleRequestExecutor                 m_RequestExecutor;
    /// Host name where server runs
    std::string                                 m_Host;
    unsigned                                    m_Port;
    unsigned                                    m_HostNetAddr;
    bool                                        m_Shutdown;
    int                                         m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned                                    m_InactivityTimeout;
    CQueueDataBase*                             m_QueueDB;
    CTime                                       m_StartTime;

    /// Log related flags
    bool                                        m_LogFlag;
    bool                                        m_LogBatchEachJobFlag;
    bool                                        m_LogNotificationThreadFlag;
    bool                                        m_LogCleaningThreadFlag;
    bool                                        m_LogExecutionWatcherThreadFlag;
    bool                                        m_LogStatisticsThreadFlag;

    // Purge() related parameters
    unsigned int                                m_DeleteBatchSize;  // Max number of jobs to be deleted
    unsigned int                                m_ScanBatchSize;    // Max number of scanned jobs
    double                                      m_PurgeTimeout;     // Interval between purges

    /// Quick local timer
    CFastLocalTime                              m_LocalTimer;
    /// List of admin stations
    CNetSchedule_AccessList                     m_AdminHosts;

    CAtomicCounter                              m_AtomicCommandNumber;

    static CNetScheduleServer*                  sm_netschedule_server;
};


END_NCBI_SCOPE

#endif

