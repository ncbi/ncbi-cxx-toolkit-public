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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_process.hpp>

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_control_thread.hpp>
#include <connect/services/grid_worker_app_impl.hpp>
#include <connect/services/error_codes.hpp>

#include <math.h>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// 
///@internal

class CGetVersionProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CGetVersionProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        os << "OK:" << node.GetJobVersion() << WN_BUILD_DATE;
    }
};

class CShutdownProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CShutdownProcessor() {}

    virtual bool Authenticate(const string& host,
                              const string& auth, 
                              const string& queue,
                              CNcbiOstream& os,
                              const CGridWorkerNode& node) 
    {
        m_Host = host;
        size_t pos = m_Host.find_first_of(':');
        if (pos != string::npos) {
            m_Host = m_Host.substr(0, pos);
        }
        if (node.IsHostInAdminHostsList(m_Host)) {
            return true;
        }
        os << "ERR:Shutdown access denied.";
        LOG_POST_X(10, Warning << CTime(CTime::eCurrent).AsString() 
                   << " Shutdown access denied: " << m_Host);
        return false;
    }

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& )
    {
        if (request.find("SUICIDE") != NPOS) {
            LOG_POST_X(11, Warning << CTime(CTime::eCurrent).AsString() 
                       << " DIE request has been received from host: " << m_Host);
            LOG_POST_X(12, Warning << CTime(CTime::eCurrent).AsString() 
                       << " SERVER IS COMMITTING SUICIDE!!");
            CGridGlobals::GetInstance().KillNode();
        } else {
            CNetScheduleAdmin::EShutdownLevel level =
                CNetScheduleAdmin::eNormalShutdown;
            if (request.find("IMMEDIATE") != NPOS) 
                level = CNetScheduleAdmin::eShutdownImmediate;
            os << "OK:";
            CGridGlobals::GetInstance().
                RequestShutdown(level);
            LOG_POST_X(13, CTime(CTime::eCurrent).AsString() 
                       << " Shutdown request has been received from host: " << m_Host);
        }
    }
private:
    string m_Host;
};

class CGetStatisticsProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    CGetStatisticsProcessor() {}
    virtual ~CGetStatisticsProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {       
        os << "OK:" << node.GetJobVersion() << WN_BUILD_DATE << endl;
        os << "Node started at: " << CGridGlobals::GetInstance().GetStartTime().AsString() << endl;
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app)
            os << "Executable path: " << app->GetProgramExecutablePath() 
               << "; PID: " << CProcess::GetCurrentPid() << endl;

        os << "Queue name: " << node.GetQueueName() << endl;
        if (node.GetMaxThreads() > 1)
            os << "Maximum job threads: " << node.GetMaxThreads() << endl;

        if (CGridGlobals::GetInstance().
            GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown) {
                os << "THE NODE IS IN A SHUTTING DOWN MODE!!!" << endl;
        }
        if (node.IsExclusiveMode())
            os << "THE NODE IS IN AN EXCLUSIVE MODE!!!" << endl;

        CGridGlobals::GetInstance().GetJobsWatcher().Print(os);
        //if (node.IsOnHold()) {
        //        os << "THE NODE IDLE TASK IS RUNNING..." << endl;
        //}

    }
};

class CGetLoadProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    CGetLoadProcessor()  {}
    virtual ~CGetLoadProcessor() {}
   
    virtual bool Authenticate(const string& host,
                              const string& auth, 
                              const string& queue,
                              CNcbiOstream& os,
                              const CGridWorkerNode& node) 
    {
        string cmp = node.GetClientName() + " prog='" + node.GetJobVersion() + '\'';
        if (auth != cmp) {
            os <<"ERR:Wrong Program. Required: " << node.GetJobVersion() 
               << endl << auth << endl << cmp;
            return false;
        } 
        string qname, connection_info;
        NStr::SplitInTwo(queue, ";", qname, connection_info);
        if (qname != node.GetQueueName()) {
            os << "ERR:Wrong Queue. Required: " << node.GetQueueName();
            return false;
        } 
        if (connection_info != node.GetServiceName()) {
            os << "ERR:Wrong Connection Info. Required: "                     
               << node.GetServiceName();
            return false;
        }
        return true;
    }

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        int load = 0;
        //if (!node.IsOnHold())
            load = node.GetMaxThreads() 
                - CGridGlobals::GetInstance().GetJobsWatcher().GetJobsRunningNumber();
        os << "OK:" << load;
    }
};

class CUnknownProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CUnknownProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        os << "ERR:Unknown command -- " << request;
    }
};

const string SHUTDOWN_CMD = "SHUTDOWN";
const string VERSION_CMD = "VERSION";
const string STAT_CMD = "STAT";
const string GETLOAD_CMD = "GETLOAD";

/////////////////////////////////////////////////////////////////////////////
// 
///@internal

