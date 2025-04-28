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
 * Author:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:  Test program for CNamedPipe[Client|Server] classes
 *
 */

#include <ncbi_pch.hpp>
#include "../ncbi_priv.h"
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_test.hpp>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_namedpipe.hpp>
#ifdef NCBI_OS_MSWIN
#  include <timeapi.h>
#endif // NCBI_OS_MSWIN

#include "test_assert.h"  // This header must go last

#define _STR(x)  #x
#define  STR(s)  _STR(s)

#define DEFAULT_TIMEOUT  3


USING_NCBI_SCOPE;


// Test pipe name
#if defined(NCBI_OS_MSWIN)
const string kPipeName = "\\\\.\\pipe\\ncbi\\test_pipename";
#elif defined(NCBI_OS_UNIX)
const string kPipeName = "./.ncbi_test_pipename";
#endif

const size_t kNumSubBlobs = 10;
const size_t kSubBlobSize = 10*1024;
const size_t kBlobSize    = kNumSubBlobs * kSubBlobSize;


////////////////////////////////
// Auxiliary functions
//

// Reading from pipe
static EIO_Status s_ReadPipe(CNamedPipe& pipe, void* buf, size_t buf_size,
                             size_t msg_size, size_t* n_read, bool silent = false)
{
    size_t     n_read_total = 0;
    size_t     x_read       = 0;
    EIO_Status status;

    do {
        status = pipe.Read((char*) buf + n_read_total,
                           buf_size - n_read_total, &x_read);
        if (!silent) {
            ERR_POST(Info <<
                     "Read from pipe: "
                     + NStr::UIntToString((unsigned int) x_read) + " byte(s): "
                     + string(IO_StatusStr(status)));
        }
        n_read_total += x_read;
    } while (status == eIO_Success  &&  n_read_total < msg_size);

    *n_read = n_read_total;
    return status;
}


// Writing to pipe
static EIO_Status s_WritePipe(CNamedPipe& pipe, const void* buf, size_t size,
                              size_t* n_written, bool silent = false)
{
    size_t     n_written_total = 0;
    size_t     x_written       = 0;
    EIO_Status status;

    do {
        status = pipe.Write((char*) buf + n_written_total,
                           size - n_written_total, &x_written);
        if (!silent) {
            ERR_POST(Info <<
                     "Write to pipe: "
                     + NStr::UIntToString((unsigned int) x_written) + " byte(s): "
                     + string(IO_StatusStr(status)));
        }
        n_written_total += x_written;
    } while (status == eIO_Success  &&  n_written_total < size);

    *n_written = n_written_total;
    return status;
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest(void);
    ~CTest();

    virtual void Init(void);
    virtual int  Run (void);

protected:
    int Client(int num);
    int Server(void);

    string   m_PipeName;
    STimeout m_Timeout;

    static const STimeout kZeroTimeout;

#ifdef NCBI_OS_MSWIN
private:
    static bool xsm_UsePrecisionTimer;
    static void x_ReportSleepPrecision(void);
#endif // NCBI_OS_MSWIN
};


const STimeout CTest::kZeroTimeout = { 0, 0 };


#ifdef NCBI_OS_MSWIN
bool CTest::xsm_UsePrecisionTimer = false;

void CTest::x_ReportSleepPrecision(void)
{
    CStopWatch sw(CStopWatch::eStart);
    SleepMilliSec(1);
    double elapsed = sw.Elapsed();
    ERR_POST(Info << "Sleep(1ms) = " << setprecision(6) << elapsed * 1000.0 << "ms");
}
#endif // NCBI_OS_MSWIN


CTest::CTest(void)
{
    m_Timeout.sec  = DEFAULT_TIMEOUT;
    m_Timeout.usec = 0;
}


CTest::~CTest()
{
#ifdef NCBI_OS_MSWIN
    if (xsm_UsePrecisionTimer) {
        timeEndPeriod(1);
    }
#endif // NCBI_OS_MSWIN
}


