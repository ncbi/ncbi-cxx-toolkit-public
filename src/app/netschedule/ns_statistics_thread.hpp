#ifndef NETSCHEDULE_STATISTICS_THREAD__HPP
#define NETSCHEDULE_STATISTICS_THREAD__HPP

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
 * Authors:  Denis Vakatov (design), Sergey Satskiy (implementation)
 *
 * File Description: NetSchedule statistics thread
 *
 *
 */

#include <util/thread_nonstop.hpp>
#include "background_host.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;
class CDiagContext_Extra;



// Each queue has this set of counters.
// The counters are never reset, they are only increased.
class CStatisticsCounters
{
public:
    // The Pending(Done) state can be reached via 3 ways:
    // - RETURN (RDRB) received
    // - Timeout has happened
    // - FPUT (FRED) received.
    // So, to separate the cases, the ETransitionPathOption is introduced
    enum ETransitionPathOption {
        eNone       = 0,   // Basically, no timeout, no fail. Normal case for
                           // the vast majority of the transitions.
        eTimeout    = 1,
        eFail       = 2,
        eClear      = 3,
        eNewSession = 4
    };


    CStatisticsCounters();

    void PrintTransitions(CDiagContext_Extra &  extra) const;
    void CountTransition(CNetScheduleAPI::EJobStatus  from,
                         CNetScheduleAPI::EJobStatus  to,
                         ETransitionPathOption        path_option = eNone);
    void CountDBDeletion(size_t  count);
    void CountSubmit(size_t  count);

    static void  PrintTotal(void);

private:
    static string x_GetTransitionCounterName(size_t  index_from,
                                             size_t  index_to);

private:
    CAtomicCounter_WithAutoInit     m_Transitions[g_ValidJobStatusesSize]
                                                 [g_ValidJobStatusesSize];
    CAtomicCounter_WithAutoInit     m_SubmitCounter;
    CAtomicCounter_WithAutoInit     m_DBDeleteCounter;

    // There are 10 special transitions:
    CAtomicCounter_WithAutoInit     m_ToPendingDueToTimeoutCounter;
    CAtomicCounter_WithAutoInit     m_ToPendingDueToFailCounter;
    CAtomicCounter_WithAutoInit     m_ToPendingDueToClearCounter;
    CAtomicCounter_WithAutoInit     m_ToPendingDueToNewSessionCounter;
    CAtomicCounter_WithAutoInit     m_ToDoneDueToTimeoutCounter;
    CAtomicCounter_WithAutoInit     m_ToDoneDueToFailCounter;
    CAtomicCounter_WithAutoInit     m_ToDoneDueToClearCounter;
    CAtomicCounter_WithAutoInit     m_ToDoneDueToNewSessionCounter;
    CAtomicCounter_WithAutoInit     m_ToFailedDueToTimeoutCounter;
    CAtomicCounter_WithAutoInit     m_ToFailedDueToClearCounter;
    CAtomicCounter_WithAutoInit     m_ToFailedDueToNewSessionCounter;
    CAtomicCounter_WithAutoInit     m_ToReadFailedDueToTimeoutCounter;
    CAtomicCounter_WithAutoInit     m_ToReadFailedDueToClearCounter;
    CAtomicCounter_WithAutoInit     m_ToReadFailedDueToNewSessionCounter;
};



/// Thread class, prints the statistics counters
///
/// @internal
class CStatisticsThread : public CThreadNonStop
{
public:
    CStatisticsThread(CBackgroundHost &   host,
                      CQueueDataBase &    qdb,
                      unsigned            run_delay,
                      const bool &        logging)
    : CThreadNonStop(run_delay),
      m_Host(host),
      m_QueueDB(qdb),
      m_StatisticsLogging(logging)
    {}

    virtual void DoJob(void);

private:
    CStatisticsThread(const CStatisticsThread &);
    CStatisticsThread &  operator=(const CStatisticsThread &);

private:
    CBackgroundHost &       m_Host;
    CQueueDataBase &        m_QueueDB;
    const bool &            m_StatisticsLogging;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_STATISTICS_THREAD__HPP */

