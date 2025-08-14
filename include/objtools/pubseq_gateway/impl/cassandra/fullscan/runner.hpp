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

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__RUNNER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__RUNNER_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/consumer.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

#include <memory>
#include <mutex>
#include <vector>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassandraFullscanRunner
{
public:
    static constexpr unsigned int kPageSizeDefault{4096};
    static constexpr unsigned int kMaxActiveStatementsDefault{256};
    static constexpr unsigned int kMaxRetryCountDefault{5};
    static constexpr size_t kThreadCountDefault{1};
    static constexpr string_view kConsistencyDefault{"LOCAL_QUORUM"};

    CCassandraFullscanRunner();
    CCassandraFullscanRunner(const CCassandraFullscanRunner&) = delete;
    CCassandraFullscanRunner(CCassandraFullscanRunner&&) = delete;
    CCassandraFullscanRunner& operator=(const CCassandraFullscanRunner&) = delete;
    CCassandraFullscanRunner& operator=(CCassandraFullscanRunner&&) = delete;

    CCassandraFullscanRunner& SetThreadCount(size_t value);
    CCassandraFullscanRunner& SetConsistency(CassConsistency value);
    CCassandraFullscanRunner& SetPageSize(unsigned int value);
    CCassandraFullscanRunner& SetMaxActiveStatements(unsigned int value);
    CCassandraFullscanRunner& SetConsumerFactory(TCassandraFullscanConsumerFactory consumer_factory);
    CCassandraFullscanRunner& SetExecutionPlan(unique_ptr<ICassandraFullscanPlan> plan);
    CCassandraFullscanRunner& SetMaxRetryCount(unsigned int max_retry_count);
    CCassandraFullscanRunner& SetConsumerCreationPolicy(ECassandraFullscanConsumerPolicy policy);

    void ApplyConfiguration(IRegistry const* registry, string const& section);
    bool Execute(IRegistry const* registry, string const& section);
    bool Execute();

protected:
    size_t GetThreadCount() const;
    TCassConsistency GetConsistency() const;
    unsigned int GetPageSize() const;
    unsigned int GetMaxActiveStatements() const;
    unsigned int GetMaxRetryCount() const;

private:
    size_t m_ThreadCount{kThreadCountDefault};
    TCassConsistency m_Consistency{CCassConsistency::FromString(kConsistencyDefault)};
    unsigned int m_PageSize{kPageSizeDefault};
    unsigned int m_MaxActiveStatements{kMaxActiveStatementsDefault};
    TCassandraFullscanConsumerFactory m_ConsumerFactory{nullptr};
    mutex m_ConsumerFactoryMutex;
    unique_ptr<ICassandraFullscanPlan> m_ExecutionPlan;
    unsigned int m_MaxRetryCount{kMaxRetryCountDefault};
    ECassandraFullscanConsumerPolicy m_ConsumerCreationPolicy{ECassandraFullscanConsumerPolicy::eOnePerThread};
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__RUNNER_HPP
