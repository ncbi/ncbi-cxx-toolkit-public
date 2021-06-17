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
#include <cassandra.h>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/consumer.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <utility>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassandraFullscanWorker
{
    struct SQueryContext
        : public CCassDataCallbackReceiver
    {
        SQueryContext(const SQueryContext &) = delete;
        SQueryContext & operator=(const SQueryContext &) = delete;
        SQueryContext(SQueryContext &&) = default;
        SQueryContext & operator=(SQueryContext &&) = default;

        SQueryContext()
            : SQueryContext(nullptr, nullptr, 0)
        {}
        SQueryContext(CCassandraFullscanPlan::TQueryPtr q, shared_ptr<CFutex> ready, unsigned int max_retries)
            : query(move(q))
            , data_ready(false)
            , total_ready(ready)
            , retires(max_retries)
        {}

        void OnData() override
        {
            data_ready = true;
            total_ready->Inc();
        }

        ~SQueryContext()
        {
            if (query) {
                query->Close();
                query = nullptr;
            }
        }

        CCassandraFullscanPlan::TQueryPtr query;
        atomic_bool data_ready;
        shared_ptr<CFutex> total_ready;
        unsigned int retires;
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
    CCassandraFullscanWorker& SetRowConsumer(unique_ptr<ICassandraFullscanConsumer> consumer);
    CCassandraFullscanWorker& SetMaxActiveStatements(unsigned int value);
    CCassandraFullscanWorker& SetTaskProvider(TTaskProvider provider);
    CCassandraFullscanWorker& SetMaxRetryCount(unsigned int max_retry_count);

    void operator()();
    bool IsFinished() const;
    bool HadError() const;
    string GetFirstError() const;

 private:
    bool StartQuery(size_t index);
    bool ProcessQueryResult(size_t index);
    CCassandraFullscanPlan::TQueryPtr GetNextTask();
    void ProcessError(string const & msg);
    void ProcessError(exception const & e);

    unique_ptr<ICassandraFullscanConsumer> m_RowConsumer;
    CassConsistency m_Consistency;
    unsigned int m_PageSize;
    unsigned int m_MaxActiveStatements;
    TTaskProvider m_TaskProvider;

    vector<shared_ptr<SQueryContext>> m_Queries;
    shared_ptr<CFutex> m_ReadyQueries;
    unique_ptr<atomic_long> m_ActiveQueries;
    unsigned int m_QueryMaxRetryCount;
    bool m_Finished;
    bool m_HadError;
    string m_FirstError;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__WORKER_HPP
