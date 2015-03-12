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
* Author:  Aaron Ucko
*
* File Description:
*   thread pool test
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/thread_pool.hpp>

#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;

CRandom s_RNG;

class CThreadPoolTester : public CNcbiApplication
{
protected:
    void Init(void);
    int  Run (void);
};

void CThreadPoolTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "test of thread pool correctness");

    arg_desc->AddDefaultKey("init_threads", "N",
                            "how many worker threads to spawn initially",
                            CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey("max_threads", "N",
                            "how many worker threads to allow total",
                            CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("requests", "N", "how many jobs to submit",
                            CArgDescriptions::eInteger, "10");
    arg_desc->AddDefaultKey("queue_size", "N",
                            "how many jobs to allow in the queue",
                            CArgDescriptions::eInteger, "2");
    arg_desc->AddDefaultKey("thread_pool_timeout", "N",
                            "WaitForRoom() timeout argument (in ms)",
                            CArgDescriptions::eInteger, "30");
    arg_desc->AddDefaultKey("max_processing_time", "N",
                            "maximum request processing time in ms",
                            CArgDescriptions::eInteger, "5");

    SetupArgDescriptions(arg_desc.release());

    s_RNG.SetSeed(CProcess::GetCurrentPid());
}

class CTestRequest : public CStdRequest
{
public:
    CTestRequest(int serial, int max_processing_time) :
        m_Serial(serial),
        m_MaxProcessingTime(max_processing_time)
    {
    }

protected:
    void Process(void);

private:
    int m_Serial;
    int m_MaxProcessingTime;
};

void CTestRequest::Process(void)
{
    int duration = s_RNG.GetRand(0, m_MaxProcessingTime);
    //LOG_POST("Request " << m_Serial << ": " << duration);
    SleepMilliSec(duration);
    //LOG_POST("Request " << m_Serial << " complete");
}

int CThreadPoolTester::Run(void)
{
    int status = 0;

    const CArgs& args = GetArgs();
    CStdPoolOfThreads pool(args["max_threads"].AsInteger(),
                           args["queue_size"].AsInteger());
    pool.Spawn(args["init_threads"].AsInteger());

    unsigned wait_timeout_ms =
            (unsigned) args["thread_pool_timeout"].AsInteger();
    unsigned wait_timeout_s = wait_timeout_ms / 1000;
    unsigned wait_timeout_ns = (wait_timeout_ms % 1000) * 1000 * 1000;

    int max_processing_time = args["max_processing_time"].AsInteger();

    try {
        for (int i = 0;  i < args["requests"].AsInteger();  ++i) {
            try {
                pool.WaitForRoom(wait_timeout_s, wait_timeout_ns);
            }
            catch (CBlockingQueueException&) {
                continue;
            }

            //LOG_POST("Request " << (i + 1) << " to be queued");
            pool.AcceptRequest(CRef<CStdRequest>(new CTestRequest(i + 1,
                    max_processing_time)));
        }
    } STD_CATCH_ALL("CThreadPoolTester: status " << (status = 1))

    pool.KillAllThreads(CStdPoolOfThreads::fKill_Wait);

    return status;
}

int main(int argc, const char* argv[])
{
    return CThreadPoolTester().AppMain(argc, argv);
}
