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


BEGIN_NCBI_SCOPE


// Only valid statuses as strings
static bool     s_Initialized = false;
static string   s_ValidStatusesNames[g_ValidJobStatusesSize];
static size_t   s_StatusToIndex[CNetScheduleAPI::eLastStatus];

// Total counters
static CAtomicCounter_WithAutoInit  s_TransitionsTotal[g_ValidJobStatusesSize]
                                                      [g_ValidJobStatusesSize];
static CAtomicCounter_WithAutoInit  s_SubmitCounterTotal;
static CAtomicCounter_WithAutoInit  s_NSSubmitRollbackCounterTotal;
static CAtomicCounter_WithAutoInit  s_NSGetRollbackCounterTotal;
static CAtomicCounter_WithAutoInit  s_NSReadRollbackCounterTotal;
static CAtomicCounter_WithAutoInit  s_DBDeleteCounterTotal;
static CAtomicCounter_WithAutoInit  s_PickedAsOutdatedTotal;

// From running
static CAtomicCounter_WithAutoInit  s_ToPendingDueToTimeoutCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToPendingDueToFailCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToPendingDueToClearCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToPendingDueToNewSessionCounterTotal;

// From reading
static CAtomicCounter_WithAutoInit  s_ToDoneDueToTimeoutCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToDoneDueToFailCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToDoneDueToClearCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToDoneDueToNewSessionCounterTotal;

static CAtomicCounter_WithAutoInit  s_ToFailedDueToTimeoutCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToFailedDueToClearCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToFailedDueToNewSessionCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToReadFailedDueToTimeoutCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToReadFailedDueToClearCounterTotal;
static CAtomicCounter_WithAutoInit  s_ToReadFailedDueToNewSessionCounterTotal;


CStatisticsCounters::CStatisticsCounters()
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

        // Mark invalid transitions with the counter value -1.
        // There are less valid transitions than invalid, so first mark all as
        // invalid and then specifically initialize valid to 0.
        for (size_t  index_from = 0;
             index_from < g_ValidJobStatusesSize; ++index_from) {
            for (size_t  index_to = 0;
                 index_to < g_ValidJobStatusesSize; ++index_to) {
                s_TransitionsTotal[index_from][index_to].Set(
                                            static_cast<TNCBIAtomicValue>(-1));
            }
        }

        // Set to 0 those counters which are allowed
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::ePending]]
                          [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::ePending]]
                          [s_StatusToIndex[CNetScheduleAPI::eRunning]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                          [s_StatusToIndex[CNetScheduleAPI::ePending]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                          [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                          [s_StatusToIndex[CNetScheduleAPI::eFailed]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eDone]]
                          [s_StatusToIndex[CNetScheduleAPI::eReading]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eDone]]
                          [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eDone]]
                          [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReading]]
                          [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReading]]
                          [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReading]]
                          [s_StatusToIndex[CNetScheduleAPI::eReadFailed]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eConfirmed]]
                          [s_StatusToIndex[CNetScheduleAPI::eConfirmed]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReadFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eReadFailed]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eFailed]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eReading]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eDone]].Set(0);

        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::ePending]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eRunning]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eCanceled]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eDone]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReading]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eConfirmed]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
        s_TransitionsTotal[s_StatusToIndex[CNetScheduleAPI::eReadFailed]]
                          [s_StatusToIndex[CNetScheduleAPI::eCanceled]].Set(0);
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
    return;
}


