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
 * File Description: NetSchedule statistics counters
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "queue_database.hpp"
#include "ns_statistics_counters.hpp"
#include "ns_handler.hpp"
#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE


// Only valid statuses as strings
static bool     s_Initialized = false;
static string   s_ValidStatusesNames[g_ValidJobStatusesSize];
static size_t   s_StatusToIndex[CNetScheduleAPI::eLastStatus];


static CStatisticsCounters  s_ServerWide(
                                    CStatisticsCounters::eServerWideCounters);
static CStatisticsCounters  s_ServerWideLastPrinted(
                                    CStatisticsCounters::eServerWideCounters);
static CNSPreciseTime       s_ServerWideLastPrintedTimestamp(0.0);



CStatisticsCounters::CStatisticsCounters(ECountersScope  scope) :
    m_Scope(scope)
{
    if (s_Initialized == false) {
        s_Initialized = true;

        for (size_t  index = 0; index < g_ValidJobStatusesSize; ++index)
            s_ValidStatusesNames[index] =
                CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]);

        for (int  status = CNetScheduleAPI::ePending;
             status < CNetScheduleAPI::eLastStatus; ++status )
            for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k)
                if (g_ValidJobStatuses[k] == status)
                    s_StatusToIndex[status] = k;
    }

    // Mark invalid transitions with the counter value -1.
    // There are less valid transitions than invalid, so first mark all as
    // invalid and then specifically initialize valid to 0.
    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {
            m_Transitions[index_from][index_to].Set(
                                static_cast<TNCBIAtomicValue>(-1));
        }
    }

    // Set to 0 those counters which are allowed
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::ePending]]
                 [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::ePending]]
                 [s_StatusToIndex[CNetScheduleAPI::eRunning]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                 [s_StatusToIndex[CNetScheduleAPI::ePending]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                 [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                 [s_StatusToIndex[CNetScheduleAPI::eFailed]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eDone]]
                 [s_StatusToIndex[CNetScheduleAPI::eReading]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eDone]]
                 [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eDone]]
                 [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReading]]
                 [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReading]]
                 [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReading]]
                 [s_StatusToIndex[CNetScheduleAPI::eReadFailed]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReading]]
                 [s_StatusToIndex[CNetScheduleAPI::eFailed]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eConfirmed]]
                 [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReadFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eReadFailed]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eFailed]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eReading]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::ePending]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eCanceled]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eDone]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReading]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eConfirmed]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eReadFailed]]
                 [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);

    m_Transitions[s_StatusToIndex[CNetScheduleAPI::eCanceled]]
                 [s_StatusToIndex[CNetScheduleAPI::eReading]].Set(0);
    return;
}


