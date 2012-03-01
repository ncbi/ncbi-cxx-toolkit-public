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

#include <ncbi_pch.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/thread_pool_old.hpp>

#include "nc_utils.hpp"

#ifdef NCBI_OS_LINUX
# include <unistd.h>
# include <sys/syscall.h>
# include <errno.h>
# include <linux/futex.h>
#endif


BEGIN_NCBI_SCOPE;

/// TLS key for storing index of the current thread
static TNCTlsKey      s_NCThreadIndexKey;
/// Next available thread index
static CAtomicCounter s_NextThreadIndex;

void
g_InitNCThreadIndexes(void)
{
    NCTlsCreate(s_NCThreadIndexKey, NULL);
    s_NextThreadIndex.Set(0);
}

TNCThreadIndex
g_GetNCThreadIndex(void)
{
    TNCThreadIndex index = static_cast<TNCThreadIndex>(
                      reinterpret_cast<size_t>(NCTlsGet(s_NCThreadIndexKey)));
    if (!index) {
        index = static_cast<TNCThreadIndex>(s_NextThreadIndex.Add(1));
        // 64-bit MSVC accepts only this 2-steps conversion to void* without
        // warning.
        size_t dummy = static_cast<size_t>(index);
        NCTlsSet(s_NCThreadIndexKey, reinterpret_cast<void*>(dummy));
    }
    return index;
}


string
g_ToSizeStr(Uint8 size)
{
    static const char* const posts[] = {" B", " KB", " MB", " GB", " TB", " PB"};

    string res = NStr::UInt8ToString(size);
    size_t dot_pos = res.size();
    Uint1 post_idx = 0;
    while (dot_pos > 3  &&  post_idx < sizeof(posts) / sizeof(posts[0])) {
        dot_pos -= 3;
        ++post_idx;
    }
    if (dot_pos >= 3  ||  res.size() <= 3) {
        res.resize(dot_pos);
    }
    else {
        res.resize(3);
        res.insert(dot_pos, 1, '.');
    }
    return res + posts[post_idx];
}


///
struct SNCBlockedOpListeners
{
    INCBlockedOpListener*  listener;
    SNCBlockedOpListeners* next;
};


static CPoolOfThreads_ForServer* s_NotifyThreadPool = NULL;


///
class CNCLongOpNotification : public CStdRequest
{
public:
    CNCLongOpNotification(INCBlockedOpListener* listener)
        : m_Listener(listener)
    {}

    virtual void Process(void)
    {
        m_Listener->OnBlockedOpFinish();
    }

private:
    INCBlockedOpListener* m_Listener;
};


void
INCBlockedOpListener::BindToThreadPool(CPoolOfThreads_ForServer* pool)
{
    s_NotifyThreadPool = pool;
}

void
INCBlockedOpListener::Notify(void)
{
    _ASSERT(s_NotifyThreadPool);
    bool done = false;
    while (!done) {
        try {
            s_NotifyThreadPool->AcceptRequest(CRef<CStdRequest>(new CNCLongOpNotification(this)));
            done = true;
        }
        catch (CBlockingQueueException& ex) {
            ERR_POST(Critical << ex);
            SleepMilliSec(1000);
        }
    }
}

INCBlockedOpListener::~INCBlockedOpListener(void)
{}


void
CNCLongOpTrigger::SetState(ENCLongOpState state)
{
    m_ObjLock.Lock();
    m_State = state;
    m_ObjLock.Unlock();
}

ENCBlockingOpResult
CNCLongOpTrigger::StartWorking(INCBlockedOpListener* listener)
{
    ENCBlockingOpResult result = eNCSuccessNoBlock;

    m_ObjLock.Lock();
    if (m_State == eNCOpInProgress) {
        SNCBlockedOpListeners* next = new SNCBlockedOpListeners;
        next->next = m_Listeners;
        next->listener = listener;
        m_Listeners = next;
        result = eNCWouldBlock;
    }
    else if (m_State != eNCOpCompleted) {
        m_State = eNCOpInProgress;
    }
    m_ObjLock.Unlock();

    return result;
}

void
CNCLongOpTrigger::OperationCompleted(void)
{
    // To avoid races with StartWorking()
    m_ObjLock.Lock();
    _ASSERT(m_State == eNCOpInProgress);
    m_State = eNCOpCompleted;
    m_ObjLock.Unlock();

    if (m_Listeners) {
        SNCBlockedOpListeners* listeners = m_Listeners;
        while (listeners) {
            listeners->listener->Notify();
            SNCBlockedOpListeners* next = listeners->next;
            delete listeners;
            listeners = next;
        }
        m_Listeners = NULL;
    }
}

/*
void
CSpinRWLock::ReadLock(void)
{
    CAtomicCounter::TValue lock_cnt = m_LockCount.Add(kReadLockValue);
    // If some writer already acquired or requested a lock we should release
    // read lock and re-acquire it after writer is done.
    while (lock_cnt > kWriteLockValue) {
        m_LockCount.Add(-kReadLockValue);
        NCBI_SCHED_YIELD();
        lock_cnt = m_LockCount.Add(kReadLockValue);
    }
}

void
CSpinRWLock::ReadUnlock(void)
{
    _VERIFY(m_LockCount.Add(-kReadLockValue) + kReadLockValue >= kReadLockValue);
}

void
CSpinRWLock::WriteLock(void)
{
    CAtomicCounter::TValue lock_cnt = m_LockCount.Add(kWriteLockValue);
    // If there's another writer who already acquired or requested the lock
    // earlier then we need to release write lock and wait until another
    // writer is done.
    while (lock_cnt >= 2 * kWriteLockValue) {
        m_LockCount.Add(-kWriteLockValue);
        NCBI_SCHED_YIELD();
        lock_cnt = m_LockCount.Add(kWriteLockValue);
    }
    // We will be here if no other writer exists or we requested a write lock
    // earlier than other writer. So we need just to wait until other readers
    // release there locks - wait without releasing our lock to prevent new
    // read locks.
    while (lock_cnt != kWriteLockValue) {
        NCBI_SCHED_YIELD();
        lock_cnt = m_LockCount.Get();
    }
}

void
CSpinRWLock::WriteUnlock(void)
{
    _VERIFY(m_LockCount.Add(-kWriteLockValue) + kWriteLockValue >= kWriteLockValue);
}
*/

CFutex::EWaitResult
CFutex::WaitValueChange(int old_value)
{
#ifdef NCBI_OS_LINUX
retry:
    int res = syscall(SYS_futex, &m_Value, FUTEX_WAIT, old_value, NULL, NULL, 0);
    if (res == EINTR)
        goto retry;
    return res == 0? eWaitWokenUp: eValueChanged;
#else
    return eValueChanged;
#endif
}

int
CFutex::WakeUpWaiters(int cnt_to_wake)
{
#ifdef NCBI_OS_LINUX
    return syscall(SYS_futex, &m_Value, FUTEX_WAKE, cnt_to_wake, NULL, NULL, 0);
#else
    return 0;
#endif
}


void
CMiniMutex::Lock(void)
{
    int val = m_Futex.AddValue(1);
    _ASSERT(val >= 1);
    if (val != 1) {
        while (m_Futex.WaitValueChange(val) == CFutex::eValueChanged)
            val = m_Futex.GetValue();
    }
}

void
CMiniMutex::Unlock(void)
{
    int val = m_Futex.AddValue(-1);
    _ASSERT(val >= 0);
    if (val != 0) {
        while (m_Futex.WakeUpWaiters(1) != 1)
            NCBI_SCHED_YIELD();
    }
}

END_NCBI_SCOPE;
