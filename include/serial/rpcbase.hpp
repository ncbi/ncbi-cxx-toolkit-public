#ifndef SERIAL___RPCBASE__HPP
#define SERIAL___RPCBASE__HPP

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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   Generic template class for ASN.1/XML RPC clients
 *
 */

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_util.h>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// CRPCClient -- prototype client for ASN.1/XML-based RPC.
/// Normally connects automatically on the first real request and
/// disconnects automatically in the destructor, but allows both events
/// to occur explicitly.

template <class TRequest, class TReply>
class CRPCClient : public    CObject,
                   protected CConnIniter
{
public:
    CRPCClient(const string&     service     = kEmptyStr,
               ESerialDataFormat format      = eSerial_AsnBinary,
               unsigned int      retry_limit = 3)
        : m_Service(service), m_Format(format), m_Timeout(kDefaultTimeout),
          m_RetryLimit(retry_limit)
    {
#if NCBI_DEVELOPMENT_VER < 20121104
        const char* sid = CORE_GetNcbiSid();
        if (sid  &&  *sid) {
            m_SessionID.assign(sid, strlen(sid));
        }
#endif
    }
    virtual ~CRPCClient(void);

    virtual void Ask(const TRequest& request, TReply& reply);
            void Connect(void);
            void Disconnect(void);
            void Reset(void);

    const string& GetService(void) const            { return m_Service; }
             void SetService(const string& service) { m_Service = service; }

    ESerialDataFormat GetFormat(void) const            { return m_Format; }
                 void SetFormat(ESerialDataFormat fmt) { m_Format = fmt; }

    unsigned int GetRetryLimit(void) const     { return m_RetryLimit; }
            void SetRetryLimit(unsigned int n) { m_RetryLimit = n; }

    const CTimeSpan GetRetryDelay(void) const          { return m_RetryDelay; }
    void            SetRetryDelay(const CTimeSpan& ts) { m_RetryDelay = ts; }

    EIO_Status      SetTimeout(const STimeout* timeout,
                               EIO_Event direction = eIO_ReadWrite);
    const STimeout* GetTimeout(EIO_Event direction = eIO_Read) const;

protected:
    virtual string GetAffinity(const TRequest& request) const;
              void SetAffinity(const string& affinity);

    /// These run with m_Mutex already acquired.
    virtual void x_Connect(void);
    virtual void x_Disconnect(void);
            void x_SetStream(CNcbiIostream* stream);
    /// Connect to a URL.  (Discouraged; please establish and use a
    /// suitable named service if possible.)
            void x_ConnectURL(const string& url);

    /// Retry policy; by default, just _TRACEs the event and returns
    /// true.  May reset the connection (or do anything else, really),
    /// but note that Ask() will always automatically reconnect if the
    /// stream is explicitly bad.  (Ask() also takes care of enforcing
    /// m_RetryLimit.)
    virtual bool x_ShouldRetry(unsigned int tries);

private:
    static bool x_IsSpecial(const STimeout* timeout)
        { return timeout == kDefaultTimeout  ||  timeout == kInfiniteTimeout; }

    typedef CRPCClient<TRequest, TReply> TSelf;
    /// Prohibit default copy constructor and assignment operator.
    CRPCClient(const TSelf& x);
    bool operator= (const TSelf& x);

    auto_ptr<CNcbiIostream>  m_Stream;
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    string                   m_Service; ///< Used by default Connect().
    string                   m_Affinity;
    string                   m_SessionID;
    ESerialDataFormat        m_Format;
    CMutex                   m_Mutex;   ///< To allow sharing across threads.
    const STimeout*          m_Timeout; ///< Cloned if not special.
    CTimeSpan                m_RetryDelay;

protected:
    unsigned int             m_RetryLimit;
};


///////////////////////////////////////////////////////////////////////////
// Inline methods


