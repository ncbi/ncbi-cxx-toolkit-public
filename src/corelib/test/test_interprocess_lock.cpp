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
#include <corelib/interprocess_lock.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

// Successful exit code for child process in the multi-process tests
#define IPCL_SUCCESS  88

// Logging macros
#define LOG_PARENT(msg)  LOG_POST(CTime(CTime::eCurrent).AsString() << "parent : " << msg)
#define LOG_CHILD(msg)   LOG_POST(CTime(CTime::eCurrent).AsString() << "child  : " << msg)


static void Test_SingleProcess()
{
    LOG_POST("=== Single-process test ===");
    const CTimeout zero_timeout = CTimeout(0,0);

    LOG_POST("\n--- Test 1");
    // Name test
    {{
        try {
            CInterProcessLock lock("");
        } catch (CInterProcessLockException&) {}
        try {
            CInterProcessLock lock("relative/path");
        } catch (CInterProcessLockException&) {}
        {{
            CInterProcessLock lock("name");
            assert(lock.GetName() == "name");
#if defined(NCBI_OS_UNIX)
            assert(lock.GetSystemName() == "/var/tmp/name");
#elif defined(NCBI_OS_MSWIN)
            assert(lock.GetSystemName() == "name");
#endif
        }}
    }}

    // Fixed names are usually more appropriate, but here we don't
    // want independent tests to step on each other...
    string lockname = CFile::GetTmpName();

    CInterProcessLock lock1(lockname);
    CInterProcessLock lock2(lockname);
    CInterProcessLock lock3(lockname);

    LOG_POST("lock name = " << lock1.GetName());
    LOG_POST("lock system name = " << lock1.GetSystemName());

    LOG_POST("\n--- Test 2");
    // Direct CInterProcessLock usage
    {{
        lock1.Lock(/*infinite*/);
        try {
            lock2.Lock(zero_timeout);
            assert(0);
        }
        catch (CInterProcessLockException&) {}
        
        assert( !lock2.TryLock() );
        lock1.Unlock();
        lock2.Lock(/*infinite*/);
        assert( !lock3.TryLock() );
        lock2.Unlock();
        lock1.Lock(zero_timeout);
        lock1.Remove();
        // everything unlocked
    }}

    LOG_POST("\n--- Test 3");
    // CGuard usage
    {{
        {{
            CGuard<CInterProcessLock> GUARD(lock1);
            try {
                lock2.Lock(zero_timeout);
                assert(0);
            }
            catch (CInterProcessLockException&) {}
        }}
        lock2.Lock(zero_timeout);
        lock2.Remove();
        // everything unlocked
    }}

    LOG_POST("\n--- Test 4");
    // Reference count test
    {{
        assert( lock1.TryLock() );
        assert( lock1.TryLock() );
        assert( !lock2.TryLock() );
        assert( lock1.TryLock() );
        lock1.Unlock();
        lock1.Unlock();
        lock1.Unlock();
        try {
            lock1.Unlock();
            assert(0);
        }
        catch (CInterProcessLockException&) {}
        // everything unlocked
    }}
    // We don't need lock object anymore, remove it from the system
    lock1.Remove();
}


static void Test_MultiProcess()
{
    LOG_POST("\n=== Multi-process tests ===");

    const CTimeout zero_timeout = CTimeout(0,0);

    // Fixed names are usually more appropriate, but here we don't
    // want independent tests to step on each other....
    string lockname = CFile::GetTmpName();
    CInterProcessLock lock(lockname);
    LOG_POST("lock name = " << lock.GetName());
    LOG_POST("lock system name = " << lock.GetSystemName());
    
    string app = CNcbiApplication::Instance()->GetArguments().GetProgramName();
    TProcessHandle handle;
    TExitCode exitcode;

    LOG_POST("\n--- Test 1");
    // Set lock and start child process, that try to set lock also (unsuccessfully).
    {{
        LOG_PARENT("lock");
        lock.Lock(/*infinite*/);
        LOG_PARENT("locked");
        LOG_PARENT("start child");
        exitcode = CExec::SpawnL(CExec::eWait, app.c_str(),
                                 "-test", "1",
                                 "-lock", lockname.c_str(), NULL).GetExitCode(); 
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
    }}

    LOG_POST("\n--- Test 2");
    // Set lock and start child process, that try to set lock also (successfully after child termination).
    {{
        // lock.Lock(); -- still locked
        LOG_PARENT("start child");
        handle = CExec::SpawnL(CExec::eNoWait, app.c_str(),
                               "-test", "2",
                               "-lock", lockname.c_str(), NULL).GetProcessHandle(); 
        CProcess process(handle, CProcess::eHandle);
        // Wait until child starts
        SleepSec(1);
        LOG_PARENT("unlock");
        lock.Unlock();
        LOG_PARENT("unlocked");
        SleepSec(2);
        // Child should set lock on this moment
        LOG_PARENT("try lock");
        assert( !lock.TryLock() );
        LOG_PARENT("unable to lock");
        exitcode = process.Wait();
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
        // Lock should be already removed
        LOG_PARENT("lock");
        lock.Lock(zero_timeout);
        LOG_PARENT("locked");
        LOG_PARENT("unlock");
        lock.Remove();
        LOG_PARENT("removed");
    }}

    LOG_POST("\n--- Test 3");
    // Child set lock and current process try to lock it also.
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
        LOG_PARENT("unable to lock");
        LOG_PARENT("lock in 3 sec");
        lock.Lock(CTimeout(3,0));
        LOG_PARENT("locked");
        LOG_PARENT("unlock");
        lock.Unlock();
        LOG_PARENT("unlocked");
        exitcode = process.Wait();
        LOG_PARENT("child exitcode = " << exitcode);
        assert( exitcode == IPCL_SUCCESS );
    }}
    
    LOG_POST("\n--- Test 4");
    // Child set lock, we kill child process and set our own lock.
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
        LOG_PARENT("unable to lock");
        LOG_PARENT("kill");
        assert(process.Kill());
        // Lock should be automaticaly released
        SleepSec(1);
        LOG_PARENT("lock");
        lock.Lock();
        LOG_PARENT("locked");
    }}
    
    // We don't need lock object anymore, remove it from the system
    lock.Remove();
    LOG_PARENT("removed");
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
        // It should be released soon, wait
        LOG_CHILD("lock in 3 sec");
        lock.Lock(CTimeout(3,0));
        LOG_CHILD("locked");
        // Parent process should try to set lock in the next 3 seconds
        // (without success)
        SleepSec(3);
        // The lock should be removed automatically on normal termination
        return IPCL_SUCCESS;
        
    case 3:
        LOG_CHILD("lock");
        lock.Lock();
        LOG_CHILD("locked");
        SleepSec(3);
        return IPCL_SUCCESS;
        
    case 4:
        LOG_CHILD("lock");
        lock.Lock(/*infinite*/);
        LOG_CHILD("locked");
        SleepSec(20);
        // Child process should be killed on this moment and lock released
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
    CTime::SetFormat("[h:m:s.l]  ");
    CArgs args = GetArgs();

    // Special arguments to test locks in multi-process configuration
    if ( args["test"].AsInteger() ) {
        return Test_MultiProcess_Child(args["test"].AsInteger(), args["lock"].AsString());
    }
    
    // Main tests
    Test_SingleProcess();
    Test_MultiProcess();
    
    return 0;
}


  
///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
