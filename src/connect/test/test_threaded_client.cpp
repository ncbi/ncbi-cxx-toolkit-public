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
