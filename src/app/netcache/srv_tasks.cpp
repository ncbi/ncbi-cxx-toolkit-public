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


BEGIN_NCBI_SCOPE;


CSrvTask::CSrvTask(void)
    : m_LastThread(0),
      m_TaskFlags(0),
      m_Priority(1),
      m_DiagCtx(NULL),
      m_DiagChain(NULL),
      m_Timer(NULL)
{}

CSrvTask::~CSrvTask(void)
{
    _ASSERT(!(m_TaskFlags & fTaskOnTimer)  &&  !m_Timer);
    _ASSERT(!m_DiagCtx);
    free(m_DiagChain);
}

void
CSrvTask::InternalRunSlice(TSrvThreadNum thr_num)
{
    ExecuteSlice(thr_num);
}


CSrvTransitionTask::CSrvTransitionTask(void)
    : m_TransState(eState_Initial)
{}

CSrvTransitionTask::~CSrvTransitionTask(void)
{
    if (m_TransState == eState_Transition  ||  !m_Consumers.empty())
        abort();
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
        abort();
    }
}

void
CSrvTransitionTask::FinishTransition(void)
{
    TSrvConsList cons_list;

    m_TransLock.Lock();
    if (m_TransState != eState_Transition)
        abort();
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
{}

CSrvTransConsumer::~CSrvTransConsumer(void)
{}

END_NCBI_SCOPE;
