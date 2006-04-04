#ifndef _GRID_CONTROL_THREADD_HPP_
#define _GRID_CONTROL_THREADD_HPP_


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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <corelib/ncbimisc.hpp>
#include <connect/threaded_server.hpp>

#include <map>

BEGIN_NCBI_SCOPE

class CGridWorkerNode;
class CWorkerNodeStatistics;
/////////////////////////////////////////////////////////////////////////////
// 
/// @internal
class NCBI_XCONNECT_EXPORT CWorkerNodeControlThread : public CThreadedServer
{
public:

    class IRequestProcessor 
    {
    public:
        virtual ~IRequestProcessor() {}

        virtual bool Authenticate(const string& host,
                                  const string& auth, 
                                  const string& queue,
                                  CNcbiOstream& replay,
                                  const CGridWorkerNode& node) { return true; }

        virtual void Process(const string& request,
                             CNcbiOstream& replay,
                             CGridWorkerNode& node) = 0;
    };
    CWorkerNodeControlThread(unsigned int port, 
                             CGridWorkerNode& worker_node,
                             const CWorkerNodeStatistics& stat);

    CGridWorkerNode& GetWorkerNode() { return m_WorkerNode; }

    virtual ~CWorkerNodeControlThread();

    virtual void Process(SOCK sock);

    virtual bool ShutdownRequested(void) 
    {
        return m_ShutdownRequested;
    }

    void RequestShutdown() { m_ShutdownRequested = true; }

private:
    CGridWorkerNode& m_WorkerNode;
    STimeout         m_ThrdSrvAcceptTimeout;
    volatile bool    m_ShutdownRequested;

    typedef map<string, AutoPtr<IRequestProcessor> > TProcessorCont;
    TProcessorCont m_Processors;

    CWorkerNodeControlThread(const CWorkerNodeControlThread&);
    CWorkerNodeControlThread& operator=(const CWorkerNodeControlThread&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/04/04 20:14:04  didenko
 * Disabled copy constractors and assignment operators
 *
 * Revision 1.3  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 1.2  2005/05/27 12:55:11  didenko
 * Added IRequestProcessor interface and Processor classes implementing this interface
 *
 * Revision 1.1  2005/05/23 15:51:13  didenko
 * Moved grid_control_thread.hpp grid_debug_context.hpp from
 * srv/connect/service
 *
 * Revision 6.2  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 6.1  2005/05/10 15:42:53  didenko
 * Moved grid worker control thread to its own file
 *
 * ===========================================================================
 */
 
#endif // _GRID_CONTROL_THREADD_HPP_
