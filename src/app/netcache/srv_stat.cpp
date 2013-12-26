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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include "srv_stat.hpp"
#include "threads_man.hpp"
#include "memory_man.hpp"


BEGIN_NCBI_SCOPE;


extern Uint4 s_TotalSockets;
extern Uint2 s_SoftSocketLimit;
extern Uint2 s_HardSocketLimit;
extern SSrvThread** s_Threads;



static inline void
s_SaveState(SSrvStateStat& state)
{
    state.cnt_threads = GetCntRunningThreads();
    state.cnt_sockets = s_TotalSockets;
}


CSrvStat::CSrvStat(void)
    : m_MMStat(new SMMStat())
{
    x_ClearStats();
}

CSrvStat::~CSrvStat(void)
{}

void
CSrvStat::SetMMStat(SMMStat* stat)
{
    m_MMStat.reset(stat);
}

void
CSrvStat::x_ClearStats(void)
{
    m_JiffyTime.Initialize();
    m_MaxSlice = 0;
    m_ExecTime = 0;
    m_WaitTime = 0;
    m_DoneTasks.Initialize();
    m_ThrStarted = 0;
    m_ThrStopped = 0;
    m_SockActiveOpens = 0;
    m_SockPassiveOpens = 0;
    m_SockErrors = 0;
    m_SockOpenTime.Initialize();
    m_SocksByStatus.clear();
    m_CntThreads.Initialize();
    m_CntSockets.Initialize();
    m_MMStat->ClearStats();
}

void
CSrvStat::InitStartState(void)
{
    s_SaveState(m_StartState);
    m_EndState = m_StartState;
    m_MMStat->InitStartState();
}

void
CSrvStat::TransferEndState(CSrvStat* src_stat)
{
    m_EndState = m_StartState = src_stat->m_EndState;
    m_MMStat->TransferEndState(src_stat->m_MMStat.get());
}

void
CSrvStat::CopyStartState(CSrvStat* src_stat)
{
    m_StartState = src_stat->m_StartState;
    m_MMStat->CopyStartState(src_stat->m_MMStat.get());
}

void
CSrvStat::CopyEndState(CSrvStat* src_stat)
{
    m_EndState = src_stat->m_EndState;
    m_MMStat->CopyEndState(src_stat->m_MMStat.get());
}

void
CSrvStat::SaveEndState(void)
{
    s_SaveState(m_EndState);
    m_MMStat->SaveEndState();
}

void
CSrvStat::x_AddStats(CSrvStat* src_stat)
{
    m_JiffyTime.AddValues(src_stat->m_JiffyTime);
    if (src_stat->m_MaxSlice > m_MaxSlice)
        m_MaxSlice = src_stat->m_MaxSlice;
    m_ExecTime += src_stat->m_ExecTime;
    m_WaitTime += src_stat->m_WaitTime;
    m_DoneTasks.AddValues(src_stat->m_DoneTasks);
    m_ThrStarted += src_stat->m_ThrStarted;
    m_ThrStopped += src_stat->m_ThrStopped;
    m_SockActiveOpens += src_stat->m_SockActiveOpens;
    m_SockPassiveOpens += src_stat->m_SockPassiveOpens;
    m_SockErrors += src_stat->m_SockErrors;
    m_SockOpenTime.AddValues(src_stat->m_SockOpenTime);
    ITERATE(TStatusOpenTimes, it_status, src_stat->m_SocksByStatus) {
        TSrvTimeTerm& time_term = g_SrvTimeTerm(m_SocksByStatus, it_status->first);
        time_term.AddValues(it_status->second);
    }
    m_CntThreads.AddValues(src_stat->m_CntThreads);
    m_CntSockets.AddValues(src_stat->m_CntSockets);
    m_MMStat->AddStats(src_stat->m_MMStat.get());
}

void
CSrvStat::AddAllStats(CSrvStat* src_stat)
{
    x_AddStats(src_stat);
}

void
CSrvStat::CollectThreads(bool need_clear)
{
    for (TSrvThreadNum i = 0; i <= s_MaxRunningThreads + 1; ++i) {
        CSrvStat* src_stat = s_Threads[i]->stat;
        src_stat->m_StatLock.Lock();
        x_AddStats(src_stat);
        if (need_clear)
            src_stat->x_ClearStats();
        src_stat->m_StatLock.Unlock();
    }
}

