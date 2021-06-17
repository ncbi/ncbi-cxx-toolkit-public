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
#include <serial/rpcbase_impl.hpp>

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
                   public    CRPCClient_Base,
                   protected CConnIniter
{
public:
    CRPCClient(const string& service = kEmptyStr)
        : CRPCClient_Base(service, eSerial_AsnBinary),
          m_Timeout(kDefaultTimeout)
        {}
    CRPCClient(const string&     service,
        ESerialDataFormat        format)
        : CRPCClient_Base(service, format),
        m_Timeout(kDefaultTimeout)
    {}
    CRPCClient(const string& service,
        ESerialDataFormat    format,
        unsigned int         try_limit)
        : CRPCClient_Base(service, format, try_limit),
        m_Timeout(kDefaultTimeout)
    {}
    virtual ~CRPCClient(void)
    {
        if ( !sx_IsSpecial(m_Timeout) ) {
            delete const_cast<STimeout*>(m_Timeout);
        }
    }

    virtual void Ask(const TRequest& request, TReply& reply)
    { x_Ask(request, reply); }

    virtual void WriteRequest(CObjectOStream& out, const TRequest& request)
    { out << request; }

    virtual void ReadReply(CObjectIStream& in, TReply& reply)
    { in >> reply; }

    EIO_Status      SetTimeout(const STimeout* timeout,
                               EIO_Event direction = eIO_ReadWrite);
    const STimeout* GetTimeout(EIO_Event direction = eIO_Read) const;

    EIO_Status AsyncConnect(void* handle_buf, size_t handle_size);

protected:
    virtual string GetAffinity(const TRequest& /*request*/) const
    {
        return string();
    }

    virtual void x_WriteRequest(CObjectOStream& out, const CSerialObject& request) override
    {
        WriteRequest(out, dynamic_cast<const TRequest&>(request));
    }

    virtual void x_ReadReply(CObjectIStream& in, CSerialObject& reply) override
    {
        ReadReply(in, dynamic_cast<TReply&>(reply));
    }

    virtual string x_GetAffinity(const CSerialObject& request) const override
    {
        return GetAffinity(dynamic_cast<const TRequest&>(request));
    }

    virtual void x_Connect(void) override;

    /// Connect to a URL.  (Discouraged; please establish and use a
    /// suitable named service if possible.)
    void x_ConnectURL(const string& url);

    // CConn_HttpStream callback for parsing headers.
    // 'user_data' must point to an instance of CRPCConnStatus.
    static EHTTP_HeaderParse sx_ParseHeader(const char* http_header,
                                            void*       user_data,
                                            int         server_error);

    static bool sx_IsSpecial(const STimeout* timeout);

    const STimeout*          m_Timeout; ///< Cloned if not special.

private:
    void x_FillConnNetInfo(SConnNetInfo& net_info, SSERVICE_Extra* x_extra);

    unique_ptr<CConn_ServiceStream> m_AsyncStream;    
};


///////////////////////////////////////////////////////////////////////////
// Inline methods

template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


template<class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_FillConnNetInfo(SConnNetInfo& net_info,
                                                     SSERVICE_Extra* x_extra)
{
    if ( !m_Args.empty() ) {
        if ( !ConnNetInfo_AppendArg(&net_info, m_Args.c_str(), 0) ) {
            NCBI_THROW(CRPCClientException, eArgs,
                "Error sending additional request arguments");
        }
    }
    if ( m_RetryCtx.IsSetArgs() ) {
        if ( !ConnNetInfo_AppendArg(&net_info, m_RetryCtx.GetArgs().c_str(),
                                    0) ) {
            NCBI_THROW(CRPCClientException, eArgs,
                "Error sending retry context arguments");
        }
    } else if (x_extra != nullptr  &&  !m_Affinity.empty()) {
        if ( !ConnNetInfo_PostOverrideArg(&net_info, m_Affinity.c_str(), 0) ) {
            NCBI_THROW(CRPCClientException, eArgs,
                "Error sending request affinity");
        }
    }
    if (x_extra == nullptr) {
        return;
    }
    // Install callback for parsing headers.
    memset(x_extra, 0, sizeof(*x_extra));
    x_extra->data = &m_RetryCtx;
    x_extra->parse_header = sx_ParseHeader;
    x_extra->flags = fHTTP_NoAutoRetry;
    const char* user_header = GetContentTypeHeader(GetFormat());
    if (user_header != NULL  &&  *user_header != '\0') {
        if ( !ConnNetInfo_OverrideUserHeader(&net_info, user_header)) {
            NCBI_THROW(CRPCClientException, eArgs,
                "Error sending user header");
        }
    }
}

