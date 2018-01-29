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

#include <ncbi_pch.hpp>

#include <atomic>
#include <thread>

#ifdef __linux__
#include <linux/unistd.h>
#include <linux/futex.h>
#endif

#include <corelib/ncbithr.hpp>

#include <objtools/pubseq_gateway/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;


bool WaitCondVar(unsigned int timeoutmks, CFastMutex& Mux, CConditionVariable& Ev, const std::function<bool()>& IsDoneCB, const std::function<void(bool*)>& UpdateRsltCB) {
    int64_t ns = 0, s = 0;
    if (timeoutmks != INT_MAX && timeoutmks > 0) {
        ns = (int64_t)timeoutmks * 1000LL;
        s = ns / 1000000000L;
        ns = ns % 1000000000L;
    }
    bool rv;

    if (IsDoneCB)
        rv = IsDoneCB();
    else
        rv = false;
    if (!rv && timeoutmks) {
        CFastMutexGuard guard(Mux);
        if (timeoutmks == INT_MAX) {
            if (IsDoneCB)
                rv = IsDoneCB();
            while(!rv) {
                Ev.WaitForSignal(Mux);
                if (IsDoneCB)
                    rv = IsDoneCB();
                else
                    break;
            }
        }
        else {
            if (IsDoneCB)
                rv = IsDoneCB();
            if (!rv && timeoutmks > 0) {
                Ev.WaitForSignal(Mux, CDeadline(s, ns));
                if (IsDoneCB)
                    rv = IsDoneCB();
            }
        }
    }
    if (UpdateRsltCB)
        UpdateRsltCB(&rv);
    return (rv);
}

#ifdef __linux__
/** CFutex */

void CFutex::DoWake(int Waiters) {
    int rt = syscall(__NR_futex, &m_value, FUTEX_WAKE, Waiters, NULL);
    if (rt < 0)
        RAISE_ERROR(eSeqFailed, string("CFutex::DoWake: failed, unexpected errno: ") + NStr::NumericToString(errno));
}

CFutex::WaitResult_t CFutex::WaitWhile(int Value, int TimeoutMks) {
    struct timespec timeout = {0}, *ptimeout = &timeout;
    if (TimeoutMks < 0)
        ptimeout = NULL;
    else {
        timeout.tv_sec = TimeoutMks / 1000000L;
        timeout.tv_nsec = (TimeoutMks % 1000000L) * 1000L;
    }

    while (syscall( __NR_futex, &m_value, FUTEX_WAIT, Value, ptimeout, 0, 0) != 0) {
        switch (errno) {
            case EWOULDBLOCK:
                return wrOkFast; /** someone has already changed value **/
            case EINTR:
                break; /** keep waiting ***/
            case ETIMEDOUT:
                return wrTimeOut;
            default:
                RAISE_ERROR(eSeqFailed, string("CFutex::WaitWhile: failed, unexpected errno: ") + NStr::NumericToString(errno));
        }
    }
    return wrOk;
}
#endif

/** SSignalHandler */

volatile sig_atomic_t SSignalHandler::m_CtrlCPressed(0);
#ifdef __linux__
CFutex SSignalHandler::m_CtrlCPressedEvent;
#endif
std::function<void()> SSignalHandler::m_OnCtrlCPressed(NULL);
unique_ptr<thread, function<void(thread*)> > SSignalHandler::m_WatchThread;
volatile bool SSignalHandler::m_Quit(false);

void SSignalHandler::WatchCtrlCPressed(bool Enable, std::function<void()> OnCtrlCPressed) {
    m_CtrlCPressed = 0;
    m_OnCtrlCPressed = OnCtrlCPressed;
#ifdef __linux__
    m_CtrlCPressedEvent.Set(0);
#endif
    if (m_WatchThread) {
        LOG5(("Joining Ctrl+C watcher thread"));
        m_WatchThread = NULL;
    }

    if (Enable) {
        if (!m_WatchThread) {
            LOG5(("Creating Ctrl+C watcher thread"));
            m_WatchThread = unique_ptr<thread, function<void(thread*)> >(new thread(
                [](){
                    m_Quit = false;
                    signal(SIGINT, [](int signum) {
                        signal(SIGINT, SIG_DFL);
                        SSignalHandler::m_CtrlCPressed = 1;
#ifdef __linux__
                        SSignalHandler::m_CtrlCPressedEvent.Inc();
#endif
                    });
                    while (m_OnCtrlCPressed && !m_Quit) {
#ifdef __linux__
                        while (SSignalHandler::m_CtrlCPressedEvent.WaitWhile(SSignalHandler::m_CtrlCPressedEvent.Inc(), 100000) == CFutex::wrTimeOut && !m_Quit);
#endif
                        LOG5(("Ctr+C watcher thread woke up"));
                        if (m_CtrlCPressed && SSignalHandler::m_OnCtrlCPressed) {
                            LOG5(("Ctr+C watcher thread: calling closure"));
                            SSignalHandler::m_OnCtrlCPressed();
                        }
                        LOG5(("Ctr+C watcher thread finished"));
                    }
                }),
                [](thread* th){
                    if (th) {
                        m_Quit = true;
                        th->join();
                        signal(SIGINT, SIG_DFL);
                        m_Quit = false;
                    }
                    delete th;
                }
            );
        }
    }
}

END_IDBLOB_SCOPE
