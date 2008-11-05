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
 * Author:  Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:  Test program for CTrigger
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>
#include <stdlib.h>
#include <common/test_assert.h>  // This header must go last


#define _STR(s)      #s
#define STRINGIFY(s) _STR(s)

#define DEFAULT_PORT    5001
#define DEFAULT_TIMEOUT 30


USING_NCBI_SCOPE;


// Trigger object
static CTrigger s_Trigger;


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest();

    virtual void Init(void);
    virtual int  Run(void);

protected:
    void Client(void);
    void Server(void);

private:
    unsigned short m_Port;
    unsigned int   m_Delay;
};


CTest::CTest()
    : m_Port(DEFAULT_PORT), m_Delay(0)
{
    // Set error posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostAllFlags(eDPF_DateTime    | eDPF_Severity |
                        eDPF_OmitInfoSev | eDPF_ErrorID);
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTest::Init(void)
{
    CONNECT_Init(&GetConfig());

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->PrintUsageIfNoArgs();
    if (arg_desc->Exist("h")) {
        arg_desc->Delete("h");
    }
    if (arg_desc->Exist("xmlhelp")) {
        arg_desc->Delete("xmlhelp");
    }

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test TRIGGER API");

    // Describe expected command-line arguments
    arg_desc->AddDefaultKey("port", "port_no",
                            "Port to listen on / connect to",
                            CArgDescriptions::eInteger,
                            STRINGIFY(DEFAULT_PORT));
    arg_desc->SetConstraint("port",
                            new CArgAllow_Integers(DEFAULT_PORT,
                                                   (1 << 16) - 1));
    arg_desc->AddOptionalKey("delay", "delay_time_ms",
                             "Delay (ms) before flipping the trigger",
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("delay",
                            new CArgAllow_Integers(0, DEFAULT_TIMEOUT*1000));
    arg_desc->AddPositional("mode", "Test mode",
                            CArgDescriptions::eString);
    arg_desc->SetConstraint("mode",
                            &(*new CArgAllow_Strings, "client", "server"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    CArgs args = GetArgs();

    m_Port = (unsigned short) args["port"].AsInteger();

    if (args["delay"].HasValue()) {
        m_Delay = args["delay"].AsInteger();
    }

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
    CTriggerThread(unsigned int delay)
        : m_Delay(delay)
    { }

protected:
    virtual ~CTriggerThread() { }
    virtual void* Main(void);

private:
    unsigned int m_Delay;
};


void* CTriggerThread::Main(void)
{
    SleepMilliSec(m_Delay);
    assert(s_Trigger.IsSet() == eIO_Closed);
    assert(s_Trigger.Set()   == eIO_Success);
    return 0;
}


/////////////////////////////////
// Client reads until EOF
//

void CTest::Client()
{
    LOG_POST("\nClient started...\n");

    CSocket socket("localhost", m_Port);

    size_t n_read = 0;
    // the client is greedy but slow
    for (int ok = 0;  ; ok = 1) {
        size_t x_read;
        char buf[12345];
        EIO_Status status = socket.Read(buf, sizeof(buf), &x_read);
        _ASSERT(status == eIO_Success  ||  ok);
        n_read += x_read;
        if (status == eIO_Closed)
            break;
        SleepMilliSec(100);
    }
    LOG_POST("Bytes received: " << n_read);
}


/////////////////////////////////
// server
//

void CTest::Server(void)
{
    LOG_POST("\nServer started...\n");

    // Create listening socket
    CListeningSocket lsock;
    _ASSERT(lsock.Listen(m_Port) == eIO_Success);

    // Spawn test thread to activate the trigger
    // (allow thread to run even in single thread environment).
    CTriggerThread* thr = new CTriggerThread(m_Delay);
    thr->Run(CThread::fRunAllowST);

    vector<CSocketAPI::SPoll> polls;
    polls.push_back(CSocketAPI::SPoll(&s_Trigger, eIO_ReadWrite));
    polls.push_back(CSocketAPI::SPoll(&lsock,     eIO_ReadWrite));

    for (;;) {
        size_t n;
        static const STimeout kTimeout = {DEFAULT_TIMEOUT, 123};
        EIO_Status status = CSocketAPI::Poll(polls, &kTimeout, &n);
        if (status == eIO_Timeout) {
            _ASSERT(!n);
            break;
        }
        _ASSERT(status == eIO_Success  &&  n);
        for (size_t i = 0;  i < polls.size();  i++) {
            CSocket* sock;

            if (polls[i].m_REvent == eIO_Open)
                continue;
            if (polls[i].m_REvent == eIO_Close) {
                _ASSERT(i > 1);
                delete polls[i].m_Pollable;
                LOG_POST("Client disconnected (while polling)...");
                polls[i].m_Pollable = 0;
                continue;
            }
            if (polls[i].m_REvent & eIO_Read) {
                static const STimeout kZero = {0, 0};
                switch (i) {
                case 0:
                    _ASSERT(polls[i].m_Pollable == &s_Trigger);
                    _ASSERT(polls[i].m_REvent == eIO_ReadWrite);
                    LOG_POST("Trigger activated...");
                    polls.resize(1);
                    _ASSERT(s_Trigger.Reset() == eIO_Success);
                    _ASSERT(CSocketAPI::Poll(polls, &kZero) == eIO_Timeout);
                    LOG_POST("Now exiting...");
                    return;
                case 1:
                    _ASSERT(polls[i].m_Pollable == &lsock);
                    _ASSERT(polls[i].m_REvent == eIO_Read);
                    status = lsock.Accept(sock, &kTimeout);
                    _ASSERT(status == eIO_Success);
                    LOG_POST("Client connected...");
                    sock->SetTimeout(eIO_ReadWrite, &kZero);
                    for (n = i + 1;  n < polls.size();  n++) {
                        if (!polls[n].m_Pollable) {
                            polls[n].m_Pollable = sock;
                            polls[n].m_Event = eIO_ReadWrite;
                            break;
                        }
                    }
                    if (n == polls.size())
                        polls.push_back(CSocketAPI::SPoll(sock,eIO_ReadWrite));
                    continue;
                default:
                    break;
                }
            }
            _ASSERT(i > 1);
            sock = dynamic_cast<CSocket*>(polls[i].m_Pollable);
            _ASSERT(sock);
            if (polls[i].m_REvent & eIO_Read) {
                char buf[12345];
                //LOG_POST("Client is sending...");
                status = sock->Read(buf, sizeof(buf));
                if (status == eIO_Closed) {
                    delete sock;
                    LOG_POST("Client disconnected (while reading)...");
                    polls[i].m_Pollable = 0;
                    continue;
                }
                _ASSERT(status == eIO_Success);
            }
            if (polls[i].m_REvent & eIO_Write) {
                char buf[1023];
                buf[0] = 'S';
                buf[1] = '1';
                buf[2] = '\0';
                for (n = 3;  n < sizeof(buf);  n++)
                    buf[n] = rand() & 0xFF;
                //LOG_POST("Client is receiving...");
                status = sock->Write(buf, sizeof(buf));
                if (status == eIO_Closed) {
                    delete sock;
                    LOG_POST("Client disconnected (while writing)...");
                    polls[i].m_Pollable = 0;
                    continue;
                }
                _ASSERT(status == eIO_Success);
            }
        }
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
