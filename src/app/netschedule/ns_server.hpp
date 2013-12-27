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

const unsigned int   kSubmitCounterInitialValue = 1000000000;


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
    string SetNSParameters(const SNS_Parameters &  new_params,
                           bool                    limited);

    virtual bool ShutdownRequested(void);

    void SetQueueDB(CQueueDataBase* qdb);

    void SetShutdownFlag(int signum = 0);

    const bool &  IsLog() const                       { return m_LogFlag; }
    const bool &  IsLogBatchEachJob() const           { return m_LogBatchEachJobFlag; }
    const bool &  IsLogNotificationThread() const     { return m_LogNotificationThreadFlag; }
    const bool &  IsLogCleaningThread() const         { return m_LogCleaningThreadFlag; }
    const bool &  IsLogExecutionWatcherThread() const { return m_LogExecutionWatcherThreadFlag; }
    const bool &  IsLogStatisticsThread() const       { return m_LogStatisticsThreadFlag; }
    const unsigned int &  GetStatInterval() const     { return m_StatInterval; }

    unsigned GetCommandNumber();

    // Queue handling
    unsigned int  Configure(const IRegistry &  reg,
                            string &           diff);
    unsigned CountActiveJobs() const;
    CRef<CQueue> OpenQueue(const string &  name);
    void CreateDynamicQueue(const CNSClientId &  client,
                            const string &  qname,
                            const string &  qclass,
                            const string &  description);
    void DeleteDynamicQueue(const CNSClientId &  client,
                            const string &  qname);
    SQueueParameters  QueueInfo(const string &  qname) const;
    string  GetQueueNames(const string &  sep) const;
    void PrintMutexStat(CNcbiOstream& out);
    void PrintLockStat(CNcbiOstream& out);
    void PrintMemStat(CNcbiOstream& out);
    string PrintTransitionCounters(void);
    string PrintJobsStat(void);
    string GetQueueClassesInfo(void) const;
    string GetQueueClassesConfig(void) const;
    string GetQueueInfo(void) const;
    string GetQueueConfig(void) const;
    string GetNetcacheApiSectionConfig(void) const;

    bool GetRefuseSubmits() const               { return m_RefuseSubmits; }
    void SetRefuseSubmits(bool  val)            { m_RefuseSubmits = val;  }

    unsigned GetInactivityTimeout(void) const   { return m_InactivityTimeout; }
    string & GetHost()                          { return m_Host; }
    unsigned GetPort() const                    { return m_Port; }
    unsigned GetDeleteBatchSize(void) const     { return m_DeleteBatchSize; }
    bool GetUseHostname(void) const             { return m_UseHostname; }
    unsigned GetMarkdelBatchSize(void) const    { return m_MarkdelBatchSize; }
    unsigned GetScanBatchSize(void) const       { return m_ScanBatchSize; }
    double   GetPurgeTimeout(void) const        { return m_PurgeTimeout; }
    unsigned GetHostNetAddr() const             { return m_HostNetAddr; }
    const CTime& GetStartTime(void) const       { return m_StartTime; }
    CBackgroundHost&  GetBackgroundHost()       { return m_BackgroundHost; }
    CRequestExecutor& GetRequestExecutor()      { return m_RequestExecutor; }
    unsigned GetMaxAffinities(void) const       { return m_MaxAffinities; }
    string GetNodeID(void) const                { return m_NodeID; }
    string GetSessionID(void) const             { return m_SessionID; }
    unsigned GetAffinityHighMarkPercentage(void) const  { return m_AffinityHighMarkPercentage; }
    unsigned GetAffinityLowMarkPercentage(void) const   { return m_AffinityLowMarkPercentage; }
    unsigned GetAffinityHighRemoval(void) const         { return m_AffinityHighRemoval; }
    unsigned GetAffinityLowRemoval(void) const          { return m_AffinityLowRemoval; }
    unsigned GetAffinityDirtPercentage(void) const      { return m_AffinityDirtPercentage; }
    bool IsDrainShutdown(void) const                   { return m_CurrentSubmitsCounter.Get() < kSubmitCounterInitialValue; }
    void SetDrainShutdown(void)
    { m_CurrentSubmitsCounter.Add(-1*static_cast<int>(kSubmitCounterInitialValue)); }
    unsigned int  GetCurrentSubmitsCounter(void)        { return m_CurrentSubmitsCounter.Get(); }
    unsigned int  IncrementCurrentSubmitsCounter(void)  { return m_CurrentSubmitsCounter.Add(1); }
    unsigned int  DecrementCurrentSubmitsCounter(void)  { return m_CurrentSubmitsCounter.Add(-1); }

    bool AdminHostValid(unsigned host) const;
    bool IsAdminClientName(const string &  name) const;

    void InitNodeID(const string &  db_path);

    static CNetScheduleServer*  GetInstance(void);
    const CNetScheduleAccessList &  GetAdminHosts(void) const { return m_AdminHosts; }
    string GetAdminClientNames(void) const;
    CCompoundIDPool GetCompoundIDPool(void) const { return m_CompoundIDPool; }

protected:
    virtual void Exit();

private:
    // API for background threads
    CNetScheduleBackgroundHost                  m_BackgroundHost;
    CNetScheduleRequestExecutor                 m_RequestExecutor;
    // Host name where server runs
    string                                      m_Host;
    unsigned                                    m_Port;
    unsigned                                    m_HostNetAddr;
    mutable bool                                m_Shutdown;
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

    bool                                        m_RefuseSubmits;
    bool                                        m_UseHostname;

    // Support for shutdown with drain
    CAtomicCounter                              m_CurrentSubmitsCounter;


    // Purge() related parameters
    unsigned int                                m_DeleteBatchSize;  // Max # of jobs to be deleted
    unsigned int                                m_MarkdelBatchSize; // Max # of jobs to be marked for deletion
    unsigned int                                m_ScanBatchSize;    // Max # of scanned jobs
    double                                      m_PurgeTimeout;     // Interval between purges
    unsigned int                                m_StatInterval;

    unsigned int                                m_MaxAffinities;
    string                                      m_NodeID;           // From the ini file
    string                                      m_SessionID;        // Generated

    // Affinity garbage collection settings
    unsigned int                                m_AffinityHighMarkPercentage;
    unsigned int                                m_AffinityLowMarkPercentage;
    unsigned int                                m_AffinityHighRemoval;
    unsigned int                                m_AffinityLowRemoval;
    unsigned int                                m_AffinityDirtPercentage;

    // List of admin stations
    CNetScheduleAccessList                      m_AdminHosts;

    CAtomicCounter                              m_AtomicCommandNumber;

    static CNetScheduleServer*                  sm_netschedule_server;

    mutable CRWLock                             m_AdminClientsLock;
    vector<string>                              m_AdminClientNames;

    CCompoundIDPool                             m_CompoundIDPool;

private:
    string  x_GenerateGUID(void) const;
    string  x_SetAdminClientNames(const string &  client_names);
};


END_NCBI_SCOPE

#endif

