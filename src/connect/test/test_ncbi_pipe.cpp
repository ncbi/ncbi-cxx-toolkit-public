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
#include <math.h>

#if defined(NCBI_OS_UNIX)
#  include <signal.h>
#  include <unistd.h>
#elif !defined(NCBI_OS_MSWIN)
#   error "CPipe tests configured for Windows and Unix only."
#endif

#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


#define DEFAULT_TIMEOUT  5


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
        ERR_POST(Info << cnt << " byte(s) read from pipe"+string(&":"[!cnt]));
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
        ERR_POST(Info << cnt << " byte(s) written to pipe"+string(&":"[!cnt]));
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
        char*  res = fgets(buf, sizeof(buf) - 1, fs);
        size_t len = res ? strlen(res) : 0;
        ERR_POST(Info << len << " byte(s) read from file" +string(&":"[!len]));
        if (!len)
            break;
        NcbiCerr.write(res, len);
        NcbiCerr << endl << flush;
        if (res[len - 1] == '\n') {
            if (len-- > 1  &&  res[len - 1] == '\r')
                len--;
            str += string(res, len);
            break;
        }
        str += string(res, len);
    }
    return str;
}


// Read line from a pipe
static string s_ReadLine(CPipe& pipe)
{
    string str;
    for (;;) {
        char   c;
        size_t n;
        EIO_Status status = pipe.Read(&c, 1, &n);
        if (!n) {
            assert(status != eIO_Success);
            break;
        }
        if (c == '\n') {
            SIZE_TYPE len = str.size();
            if (len-- > 1  &&  str[len] == '\r')
                str.resize(len);
            break;
        }
        str += c;
        if (status != eIO_Success)
            break;
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
        ERR_POST(Info << cnt << " byte(s) written to file"+string(&":"[!cnt]));
        if (!cnt)
            break;
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
static void s_ReadStream(istream& is)
{
    size_t total = 0;
    for (;;) {
        char   buf[kBufferSize];
        is.read(buf, sizeof(buf));
        size_t cnt = (size_t) is.gcount();
        ERR_POST(Info << cnt <<" byte(s) read from stream"+string(&":"[!cnt]));
        if (cnt) {
            NcbiCerr.write(buf, cnt);
            NcbiCerr << endl << flush;
            total += cnt;
        } else if (is.eof()  ||  is.bad())
            break;
        is.clear();
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


enum ETestCase {
    ePipeRead,
    ePipeWrite,
    ePipe,
    eStreamRead,
    eStream,
    eStreamStderr,
    eStreamErrOut,
    eFlagsOnClose
};

int CTest::Run(void)
{
    const string&  app = GetArguments().GetProgramName();

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

    static const STimeout timeout = { DEFAULT_TIMEOUT, 0 };

    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Close, &timeout) == eIO_Success);


    // Check bad executable
    ERR_POST(Info << "TEST:  Bad executable");
    assert(pipe.Open("blahblahblah", args) != eIO_Success);


    // Unidirectional pipe (read from pipe)
    args.clear();
    args.push_back(NStr::IntToString(ePipeRead));
    ERR_POST(Info << "TEST:  Unidirectional pipe read");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdIn_Close | share) == eIO_Success);

    assert(pipe.SetReadHandle(CPipe::eStdIn) == eIO_InvalidArg);
    assert(pipe.SetReadHandle(CPipe::eStdErr) == eIO_Success);
    assert(pipe.Read(buf, sizeof(buf), NULL) == eIO_Unknown);
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
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Unidirectional pipe (read from istream)
    args.back() = NStr::IntToString(eStreamRead);
    ERR_POST(Info << "TEST:  Unidirectional stream read");
    CConn_PipeStream is(app.c_str(), args,
                        CPipe::fStdIn_Close | share, 0, &timeout);
    s_ReadStream(is);

    status = is.GetPipe().Close(&exitcode);
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Unidirectional pipe (write to pipe)
    args.back() = NStr::IntToString(ePipeWrite);
    ERR_POST(Info << "TEST:  Unidirectional pipe write");
    assert(pipe.Open(app.c_str(), args,
                     CPipe::fStdOut_Close | share) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Unknown);
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
    args.back() = NStr::IntToString(ePipe);
    ERR_POST(Info << "TEST:  Bidirectional pipe");
    assert(pipe.Open(app.c_str(), args, share) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Timeout);
    assert(n_read == 0);
    str = "Child, are you ready again?\n";
    assert(s_WritePipe(pipe, str.c_str(), str.length(),
                       &n_written) == eIO_Success);
    assert(n_written == str.length());
    str = "Ok.";
    string ack = s_ReadLine(pipe);
    assert(ack.length() == str.length());
    assert(memcmp(ack.data(), str.data(), str.length()) == 0);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);

    status = pipe.Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Unknown);
    assert(n_read == 0);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);


    // Bidirectional pipe (iostream)
    args.back() = NStr::IntToString(eStream);
    ERR_POST(Info << "TEST:  Bidirectional stream");
    CConn_PipeStream ps(app.c_str(), args, share, 0, &timeout);

    NcbiCout << endl;
    for (int i = 5;  i <= 10;  ++i) {
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
    assert(!(ps >> str));

    status = ps.GetPipe().Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Stdout / stderr separation
    ps.clear();
    args.back() = NStr::IntToString(eStreamStderr);
    ERR_POST(Info << "TEST:  Stdout / stderr separation");
    status = ps.GetPipe().Open(app.c_str(), args, CPipe::fStdErr_Open);
    assert(status == eIO_Success);
    ps >> str;
    assert(str == "stdout");
    assert(!(ps >> str));
    ps.clear();
    ps.GetPipe().SetReadHandle(CPipe::eStdErr);
    ps >> str;
    assert(str == "stderr");
    assert(!(ps >> str));
    
    assert(ps.GetPipe().Open(app.c_str(), args) != eIO_Success);

    status = ps.GetPipe().Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Stderr > stdout redirection
    ps.clear();
    args.back() = NStr::IntToString(eStreamErrOut);
    ERR_POST(Info << "TEST:  Stderr > stdout redirection");
    status = ps.GetPipe().Open(app.c_str(), args, CPipe::fStdErr_StdOut);
    assert(status == eIO_Success);
    ps.GetPipe().SetReadHandle(CPipe::eStdOut);
    ps >> str;
    assert(str == "stdout");
    ps >> str;
    assert(str == "stderr");
    assert(!(ps >> str));

    status = ps.GetPipe().Close(&exitcode); 
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    assert(status == eIO_Success  &&  exitcode == kTestResult);

    status = ps.Close();
    assert(status == eIO_Success  ||  status == eIO_Closed);


    // f*OnClose flags tests
    args.back() = NStr::IntToString(eFlagsOnClose);

    ERR_POST(Info << "TEST:  Checking timeout");
    status = pipe.Open(app.c_str(), args,
                       CPipe::fStdIn_Close | CPipe::fStdOut_Close
                       | CPipe::fKeepOnClose);
    assert(status == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    CStopWatch sw(CStopWatch::eStart);
    status = pipe.Close(&exitcode);
    double elapsed = sw.Elapsed();
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    if (status != eIO_Success  ||  elapsed <= DEFAULT_TIMEOUT) {
        assert(status == eIO_Timeout  &&  exitcode == -1);
        {{
            CProcess process(handle, CProcess::eHandle);
            assert(process.IsAlive());
            assert(process.Kill((DEFAULT_TIMEOUT / 2) * 1000));
            assert(!process.IsAlive());
        }}
        status = pipe.Close();
        ERR_POST(Info << "Pipe closed: " << IO_StatusStr(status));
    } else
        ERR_POST(Warning << "Pipe closed okay because of an extended wait");

    ERR_POST(Info << "Checking kill-on-close");
    status = pipe.Open(app.c_str(), args,
                       CPipe::fStdIn_Close | CPipe::fStdOut_Close
                       | CPipe::fKillOnClose | CPipe::fNewGroup);
    assert(status == eIO_Success);
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
    status = pipe.Open(app.c_str(), args,
                       CPipe::fStdIn_Close | CPipe::fStdOut_Close
                       | CPipe::fKeepOnClose);
    assert(status == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    sw.Restart();
    status = pipe.Close(&exitcode);
    elapsed = sw.Elapsed();
    ERR_POST(Info << "Command completed with status " << IO_StatusStr(status)
             << " and exitcode " << exitcode);
    if (status != eIO_Success  ||  elapsed <= DEFAULT_TIMEOUT) {
        assert(status == eIO_Timeout  &&  exitcode == -1);
        {{
            CProcess process(handle, CProcess::eHandle);
            assert(process.IsAlive());
            CProcess::CExitInfo exitinfo;
            exitcode = process.Wait(DEFAULT_TIMEOUT * 1000, &exitinfo);
            string infostr;
            if (exitinfo.IsPresent()) {
                if (exitinfo.IsExited()) {
                    assert(exitinfo.GetExitCode() == exitcode);
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
        status = pipe.Close();
        ERR_POST(Info << "Pipe closed: " << IO_StatusStr(status));
    } else
        ERR_POST(Warning << "Pipe closed okay because of an extended wait");

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
        return 1;
    }
    if (argc == 1) {
        // Execute main application function
        return CTest().AppMain(argc, argv);
    }

    // Internal tests
    x_SetupDiag("Child");
    int test_num = NStr::StringToInt(argv[1]);

    switch ( test_num ) {
    // Spawned process for unidirectional test (writes to pipe)
    case ePipeRead:
    case eStreamRead:
    {
        ERR_POST(Info << "--- CPipe unidirectional test (1) ---");
        // Outputs a multiplication table
        const int kXMax = 22;
        const int kYChunk = 222;

        const int kStep = test_num == ePipeRead ? 0 : 1;
        const int kYFrom  = kYChunk * kStep + 1;
        const int kYTo = kYChunk * (kStep + 1);
        const int kLength = (int)::log10((double)kXMax * kYTo) + 2;
        for (int i = kYFrom;  i <= kYTo;  ++i) {
            for (int j = 1;  j <= kXMax;  ++j)
                NcbiCout << setw(kLength) << i * j;
            NcbiCout << endl;
        }
        ERR_POST(Info << "--- CPipe unidirectional test (1) done ---");
        return kTestResult;
    }
    // Spawned process for unidirectional test (reads from pipe)
    case ePipeWrite:
    {
        ERR_POST(Info << "--- CPipe unidirectional test (2) ---");
        string command = s_ReadLine(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        ERR_POST(Info << "--- CPipe unidirectional test (2) done ---");
        return kTestResult;
    }
    // Spawned process for bidirectional test (direct from pipe)
    case ePipe:
    {
        ERR_POST(Info << "--- CPipe bidirectional test (pipe) ---");
        string command = s_ReadLine(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteLine(stdout, "Ok.");
        ERR_POST(Info << "--- CPipe bidirectional test (pipe) done ---");
        return kTestResult;
    }
    // Spawned process for bidirectional test (iostream)
    case eStream:
    {
        ERR_POST(Info << "--- CPipe bidirectional test (iostream) ---");
        for (int i = 5;  i <= 10;  ++i) {
            int value;
            NcbiCin >> value;
            assert(value == i);
            NcbiCout << value * value << endl << flush;
        }
        NcbiCout << "Done." << endl;
        ERR_POST(Info << "--- CPipe bidirectional test (iostream) done ---");
        return kTestResult;
    }
    // Check stdout / stderr separation
    case eStreamStderr:
    {
        NcbiCout << "stdout" << endl << flush;
        NcbiCerr << "stderr" << endl << flush;
        return kTestResult;
    }
    // Check stderr > stdout redirection
    case eStreamErrOut:
    {
        NcbiCout << "stdout" << endl << flush;
        NcbiCerr << "stderr" << endl << flush;
        return kTestResult;
    }
    // Test for fKeepOnClose && fKillOnClose flags
    case eFlagsOnClose:
    {
#ifdef NCBI_OS_UNIX
        // Ignore pipe signals, convert them to errors
        ::signal(SIGPIPE, SIG_IGN);
#endif /*NCBI_OS_UNIX*/
        ERR_POST(Info << "--- CPipe sleeping test ---");
        SleepMilliSec(DEFAULT_TIMEOUT * 1500);
        ERR_POST(Info << "--- CPipe sleeping test done ---");
        return kTestResult;
    }}

    ERR_POST(Fatal << "Huh?");
    return -1;
}