template<class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_Connect(void)
{
    if (m_AsyncStream.get() != nullptr) {
        m_AsyncStream->SetTimeout(eIO_Open, m_Timeout);
        m_AsyncStream->SetTimeout(eIO_ReadWrite, m_Timeout);
        x_SetStream(m_AsyncStream.release());
        return;
    } else if ( m_RetryCtx.IsSetUrl() ) {
        x_ConnectURL(m_RetryCtx.GetUrl());
        return;
    }
    _ASSERT( !m_Service.empty() );
    SSERVICE_Extra x_extra;
    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(m_Service.c_str()));
    x_FillConnNetInfo(*net_info, &x_extra);

    unique_ptr<CConn_ServiceStream> stream
        (new CConn_ServiceStream
         (m_Service, fSERV_Any | fSERV_DelayOpen, net_info.get(), &x_extra, m_Timeout));
    if ( m_Canceler.NotNull() ) {
        stream->SetCanceledCallback(m_Canceler.GetNonNullPointer());
    }
    x_SetStream(stream.release());
}


template<class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_ConnectURL(const string& url)
{
    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(nullptr));
    if ( !ConnNetInfo_ParseURL(net_info.get(), url.c_str()) ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Error parsing URL " + url);
    }
    x_FillConnNetInfo(*net_info, nullptr);
    unique_ptr<CConn_HttpStream> stream(new CConn_HttpStream(net_info.get(),
        GetContentTypeHeader(GetFormat()),
        sx_ParseHeader, // callback
        &m_RetryCtx,    // user data for the callback
        0, // adjust callback
        0, // cleanup callback
        fHTTP_AutoReconnect | fHTTP_NoAutoRetry,
        m_Timeout));
    if ( m_Canceler.NotNull() ) {
        stream->SetCanceledCallback(m_Canceler.GetNonNullPointer());
    }
    x_SetStream(stream.release());
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
    }
    else {
        return m_Timeout;
    }
}


template<class TRequest, class TReply>
inline
EIO_Status CRPCClient<TRequest, TReply>::AsyncConnect(void* handle_buf,
                                                      size_t handle_size)
{
    static const STimeout kZeroTimeout = { 0, 0 };
    _ASSERT( !m_Service.empty() );
    SSERVICE_Extra x_extra;
    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(m_Service.c_str()));
    x_FillConnNetInfo(*net_info, &x_extra);

    m_AsyncStream.reset(
        new CConn_ServiceStream(m_Service, fSERV_Any, net_info.get(), &x_extra,
                                &kZeroTimeout));
    if (m_Timeout == kDefaultTimeout) {
        m_Timeout = kInfiniteTimeout;
    }
    EIO_Status status = m_AsyncStream->Status();
    if ( m_Canceler.NotNull() ) {
        m_AsyncStream->SetCanceledCallback(m_Canceler.GetNonNullPointer());
    }
    if (handle_buf != nullptr) {
        CONN conn = m_AsyncStream->GetCONN();
        if (conn != nullptr) {
            SOCK sock = nullptr;
            if ((status = CONN_GetSOCK(conn, &sock)) == eIO_Success
                &&  sock != nullptr) {
                status = SOCK_GetOSHandle(sock, handle_buf, handle_size);
            }
        }
    }
    return status;
}


template<class TRequest, class TReply>
inline
EHTTP_HeaderParse
CRPCClient<TRequest, TReply>::sx_ParseHeader(const char* http_header,
                                             void*       user_data,
                                             int         /*server_error*/)
{
    if ( !user_data ) {
        return eHTTP_HeaderContinue;
    }
    CHttpRetryContext* retry_ctx = reinterpret_cast<CHttpRetryContext*>(user_data);
    _ASSERT(retry_ctx);
    retry_ctx->ParseHeader(http_header);

    // Always read response body - normal content or error.
    return eHTTP_HeaderContinue;
}


template<class TRequest, class TReply>
inline
bool CRPCClient<TRequest, TReply>::sx_IsSpecial(const STimeout* timeout)
{
    return timeout == kDefaultTimeout  ||  timeout == kInfiniteTimeout;
}


END_NCBI_SCOPE


/* @} */

#endif  /* SERIAL___RPCBASE__HPP */
