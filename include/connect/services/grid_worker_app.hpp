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
#include <connect/services/grid_worker_app_impl.hpp>
#include <connect/services/blob_storage_netcache.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Default implementation of a worker node initialization 
/// interface
class CDefalutWorkerNodeInitContext : public IWorkerNodeInitContext
{
public:
    CDefalutWorkerNodeInitContext(const CNcbiApplication& app)
        : m_App(app) 
    {}

    virtual ~CDefalutWorkerNodeInitContext() {}

    virtual const IRegistry&        GetConfig() const
    { return m_App.GetConfig(); }

    virtual const CArgs&            GetArgs() const
    { return m_App.GetArgs(); }

    virtual const CNcbiEnvironment& GetEnvironment() const
    { return m_App.GetEnvironment(); }

private:
    const CNcbiApplication& m_App;

    CDefalutWorkerNodeInitContext(const CDefalutWorkerNodeInitContext&);
    CDefalutWorkerNodeInitContext& operator=(const CDefalutWorkerNodeInitContext&);
};

/// Main Worker Node application
///
/// @note
/// Worker node application is parameterised using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerApp : public CNcbiApplication
{
public:

    enum ESignalHandling {
        eStandardSignalHandling = 0,
        eManualSignalHandling
    };

    CGridWorkerApp(IWorkerNodeJobFactory* job_factory, 
                   IBlobStorageFactory*   storage_factory = NULL,
                   INetScheduleClientFactory* client_factory = NULL,
                   ESignalHandling signal_handling = eStandardSignalHandling);
    virtual ~CGridWorkerApp();

    /// If you override this method, do call CGridWorkerApp::Init()
    /// from inside your overriding method.    
    virtual void Init(void);

    /// Do not override this method yourself! It includes all the Worker Node
    /// specific machinery. If you override it, do call CGridWorkerApp::Run()
    /// from inside your overriding method.
    virtual int Run(void);

    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

    void RequestShutdown();

protected:

    virtual const IWorkerNodeInitContext&  GetInitContext() const;

private:
    mutable auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;
    auto_ptr<CGridWorkerApp_Impl> m_AppImpl;

    CGridWorkerApp(const CGridWorkerApp&);
    CGridWorkerApp& operator=(const CGridWorkerApp&);
};

#define NCBI_WORKERNODE_MAIN(TWorkerNodeJob, Version)     \
NCBI_DECLARE_WORKERNODE_FACTORY(TWorkerNodeJob, Version); \
int main(int argc, const char* argv[])                    \
{                                                         \
    GetDiagContext().SetOldPostFormat(false);             \
    BlobStorage_RegisterDriver_NetCache();                \
    CGridWorkerApp app(new TWorkerNodeJob##Factory);      \
    return app.AppMain(argc, argv, NULL, eDS_ToStdlog);  \
}

#define NCBI_WORKERNODE_MAIN_EX(TWorkerNodeJob, TWorkerNodeIdleTask, Version)     \
NCBI_DECLARE_WORKERNODE_FACTORY_EX(TWorkerNodeJob, TWorkerNodeIdleTask, Version); \
int main(int argc, const char* argv[])                                            \
{                                                                                 \
    GetDiagContext().SetOldPostFormat(false);                                     \
    BlobStorage_RegisterDriver_NetCache();                                        \
    CGridWorkerApp app(new TWorkerNodeJob##FactoryEx);                            \
    return app.AppMain(argc, argv, NULL, eDS_ToStdlog);                          \
}

/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2006/11/30 15:33:33  didenko
 * Moved to a new log system
 *
 * Revision 1.14  2006/04/04 20:14:04  didenko
 * Disabled copy constractors and assignment operators
 *
 * Revision 1.13  2006/02/27 14:50:20  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 1.12  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 1.11  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.10  2005/05/25 18:52:37  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * Revision 1.9  2005/05/12 18:35:46  vakatov
 * Minor warning heeded
 *
 * Revision 1.8  2005/04/27 15:16:29  didenko
 * Added rotating log
 * Added optional deamonize
 *
 * Revision 1.7  2005/04/21 20:15:52  didenko
 * Added some comments
 *
 * Revision 1.6  2005/04/21 19:10:01  didenko
 * Added IWorkerNodeInitContext
 * Added some convenient macros
 *
 * Revision 1.5  2005/03/28 14:38:49  didenko
 * Removed signal handling
 *
 * Revision 1.4  2005/03/25 16:24:58  didenko
 * Classes restructure
 *
 * Revision 1.3  2005/03/23 21:26:04  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.2  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */



#endif //CONNECT_SERVICES__GRID_WORKER_APP_HPP
