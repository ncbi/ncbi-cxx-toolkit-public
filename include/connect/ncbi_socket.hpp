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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   C++ wrapper for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *     NOTE:  for more details and documentation see "ncbi_socket.h"
 *
 * ---------------------------------------------------------------------------
 */

#include <corelib/ncbistl.hpp>
#include <connect/ncbi_socket_unix.h>
#include <cstring>
#include <string>
#include <vector>

#ifdef NCBI_DEPRECATED
#  define NCBI_XSOCK_DEPRECATED  NCBI_DEPRECATED
#else
#  define NCBI_XSOCK_DEPRECATED
#endif // NCBI_DEPRECATED


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
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size,
                           EOwnership ownership = eNoOwnership) const = 0;

    virtual
    POLLABLE   GetPOLLABLE(void) const = 0;

    virtual ~CPollable() { }

protected:
    CPollable(void) { }

private:
    // disable copy constructor and assignment
    CPollable(const CPollable&) = delete;
    CPollable& operator= (const CPollable&) = delete;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CTrigger::
///
/// @note  For documentation see TRIGGER_***() functions in "ncbi_socket.h".
///

class NCBI_XCONNECT_EXPORT CTrigger : public CPollable
{
public:
    CTrigger(ESwitch log = eDefault);
    virtual ~CTrigger();

    EIO_Status GetStatus(void) const;

    EIO_Status Set(void);
    EIO_Status IsSet(void);
    EIO_Status Reset(void);

    /// Access to the system-specific handle.
    virtual
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /// Access to the underlying "TRIGGER".
    TRIGGER GetTRIGGER(void) const;

    virtual
    POLLABLE GetPOLLABLE(void) const {return POLLABLE_FromTRIGGER(m_Trigger);}

protected:
    TRIGGER m_Trigger;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CNCBI_Addr::
///
/// IPv4/IPv6 address container / converter
///

class CNCBI_IPAddr
{
public:
    CNCBI_IPAddr(void)
    {
        Clear();
    }

    // "ipv4" is in network byte order
    CNCBI_IPAddr(unsigned int ipv4)
    {
        NcbiIPv4ToIPv6(&m_IPAddr, ipv4, 0);
    }

    CNCBI_IPAddr(const TNCBI_IPv6Addr& ipv6)
    {
        m_IPAddr = ipv6;
    }

    // returned ipv4 is in network byte order
    operator unsigned int  (void) const
    {
        return NcbiIPv6ToIPv4(&m_IPAddr, 0);
    }

    operator TNCBI_IPv6Addr(void) const
    {
        return m_IPAddr;
    }

    explicit operator bool(void) const
    {
        return !IsEmpty();
    }

    bool operator !(void) const
    {
        return IsEmpty();
    }

    void Clear(void)
    {
        memset(&m_IPAddr, 0, sizeof(m_IPAddr));
    }

    bool IsEmpty(void) const
    {
        return NcbiIsEmptyIPv6(&m_IPAddr);
    }

    const TNCBI_IPv6Addr& GetAddr(void) const
    {
        return  m_IPAddr;
    }

    TNCBI_IPv6Addr* GetAddrPtr(void)
    {
        return &m_IPAddr;
    }

private:
    TNCBI_IPv6Addr  m_IPAddr;
};

NCBI_XSOCK_DEPRECATED
inline bool operator==(const CNCBI_IPAddr& lhs, unsigned int rhs)
{
    return unsigned(lhs) == rhs/*network byte order*/;
}

NCBI_XSOCK_DEPRECATED
inline bool operator!=(const CNCBI_IPAddr& lhs, unsigned int rhs)
{
    return unsigned(lhs) != rhs/*network byte order*/;
}

NCBI_XSOCK_DEPRECATED
inline bool operator==(unsigned int lhs, const CNCBI_IPAddr& rhs)
{
    return lhs/*network byte order*/ == unsigned(rhs);
}

NCBI_XSOCK_DEPRECATED
inline bool operator!=(unsigned int lhs, const CNCBI_IPAddr& rhs)
{
    return lhs/*network byte order*/ != unsigned(rhs);
}



/////////////////////////////////////////////////////////////////////////////
///
///  CSocket::
///
/// @note  For documentation see SOCK_***() functions in "ncbi_socket.h".
///
/// Initially, all timeouts are infinite.
///

class NCBI_XCONNECT_EXPORT CSocket : public CPollable
{
public:
    CSocket(void);

