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
#include <util/retry_ctx.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Base class for CRPCClient template - defines methods
/// independent of request and response types.
class NCBI_XSERIAL_EXPORT CRPCClient_Base
{
public:
    CRPCClient_Base(const string&     service,
                    ESerialDataFormat format,
                    unsigned int      retry_limit);
    virtual ~CRPCClient_Base(void);

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

protected:
    void SetAffinity(const string& affinity);

    /// These run with m_Mutex already acquired.
    virtual void x_Connect(void) = 0;
    virtual void x_Disconnect(void);
            void x_SetStream(CNcbiIostream* stream);

    void x_Ask(const CSerialObject& request, CSerialObject& reply);
    // Casting stubs.
    virtual void x_WriteRequest(CObjectOStream& out, const CSerialObject& request) = 0;
    virtual void x_ReadReply(CObjectIStream& in, CSerialObject& reply) = 0;
    virtual string x_GetAffinity(const CSerialObject& request) const = 0;

private:
    /// Prohibit default copy constructor and assignment operator.
    CRPCClient_Base(const CRPCClient_Base&);
    bool operator= (const CRPCClient_Base&);

    ESerialDataFormat        m_Format;
    CMutex                   m_Mutex;   ///< To allow sharing across threads.
    CTimeSpan                m_RetryDelay;
    int                      m_RetryCount;
    int                      m_RecursionCount;

protected:
    string                   m_Service; ///< Used by default Connect().
    auto_ptr<CNcbiIostream>  m_Stream; // This must be destroyed after m_In/m_Out.
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    string                   m_Affinity;
    const STimeout*          m_Timeout; ///< Cloned if not special.
    unsigned int             m_RetryLimit;
    CHttpRetryContext        m_RetryCtx;

    // Retry policy; by default, just _TRACEs the event and returns
    // true.  May reset the connection (or do anything else, really),
    // but note that Ask() will always automatically reconnect if the
    // stream is explicitly bad.  (Ask() also takes care of enforcing
    // m_RetryLimit.)
    virtual bool x_ShouldRetry(unsigned int tries);

    // Calculate effective retry delay. Returns value from CRetryContext
    // if any, or the value set by SetRetryDelay. The returned value never
    // exceeds max_delay.
    CTimeSpan x_GetRetryDelay(double max_delay) const;

    // CConn_HttpStream callback for parsing headers.
    // 'user_data' must point to an instance of CRPCConnStatus.
    static EHTTP_HeaderParse sx_ParseHeader(const char* http_header,
                                            void*       user_data,
                                            int         server_error);

    static bool sx_IsSpecial(const STimeout* timeout);
};


/// CRPCClient -- prototype client for ASN.1/XML-based RPC.
/// Normally connects automatically on the first real request and
/// disconnects automatically in the destructor, but allows both events
/// to occur explicitly.

template <class TRequest, class TReply>
class CRPCClient : public    CObject,
                   public    CRPCClient_Base,
                   protected CConnIniter
{
public:
    CRPCClient(const string&     service     = kEmptyStr,
               ESerialDataFormat format      = eSerial_AsnBinary,
               unsigned int      retry_limit = 3)
        : CRPCClient_Base(service, format, retry_limit) {}
    virtual ~CRPCClient(void) {}

    virtual void Ask(const TRequest& request, TReply& reply)
    { x_Ask(request, reply); }

    virtual void WriteRequest(CObjectOStream& out, const TRequest& request)
    { out << request; }

    virtual void ReadReply(CObjectIStream& in, TReply& reply)
    { in >> reply; }

    EIO_Status      SetTimeout(const STimeout* timeout,
                               EIO_Event direction = eIO_ReadWrite);
    const STimeout* GetTimeout(EIO_Event direction = eIO_Read) const;

protected:
    virtual string GetAffinity(const TRequest& request) const
    {
        return "";
    }

    virtual void x_WriteRequest(CObjectOStream& out, const CSerialObject& request)
    {
        WriteRequest(out, dynamic_cast<const TRequest&>(request));
    }

    virtual void x_ReadReply(CObjectIStream& in, CSerialObject& reply)
    {
        ReadReply(in, dynamic_cast<TReply&>(reply));
    }

    virtual string x_GetAffinity(const CSerialObject& request) const
    {
        return GetAffinity(dynamic_cast<const TRequest&>(request));
    }

    virtual void x_Connect(void);

    /// Connect to a URL.  (Discouraged; please establish and use a
    /// suitable named service if possible.)
            void x_ConnectURL(const string& url);
};


class NCBI_XSERIAL_EXPORT CRPCClientException : public CException
{
public:
    enum EErrCode {
        eRetry,   ///< Request failed, should be retried if possible.
        eFailed,  ///< Request (or retry) failed.
        eOther
    };

    virtual const char* GetErrCodeString(void) const;

    bool IsSetRetryContext(void) const { return m_RetryCtx; }
    /// Read retry related data.
    CRetryContext& GetRetryContext(void) { return *m_RetryCtx; }
    /// Set new retry context.
    void SetRetryContext(CRetryContext& ctx) { m_RetryCtx.Reset(&ctx); }

    NCBI_EXCEPTION_DEFAULT(CRPCClientException, CException);

private:
    CRef<CRetryContext> m_RetryCtx;
};


///////////////////////////////////////////////////////////////////////////
// Inline methods

template<class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_Connect(void)
{
    if ( m_RetryCtx.IsSetUrl() ) {
        x_ConnectURL(m_RetryCtx.GetUrl());
        return;
    }
    _ASSERT( !m_Service.empty() );
    SConnNetInfo* net_info = ConnNetInfo_Create(m_Service.c_str());
    if ( m_RetryCtx.IsSetArgs() ) {
        ConnNetInfo_AppendArg(net_info, m_RetryCtx.GetArgs().c_str(), 0);
    }
    else if (!m_Affinity.empty()) {
        ConnNetInfo_PostOverrideArg(net_info, m_Affinity.c_str(), 0);
    }

    // Install callback for parsing headers.
    SSERVICE_Extra x_extra;
    memset(&x_extra, 0, sizeof(x_extra));
    x_extra.data = &m_RetryCtx;
    x_extra.parse_header = sx_ParseHeader;

    x_SetStream(new CConn_ServiceStream(m_Service, fSERV_Any, net_info,
        &x_extra, m_Timeout));
    ConnNetInfo_Destroy(net_info);
}


template<class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_ConnectURL(const string& url)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    ConnNetInfo_ParseURL(net_info, url.c_str());
    if ( m_RetryCtx.IsSetArgs() ) {
        ConnNetInfo_PostOverrideArg(net_info, m_RetryCtx.GetArgs().c_str(), 0);
    }
    x_SetStream(new CConn_HttpStream(net_info,
        kEmptyStr, // user_header
        sx_ParseHeader, // callback
        &m_RetryCtx,    // user data for the callback
        0, // adjust callback
        0, // cleanup callback
        fHTTP_AutoReconnect,
        m_Timeout));
}


template<class TRequest, class TReply>
inline
EIO_Status CRPCClient<TRequest, TReply>::SetTimeout(const STimeout* timeout,
                                                    EIO_Event direction)
{
    // save for future use, especially if there's no stream at present.
    {{
        const STimeout* old_timeout = m_Timeout;
        if (sx_IsSpecial(timeout)) {
            m_Timeout = timeout;
        } else { // make a copy
            m_Timeout = new STimeout(*timeout);
        }
        if ( !sx_IsSpecial(old_timeout) ) {
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


template<class TRequest, class TReply>
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


END_NCBI_SCOPE


/* @} */

#endif  /* SERIAL___RPCBASE__HPP */
