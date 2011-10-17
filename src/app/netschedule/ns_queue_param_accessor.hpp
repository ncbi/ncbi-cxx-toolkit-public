#ifndef NETSCHEDULE_NS_QUEUE_PARAM_ACCESSOR__HPP
#define NETSCHEDULE_NS_QUEUE_PARAM_ACCESSOR__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule queue parameters accessor
 *
 */

#include "ns_queue.hpp"



BEGIN_NCBI_SCOPE

class CQueueParamAccessor
{
public:
    CQueueParamAccessor(const CQueue &  queue) :
        m_Queue(queue), m_Guard(queue.m_ParamLock)
    {}

    int GetTimeout() const
    { return m_Queue.m_Timeout; }

    double GetNotifyTimeout() const
    { return m_Queue.m_NotifyTimeout; }

    int GetRunTimeout() const
    { return m_Queue.m_RunTimeout; }

    int GetRunTimeoutPrecision() const
    { return m_Queue.m_RunTimeoutPrecision; }

    unsigned int GetFailedRetries() const
    { return m_Queue.m_FailedRetries; }

    time_t GetBlacklistTime() const
    { return m_Queue.m_BlacklistTime; }

    time_t GetEmptyLifetime() const
    { return m_Queue.m_EmptyLifetime; }

    unsigned int GetMaxInputSize() const
    { return m_Queue.m_MaxInputSize; }

    unsigned int GetMaxOutputSize() const
    { return m_Queue.m_MaxOutputSize; }

    bool GetDenyAccessViolations() const
    { return m_Queue.m_DenyAccessViolations; }

    const CQueueClientInfoList &  GetProgramVersionList() const
    { return m_Queue.m_ProgramVersionList; }

    const CNetSchedule_AccessList &  GetSubmHosts() const
    { return m_Queue.m_SubmHosts; }

    const CNetSchedule_AccessList &  GetWnodeHosts() const
    { return m_Queue.m_WnodeHosts; }

    unsigned GetNumParams() const
    { return 13; }

    string GetParamName(unsigned int  n) const {
        switch (n) {
        case 0:  return "timeout";
        case 1:  return "notif_timeout";
        case 2:  return "run_timeout";
        case 3:  return "run_timeout_precision";
        case 4:  return "failed_retries";
        case 5:  return "blacklist_time";
        case 6:  return "empty_lifetime";
        case 7:  return "max_input_size";
        case 8:  return "max_output_size";
        case 9:  return "deny_access_violations";
        case 10: return "program";
        case 11: return "subm_host";
        case 12: return "wnode_host";
        default: return "";
        }
    }

    string GetParamValue(unsigned int  n) const {
        switch (n) {
        case 0:  return NStr::IntToString(m_Queue.m_Timeout);
        case 1:  return NStr::DoubleToString(m_Queue.m_NotifyTimeout);
        case 2:  return NStr::IntToString(m_Queue.m_RunTimeout);
        case 3:  return NStr::IntToString(m_Queue.m_RunTimeoutPrecision);
        case 4:  return NStr::IntToString(m_Queue.m_FailedRetries);
        case 5:  return NStr::Int8ToString(m_Queue.m_BlacklistTime);
        case 6:  return NStr::Int8ToString(m_Queue.m_EmptyLifetime);
        case 7:  return NStr::IntToString(m_Queue.m_MaxInputSize);
        case 8:  return NStr::IntToString(m_Queue.m_MaxOutputSize);
        case 9:  return m_Queue.m_DenyAccessViolations ? "true" : "false";
        case 10: return m_Queue.m_ProgramVersionList.Print();
        case 11: return m_Queue.m_SubmHosts.Print();
        case 12: return m_Queue.m_WnodeHosts.Print();
        default: return "";
        }
    }

private:
    const CQueue &      m_Queue;
    CReadLockGuard      m_Guard;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_QUEUE_PARAM_ACCESSOR__HPP */

