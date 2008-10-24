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
 * Author:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:  Test program for CNamedPipe[Client|Server] classes
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_namedpipe.hpp>
#include <common/test_assert.h>  // This header must go last

#ifdef pipe
#undef pipe
#endif


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
static EIO_Status s_ReadPipe(CNamedPipe& pipe, void* buf, size_t size,
                             size_t* n_read) 
{
    size_t     n_read_total = 0;
    size_t     x_read       = 0;
    EIO_Status status;
    
    do {
        status = pipe.Read((char*)buf + n_read_total,
                           size - n_read_total, &x_read);
        LOG_POST("Read from pipe: " + NStr::UIntToString(x_read) + " bytes");
        n_read_total += x_read;
    } while (status == eIO_Success  &&  n_read_total < size);
    
    if (status == eIO_Timeout) {
    	status = eIO_Success;
    }
    *n_read = n_read_total;
    return status;
}


// Writing to pipe
static EIO_Status s_WritePipe(CNamedPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    size_t     n_written_total = 0;
    size_t     x_written       = 0;
    EIO_Status status;
    
    do {
        status = pipe.Write((char*)buf + n_written_total,
                           size - n_written_total, &x_written);
        LOG_POST("Write to pipe: " + NStr::UIntToString(x_written) + " bytes");
        n_written_total += x_written;
    } while (status == eIO_Success  &&  n_written_total < size);
    
    if (status == eIO_Timeout) {
    	status = eIO_Success;
    }
    *n_written = n_written_total;
    return status;
}



////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
private:
    void Client(int num);
    void Server(void);
private:
    string m_PipeName;
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
                              "Test named pipes API");

    // Describe the expected command-line arguments
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));
    arg_desc->AddDefaultPositional
        ("postfix", "Unique string that will be added to the base pipe name",
         CArgDescriptions::eString, "");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    CArgs args = GetArgs();

    m_PipeName = kPipeName;
    if ( !args["postfix"].AsString().empty() ) {
        m_PipeName += "_" + args["postfix"].AsString();
    }
    LOG_POST("Used pipe name: " + m_PipeName);

    if (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        for (int i=1; i<=3; i++) {
            Client(i);
        }
    }
    else if (args["mode"].AsString() == "server") {
        SetDiagPostPrefix("Server");
        Server();
    }
    else {
        _TROUBLE;
    }
    return 0;
}


/////////////////////////////////
// Named pipe client
//

void CTest::Client(int num)
{
    LOG_POST("\nStart client " + NStr::IntToString(num) + "...\n");
    STimeout timeout = {2,0};

    CNamedPipeClient pipe;
    assert(pipe.IsClientSide());
    assert(pipe.SetTimeout(eIO_Open,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    assert(pipe.Open(m_PipeName,kDefaultTimeout,kSubBlobSize) == eIO_Success);

    char buf[kSubBlobSize];
    size_t n_read    = 0;
    size_t n_written = 0;

    // "Hello" test
    {{
        assert(s_WritePipe(pipe, "Hello", 5, &n_written) == eIO_Success);
        assert(n_written == 5);

        assert(s_ReadPipe(pipe, buf, 2, &n_read) == eIO_Success);
        assert(n_read == 2);
        assert(memcmp(buf, "OK", 2) == 0);

    }}
    // Big binary blob test
    {{
        // Send a very big binary blob
        size_t i;
        unsigned char* blob = (unsigned char*) malloc(kBlobSize);
        for (i = 0; i < kBlobSize; blob[i] = (unsigned char) i, i++); 
        for (i = 0; i < kNumSubBlobs; i++) {
            assert(s_WritePipe(pipe, blob + i*kSubBlobSize, kSubBlobSize,
                               &n_written) == eIO_Success);
            assert(n_written == kSubBlobSize);
        }
        // Receive back a very big binary blob
        memset(blob, 0, kBlobSize);
        for (i = 0;  i < kNumSubBlobs; i++) {
            assert(s_ReadPipe(pipe, blob + i*kSubBlobSize, kSubBlobSize,
                              &n_read) == eIO_Success);
            assert(n_read == kSubBlobSize);
        }
        // Check its content
        for (i = 0; i < kBlobSize; i++) {
            assert(blob[i] == (unsigned char)i);
        }
        free(blob);
        LOG_POST("Blob test is OK...");
    }}
}


/////////////////////////////////
// Named pipe server
//

void CTest::Server(void)
{
    LOG_POST("\nStart server...\n");

#if defined(NCBI_OS_UNIX)
    // Remove the pipe if it is already exists
    CFile(m_PipeName).Remove();
#endif  

    char buf[kSubBlobSize];
    size_t   n_read    = 0;
    size_t   n_written = 0;

    STimeout timeout   = {30,0};
    CNamedPipeServer pipe(m_PipeName, &timeout, kSubBlobSize + 512);

    assert(pipe.IsServerSide());
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    for (;;) {
        LOG_POST("Listening pipe...");

        EIO_Status status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            LOG_POST("Client connected...");

            // "Hello" test
            {{
                assert(s_ReadPipe(pipe, buf, 5 , &n_read) == eIO_Success);
                assert(n_read == 5);
                assert(memcmp(buf, "Hello", 5) == 0);

                assert(s_WritePipe(pipe, "OK", 2, &n_written) == eIO_Success);
                assert(n_written == 2);
            }}

            // Big binary blob test
            {{
                // Receive a very big binary blob
                size_t i;
                unsigned char* blob = (unsigned char*) malloc(kBlobSize);

                for (i = 0;  i < kNumSubBlobs; i++) {
                    assert(s_ReadPipe(pipe, blob + i*kSubBlobSize,
                                      kSubBlobSize, &n_read) == eIO_Success);
                    assert(n_read == kSubBlobSize);
                }
 
                // Check its content
                for (i = 0; i < kBlobSize; i++) {
                    assert(blob[i] == (unsigned char)i);
                }
                // Write back a received big blob
                for (i = 0; i < kNumSubBlobs; i++) {
                    assert(s_WritePipe(pipe, blob + i*kSubBlobSize,
                                       kSubBlobSize, &n_written)
                           == eIO_Success);
                    assert(n_written == kSubBlobSize);
                }
                memset(blob, 0, kBlobSize);
                free(blob);
                LOG_POST("Blob test is OK...");
            }}
            LOG_POST("Disconnect client...");
            assert(pipe.Disconnect() == eIO_Success);
            break;

        case eIO_Timeout:
            LOG_POST("Timeout detected...");
            break;

        default:
            _TROUBLE;
        }
    }
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
