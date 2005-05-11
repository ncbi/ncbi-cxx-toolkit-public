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

#include <ncbi_pch.hpp>
#include <connect/services/grid_worker.hpp>
#include "grid_control_thread.hpp"

BEGIN_NCBI_SCOPE

CWorkerNodeControlThread::CWorkerNodeControlThread(
                                                unsigned int port, 
                                                CGridWorkerNode& worker_node)
    : CThreadedServer(port), m_WorkerNode(worker_node), 
      m_ShutdownRequested(false)
{
    m_InitThreads = 1;
    m_MaxThreads = 3;
    m_ThrdSrvAcceptTimeout.sec = 0;
    m_ThrdSrvAcceptTimeout.usec = 500;
    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
}

CWorkerNodeControlThread::~CWorkerNodeControlThread()
{
    LOG_POST(Info << "Control server stopped.");
}

#define JS_CHECK_IO_STATUS(x) \
        switch (x)  { \
        case eIO_Success: \
            break; \
        default: \
            return; \
        } 

const string SHUTDOWN_CMD = "SHUTDOWN";
const string VERSION_CMD = "VERSION";
const string STAT_CMD = "STAT";

void CWorkerNodeControlThread::Process(SOCK sock)
{
    EIO_Status io_st;
    CSocket socket;
    try {
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
        socket.DisableOSSendDelay();
        
        string auth;
        io_st = socket.ReadLine(auth);
        JS_CHECK_IO_STATUS(io_st);
        
        string queue;
        io_st = socket.ReadLine(queue);
        JS_CHECK_IO_STATUS(io_st);

        string request;
        io_st = socket.ReadLine(request);
        JS_CHECK_IO_STATUS(io_st);
        
        if( strncmp( request.c_str(), SHUTDOWN_CMD.c_str(), 
                     SHUTDOWN_CMD.length() ) == 0 ) {
            m_WorkerNode.RequestShutdown(CNetScheduleClient::eNormalShutdown);
            string ans = "OK:";
            socket.Write(ans.c_str(), ans.length() + 1 );
            LOG_POST(Info << "Shutdown request has been received.");
        }
        else if( strncmp( request.c_str(), VERSION_CMD.c_str(), 
                     VERSION_CMD.length() ) == 0 ) {
            string ans = "OK:" + m_WorkerNode.GetJobVersion();
            socket.Write(ans.c_str(), ans.length() + 1 );
        } 
        else if( strncmp( request.c_str(), STAT_CMD.c_str(), 
                     STAT_CMD.length() ) == 0 ) {
            string ans = "OK:";
            ans += m_WorkerNode.GetJobVersion();
            if (m_WorkerNode.GetShutdownLevel() != 
                CNetScheduleClient::eNoShutdown) {
                ans += " IS IN A SHUTTING DOWN MODE!!!";
            }
            ans += "\n";
            ans += "Maximum job threads: " + 
                NStr::UIntToString(m_WorkerNode.GetMaxThreads()) + "\n";
            ans += "Queue name: " + m_WorkerNode.GetQueueName() + "\n";
            ans += "Jobs Succeed: " + 
                NStr::UIntToString(m_WorkerNode.GetJobsSucceedNumber()) + "\n";
            ans += "Jobs Failed: " + 
                NStr::UIntToString(m_WorkerNode.GetJobsFailedNumber()) + "\n";
            ans += "Jobs Returned: " + 
                NStr::UIntToString(m_WorkerNode.GetJobsReturnedNumber()) + "\n";
            ans += "Jobs Running: " + 
                NStr::UIntToString(m_WorkerNode.GetJobsRunningNumber()) + "\n";
            vector<CWorkerNodeJobContext::SJobStat> jobs;
            CWorkerNodeJobContext::CollectStatictics(jobs);
            CTime now(CTime::eCurrent);
            ITERATE(vector<CWorkerNodeJobContext::SJobStat>, it, jobs) {
                CTimeSpan ts = now - it->start_time.GetLocalTime();
                string line = it->job_key + " " + it->job_input;
                line += " -- running for " + ts.AsString("S") + " seconds.\n";
                ans += line;
            }

            socket.Write(ans.c_str(), ans.length() + 1 );
        }
        else {
            string ans = "ERR: Unknown command -- " + request;
            socket.Write(ans.c_str(), ans.length() + 1 );
        }
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "Exception in the control server : " << ex.what() );
        string err = "ERR:" + NStr::PrintableString(ex.what());
        socket.Write(err.c_str(), err.length() + 1 );     
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2005/05/11 20:22:43  didenko
 * Added a shutdown message to a statistic
 *
 * Revision 6.2  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 6.1  2005/05/10 15:42:53  didenko
 * Moved grid worker control thread to its own file
 *
 * ===========================================================================
 */
 
