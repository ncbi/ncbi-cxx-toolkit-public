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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_pipe.hpp>
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

// Delay a some time. Needs only for more proper output.
static void Delay() 
{
    //SleepMilliSec(300);
}


// Reading from pipe
static EIO_Status s_ReadPipe(CPipe& pipe, void* buf, size_t size,
                             size_t* n_read) 
{
    EIO_Status status = pipe.Read(buf, size, n_read, CPipe::eStdOut);
    Delay();
    cerr << "Read from pipe " << *n_read << " bytes:" << endl;
    cerr.write((char*)buf, *n_read);
    cerr << endl;
    return status;
}


// Writing to pipe
static EIO_Status s_WritePipe(CPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    EIO_Status status = pipe.Write(buf, size, n_written);
    cerr << "Written to pipe " << *n_written << " bytes:" << endl;
    cerr.write((char*)buf, *n_written);
    cerr << endl;
    return status;
}


// Reading from file stream
static string s_ReadFile(FILE* fs) 
{
    char buf[kBufferSize];
    size_t cnt = read(fileno(fs), buf, kBufferSize-1);
    buf[cnt] = 0;
    string str = buf;
    Delay();
    cerr << "Read from file stream " << cnt << " bytes:" << endl;
    cerr.write(buf, cnt);
    cerr << endl;
    return str;
}


// Writing to file stream
static void s_WriteFile(FILE* fs, string message) 
{
    write(fileno(fs), message.c_str(), message.length());
    cerr << "Written to file stream " << message.length() << " bytes:" << endl;
    cerr << message << endl;
}


// Reading from iostream
static string s_ReadStream(istream& ios)
{
    char buf[kBufferSize];
    size_t total = 0;
    size_t size  = kBufferSize-1;
    for (;;) {
        ios.read(buf+total, size);
        size_t cnt = ios.gcount();
        cerr << "Read from iostream: " << cnt << endl;
        total += cnt;
        size  -= cnt;
        if (size == 0  ||  (cnt == 0 && ios.eof()))
            break;
        ios.clear();
    }
    buf[total] = 0;
    string str = buf;
    Delay();
    cerr << "Read from fstream " << total << " bytes:" << endl;
    cerr << str << endl;
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

    char        buf[kBufferSize];
    size_t      n_read    = 0;
    size_t      n_written = 0;
    int         exitcode  = 0;
    string      message;
    EIO_Status  status;

    // Create pipe object
    CPipe pipe;
    STimeout timeout = {2,0};
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Close, &timeout) == eIO_Success);

#if defined(NCBI_OS_UNIX)
    // Pipe for reading (direct from pipe)
    args.push_back("-l");
    assert(pipe.Open("ls", args, CPipe::fStdIn_Close) == eIO_Success);
    Delay();
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);
    status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
    assert(status == eIO_Success  ||  status == eIO_Timeout);
    assert(n_read > 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

    // Pipe for reading (iostream)
    assert(pipe.Open("ls", args, CPipe::fStdIn_Close) == eIO_Success);
    Delay();
    CPipeIOStream ios(pipe);
    s_ReadStream(ios);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

#elif defined (NCBI_OS_MSWIN)
    string cmd = GetEnvironment().Get("COMSPEC");
    // Pipe for reading (direct from pipe)
    args.push_back("/c");
    args.push_back("dir *.*");
    assert(pipe.Open(cmd.c_str(), args, CPipe::fStdIn_Close) == eIO_Success);
    Delay();
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);
    status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
    assert(status == eIO_Success  ||  status == eIO_Timeout);
    assert(n_read > 0);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);

    // Pipe for reading (iostream)
    assert(pipe.Open(cmd.c_str(), args, CPipe::fStdIn_Close) == eIO_Success);
    Delay();
    CPipeIOStream ios(pipe);
    s_ReadStream(ios);
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == 0);
#endif

    // Pipe for writing (direct to pipe)
    args.clear();
    args.push_back("one");
    assert(pipe.Open(app.c_str(), args, CPipe::fStdOut_Close) == eIO_Success);
    Delay();
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
    args.push_back("one");
    args.push_back("two");
    assert(pipe.Open(app.c_str(), args) == eIO_Success);
    Delay();
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Timeout);
    assert(n_read == 0);
    message = "Child, are you ready again?";
    assert(s_WritePipe(pipe, message.c_str(), message.length(),
                       &n_written) == eIO_Success);
    assert(n_written == message.length());
    message = "Ok. Test 2 running.";
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Success);
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
    args.push_back("one");
    args.push_back("two");
    args.push_back("three");
    assert(pipe.Open(app.c_str(), args, CPipe::fStdErr_Open) == eIO_Success);
    Delay();
    CPipeIOStream ps(pipe);
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
    ps.SetReadHandle(CPipe::eStdErr);
    ps >> str;
    cout << str << endl;
    assert(str == "Done");
    assert(pipe.Close(&exitcode) == eIO_Success);
    assert(exitcode == kTestResult);

    cout << "\nTEST execution completed successfully!\n";

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    string command;

    // Invalid arguments
    if (argc > 4) {
        exit(1);
    }

    // Spawned process for unidirectional test
    if (argc == 2) {
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        command = s_ReadFile(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        cout << "Ok. Test 1 running." << endl;
        Delay();
        exit(kTestResult);
    }

    // Spawned process for bidirectional test (direct from pipe)
    if (argc == 3) {
        cerr << endl << "--- CPipe bidirectional test (pipe) ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteFile(stdout, "Ok. Test 2 running.");
        Delay();
        exit(kTestResult);
    }

    // Spawned process for bidirectional test (iostream)
    if (argc == 4) {
        //cerr << endl << "--- CPipe bidirectional test (iostream) ---"<<endl;
        for (int i = 5; i<=10; i++) {
            int value;
            cin >> value;
            assert(value == i);
            cout << value*value << endl;
            cout.flush();
        }
        cerr << "Done" << endl;
        Delay();
        exit(kTestResult);
    }

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
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

