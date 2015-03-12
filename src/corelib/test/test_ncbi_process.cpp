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
 * Authors:  Aaron Ucko, Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_process.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////
// General tests
//

static void Test_Process(void)
{
    LOG_POST("\nProcess tests:\n");

    string app = CNcbiApplication::Instance()->GetArguments().GetProgramName();
    TProcessHandle handle;
    {{
        LOG_POST("CMD = " << app << " -sleep 3");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-sleep", "3", NULL).GetProcessHandle();
        LOG_POST("PID/HANDLE = " << (intptr_t)handle);
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert(process.Wait() == 88);
        assert(!process.IsAlive());
    }}
    {{
        LOG_POST("CMD = " << app << " -sleep 10");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-sleep", "10", NULL).GetProcessHandle();
        LOG_POST("PID/HANDLE = " << (intptr_t)handle);
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert( process.Wait(2000) == -1);
        assert(process.Kill());
        assert(!process.IsAlive());
        int exitcode = process.Wait();
        LOG_POST("Wait(pid) = " << exitcode);
        assert(exitcode != 88);
    }}
}


/////////////////////////////////
// PIDGuard test
//
// NOTE:  ppid is int rather than TPid in these two functions because we need
//        it to be signed.

static void Test_PIDGuardChild(int ppid, string lockfile)
{
    string s_app =
    CNcbiApplication::Instance()->GetArguments().GetProgramName();
    string s_pid = NStr::IntToString(ppid);
    TExitCode ret_code =
        CExec::SpawnL(CExec::eWait, s_app.c_str(),
                      "-parent", s_pid.c_str() ,
                      "-lockfile", lockfile.c_str(), NULL).GetExitCode(); 
    assert( !ret_code );
}

static void Test_PIDGuard(int ppid, string lockfile)
{
    if (lockfile.empty()) {
        // Fixed names are usually more appropriate, but here we don't
        // want independent tests to step on each other....
        lockfile = CFile::GetTmpName();
    }
    CFile lf(lockfile);
    TPid my_pid = CProcess::GetCurrentPid();
    assert(my_pid > 0);
    LOG_POST("\nTest_PIDGuard starting:\nmy_pid = " << my_pid
             << ", ppid = " << ppid << ", lockfile = " << lockfile << '\n');

    // Parent
    if (ppid == 0) {
        CPIDGuard guard(lockfile);
        {
            CPIDGuard guard2(lockfile);
        }
        assert(lf.Exists());
        Test_PIDGuardChild(my_pid, lockfile);
        assert(lf.Exists());
        guard.Release();
        assert(!lf.Exists());
        Test_PIDGuardChild(-1, lockfile);
        assert(lf.Exists());
#if defined(NCBI_OS_MSWIN)
        // Additional check on stuck child process.
        //
        // On some Windows machines OS report that child process is still
        // running even if we already have its exit code.
        CNcbiIfstream in(lockfile.c_str());
        if (in.good()) {
            int child_pid = 0;
            in >> child_pid;
            if ( child_pid > 0 ) {
                CProcess child(child_pid, CProcess::ePid);
                child.Wait();
            }
        }
        in.close();
#endif
        Test_PIDGuardChild(-2, lockfile);
        assert(!lf.Exists());
    }
    // Child run with parent lock open
    else if (ppid > 0) {
        try {
            LOG_POST("Expect an exception now.");
            CPIDGuard guard(lockfile);
            ERR_POST("Should have been locked (by parent)");
            _TROUBLE;
        } catch (CPIDGuardException& e) {
            LOG_POST(e.what());
            assert(e.GetErrCode() == CPIDGuardException::eStillRunning);
            assert(e.GetPID() == ppid);
        }
    } else if (ppid == -1) {
        new CPIDGuard(lockfile); // deliberate leak
        LOG_POST("Left stale lock.");
    } else if (ppid == -2) {
        CPIDGuard guard(lockfile);
        TPid old_pid = guard.GetOldPID();
        assert(old_pid > 0);
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
                              "Test process management functions");
    // Specific to Process test
    arg_desc->AddDefaultKey("sleep", "sec", "for internal use only",
                            CArgDescriptions::eInteger, "0");
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

    // Is this an instance executed from process test?
    int sec = args["sleep"].AsInteger();
    if ( sec ) {
        LOG_POST("Client is sleeping " << sec << " sec.");
        SleepSec(sec);
        return 88;
    }

    // Main tests

    // General tests
    if ( !args["parent"].AsInteger() ) {
        Test_Process();
    }
    // PIDGuard tests
    Test_PIDGuard(args["parent"].AsInteger(), args["lockfile"].AsString());

    return 0;
}


  
///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
