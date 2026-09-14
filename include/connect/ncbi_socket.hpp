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


/** Direction in which CSocket::Reset() copies I/O timeouts. */
enum ECopyTimeout {
    eCopyTimeoutsFromSOCK,
    eCopyTimeoutsToSOCK
};


/** Common interface for objects usable with CSocketAPI::Poll(). */
class NCBI_XCONNECT_EXPORT CPollable
{
public:
    /** Get the underlying system-specific handle.
     * @param handle_buf
     *  Buffer receiving the native handle.
     * @param handle_size
     *  Exact size of the native handle.
     * @param ownership
     *  Whether to transfer ownership of the native handle.
     */
    virtual
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size,
                           EOwnership ownership = eNoOwnership) const = 0;

    /** Get the generic pollable handle. */
    virtual
    POLLABLE   GetPOLLABLE(void) const = 0;

    /** Destroy the pollable wrapper. */
    virtual ~CPollable() { }

protected:
    CPollable(void) { }

private:
    // disable copy constructor and assignment
    CPollable(const CPollable&) = delete;
    CPollable& operator= (const CPollable&) = delete;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTrigger::
//

/** Event trigger wrapper.
 *
 * @sa
 *   TRIGGER_Create, TRIGGER_Close, TRIGGER_Set, TRIGGER_IsSet, TRIGGER_Reset
 */

class NCBI_XCONNECT_EXPORT CTrigger : public CPollable
{
public:
    /** Create an event trigger.
     * @param log
     *  Trigger logging setting.
     */
    CTrigger(ESwitch log = eDefault);

    /** Close and destroy the trigger. */
    virtual ~CTrigger();

    /** Get trigger object status.
     * @return
     *  eIO_Success if created; eIO_Closed otherwise.
     */
    EIO_Status GetStatus(void) const;

    /** Set the trigger. */
    EIO_Status Set(void);

    /** Check whether the trigger is set. */
    EIO_Status IsSet(void);

    /** Reset the trigger. */
    EIO_Status Reset(void);

    /** Query native-handle access for the trigger.  This call never succeeds.
     * @param handle_buf
     *  Unused.
     * @param handle_size
     *  Unused.
     * @param ownership
     *  Unused.
     * @return
     *  eIO_Closed if this object has no underlying trigger handle;
     *  eIO_NotSupported otherwise, because native-handle extraction is
     *  unsupported.
     */
    virtual
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /** Get the underlying C API trigger handle. */
    TRIGGER GetTRIGGER(void) const;

    /** Get the generic pollable handle. */
    virtual
    POLLABLE GetPOLLABLE(void) const {return POLLABLE_FromTRIGGER(m_Trigger);}

protected:
    TRIGGER m_Trigger;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CNCBI_IPAddr::
//

/** IPv4/IPv6 address container and converter. */

class CNCBI_IPAddr
{
public:
    /** Construct an empty address. */
    CNCBI_IPAddr(void)
    {
        Clear();
    }

    /** Construct from an IPv4 address in network byte order. */
    CNCBI_IPAddr(unsigned int ipv4)
    {
        NcbiIPv4ToIPv6(&m_IPAddr, ipv4, 0);
    }

    /** Construct from an IPv6-compatible address. */
    CNCBI_IPAddr(const TNCBI_IPv6Addr& ipv6)
    {
        m_IPAddr = ipv6;
    }

    /** Convert to an IPv4 address in network byte order. */
    operator unsigned int  (void) const
    {
        return NcbiIPv6ToIPv4(&m_IPAddr, 0);
    }

    /** Convert to an IPv6-compatible address. */
    operator TNCBI_IPv6Addr(void) const
    {
        return m_IPAddr;
    }

    /** Test whether the address is non-empty. */
    explicit operator bool(void) const
    {
        return !IsEmpty();
    }

    /** Test whether the address is empty. */
    bool operator !(void) const
    {
        return IsEmpty();
    }

    /** Clear the address. */
    void Clear(void)
    {
        memset(&m_IPAddr, 0, sizeof(m_IPAddr));
    }

    /** Check whether the address is empty. */
    bool IsEmpty(void) const
    {
        return NcbiIsEmptyIPv6(&m_IPAddr);
    }

    /** Get the stored address. */
    const TNCBI_IPv6Addr& GetAddr(void) const
    {
        return  m_IPAddr;
    }

