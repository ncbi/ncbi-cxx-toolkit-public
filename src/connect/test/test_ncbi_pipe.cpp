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

#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


#define TEST_RESULT    99   // Test exit code
#define BUFFER_SIZE  1024   // Size of read buffer


////////////////////////////////
// Auxiliary functions
//

// Reading from pipe
string s_ReadPipe(CPipe* pipe) 
{
    char buf[BUFFER_SIZE];
    unsigned int cnt = 0;
    cnt = pipe->Read(buf, BUFFER_SIZE-1);
    buf[cnt] = 0;
    string str = buf;
    cerr << "Read from pipe: " << str << endl;
    return str;
}


// Writing to pipe
void s_WritePipe(CPipe* pipe, string message) 
{
    pipe->Write(message.c_str(), message.length());
    cerr << "Wrote to pipe: " << message << endl;
}


// Reading from file stream
string s_ReadFile(FILE* fs) 
{
    char buf[BUFFER_SIZE];
    unsigned int cnt = 0;
    cnt = read(fileno(fs), buf, BUFFER_SIZE-1);
    buf[cnt] = 0;
    string str = buf;
    cerr << "Read from file stream: " << str << endl;
    return str;
}

// Writing to file stream
void s_WriteFile(FILE* fs, string message) 
{
    write(fileno(fs), message.c_str(), message.length());
    cerr << "Wrote to file stream: " << message << endl;
}


// Delay a some time. Needs only for better output.
void Delay() 
{
#if defined(NCBI_OS_MSWIN)
    Sleep(300);
#elif defined(NCBI_OS_UNIX)
    sleep(1);
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
    char* args[4];

    // Pipe for reading
    args[1] = "-l";
    args[2] = 0;
    CPipe pipe("ls", args, CPipe::eDoNotUse, CPipe::eText);
    s_ReadPipe(&pipe);
    assert(pipe.Close() == 0);

    // Pipe for writing
    args[1] = "one_argument";
    args[2] = 0;
    pipe.Open(app.c_str(), args, CPipe::eText, CPipe::eDoNotUse);
    s_WritePipe(&pipe, "Child, are You ready?");
    Delay();
    assert(pipe.Close() == TEST_RESULT);

    // Bi-directional pipe
    args[1] = "two";
    args[2] = "arguments";
    args[3] = 0;
    pipe.Open(app.c_str(), args);
    s_WritePipe(&pipe, "Child, are You ready again?");
    Delay();
    str = s_ReadPipe(&pipe);
    assert(str == "Ok. Test 2 running.");
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
    if ( argc == 2) {
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are You ready?");
        cout << "Ok. Test 1 running." << endl;
        exit(TEST_RESULT);
    }

    // Spawned process for bidirectional test
    if ( argc == 3) {
        cerr << endl << "--- CPipe bidirectional test ---" << endl;
        command = s_ReadFile(stdin);
        assert(command == "Child, are You ready again?");
        s_WriteFile(stdout, "Ok. Test 2 running.");
        exit(TEST_RESULT);
    }

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2002/06/10 17:00:30  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
