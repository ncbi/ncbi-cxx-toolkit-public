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

#include "scheduler.hpp"
#include "srv_stat.hpp"

BEGIN_NCBI_SCOPE;

#if __NC_TASKS_MONITOR
static CMiniMutex     s_all_tasks_lock;
#if __NC_TASKS_INTR_SET
struct SrvTaskCompare
{
    bool operator() (const CSrvTask& x, const CSrvTask& y) const
    {
        return &x < &y;
    }
};
#if 0
typedef intr::set< CSrvTask, intr::compare< SrvTaskCompare > > TAllTasksSet;
#else
typedef intr::member_hook<CSrvTask, intr::set_member_hook<>, &CSrvTask::m_intr_member_hook> TAllTasksSetOption;
typedef intr::set< CSrvTask, TAllTasksSetOption, intr::compare< SrvTaskCompare > >   TAllTasksSet;
#endif
#else
typedef std::set<const CSrvTask*>  TAllTasksSet;
#endif

TAllTasksSet  s_all_tasks;
#endif

CSrvTask::CSrvTask(void)
    : m_LastThread(0),
      m_TaskFlags(0),
      m_LastActive(0),
      m_Priority(1),
      m_DiagCtx(NULL),
      m_DiagChain(NULL),
      m_Timer(NULL)
{
#if __NC_TASKS_MONITOR
    s_all_tasks_lock.Lock();
#if __NC_TASKS_INTR_SET
    s_all_tasks.insert(*this);
#else
    s_all_tasks.insert(this);
#endif
    s_all_tasks_lock.Unlock();
    m_TaskName = "CSrvTask";
#endif
}

CSrvTask::~CSrvTask(void)
{
    _ASSERT(!(m_TaskFlags & fTaskOnTimer)  &&  !m_Timer);
    _ASSERT(!m_DiagCtx);
    free(m_DiagChain);
#if __NC_TASKS_MONITOR
    s_all_tasks_lock.Lock();
#if __NC_TASKS_INTR_SET
    s_all_tasks.erase(*this);
#else
    s_all_tasks.erase(this);
#endif
    s_all_tasks_lock.Unlock();
#endif
}

void
CSrvTask::InternalRunSlice(TSrvThreadNum thr_num)
{
    m_LastActive = CSrvTime::CurSecs();
    ExecuteSlice(thr_num);
    m_LastActive = CSrvTime::CurSecs();
}

#if __NC_TASKS_MONITOR
void CSrvTask::PrintState(CSrvSocketTask& task)
{
    string is("\": "), eol(",\n\"");
    map<string, int> idle_tasks;
    map<string, CSrvStatTerm<int>> busy_tasks;
    size_t len = 0;

    s_all_tasks_lock.Lock();
    int now = CSrvTime::CurSecs();
    ITERATE(TAllTasksSet, t,  s_all_tasks) {
#if __NC_TASKS_INTR_SET
        const CSrvTask* job = &*t;
#else
        const CSrvTask* job = *t;
#endif
        if (busy_tasks.find(job->m_TaskName) == busy_tasks.end()) {
            busy_tasks[job->m_TaskName].Initialize();
        }
        if (idle_tasks.find(job->m_TaskName) == idle_tasks.end()) {
            idle_tasks[job->m_TaskName] = 0;
        }
        if (job->m_LastActive != 0) {
            busy_tasks[job->m_TaskName].AddValue(max(0, now - job->m_LastActive));
        } else {
            ++idle_tasks[job->m_TaskName];
        }
        len = max(len, job->m_TaskName.size());
    }
    s_all_tasks_lock.Unlock();

    task.WriteText(eol).WriteText("cnt_tasks").WriteText(is).WriteNumber(s_all_tasks.size());
    for (map<string, CSrvStatTerm<int>>::const_iterator t = busy_tasks.begin(); t != busy_tasks.end(); ++t) {
        task.WriteText(eol).WriteText(t->first).WriteText(is).WriteText(string( len - t->first.size(), ' ')).WriteText("{ \"");
        task.WriteText("cnt").WriteText(is).WriteNumber(t->second.GetCount());
        task.WriteText(", \"").WriteText("avg").WriteText(is).WriteNumber(t->second.GetAverage());
        task.WriteText(", \"").WriteText("max").WriteText(is).WriteNumber(t->second.GetMaximum());
        task.WriteText(", \"").WriteText("idle").WriteText(is).WriteNumber(idle_tasks.at(t->first));
        task.WriteText("}");
    }
}
#endif

CSrvTransitionTask::CSrvTransitionTask(void)
    : m_TransState(eState_Initial)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CSrvTransitionTask";
#endif
}

CSrvTransitionTask::~CSrvTransitionTask(void)
{
    if (m_TransState == eState_Transition  ||  !m_Consumers.empty()) {
        SRV_FATAL("CSrvTransitionTask destructor unexpected: m_TransState: "
                  << m_TransState << ", Consumers: " << m_Consumers.size());
    }
}

void
CSrvTransitionTask::RequestTransition(CSrvTransConsumer* consumer)
{
    m_TransLock.Lock();
    switch (m_TransState) {
    case eState_Initial:
        m_TransState = eState_Transition;
        consumer->m_TransFinished = false;
        m_Consumers.push_front(*consumer);
        m_TransLock.Unlock();
        SetRunnable();
        break;
    case eState_Transition:
        consumer->m_TransFinished = false;
        m_Consumers.push_front(*consumer);
        m_TransLock.Unlock();
        break;
    case eState_Final:
        m_TransLock.Unlock();
        consumer->m_TransFinished = true;
        consumer->SetRunnable();
        break;
    default:
        SRV_FATAL("Unsupported transition state: " << m_TransState);
    }
}

void
CSrvTransitionTask::FinishTransition(void)
{
    TSrvConsList cons_list;

    m_TransLock.Lock();
    if (m_TransState != eState_Transition) {
        SRV_FATAL("Unexpected transition state: " << m_TransState);
    }
    m_TransState = eState_Final;
    cons_list.swap(m_Consumers);
    m_TransLock.Unlock();

    while (!cons_list.empty()) {
        CSrvTransConsumer* consumer = &cons_list.front();
        cons_list.pop_front();
        consumer->m_TransFinished = true;
        consumer->SetRunnable();
    }
}

void
CSrvTransitionTask::CancelTransRequest(CSrvTransConsumer* consumer)
{
    m_TransLock.Lock();
    if (m_TransState != eState_Final)
        m_Consumers.erase(m_Consumers.iterator_to(*consumer));
    m_TransLock.Unlock();
}


CSrvTransConsumer::CSrvTransConsumer(void)
    : m_TransFinished(false)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CSrvTransConsumer";
#endif
}

CSrvTransConsumer::~CSrvTransConsumer(void)
{}

END_NCBI_SCOPE;