void
CSrvStat::SaveEndStateStat(void)
{
    CSrvStat* thr_stat = GetCurThread()->stat;
    thr_stat->m_StatLock.Lock();
    thr_stat->m_CntThreads.AddValue(m_EndState.cnt_threads);
    thr_stat->m_CntSockets.AddValue(m_EndState.cnt_sockets);
    thr_stat->m_MMStat->SaveEndStateStat(m_MMStat.get());
    thr_stat->m_StatLock.Unlock();
}

void
CSrvStat::SchedJiffyStat(Uint8 jiffy_len, Uint8 max_slice,
                         Uint8 exec_time, Uint8 done_tasks, Uint8 wait_time)
{
    m_StatLock.Lock();
    m_JiffyTime.AddValue(jiffy_len);
    m_ExecTime += exec_time;
    m_WaitTime += wait_time;
    m_DoneTasks.AddValue(done_tasks);
    if (max_slice > m_MaxSlice)
        m_MaxSlice = max_slice;
    m_StatLock.Unlock();
}

void
CSrvStat::ThreadStarted(void)
{
    AtomicAdd(m_ThrStarted, 1);
}

void
CSrvStat::ThreadStopped(void)
{
    AtomicAdd(m_ThrStopped, 1);
}

void
CSrvStat::SockOpenActive(void)
{
    AtomicAdd(m_SockActiveOpens, 1);
}

void
CSrvStat::SockOpenPassive(void)
{
    AtomicAdd(m_SockPassiveOpens, 1);
}

void
CSrvStat::SockClose(int status, Uint8 open_time)
{
    m_StatLock.Lock();
    m_SockOpenTime.AddValue(open_time);
    TSrvTimeTerm& time_term = g_SrvTimeTerm(m_SocksByStatus, status);
    time_term.AddValue(open_time);
    m_StatLock.Unlock();
}

void
CSrvStat::ErrorOnSocket(void)
{
    AtomicAdd(m_SockErrors, 1);
}

void
CSrvStat::x_PrintUnstructured(CSrvPrintProxy& proxy)
{
    if (m_SockOpenTime.GetCount() != 0) {
        proxy << "Sockets by status:" << endl;
        ITERATE(TStatusOpenTimes, it_st, m_SocksByStatus) {
            const TSrvTimeTerm& time_term = it_st->second;
            proxy << it_st->first << ": "
                    << g_ToSmartStr(time_term.GetCount()) << " (cnt), "
                    << g_AsMSecStat(time_term.GetAverage()) << " (avg msec), "
                    << g_AsMSecStat(time_term.GetMaximum()) << " (max msec)" << endl;
        }
        proxy << endl;
    }
}

void
CSrvStat::PrintToLogs(CRequestContext* ctx, CSrvPrintProxy& proxy)
{
    CSrvDiagMsg diag;
    diag.PrintExtra(ctx);
    diag.PrintParam("start_threads", m_StartState.cnt_threads)
        .PrintParam("end_threads", m_EndState.cnt_threads)
        .PrintParam("avg_threads", m_CntThreads.GetAverage())
        .PrintParam("max_threads", m_CntThreads.GetMaximum())
        .PrintParam("thr_started", m_ThrStarted)
        .PrintParam("thr_stopped", m_ThrStopped);
    diag.PrintParam("start_socks", m_StartState.cnt_sockets)
        .PrintParam("end_socks", m_EndState.cnt_sockets)
        .PrintParam("avg_socks", m_CntSockets.GetAverage())
        .PrintParam("max_socks", m_CntSockets.GetMaximum())
        .PrintParam("socks_active", m_SockActiveOpens)
        .PrintParam("socks_passive", m_SockPassiveOpens)
        .PrintParam("socks_close", m_SockOpenTime.GetCount())
        .PrintParam("avg_sock_open", m_SockOpenTime.GetAverage())
        .PrintParam("max_sock_open", m_SockOpenTime.GetMaximum())
        .PrintParam("sock_errors", m_SockErrors);
    diag.PrintParam("cnt_jiffies", m_JiffyTime.GetCount())
        .PrintParam("avg_jiffy_time", g_AsMSecStat(m_JiffyTime.GetAverage()))
        .PrintParam("max_jiffy_time", g_AsMSecStat(m_JiffyTime.GetMaximum()))
        .PrintParam("avg_busy_pct", g_CalcStatPct(m_ExecTime, m_JiffyTime.GetSum()))
        .PrintParam("avg_idle_pct", g_CalcStatPct(m_WaitTime, m_JiffyTime.GetSum()))
        .PrintParam("cnt_tasks", m_DoneTasks.GetSum())
        .PrintParam("avg_jiffy_tasks", m_DoneTasks.GetDoubleAvg())
        .PrintParam("max_jiffy_tasks", m_DoneTasks.GetMaximum())
        .PrintParam("avg_idle_slice", double(m_WaitTime) / m_DoneTasks.GetSum() / kUSecsPerMSec)
        .PrintParam("avg_task_slice", double(m_ExecTime) / m_DoneTasks.GetSum() / kUSecsPerMSec)
        .PrintParam("max_task_slice", g_AsMSecStat(m_MaxSlice));
    diag.Flush();

    m_MMStat->PrintToLogs(ctx, proxy);

    x_PrintUnstructured(proxy);
}