void CStatisticsCounters::PrintTransitions(CDiagContext_Extra &  extra) const
{
    extra.Print("TOTAL_submits", m_SubmitCounter.Get())
         .Print("TOTAL_ns_submit_rollbacks", m_NSSubmitRollbackCounter.Get())
         .Print("TOTAL_ns_get_rollbacks", m_NSGetRollbackCounter.Get())
         .Print("TOTAL_ns_read_rollbacks", m_NSReadRollbackCounter.Get())
         .Print("TOTAL_picked_as_pending_outdated", m_PickedAsPendingOutdated.Get())
         .Print("TOTAL_picked_as_read_outdated", m_PickedAsReadOutdated.Get())
         .Print("TOTAL_dbdeletions", m_DBDeleteCounter.Get());

    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {

            // All invalid transitions are marked as -1
            if (m_Transitions[index_from][index_to].Get() !=
                    static_cast<TNCBIAtomicValue>(-1))
                extra.Print("TOTAL_" +
                            x_GetTransitionCounterName(index_from, index_to),
                            m_Transitions[index_from][index_to].Get());
        }
    }
    extra.Print("TOTAL_Running_Pending_timeout",
                m_ToPendingDueToTimeoutCounter.Get())
         .Print("TOTAL_Running_Pending_fail",
                m_ToPendingDueToFailCounter.Get())
         .Print("TOTAL_Running_Pending_clear",
                m_ToPendingDueToClearCounter.Get())
         .Print("TOTAL_Running_Failed_clear",
                m_ToFailedDueToClearCounter.Get())
         .Print("TOTAL_Running_Pending_new_session",
                m_ToPendingDueToNewSessionCounter.Get())
         .Print("TOTAL_Running_Pending_without_blacklist",
                m_ToPendingWithoutBlacklist.Get())
         .Print("TOTAL_Running_Pending_rescheduled",
                m_ToPendingRescheduled.Get())
         .Print("TOTAL_Running_Failed_new_session",
                m_ToFailedDueToNewSessionCounter.Get())
         .Print("TOTAL_Running_Failed_timeout",
                m_ToFailedDueToTimeoutCounter.Get())
         .Print("TOTAL_Reading_Done_timeout",
                m_ToDoneDueToTimeoutCounter.Get())
         .Print("TOTAL_Reading_Done_fail",
                m_ToDoneDueToFailCounter.Get())
         .Print("TOTAL_Reading_Done_clear",
                m_ToDoneDueToClearCounter.Get())
         .Print("TOTAL_Reading_ReadFailed_clear",
                m_ToReadFailedDueToClearCounter.Get())
         .Print("TOTAL_Reading_Done_new_session",
                m_ToDoneDueToNewSessionCounter.Get())
         .Print("TOTAL_Reading_ReadFailed_new_session",
                m_ToReadFailedDueToNewSessionCounter.Get())
         .Print("TOTAL_Reading_ReadFailed_timeout",
                m_ToReadFailedDueToTimeoutCounter.Get())
         .Print("TOTAL_Reading_Canceled_read_timeout",
                m_ToCanceledDueToReadTimeoutCounter.Get())
         .Print("TOTAL_Reading_Failed_read_timeout",
                m_ToFailedDueToReadTimeoutCounter.Get())
         .Print("TOTAL_Reading_Canceled_new_session",
                m_ToCanceledDueToReadNewSessionCounter.Get())
         .Print("TOTAL_Reading_Canceled_clear",
                m_ToCanceledDueToReadClearCounter.Get())
         .Print("TOTAL_Reading_Failed_new_session",
                m_ToFailedDueToReadNewSessionCounter.Get())
         .Print("TOTAL_Reading_Failed_clear",
                m_ToFailedDueToReadClearCounter.Get());
}


