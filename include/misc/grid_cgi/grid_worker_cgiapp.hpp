#ifndef __GRID_WORKER_CGIAPP_HPP
#define __GRID_WORKER_CGIAPP_HPP

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
 * Author:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <corelib/ncbimisc.hpp>
#include <cgi/cgiapp_iface.hpp>
#include <cgi/cgictx.hpp>

#include <connect/services/grid_worker.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>

#include <util/logrotate.hpp>

#include <map>

/// @file grid_worker_cgiapp.hpp
/// NetSchedule Framework specs. 
///


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CCgiWorkerNodeJob;

class CGridWorkerCgiApp : public CCgiApplication
{
public:
    CGridWorkerCgiApp( 
                   INetScheduleStorageFactory* storage_factory = NULL,
                   INetScheduleClientFactory* client_factory = NULL);


    virtual void Init(void);
    virtual int Run(void);

    void RequestShutdown(void);

    virtual string GetJobVersion() const;

protected:

    IWorkerNodeJobFactory&      GetJobFactory() { return *m_JobFactory; }
    INetScheduleStorageFactory& GetStorageFactory() 
                                           { return *m_StorageFactory; }
    INetScheduleClientFactory&  GetClientFactory()
                                           { return *m_ClientFactory; }

    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

private:
    auto_ptr<IWorkerNodeJobFactory>      m_JobFactory;
    auto_ptr<INetScheduleStorageFactory> m_StorageFactory;
    auto_ptr<INetScheduleClientFactory>  m_ClientFactory;

    auto_ptr<CGridWorkerNode>                m_WorkerNode;
    mutable auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;

    auto_ptr<CRotatingLogStream> m_ErrLog;

    friend class CCgiWorkerNodeJob;
    int RunJob(CNcbiIstream& is, CNcbiOstream& os);
    CCgiContext* CreateCgiContext(CNcbiIstream& is, CNcbiOstream& os);


};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/25 14:13:40  didenko
 * Added new Application class from easy transfer existing cgis to worker nodes
 *
 * ===========================================================================
 */


#endif //__GRID_WORKER_CGIAPP_HPP
