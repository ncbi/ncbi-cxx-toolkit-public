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

#include <corelib/ncbireg.hpp>

#include "threads_man.hpp"
#include "timers.hpp"
#include "sockets_man.hpp"
#include "memory_man.hpp"
#include "logging.hpp"
#include "time_man.hpp"
#include "server_core.hpp"
#include "rcu.hpp"
#include "scheduler.hpp"
#include "srv_stat.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/prctl.h>
#endif


BEGIN_NCBI_SCOPE;


// We should reserve space for main thread and service thread, so that total
// amount doesn't overflow.
static const TSrvThreadNum kMaxNumberOfThreads
                                = numeric_limits<TSrvThreadNum>::max() - 2;


SSrvThread** s_Threads = NULL;
static CMiniMutex s_ThrMgrLock;
static EThreadMgrState s_ThreadMgrState = eThrMgrIdle;
static SSrvThread* s_CurMgrThread = NULL;
static SSrvThread* s_MainThr = NULL;
static SSrvThread* s_SvcThr = NULL;

#ifdef NCBI_OS_LINUX
static pthread_key_t s_CurThreadKey;
#endif
static CFutex s_SvcSignal;

TSrvThreadNum s_MaxRunningThreads = 20;


extern Uint4 s_CurJiffies;
extern CSrvTime s_JiffyTime;
extern string s_AppBaseName;




SSrvThread*
GetCurThread(void)
{
    SSrvThread* thr = NULL;
#ifdef NCBI_OS_LINUX
    thr = (SSrvThread*)pthread_getspecific(s_CurThreadKey);
#endif
    return thr;
}

TSrvThreadNum
CTaskServer::GetCurThreadNum(void)
{
    SSrvThread* thr = GetCurThread();
    if (thr->thread_num == 0  ||  thr->thread_num == s_MaxRunningThreads + 1) {
        SRV_FATAL("Unexpected ThreadNum: " << thr->thread_num);
    }
    return thr->thread_num - 1;
}

TSrvThreadNum
GetCntRunningThreads(void)
{
    TSrvThreadNum result = 0;
    for (TSrvThreadNum i = 1; i <= s_MaxRunningThreads; ++i, ++result) {
        if (!IsThreadRunning(s_Threads[i]))
            break;
    }
    return result;
}

static void
s_SetCurThread(SSrvThread* thr)
{
#ifdef NCBI_OS_LINUX
    pthread_setspecific(s_CurThreadKey, thr);
    char buf[20];
    if (thr->thread_state != eThreadDormant)
        snprintf(buf, 20, "%s_%d", s_AppBaseName.c_str(), thr->thread_num);
    else if (thr->thread_num == 0)
        snprintf(buf, 20, "%s", s_AppBaseName.c_str());
    else
        snprintf(buf, 20, "%s_S", s_AppBaseName.c_str());
    prctl(PR_SET_NAME, (unsigned long)buf, 0, 0, 0);
#endif
}

static void
s_RegisterNewThread(SSrvThread* thr)
{
    thr->thread_state = eThreadRunning;
    s_SetCurThread(thr);
    if (s_ThreadMgrState == eThrMgrStarting) {
        thr->stat->ThreadStarted();
        s_ThrMgrLock.Lock();
        if (s_CurMgrThread != thr) {
            SRV_FATAL("Invalid SrvThread state");
        }
        s_ThreadMgrState = eThrMgrIdle;
        s_CurMgrThread = NULL;
        s_ThrMgrLock.Unlock();
    }
}

static void
s_PerJiffyTasks_Main(SSrvThread* thr)
{
    if (CTaskServer::IsInShutdown()) {
        RCUPassQS(thr->rcu);
        return;
    }

    if (thr->seen_jiffy == s_CurJiffies)
        return;
    thr->seen_jiffy = s_CurJiffies;
    RCUPassQS(thr->rcu);

    if (thr->seen_secs == CSrvTime::CurSecs())
        return;
    thr->seen_secs = CSrvTime::CurSecs();
    CheckLoggingFlush(thr);
}

