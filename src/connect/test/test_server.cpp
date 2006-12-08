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
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_util.h>
#include <connect/server.hpp>


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
    string hello("Hello!\n");
    CSocket &socket = GetSocket();

    socket.Write(hello.c_str(), hello.size());
}


void CTestConnectionHandler::OnMessage(BUF buf)
{
    char data[1024];
    string goodbye("Goodbye!\n");
    CSocket &socket = GetSocket();

    size_t msg_size = BUF_Read(buf, data, sizeof(data));
    if (msg_size >= 0) {
        data[msg_size] = '\0';
        if (strncmp(data, "Hello!", strlen("Hello!")) == 0)
            m_Server->AddHello();
        printf("got \"%s\"\n", data);
    } else {
        printf("got empty buffer\n");
    }
    socket.Write(goodbye.c_str(), goodbye.size());
    socket.Close();
    if (strncmp(data, "Goodbye!", strlen("Goodbye!")) == 0) {
        printf("got shutdown request\n");
        printf("%d Hello's counted\n", m_Server->CountHellos());
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


// App
class CThreadedServerApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CThreadedServerApp::Init(void)
{
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetLOCK(MT_LOCK_cxx2c());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                             "sample server using thread pools");

    arg_desc->AddKey("port", "N", "TCP port number on which to listen",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint("port", new CArgAllow_Integers(0, 0xFFFF));

    arg_desc->AddDefaultKey("threads", "N", "Number of initial threads",
                            CArgDescriptions::eInteger, "5");
    
    arg_desc->AddDefaultKey("maxThreads", "N",
                            "Maximum number of simultaneous threads",
                            CArgDescriptions::eInteger, "10");
    
    arg_desc->AddDefaultKey("queue", "N", "Maximum size of request queue",
                            CArgDescriptions::eInteger, "20");

    {{
        CArgAllow* constraint = new CArgAllow_Integers(1, 999);
        arg_desc->SetConstraint("threads",    constraint);
        arg_desc->SetConstraint("maxThreads", constraint);
        arg_desc->SetConstraint("queue",      constraint);        
    }}

    SetupArgDescriptions(arg_desc.release());
}


// Check for shutdown request every second
static STimeout kAcceptTimeout = { 1, 0 };

int CThreadedServerApp::Run(void)
{
    const CArgs& args = GetArgs();

    SServer_Parameters params;
    params.init_threads = args["threads"].AsInteger();
    params.max_threads  = args["maxThreads"].AsInteger();
    params.queue_size   = args["queue"].AsInteger();
    params.accept_timeout = &kAcceptTimeout;

    CTestServer server;
    server.SetParameters(params);

    unsigned short port = args["port"].AsInteger();
    server.AddListener(
        new CTestConnectionFactory(&server),
        port);

    server.Run();

    return 0;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CThreadedServerApp().AppMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2006/12/08 16:28:08  joukovv
 * Slip of mind fixed.
 *
 * Revision 6.2  2006/09/27 21:26:06  joukovv
 * Thread-per-request is finally implemented. Interface changed to enable
 * streams, line-based message handler added, netscedule adapted.
 *
 * Revision 6.1  2006/09/13 18:32:22  joukovv
 * Added (non-functional yet) framework for thread-per-request thread pooling model,
 * netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
 *
 *
 * ===========================================================================
 */
