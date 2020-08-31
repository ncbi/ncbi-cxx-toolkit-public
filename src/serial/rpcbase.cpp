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
#include <serial/rpcbase_impl.hpp>

BEGIN_NCBI_SCOPE


static string s_GetConfigString(const string& service,
                                const string& variable)
{
    if (service.empty() || variable.empty()) return kEmptyStr;

    string env_var = service + "__RPC_CLIENT__" + variable;
    NStr::ToUpper(env_var);
    const TXChar* str = NcbiSys_getenv(_T_XCSTRING(env_var.c_str()));

    if (str && *str) {
        return _T_CSTRING(str);
    }

    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app  &&  app->HasLoadedConfig()) {
        return app->GetConfig().Get(service + ".rpc_client", variable);
    }
    return kEmptyStr;
}


/*
[<service_name>.rpc_client]
max_retries = 1
retry_delay = 0.2
*/

static unsigned int s_GetRetryLimit(const string& service)
{
    string str = s_GetConfigString(service, "max_retries");
    if (!str.empty()) {
        try {
            unsigned int ret = NStr::StringToNumeric<unsigned int>(str);
            return ret;
        }
        catch (...) {
            ERR_POST(Warning << "Bad " << service << "/max_retries value: " << str);
        }
    }
    return 3;
}


static CTimeSpan s_GetRetryDelay(const string& service)
{
    CTimeSpan ret;
    string str = s_GetConfigString(service, "retry_delay");
    if (!str.empty()) {
        try {
            double sec = NStr::StringToNumeric<double>(str);
            return CTimeSpan(sec);
        }
        catch (...) {
            ERR_POST(Warning << "Bad " << service << "/retry_delay value: " << str);
        }
    }
    return ret;
}


CRPCClient_Base::CRPCClient_Base(const string&     service,
                                 ESerialDataFormat format)
    : m_Format(format),
      m_RetryDelay(s_GetRetryDelay(service)),
      m_RetryCount(0),
      m_RecursionCount(0),
      m_Service(service),
      m_RetryLimit(s_GetRetryLimit(service))
{
}


CRPCClient_Base::CRPCClient_Base(const string&     service,
    ESerialDataFormat format,
    unsigned int      retry_limit)
    : m_Format(format),
      m_RetryDelay(s_GetRetryDelay(service)),
      m_RetryCount(0),
      m_RecursionCount(0),
      m_Service(service),
      m_RetryLimit(retry_limit)
{
}


CRPCClient_Base::~CRPCClient_Base(void)
{
    try {
        Disconnect();
    } STD_CATCH_ALL_XX(Serial_RPCClient, 2, "CRPCClient_Base::Disconnect()");
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
            ERR_POST("Affinity cannot be changed on a recursive request");
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

    const string& request_name = request.GetThisTypeInfo() != NULL 
        ? ("("+request.GetThisTypeInfo()->GetName()+")")
        : "(no_request_type)";

    // Reset headers from previous requests if any.
    m_RetryCtx.Reset();
    double max_span = m_RetryDelay.GetAsDouble()*m_RetryLimit;
    double span = max_span;
    bool limit_by_time = !m_RetryDelay.IsEmpty();
    // Retry context can be either the default one (m_RetryCtx), or provided
    // through an exception.
    for (;;) {
        if ( IsCanceled() ) {
            NCBI_THROW(CRPCClientException, eFailed,
                       "Request canceled " + request_name);
        }
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
            if (!m_Stream->good()  &&  !m_Stream->eof()) {
                NCBI_THROW(CRPCClientException, eFailed,
                           "Connection stream is in bad state " + request_name);
            }
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
                       "Failed to receive reply after "
                       + NStr::NumericToString(m_RetryCount)
                       + (m_RetryCount == 1 ? " try " : " tries ")
                       + request_name );
        }
        if ( m_RetryCtx.IsSetStop() ) {
            NCBI_THROW(CRPCClientException, eFailed,
                       "Retrying request stopped by the server: "
                       + m_RetryCtx.GetStopReason() + ' ' + request_name);
        }
        CTimeSpan delay = x_GetRetryDelay(span);
        if ( !delay.IsEmpty() ) {
            SleepSec(delay.GetCompleteSeconds());
            SleepMicroSec(delay.GetNanoSecondsAfterSecond() / 1000);
            span -= delay.GetAsDouble();
            if (limit_by_time  &&  span <= 0) {
                NCBI_THROW(CRPCClientException, eFailed,
                           "Failed to receive reply in "
                           + CTimeSpan(max_span).AsSmartString()
                           + ' ' + request_name);
            }
        }
        // Always reconnect on retry.
        if ( IsCanceled() ) {
            NCBI_THROW(CRPCClientException, eFailed,
                       "Request canceled " + request_name);
        }
        try {
            Reset();
        } STD_CATCH_ALL_XX(Serial_RPCClient, 1,
                           "CRPCClient_Base::Reset() " + request_name);
    }
    // Reset retry context when done.
    m_RetryCtx.Reset();
    // If there were any retries, force disconnect to prevent using old
    // retry url, args etc. with the next request.
    if ( m_RetryCount > 0  &&  m_RecursionCount <= 1 ) {
        Disconnect();
    }
}


bool CRPCClient_Base::x_ShouldRetry(unsigned int tries) /* NCBI_FAKE_WARNING */
{
    _TRACE("CRPCClient_Base::x_ShouldRetry: retrying after " << tries
           << " failure(s)");
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


const char* CRPCClient_Base::GetContentTypeHeader(ESerialDataFormat format)
{
    switch (format) {
    case eSerial_None:
        break;
    case eSerial_AsnText:
        return "Content-Type: x-ncbi-data/x-asn-text\r\n";
    case eSerial_AsnBinary:
        return "Content-Type: x-ncbi-data/x-asn-binary\r\n";
    case eSerial_Xml:
        return "Content-Type: application/xml\r\n";
    case eSerial_Json:
        return "Content-Type: application/json\r\n";
    }
    return NULL; // kEmptyCStr?
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
