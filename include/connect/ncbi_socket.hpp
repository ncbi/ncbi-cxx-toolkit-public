#ifndef NCBI_SOCKET__HPP
#define NCBI_SOCKET__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   C++ wrapper for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *   NOTE:  for more details and documentation see "ncbi_socket.h"
 *     CSocketAPI
 *     CSocket
 *     CListeningSocket
 *
 * ---------------------------------------------------------------------------
 */

#include <connect/ncbi_socket.h>
#include <corelib/ncbistd.hpp>
#include <vector>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class CSocket
{
public:
    CSocket(void);

    // Create a client-side socket connected to "host:port"
    CSocket(const string&   host,
            unsigned short  port,
            const STimeout* timeout = 0);

    // Call Close() and destruct
    ~CSocket(void);

    // Direction is one of
    //     eIO_Open  - return eIO_Success if CSocket is okay and open,
    //                 eIO_Closed if closed by Close() or not yet open;
    //     eIO_Read  - status of last read operation;
    //     eIO_Write - status of last write operation.
    // Direction eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    EIO_Status GetStatus(EIO_Event direction) const;

    // Connect to "host:port".
    // NOTE:  should not be called if already connected.
    EIO_Status Connect(const string&   host,
                       unsigned short  port,
                       const STimeout* timeout = 0);

    // Reconnect to the same address.
    // NOTE 1:  the socket must not be closed by the time this call is made.
    // NOTE 2:  not for the sockets created by CListeningSocket::Accept().
    EIO_Status Reconnect(const STimeout* timeout = 0);

    EIO_Status Shutdown(EIO_Event how);

    // NOTE:  do not actually close the undelying SOCK if it is not owned
    EIO_Status Close(void);

    // NOTE:  use CSocketAPI::Poll() to wait on several sockets at once
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);

    EIO_Status      SetTimeout(EIO_Event event, const STimeout* timeout);
    const STimeout* GetTimeout(EIO_Event event) const;

    EIO_Status Read(void* buf, size_t size, size_t* n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    EIO_Status PushBack(const void* buf, size_t size);

    EIO_Status Write(const void* buf, size_t size, size_t* n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    void GetPeerAddress(unsigned int* host, unsigned short* port,
                        ENH_ByteOrder byte_order) const;

    SOCK       GetSOCK     (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

    // use CSocketAPI::SetReadOnWrite() to set the default value
    void SetReadOnWrite(ESwitch read_on_write = eOn);
    // use CSocketAPI::SetDataLogging() to set the default value
    void SetDataLogging(ESwitch log_data = eOn);
    // use CSocketAPI::SetInterruptOnSignal() to set the default value
    void SetInterruptOnSignal(ESwitch interrupt = eOn);

    bool IsServerSide(void) const;

    // old SOCK (if any) will be closed; "if_to_own" passed as eTakeOwnership
    // causes "sock" to close upon Close() call or destruction of the object.
    void Reset(SOCK sock, EOwnership if_to_own);

private:
    SOCK       m_Socket;
    EOwnership m_IsOwned;

    // disable copy constructor and assignment
    CSocket(const CSocket&);
    CSocket& operator= (const CSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CListeningSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see LSOCK_***() functions in "ncbi_socket.h"
//

class CListeningSocket
{
public:
    CListeningSocket(void);
    CListeningSocket(unsigned short port, unsigned short backlog = 5);
    ~CListeningSocket(void);

    // Return eIO_Success if CListeningSocket is opened and bound;
    // Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    EIO_Status Listen(unsigned short port, unsigned short backlog = 5);
    EIO_Status Accept(CSocket*& sock, const STimeout* timeout = 0) const;
    EIO_Status Accept(CSocket&  sock, const STimeout* timeout = 0) const;
    EIO_Status Close(void);

    LSOCK      GetLSOCK    (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

private:
    LSOCK m_Socket;

    // disable copy constructor and assignment
    CListeningSocket(const CListeningSocket&);
    CListeningSocket& operator= (const CListeningSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSocketAPI::
//
//    Global settings related to the socket API
//
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class CSocketAPI
{
public:
    // Generic
    static void       AllowSigPipe   (void);
    static EIO_Status Initialize     (void);
    static EIO_Status Shutdown       (void);

    // Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    static void SetReadOnWrite       (ESwitch read_on_write);
    static void SetDataLogging       (ESwitch log_data);
    static void SetInterruptOnSignal (ESwitch interrupt);

    // NOTE:  use CSocket::Wait() to wait for I/O event(s) on a single socket
    struct SPoll {
        SPoll(CSocket& sock, EIO_Event event, EIO_Event revent)
            : m_Socket(sock), m_Event(event), m_REvent(revent) {}
        CSocket&  m_Socket;
        EIO_Event m_Event;
        EIO_Event m_REvent;        
    };
    static EIO_Status Poll(vector<SPoll>&  sockets,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    // Misc  (mostly BSD-like)
    static string gethostname  (void);               // empty string on error
    static string ntoa         (unsigned int host);
    static string gethostbyaddr(unsigned int host);  // empty string on error

    static unsigned int gethostbyname(const string& hostname);  // 0 on error

    static unsigned int htonl(unsigned int   value);
    static unsigned int ntohl(unsigned int   value);
    static unsigned int htons(unsigned short value);
    static unsigned int ntohs(unsigned short value);
};


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

CSocket::CSocket(void) :
    m_Socket(0), m_IsOwned(eTakeOwnership)
{
}


CSocket::CSocket(const string&   host,
                 unsigned short  port,
                 const STimeout* timeout) :
    m_IsOwned(eTakeOwnership)
{
    if (SOCK_Create(host.c_str(), port, timeout, &m_Socket) != eIO_Success)
        m_Socket = 0;
}


CSocket::~CSocket(void)
{
    if (m_Socket && m_IsOwned != eNoOwnership)
        SOCK_Close(m_Socket);
}


void CSocket::Reset(SOCK sock, EOwnership if_to_own)
{
    if ( m_Socket )
        Close();
    m_Socket = sock;
    m_IsOwned = if_to_own;
}


EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return direction == eIO_Open ? (m_Socket ? eIO_Success : eIO_Closed)
        : (m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed);
}


EIO_Status CSocket::Connect(const string&   host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    if ( m_Socket )
        return eIO_Unknown;
    EIO_Status status = SOCK_Create(host.c_str(), port, timeout, &m_Socket);
    if (status != eIO_Success)
        m_Socket = 0;
    return status;
}


EIO_Status CSocket::Reconnect(const STimeout* timeout)
{
    return m_Socket ? SOCK_Reconnect(m_Socket, 0, 0, timeout) : eIO_Closed;
}


EIO_Status CSocket::Shutdown(EIO_Event how)
{
    return m_Socket ? SOCK_Shutdown(m_Socket, how) : eIO_Closed;
}


EIO_Status CSocket::Close(void)
{
    if ( !m_Socket )
        return eIO_Closed;
    EIO_Status status = m_IsOwned != eNoOwnership
        ? SOCK_Close(m_Socket) : eIO_Success;
    m_Socket = 0;
    return status;
}


EIO_Status CSocket::Wait(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_Wait(m_Socket, event, timeout) : eIO_Closed; 
}


EIO_Status CSocket::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_SetTimeout(m_Socket, event, timeout) : eIO_Closed;
}


const STimeout* CSocket::GetTimeout(EIO_Event event) const
{
    return m_Socket ? SOCK_GetTimeout(m_Socket, event) : 0;
}


EIO_Status CSocket::Read(void*          buf,
                         size_t         size,
                         size_t*        n_read,
                         EIO_ReadMethod how)
{
    if ( m_Socket )
        return SOCK_Read(m_Socket, buf, size, n_read, how);
    if ( n_read )
        *n_read = 0;
    return eIO_Closed;
}


EIO_Status CSocket::PushBack(const void* buf, size_t size)
{
    return m_Socket ? SOCK_PushBack(m_Socket, buf, size) : eIO_Closed;
}


EIO_Status CSocket::Write(const void*     buf,
                          size_t          size,
                          size_t*         n_written,
                          EIO_WriteMethod how)
{
    if ( m_Socket )
        return SOCK_Write(m_Socket, buf, size, n_written, how);
    if ( n_written )
        *n_written = 0;
    return eIO_Closed;
}


void CSocket::GetPeerAddress(unsigned int* host, unsigned short* port,
                             ENH_ByteOrder byte_order) const
{
    if ( !m_Socket ) {
        if ( host )
            *host = 0;
        if ( port )
            *port = 0;
    } else
        SOCK_GetPeerAddress(m_Socket, host, port, byte_order);
}


SOCK CSocket::GetSOCK(void) const
{
    return m_Socket;
}

 
EIO_Status CSocket::GetOSHandle(void* handle_buf, size_t handle_size) const
{
    return m_Socket
        ? SOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}


void CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    if ( m_Socket )
        SOCK_SetReadOnWrite(m_Socket, read_on_write);
}


void CSocket::SetDataLogging(ESwitch log_data)
{
    if ( m_Socket )
        SOCK_SetDataLogging(m_Socket, log_data);
}


void CSocket::SetInterruptOnSignal(ESwitch interrupt)
{
    if ( m_Socket )
        SOCK_SetInterruptOnSignal(m_Socket, interrupt);
}


bool CSocket::IsServerSide(void) const
{
    return m_Socket && SOCK_IsServerSide(m_Socket) ? true : false;
}



/////////////////////////////////////////////////////////////////////////////
//  CListeningSocket::
//

CListeningSocket::CListeningSocket(void) :
    m_Socket(0)
{
}


CListeningSocket::CListeningSocket(unsigned short port, unsigned short backlog)
{
    if (LSOCK_Create(port, backlog, &m_Socket) != eIO_Success)
        m_Socket = 0;
}


CListeningSocket::~CListeningSocket(void)
{
    if ( m_Socket )
        LSOCK_Close(m_Socket);
}


EIO_Status CListeningSocket::GetStatus(void) const
{
    return m_Socket ? eIO_Success : eIO_Closed;
}


EIO_Status CListeningSocket::Listen(unsigned short port,
                                    unsigned short backlog)
{
    if ( m_Socket )
        return eIO_Unknown;
    EIO_Status status = LSOCK_Create(port, backlog, &m_Socket);
    if (status != eIO_Success)
        m_Socket = 0;
    return status;
}


EIO_Status CListeningSocket::Accept(CSocket*&       sock,
                                    const STimeout* timeout) const
{
    if ( !m_Socket )
        return eIO_Unknown;
    SOCK x_sock;
    EIO_Status status = LSOCK_Accept(m_Socket, timeout, &x_sock);
    if (status != eIO_Success)
        sock = 0;
    else if (!(sock = new CSocket)) {
        SOCK_Close(x_sock);
        status = eIO_Unknown;
    } else
        sock->Reset(x_sock, eTakeOwnership);
    return status;
}


EIO_Status CListeningSocket::Accept(CSocket&        sock,
                                    const STimeout* timeout) const
{
    if ( !m_Socket )
        return eIO_Unknown;
    SOCK x_sock;
    EIO_Status status = LSOCK_Accept(m_Socket, timeout, &x_sock);
    if (status == eIO_Success)
        sock.Reset(x_sock, eTakeOwnership);
    return status;
}


EIO_Status CListeningSocket::Close(void)
{
    if ( !m_Socket )
        return eIO_Closed;
    EIO_Status status = LSOCK_Close(m_Socket);
    m_Socket = 0;
    return status;
}


LSOCK CListeningSocket::GetLSOCK(void) const
{
    return m_Socket;
}


EIO_Status CListeningSocket::GetOSHandle(void*  handle_buf,
                                         size_t handle_size) const
{
    return m_Socket
        ? LSOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}



/////////////////////////////////////////////////////////////////////////////
//  CSocketAPI::
//

void CSocketAPI::AllowSigPipe(void)
{
    SOCK_AllowSigPipeAPI();
}


EIO_Status CSocketAPI::Initialize(void)
{
    return SOCK_InitializeAPI();
}


EIO_Status CSocketAPI::Shutdown(void)
{
    return SOCK_ShutdownAPI();
}


void CSocketAPI::SetReadOnWrite(ESwitch read_on_write)
{
    SOCK_SetReadOnWriteAPI(read_on_write);
}


void CSocketAPI::SetDataLogging(ESwitch log_data)
{
    SOCK_SetDataLoggingAPI(log_data);
}


void CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    SOCK_SetInterruptOnSignalAPI(interrupt);
}


string CSocketAPI::gethostname(void)
{
    char hostname[128];
    if (SOCK_gethostname(hostname, sizeof(hostname)) == 0)
        return string(hostname);
    return kEmptyStr;
}


string CSocketAPI::ntoa(unsigned int host)
{
    char ipaddr[64];
    if (SOCK_ntoa(host, ipaddr, sizeof(ipaddr)) == 0)
        return string(ipaddr);
    return kEmptyStr;
}


string CSocketAPI::gethostbyaddr(unsigned int host)
{
    char hostname[128];

    if (SOCK_gethostbyaddr(host, hostname, sizeof(hostname)) != 0)
        return string(hostname);
    return kEmptyStr;
}    


unsigned int CSocketAPI::gethostbyname(const string& hostname)
{
    const char* host = hostname.c_str();
    return SOCK_gethostbyname(*host ? host : 0);
}


unsigned int CSocketAPI::htonl(unsigned int value)
{
    return SOCK_htonl(value);
}


unsigned int CSocketAPI::ntohl(unsigned int value)
{
    return SOCK_ntohl(value);
}


unsigned int CSocketAPI::htons(unsigned short value)
{
    return SOCK_htons(value);
}


unsigned int CSocketAPI::ntohs(unsigned short value)
{
    return SOCK_ntohs(value);
}



END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2002/08/12 15:11:34  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* NCBI_SOCKET__HPP */
