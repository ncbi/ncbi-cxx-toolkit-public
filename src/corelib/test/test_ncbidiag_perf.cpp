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
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbidiag.hpp>
#include <algorithm>
#include <thread>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestDiagApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    // parameters
    bool m_UseStdThreads;
    int m_SubThreads;
    int m_ContextCalls;
    // statistics
    CFastMutex m_Mutex;
    int m_StopWatchCalls;
    double m_StopWatchTime;
    int m_FirstCalls;
    double m_FirstTime;
    Int8 m_OtherCalls;
    double m_OtherTime;

    class CTestThread : public CThread
    {
    public:
        CTestThread(int call_count)
            : m_CallCount(call_count)
            {
            }
        
        virtual void* Main(void)
            {
                CStopWatch sw(CStopWatch::eStart);
                m_StopWatchTime = sw.Restart();
                CDiagContext::GetRequestContext();
                m_FirstTime = sw.Restart();
                for ( int i = 1; i < m_CallCount; ++i ) {
                    CDiagContext::GetRequestContext();
                }
                m_OtherTime = sw.Elapsed();
                return 0;
            }
        
        int m_CallCount;
        double m_StopWatchTime;
        double m_FirstTime;
        double m_OtherTime;
    };
};


bool CTestDiagApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddFlag("use-std-threads",
                 "Use std::thread for testing");
    args.AddDefaultKey("sub-threads", "SubThreads",
                       "Number of sub-threads to create",
                       CArgDescriptions::eInteger, "1000");
    args.AddDefaultKey("context-calls", "ContextCalls",
                       "Number of GetDiagContext() calls in sub-thread",
                       CArgDescriptions::eInteger, "100000");
    return true;
}


bool CTestDiagApp::Thread_Init(int idx)
{
    return true;
}

bool CTestDiagApp::Thread_Run(int idx)
{
    for ( int i = 0; i < m_SubThreads; ++i ) {
        int calls = int((8+(i+idx)%8)/15.*m_ContextCalls);
        double stop_watch_time = 1e9;
        double first_time = 1e9;
        double other_time = 1e9;
        if ( m_UseStdThreads ) {
            std::thread([&]
                        ()
                {
                    CStopWatch sw(CStopWatch::eStart);
                    stop_watch_time = sw.Restart();
                    CDiagContext::GetRequestContext();
                    first_time = sw.Restart();
                    for ( int i = 1; i < calls; ++i ) {
                        CDiagContext::GetRequestContext();
                    }
                    other_time = sw.Elapsed();
                }).join();
        }
        else {
            CRef<CTestThread> thr(new CTestThread(calls));
            thr->Run();
            void* exit_data = 0;
            thr->Join(&exit_data);
            stop_watch_time = thr->m_StopWatchTime;
            first_time = thr->m_FirstTime;
            other_time = thr->m_OtherTime;
        }
        CFastMutexGuard guard(m_Mutex);
        m_StopWatchCalls += 1;
        m_StopWatchTime += stop_watch_time;
        m_FirstCalls += 1;
        m_FirstTime += first_time;
        m_OtherCalls += calls-1;
        m_OtherTime += other_time;
    }
    return true;
}

bool CTestDiagApp::TestApp_Init(void)
{
    const CArgs& args = GetArgs();
    m_UseStdThreads = args["use-std-threads"];
    m_SubThreads = args["sub-threads"].AsInteger();
    m_ContextCalls = args["context-calls"].AsInteger();
    return true;
}

bool CTestDiagApp::TestApp_Exit(void)
{
    // Print statistics
    NcbiCout << "Average times for GetRequestContext() call:"
             << NcbiEndl;
    NcbiCout <<"watch: "<<(m_StopWatchTime/m_StopWatchCalls*1e6)<<" us"
             << NcbiEndl;
    NcbiCout <<"first: "<<(m_FirstTime/m_FirstCalls*1e6)<<" us"
             << NcbiEndl;
    NcbiCout <<"other: "<<(m_OtherTime/m_OtherCalls*1e6)<<" us"
             << NcbiEndl;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestDiagApp().AppMain(argc, argv);
}
