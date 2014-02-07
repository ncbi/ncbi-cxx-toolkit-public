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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 * File Description:
 *   CServer test application
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_control.hpp>
#include <connect/ncbi_util.h>
#include <connect/server.hpp>
#include <util/random_gen.hpp>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


/// CTestServer --
///
/// Lightly customized subclass of CServer.
///
/// CTestServer collaborates with CTestConnectionHandler to exit
/// cleanly after processing a predetermined number of client
/// connections.  It also holds some application-wide state that
/// CTestConnectionHandler consults and in some cases modifies.

class CTestServer : public CServer
{
public:
    /// Constructor: initialize class-specific fields.  (CServer has
    /// no construction-time parameters of its own.)
    ///
    /// @param max_number_of_clients
    ///  How many client connections to process before automatically
    ///  shutting down (when used in conjunction with CTestConnection
    ///  Handler).
    /// @param max_delay
    ///  Maximum (artificial) per-request processing time, in
    ///  milliseconds.
    CTestServer(int max_number_of_clients, unsigned int max_delay)
        : m_MaxNumberOfClients(max_number_of_clients),
          m_MaxDelay(max_delay),
          m_ShutdownRequested(false)
    {
        m_ClientCount.Set(0);
    }

    /// Callback indicating whether to proceed with a clean exit.
    virtual bool ShutdownRequested(void) { return m_ShutdownRequested; }

    /// Request a clean exit.  (Called by CTestConnectionHandler upon
    /// processing the Nth connection.)
    void RequestShutdown(void) { m_ShutdownRequested = true; }

    /// Increment the count of client connections received, and return
    /// the new value.
    int  RegisterClient(void) { return m_ClientCount.Add(1); }

    /// Return the (cumulative) connection limit originally supplied
    /// to the constructor.
    int  GetMaxNumberOfClients(void) const { return m_MaxNumberOfClients; }

    /// Return a randomized "processing time" up to the limit
    /// originally supplied to the constructor.
    unsigned int GetRandomDelay(void) const;

private:
    int                m_MaxNumberOfClients;  ///< Limit on total connections
    CAtomicCounter     m_ClientCount;         ///< Number of connections so far
    unsigned int       m_MaxDelay;            ///< Max processing time, in ms
    volatile bool      m_ShutdownRequested;   ///< Done with the last client?
    mutable CRandom    m_Rng;                 ///< Random delay generator
    mutable CFastMutex m_RngMutex;            ///< Ensure RNG thread-safety
};


unsigned int CTestServer::GetRandomDelay() const
{
    CFastMutexGuard LOCK(m_RngMutex);
    return m_Rng.GetRand(0, m_MaxDelay);
}


/// CTestConnectionHandler --
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

class CTestConnectionHandler : public IServer_LineMessageHandler
{
public:
    /// Constructor.
    CTestConnectionHandler(CTestServer* server)
        : m_Server(server), m_AlarmTime(CTime::eEmpty, CTime::eGmt)
    {
    }

    /// optional
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void); ///< MANDATORY to implement
    virtual void OnTimer(void); ///< optional (no-op by default)

    /// MANDATORY for subclasses of IServer_(Line)MessageHandler
    virtual void OnMessage(BUF buf);
    virtual void OnWrite(void); ///< MANDATORY to implement

private:
    CTestServer* m_Server;
    CTime        m_AlarmTime;
    enum {        ///< Possible connection states, in sequence
        Delay,    ///< (Randomized) "processing time"
        SayHello, ///< Greeting the client (or waiting to do so)
        Read      ///< Receiving (or awaiting) the client's response
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
        // fall through

    default:
        return eIO_Read;
    }
}

