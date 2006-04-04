#ifndef CONNECT_SERVICES__GRID_WORKER_APP_IMPL_HPP
#define CONNECT_SERVICES__GRID_WORKER_APP_IMPL_HPP


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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Application.
 *
 */

/// @file grid_worker_app.hpp
/// NetSchedule worker node application. 
///


#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/connect_export.h>
#include <connect/services/grid_worker.hpp>
#include <util/logrotate.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class NCBI_XCONNECT_EXPORT CWorkerNodeStatistics : public IWorkerNodeJobWatcher
{
public:
    CWorkerNodeStatistics(unsigned int max_jobs_allowed, 
                          unsigned int max_failures_allowed);
    virtual ~CWorkerNodeStatistics();
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event);

    void Print(CNcbiOstream& os) const;
    unsigned int GetJobsRunningNumber() const { return m_ActiveJobs.size(); }

    const CTime& GetStartTime() const { return m_StartTime; }
private:        
    unsigned int m_JobsStarted;
    unsigned int m_JobsSucceed;
    unsigned int m_JobsFailed;
    unsigned int m_JobsReturned;
    unsigned int m_JobsCanceled;
    unsigned int m_JobsLost;
    const CTime  m_StartTime;
    const unsigned int m_MaxJobsAllowed;
    const unsigned int m_MaxFailuresAllowed;

    typedef map<const CWorkerNodeJobContext*, CTime> TActiveJobs;
    TActiveJobs    m_ActiveJobs;
    mutable CMutex m_ActiveJobsMutex;
};


class CWorkerNodeJobWatchers;
class CWorkerNodeIdleThread;
/// Main Worker Node application
///
/// @note
/// Worker node application is parameterised using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerApp_Impl
{
public:

    CGridWorkerApp_Impl(CNcbiApplication& app,
                        IWorkerNodeJobFactory* job_factory, 
                        IBlobStorageFactory*   storage_factory = NULL,
                        INetScheduleClientFactory* client_factory = NULL);

    ~CGridWorkerApp_Impl();

    void Init();
    int Run();
    void RequestShutdown();

    void ForceSingleThread(){ m_SingleThreadForced = true; }

    void AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher, 
                          EOwnership owner = eNoOwnership);


    IWorkerNodeJobFactory&      GetJobFactory() { return *m_JobFactory; }
    IBlobStorageFactory& GetStorageFactory()    { return *m_StorageFactory; }
    INetScheduleClientFactory&  GetClientFactory()
                                                { return *m_ClientFactory; }

private:
    auto_ptr<IWorkerNodeJobFactory>      m_JobFactory;
    auto_ptr<IBlobStorageFactory>        m_StorageFactory;
    auto_ptr<INetScheduleClientFactory>  m_ClientFactory;
    auto_ptr<CWorkerNodeJobWatchers>     m_JobWatchers;

    auto_ptr<CGridWorkerNode>                m_WorkerNode;
    mutable auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;

    CRef<CWorkerNodeIdleThread>  m_IdleThread;

    auto_ptr<CRotatingLogStream> m_ErrLog;
    CNcbiApplication& m_App;
    bool m_SingleThreadForced;
    auto_ptr<CWorkerNodeStatistics> m_Statistics;

    string GetLogName(void) const;
};


class NCBI_XCONNECT_EXPORT CGridWorkerAppException : public CException
{
public:
    enum EErrCode {
        eJobFactoryIsNotSet
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eJobFactoryIsNotSet: return "eJobFactoryIsNotSetError";
        default:      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridWorkerAppException, CException);
};


/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/04/04 19:15:01  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 1.5  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 1.4  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 1.3  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.2  2005/07/26 15:25:00  didenko
 * Added logging type parameter
 *
 * Revision 1.1  2005/05/25 18:52:37  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * ===========================================================================
 */



#endif //CONNECT_SERVICES__GRID_WORKER_APP_IMPL_HPP