void CStatisticsCounters::PrintTransitions(CDiagContext_Extra &  extra) const
{
    extra.Print("submits", m_SubmitCounter.Get())
         .Print("ns_submit_rollbacks", m_NSSubmitRollbackCounter.Get())
         .Print("ns_get_rollbacks", m_NSGetRollbackCounter.Get())
         .Print("ns_read_rollbacks", m_NSReadRollbackCounter.Get())
         .Print("picked_as_outdated", m_PickedAsOutdated.Get())
         .Print("dbdeletions", m_DBDeleteCounter.Get());

    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {

            // All invalid transitions are marked as -1
            if (m_Transitions[index_from][index_to].Get() !=
                    static_cast<TNCBIAtomicValue>(-1))
                extra.Print(x_GetTransitionCounterName(index_from, index_to),
                            m_Transitions[index_from][index_to].Get());
        }
    }
    extra.Print("Running_Pending_timeout",
                m_ToPendingDueToTimeoutCounter.Get())
         .Print("Running_Pending_fail",
                m_ToPendingDueToFailCounter.Get())
         .Print("Running_Pending_clear",
                m_ToPendingDueToClearCounter.Get())
         .Print("Running_Failed_clear",
                m_ToFailedDueToClearCounter.Get())
         .Print("Running_Pending_new_session",
                m_ToPendingDueToNewSessionCounter.Get())
         .Print("Running_Failed_new_session",
                m_ToFailedDueToNewSessionCounter.Get())
         .Print("Running_Failed_timeout",
                m_ToFailedDueToTimeoutCounter.Get())
         .Print("Reading_Done_timeout",
                m_ToDoneDueToTimeoutCounter.Get())
         .Print("Reading_Done_fail",
                m_ToDoneDueToFailCounter.Get())
         .Print("Reading_Done_clear",
                m_ToDoneDueToClearCounter.Get())
         .Print("Reading_ReadFailed_clear",
                m_ToReadFailedDueToClearCounter.Get())
         .Print("Reading_Done_new_session",
                m_ToDoneDueToNewSessionCounter.Get())
         .Print("Reading_ReadFailed_new_session",
                m_ToReadFailedDueToNewSessionCounter.Get())
         .Print("Reading_ReadFailed_timeout",
                m_ToReadFailedDueToTimeoutCounter.Get());
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
                    "OK:picked_as_outdated: " +
                    NStr::NumericToString(m_PickedAsOutdated.Get()) + "\n"
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
           NStr::NumericToString(m_ToReadFailedDueToTimeoutCounter.Get()) + "\n";
}


void CStatisticsCounters::CountTransition(CNetScheduleAPI::EJobStatus  from,
                                          CNetScheduleAPI::EJobStatus  to,
                                          ETransitionPathOption        path_option)
{
    size_t      index_from = s_StatusToIndex[from];
    size_t      index_to = s_StatusToIndex[to];

    if (path_option == eNone) {
        // Debug checking. the release version should not check this
        if (m_Transitions[index_from][index_to].Get() ==
               static_cast<TNCBIAtomicValue>(-1)) {
            ERR_POST("Disabled transition is counted. From index: " +
                     NStr::NumericToString(index_from) + " To index: " +
                     NStr::NumericToString(index_to));
            m_Transitions[index_from][index_to].Add(1);
        }
        m_Transitions[index_from][index_to].Add(1);     // Common case
        s_TransitionsTotal[index_from][index_to].Add(1);
    }
    else {
        if (to == CNetScheduleAPI::eFailed) {
            switch (path_option) {
                case eTimeout:
                    m_ToFailedDueToTimeoutCounter.Add(1);
                    s_ToFailedDueToTimeoutCounterTotal.Add(1);
                    break;
                case eClear:
                    m_ToFailedDueToClearCounter.Add(1);
                    s_ToFailedDueToClearCounterTotal.Add(1);
                    break;
                case eNewSession:
                    m_ToFailedDueToNewSessionCounter.Add(1);
                    s_ToFailedDueToNewSessionCounterTotal.Add(1);
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
                    s_ToReadFailedDueToTimeoutCounterTotal.Add(1);
                    break;
                case eClear:
                    m_ToReadFailedDueToClearCounter.Add(1);
                    s_ToReadFailedDueToClearCounterTotal.Add(1);
                    break;
                case eNewSession:
                    m_ToReadFailedDueToNewSessionCounter.Add(1);
                    s_ToReadFailedDueToNewSessionCounterTotal.Add(1);
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
                    s_ToPendingDueToTimeoutCounterTotal.Add(1);
                    break;
                case eFail:
                    m_ToPendingDueToFailCounter.Add(1);
                    s_ToPendingDueToFailCounterTotal.Add(1);
                    break;
                case eClear:
                    m_ToPendingDueToClearCounter.Add(1);
                    s_ToPendingDueToClearCounterTotal.Add(1);
                    break;
                case eNewSession:
                    m_ToPendingDueToNewSessionCounter.Add(1);
                    s_ToPendingDueToNewSessionCounterTotal.Add(1);
                    break;
                default:
                    break;
            }
            return;
        }

        // Here: this is a transition from the Reading state
        switch (path_option) {
                case eTimeout:
                    m_ToDoneDueToTimeoutCounter.Add(1);
                    s_ToDoneDueToTimeoutCounterTotal.Add(1);
                    break;
                case eFail:
                    m_ToDoneDueToFailCounter.Add(1);
                    s_ToDoneDueToFailCounterTotal.Add(1);
                    break;
                case eClear:
                    m_ToDoneDueToClearCounter.Add(1);
                    s_ToDoneDueToClearCounterTotal.Add(1);
                    break;
                case eNewSession:
                    m_ToDoneDueToNewSessionCounter.Add(1);
                    s_ToDoneDueToNewSessionCounterTotal.Add(1);
                    break;
                default:
                    break;
        }
    }
}