static void
s_PerJiffyTasks_Service(SSrvThread* thr)
{
    if (CTaskServer::IsInShutdown()) {
        RCUPassQS(thr->rcu);
        return;
    }

    if (thr->seen_jiffy == s_CurJiffies)
        return;
    thr->seen_jiffy = s_CurJiffies;
    RCUPassQS(thr->rcu);
    SchedCheckOverloads();

    if (thr->seen_secs == CSrvTime::CurSecs())
        return;
    thr->seen_secs = CSrvTime::CurSecs();
    CheckLoggingFlush(thr);
    TimerTick();
}

static void
s_PerJiffyTasks_Worker(SSrvThread* thr)
{
    if (CTaskServer::IsInShutdown()) {
        if (thr->seen_srv_state != s_SrvState) {
            thr->seen_srv_state = s_SrvState;
            SetAllSocksRunnable(thr->socks);
        }
        RCUPassQS(thr->rcu);
        PromoteSockAmount(thr->socks);
        CheckConnectsTimeout(thr->socks);
        if (IsServerStopping()  &&  !RCUHasCalls(thr->rcu))
            thr->thread_state = eThreadStopped;
        return;
    }

    if (thr->seen_jiffy == s_CurJiffies)
        return;
    thr->seen_jiffy = s_CurJiffies;
    RCUPassQS(thr->rcu);
    SchedStartJiffy(thr);
    PromoteSockAmount(thr->socks);
    CheckConnectsTimeout(thr->socks);

    if (thr->seen_secs == CSrvTime::CurSecs())
        return;
    thr->seen_secs = CSrvTime::CurSecs();
    CheckLoggingFlush(thr);
    if (thr->thread_state != eThreadLockedForStop) {
        CleanSocketList(thr->socks);
        if (thr->thread_num == 1  &&  s_ThreadMgrState == eThrMgrThreadExited) {
            MoveAllSockets(thr->socks, s_CurMgrThread->socks);
            s_ThrMgrLock.Lock();
            s_ThreadMgrState = eThrMgrSocksMoved;
            s_ThrMgrLock.Unlock();
        }
    }
}

static SSrvThread*
s_AllocThread(TSrvThreadNum thread_num)
{
    SSrvThread* new_thr = new SSrvThread();
    new_thr->thread_num = thread_num;
    new_thr->stat = new CSrvStat();
    AssignThreadMemMgr(new_thr);
    AssignThreadLogging(new_thr);
    AssignThreadSched(new_thr);
    AssignThreadSocks(new_thr);
    s_Threads[thread_num] = new_thr;
    return new_thr;
}

static void*
s_WorkerThreadMain(void* data)
{
    SSrvThread* thr = (SSrvThread*)data;
    s_RegisterNewThread(thr);
    RCUInitNewThread(thr);

    while (thr->thread_state != eThreadStopped) {
        SchedExecuteTask(thr);
        s_PerJiffyTasks_Worker(thr);
    }

    RCUFinalizeThread(thr);

    if (!IsServerStopping()) {
        s_ThrMgrLock.Lock();
        if(s_CurMgrThread != thr  ||  s_ThreadMgrState != eThrMgrPreparesToStop) {
            SRV_FATAL("Invalid WorkerThread state");
        }
        s_ThreadMgrState = eThrMgrThreadExited;
        s_ThrMgrLock.Unlock();
    }
    return NULL;
}

static bool
s_StartThread(SSrvThread* thr, void* (*thr_func)(void*))
{
#ifdef NCBI_OS_LINUX
    int res = pthread_create(&thr->thread_handle, NULL, thr_func, (void*)thr);
    if (res != 0) {
        SRV_LOG(Critical, "Unable to create new thread, result=" << res);
        return false;
    }
#endif
    return true;
}

