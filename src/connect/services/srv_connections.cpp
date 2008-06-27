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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/srv_connections.hpp>
#include <connect/services/netservice_params.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <corelib/ncbi_system.hpp>


BEGIN_NCBI_SCOPE


struct SNetServerConnectionImpl
{
    SNetServerConnectionImpl(CNetServerConnectionPool* pool);

    void AddRef();
    void Release();

    bool IsConnected();
    void SetDisconnectedState();

    void Close();
    void Abort();

    ~SNetServerConnectionImpl();

    CAtomicCounter m_Refs;

    CNetServerConnectionPool* m_ConnectionPool;
    SNetServerConnectionImpl* m_NextFree;

    CSocket m_Socket;
    bool m_Connected;
};

inline SNetServerConnectionImpl::SNetServerConnectionImpl(
    CNetServerConnectionPool* pool) :
        m_ConnectionPool(pool),
        m_NextFree(NULL),
        m_Connected(false)
{
    m_Refs.Set(0);
    if (TServConn_UserLinger2::GetDefault()) {
        STimeout zero = {0,0};
        m_Socket.SetTimeout(eIO_Close,&zero);
    }
}

inline void SNetServerConnectionImpl::AddRef()
{
    m_Refs.Add(1);
}

inline void SNetServerConnectionImpl::Release()
{
    if (m_Refs.Add(-1) == 0)
        m_ConnectionPool->Put(this);
}

bool SNetServerConnectionImpl::IsConnected()
{
    EIO_Status st = m_Socket.GetStatus(eIO_Open);
    if (st != eIO_Success)
        return false;

    STimeout zero = {0, 0}, tmo;
    const STimeout* tmp = m_Socket.GetTimeout(eIO_Read);
    if (tmp) {
        tmo = *tmp;
        tmp = &tmo;
    }
    if (m_Socket.SetTimeout(eIO_Read, &zero) != eIO_Success)
        return false;

    st = m_Socket.Read(0,1,0,eIO_ReadPeek);
    bool ret = false;
    switch (st) {
         case eIO_Closed:
            m_Socket.Close();
            return false;
         case eIO_Success:
            if (m_Socket.GetStatus(eIO_Read) != eIO_Success) {
                m_Socket.Close();
                break;
            }
         case eIO_Timeout:
            ret = true;
         default:
            break;
    }

    if (m_Socket.SetTimeout(eIO_Read, tmp) != eIO_Success)
        return false;

    return ret;
}

void SNetServerConnectionImpl::SetDisconnectedState()
{
    if (m_Connected) {
        INetServerConnectionListener* listener =
            m_ConnectionPool->GetEventListener();

        if (listener)
            listener->OnDisconnected(m_ConnectionPool);

        m_Connected = false;
    }
}

void SNetServerConnectionImpl::Close()
{
    if (IsConnected())
        m_Socket.Close();

    SetDisconnectedState();
}

void SNetServerConnectionImpl::Abort()
{
    if (IsConnected())
        m_Socket.Abort();

    SetDisconnectedState();
}

SNetServerConnectionImpl::~SNetServerConnectionImpl()
{
    try {
        Close();
    } catch (...) {
    }
}


CNetServerConnection::CNetServerConnection(SNetServerConnectionImpl* impl) :
    m_ConnectionImpl(impl)
{
    m_ConnectionImpl->AddRef();
}

CNetServerConnection::CNetServerConnection(const CNetServerConnection& src)
{
    (m_ConnectionImpl = src.m_ConnectionImpl)->AddRef();
}

CNetServerConnection& CNetServerConnection::operator =(
    const CNetServerConnection& src)
{
    src.m_ConnectionImpl->AddRef();
    m_ConnectionImpl->Release();
    m_ConnectionImpl = src.m_ConnectionImpl;
    return *this;
}

CNetServerConnection::~CNetServerConnection()
{
    m_ConnectionImpl->Release();
}

