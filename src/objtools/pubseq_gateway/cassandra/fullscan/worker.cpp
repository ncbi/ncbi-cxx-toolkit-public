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

#include <ncbi_pch.hpp>

#include "worker.hpp"

#include <chrono>
#include <condition_variable>
#include <utility>
#include <string>

#include <corelib/ncbistr.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassandraFullscanWorker::CCassandraFullscanWorker() = default;
CCassandraFullscanWorker& CCassandraFullscanWorker::SetConsistency(CassConsistency value)
{
    m_Consistency = value;
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetPageSize(unsigned int value)
{
    if (value > 0) {
        m_PageSize = value;
    }
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetMaxActiveStatements(unsigned int value)
{
    if (value > 0) {
        m_MaxActiveStatements = value;
    }
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetConsumerFactory(
    TCassandraFullscanConsumerFactory consumer_factory
)
{
    m_ConsumerFactory = std::move(consumer_factory);
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetConsumerCreationPolicy(ECassandraFullscanConsumerPolicy policy)
{
    m_ConsumerCreationPolicy = policy;
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetTaskProvider(TTaskProvider provider)
{
    m_TaskProvider = std::move(provider);
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetMaxRetryCount(unsigned int max_retry_count)
{
    m_QueryMaxRetryCount = max_retry_count;
    return *this;
}

CCassandraFullscanPlan::TQueryPtr CCassandraFullscanWorker::x_GetNextTask()
{
    if (m_TaskProvider) {
        return m_TaskProvider();
    }
    return nullptr;
}

void CCassandraFullscanWorker::x_ProcessError(string const& msg)
{
    if (m_FirstError.empty()) {
        m_FirstError = msg;
    }
    m_HadError = true;
}

void CCassandraFullscanWorker::x_ProcessError(exception const & e)
{
    if (m_FirstError.empty()) {
        m_FirstError = e.what();
    }
    m_HadError = true;
}

bool CCassandraFullscanWorker::x_StartQuery(size_t index)
{
    try {
        CCassandraFullscanPlan::TQueryPtr task = x_GetNextTask();
        if (task) {
            m_Queries[index] = make_shared<SQueryContext>(std::move(task), m_ProgressStatus, m_QueryMaxRetryCount);
            m_Queries[index]->query->SetOnData3(m_Queries[index]);
            m_Queries[index]->query->Query(m_Consistency, true, true, m_PageSize);
            if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerQuery) {
                m_Queries[index]->row_consumer = m_ConsumerFactory();
            }
            (*m_ActiveQueries)++;
            return true;
        }
    }
    catch (const exception & e) {
        x_ProcessError(e);
    }
    return false;
}

bool CCassandraFullscanWorker::x_ProcessQueryResult(size_t index)
{
    assert(m_Queries[index] && m_Queries[index]->query != nullptr);
    bool do_next{true}, restart_request{false}, result{true};
    ICassandraFullscanConsumer* consumer = m_Queries[index]->row_consumer == nullptr
        ? m_RowConsumer.get() : m_Queries[index]->row_consumer.get();
    m_Queries[index]->query_ready = false;
    --m_ProgressStatus->ready_queries;
    try {
        auto state = m_Queries[index]->query->NextRow();
        while (do_next && state == ar_dataready) {
            do_next = consumer->ReadRow(*(m_Queries[index]->query));
            if (do_next) {
                state = m_Queries[index]->query->NextRow();
            }
        }
        if (!do_next || m_Queries[index]->query->IsEOF()) {
            try {
                if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerQuery) {
                    m_Queries[index]->row_consumer->Finalize();
                    m_Queries[index]->row_consumer = nullptr;
                }
            } catch (const exception &  e) {
                x_ProcessError(e);
            }
            m_Queries[index].reset();
            (*m_ActiveQueries)--;
        }
    } catch (CCassandraException const & e) {
        if ((
                e.GetErrCode() == CCassandraException::eQueryTimeout
                || e.GetErrCode() == CCassandraException::eQueryFailedRestartable
            )
            && m_Queries[index]->retires > 0
        ) {
            --m_Queries[index]->retires;
            restart_request = true;
        } else {
            x_ProcessError(e);
            result = false;
        }
    } catch(exception const & e) {
        x_ProcessError(e);
        result = false;
    }

    if (restart_request) {
        try {
            ERR_POST(Warning << "Fullscan query retry: retries left - " << m_Queries[index]->retires);
            m_Queries[index]->query->Restart(m_Consistency);
            consumer->OnQueryRestart();
        } catch(exception const & e) {
            ERR_POST(Error << "Query restart failed");
            x_ProcessError(e);
            result = false;
        }
    }
    return result && do_next;
}

bool CCassandraFullscanWorker::IsFinished() const
{
    return m_Finished;
}

bool CCassandraFullscanWorker::HadError() const
{
    return m_HadError;
}

string CCassandraFullscanWorker::GetFirstError() const
{
    return m_FirstError;
}

void CCassandraFullscanWorker::operator()()
{
    if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerThread) {
        m_RowConsumer = m_ConsumerFactory();
    }
    m_Queries.clear();
    m_Queries.resize(m_MaxActiveStatements);
    m_Finished = false;
    bool need_exit{false}, more_tasks{true}, contunue_processing{true};
    while (!need_exit && !m_HadError) {
        for (
            size_t i = 0;
            more_tasks && i < m_Queries.size() && *m_ActiveQueries <= m_MaxActiveStatements;
            ++i
        ) {
            if (!m_Queries[i]) {
                more_tasks = x_StartQuery(i);
            }
        }

        //Wait for query completion
        {
            unique_lock ready_lock(m_ProgressStatus->ready_mutex);
            m_ProgressStatus->ready_condition.wait_for(
                ready_lock,
                chrono::milliseconds(10),
                [this] { return m_ProgressStatus->ready_queries > 0; }
            );
        }
        for (
            size_t i = 0;
            i < m_Queries.size()
                && m_ProgressStatus->ready_queries > 0
                && *m_ActiveQueries > 0
                && contunue_processing;
            ++i
        ) {
            if (m_Queries[i] && m_Queries[i]->query_ready) {
                if (m_Queries[i]->query == nullptr) {
                    string error_msg = "m_Queries[index].data_ready == true while m_Queries[index].query == nullptr";
                    ERR_POST(Warning << error_msg);
                    x_ProcessError(error_msg);
                } else {
                    contunue_processing = x_ProcessQueryResult(i);
                }
            }
        }
        if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerThread) {
            contunue_processing = contunue_processing && m_RowConsumer->Tick();
        }
        else if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerQuery) {
            for (auto& query : m_Queries) {
                if (query && query->row_consumer) {
                    contunue_processing = contunue_processing && query->row_consumer->Tick();
                };
            }
        }

        need_exit = (!more_tasks && *m_ActiveQueries == 0) || !contunue_processing;
    }
    m_Queries.clear();

    try {
        if (m_ConsumerCreationPolicy == ECassandraFullscanConsumerPolicy::eOnePerThread) {
            m_RowConsumer->Finalize();
        }
    } catch (const exception &  e) {
        x_ProcessError(e);
    }
    m_Finished = contunue_processing && !m_HadError;
}

END_IDBLOB_SCOPE