void CTestConnectionHandler::OnOpen(void)
{
    CRandom rng;
    unsigned int delay = m_Server->GetRandomDelay();
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
    CSocket& socket = GetSocket();

    size_t msg_size = BUF_Read(buf, data, sizeof(data));
    if (msg_size > 0) {
        ERR_POST(Info << "Got \"" << string(data, msg_size) << "\"");
    } else {
        ERR_POST(Info << "Got empty line");
    }

    if (m_State != Read) {
        ERR_POST(Info << "... ignored");
        return;
    }

    socket.Write("Goodbye!\n", sizeof("Goodbye!\n") - 1);
    socket.Close();

    int this_client_number = m_Server->RegisterClient();
    int max_number_of_clients = m_Server->GetMaxNumberOfClients();

    ERR_POST(Info << "Processed " << this_client_number
             << "/" << max_number_of_clients);

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

/// CTestConnectionFactory --
///
/// Factory for CTestConnectionHandler objects.

class CTestConnectionFactory : public IServer_ConnectionFactory
{
public:
    CTestConnectionFactory(CTestServer* server)
        : m_Server(server)
    {
    }

    /// Create a new CTestConnectionHandler object tied to the main
    /// (only ;-) CTestServer instance.
    ///
    /// Called on each incoming connection.
    IServer_ConnectionHandler* Create(void) {
        return new CTestConnectionHandler(m_Server);
    }
private:
    CTestServer* m_Server;
};


/// CConnectionRequest --
///
/// Simple built-in client code to ease testing.

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(unsigned short port,
                       CRequestRateControl& rate_control,
                       CFastMutex& mutex)
        : m_Port(port),
          m_RateControl(rate_control),
          m_Mutex(mutex)
    {
    }

protected:
    virtual void Process(void);

private:
    unsigned short m_Port;
    CRequestRateControl& m_RateControl;
    CFastMutex& m_Mutex;
};

void CConnectionRequest::Process(void)
{
    CTimeSpan sleep_time;
    do {
        {{
            CFastMutexGuard guard(m_Mutex);
            sleep_time = m_RateControl.ApproveTime();
        }}
        m_RateControl.Sleep(sleep_time);
    } while (!sleep_time.IsEmpty());
    CConn_SocketStream stream("localhost", m_Port);

    string junk;

    // FIXME: May not be always possible to read here (eg connection refused)
    stream >> junk;

    stream << "Hello!" << endl;

    stream >> junk;
}


/// CServerTestApp --
///
/// Main application class pulling everything together.
class CServerTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);
};


void CServerTestApp::Init(void)
{
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CServer test application");

    arg_desc->AddDefaultKey("srvthreads", "N",
                            "Initial number of server threads",
                            CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey("maxsrvthreads", "N",
                            "Maximum number of server threads",
                            CArgDescriptions::eInteger, "10");

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

    arg_desc->SetConstraint("clthreads", constraint);
    arg_desc->SetConstraint("maxclthreads", constraint);
    arg_desc->SetConstraint("requests", constraint);

    arg_desc->AddDefaultKey("maxdelay", "N",
                            "Maximum delay in milliseconds",
                            CArgDescriptions::eInteger, "1000");

    SetupArgDescriptions(arg_desc.release());
}

void CServerTestApp::Exit(void)
{
    CORE_SetLOG(0);
    CORE_SetLOCK(0);
}

// Check for shutdown request every second
static STimeout kAcceptTimeout = { 1, 0 };

int CServerTestApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_Severity | eDPF_OmitInfoSev | eDPF_ErrorID);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
    CRequestRateControl rate_control(6);
#else
    CRequestRateControl rate_control(CRequestRateControl::kNoLimit);
#endif
    CFastMutex rate_mutex;

    const CArgs& args = GetArgs();

    unsigned short port = 4096;

    {
        CListeningSocket listener;

        while (++port & 0xFFFF) {
            if (listener.Listen(port, 5, fSOCK_BindAny | fSOCK_LogOff)
                == eIO_Success)
                break;
        }
        if (port == 0) {
            ERR_POST("CServer test: unable to find a free port to listen on");
            return 2;
        }
    }

    SServer_Parameters params;
    params.init_threads = args["srvthreads"].AsInteger();
    params.max_threads = args["maxsrvthreads"].AsInteger();
    params.accept_timeout = &kAcceptTimeout;

    int max_number_of_clients = args["requests"].AsInteger();

    CTestServer server(max_number_of_clients, args["maxdelay"].AsInteger());
    server.SetParameters(params);

    server.AddListener(new CTestConnectionFactory(&server), port);
    server.StartListening();

    CStdPoolOfThreads pool(args["maxclthreads"].AsInteger(),
                           max_number_of_clients);

    pool.Spawn(args["clthreads"].AsInteger());

    for (int i = max_number_of_clients;  i > 0;  i--) {
        pool.AcceptRequest(CRef<ncbi::CStdRequest>
            (new CConnectionRequest(port, rate_control, rate_mutex)));
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
