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
 *   Standard test for the the NAMEDPIPE-based CONNECTOR
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_namedpipe_connector.hpp>
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "ncbi_conntest.h"

#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;

// Test pipe name
#if defined(NCBI_OS_MSWIN)
    const char* kPipeName = "\\\\.\\pipe\\ncbi\\test_namedpipe";
#elif defined(NCBI_OS_UNIX)
    const char* kPipeName = "./.ncbi_test_namedpipe";
#endif

const size_t kBufferSize = 25*1024;


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest(void);

    virtual void Init(void);
    virtual int  Run(void);

protected:
    void Client(void);
    void Server(int num);

    string   m_PipeName;
    STimeout m_Timeout;
};


CTest::CTest(void)
{
    m_Timeout.sec  = 5;
    m_Timeout.usec = 123456;
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

    // Set CORE log
    CORE_SetLOG(LOG_cxx2c());

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test for named pipe connector");

    // Describe the expected command-line arguments
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
        ("timeout", "Input/output timeout",
         CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("timeout", new CArgAllow_Doubles(0.0, 200.0));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    CArgs args = GetArgs();

    m_PipeName = kPipeName;
    if ( !args["suffix"].AsString().empty() ) {
        m_PipeName += "_" + args["suffix"].AsString();
    }
    ERR_POST(Info << "Using pipe name: " + m_PipeName);

    if (args["timeout"].HasValue()) {
        double tv = args["timeout"].AsDouble();
        m_Timeout.sec  = (unsigned int)  tv;
        m_Timeout.usec = (unsigned int)((tv - m_Timeout.sec) * 1000000);
    }
    if      (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        Client();
    }
    else if (args["mode"].AsString() == "server") {
        SetDiagPostPrefix("Server");
        Server(10);
    }
    else {
        _TROUBLE;
    }

    return 0;
}


void CTest::Client(void)
{
    CONNECTOR connector;

    FILE* logfile = fopen("test_ncbi_namedpipe_connector_client.log", "ab");
    assert(logfile);

    // Tests for NAMEDPIPE CONNECTOR
    ERR_POST(Info << "Starting NAMEDPIPE CONNECTOR test ...");
    ERR_POST(Info << m_PipeName + ", timeout = "
             + NStr::DoubleToString(m_Timeout.sec
                                    + (double) m_Timeout.usec / 1000000.0, 6)
             + " sec.");

    connector = NAMEDPIPE_CreateConnector(m_PipeName.c_str());
    CONN_TestConnector(connector, &m_Timeout, logfile, fTC_SingleBouncePrint);

    connector = NAMEDPIPE_CreateConnector(m_PipeName.c_str(), kBufferSize);
    CONN_TestConnector(connector, &m_Timeout, logfile, fTC_SingleBounceCheck);

    connector = NAMEDPIPE_CreateConnector(m_PipeName.c_str(), kBufferSize);
    CONN_TestConnector(connector, &m_Timeout, logfile, fTC_Everything);

    ERR_POST(Info << "TEST completed successfully");

    fclose(logfile);
}


// The server just bounces all incoming data back to the client
void CTest::Server(int n_cycle)
{
    EIO_Status status;

#ifdef NCBI_OS_UNIX
    // Remove the pipe if it already exists
    CFile(m_PipeName).Remove();
#endif //NCBI_OS_UNIX

    ERR_POST(Info << "Starting NAMEDPIPE CONNECTOR bouncer");
    ERR_POST(Info << m_PipeName + ", timeout = "
             + NStr::DoubleToString(m_Timeout.sec
                                    + (double) m_Timeout.usec / 1000000.0, 6)
             + " sec., n_cycle = " + NStr::UIntToString(n_cycle));

    // Create listening named pipe
    CNamedPipeServer pipe;
    assert(pipe.Create(m_PipeName, &m_Timeout, kBufferSize) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &m_Timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &m_Timeout) == eIO_Success);

    // Accept connections from clients and run test sessions
    do {
        size_t  n_read, n_written;
        char    buf[kBufferSize];

        ERR_POST(Info << "n_cycle = " + NStr::IntToString(n_cycle));

        // Listening pipe
        status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            ERR_POST(Info << "Client connected...");

            // Bounce all incoming data back to the client
            do {
                status = pipe.Read(buf, kBufferSize, &n_read);

                // Report received data
                ERR_POST(Info <<
                         NStr::SizetToString(n_read) + " byte(s) read: " +
                         (n_read ? "" : IO_StatusStr(status)));

                // Write data back to the pipe
                size_t n_done = 0;
                while (n_done < n_read) {
                    if (!n_done) {
                        // NB: first time in the loop -- dump received data
                        NcbiCerr.write(buf, n_read);
                        assert(NcbiCerr.good());
                        NcbiCerr.flush();
                    }
                    status = pipe.Write(buf + n_done, n_read - n_done,
                                        &n_written);
                    if (!n_written) {
                        assert(status != eIO_Success);
                        ERR_POST(Warning << "Failed to write "
                                 + NStr::SizetToString(n_read - n_done)
                                 + " byte(s): "
                                 + IO_StatusStr(status));
                        break;
                    }
                    n_done += n_written;
                    ERR_POST(Info <<
                             NStr::SizetToString(n_written)
                             + " byte(s) written, "
                             + NStr::SizetToString(n_read - n_done)
                             + " remaining (status: "
                             + IO_StatusStr(status)
                             + ')');
                }
            } while (status == eIO_Success);

            ERR_POST(Info << "Client disconnected...");
            status = pipe.Disconnect();
            assert(status == eIO_Success);
            break;

        case eIO_Timeout:
            ERR_POST(Info << "Timeout...");
            break;

        default:
            ERR_POST(string("Cannot listen (status: ")
                     + IO_StatusStr(status)
                     + ')');
            _TROUBLE;
        }
    } while (--n_cycle > 0);
    // Close named pipe
    status = pipe.Close();
    assert(status == eIO_Success  ||  status == eIO_Closed);
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
