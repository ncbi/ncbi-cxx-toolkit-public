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
 * File Description:  Test program for CPipe class
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbipipe.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif

#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


#define TEST_RESULT       99   // Test exit code
#define BUFFER_SIZE  1024*10   // Size of read buffer


////////////////////////////////
// Auxiliary functions
//

// Reading from pipe
static string s_ReadPipe(CPipe& pipe) 
{
    char buf[BUFFER_SIZE];
    size_t read = 0;
    size_t size = BUFFER_SIZE-1;
    for (;;) {
        size_t cnt = pipe.Read(buf+read, size);
        read += cnt;
        size -= cnt;
        if (size == 0  ||  cnt == 0 )
            break;
    }
    buf[read] = 0;
    string str = buf;
    cerr << "Read from pipe: " << str << endl;
    return str;
}


// Writing to pipe
static void s_WritePipe(CPipe& pipe, string message) 
{
    pipe.Write(message.c_str(), message.length());
    cerr << "Wrote to pipe: " << message << endl;
}


// Reading from file stream
static string s_ReadFile(FILE* fs) 
{
    char buf[BUFFER_SIZE];
    size_t cnt = read(fileno(fs), buf, BUFFER_SIZE-1);
    buf[cnt] = 0;
    string str = buf;
    cerr << "Read from file stream: " << str << endl;
    return str;
}

// Writing to file stream
static void s_WriteFile(FILE* fs, string message) 
{
    write(fileno(fs), message.c_str(), message.length());
    cerr << "Wrote to file stream: " << message << endl;
}

// Reading from iostream
static string s_ReadStream(istream& ios)
{
    char buf[BUFFER_SIZE];
    size_t read = 0;
    size_t size = BUFFER_SIZE-1;
    for (;;) {
        ios.read(buf+read, size);
        size_t cnt = ios.gcount();
        read += cnt;
        size -= cnt;
        if (size == 0  ||  (cnt == 0 && ios.eof()))
            break;
        ios.clear();
    }
    buf[read] = 0;
    string str = buf;
    cerr << "Read from iostream: " << str << endl;
    return str;
}


// Delay a some time. Needs only for more proper output.
void Delay() 
{
#if defined(NCBI_OS_MSWIN)
    Sleep(300);
#elif defined(NCBI_OS_UNIX)
    usleep(300000);
#elif defined(NCBI_OS_MAC)
    ?
#endif
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
    SetDiagPostLevel(eDiag_Warning);
}


int CTest::Run(void)
{
    // Initialization of variables and structures
    const string app = GetArguments().GetProgramName();
    string str;
    vector<string> args;

    // Pipe for reading (direct from pipe)
    args.push_back("-l");
    CPipe pipe("ls", args, CPipe::eDoNotUse, CPipe::eText);
    s_ReadPipe(pipe);
    assert(pipe.Close() == 0);

    // Pipe for reading (iostream)
    pipe.Open("ls", args, CPipe::eDoNotUse, CPipe::eText);
    CPipeIOStream ios(pipe);
    s_ReadStream(ios);
    assert(pipe.Close() == 0);

    // Pipe for writing (direct from pipe)
    args.clear();
    args.push_back("one_argument");
    pipe.Open(app.c_str(), args, CPipe::eText, CPipe::eDoNotUse);
    s_WritePipe(pipe, "Child, are you ready?");
    Delay();
    assert(pipe.Close() == TEST_RESULT);

    // Pipe for writing (iostream)
    pipe.Open(app.c_str(), args, CPipe::eText, CPipe::eDoNotUse);
    s_WritePipe(pipe, "Child, are you ready?");
    Delay();
    assert(pipe.Close() == TEST_RESULT);
    
    // Bidirectional pipe (direct from pipe)
    args.clear();
    args.push_back("two");
    args.push_back("arguments");
    pipe.Open(app.c_str(), args);
    Delay();
    s_WritePipe(pipe, "Child, are you ready again?");
    Delay();
    str = s_ReadPipe(pipe);
    assert(str == "Ok. Test 2 running.");
    assert(pipe.Close() == TEST_RESULT);

    // Bidirectional pipe (iostream)
    args.clear();
    args.push_back("one");
    args.push_back("two");
    args.push_back("three");
    pipe.Open(app.c_str(), args, CPipe::eText,  CPipe::eText,  CPipe::eText);
    CPipeIOStream ps(pipe);
    Delay();
    cout << endl;
    for (int i = 5; i<=10; i++) {
        int value; 
        cout << "How much is " << i << "*" << i << "?\t";
        ps << i << endl;
        ps.flush();
        ps >> value;
        cout << value << endl;
        assert(value == i*i);
    }
    ps.SetReadHandle(CPipe::eStdErr);
    ps >> str;
    cout << str << endl;
    assert(str == "Done");
    assert(pipe.Close() == TEST_RESULT);

    cout << endl << "TEST execution completed successfully!" << endl << endl;

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    string command;
    // Spawned process for unidirectional test
    if (argc == 2) {
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are you ready?");
        cout << "Ok. Test 1 running." << endl;
        _exit(TEST_RESULT);
    }

    // Spawned process for bidirectional test (direct from pipe)
    if (argc == 3) {
        cerr << endl << "--- CPipe bidirectional test (pipe) ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteFile(stdout, "Ok. Test 2 running.");
        _exit(TEST_RESULT);
    }

    // Spawned process for bidirectional test (iostream)
    if (argc == 4) {
        //cerr << endl << "--- CPipe bidirectional test (iostream) ---" << endl;
        for (int i = 5; i<=10; i++) {
            int value;
            cin >> value;
            assert(value == i);
            cout << value*value << endl;
            cout.flush();
        }
        cerr << "Done" << endl;
        _exit(TEST_RESULT);
    }

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
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
