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
 * Author:  Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:  Test program for CTrigger
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>
#include <stdlib.h>

#include "test_assert.h"  // This header must go last

#define _STR(s)     #s
#define  STR(s) _STR(s)

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
    virtual int  Run (void);

protected:
    void Client(void);
    void Server(void);

private:
    string       m_Port;
    unsigned int m_Delay;
};


CTest::CTest()
    : m_Delay(0)
{
    // Set error posting and tracing on maximum
    //SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTest::Init(void)
{
    CONNECT_Init(&GetConfig());

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetMiscFlags(CArgDescriptions::fUsageIfNoArgs);
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
                            CArgDescriptions::eString,
                            STR(DEFAULT_PORT));
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

    m_Port = args["port"].AsString();

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
    _ASSERT(s_Trigger.IsSet() == eIO_Closed);
    SleepMilliSec(m_Delay);
    _ASSERT(s_Trigger.IsSet() == eIO_Closed);
    _ASSERT(s_Trigger.Set()   == eIO_Success);
    return 0;
}


/////////////////////////////////
// Client reads until EOF
//

void CTest::Client()
{
    ERR_POST(Info << "Client started...");

    CSocket socket("localhost", NStr::StringToNumeric<unsigned short>(m_Port));

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
    ERR_POST(Info << "Bytes received: " << n_read);
}


/////////////////////////////////
// server
//

void CTest::Server(void)
{
    CDatagramSocket  dsock; /*dummy*/

    // Create listening socket
    CListeningSocket lsock;
    unsigned short port =
        NStr::StringToNumeric<unsigned short>(m_Port,NStr::fConvErr_NoThrow);
    EIO_Status status = lsock.Listen(port);
    if (status == eIO_Success  &&  !port) {
        port = lsock.GetPort(eNH_HostByteOrder);
        if (port) {
            ofstream of(m_Port.c_str());
            if (!(of << port << flush))
                status = eIO_Unknown;
        } else
            status = eIO_Unknown;
    }
    if (status != eIO_Success) {
        ERR_POST("Cannot start server on port " << port
                 << " ("  << (status == eIO_Closed
                              ? "  port busy "
                              : IO_StatusStr(status)) << ')');
    } else
        ERR_POST(Info << "Server started on port " << port << "...");
    _ASSERT(status == eIO_Success);

    // Spawn test thread to activate the trigger
    // (allow thread to run even in a single-thread environment).
    CTriggerThread* thr = new CTriggerThread(m_Delay);
    thr->Run(CThread::fRunAllowST);

    vector<CSocketAPI::SPoll> polls;
    polls.push_back(CSocketAPI::SPoll(&s_Trigger, eIO_ReadWrite));
    polls.push_back(CSocketAPI::SPoll(&lsock,     eIO_ReadWrite));
    polls.push_back(CSocketAPI::SPoll(&dsock,     eIO_ReadWrite));

    for (;;) {
        size_t n;
        static const STimeout kTimeout = {DEFAULT_TIMEOUT - 1, 123456};
        status = CSocketAPI::Poll(polls, &kTimeout, &n);
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
                ERR_POST(Info << "Client disconnected (while polling)...");
                polls[i].m_Pollable = 0;
                continue;
            }
            if (polls[i].m_REvent & eIO_Read) {
                static const STimeout kZero = {0, 0};
                switch (i) {
                case 0:
                    _ASSERT(polls[i].m_Pollable == &s_Trigger);
                    _ASSERT(polls[i].m_REvent == eIO_ReadWrite);
                    ERR_POST(Info << "Trigger activated...");
                    polls.resize(1);
                    _ASSERT(s_Trigger.Reset() == eIO_Success);
                    _ASSERT(CSocketAPI::Poll(polls, &kZero) == eIO_Timeout);
                    ERR_POST(Info << "Now exiting...");
                    return;
                case 1:
                    _ASSERT(polls[i].m_Pollable == &lsock);
                    _ASSERT(polls[i].m_REvent == eIO_Read);
                    status = lsock.Accept(sock, &kZero);
                    _ASSERT(status == eIO_Success);
                    ERR_POST(Info << "Client connected...");
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
                case 2:
                    /* No activity is expected on an unnamed datagram socket */
                    _ASSERT(polls[i].m_Pollable == &dsock);
                    _ASSERT(0);
                    /*FALLTRHU*/
                default:
                    break;
                }
            }
            _ASSERT(i > 1);
            sock = dynamic_cast<CSocket*>(polls[i].m_Pollable);
            _ASSERT(sock);
            if (polls[i].m_REvent & eIO_Read) {
                char buf[12345];
                //ERR_POST(Info << "Client is sending...");
                status = sock->Read(buf, sizeof(buf));
                if (status == eIO_Closed) {
                    delete sock;
                    ERR_POST(Info << "Client disconnected (while reading)...");
                    polls[i].m_Pollable = 0;
                    continue;
                }
            }
            if (polls[i].m_REvent & eIO_Write) {
                char buf[1023];
                buf[0] = 'S';
                buf[1] = '1';
                buf[2] = '\0';
                for (n = 3;  n < sizeof(buf);  n++)
                    buf[n] = rand() & 0xFF;
                //ERR_POST(Info << "Client is receiving...");
                status = sock->Write(buf, sizeof(buf));
                if (status == eIO_Closed) {
                    delete sock;
                    ERR_POST(Info << "Client disconnected (while writing)...");
                    polls[i].m_Pollable = 0;
                    continue;
                }
            }
        }
    }
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    CDiagContext::SetOldPostFormat(true);
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