void CTest::Init(void)
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

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test named pipe API");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("basename", "basename",
         "Base name for the pipe",
         CArgDescriptions::eString, kPipeName);
    arg_desc->AddDefaultKey
        ("suffix", "suffix",
         "Unique string that will be added to the base pipe name",
         CArgDescriptions::eString, "");
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));
    arg_desc->AddOptionalPositional 
        ("timeout", "Input/output timeout"
         " (default = " + string(STR(DEFAULT_TIMEOUT)) + ')',
         CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("timeout", new CArgAllow_Doubles(0.0, 200.0));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    // Set randomization seed for the test
    g_NCBI_ConnectRandomSeed = CNcbiTest::SetRandomSeed();    

#ifdef NCBI_OS_MSWIN
    x_ReportSleepPrecision();
    if (::rand() & 1) {
        ERR_POST(Info << "Using precision timer");
        xsm_UsePrecisionTimer = true;
        timeBeginPeriod(1);
        x_ReportSleepPrecision();
    }
#endif // NCBI_OS_MSWIN

    const CArgs& args = GetArgs();
        
    m_PipeName = args["basename"].AsString();
    if ( m_PipeName.empty() ) {
        m_PipeName = kPipeName;
    }
    if ( !args["suffix"].AsString().empty() ) {
        m_PipeName += '_' + args["suffix"].AsString();
    }
    ERR_POST(Info << "Using pipe name: " + m_PipeName);
    if (args["timeout"].HasValue()) {
        double tv = args["timeout"].AsDouble();
        m_Timeout.sec  = (unsigned int)  tv;
        m_Timeout.usec = (unsigned int)((tv - m_Timeout.sec) * kMicroSecondsPerSecond);
    }
    if     (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        for (int i = 1;  i <= 3;  ++i) {
            int exitcode = Client(i);
            if (exitcode)
                return exitcode;
        }
    }
    else if (args["mode"].AsString() == "server") {
        SetDiagPostPrefix("Server");
        return Server();
    }
    else {
        _TROUBLE;
    }

    return 0;
}


/////////////////////////////////
// Named pipe client
//

