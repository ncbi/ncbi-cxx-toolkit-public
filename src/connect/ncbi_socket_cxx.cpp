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

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket_unix.hpp>
#include <limits.h>                     // for PATH_MAX
#if defined(NCBI_OS_MSWIN) && !defined(PATH_MAX)
#  define PATH_MAX 512                  // will actually use less than 32 chars
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

CSocket::CSocket(void)
    : m_Socket(0),
      m_IsOwned(eTakeOwnership),
      o_timeout(0), r_timeout(0), w_timeout(0), c_timeout(0)
{
    return;
}


CSocket::CSocket(const string&   host,
                 unsigned short  port,
                 const STimeout* timeout,
                 ESwitch         log)
    : m_IsOwned(eTakeOwnership),
      r_timeout(0), w_timeout(0), c_timeout(0)
{
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout  = &oo_timeout;
    } else
        o_timeout  = 0;
    if (SOCK_CreateEx(host.c_str(), port, o_timeout, &m_Socket, 0, 0, log)
        != eIO_Success) {
        m_Socket = 0;
    }
}


CSocket::CSocket(unsigned int    host,
                 unsigned short  port,
                 const STimeout* timeout,
                 ESwitch         log)
    : m_IsOwned(eTakeOwnership),
      r_timeout(0), w_timeout(0), c_timeout(0)
{
    char x_host[16/*sizeof("255.255.255.255")*/];
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout = &oo_timeout;
    } else
        o_timeout = 0;
    if (SOCK_ntoa(host, x_host, sizeof(x_host)) != 0  ||
        SOCK_CreateEx(x_host, port, o_timeout, &m_Socket, 0, 0, log)
        != eIO_Success) {
        m_Socket = 0;
    }
}


CUNIXSocket::CUNIXSocket(const string&   filename,
                         const STimeout* timeout,
                         ESwitch         log)
    : CSocket()
{
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout = &oo_timeout;
    } else
        o_timeout = 0;
    if (SOCK_CreateUNIX(filename.c_str(), o_timeout, &m_Socket, 0, 0, log)
        != eIO_Success) {
        m_Socket = 0;
    }
}


CSocket::~CSocket(void)
{
    if (m_Socket  &&  m_IsOwned != eNoOwnership)
        SOCK_Close(m_Socket);
}


void CSocket::Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence)
{
    if (m_Socket  &&  m_IsOwned != eNoOwnership)
        SOCK_Close(m_Socket);
    m_Socket  = sock;
    m_IsOwned = if_to_own;
    if ( sock ) {
        if (whence == eCopyTimeoutsFromSOCK) {
            const STimeout* timeout;
            timeout = SOCK_GetTimeout(sock, eIO_Read);
            if ( timeout ) {
                rr_timeout = *timeout;
                r_timeout  = &rr_timeout;
            } else
                r_timeout  = 0;
            timeout = SOCK_GetTimeout(sock, eIO_Write);
            if ( timeout ) {
                ww_timeout = *timeout;
                w_timeout  = &ww_timeout;
            } else
                w_timeout  = 0;
            timeout = SOCK_GetTimeout(sock, eIO_Close);
            if ( timeout ) {
                cc_timeout = *timeout;
                c_timeout  = &cc_timeout;
            } else
                c_timeout  = 0;
        } else {
            SOCK_SetTimeout(sock, eIO_Read,  r_timeout);
            SOCK_SetTimeout(sock, eIO_Write, w_timeout);
            SOCK_SetTimeout(sock, eIO_Close, c_timeout);
        }
    }
}


EIO_Status CSocket::Connect(const string&   host,
                            unsigned short  port,
                            const STimeout* timeout,
                            ESwitch         log)
{
    if ( m_Socket ) {
        if (SOCK_Status(m_Socket, eIO_Open) != eIO_Closed)
            return eIO_Unknown;
        SOCK_Close(m_Socket);
    }
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout = 0;
    }
    EIO_Status status = SOCK_CreateEx(host.c_str(), port, o_timeout,
                                      &m_Socket, 0, 0, log);
    if (status == eIO_Success) {
        SOCK_SetTimeout(m_Socket, eIO_Read,  r_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Write, w_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Close, c_timeout);        
    } else
        m_Socket = 0;
    return status;
}


