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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <util/distribution.hpp>

#include <connect/services/grid_worker_app.hpp>

#include <corelib/ncbi_system.hpp>

USING_NCBI_SCOPE;

NCBI_PARAM_DECL(string, test, sleep_time_distr);
typedef NCBI_PARAM_TYPE(test, sleep_time_distr) TParam_SleepTimeDistr;
NCBI_PARAM_DEF_EX(string, test, sleep_time_distr, "0",
    eParam_NoThread, GRID_WORKER_TEST_SLEEP_TIME_DISTR);

NCBI_PARAM_DECL(double, test, failure_rate);
typedef NCBI_PARAM_TYPE(test, failure_rate) TParam_FailureRate;
NCBI_PARAM_DEF_EX(double, test, failure_rate, 0.0,
    eParam_NoThread, GRID_WORKER_TEST_FAILURE_RATE);


/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///
class CGridWorkerTest : public IWorkerNodeJob
{
public:
    CGridWorkerTest(const IWorkerNodeInitContext& context)
    {
        GetDiagContext().SetOldPostFormat(false);

        m_SleepTimeDistr.InitFromParameter("sleep_time_distr",
            TParam_SleepTimeDistr::GetDefault().c_str(), &m_Random);
    }

    virtual ~CGridWorkerTest() {}

    int Do(CWorkerNodeJobContext& context)
    {
        CNcbiIstream& is = context.GetIStream();
        string input_type;
        is >> input_type;
        if (input_type != "doubles") {
            context.CommitJobWithFailure(
                "This worker node can only process the 'doubles' input type.");
            return 1;
        }
        int vsize;
        is >> vsize;
        vector<double> v(vsize);
        for (int i = 0; i < vsize; ++i)
            is >> v[i];

        unsigned delay = m_SleepTimeDistr.GetNextValue();

        if (delay > 0)
            SleepMilliSec(delay);

        if (m_Random.GetRand() <
                TParam_FailureRate::GetDefault() * m_Random.GetMax())
            context.CommitJobWithFailure("FAILED");
        else {
            sort(v.begin(), v.end());

            CNcbiOstream& os = context.GetOStream();
            os << vsize << ' ';
            for (int i = 0; i < vsize; ++i)
                os << v[i] << ' ';
            context.CommitJob();
        }

        return 0;
    }

private:
    CRandom m_Random;
    CDiscreteDistribution m_SleepTimeDistr;
};

class CGridWorkerIdleTask : public IWorkerNodeIdleTask
{
public:
    CGridWorkerIdleTask(const IWorkerNodeInitContext& context) : m_Count(0)
    {
    }

    virtual ~CGridWorkerIdleTask() {}

    virtual void Run(CWorkerNodeIdleTaskContext& context)
    {
        LOG_POST("Staring idle task...");

        for (int i = 0; i < 3 && !context.IsShutdownRequested(); ++i) {
            LOG_POST("Idle task: iteration: " << i);

            SleepSec(2);
        }

        if (++m_Count % 3 == 0)
            context.SetRunAgain();

        LOG_POST("Stopping idle task...");
    }

private:
    int m_Count;
};

// Use this macro to implement the main function for the CGridWorkerTest
// with idle task CGridWorkerIdleTask.
NCBI_WORKERNODE_MAIN_EX(CGridWorkerTest, CGridWorkerIdleTask, 1.0.0);
