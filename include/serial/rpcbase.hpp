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

#include <connect/ncbi_conn_stream.hpp>
#include <corelib/ncbimtx.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// CRPCClient -- prototype client for ASN.1/XML-based RPC.
/// Normally connects automatically on the first real request and
/// disconnects automatically in the destructor, but allows both events
/// to occur explicitly.

template <class TRequest, class TReply>
class CRPCClient : public CObject
{
public:
    CRPCClient(const string&     service     = kEmptyStr,
               ESerialDataFormat format      = eSerial_AsnBinary,
               unsigned int      retry_limit = 3)
        : m_Service(service), m_Format(format), m_RetryLimit(retry_limit)
        { }
    virtual ~CRPCClient(void) { Disconnect(); }

    virtual void Ask(const TRequest& request, TReply& reply);
            void Connect(void);
            void Disconnect(void);
            void Reset(void);
      EIO_Status SetTimeout(const STimeout* timeout,
                            EIO_Event direction = eIO_ReadWrite);

protected:
    /// These run with m_Mutex already acquired.
    virtual void x_Connect(void);
    virtual void x_Disconnect(void);
            void x_SetStream(CNcbiIostream* stream);

    /// Retry policy; by default, just _TRACEs the event and returns
    /// true.  May reset the connection (or do anything else, really),
    /// but note that Ask will already automatically reconnect if the
    /// stream is explicitly bad.  (Ask also takes care of enforcing
    /// m_RetryLimit.)
    virtual bool x_ShouldRetry(unsigned int tries);

private:
    typedef CRPCClient<TRequest, TReply> TSelf;
    /// Prohibit default copy constructor and assignment operator.
    CRPCClient(const TSelf& x);
    bool operator= (const TSelf& x);

    auto_ptr<CNcbiIostream>  m_Stream;
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    string                   m_Service; ///< Used by default Connect().
    ESerialDataFormat        m_Format;
    CMutex                   m_Mutex;   ///< To allow sharing across threads.

protected:
    unsigned int             m_RetryLimit;
};


///////////////////////////////////////////////////////////////////////////
// Inline methods


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
EIO_Status CRPCClient<TRequest, TReply>::SetTimeout(const STimeout* timeout,
                                                    EIO_Event direction)
{
    CConn_IOStream conn_stream = dynamic_cast<CConn_IOStream*>(m_Stream);
    if (conn_stream) {
        return CONN_SetTimeout(conn_stream.GetCONN(), direction, timeout);
    } else {
        return eIO_NotSupported;
    }
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Ask(const TRequest& request, TReply& reply)
{
    CMutexGuard LOCK(m_Mutex);
    
    for (unsigned int tries = 1;  tries <= m_RetryLimit;  ++tries) {
        try {
            Connect(); // No-op if already connected
            *m_Out << request;
            *m_In >> reply;
            break;
        } catch (CSerialException& e) {
            if ( !x_ShouldRetry(tries) ) {
                throw;
            }
        }
    }
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::x_Connect(void)
{
    _ASSERT( !m_Service.empty() );
    x_SetStream(new CConn_ServiceStream(m_Service));
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
    m_Stream.reset(stream);
    m_In .reset(CObjectIStream::Open(m_Format, *stream));
    m_Out.reset(CObjectOStream::Open(m_Format, *stream));
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


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/12/12 21:31:43  ucko
* Support (configurably) retrying requests that run into I/O errors.
* Partially doxygenize.
*
* Revision 1.2  2003/04/15 16:18:43  siyan
* Added doxygen support
*
* Revision 1.1  2002/11/13 00:46:05  ucko
* Add RPC client generator; CVS logs to end in generate.?pp
*
* ===========================================================================
*/

#endif  /* SERIAL___RPCBASE__HPP */
