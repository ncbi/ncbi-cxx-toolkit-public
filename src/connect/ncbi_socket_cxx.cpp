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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   C++ wrappers for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *   Implementation of out-of-line methods
 *
 */

#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

CSocket::CSocket(void)
    : m_Socket(0),
      m_IsOwned(eTakeOwnership)
{
    return;
}


CSocket::CSocket(const string&   host,
                 unsigned short  port,
                 const STimeout* timeout)
    : m_IsOwned(eTakeOwnership)
{
    if (SOCK_Create(host.c_str(), port, timeout, &m_Socket) != eIO_Success)
        m_Socket = 0;
}


CSocket::~CSocket(void)
{
    Close();
}


void CSocket::Reset(SOCK sock, EOwnership if_to_own)
{
    if ( m_Socket )
        Close();
    m_Socket  = sock;
    m_IsOwned = if_to_own;
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
    } else {
        SOCK_GetPeerAddress(m_Socket, host, port, byte_order);
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CListeningSocket::
//

CListeningSocket::CListeningSocket(void)
    : m_Socket(0),
      m_IsOwned(eTakeOwnership)
{
    return;
}


CListeningSocket::CListeningSocket(unsigned short port, unsigned short backlog)
    : m_Socket(0),
      m_IsOwned(eTakeOwnership)
{
    if (LSOCK_Create(port, backlog, &m_Socket) != eIO_Success)
        m_Socket = 0;
}


CListeningSocket::~CListeningSocket(void)
{
    Close();
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
    if (status != eIO_Success) {
        sock = 0;
    } else if ( !(sock = new CSocket) ) {
        SOCK_Close(x_sock);
        status = eIO_Unknown;
    } else {
        sock->Reset(x_sock, eTakeOwnership);
    }
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

    EIO_Status status = m_IsOwned != eNoOwnership
        ? LSOCK_Close(m_Socket) : eIO_Success;
    m_Socket = 0;
    return status;
}



/////////////////////////////////////////////////////////////////////////////
//  CSocketAPI::
//

string CSocketAPI::gethostname(void)
{
    char hostname[256];
    if (SOCK_gethostname(hostname, sizeof(hostname)) != 0)
        *hostname = 0;
    return string(hostname);
}


string CSocketAPI::ntoa(unsigned int host)
{
    char ipaddr[64];
    if (SOCK_ntoa(host, ipaddr, sizeof(ipaddr)) != 0)
        *ipaddr = 0;
    return string(ipaddr);
}


string CSocketAPI::gethostbyaddr(unsigned int host)
{
    char hostname[256];
    if ( !SOCK_gethostbyaddr(host, hostname, sizeof(hostname)) )
        *hostname = 0;
    return string(hostname);
}    


unsigned int CSocketAPI::gethostbyname(const string& hostname)
{
    const char* host = hostname.c_str();
    return SOCK_gethostbyname(*host ? host : 0);
}


EIO_Status CSocketAPI::Poll(vector<SPoll>&  polls,
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    size_t      x_n     = polls.size();
    SSOCK_Poll* x_polls = 0;

    if (x_n  &&  !(x_polls = new SSOCK_Poll[x_n]))
        return eIO_Unknown;

    for (size_t i = 0;  i < x_n;  i++) {
        CSocket& s = polls[i].m_Socket;
        if (s.GetStatus(eIO_Open) == eIO_Success) {
            x_polls[i].sock = s.GetSOCK();
            x_polls[i].event = polls[i].m_Event;
        } else {
            x_polls[i].sock = 0;
        }
    }

    size_t x_ready;
    EIO_Status status = SOCK_Poll(x_n, x_polls, timeout, &x_ready);

    if (status == eIO_Success) {
        for (size_t i = 0;  i < x_n;  i++)
            polls[i].m_REvent = x_polls[i].revent;
    }

    if ( n_ready )
        *n_ready = x_ready;

    delete[] x_polls;
    return status;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.7  2002/11/01 20:13:15  lavr
 * Expand hostname buffers to hold up to 256 chars
 *
 * Revision 6.6  2002/09/17 20:43:49  lavr
 * Style conforming tiny little change
 *
 * Revision 6.5  2002/09/16 22:32:49  vakatov
 * Allow to change ownership for the underlying sockets "on-the-fly";
 * plus some minor (mostly formal) code and comments rearrangements
 *
 * Revision 6.4  2002/08/15 18:48:01  lavr
 * Change all internal variables to have "x_" prefix in CSocketAPI::Poll()
 *
 * Revision 6.3  2002/08/14 15:16:25  lavr
 * Do not use kEmptyStr for not to depend on xncbi(corelib) library
 *
 * Revision 6.2  2002/08/13 19:29:02  lavr
 * Move most methods out-of-line to this file
 *
 * Revision 6.1  2002/08/12 15:15:36  lavr
 * Initial revision
 *
 * ===========================================================================
 */
