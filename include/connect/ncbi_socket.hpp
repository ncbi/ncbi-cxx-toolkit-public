#ifndef CONNECT___NCBI_SOCKET__HPP
#define CONNECT___NCBI_SOCKET__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   C++ wrapper for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *     NOTE:  for more details and documentation see "ncbi_socket.h"
 *     CSocket
 *     CListeningSocket
 *     CSocketAPI
 *
 * ---------------------------------------------------------------------------
 */

#include <corelib/ncbistr.hpp>
#include <connect/ncbi_socket_unix.h>
#include <string>


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
    virtual ~CPollable() { };

private:
    /// disable copy constructor and assignment
    CPollable(const CPollable&);
    CPollable& operator= (const CPollable&);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CTrigger::
///
/// @li <b>NOTE:</b>  For documentation see TRIGGER_***() functions in
/// "ncbi_socket.h".
///

class NCBI_XCONNECT_EXPORT CTrigger : public CPollable
{
public:
    CTrigger(ESwitch log = eDefault);
    virtual ~CTrigger();

    EIO_Status Set(void);
    EIO_Status IsSet(void);
    EIO_Status Reset(void);

    EIO_Status GetStatus(void) const;

    // Access to the underlying "TRIGGER" and the system-specific handle.
    TRIGGER            GetTRIGGER (void) const;

    virtual EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const;

protected:
    TRIGGER    m_Trigger;

private:
    /// disable copy constructor and assignment
    CTrigger(const CTrigger&);
    CTrigger& operator= (const CTrigger&);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CSocket::
///
/// @li <b>NOTE:</b>  For documentation see SOCK_***() functions in
/// "ncbi_socket.h".
///        Initially, all timeouts are infinite.
///

class NCBI_XCONNECT_EXPORT CSocket : public CPollable
{
public:
    CSocket(void);