void CStatisticsCounters::PrintDelta(CDiagContext_Extra &  extra,
                                     const CStatisticsCounters &  prev) const
{
    extra.Print("DELTA_submits",
                m_SubmitCounter.Get() -
                prev.m_SubmitCounter.Get())
         .Print("DELTA_ns_submit_rollbacks",
                m_NSSubmitRollbackCounter.Get() -
                prev.m_NSSubmitRollbackCounter.Get())
         .Print("DELTA_ns_get_rollbacks",
                m_NSGetRollbackCounter.Get() -
                prev.m_NSGetRollbackCounter.Get())
         .Print("DELTA_ns_read_rollbacks",
                m_NSReadRollbackCounter.Get() -
                prev.m_NSReadRollbackCounter.Get())
         .Print("DELTA_picked_as_pending_outdated",
                m_PickedAsPendingOutdated.Get() -
                prev.m_PickedAsPendingOutdated.Get())
         .Print("DELTA_picked_as_read_outdated",
                m_PickedAsReadOutdated.Get() -
                prev.m_PickedAsReadOutdated.Get())
         .Print("DELTA_dbdeletions",
                m_DBDeleteCounter.Get() -
                prev.m_DBDeleteCounter.Get());

    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {

            // All invalid transitions are marked as -1
            if (m_Transitions[index_from][index_to].Get() !=
                    static_cast<TNCBIAtomicValue>(-1))
                extra.Print("DELTA_" +
                            x_GetTransitionCounterName(index_from, index_to),
                            m_Transitions[index_from][index_to].Get() -
                            prev.m_Transitions[index_from][index_to].Get());
        }
    }

    extra.Print("DELTA_Running_Pending_timeout",
                m_ToPendingDueToTimeoutCounter.Get() -
                prev.m_ToPendingDueToTimeoutCounter.Get())
         .Print("DELTA_Running_Pending_fail",
                m_ToPendingDueToFailCounter.Get() -
                prev.m_ToPendingDueToFailCounter.Get())
         .Print("DELTA_Running_Pending_clear",
                m_ToPendingDueToClearCounter.Get() -
                prev.m_ToPendingDueToClearCounter.Get())
         .Print("DELTA_Running_Failed_clear",
                m_ToFailedDueToClearCounter.Get() -
                prev.m_ToFailedDueToClearCounter.Get())
         .Print("DELTA_Running_Pending_new_session",
                m_ToPendingDueToNewSessionCounter.Get() -
                prev.m_ToPendingDueToNewSessionCounter.Get())
         .Print("DELTA_Running_Pending_without_blacklist",
                m_ToPendingWithoutBlacklist.Get() -
                prev.m_ToPendingWithoutBlacklist.Get())
         .Print("DELTA_Running_Pending_rescheduled",
                m_ToPendingRescheduled.Get() -
                prev.m_ToPendingRescheduled.Get())
         .Print("DELTA_Running_Failed_new_session",
                m_ToFailedDueToNewSessionCounter.Get() -
                prev.m_ToFailedDueToNewSessionCounter.Get())
         .Print("DELTA_Running_Failed_timeout",
                m_ToFailedDueToTimeoutCounter.Get() -
                prev.m_ToFailedDueToTimeoutCounter.Get())
         .Print("DELTA_Reading_Done_timeout",
                m_ToDoneDueToTimeoutCounter.Get() -
                prev.m_ToDoneDueToTimeoutCounter.Get())
         .Print("DELTA_Reading_Done_fail",
                m_ToDoneDueToFailCounter.Get() -
                prev.m_ToDoneDueToFailCounter.Get())
         .Print("DELTA_Reading_Done_clear",
                m_ToDoneDueToClearCounter.Get() -
                prev.m_ToDoneDueToClearCounter.Get())
         .Print("DELTA_Reading_ReadFailed_clear",
                m_ToReadFailedDueToClearCounter.Get() -
                prev.m_ToReadFailedDueToClearCounter.Get())
         .Print("DELTA_Reading_Done_new_session",
                m_ToDoneDueToNewSessionCounter.Get() -
                prev.m_ToDoneDueToNewSessionCounter.Get())
         .Print("DELTA_Reading_ReadFailed_new_session",
                m_ToReadFailedDueToNewSessionCounter.Get() -
                prev.m_ToReadFailedDueToNewSessionCounter.Get())
         .Print("DELTA_Reading_ReadFailed_timeout",
                m_ToReadFailedDueToTimeoutCounter.Get() -
                prev.m_ToReadFailedDueToTimeoutCounter.Get())
         .Print("DELTA_Reading_Canceled_read_timeout",
                m_ToCanceledDueToReadTimeoutCounter.Get() -
                prev.m_ToCanceledDueToReadTimeoutCounter.Get())
         .Print("DELTA_Reading_Failed_read_timeout",
                m_ToFailedDueToReadTimeoutCounter.Get() -
                prev.m_ToFailedDueToReadTimeoutCounter.Get())
         .Print("DELTA_Reading_Canceled_new_session",
                m_ToCanceledDueToReadNewSessionCounter.Get() -
                prev.m_ToCanceledDueToReadNewSessionCounter.Get())
         .Print("DELTA_Reading_Canceled_clear",
                m_ToCanceledDueToReadClearCounter.Get() -
                prev.m_ToCanceledDueToReadClearCounter.Get())
         .Print("DELTA_Reading_Failed_new_session",
                m_ToFailedDueToReadNewSessionCounter.Get() -
                prev.m_ToFailedDueToReadNewSessionCounter.Get())
         .Print("DELTA_Reading_Failed_clear",
                m_ToFailedDueToReadClearCounter.Get() -
                prev.m_ToFailedDueToReadClearCounter.Get());
}


