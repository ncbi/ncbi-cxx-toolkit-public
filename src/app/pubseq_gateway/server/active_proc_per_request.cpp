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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <atomic>
using namespace std;

#include "pubseq_gateway_utils.hpp"
#include "active_proc_per_request.hpp"
#include "psgs_dispatcher.hpp"
#include "backlog_per_request.hpp"


SActiveProcPerRequest   g_ActiveProcPerRequest;
atomic<bool>            g_ActiveProcPerRequestLock;


void RegisterProcessorGroupName(const string &  group_name,
                                TProcessorPriority  priority)
{
    // There is no need to take a lock because populating the list happens once
    // when there is only one thread
    g_ActiveProcPerRequest.m_ProcPriorityToIndex[priority] =
        g_ActiveProcPerRequest.m_RegisteredProcessorGroups.size();
    g_ActiveProcPerRequest.m_RegisteredProcessorGroups.push_back(group_name);
}


void RegisterActiveProcGroup(CPSGS_Request::EPSGS_Type  request_type,
                             SProcessorGroup *  proc_group)
{
    size_t                      index = 0;
    auto *                      item = & g_ActiveProcPerRequest.m_ProcPerRequest[request_type];
    CSpinlockGuard              guard(&g_ActiveProcPerRequestLock);

    ++item->m_RequestCounter;
    for (const auto & proc: proc_group->m_Processors) {
        index = g_ActiveProcPerRequest.GetProcessorIndex(proc.m_Processor->GetPriority());
        ++item->m_ProcessorCounter[index];
    }
}


void UnregisterActiveProcGroup(CPSGS_Request::EPSGS_Type  request_type,
                               SProcessorGroup *  proc_group)
{
    size_t                      index = 0;
    auto *                      item = & g_ActiveProcPerRequest.m_ProcPerRequest[request_type];
    CSpinlockGuard              guard(&g_ActiveProcPerRequestLock);

    for (const auto & proc: proc_group->m_Processors) {
        index = g_ActiveProcPerRequest.GetProcessorIndex(proc.m_Processor->GetPriority());
        --item->m_ProcessorCounter[index];
    }

    --item->m_RequestCounter;
}


size_t  GetActiveProcGroupCounter(void)
{
    // The lock is taken when a snapshot is formed.
    // The aggregation is done on a snapshot.
    SActiveProcPerRequest   current = GetActiveProcGroupSnapshot();
    size_t                  total = 0;

    for (size_t  k = 0; k < sizeof(g_ActiveProcPerRequest.m_ProcPerRequest) / sizeof(SProcPerRequest); ++k) {
        total += g_ActiveProcPerRequest.m_ProcPerRequest[k].m_RequestCounter;
    }
    return total;
}


SActiveProcPerRequest  GetActiveProcGroupSnapshot(void)
{
    SActiveProcPerRequest       ret;
    CSpinlockGuard              guard(&g_ActiveProcPerRequestLock);
    ret = g_ActiveProcPerRequest;
    return ret;
}


void PopulatePerRequestMomentousDictionary(CJsonNode &  dict)
{
    // Populates per request momentous counters including backlog ones
    SActiveProcPerRequest   active_proc_snapshot = GetActiveProcGroupSnapshot();
    SBacklogPerRequest      backlog_snapshot = GetBacklogPerRequestSnapshot();
    CJsonNode               per_request(CJsonNode::NewObjectNode());

    for (size_t  k = 0; k < sizeof(g_ActiveProcPerRequest.m_ProcPerRequest) / sizeof(SProcPerRequest); ++k) {
        CJsonNode   one_request(CJsonNode::NewObjectNode());
        string      request_name = CPSGS_Request::TypeToString(
                                    static_cast<CPSGS_Request::EPSGS_Type>(k));

        size_t      proc_index = 0;
        for (const auto &  proc_group_name : active_proc_snapshot.m_RegisteredProcessorGroups) {
            one_request.SetInteger(proc_group_name,
                                   active_proc_snapshot.m_ProcPerRequest[k].m_ProcessorCounter[proc_index]);
            ++proc_index;
        }
        one_request.SetInteger("active_requests",
                               active_proc_snapshot.m_ProcPerRequest[k].m_RequestCounter);

        one_request.SetInteger("backlogged",
                               backlog_snapshot.m_BacklogPerRequest[k]);
        per_request.SetByKey(request_name, one_request);
    }

    dict.SetByKey("per_request", per_request);
}

