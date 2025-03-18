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
 *  Db Cassandra: class executing single thread of cassandra fullscan.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__WORKER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__WORKER_HPP

#include <corelib/ncbistl.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/consumer.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassandraFullscanWorker
{
    struct SProgressStatus
    {
        atomic_long ready_queries{0};
        condition_variable ready_condition;
        mutex ready_mutex;
    };
    struct SQueryContext
        : CCassDataCallbackReceiver
    {
        SQueryContext(const SQueryContext &) = delete;
        SQueryContext & operator=(const SQueryContext &) = delete;
        SQueryContext(SQueryContext &&) = delete;
        SQueryContext & operator=(SQueryContext &&) = delete;

        SQueryContext()
            : SQueryContext(nullptr, nullptr, 0)
        {}
        SQueryContext(CCassandraFullscanPlan::TQueryPtr q, shared_ptr<SProgressStatus> p, unsigned int max_retries)
            : query(std::move(q))
            , progress(std::move(p))
            , retires(max_retries)
        {}

        void OnData() override
        {
            {
                lock_guard _(progress->ready_mutex);
                ++progress->ready_queries;
                query_ready = true;
            }
            progress->ready_condition.notify_one();
        }

        ~SQueryContext() override
        {
            if (query) {
                query->Close();
                query = nullptr;
            }
        }

        CCassandraFullscanPlan::TQueryPtr query;
        unique_ptr<ICassandraFullscanConsumer> row_consumer{nullptr};
        shared_ptr<SProgressStatus> progress;
        unsigned int retires{0};
        atomic_bool query_ready{false};
    };

public:
    using TTaskProvider = function<CCassandraFullscanPlan::TQueryPtr()>;

    CCassandraFullscanWorker();
    CCassandraFullscanWorker(CCassandraFullscanWorker&&) = default;
    CCassandraFullscanWorker(const CCassandraFullscanWorker&) = delete;
    CCassandraFullscanWorker& operator=(CCassandraFullscanWorker&&) = default;
    CCassandraFullscanWorker& operator=(const CCassandraFullscanWorker&) = delete;

    CCassandraFullscanWorker& SetConsistency(CassConsistency value);
    CCassandraFullscanWorker& SetPageSize(unsigned int value);
    CCassandraFullscanWorker& SetConsumerFactory(TCassandraFullscanConsumerFactory consumer_factory);
    CCassandraFullscanWorker& SetMaxActiveStatements(unsigned int value);
    CCassandraFullscanWorker& SetTaskProvider(TTaskProvider provider);
    CCassandraFullscanWorker& SetMaxRetryCount(unsigned int max_retry_count);
    CCassandraFullscanWorker& SetConsumerCreationPolicy(ECassandraFullscanConsumerPolicy policy);

    void operator()();
    bool IsFinished() const;
    bool HadError() const;
    string GetFirstError() const;

private:
    bool x_StartQuery(size_t index);
    bool x_ProcessQueryResult(size_t index);
    CCassandraFullscanPlan::TQueryPtr x_GetNextTask();
    void x_ProcessError(string const & msg);
    void x_ProcessError(exception const & e);

    unique_ptr<ICassandraFullscanConsumer> m_RowConsumer{nullptr};
    TCassConsistency m_Consistency{CCassConsistency::kLocalQuorum};
    unsigned int m_PageSize{CCassandraFullscanRunner::kPageSizeDefault};
    unsigned int m_MaxActiveStatements{CCassandraFullscanRunner::kMaxActiveStatementsDefault};
    TTaskProvider m_TaskProvider;

    vector<shared_ptr<SQueryContext>> m_Queries;
    shared_ptr<SProgressStatus> m_ProgressStatus{make_shared<SProgressStatus>()};
    unique_ptr<atomic_long> m_ActiveQueries{make_unique<atomic_long>(0)};
    unsigned int m_QueryMaxRetryCount{CCassandraFullscanRunner::kMaxRetryCountDefault};
    TCassandraFullscanConsumerFactory m_ConsumerFactory;
    ECassandraFullscanConsumerPolicy m_ConsumerCreationPolicy{ECassandraFullscanConsumerPolicy::eOnePerThread};
    bool m_Finished{false};
    bool m_HadError{false};
    string m_FirstError;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__WORKER_HPP