string CStatisticsCounters::PrintTransitions(void) const
{
    string result = "OK:submits: " +
                    NStr::NumericToString(m_SubmitCounter.Get()) + "\n"
                    "OK:ns_submit_rollbacks: " +
                    NStr::NumericToString(m_NSSubmitRollbackCounter.Get()) + "\n"
                    "OK:ns_get_rollbacks: " +
                    NStr::NumericToString(m_NSGetRollbackCounter.Get()) + "\n"
                    "OK:ns_read_rollbacks: " +
                    NStr::NumericToString(m_NSReadRollbackCounter.Get()) + "\n"
                    "OK:picked_as_pending_outdated: " +
                    NStr::NumericToString(m_PickedAsPendingOutdated.Get()) + "\n"
                    "OK:picked_as_read_outdated: " +
                    NStr::NumericToString(m_PickedAsReadOutdated.Get()) + "\n"
                    "OK:dbdeletions: " +
                    NStr::NumericToString(m_DBDeleteCounter.Get()) + "\n";

    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {

            // All invalid transitions are marked as -1
            if (m_Transitions[index_from][index_to].Get() !=
                    static_cast<TNCBIAtomicValue>(-1))
                result += "OK:" +
                          x_GetTransitionCounterName(index_from, index_to) + ": " +
                          NStr::NumericToString(m_Transitions[index_from][index_to].Get()) + "\n";
        }
    }
    return result +
           "OK:Running_Pending_timeout: " +
           NStr::NumericToString(m_ToPendingDueToTimeoutCounter.Get()) + "\n"
           "OK:Running_Pending_fail: " +
           NStr::NumericToString(m_ToPendingDueToFailCounter.Get()) + "\n"
           "OK:Running_Pending_clear: " +
           NStr::NumericToString(m_ToPendingDueToClearCounter.Get()) + "\n"
           "OK:Running_Failed_clear: " +
           NStr::NumericToString(m_ToFailedDueToClearCounter.Get()) + "\n"
           "OK:Running_Pending_new_session: " +
           NStr::NumericToString(m_ToPendingDueToNewSessionCounter.Get()) + "\n"
           "OK:Running_Pending_without_blacklist: " +
           NStr::NumericToString(m_ToPendingWithoutBlacklist.Get()) + "\n"
           "OK:Running_Pending_rescheduled: " +
           NStr::NumericToString(m_ToPendingRescheduled.Get()) + "\n"
           "OK:Running_Failed_new_session: " +
           NStr::NumericToString(m_ToFailedDueToNewSessionCounter.Get()) + "\n"
           "OK:Running_Failed_timeout: " +
           NStr::NumericToString(m_ToFailedDueToTimeoutCounter.Get()) + "\n"
           "OK:Reading_Done_timeout: " +
           NStr::NumericToString(m_ToDoneDueToTimeoutCounter.Get()) + "\n"
           "OK:Reading_Done_fail: " +
           NStr::NumericToString(m_ToDoneDueToFailCounter.Get()) + "\n"
           "OK:Reading_Done_clear: " +
           NStr::NumericToString(m_ToDoneDueToClearCounter.Get()) + "\n"
           "OK:Reading_ReadFailed_clear: " +
           NStr::NumericToString(m_ToReadFailedDueToClearCounter.Get()) + "\n"
           "OK:Reading_Done_new_session: " +
           NStr::NumericToString(m_ToDoneDueToNewSessionCounter.Get()) + "\n"
           "OK:Reading_ReadFailed_new_session: " +
           NStr::NumericToString(m_ToReadFailedDueToNewSessionCounter.Get()) + "\n"
           "OK:Reading_ReadFailed_timeout: " +
           NStr::NumericToString(m_ToReadFailedDueToTimeoutCounter.Get()) + "\n"
           "OK:Reading_Canceled_read_timeout: " +
           NStr::NumericToString(m_ToCanceledDueToReadTimeoutCounter.Get()) + "\n"
           "OK:Reading_Failed_read_timeout: " +
           NStr::NumericToString(m_ToFailedDueToReadTimeoutCounter.Get()) + "\n"
           "OK:Reading_Canceled_new_session: " +
           NStr::NumericToString(m_ToCanceledDueToReadNewSessionCounter.Get()) + "\n"
           "OK:Reading_Canceled_clear: " +
           NStr::NumericToString(m_ToCanceledDueToReadClearCounter.Get()) + "\n"
           "OK:Reading_Failed_new_session: " +
           NStr::NumericToString(m_ToFailedDueToReadNewSessionCounter.Get()) + "\n"
           "OK:Reading_Failed_clear: " +
           NStr::NumericToString(m_ToFailedDueToReadClearCounter.Get()) + "\n";
}


