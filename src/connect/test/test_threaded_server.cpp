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
*   sample server using a thread pool
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.4  2004/05/17 20:58:22  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 6.3  2002/01/25 15:39:30  ucko
* Completely reorganized threaded servers.
*
* Revision 6.2  2002/01/15 22:24:43  ucko
* Take advantage of MT_LOCK_cxx2c
*
* Revision 6.1  2001/12/11 19:55:24  ucko
* Introduce thread-pool-based servers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <connect/threaded_server.hpp>

BEGIN_NCBI_SCOPE


class CTestThreadedServer : public CThreadedServer
{
public:
    CTestThreadedServer(unsigned int port, unsigned int threads,
                        unsigned int max, unsigned int queue_size)
        : CThreadedServer(port)
        {
            m_InitThreads = threads;
            m_MaxThreads  = max;
            m_QueueSize   = queue_size;
        }
    virtual void Process(SOCK sock);
};


void CTestThreadedServer::Process(SOCK sock)
{
    CConn_SocketStream stream(sock);
    string junk;
    
    stream << "Hello!" << endl;
    stream >> junk;
    stream << "Goodbye!" << endl;
}


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


int CThreadedServerApp::Run(void)
{
    const CArgs& args = GetArgs();

    CTestThreadedServer server(args["port"].AsInteger(),
                               args["threads"].AsInteger(),
                               args["maxThreads"].AsInteger(),
                               args["queue"].AsInteger());
    server.Run();

    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) {
    return CThreadedServerApp().AppMain(argc, argv);
}
