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

#include <connect/ncbi_socket_unix.h>
#include <corelib/ncbitype.h>
#include <corelib/ncbimisc.hpp>
#include <string>
#include <vector>


/** @addtogroup Sockets
 *
 * @{
 */


BEGIN_NCBI_SCOPE


enum ECopyTimeout {
    eCopyTimeoutsFromSOCK,
    eCopyTimeoutsToSOCK
};


class NCBI_XCONNECT_EXPORT CPollable
{
public:
    virtual
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const = 0;

protected:
    CPollable(void) { };
    virtual ~CPollable(void) { };

private:
    // disable copy constructor and assignment
    CPollable(const CPollable&);
    CPollable& operator= (const CPollable&);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CSocket::
//
// NOTE:  For documentation see SOCK_***() functions in "ncbi_socket.h".
//        Initially, all timeouts are infinite.
//

class NCBI_XCONNECT_EXPORT CSocket : public CPollable
{
public:
    CSocket(void);

    // Create a client-side socket connected to "host:port".
    // NOTE 1:  the created underlying "SOCK" will be owned by the "CSocket";
    // NOTE 2:  timeout from the argument becomes new eIO_Open timeout.
    CSocket(const string&   host,
            unsigned short  port,      // always in host byte order
            const STimeout* timeout = kInfiniteTimeout,
            ESwitch         log     = eDefault);
    // Variant of the above, which takes host as a binary value in network b.o.
    CSocket(unsigned int    host,      // network byte order
            unsigned short  port,      // host byte order
            const STimeout* timeout = kInfiniteTimeout,
            ESwitch         log     = eDefault);

    // Call Close(), then self-destruct
    virtual ~CSocket(void);

    // Direction is one of
    //     eIO_Open  - return eIO_Success if CSocket is okay and open,
    //                 eIO_Closed if closed by Close() or not yet open;
    //     eIO_Read  - status of last read operation;
    //     eIO_Write - status of last write operation.
    // Direction eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    EIO_Status GetStatus(EIO_Event direction) const;

    // Connect to "host:port".
    // NOTE 1:  should not be called if already connected;
    // NOTE 2:  timeout from the argument becomes new eIO_Open timeout.
    EIO_Status Connect(const string&   host,
                       unsigned short  port,      // always in host byte order
                       const STimeout* timeout = kDefaultTimeout,
                       ESwitch         log     = eDefault);

    // Reconnect to the same address.
    // NOTE 1:  the socket must not be closed by the time this call is made;
    // NOTE 2:  not for the sockets created by CListeningSocket::Accept();
    // NOTE 3:  timeout from the argument becomes new eIO_Open timeout.
    EIO_Status Reconnect(const STimeout* timeout = kDefaultTimeout);

    EIO_Status Shutdown(EIO_Event how);

    // NOTE:  closes the undelying SOCK only if it is owned by this "CSocket"!
    EIO_Status Close(void);

    // NOTE:  use CSocketAPI::Poll() to wait on several sockets at once
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);

    // NOTE 1:  by default, initially all timeouts are infinite;
    // NOTE 2:  SetTimeout(..., kDefaultTimeout) has no effect;
    // Note 3:  GetTimeout(eIO_ReadWrite) returns the least
    //          of eIO_Read and eIO_Write ones.
    EIO_Status      SetTimeout(EIO_Event event, const STimeout* timeout);
    const STimeout* GetTimeout(EIO_Event event) const;

    EIO_Status Read(void* buf, size_t size, size_t* n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);
    // Read up to '\r\n', '\n' or '\0', discarding any of the EOLs
    EIO_Status ReadLine(string& str);

    EIO_Status PushBack(const void* buf, size_t size);

