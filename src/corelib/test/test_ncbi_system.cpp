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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      System functions
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbifile.hpp>
#include <memory>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////
// General tests
//

static void Test_General(void)
{
    LOG_POST("\nGeneral tests\n");

    // Number of processors
    cout << "Number of processors: " << GetCpuCount() << endl;
}


/////////////////////////////////
// User defined dump print handler
//

int s_PrintParameter = 0;

static void PrintHandler (ELimitsExitCode code, size_t limit, CTime& time, 
                   TLimitsPrintParameter param) 
{
    cout << "Type          : " << 
        (code == eLEC_Memory ? "Memory limit" : "CPU limit") << endl;
    cout << "Limit value   : " << limit << endl;
    cout << "Set time      : " << time.AsString() << endl;
    cout << "Our parameter : " << (param ? *(int*)param : 0) << endl;
}


/////////////////////////////////
// Memory limits
//

static void Test_MemLimit(void)
{
    LOG_POST("\nHeap limit test\n");

    const size_t kHeapLimit = 100000;

    assert( SetHeapLimit(kHeapLimit, PrintHandler, &s_PrintParameter) );
    
    for (size_t i = 0;  i <= kHeapLimit/10;  i++) {
        s_PrintParameter++;
        int* pi = new int[10];
        assert(pi);
    }
}


/////////////////////////////////
// CPU time limits
//

static void Test_CpuLimit(void)
{
    LOG_POST("\nCPU time limit test\n");

    assert( SetCpuTimeLimit(2) );

    for (;;) {
        continue;
    }
}


// ppid is int rather than TPid in these two functions because we need
// it to be signed.

static void Test_PIDGuardChild(int ppid, string lockfile)
{
    CNcbiOstrstream oss;
    oss << CNcbiApplication::Instance()->GetArguments().GetProgramName()
        << " -parent " << ppid << " -lockfile " << lockfile << " pid" << '\0';
    _ASSERT( !CExec::System(oss.str()) );
    oss.freeze(false);
}

static void Test_PIDGuard(int ppid, string lockfile)
{
    if (lockfile.empty()) {
        // Fixed names are usually more appropriate, but here we don't
        // want independent tests to step on each other....
        lockfile = CFile::GetTmpName();
    }
    TPid my_pid = GetPID();
    _ASSERT(my_pid > 0);
    LOG_POST("\nTest_PIDGuard starting:\nmy_pid = " << my_pid
             << ", ppid = " << ppid << ", lockfile = " << lockfile << '\n');
    if (ppid == 0) { // parent
        CPIDGuard guard1(lockfile);
        try {
            LOG_POST("Expect an exception now.");
            CPIDGuard guard2(lockfile);
            ERR_POST("Should have been locked (by myself)");
            _TROUBLE;
        } catch (CPIDGuardException& e) {
            LOG_POST(e.what());
            _ASSERT(e.GetErrCode() == CPIDGuardException::eStillRunning);
            _ASSERT(e.GetPID() == my_pid);
        }
        Test_PIDGuardChild(my_pid, lockfile);
        guard1.Release();
        Test_PIDGuardChild(-1, lockfile);
        Test_PIDGuardChild(-2, lockfile);
    } else if (ppid > 0) { // child run with parent lock open
        try {
            LOG_POST("Expect an exception now.");
            CPIDGuard guard(lockfile);
            ERR_POST("Should have been locked (by parent)");
            _TROUBLE;
        } catch (CPIDGuardException& e) {
            LOG_POST(e.what());
            _ASSERT(e.GetErrCode() == CPIDGuardException::eStillRunning);
            _ASSERT(e.GetPID() == ppid);
        }
    } else if (ppid == -1) {
        new CPIDGuard(lockfile); // deliberate leak
        LOG_POST("Left stale lock.");
    } else if (ppid == -2) {
        CPIDGuard guard(lockfile);
        TPid old_pid = guard.GetOldPID();
        _ASSERT(old_pid);
        LOG_POST("Old PID was " << old_pid);
    } else {
        _TROUBLE;
    }

    CPIDGuard unique_guard(CFile::GetTmpName());
}


/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test some system-specific functions");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("feature",
         "Platform-specific feature to test",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("feature", &(*new CArgAllow_Strings, "mem", "cpu", "pid"));

    // Specific to PID guard test
    arg_desc->AddDefaultKey("parent", "PID", "for internal use only",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("lockfile", "filename", "parent's lock file",
                            CArgDescriptions::eString, kEmptyStr);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestApplication::Run(void)
{
    CArgs args = GetArgs();

    // General tests
    if ( !args["parent"].AsInteger() ) {
        Test_General();
    }

    // Specific tests
    if (args["feature"].AsString() == "mem") {
        Test_MemLimit();
    }
    else if (args["feature"].AsString() == "cpu") {
        Test_CpuLimit();
    }
    else if (args["feature"].AsString() == "pid") {
        Test_PIDGuard(args["parent"].AsInteger(), args["lockfile"].AsString());
    }
    else {
        _TROUBLE;
    }

    return 0;
}



  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

static CTestApplication theTestApplication;


int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.7  2003/08/12 17:25:14  ucko
 * Test the new PID-file support.
 *
 * Revision 6.6  2002/04/16 18:49:07  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.5  2001/11/08 21:31:45  ivanov
 * Renamed GetCPUNumber() -> GetCpuCount()
 *
 * Revision 6.4  2001/11/08 21:10:59  ivanov
 * Added test for GetCPUNumber()
 *
 * Revision 6.3  2001/07/23 15:24:06  ivanov
 * Fixed bug in Get/Set times in DB-format (1 day difference)
 *
 * Revision 6.2  2001/07/02 21:33:09  vakatov
 * Fixed against SIGXCPU during the signal handling.
 * Increase the amount of reserved memory for the memory limit handler
 * to 10K (to fix for the 64-bit WorkShop compiler).
 * Use standard C++ arg.processing (ncbiargs) in the test suite.
 * Cleaned up the code. Get rid of the "Ncbi_" prefix.
 *
 * Revision 6.1  2001/07/02 16:43:43  ivanov
 * Initialization
 *
 * ===========================================================================
 */
