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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   BDB environment keeper server
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

#include <db/bdb/bdb_util.hpp>
#include <db/bdb/bdb_env.hpp>

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_process.hpp>
# include <signal.h>
#endif


BEGIN_NCBI_SCOPE

#define BDB_ENV_KEEPER_VERSION "0.1.1"

#define BDB_ENV_KEEPER_FULL_VERSION \
    "NCBI BDB Env keeper server version " BDB_ENV_KEEPER_VERSION \
    " build " __DATE__ " " __TIME__

/// Main server class
///
///
class CBDBEnvKeeperServer : public CServer
{
public:
    CBDBEnvKeeperServer()
        : m_ShutdownRequested(false)
    {}
    virtual bool ShutdownRequested(void) { return m_ShutdownRequested; }
    void RequestShutdown(void) { m_ShutdownRequested = true; }
private:
    volatile bool m_ShutdownRequested;
};



/// @internal
static CBDBEnvKeeperServer* s_server = 0;



/// Actual event (command) responder (session state machine)
///
class CBDBEnvKeeperConnectionHandler : public IServer_LineMessageHandler
{
public:
    CBDBEnvKeeperConnectionHandler(CBDBEnvKeeperServer* server) :
        m_Server(server)
    {
    }
    virtual void OnOpen(void);
    virtual void OnMessage(BUF buf);
    virtual void OnWrite(void) {}
private:
    CBDBEnvKeeperServer* m_Server;
};

void CBDBEnvKeeperConnectionHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
}

/// @internal
static
string s_ReadStrFromBUF(BUF buf)
{
    size_t size = BUF_Size(buf);
    string ret(size, '\0');
    BUF_Read(buf, &ret[0], size);
    return ret;
}


void CBDBEnvKeeperConnectionHandler::OnMessage(BUF buf)
{
    CSocket &socket = GetSocket();

    string cmd = s_ReadStrFromBUF(buf);

    if (cmd == "VERSION") {
        string msg = BDB_ENV_KEEPER_FULL_VERSION;
        msg.append("\n");
        socket.Write(msg.c_str(), msg.length());
    } else
    if (cmd == "SHUT" || cmd == "SHUTDOWN") {
        string msg = "Shutdown initiated.\n";
        socket.Write(msg.c_str(), msg.length() - 1);
        s_server->RequestShutdown();
    } else {
        string msg = "Incorrect command.\n";
        socket.Write(msg.c_str(), msg.length() - 1);
    }

    socket.Close();
}


class CBDBEnvKeeperConnectionFactory : public IServer_ConnectionFactory
{
public:
    CBDBEnvKeeperConnectionFactory(CBDBEnvKeeperServer* server) :
        m_Server(server)
    {
    }
    IServer_ConnectionHandler* Create(void) {
        return new CBDBEnvKeeperConnectionHandler(m_Server);
    }
private:
    CBDBEnvKeeperServer* m_Server;
};


// Application
class CBDBEnvKeeperApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CBDBEnvKeeperApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "BDB Environment Keeper");

    SetupArgDescriptions(arg_desc.release());
}

// Check for shutdown request every second
static STimeout kAcceptTimeout = { 1, 0 };

/// @internal
extern "C" void Server_SignalHandler(int signum)
{
    if (s_server && (!s_server->ShutdownRequested()) ) {
        s_server->RequestShutdown();
    }
}


int CBDBEnvKeeperApp::Run(void)
{
    LOG_POST(BDB_ENV_KEEPER_FULL_VERSION);

    const CNcbiRegistry& reg = GetConfig();
    bool is_daemon =
        reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);

#if defined(NCBI_OS_UNIX)
        // attempt to get server gracefully shutdown on signal
        signal( SIGINT,  Server_SignalHandler);
        signal( SIGTERM, Server_SignalHandler);
#endif



#if defined(NCBI_OS_UNIX)
        if (is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = CProcess::Daemonize(0, CProcess::fDontChroot);
            if (!daemon) {
                return 0;
            }

        }

#endif


    auto_ptr<CBDB_Env> env(BDB_CreateEnv(reg, "bdb"));

//    const CArgs& args = GetArgs();

    unsigned short port =
        reg.GetInt("server", "port", 9200, 0, CNcbiRegistry::eReturn);

    SServer_Parameters params;
    params.init_threads = 1;
    params.max_threads = 3;
    params.accept_timeout = &kAcceptTimeout;

    CBDBEnvKeeperServer server;
    s_server = &server;
    server.SetParameters(params);

    server.AddListener(new CBDBEnvKeeperConnectionFactory(&server), port);

    server.Run();


    // Finalization
    {{
        if (env->IsTransactional()) {
            env->ForceTransactionCheckpoint();
        }


        if (env->CheckRemove()) {
            LOG_POST(Info
                << "Environment has been unmounted and deleted.");
        } else {
            LOG_POST(Warning
                << "Environment still in use. Cannot delete it.");
        }
    }}

    return 0;
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CBDBEnvKeeperApp().AppMain(argc, argv);
}
