/* $Id$
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
//  CTrigger::
//

CTrigger::~CTrigger()
{
    if (m_Trigger)
        TRIGGER_Close(m_Trigger);
}



/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//


CSocket::CSocket(const string&   host,
                 unsigned short  port,
                 const STimeout* timeout,
                 TSOCK_Flags     flags)
    : m_IsOwned(eTakeOwnership),
      r_timeout(0), w_timeout(0), c_timeout(0)
{
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout  = &oo_timeout;
    } else
        o_timeout  = 0;
    SOCK_CreateEx(host.c_str(), port, o_timeout, &m_Socket, 0, 0, flags);
}


CSocket::CSocket(unsigned int    host,
                 unsigned short  port,
                 const STimeout* timeout,
                 TSOCK_Flags     flags)
    : m_IsOwned(eTakeOwnership),
      r_timeout(0), w_timeout(0), c_timeout(0)
{
    char x_host[16/*sizeof("255.255.255.255")*/];
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout = &oo_timeout;
    } else
        o_timeout = 0;
    if (SOCK_ntoa(host, x_host, sizeof(x_host)) != 0)
        m_Socket = 0;
    else
        SOCK_CreateEx(x_host, port, o_timeout, &m_Socket, 0, 0, flags);

}


CUNIXSocket::CUNIXSocket(const string&   path,
                         const STimeout* timeout,
                         TSOCK_Flags     flags)
{
    if (timeout && timeout != kDefaultTimeout) {
        oo_timeout = *timeout;
        o_timeout = &oo_timeout;
    } else
        o_timeout = 0;
    SOCK_CreateUNIX(path.c_str(), o_timeout, &m_Socket, 0, 0, flags);
}


CSocket::~CSocket()
{
    if (m_Socket  &&  m_IsOwned != eNoOwnership)
        SOCK_Close(m_Socket);
}


EIO_Status CSocket::Connect(const string&   host,
                            unsigned short  port,
                            const STimeout* timeout,
                            TSOCK_Flags     flags)
{
    if ( m_Socket ) {
        if (SOCK_Status(m_Socket, eIO_Open) != eIO_Closed)
            return eIO_Unknown;
        if (m_IsOwned != eNoOwnership)
            SOCK_Close(m_Socket);
    }
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            if (&oo_timeout != timeout)
                oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout = 0;
    }
    EIO_Status status = SOCK_CreateEx(host.c_str(), port, o_timeout,
                                      &m_Socket, 0, 0, flags);
    if (status == eIO_Success) {
        SOCK_SetTimeout(m_Socket, eIO_Read,  r_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Write, w_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Close, c_timeout);        
    } else
        _ASSERT(!m_Socket);
    return status;
}


EIO_Status CUNIXSocket::Connect(const string&   path,
                                const STimeout* timeout,
                                TSOCK_Flags     flags)
{
    if ( m_Socket ) {
        if (SOCK_Status(m_Socket, eIO_Open) != eIO_Closed)
            return eIO_Unknown;
        if (m_IsOwned != eNoOwnership)
            SOCK_Close(m_Socket);
    }
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            if (&oo_timeout != timeout)
                oo_timeout = *timeout;
            o_timeout = &oo_timeout;
        } else
            o_timeout = 0;
    }
    EIO_Status status = SOCK_CreateUNIX(path.c_str(), o_timeout,
                                        &m_Socket, 0, 0, flags);
    if (status == eIO_Success) {
        SOCK_SetTimeout(m_Socket, eIO_Read,  r_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Write, w_timeout);
        SOCK_SetTimeout(m_Socket, eIO_Close, c_timeout);        
    } else
        _ASSERT(!m_Socket);
    return status;
}


