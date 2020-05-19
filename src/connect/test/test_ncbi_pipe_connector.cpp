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
 * Author:  Denis Vakatov, Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:
 *   Standard test for the PIPE CONNECTOR
 *
 */

#include <ncbi_pch.hpp>
/* Cancel __wur (warn unused result) ill effects in GCC */
#ifdef   _FORTIFY_SOURCE
#  undef _FORTIFY_SOURCE
#endif /*_FORTIFY_SOURCE*/
#define  _FORTIFY_SOURCE 0
#include "../ncbi_priv.h"
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_pipe_connector.hpp>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_util.h>
#include "ncbi_conntest.h"

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#  include <fcntl.h>  // For _O_BINARY
#  define DEVNULL   "NUL"
// On Windows can't do large pipe transactions because of limited pipe size
#  define CONNTEST  fTC_MultiBouncePrint
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  define DEVNULL   "/dev/null"
#  define CONNTEST  fTC_Everything
#else
#  error "CPipe tests configured for Windows and Unix only."
#endif

#include "test_assert.h"  // This header must go last


#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
/* Microsoft does not want to use POSIX name, not to accept POSIX compliance */
#  define sys_read(a, b, c)    _read(a, b, (unsigned int)(c))
#  define sys_write(a, b, c)   _write(a, b, (unsigned int)(c))
#  define sys_fileno           _fileno
#else
#  define sys_read              read
#  define sys_write             write
#  define sys_fileno            fileno
#endif /*NCBI_COMPILER_MSVC && _MSC_VER>=1400*/


USING_NCBI_SCOPE;


const size_t   kBufferSize  = 1024;    // I/O buffer size
const STimeout kTimeout     = {2, 0};  // I/O timeout


////////////////////////////////
// Auxiliary functions
//


static void x_SetupDiag(const char* who)
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

    SetDiagPostPrefix(who);

    // Set CORE log
    CORE_SetLOG(LOG_cxx2c());
}


// Read from file stream
// CAUTION:  read(2) can short read
static string s_ReadLine(FILE* fs) 
{
    string str;
    for (;;) {
        char    c;
        ssize_t cnt = ::sys_read(sys_fileno(fs), &c, 1);
        if (cnt <= 0)
            break;
        assert(cnt == 1);
        if (c == '\n')
            break;
        str += c;
    }
    return str;
}


// Write to file stream
// CAUTION:  write(2) can short write
static void s_WriteLine(FILE* fs, string str)
{
    size_t      written = 0;
    size_t      size = str.size();
    const char* data = str.c_str();
    do { 
        ssize_t cnt = ::sys_write(sys_fileno(fs), data + written, size - written);
        if (cnt <= 0)
            break;
        written += cnt;
    } while (written < size);
    if (written == size) {
        static const char eol[] = { '\n' };
        (void) ::sys_write(sys_fileno(fs), eol, sizeof(eol));
    }
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
    x_SetupDiag("Parent");
}


