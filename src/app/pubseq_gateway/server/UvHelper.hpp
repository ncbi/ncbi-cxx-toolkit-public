#ifndef UV_HELPER__HPP
#define UV_HELPER__HPP

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
 */

#include <stdexcept>
#include <sstream>

#include <corelib/ncbimtx.hpp>
USING_NCBI_SCOPE;

#include "uv.h"

#include "pubseq_gateway_logging.hpp"

class CUvBaseException : public CException
{
public:
    enum EErrCode {
        eSignalAlreadyStarted,
        eSignalNotStarted,
        eTcpHandleClosed
    };

    const char* GetErrCodeString(void) const override;

    NCBI_EXCEPTION_DEFAULT(CUvBaseException, CException);
};

class CUvException : public CUvBaseException
{
public:
    enum EErrCode {
        eUvLoopInitFailure,
        eUvSignalInitFailure,
        eUvSignalStartFailure,
        eUvIp4AddrFailure,
        eUvTcpInitFailure,
        eUvTcpBindFailure,
        eUvSemInitFailure
    };

    CUvException(const CDiagCompileInfo& info,
                 const CException* prev_exception,
                 EErrCode err_code,
                 const string& message,
                 int uv_err_code,
                 EDiagSev severity = eDiag_Error) :
        CUvBaseException(info, prev_exception, (CUvBaseException::EErrCode) err_code,
                message + ": " + uv_strerror(uv_err_code), severity),
        m_UvErrCode(uv_err_code)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CUvException, CUvBaseException);

    const char* GetErrCodeString() const override;

private:
    int m_UvErrCode;
};

class CUvLoop {
private:
    uv_loop_t m_loop;
    bool m_initialized;
    static void s_walk(uv_handle_t* handle, void* arg) {
        std::function<void(uv_handle_t* handle)> *cb = static_cast<std::function<void(uv_handle_t* handle)>*>(arg);
        (*cb)(handle);
    }
public:
    CUvLoop() :
        m_loop({0}),
        m_initialized(false)
    {
        PSG_TRACE("CUvLoop::CUvLoop " << &m_loop);
        int rc = uv_loop_init(&m_loop);
        if (rc)
            NCBI_THROW2(CUvException, eUvLoopInitFailure, "uv_loop_init failed", rc);
        m_initialized = true;
    }
    ~CUvLoop() {
        PSG_TRACE("CUvLoop::~CUvLoop " << &m_loop);
        Close();
    }
    uv_loop_t* Handle() {
        return &m_loop;
    }
    int Close() {
        PSG_TRACE("CUvLoop::Close " << &m_loop);
        int rc = 0;
        if (m_initialized) {
            rc = uv_run(&m_loop, UV_RUN_DEFAULT);
            if (rc)
                PSG_TRACE("uv_run returned " << rc);
            rc = uv_loop_close(&m_loop);
            if (rc)
                PSG_TRACE("uv_loop_close returned " << rc);
            m_initialized = false;
        }
        return rc;
    }
    void Stop() {
        if (m_initialized) {
            PSG_TRACE("CUvLoop::Stop " << &m_loop);
            uv_stop(&m_loop);
        }
    }
    void Walk(std::function<void(uv_handle_t* handle)> cb) {
        if (m_initialized) {
            uv_walk(&m_loop, s_walk, &cb);
        }
    }
};

class CUvSignal {
private:
    uv_signal_t m_sig;
    bool m_initialized;
    bool m_started;
public:
    CUvSignal(uv_loop_t *loop) :
        m_sig({0}),
        m_initialized(false),
        m_started(false)
    {
        int rc;
        rc = uv_signal_init(loop, &m_sig);
        if (rc)
            NCBI_THROW2(CUvException, eUvSignalInitFailure, "uv_signal_init failed", rc);
        m_initialized = true;
    }
    ~CUvSignal() {
        Close();
    }
    void Start(int signum,  uv_signal_cb cb) {
        if (m_started)
            NCBI_THROW(CUvBaseException, eSignalAlreadyStarted, "signal has already started");
        int rc = uv_signal_start(&m_sig, cb, signum);
        if (rc)
            NCBI_THROW2(CUvException, eUvSignalStartFailure, "uv_signal_start failed", rc);
        m_started = true;
    }
    void Stop() {
        if (!m_started)
            NCBI_THROW(CUvBaseException, eSignalNotStarted, "signal hasn't started");
        uv_signal_stop(&m_sig);
        m_started = false;
    }
    void Close() {
        if (m_initialized) {
            if (m_started) {
                uv_signal_stop(&m_sig);
                m_started = false;
            }
            uv_close(reinterpret_cast<uv_handle_t*>(&m_sig), NULL);
            m_initialized = false;
        }
    }
};

