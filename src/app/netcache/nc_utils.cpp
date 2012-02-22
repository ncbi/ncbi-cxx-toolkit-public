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


///
static const unsigned int     kListenStubsCnt = 1000;
///
static const unsigned int     kMinListenStubsCnt = 50;
///
static CStdPoolOfThreads*     s_NotifyThreadPool = NULL;
///
static SNCBlockedOpListeners  s_ListenStubs   [kListenStubsCnt];
///
static SNCBlockedOpListeners* s_ListenStubPtrs[kListenStubsCnt];
///
static CAtomicCounter         s_NextGetPtr;
///
static CAtomicCounter         s_NextReturnPtr;
///
static CAtomicCounter         s_CntListenStubs;
///
static CSpinLock              s_WrappingLock;


///
struct SNCListenStubsInitializer
{
    SNCListenStubsInitializer(void) {
        s_NextGetPtr.Set(0);
        s_NextReturnPtr.Set(0);
        s_CntListenStubs.Set(kListenStubsCnt);
        for (unsigned int i = 0; i < kListenStubsCnt; ++i) {
            s_ListenStubPtrs[i] = &s_ListenStubs[i];
        }
    }
};

static SNCListenStubsInitializer s_StubsInitializer;


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
INCBlockedOpListener::BindToThreadPool(CStdPoolOfThreads* pool)
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


static unsigned int
s_WrapNextGetPtr(void)
{
    CSpinGuard guard(s_WrappingLock);
    if (s_NextGetPtr.Get() >= kListenStubsCnt) {
        s_NextGetPtr.Set(0);
        return 0;
    }
    else {
        return s_NextGetPtr.Add(1);
    }
}

static unsigned int
s_WrapNextReturnPtr(void)
{
    CSpinGuard guard(s_WrappingLock);
    if (s_NextReturnPtr.Get() >= kListenStubsCnt) {
        s_NextReturnPtr.Set(0);
        return 0;
    }
    else {
        return s_NextReturnPtr.Add(1);
    }
}

static void
s_ReturnListenStub(SNCBlockedOpListeners* stub)
{
    if (stub >= s_ListenStubs  &&  stub < &s_ListenStubs[kListenStubsCnt]) {
        unsigned int ind = s_NextReturnPtr.Add(1);
        if (ind >= kListenStubsCnt)
            ind = s_WrapNextReturnPtr();
        s_ListenStubPtrs[ind] = stub;
        s_CntListenStubs.Add(1);
    }
    else {
        delete stub;
    }
}

static SNCBlockedOpListeners*
s_GetNextListenStub_Overflowed(unsigned int ind)
{
    s_ReturnListenStub(s_ListenStubPtrs[ind]);
    //printf("Listener stubs array was overflowed!\n");
    return new SNCBlockedOpListeners;
}

static SNCBlockedOpListeners*
s_GetNextListenStub(void)
{
    unsigned int ind = s_NextGetPtr.Add(1);
    if (ind >= kListenStubsCnt)
        ind = s_WrapNextGetPtr();
    if (s_CntListenStubs.Add(-1) <= kMinListenStubsCnt)
        return s_GetNextListenStub_Overflowed(ind);
    else
        return s_ListenStubPtrs[ind];
}

void
CNCLongOpTrigger::SetState(ENCLongOpState state)
{
    CSpinGuard guard(m_ObjLock);
    m_State = state;
}

ENCBlockingOpResult
CNCLongOpTrigger::StartWorking(INCBlockedOpListener* listener)
{
    CSpinGuard guard(m_ObjLock);
    
    if (m_State == eNCOpInProgress) {
        SNCBlockedOpListeners* next = s_GetNextListenStub();
        next->next = m_Listeners;
        next->listener = listener;
        m_Listeners = next;
        return eNCWouldBlock;
    }
    else if (m_State != eNCOpCompleted) {
        m_State = eNCOpInProgress;
    }
    return eNCSuccessNoBlock;
}

void
CNCLongOpTrigger::OperationCompleted(void)
{
    {{
        // To avoid races with StartWorking()
        CSpinGuard guard(m_ObjLock);
        _ASSERT(m_State == eNCOpInProgress);
        m_State = eNCOpCompleted;
    }}

    if (m_Listeners) {
        SNCBlockedOpListeners* listeners = m_Listeners;
        while (listeners) {
            listeners->listener->Notify();
            SNCBlockedOpListeners* next = listeners->next;
            s_ReturnListenStub(listeners);
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
