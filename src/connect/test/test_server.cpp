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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 * File Description:
 *   Sample server using a thread pool
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_util.h>
#include <connect/server.hpp>
#include <util/random_gen.hpp>
#include <util/thread_pool.hpp>


BEGIN_NCBI_SCOPE


// Subclass CServer for implementation of ShutdownRequested
class CTestServer : public CServer
{
public:
    CTestServer(int max_number_of_clients, unsigned int max_delay) :
        m_MaxNumberOfClients(max_number_of_clients),
        m_ClientCount(0),
        m_MaxDelay(max_delay),
        m_ShutdownRequested(false)
    {
    }
    virtual bool ShutdownRequested(void) { return m_ShutdownRequested; }
    void RequestShutdown(void) { m_ShutdownRequested = true; }
    int  RegisterClient();
    int  GetMaxNumberOfClients() const;
    unsigned int GetMaxDelay() const {return m_MaxDelay;}
private:
    int m_MaxNumberOfClients;
    int m_ClientCount;
    CFastMutex m_ClientCountMutex;
    unsigned int m_MaxDelay;
    volatile bool m_ShutdownRequested;
};


int CTestServer::RegisterClient()
{
    CFastMutexGuard guard(m_ClientCountMutex);
    return ++m_ClientCount;
}


int CTestServer::GetMaxNumberOfClients() const
{
    return m_MaxNumberOfClients;
}


// ConnectionHandler
class CTestConnectionHandler : public IServer_LineMessageHandler
{
public:
    CTestConnectionHandler(CTestServer* server) :
        m_Server(server), m_AlarmTime(CTime::eEmpty, CTime::eGmt)
    {
    }
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnTimer(void);
    virtual void OnMessage(BUF buf);
    virtual void OnWrite(void);
    virtual void OnClose(void) { }
private:
    CTestServer* m_Server;
    CTime m_AlarmTime;
    enum {
        Delay,
        SayHello,
        Read
    } m_State;
};

EIO_Event CTestConnectionHandler::GetEventsToPollFor(
    const CTime** alarm_time) const
{
    switch (m_State) {
    case SayHello:
        return eIO_Write;

    case Delay:
        *alarm_time = &m_AlarmTime;

    default:
        return eIO_Read;
    }
}

void CTestConnectionHandler::OnOpen(void)
{
    CRandom rng;
    unsigned int delay = rng.GetRand(0, m_Server->GetMaxDelay());
    m_AlarmTime.SetCurrent();
    m_AlarmTime.AddTimeSpan(CTimeSpan(delay / 1000,
        (delay % 1000) * 1000 * 1000));
    m_State = Delay;
}

void CTestConnectionHandler::OnTimer(void)
{
    m_State = SayHello;
}

void CTestConnectionHandler::OnMessage(BUF buf)
{
    char data[1024];
    CSocket &socket = GetSocket();

    size_t msg_size = BUF_Read(buf, data, sizeof(data));
    if (msg_size > 0) {
        data[msg_size] = '\0';
        ERR_POST(Info << "got \"" << data << "\"");
    } else {
        ERR_POST(Info << "got empty line");
    }

    if (m_State != Read) {
        ERR_POST(Info << "... ignored");
        return;
    }

    socket.Write("Goodbye!\n", sizeof("Goodbye!\n") - 1);
    socket.Close();

    int this_client_number = m_Server->RegisterClient();
    int max_number_of_clients = m_Server->GetMaxNumberOfClients();

    ERR_POST(Info << "Processed " << this_client_number <<
        "/" << max_number_of_clients);

    if (this_client_number == max_number_of_clients) {
        ERR_POST(Info << "All clients processed");
        m_Server->RequestShutdown();
    }
}

void CTestConnectionHandler::OnWrite(void)
{
    GetSocket().Write("Hello!\n", sizeof("Hello!\n") - 1);
    m_State = Read;
}

// ConnectionFactory
class CTestConnectionFactory : public IServer_ConnectionFactory
{
public:
    CTestConnectionFactory(CTestServer* server) :
        m_Server(server)
    {
    }
    IServer_ConnectionHandler* Create(void) {
        return new CTestConnectionHandler(m_Server);
    }
private:
    CTestServer* m_Server;
};


// The client part

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(unsigned short port) : m_Port(port)
    {
    }

protected:
    virtual void Process(void);

private:
    unsigned short m_Port;
};

void CConnectionRequest::Process(void)
{
    CConn_SocketStream stream("localhost", m_Port);

    string junk;

    stream >> junk;

    stream << "Hello!" << endl;

    stream >> junk;
}


// Application
class CServerTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CServerTestApp::Init(void)
{
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetLOCK(MT_LOCK_cxx2c());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "CServer test application");

    arg_desc->AddDefaultKey("srvthreads", "N",
        "Initial number of server threads",
        CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey("maxsrvthreads", "N",
        "Maximum number of server threads",
        CArgDescriptions::eInteger, "10");

    arg_desc->AddDefaultKey("queuesize", "N",
        "Maximum size of request queue",
        CArgDescriptions::eInteger, "20");

    arg_desc->AddDefaultKey("clthreads", "N",
        "Initial number of client threads",
        CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey("maxclthreads", "N",
        "Maximum number of client threads",
        CArgDescriptions::eInteger, "10");

    arg_desc->AddDefaultKey("requests", "N",
        "Number of requests to make",
        CArgDescriptions::eInteger, "100");

    CArgAllow* constraint = new CArgAllow_Integers(1, 999);

    arg_desc->SetConstraint("srvthreads", constraint);
    arg_desc->SetConstraint("maxsrvthreads", constraint);
    arg_desc->SetConstraint("queuesize", constraint);

    arg_desc->SetConstraint("clthreads", constraint);
    arg_desc->SetConstraint("maxclthreads", constraint);
    arg_desc->SetConstraint("requests", constraint);

    arg_desc->AddDefaultKey("maxdelay", "N",
        "Maximum delay in milliseconds",
        CArgDescriptions::eInteger, "1000");

    SetupArgDescriptions(arg_desc.release());
}

// Check for shutdown request every second
static STimeout kAcceptTimeout = { 1, 0 };

int CServerTestApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);

    const CArgs& args = GetArgs();

    unsigned short port = 4096;

    {
        CListeningSocket listener;

        while ((++port & 0xFFFF) != 0 && listener.Listen(port, 5,
            fLSCE_BindAny | fLSCE_LogOff) != eIO_Success)
            ;

        if (port == 0) {
            ERR_POST("CServer test: unable to find a free port to listen to");
            return 2;
        }
    }

    SServer_Parameters params;
    params.init_threads = args["srvthreads"].AsInteger();
    params.max_threads = args["maxsrvthreads"].AsInteger();
    params.queue_size = args["queuesize"].AsInteger();
    params.accept_timeout = &kAcceptTimeout;

    int max_number_of_clients = args["requests"].AsInteger();

    CTestServer server(max_number_of_clients, args["maxdelay"].AsInteger());
    server.SetParameters(params);

    server.AddListener(new CTestConnectionFactory(&server), port);

    CStdPoolOfThreads pool(args["maxclthreads"].AsInteger(),
        max_number_of_clients);

    pool.Spawn(args["clthreads"].AsInteger());

    int i = max_number_of_clients;

    while (--i >= 0) {
        pool.AcceptRequest(CRef<ncbi::CStdRequest>(
            new CConnectionRequest(port)));
    }

    server.Run();

    pool.KillAllThreads(true);

    return 0;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CServerTestApp().AppMain(argc, argv);
}
