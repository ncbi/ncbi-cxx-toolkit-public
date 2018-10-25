#ifndef SYNCOBJ__HPP
#define SYNCOBJ__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Synchronization classes and utilities
 *
 */

#include <atomic>
#include <thread>
#include <signal.h>
#include <functional>

#include <corelib/ncbithr.hpp>

#include "IdCassScope.hpp"


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

bool WaitCondVar(unsigned int  timeoutmks, CFastMutex &  mtx,
                 CConditionVariable &  ev,
                 const function<bool()> &  is_done_cb,
                 const function<void(bool*)> &  update_rslt_cb);


#ifdef __linux__
class CFutex
{
public:
    enum EWaitResult {
        eWaitResultTimeOut, // value hasn't changed
        eWaitResultOk,      // value is changed and we waited
        eWaitResultOkFast   // value is changed but we didn't wait
    };

    void DoWake(int waiters = INT_MAX);

public:
    CFutex(int start = 0) :
        m_Value(start)
    {}

    CFutex& operator=(const CFutex&) = delete;
    CFutex(const CFutex&) = delete;

    bool CompareExchange(int expected, int replace_with,
                         bool wake_others = true)
    {
        bool    rv = m_Value.compare_exchange_weak(expected, replace_with,
                                                   memory_order_relaxed);
        if (rv && wake_others)
            DoWake();
        return rv;
    }

    int Value(void)
    {
        return m_Value;
    }

    EWaitResult WaitWhile(int value, int timeout_mks = -1);

    void Wake(void)
    {
        DoWake();
    }

    int Set(int value, bool wake_others = true)
    {
        int rv = m_Value.exchange(value);
        if ((rv != value) && wake_others)
            DoWake();
        return rv;
    }

    int Inc(bool wake_others = true)
    {
        int rv = atomic_fetch_add(&m_Value, 1);
        if (wake_others)
            DoWake();
        return rv;
    }

    int Dec(bool wake_others = true)
    {
        int     rv = atomic_fetch_sub(&m_Value, 1);
        if (wake_others)
            DoWake();
        return rv;
    }

    int SemInc(void)
    {
        int     rv = atomic_fetch_add(&m_Value, 1);
        return rv;
    }

    int SemDec(void)
    {
        int     rv = atomic_fetch_sub(&m_Value, 1);
        if (rv < 0)
            DoWake();
        return rv;
    }

private:
    volatile atomic_int     m_Value;
};
#endif


class SSignalHandler
{
public:
    static bool s_CtrlCPressed(void)
    {
        return sm_CtrlCPressed != 0;
    }

    static void s_WatchCtrlCPressed(bool  enable,
                                    function<void()> on_ctrl_c_pressed = NULL);

private:
    static volatile sig_atomic_t                        sm_CtrlCPressed;
#ifdef __linux__
    static CFutex                                       sm_CtrlCPressedEvent;
#endif
    static unique_ptr<thread, function<void(thread*)> > sm_WatchThread;
    static function<void()>                             sm_OnCtrlCPressed;
    static volatile bool                                sm_Quit;
};


END_IDBLOB_SCOPE

#endif