int CTest::Client(int num)
{
    int exitcode = 0;

    if (::rand() & 1) {
        SleepMilliSec(100);
    }
    ERR_POST(Info << "Starting client " + NStr::IntToString(num) + "...");

    CNamedPipeClient pipe;
    assert(pipe.IsClientSide());
    assert(pipe.SetTimeout(eIO_Open,  &m_Timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &m_Timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &m_Timeout) == eIO_Success);

    EIO_Status status;

    CDeadline timeout(g_STimeoutToCTimeout(&m_Timeout));
    for (;;) {
        // Wait for server to come up online
        status = pipe.Open(m_PipeName, kDefaultTimeout, kSubBlobSize,
                           CNamedPipeClient::fNoLogIfClosed);
        if (status == eIO_Success) {
            break;
        }
        if (timeout.IsExpired()) {
            status  = eIO_Timeout;
        }
        if (status != eIO_Closed) {
            ERR_POST(Error << "Open() failed: " << IO_StatusStr(status));
            _TROUBLE;
        }
        ERR_POST(Info << "Waiting for server...");
        SleepMilliSec(500);
    }

    char buf[kSubBlobSize];
    size_t n_read    = 0;
    size_t n_written = 0;

    if ((::rand() & 1)  &&  num > 2) {
        // Super quick write-and-flee behavior
        ERR_POST(Info << "Quitting the server!");
        assert(s_WritePipe(pipe, "Quit!", 5, &n_written) == eIO_Success);
        assert(n_written == 5);
        if (rand() & 1) {
            status = pipe.Close();
            if (status != eIO_Success) {
                ERR_POST(Error << "Close() failed: " << IO_StatusStr(status));
                _TROUBLE;
            }
        }
        exitcode = 2;
        goto out;
    }

    if (::rand() & 1) {
        SleepMilliSec(100);
    }
    // "Hello" test
    {{
        assert(s_WritePipe(pipe, "Hello", 5, &n_written) == eIO_Success);
        assert(n_written == 5);
        assert(s_ReadPipe(pipe, buf, sizeof(buf), 2, &n_read) == eIO_Success);
        assert(n_read == 2);
        assert(::memcmp(buf, "OK", 2) == 0);
    }}

    if (::rand() & 1) {
        SleepMilliSec(100);
    }
    // Big binary blob test
    {{
        // Send a very big binary blob
        size_t i;
        unsigned char* blob = new unsigned char[kBlobSize + 1];
        for (i = 0;  i < kBlobSize;  ++i) {
            blob[i] = (unsigned char) i;
        }
        for (i = 0;  i < kNumSubBlobs;  ++i) {
            assert(s_WritePipe(pipe, blob + i*kSubBlobSize, kSubBlobSize,
                               &n_written) == eIO_Success);
            assert(n_written == kSubBlobSize);
        }
        // Receive the blob back
        ::memset(blob, 0, kBlobSize);
        assert(s_ReadPipe(pipe, blob, kBlobSize + 1, kBlobSize, &n_read)
               == eIO_Success);
        // Check its contents
        for (i = 0;  i < kBlobSize;  ++i) {
            assert(blob[i] == (unsigned char) i);
        }
        delete[] blob;
        ERR_POST(Info << "Blob test is OK!");
    }}

    if (::rand() & 1) {
        SleepMilliSec(100);
    }
    // Download test
    {{
        _ASSERT(sizeof(buf) == kSubBlobSize);
        if (::rand() & 1) {
            ERR_POST(Info << "Downloading using non-blocking reads");
            pipe.SetTimeout(eIO_Read, &kZeroTimeout);
        } else {
            ERR_POST(Info << "Downloading using blocking reads");
        }
        n_read = 0;
        bool expired;
        CStopWatch sw(CStopWatch::eStart);
        timeout = CDeadline(g_STimeoutToCTimeout(&m_Timeout));
        do {
            status = s_ReadPipe(pipe, buf, sizeof(buf), sizeof(buf), &n_written, true);
            n_read += n_written/*temp*/;
            expired = timeout.IsExpired();
            if (expired) {
                break;
            }
            if (!n_written  &&  status != eIO_Timeout) {
                break;
            }
        } while (n_read < kBlobSize * sizeof(buf));
        double interval = sw.Elapsed();
        ERR_POST(Info << (expired ? "Download expired: " : "Downloaded ")
                 << NStr::NumericToString(n_read, NStr::fWithCommas)
                 << " byte(s) in "
                 << setprecision(6) << interval << " second(s) @ "
                 << setprecision(3) << (interval ? n_read / (1024.0 * interval) : 0.0)
                 << "KB/s: " << IO_StatusStr(status));
        if (expired)
            status = eIO_Timeout;
    }}

    if (::rand() & 1) {
        SleepMilliSec(100);
    }
    if ((::rand() & 1)  &&  status == eIO_Success) {
        status = s_ReadPipe(pipe, buf, sizeof(buf), sizeof(buf), &n_read);
        if (status == eIO_Success) {
            ERR_POST(Error << "Extra read succeeded");
            _TROUBLE;
        }
        ERR_POST(Info << "Error expected, " << IO_StatusStr(status));
    }

    if (::rand() & 1) {
        status = pipe.Close();
        if (status != eIO_Success) {
            ERR_POST(Error << "Close() failed: " << IO_StatusStr(status));
            _TROUBLE;
        }
        status = s_ReadPipe(pipe, buf, sizeof(buf), sizeof(buf), &n_read);
        ERR_POST(Info << "Error expected, " << IO_StatusStr(status));
        _ASSERT(status == eIO_Unknown);
    }

 out:
    ERR_POST(Info << "TEST completed successfully");
    return exitcode;
}


/////////////////////////////////
// Named pipe server
//

