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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for the Pipe API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <stdio.h>
#include <string.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <signal.h>
#  include <unistd.h>
#else
#   error "Pipe tests configured for Windows and Unix only."
#endif

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


// Test exit code
const int    kTestResult = 99;

// Use oddly-sized buffers that cross internal block boundaries
const size_t kBufferSize = 1234;


////////////////////////////////
// Auxiliary functions
//

static void x_SetupDiag(const char* who)
{
    // Set error posting and tracing on maximum
    //SetDiagTrace(eDT_Enable);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);

    SetDiagPostPrefix(who);
}


// Read from a pipe (up to "size" bytes)
static EIO_Status s_ReadPipe(CPipe& pipe, void* buf, size_t size,
                             size_t* n_read)
{
    size_t     total = 0;
    EIO_Status status;

    do {
        size_t cnt;
        status = pipe.Read((char*) buf + total, size - total, &cnt);
        ERR_POST(Info << cnt << " byte(s) read from pipe"+string(":"+!cnt));
        if ( cnt ) {
            NcbiCerr.write((char*) buf + total, cnt);
            NcbiCerr << endl << flush;
            total += cnt;
        }
    } while (status == eIO_Success  &&  total < size);    

    *n_read = total;
    ERR_POST(Info << "Total read from pipe: " << total << " byte(s) ["
             << IO_StatusStr(status) << ']');
    return status;
}


// Write to a pipe
static EIO_Status s_WritePipe(CPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    size_t     total = 0;
    EIO_Status status;

    do {
        size_t cnt;
        status = pipe.Write((char*) buf + total, size - total, &cnt);
        ERR_POST(Info << cnt << " byte(s) written to pipe"+string(":"+!cnt));
        if ( cnt ) {
            NcbiCerr.write((char*) buf + total, cnt);
            NcbiCerr << endl << flush;
            total += cnt;
        }
    } while (status == eIO_Success  &&  total < size);

    *n_written = total;
    ERR_POST(Info << "Total written to pipe: " << total << " byte(s) ["
             << IO_StatusStr(status) << ']');
    return status;
}


// Read from a file stream
static string s_ReadLine(FILE* fs) 
{
    string str;
    for (;;) {
        char   buf[80];
        char*  res = fgets(buf, sizeof(buf)-1, fs);
        size_t len = res ? strlen(res) : 0;
        ERR_POST(Info << len << " byte(s) read from file"+string(":"+!len));
        if (!len) {
            break;
        }
        NcbiCerr.write(res, len);
        NcbiCerr << endl << flush;
        if (res[len - 1] == '\n') {
            str += string(res, len - 1);
            break;
        }
        str += string(res, len);
    }
    return str;
}


// Write to a file stream
static void s_WriteLine(FILE* fs, const string& str)
{
    size_t      written = 0;
    size_t      size = str.size();
    const char* data = str.c_str();
    do {
        size_t cnt = fwrite(data + written, 1, size - written, fs);
        ERR_POST(Info << cnt << " byte(s) written to file"+string(":"+!cnt));
        if (!cnt) {
            break;
        }
        NcbiCerr.write(data + written, cnt);
        NcbiCerr << endl << flush;
        written += cnt;
    } while (written < size);
    if (written == size) {
        static const char eol[] = { '\n' };
        fwrite(eol, 1, 1, fs);
    }
    fflush(fs);
}


// Soak up from istream until EOF, dump into NcbiCerr
static void s_ReadStream(istream& ios)
{
    size_t total = 0;

    for (;;) {
        char   buf[kBufferSize];
        ios.read(buf, sizeof(buf));
        size_t cnt = ios.gcount();
        ERR_POST(Info << cnt << " byte(s) read from stream"+string(":"+!cnt));
        if (cnt) {
            NcbiCerr.write(buf, cnt);
            NcbiCerr << endl << flush;
            total += cnt;
        } else if (ios.eof()) {
            break;
        }
        ios.clear();
    }
    ERR_POST(Info << "Total read from istream " << total << " byte(s)");
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
};


