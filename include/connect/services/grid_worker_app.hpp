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
#include <connect/services/grid_worker.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


/// Main Worker Node application
///
/// @note
/// Worker node application is parameterised using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerApp : public CNcbiApplication
{
public:

    CGridWorkerApp(IWorkerNodeJobFactory* job_factory, 
                   INetScheduleStorageFactory* storage_factory = NULL,
                   INetScheduleClientFactory* client_factory = NULL);
    virtual ~CGridWorkerApp();

    /// If you override this method, do call CGridWorkerApp::Init()
    /// from inside your overriding method.    
    virtual void Init(void);

    /// Do not override this method yourself! It includes all the Worker Node
    /// specific machinery. If you override it, do call CGridWorkerApp::Run()
    /// from inside your overriding method.
    virtual int Run(void);

    void RequestShutdown();

protected:

    IWorkerNodeJobFactory&      GetJobFactory() { return *m_JobFactory; }
    INetScheduleStorageFactory& GetStorageFactory() 
                                           { return *m_StorageFactory; }
    INetScheduleClientFactory&  GetClientFactory()
                                           { return *m_ClientFactory; }

private:
    auto_ptr<IWorkerNodeJobFactory>      m_JobFactory;
    auto_ptr<INetScheduleStorageFactory> m_StorageFactory;
    auto_ptr<INetScheduleClientFactory>  m_ClientFactory;

    auto_ptr<CGridWorkerNode> m_WorkerNode;

};

class NCBI_XCONNECT_EXPORT CGridWorkerAppException : public CException
{
public:
    enum EErrCode {
        eJobFactoryIsNotSet,
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