static void
s_StopCurMgrThread(void)
{
#ifdef NCBI_OS_LINUX
    int res = pthread_join(s_CurMgrThread->thread_handle, NULL);
    if (res != 0) {
        SRV_LOG(Critical, "Cannot join the thread, res=" << res);
    }
#endif

    ReleaseThreadMemMgr(s_CurMgrThread);
    ReleaseThreadSched(s_CurMgrThread);
    ReleaseThreadSocks(s_CurMgrThread);
    StopThreadLogging(s_CurMgrThread);
    s_CurMgrThread->stat->ThreadStopped();

    s_ThrMgrLock.Lock();
    s_ThreadMgrState = eThrMgrIdle;
    s_CurMgrThread->thread_state = eThreadReleased;
    s_CurMgrThread = NULL;
    s_ThrMgrLock.Unlock();
}

static void
s_StartCurMgrThread(void)
{
    s_ThrMgrLock.Lock();
    s_ThreadMgrState = eThrMgrStarting;
    s_ThrMgrLock.Unlock();
    StartThreadLogging(s_CurMgrThread);
    if (!s_StartThread(s_CurMgrThread, &s_WorkerThreadMain)) {
        s_ThrMgrLock.Lock();
        s_ThreadMgrState = eThrMgrIdle;
        s_CurMgrThread->thread_state = eThreadReleased;
        s_CurMgrThread = NULL;
        s_ThrMgrLock.Unlock();
    }
}

static void*
s_ServiceThreadMain(void*)
{
    s_SetCurThread(s_SvcThr);
    RCUInitNewThread(s_SvcThr);

    CSrvTime next_jfy_time = CSrvTime::Current();
    next_jfy_time += s_JiffyTime;
    while (!IsServerStopping()  ||  RCUHasCalls(s_SvcThr->rcu)) {
        s_PerJiffyTasks_Service(s_SvcThr);

        if (s_ThreadMgrState == eThrMgrNeedNewThread)
            s_StartCurMgrThread();
        else if (s_ThreadMgrState == eThrMgrSocksMoved)
            s_StopCurMgrThread();

        if (CTaskServer::IsInShutdown())
            TrackShuttingDown();

        CSrvTime cur_time = CSrvTime::Current();
        if (next_jfy_time > cur_time) {
            CSrvTime wait_time = next_jfy_time;
            wait_time -= cur_time;
            s_SvcSignal.WaitValueChange(0, wait_time);
        }
        IncCurJiffies();

        next_jfy_time = s_LastJiffyTime;
        next_jfy_time += s_JiffyTime;
    }

    RCUFinalizeThread(s_SvcThr);
    return NULL;
}

void
RequestThreadStart(SSrvThread* thr)
{
    if (s_ThreadMgrState != eThrMgrIdle  ||  CTaskServer::IsInShutdown())
        return;

    s_ThrMgrLock.Lock();
    if (s_ThreadMgrState == eThrMgrIdle  &&  thr->thread_state == eThreadReleased) {
        s_ThreadMgrState = eThrMgrNeedNewThread;
        s_CurMgrThread = thr;
        thr->thread_state = eThreadStarting;
    }
    s_ThrMgrLock.Unlock();
}

void
RequestThreadStop(SSrvThread* thr)
{
    if (s_ThreadMgrState != eThrMgrIdle  ||  CTaskServer::IsInShutdown())
        return;

    s_ThrMgrLock.Lock();
    if (s_ThreadMgrState == eThrMgrIdle  &&  thr->thread_state == eThreadRunning) {
        s_ThreadMgrState = eThrMgrPreparesToStop;
        s_CurMgrThread = thr;
        thr->thread_state = eThreadLockedForStop;
        thr->CallRCU();
    }
    s_ThrMgrLock.Unlock();
}

