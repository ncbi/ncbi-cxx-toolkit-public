#ifndef CONNECT___NCBI_SOCKET__HPP
#define CONNECT___NCBI_SOCKET__HPP

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
 *     CSocket
 *     CListeningSocket
 *     CSocketAPI
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
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CSocket
{
public:
    CSocket(void);

    // Create a client-side socket connected to "host:port".
    // NOTE: the created underlying "SOCK" will be owned by the "CSocket".
    CSocket(const string&   host,
            unsigned short  port,      // always in host byte order
            const STimeout* timeout = 0/*infinite*/,
            ESwitch         do_log  = eDefault);

    // Call Close(), then self-destruct
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
                       unsigned short  port,      // always in host byte order
                       const STimeout* timeout = 0/*infinite*/,
                       ESwitch         do_log  = eDefault);

    // Reconnect to the same address.
    // NOTE 1:  the socket must not be closed by the time this call is made.
    // NOTE 2:  not for the sockets created by CListeningSocket::Accept().
    EIO_Status Reconnect(const STimeout* timeout = 0/*infinite*/);

    EIO_Status Shutdown(EIO_Event how);

    // NOTE:  closes the undelying SOCK only if it is owned by this "CSocket"!
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

    // NOTE 1:  either of "host", "port" can be NULL to opt out from
    //          obtaining the corresponding value;
    // NOTE 2:  both "*host" and "*port" come out in the same
    //          byte order requested by the third argument.
    void GetPeerAddress(unsigned int* host, unsigned short* port,
                        ENH_ByteOrder byte_order) const;

    // Specify if this "CSocket" is to own the underlying "SOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "SOCK" and the system-specific socket handle
    SOCK       GetSOCK     (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

    // use CSocketAPI::SetReadOnWrite() to set the default value
    void SetReadOnWrite(ESwitch read_on_write = eOn);
    // use CSocketAPI::SetDataLogging() to set the default value
    void SetDataLogging(ESwitch log_data = eOn);
    // use CSocketAPI::SetInterruptOnSignal() to set the default value
    void SetInterruptOnSignal(ESwitch interrupt = eOn);

    bool IsServerSide(void) const;

    // Close the current underlying "SOCK" (if any, and if owned),
    // and from now on use "sock" as the underlying "SOCK" instead.
    // NOTE:  "if_to_own" applies to the (new) "sock".
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

class NCBI_XCONNECT_EXPORT CListeningSocket
{
public:
    CListeningSocket(void);
    // NOTE:  "port" ought to be in host byte order
    CListeningSocket(unsigned short port, unsigned short backlog = 5);

    // Call Close(), then self-destruct
    ~CListeningSocket(void);

    // Return eIO_Success if CListeningSocket is opened and bound;
    // Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    // NOTE:  "port" ought to be in host byte order
    EIO_Status Listen(unsigned short port, unsigned short backlog = 5);

    // NOTE: the created "CSocket" will own its underlying "SOCK"
    EIO_Status Accept(CSocket*& sock, const STimeout* timeout = 0) const;
    EIO_Status Accept(CSocket&  sock, const STimeout* timeout = 0) const;

    // NOTE:  closes the undelying SOCK only if it is owned by this "CSocket"!
    EIO_Status Close(void);

    // Specify if this "CListeningSocket" is to own the underlying "LSOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "LSOCK" and the system-specific socket handle
    LSOCK      GetLSOCK    (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

private:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;

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

class NCBI_XCONNECT_EXPORT CSocketAPI
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
    static EIO_Status Poll(vector<SPoll>&  polls,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    // Misc  (mostly BSD-like); "host" ought to be in network byte order
    static string       gethostname  (void);                // empty str on err
    static string       ntoa         (unsigned int  host);
    static string       gethostbyaddr(unsigned int  host);  // empty str on err
    static unsigned int gethostbyname(const string& hostname);  // 0 on error

    static unsigned int HostToNetLong (unsigned int   value);
    static unsigned int NetToHostLong (unsigned int   value);
    static unsigned int HostToNetShort(unsigned short value);
    static unsigned int NetToHostShort(unsigned short value);
};


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

inline EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return direction == eIO_Open ? (m_Socket ? eIO_Success : eIO_Closed)
        : (m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed);
}


inline EIO_Status CSocket::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_SetTimeout(m_Socket, event, timeout) : eIO_Closed;
}


inline const STimeout* CSocket::GetTimeout(EIO_Event event) const
{
    return m_Socket ? SOCK_GetTimeout(m_Socket, event) : 0;
}


inline EOwnership CSocket::SetOwnership(EOwnership if_to_own)
{
    EOwnership prev_ownership = m_IsOwned;
    m_IsOwned                 = if_to_own;
    return prev_ownership;
}


inline SOCK CSocket::GetSOCK(void) const
{
    return m_Socket;
}


inline EIO_Status CSocket::GetOSHandle(void* handle_buf, size_t handle_size) const
{
    return m_Socket
        ? SOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}

 
inline void CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    if ( m_Socket )
        SOCK_SetReadOnWrite(m_Socket, read_on_write);
}


