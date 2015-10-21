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
* Author: Aleksey Grichenko
*
* File Description:
*   CRPCClient helpers
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_cookies.hpp>
#include <serial/rpcbase.hpp>

BEGIN_NCBI_SCOPE


// HTTP headers controlling retries.

// Stop retries if set, value describes the reason.
const char* kRetryStop = "X-NCBI-Retry-Stop";
// Override retries delay. Use min of the header and m_RetryDelay.
const char* kRetryDelay = "X-NCBI-Retry-Delay";
// Add args to the request when sending next retry.
const char* kRetryArgs = "X-NCBI-Retry-Args";
// Use the specified URL for the next retry.
const char* kRetryURL = "X-NCBI-Retry-URL";
// Use different content when sending retry. The values are described below.
const char* kRetryContent = "X-NCBI-Retry-Content";
// Send no content whith the next retry.
const char* kRetryContent_no_content = "no_content";
// Send content from the last response when retrying.
const char* kRetryContent_from_response = "from_response";
// Send content following the prefix (URL-encoded).
const char* kRetryContent_content = "content:";


CRPCClient_Base::CRPCClient_Base(const string&     service,
                                 ESerialDataFormat format,
                                 unsigned int      retry_limit)
    : m_Format(format),
      m_RetryCount(0),
      m_RecursionCount(0),
      m_Service(service),
      m_Timeout(kDefaultTimeout),
      m_RetryLimit(retry_limit)
{
}


CRPCClient_Base::~CRPCClient_Base(void)
{
    Disconnect();
    if ( !sx_IsSpecial(m_Timeout) ) {
        delete const_cast<STimeout*>(m_Timeout);
    }
}


void CRPCClient_Base::Connect(void)
{
    // Do not connect from recursive requests - this must be done
    // by the main request only.
    if (m_RecursionCount > 1) return;

    if (m_Stream.get()  &&  m_Stream->good()) {
        return; // already connected
    }
    CMutexGuard LOCK(m_Mutex);
    // repeat test with mutex held to avoid races
    if (m_Stream.get()  &&  m_Stream->good()) {
        return; // already connected
    }
    x_Connect();
    m_RetryCtx.ResetNeedReconnect();
}


void CRPCClient_Base::Disconnect(void)
{
    CMutexGuard LOCK(m_Mutex);
    if ( !m_Stream.get()  ||  !m_Stream->good() ) {
        // not connected -- don't call x_Disconnect, which might
        // temporarily reconnect to send a fini!
        return;
    }
    x_Disconnect();
}


void CRPCClient_Base::Reset(void)
{
    CMutexGuard LOCK(m_Mutex);
    if (m_Stream.get()  &&  m_Stream->good()) {
        x_Disconnect();
    }
    x_Connect();
}


void CRPCClient_Base::SetAffinity(const string& affinity)
{
    if (m_Affinity != affinity) {
        if (m_RecursionCount > 1) {
            ERR_POST("Affinity can not be changed on a recursive request");
            return;
        }
        Disconnect();
        m_Affinity  = affinity;
    }
}


void CRPCClient_Base::x_Disconnect(void)
{
    m_In.reset();
    m_Out.reset();
    m_Stream.reset();
}


void CRPCClient_Base::x_SetStream(CNcbiIostream* stream)
{
    m_In .reset();
    m_Out.reset();
    m_Stream.reset(stream);
    m_In .reset(CObjectIStream::Open(m_Format, *stream));
    m_Out.reset(CObjectOStream::Open(m_Format, *stream));
}


class CCounterGuard
{
public:
    CCounterGuard(int* counter)
        : m_Counter(*counter)
    {

        m_Counter++;
    }

    ~CCounterGuard(void)
    {
        m_Counter--;
    }

private:
    int& m_Counter;
};