void
RequestThreadRevive(SSrvThread* thr)
{
    s_ThrMgrLock.Lock();
    if (thr->thread_state != eThreadLockedForStop ||
        s_ThreadMgrState != eThrMgrPreparesToStop) {
        SRV_FATAL("Invalid thread state");
    }
    thr->thread_state = eThreadRevived;
    s_ThreadMgrState = eThrMgrIdle;
    s_CurMgrThread = NULL;
    s_ThrMgrLock.Unlock();
}

static bool
s_StartAllThreads(void)
{
    LogNoteThreadsStarted();

    if (!s_StartThread(s_SvcThr, &s_ServiceThreadMain))
        return false;
    for (TSrvThreadNum i = 1; i <= s_MaxRunningThreads; ++i) {
        if (!s_StartThread(s_Threads[i], &s_WorkerThreadMain)) {
            for (TSrvThreadNum j = i; j <= s_MaxRunningThreads; ++j) {
                s_Threads[j]->thread_state = eThreadReleased;
            }
            break;
        }
    }
    return true;
}

static void
s_JoinAllThreads(void)
{
    RCUFinalizeThread(s_MainThr);

#ifdef NCBI_OS_LINUX
    for (TSrvThreadNum i = 1; i <= s_MaxRunningThreads + 1; ++i) {
        SSrvThread* thr = s_Threads[i];
        if (thr->thread_state < eThreadReleased) {
            pthread_join(thr->thread_handle, NULL);
            ReleaseThreadLogging(thr);
        }
    }
    ReleaseThreadLogging(s_SvcThr);
#endif
}

void
InitCurThreadStorage(void)
{
#ifdef NCBI_OS_LINUX
    int res = pthread_key_create(&s_CurThreadKey, NULL);
    if (res) {
        printf("terminating after pthread_key_create returned error %d\n", res);
        SRV_FATAL("pthread_key_create returned error: " << res);
    }
#endif
}

void
ConfigureThreads(CNcbiRegistry* reg, CTempString section)
{
    s_MaxRunningThreads = TSrvThreadNum(reg->GetInt(section, "max_threads", 20));
    if (s_MaxRunningThreads > kMaxNumberOfThreads)
        s_MaxRunningThreads = kMaxNumberOfThreads;
}

bool
InitThreadsMan(void)
{
    s_Threads = (SSrvThread**)calloc(s_MaxRunningThreads + 2, sizeof(s_Threads[0]));
    for (TSrvThreadNum i = 0; i <= s_MaxRunningThreads + 1; ++i) {
        s_AllocThread(i);
    }

    s_MainThr = s_Threads[0];
    s_SvcThr = s_Threads[s_MaxRunningThreads + 1];

    s_MainThr->thread_state = eThreadDormant;
    s_SvcThr->thread_state = eThreadDormant;
    s_SetCurThread(s_MainThr);
    RCUInitNewThread(s_MainThr);

    return true;
}

void
RunMainThread(void)
{
    if (!s_StartAllThreads())
        return;
    if (!IsThreadRunning(s_Threads[1]))
        CTaskServer::RequestShutdown(eSrvFastShutdown);

    while (!IsServerStopping()) {
        s_PerJiffyTasks_Main(s_MainThr);
        DoSocketWait();
    }

    s_JoinAllThreads();
}

void
FinalizeThreadsMan(void)
{}



SSrvThread::SSrvThread(void)
    : seen_jiffy(0),
      seen_secs(0),
      thread_state(eThreadStarting),
      seen_srv_state(eSrvRunning),
      cur_task(NULL),
      mm_pool(NULL),
      sched(NULL),
      log_data(NULL),
      rcu(NULL),
      socks(NULL)
{}

SSrvThread::~SSrvThread(void)
{}

void
SSrvThread::ExecuteRCU(void)
{
    switch (thread_state) {
    case eThreadLockedForStop:
        thread_state = eThreadStopped;
        break;
    case eThreadRevived:
        thread_state = eThreadRunning;
        break;
    default:
        SRV_FATAL("Unexpected thread state: " << thread_state);
    }
}

END_NCBI_SCOPE;
