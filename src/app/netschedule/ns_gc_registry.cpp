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
 *   Net schedule garbage collection registry
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbistd.hpp>
#include <connect/services/netschedule_api.hpp>

#include "ns_gc_registry.hpp"


BEGIN_NCBI_SCOPE


CJobGCRegistry::CJobGCRegistry()
{}

CJobGCRegistry::~CJobGCRegistry()
{}


// Registers the job in the GC registry
void CJobGCRegistry::RegisterJob(unsigned int   job_id,
                                 unsigned int   aff_id,
                                 unsigned int   group_id,
                                 time_t         life_time)
{
    CFastMutexGuard     guard(m_Lock);
    SJobGCInfo          job_attr(aff_id, group_id, life_time);

    m_JobsAttrs[job_id] = job_attr;
}


// Returns true if the record has been deleted, i.e. the has to be marked
// for deletion. The aff_id and group_id are filled only if the job is
// deleted
bool CJobGCRegistry::DeleteIfTimedOut(unsigned int    job_id,
                                      time_t          current_time,
                                      unsigned int *  aff_id,
                                      unsigned int *  group_id)
{
    CFastMutexGuard                          guard(m_Lock);
    map<unsigned int, SJobGCInfo>::iterator  attrs = m_JobsAttrs.find(job_id);

    if (attrs == m_JobsAttrs.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Testing life time of non-registered job (ID: " +
                   NStr::UIntToString(job_id) + ")");

    if (current_time < attrs->second.m_LifeTime)
        return false;

    // The record should be deleted
    *aff_id = attrs->second.m_AffinityID;
    *group_id = attrs->second.m_GroupID;
    m_JobsAttrs.erase(attrs);
    return true;
}


// Updates the job life time
void CJobGCRegistry::UpdateLifetime(unsigned int  job_id,
                                    time_t        life_time)
{
    CFastMutexGuard                          guard(m_Lock);
    map<unsigned int, SJobGCInfo>::iterator  attrs = m_JobsAttrs.find(job_id);

    if (attrs == m_JobsAttrs.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Updating life time of non-registered job (ID: " +
                   NStr::UIntToString(job_id) + ")");

    attrs->second.m_LifeTime = life_time;
    return;
}


END_NCBI_SCOPE

