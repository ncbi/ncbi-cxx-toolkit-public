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

#include "rcu.hpp"
#include "threads_man.hpp"


BEGIN_NCBI_SCOPE;

                

struct SRCUInfo
{
    TSrvRCUList calls;
    CSrvRCUUser* gp_marker_cur;
    CSrvRCUUser* gp_marker_next;
    Uint1 seen_gp;
};


class CFakeRCUUser : public CSrvRCUUser
{
public:
    CFakeRCUUser(void);
    virtual ~CFakeRCUUser(void);

    virtual void ExecuteRCU(void);
};

static CMiniMutex s_RCULock;
static Uint1 s_FinishedGP = 0;
static Uint1 s_CurrentGP = 0;
static TSrvThreadNum s_ThreadsPassedQS = 0;
static TSrvThreadNum s_ThreadsEnteredGP = 0;
static TSrvThreadNum s_ActiveThreads = 0;




static void
s_RCUExecuteCalls(SRCUInfo* rcu)
{
    for (;;) {
        CSrvRCUUser* user = &rcu->calls.front();
        if (user == rcu->gp_marker_cur)
            return;
        rcu->calls.pop_front();
        user->ExecuteRCU();
    }
}

static inline void
s_RCUMoveGPMarkers(SRCUInfo* rcu)
{
    if (&rcu->calls.front() != rcu->gp_marker_cur)
        abort();
    rcu->calls.pop_front();
    rcu->calls.push_back(*rcu->gp_marker_cur);
    swap(rcu->gp_marker_cur, rcu->gp_marker_next);
}

static inline void
s_RCUNoteStartedGP(SRCUInfo* rcu)
{
    rcu->seen_gp = s_CurrentGP;
    if (++s_ThreadsPassedQS == s_ThreadsEnteredGP)
        s_FinishedGP = s_CurrentGP;
}

static void
s_RCUNoteGPorStartNew(SRCUInfo* rcu)
{
    s_RCULock.Lock();
    if (rcu->seen_gp != s_CurrentGP) {
        s_RCUNoteStartedGP(rcu);
    }
    else {
        rcu->seen_gp = ++s_CurrentGP;
        s_ThreadsEnteredGP = s_ActiveThreads;
        s_ThreadsPassedQS = 1;
    }
    s_RCULock.Unlock();

    s_RCUMoveGPMarkers(rcu);
}

void
RCUPassQS(SRCUInfo* rcu)
{
    s_RCUExecuteCalls(rcu);
    while (rcu->seen_gp != s_CurrentGP
           ||  (s_FinishedGP == s_CurrentGP  &&  rcu->calls.size() != 2))
    {
        s_RCUNoteGPorStartNew(rcu);
        // We can be last in noted GP or instead of starting new GP we could
        // note concurrently started one and we can be last in that GP.
        // Either way some calls might become executable.
        s_RCUExecuteCalls(rcu);
    }
}

bool
RCUHasCalls(SRCUInfo* rcu)
{
    return rcu->calls.size() != 2;
}

void
RCUInitNewThread(SSrvThread* thr)
{
    SRCUInfo* rcu = new SRCUInfo();
    thr->rcu = rcu;

    rcu->gp_marker_cur = new CFakeRCUUser();
    rcu->calls.push_back(*rcu->gp_marker_cur);
    rcu->gp_marker_next = new CFakeRCUUser();
    rcu->calls.push_back(*rcu->gp_marker_next);

    s_RCULock.Lock();
    ++s_ActiveThreads;
    rcu->seen_gp = s_CurrentGP;
    s_RCULock.Unlock();
}

void
RCUFinalizeThread(SSrvThread* thr)
{
    SRCUInfo* rcu = thr->rcu;

    if (RCUHasCalls(rcu))
        abort();

    s_RCULock.Lock();
    if (rcu->seen_gp != s_CurrentGP)
        s_RCUNoteStartedGP(rcu);
    --s_ActiveThreads;
    s_RCULock.Unlock();

    rcu->calls.pop_front();
    rcu->calls.pop_front();
    delete rcu->gp_marker_cur;
    rcu->gp_marker_cur = NULL;
    delete rcu->gp_marker_next;
    rcu->gp_marker_next = NULL;

    delete rcu;
    thr->rcu = NULL;
}



CSrvRCUUser::CSrvRCUUser(void)
{}

CSrvRCUUser::~CSrvRCUUser(void)
{}

void
CSrvRCUUser::CallRCU(void)
{
    SSrvThread* thr = GetCurThread();
    if ((thr->thread_state == eThreadLockedForStop
            ||  thr->thread_state >= eThreadStopped)
        &&  this != thr)
    {
        abort();
    }
    thr->rcu->calls.push_back(*this);
}


CFakeRCUUser::CFakeRCUUser(void)
{}

CFakeRCUUser::~CFakeRCUUser(void)
{}

void
CFakeRCUUser::ExecuteRCU(void)
{}

END_NCBI_SCOPE;
