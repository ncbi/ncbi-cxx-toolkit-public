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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for the Pipe API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <stdio.h>
#include <string.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#else
#   error "Pipe tests configured for Windows and Unix only."
#endif

#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


const int    kTestResult =      99;   // Tests exit code
const size_t kBufferSize = 10*1024;   // I/O buffer size


////////////////////////////////
// Auxiliary functions
//

// Read from pipe
static EIO_Status s_ReadPipe(CPipe& pipe, void* buf, size_t size,
                             size_t* n_read) 
{
    size_t     total = 0;
    size_t     cnt   = 0;
    EIO_Status status;
    
    do {
        status = pipe.Read((char*)buf + total, size - total, &cnt);
        cerr << "Read from pipe: " << cnt << endl;
        cerr.write((char*)buf + total, cnt);
        if ( cnt ) {
            cerr << endl;
        }
        total += cnt;
    } while (status == eIO_Success  &&  total < size);    
    
    *n_read = total;
    cerr << "Read from pipe " << total << " bytes." << endl;
    return status;
}


// Write to pipe
static EIO_Status s_WritePipe(CPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    size_t     total = 0;
    size_t     cnt   = 0;
    EIO_Status status;
    
    do {
        status = pipe.Write((char*)buf + total, size - total, &cnt);
        cerr << "Written to pipe: " << cnt << endl;
        cerr.write((char*)buf + total, cnt);
        if ( cnt ) {
            cerr << endl;
        }
        total += cnt;
    } while (status == eIO_Success  &&  total < size);
    
    *n_written = total;
    cerr << "Written to pipe " << total << " bytes." << endl;
    return status;
}


// Read from file stream
static string s_ReadFile(FILE* fs) 
{
    char   buf[kBufferSize];
    size_t cnt = read(fileno(fs), buf, kBufferSize - 1);
    buf[cnt] = 0;
    string str = buf;
    cerr << "Read from file stream " << cnt << " bytes:" << endl;
    cerr.write(buf, cnt);
    if ( cnt ) {
        cerr << endl;
    }
    return str;
}


// Write to file stream
static void s_WriteFile(FILE* fs, string message) 
{
    write(fileno(fs), message.c_str(), message.length());
    cerr << "Written to file stream " << message.length() << " bytes:" << endl;
    cerr << message << endl;
}


// Read from iostream
static string s_ReadStream(istream& ios)
{
    char   buf[kBufferSize];
    size_t total = 0;
    size_t size  = kBufferSize - 1;
    for (;;) {
        ios.read(buf + total, size);
        size_t cnt = ios.gcount();
        cerr << "Read from iostream: " << cnt << endl;
        cerr.write(buf + total, cnt);
        if ( cnt ) {
            cerr << endl;
        }
        total += cnt;
        size  -= cnt;
        if (size == 0  ||  (cnt == 0  &&  ios.eof())) {
            break;
        }
        ios.clear();
    }
    buf[total] = 0;
    string str = buf;
    cerr << "Read from iostream " << total << " bytes." << endl;
    return str;
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}


int CTest::Run(void)
{
    // Initialization of variables and structures
    const string app = GetArguments().GetProgramName();
    string str;
    vector<string> args;

    char           buf[kBufferSize];
    size_t         n_read    = 0;
    size_t         n_written = 0;
    int            exitcode  = 0;
    string         message;
    EIO_Status     status;
    TProcessHandle handle;

    // Create pipe object
    CPipe pipe;
    STimeout io_timeout    = {2,0};
    STimeout close_timeout = {1,0};

    assert(pipe.SetTimeout(eIO_Read,  &io_timeout)    == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &io_timeout)    == eIO_Success);
    assert(pipe.SetTimeout(eIO_Close, &close_timeout) == eIO_Success);