/* static */
CWorkerNodeControlThread::IRequestProcessor* 
CWorkerNodeControlThread::MakeProcessor(const string& cmd)
{
    if (NStr::StartsWith(cmd, SHUTDOWN_CMD))
        return new CShutdownProcessor;
    else if (NStr::StartsWith(cmd, VERSION_CMD))
        return new CGetVersionProcessor;
    else if (NStr::StartsWith(cmd, STAT_CMD))
        return new CGetStatisticsProcessor;
    else if (NStr::StartsWith(cmd, GETLOAD_CMD))
        return new CGetLoadProcessor;
    return new CUnknownProcessor;
}

class CWNCTConnectionFactory : public IServer_ConnectionFactory
{
public:
    CWNCTConnectionFactory(CWorkerNodeControlThread& server, unsigned int& start_port, unsigned int end_port)
        : m_Server(server), m_Port(start_port), m_EndPort(end_port)
    {}
    virtual IServer_ConnectionHandler* Create(void) {
        return new CWNCTConnectionHandler(m_Server);
    }
    virtual EListenAction OnFailure(unsigned short* port )
    { 
        if (*port >= m_EndPort)
            return eLAFail; 
        m_Port = ++(*port);
        return eLARetry;
    }

private:
    CWorkerNodeControlThread& m_Server;
    unsigned int& m_Port;
    unsigned int m_EndPort;
};

static STimeout kAcceptTimeout = {1,0};
CWorkerNodeControlThread::CWorkerNodeControlThread(unsigned int start_port, unsigned int end_port,
                                                   CGridWorkerNode& worker_node)
    : m_WorkerNode(worker_node), m_ShutdownRequested(false), m_Port(start_port)
{
    SServer_Parameters params;
    params.init_threads = 1;
    params.max_threads = 3;
    params.accept_timeout = &kAcceptTimeout;
    SetParameters(params);    
    AddListener(new CWNCTConnectionFactory(*this, m_Port, end_port),m_Port);
}

CWorkerNodeControlThread::~CWorkerNodeControlThread()
{
    LOG_POST_X(14, CTime(CTime::eCurrent).AsString() << " Control server stopped.");
}
bool CWorkerNodeControlThread::ShutdownRequested(void) 
{
    //    return CGridGlobals::GetInstance().
    //        GetShutdownLevel() != CNetScheduleClient::eNoShutdown;
    return m_ShutdownRequested;
}

void CWorkerNodeControlThread::ProcessTimeout(void)
{
    CGridGlobals::GetInstance().GetJobsWatcher().CheckInfinitLoop();
}



////////////////////////////////////////////////
static string s_ReadStrFromBUF(BUF buf)
{
    size_t size = BUF_Size(buf);
    string ret(size, '\0');
    BUF_Read(buf, &ret[0], size);
    return ret;
}

CWNCTConnectionHandler::CWNCTConnectionHandler(CWorkerNodeControlThread& server) 
    : m_Server(server)
{}

CWNCTConnectionHandler::~CWNCTConnectionHandler()
{}

void CWNCTConnectionHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    m_ProcessMessage = &CWNCTConnectionHandler::x_ProcessAuth;

}

static void s_HandleError(CSocket& socket, const string& msg)
{
    ERR_POST_X(15, CTime(CTime::eCurrent).AsString() 
               << " Exception in the control server : " << msg);
    string err = "ERR:" + NStr::PrintableString(msg);
    socket.Write(&err[0], err.size());
    socket.Close();
}
void CWNCTConnectionHandler::OnMessage(BUF buffer)
{
    try {
        (this->*m_ProcessMessage)(buffer);
    } catch(exception& ex) {
        s_HandleError(GetSocket(), ex.what());
    } catch(...) {
        s_HandleError(GetSocket(), "Unknown Error");
    }
}
    
void CWNCTConnectionHandler::x_ProcessAuth(BUF buffer)
{
    m_Auth = s_ReadStrFromBUF(buffer);
    m_ProcessMessage = &CWNCTConnectionHandler::x_ProcessQueue;
}
void CWNCTConnectionHandler::x_ProcessQueue(BUF buffer)
{
    m_Queue = s_ReadStrFromBUF(buffer);
    m_ProcessMessage = &CWNCTConnectionHandler::x_ProcessRequest;
}
void CWNCTConnectionHandler::x_ProcessRequest(BUF buffer)
{
    string request = s_ReadStrFromBUF(buffer);

    CSocket& socket = GetSocket();
    string host = socket.GetPeerAddress();
        
    CNcbiOstrstream os;
    auto_ptr<CWorkerNodeControlThread::IRequestProcessor> 
        processor(m_Server.MakeProcessor(request));
    if (processor->Authenticate(host, m_Auth, m_Queue, os, 
                                m_Server.GetWorkerNode()))
        processor->Process(request, os, m_Server.GetWorkerNode());

    os << ends;
    try {
        socket.Write(os.str(), os.pcount());
    }  catch (...) {
        os.freeze(false);
        throw;
    }
    os.freeze(false);
    socket.Close();
}

END_NCBI_SCOPE
