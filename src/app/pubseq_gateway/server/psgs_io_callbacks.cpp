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

#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "psgs_io_callbacks.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"


void timer_socket_io_cb(uv_timer_t *  handle)
{
    CPSGS_SocketIOCallback *    socket_io_cb = (CPSGS_SocketIOCallback*)(handle->data);
    socket_io_cb->x_UvOnTimer();
}

void poll_socket_io_cb(uv_poll_t *  handle, int  status, int  events)
{
    CPSGS_SocketIOCallback *    socket_io_cb = (CPSGS_SocketIOCallback*)(handle->data);
    socket_io_cb->x_UvOnSocketEvent(status);
}

CPSGS_SocketIOCallback::CPSGS_SocketIOCallback(int  fd,
                                               EPSGS_Event  event,
                                               uint64_t  timeout_millisec,
                                               void *  user_data,
                                               TEventCB  event_cb,
                                               TTimeoutCB  timeout_cb,
                                               TErrorCB  error_cb) :
    m_TimerMillisec(timeout_millisec),
    m_TimerActive(false),
    m_PollActive(false),
    m_UserData(user_data),
    m_EventCB(event_cb),
    m_TimeoutCB(timeout_cb),
    m_ErrorCB(error_cb)
{
    x_StartTimer();
    try {
        x_StartPolling(fd, event);
    } catch (...) {
        x_StopTimer();
        throw;
    }
}


CPSGS_SocketIOCallback::~CPSGS_SocketIOCallback()
{
    x_StopPolling();
}


void CPSGS_SocketIOCallback::x_StartPolling(int  fd,
                                            EPSGS_Event  event)
{
    uv_loop_t * loop = CPubseqGatewayApp::GetInstance()->GetUVLoop();

    int     ret = uv_poll_init(loop, &m_PollReq, fd);
    if (ret < 0) {
        NCBI_THROW(CPubseqGatewayException, eInvalidPollInit, uv_strerror(ret));
    }
    m_PollReq.data = this;

    switch (event) {
        case ePSGS_Readable:
            ret = uv_poll_start(&m_PollReq, UV_READABLE, poll_socket_io_cb);
            break;
        case ePSGS_Writable:
            ret = uv_poll_start(&m_PollReq, UV_WRITABLE, poll_socket_io_cb);
            break;
        case ePSGS_Disconnect:
            ret = uv_poll_start(&m_PollReq, UV_DISCONNECT, poll_socket_io_cb);
            break;
        case ePSGS_Prioritized:
            ret = uv_poll_start(&m_PollReq, UV_PRIORITIZED, poll_socket_io_cb);
            break;
    }
    if (ret < 0) {
        NCBI_THROW(CPubseqGatewayException, eInvalidPollStart, uv_strerror(ret));
    }

    m_PollActive = true;
}


void CPSGS_SocketIOCallback::x_StopPolling(void)
{
    if (m_PollActive) {
        m_PollActive = false;

        int     ret = uv_poll_stop(&m_PollReq);
        if (ret < 0)
            PSG_ERROR("Stop polling error: " + string(uv_strerror(ret)));
    }
}


void CPSGS_SocketIOCallback::x_StartTimer(void)
{
    uv_loop_t * loop = CPubseqGatewayApp::GetInstance()->GetUVLoop();

    int     ret = uv_timer_init(loop, &m_TimerReq);
    if (ret < 0) {
        NCBI_THROW(CPubseqGatewayException, eInvalidTimerInit, uv_strerror(ret));
    }
    m_TimerReq.data = this;

    ret = uv_timer_start(&m_TimerReq, timer_socket_io_cb, m_TimerMillisec, 0);
    if (ret < 0) {
        NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart, uv_strerror(ret));
    }
    m_TimerActive = true;
}


void CPSGS_SocketIOCallback::x_StopTimer(void)
{
    if (m_TimerActive) {
        m_TimerActive = false;

        int     ret = uv_timer_stop(&m_TimerReq);
        if (ret < 0)
            PSG_ERROR("Stop timer error: " + string(uv_strerror(ret)));
    }
}


void CPSGS_SocketIOCallback::x_UvOnTimer(void)
{
    EPSGS_PollContinue  ret = m_TimeoutCB(m_UserData);
    if (ret == ePSGS_StopPolling) {
        x_StopPolling();
    } else {
        x_StartTimer();
    }
}


void CPSGS_SocketIOCallback::x_UvOnSocketEvent(int  status)
{
    x_StopTimer();

    EPSGS_PollContinue  ret;
    if (status < 0) {
        // That's an error
        string      msg(uv_strerror(status));
        ret = m_ErrorCB(msg, m_UserData);
    } else {
        // OK event
        ret = m_EventCB(m_UserData);
    }

    if (ret == ePSGS_StopPolling) {
        x_StopPolling();
    } else {
        x_StartTimer();
    }
}