void CRPCClient_Base::x_Ask(const CSerialObject& request, CSerialObject& reply)
{
    CMutexGuard LOCK(m_Mutex);
    if (m_RecursionCount == 0) {
        m_RetryCount = 0;
    }
    // Recursion counter needs to be decremented on both success and failure.
    CCounterGuard recursion_guard(&m_RecursionCount);

    // Reset headers from previous requests if any.
    m_RetryCtx.Reset();
    double max_span = m_RetryDelay.GetAsDouble()*m_RetryLimit;
    double span = max_span;
    bool limit_by_time = !m_RetryDelay.IsEmpty();
    // Retry context can be either the default one (m_RetryCtx), or provided
    // through an exception.
    for (;;) {
        try {
            SetAffinity(x_GetAffinity(request));
            Connect(); // No-op if already connected
            if ( m_RetryCtx.IsSetContentOverride() ) {
                if (m_RetryCtx.GetContentOverride() != CHttpRetryContext::eNoContent  &&
                    m_RetryCtx.IsSetContent() ) {
                    *m_Stream << m_RetryCtx.GetContent();
                }
            }
            else {
                // by default re-send the original request
                x_WriteRequest(*m_Out, request);
            }
            m_Stream->peek(); // send data, read response headers
            if (m_RetryCtx.IsSetContentOverride()  &&
                m_RetryCtx.GetContentOverride() == CHttpRetryContext::eFromResponse) {
                // store response content to send it with the next retry
                CNcbiOstrstream buf;
                NcbiStreamCopy(buf, *m_Stream);
                m_RetryCtx.SetContent(CNcbiOstrstreamToString(buf));
            }
            else {
                // read normal response
                x_ReadReply(*m_In, reply);
            }
            // If reading reply succeeded and no retry was requested by the server, break.
            if ( !m_RetryCtx.GetNeedRetry() ) {
                break;
            }
        } catch (CException& e) {
            // Some exceptions tend to correspond to transient glitches;
            // the remainder, however, may as well get propagated immediately.
            CRPCClientException* rpc_ex = dynamic_cast<CRPCClientException*>(&e);
            if (rpc_ex  &&  rpc_ex->GetErrCode() == CRPCClientException::eRetry) {
                if ( rpc_ex->IsSetRetryContext() ) {
                    // Save information to the local retry context and proceed.
                    m_RetryCtx = rpc_ex->GetRetryContext();
                }
                // proceed to retry
            }
            else if ( !dynamic_cast<CSerialException*>(&e)
                &&  !dynamic_cast<CIOException*>(&e) ) {
                // Not a retry related exception, abort.
                throw;
            }
        }
        // No retries for recursive requests (e.g. AskInit called by Connect).
        // Exit immediately, do not reset retry context - it may be used by
        // the main request's retry loop.
        if (m_RecursionCount > 1) return;

        // Retry request on exception or on explicit retry request from the server.

        // If using time limit, allow to make more than m_RetryLimit attempts
        // if the server has set shorter delay.
        if ((!limit_by_time  &&  ++m_RetryCount >= m_RetryLimit)  ||
            !x_ShouldRetry(m_RetryCount)) {
            NCBI_THROW(CRPCClientException, eFailed,
                "Failed to receive reply after " +
                NStr::NumericToString(m_RetryCount) + " tries");
        }
        if ( m_RetryCtx.IsSetStop() ) {
            NCBI_THROW(CRPCClientException, eFailed,
                "Retrying request stopped by the server: " +
                m_RetryCtx.GetStopReason());
        }
        CTimeSpan delay = x_GetRetryDelay(span);
        if ( !delay.IsEmpty() ) {
            SleepSec(delay.GetCompleteSeconds());
            SleepMicroSec(delay.GetNanoSecondsAfterSecond() / 1000);
            span -= delay.GetAsDouble();
            if (limit_by_time  &&  span <= 0) {
                NCBI_THROW(CRPCClientException, eFailed,
                    "Failed to receive reply in " +
                    CTimeSpan(max_span).AsSmartString());
            }
        }
        // Do not try to force reconnect in recursive calls.
        if ( !(m_RetryCount & 1)  ||  m_RetryCtx.NeedReconnect() ) {
            // reset on every other attempt in case we're out of sync
            try {
                Reset();
            } STD_CATCH_ALL_XX(Serial_RPCClient, 1 ,"CRPCClient_Base::Reset()");
        }
    }
    // Reset retry context when done.
    m_RetryCtx.Reset();
}


bool CRPCClient_Base::x_ShouldRetry(unsigned int tries)
{
    _TRACE("CRPCClient_Base::x_ShouldRetry: retrying after " << tries
           << " failures");
    return true;
}


CTimeSpan CRPCClient_Base::x_GetRetryDelay(double max_delay) const
{
    // If not set by the server, use local delay.
    if ( !m_RetryCtx.IsSetDelay() ) {
        return m_RetryDelay;
    }
    // If local delay is not zero, we have to limit total retries time to max_delay.
    if (!m_RetryDelay.IsEmpty()  &&
        m_RetryCtx.GetDelay().GetAsDouble() > max_delay) {
        return CTimeSpan(max_delay);
    }
    return m_RetryCtx.GetDelay();
}


EHTTP_HeaderParse
CRPCClient_Base::sx_ParseHeader(const char* http_header,
                                void*       user_data,
                                int         server_error)
{
    if ( !user_data ) return eHTTP_HeaderContinue;
    CHttpRetryContext* retry_ctx = reinterpret_cast<CHttpRetryContext*>(user_data);
    _ASSERT(retry_ctx);
    retry_ctx->ParseHeader(http_header);

    // Always read response body - normal content or error.
    return eHTTP_HeaderContinue;
}


bool CRPCClient_Base::sx_IsSpecial(const STimeout* timeout)
{
    return timeout == kDefaultTimeout  ||  timeout == kInfiniteTimeout;
}


const char* CRPCClientException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eRetry:  return "eRetry";
    case eFailed: return "eFailed";
    case eArgs:   return "eArgs";
    case eOther:  return "eOther";
    default:      return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