    EIO_Status Write(const void* buf, size_t size, size_t* n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    EIO_Status Abort(void);

    // NOTE 1:  either of "host", "port" can be NULL to opt out from
    //          obtaining the corresponding value;
    // NOTE 2:  both "*host" and "*port" come out in the same
    //          byte order requested by the third argument.
    void GetPeerAddress(unsigned int* host, unsigned short* port,
                        ENH_ByteOrder byte_order) const;
    // Return textual string representing the peer's address
    string GetPeerAddress(void) const;

    // Specify if this "CSocket" is to own the underlying "SOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "SOCK" and the system-specific socket handle.
    SOCK               GetSOCK    (void) const;
    virtual EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const;

    // NOTE:  use CSocketAPI::SetReadOnWrite() to set the default value
    ESwitch SetReadOnWrite(ESwitch read_on_write = eOn);
    // NOTE:  use CSocketAPI::SetDataLogging() to set the default value
    ESwitch SetDataLogging(ESwitch log = eOn);
    // NOTE:  use CSocketAPI::SetInterruptOnSignal() to set the default value
    ESwitch SetInterruptOnSignal(ESwitch interrupt = eOn);
    // NOTE:  use CSocketAPI::SetReuseAddress() to set the default value
    void    SetReuseAddress(ESwitch reuse = eOff);
    // NOTE:  see comments for SOCK_DisableOSSendDelay() in ncbi_socket.h
    void    DisableOSSendDelay(bool on_off = true);

    bool IsClientSide(void) const;
    bool IsServerSide(void) const;
    bool IsDatagram  (void) const;
    bool IsUNIX      (void) const;

    // Close the current underlying "SOCK" (if any, and if owned),
    // and from now on use "sock" as the underlying "SOCK" instead.
    // NOTE:  "if_to_own" applies to the (new) "sock"
    void Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence);

protected:
    SOCK       m_Socket;
    EOwnership m_IsOwned;
    // Timeouts
    STimeout*  o_timeout;  // eIO_Open
    STimeout*  r_timeout;  // eIO_Read
    STimeout*  w_timeout;  // eIO_Write
    STimeout*  c_timeout;  // eIO_Close
    STimeout  oo_timeout;  // storage for o_timeout
    STimeout  rr_timeout;  // storage for r_timeout
    STimeout  ww_timeout;  // storage for w_timeout
    STimeout  cc_timeout;  // storage for c_timeout

private:
    // disable copy constructor and assignment
    CSocket(const CSocket&);
    CSocket& operator= (const CSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDatagramSocket::
//
//    Datagram socket
//
// NOTE:  for documentation see DSOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CDatagramSocket : public CSocket
{
public:
    // NOTE:  the created underlying "SOCK" will be owned by the object
    CDatagramSocket(ESwitch do_log = eDefault);

    EIO_Status Bind(unsigned short port);

    // "port" is in host byte order
    EIO_Status Connect(const string& host, unsigned short port);
    // "host" is accepted in network byte order; "port" is in host one
    EIO_Status Connect(unsigned int host, unsigned short port);

    EIO_Status Wait(const STimeout* timeout = kInfiniteTimeout);

    EIO_Status Send(const void*     data    = 0,
                    size_t          datalen = 0,
                    const string&   host    = string(),
                    unsigned short  port    = 0);  // host byte order

    EIO_Status Recv(void*           buf         = 0,
                    size_t          buflen      = 0,
                    size_t*         msglen      = 0,
                    string*         sender_host = 0,
                    unsigned short* sender_port = 0,
                    size_t          maxmsglen   = 0);

    EIO_Status Clear(EIO_Event direction);

    EIO_Status SetBroadcast(bool do_broadcast = true);

protected:
    // NOTE: these calls are not valid with datagram sockets
    EIO_Status Shutdown(EIO_Event how);
    EIO_Status Reconnect(const STimeout* timeout);
    EIO_Status Abort(void);

private:
    // disable copy constructor and assignment
    CDatagramSocket(const CDatagramSocket&);
    CDatagramSocket& operator= (const CDatagramSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CListeningSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see LSOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CListeningSocket : public CPollable
{
public:
    CListeningSocket(void);
    // NOTE:  "port" ought to be in host byte order
    CListeningSocket(unsigned short port,
                     unsigned short backlog = 5,
                     TLSCE_Flags    flags   = fLSCE_LogDefault);

    // Call Close(), then self-destruct
    virtual ~CListeningSocket(void);

    // Return eIO_Success if CListeningSocket is opened and bound;
    // Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    // NOTE:  "port" ought to be in host byte order
    EIO_Status Listen(unsigned short port,
                      unsigned short backlog = 5,
                      TLSCE_Flags    flags   = fLSCE_LogDefault);

    // NOTE: the created "CSocket" will own its underlying "SOCK"
    EIO_Status Accept(CSocket*& sock,
                      const STimeout* timeout = kInfiniteTimeout) const;
    EIO_Status Accept(CSocket&  sock,
                      const STimeout* timeout = kInfiniteTimeout) const;

    // NOTE:  closes the undelying LSOCK only if it is owned by this object!
    EIO_Status Close(void);

    // Specify if this "CListeningSocket" is to own the underlying "LSOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "LSOCK" and the system-specific socket handle
    LSOCK              GetLSOCK   (void) const;
    virtual EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const;

protected:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;

private:
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
    static const STimeout* SetSelectInternalRestartTimeout(const STimeout* t);
    static void         AllowSigPipe(void);
    static EIO_Status   Initialize  (void);
    static EIO_Status   Shutdown    (void);

    // Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    static ESwitch SetReadOnWrite       (ESwitch read_on_write);
    static ESwitch SetDataLogging       (ESwitch log);
    static ESwitch SetInterruptOnSignal (ESwitch interrupt);
    static ESwitch SetReuseAddress      (ESwitch reuse);

    // NOTE:  use CSocket::Wait() to wait for I/O event(s) on a single socket
    struct SPoll {
        CPollable* m_Pollable;
        EIO_Event  m_Event;
        EIO_Event  m_REvent;

        SPoll(CPollable* pollable = 0, EIO_Event event = eIO_Open)
            : m_Pollable(pollable), m_Event(event) { m_REvent = eIO_Open; }
    };
    static EIO_Status Poll(vector<SPoll>&  polls,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    // Misc  (mostly BSD-like); "host" ought to be in network byte order
    static string       gethostname  (void);                // empty str on err
    static string       ntoa         (unsigned int  host);
    static string       gethostbyaddr(unsigned int  host);  // empty str on err
    static unsigned int gethostbyname(const string& hostname);  // 0 on error

    static unsigned int   HostToNetLong (unsigned int   value);
    static unsigned int   NetToHostLong (unsigned int   value);
    static unsigned short HostToNetShort(unsigned short value);
    static unsigned short NetToHostShort(unsigned short value);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

inline EIO_Status CSocket::Shutdown(EIO_Event how)
{
    return m_Socket ? SOCK_Shutdown(m_Socket, how) : eIO_Closed;
}


inline EIO_Status CSocket::Wait(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_Wait(m_Socket, event, timeout) : eIO_Closed; 
}


inline EIO_Status CSocket::PushBack(const void* buf, size_t size)
{
    return m_Socket ? SOCK_PushBack(m_Socket, buf, size) : eIO_Closed;
}


inline EIO_Status CSocket::Abort(void)
{
    return m_Socket ? SOCK_Abort(m_Socket) : eIO_Closed;
}


inline EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed;
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


inline EIO_Status CSocket::GetOSHandle(void* hnd_buf, size_t hnd_siz) const
{
    return m_Socket? SOCK_GetOSHandle(m_Socket, hnd_buf, hnd_siz) : eIO_Closed;
}

 
inline ESwitch CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    return m_Socket? SOCK_SetReadOnWrite(m_Socket, read_on_write) : eDefault;
}


inline ESwitch CSocket::SetDataLogging(ESwitch log)
{
    return m_Socket? SOCK_SetDataLogging(m_Socket, log) : eDefault;
}


inline ESwitch CSocket::SetInterruptOnSignal(ESwitch interrupt)
{
    return m_Socket? SOCK_SetInterruptOnSignal(m_Socket, interrupt) : eDefault;
}


inline void CSocket::SetReuseAddress(ESwitch reuse)
{
    if ( m_Socket )
        SOCK_SetReuseAddress(m_Socket, reuse);
}


inline void CSocket::DisableOSSendDelay(bool on_off)
{
    if ( m_Socket)
        SOCK_DisableOSSendDelay(m_Socket, on_off);
}


inline bool CSocket::IsClientSide(void) const
{
    return m_Socket && SOCK_IsClientSide(m_Socket) ? true : false;
}


inline bool CSocket::IsServerSide(void) const
{
    return m_Socket && SOCK_IsServerSide(m_Socket) ? true : false;
}


inline bool CSocket::IsDatagram(void) const
{
    return m_Socket && SOCK_IsDatagram(m_Socket) ? true : false;
}


inline bool CSocket::IsUNIX(void) const
{
    return m_Socket && SOCK_IsUNIX(m_Socket) ? true : false;
}


/////////////////////////////////////////////////////////////////////////////
//  CDatagramSocket::
//

inline EIO_Status CDatagramSocket::Wait(const STimeout* timeout)
{
    return m_Socket ? DSOCK_WaitMsg(m_Socket, timeout) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::Clear(EIO_Event direction)
{
    return m_Socket ? DSOCK_WipeMsg(m_Socket, direction) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::SetBroadcast(bool do_broadcast)
{
    return m_Socket ? DSOCK_SetBroadcast(m_Socket, do_broadcast) : eIO_Closed;
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


inline const STimeout* CSocketAPI::SetSelectInternalRestartTimeout
(const STimeout* tmo)
{
    return SOCK_SetSelectInternalRestartTimeout(tmo);
}


inline EIO_Status CSocketAPI::Initialize(void)
{
    return SOCK_InitializeAPI();
}


inline EIO_Status CSocketAPI::Shutdown(void)
{
    return SOCK_ShutdownAPI();
}


inline ESwitch CSocketAPI::SetReadOnWrite(ESwitch read_on_write)
{
    return SOCK_SetReadOnWriteAPI(read_on_write);
}


inline ESwitch CSocketAPI::SetDataLogging(ESwitch log)
{
    return SOCK_SetDataLoggingAPI(log);
}


inline ESwitch CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    return SOCK_SetInterruptOnSignalAPI(interrupt);
}


inline ESwitch CSocketAPI::SetReuseAddress(ESwitch reuse)
{
    return SOCK_SetReuseAddressAPI(reuse);
}


inline unsigned int CSocketAPI::HostToNetLong(unsigned int value)
{
    return SOCK_HostToNetLong(value);
}


inline unsigned int CSocketAPI::NetToHostLong(unsigned int value)
{
    return SOCK_NetToHostLong(value);
}


inline unsigned short CSocketAPI::HostToNetShort(unsigned short value)
{
    return SOCK_HostToNetShort(value);
}


inline unsigned short CSocketAPI::NetToHostShort(unsigned short value)
{
    return SOCK_NetToHostShort(value);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.42  2005/03/09 15:04:16  lavr
 * Remove CDatagramSocket::Send(...unsigned int host = 0...)
 *
 * Revision 6.41  2005/03/02 16:11:46  lavr
 * Extend CDatagramSocket::Connect and Send with addtl signatures
 *
 * Revision 6.40  2004/11/09 21:13:00  lavr
 * +ReadLine
 *
 * Revision 6.39  2004/10/26 14:45:53  lavr
 * <ncbi_socket.hpp> -> <ncbi_socket_unix.hpp>
 *
 * Revision 6.38  2004/10/19 18:13:06  lavr
 * +CSocket::DisableOSSendDelay()
 *
 * Revision 6.37  2004/07/23 19:05:13  lavr
 * CListeningSocket(), CListeningSocket::Listen() to accept flags
 *
 * Revision 6.36  2003/11/25 15:07:40  lavr
 * CSocket::GetStatus() to pass eIO_Open to SOCK_Status()
 *
 * Revision 6.35  2003/11/24 19:22:24  lavr
 * SetSelectInternalRestartTimeout() to accept ptr to STimeout
 *
 * Revision 6.34  2003/11/18 20:18:49  lavr
 * +SetSelectInternalRestartTimeout()
 *
 * Revision 6.33  2003/11/12 17:43:53  lavr
 * Few (non-functional) rearrangements
 *
 * Revision 6.32  2003/10/24 16:51:11  lavr
 * GetTimeout(eIO_ReadWrite): return the lesser of eIO_Read and eIO_Write
 *
 * Revision 6.31  2003/10/23 12:14:52  lavr
 * Socket feature setters made returning old feature values
 *
 * Revision 6.30  2003/10/09 19:58:26  vakatov
 * Fine-tune the set of included CORELIB headers, mostly to avoid the weird
 * dependence of XNCBI library on ICC 7.1.
 *
 * Revision 6.29  2003/08/25 14:37:07  lavr
 * Introduce CPollable and more generic SCocketAPI::Poll()
 *
 * Revision 6.28  2003/07/15 16:49:16  lavr
 * +CSocket::GetPeerAddress(void)
 *
 * Revision 6.27  2003/05/31 05:12:09  lavr
 * Fix return value type of HostToNetShort and NetToHostShort
 *
 * Revision 6.26  2003/05/21 17:51:50  lavr
 * SOCK_Abort() redeclared protected in CDatagramSocket
 *
 * Revision 6.25  2003/05/20 21:24:55  lavr
 * +CSocket::Abort()
 *
 * Revision 6.24  2003/05/14 03:46:44  lavr
 * Match revised datagram socket API
 *
 * Revision 6.23  2003/04/30 17:03:22  lavr
 * Modified prototypes for CDatagramSocket::Send() and CDatagramSocket::Recv()
 *
 * Revision 6.22  2003/04/11 20:58:12  lavr
 * CDatagramSocket:: API defined completely
 *
 * Revision 6.21  2003/04/09 19:05:55  siyan
 * Added doxygen support
 *
 * Revision 6.20  2003/02/20 17:55:09  lavr
 * Inlining CSocket::Shutdown() and CSocket::Wait()
 *
 * Revision 6.19  2003/02/14 22:03:32  lavr
 * Add internal CSocket timeouts and document them
 *
 * Revision 6.18  2003/01/28 19:29:07  lavr
 * DSOCK: Remove reference to kEmptyStr and thus to xncbi
 *
 * Revision 6.17  2003/01/26 04:28:37  lavr
 * Quick fix CDatagramSocket ctor ambiguity
 *
 * Revision 6.16  2003/01/24 23:01:32  lavr
 * Added class CDatagramSocket
 *
 * Revision 6.15  2003/01/07 22:01:43  lavr
 * ChangeLog message corrected
 *
 * Revision 6.14  2003/01/07 21:58:24  lavr
 * Comment corrected
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