void CStatisticsCounters::CountDBDeletion(size_t  count)
{
    m_DBDeleteCounter.Add(count);
    s_DBDeleteCounterTotal.Add(count);
}


void CStatisticsCounters::CountSubmit(size_t  count)
{
    m_SubmitCounter.Add(count);
    s_SubmitCounterTotal.Add(count);
}


void CStatisticsCounters::CountNSSubmitRollback(size_t  count)
{
    m_NSSubmitRollbackCounter.Add(count);
    s_NSSubmitRollbackCounterTotal.Add(count);
}


void CStatisticsCounters::CountNSGetRollback(size_t  count)
{
    m_NSGetRollbackCounter.Add(count);
    s_NSGetRollbackCounterTotal.Add(count);
}


void CStatisticsCounters::CountNSReadRollback(size_t  count)
{
    m_NSReadRollbackCounter.Add(count);
    s_NSReadRollbackCounterTotal.Add(count);
}


void CStatisticsCounters::CountOutdatedPick(void)
{
    m_PickedAsOutdated.Add(1);
    s_PickedAsOutdatedTotal.Add(1);
}


void  CStatisticsCounters::PrintTotal(size_t  affinities)
{
    CRef<CRequestContext>   ctx;
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();

    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    CDiagContext_Extra      extra = diag_context.PrintRequestStart();

    // Prints the total counters
    extra.Print("_type", "statistics_thread")
         .Print("counters", "total")
         .Print("submits", s_SubmitCounterTotal.Get())
         .Print("ns_submit_rollbacks", s_NSSubmitRollbackCounterTotal.Get())
         .Print("picked_as_outdated", s_PickedAsOutdatedTotal.Get())
         .Print("dbdeletions", s_DBDeleteCounterTotal.Get())
         .Print("affinities", affinities);

    for (size_t  index_from = 0;
         index_from < g_ValidJobStatusesSize; ++index_from) {
        for (size_t  index_to = 0;
             index_to < g_ValidJobStatusesSize; ++index_to) {

            // All invalid transitions are marked as -1
            if (s_TransitionsTotal[index_from][index_to].Get() !=
                   static_cast<TNCBIAtomicValue>(-1))
                extra.Print(x_GetTransitionCounterName(index_from, index_to),
                            s_TransitionsTotal[index_from][index_to].Get());
        }
    }
    extra.Print("Running_Pending_timeout",
                s_ToPendingDueToTimeoutCounterTotal.Get())
         .Print("Running_Pending_fail",
                s_ToPendingDueToFailCounterTotal.Get())
         .Print("Running_Pending_clear",
                s_ToPendingDueToClearCounterTotal.Get())
         .Print("Running_Failed_clear",
                s_ToFailedDueToClearCounterTotal.Get())
         .Print("Running_Pending_new_session",
                s_ToPendingDueToNewSessionCounterTotal.Get())
         .Print("Running_Failed_new_session",
                s_ToFailedDueToNewSessionCounterTotal.Get())
         .Print("Running_Failed_timeout",
                s_ToFailedDueToTimeoutCounterTotal.Get())
         .Print("Reading_Done_timeout",
                s_ToDoneDueToTimeoutCounterTotal.Get())
         .Print("Reading_Done_fail",
                s_ToDoneDueToFailCounterTotal.Get())
         .Print("Reading_Done_clear",
                s_ToDoneDueToClearCounterTotal.Get())
         .Print("Reading_ReadFailed_clear",
                s_ToReadFailedDueToClearCounterTotal.Get())
         .Print("Reading_Done_new_session",
                s_ToDoneDueToNewSessionCounterTotal.Get())
         .Print("Reading_ReadFailed_new_session",
                s_ToReadFailedDueToNewSessionCounterTotal.Get())
         .Print("Reading_ReadFailed_timeout",
                s_ToReadFailedDueToTimeoutCounterTotal.Get());
    extra.Flush();

    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);
}


string  CStatisticsCounters::x_GetTransitionCounterName(size_t  index_from,
                                                        size_t  index_to)
{
    return s_ValidStatusesNames[index_from] +
           "_" +
           s_ValidStatusesNames[index_to];
}


END_NCBI_SCOPE