bool CNetServerConnection::ReadStr(string& str)
{
    if (!m_ConnectionImpl->IsConnected())
        return false;

    EIO_Status io_st = m_ConnectionImpl->m_Socket.ReadLine(str);
    switch (io_st)
    {
    case eIO_Success:
        return true;
    case eIO_Timeout:
        m_ConnectionImpl->Abort();
        NCBI_THROW(CNetSrvConnException, eReadTimeout,
            "Communication timeout reading from server " +
            GetHostDNSName(m_ConnectionImpl->
                m_ConnectionPool->GetHost()) + ":" +
            NStr::UIntToString(m_ConnectionImpl->
                m_ConnectionPool->GetPort()) + ".");
        break;
    default: // invalid socket or request, bailing out
        return false;
    }
    return true;
}

void CNetServerConnection::WriteStr(const string& str)
{
    WriteBuf(str.data(), str.size());
}

void CNetServerConnection::WriteBuf(const void* buf, size_t len)
{
    CheckConnect();
    const char* buf_ptr = (const char*)buf;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;

        EIO_Status io_st = m_ConnectionImpl->m_Socket.Write(
            buf_ptr, size_to_write, &n_written);

        if (io_st != eIO_Success) {
            m_ConnectionImpl->Abort();

            CIO_Exception io_ex(DIAG_COMPILE_INFO, 0,
                (CIO_Exception::EErrCode) io_st, "IO error.");

            CNetServerConnectionPool* pool = m_ConnectionImpl->m_ConnectionPool;

            NCBI_THROW(CNetSrvConnException, eWriteFailure,
                "Failed to write to server " +
                GetHostDNSName(pool->GetHost()) + ":" +
                NStr::UIntToString(pool->GetPort()) +
                    ". Reason: " + string(io_ex.what()));
        }
        size_to_write -= n_written;
        buf_ptr += n_written;
    } // while
}

// if wait_sec is set to 0 m_Timeout will be used
void CNetServerConnection::WaitForServer(unsigned int wait_sec)
{
    _ASSERT(m_ConnectionImpl->IsConnected());

    STimeout to = {wait_sec, 0};

    CNetServerConnectionPool* pool = m_ConnectionImpl->m_ConnectionPool;

    if (wait_sec == 0)
        to = pool->GetCommunicationTimeout();

    EIO_Status io_st = m_ConnectionImpl->m_Socket.Wait(eIO_Read, &to);

    if (io_st == eIO_Timeout) {
        m_ConnectionImpl->Abort();

        NCBI_THROW(CNetSrvConnException, eResponseTimeout,
            "No response from the server " +
                GetHostDNSName(pool->GetHost()) + ":" +
                NStr::UIntToString(pool->GetPort()) + ".");
    }
}


CSocket* CNetServerConnection::GetSocket()
{
    _ASSERT(m_ConnectionImpl->IsConnected());
    return &m_ConnectionImpl->m_Socket;
}

void CNetServerConnection::CheckConnect()
{
    if (m_ConnectionImpl->IsConnected())
        return;

    CNetServerConnectionPool* pool = m_ConnectionImpl->m_ConnectionPool;

    INetServerConnectionListener* listener = pool->GetEventListener();

    if (m_ConnectionImpl->m_Connected) {
        if (listener)
            listener->OnDisconnected(pool);

        m_ConnectionImpl->m_Connected = false;
    }

    unsigned conn_repeats = 0;

    CSocket* the_socket = &m_ConnectionImpl->m_Socket;

    EIO_Status io_st;

    while ((io_st = the_socket->Connect(pool->GetHost(), pool->GetPort(),
        &pool->GetCommunicationTimeout(), eOn)) != eIO_Success) {

        if (io_st == eIO_Unknown) {

            the_socket->Close();

            // most likely this is an indication we have too many
            // open ports on the client side
            // (this kernel limitation manifests itself on Linux)
            if (++conn_repeats > pool->GetCreateSocketMaxRetries()) {
                if (io_st != eIO_Success) {
                    CIO_Exception io_ex(DIAG_COMPILE_INFO,
                        0, (CIO_Exception::EErrCode)io_st, "IO error.");
                    NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                        "Failed to connect to server " +
                        GetHostDNSName(pool->GetHost()) + ":" +
                        NStr::UIntToString(pool->GetPort()) +
                            ". Reason: " + string(io_ex.what()));
                }
            }
            // give system a chance to recover

            SleepMilliSec(1000 * conn_repeats);
        } else {
            CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
            NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                "Failed to connect to server " +
                GetHostDNSName(pool->GetHost()) + ":" +
                NStr::UIntToString(pool->GetPort()) +
                ". Reason: " + string(io_ex.what()));
        }
    }

    the_socket->SetDataLogging(eDefault);
    the_socket->SetTimeout(eIO_ReadWrite, &pool->GetCommunicationTimeout());
    the_socket->DisableOSSendDelay();
    the_socket->SetReuseAddress(eOn);

    if (listener)
        listener->OnConnected(*this);

    m_ConnectionImpl->m_Connected = true;
}

