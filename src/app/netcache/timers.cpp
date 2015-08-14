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

#include "timers.hpp"
#include "scheduler.hpp"


namespace intr = boost::intrusive;

BEGIN_NCBI_SCOPE;


static const Uint1  kTimerLowBits = 8;
static const Uint1  kTimerMidBits = 5;
static const Uint1  kTimerMidLevels = 2;
static const int    kTimerLowMask = (1 << kTimerLowBits) - 1;
static const int    kTimerMidMask = (1 << kTimerMidBits) - 1;


struct STimerList_tag;
typedef intr::list_base_hook<intr::tag<STimerList_tag>,
                             intr::link_mode<intr::auto_unlink> >   TTimerListHook;

struct STimerTicket : public TTimerListHook
{
    int timer_time;
    CSrvTask* task;

    STimerTicket(void)
        : timer_time(0), task(0)
    {
    }
};

typedef intr::list<STimerTicket,
                   intr::base_hook<TTimerListHook>,
                   intr::constant_time_size<false> >    TTimerList;

static CMiniMutex s_TimerLock;
static int s_LastFiredTime = 0;
static TTimerList s_TimerLowQ[kTimerLowMask + 1];
static TTimerList s_TimerMidQ[kTimerMidLevels][kTimerMidMask + 1];



static void
s_AddTimerTicket(STimerTicket* ticket)
{
    int cur_time = CSrvTime::CurSecs();
    int ticket_time = ticket->timer_time;
    if (ticket_time <= cur_time + kTimerLowMask) {
        if (ticket_time <= cur_time)
            ticket_time = ticket->timer_time = cur_time + 1;
        s_TimerLowQ[ticket_time & kTimerLowMask].push_back(*ticket);
    }
    else {
        ticket_time >>= kTimerLowBits;
        cur_time >>= kTimerLowBits;
        for (Uint1 i = 0; i < kTimerMidLevels; ++i)
        {
            if (ticket_time < cur_time + kTimerMidMask
                ||  i == kTimerMidLevels - 1)
            {
                s_TimerMidQ[i][ticket_time & kTimerMidMask].push_back(*ticket);
                break;
            }
            ticket_time >>= kTimerMidBits;
            cur_time >>= kTimerMidBits;
        }
    }
}

static void
s_ShiftTimers(int fire_time)
{
    fire_time >>= kTimerLowBits;
    for (Uint1 i = 0; i < kTimerMidLevels; ++i, fire_time >>= kTimerMidBits) {
        TTimerList tlist;
        tlist.swap(s_TimerMidQ[i][fire_time & kTimerMidMask]);
        while (!tlist.empty()) {
            STimerTicket* ticket = &tlist.front();
            tlist.pop_front();
            s_AddTimerTicket(ticket);
        }
    }
}

static void
s_ExecuteTimerTicket(STimerTicket* ticket)
{
    CSrvTask* task = ticket->task;
    task->m_Timer = NULL;

retry_set_flags:
    TSrvTaskFlags old_flags = ACCESS_ONCE(task->m_TaskFlags);
    _ASSERT(old_flags & fTaskOnTimer);
    TSrvTaskFlags new_flags = old_flags - fTaskOnTimer;
    if (!AtomicCAS(task->m_TaskFlags, old_flags, new_flags))
        goto retry_set_flags;

    task->SetRunnable();
    delete ticket;
}

static void
s_FireTimers(int fire_time)
{
    TTimerList& tlist = s_TimerLowQ[fire_time & kTimerLowMask];
    while (!tlist.empty()) {
        STimerTicket* ticket = &tlist.front();
        if (ticket->timer_time != fire_time) {
            SRV_LOG(Critical, "Timers broken");
        }
        tlist.pop_front();
        s_ExecuteTimerTicket(ticket);
    }
}

void
InitTimers(void)
{
    s_LastFiredTime = CSrvTime::CurSecs();
}

void
FireAllTimers(void)
{
    s_TimerLock.Lock();
    int cur_time = CSrvTime::CurSecs();
    for (int fire_time = cur_time; fire_time <= cur_time + kTimerLowMask; ++fire_time)
    {
        s_FireTimers(fire_time);
    }
    for (Uint1 i = 0; i < kTimerMidLevels; ++i) {
        for (Uint1 j = 0; j <= kTimerMidMask; ++j) {
            TTimerList& tlist = s_TimerMidQ[i][j];
            while (!tlist.empty()) {
                STimerTicket* ticket = &tlist.front();
                tlist.pop_front();
                s_ExecuteTimerTicket(ticket);
            }
        }
    }
    s_TimerLock.Unlock();
}

void
TimerTick(void)
{
    s_TimerLock.Lock();
    int cur_time = CSrvTime::CurSecs();
    for (int fire_time = s_LastFiredTime + 1; fire_time <= cur_time; ++fire_time)
    {
        if ((fire_time & kTimerLowMask) == 0)
            s_ShiftTimers(fire_time);
        s_FireTimers(fire_time);
    }
    s_LastFiredTime = cur_time;
    s_TimerLock.Unlock();
};

void
RemoveTaskFromTimer(CSrvTask* task, TSrvTaskFlags new_flags)
{
    if (!(new_flags & fTaskOnTimer))
        return;

    s_TimerLock.Lock();
retry:
    TSrvTaskFlags old_flags = ACCESS_ONCE(task->m_TaskFlags);
    if (old_flags & fTaskOnTimer) {
        new_flags = old_flags - fTaskOnTimer;
        if (!AtomicCAS(task->m_TaskFlags, old_flags, new_flags))
            goto retry;
        if (task->m_Timer) {
            task->m_Timer->unlink();
            delete task->m_Timer;
        }
        task->m_Timer = NULL;
    }
    s_TimerLock.Unlock();
}



void
CSrvTask::RunAfter(Uint4 delay_sec)
{
    STimerTicket* ticket = new STimerTicket();
    ticket->task = this;
    s_TimerLock.Lock();
    ticket->timer_time = CSrvTime::CurSecs() + delay_sec;
    if (CTaskServer::IsInShutdown()) {
        s_TimerLock.Unlock();
        delete ticket;
        SetRunnable();
    }
    else {
retry:
        TSrvTaskFlags old_flags = ACCESS_ONCE(m_TaskFlags);
        if ((old_flags & fTaskOnTimer)) {
            SRV_LOG(Critical, "Invalid task flags: " << old_flags);
            RemoveTaskFromTimer(this, old_flags - fTaskOnTimer);
        }
        if (!(old_flags & (fTaskQueued + fTaskRunnable))) {
            TSrvTaskFlags new_flags = old_flags | fTaskOnTimer;
            if (!AtomicCAS(m_TaskFlags, old_flags, new_flags))
                goto retry;
            s_AddTimerTicket(ticket);
            m_Timer = ticket;
            s_TimerLock.Unlock();
        }
        else {
            s_TimerLock.Unlock();
            delete ticket;
        }
    }
}

END_NCBI_SCOPE;