    /** Get a mutable pointer to the stored address. */
    TNCBI_IPv6Addr* GetAddrPtr(void)
    {
        return &m_IPAddr;
    }

private:
    TNCBI_IPv6Addr  m_IPAddr;
};

/** Compare an address with an IPv4 address in network byte order. */
NCBI_XSOCK_DEPRECATED
inline bool operator==(const CNCBI_IPAddr& lhs, unsigned int rhs)
{
    return unsigned(lhs) == rhs/*network byte order*/;
}

/** Compare an address with an IPv4 address in network byte order. */
NCBI_XSOCK_DEPRECATED
inline bool operator!=(const CNCBI_IPAddr& lhs, unsigned int rhs)
{
    return unsigned(lhs) != rhs/*network byte order*/;
}

/** Compare an IPv4 address in network byte order with an address. */
NCBI_XSOCK_DEPRECATED
inline bool operator==(unsigned int lhs, const CNCBI_IPAddr& rhs)
{
    return lhs/*network byte order*/ == unsigned(rhs);
}

/** Compare an IPv4 address in network byte order with an address. */
NCBI_XSOCK_DEPRECATED
inline bool operator!=(unsigned int lhs, const CNCBI_IPAddr& rhs)
{
    return lhs/*network byte order*/ != unsigned(rhs);
}



/////////////////////////////////////////////////////////////////////////////
//
//  CSocket::
//

/** Connection-oriented socket wrapper.
 *
 * All I/O timeouts are initially infinite.
 */

class NCBI_XCONNECT_EXPORT CSocket : public CPollable
{
public:
    /** Construct an empty socket wrapper. */
    CSocket(void);