EIO_Status CSocket::Reconnect(const STimeout* timeout)
{
    if (timeout != kDefaultTimeout) {
        if ( timeout ) {
            if (&oo_timeout != timeout)
                oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout = 0;
    }
    return m_Socket ? SOCK_Reconnect(m_Socket, 0, 0, o_timeout) : eIO_Closed;
}


EIO_Status CSocket::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    if (timeout == kDefaultTimeout)
        return eIO_Success;

    switch (event) {
    case eIO_Open:
        if ( timeout ) {
            if (&oo_timeout != timeout)
                oo_timeout = *timeout;
            o_timeout  = &oo_timeout;
        } else
            o_timeout  = 0;
        break;
    case eIO_Read:
        if ( timeout ) {
            if (&rr_timeout != timeout)
                rr_timeout = *timeout;
            r_timeout  = &rr_timeout;
        } else
            r_timeout  = 0;
        break;
    case eIO_Write:
        if ( timeout ) {
            if (&ww_timeout != timeout)
                ww_timeout = *timeout;
            w_timeout  = &ww_timeout;
        } else
            w_timeout  = 0;
        break;
    case eIO_ReadWrite:
        if ( timeout ) {
            if (&rr_timeout != timeout)
                rr_timeout = *timeout;
            r_timeout  = &rr_timeout;
            if (&ww_timeout != timeout)
                ww_timeout = *timeout;
            w_timeout  = &ww_timeout;
        } else {
            r_timeout  = 0;
            w_timeout  = 0;
        }
        break;
    case eIO_Close:
        if ( timeout ) {
            if (&cc_timeout != timeout)
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
        return ((unsigned long) r_timeout->sec * 1000000 + r_timeout->usec >
                (unsigned long) w_timeout->sec * 1000000 + w_timeout->usec)
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


void CSocket::GetPeerAddress(unsigned int*   host,
                             unsigned short* port,
                             ENH_ByteOrder   byte_order) const
{
    if ( !m_Socket ) {
        if ( host )
            *host = 0;
        if ( port )
            *port = 0;
    } else
        SOCK_GetPeerAddress(m_Socket, host, port, byte_order);
}


string CSocket::GetPeerAddress(ESOCK_AddressFormat format) const
{
    char buf[PATH_MAX + 1];
    if (m_Socket  &&
        SOCK_GetPeerAddressStringEx(m_Socket, buf, sizeof(buf), format) != 0) {
        return string(buf);
    }
    return "";
}


void CSocket::Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence)
{
    if (m_Socket  &&  m_IsOwned != eNoOwnership)
        SOCK_Close(m_Socket);
    m_Socket  = sock;
    m_IsOwned = if_to_own;
    if (whence == eCopyTimeoutsFromSOCK) {
        if ( sock ) {
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
        } else
            r_timeout = w_timeout = c_timeout = 0;
    } else if ( sock ) {
        SOCK_SetTimeout(sock, eIO_Read,  r_timeout);
        SOCK_SetTimeout(sock, eIO_Write, w_timeout);
        SOCK_SetTimeout(sock, eIO_Close, c_timeout);
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CDatagramSocket::
//


EIO_Status CDatagramSocket::Connect(unsigned int   host,
                                    unsigned short port)
{
    char addr[32];
    if (host  &&  SOCK_ntoa(host, addr, sizeof(addr)) != 0)
        return eIO_Unknown;
    return m_Socket
        ? DSOCK_Connect(m_Socket, host ? addr : 0, port)
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


CListeningSocket::~CListeningSocket()
{
    Close();
}


EIO_Status CListeningSocket::Accept(CSocket*&       sock,
                                    const STimeout* timeout,
                                    TSOCK_Flags     flags) const
{
    if ( !m_Socket )
        return eIO_Closed;

    SOCK x_sock;
    EIO_Status status = LSOCK_AcceptEx(m_Socket, timeout, &x_sock, flags);
    if (status == eIO_Success) {
        try {
            sock = new CSocket;
        } catch (...) {
            sock = 0;
            SOCK_Close(x_sock);
            throw;
        }
        sock->Reset(x_sock, eTakeOwnership, eCopyTimeoutsToSOCK);
    } else
        sock = 0;
    return status;
}


EIO_Status CListeningSocket::Accept(CSocket&        sock,
                                    const STimeout* timeout,
                                    TSOCK_Flags     flags) const
{
    if ( !m_Socket )
        return eIO_Closed;

    SOCK x_sock;
    EIO_Status status = LSOCK_AcceptEx(m_Socket, timeout, &x_sock, flags);
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


EIO_Status CSocketAPI::Poll(vector<SPoll>&  polls,
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    static const STimeout kZero = {0, 0};
    size_t          x_n     = polls.size();
    SPOLLABLE_Poll* x_polls = 0;
    size_t          x_ready = 0;

    if (x_n  &&  !(x_polls = new SPOLLABLE_Poll[x_n]))
        return eIO_Unknown;

    for (size_t i = 0;  i < x_n;  i++) {
        CPollable* p     = polls[i].m_Pollable;
        EIO_Event  event = polls[i].m_Event;
        if (p  &&  event) {
            CSocket* s = dynamic_cast<CSocket*> (p);
            if (!s) {
                CListeningSocket* ls = dynamic_cast<CListeningSocket*> (p);
                if (!ls) {
                    CTrigger* tr = dynamic_cast<CTrigger*> (p);
                    x_polls[i].poll = POLLABLE_FromTRIGGER(tr
                                                           ? tr->GetTRIGGER()
                                                           : 0);
                } else
                    x_polls[i].poll = POLLABLE_FromLSOCK(ls->GetLSOCK());
                polls[i].m_REvent = eIO_Open;
            } else {
                EIO_Event revent;
                if (s->GetStatus(eIO_Open) != eIO_Closed) {
                    x_polls[i].poll = POLLABLE_FromSOCK(s->GetSOCK());
                    revent = eIO_Open;
                } else {
                    x_polls[i].poll = 0;
                    revent = eIO_Close;
                    x_ready++;
                }
                polls[i].m_REvent = revent;
            }
            x_polls[i].event = event;
        } else {
            x_polls[i].poll = 0;
            polls[i].m_REvent = eIO_Open;
        }
    }

    size_t xx_ready;
    EIO_Status status = POLLABLE_Poll(x_n, x_polls,
                                      x_ready ? &kZero : timeout, &xx_ready);

    for (size_t i = 0;  i < x_n;  i++) {
        if (x_polls[i].revent)
            polls[i].m_REvent = x_polls[i].revent;
    }

    if (n_ready)
        *n_ready = xx_ready + x_ready;

    delete[] x_polls;
    return status;
}


string CSocketAPI::ntoa(unsigned int host)
{
    char ipaddr[64];
    if (SOCK_ntoa(host, ipaddr, sizeof(ipaddr)) != 0)
        *ipaddr = 0;
    return string(ipaddr);
}


string CSocketAPI::gethostname(ESwitch log)
{
    char hostname[256];
    if (SOCK_gethostnameEx(hostname, sizeof(hostname), log) != 0)
        *hostname = 0;
    return string(hostname);
}


string CSocketAPI::gethostbyaddr(unsigned int host, ESwitch log)
{
    char hostname[256];
    if (!SOCK_gethostbyaddrEx(host, hostname, sizeof(hostname), log))
        *hostname = 0;
    return string(hostname);
}


unsigned int CSocketAPI::gethostbyname(const string& host, ESwitch log)
{
    return SOCK_gethostbynameEx(host == kEmptyStr ? 0 : host.c_str(), log);
}


string    CSocketAPI::HostPortToString(unsigned int    host,
                                       unsigned short  port)
{
    char   buf[80];
    size_t len = SOCK_HostPortToString(host, port, buf, sizeof(buf));
    return string(buf, len);
}


SIZE_TYPE CSocketAPI::StringToHostPort(const string&   str,
                                       unsigned int*   host,
                                       unsigned short* port)
{
    const char* s = str.c_str();
    const char* e = SOCK_StringToHostPort(s, host, port);
    return e ? (SIZE_TYPE)(e - s) : NPOS;
}


END_NCBI_SCOPE