inline void CSocket::SetDataLogging(ESwitch log_data)
{
    if ( m_Socket )
        SOCK_SetDataLogging(m_Socket, log_data);
}


inline void CSocket::SetInterruptOnSignal(ESwitch interrupt)
{
    if ( m_Socket )
        SOCK_SetInterruptOnSignal(m_Socket, interrupt);
}


inline bool CSocket::IsServerSide(void) const
{
    return m_Socket && SOCK_IsServerSide(m_Socket) ? true : false;
}



/////////////////////////////////////////////////////////////////////////////
//  CListeningSocket::
//

inline EIO_Status CListeningSocket::GetStatus(void) const
{
    return m_Socket ? eIO_Success : eIO_Closed;
}


inline EOwnership CListeningSocket::SetOwnership(EOwnership if_to_own)
{
    EOwnership prev_ownership = m_IsOwned;
    m_IsOwned                 = if_to_own;
    return prev_ownership;
}


inline LSOCK CListeningSocket::GetLSOCK(void) const
{
    return m_Socket;
}


inline EIO_Status CListeningSocket::GetOSHandle(void*  handle_buf,
                                                size_t handle_size) const
{
    return m_Socket
        ? LSOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}



/////////////////////////////////////////////////////////////////////////////
//  CSocketAPI::
//

inline void CSocketAPI::AllowSigPipe(void)
{
    SOCK_AllowSigPipeAPI();
}


inline EIO_Status CSocketAPI::Initialize(void)
{
    return SOCK_InitializeAPI();
}


inline EIO_Status CSocketAPI::Shutdown(void)
{
    return SOCK_ShutdownAPI();
}


inline void CSocketAPI::SetReadOnWrite(ESwitch read_on_write)
{
    SOCK_SetReadOnWriteAPI(read_on_write);
}


inline void CSocketAPI::SetDataLogging(ESwitch log_data)
{
    SOCK_SetDataLoggingAPI(log_data);
}


inline void CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    SOCK_SetInterruptOnSignalAPI(interrupt);
}


inline unsigned int CSocketAPI::HostToNetLong(unsigned int value)
{
    return SOCK_HostToNetLong(value);
}


inline unsigned int CSocketAPI::NetToHostLong(unsigned int value)
{
    return SOCK_NetToHostLong(value);
}


inline unsigned int CSocketAPI::HostToNetShort(unsigned short value)
{
    return SOCK_HostToNetShort(value);
}


inline unsigned int CSocketAPI::NetToHostShort(unsigned short value)
{
    return SOCK_NetToHostShort(value);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.14  2003/01/07 21:58:24  lavr
 * Initial revision
 *
 * Revision 6.13  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.12  2002/12/04 16:54:08  lavr
 * Add extra parameter "log" to CSocket() constructor and CSocket::Connect()
 *
 * Revision 6.11  2002/11/14 01:11:33  lavr
 * Minor formatting changes
 *
 * Revision 6.10  2002/09/19 18:05:25  lavr
 * Header file guard macro changed
 *
 * Revision 6.9  2002/09/17 20:45:28  lavr
 * A typo corrected
 *
 * Revision 6.8  2002/09/16 22:32:47  vakatov
 * Allow to change ownership for the underlying sockets "on-the-fly";
 * plus some minor (mostly formal) code and comments rearrangements
 *
 * Revision 6.7  2002/08/27 03:19:29  lavr
 * CSocketAPI:: Removed methods: htonl(), htons(), ntohl(), ntohs(). Added
 * as replacements: {Host|Net}To{Net|Host}{Long|Short}() (see ncbi_socket.h)
 *
 * Revision 6.6  2002/08/15 18:45:03  lavr
 * CSocketAPI::Poll() documented in more details in ncbi_socket.h(SOCK_Poll)
 *
 * Revision 6.5  2002/08/14 15:04:37  sadykov
 * Prepend "inline" for GetOSHandle() method impl
 *
 * Revision 6.4  2002/08/13 19:28:29  lavr
 * Move most methods out-of-line; fix inline methods to have "inline"
 *
 * Revision 6.3  2002/08/12 20:58:09  lavr
 * More comments on parameters usage of certain methods
 *
 * Revision 6.2  2002/08/12 20:24:04  lavr
 * Resolve expansion mess with byte order marcos (use calls instead) -- FreeBSD
 *
 * Revision 6.1  2002/08/12 15:11:34  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* CONNECT___NCBI_SOCKET__HPP */
