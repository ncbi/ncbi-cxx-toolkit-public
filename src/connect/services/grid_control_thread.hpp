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

#include <connect/threaded_server.hpp>
#include <connect/services/grid_worker.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// 
/// @internal
class CWorkerNodeControlThread : public CThreadedServer
{
public:
    CWorkerNodeControlThread(unsigned int port, 
                             CGridWorkerNode& worker_node);

    virtual ~CWorkerNodeControlThread();

    virtual void Process(SOCK sock);

    virtual bool ShutdownRequested(void) 
    {
        return m_WorkerNode.GetShutdownLevel() 
                       != CNetScheduleClient::eNoShutdown;
    }

private:
    CGridWorkerNode& m_WorkerNode;
    STimeout         m_ThrdSrvAcceptTimeout;

};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2005/05/10 15:42:53  didenko
 * Moved grid worker control thread to its own file
 *
 * ===========================================================================
 */
 
#endif // _GRID_CONTROL_THREADD_HPP_
