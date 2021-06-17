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
#include <string>
#include <memory>

#ifdef __linux__
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include <corelib/ncbithr.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_exception.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;


bool WaitCondVar(unsigned int  timeout_mks, CFastMutex &  mux,
                 CConditionVariable &  ev,
                 const function<bool()> &  is_done_cb,
                 const function<void(bool*)> &  update_result_cb)
{
    int64_t     ns = 0;
    int64_t     s = 0;
    if (timeout_mks != INT_MAX && timeout_mks > 0) {
        ns = (int64_t)timeout_mks * 1000LL;
        s = ns / 1000000000L;
        ns = ns % 1000000000L;
    }

    bool rv = false;
    if (is_done_cb) {
        rv = is_done_cb();
    }

    if (!rv && timeout_mks) {
        CFastMutexGuard guard(mux);
        if (is_done_cb) {
            rv = is_done_cb();
        }
        if (timeout_mks == INT_MAX) {
            while(!rv) {
                ev.WaitForSignal(mux);
                if (is_done_cb) {
                    rv = is_done_cb();
                } else {
                    break;
                }
            }
        } else {
            if (!rv && timeout_mks > 0) {
                ev.WaitForSignal(mux, CDeadline(s, ns));
                if (is_done_cb) {
                    rv = is_done_cb();
                }
            }
        }
    }
    if (update_result_cb) {
        update_result_cb(&rv);
    }
    return (rv);
}

#ifdef __linux__
/** CFutex */

void CFutex::DoWake(int waiters)
{
    int rt = syscall(__NR_futex, &m_Value, FUTEX_WAKE, waiters, NULL);
    if (rt < 0) {
        NCBI_THROW(CCassandraException, eSeqFailed,
                   string("CFutex::DoWake: failed, unexpected errno: ") +
                   NStr::NumericToString(errno));
    }
}


CFutex::EWaitResult CFutex::WaitWhile(int value, int timeout_mks)
{
    struct timespec timeout = {0, 0};
    struct timespec * ptimeout = &timeout;

    if (timeout_mks < 0) {
        ptimeout = nullptr;
    } else {
        timeout.tv_sec = timeout_mks / 1000000L;
        timeout.tv_nsec = (timeout_mks % 1000000L) * 1000L;
    }

    while (syscall(__NR_futex, &m_Value, FUTEX_WAIT, value, ptimeout, 0, 0) != 0) {
        switch (errno) {
            case EWOULDBLOCK:
                /* someone has already changed value */
                return eWaitResultOkFast;
            case EINTR:
                break; /** keep waiting ***/
            case ETIMEDOUT:
                return eWaitResultTimeOut;
            default:
                NCBI_THROW(CCassandraException, eSeqFailed,
                           string("CFutex::WaitWhile: failed, unexpected errno: ") +
                           NStr::NumericToString(errno));
        }
    }
    return eWaitResultOk;
}
#endif


/** SSignalHandler */

volatile sig_atomic_t SSignalHandler::sm_CtrlCPressed(0);
#ifdef __linux__
CFutex SSignalHandler::sm_CtrlCPressedEvent;
#endif

function<void()> SSignalHandler::sm_OnCtrlCPressed(NULL);
unique_ptr<thread, function<void(thread*)> > SSignalHandler::sm_WatchThread;
volatile bool SSignalHandler::sm_Quit(false);


void SSignalHandler::s_WatchCtrlCPressed(bool enable, function<void()> on_ctrl_c_pressed)
{
    sm_CtrlCPressed = 0;
    sm_OnCtrlCPressed = on_ctrl_c_pressed;
#ifdef __linux__
    sm_CtrlCPressedEvent.Set(0);
#endif
    if (sm_WatchThread) {
        ERR_POST(Trace << "Joining Ctrl+C watcher thread");
        sm_WatchThread = NULL;
    }

    if (enable) {
        if (!sm_WatchThread) {
            ERR_POST(Trace << "Creating Ctrl+C watcher thread");
            sm_WatchThread = unique_ptr<thread, function<void(thread*)> >(new thread(
                [](){
                    sm_Quit = false;
                    signal(SIGINT, [](int) {
                        signal(SIGINT, SIG_DFL);
                        SSignalHandler::sm_CtrlCPressed = 1;
#ifdef __linux__
                        SSignalHandler::sm_CtrlCPressedEvent.Inc();
#endif
                    });
                    while (sm_OnCtrlCPressed && !sm_Quit) {
#ifdef __linux__
                        while (SSignalHandler::sm_CtrlCPressedEvent.WaitWhile(
                                    SSignalHandler::sm_CtrlCPressedEvent.Inc(),
                                    100000) == CFutex::eWaitResultTimeOut && !sm_Quit);
#endif
                        ERR_POST(Trace << "Ctr+C watcher thread woke up");
                        if (sm_CtrlCPressed && SSignalHandler::sm_OnCtrlCPressed) {
                            ERR_POST(Trace << "Ctr+C watcher thread: calling closure");
                            SSignalHandler::sm_OnCtrlCPressed();
                        }
                        ERR_POST(Trace << "Ctr+C watcher thread finished");
                    }
                }),
                [](thread* th){
                    if (th) {
                        sm_Quit = true;
                        th->join();
                        signal(SIGINT, SIG_DFL);
                        sm_Quit = false;
                    }
                    delete th;
                }
            );
        }
    }
}

END_IDBLOB_SCOPE
