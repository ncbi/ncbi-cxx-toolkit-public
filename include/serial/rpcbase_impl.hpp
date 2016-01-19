#ifndef SERIAL___RPCBASE_IMPL__HPP
#define SERIAL___RPCBASE_IMPL__HPP

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

    /// Set additional connection arguments.
    void SetArgs(const string& args) { m_Args = args; }
    void SetArgs(const CUrlArgs& args) { m_Args = args.GetQueryString(CUrlArgs::eAmp_Char); }
    /// Get additional connection arguments.
    const string& GetArgs(void) const { return m_Args; }

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
    unsigned int             m_RetryCount;
    int                      m_RecursionCount;

protected:
    string                   m_Service; ///< Used by default Connect().
    string                   m_Args;
    auto_ptr<CNcbiIostream>  m_Stream; // This must be destroyed after m_In/m_Out.
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    string                   m_Affinity;
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
};


class NCBI_XSERIAL_EXPORT CRPCClientException : public CException
{
public:
    enum EErrCode {
        eRetry,   ///< Request failed, should be retried if possible.
        eFailed,  ///< Request (or retry) failed.
        eArgs,    ///< Failed to send request arguments.
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


END_NCBI_SCOPE


/* @} */

#endif  /* SERIAL___RPCBASE_IMPL__HPP */