void
CStatisticsCounters::CountTransition(CNetScheduleAPI::EJobStatus  from,
                                     CNetScheduleAPI::EJobStatus  to,
                                     ETransitionPathOption        path_option)
{
    size_t      index_from = s_StatusToIndex[from];
    size_t      index_to = s_StatusToIndex[to];

    if (path_option == eNone) {
        #if defined(_DEBUG) && !defined(NDEBUG)
        // Debug checking. the release version should not check this
        if (m_Transitions[index_from][index_to].Get() ==
               static_cast<TNCBIAtomicValue>(-1)) {
            ERR_POST("Disabled transition is counted. From " +
                     s_ValidStatusesNames[index_from] + " (index: " +
                     NStr::NumericToString(index_from) + ") To " +
                     s_ValidStatusesNames[index_to] + " (index: " +
                     NStr::NumericToString(index_to) + ")");
            m_Transitions[index_from][index_to].Add(1);
        }
        #endif
        m_Transitions[index_from][index_to].Add(1);     // Common case
    }
    else {
        if (to == CNetScheduleAPI::eFailed) {
            switch (path_option) {
                case eTimeout:
                    if (from == CNetScheduleAPI::eReading)
                        m_ToFailedDueToReadTimeoutCounter.Add(1);
                    else
                        m_ToFailedDueToTimeoutCounter.Add(1);
                    break;
                case eClear:
                    if (from == CNetScheduleAPI::eReading)
                        m_ToFailedDueToReadClearCounter.Add(1);
                    else
                        m_ToFailedDueToClearCounter.Add(1);
                    break;
                case eNewSession:
                    if (from == CNetScheduleAPI::eReading)
                        m_ToFailedDueToReadNewSessionCounter.Add(1);
                    else
                        m_ToFailedDueToNewSessionCounter.Add(1);
                    break;
                default:
                    break;
            }
            return;
        }

        if (to == CNetScheduleAPI::eReadFailed) {
            switch (path_option) {
                case eTimeout:
                    m_ToReadFailedDueToTimeoutCounter.Add(1);
                    break;
                case eClear:
                    m_ToReadFailedDueToClearCounter.Add(1);
                    break;
                case eNewSession:
                    m_ToReadFailedDueToNewSessionCounter.Add(1);
                    break;
                default:
                    break;
            }
            return;
        }

        if (from == CNetScheduleAPI::eRunning) {
            switch (path_option) {
                case eTimeout:
                    m_ToPendingDueToTimeoutCounter.Add(1);
                    break;
                case eFail:
                    m_ToPendingDueToFailCounter.Add(1);
                    break;
                case eClear:
                    m_ToPendingDueToClearCounter.Add(1);
                    break;
                case eNewSession:
                    m_ToPendingDueToNewSessionCounter.Add(1);
                    break;
                default:
                    break;
            }
            return;
        }

        // Here: this is a transition from the Reading state
        switch (path_option) {
                case eTimeout:
                    if (to == CNetScheduleAPI::eCanceled)
                        m_ToCanceledDueToReadTimeoutCounter.Add(1);
                    else
                        m_ToDoneDueToTimeoutCounter.Add(1);
                    break;
                case eFail:
                    m_ToDoneDueToFailCounter.Add(1);
                    break;
                case eClear:
                    if (to == CNetScheduleAPI::eCanceled)
                        m_ToCanceledDueToReadClearCounter.Add(1);
                    else
                        m_ToDoneDueToClearCounter.Add(1);
                    break;
                case eNewSession:
                    if (to == CNetScheduleAPI::eCanceled)
                        m_ToCanceledDueToReadNewSessionCounter.Add(1);
                    else
                        m_ToDoneDueToNewSessionCounter.Add(1);
                    break;
                default:
                    break;
        }
    }

    if (m_Scope == eQueueCounters)
        s_ServerWide.CountTransition(from, to, path_option);
}