#if defined(NCBI_OS_UNIX)
    // Pipe for reading (direct from pipe)
    args.push_back("-l");
    assert(pipe.Open("ls", args, CPipe::fStdIn_Close) == eIO_Success);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);
    status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
    assert(status == eIO_Success  ||  status == eIO_Closed);
    assert(n_read > 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

    // Pipe for reading (iostream)
    CConn_PipeStream ios("ls", args, CPipe::fStdIn_Close);
    s_ReadStream(ios);
    assert(ios.GetPipe().Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

#elif defined (NCBI_OS_MSWIN)
    string cmd = GetEnvironment().Get("COMSPEC");
    // Pipe for reading (direct from pipe)
    args.push_back("/c");
    args.push_back("dir *.*");
    assert(pipe.Open(cmd.c_str(), args, CPipe::fStdIn_Close) == eIO_Success);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);
    status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
    assert(status == eIO_Success  ||  status == eIO_Closed);
    assert(n_read > 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

    // Pipe for reading (iostream)
    CConn_PipeStream ios(cmd.c_str(), args, CPipe::fStdIn_Close);
    s_ReadStream(ios);
    assert(ios.GetPipe().Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);
#endif


    // Pipe for writing (direct to pipe)

    args.clear();
    args.push_back("1");
    assert(pipe.Open(app.c_str(), args, CPipe::fStdOut_Close) == eIO_Success);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Unknown);
    assert(n_read == 0);
    message = "Child, are you ready?";
    assert(s_WritePipe(pipe, message.c_str(), message.length(),
                       &n_written) == eIO_Success);
    assert(n_written == message.length());
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == kTestResult);


    // Bidirectional pipe (direct from pipe)

    args.clear();
    args.push_back("2");
    assert(pipe.Open(app.c_str(), args) == eIO_Success);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Timeout);
    assert(n_read == 0);
    message = "Child, are you ready again?";
    assert(s_WritePipe(pipe, message.c_str(), message.length(),
                       &n_written) == eIO_Success);
    assert(n_written == message.length());
    message = "Ok. Test 2 running.";
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == message.length());
    assert(memcmp(buf, message.c_str(), n_read) == 0);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == kTestResult);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Closed);
    assert(n_written == 0);


    // Bidirectional pipe (iostream)

    args.clear();
    args.push_back("3");
    CConn_PipeStream ps(app.c_str(), args, CPipe::fStdErr_Open);
    cout << endl;
    for (int i = 5; i<=10; i++) {
        int value; 
        cout << "How much is " << i << "*" << i << "?\t";
        ps << i << endl;
        ps.flush();
        ps >> value;
        cout << value << endl;
        assert(ps.good());
        assert(value == i*i);
    }
    ps.GetPipe().SetReadHandle(CPipe::eStdErr);
    ps >> str;
    cout << str << endl;
    assert(str == "Done");
    assert(ps.GetPipe().Close(&exitcode) == eIO_Success);
    assert(exitcode == kTestResult);


    // f*OnClose flags test

    args.clear();
    args.push_back("4");
    
    assert(pipe.Open(app.c_str(), args, CPipe::fKeepOnClose) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);
    assert(pipe.Close(&exitcode) == eIO_Timeout);
    assert(exitcode == -1 );
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert(process.Kill());
        assert(!process.IsAlive());
    }}

    args.clear();
    args.push_back("4");

    assert(pipe.Open(app.c_str(), args, CPipe::fKillOnClose) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == -1 );
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(!process.IsAlive());
    }}

    assert(pipe.Open(app.c_str(), args, CPipe::fKeepOnClose) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);
    assert(pipe.Close(&exitcode) == eIO_Timeout);
    assert(exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert(process.Wait(4000) == kTestResult);
        assert(!process.IsAlive());
    }}

    // Done
    cout << "\nTEST execution completed successfully!\n";

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    string command;

    // Check arguments
    if (argc > 2) {
        // Invalid arguments
        exit(1);
    }
    if (argc == 1) {
        // Execute main application function
        return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
    }

    // Internal tests
    int test_num = NStr::StringToInt(argv[1]);

    switch ( test_num ) {
    // Spawned process for unidirectional test
    case 1:
    {
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        command = s_ReadFile(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        cerr << "Ok. Test 1 running." << endl;
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (direct from pipe)
    case 2:
    {
        cerr << endl << "--- CPipe bidirectional test (pipe) ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteFile(stdout, "Ok. Test 2 running.");
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (iostream)
    case 3:
    {
        //cerr << endl << "--- CPipe bidirectional test (iostream) ---"<<endl;
        for (int i = 5; i<=10; i++) {
            int value;
            cin >> value;
            assert(value == i);
            cout << value*value << endl;
            cout.flush();
        }
        cerr << "Done" << endl;
        exit(kTestResult);
    }
    // Test for fKeepOnClose && fKillOnClose flags
    case 4:
    {
        SleepSec(3);
        exit(kTestResult);
    }}

    return -1;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.22  2004/05/17 20:58:22  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.21  2003/12/04 16:29:22  ivanov
 * Added f*OnClose flags test
 *
 * Revision 6.20  2003/11/15 15:35:36  ucko
 * +<stdio.h> (no longer included by ncbi_pipe.hpp)
 *
 * Revision 6.19  2003/11/12 16:43:36  ivanov
 * Using SetReadHandle() through GetPipe()
 *
 * Revision 6.18  2003/09/30 21:00:34  lavr
 * Formatting
 *
 * Revision 6.17  2003/09/23 21:13:32  lavr
 * Adapt test for new stream and pipe API
 *
 * Revision 6.16  2003/09/10 15:54:47  ivanov
 * Rewritten s_ReadPipe/s_WritePipe with using I/O loops.
 * Removed unused Delay().
 *
 * Revision 6.15  2003/09/09 19:42:19  ivanov
 * Added more checks
 *
 * Revision 6.14  2003/09/02 20:38:10  ivanov
 * Changes concerned moving ncbipipe to CONNECT library from CORELIB and
 * rewritting CPipe class using I/O timeouts.
 *
 * Revision 6.13  2003/04/23 20:57:45  ivanov
 * Slightly changed static read/write functions. Minor cosmetics.
 *
 * Revision 6.12  2003/03/07 16:24:44  ivanov
 * Simplify Delay(). Added a delays before checking exit code of a
 * pipe'd application.
 *
 * Revision 6.11  2003/03/06 21:19:16  ivanov
 * Added comment for R6.10:
 * Run the "dir" command for MS Windows via default command interpreter.
 *
 * Revision 6.10  2003/03/06 21:12:35  ivanov
 * *** empty log message ***
 *
 * Revision 6.9  2003/03/03 14:47:21  dicuccio
 * Remplemented CPipe using private platform specific classes.  Remplemented
 * Win32 pipes using CreatePipe() / CreateProcess() - enabled CPipe in windows
 * subsystem
 *
 * Revision 6.8  2002/08/14 14:33:29  ivanov
 * Changed allcalls _exit() to exit() back -- non crossplatform function
 *
 * Revision 6.7  2002/08/13 14:09:48  ivanov
 * Changed exit() to _exit() in the child's branch of the test
 *
 * Revision 6.6  2002/06/24 21:44:36  ivanov
 * Fixed s_ReadFile(), c_ReadStream()
 *
 * Revision 6.5  2002/06/12 14:01:38  ivanov
 * Increased size of read buffer. Fixed s_ReadStream().
 *
 * Revision 6.4  2002/06/11 19:25:56  ivanov
 * Added tests for CPipeIOStream
 *
 * Revision 6.3  2002/06/10 18:49:08  ivanov
 * Fixed argument passed to child process
 *
 * Revision 6.2  2002/06/10 18:35:29  ivanov
 * Changed argument's type of a running child program from char*[]
 * to vector<string>
 *
 * Revision 6.1  2002/06/10 17:00:30  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
