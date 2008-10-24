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
 *   Standard test for the the NAMEDPIPE-based CONNECTOR
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_namedpipe.hpp>
#include <connect/ncbi_namedpipe_connector.hpp>
#include "../ncbi_priv.h"
#include "ncbi_conntest.h"
#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


// Test pipe name
#if defined(NCBI_OS_MSWIN)
    const char* kPipeName = "\\\\.\\pipe\\ncbi\\test_ncbi_namedpipe";
#elif defined(NCBI_OS_UNIX)
    const char* kPipeName = "./.test_ncbi_namedpipe";
#endif

const size_t kBufferSize = 25*1024;


static void Client(STimeout timeout)
{
    CONNECTOR  connector;

    CORE_SetLOGFILE(stderr, 0/*false*/);

    FILE* data_file = fopen("test_ncbi_namedpipe_connector_client.log", "ab");
    assert(data_file);

    // Tests for NAMEDPIPE CONNECTOR
    LOG_POST(string("Starting the NAMEDPIPE CONNECTOR test ...\n\n") +
             kPipeName + ", timeout = " +
             NStr::DoubleToString(timeout.sec+(double)timeout.usec/1000000,6)+
             " sec.\n");

    connector = NAMEDPIPE_CreateConnector(kPipeName);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBouncePrint);

    connector = NAMEDPIPE_CreateConnector(kPipeName, kBufferSize);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBounceCheck);

    connector = NAMEDPIPE_CreateConnector(kPipeName, kBufferSize);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    fclose(data_file);
}


// The server just bounces all incoming data back to the client
static void Server(STimeout timeout, int n_cycle)
{
    EIO_Status status;

#if defined(NCBI_OS_UNIX)
    // Remove the pipe if it is already exists
    CFile(kPipeName).Remove();
#endif  

    LOG_POST(string("Starting the NAMEDPIPE CONNECTOR bouncer ...\n\n") +
             kPipeName + ", timeout = " +
             NStr::DoubleToString(timeout.sec+(double)timeout.usec/1000000,6)+
             ", n_cycle = " + NStr::UIntToString(n_cycle) + "\n");

    // Create listening named pipe
    CNamedPipeServer pipe;
    assert(pipe.Create(kPipeName, &timeout, kBufferSize) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    // Accept connections from clients and run test sessions
    do {
        size_t  n_read, n_written;
        char    buf[kBufferSize];

        LOG_POST("Server(n_cycle = " + NStr::UIntToString(n_cycle) + ")");

        // Listening pipe
        status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            LOG_POST("Client connected...");

            // Bounce all incoming data back to the client
            do {
                status = pipe.Read(buf, kBufferSize, &n_read);

                // Dump received data
                LOG_POST(NStr::UIntToString(n_read) + " byte(s) read: " +
                         (n_read ? "" : IO_StatusStr(status)));
                NcbiCout.write(buf, n_read);
                assert(NcbiCout.good());
                NcbiCout.flush();

                // Write data back to the pipe
                size_t n_total = 0;
                while (n_total < n_read) {
                    status = pipe.Write(buf + n_total, n_read - n_total,
                                        &n_written);
                    if (!n_written) {
                        assert(status != eIO_Success);
                        LOG_POST("Failed to write "
                                 + NStr::UIntToString(n_read)
                                 + " byte(s): "
                                 + IO_StatusStr(status));
                        break;
                    }
                    n_total += n_written;
                    LOG_POST(NStr::UIntToString(n_written)
                             + " byte(s) written, "
                             + NStr::UIntToString(n_read - n_total)
                             + " remaining (status: "
                             + IO_StatusStr(status)
                             + ')');
                }
            } while (status == eIO_Success);
            assert(status == eIO_Timeout  ||  status == eIO_Closed);

            LOG_POST("Disconnect client...");
            assert(pipe.Disconnect() == eIO_Success);
            break;

        case eIO_Timeout:
            LOG_POST("Timeout detected...");
            break;

        default:
            _TROUBLE;
        }
    } while (--n_cycle > 0);
    // Close named pipe
    status = pipe.Close();
    assert(status == eIO_Success  ||  status == eIO_Closed);
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
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test for named pipe connector");

    // Describe the expected command-line arguments
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));

    arg_desc->AddOptionalPositional 
        ("timeout", "Input/output timeout",
         CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("timeout", new CArgAllow_Doubles(0.0, 200.0));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    // Log and data log streams
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CArgs args = GetArgs();
    STimeout timeout = {1, 123456};

    if (args["timeout"].HasValue()) {
        double tv = args["timeout"].AsDouble();
        timeout.sec  = (unsigned int) tv;
        timeout.usec = (unsigned int) ((tv - timeout.sec) * 1000000);
    }
    if (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        Client(timeout);
    }
    else if (args["mode"].AsString() == "server") {
        if (!args["timeout"].HasValue()) {
            timeout.sec = 5;
        }
        SetDiagPostPrefix("Server");
        Server(timeout, 10);
    }
    else {
        _TROUBLE;
    }
    CORE_SetLOG(0);
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