class CUvTcp {
private:
    uv_tcp_t m_tcp;
    bool m_initialized;
    static void s_close_cb(uv_handle_t *handle) {
        CUvTcp *self = static_cast<CUvTcp*>(handle->data);
        self->m_initialized = false;
    }
    void InternalClose(void (*close_cb)(uv_handle_t* handle), bool from_dtor) {
        if (m_initialized) {
            PSG_TRACE("CUvTcp::Close " << &m_tcp);
            uv_handle_t *handle = reinterpret_cast<uv_handle_t*>(&m_tcp);
            if (from_dtor) {
                handle->data = this;
                uv_close(handle, s_close_cb);
                while (m_initialized) 
                    uv_run(handle->loop, UV_RUN_ONCE);
            }
            else {
                m_initialized = false;
                uv_close(handle, close_cb);
            }
        }
    }
public:
    CUvTcp(uv_loop_t *loop) :
        m_tcp({0}),
        m_initialized(false)
    {
        PSG_TRACE("CUvTcp::CUvTcp " << &m_tcp);
        Init(loop);
    }
    ~CUvTcp() {
        PSG_TRACE("CUvTcp::~CUvTcp " << &m_tcp);
        InternalClose(nullptr, true);
    }
    void Init(uv_loop_t *loop) {
        if (!m_initialized) {
            int rc;
            rc = uv_tcp_init(loop, &m_tcp);
            if (rc)
                NCBI_THROW2(CUvException, eUvTcpInitFailure, "uv_tcp_init failed", rc);
            m_initialized = true;
        }
    }
    bool Initialized() const {
        return m_initialized;
    }
    void NoDelay(bool set) {
        if (m_initialized) {
            uv_tcp_nodelay(&m_tcp, set ? 1 : 0);
        }
    }
    void KeepAlive(bool set) {
        if (m_initialized) {
            uv_tcp_keepalive(&m_tcp, set ? 1 : 0, 120);
        }
    }
    uv_tcp_t* Handle() {
        if (!m_initialized)
            NCBI_THROW(CUvBaseException, eTcpHandleClosed, "tcp handle is closed");
        return &m_tcp;
    }
    void Bind(const char* addr, unsigned int port) {
        int e;
        std::stringstream ss;
        struct sockaddr_in addr_in;

        e = uv_ip4_addr(addr, port, &addr_in);

        if (e != 0) {
            ss << (e == EINVAL ? "invalid" : "failed to parse") << " address/port: " << addr << ':' << port;
            NCBI_THROW2(CUvException, eUvIp4AddrFailure, ss.str(), e);
        }

        e = uv_tcp_bind(&m_tcp, reinterpret_cast<struct sockaddr*>(&addr_in), 0);
        if (e != 0 || errno == EADDRINUSE) {
            ss << "failed to bind socket to address/port: " << addr << ':' << port;
            NCBI_THROW2(CUvException, eUvTcpBindFailure, ss.str(), e);
        }
    }
    void Close(void (*close_cb)(uv_handle_t* handle)) {
        InternalClose(close_cb, false);
    }
    void StopRead() {
        if (m_initialized) {
            PSG_TRACE("CUvTcp::StopRead " << &m_tcp);
            uv_read_stop(reinterpret_cast<uv_stream_t*>(&m_tcp));
        }
    }
};

inline const char* CUvBaseException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
        case eSignalAlreadyStarted: return "eSignalAlreadyStarted";
        case eSignalNotStarted:     return "eSignalNotStarted";
        case eTcpHandleClosed:      return "eTcpHandleClosed";
        default:                    return CException::GetErrCodeString();
    }
}

inline const char* CUvException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eUvLoopInitFailure:    return "eUvLoopInitFailure";
        case eUvSignalInitFailure:  return "eUvSignalInitFailure";
        case eUvSignalStartFailure: return "eUvSignalStartFailure";
        case eUvIp4AddrFailure:     return "eUvIp4AddrFailure";
        case eUvTcpInitFailure:     return "eUvTcpInitFailure";
        case eUvTcpBindFailure:     return "eUvTcpBindFailure";
        case eUvSemInitFailure:     return "eUvSemInitFailure";
        default:                    return CException::GetErrCodeString();
    }
}

#endif
