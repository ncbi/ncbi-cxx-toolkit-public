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
 * Author:  Aaron Ucko
 *
 * File Description:
 *   Sample client using a thread pool; designed to connect to
 *   test_threaded_server
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <util/random_gen.hpp>
#include <util/thread_pool.hpp>


BEGIN_NCBI_SCOPE


static          unsigned int s_Requests;
static volatile unsigned int s_Processed = 0;
DEFINE_STATIC_FAST_MUTEX(s_Mutex);


enum EMessage {
    eHello,
    eGoodbye
};

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(const string& host, unsigned short port,
                       unsigned int delay /* in ms */, EMessage message)
        : m_Host(host), m_Port(port), m_Delay(delay), m_Message(message) {}

protected:
    virtual void Process(void);

private:
    string         m_Host;
    unsigned short m_Port;
    unsigned int   m_Delay;
    EMessage       m_Message;
};

static void s_Send(const string& host, unsigned short port,
                   unsigned int delay, EMessage message)
{
    CConn_SocketStream stream(host, port);
    SleepMilliSec(delay);
    string junk;

    const char* text = 0;
    switch (message) {
    case eHello:   text = "Hello!";   break;
    case eGoodbye: text = "Goodbye!"; break;
    }

    stream >> junk;
    stream << text << endl;
    stream >> junk;

    switch (message) {
    case eHello:
    {
        CFastMutexGuard guard(s_Mutex);
        cerr << "Processed " << ++s_Processed << "/" << s_Requests << endl;
        break;
    }
    case eGoodbye:
        cerr << "Requested server shutdown." << endl;
    }
}


void CConnectionRequest::Process(void)
{
    s_Send(m_Host, m_Port, m_Delay, m_Message);
}


class CThreadedClientApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CThreadedClientApp::Init(void)
{
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetLOCK(MT_LOCK_cxx2c());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                             "sample client using thread pools");

    arg_desc->AddDefaultKey("host", "name", "Host to connect to",
                            CArgDescriptions::eString, "localhost");
    arg_desc->AddKey("port", "N", "TCP port number to connect to",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint("port", new CArgAllow_Integers(0, 0xFFFF));

    arg_desc->AddDefaultKey("delay", "N", "Average delay in milliseconds",
        CArgDescriptions::eInteger, "500");

    arg_desc->AddDefaultKey("threads", "N", "Number of initial threads",
                            CArgDescriptions::eInteger, "5");
    
    arg_desc->AddDefaultKey("maxThreads", "N",
                            "Maximum number of simultaneous threads",
                            CArgDescriptions::eInteger, "10");
    
    arg_desc->AddDefaultKey("requests", "N", "Number of requests to make",
                            CArgDescriptions::eInteger, "100");

    {{
        CArgAllow* constraint = new CArgAllow_Integers(1, 999);
        arg_desc->SetConstraint("threads",    constraint);
        arg_desc->SetConstraint("maxThreads", constraint);
        arg_desc->SetConstraint("requests",   constraint);        
    }}

    SetupArgDescriptions(arg_desc.release());
}


int CThreadedClientApp::Run(void)
{
    const CArgs& args = GetArgs();
    s_Requests = args["requests"].AsInteger();

    CStdPoolOfThreads pool(args["maxThreads"].AsInteger(), s_Requests);
    CRandom rng;

    pool.Spawn(args["threads"].AsInteger());

    const string&  host = args["host"].AsString();
    unsigned short port = args["port"].AsInteger();
    int avg_delay = args["delay"].AsInteger();

    for (unsigned int i = 0;  i < s_Requests;  ++i) {
        pool.AcceptRequest(CRef<ncbi::CStdRequest>
                           (new CConnectionRequest
                            (host, port, rng.GetRand(0, avg_delay*2), eHello)));
        SleepMilliSec(rng.GetRand(0, avg_delay*2));
    }

    // s_Send(host, port, 500, eGoodbye);
    pool.AcceptRequest(CRef<ncbi::CStdRequest>
                       (new CConnectionRequest(host, port, 500, eGoodbye)));

    pool.KillAllThreads(true);

    return 0;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CThreadedClientApp().AppMain(argc, argv);
}


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.11  2006/09/27 21:26:06  joukovv
 * Thread-per-request is finally implemented. Interface changed to enable
 * streams, line-based message handler added, netscedule adapted.
 *
 * Revision 6.10  2004/11/03 16:46:47  ucko
 * Queue the server shutdown request to keep it from occurring prematurely.
 *
 * Revision 6.9  2004/10/18 18:15:09  ucko
 * Support a clean server shutdown request.
 *
 * Revision 6.8  2004/10/08 12:41:49  lavr
 * Cosmetics
 *
 * Revision 6.7  2004/05/17 20:58:22  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.6  2004/05/17 16:01:55  ucko
 * Use SleepMilliSec to sleep only a fraction of a second at a time.
 * Move CVS log to end.
 *
 * Revision 6.5  2002/11/04 21:29:03  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 6.4  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 6.3  2002/01/15 22:24:42  ucko
 * Take advantage of MT_LOCK_cxx2c
 *
 * Revision 6.2  2002/01/07 17:08:44  ucko
 * Display progress.
 *
 * Revision 6.1  2001/12/11 19:55:24  ucko
 * Introduce thread-pool-based servers.
 *
 * ===========================================================================
 */