template <class TRequest, class TReply>
inline
CRPCClient<TRequest, TReply>::~CRPCClient(void)
{
    Disconnect();
    if ( !x_IsSpecial(m_Timeout) ) {
        delete const_cast<STimeout*>(m_Timeout);
    }
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Connect(void)
{
    if (m_Stream.get()  &&  m_Stream->good()) {
        return; // already connected
    }
    CMutexGuard LOCK(m_Mutex);
    // repeat test with mutex held to avoid races
    if (m_Stream.get()  &&  m_Stream->good()) {
        return; // already connected
    }
    x_Connect();
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Disconnect(void)
{
    CMutexGuard LOCK(m_Mutex);
    if ( !m_Stream.get()  ||  !m_Stream->good() ) {
        // not connected -- don't call x_Disconnect, which might
        // temporarily reconnect to send a fini!
        return;
    }
    x_Disconnect();
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Reset(void)
{
    CMutexGuard LOCK(m_Mutex);
    if (m_Stream.get()  &&  m_Stream->good()) {
        x_Disconnect();
    }
    x_Connect();
}


template <class TRequest, class TReply>
inline
string CRPCClient<TRequest, TReply>::GetAffinity(const TRequest& ) const
{
    return kEmptyStr;
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::SetAffinity(const string& affinity)
{
    if (m_Affinity != affinity) {
        Disconnect();
        m_Affinity  = affinity;
    }
}


template <class TRequest, class TReply>
inline
EIO_Status CRPCClient<TRequest, TReply>::SetTimeout(const STimeout* timeout,
                                                    EIO_Event direction)
{
    // save for future use, especially if there's no stream at present.
    {{
        const STimeout* old_timeout = m_Timeout;
        if (x_IsSpecial(timeout)) {
            m_Timeout = timeout;
        } else { // make a copy
            m_Timeout = new STimeout(*timeout);
        }
        if ( !x_IsSpecial(old_timeout) ) {
            delete const_cast<STimeout*>(old_timeout);
        }
    }}

    CConn_IOStream* conn_stream
        = dynamic_cast<CConn_IOStream*>(m_Stream.get());
    if (conn_stream) {
        return conn_stream->SetTimeout(direction, timeout);
    } else if ( !m_Stream.get() ) {
        return eIO_Success; // we've saved it, which is the best we can do...
    } else {
        return eIO_NotSupported;
    }
}


template <class TRequest, class TReply>
inline
const STimeout* CRPCClient<TRequest, TReply>::GetTimeout(EIO_Event direction)
    const
{
    CConn_IOStream* conn_stream
        = dynamic_cast<CConn_IOStream*>(m_Stream.get());
    if (conn_stream) {
        return conn_stream->GetTimeout(direction);
    } else {
        return m_Timeout;
    }
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Ask(const TRequest& request, TReply& reply)
{
    CMutexGuard LOCK(m_Mutex);
    
    unsigned int tries = 0;
    for (;;) {
        try {
            SetAffinity(GetAffinity(request));
            Connect(); // No-op if already connected
            *m_Out << request;
            *m_In >> reply;
            break;
        } catch (CException& e) {
            // Some exceptions tend to correspond to transient glitches;
            // the remainder, however, may as well get propagated immediately.
            if ( !dynamic_cast<CSerialException*>(&e)
                 &&  !dynamic_cast<CIOException*>(&e) ) {
                throw;
            } else if (++tries == m_RetryLimit  ||  !x_ShouldRetry(tries) ) {
                throw;
            } else if ( !(tries & 1) ) {
                // reset on every other attempt in case we're out of sync
                try {
                    Reset();
                } STD_CATCH_ALL_XX(Serial_RPCClient, 1, "CRPCClient<>::Reset()")
            }
            SleepSec(m_RetryDelay.GetCompleteSeconds());
            SleepMicroSec(m_RetryDelay.GetNanoSecondsAfterSecond() / 1000);
        }
    }
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_Connect(void)
{
    _ASSERT( !m_Service.empty() );
    SConnNetInfo* net_info = ConnNetInfo_Create(m_Service.c_str());
    if (!m_SessionID.empty()) {
        string header = "Cookie: ncbi_sid=" + m_SessionID;
        ConnNetInfo_AppendUserHeader(net_info, header.c_str());
    }
    if (!m_Affinity.empty()) {
        ConnNetInfo_PostOverrideArg(net_info, m_Affinity.c_str(), 0);
    }
    x_SetStream(new CConn_ServiceStream(m_Service, fSERV_Any, net_info, 0,
                                        m_Timeout));
    ConnNetInfo_Destroy(net_info);
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_Disconnect(void)
{
    m_In.reset();
    m_Out.reset();
    m_Stream.reset();
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_SetStream(CNcbiIostream* stream)
{
    m_In .reset();
    m_Out.reset();
    m_Stream.reset(stream);
    m_In .reset(CObjectIStream::Open(m_Format, *stream));
    m_Out.reset(CObjectOStream::Open(m_Format, *stream));
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_ConnectURL(const string& url)
{
    x_SetStream(new CConn_HttpStream(url, fHCC_AutoReconnect, m_Timeout));
}


template <class TRequest, class TReply>
inline
bool CRPCClient<TRequest, TReply>::x_ShouldRetry(unsigned int tries)
{
    _TRACE("CRPCClient<>::x_ShouldRetry: retrying after " << tries
           << " failures");
    return true;
}


END_NCBI_SCOPE


/* @} */

#endif  /* SERIAL___RPCBASE__HPP */
