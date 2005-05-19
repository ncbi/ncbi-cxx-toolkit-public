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
#include <corelib/ncbistre.hpp>
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
const string GETLOAD_CMD = "GETLOAD";

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

            string host = socket.GetPeerAddress();
            size_t pos = host.find_first_of(':');
            if (pos != string::npos) {
                host = host.substr(0, pos);
            }
            string ans = "ERR:";
            if (m_WorkerNode.IsHostInAdminHostsList(host)) {
                m_WorkerNode.RequestShutdown(CNetScheduleClient::eNormalShutdown);
                ans = "OK:";
                LOG_POST(Info << "Shutdown request has been received.");
            } else {
                ans = "ERR:Shutdown access denied.";
                LOG_POST(Warning << "Shutdown access denied: " << host);
            }
            socket.Write(ans.c_str(), ans.length() + 1 );
        }
        else if( strncmp( request.c_str(), VERSION_CMD.c_str(), 
                     VERSION_CMD.length() ) == 0 ) {
            string ans = "OK:" + m_WorkerNode.GetJobVersion() + WN_BUILD_DATE;
            socket.Write(ans.c_str(), ans.length() + 1 );
        } 
        else if( strncmp( request.c_str(), STAT_CMD.c_str(), 
                     STAT_CMD.length() ) == 0 ) {
            CNcbiOstrstream os;
            os << "OK:";
            os << m_WorkerNode.GetJobVersion() << WN_BUILD_DATE << endl;
            os << "Started: " << m_WorkerNode.GetStartTime().AsString() << endl;
            if (m_WorkerNode.GetShutdownLevel() != 
                CNetScheduleClient::eNoShutdown) {
                os << "THE NODE IS IN A SHUTTING DOWN MODE!!!" << endl;
            }
            os << "Maximum job threads: " 
               << NStr::UIntToString(m_WorkerNode.GetMaxThreads()) << endl
               << "Queue name: " << m_WorkerNode.GetQueueName() << endl
               << "Jobs Succeed: " 
               << NStr::UIntToString(m_WorkerNode.GetJobsSucceedNumber()) << endl
               << "Jobs Failed: " 
               << NStr::UIntToString(m_WorkerNode.GetJobsFailedNumber()) << endl
               << "Jobs Returned: " 
               << NStr::UIntToString(m_WorkerNode.GetJobsReturnedNumber()) << endl
               << "Jobs Running: " 
               << NStr::UIntToString(m_WorkerNode.GetJobsRunningNumber()) << endl;
            vector<CWorkerNodeJobContext::SJobStat> jobs;
            CWorkerNodeJobContext::CollectStatictics(jobs);
            CTime now(CTime::eCurrent);
            ITERATE(vector<CWorkerNodeJobContext::SJobStat>, it, jobs) {
                CTimeSpan ts = now - it->start_time.GetLocalTime();
                os << it->job_key << " " << it->job_input
                   << " -- running for " << ts.AsString("S") << " seconds." << endl;
            }
            os << ends;
            try {
                socket.Write(os.str(), os.pcount());
            }  catch (...) {
                os.freeze(false);
                throw;
            }
            os.freeze(false);

        }
        else if ( strncmp( request.c_str(), GETLOAD_CMD.c_str(), 
                           GETLOAD_CMD.length() ) == 0 ) {
            string ans = "ERR:";
            if (auth != m_WorkerNode.GetJobVersion()) {
                ans = "ERR:Wrong Program. Required: " 
                    + m_WorkerNode.GetJobVersion();
            } else {
                string qname, connection_info;
                NStr::SplitInTwo(queue, ";", qname, connection_info);
                if (qname != m_WorkerNode.GetQueueName())
                    ans = "ERR:Wrong Queue. Required: " 
                        + m_WorkerNode.GetQueueName();
                else if (connection_info != m_WorkerNode.GetConnectionInfo())
                    ans = "ERR:Wrong Connection Info. Required: " 
                        + m_WorkerNode.GetConnectionInfo();
                else {
                    int load = m_WorkerNode.GetMaxThreads() - 
                        m_WorkerNode.GetJobsRunningNumber();
                    ans = "OK:" + NStr::IntToString(load);
                }
            }
            socket.Write(ans.c_str(), ans.length() + 1 );
        }
        else {
            string ans = "ERR:Unknown command -- " + request;
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
 * Revision 6.7  2005/05/19 15:15:24  didenko
 * Added admin_hosts parameter to worker nodes configurations
 *
 * Revision 6.6  2005/05/16 14:02:45  didenko
 * Added GETLOAD command recognition
 *
 * Revision 6.5  2005/05/12 18:59:39  didenko
 * Added a node build date to the version command
 *
 * Revision 6.4  2005/05/12 14:52:05  didenko
 * Added a worker node build time and start time to the statistic
 *
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
 
