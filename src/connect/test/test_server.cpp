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
    CTestServer() : m_ShutdownRequested(false), m_HelloCount(0) { }
    virtual bool ShutdownRequested(void) { return m_ShutdownRequested; }
    void RequestShutdown(void) { m_ShutdownRequested = true; }
    void AddHello();
    int  CountHellos();
private:
    volatile bool m_ShutdownRequested;
    CFastMutex m_HelloMutex;
    int m_HelloCount;
};


void CTestServer::AddHello()
{
    CFastMutexGuard guard(m_HelloMutex);
    ++m_HelloCount;
}


int CTestServer::CountHellos()
{
    CFastMutexGuard guard(m_HelloMutex);
    return m_HelloCount;
}


// ConnectionHandler
class CTestConnectionHandler : public IServer_LineMessageHandler
{
public:
    CTestConnectionHandler(CTestServer* server) :
        m_Server(server)
    {
    }
    virtual void OnOpen(void);
    virtual void OnMessage(BUF buf);
    virtual void OnWrite(void) { }
    virtual void OnClose(void) { }
private:
    CTestServer* m_Server;
};


void CTestConnectionHandler::OnOpen(void)
{
    GetSocket().Write("Hello!\n", sizeof("Hello!\n") - 1);
}


void CTestConnectionHandler::OnMessage(BUF buf)
{
    char data[1024];
    CSocket &socket = GetSocket();

    size_t msg_size = BUF_Read(buf, data, sizeof(data));
    if (msg_size > 0) {
        data[msg_size] = '\0';
        if (strncmp(data, "Hello!", strlen("Hello!")) == 0)
            m_Server->AddHello();
        ERR_POST(Info << "got \"" << data << "\"");
    } else {
        ERR_POST(Info << "got empty buffer");
    }

    socket.Write("Goodbye!\n", sizeof("Goodbye!\n") - 1);
    socket.Close();

    if (memcmp(data, "Goodbye!", sizeof("Goodbye!") - 1) == 0) {
        ERR_POST(Info << "got shutdown request");
        ERR_POST(Info << "Hello counter = " << m_Server->CountHellos());
        m_Server->RequestShutdown();
    }
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

static unsigned int s_Requests;
static volatile unsigned int s_Processed = 0;

DEFINE_STATIC_FAST_MUTEX(s_Mutex);

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(unsigned short port, unsigned int delay) :
        m_Port(port), m_Delay(delay)
    {
    }

protected:
    virtual void Process(void);

private:
    unsigned short m_Port;
    unsigned int m_Delay;
};

void CConnectionRequest::Process(void)
{
    CConn_SocketStream stream("localhost", m_Port);

    SleepMilliSec(m_Delay);

    unsigned int request_number;

    {
        CFastMutexGuard guard(s_Mutex);
        request_number = ++s_Processed;
    }

    string junk;

    stream >> junk;

    stream << (request_number < s_Requests ? "Hello!" : "Goodbye!") << endl;

    stream >> junk;

    ERR_POST(Info << "Processed " << request_number << "/" << s_Requests);
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

    arg_desc->AddDefaultKey("delay", "N",
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

    CTestServer server;
    server.SetParameters(params);

    server.AddListener(new CTestConnectionFactory(&server), port);

    s_Requests = args["requests"].AsInteger();

    CStdPoolOfThreads pool(args["maxclthreads"].AsInteger(), s_Requests);
    CRandom rng;

    pool.Spawn(args["clthreads"].AsInteger());

    int delay = args["delay"].AsInteger();

    for (unsigned int i = 0;  i < s_Requests;  ++i) {
        pool.AcceptRequest(CRef<ncbi::CStdRequest>(new CConnectionRequest(port,
            rng.GetRand(0, delay))));

        //SleepMilliSec(rng.GetRand(0, delay));
    }

    // s_Send(host, port, 500, eGoodbye);
    /*pool.AcceptRequest(CRef<ncbi::CStdRequest>
        (new CConnectionRequest(port, 500, eGoodbye)));*/

    //SleepMilliSec(1000);
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