    /// Create a client-side socket connected to "host:port".
    /// @note  The created underlying "SOCK" will be owned by the "CSocket";
    /// @note  Timeout from the argument becomes new eIO_Open timeout.
    ///
    /// @param host
    ///  host name (or text IP address) of the host to connect to
    /// @param port
    ///  port number to connect at "host", always in host byte order
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///  CSocket::Connect, SOCK_Create
    CSocket(const string&   host,
            unsigned short  port,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Create a client-side socket connected to "host:port".
    /// @note  The created underlying "SOCK" will be owned by the "CSocket";
    /// @note  Timeout from the argument becomes new eIO_Open timeout.
    ///
    /// @param hostport
    ///  "host:port" as a string (the host part must be in [] if bare IPv6)
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///  CSocket::Connect, SOCK_Create
    CSocket(const string&   hostport,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Variant of the above, which takes an arbitrary IP address
    /// @param addr
    ///  an IP address (IPv4/IPv6)
    /// @param port
    ///  host byte order
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///   CSocket::Connect, SOCK_Create
    CSocket(const CNCBI_IPAddr& addr,
            unsigned short      port,
            const STimeout*     timeout = kInfiniteTimeout,
            TSOCK_Flags         flags   = fSOCK_LogDefault);

    /// Call Close(), then self-destruct
    virtual ~CSocket(void);

    /// Return status of *last* I/O operation without making any actual I/O.
    /// @param direction
    ///  is one of
    ///     - eIO_Open  - return eIO_Success if CSocket has been opened,
    ///                   eIO_Timeout if CSocket is still pending connect,
    ///                   eIO_Closed if closed by Close() or not yet open;
    ///     - eIO_Read  - status of last read operation;
    ///     - eIO_Write - status of last write operation.
    /// Directions eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    /// @sa
    ///  SOCK_Status
    EIO_Status GetStatus(EIO_Event direction) const;

    /// Connect to "host:port".
    /// @note  Should not be called if already connected;
    /// @note  Timeout from the argument becomes new eIO_Open timeout.
    ///
    /// @param host
    ///  host name (or text IP address) of the host to connect to
    /// @param port
    ///  port number to connect at "host", always in host byte order
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///  SOCK_Create
    EIO_Status Connect(const string&   host,
                       unsigned short  port,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Connect to "host:port".
    /// @note  Should not be called if already connected;
    /// @note  Timeout from the argument becomes new eIO_Open timeout.
    ///
    /// @param hostport
    ///  "host:port" as a string (the host part must be in [] if bare IPv6)
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///  SOCK_Create
    EIO_Status Connect(const string&   hostport,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);

    /// Variant of the above, which takes an arbitrary IP address
    /// @param addr
    ///  an IP address (IPv4/IPv6)
    /// @param port
    ///  host byte order
    /// @param timeout
    ///  maximal time to wait for connection to be established
    /// @param flags
    ///  additional socket properties (including logging)
    /// @sa
    ///   CSocket::Connect, SOCK_Create
    EIO_Status Connect(const CNCBI_IPAddr& addr,
                       unsigned short      port,
                       const STimeout*     timeout = kInfiniteTimeout,
                       TSOCK_Flags         flags   = fSOCK_LogDefault);

    /// Reconnect to the same address.
    /// @note  The socket must not be closed by the time this call is made;
    /// @note  Not for the sockets created by CListeningSocket::Accept();
    /// @note  Timeout from the argument becomes new eIO_Open timeout.
    ///
    /// @param timeout
    ///  maximal time to reconnect
    /// @sa
    ///  SOCK_Reconnect
    EIO_Status Reconnect(const STimeout* timeout = kDefaultTimeout);

    /// Shutdown socket I/O in the specified direction.
    /// @param how
    ///  either of eIO_Read, eIO_Write, or eIO_ReadWrite
    /// @sa
    ///  SOCK_Shutdown
    EIO_Status Shutdown(EIO_Event how);

    /// Close socket.
    /// @note  Closes the underlying SOCK only if it is owned by this "CSocket"!
    /// @sa
    ///  SOCK_CloseEx
    EIO_Status Close(void);

    /// Wait for I/O availability in the socket.
    /// @note  Use CSocketAPI::Poll() to wait for multiple sockets at once.
    ///
    /// @param event
    ///  either of eIO_Read, eIO_Write or eIO_ReadWrite
    /// @param timeout
    ///  maximal time to wait for I/O to become ready
    /// @sa
    ///  SOCK_Wait
    EIO_Status Wait(EIO_Event       event,
                    const STimeout* timeout);

    /// Set timeout for I/O in the specified direction.
    /// @note  Initially all timeouts are infinite by default;
    /// @note  SetTimeout(..., kDefaultTimeout) has no effect and results
    ///        eIO_Success unconditionally.
    /// @param event
    ///  either of eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite, or eIO_Close
    /// @param timeout
    ///  new value to set (including special kInfiniteTimeout)
    /// @sa
    ///  CSocket::GetTimeout, SOCK_SetTimeout, SOCK_GetTimeout
    EIO_Status      SetTimeout(EIO_Event       event,
                               const STimeout* timeout);

    /// Get timeout for I/O in the specified direction.
    /// @param event
    ///  either of eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite, or eIO_Close
    /// @return
    ///  A timeout value set (or defaulted) in the "event" direction; or
    ///  kDefaultTimeout for an unrecognized "event" value
    /// @note  GetTimeout(eIO_ReadWrite) returns the least
    ///        of eIO_Read and eIO_Write ones.
    /// @sa
    ///  CSocket::SetTimeout, SOCK_GetTimeout
    const STimeout* GetTimeout(EIO_Event event) const;

    /// Read from socket.
    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_read
    ///
    /// @param how
    ///
    /// @sa
    ///  SOCK_Read
    EIO_Status Read(void*          buf,
                    size_t         size,
                    size_t*        n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    /// Read a line from socket (up to CR-LF, LF, or null character,
    /// discarding any of the EOLs).
    /// @param str
    ///
    /// @sa
    ///  SOCK_ReadLine
    EIO_Status ReadLine(string& str);

    /// Read a line from socket (up to CR-LF, LF, or null character,
    /// discarding any of the EOLs).
    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_read
    ///
    /// @sa
    ///  SOCK_ReadLine
    EIO_Status ReadLine(char*   buf,
                        size_t  size,
                        size_t* n_read = 0);

    /// Push back data to socket (to be read out first).
    /// @param buf
    ///
    /// @param size
    ///
    /// @sa
    ///  SOCK_Pushback
    EIO_Status Pushback(const void* buf,
                        size_t      size);

    /// Write to socket.
    /// @param buf
    ///
    /// @param size
    ///
    /// @param n_written
    ///
    /// @param how
    ///
    /// @sa
    ///  SOCK_Write
    EIO_Status Write(const void*     buf,
                     size_t          size,
                     size_t*         n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    /// Abort socket connection.
    /// @sa
    ///  SOCK_Abort
    EIO_Status Abort(void);

    /// Get socket local port number.
    /// @param byte_order
    ///
    /// @sa
    ///  SOCK_GetLocalPort
    unsigned short GetLocalPort(ENH_ByteOrder byte_order = eNH_HostByteOrder,
                                bool trueport = false) const;

    /// Get socket remote port number.
    /// @param byte_order
    ///
    /// @sa
    ///  SOCK_GetRemotePort
    unsigned short GetRemotePort(ENH_ByteOrder byte_order = eNH_HostByteOrder) const;

    /// Get peer address.
    /// @note  Either of "host", "port" can be NULL to opt out
    ///        from obtaining the corresponding value;
    /// @note  Both "*host" and "*port" come out in the same
    ///        byte order as requested by the last argument.
    ///
    /// @param host
    ///  returned in requested byte order [can be NULL not to obtain]
    /// @param port
    ///  returned in requested byte order [can be NULL not to obtain]
    /// @param byte_order
    ///
    /// @sa
    ///  SOCK_GetPeerAddress
    NCBI_XSOCK_DEPRECATED
    void GetPeerAddress(unsigned int*   host,
                        unsigned short* port,
                        ENH_ByteOrder   byte_order) const;

    /// Get peer address.
    /// @note  Either of "addr", "port" can be NULL to opt out
    ///        from obtaining the corresponding value;
    /// @note  "byte_order" applies to "port" only
    ///
    /// @param addr
    ///                                    [can be NULL not to obtain]
    /// @param port
    ///   returned in requested byte order [can be NULL not to obtain]
    /// @param byte_order
    ///
    /// @sa
    ///  SOCK_GetPeerAddressIPv6
    void GetPeerAddress(CNCBI_IPAddr*   addr,
                        unsigned short* port,
                        ENH_ByteOrder   byte_order = eNH_HostByteOrder) const;

    /// Get peer address as a text string.
    /// @return
    ///  Text string representing the (parts of) peer's address
    /// @sa
    ///  SOCK_GetPeerAddressEx
    string GetPeerAddress(ESOCK_AddressFormat format = eSAF_Full) const;

    /// Access to the system-specific socket handle.
    /// @param handle_buf
    ///
    /// @param handle_size
    ///
    /// @sa
    ///  SOCK_GetOSHandleEx, CSocketAPI::OSHandleSize, SOCK_GetOSHandleSize
    virtual
    EIO_Status GetOSHandle(void*      handle_buf,
                           size_t     handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /// @note  Use CSocketAPI::SetReadOnWrite() to set the default value.
    ///
    /// @param read_on_write
    ///
    /// @sa
    ///  SOCK_SetReadOnWrite
    ESwitch SetReadOnWrite(ESwitch read_on_write = eOn);

    /// @note  Use CSocketAPI::SetInterruptOnSignal() to set the default value.
    ///
    /// @param interrupt
    ///
    /// @sa
    ///  SOCK_SetInterruptOnSignal
    ESwitch SetInterruptOnSignal(ESwitch interrupt = eOn);

    /// @note  See comments for SOCK_SetReuseAddress() in "ncbi_socket.h".
    ///
    /// @param reuse
    ///
    /// @sa
    ///  SOCK_SetReuseAddress
    void    SetReuseAddress(ESwitch reuse = eOn);

    /// @note  See comments for SOCK_DisableOSSendDelay() in "ncbi_socket.h".
    ///
    /// @param on_off
    ///
    /// @sa
    ///  SOCK_DisableOSSendDelay
    void    DisableOSSendDelay(bool on_off = true);

    /// @note  See comments for SOCK_SetCork() in "ncbi_socket.h".
    ///
    /// @param on_off
    ///
    /// @sa
    ///  SOCK_SetCork
    void    SetCork(bool on_off = true);

    /// @note  Use CSocketAPI::SetDataLogging() to set the default value.
    ///
    /// @param log
    ///
    /// @sa
    ///  SOCK_SetDataLogging
    ESwitch SetDataLogging(ESwitch log = eOn);

    bool IsDatagram  (void) const;
    bool IsClientSide(void) const;
    bool IsServerSide(void) const;
    bool IsUNIX      (void) const;
    bool IsSecure    (void) const;

    /// Positions and stats (direction is either eIO_Read or eIO_Write).
    TNCBI_BigCount GetPosition  (EIO_Event direction) const;
    TNCBI_BigCount GetCount     (EIO_Event direction) const;
    TNCBI_BigCount GetTotalCount(EIO_Event direction) const;

    /// Close the current underlying "SOCK" (if any, and if owned),
    /// and from now on use "sock" as the underlying "SOCK" instead.
    /// @note  "if_to_own" applies to the (new) "sock"
    ///
    /// @param sock
    ///
    /// @param if_to_own
    ///
    /// @param whence
    ///
    void Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence);

    /// Specify if this "CSocket" is to own the underlying "SOCK"
    /// @return
    ///  Previous ownership mode
    /// @param if_to_own
    ///
    EOwnership SetOwnership(EOwnership if_to_own);

    /// Access to the underlying "SOCK"
    SOCK GetSOCK(void) const;

    bool IsEmpty(void) const { return !m_Socket; }

    virtual
    POLLABLE GetPOLLABLE(void) const { return POLLABLE_FromSOCK(m_Socket); }

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
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDatagramSocket::
///
///    Datagram socket
///
/// @note  For documentation see DSOCK_***() functions in "ncbi_socket.h".
///

class NCBI_XCONNECT_EXPORT CDatagramSocket : public CSocket
{
public:
    /// @note  The created underlying "SOCK" will be owned by the object.
    ///
    /// @param do_log
    ///
    CDatagramSocket(TSOCK_Flags flags = fSOCK_LogDefault);

    /// @param port
    ///  port is in host byte order
    EIO_Status Bind(unsigned short port,
                    ESwitch        ipv6 = eDefault);
  
    /// @param host
    ///  host name or IP address as a string
    /// @param port
    ///  "port" is in host byte order
    EIO_Status Connect(const string&  host,
                       unsigned short port);

    /// @param hostport
    ///   "host:port" as a string (no-port and no-host accepted, such as ":0")
    EIO_Status Connect(const string& hostport);
    
    /// @param host
    ///  "host" is in network byte order
    /// @param port
    ///  "port" is in host byte order
    EIO_Status Connect(unsigned int   host,
                       unsigned short port);

    /// @param host
    ///  "addr" is an arbitrary IP address (IPv4/IPv6)
    /// @param port
    ///  "port" is in host byte order
    EIO_Status Connect(const CNCBI_IPAddr& addr,
                       unsigned short      port);

    /// @param timeout
    ///
    EIO_Status Wait(const STimeout* timeout = kInfiniteTimeout);

    /// @param buf
    ///
    /// @param buflen
    ///
    /// @param msglen
    ///
    /// @param sender_host
    ///  [can be NULL to not obtain]
    /// @param sender_port
    ///  [can be NULL to not obtain]
    /// @param maxmsglen
    ///
    EIO_Status Recv(void*           buf,
                    size_t          buflen,
                    size_t*         msglen      = 0,
                    string*         sender_host = 0,
                    unsigned short* sender_port = 0,
                    size_t          maxmsglen   = 0);

    /// @param data
    ///
    /// @param datalen
    ///
    /// @param host
    ///
    /// @param port
    ///  host byte order
    EIO_Status Send(const void*     data,
                    size_t          datalen,
                    const string&   host = string(),
                    unsigned short  port = 0);

    /// @param direction
    /// @sa
    ///   DSOCK_WipeMsg()
    EIO_Status Clear(EIO_Event direction);

    /// @param do_broadcast
    ///
    EIO_Status SetBroadcast(bool do_broadcast = true);

    /// Message count
    TNCBI_BigCount GetMessageCount(EIO_Event direction) const;

protected:
    /// @note  The call is not valid with datagram sockets.
    EIO_Status Shutdown(EIO_Event how) = delete;

    /// @note  The call is not valid with datagram sockets.
    EIO_Status Reconnect(const STimeout* timeout) = delete;

    /// @note  The call is not valid with datagram sockets.
    EIO_Status Abort(void) = delete;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CListeningSocket::
///
///    Listening socket (to accept connections on the server side)
///
/// @note  For documentation see LSOCK_***() functions in "ncbi_socket.h".
///

class NCBI_XCONNECT_EXPORT CListeningSocket : public CPollable
{
public:
    CListeningSocket(void);

    /// @note  "port" ought to be in host byte order
    ///
    /// @param port
    ///
    /// @param backlog
    ///
    /// @param flags
    ///
    CListeningSocket(unsigned short port,
                     unsigned short backlog = 64,
                     TSOCK_Flags    flags   = fSOCK_LogDefault,
                     ESwitch        ipv6    = eDefault);

    /// Call Close(), then self-destruct
    virtual ~CListeningSocket(void);

    /// Return eIO_Closed if not yet bound or Close()'d
    EIO_Status GetStatus(void) const;

    /// @note  "port" ought to be in host byte order
    ///
    /// @param port
    ///
    /// @param backlog
    ///
    /// @param flags
    ///
    EIO_Status Listen(unsigned short port,
                      unsigned short backlog = 64,
                      TSOCK_Flags    flags   = fSOCK_LogDefault,
                      ESwitch        ipv6    = eDefault);

    /// @note  The created "CSocket" will own its underlying "SOCK".
    ///
    /// @param sock
    ///
    /// @param timeout
    ///
    EIO_Status Accept(CSocket*&       sock,
                      const STimeout* timeout = kInfiniteTimeout,
                      TSOCK_Flags     flags   = fSOCK_LogDefault) const;

    /// @param sock
    ///
    /// @param timeout
    ///
    EIO_Status Accept(CSocket&        sock,
                      const STimeout* timeout = kInfiniteTimeout,
                      TSOCK_Flags     flags   = fSOCK_LogDefault) const;

    /// @note  Closes the undelying LSOCK only if it is owned by this object!
    EIO_Status Close(void);

    /// Return port which the server listens on
    unsigned short GetPort(ENH_ByteOrder byte_order = eNH_HostByteOrder) const;

    /// Get listening address.
    /// @note  Either of "addr", "port" can be NULL to opt out
    ///        from obtaining the corresponding value;
    /// @note  "byte_order" applies to "port" only
    ///
    /// @param addr
    ///                                   [can be NULL not to obtain]
    /// @param port
    ///  returned in requested byte order [can be NULL not to obtain]
    /// @param byte_order
    ///
    /// @sa
    ///  LSOCK_GetListeningAddressIPv6
    void GetListeningAddress(CNCBI_IPAddr*   addr,
                             unsigned short* port,
                             ENH_ByteOrder   byte_order = eNH_HostByteOrder) const;

    /// Get listening address as a text string.
    /// @return
    ///  Text string representing the (parts of) the listening address
    /// @sa
    ///  LSOCK_GetListeningAddressStringEx
    string GetListeningAddress(ESOCK_AddressFormat format = eSAF_Full) const;

    /// Access to the system-specific socket handle
    /// @param handle_buf
    ///
    /// @param handle_size
    ///
    virtual
    EIO_Status GetOSHandle(void*      handle_buf,
                           size_t     handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /// Specify if this "CListeningSocket" is to own the underlying "LSOCK"
    /// @param if_to_own
    ///
    /// @return
    ///  Previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    /// Access to the underlying "LSOCK"
    LSOCK GetLSOCK(void) const;

    bool IsEmpty(void) const { return !m_Socket; }

    virtual
    POLLABLE GetPOLLABLE(void) const { return POLLABLE_FromLSOCK(m_Socket); }

protected:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CSocketAPI::
///
///    Global utility routines / settings related to the socket API
///
/// @note  For documentation see SOCK_***() functions in "ncbi_socket.h".
///

class NCBI_XCONNECT_EXPORT CSocketAPI
{
public:
    /// Generic
    ///
    static EIO_Status Initialize   (void);
    static EIO_Status Shutdown     (void);
    static size_t     OSHandleSize (void);
    static void       AllowSigPipe (void);
    static ESwitch    SetIPv6      (ESwitch ipv6);
    static EIO_Status CloseOSHandle(const void* handle,
                                    size_t      handle_size);

    /// Utility
    ///
    static const STimeout*    SetSelectInternalRestartTimeout
                                  (const STimeout*    timeout);
    static ESOCK_IOWaitSysAPI SetIOWaitSysAPI
                                  (ESOCK_IOWaitSysAPI api);

    /// Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    /// @param read_on_write
    ///
    static ESwitch SetReadOnWrite(ESwitch read_on_write);

    /// @param interrupt
    ///
    static ESwitch SetInterruptOnSignal(ESwitch interrupt);

    /// @param reuse
    ///
    static ESwitch SetReuseAddress(ESwitch reuse);

    /// @param log
    ///
    static ESwitch SetDataLogging(ESwitch log);

    /// The polling structure
    /// m_Event can be either of eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite
    struct SPoll {
        CPollable* m_Pollable;  ///< [in]  object pointer (or NULL not to poll)
        EIO_Event  m_Event;     ///< [in]  event inquiry (or eIO_Open for none)
        EIO_Event  m_REvent;    ///< [out] event ready (eIO_Open if not ready)

        SPoll(CPollable* pollable = 0, EIO_Event event = eIO_Open)
            : m_Pollable(pollable), m_Event(event), m_REvent(eIO_Open)
        { }
    };

    /// Poll a vector of CPollable objects for I/O readiness.
    ///
    /// @note  If possible, please consider using lower-level SOCK_Poll() for
    ///        performance reasons (the code may still use a vector to prepare
    ///        the array of elements, then pass vector::data() and
    ///        vector::size() when making the SOCK_Poll() call).
    ///
    /// @note  Use CSocket::Wait() to wait for I/O event(s) in a single socket.
    ///
    static EIO_Status Poll(vector<SPoll>&  polls,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    /// BSD-like API.

    /// Convert to "dot" or "colon" notation (IPv4 or IPv6)
    static string ntoa(const CNCBI_IPAddr& addr);

    /// For IPv4 only
    NCBI_XSOCK_DEPRECATED
    static bool   isip(const string& host,
                       bool fullquad = false);
    /// Broader string classificator
    enum EIP_StringKind {
        eIP_HistoricIPv4 = 0,       ///< Accept old-fashioned IPv4 notations
        eIP_FullQuadIPv4,           ///< Accept full-quad IPv4 notations only
        eIP_IPv6,                   ///< Accept IPv6 notations only
        eIP_Any                     ///< Any IPv4 or IPv6 notations
    };
    /// @sa
    ///  SOCK_isipEx, SOCK_isip6, SOCK_IsAddress
    static bool   isip(const string& host,
                       EIP_StringKind kind = eIP_HistoricIPv4);

    static unsigned int   HostToNetLong (unsigned int   value);
    static unsigned int   NetToHostLong (unsigned int   value);
    static unsigned short HostToNetShort(unsigned short value);
    static unsigned short NetToHostShort(unsigned short value);

    /// Return empty string on error
    static string         gethostname  (ESwitch log = eOff);

    /// Return 0 or empty address on error
    static CNCBI_IPAddr   gethostbyname(const string& host,
                                        ESwitch log = eOff);

    /// Return empty string on error
    static string         gethostbyaddr(const CNCBI_IPAddr& addr,
                                        ESwitch log = eOff);

    /// Local host address in network byte order (cached for faster retrieval)
    static CNCBI_IPAddr   GetLocalHostAddress(ESwitch reget = eDefault);

    /// Loopback address is in network byte order (when "unsigned int")
    static CNCBI_IPAddr   GetLoopbackAddress (void);
    static unsigned int   GetLoopbackAddress4(void);
    static TNCBI_IPv6Addr GetLoopbackAddress6(void);
    static bool           IsLoopbackAddress(const CNCBI_IPAddr& addr);

    /// See SOCK_HostPortToString[6], return empty when failed
    static string         HostPortToString(const CNCBI_IPAddr& addr,
                                           unsigned short      port);

    /// Return number of characters parsed (0 if cannot detect "host:port"), and
    /// string::npos on error ("host" too long).  See SOCK_StringToHostPort[6]
    NCBI_XSOCK_DEPRECATED
    static size_t         StringToHostPort(const string&   str,
                                           unsigned int*   host,
                                           unsigned short* port);
    static size_t         StringToHostPort(const string&   str,
                                           CNCBI_IPAddr*   addr,
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

inline CTrigger::CTrigger(ESwitch log)
{
    TRIGGER_Create(&m_Trigger, log);
}


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


inline EIO_Status CTrigger::GetOSHandle(void*, size_t, EOwnership) const
{
    return m_Trigger ? eIO_NotSupported : eIO_Closed;
}


inline TRIGGER    CTrigger::GetTRIGGER(void) const
{
    return m_Trigger;
}



/////////////////////////////////////////////////////////////////////////////
/// CSocket::
///

inline CSocket::CSocket(void)
    : m_Socket(0), m_IsOwned(eTakeOwnership),
      o_timeout(0), r_timeout(0), w_timeout(0), c_timeout(0)
{
    return;
}


inline EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed;
}


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


inline EIO_Status CSocket::Pushback(const void* buf, size_t size)
{
    return m_Socket ? SOCK_Pushback(m_Socket, buf, size) : eIO_Closed;
}


inline EIO_Status CSocket::Abort(void)
{
    return m_Socket ? SOCK_Abort(m_Socket) : eIO_Closed;
}


inline EIO_Status CSocket::Close(void)
{
    return m_Socket
        ? SOCK_CloseEx(m_Socket, 0/*do not destroy handle*/) : eIO_Closed;
}


inline unsigned short CSocket::GetLocalPort(ENH_ByteOrder byte_order,
                                            bool          trueport) const
{
    return SOCK_GetLocalPortEx(m_Socket, trueport, byte_order);
}


inline unsigned short CSocket::GetRemotePort(ENH_ByteOrder byte_order) const
{
    return SOCK_GetRemotePort(m_Socket, byte_order);
}


inline EIO_Status CSocket::GetOSHandle(void* handle_buf, size_t handle_size,
                                       EOwnership ownership) const
{
    return m_Socket
        ? SOCK_GetOSHandleEx(m_Socket, handle_buf, handle_size, ownership)
        : eIO_Closed;
}

 
inline ESwitch CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    return m_Socket? SOCK_SetReadOnWrite(m_Socket, read_on_write) : eDefault;
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
    if ( m_Socket )
        SOCK_DisableOSSendDelay(m_Socket, on_off);
}


inline void CSocket::SetCork(bool on_off)
{
    if ( m_Socket )
        SOCK_SetCork(m_Socket, on_off);
}


inline ESwitch CSocket::SetDataLogging(ESwitch log)
{
    return m_Socket ? SOCK_SetDataLogging(m_Socket, log) : eDefault;
}


inline bool CSocket::IsDatagram(void) const
{
    return m_Socket  &&  SOCK_IsDatagram(m_Socket) ? true : false;
}


inline bool CSocket::IsClientSide(void) const
{
    return m_Socket  &&  SOCK_IsClientSide(m_Socket) ? true : false;
}


inline bool CSocket::IsServerSide(void) const
{
    return m_Socket  &&  SOCK_IsServerSide(m_Socket) ? true : false;
}


inline bool CSocket::IsUNIX(void) const
{
    return m_Socket  &&  SOCK_IsUNIX(m_Socket) ? true : false;
}


inline bool CSocket::IsSecure(void) const
{
    return m_Socket  &&  SOCK_IsSecure(m_Socket) ? true : false;
}


inline TNCBI_BigCount CSocket::GetPosition(EIO_Event direction) const
{
    return m_Socket ? SOCK_GetPosition(m_Socket, direction) : 0;
}


inline TNCBI_BigCount CSocket::GetCount(EIO_Event direction) const
{
    return m_Socket ? SOCK_GetCount(m_Socket, direction) : 0;
}


inline TNCBI_BigCount CSocket::GetTotalCount(EIO_Event direction) const
{
    return m_Socket ? SOCK_GetTotalCount(m_Socket, direction) : 0;
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



/////////////////////////////////////////////////////////////////////////////
///  CDatagramSocket::
///

inline CDatagramSocket::CDatagramSocket(TSOCK_Flags flags)
{
    DSOCK_CreateEx(&m_Socket, flags);
}


inline EIO_Status CDatagramSocket::Bind(unsigned short port, ESwitch ipv6)
{
    return m_Socket ? DSOCK_Bind6(m_Socket, port, ipv6) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::Connect(const string&  host,
                                           unsigned short port)
{
    return m_Socket ? DSOCK_Connect(m_Socket, host.c_str(), port) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::Send(const void*    data,
                                        size_t         datalen,
                                        const string&  host,
                                        unsigned short port)
{
    return m_Socket
        ? DSOCK_SendMsg(m_Socket, host.c_str(), port, data, datalen)
        : eIO_Closed;
}


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


inline TNCBI_BigCount CDatagramSocket::GetMessageCount(EIO_Event dir) const
{
    return m_Socket ? DSOCK_GetMessageCount(m_Socket, dir) : 0;
}



/////////////////////////////////////////////////////////////////////////////
///  CListeningSocket::
///

inline CListeningSocket::CListeningSocket(void)
    : m_Socket(0), m_IsOwned(eTakeOwnership)
{
    return;
}


inline CListeningSocket::CListeningSocket(unsigned short port,
                                          unsigned short backlog,
                                          TSOCK_Flags    flags,
                                          ESwitch        ipv6)
    : m_IsOwned(eTakeOwnership)
{
    LSOCK_CreateEx6(port, backlog, &m_Socket, flags, ipv6);
}


inline EIO_Status CListeningSocket::GetStatus(void) const
{
    return m_Socket ? eIO_Success : eIO_Closed;
}


inline EIO_Status CListeningSocket::Listen(unsigned short port,
                                           unsigned short backlog,
                                           TSOCK_Flags    flags,
                                           ESwitch        ipv6)
{
    return m_Socket
        ? eIO_Unknown : LSOCK_CreateEx6(port, backlog, &m_Socket, flags, ipv6);
}


inline unsigned short CListeningSocket::GetPort(ENH_ByteOrder byte_order) const
{
    return m_Socket ? LSOCK_GetPort(m_Socket, byte_order) : 0;
}


inline EIO_Status CListeningSocket::GetOSHandle(void*      handle_buf,
                                                size_t     handle_size,
                                                EOwnership ownership) const
{
    return m_Socket
        ? LSOCK_GetOSHandleEx(m_Socket, handle_buf, handle_size, ownership)
        : eIO_Closed;
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



/////////////////////////////////////////////////////////////////////////////
///  CSocketAPI::
///

inline EIO_Status CSocketAPI::Initialize(void)
{
    return SOCK_InitializeAPI();
}


inline EIO_Status CSocketAPI::Shutdown(void)
{
    return SOCK_ShutdownAPI();
}


inline size_t CSocketAPI::OSHandleSize(void)
{
    return SOCK_OSHandleSize();
}


inline void CSocketAPI::AllowSigPipe(void)
{
    SOCK_AllowSigPipeAPI();
}


inline ESwitch CSocketAPI::SetIPv6(ESwitch ipv6)
{
    return SOCK_SetIPv6API(ipv6);
}


inline EIO_Status CSocketAPI::CloseOSHandle(const void* handle,
                                            size_t      handle_size)
{
    return SOCK_CloseOSHandle(handle, handle_size);
}


inline const STimeout* CSocketAPI::SetSelectInternalRestartTimeout
(const STimeout* timeslice)
{
    return SOCK_SetSelectInternalRestartTimeout(timeslice);
}


inline ESOCK_IOWaitSysAPI CSocketAPI::SetIOWaitSysAPI
(ESOCK_IOWaitSysAPI api)
{
    return SOCK_SetIOWaitSysAPI(api);
}


inline ESwitch CSocketAPI::SetReadOnWrite(ESwitch read_on_write)
{
    return SOCK_SetReadOnWriteAPI(read_on_write);
}


inline ESwitch CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    return SOCK_SetInterruptOnSignalAPI(interrupt);
}


inline ESwitch CSocketAPI::SetReuseAddress(ESwitch reuse)
{
    return SOCK_SetReuseAddressAPI(reuse);
}


inline ESwitch CSocketAPI::SetDataLogging(ESwitch log)
{
    return SOCK_SetDataLoggingAPI(log);
}


inline bool CSocketAPI::isip(const string& host, bool fullquad)
{
    return SOCK_isipEx(host.c_str(), fullquad ? 1 : 0) ? true : false;
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


inline unsigned int CSocketAPI::GetLoopbackAddress4(void)
{
    return SOCK_GetLoopbackAddress();
}


inline bool CSocketAPI::IsLoopbackAddress(const CNCBI_IPAddr& addr)
{
    return SOCK_IsLoopbackAddress6(&addr.GetAddr()) ? true : false;
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

#endif /* CONNECT___NCBI_SOCKET__HPP */
