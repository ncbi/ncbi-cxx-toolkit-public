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

    unsigned int GetFailedRetries() const
    { return m_Queue.m_FailedRetries; }

    unsigned int GetMaxInputSize() const
    { return m_Queue.m_MaxInputSize; }

    unsigned int GetMaxOutputSize() const
    { return m_Queue.m_MaxOutputSize; }

    unsigned GetNumParams() const
    { return 19; }

    string GetParamName(unsigned int  n) const {
        switch (n) {
        case 0:  return "timeout";
        case 1:  return "notif_hifreq_interval";
        case 2:  return "notif_hifreq_period";
        case 3:  return "notif_lofreq_mult";
        case 4:  return "notif_handicap";
        case 5:  return "dump_buffer_size";
        case 6:  return "run_timeout";
        case 7:  return "run_timeout_precision";
        case 8:  return "failed_retries";
        case 9:  return "blacklist_time";
        case 10: return "max_input_size";
        case 11: return "max_output_size";
        case 12: return "program";
        case 13: return "subm_host";
        case 14: return "wnode_host";
        case 15: return "dump_client_buffer_size";
        case 16: return "dump_aff_buffer_size";
        case 17: return "dump_group_buffer_size";
        case 18: return "scramble_job_keys";
        default: return "";
        }
    }

    string GetParamValue(unsigned int  n) const {
        switch (n) {
        case 0:  return NStr::NumericToString(m_Queue.m_Timeout.Sec());
        case 1:  return NS_FormatPreciseTimeAsSec(m_Queue.m_NotifHifreqInterval);
        case 2:  return NS_FormatPreciseTimeAsSec(m_Queue.m_NotifHifreqPeriod);
        case 3:  return NStr::NumericToString(m_Queue.m_NotifLofreqMult);
        case 4:  return NS_FormatPreciseTimeAsSec(m_Queue.m_HandicapTimeout);
        case 5:  return NStr::NumericToString(m_Queue.m_DumpBufferSize);
        case 6:  return NS_FormatPreciseTimeAsSec(m_Queue.m_RunTimeout);
        case 7:  return NS_FormatPreciseTimeAsSec(m_Queue.m_RunTimeoutPrecision);
        case 8:  return NStr::NumericToString(m_Queue.m_FailedRetries);
        case 9:  return NS_FormatPreciseTimeAsSec(m_Queue.m_BlacklistTime);
        case 10: return NStr::NumericToString(m_Queue.m_MaxInputSize);
        case 11: return NStr::NumericToString(m_Queue.m_MaxOutputSize);
        case 12: return m_Queue.m_ProgramVersionList.Print();
        case 13: return m_Queue.m_SubmHosts.Print("", ",");
        case 14: return m_Queue.m_WnodeHosts.Print("", ",");
        case 15: return NStr::NumericToString(m_Queue.m_DumpClientBufferSize);
        case 16: return NStr::NumericToString(m_Queue.m_DumpAffBufferSize);
        case 17: return NStr::NumericToString(m_Queue.m_DumpGroupBufferSize);
        case 18: return NStr::BoolToString(m_Queue.m_ScrambleJobKeys);
        default: return "";
        }
    }

private:
    const CQueue &      m_Queue;
    CFastMutexGuard     m_Guard;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_QUEUE_PARAM_ACCESSOR__HPP */

