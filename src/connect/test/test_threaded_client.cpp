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
*   sample client using a thread pool; designed to connect to
*   test_threaded_server
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <util/random_gen.hpp>
#include <util/thread_pool.hpp>

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  define X_SLEEP(x) ((void) sleep(x))
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#  define X_SLEEP(x) ((void) Sleep(1000 * x))
#else
#  define X_SLEEP(x) ((void) 0)
#endif

BEGIN_NCBI_SCOPE

static          unsigned int s_Requests;
static volatile unsigned int s_Processed = 0;
static CFastMutex            s_Mutex;

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(const string& host, unsigned int port,
                       unsigned int delay) // in seconds
        : m_Host(host), m_Port(port), m_Delay(delay) {}

protected:
    virtual void Process(void);

private:
    string       m_Host;
    unsigned int m_Port;
    unsigned int m_Delay;
};


void CConnectionRequest::Process(void)
{
    CConn_SocketStream stream(m_Host, m_Port);
    X_SLEEP(m_Delay);
    string junk;

    stream >> junk;
    stream << "Hello!" << endl;
    stream >> junk;

    {{
        CFastMutexGuard guard(s_Mutex);
        cerr << "Processed " << ++s_Processed << "/" << s_Requests << endl;
    }}
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
    arg_desc->AddKey("port", "N", "TCP port number on which to listen",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint("port", new CArgAllow_Integers(0, 0xFFFF));

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

    for (unsigned int i = 0;  i < s_Requests;  ++i) {
        X_SLEEP(rng.GetRand(0, 9));
        pool.AcceptRequest(new CConnectionRequest(args["host"].AsString(),
                                                  args["port"].AsInteger(),
                                                  rng.GetRand(0, 9)));
    }

    pool.KillAllThreads(true);
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) {
    return CThreadedClientApp().AppMain(argc, argv);
}