void CTest::Init(void)
{
    x_SetupDiag("Parent");
}


int CTest::Run(void)
{
    const string&  app = GetArguments().GetProgramName();

    string         cmd;
    string         str;
    vector<string> args;
    char           buf[kBufferSize];
    size_t         total;
    size_t         n_read;
    size_t         n_written;
    int            exitcode;
    EIO_Status     status;
    TProcessHandle handle;

    ERR_POST(Info << "Starting CPipe test...");


    // Create pipe object

    CPipe pipe;
    const CPipe::TCreateFlags share = CPipe::fStdErr_Share;

    static const STimeout iotimeout = {3, 0};

    assert(pipe.SetTimeout(eIO_Read,  &iotimeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &iotimeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Close, &iotimeout) == eIO_Success);


    // Check bad executable
    ERR_POST(Info << "TEST:  Bad executable");
    assert(pipe.Open("blahblahblah", args) != eIO_Success);


#if defined(NCBI_OS_UNIX)
    cmd = "ls";
    args.push_back("-l");
#elif defined (NCBI_OS_MSWIN)
    cmd = GetEnvironment().Get("COMSPEC");
    args.push_back("/c");
    args.push_back("dir *.*");
#endif


    // Unidirectional pipe (read from pipe)
    ERR_POST(Info << "TEST:  Unidirectional pipe read");
    assert(pipe.Open(cmd.c_str(), args,
                     CPipe::fStdIn_Close | share) == eIO_Success);

    assert(pipe.SetReadHandle(CPipe::eStdIn) == eIO_InvalidArg);
    assert(pipe.SetReadHandle(CPipe::eStdErr) == eIO_Success);
    assert(pipe.Read(buf, sizeof(buf), NULL) == eIO_Closed);
    assert(pipe.SetReadHandle(CPipe::eStdOut) == eIO_Success);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Closed);
    assert(n_written == 0);

    total = 0;
    do {
        status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
        total += n_read;
    } while (status == eIO_Success);
    assert(status == eIO_Closed);
    assert(total > 0);

    status = pipe.Close(&exitcode);
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == 0);


    // Unidirectional pipe (read from iostream)
    ERR_POST(Info << "TEST:  Unidirectional stream read");
    CConn_PipeStream ios(cmd.c_str(), args,
                         CPipe::fStdIn_Close | share, &iotimeout);
    s_ReadStream(ios);

    status = ios.GetPipe().Close(&exitcode);
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == 0);


    // Unidirectional pipe (write to pipe)
    args.clear();
    args.push_back("1");
    ERR_POST(Info << "TEST:  Unidirectional pipe write");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdOut_Close | share) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);
    str = "Child, are you ready?";
    assert(s_WritePipe(pipe, str.c_str(), str.length(),
                       &n_written) == eIO_Success);
    assert(n_written == str.length());
    assert(pipe.CloseHandle(CPipe::eStdIn) == eIO_Success);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Bidirectional pipe (pipe)
    args.clear();
    args.push_back("2");
    ERR_POST(Info << "TEST:  Bidirectional pipe");
    assert(pipe.Open(app.c_str(), args, share) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Timeout);
    assert(n_read == 0);
    str = "Child, are you ready again?\n";
    assert(s_WritePipe(pipe, str.c_str(), str.length(),
                       &n_written) == eIO_Success);
    assert(n_written == str.length());
    str = "Ok.";
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read >= str.length());  // do not count EOL in
    assert(memcmp(buf, str.c_str(), str.length()) == 0);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Closed);
    assert(n_written == 0);


    // Bidirectional pipe (iostream)
    args.clear();
    args.push_back("3");
    ERR_POST(Info << "TEST:  Bidirectional stream");
    CConn_PipeStream ps(app.c_str(), args, share, &iotimeout);

    NcbiCout << endl;
    for (int i = 5; i<=10; i++) {
        int value; 
        NcbiCout << "How much is " << i << "*" << i << "?\t" << flush;
        ps << i << endl;
        ps.flush();
        ps >> value;
        assert(ps.good());
        NcbiCout << value << endl << flush;
        assert(value == i*i);
    }
    ps >> str;
    NcbiCout << str << endl << flush;
    assert(str == "Done.");
    ps.GetPipe().SetReadHandle(CPipe::eStdErr);
    ps >> str;
    assert(!str.empty());

    status = ps.GetPipe().Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // f*OnClose flags tests
    args.clear();
    args.push_back("4");

    ERR_POST(Info << "TEST:  Checking timeout");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdIn_Close | CPipe::fStdOut_Close
                     | CPipe::fKeepOnClose) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Timeout  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert(process.Kill(2000));
        assert(!process.IsAlive());
    }}

    ERR_POST(Info << "Checking kill-on-close");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdIn_Close | CPipe::fStdOut_Close
                     | CPipe::fKillOnClose | CPipe::fNewGroup) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(!process.IsAlive());
    }}

    ERR_POST(Info << "TEST:  Checking extended timeout/kill");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdIn_Close | CPipe::fStdOut_Close
                     | CPipe::fKeepOnClose) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Timeout  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        CProcess::CExitInfo exitinfo;
        exitcode = process.Wait(10000/*10 sec*/, &exitinfo);
        string infostr;
        if (exitinfo.IsPresent()) {
            if (exitinfo.IsExited()) {
                _ASSERT(exitinfo.GetExitCode() == exitcode);
                infostr = "code=" + NStr::IntToString(exitcode);
            } else if (exitinfo.IsSignaled())
                infostr = "signal=" + NStr::IntToString(exitinfo.GetSignal());
        }
        ERR_POST(Info << "Command completed with exit code " << exitcode
                 << " and state "
                 << (!exitinfo.IsPresent() ? "Inexistent" :
                     exitinfo.IsExited()   ? "Terminated" :
                     exitinfo.IsAlive()    ? "Alive"      :
                     exitinfo.IsSignaled() ? "Signaled"   : "Unknown")
                 << (infostr.empty() ? "" : ", ") << infostr);
        assert(exitcode == kTestResult);
        assert(!process.IsAlive());
    }}


    // Done
    ERR_POST(Info << "TEST completed successfully");

    return 0;
}