int CTest::Server(void)
{
    int exitcode = 1;  //  Normally getting killed at the test completion

    ERR_POST(Info << "Starting server...");

    char buf[kSubBlobSize];
    size_t n_read    = 0;
    size_t n_written = 0;

    CNamedPipeServer pipe(m_PipeName, &m_Timeout, kSubBlobSize + 512);
    assert(pipe.IsServerSide());

    EIO_Status status;

    for (int n = 1;  n <= 10;  ++n) {
        assert(pipe.SetTimeout(eIO_Read,  &m_Timeout) == eIO_Success);
        assert(pipe.SetTimeout(eIO_Write, &m_Timeout) == eIO_Success);

        if (::rand() & 1) {
            SleepMilliSec(100);
        }
        ERR_POST(Info << "Listening on \"" + m_PipeName + "\", round "
                 + NStr::IntToString(n) + "...");

        status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            ERR_POST(Info << "Client connected!");

            if (::rand() & 1) {
                SleepMilliSec(100);
            }
            // "Hello" test
            {{
                assert(s_ReadPipe(pipe, buf, sizeof(buf), 5, &n_read) == eIO_Success);
                assert(n_read == 5);
                if (memcmp(buf, "Quit!", 5) == 0) {
                    ERR_POST(Info << "Quit received!");
                    exitcode = 0;
                    goto done;
                }
                assert(memcmp(buf, "Hello", 5) == 0);
                assert(s_WritePipe(pipe, "OK", 2, &n_written) == eIO_Success);
                assert(n_written == 2);
            }}

            if (::rand() & 1) {
                SleepMilliSec(100);
            }
            // Big binary blob test
            {{
                // Receive a very big binary blob
                size_t i;
                unsigned char* blob = new unsigned char[kBlobSize + 1];
                assert(s_ReadPipe(pipe, blob, kBlobSize +1, kBlobSize, &n_read)
                       == eIO_Success);
                assert(n_read == kBlobSize);
                // Check its contents
                for (i = 0;  i < kBlobSize;  ++i) {
                    assert(blob[i] == (unsigned char) i);
                }
                // Write the blob back
                for (i = 0;  i < kNumSubBlobs;  ++i) {
                    assert(s_WritePipe(pipe, blob + i*kSubBlobSize,
                                       kSubBlobSize, &n_written)
                           == eIO_Success);
                    assert(n_written == kSubBlobSize);
                }
                ::memset(blob, 0, kBlobSize);
                delete[] blob;
                ERR_POST(Info << "Blob test is OK!");
            }}

            if (::rand() & 1) {
                SleepMilliSec(100);
            }
            // Upload test
            {{
                _ASSERT(sizeof(buf) == kSubBlobSize);
                for (size_t i = 0;  i < sizeof(buf);  ++i) {
                    buf[i] = char(i & 0xFF);
                }
                if (::rand() & 1) {
                    ERR_POST(Info << "Uploading using non-blocking writes");
                    pipe.SetTimeout(eIO_Write, &kZeroTimeout);
                } else {
                    ERR_POST(Info << "Uploading using blocking writes");
                }
                bool expired;
                n_written = 0;
                CDeadline timeout(g_STimeoutToCTimeout(&m_Timeout));
                do {
                    size_t n_done = n_written % sizeof(buf);
                    size_t n_todo = sizeof(buf) - n_done;
                    status = s_WritePipe(pipe, &buf[n_done], n_todo, &n_read, true);
                    n_written += n_read/*temp*/;
                    expired = timeout.IsExpired();
                    if (expired) {
                        break;
                    }
                    if (!n_read  &&  status != eIO_Timeout) {
                        break;
                    }
                } while (n_written < kBlobSize * sizeof(buf));
                ERR_POST(Info << (expired ? "Upload expired: " : "Uploaded ")
                         << NStr::NumericToString(n_written, NStr::fWithCommas)
                         << " byte(s): " << IO_StatusStr(status));
            }}

            if (::rand() & 1) {
                SleepMilliSec(100);
            }
            if (::rand() & 1) {
                status = s_ReadPipe(pipe, buf, sizeof(buf), sizeof(buf), &n_read);
                if (status == eIO_Success) {
                    ERR_POST(Error << "Extra read succeeded");
                    _TROUBLE;
                }
                ERR_POST(Info << "Error expected, " << IO_StatusStr(status));
            }

            ERR_POST(Info << "Disconnecting client...");
            status = pipe.Disconnect();
            if (status != eIO_Success) {
                ERR_POST(Error << "Disconnect() failed: " << IO_StatusStr(status));
                _TROUBLE;
            }
            if (::rand() & 1) {
                status = s_ReadPipe(pipe, buf, sizeof(buf), sizeof(buf), &n_read);
                ERR_POST(Info << "Error expected, " << IO_StatusStr(status));
                _ASSERT(status == eIO_Unknown);
            }
            ERR_POST(Info << "Round completed successfully!");
            break;

        case eIO_Timeout:
            ERR_POST(Info << "Timeout...");
            break;

        default:
            ERR_POST(Error << IO_StatusStr(status));
            _TROUBLE;
        }
    }

 done:
    // Close named pipe
    status = pipe.Close();
    assert(status == eIO_Success  ||  status == eIO_Closed);
    return exitcode;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv);
}
