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

#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <thread>

#include <corelib/ncbistr.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassandraFullscanWorker::CCassandraFullscanWorker()
    : m_RowConsumer(nullptr)
    , m_Consistency(CASS_CONSISTENCY_LOCAL_QUORUM)
    , m_PageSize(CCassandraFullscanRunner::kPageSizeDefault)
    , m_MaxActiveStatements(CCassandraFullscanRunner::kMaxActiveStatementsDefault)
    , m_ReadyQueries(new CFutex(0))
    , m_ActiveQueries(new atomic_long(0))
    , m_QueryMaxRetryCount(CCassandraFullscanRunner::kMaxRetryCountDefault)
    , m_Finished(false)
    , m_HadError(false)
{
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetRowConsumer(unique_ptr<ICassandraFullscanConsumer> consumer)
{
    m_RowConsumer = move(consumer);
    return *this;
}

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

CCassandraFullscanWorker& CCassandraFullscanWorker::SetTaskProvider(TTaskProvider provider)
{
    m_TaskProvider = move(provider);
    return *this;
}

CCassandraFullscanWorker& CCassandraFullscanWorker::SetMaxRetryCount(unsigned int max_retry_count)
{
    m_QueryMaxRetryCount = max_retry_count;
    return *this;
}

CCassandraFullscanPlan::TQueryPtr CCassandraFullscanWorker::GetNextTask()
{
    if (m_TaskProvider) {
        return m_TaskProvider();
    }
    return nullptr;
}

void CCassandraFullscanWorker::ProcessError(string const& msg)
{
    if (m_FirstError.empty()) {
        m_FirstError = msg;
    }
    m_HadError = true;
}

void CCassandraFullscanWorker::ProcessError(exception const & e)
{
    if (m_FirstError.empty()) {
        m_FirstError = e.what();
    }
    m_HadError = true;
}

bool CCassandraFullscanWorker::StartQuery(size_t index)
{
    try {
        CCassandraFullscanPlan::TQueryPtr task = GetNextTask();
        if (task) {
            m_Queries[index] = make_shared<SQueryContext>(move(task), m_ReadyQueries, m_QueryMaxRetryCount);
            auto &context = m_Queries[index];
            context->query->SetOnData3(context);
            context->query->Query(m_Consistency, true, true, m_PageSize);
            (*m_ActiveQueries)++;
            return true;
        }
    }
    catch (const exception & e) {
        ProcessError(e);
    }
    return false;
}

bool CCassandraFullscanWorker::ProcessQueryResult(size_t index)
{
    assert(m_Queries[index] && m_Queries[index]->query != nullptr);
    bool do_next = true, restart_request = false, result = true;
    SQueryContext * context = m_Queries[index].get();
    context->data_ready = false;
    m_ReadyQueries->Dec(false);
    try {
        async_rslt_t state = context->query->NextRow();
        while (do_next && state == ar_dataready) {
            do_next = m_RowConsumer->ReadRow(*(context->query));
            if (do_next) {
                state = context->query->NextRow();
            }
        }
        if (!do_next || context->query->IsEOF()) {
            m_Queries[index].reset();
            (*m_ActiveQueries)--;
        }
    } catch (CCassandraException const & e) {
        if ((
                e.GetErrCode() == CCassandraException::eQueryTimeout
                || e.GetErrCode() == CCassandraException::eQueryFailedRestartable
            )
            && context->retires > 0
        ) {
            --context->retires;
            restart_request = true;
        } else {
            ProcessError(e);
            result = false;
        }
    } catch(exception const & e) {
        ProcessError(e);
        result = false;
    }

    if (restart_request) {
        string params;
        for (size_t i = 0; i < context->query->ParamCount(); ++i) {
            params += (i > 0 ? "," : "" ) + context->query->ParamAsStr(i);
        }
        ERR_POST(
            (context->retires > 1 ? Info : Warning) << "Query restarted! SQL - " << context->query->ToString()
            << " Params - (" << params  << ") Restart count - " << context->retires
        );
        try {
            context->query->RestartQuery(m_Consistency);
        } catch(exception const & e) {
            ERR_POST(Warning << "Query restart failed");
            ProcessError(e);
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
    m_Queries.clear();
    m_Queries.resize(m_MaxActiveStatements);
    m_Finished = false;
    bool need_exit = false, more_tasks = true, contunue_processing = true;
    while (!need_exit && !m_HadError) {
        for (
            size_t i = 0;
            more_tasks && i < m_Queries.size() && *m_ActiveQueries <= m_MaxActiveStatements;
            ++i
        ) {
            if (!m_Queries[i]) {
                more_tasks = StartQuery(i);
            }
        }

        m_ReadyQueries->WaitWhile(0, 10000);
        for (
            size_t i = 0;
            i < m_Queries.size()
                && m_ReadyQueries->Value() > 0
                && *m_ActiveQueries > 0
                && contunue_processing;
            ++i
        ) {
            if (m_Queries[i] && m_Queries[i]->data_ready) {
                if (m_Queries[i]->query == nullptr) {
                    string error_msg = "context.data_ready == true while context.query === nullptr";
                    ERR_POST(Warning << error_msg);
                    ProcessError(error_msg);
                } else {
                    contunue_processing = ProcessQueryResult(i);
                }
            }
        }
        contunue_processing = contunue_processing && m_RowConsumer->Tick();
        need_exit = (!more_tasks && *m_ActiveQueries == 0) || !contunue_processing;
    }
    m_Queries.clear();

    try {
        m_RowConsumer->Finalize();
    } catch (const exception &  e) {
        ProcessError(e);
    }
    m_Finished = contunue_processing && !m_HadError;
}

END_IDBLOB_SCOPE
