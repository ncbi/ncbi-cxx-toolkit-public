/*****************************************************************************
 *  $Id$
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
 *  Db Cassandra: class managing Cassandra fullscans.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>

#include <corelib/ncbistr.hpp>
#include <cassandra.h>
#include "worker.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

const unsigned int CCassandraFullscanRunner::kPageSizeDefault = 4096;
const unsigned int CCassandraFullscanRunner::kMaxActiveStatementsDefault = 256;
const unsigned int CCassandraFullscanRunner::kMaxRetryCountDefault = 5;

CCassandraFullscanRunner::CCassandraFullscanRunner()
    : m_ThreadCount(1)
    , m_Consistency(CASS_CONSISTENCY_LOCAL_QUORUM)
    , m_PageSize(kPageSizeDefault)
    , m_MaxActiveStatements(kMaxActiveStatementsDefault)
    , m_MaxRetryCount(kMaxRetryCountDefault)
{
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetThreadCount(size_t value)
{
    m_ThreadCount = value;
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetConsistency(CassConsistency value)
{
    m_Consistency = value;
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetPageSize(unsigned int value)
{
    if (value > 0) {
        m_PageSize = value;
    }
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetMaxActiveStatements(unsigned int value)
{
    if (value > 0) {
        m_MaxActiveStatements = value;
    }
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetConsumerFactory(
    TCassandraFullscanConsumerFactory consumer_factory
)
{
    m_ConsumerFactory = move(consumer_factory);
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetExecutionPlan(
    unique_ptr<ICassandraFullscanPlan> plan
)
{
    if (m_ExecutionPlan) {
        NCBI_THROW(CCassandraException, eSeqFailed,
               "Invalid sequence of operations, execution plan should not be overriden"
            );
    }
    m_ExecutionPlan = move(plan);
    return *this;
}

CCassandraFullscanRunner& CCassandraFullscanRunner::SetMaxRetryCount(
    unsigned int max_retry_count
)
{
    m_MaxRetryCount = max_retry_count;
    return *this;
}

bool CCassandraFullscanRunner::Execute()
{
    if (!m_ExecutionPlan) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "Invalid sequence of operations, execution plan should be provided"
        );
    }
    m_ExecutionPlan->Generate();
    ICassandraFullscanPlan* plan = m_ExecutionPlan.get();

    size_t thread_count = min(m_ThreadCount, plan->GetQueryCount());
    thread_count = max(thread_count, 1UL);
    if (thread_count > m_MaxActiveStatements) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "Invalid sequence of operations. Thread count is greater than max_active_statements total."
        );
    }
    vector<thread> worker_threads;
    vector<CCassandraFullscanWorker> workers;
    CFastMutex plan_mutex;
    worker_threads.reserve(thread_count - 1);
    workers.reserve(thread_count);

    CCassandraFullscanWorker::TTaskProvider task_provider;
    if (thread_count > 1) {
        task_provider = [&plan_mutex, plan]() -> CCassandraFullscanPlan::TQueryPtr {
            CCassandraFullscanPlan::TQueryPtr query;
            {
                CFastMutexGuard _(plan_mutex);
                query = plan->GetNextQuery();
            }
            return query;
        };
    } else {
        task_provider = [plan]() -> CCassandraFullscanPlan::TQueryPtr {
            return plan->GetNextQuery();
        };
    }

    // Creating workers
    for (size_t i = 0; i < thread_count; ++i) {
        CCassandraFullscanWorker worker;
        worker
            .SetConsistency(m_Consistency)
            .SetPageSize(m_PageSize)
            .SetMaxRetryCount(m_MaxRetryCount)
            .SetMaxActiveStatements(m_MaxActiveStatements / thread_count)
            .SetRowConsumer(m_ConsumerFactory())
            .SetTaskProvider(task_provider);
        workers.emplace_back(move(worker));
    }

    // Starting threads for all except the last worker
    for (size_t i = 0; i < workers.size() - 1; ++i) {
        worker_threads.emplace_back(
            [i, &workers]() {
                workers[i]();
            }
        );
    }

    // Executing last worker in the current thread
    workers[workers.size() - 1]();

    // Waiting for threads of all except the last worker
    for (thread& t : worker_threads) {
        t.join();
    }
    worker_threads.clear();

    bool all_finished = true;
    for (size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].HadError()) {
            NCBI_THROW(CCassandraException, eFatal,
               "Fullscan failed: one of workers got exception with message - " + workers[i].GetFirstError()
            );
        }
        if (!workers[i].IsFinished()) {
            all_finished = false;
        }
    }
    workers.clear();
    return all_finished;
}

END_IDBLOB_SCOPE