    /** Create and connect a client-side socket.
     * @param host
     *  Host name or numeric IP address.
     * @param port
     *  Destination port in host byte order.
     * @param timeout
     *  Maximum connection time; NULL means infinite.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  This object owns the created SOCK.  The supplied timeout becomes the
     *  eIO_Open timeout.
     * @sa
     *   CSocket::Connect, SOCK_Create
     */
    CSocket(const string&   host,
            unsigned short  port,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /** Create and connect a client-side socket from a host-port string.
     * @param hostport
     *  Host and port string; a bare IPv6 address must be bracketed.
     * @param timeout
     *  Maximum connection time; NULL means infinite.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  This object owns the created SOCK.  The supplied timeout becomes the
     *  eIO_Open timeout.
     * @sa
     *   CSocket::Connect, SOCK_Create
     */
    CSocket(const string&   hostport,
            const STimeout* timeout = kInfiniteTimeout,
            TSOCK_Flags     flags   = fSOCK_LogDefault);

    /** Create and connect a client-side socket to an IP address.
     * @param addr
     *  IPv4 or IPv6 destination address.
     * @param port
     *  Destination port in host byte order.
     * @param timeout
     *  Maximum connection time; NULL means infinite.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  This object owns the created SOCK.  The supplied timeout becomes the
     *  eIO_Open timeout.
     * @sa
     *   CSocket::Connect, SOCK_Create
     */
    CSocket(const CNCBI_IPAddr& addr,
            unsigned short      port,
            const STimeout*     timeout = kInfiniteTimeout,
            TSOCK_Flags         flags   = fSOCK_LogDefault);

    /** Close the owned socket, if any, and destroy the wrapper. */
    virtual ~CSocket(void);

    /** Get status from the last socket operation without performing I/O.
     * @param direction
     *  eIO_Open, eIO_Read, or eIO_Write.
     * @return
     *  Status for the requested direction, or eIO_InvalidArg for any
     *  other direction.
     * @sa
     *   SOCK_Status
     */
    EIO_Status GetStatus(EIO_Event direction) const;

    /** Connect an empty socket wrapper to a server.
     * @param host
     *  Host name or numeric IP address.
     * @param port
     *  Destination port in host byte order.
     * @param timeout
     *  Maximum connection time; kDefaultTimeout uses the stored open timeout.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  Do not call this on an already connected socket.  The effective timeout
     *  becomes the new eIO_Open timeout.
     * @sa
     *   SOCK_Create
     */
    EIO_Status Connect(const string&   host,
                       unsigned short  port,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);

    /** Connect an empty socket wrapper from a host-port string.
     * @param hostport
     *  Host and port string; a bare IPv6 address must be bracketed.
     * @param timeout
     *  Maximum connection time; kDefaultTimeout uses the stored open timeout.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  Do not call this on an already connected socket.  The effective timeout
     *  becomes the new eIO_Open timeout.
     * @sa
     *   SOCK_Create
     */
    EIO_Status Connect(const string&   hostport,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);

    /** Connect an empty socket wrapper to an IP address.
     * @param addr
     *  IPv4 or IPv6 destination address.
     * @param port
     *  Destination port in host byte order.
     * @param timeout
     *  Maximum connection time; NULL means infinite.
     * @param flags
     *  Additional socket properties, including logging.
     * @note
     *  Do not call this on an already connected socket.  The supplied timeout
     *  becomes the new eIO_Open timeout.
     * @sa
     *   SOCK_Create
     */
    EIO_Status Connect(const CNCBI_IPAddr& addr,
                       unsigned short      port,
                       const STimeout*     timeout = kInfiniteTimeout,
                       TSOCK_Flags         flags   = fSOCK_LogDefault);

    /** Reconnect a client-side socket to the same address.
     * @param timeout
     *  Maximum reconnection time; kDefaultTimeout uses the stored open timeout.
     * @note
     *  The socket must still have its underlying handle and must not originate
     *  from CListeningSocket::Accept().  The effective timeout becomes the new
     *  eIO_Open timeout.
     * @sa
     *   SOCK_Reconnect
     */
    EIO_Status Reconnect(const STimeout* timeout = kDefaultTimeout);

    /** Shut down socket I/O in the specified direction.
     * @param how
     *  eIO_Read, eIO_Write, or eIO_ReadWrite.
     * @sa
     *   SOCK_Shutdown
     */
    EIO_Status Shutdown(EIO_Event how);

    /** Close this socket wrapper.
     * @note
     *  The underlying SOCK is closed only when owned by this object.
     * @sa
     *   SOCK_CloseEx
     */
    EIO_Status Close(void);

    /** Wait for socket I/O readiness.
     * @param event
     *  eIO_Read, eIO_Write, or eIO_ReadWrite.
     * @param timeout
     *  Maximum wait time; NULL means infinite.
     * @note
     *  Use CSocketAPI::Poll() to wait for multiple objects.
     * @sa
     *   SOCK_Wait
     */
    EIO_Status Wait(EIO_Event       event,
                    const STimeout* timeout);

    /** Set a socket timeout.
     * @param event
     *  eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite, or eIO_Close.
     * @param timeout
     *  New timeout, including kInfiniteTimeout.
     * @note
     *  Timeouts are initially infinite.  kDefaultTimeout leaves the setting
     *  unchanged and returns eIO_Success.
     * @sa
     *   CSocket::GetTimeout, SOCK_SetTimeout, SOCK_GetTimeout
     */
    EIO_Status      SetTimeout(EIO_Event       event,
                               const STimeout* timeout);

    /** Get a socket timeout.
     * @param event
     *  eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite, or eIO_Close.
     * @return
     *  Stored timeout, or kDefaultTimeout for an unrecognized event.
     * @note
     *  For eIO_ReadWrite, returns the lesser read and write timeout.
     * @sa
     *   CSocket::SetTimeout, SOCK_GetTimeout
     */
    const STimeout* GetTimeout(EIO_Event event) const;

    /** Read data from the socket.
     * @param buf
     *  Destination buffer, or NULL for discard/prefetch operation.
     * @param size
     *  Maximum number of bytes to read.
     * @param n_read
     *  Receives the number of bytes read; may be NULL.
     * @param how
     *  eIO_ReadPlain reads available data, eIO_ReadPeek leaves it queued, and
     *  eIO_ReadPersist attempts to read exactly "size" bytes.
     * @sa
     *   SOCK_Read
     */
    EIO_Status Read(void*          buf,
                    size_t         size,
                    size_t*        n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    /** Read a line, stripping its CR-LF, LF, or null terminator.
     * @param str
     *  String receiving the line.
     * @sa
     *   SOCK_ReadLine
     */
    EIO_Status ReadLine(string& str);

    /** Read a line, stripping its CR-LF, LF, or null terminator.
     * @param buf
     *  Buffer receiving the line.
     * @param size
     *  Size of the destination buffer.
     * @param n_read
     *  Receives the number of bytes stored; may be NULL.
     * @sa
     *   SOCK_ReadLine
     */
    EIO_Status ReadLine(char*   buf,
                        size_t  size,
                        size_t* n_read = 0);

    /** Push data back so it is returned by subsequent reads first.
     * @param buf
     *  Data to push back.
     * @param size
     *  Number of bytes to push back.
     * @sa
     *   SOCK_Pushback
     */
    EIO_Status Pushback(const void* buf,
                        size_t      size);

    /** Write data to the socket.
     * @param buf
     *  Data to write.
     * @param size
     *  Number of bytes to write.
     * @param n_written
     *  Receives the number of bytes written; may be NULL.
     * @param how
     *  eIO_WritePlain writes what it can; eIO_WritePersist attempts all bytes.
     * @sa
     *   SOCK_Write
     */
    EIO_Status Write(const void*     buf,
                     size_t          size,
                     size_t*         n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    /** Abort socket connection.
     * @sa
     *   SOCK_Abort
     */
    EIO_Status Abort(void);

    /** Get the local port number.
     * @param byte_order
     *  Byte order for the returned port.
     * @param trueport
     *  Query the network layer instead of using the cached port when true.
     * @return
     *  Local port number, or 0 on error.
     * @sa
     *   SOCK_GetLocalPortEx, SOCK_GetLocalPort
     */
    unsigned short GetLocalPort(ENH_ByteOrder byte_order = eNH_HostByteOrder,
                                bool trueport = false) const;

    /** Get the remote port number.
     * @param byte_order
     *  Byte order for the returned port.
     * @return
     *  Remote port number, or 0 on error.
     * @sa
     *   SOCK_GetRemotePort
     */
    unsigned short GetRemotePort
        (ENH_ByteOrder byte_order = eNH_HostByteOrder) const;

    /** Get the IPv4 peer address and port.
     * @param host
     *  Receives the peer IPv4 address; may be NULL.
     * @param port
     *  Receives the peer port; may be NULL.
     * @param byte_order
     *  Byte order for both returned values.
     * @sa
     *   SOCK_GetPeerAddress
     */
    NCBI_XSOCK_DEPRECATED
    void GetPeerAddress(unsigned int*   host,
                        unsigned short* port,
                        ENH_ByteOrder   byte_order) const;

    /** Get the IPv4 or IPv6 peer address and port.
     * @param addr
     *  Receives the peer address; may be NULL.
     * @param port
     *  Receives the peer port; may be NULL.
     * @param byte_order
     *  Byte order for the returned port.
     * @sa
     *   SOCK_GetPeerAddress6
     */
    void GetPeerAddress(CNCBI_IPAddr*   addr,
                        unsigned short* port,
                        ENH_ByteOrder   byte_order = eNH_HostByteOrder) const;

    /** Get the peer address as text in the requested format.
     * @return
     *  Requested address representation, or an empty string on error.
     * @sa
     *   SOCK_GetPeerAddressStringEx
     */
    string GetPeerAddress(ESOCK_AddressFormat format = eSAF_Full) const;

    /** Get the underlying system-specific socket handle.
     * @param handle_buf
     *  Buffer receiving the native handle.
     * @param handle_size
     *  Exact size of the native handle.
     * @param ownership
     *  Whether to transfer ownership of the native handle.
     * @sa
     *   SOCK_GetOSHandleEx, CSocketAPI::OSHandleSize
     */
    virtual
    EIO_Status GetOSHandle(void*      handle_buf,
                           size_t     handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /** Set this socket's read-on-write behavior.
     * @param read_on_write
     *  eOn enables, eOff disables, and eDefault restores the API default.
     * @return
     *  Previous setting.
     * @sa
     *   CSocketAPI::SetReadOnWrite, SOCK_SetReadOnWrite
     */
    ESwitch SetReadOnWrite(ESwitch read_on_write = eOn);

    /** Set this socket's signal-interruption behavior.
     * @param interrupt
     *  eOn cancels I/O on signals, eOff restarts it, and eDefault restores the
     *  API default.
     * @return
     *  Previous setting.
     * @sa
     *   CSocketAPI::SetInterruptOnSignal, SOCK_SetInterruptOnSignal
     */
    ESwitch SetInterruptOnSignal(ESwitch interrupt = eOn);

    /** Set address reuse for this socket.
     * @param reuse
     *  eOn enables and eOff disables address reuse.
     * @sa
     *   CSocketAPI::SetReuseAddress, SOCK_SetReuseAddress
     */
    void    SetReuseAddress(ESwitch reuse = eOn);

    /** Disable or enable the TCP send delay (Nagle algorithm).
     * @param on_off
     *  true disables the delay; false enables it.
     * @note
     *  SOCK_SetCork() overrides this setting.
     * @sa
     *   SOCK_DisableOSSendDelay, CSocket::SetCork
     */
    void    DisableOSSendDelay(bool on_off = true);

    /** Enable or disable TCP corking.
     * @param on_off
     *  true enables corking; false disables it.
     * @note
     *  This setting overrides DisableOSSendDelay().
     * @sa
     *   SOCK_SetCork, CSocket::DisableOSSendDelay
     */
    void    SetCork(bool on_off = true);

    /** Set data logging for this socket.
     * @param log
     *  eOn enables, eOff disables, and eDefault restores the API default.
     * @return
     *  Previous setting.
     * @sa
     *   CSocketAPI::SetDataLogging, SOCK_SetDataLogging
     */
    ESwitch SetDataLogging(ESwitch log = eOn);

    /** Check whether this is a datagram socket. */
    bool IsDatagram  (void) const;

    /** Check whether this is a client-side socket. */
    bool IsClientSide(void) const;

    /** Check whether this is a server-side socket. */
    bool IsServerSide(void) const;

    /** Check whether this is a UNIX-domain socket. */
    bool IsUNIX      (void) const;

    /** Check whether this socket uses SSL. */
    bool IsSecure    (void) const;

    /** Get the current logical read or write position.
     * @param direction
     *  eIO_Read or eIO_Write.
     */
    TNCBI_BigCount GetPosition  (EIO_Event direction) const;

    /** Get the current-session byte count.
     * @param direction
     *  eIO_Read or eIO_Write.
     */
    TNCBI_BigCount GetCount     (EIO_Event direction) const;

    /** Get the lifetime byte count.
     * @param direction
     *  eIO_Read or eIO_Write.
     */
    TNCBI_BigCount GetTotalCount(EIO_Event direction) const;

    /** Replace the underlying SOCK handle, closing the old one if owned.
     * @param sock
     *  New underlying SOCK handle.
     * @param if_to_own
     *  Ownership mode for the new handle.
     * @param whence
     *  Direction in which I/O timeouts are copied.
     */
    void Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence);

    /** Set ownership of the underlying SOCK handle.
     * @param if_to_own
     *  New ownership mode.
     * @return
     *  Previous ownership mode.
     */
    EOwnership SetOwnership(EOwnership if_to_own);

    /** Get the underlying C API socket handle. */
    SOCK GetSOCK(void) const;

    /** Check whether no underlying socket is assigned. */
    bool IsEmpty(void) const { return !m_Socket; }

    /** Get the generic pollable handle. */
    virtual
    POLLABLE GetPOLLABLE(void) const { return POLLABLE_FromSOCK(m_Socket); }

protected:
    SOCK       m_Socket;
    EOwnership m_IsOwned;

    ///< Timeouts

    /** eIO_Open
     */
    STimeout*  o_timeout;
    /** eIO_Read
     */
    STimeout*  r_timeout;
    /** eIO_Write
     */
    STimeout*  w_timeout;
    /** eIO_Close
     */
    STimeout*  c_timeout;
    /** storage for o_timeout
     */
    STimeout  oo_timeout;
    /** storage for r_timeout
     */
    STimeout  rr_timeout;
    /** storage for w_timeout
     */
    STimeout  ww_timeout;
    /** storage for c_timeout
     */
    STimeout  cc_timeout;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDatagramSocket::
//

/** Connectionless datagram socket wrapper. */

class NCBI_XCONNECT_EXPORT CDatagramSocket : public CSocket
{
public:
    /** Create a datagram socket owned by this object.
     * @param flags
     *  Additional socket properties, including logging.
     */
    CDatagramSocket(TSOCK_Flags flags = fSOCK_LogDefault);

    /** Bind the socket to a local port.
     * @param port
     *  Local port in host byte order; 0 lets the OS choose one.
     * @param ipv6
     *  IPv4/IPv6 selection.
     */
    EIO_Status Bind(unsigned short port,
                    ESwitch        ipv6 = eDefault);

    /** Set the datagram destination.
     * @param host
     *  Host name or numeric IP address.
     * @param port
     *  Destination port in host byte order.
     */
    EIO_Status Connect(const string&  host,
                       unsigned short port);

    /** Set the datagram destination from a host-port string.
     * @param hostport
     *  Host and port string; missing host or port is accepted, such as ":0".
     */
    EIO_Status Connect(const string& hostport);

    /** Set an IPv4 datagram destination.
     * @param host
     *  IPv4 address in network byte order.
     * @param port
     *  Destination port in host byte order.
     */
    EIO_Status Connect(unsigned int   host,
                       unsigned short port);

    /** Set an IPv4 or IPv6 datagram destination.
     * @param addr
     *  Destination IP address.
     * @param port
     *  Destination port in host byte order.
     */
    EIO_Status Connect(const CNCBI_IPAddr& addr,
                       unsigned short      port);

    /** Wait for a datagram to become available.
     * @param timeout
     *  Maximum wait time; NULL means infinite.
     */
    EIO_Status Wait(const STimeout* timeout = kInfiniteTimeout);

    /** Receive a datagram.
     * @param buf
     *  Buffer receiving the initial message data; may be NULL.
     * @param buflen
     *  Size of the receive buffer.
     * @param msglen
     *  Receives the full message size; may be NULL.
     * @param sender_host
     *  Receives the sender address; may be NULL.
     * @param sender_port
     *  Receives the sender port in host byte order; may be NULL.
     * @param maxmsglen
     *  Maximum expected message size; 0 accepts any allowed size.
     * @sa
     *   DSOCK_RecvMsg6, CSocket::Read
     */
    EIO_Status Recv(void*           buf,
                    size_t          buflen,
                    size_t*         msglen      = 0,
                    string*         sender_host = 0,
                    unsigned short* sender_port = 0,
                    size_t          maxmsglen   = 0);

    /** Send a datagram.
     * @param data
     *  Additional message data; may be NULL.
     * @param datalen
     *  Size of the additional data.
     * @param host
     *  Destination host; empty uses the connected destination.
     * @param port
     *  Destination port in host byte order; 0 uses the connected destination.
     * @sa
     *   DSOCK_SendMsg, CSocket::Write
     */
    EIO_Status Send(const void*     data,
                    size_t          datalen,
                    const string&   host = string(),
                    unsigned short  port = 0);

    /** Clear the current incoming or outgoing datagram message.
     * @param direction
     *  eIO_Read for incoming data or eIO_Write for outgoing data.
     * @sa
     *   DSOCK_WipeMsg
     */
    EIO_Status Clear(EIO_Event direction);

    /** Enable or disable datagram broadcast.
     * @param do_broadcast
     *  Enable broadcast when true.
     */
    EIO_Status SetBroadcast(bool do_broadcast = true);

    /** Get the number of messages sent or received.
     * @param direction
     *  eIO_Read for received messages or eIO_Write for sent messages.
     */
    TNCBI_BigCount GetMessageCount(EIO_Event direction) const;

protected:
    /** @note  The call is not valid with datagram sockets.
     */
    EIO_Status Shutdown(EIO_Event how) = delete;

    /** @note  The call is not valid with datagram sockets.
     */
    EIO_Status Reconnect(const STimeout* timeout) = delete;

    /** @note  The call is not valid with datagram sockets.
     */
    EIO_Status Abort(void) = delete;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CListeningSocket::
//

/** Server-side listening socket wrapper. */

class NCBI_XCONNECT_EXPORT CListeningSocket : public CPollable
{
public:
    /** Construct an empty listening-socket wrapper. */
    CListeningSocket(void);

    /** Create a listening socket.
     * @param port
     *  Local port in host byte order; 0 lets the OS choose one.
     * @param backlog
     *  Maximum number of pending connections.
     * @param flags
     *  Additional socket properties, including logging.
     * @param ipv6
     *  IPv4/IPv6 selection.
     */
    CListeningSocket(unsigned short port,
                     unsigned short backlog = 64,
                     TSOCK_Flags    flags   = fSOCK_LogDefault,
                     ESwitch        ipv6    = eDefault);

    /** Close the owned listening socket and destroy the wrapper. */
    virtual ~CListeningSocket(void);

    /** Get listening-socket status.
     * @return
     *  eIO_Success if open; eIO_Closed otherwise.
     */
    EIO_Status GetStatus(void) const;

    /** Start listening on a local port.
     * @param port
     *  Local port in host byte order; 0 lets the OS choose one.
     * @param backlog
     *  Maximum number of pending connections.
     * @param flags
     *  Additional socket properties, including logging.
     * @param ipv6
     *  IPv4/IPv6 selection.
     */
    EIO_Status Listen(unsigned short port,
                      unsigned short backlog = 64,
                      TSOCK_Flags    flags   = fSOCK_LogDefault,
                      ESwitch        ipv6    = eDefault);

    /** Accept a client connection and allocate its CSocket wrapper.
     * @param sock
     *  Receives the allocated socket, which owns the accepted SOCK handle.
     * @param timeout
     *  Maximum time to wait; NULL means infinite.
     * @param flags
     *  Properties for the accepted socket.
     */
    EIO_Status Accept(CSocket*&       sock,
                      const STimeout* timeout = kInfiniteTimeout,
                      TSOCK_Flags     flags   = fSOCK_LogDefault) const;

    /** Accept a client connection into an existing CSocket wrapper.
     * @param sock
     *  Socket wrapper receiving the accepted connection.
     * @param timeout
     *  Maximum time to wait; NULL means infinite.
     * @param flags
     *  Properties for the accepted socket.
     */
    EIO_Status Accept(CSocket&        sock,
                      const STimeout* timeout = kInfiniteTimeout,
                      TSOCK_Flags     flags   = fSOCK_LogDefault) const;

    /** Close the listening socket if this object owns it. */
    EIO_Status Close(void);

    /** Get the listening port.
     * @return
     *  Listening port in the requested byte order, or 0 on error.
     */
    unsigned short GetPort(ENH_ByteOrder byte_order = eNH_HostByteOrder) const;

    /** Get the listening address and port.
     * @param addr
     *  Receives the IPv4 or IPv6 address; may be NULL.
     * @param port
     *  Receives the listening port; may be NULL.
     * @param byte_order
     *  Byte order for the returned port.
     * @sa
     *   LSOCK_GetListeningAddress6
     */
    void GetListeningAddress
        (CNCBI_IPAddr*   addr,
         unsigned short* port,
         ENH_ByteOrder   byte_order = eNH_HostByteOrder) const;

    /** Get the listening address as text in the requested format.
     * @return
     *  Requested address representation, or an empty string on error.
     * @sa
     *   LSOCK_GetListeningAddressStringEx
     */
    string GetListeningAddress(ESOCK_AddressFormat format = eSAF_Full) const;

    /** Get the underlying system-specific listening-socket handle.
     * @param handle_buf
     *  Buffer receiving the native handle.
     * @param handle_size
     *  Exact size of the native handle.
     * @param ownership
     *  Whether to transfer ownership of the native handle.
     */
    virtual
    EIO_Status GetOSHandle(void*      handle_buf,
                           size_t     handle_size,
                           EOwnership ownership = eNoOwnership) const;

    /** Set ownership of the underlying LSOCK handle.
     * @param if_to_own
     *  New ownership mode.
     * @return
     *  Previous ownership mode.
     */
    EOwnership SetOwnership(EOwnership if_to_own);

    /** Get the underlying C API listening-socket handle. */
    LSOCK GetLSOCK(void) const;

    /** Check whether no underlying listening socket is assigned. */
    bool IsEmpty(void) const { return !m_Socket; }

    /** Get the generic pollable handle. */
    virtual
    POLLABLE GetPOLLABLE(void) const { return POLLABLE_FromLSOCK(m_Socket); }

protected:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSocketAPI::
//

/** Global socket API settings and network utility routines. */

class NCBI_XCONNECT_EXPORT CSocketAPI
{
public:
    /** Initialize the socket API.
     * @return
     *  eIO_Success on success; another status on error.
     * @sa
     *   SOCK_InitializeAPI
     */
    static EIO_Status Initialize   (void);

    /** Shut down and clean up the socket API.
     * @return
     *  eIO_Success on success; another status on error.
     * @sa
     *   SOCK_ShutdownAPI
     */
    static EIO_Status Shutdown     (void);

    /** Get the size of a native socket handle.
     * @return
     *  Native handle size, or 0 on error.
     * @sa
     *   SOCK_OSHandleSize
     */
    static size_t     OSHandleSize (void);

    /** Allow SIGPIPE instead of suppressing it during API initialization.
     * @sa
     *   SOCK_AllowSigPipeAPI
     */
    static void       AllowSigPipe (void);

    /** Select the IP protocol mode.
     * @param ipv6
     *  eOn selects IPv6, eOff selects IPv4, and eDefault allows both.
     * @return
     *  Previous selection.
     * @note Using both versions requires dual-stack support from the host OS.
     * @sa
     *   SOCK_SetIPv6API
     */
    static ESwitch    SetIPv6      (ESwitch ipv6);

    /** Close a native socket handle.
     * @param handle
     *  Pointer to the native handle.
     * @param handle_size
     *  Exact native handle size.
     * @sa
     *   SOCK_CloseOSHandle
     */
    static EIO_Status CloseOSHandle(const void* handle,
                                    size_t      handle_size);

    /** Set the internal restart interval for long I/O waits.
     * @param timeout
     *  Maximum interval between internal wait restarts.
     * @return
     *  Previous interval.
     * @sa
     *   SOCK_SetSelectInternalRestartTimeout
     */
    static const STimeout*    SetSelectInternalRestartTimeout
                                  (const STimeout*    timeout);

    /** Select the system I/O wait API.
     * @param api
     *  Automatic, poll(), or select() selection.
     * @return
     *  Previous selection.
     * @sa
     *   SOCK_SetIOWaitSysAPI
     */
    static ESOCK_IOWaitSysAPI SetIOWaitSysAPI
                                  (ESOCK_IOWaitSysAPI api);

    /** Set the default read-on-write behavior.
     * @param read_on_write
     *  eOn, eOff, or eDefault to query the current setting.
     * @return
     *  Previous setting.
     * @sa
     *   CSocket::SetReadOnWrite, SOCK_SetReadOnWriteAPI
     */
    static ESwitch SetReadOnWrite(ESwitch read_on_write);

    /** Set the default signal-interruption behavior.
     * @param interrupt
     *  eOn cancels I/O on a signal; eOff restarts it; eDefault queries.
     * @return
     *  Previous setting.
     * @sa
     *   CSocket::SetInterruptOnSignal, SOCK_SetInterruptOnSignalAPI
     */
    static ESwitch SetInterruptOnSignal(ESwitch interrupt);

    /** Set the default address-reuse behavior.
     * @param reuse
     *  eOn, eOff, or eDefault to query the current setting.
     * @return
     *  Previous setting.
     * @sa
     *   CSocket::SetReuseAddress, SOCK_SetReuseAddressAPI
     */
    static ESwitch SetReuseAddress(ESwitch reuse);

    /** Set the default socket data logging behavior.
     * @param log
     *  eOn, eOff, or eDefault to query the current setting.
     * @return
     *  Previous setting.
     * @sa
     *   CSocket::SetDataLogging, SOCK_SetDataLoggingAPI
     */
    static ESwitch SetDataLogging(ESwitch log);

    /** The polling structure
     * m_Event can be either of eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite
     */
    struct SPoll {
        CPollable* m_Pollable;  ///< [in]  object pointer (or NULL not to poll)
        EIO_Event  m_Event;     ///< [in]  event inquiry (or eIO_Open for none)
        EIO_Event  m_REvent;    ///< [out] event ready (eIO_Open if not ready)

        /** Construct a poll entry.
         * @param pollable
         *  Object to poll, or NULL.
         * @param event
         *  Requested event.
         */
        SPoll(CPollable* pollable = 0, EIO_Event event = eIO_Open)
            : m_Pollable(pollable), m_Event(event), m_REvent(eIO_Open)
        { }
    };

    /** Poll objects for I/O readiness.
     * @param polls
     *  Poll requests updated with the events that become ready.
     * @param timeout
     *  Maximum wait time; NULL means infinite.
     * @param n_ready
     *  Receives the number of ready objects; may be NULL.
     * @return eIO_Success if at least one object is ready, eIO_Timeout if the
     *  wait expires, or another status on error.
     * @note For large poll sets, consider using the lower-level SOCK_Poll() to
     *  reduce wrapper overhead.  A vector can still be used to prepare the poll
     *  array; pass vector::data() and vector::size() directly to SOCK_Poll().
     * @note Use CSocket::Wait() for a single socket.
     * @sa
     *   POLLABLE_Poll, SOCK_Poll
     */
    static EIO_Status Poll(vector<SPoll>&  polls,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    /** Convert an IP address to numeric text notation.
     * @return
     *  Numeric IPv4 or IPv6 string, or an empty string on error.
     */
    static string ntoa(const CNCBI_IPAddr& addr);

    /** Check whether a string is an IPv4 address.
     * @param host
     *  String to inspect.
     * @param fullquad
     *  Require full dotted-quad notation when true.
     */
    NCBI_XSOCK_DEPRECATED
    static bool   isip(const string& host, bool fullquad);
    /** Accepted IP address string syntax. */
    enum EIP_StringKind {
        eIP_HistoricIPv4 = 0,       ///< Accept old-fashioned IPv4 notations
        eIP_FullQuadIPv4,           ///< Accept full-quad IPv4 notations only
        eIP_IPv6,                   ///< Accept IPv6 notations only
        eIP_Any                     ///< Any IPv4 or IPv6 notations
    };
    /** Classify whether a string is an accepted IP address.
     * @param host
     *  String to inspect.
     * @param kind
     *  Accepted address syntax.
     * @sa
     *   SOCK_isipEx, SOCK_isip6, SOCK_IsAddress
     */
    static bool   isip(const string& host,
                       EIP_StringKind kind = eIP_HistoricIPv4);

    /** Convert a 32-bit value from host to network byte order. */
    static unsigned int   HostToNetLong (unsigned int   value);

    /** Convert a 32-bit value from network to host byte order. */
    static unsigned int   NetToHostLong (unsigned int   value);

    /** Convert a 16-bit value from host to network byte order. */
    static unsigned short HostToNetShort(unsigned short value);

    /** Convert a 16-bit value from network to host byte order. */
    static unsigned short NetToHostShort(unsigned short value);

    /** Get the local host name, optionally logging failures.
     * @return
     *  Local host name, or an empty string on error.
     */
    static string         gethostname  (ESwitch log = eOff);

    /** Resolve a host name or numeric address.
     * @param host
     *  Host name or numeric address; empty selects the local host.
     * @param log
     *  Whether to log resolution failures.
     * @return
     *  Resolved address, or an empty address on error.
     */
    static CNCBI_IPAddr   gethostbyname(const string& host,
                                        ESwitch log = eOff);

    /** Resolve an address to a host name.
     * @param addr
     *  Address to resolve; an empty address selects the local host.
     * @param log
     *  Whether to log resolution failures.
     * @return
     *  Host name or numeric address, or an empty string on error.
     */
    static string         gethostbyaddr(const CNCBI_IPAddr& addr,
                                        ESwitch log = eOff);

    /** Get the cached local host address.
     * @param reget
     *  Controls whether the cached address is refreshed.
     * @return
     *  Local host address, or an empty address on error.
     */
    static CNCBI_IPAddr   GetLocalHostAddress(ESwitch reget = eDefault);

    /** Get the loopback address for the current IP mode. */
    static CNCBI_IPAddr   GetLoopbackAddress (void);

    /** Get the IPv4 loopback address in network byte order. */
    static unsigned int   GetLoopbackAddress4(void);

    /** Get the IPv6 loopback address. */
    static TNCBI_IPv6Addr GetLoopbackAddress6(void);

    /** Check whether an address is a loopback address. */
    static bool           IsLoopbackAddress(const CNCBI_IPAddr& addr);

    /** Format a numeric host-port string.
     * @param addr
     *  IP address.
     * @param port
     *  Port in host byte order.
     * @return
     *  Formatted string, or an empty string on error.
     * @sa
     *   SOCK_HostPortToString6
     */
    static string         HostPortToString(const CNCBI_IPAddr& addr,
                                           unsigned short      port);

    /** Parse an IPv4 host-port string.
     * @param str
     *  String to parse.
     * @param host
     *  Receives the IPv4 address in network byte order; may be NULL.
     * @param port
     *  Receives the port in host byte order; may be NULL.
     * @return
     *  Number of characters parsed, 0 if no host-port is found, or
     *  string::npos on error.
     * @sa
     *   SOCK_StringToHostPort
     */
    NCBI_XSOCK_DEPRECATED
    static size_t         StringToHostPort(const string&   str,
                                           unsigned int*   host,
                                           unsigned short* port);

    /** Parse an IPv4 or IPv6 host-port string.
     * @param str
     *  String to parse.
     * @param addr
     *  Receives the IP address; may be NULL.
     * @param port
     *  Receives the port in host byte order; may be NULL.
     * @return
     *  Number of characters parsed, 0 if no host-port is found, or
     *  string::npos on error.
     * @sa
     *   SOCK_StringToHostPort6
     */
    static size_t         StringToHostPort(const string&   str,
                                           CNCBI_IPAddr*   addr,
                                           unsigned short* port);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CTrigger::
//

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
// CSocket::
//

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
//  CDatagramSocket::
//

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
//  CListeningSocket::
//

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
//  CSocketAPI::
//

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
