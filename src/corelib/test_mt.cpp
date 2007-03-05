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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Wrapper for testing modules in MT environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbimtx.hpp>
#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE


DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);
static CThreadedApp* s_Application;

// Default values
unsigned int  s_NumThreads    = 34;
int           s_SpawnBy       = 6;

// Next test thread index
static volatile unsigned int  s_NextIndex = 0;


/////////////////////////////////////////////////////////////////////////////
// Test thread
//

class CTestThread : public CThread
{
public:
    CTestThread(int id);
protected:
    ~CTestThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);
private:
    int m_Idx;
};


CTestThread::CTestThread(int idx)
    : m_Idx(idx)
{
    if ( s_Application != 0 )
        assert(s_Application->Thread_Init(m_Idx));
}


CTestThread::~CTestThread(void)
{
    if ( s_Application != 0 )
        assert(s_Application->Thread_Destroy(m_Idx));
}


void CTestThread::OnExit(void)
{
    if ( s_Application != 0 )
        assert(s_Application->Thread_Exit(m_Idx));
}

CRef<CTestThread> thr[k_NumThreadsMax];

void* CTestThread::Main(void)
{
    int spawn_max;
    int first_idx;
    {{
        CFastMutexGuard spawn_guard(s_GlobalLock);
        spawn_max = s_NumThreads - s_NextIndex;
        if (spawn_max > s_SpawnBy) {
            spawn_max = s_SpawnBy;
        }
        first_idx = s_NextIndex;
        s_NextIndex += s_SpawnBy;
    }}
    // Spawn more threads
    for (int i = first_idx; i < first_idx + spawn_max; i++) {
        thr[i] = new CTestThread(i);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Run the test
    if ( s_Application != 0 && s_Application->Thread_Run(m_Idx) ) {
        return this;
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  Test application


CThreadedApp::CThreadedApp(void)
{
    s_Application = this;
}


CThreadedApp::~CThreadedApp(void)
{
    s_Application = 0;
}


void CThreadedApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // s_NumThreads
    arg_desc->AddDefaultKey
        ("threads", "NumThreads",
         "Total number of threads to create and run",
         CArgDescriptions::eInteger, NStr::IntToString(s_NumThreads));
    arg_desc->SetConstraint
        ("threads", new CArgAllow_Integers(k_NumThreadsMin, k_NumThreadsMax));

    // s_NumThreads (emulation in ST)
    arg_desc->AddDefaultKey
        ("repeats", "NumRepeats",
         "In non-MT mode only(!) -- how many times to repeat the test. "
         "If passed 0, then the value of argument `-threads' will be used.",
         CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint
        ("repeats", new CArgAllow_Integers(0, k_NumThreadsMax));

    // s_SpawnBy
    arg_desc->AddDefaultKey
        ("spawnby", "SpawnBy",
         "Threads spawning factor",
         CArgDescriptions::eInteger, NStr::IntToString(s_SpawnBy));
    arg_desc->SetConstraint
        ("spawnby", new CArgAllow_Integers(k_SpawnByMin, k_SpawnByMax));

    // Let test application add its own arguments
    TestApp_Args(*arg_desc);

    string prog_description =
        "MT-environment test";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CThreadedApp::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

#if !defined(_MT)
    s_NumThreads = args["repeats"].AsInteger();
    if ( !s_NumThreads )
#endif
        s_NumThreads = args["threads"].AsInteger();

#if !defined(_MT)
    // Set reasonable repeats if not set through the argument
    if (!args["repeats"].AsInteger()) {
        unsigned int repeats = s_NumThreads/6;
        if (repeats < 4)
            repeats = 4;
        if (repeats < s_NumThreads)
            s_NumThreads = repeats;
    }
#endif

    s_SpawnBy = args["spawnby"].AsInteger();

    //
    assert(TestApp_Init());

#if defined(_MT)
    LOG_POST("Running " << s_NumThreads << " threads");
#else
    LOG_POST("Simulating " << s_NumThreads << " threads in ST mode");
#endif

    int spawn_max;
    int first_idx;
    {{
        CFastMutexGuard spawn_guard(s_GlobalLock);
        spawn_max = s_NumThreads - s_NextIndex;
        if (spawn_max > s_SpawnBy) {
            spawn_max = s_SpawnBy;
        }
        first_idx = s_NextIndex;
        s_NextIndex += s_SpawnBy;
    }}

    // Create and run threads
    for (int i = first_idx; i < first_idx + spawn_max; i++) {
        thr[i] = new CTestThread(i);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i=0; i<s_NumThreads; i++) {
        void* ok;
        thr[i]->Join(&ok);
        assert(ok);
    }

    assert(TestApp_Exit());

    // Destroy all threads
    for (unsigned int i=0; i<s_NumThreads; i++) {
        thr[i].Reset();
    }

    return 0;
}


bool CThreadedApp::Thread_Init(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Run(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Exit(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Destroy(int /*idx*/)
{
    return true;
}

bool CThreadedApp::TestApp_Args(CArgDescriptions& /*args*/)
{
    return true;
}

bool CThreadedApp::TestApp_Init(void)
{
    return true;
}


bool CThreadedApp::TestApp_Exit(void)
{
    return true;
}


END_NCBI_SCOPE
