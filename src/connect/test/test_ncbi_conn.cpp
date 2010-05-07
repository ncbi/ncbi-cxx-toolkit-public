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
#include "../ncbi_priv.h"
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#ifdef NCBI_OS_MSWIN
#  include <corelib/ncbi_system.hpp>
#endif //NCBI_OS_MSWIN
#include <corelib/rwstream.hpp>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_conn_test.hpp>
#include <connect/ncbi_socket.hpp>
#include <util/multi_writer.hpp>
#ifdef NCBI_OS_MSWIN
#  include <conio.h>
#endif //NCBI_OS_MSWIN
#include <iomanip>
#include <math.h>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


static const char kLogfile[] = "test_ncbi_conn.log";


static list<CNcbiOstream*> s_MakeList(CNcbiOstream& os1,
                                      CNcbiOstream& os2)
{
    list<CNcbiOstream*> rv;
    rv.push_front(&os1);
    rv.push_back (&os2);
    return rv;
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest(CNcbiOstream& log);

    virtual void Init(void);
    virtual int  Run (void);

private:
    CWStream m_Tee;
};


CTest::CTest(CNcbiOstream& log)
    : m_Tee(new CMultiWriter(s_MakeList(NcbiCout, log)),
            0, 0, CRWStreambuf::fOwnWriter)
{
    HideStdArgs(-1/*everything*/);
}


void CTest::Init(void)
{
    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    if (args->Exist ("h"))
        args->Delete("h");
    if (args->Exist ("xmlhelp"))
        args->Delete("xmlhelp");
    args->AddFlag("nopause",
                  "Do not pause at the end"
#ifndef NCBI_OS_MSWIN
                  " (MS-WIN only, no effect on this platform)"
#endif //!NCBI_OS_MSWIN
                  );
    args->AddExtra(0/*no mandatory*/, 1/*single timeout argument allowed*/,
                   "Timeout", CArgDescriptions::eDouble);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "NCBI Connectivity Test Suite");
    SetupArgDescriptions(args.release());
}


int CTest::Run(void)
{
    IRWRegistry& reg = GetConfig();
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "DATA");
    CONNECT_Init(&reg);

    const CArgs& args = GetArgs();
    size_t n = args.GetNExtra();
    double timeout;

    if (!n) {
        char val[40];
        ConnNetInfo_GetValue(0, REG_CONN_TIMEOUT, val, sizeof(val), "");
        if (!*val)
            timeout = DEF_CONN_TIMEOUT;
        else
            timeout = fabs(atof(val));
    } else
        timeout = fabs(args[1].AsDouble());

    m_Tee << NcbiEndl << "NCBI Connectivity Test (Timeout = "
          << setprecision(6) << timeout << "s)" << NcbiEndl;

    STimeout tmo;
    tmo.sec  = (unsigned int)  timeout;
    tmo.usec = (unsigned int)((timeout - tmo.sec) * 1000000.0);

    CConnTest test(&tmo, &m_Tee);

    CConnTest::EStage everything = CConnTest::eStatefulService;
    EIO_Status status = test.Execute(everything);

    m_Tee << NcbiEndl;
    if (status != eIO_Success) {
        m_Tee << "Check " << CDirEntry::CreateAbsolutePath(kLogfile)
              << " for more information." << NcbiEndl
              << "Please remember to make its contents"
            " available if contacting NCBI.";
    } else {
        _ASSERT(everything == CConnTest::eStatefulService);
        m_Tee << "NCBI Connectivity Test PASSED!";
    }
    m_Tee << NcbiEndl << NcbiEndl << NcbiFlush;

    int retval = status == eIO_Success ? 0 : 1;
#ifdef NCBI_OS_MSWIN
    if (args["nopause"].HasValue())
        retval = ~retval;
#endif //NCBI_OS_MSWIN

    CORE_SetREG(0);
    return retval;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    // Set error posting and tracing at maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Trace);

    int retval = 1/*failure*/;

    try {
        CNcbiOfstream log(kLogfile, IOS_BASE::out | IOS_BASE::trunc);
        if (log) {
            SetDiagStream(&log);

            // Execute main application function
            int rv = CTest(log).AppMain(argc, argv, 0, eDS_User, 0);

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
    else {
        NcbiCout << "Hit any key or program will bail out in 1 minute..."
                 << NcbiFlush;
        for (int n = 0;  n < 120;  n++) {
            if (_kbhit())
                break;
            SleepMilliSec(500);
        }
    }
#endif //NCBI_OS_MSWIN

    return retval;
}