int CTest::Run(void)
{
    CONN        conn;
    CONNECTOR   connector;
    char        buf[kBufferSize];
    size_t      n_read;
    size_t      n_written;
    EIO_Status  status;

    g_NCBI_ConnectRandomSeed
        = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    ::srand(g_NCBI_ConnectRandomSeed);

    // Run the test
    ERR_POST(Info << "Starting PIPE CONNECTOR test...");

    const string app = GetArguments().GetProgramName();
    string cmd, str;
    vector<string> args;

    // Pipe connector for reading
#if defined(NCBI_OS_UNIX)
    cmd = "ls";
    args.push_back("-l");
#elif defined (NCBI_OS_MSWIN)
    cmd = GetEnvironment().Get("COMSPEC");
    args.push_back("/c");
    args.push_back("dir *.*");
#endif
    connector = PIPE_CreateConnector(cmd, args, (CPipe::fStdIn_Close |
                                                 CPipe::fStdErr_Share));
    assert(connector);

    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_Read,  &kTimeout);
    CONN_SetTimeout(conn, eIO_Close, &kTimeout);
    status = CONN_Write(conn, buf, kBufferSize, &n_written, eIO_WritePersist);
    assert(status == eIO_Closed);
    assert(n_written == 0);

    size_t total = 0;
    do {
        status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPersist);
        total += n_read;
        NcbiCout.write(buf, n_read);
    } while (n_read > 0);
    NcbiCout << endl;
    NcbiCout.flush();
    assert(total > 0); 
    assert(status == eIO_Closed);
    assert(CONN_Close(conn) == eIO_Success);


    // Pipe connector for writing
    args.clear();
    args.push_back("one");
    connector = PIPE_CreateConnector(app, args, (CPipe::fStdOut_Close |
                                                 CPipe::fStdErr_Share));
    assert(connector);

    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_Write, &kTimeout);
    CONN_SetTimeout(conn, eIO_Close, &kTimeout);
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Unknown);
    assert(n_read == 0);

    str = "Child, are you ready?";
    status = CONN_Write(conn, str.c_str(), str.length(),
                        &n_written, eIO_WritePersist);
    assert(status == eIO_Success);
    assert(n_written == str.length());
    assert(CONN_Close(conn) == eIO_Success);


    // Bidirectional pipe
    args.push_back("two");
    connector = PIPE_CreateConnector(app, args, CPipe::fStdErr_Share);
    assert(connector);

    assert(CONN_Create(connector, &conn) == eIO_Success);
    CONN_SetTimeout(conn, eIO_ReadWrite, &kTimeout);
    CONN_SetTimeout(conn, eIO_Close,     &kTimeout);
    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Timeout);
    assert(n_read == 0);

    str = "Child, are you ready again?\n";
    status = CONN_Write(conn, str.c_str(), str.length(),
                        &n_written, eIO_WritePersist);
    assert(status == eIO_Success);
    assert(n_written == str.length());

    str = "Ok. Test 2 running.";
    status = CONN_ReadLine(conn, buf, kBufferSize, &n_read);
    assert(status == eIO_Success);
    assert(n_read >= str.length());  // do not count EOL in
    assert(memcmp(buf, str.c_str(), str.length()) == 0);

    status = CONN_Read(conn, buf, kBufferSize, &n_read, eIO_ReadPlain);
    assert(status == eIO_Closed);
    assert(n_read == 0);
    assert(CONN_Close(conn) == eIO_Success);


    // Bidirectional standard CONNECTOR test
#ifdef NCBI_OS_UNIX
    if (::rand() & 1) {
        cmd = "cat";
        args.clear();
    } else
#endif // NCBI_OS_UNIX
    {
        cmd = app;
        args.push_back("3");
    }
    FILE* logfile = fopen(DEVNULL, "ab");
    assert(logfile);

    connector = PIPE_CreateConnector(cmd, args, CPipe::fStdErr_Share);
    assert(connector);

    CONN_TestConnector(connector, &kTimeout, logfile, CONNTEST);

    fclose(logfile);


    // All done!
    ERR_POST(Info << "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Invalid arguments
    if (argc > 4) {
        exit(1);
    }

    if (argc == 1) {
        // Execute main application function
        return CTest().AppMain(argc, argv);
    }

    x_SetupDiag("Child");

    // Spawned process for unidirectional test
    if (argc == 2) {
        ERR_POST(Info << "--- PIPE CONNECTOR unidirectional test ---");
        string command = s_ReadLine(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        NcbiCout << "Ok. Test 1 running." << endl;
        ERR_POST(Info << "--- PIPE CONNECTOR unidirectional test done ---");
        return 0;
    }

    // Spawned process for bidirectional test
    if (argc == 3) {
        ERR_POST(Info << "--- PIPE CONNECTOR bidirectional test ---");
        string command = s_ReadLine(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteLine(stdout, "Ok. Test 2 running.");
        ERR_POST(Info << "--- PIPE CONNECTOR bidirectional test done ---");
        return 0;
    }

    // Spawned process for bidirectional standard test
    if (argc == 4) {
#ifdef NCBI_OS_MSWIN
        _setmode(_fileno(stdin),  _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif // NCBI_OS_MSWIN
        ERR_POST(Info << "--- PIPE CONNECTOR bidirectional standard test ---");
        // NB: NcbiStreamCopy(cout, cin) can't be used because of buffering
        do {
            int c;
            if ((c = cin.get()) != EOF) {
                cout.put(c).flush();
            }
        } while (cin  &&  cout);
        ERR_POST(Info << "--- PIPE CONNECTOR bidirectional standard test done ---");
        return 0;
    }

    ERR_POST(Fatal << "Huh?");
    return -1;
}