void CNetServerConnection::Close()
{
    m_ConnectionImpl->Close();
}

void CNetServerConnection::Abort()
{
    m_ConnectionImpl->Abort();
}

void CNetServerConnection::Telnet(CNcbiOstream* out,  CNetServerConnection::IStringProcessor* processor)
{
    _ASSERT(m_ConnectionImpl->IsConnected());

    STimeout rto = {1, 0};

    CSocket* the_socket = &m_ConnectionImpl->m_Socket;

    the_socket->SetTimeout(eIO_Read, &rto);

    string line;

    for (;;) {
        EIO_Status st = the_socket->ReadLine(line);
        if (st == eIO_Success) {
            if (processor && !processor->Process(line))
                break;
            if (out) *out << line << "\n" << flush;
        } else {
            EIO_Status st = the_socket->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }
    m_ConnectionImpl->Close();
}

const string& CNetServerConnection::GetHost() const
{
    return m_ConnectionImpl->m_ConnectionPool->GetHost();
}

unsigned int CNetServerConnection::GetPort() const
{
    return m_ConnectionImpl->m_ConnectionPool->GetPort();
}


/*************************************************************************/
CNetServerConnectionPool::CNetServerConnectionPool(const string& host,
    unsigned short port, INetServerConnectionListener* listener) :
        m_Host(host),
        m_Port(port),
        m_Timeout(s_GetDefaultCommTimeout()),
        m_EventListener(listener),
        m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()),
        m_FreeConnectionListHead(NULL),
        m_FreeConnectionListSize(0),
        m_PermanentConnection(eDefault)
{
}

void CNetServerConnectionPool::Put(SNetServerConnectionImpl* impl)
{
    if (m_PermanentConnection != eOn)
        impl->Close();
    else
        if (impl->m_Socket.GetStatus(eIO_Open) == eIO_Success) {
            // we need to check if all data has been read.
            // if it has not then we have to close the connection
            STimeout to = {0, 0};

            if (impl->m_Socket.Wait(eIO_Read, &to) == eIO_Success)
                impl->Abort();
            else {
                TFastMutexGuard guard(m_FreeConnectionListLock);

                int upper_limit = TServConn_MaxConnPoolSize::GetDefault();

                if (upper_limit == 0 ||
                        m_FreeConnectionListSize < upper_limit) {
                    impl->m_NextFree = m_FreeConnectionListHead;
                    m_FreeConnectionListHead = impl;
                    ++m_FreeConnectionListSize;
                    return;
                }
            }
        }

    delete impl;
}

CNetServerConnection CNetServerConnectionPool::GetConnection()
{
    TFastMutexGuard guard(m_FreeConnectionListLock);

    SNetServerConnectionImpl* impl;

    if (m_FreeConnectionListSize == 0)
        impl = new SNetServerConnectionImpl(this);
    else {
        impl = m_FreeConnectionListHead;
        m_FreeConnectionListHead = impl->m_NextFree;
        --m_FreeConnectionListSize;
    }

    return CNetServerConnection(impl);
}

CNetServerConnectionPool::~CNetServerConnectionPool()
{
    // No need to lock the mutex in the destructor.
    SNetServerConnectionImpl* impl = m_FreeConnectionListHead;
    while (impl != NULL) {
        SNetServerConnectionImpl* next_impl = impl->m_NextFree;
        delete impl;
        impl = next_impl;
    }
}

void CNetServerConnectionPool::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
}

/* static */
void CNetServerConnectionPool::SetDefaultCommunicationTimeout(const STimeout& to)
{
    s_SetDefaultCommTimeout(to);
}

/* static */
void CNetServerConnectionPool::SetDefaultCreateSocketMaxReties(unsigned int retries)
{
    TServConn_ConnMaxRetries::SetDefault(retries);
}

END_NCBI_SCOPE