void CStatisticsCounters::CountDBDeletion(size_t  count)
{
    m_DBDeleteCounter.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountDBDeletion(count);
}


void CStatisticsCounters::CountSubmit(size_t  count)
{
    m_SubmitCounter.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountSubmit(count);
}


void CStatisticsCounters::CountNSSubmitRollback(size_t  count)
{
    m_NSSubmitRollbackCounter.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountNSSubmitRollback(count);
}


void CStatisticsCounters::CountNSGetRollback(size_t  count)
{
    m_NSGetRollbackCounter.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountNSGetRollback(count);
}


void CStatisticsCounters::CountNSReadRollback(size_t  count)
{
    m_NSReadRollbackCounter.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountNSReadRollback(count);
}


void CStatisticsCounters::CountOutdatedPick(ECommandGroup  cmd_group)
{
    if (cmd_group == eGet)
        m_PickedAsPendingOutdated.Add(1);
    else
        m_PickedAsReadOutdated.Add(1);

    if (m_Scope == eQueueCounters)
        s_ServerWide.CountOutdatedPick(cmd_group);
}


void CStatisticsCounters::CountToPendingWithoutBlacklist(size_t  count)
{
    m_ToPendingWithoutBlacklist.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountToPendingWithoutBlacklist(count);
}


void CStatisticsCounters::CountToPendingRescheduled(size_t  count)
{
    m_ToPendingRescheduled.Add(count);
    if (m_Scope == eQueueCounters)
        s_ServerWide.CountToPendingRescheduled(count);
}


void  CStatisticsCounters::PrintServerWide(size_t  affinities)
{
    CStatisticsCounters server_wide_copy = s_ServerWide;

    // Do not print the server wide statistics the very first time
    CNSPreciseTime      current = CNSPreciseTime::Current();

    if (double(s_ServerWideLastPrintedTimestamp) == 0.0) {
        s_ServerWideLastPrinted = server_wide_copy;
        s_ServerWideLastPrintedTimestamp = current;
        return;
    }

    // Calculate the delta since the last time
    CNSPreciseTime      delta = current - s_ServerWideLastPrintedTimestamp;


    CRef<CRequestContext>   ctx;
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();


    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    CDiagContext_Extra      extra = diag_context.PrintRequestStart();

    // Prints the server wide counters
    extra.Print("_type", "statistics_thread")
         .Print("counters", "server_wide")
         .Print("time_interval", NS_FormatPreciseTimeAsSec(delta))
         .Print("affinities", affinities);
    server_wide_copy.PrintTransitions(extra);
    server_wide_copy.PrintDelta(extra, s_ServerWideLastPrinted);
    extra.Flush();

    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);

    s_ServerWideLastPrinted = server_wide_copy;
    s_ServerWideLastPrintedTimestamp = current;
}


string  CStatisticsCounters::x_GetTransitionCounterName(size_t  index_from,
                                                        size_t  index_to)
{
    return s_ValidStatusesNames[index_from] +
           "_" +
           s_ValidStatusesNames[index_to];
}


END_NCBI_SCOPE

