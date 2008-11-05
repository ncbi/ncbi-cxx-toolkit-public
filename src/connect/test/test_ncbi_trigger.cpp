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
 * Author:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:  Test program for CTrigger
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;

// Number of sockets and starting port number
unsigned short kCount = 2;
unsigned short kStartPort = 4000;

// Trigger object
static CTrigger s_Trigger;



////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
private:
    void Client(void);
    void Server(void);
};


void CTest::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test named pipes API");

    // Describe the expected command-line arguments
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));
    arg_desc->AddDefaultPositional
        ("postfix", "Unique string that will be added to the base pipe name",
         CArgDescriptions::eString, "");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    CArgs args = GetArgs();

    if (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        Client();
    }
    else if (args["mode"].AsString() == "server") {
        SetDiagPostPrefix("Server");
        Server();
    }
    else {
        _TROUBLE;
    }
    return 0;
}


/////////////////////////////////
// Thread to activate trigger
//

class CTriggerThread : public CThread
{
public:
    CTriggerThread(void) {}
protected:
    ~CTriggerThread() {}
    virtual void* Main();
};

void* CTriggerThread::Main(void)
{
    SleepSec(10);
    assert(s_Trigger.IsSet() == eIO_Closed);
    assert(s_Trigger.Set() == eIO_Success);
    return 0;
}


/////////////////////////////////
// Named pipe client
//

void CTest::Client()
{
    unsigned short port = kStartPort;// + 10;
    CConn_SocketStream stream("localhost", port);

    string junk;
    stream >> junk;
    stream << "Hello!" << endl;
    stream >> junk;
}


/////////////////////////////////
// server
//

void CTest::Server(void)
{
    LOG_POST("\nStart server...\n");

    // Create listening sockets
    vector<CListeningSocket*> lsocks;
    lsocks.resize(kCount+1);
    for (int i=0; i<kCount; i++) {
        lsocks[i] = new CListeningSocket;
        assert(lsocks[i]->Listen(kStartPort+i) == eIO_Success);
    }

    // Create polls
    vector<CSocketAPI::SPoll> polls;
/*
    polls.resize(kCount+1);
    for (int i=0; i<kCount; i++) {
        polls[i].m_Pollable = lsocks[i];
        polls[i].m_Event    = eIO_Read;
        polls[i].m_REvent   = eIO_Open;
    }
    polls[kCount].m_Pollable = &s_Trigger;
    polls[kCount].m_Event    = eIO_Read;
    polls[kCount].m_REvent   = eIO_Open;
*/
    polls.resize(1);
    polls[0].m_Pollable = &s_Trigger;
    polls[0].m_Event    = eIO_Read;
    polls[0].m_REvent   = eIO_Open;

    // Spawn test thread to activate trigger.
    // Allow thread to run even in single thread environment.
    CTriggerThread* thr = new CTriggerThread();
    thr->Run(CThread::fRunAllowST);


    // Poll

    size_t   count   = 0;
    STimeout timeout = {30,0};

    LOG_POST("Listening...");
    EIO_Status status = ncbi::CSocketAPI::Poll(polls, &timeout, &count);
    cout << "status = " << status << endl;
    cout << "count  = " << count << endl;

    switch (status) {
    case eIO_Success:
        LOG_POST("Client is connected...");
        for (int i=0; i<kCount; i++) {
            if (polls[i].m_REvent == eIO_Read) {
                LOG_POST("Port = " << kStartPort + i);
                break;
            }
        }
        if (polls[kCount].m_REvent == eIO_Read) {
            LOG_POST("Trigger activated");
        }
        break;
    case eIO_Timeout:
        LOG_POST("Timeout detected...");
        break;
    default:
        _TROUBLE;
    }


    if (status == eIO_Success  &&  count > 0) {
        cout << "Ok!" << endl;
    }

}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