EIO_Status CUNIXSocket::Connect(const string&   filename,
                                const STimeout* timeout,
                                ESwitch         log)
{
    if ( m_Socket ) {
        if (SOCK_Status(m_Socket, eIO_Open) != eIO_Closed)
            return eIO_Unknown;
        SOCK_Close(m_Socket);
    }
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            oo_timeout = *timeout;
            o_timeout = &oo_timeout;
        } else
            o_timeout = 0;
    }
    EIO_Status status = SOCK_CreateUNIX(filename.c_str(), o_timeout,
                                        &m_Socket, 0, 0, log);
    if (status != eIO_Success) {
        SOCK_SetTimeout(m_Socket, eIO_Read,  r_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Write, w_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Close, c_timeout);        
    } else
        m_Socket = 0;
    return status;
}


EIO_Status CSocket::Reconnect(const STimeout* timeout)
{
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout = 0;
    }
    return m_Socket ? SOCK_Reconnect(m_Socket, 0, 0, o_timeout) : eIO_Closed;
}


EIO_Status CSocket::Close(void)
{
    if ( !m_Socket )
        return eIO_Success;

    EIO_Status status = m_IsOwned != eNoOwnership
        ? SOCK_CloseEx(m_Socket, 0) : eIO_Success;
    return status;
}


EIO_Status CSocket::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    if (timeout == kDefaultTimeout)
        return eIO_Success;

    switch (event) {
    case eIO_Open:
        if ( timeout ) {
            oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout  = 0;
        break;
    case eIO_Read:
        if ( timeout ) {
            rr_timeout = *timeout;
            r_timeout  = &rr_timeout;
        } else
            r_timeout  = 0;
        break;
    case eIO_Write:
        if ( timeout ) {
            ww_timeout = *timeout;
            w_timeout  = &ww_timeout;
        } else
            w_timeout  = 0;
        break;
    case eIO_ReadWrite:
        if ( timeout ) {
            rr_timeout = *timeout;
            ww_timeout = *timeout;
            r_timeout  = &rr_timeout;
            w_timeout  = &ww_timeout;
        } else {
            r_timeout  = 0;
            w_timeout  = 0;
        }
        break;
    case eIO_Close:
        if ( timeout ) {
            cc_timeout = *timeout;
            c_timeout  = &cc_timeout;
        } else
            c_timeout  = 0;
        break;
    default:
        return eIO_InvalidArg;
    }
    return m_Socket ? SOCK_SetTimeout(m_Socket, event, timeout) : eIO_Success;
}