    /// Create a client-side socket connected to "host:port".
    /// @li <b>NOTE 1:</b>  the created underlying "SOCK" will be owned by the
    /// "CSocket";
    /// @li <b>NOTE 2:</b>  timeout from the argument becomes new eIO_Open 
    /// timeout.
    ///
    /// @param host
    ///
    /// @param port
    ///  always in host byte order
    /// @param timeout
    ///
    /// @param log
    ///
    CSocket(const string&   host,
            unsigned short  port,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Variant of the above, which takes host as a binary value in 
    /// network b.o.
    /// @param host
    ///  network byte order
    /// @param port
    ///  host byte order
    /// @param timeout
    ///
    /// @param log
    ///
    CSocket(unsigned int    host,
            unsigned short  port,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Call Close(), then self-destruct
    virtual ~CSocket(void);

    /// @param direction
    ///  is one of
    ///     - eIO_Open  - return eIO_Success if CSocket is okay and open,
    ///                   eIO_Closed if closed by Close() or not yet open;
    ///     - eIO_Read  - status of last read operation;
    ///     - eIO_Write - status of last write operation.
    /// Direction eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    EIO_Status GetStatus(EIO_Event direction) const;

    /// Connect to "host:port".
    /// @li <b>NOTE 1:</b>  should not be called if already connected;
    /// @li <b>NOTE 2:</b>  timeout from the argument becomes new eIO_Open
    /// timeout.
    ///
    /// @param host
    ///
    /// @param port
    ///  always in host byte order
    /// @param timeout
    ///
    /// @param log
    ///
    EIO_Status Connect(const string&   host,
                       unsigned short  port,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Reconnect to the same address.
    /// @li <b>NOTE 1:</b>   the socket must not be closed by the time this
    /// call is made;
    /// @li <b>NOTE 2:</b> not for the sockets created by
    /// CListeningSocket::Accept();
    /// @li <b>NOTE 3:</b>   timeout from the argument becomes new eIO_Open
    /// timeout.
    ///
    /// @param timeout
    ///
    EIO_Status Reconnect(const STimeout* timeout = kDefaultTimeout);

    /// @param how
    ///
    EIO_Status Shutdown(EIO_Event how);

    /// @li <b>NOTE:</b>  closes the undelying SOCK only if it is owned by
    /// this "CSocket"!
    ///
    EIO_Status Close(void);

    /// @li <b>NOTE:</b>  use CSocketAPI::Poll() to wait on several sockets
    /// at once
    ///
    /// @param event
    ///
    /// @param timeout
    ///
    EIO_Status Wait(EIO_Event       event,
                    const STimeout* timeout);

    /// @li <b>NOTE 1:</b>  by default, initially all timeouts are infinite;
    /// @li <b>NOTE 2:</b>   SetTimeout(..., kDefaultTimeout) has no effect;
    /// @li <b>NOTE 3:</b>   GetTimeout(eIO_ReadWrite) returns the least
    ///          of eIO_Read and eIO_Write ones.
    ///
    /// @param  event
    ///
    /// @param timeout
    ///
    EIO_Status      SetTimeout(EIO_Event       event,
                               const STimeout* timeout);
    const STimeout* GetTimeout(EIO_Event event) const;

    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_read
    ///
    /// @param how
    ///
    EIO_Status Read(void*          buf,
                    size_t         size,
                    size_t*        n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    /// Read up to '\r\n', '\n' or '\0', discarding any of the EOLs
    /// @param str
    ///
    EIO_Status ReadLine(string& str);

    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_read
    ///
    EIO_Status ReadLine(char*   buf,
                        size_t  size,
                        size_t* n_read = 0);

    /// @param buf
    ///
    /// @param size
    ///
    EIO_Status PushBack(const void* buf,
                        size_t      size);

    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_written
    ///
    /// @param how
    ///
    EIO_Status Write(const void*     buf,
                     size_t          size,
                     size_t*         n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    EIO_Status Abort(void);

    /// @param byte_order
    ///
    unsigned short GetLocalPort(ENH_ByteOrder byte_order) const;

    /// @li  <b>NOTE 1:</b>  either of "host", "port" can be NULL to opt out
    ///          from obtaining the corresponding value;
    /// @li  <b>NOTE 2:</b> both "*host" and "*port" come out in the same
    ///          byte order requested by the third argument.
    ///
    /// @param host
    ///
    /// @param port
    ///
    /// @param byte_order
    ///
    void GetPeerAddress(unsigned int*   host,
                        unsigned short* port,
                        ENH_ByteOrder   byte_order) const;
    /// @return
    ///  Textual string representing the peer's address
    string GetPeerAddress(void) const;

    /// Specify if this "CSocket" is to own the underlying "SOCK".
    /// @return
    ///  Previous ownership mode.
    /// @param if_to_own
    ///
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "SOCK" and the system-specific socket handle.
    SOCK               GetSOCK    (void) const;

    /// @param handle_buf
    ///
    /// @param handle_size
    ///
    virtual EIO_Status GetOSHandle(void*  handle_buf,
                                   size_t handle_size) const;

    /// @li <b>NOTE:</b>  use CSocketAPI::SetReadOnWrite() to set the default
    /// value
    ///
    /// @param read_on_write
    ///
    ESwitch SetReadOnWrite(ESwitch read_on_write = eOn);

    /// @li <b>NOTE:</b>  use CSocketAPI::SetDataLogging() to set the default
    /// value
    ///
    /// @param log
    ///
    ESwitch SetDataLogging(ESwitch log = eOn);

    /// @li <b>NOTE:</b>  use CSocketAPI::SetInterruptOnSignal() to set the
    /// default value
    ///
    /// @param interrupt
    ///
    ESwitch SetInterruptOnSignal(ESwitch interrupt = eOn);

    /// @li <b>NOTE:</b>  see comments for SOCK_SetReuseAddress() in
    /// ncbi_socket.h
    ///
    /// @param reuse
    ///
    void    SetReuseAddress(ESwitch reuse = eOff);

    /// @li <b>NOTE:</b>  see comments for SOCK_DisableOSSendDelay() in
    /// ncbi_socket.h
    ///
    /// @param on_off
    ///
    void    DisableOSSendDelay(bool on_off = true);

    bool IsClientSide(void) const;
    bool IsServerSide(void) const;
    bool IsDatagram  (void) const;
    bool IsUNIX      (void) const;
    bool IsSecure    (void) const;

    /// Close the current underlying "SOCK" (if any, and if owned),
    /// and from now on use "sock" as the underlying "SOCK" instead.
    /// @li <b>NOTE:</b>  "if_to_own" applies to the (new) "sock"
    ///
    /// @param sock
    ///
    /// @param if_to_own
    ///
    /// @param whence
    ///
    void Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence);

protected:
    SOCK       m_Socket;
    EOwnership m_IsOwned;

    ///< Timeouts 

    /// eIO_Open
    STimeout*  o_timeout;
    /// eIO_Read
    STimeout*  r_timeout;
    /// eIO_Write
    STimeout*  w_timeout;
    /// eIO_Close
    STimeout*  c_timeout;
    /// storage for o_timeout
    STimeout  oo_timeout;
    /// storage for r_timeout
    STimeout  rr_timeout;
    /// storage for w_timeout
    STimeout  ww_timeout;
    /// storage for c_timeout
    STimeout  cc_timeout;

private:
    /// disable copy constructor and assignment
    CSocket(const CSocket&);
    CSocket& operator= (const CSocket&);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDatagramSocket::
///
///    Datagram socket
///
/// @li <b>NOTE:</b>  for documentation see DSOCK_***() functions in
/// "ncbi_socket.h"
///

class NCBI_XCONNECT_EXPORT CDatagramSocket : public CSocket
{
public:
    /// @li <b>NOTE:</b>  the created underlying "SOCK" will be owned by the
    /// object
    ///
    /// @param do_log
    ///
    CDatagramSocket(TSOCK_Flags flags = fSOCK_LogDefault);

    /// @param port
    ///
    EIO_Status Bind(unsigned short port);

  
    /// @param host
    ///
    /// @param port
    ///  "port" is in host byte order
    EIO_Status Connect(const string& host, unsigned short port);

    
    /// @param host
    ///  "host" is accepted in network byte order
    /// @param port
    ///  "port" is in host one
    EIO_Status Connect(unsigned int host, unsigned short port);

    /// @param timeout
    ///
    EIO_Status Wait(const STimeout* timeout = kInfiniteTimeout);

    /// @param data
    ///
    /// @param datalen
    ///
    /// @param host
    ///
    /// @param port
    ///  host byte order
    EIO_Status Send(const void*     data    = 0,
                    size_t          datalen = 0,
                    const string&   host    = string(),
                    unsigned short  port    = 0);

    /// @param buf
    ///
    /// @param buflen
    ///
    /// @param msglen
    ///
    /// @param sender_host
    ///
    /// @param sender_port
    ///
    /// @param maxmsglen
    ///
    EIO_Status Recv(void*           buf         = 0,
                    size_t          buflen      = 0,
                    size_t*         msglen      = 0,
                    string*         sender_host = 0,
                    unsigned short* sender_port = 0,
                    size_t          maxmsglen   = 0);

    /// @param direction
    ///
    EIO_Status Clear(EIO_Event direction);

    /// @param do_broadcast
    ///
    EIO_Status SetBroadcast(bool do_broadcast = true);

protected:
    /// @li <b>NOTE:</b> these calls are not valid with datagram sockets
    ///
    /// @param how
    ///
    EIO_Status Shutdown(EIO_Event how);

    /// @param timeout
    ///
    EIO_Status Reconnect(const STimeout* timeout);

    EIO_Status Abort(void);

private:
    /// disable copy constructor and assignment
    CDatagramSocket(const CDatagramSocket&);
    CDatagramSocket& operator= (const CDatagramSocket&);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CListeningSocket::
///
///    Listening socket (to accept connections on the server side)
///
/// @li <b>NOTE:</b>  for documentation see LSOCK_***() functions in
/// "ncbi_socket.h"
///

class NCBI_XCONNECT_EXPORT CListeningSocket : public CPollable
{
public:
    CListeningSocket(void);

    /// @li <b>NOTE:</b>  "port" ought to be in host byte order
    ///
    /// @param port
    ///
    /// @param backlog
    ///
    /// @param flags
    ///
    CListeningSocket(unsigned short port,
                     unsigned short backlog = 5,
                     TSOCK_Flags    flags   = fSOCK_LogDefault);

    /// Call Close(), then self-destruct
    virtual ~CListeningSocket(void);

    /// Return eIO_Success if CListeningSocket is opened and bound;
    /// Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    /// @li <b>NOTE:</b>  "port" ought to be in host byte order
    ///
    /// @param port
    ///
    /// @param backlog
    ///
    /// @param flags
    ///
    EIO_Status Listen(unsigned short port,
                      unsigned short backlog = 5,
                      TSOCK_Flags    flags   = fSOCK_LogDefault);

    /// @li <b>NOTE:</b> the created "CSocket" will own its underlying "SOCK"
    ///
    /// @param sock
    ///
    /// @param timeout
    ///
    EIO_Status Accept(CSocket*&       sock,
                      const STimeout* timeout = kInfiniteTimeout) const;

    /// @param sock
    ///
    /// @param timeout
    ///
    EIO_Status Accept(CSocket&        sock,
                      const STimeout* timeout = kInfiniteTimeout) const;

    /// @li <b>NOTE:</b>  closes the undelying LSOCK only if it is owned by
    /// this object!
    ///
    EIO_Status Close(void);

    /// Specify if this "CListeningSocket" is to own the underlying "LSOCK".
    /// @param if_to_own
    ///
    /// @return
    ///  Previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    /// Access to the underlying "LSOCK" and the system-specific socket
    /// handle
    LSOCK              GetLSOCK   (void) const;

    /// @param handle_buf
    ///
    /// @param handle_size
    ///
    virtual EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const;

protected:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;

private:
    /// disable copy constructor and assignment
    CListeningSocket(const CListeningSocket&);
    CListeningSocket& operator= (const CListeningSocket&);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CSocketAPI::
///
///    Global settings related to the socket API
///
/// @li <b>NOTE:</b>  for documentation see SOCK_***() functions in
/// "ncbi_socket.h"
///

class NCBI_XCONNECT_EXPORT CSocketAPI
{
public:
    /// Generic
    /// @param t
    ///
    static const STimeout* SetSelectInternalRestartTimeout(const STimeout* t);
    static void         AllowSigPipe(void);
    static EIO_Status   Initialize  (void);
    static EIO_Status   Shutdown    (void);

    /// Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    /// @param read_on_write
    ///
    static ESwitch SetReadOnWrite(ESwitch read_on_write);

    /// @param log
    ///
    static ESwitch SetDataLogging(ESwitch log);

    /// @param interrupt
    ///
    static ESwitch SetInterruptOnSignal(ESwitch interrupt);

    /// @param reuse
    ///
    static ESwitch SetReuseAddress(ESwitch reuse);

    /// @li <b>NOTE:</b>  use CSocket::Wait() to wait for I/O event(s) on a
    /// single socket
    ///
    /// @param m_Pollable
    ///
    /// @param m_Event
    ///
    /// @param m_REvent
    ///
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

    /// Misc  (mostly BSD-like); "host" ought to be in network byte order
    /// empty str on err
    static string gethostname(void);

    static string ntoa(unsigned int  host);
    bool          isip(const string& host);

    /// empty str on err
    static string       gethostbyaddr(unsigned int  host);
    /// 0 on error
    static unsigned int gethostbyname(const string& hostname);

    static unsigned int   HostToNetLong (unsigned int   value);
    static unsigned int   NetToHostLong (unsigned int   value);
    static unsigned short HostToNetShort(unsigned short value);
    static unsigned short NetToHostShort(unsigned short value);

    /// Loopback address gets returned in network byte order
    static unsigned int   GetLoopbackAddress(void);

    /// Local host address in network byte order (cached for faster retrieval)
    static unsigned int   GetLocalHostAddress(ESwitch reget = eDefault);

    /// See SOCK_HostPortToString()
    static string         HostPortToString(unsigned int   host,
                                           unsigned short port);

    /// Return position past the end of parsed portion, NPOS on error
    static SIZE_TYPE      StringToHostPort(const string&   str,
                                           unsigned int*   host,
                                           unsigned short* port);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
///  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
/// CTrigger::
///

inline EIO_Status CTrigger::GetStatus(void) const
{
    return m_Trigger ? eIO_Success : eIO_Closed;
}


inline EIO_Status CTrigger::Set(void)
{
    return m_Trigger ? TRIGGER_Set(m_Trigger) : eIO_Unknown;
}


inline EIO_Status CTrigger::IsSet(void)
{
    return m_Trigger ? TRIGGER_IsSet(m_Trigger) : eIO_Unknown;
}


inline EIO_Status CTrigger::Reset(void)
{
    return m_Trigger ? TRIGGER_Reset(m_Trigger) : eIO_Unknown;
}


inline TRIGGER    CTrigger::GetTRIGGER(void) const
{
    return m_Trigger;
}


inline EIO_Status CTrigger::GetOSHandle(void*, size_t) const
{
    return m_Trigger ? eIO_NotSupported : eIO_Closed;
}



/////////////////////////////////////////////////////////////////////////////
/// CSocket::
///

inline EIO_Status CSocket::Shutdown(EIO_Event how)
{
    return m_Socket ? SOCK_Shutdown(m_Socket, how) : eIO_Closed;
}


inline EIO_Status CSocket::Wait(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_Wait(m_Socket, event, timeout) : eIO_Closed;
}


inline EIO_Status CSocket::ReadLine(char* buf, size_t size, size_t* n_read)
{
    return m_Socket ? SOCK_ReadLine(m_Socket, buf, size, n_read) : eIO_Closed;
}


inline EIO_Status CSocket::PushBack(const void* buf, size_t size)
{
    return m_Socket ? SOCK_PushBack(m_Socket, buf, size) : eIO_Closed;
}


inline EIO_Status CSocket::Abort(void)
{
    return m_Socket ? SOCK_Abort(m_Socket) : eIO_Closed;
}


inline unsigned short CSocket::GetLocalPort(ENH_ByteOrder byte_order) const
{
    return m_Socket ? SOCK_GetLocalPort(m_Socket, byte_order) : 0;
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


inline bool CSocket::IsSecure(void) const
{
    return m_Socket && SOCK_IsSecure(m_Socket) ? true : false;
}


/////////////////////////////////////////////////////////////////////////////
///  CDatagramSocket::
///

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
///  CListeningSocket::
///

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
///  CSocketAPI::
///

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


inline bool CSocketAPI::isip(const string& host)
{
    return SOCK_isip(host.c_str()) ? true : false;
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


inline unsigned int CSocketAPI::GetLoopbackAddress(void)
{
    return SOCK_GetLoopbackAddress();
}


inline unsigned int CSocketAPI::GetLocalHostAddress(ESwitch reget)
{
    return SOCK_GetLocalHostAddress(reget);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

#endif /* CONNECT___NCBI_SOCKET__HPP */
