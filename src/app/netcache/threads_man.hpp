#ifndef NETCACHE__THREADS_MAN__HPP
#define NETCACHE__THREADS_MAN__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


#include "scheduler.hpp"

#ifdef NCBI_OS_LINUX
# include <pthread.h>
#endif


BEGIN_NCBI_SCOPE


struct SMMMemPoolsSet;
struct SLogData;
struct SRCUInfo;
struct SSchedInfo;
struct SSocketsData;
class CSrvStat;


void InitCurThreadStorage(void);
SSrvThread* GetCurThread(void);
void ConfigureThreads(CNcbiRegistry* reg, CTempString section);
bool InitThreadsMan(void);
void RunMainThread(void);
void FinalizeThreadsMan(void);
void RequestThreadStart(SSrvThread* thr);
void RequestThreadStop(SSrvThread* thr);
void RequestThreadRevive(SSrvThread* thr);
TSrvThreadNum GetCntRunningThreads(void);



enum EThreadState {
    eThreadStarting,
    eThreadRunning,
    eThreadRevived,
    eThreadLockedForStop,
    eThreadStopped,
    eThreadReleased,
    eThreadDormant
};

struct SSrvThread : public CSrvRCUUser
{
    TSrvThreadNum thread_num;
    Uint4 seen_jiffy;
    int seen_secs;
    EThreadState thread_state;
    EServerState seen_srv_state;
#ifdef NCBI_OS_LINUX
    pthread_t thread_handle;
#endif
    CSrvTask* cur_task;
    SMMMemPoolsSet* mm_pool;
    SSchedInfo* sched;
    SLogData* log_data;
    SRCUInfo* rcu;
    SSocketsData* socks;
    CSrvStat* stat;


    SSrvThread(void);
    virtual ~SSrvThread(void);

private:
    virtual void ExecuteRCU(void);
};


enum EThreadMgrState {
    eThrMgrIdle,
    eThrMgrPreparesToStop,
    eThrMgrThreadExited,
    eThrMgrSocksMoved,
    eThrMgrNeedNewThread,
    eThrMgrStarting
};


//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline bool
IsThreadRunning(SSrvThread* thr)
{
    return thr->thread_state <= eThreadRevived;
}

END_NCBI_SCOPE

#endif /* NETCACHE__THREADS_MAN__HPP */
