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

#ifdef NCBI_OS_LINUX
# include <unistd.h>
# include <sys/syscall.h>
# include <errno.h>
# include <linux/futex.h>
#endif


BEGIN_NCBI_SCOPE;


CFutex::EWaitResult
CFutex::WaitValueChange(int old_value)
{
#ifdef NCBI_OS_LINUX
retry:
    int res = syscall(SYS_futex, &m_Value, FUTEX_WAIT, old_value, NULL, NULL, 0);
    if (res == -1)
        res = errno;
    if (res == EINTR)
        goto retry;
    return res == 0? eWaitWokenUp: eValueChanged;
#else
    return eValueChanged;
#endif
}

CFutex::EWaitResult
CFutex::WaitValueChange(int old_value, const CSrvTime& timeout)
{
#ifdef NCBI_OS_LINUX
    CSrvTime start_time = CSrvTime::Current();
    CSrvTime left_time = timeout;

retry:
    int res = syscall(SYS_futex, &m_Value, FUTEX_WAIT, old_value, &left_time, NULL, 0);
    if (res == -1)
        res = errno;
    if (res != EINTR) {
        switch (res) {
        case 0:
            return eWaitWokenUp;
        case EWOULDBLOCK:
            return eValueChanged;
        case ETIMEDOUT:
            return eTimedOut;
        default:
            abort();
        }
    }

    CSrvTime spent_time = CSrvTime::Current();
    spent_time -= start_time;
    if (spent_time >= timeout)
        return eTimedOut;
    left_time = timeout;
    left_time -= spent_time;

    goto retry;

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
        while (m_Futex.WakeUpWaiters(1) != 1) {
#ifdef NCBI_OS_LINUX
            sched_yield();
#endif
        }
    }
}

END_NCBI_SCOPE;
