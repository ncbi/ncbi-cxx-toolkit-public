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
 *   Test for "NCBIDIAG" in multithreaded environment
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/03/30 22:46:59  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <algorithm>

USING_NCBI_SCOPE;

#ifndef _DEBUG
inline
void s_Verify(bool expr)
{
    if ( !expr ) {
        throw runtime_error("Test failed");
    }
}
#else
#include <assert.h>
#define s_Verify assert
#endif


/////////////////////////////////////////////////////////////////////////////
// Globals


const unsigned int   cNumThreadsMin = 1;
const unsigned int   cNumThreadsMax = 500;
const int   cSpawnByMin    = 1;
const int   cSpawnByMax    = 100;

static unsigned int  sNumThreads    = 40;
static int  sSpawnBy       = 13;

static unsigned int  s_NextIndex    = 0;


CFastMutex s_GlobalLock;


/////////////////////////////////////////////////////////////////////////////
// Test thread


class CTestThread : public CThread
{
public:
    CTestThread(int idx);
protected:
    ~CTestThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);
private:
    int m_Idx;
};


CTestThread::CTestThread(int idx) : m_Idx(idx)
{
}


CTestThread::~CTestThread(void)
{
}


void CTestThread::OnExit(void)
{
}

CRef<CTestThread> thr[cNumThreadsMax];

void* CTestThread::Main(void)
{
    for (int i = 0; i<sSpawnBy; i++) {
        int idx;
        {{
            CFastMutexGuard spawn_guard(s_GlobalLock);
            if (s_NextIndex >= sNumThreads) {
                break;
            }
            idx = s_NextIndex;
            s_NextIndex++;
        }}
        thr[idx] = new CTestThread(idx);
        thr[idx]->Run();
        LOG_POST("Thread " + NStr::IntToString(idx) + " created");
    }

    LOG_POST("LOG message from thread " + NStr::IntToString(m_Idx));
    ERR_POST("ERROR message from thread " + NStr::IntToString(m_Idx));

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  Test application

class CThreadedApp : public CNcbiApplication
{
    void Init(void);
    int Run(void);
};


void CThreadedApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // sNumThreads
    arg_desc->AddDefaultKey
        ("threads", "NumThreads",
         "Total number of threads to create and run",
         CArgDescriptions::eInteger, NStr::IntToString(sNumThreads));
    arg_desc->SetConstraint
        ("threads", new CArgAllow_Integers(cNumThreadsMin, cNumThreadsMax));

    // sSpawnBy
    arg_desc->AddDefaultKey
        ("spawnby", "SpawnBy",
         "Threads spawning factor",
         CArgDescriptions::eInteger, NStr::IntToString(sSpawnBy));
    arg_desc->SetConstraint
        ("spawnby", new CArgAllow_Integers(cSpawnByMin, cSpawnByMax));


    string prog_description =
        "NCBIDIAG test.";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CThreadedApp::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

    sNumThreads = args["threads"].AsInteger();
    sSpawnBy    = args["spawnby"].AsInteger();

    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG with "
             << NStr::IntToString(sNumThreads)
             << " threads..."
             << NcbiEndl;
    // Output to the string stream -- to verify the result
    static char str[cNumThreadsMax*200] = "";
    static ostrstream sout(str, cNumThreadsMax*200, ios::out);
    SetDiagStream(&sout);

    // Create and run threads
    for (int i=0; i<sSpawnBy; i++) {
        int idx;
        {{
            CFastMutexGuard spawn_guard(s_GlobalLock);
            if (s_NextIndex >= sNumThreads) {
                break;
            }
            idx = s_NextIndex;
            s_NextIndex++;
        }}

        thr[idx] = new CTestThread(idx);
        thr[idx]->Run();
        LOG_POST("Thread " + NStr::IntToString(idx) + " created");
    }

    // Wait for all threads
    for (unsigned int i=0; i<sNumThreads; i++) {
        thr[i]->Join();
    }

    // Verify the result
    string test_res(str);
    typedef list<string> string_list;
    string_list messages;

    // Get the list of messages and check the size
    NStr::Split(test_res, "\xA\xD", messages);

    s_Verify(messages.size() == sNumThreads*3);

    // Verify "created" messages
    for (unsigned int i=0; i<sNumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "Thread " + NStr::IntToString(i) + " created");
        s_Verify(it != messages.end());
        messages.erase(it);
    }
    s_Verify(messages.size() == sNumThreads*2);

    // Verify "Error" messages
    for (unsigned int i=0; i<sNumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "Error: ERROR message from thread " + NStr::IntToString(i));
        s_Verify(it != messages.end());
        messages.erase(it);
    }
    s_Verify(messages.size() == sNumThreads);

    // Verify "Log" messages
    for (unsigned int i=0; i<sNumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "LOG message from thread " + NStr::IntToString(i));
        s_Verify(it != messages.end());
        messages.erase(it);
    }
    s_Verify(messages.size() == 0);

    // Cleaunp
    SetDiagStream(0);

    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl << NcbiEndl;
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CThreadedApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}
