#ifndef CONNECT_SERVICES__GRID_WORKER_APP_HPP
#define CONNECT_SERVICES__GRID_WORKER_APP_HPP


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
 *   NetSchedule Worker Node Framework Application.
 *
 */

/// @file grid_worker_app.hpp
/// NetSchedule worker node application. 
///


#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/plugin_manager.hpp>
#include <connect/connect_export.h>
#include <connect/services/grid_worker.hpp>
#include <connect/services/netcache_client.hpp>
#include <connect/services/netschedule_client.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


/// Base worker node application
///
class NCBI_XCONNECT_EXPORT CGridWorkerApp 
    : public CNcbiApplication,
      protected IWorkerNodeJobFactory,
      protected INetScheduleClientFactory,
      protected INetScheduleStorageFactory
{
public:

    CGridWorkerApp();
    virtual ~CGridWorkerApp();

    virtual int Run(void);

    void RequestShutdown();

    void HandleSignals(bool on_off) { m_HandleSignals = on_off; }

protected:

    /// IWorkerNodeJobFactory interface, should be overloaded
    /// to create job execution units.
    /// 
    virtual IWorkerNodeJob* CreateJob(void) = 0;

    /// Get the worker node version
    ///
    virtual string GetJobVersion(void) const = 0;

    /// INetScheduleClientFactory interface
    virtual CNetScheduleClient* CreateClient(void);

    /// INetScheduleStorageFactory interface
    /// Elements on netschedule infrastructure can store job input and
    /// output information using different storage options.
    /// The default is NetCache, but potentially we may want to use
    /// SQL server
    ///
    virtual INetScheduleStorage* CreateStorage(void) = 0;

private:
    typedef CPluginManager<CNetScheduleClient> TPMNetSchedule;
    TPMNetSchedule            m_PM_NetSchedule;

    auto_ptr<CGridWorkerNode> m_WorkerNode;

    bool                      m_HandleSignals;

};

/// Main worker node application
///
/// This application uses NetCache as a job network storage.
/// 
/// @note
/// Worker node application is parameterised using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerNCSApp : public CGridWorkerApp
{
public:
    CGridWorkerNCSApp();

protected:
    /// Override this function to implement worker node functionality
    virtual IWorkerNodeJob* CreateJob(void) = 0;

protected:
    virtual INetScheduleStorage* CreateStorage(void);

private:
    typedef CPluginManager<CNetCacheClient>    TPMNetCache;
    TPMNetCache                                m_PM_NetCache;
};


/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */



#endif //CONNECT_SERVICES__GRID_WORKER_APP_HPP
