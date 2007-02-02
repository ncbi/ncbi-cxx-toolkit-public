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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   Test for log rotation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>

#include <util/logrotate.hpp>

#include <stdlib.h>
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#  include <unistd.h>
#endif

#include <test/test_assert.h> // must be last

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CTestLogrotateApplication::


// defined here so SIGHUP can kick it
static auto_ptr<CRotatingLogStream> s_LogStream;

#ifdef NCBI_OS_UNIX
static volatile int s_Signal = 0;

extern "C" {
    static void s_SignalHandler(int signum)
    {
        s_Signal = signum;
    }
}
#endif

class CTestLogrotateApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

};


void CTestLogrotateApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test of log rotation");

    arg_desc->AddDefaultKey
        ("limit", "N",
         "Approximate maximum length between rotations, in bytes",
         CArgDescriptions::eInteger, "16000");
    arg_desc->AddOptionalKey
        ("prefill", "N", "Start off with an N-byte log file",
         CArgDescriptions::eInteger);
    arg_desc->AddFlag
        ("direct",
         "Use the stream directly rather than through the diagnostics module");
    arg_desc->AddFlag
        ("perline", "Log each line to a separate stream instance");

    SetupArgDescriptions(arg_desc.release());
}


static unsigned int s_Rand(unsigned int limit)
{
    return (rand() >> 5) % limit;
}


int CTestLogrotateApplication::Run(void)
{
    CArgs              args    = GetArgs();
    string             me      = GetArguments().GetProgramBasename();
    string             logname = me + ".log";
    CNcbiStreamoff     limit   = args["limit"].AsInteger();
    bool               direct  = args["direct"];
    bool               perline = args["perline"];
    IOS_BASE::openmode mode    = IOS_BASE::app | IOS_BASE::ate | IOS_BASE::out;

    if (args["prefill"]) {
        mode |= IOS_BASE::binary;
        CNcbiOfstream out(logname.c_str(),
                          IOS_BASE::out | IOS_BASE::trunc | IOS_BASE::binary);
        int size = args["prefill"].AsInteger();
        if (size > 0) {
            out.seekp(NcbiInt8ToStreampos(size - 1));
            out << endl;
        }
    }

    s_LogStream.reset(new CRotatingLogStream(logname, limit));
    if ( !direct ) {
        SetDiagStream(s_LogStream.get());
        // enable verbose diagnostics for best effect
        SetDiagPostLevel(eDiag_Info);
        SetDiagTrace(eDT_Enable);
        SetDiagPostFlag(eDPF_All);
    }
#ifdef NCBI_OS_UNIX
    {{
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = s_SignalHandler;
        sa.sa_flags   = SA_RESTART;
        sigaction(SIGHUP, &sa, 0);
    }}
    me += '/' + NStr::IntToString(getpid());
#endif
    SetDiagPostPrefix(me.c_str());
    srand((unsigned int)time(0));

    for (unsigned int n = 0;  n < 1000000;  ++n) {
        if (perline) {
            s_LogStream.reset(new CRotatingLogStream(logname, limit));
            if ( !direct ) {
                SetDiagStream(s_LogStream.get());
            }
        }
        static const char* messages[] = { "foo", "bar", "baz", "quux" };
        const char* message = messages[s_Rand(4)];
        if (direct) {
            *s_LogStream << CurrentTime().AsString("Y-M-D h:m:s.S ")
                         << me << ": " << message << endl;
        } else {
            EDiagSev  severity = (EDiagSev)(s_Rand(eDiag_Fatal));
            ErrCode   errcode(rand(), rand());
            CNcbiDiag diag(DIAG_COMPILE_INFO, severity);
            diag << errcode << message << Endm;
        }
        SleepMilliSec(s_Rand(256));
#ifdef NCBI_OS_UNIX
        if (s_Signal) {
            // dummy var, to avoid creating a diag template for 'volatile' type
            int x_Signal = s_Signal;
            LOG_POST("Received signal " << x_Signal << "; rotating logs");
            s_LogStream->Rotate();
            LOG_POST("Done rotating logs");
        }
        s_Signal = 0;
#endif
    }
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestLogrotateApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestLogrotateApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/11/22 16:33:25  vakatov
 * Use regular rather than ('volatile') var for the err.posting of s_Signal
 *
 * Revision 1.6  2005/04/25 19:05:24  ivanov
 * Fixed compilation warnings on 64-bit Worshop compiler
 *
 * Revision 1.5  2004/09/22 20:12:10  ucko
 * Tweak to avoid confusing older GCC versions.
 *
 * Revision 1.4  2004/09/22 13:32:18  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.3  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2002/12/30 20:56:18  ucko
 * Bound the main loop to avoid "statement is unreachable" warnings.
 *
 * Revision 1.1  2002/11/25 17:21:01  ucko
 * Add support for automatic log rotation (CRotatingLogStream)
 *
 *
 * ===========================================================================
 */