const STimeout* CSocket::GetTimeout(EIO_Event event) const
{
    switch (event) {
    case eIO_Open:
        return o_timeout;
    case eIO_Read:
        return r_timeout;
    case eIO_Write:
        return w_timeout;
    case eIO_ReadWrite:
        if ( !r_timeout )
            return w_timeout;
        if ( !w_timeout )
            return r_timeout;
        return ((unsigned long) r_timeout->sec*1000000 + r_timeout->usec >
                (unsigned long) w_timeout->sec*1000000 + w_timeout->usec)
            ? w_timeout : r_timeout;
    case eIO_Close:
        return c_timeout;
    default:
        break;
    }
    return kDefaultTimeout;
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


EIO_Status CSocket::ReadLine(string& str)
{
    if ( !m_Socket )
        return eIO_Closed;
    EIO_Status status;
    char buf[1024];
    size_t size;
    str.erase();
    do {
        status = SOCK_ReadLine(m_Socket, buf, sizeof(buf), &size);
        if (!size)
            break;
        str.append(buf, size);
    } while (status == eIO_Success  &&  size == sizeof(buf));
    return status;
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


string CSocket::GetPeerAddress(void) const
{
    char buf[PATH_MAX + 1];
    if (m_Socket  &&
        SOCK_GetPeerAddressString(m_Socket, buf, sizeof(buf)) != 0) {
        return string(buf);
    }
    return "";
}


/////////////////////////////////////////////////////////////////////////////
//  CDatagramSocket::
//

CDatagramSocket::CDatagramSocket(ESwitch log)
{
    if (DSOCK_CreateEx(&m_Socket, log) != eIO_Success)
        m_Socket = 0;
}


EIO_Status CDatagramSocket::Bind(unsigned short  port)
{
    return m_Socket ? DSOCK_Bind(m_Socket, port) : eIO_Closed;
}


EIO_Status CDatagramSocket::Connect(const string&  host,
                                    unsigned short port)
{
    return m_Socket ? DSOCK_Connect(m_Socket, host.c_str(), port) : eIO_Closed;
}


EIO_Status CDatagramSocket::Connect(unsigned int host,
                                    unsigned short port)
{
    char addr[32];
    if (host  &&  SOCK_ntoa(host, addr, sizeof(addr)) != 0)
        return eIO_Unknown;
    return m_Socket
        ? DSOCK_Connect(m_Socket, host ? addr : 0, port)
        : eIO_Closed;
}


EIO_Status CDatagramSocket::Send(const void*     data,
                                 size_t          datalen,
                                 const string&   host,
                                 unsigned short  port)
{
    return m_Socket
        ? DSOCK_SendMsg(m_Socket, host.c_str(), port, data, datalen)
        : eIO_Closed;
}


EIO_Status CDatagramSocket::Recv(void*           buf,
                                 size_t          buflen,
                                 size_t*         msglen,
                                 string*         sender_host,
                                 unsigned short* sender_port,
                                 size_t          maxmsglen)
{
    if ( !m_Socket )
        return eIO_Closed;

    unsigned int addr;
    EIO_Status status = DSOCK_RecvMsg(m_Socket, buf, buflen, maxmsglen,
                                      msglen, &addr, sender_port);
    if ( sender_host )
        *sender_host = CSocketAPI::ntoa(addr);

    return status;
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


CListeningSocket::CListeningSocket(unsigned short port,
                                   unsigned short backlog,
                                   TLSCE_Flags    flags)
    : m_Socket(0),
      m_IsOwned(eTakeOwnership)
{
    if (LSOCK_CreateEx(port, backlog, &m_Socket, flags) != eIO_Success) {
        m_Socket = 0;
    }
}


CUNIXListeningSocket::CUNIXListeningSocket(const string&  filename,
                                           unsigned short backlog,
                                           ESwitch        log)
    : CListeningSocket()
{
    if (LSOCK_CreateUNIX(filename.c_str(), backlog,
                         &m_Socket, log) != eIO_Success) {
        m_Socket = 0;
    }
}


CListeningSocket::~CListeningSocket(void)
{
    Close();
}


EIO_Status CListeningSocket::Listen(unsigned short port,
                                    unsigned short backlog,
                                    TLSCE_Flags    flags)
{
    if ( m_Socket )
        return eIO_Unknown;

    EIO_Status status = LSOCK_CreateEx(port, backlog, &m_Socket, flags);
    if (status != eIO_Success)
        m_Socket = 0;
    return status;
}


EIO_Status CUNIXListeningSocket::Listen(const string&  filename,
                                        unsigned short backlog,
                                        ESwitch        log)
{
    if ( m_Socket )
        return eIO_Unknown;
    EIO_Status status = LSOCK_CreateUNIX(filename.c_str(), backlog,
                                         &m_Socket, log);
    if (status != eIO_Success)
        m_Socket = 0;
    return status;
}


EIO_Status CListeningSocket::Accept(CSocket*&       sock,
                                    const STimeout* timeout) const
{
    if ( !m_Socket )
        return eIO_Closed;

    SOCK x_sock;
    EIO_Status status = LSOCK_Accept(m_Socket, timeout, &x_sock);
    if (status != eIO_Success) {
        sock = 0;
    } else {
        try {
            sock = new CSocket;
        } catch (...) {
            sock = 0;
            SOCK_CloseEx(x_sock, 1);
            throw;
        }
        sock->Reset(x_sock, eTakeOwnership, eCopyTimeoutsToSOCK);
    }
    return status;
}


EIO_Status CListeningSocket::Accept(CSocket&        sock,
                                    const STimeout* timeout) const
{
    if ( !m_Socket )
        return eIO_Closed;

    SOCK x_sock;
    EIO_Status status = LSOCK_Accept(m_Socket, timeout, &x_sock);
    if (status == eIO_Success)
        sock.Reset(x_sock, eTakeOwnership, eCopyTimeoutsToSOCK);
    return status;
}


EIO_Status CListeningSocket::Close(void)
{
    if ( !m_Socket )
        return eIO_Success;

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
    if ( !SOCK_gethostbyaddr(host, hostname, sizeof(hostname))  &&
         SOCK_ntoa(host, hostname, sizeof(hostname)) != 0) {
        *hostname = 0;
    }
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
    size_t          x_n     = polls.size();
    SPOLLABLE_Poll* x_polls = 0;

    if (x_n  &&  !(x_polls = new SPOLLABLE_Poll[x_n]))
        return eIO_Unknown;

    for (size_t i = 0;  i < x_n;  i++) {
        CPollable* p     = polls[i].m_Pollable;
        EIO_Event  event = polls[i].m_Event;
        if (p  &&  event) {
            CSocket* s = dynamic_cast<CSocket*> (p);
            if (!s) {
                CListeningSocket* ls = dynamic_cast<CListeningSocket*> (p);
                x_polls[i].poll = POLLABLE_FromLSOCK(ls ? ls->GetLSOCK() : 0);
            } else {
                x_polls[i].poll =
                    POLLABLE_FromSOCK(s->GetStatus(eIO_Open) == eIO_Success ?
                                      s->GetSOCK() : 0);
            }
            x_polls[i].event = event;
        } else
            x_polls[i].poll  = 0;
    }

    size_t x_ready;
    EIO_Status status = POLLABLE_Poll(x_n, x_polls, timeout, &x_ready);

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
 * Revision 6.31  2005/03/09 15:05:27  lavr
 * Remove CDatagramSocket::Send(...unsigned int host = 0...)
 *
 * Revision 6.30  2005/03/02 16:12:11  lavr
 * Extend CDatagramSocket::Connect and Send with addtl signatures
 *
 * Revision 6.29  2004/11/09 21:26:38  lavr
 * CSocket::ReadLine(): Use string::append() instead of operator+()
 *
 * Revision 6.28  2004/11/09 21:13:28  lavr
 * +CSocket::ReadLine()
 *
 * Revision 6.27  2004/10/26 14:48:28  lavr
 * Implement UNIX socket extensions to the socket API
 *
 * Revision 6.26  2004/09/24 18:43:55  lavr
 * CSocketAPI::gethostbyaddr(): to try to do 'ntoa' if no name was found
 *
 * Revision 6.25  2004/07/23 19:05:26  lavr
 * CListeningSocket(), CListeningSocket::Listen() to accept flags
 *
 * Revision 6.24  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.23  2003/11/25 15:09:47  lavr
 * Allow CSocket::Connect() after closing with CSocket::Close()
 *
 * Revision 6.22  2003/11/12 17:47:58  lavr
 * Few fixes of return status; take advantage of SOCK_CloseEx()
 *
 * Revision 6.21  2003/10/24 16:51:36  lavr
 * GetTimeout(eIO_ReadWrite): return the lesser of eIO_Read and eIO_Write
 *
 * Revision 6.20  2003/08/25 14:42:42  lavr
 * Employ new k..Timeout constants;  reimplement more generic form of Poll()
 *
 * Revision 6.19  2003/07/15 19:04:04  lavr
 * Yet another fix for MS-Win compilation
 *
 * Revision 6.18  2003/07/15 18:16:44  lavr
 * Fix MS-Win compilation
 *
 * Revision 6.17  2003/07/15 18:09:51  lavr
 * Use <limits.h> instead of <stdlib.h> to fetch PATH_MAX
 *
 * Revision 6.16  2003/07/15 16:50:57  lavr
 * Implementation of CSocket::GetPeerAddress(void) added
 *
 * Revision 6.15  2003/05/14 03:50:54  lavr
 * Match changes in ncbi_socket.hpp
 *
 * Revision 6.14  2003/04/30 17:03:33  lavr
 * Modified prototypes for CDatagramSocket::Send() and CDatagramSocket::Recv()
 *
 * Revision 6.13  2003/04/11 20:59:30  lavr
 * CDatagramSocket:: API defined completely
 *
 * Revision 6.12  2003/02/20 17:55:39  lavr
 * Inlining CSocket::Shutdown() and CSocket::Wait()
 *
 * Revision 6.11  2003/02/14 22:03:43  lavr
 * Add internal CSocket timeouts and document them
 *
 * Revision 6.10  2003/01/24 23:01:19  lavr
 * Added class CDatagramSocket
 *
 * Revision 6.9  2002/12/04 16:56:02  lavr
 * Employ SOCK_CreateEx()
 *
 * Revision 6.8  2002/11/14 01:11:49  lavr
 * Minor formatting changes
 *
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
