/* $Id$
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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   CServer listeners interactive test application.
 *   You can run test_server_listeners, connect to the listening port using
 *   telnet and issue one of the following commands:
 *   - close - closes the socket
 *   - ports - prints listening ports
 *   - shutdown - shuts the server down
 *   - add <port> - adds a listener
 *   - remove <port> - removes a listener
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_control.hpp>
#include <connect/ncbi_util.h>
#include <connect/server.hpp>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


/// CListenerTestServer --
///
/// CListenerTestServer collaborates with CListenerTestConnectionHandler to exit
/// cleanly when a certain command is received
class CListenerTestServer : public CServer
{
public:
    /// Constructor: initialize class-specific fields.  (CServer has
    /// no construction-time parameters of its own.)
    CListenerTestServer(void) :
          m_ShutdownRequested(false)
    {}

    /// Callback indicating whether to proceed with a clean exit.
    virtual bool ShutdownRequested(void) { return m_ShutdownRequested; }

    /// Request a clean exit.  (Called by CTestConnectionHandler upon
    /// processing the Nth connection.)
    void RequestShutdown(void) { m_ShutdownRequested = true; }

private:
    volatile bool      m_ShutdownRequested;
};


/// CListenerTestConnectionFactory --
///
/// Factory for CListenerTestConnectionHandler objects.
class CListenerTestConnectionFactory : public IServer_ConnectionFactory
{
public:
    CListenerTestConnectionFactory(CListenerTestServer *  server)
        : m_Server(server)
    {}
    IServer_ConnectionHandler *  Create(void);
private:
    CListenerTestServer *   m_Server;
};


/// CListenerTestConnectionHandler --
///
/// Class for processing incoming client connections.
///
/// Each connection spawns a fresh instance of this class, which
/// collaborates with CTestServer to maintain the (small) amount of
/// state it needs.
///
/// All virtual methods here are implementations of parent classes'
/// hooks (some of which are optional to define); see their
/// documentation (particularly IServer_ConnectionHandler's) for more
/// details.
class CListenerTestConnectionHandler : public IServer_LineMessageHandler
{
public:
    /// Constructor.
    CListenerTestConnectionHandler(CListenerTestServer *  server)
        : m_Server(server)
    {}

    /// optional
    virtual void OnOpen(void);
    virtual void OnClose(IServer_ConnectionHandler::EClosePeer peer);

    virtual void OnMessage(BUF buf);
    virtual void OnWrite(void) {}

private:
    CListenerTestServer* m_Server;
};


void CListenerTestConnectionHandler::OnOpen(void)
{
    cout << "OnOpen a new connection" << endl;
}

void
CListenerTestConnectionHandler::OnClose(
                            IServer_ConnectionHandler::EClosePeer peer)
{
    cout << "OnClose a connection: ";
    if (peer == IServer_ConnectionHandler::eOurClose)
        cout << "our close" << endl;
    else
        cout << "client close" << endl;
}

void CListenerTestConnectionHandler::OnMessage(BUF buf)
{
    char            data[1024];
    CSocket &       socket = GetSocket();
    size_t          msg_size = BUF_Read(buf, data, sizeof(data));

    if (msg_size == 0) {
        cout << "Got empty line" << endl;
        return;
    }

    string          message(data, msg_size);
    cout << "Got \"" << message << "\"" << endl;

    NStr::ToLower(message);
    message = NStr::TruncateSpaces(message);

    if (message == "close") {
        cout << "Closing the connection" << endl;
        socket.Close();
        return;
    }

    if (message == "ports") {
        vector<unsigned short>  ports = m_Server->GetListenerPorts();
        if (ports.empty()) {
            cout << "No ports are listened" << endl;
            return;
        }
        cout << "Listened ports: ";
        for (vector<unsigned short>::const_iterator k = ports.begin();
                k != ports.end(); ++k)
            cout << *k << " ";
        cout << endl;
        return;
    }

    if (message == "help") {
        const char *    m = "Accepted commands:\n"
                            "close - closes the socket\n"
                            "ports - prints listening ports\n"
                            "shutdown - shuts the server down\n"
                            "add <port> - adds a listener\n"
                            "remove <port> - removes a listener\n";
        socket.Write(m, strlen(m));
        return;
    }

    if (message == "shutdown") {
        m_Server->RequestShutdown();
        return;
    }

    if (NStr::StartsWith(message, "add ")) {
        message = string(message.c_str() + 3);
        message = NStr::TruncateSpaces(message);
        int     port = atoi(message.c_str());
        if (port <= 0 || port >= 65536) {
            cout << "Invalid port " << port << endl;
            return;
        }

        try {
            m_Server->AddListener(new CListenerTestConnectionFactory(m_Server),
                                  (unsigned short)(port));
        } catch (const exception &  ex) {
            cout << "Error adding a listener: " << ex.what() << endl;
        }

        return;
    }

    if (NStr::StartsWith(message, "remove ")) {
        message = string(message.c_str() + 6);
        message = NStr::TruncateSpaces(message);
        int     port = atoi(message.c_str());
        if (port <= 0 || port >= 65536) {
            cout << "Invalid port " << port << endl;
            return;
        }

        try {
            m_Server->RemoveListener((unsigned short)(port));
        } catch (const exception &  ex) {
            cout << "Error remopving a listener: " << ex.what() << endl;
        }

        return;
    }

    cout << "Invalid command" << endl;
}


IServer_ConnectionHandler *  CListenerTestConnectionFactory::Create(void)
{
    return new CListenerTestConnectionHandler(m_Server);
}



/// CServerListenerTestApp --
///
/// Main application class pulling everything together.
class CServerListenerTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void) {}
};


void CServerListenerTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CServer listener test application");

    arg_desc->AddDefaultKey("port", "N",
                            "Server port to start listening on",
                            CArgDescriptions::eInteger, "9001");

    SetupArgDescriptions(arg_desc.release());
}


// Check for shutdown request every second
static STimeout kAcceptTimeout = { 1, 0 };

int CServerListenerTestApp::Run(void)
{
    const CArgs &   args = GetArgs();
    int             port_int = args["port"].AsInteger();

    if (port_int <= 0 || port_int >= 65536) {
        cerr << "Invalid port" << endl;
        return 1;
    }

    SServer_Parameters      params;
    params.init_threads = 5;
    params.max_threads = 5;
    params.accept_timeout = &kAcceptTimeout;


    CListenerTestServer     server;
    server.SetParameters(params);

    server.AddListener(new CListenerTestConnectionFactory(&server),
                       (unsigned short)(port_int));
    server.Run();
    return 0;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CServerListenerTestApp().AppMain(argc, argv);
}

