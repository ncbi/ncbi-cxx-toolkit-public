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
 * Author:  Denis Vakatov
 *
 * File Description:
 *
 *   A very simple CGI to produce an HTTP response with controlled
 *   (via arguments) contents and timing.
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgictx.hpp>

#if defined(NCBI_OS_UNIX)
#  include <signal.h>
#endif


using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//  CCgiIOTestApplication::
//

class CCgiIOTestApplication : public CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

private:
    void x_SetupArgs(void);
};


#if defined(NCBI_OS_UNIX)
static unsigned int s_SIGPIPE_Count = 0;
extern "C" {
    static void s_SIGPIPE_Handler(int)
    {
        s_SIGPIPE_Count++;
    }
}
#endif


void CCgiIOTestApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Describe possible cmd-line and HTTP entries
    x_SetupArgs();

#if defined(NCBI_OS_UNIX)
    struct sigaction sa_old;
    struct sigaction sa_new;
    memset(&sa_new, 0, sizeof(sa_new));
    sa_new.sa_handler = s_SIGPIPE_Handler;
    _VERIFY(sigaction(SIGPIPE, &sa_new, &sa_old) == 0);
#endif
}


static const CArgValue& s_Arg(const char* arg_name)
{
    return CNcbiApplication::Instance()->GetArgs()[arg_name];
}

static void s_Sleep(const char* wait_time)
{
    unsigned long t = (unsigned long)
        (s_Arg(wait_time).AsDouble() * 1000000.0);
    _TRACE("SleepMicroSec(" << t << ")");
    if ( t ) {
        SleepMicroSec(t);
    }
}


int CCgiIOTestApplication::ProcessRequest(CCgiContext& ctx)
{
#if defined(NCBI_OS_UNIX)
    unsigned int sig_count = s_SIGPIPE_Count;
#  define X_TRACE(expr) \
    do { \
        if (sig_count != s_SIGPIPE_Count) { \
            sig_count = s_SIGPIPE_Count; \
            _TRACE("SIGPIPE! = " << s_SIGPIPE_Count);  \
        } \
        _TRACE(expr); \
} while (0)
#else
#  define X_TRACE _TRACE
#endif


    GetDiagContext().Extra().Print("ReqID", s_Arg("id").AsString());

    static bool printed_args = false;
    if ( !printed_args ) {
        string args;
        X_TRACE("Args:: \n" << GetArgs().Print(args) << "\n\n");
        printed_args = true;
    }


    X_TRACE("Enter ProcessRequest()");

    CCgiResponse& response = ctx.GetResponse();
    response.SetContentType("text/plain");

    size_t buf_size = (size_t) s_Arg("chunk_size").AsInteger();
    AutoArray<char> buf(buf_size);
    for (size_t j = 0;  j < buf_size;  j++) {
        static char x[101] = "ABCDE6789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n";
        buf[j] = x[j%100];
    }

    //
    X_TRACE("sleep(header_before)");
    s_Sleep("header_before");

    X_TRACE("WriteHeader()");
    response.WriteHeader();

    X_TRACE("flush()");
    response.out().flush();

    X_TRACE("sleep(header_after)");
    s_Sleep("header_after");

    X_TRACE("entering data write loop...");

    int data_chunks = s_Arg("data_chunks").AsInteger();
    for (int i = 0 ;  i < data_chunks;  i++) {
        X_TRACE("write(buf)");
        response.out().write(buf.get(), buf_size);
        X_TRACE("flush()");
        response.out().flush();
        X_TRACE("sleep(chunk_wait)");
        s_Sleep("chunk_wait");
    }

    X_TRACE("Exit ProcessRequest()");
    return 0;
}



void CCgiIOTestApplication::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test CGI reaction to aborted connection");

    //
    arg_desc->AddKey("header_before",
                     "header_before",
                     "Wait (in sec) before sending HTTP header",
                     CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("header_before", new CArgAllow_Doubles(0.0, 1000.0));

    //
    arg_desc->AddKey("header_after",
                     "header_after",
                     "Wait (in sec) before sending data, after HTTP header",
                     CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("header_after", new CArgAllow_Doubles(0.0, 1000.0));

    //
    arg_desc->AddKey("data_chunks",
                     "data_chunks",
                     "Number of data chunks to send in the response's body",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("data_chunks", new CArgAllow_Integers(0, 1000));

    //
    arg_desc->AddKey("chunk_size",
                     "chunk_size",
                     "Size of each data chunk",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("chunk_size", new CArgAllow_Integers(1, 1024 * 1024 * 100));

    //
    arg_desc->AddKey("chunk_wait",
                     "chunk_wait",
                     "Wait (in sec) between sending data chunks",
                     CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("chunk_wait", new CArgAllow_Doubles(0.0, 1000.0));


    //
    arg_desc->AddKey("id",
                     "id",
                     "Alphanum string to help identify the request",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("id", new CArgAllow_String(CArgAllow_Symbols::eAlnum));


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    CDiagContext::SetOldPostFormat(false);
    SetDiagFixedPostLevel(eDiag_Trace);
    return CCgiIOTestApplication().AppMain(argc, argv, 0, eDS_ToStdlog);
}
