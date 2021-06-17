#ifndef PSGS_IO_CALLBACKS__HPP
#define PSGS_IO_CALLBACKS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG server processor callbacks for async I/O
 *
 */

#include <uv.h>

#include <functional>
#include <string>
using namespace std;


// Asynchronous callback on a socket event.
// The instance must live while the user is still interested in the events.
// The processor instance must live longer than the callback class instance.
class CPSGS_SocketIOCallback
{
    public:
        // The socket events on which asyncronous callbacks are supported
        enum EPSGS_Event {
            ePSGS_Readable,
            ePSGS_Writable,
            ePSGS_Disconnect,
            ePSGS_Prioritized
        };

        // All callbacks must tell what to do with the polled socket:
        // - to continue: the timer will be restarted
        // - to stop polling: the timer will be stopped and it is safe to
        //   destroy the callback object
        enum EPSGS_PollContinue {
            ePSGS_StopPolling,
            ePSGS_ContinuePolling
        };

        using TEventCB = function<EPSGS_PollContinue(void *  user_data)>;
        using TTimeoutCB = function<EPSGS_PollContinue(void *  user_data)>;
        using TErrorCB = function<EPSGS_PollContinue(const string &  message,
                                                     void *  user_data)>;

        // Starts polling the given fd for the given event
        CPSGS_SocketIOCallback(int  fd,
                               EPSGS_Event  event,
                               uint64_t  timeout_millisec,
                               void *  user_data,
                               TEventCB  event_cb,
                               TTimeoutCB  timeout_cb,
                               TErrorCB  error_cb);

        ~CPSGS_SocketIOCallback();

        CPSGS_SocketIOCallback(const CPSGS_SocketIOCallback &) = delete;
        CPSGS_SocketIOCallback(CPSGS_SocketIOCallback &&) = delete;
        CPSGS_SocketIOCallback & operator=(const CPSGS_SocketIOCallback &) = delete;

    public:
        void x_UvOnTimer(void);
        void x_UvOnSocketEvent(int  status);

    private:
        void x_StartPolling(int  fd, EPSGS_Event  event);
        void x_StopPolling(void);
        void x_StartTimer(void);
        void x_StopTimer(void);

    private:
        uv_timer_t      m_TimerReq;
        uint64_t        m_TimerMillisec;
        bool            m_TimerActive;
        uv_poll_t       m_PollReq;
        bool            m_PollActive;

        void *          m_UserData;
        TEventCB        m_EventCB;
        TTimeoutCB      m_TimeoutCB;
        TErrorCB        m_ErrorCB;
};


#endif  // PSGS_IO_CALLBACKS__HPP

