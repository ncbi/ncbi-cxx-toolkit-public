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

BEGIN_NCBI_SCOPE

// CRPCClient -- prototype client for ASN.1/XML-based RPC.
// Normally connects automatically on the first real request and
// disconnects automatically in the destructor, but allows both events
// to occur explicitly.

template <class TRequest, class TReply>
class CRPCClient : public CObject
{
public:
    CRPCClient(const string& service = kEmptyStr,
               ESerialDataFormat format = eSerial_AsnBinary)
        : m_Service(service), m_Format(format)
        { }
    virtual ~CRPCClient(void) { Disconnect(); }

    virtual void Ask(const TRequest& request, TReply& reply);
            void Connect(void);
            void Disconnect(void);
            void Reset(void) { Disconnect(); Connect(); }

protected:
    // These run with m_Mutex already acquired
    virtual void x_Connect(void);
    virtual void x_Disconnect(void);
            void x_SetStream(CNcbiIostream* stream);

private:
    typedef CRPCClient<TRequest, TReply> TSelf;
    // prohibit default copy constructor and assignment operator
    CRPCClient(const TSelf& x);
    bool operator= (const TSelf& x);

    auto_ptr<CNcbiIostream>  m_Stream;
    auto_ptr<CObjectIStream> m_In;
    auto_ptr<CObjectOStream> m_Out;
    string                   m_Service; // used by default Connect()
    ESerialDataFormat        m_Format;
    CMutex                   m_Mutex; // to allow sharing across threads
};


///////////////////////////////////////////////////////////////////////////
// Inline methods


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Connect(void)
{
    if (m_Stream.get()) {
        return; // already connected
    }
    CMutexGuard LOCK(m_Mutex);
    // repeat test with mutex held to avoid races
    if (m_Stream.get()) {
        return; // already connected
    }
    x_Connect();
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Disconnect(void)
{
    CMutexGuard LOCK(m_Mutex);
    if ( !m_Stream.get() ) {
        // not connected -- don't call x_Disconnect, which might
        // temporarily reconnect to send a fini!
        return;
    }
    x_Disconnect();
}


template <class TRequest, class TReply>
inline
void CRPCClient<TRequest, TReply>::Ask(const TRequest& request, TReply& reply)
{
    CMutexGuard LOCK(m_Mutex);
    Connect(); // No-op if already connected
    *m_Out << request;
    *m_In >> reply;
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


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2002/11/13 00:46:05  ucko
* Add RPC client generator; CVS logs to end in generate.?pp
*
*
* ===========================================================================
*/

#endif  /* SERIAL___RPCBASE__HPP */
