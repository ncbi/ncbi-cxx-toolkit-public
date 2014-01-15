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
 *   Run NCBI connectivity test
 *
 */

#include <ncbi_pch.hpp>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#ifdef NCBI_OS_MSWIN
#  include <corelib/ncbi_system.hpp>
#endif // NCBI_OS_MSWIN
#include <corelib/rwstream.hpp>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_conn_test.hpp>
#include <connect/ncbi_socket.hpp>
#include <util/multi_writer.hpp>
#include <iomanip>
#include <locale.h>
#if   defined(NCBI_OS_MSWIN)
#  include <conio.h>
#elif defined(NCBI_OS_UNIX)
#  include <signal.h>
#endif // NCBI_OS


#define PAGE_WIDTH  72


BEGIN_NCBI_SCOPE


static const char kLogfile[] = "test_ncbi_conn.log";


static bool s_Run = true;


static volatile bool s_Canceled = false;


class CCanceled : public CObject, public ICanceled
{
public:
    virtual bool IsCanceled(void) const { return s_Canceled; }
};


////////////////////////////////
// Test application
//

class CTestApp : public CNcbiApplication
{
public:
    CTestApp(CNcbiOstream& log);

    virtual void Init(void);
    virtual int  Run (void);

protected:
    virtual bool LoadConfig(CNcbiRegistry& reg, const string* conf);
    using CNcbiApplication::LoadConfig;

private:
    CWStream m_Tee;
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


static list<CNcbiOstream*> s_MakeList(CNcbiOstream& os1,
                                      CNcbiOstream& os2)
{
    list<CNcbiOstream*> rv;
    rv.push_front(&os1);
    rv.push_back (&os2);
    return rv;
}


CTestApp::CTestApp(CNcbiOstream& log)
    : m_Tee(new CMultiWriter(s_MakeList(NcbiCout, log)),
            0, 0, CRWStreambuf::fOwnWriter)
{
    HideStdArgs(-1/*everything*/);
}


bool CTestApp::LoadConfig(CNcbiRegistry& reg, const string* conf)
{
    string dir;
    const string& path = GetProgramExecutablePath();
    CDirEntry::SplitPath(path, &dir, NULL, NULL);
    CMetaRegistry::SetSearchPath().clear();
    CMetaRegistry::SetSearchPath().push_back(dir);
    return LoadConfig(reg, conf, 0);
}


void CTestApp::Init(void)
{
    s_Run = false;

    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    if (args->Exist ("h"))
        args->Delete("h");
    if (args->Exist ("xmlhelp"))
        args->Delete("xmlhelp");
    args->AddFlag("nopause",
                  "Do not pause at the end"
#ifndef NCBI_OS_MSWIN
                  " (MS-WIN only, no effect on this platform)"
#endif // !NCBI_OS_MSWIN
                  );
    args->AddExtra(0/*no mandatory*/, 1/*single timeout argument allowed*/,
                   "Timeout", CArgDescriptions::eDouble);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "NCBI Connectivity Test Suite");
    SetupArgDescriptions(args.release());
}


int CTestApp::Run(void)
{
    s_Run = true;

    CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();
    size_t n = args.GetNExtra();
    double timeout;

    if (!n) {
        char val[40], *e;
        if (!ConnNetInfo_GetValue(0, REG_CONN_TIMEOUT, val, sizeof(val), 0)
            ||  !*val  ||  (timeout = NCBI_simple_atof(val, &e)) < 0.0
            ||  errno  ||  *e) {
            timeout = DEF_CONN_TIMEOUT;
        }
    } else if ((timeout = args[1].AsDouble()) < 0.0)
        timeout = DEF_CONN_TIMEOUT;

    m_Tee << NcbiEndl << "NCBI Connectivity Test (Timeout = "
          << setprecision(6) << timeout << "s, "
          << (CConnTest::IsNcbiInhouseClient() ? "" : "non-")
          << "local client)" << NcbiEndl;

    STimeout tmo;
    tmo.sec  = (unsigned int)  timeout;
    tmo.usec = (unsigned int)((timeout - tmo.sec) * 1000000.0);

    CCanceled canceled;
    CConnTest test(&tmo, &m_Tee);
    test.SetCanceledCallback(&canceled);
    CSocketAPI::SetInterruptOnSignal(eOn);

    test.SetDebugPrintout(eDebugPrintout_Data);
    CConnTest::EStage everything = CConnTest::eStatefulService;
    EIO_Status status = test.Execute(everything);

    test.SetOutput(0);

    if (status != eIO_Success) {
        list<string> msg;
        NStr::Justify("Detailed transaction log has been saved in:",
                      PAGE_WIDTH, msg);
        msg.push_back('"' + CDirEntry::CreateAbsolutePath(kLogfile) + "\".");
        msg.push_back(kEmptyStr);
        NStr::Justify("Should you choose to make its contents available to"
                      " NCBI, please keep in mind that the log can contain"
                      " authorization credentials, which you may want to"
                      " redact prior to actually submitting the file for"
                      " review.", PAGE_WIDTH, msg, "      ", "NOTE: ");
        ITERATE(list<string>, line, msg) {
            m_Tee << NcbiEndl << *line;
        }
    } else {
        _ASSERT(everything == CConnTest::eStatefulService);
        m_Tee << NcbiEndl << "NCBI Connectivity Test PASSED!";
    }
    m_Tee << NcbiEndl << NcbiEndl << NcbiFlush;

    int retval = status == eIO_Success ? 0 : 1;
#ifdef NCBI_OS_MSWIN
    if (args["nopause"].HasValue())
        retval = ~retval;
#endif // NCBI_OS_MSWIN

    CORE_SetREG(0);
    return retval;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    setlocale(LC_ALL, "");
    // Set error posting and tracing at what regular use would do.
    // If traces are needed, they can be enabled from the environment.
    //SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

#if   defined(NCBI_OS_MSWIN)
    SetConsoleCtrlHandler(s_Interrupt, TRUE);
#elif defined(NCBI_OS_UNIX)
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#endif // NCBI_OS

    int retval = 1/*failure*/;

    try {
        CNcbiOfstream log(kLogfile, IOS_BASE::out | IOS_BASE::trunc);
        log.close();
        log.open(kLogfile, IOS_BASE:: out | IOS_BASE::app);
        if (log) {
            SetDiagStream(&log);

            // Execute main application function
            int rv = CTestApp(log).AppMain(argc, argv, 0, eDS_User);

            log.flush();
            // Make sure CNcbiDiag remains valid when main() returns
            SetLogFile(kLogfile);
            retval = rv;
        } else {
            int/*bool*/ dynamic = 0/*false*/;
            const char* msg = NcbiMessagePlusError
                (&dynamic, ("Cannot open logfile " +
                            CDirEntry::CreateAbsolutePath(kLogfile)).c_str(),
                 errno, 0);
            ERR_POST(Critical << msg);
            if (dynamic)
                free((void*) msg);
        }
    }
    NCBI_CATCH_ALL("Test failed");

#ifdef NCBI_OS_MSWIN
    if (retval < 0)
        retval = ~retval;
    else if (s_Run) {
        NcbiCout << "Hit any key or program will bail out in 1 minute..."
                 << NcbiFlush;
        for (int n = 0;  n < 120;  n++) {
            if (_kbhit()) {
                _getch();
                break;
            }
            SleepMilliSec(500);
        }
    }
#endif // NCBI_OS_MSWIN

    return retval;
}
