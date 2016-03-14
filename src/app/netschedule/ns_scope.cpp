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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   Net schedule scopes
 *
 */


#include <ncbi_pch.hpp>

#include "ns_scope.hpp"
#include "ns_queue.hpp"


BEGIN_NCBI_SCOPE


CNSScopeRegistry::CNSScopeRegistry()
{}


CNSScopeRegistry::~CNSScopeRegistry()
{
    Clear();
}


size_t CNSScopeRegistry::size(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_ScopeToJobs.size();
}


bool CNSScopeRegistry::CanAccept(const string &  scope,
                                 size_t  max_records) const
{
    CMutexGuard         guard(m_Lock);
    if (m_ScopeToJobs.size() < max_records)
        return true;

    if (scope == kNoScopeOnly)
        return true;

    return m_ScopeToJobs.find(scope) != m_ScopeToJobs.end();
}


TNSBitVector CNSScopeRegistry::GetJobs(const string &  scope) const
{
    TScopeToJobsMap::const_iterator   scope_it;

    CMutexGuard     guard(m_Lock);
    scope_it = m_ScopeToJobs.find(scope);
    if (scope_it == m_ScopeToJobs.end())
        return kEmptyBitVector;
    return scope_it->second;
}


TNSBitVector CNSScopeRegistry::GetAllJobsInScopes(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_AllScopedJobs;
}


void CNSScopeRegistry::AddJob(const string &  scope,
                              unsigned int  job_id)
{
    if (job_id == 0 || scope.empty() || scope == kNoScopeOnly)
        return;

    TScopeToJobsMap::iterator   scope_it;

    CMutexGuard     guard(m_Lock);
    if (m_AllScopedJobs[job_id])
        return;

    m_AllScopedJobs[job_id] = true;
    scope_it = m_ScopeToJobs.find(scope);
    if (scope_it == m_ScopeToJobs.end()) {
        TNSBitVector    scope_jobs;
        scope_jobs[job_id] = true;
        m_ScopeToJobs[scope] = scope_jobs;
    } else {
        scope_it->second[job_id] = true;
    }
}


void CNSScopeRegistry::AddJobs(const string &  scope,
                               unsigned int  first_job_id,
                               unsigned int  count)
{
    if (first_job_id == 0 || count == 0 ||
        scope.empty() || scope == kNoScopeOnly )
        return;

    TScopeToJobsMap::iterator   scope_it;

    CMutexGuard     guard(m_Lock);
    scope_it = m_ScopeToJobs.find(scope);
    if (scope_it == m_ScopeToJobs.end()) {
        TNSBitVector    scope_jobs;
        scope_jobs.set_range(first_job_id, first_job_id + count - 1);
        m_ScopeToJobs[scope] = scope_jobs;
    } else {
        scope_it->second.set_range(first_job_id, first_job_id + count - 1);
    }
}


void CNSScopeRegistry::RemoveJob(unsigned int  job_id)
{
    CMutexGuard     guard(m_Lock);
    if (!m_AllScopedJobs[job_id])
        return;

    m_AllScopedJobs[job_id] = false;
    for (TScopeToJobsMap::iterator  k = m_ScopeToJobs.begin();
            k != m_ScopeToJobs.end(); ++k) {
        if (k->second[job_id]) {
            k->second[job_id] = false;
            break;
        }
    }
}


string CNSScopeRegistry::Print(const CQueue *  queue,
                               size_t  batch_size,
                               bool  verbose) const
{
    string          result;
    deque<string>   scope_names = GetScopeNames();
    deque<string>   batch;

    while (!scope_names.empty()) {
        batch.push_back(scope_names.front());
        scope_names.pop_front();

        if (batch.size() >= batch_size) {
            result += x_PrintSelected(batch, queue, verbose);
            batch.clear();
        }
    }

    if (!batch.empty())
        result += x_PrintSelected(batch, queue, verbose);
    return result;
}


string
CNSScopeRegistry::x_PrintSelected(const deque<string> &  batch,
                                  const CQueue *  queue,
                                  bool  verbose) const
{
    string                          buffer;
    deque<string>::const_iterator   k = batch.begin();
    TScopeToJobsMap::const_iterator scope_it;

    CMutexGuard                         guard(m_Lock);
    for ( ; k != batch.end(); ++k ) {
        scope_it = m_ScopeToJobs.find(*k);
        if (scope_it != m_ScopeToJobs.end())
            buffer += x_PrintOne(*k, scope_it->second, queue, verbose);
    }

    return buffer;
}


string
CNSScopeRegistry::x_PrintOne(const string &  scope_name,
                             const TNSBitVector &  jobs,
                             const CQueue *  queue,
                             bool  verbose) const
{
    string      buffer;

    buffer += "OK:SCOPE: '" +
              NStr::PrintableString(scope_name) + "'\n"
              "OK:  NUMBER OF JOBS: " +
              NStr::NumericToString(jobs.count()) + "\n";

    if (verbose) {
        if (jobs.any()) {
            buffer += "OK:  JOBS:\n";

            TNSBitVector::enumerator    en(jobs.first());
            for ( ; en.valid(); ++en) {
                unsigned int    job_id = *en;
                TJobStatus      status = queue->GetJobStatus(job_id);
                buffer += "OK:    " + queue->MakeJobKey(job_id) + " " +
                          CNetScheduleAPI::StatusToString(status) + "\n";
            }
        }
        else
            buffer += "OK:  JOBS: NONE\n";
    }

    return buffer;
}


void CNSScopeRegistry::Clear(void)
{
    CMutexGuard         guard(m_Lock);
    m_ScopeToJobs.clear();
    m_AllScopedJobs.clear();
}


deque<string> CNSScopeRegistry::GetScopeNames(void) const
{
    deque<string>       result;

    CMutexGuard         guard(m_Lock);
    for (TScopeToJobsMap::const_iterator  k = m_ScopeToJobs.begin();
            k != m_ScopeToJobs.end(); ++k) {
        result.push_back(k->first);
    }
    return result;
}


unsigned int  CNSScopeRegistry::CollectGarbage(unsigned int  max_to_del)
{
    unsigned int                        del_count = 0;
    TScopeToJobsMap::const_iterator     it;
    deque<string>                       to_be_deleted;
    CMutexGuard                         guard(m_Lock);

    for (it = m_ScopeToJobs.begin(); it != m_ScopeToJobs.end(); ++it) {
        if (!it->second.any()) {
            to_be_deleted.push_back(it->first);
            ++del_count;
            if (del_count >= max_to_del)
                break;
        }
    }

    for (deque<string>::const_iterator k = to_be_deleted.begin();
            k != to_be_deleted.end(); ++k)
        m_ScopeToJobs.erase(*k);
    return to_be_deleted.size();
}


unsigned int  CNSScopeRegistry::CheckRemoveCandidates(void)
{
    unsigned int                        count = 0;
    TScopeToJobsMap::const_iterator     it;
    CMutexGuard                         guard(m_Lock);

    for (it = m_ScopeToJobs.begin(); it != m_ScopeToJobs.end(); ++it)
        if (!it->second.any())
            ++count;

    return count;
}


END_NCBI_SCOPE

