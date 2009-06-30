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
 * Authors:  Vladimir Ivanov
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
#include <corelib/ipc_lock.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

// Successful exit code for child process in the multi-process tests
#define IPCL_SUCCESS  88

// Logging macros
#define LOG_PARENT(msg)  LOG_POST("parent : " << msg)
#define LOG_CHILD(msg)   LOG_POST("child  : " << msg)


static void Test_SingleProcess()
{
    LOG_POST("==== Single-process test ====");
    
    // Generate lock name automaticaly for lock1.
    // Fixed names are usually more appropriate, but here we don't
    // want independent tests to step on each other....
    CInterProcessLock lock1; 
    string lockname = lock1.GetName();
    LOG_POST("lock = " << lockname);
    CInterProcessLock lock2(lockname);
    CInterProcessLock lock3(lockname);
  
    // Direct CInterProcessLock usage
    {{
        lock1.Lock();
        try {
            lock2.Lock();
            assert(0);
        }
        catch (CInterProcessLockException&) {}
        
        assert( !lock2.TryLock() );
        lock1.Unlock();
        lock2.Lock();
        assert( !lock3.TryLock() );
        lock2.Unlock();
        lock1.Lock();
        lock1.Unlock();
        // everything unlocked
    }}

    // CGuard usage
    {{
        {{
            CGuard<CInterProcessLock> GUARD(lock1);
            try {
                lock2.Lock();
                assert(0);
            }
            catch (CInterProcessLockException&) {}
        }}
        lock2.Lock();
        lock2.Unlock();
        // everything unlocked
    }}
}


static void Test_MultiProcess()
{
    string lockname = CInterProcessLock::GenerateUniqueName();
    CInterProcessLock lock(lockname);
    
    string app = CNcbiApplication::Instance()->GetArguments().GetProgramName();
    TProcessHandle handle;
    TExitCode exitcode;
    
    LOG_POST("\n==== Multi-process tests ====");
    LOG_POST("lock = " << lockname);
    LOG_POST("\n--- Test 1");
    {{
        LOG_PARENT("lock");
        lock.Lock();
        LOG_PARENT("start child");
        exitcode = CExec::SpawnL(CExec::eWait, app.c_str(),
                                 "-test", "1",
                                 "-lock", lockname.c_str(), NULL).GetExitCode(); 
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
    }}
    
    LOG_POST("\n--- Test 2");
    {{
        // lock.Lock(); -- still locked
        LOG_PARENT("start child");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-test", "2",
                               "-lock", lockname.c_str(), NULL).GetProcessHandle(); 
        CProcess process(handle, CProcess::eHandle);
        // Wait until child starts
        SleepSec(1);
        lock.Unlock();
        LOG_PARENT("unlock");
        SleepSec(3);
        // Child should set the lock on this moment
        LOG_PARENT("try lock");
        assert( !lock.TryLock() );
        LOG_PARENT("- not locked");
        exitcode = process.Wait();
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
        // Lock should be already removed
        LOG_PARENT("lock");
        lock.Lock();
        LOG_PARENT("unlock");
        lock.Unlock();
    }}

    LOG_POST("\n--- Test 3");
    {{
        LOG_PARENT("start child");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-test", "3",
                               "-lock", lockname.c_str(), NULL).GetProcessHandle(); 
        CProcess process(handle, CProcess::eHandle);
        // Wait until child starts
        SleepSec(1);
        LOG_PARENT("try lock");
        assert( !lock.TryLock() );
        SleepSec(3);
        LOG_PARENT("lock");
        lock.Lock();
        LOG_PARENT("unlock");
        lock.Unlock();
        exitcode = process.Wait();
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
    }}
    
    LOG_POST("\n--- Test 4");
    {{
        LOG_PARENT("start child");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-test", "3",
                               "-lock", lockname.c_str(), NULL).GetProcessHandle(); 
        CProcess process(handle, CProcess::eHandle);
        // Wait until child starts
        SleepSec(1);
        // Lock should be set by child
        LOG_PARENT("try lock");
        assert( !lock.TryLock() );
        LOG_PARENT("kill");
        assert(process.Kill());
        // Lock should be automaticaly released
        SleepSec(1);
        LOG_PARENT("lock");
        lock.Lock();
    }}
}



static int Test_MultiProcess_Child(int test, string lockname)
{
    LOG_CHILD("started");
    CInterProcessLock lock(lockname);
    
    switch (test) {
    
    case 1:
        // The lock already set in the parent process
        LOG_CHILD("try lock");
        assert( !lock.TryLock() );
        // Error acquire lock
        return IPCL_SUCCESS;
        
    case 2:
        // The lock already set in the parent process
        LOG_CHILD("try lock");
        assert( !lock.TryLock() );
        SleepSec(3);
        LOG_CHILD("try lock again");
        // It should be released on this moment
        assert( lock.TryLock() );
        SleepSec(3);
        // The lock should be removed automatically on normal termination
        return IPCL_SUCCESS;
        
    case 3:
        LOG_CHILD("lock");
        lock.Lock();
        SleepSec(3);
        return IPCL_SUCCESS;
        
    case 4:
        LOG_CHILD("lock");
        lock.Lock();
        SleepSec(20);
        // Child process should be killed on this moment
        return 1;
    }
    // Unsuccessfull exit code 
    return 1;
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
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Test inter-process locks");
    
    // Specific to multi-process configuration test
    arg_desc->AddDefaultKey("test", "number", "test case number (internal use only)",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("lock", "name", "parent's lock (internal use only)",
                            CArgDescriptions::eString, kEmptyStr);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestApplication::Run(void)
{
    CArgs args = GetArgs();

    // Special arguments to test locks in multi-process configuration
    if ( args["test"].AsInteger() ) {
        return Test_MultiProcess_Child(args["test"].AsInteger(), args["lock"].AsString());
    }
    
    // Main tests
//    Test_SingleProcess();
    Test_MultiProcess();
    
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