///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
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
    x_SetupDiag("Child");
    int test_num = NStr::StringToInt(argv[1]);

    switch ( test_num ) {
    // Spawned process for unidirectional test
    case 1:
    {
        ERR_POST(Info << "--- CPipe unidirectional test ---");
        string command = s_ReadLine(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        ERR_POST(Info << "--- CPipe unidirectional test done ---");
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (direct from pipe)
    case 2:
    {
        ERR_POST(Info << "--- CPipe bidirectional test (pipe) ---");
        string command = s_ReadLine(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteLine(stdout, "Ok.");
        ERR_POST(Info << "--- CPipe bidirectional test (pipe) done ---");
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (iostream)
    case 3:
    {
        ERR_POST(Info << "--- CPipe bidirectional test (iostream) ---");
        for (int i = 5; i <= 10; ++i) {
            int value;
            cin >> value;
            assert(value == i);
            NcbiCout << value * value << endl << flush;
        }
        NcbiCout << "Done." << endl;
        ERR_POST(Info << "--- CPipe bidirectional test (iostream) done ---");
        exit(kTestResult);
    }
    // Test for fKeepOnClose && fKillOnClose flags
    case 4:
    {
#ifdef NCBI_OS_UNIX
        // Ignore pipe signals, convert them to errors
        ::signal(SIGPIPE, SIG_IGN);
#endif /*NCBI_OS_UNIX*/
        ERR_POST(Info << "--- CPipe sleeping test ---");
        SleepMilliSec(6000);
        ERR_POST(Info << "--- CPipe sleeping test done ---");
        exit(kTestResult);
    }}

    return -1;
}
