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
 * Author:  Denis Vakatov, Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:
 *   Standard test for the PIPE CONNECTOR
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/ncbi_pipe.hpp>
#include <connect/ncbi_pipe_connector.hpp>
#include "../ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <stdio.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#else
#   error "Pipe tests configured for Windows and Unix only."
#endif

#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


const size_t   kBufferSize  = 1024;    // I/O buffer size
const STimeout kTimeout     = {2, 0};  // I/O timeout


////////////////////////////////
// Auxiliary functions
//

// Reading from file stream
static string s_ReadFile(FILE* fs) 
{
    char buf[kBufferSize];
    size_t cnt = read(fileno(fs), buf, kBufferSize-1);
    buf[cnt] = 0;
    string str = buf;
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


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CTest::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Log and data log streams
    CORE_SetLOGFILE(stderr, 0/*false*/);
}


int CTest::Run(void)
{
    CONN        conn;
    CONNECTOR   connector;
    char        buf[kBufferSize];
    size_t      n_read    = 0;
    size_t      n_written = 0;
    string      message;
    EIO_Status  status;

    // Run the test
    LOG_POST("Starting the PIPE CONNECTOR test...\n");

    // Initialization of variables and structures
    const string app = GetArguments().GetProgramName();
    string str;
    vector<string> args;

    // Pipe connector for reading
#if defined(NCBI_OS_UNIX)
    args.push_back("-l");
    connector = PIPE_CreateConnector("ls", args, CPipe::fStdIn_Close);
#elif defined (NCBI_OS_MSWIN)
    string cmd = GetEnvironment().Get("COMSPEC");
    args.push_back("/c");
    args.push_back("dir *.*");
    connector = PIPE_CreateConnector(cmd, args, CPipe::fStdIn_Close);
#endif
    assert(connector);
    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_Read,  &kTimeout);
    CONN_SetTimeout(conn, eIO_Close, &kTimeout);
    status = CONN_Write(conn, buf, kBufferSize, &n_written, eIO_WritePersist);
    assert(status == eIO_Unknown);
    assert(n_written == 0);
    size_t n_read_total = 0;
    do {
        status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPersist);
        n_read_total += n_read;
        NcbiCout.write(buf, n_read);
    } while (n_read > 0);
    NcbiCout << endl;
    NcbiCout.flush();
    assert(n_read_total > 0); 
    assert(status == eIO_Closed);
    assert(CONN_Close(conn) == eIO_Success);

    // Pipe connector for writing
    args.clear();
    args.push_back("one");
    connector = PIPE_CreateConnector(app, args, CPipe::fStdOut_Close);
    assert(connector);
    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_Write, &kTimeout);
    CONN_SetTimeout(conn, eIO_Close, &kTimeout);
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Unknown);
    assert(n_read == 0);
    message = "Child, are you ready?";
    status = CONN_Write(conn, message.c_str(), message.length(),
                        &n_written, eIO_WritePersist);
    assert(status == eIO_Success);
    assert(n_written == message.length());
    assert(CONN_Close(conn) == eIO_Success);
   
    // Bidirectional pipe
    args.clear();
    args.push_back("one");
    args.push_back("two");
    connector = PIPE_CreateConnector(app, args);
    assert(connector);
    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_ReadWrite, &kTimeout);
    CONN_SetTimeout(conn, eIO_Close,     &kTimeout);
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Timeout);
    assert(n_read == 0);
    message = "Child, are you ready again?";
    status = CONN_Write(conn, message.c_str(), message.length(),
                        &n_written, eIO_WritePersist);
    assert(status == eIO_Success);
    assert(n_written == message.length());
    message = "Ok. Test 2 running.";
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Success);
    assert(n_read == message.length());
    assert(memcmp(buf, message.c_str(), n_read) == 0);
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Closed);
    assert(n_read == 0);
    assert(CONN_Close(conn) == eIO_Success);

    LOG_POST("\nTEST execution completed successfully!\n");
    CORE_SetLOG(0);

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Invalid arguments
    if (argc > 3) {
        exit(1);
    }

    // Spawned process for unidirectional test
    if (argc == 2) {
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        string command = s_ReadFile(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        NcbiCout << "Ok. Test 1 running." << endl;
        exit(0);
    }

    // Spawned process for bidirectional test (direct from pipe)
    if (argc == 3) {
        cerr << endl << "--- CPipe bidirectional test (pipe) ---" << endl;
        string command = s_ReadFile(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteFile(stdout, "Ok. Test 2 running.");
        exit(0);
    }

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/05/17 20:58:22  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/02/23 15:23:43  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 1.7  2003/12/04 16:35:54  ivanov
 * Removed unused function Delay()
 *
 * Revision 1.6  2003/11/15 15:35:36  ucko
 * +<stdio.h> (no longer included by ncbi_pipe.hpp)
 *
 * Revision 1.5  2003/09/09 19:47:34  ivanov
 * Fix for previous commit
 *
 * Revision 1.4  2003/09/09 19:42:19  ivanov
 * Added more checks
 *
 * Revision 1.3  2003/09/05 14:04:20  ivanov
 * CONN_Read(..., eIO_ReadPersist) can return eIO_Closed also
 *
 * Revision 1.2  2003/09/03 21:37:25  ivanov
 * Removed Linux ESPIPE workaround
 *
 * Revision 1.1  2003/09/02 20:39:40  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