void CSrvStat::PrintState(CSrvSocketTask& task)
{
    string is("\": "), eol(",\n\"");
    task.WriteText(eol).WriteText("cnt_threads").WriteText(is).WriteNumber(m_EndState.cnt_threads);
    task.WriteText(eol).WriteText("cnt_sockets").WriteText(is).WriteNumber(m_EndState.cnt_sockets);
}

void
CSrvStat::PrintToSocket(CSrvPrintProxy& proxy)
{
    proxy << "Srv config - "
                    << s_MaxRunningThreads << " (max threads), "
                    << s_SoftSocketLimit << " (soft sock limit), "
                    << s_HardSocketLimit << " (hard sock limit)" << endl;
    proxy << "Threads - "
                    << m_StartState.cnt_threads << " to "
                    << m_EndState.cnt_threads << " (avg "
                    << m_CntThreads.GetAverage() << ", max "
                    << m_CntThreads.GetMaximum() << "), avg busy "
                    << g_CalcStatPct(m_ExecTime, m_JiffyTime.GetSum()) << "%, avg idle "
                    << g_CalcStatPct(m_WaitTime, m_JiffyTime.GetSum()) << "%" << endl;
    proxy << "Sockets - "
                    << m_StartState.cnt_sockets << " to "
                    << m_EndState.cnt_sockets << " (avg "
                    << m_CntSockets.GetAverage() << ", max "
                    << m_CntSockets.GetMaximum() << ")" << endl;
    proxy << "Threads flow - "
                    << m_ThrStarted << " (start), "
                    << m_ThrStopped << " (stop)" << endl;
    proxy << "Sockets flow - "
                    << g_ToSmartStr(m_SockActiveOpens) << " (active open), "
                    << g_ToSmartStr(m_SockPassiveOpens) << " (passive open), "
                    << m_SockOpenTime.GetCount() << " (closed)" << endl;
    proxy << "Jiffies - "
                    << g_AsMSecStat(m_JiffyTime.GetAverage()) << " (avg msec), "
                    << g_AsMSecStat(m_JiffyTime.GetMaximum()) << " (max msec), "
                    << m_DoneTasks.GetDoubleAvg() << " (avg tasks), "
                    << m_DoneTasks.GetMaximum() << " (max tasks)" << endl;
    proxy << "Tasks - "
                    << g_ToSmartStr(m_DoneTasks.GetSum()) << " (cnt), "
                    << double(m_ExecTime) / m_DoneTasks.GetSum() / kUSecsPerMSec << " (avg slice), "
                    << g_AsMSecStat(m_MaxSlice) << " (max slice), "
                    << double(m_WaitTime) / m_DoneTasks.GetSum() / kUSecsPerMSec << " (avg wait)" << endl;

    m_MMStat->PrintToSocket(proxy);

    x_PrintUnstructured(proxy);
}

END_NCBI_SCOPE;
