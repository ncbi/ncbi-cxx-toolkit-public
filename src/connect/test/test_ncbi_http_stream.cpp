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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test NCBI HTTP stream
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_conn_stream.hpp>
#ifdef NCBI_OS_MSWIN
#  include <conio.h>
#endif // NCBI_OS_MSWIN
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#endif

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


static volatile bool s_Canceled = false;


class CCanceled : public CObject, public ICanceled
{
public:
    virtual bool IsCanceled(void) const { return s_Canceled; }
};


#if   defined(NCBI_OS_MSWIN)
static BOOL WINAPI s_Interrupt(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        s_Canceled = true;
        return TRUE;  // handled
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    default:
        break;
    }
    return FALSE;  // unhandled
}
#elif defined(NCBI_OS_UNIX)
extern "C" {
static void s_Interrupt(int /*signo*/)
{
    s_Canceled = true;
}
}
#endif // NCBI_OS


class CNCBITestHttpStreamApp : public CNcbiApplication
{
public:
    CNCBITestHttpStreamApp(void);

public:
    void Init(void);
    int  Run (void);
};


CNCBITestHttpStreamApp::CNCBITestHttpStreamApp(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CNCBITestHttpStreamApp::Init(void)
{
    // Ctrl-C handling
#  if   defined(NCBI_OS_MSWIN)
    SetConsoleCtrlHandler(s_Interrupt, TRUE);
#  elif defined(NCBI_OS_UNIX)
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#  endif // NCBI_OS

    // Usage setup
    unique_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->SetMiscFlags(CArgDescriptions::fUsageIfNoArgs);
    if (args->Exist ("h"))
        args->Delete("h");
    if (args->Exist ("xmlhelp"))
        args->Delete("xmlhelp");
    args->AddExtra(1/*one mandatory*/, kMax_UInt/*unlimited optional*/,
                   "List of URL(s) to process", CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Test NCBI HTTP stream");
    SetupArgDescriptions(args.release());
}


int CNCBITestHttpStreamApp::Run(void)
{
    const CArgs& args = GetArgs();
    size_t n = args.GetNExtra();
    _ASSERT(n > 0);

    CCanceled canceled;
    CConn_HttpStream http(args[1].AsString(), eReqMethod_Get11);
    http.SetCanceledCallback(&canceled);

    size_t i = 1;
    for (;;) {
        NcbiStreamCopy(NcbiCout, http);
        NcbiCout << NcbiEndl;
        http.clear();
        if (i >= n)
            break;
        if (http.Status() == eIO_Interrupt)
            return 2;
        http.SetURL(args[++i].AsString());
    }

    return 0;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

#if   defined(NCBI_OS_MSWIN)
    SetConsoleCtrlHandler(s_Interrupt, TRUE);
    static char buf[4096];
    cerr.rdbuf()->pubsetbuf(buf, sizeof(buf));
    cerr.unsetf(ios_base::unitbuf);
#elif defined(NCBI_OS_UNIX)
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#endif // NCBI_OS

    return CNCBITestHttpStreamApp().AppMain(argc, argv);
}
