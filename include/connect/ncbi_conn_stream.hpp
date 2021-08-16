#ifndef CONNECT___NCBI_CONN_STREAM__HPP
#define CONNECT___NCBI_CONN_STREAM__HPP

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
 * @file ncbi_conn_stream.hpp
 *   CONN-based C++ streams
 *
 * Classes:
 *   CConn_IOStream 
 *      base class derived from "std::iostream" to perform I/O by means
 *      of underlying CConn_Streambuf (implemented privately in
 *      ncbi_conn_streambuf.[ch]pp).
 *
 *   CConn_SocketStream
 *      I/O stream based on socket connector.
 *
 *   CConn_HttpStream
 *      I/O stream based on HTTP connector (that is, the stream, which
 *      connects to HTTP server and exchanges information using HTTP(S)
 *      protocol).
 *
 *   CConn_ServiceStream
 *      I/O stream based on service connector, which is able to exchange
 *      data to/from a named service  that can be found via
 *      dispatcher/load-balancing  daemon and implemented as either
 *      HTTP GCI, standalone server, or NCBID service.
 *
 *   CConn_MemoryStream
 *      In-memory stream of data (analogous to strstream).
 *
 *   CConn_PipeStream
 *      I/O stream based on PIPE connector, which is able to exchange data
 *      with a spawned child process, specified via a command.
 *
 *   CConn_NamedPipeStream
 *      I/O stream based on NAMEDPIPE connector, which is able to exchange
 *      data with another process.
 *
 *   CConn_FtpStream
 *      I/O stream based on FTP connector, which is able to retrieve files
 *      and file lists from remote FTP servers, and upload files as well.
 *
 * API:
 *   NcbiOpenURL()
 *      Given a URL, return CConn_IOStream that is reading from the source.
 *      Supported schemes include: file://, http[s]://, ftp://, and finally
 *      a named NCBI service (null scheme).
 *
 */

#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_namedpipe_connector.hpp>
#include <connect/ncbi_pipe_connector.hpp>
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_socket.hpp>
#include <util/icanceled.hpp>
#include <utility>


/** @addtogroup ConnStreams
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CConn_Streambuf;  // Forward declaration


const size_t kConn_DefaultBufSize = 16384;  ///< I/O buffer size per direction



/////////////////////////////////////////////////////////////////////////////
///
/// Base class, inherited from "std::iostream", does both input and output,
/// using the specified CONNECTOR.
/// The "buf_size" parameter designates the size of internal I/O buffers, which
/// reside in between the stream and an underlying connector (which in turn may
/// do further buffering, if needed).
/// By default, all input operations are tied to the output ones, which means
/// that any input attempt first flushes any pending output from the internal
/// buffers.  The fConn_Untie flag can be used to untie the I/O directions.
///
/// @note
///   CConn_IOStream implementation utilizes some connection callbacks on the
///   underlying CONN object.  Care must be taken when intercepting the
///   callbacks using the native CONN API.
/// @sa
///   CONN_SetCallback

class NCBI_XCONNECT_EXPORT CConn_IOStream : public            CNcbiIostream,
                                            virtual protected CConnIniter
{
public:
    /// Polling timeout (non-NULL), with 0.0 time in it
    static const STimeout* kZeroTimeout;

    /// The values must be compatible with TCONN_Flags.
    enum {
        fConn_Untie           = fCONN_Untie,///< do not flush before reading
        fConn_DelayOpen       = 2,          ///< do not force CONN open in ctor
        fConn_ReadUnbuffered  = 4,          ///< read buffer NOT to be alloc'd
        fConn_WriteUnbuffered = 8           ///< write buffer NOT 2.b. alloc'd
    } EConn_Flag;
    typedef unsigned int TConn_Flags;       ///< bitwise OR of EConn_Flag

    /// Create a stream based on a CONN, which is to be closed upon stream dtor
    /// but only if "close" parameter is passed as "true".
    ///
    /// @param conn
    ///   A C object of type CONN (ncbi_connection.h) on top of which the
    ///   stream is being constructed.  May not be NULL.
    /// @param close
    ///   True if to close CONN automatically (otherwise CONN remains open)
    /// @param timeout
    ///   Default I/O timeout (including to open the stream)
    /// @param buf_size
    ///   Default size of underlying stream buffer's I/O arena (per direction)
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @param ptr
    ///   Specifies data which will be read from the stream prior to extracting
    ///   from the actual connection
    /// @param size
    ///   The size of the area pointed to by the "ptr" argument
    /// @sa
    ///   CONN, ncbi_connection.h
    ///
    CConn_IOStream
    (CONN            conn,
     bool            close    = false,
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize,
     TConn_Flags     flags    = 0,
     CT_CHAR_TYPE*   ptr      = 0,
     size_t          size     = 0);

    typedef pair<CONNECTOR, EIO_Status> TConnPair;
    /// Helper class to build streams on top of CONNECTOR (internal use only).
    class TConnector : public TConnPair
    {
    public:
        /// @param connector
        ///   A C object of type CONNECTOR (ncbi_connector.h) on top of which a
        ///   stream will be constructed.  NULL CONNECTOR indicates an error
        ///   (if none is passed in the second argument, eIO_Unknown results).
        /// @param status
        ///   I/O status to use in the underlying streambuf (e.g. when
        ///   CONNECTOR is NULL), and if non-eIO_Success will also cause a
        ///   non-NULL CONNECTOR (if any passed in the first argument) to be
        ///   destroyed.
        /// @sa
        ///   CONNECTOR, ncbi_connector.h
        TConnector(CONNECTOR connector, EIO_Status status = eIO_Success)
            : TConnPair(connector, status != eIO_Success ? status :
                        connector ? eIO_Success : eIO_Unknown)
        { }
    };
protected:
    /// Create a stream based on a CONNECTOR -- only for internal use in
    /// derived classes.  The CONNECTOR always gets closed by stream dtor.
    ///
    /// @param connector
    ///   CONNECTOR coupled with an error code (if any)
    /// @param timeout
    ///   Default I/O timeout (including to open the stream)
    /// @param buf_size
    ///   Default size of underlying stream buffer's I/O arena (per direction)
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @param ptr
    ///   Specifies data, which will be read from the stream prior to
    ///   extracting from the actual connection
    /// @param size
    ///   The size of the area pointed to by the "ptr" argument
    CConn_IOStream
    (const TConnector& connector,
     const STimeout*   timeout  = kDefaultTimeout,
     size_t            buf_size = kConn_DefaultBufSize,
     TConn_Flags       flags    = 0,
     CT_CHAR_TYPE*     ptr      = 0,
     size_t            size     = 0);

public:
    virtual ~CConn_IOStream();

    /// @return
    ///   Verbal connection type (empty if unknown)
    /// @sa
    ///   CONN_GetType
    string             GetType(void) const;

    /// @return
    ///   Verbal connection description (empty if unknown)
    /// @sa
    ///   CONN_Description
    string             GetDescription(void) const;

    /// Set connection timeout for "direction"
    /// @param
    ///   Can accept a pointer to a finite timeout, or either of the special
    ///   values: kInfiniteTimeout, kDefaultTimeout
    /// @sa
    ///   CONN_SetTimeout, SetReadTimeout, SetWriteTimeout
    EIO_Status         SetTimeout(EIO_Event       direction,
                                  const STimeout* timeout) const;

    /// @return
    ///   Connection timeout for "direction"
    /// @sa
    ///   CONN_GetTimeout
    const STimeout*    GetTimeout(EIO_Event direction) const;

    /// @return
    ///   Status of the last I/O performed by the underlying CONN in the
    ///   specified "direction" (either eIO_Open, IO_Read, or eIO_Write);
    ///   if "direction" is not specified (eIO_Close), return status of the
    ///   last CONN I/O performed by the stream.
    /// @sa
    ///   CONN_Status
    EIO_Status         Status(EIO_Event direction = eIO_Close) const;

    /// Flush the stream and fetch the response (w/o extracting any user data).
    /// @return
    ///   eIO_Success if the operation was successful, and some input
    ///   (including empty in case of EOF) will be available upon read.
    /// @note
    ///   Status eIO_Closed does not necessarily mean a proper EOF here. 
    EIO_Status         Fetch(const STimeout* timeout = kDefaultTimeout);

    /// Return the specified data "data" of size "size" into the underlying
    /// connection CONN.
    /// If there is any non-empty pending input sequence (internal read buffer)
    /// it will first be attempted to return to CONN.  That excludes any
    /// initial read area ("ptr") that might have been specified in the ctor.
    /// Any status different from eIO_Success means that nothing from "data"
    /// has been pushed back to the connection.
    /// @note
    ///   Can be used to push just the pending internal input alone back into
    ///   the CONN if used with a "size" of 0 ("data" is ignored then).
    /// @sa
    ///   CONN_Pushback
    EIO_Status         Pushback(const CT_CHAR_TYPE* data, streamsize size);

    /// Get underlying SOCK, if available (e.g. after Fetch())
    SOCK               GetSOCK(void);

    /// Get CSocket, if available (else empty).  The returned CSocket doesn't
    /// own the underlying SOCK, and is valid for as long as the stream exists.
    CSocket&           GetSocket(void);

    /// Close CONNection, free all internal buffers and underlying structures,
    /// and render the stream unusable for further I/O.
    /// @note
    ///   Can be used at places where reaching end-of-scope for the stream
    ///   would be impractical.
    /// @sa
    ///   CONN_Close
    virtual EIO_Status Close(void);

    /// Cancellation support.
    /// @note
    ///   The ICanceled implementation must be derived from CObject as its
    ///   first superclass.
    /// @sa
    ///   ICanceled
    EIO_Status         SetCanceledCallback(const ICanceled* canceled);

    /// @return
    ///   Internal CONNection handle, which is still owned and used by the
    ///   stream (or NULL if no such handle exists).
    /// @note
    ///   Connection can have additional flags set for I/O processing.
    /// @sa
    ///   CONN, ncbi_connection.h, CONN_GetFlags
    CONN               GetCONN(void) const;


    /// Equivalent to CONN_Wait(GetCONN(), event, timeout)
    /// @param event
    ///   eIO_Read or eIO_Write
    /// @param timeout
    ///   Time to wait for the event (poll if zero time specified, and return
    ///   immediately).
    /// @return
    ///   eIO_Success if the event is available, eIO_Timeout if the time has
    ///   expired;  other code to signal other error condition.
    /// @sa
    ///   CONN_Wait
    EIO_Status         Wait(EIO_Event event,
                            const STimeout* timeout = kZeroTimeout);

protected:
    void x_Destroy(void);

private:
    CConn_Streambuf*      m_CSb;

    // Cancellation
    SCONN_Callback        m_CB[4];
    CSocket               m_Socket;
    CConstIRef<ICanceled> m_Canceled;
    static EIO_Status x_IsCanceled(CONN conn, TCONN_Callback type, void* data);

private:
    // Disable copy constructor and assignment
    CConn_IOStream(const CConn_IOStream&);
    CConn_IOStream& operator= (const CConn_IOStream&);
};


class CConn_IOStreamSetTimeout {
public:
    const STimeout* GetTimeout(void) const { return m_Timeout; }

protected:
    CConn_IOStreamSetTimeout(const STimeout* timeout)
        : m_Timeout(timeout)
    { }

private:
    const STimeout* m_Timeout;
};


class CConn_IOStreamSetReadTimeout : protected CConn_IOStreamSetTimeout
{
public:
    using CConn_IOStreamSetTimeout::GetTimeout;

protected:
    CConn_IOStreamSetReadTimeout(const STimeout* timeout)
        : CConn_IOStreamSetTimeout(timeout)
    { }
    friend CConn_IOStreamSetReadTimeout SetReadTimeout(const STimeout*);
};


inline CConn_IOStreamSetReadTimeout SetReadTimeout(const STimeout* timeout)
{
    return CConn_IOStreamSetReadTimeout(timeout);
}


/// Stream manipulator "is >> SetReadTimeout(timeout)"
inline CConn_IOStream& operator>> (CConn_IOStream& is,
                                   const CConn_IOStreamSetReadTimeout& s)
{
    if (is.good()  &&  is.SetTimeout(eIO_Read, s.GetTimeout()) != eIO_Success)
        is.clear(NcbiBadbit);
    return is;
}


class CConn_IOStreamSetWriteTimeout : protected CConn_IOStreamSetTimeout
{
public:
    using CConn_IOStreamSetTimeout::GetTimeout;

protected:
    CConn_IOStreamSetWriteTimeout(const STimeout* timeout)
        : CConn_IOStreamSetTimeout(timeout)
    { }
    friend CConn_IOStreamSetWriteTimeout SetWriteTimeout(const STimeout*);
};


inline CConn_IOStreamSetWriteTimeout SetWriteTimeout(const STimeout* timeout)
{
    return CConn_IOStreamSetWriteTimeout(timeout);
}


/// Stream manipulator "os << SetWriteTimeout(timeout)"
inline CConn_IOStream& operator<< (CConn_IOStream& os,
                                   const CConn_IOStreamSetWriteTimeout& s)
{
    if (os.good()  &&  os.SetTimeout(eIO_Write, s.GetTimeout()) != eIO_Success)
        os.clear(NcbiBadbit);
    return os;
}


inline CSocket& CConn_IOStream::GetSocket(void)
{
    m_Socket.Reset(GetSOCK(), eNoOwnership, eCopyTimeoutsFromSOCK);
    return m_Socket;
}



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges data in a TCP channel, using the SOCK socket API.
/// The endpoint is specified as a "host:port" pair.  The maximal number of
/// connection attempts is given via "max_try".
/// More details on that: <connect/ncbi_socket_connector.h>.
///
/// @sa
///   SOCK_CreateConnector, SOCK_Create
///

class NCBI_XCONNECT_EXPORT CConn_SocketStream : public CConn_IOStream
{
public:
    /// Create a direct connection with host:port.
    ///
    /// @param host
    ///   Host to connect to
    /// @param port
    ///   ... and port number
    /// @param max_try
    ///   Number of attempts
    /// @param timeout
    ///   Default I/O timeout
    /// @param buf_size
    ///   Default buffer size
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @sa
    ///   CConn_IOStream
    CConn_SocketStream
    (const string&   host,                        ///< host to connect to
     unsigned short  port,                        ///< ... and port number
     unsigned short  max_try,                     ///< number of attempts
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize,
     TConn_Flags     flags    = 0);

    /// Create a direct connection with "host:port" and send an initial "data"
    /// block of the specified "size" first;  the remaining communication can
    /// proceed as usual.
    ///
    /// @param host
    ///   Host to connect to
    /// @param port
    ///   ... and port number (however, see a note below)
    /// @param data
    ///   Pointer to block of data to send once connection is ready
    /// @param size
    ///   Size of the data block to send (or 0 if to send nothing)
    /// @param flgs
    ///   Socket flgs (see <connect/ncbi_socket.h>)
    /// @param max_try
    ///   Number of attempts
    /// @param timeout
    ///   Default I/O timeout
    /// @param buf_size
    ///   Default buffer size
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @note As a special case, if "port" is specified as 0, then the "host"
    ///   parameter is expected to have a "host:port" string as its value.
    /// @sa
    ///   CConn_IOStream
    CConn_SocketStream
    (const string&   host,                        ///< host[:port] to connect
     unsigned short  port     = 0,                ///< ... and port number
     const void*     data     = 0,                ///< initial data block
     size_t          size     = 0,                ///< size of the data block
     TSOCK_Flags     flgs     = fSOCK_LogDefault, ///< see ncbi_socket.h
     unsigned short  max_try  = DEF_CONN_MAX_TRY, ///< number of attempts
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize,
     TConn_Flags     flags    = 0);

    /// This variant uses an existing socket "sock" to build a stream upon it.
    /// The caller may retain the ownership of "sock" by passing "if_to_own" as
    /// "eNoOwnership" to the stream constructor -- in that case, the socket
    /// "sock" will not be closed / destroyed upon stream destruction, and can
    /// further be used (including proper closing when no longer needed).
    /// Otherwise, "sock" becomes invalid once the stream is closed/destroyed.
    /// NOTE:  To maintain data integrity and consistency, "sock" should not
    ///        be used elsewhere while it is also being in use by the stream.
    /// More details:  <ncbi_socket_connector.h>::SOCK_CreateConnectorOnTop().
    ///
    /// @param sock
    ///   Socket to build the stream on
    /// @param if_to_own
    ///   Whether the sock object is owned (managed) by the stream at dtor
    /// @param timeout
    ///   Default I/O timeout
    /// @param buf_size
    ///   Default buffer size
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @sa
    ///   CConn_IOStream, ncbi_socket.h, SOCK_CreateConnectorOnTop
    CConn_SocketStream
    (SOCK            sock,         ///< socket
     EOwnership      if_to_own,    ///< whether stream to own "sock" param
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize,
     TConn_Flags     flags    = 0);

    /// This variant uses an existing CSocket to build a stream up on it.
    /// NOTE:  this always revokes all ownership of the "socket"'s internals
    /// (effectively leaving the CSocket empty);  CIO_Exception(eInvalidArg)
    /// is thrown if the internal SOCK is not owned by the passed CSocket.
    /// More details:  <ncbi_socket_connector.h>::SOCK_CreateConnectorOnTop().
    ///
    /// @param socket
    ///   Socket to build the stream up on
    /// @param timeout
    ///   Default I/O timeout
    /// @param buf_size
    ///   Default buffer size
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @sa
    ///   CConn_IOStream, ncbi_socket.hpp, SOCK_CreateConnectorOnTop
    CConn_SocketStream
    (CSocket&        socket,       ///< socket, underlying SOCK always grabbed
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize,
     TConn_Flags     flags    = 0);

    /// Create a tunneled socket stream connection.
    ///
    /// The following fields of SConnNetInfo are used (other ignored):
    ///
    /// scheme                          -- must be http or unspecified, checked
    /// host:port                       -- target server
    /// http_proxy_host:http_proxy_port -- HTTP proxy server to tunnel thru
    /// http_proxy_user:http_proxy_pass -- credentials for the proxy, if needed
    /// http_proxy_leak                 -- ignore bad proxy and connect direct
    /// timeout                         -- timeout to connect to HTTP proxy
    /// firewall                        -- if true then look at proxy_server
    /// proxy_server                    -- use as "host" if non-empty and FW
    /// debug_printout                  -- how to log socket data by default
    /// http_push_auth                  -- whether to push credentials at once
    ///
    /// @param net_info
    ///   Connection point and proxy tunnel location
    /// @param data
    ///   Pointer to block of data to send once connection is ready
    /// @param size
    ///   Size of the data block to send (or 0 if to send nothing)
    /// @param flgs
    ///   Socket flags (see <connect/ncbi_socket.h>)
    /// @param timeout
    ///   Default I/O timeout
    /// @param buf_size
    ///   Default buffer size
    /// @param flags
    ///   Specifies whether to tie input and output (a tied stream flushes all
    ///   pending output prior to doing any input) and what to buffer
    /// @sa
    ///   CConn_IOStream, SConnNetInfo
    CConn_SocketStream
    (const SConnNetInfo& net_info,
     const void*         data     = 0,
     size_t              size     = 0,
     TSOCK_Flags         flgs     = fSOCK_LogDefault,
     const STimeout*     timeout  = kDefaultTimeout,
     size_t              buf_size = kConn_DefaultBufSize,
     TConn_Flags         flags    = 0);
};



/// Helper class to fetch HTTP status code and text
struct SHTTP_StatusData {
    int         m_Code;
    CTempString m_Text;
    string      m_Header;

    SHTTP_StatusData(void) : m_Code(0)
    { }

    EHTTP_HeaderParse Parse(const char* header);

    void              Clear(void)
    { m_Code = 0; m_Text.clear(); m_Header = kEmptyStr; }

private:
    // Disable copy constructor and assignment
    SHTTP_StatusData(const SHTTP_StatusData&);
    SHTTP_StatusData& operator= (const SHTTP_StatusData&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges data with an HTTP server located at the URL:
/// http[s]://host[:port]/path[?args]
///
/// Note that "path" must include the leading slash, "args" can be empty, in
/// which case the '?' does not get appended to the path.
///
/// "User_header" (if not empty) should be a sequence of lines in the form
/// 'HTTP-tag: Tag value', with each line separated by a CR LF sequence, and
/// the last line optionally terminated with a CR LF sequence.  For example:
/// "Content-Encoding: gzip\r\nContent-Type: application/octet-stream"
/// It is included in the HTTP-header of each transaction.
///
/// More elaborate specification(s) of the server can be made via the
/// SConnNetInfo structure, which otherwise will be created with the use of a
/// standard registry section to obtain default values from (details:
/// <connect/ncbi_connutil.h>).  To make sure the actual user header is empty,
/// remember to reset it with ConnNetInfo_SetUserHeader(net_info, 0).
///
/// THTTP_Flags and other details: <connect/ncbi_http_connector.h>.
///
/// Provided "timeout" is set at the connection level, and if different from
/// kDefaultTimeout, it overrides a value supplied by the HTTP connector (the
/// latter value is kept at SConnNetInfo::timeout).
///
/// @sa
///   CConn_IOStream, HTTP_CreateConnector, ConnNetInfo_Create
///

class NCBI_XCONNECT_EXPORT CConn_HttpStream : public CConn_IOStream
{
public:
    CConn_HttpStream
    (const string&       host,
     const string&       path,
     const string&       args         = kEmptyStr,
     const string&       user_header  = kEmptyStr,
     unsigned short      port         = 0, ///< default(e.g. 80=HTTP/443=HTTPS)
     THTTP_Flags         flags        = fHTTP_AutoReconnect,
     const STimeout*     timeout      = kDefaultTimeout,
     size_t              buf_size     = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&       url,
     THTTP_Flags         flags        = fHTTP_AutoReconnect,
     const STimeout*     timeout      = kDefaultTimeout,
     size_t              buf_size     = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&       url,
     EReqMethod          method,
     const string&       user_header  = kEmptyStr,
     THTTP_Flags         flags        = fHTTP_AutoReconnect,
     const STimeout*     timeout      = kDefaultTimeout,
     size_t              buf_size     = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&       url,
     const SConnNetInfo* net_info,
     const string&       user_header  = kEmptyStr,
     FHTTP_ParseHeader   parse_header = 0,
     void*               user_data    = 0,
     FHTTP_Adjust        adjust       = 0,
     FHTTP_Cleanup       cleanup      = 0,
     THTTP_Flags         flags        = fHTTP_AutoReconnect,
     const STimeout*     timeout      = kDefaultTimeout,
     size_t              buf_size     = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const SConnNetInfo* net_info     = 0,
     const string&       user_header  = kEmptyStr,
     FHTTP_ParseHeader   parse_header = 0,
     void*               user_data    = 0,
     FHTTP_Adjust        adjust       = 0,
     FHTTP_Cleanup       cleanup      = 0,
     THTTP_Flags         flags        = fHTTP_AutoReconnect,
     const STimeout*     timeout      = kDefaultTimeout,
     size_t              buf_size     = kConn_DefaultBufSize
     );

    ~CConn_HttpStream();

    /// Get the last seen HTTP status code
    int               GetStatusCode(void) const {return m_StatusData.m_Code;  }

    /// Get the last seen HTTP status text
    const CTempString GetStatusText(void) const {return m_StatusData.m_Text;  }

    /// Get the entire HTTP header as received
    const string&     GetHTTPHeader(void) const {return m_StatusData.m_Header;}

    /// Set new URL to hit next
    void              SetURL(const string& url) { m_URL = url; }

protected:
    // Chained callbacks
    void*                    m_UserData;
    FHTTP_Adjust             m_UserAdjust;
    FHTTP_Cleanup            m_UserCleanup;
    FHTTP_ParseHeader        m_UserParseHeader;

    // HTTP status & text seen last
    SHTTP_StatusData         m_StatusData;

    // URL to hit next
    string                   m_URL;

private:
    // Interceptors
    static int/*bool*/       x_Adjust     (SConnNetInfo* net_info,
                                           void*         data,
                                           unsigned int  count);
    static void              x_Cleanup    (void*         data);
    static EHTTP_HeaderParse x_ParseHeader(const char*   header,
                                           void*         data,
                                           int           code);
};



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges data with a named service, in a constraint that the
/// service is implemented as one of the specified server "types"
/// (details: <connect/ncbi_server_info.h>).
///
/// Additional specifications can be passed in an SConnNetInfo structure,
/// otherwise created by using the service name as a registry section to obtain
/// the information from (details: <connect/ncbi_connutil.h>).
///
/// Provided "timeout" is set at the connection level, and if different from
/// kDefaultTimeout, it overrides the value supplied by an underlying connector
/// (the latter value is kept in SConnNetInfo::timeout).
///
/// @sa
///   SERVICE_CreateConnector
///

class NCBI_XCONNECT_EXPORT CConn_ServiceStream : public CConn_IOStream
{
public:
    CConn_ServiceStream
    (const string&           service,
     TSERV_Type              types    = fSERV_Any,
     const SConnNetInfo*     net_info = 0,
     const SSERVICE_Extra*   extra    = 0,
     const STimeout*         timeout  = kDefaultTimeout,
     size_t                  buf_size = kConn_DefaultBufSize);

    CConn_ServiceStream
    (const string&           service,
     const string&           user_header,
     TSERV_Type              types    = fSERV_Any,
     const SSERVICE_Extra*   extra    = 0,
     const STimeout*         timeout  = kDefaultTimeout,
     size_t                  buf_size = kConn_DefaultBufSize);

    ~CConn_ServiceStream();

    /// Get the last seen HTTP status code, if available
    int               GetStatusCode(void) const {return m_CBD.status.m_Code;}

    /// Get the last seen HTTP status text, if available
    const CTempString GetStatusText(void) const {return m_CBD.status.m_Text;}

    /// Get the last seen HTTP status text, if available
    const string&     GetHTTPHeader(void) const {return m_CBD.status.m_Header;}

public:
    /// Helper class
    struct SSERVICE_CBData {
        SHTTP_StatusData     status;
        SSERVICE_Extra       extra;
    };

protected:
    // Chained callbacks
    SSERVICE_CBData          m_CBD;

private:
    // Interceptors
    static void              x_Reset      (void*         data);
    static int/*bool*/       x_Adjust     (SConnNetInfo* net_info,
                                           void*         data,
                                           unsigned int  count);
    static void              x_Cleanup    (void*         data);
    static EHTTP_HeaderParse x_ParseHeader(const char*   header,
                                           void*         data,
                                           int           code);
    static const SSERV_Info* x_GetNextInfo(void*         data,
                                           SERV_ITER     iter);
};



/////////////////////////////////////////////////////////////////////////////
///
/// In-memory stream (a la strstream or stringstream)
///
/// @sa
///   MEMORY_CreateConnector
///

class NCBI_XCONNECT_EXPORT CConn_MemoryStream : public CConn_IOStream
{
public:
    CConn_MemoryStream(size_t      buf_size = kConn_DefaultBufSize);

    /// Build a stream on top of an NCBI buffer (which in turn
    /// could have been built over a memory area of a specified size).
    /// BUF's ownership is assumed by the stream as specified in "owner".
    CConn_MemoryStream(BUF         buf,
                       EOwnership  owner    = eTakeOwnership,
                       size_t      buf_size = kConn_DefaultBufSize);

    /// Build a stream on top of an existing data area of a specified size.
    /// The contents of the area is what will be read first from the stream.
    /// Writing to the stream will _not_ modify the contents of the area.
    /// When read from the stream, the written data will appear following the
    /// initial data block.
    /// Ownership of the area pointed to by "ptr" is controlled by the "owner"
    /// parameter, and if the ownership is passed to the stream the area will
    /// be deleted by "delete[] (char*)" from the stream destructor.  That is,
    /// if there are any requirements to be considered for deleting the area
    /// (like deleting an object or an array of objects), then the ownership
    /// must not be passed to the stream.
    /// Note that the area pointed to by "ptr" should not be changed while it
    /// is still holding the data yet to be read from the stream.
    CConn_MemoryStream(const void* ptr,
                       size_t      size,
                       EOwnership  owner/**no default for safety*/,
                       size_t      buf_size = kConn_DefaultBufSize);

    virtual ~CConn_MemoryStream();

    /// The CConnMemoryStream::To* methods allow to obtain unread portion of
    /// the stream into a single container (as a string or a vector) so that
    /// all data is kept in sequential memory locations.
    /// Note that the operation is considered an extraction, so it effectively
    /// empties the stream.
    void    ToString(string*);      ///< fill in the data, NULL is not accepted
    void    ToVector(vector<char>*);///< fill in the data, NULL is not accepted

    /// Get the underlying BUF handle (it still remains managed by the stream).
    /// @note Causes the stream to flush()
    BUF     GetBUF(void);

protected:
    const void* m_Ptr;              ///< pointer to read memory area (if owned)
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_PipeStream for command piping
///
/// @note
///   Exercise caution when operating on the underlying pipe while it's  being
///   in use as it may cause undefined behavior.
///
/// Provided "timeout" is set at the connection level if different from
/// kDefaultTimeout (which is infinite for this class by default).
///
/// @sa
///   PIPE_CreateConnector, CPipe
///

class NCBI_XCONNECT_EXPORT CConn_PipeStream : public CConn_IOStream
{
public:
    CConn_PipeStream
    (const string&         cmd,
     const vector<string>& args,
     CPipe::TCreateFlags   flags     = 0,
     size_t                pipe_size = 0,
     const STimeout*       timeout   = kDefaultTimeout,
     size_t                buf_size  = kConn_DefaultBufSize
     );
    virtual ~CConn_PipeStream();

    virtual EIO_Status Close(void);

    /// A valid exit code is only made available after an explicit Close().
    int    GetExitCode(void) const { return m_ExitCode; }

    CPipe& GetPipe(void)           { return *m_Pipe;    }

protected:
    CPipe* m_Pipe;      ///< Underlying pipe.
    int    m_ExitCode;  ///< Process exit code.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_NamedPipeStream for inter-process communication
///
/// Provided "timeout" is set at the connection level if different from
/// kDefaultTimeout (which is infinite for this class by default).
///
/// @sa
///   NAMEDPIPE_CreateConnector, CNamedPipe
///

class NCBI_XCONNECT_EXPORT CConn_NamedPipeStream : public CConn_IOStream
{
public:
    CConn_NamedPipeStream
    (const string&   pipename,
     size_t          pipesize = 0/*default*/,
     const STimeout* timeout  = kDefaultTimeout,
     size_t          buf_size = kConn_DefaultBufSize
     );
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_FtpStream is an elaborate FTP client, can be used for data
/// downloading and/or uploading to and from an FTP server.
/// See <connect/ncbi_ftp_connector.h> for detailed explanations
/// of supported features.
///
/// Provided "timeout" is set at the connection level, and if different from
/// kDefaultTimeout, it overrides a value supplied by the FTP connector
/// (the latter value is kept at SConnNetInfo::timeout).
///
/// @sa
///   FTP_CreateConnector
///

class NCBI_XCONNECT_EXPORT CConn_FtpStream : public CConn_IOStream
{
public:
    CConn_FtpStream
    (const string&        host,
     const string&        user,
     const string&        pass,
     const string&        path     = kEmptyStr,
     unsigned short       port     = 0,
     TFTP_Flags           flag     = 0,
     const SFTP_Callback* cmcb     = 0,
     const STimeout*      timeout  = kDefaultTimeout,
     size_t               buf_size = kConn_DefaultBufSize
     );

    CConn_FtpStream
    (const SConnNetInfo&  net_info,
     TFTP_Flags           flag     = 0,
     const SFTP_Callback* cmcb     = 0,
     const STimeout*      timeout  = kDefaultTimeout,
     size_t               buf_size = kConn_DefaultBufSize
     );

    /// Abort any command in progress, read and discard all input data, and
    /// clear stream error state when successful (eIO_Success returns).
    /// @note
    ///   The call empties both the stream and the underlying CONN.
    virtual EIO_Status Drain(const STimeout* timeout = kDefaultTimeout);
};


/// CConn_FtpStream specialization (ctor) for download
///
/// @attention
///   Pay attention to the order of parameters vs generic CConn_FtpStream ctor.
///
class NCBI_XCONNECT_EXPORT CConn_FTPDownloadStream : public CConn_FtpStream
{
public:
    CConn_FTPDownloadStream
    (const string&        host,
     const string&        file     = kEmptyStr,
     const string&        user     = "ftp",
     const string&        pass     = "-none@", // "-" helps make login quieter
     const string&        path     = kEmptyStr,
     unsigned short       port     = 0, ///< 0 means default (21 for FTP)
     TFTP_Flags           flag     = 0,
     const SFTP_Callback* cmcb     = 0,
     Uint8                offset   = 0, ///< file offset to begin download at
     const STimeout*      timeout  = kDefaultTimeout,
     size_t               buf_size = kConn_DefaultBufSize
     );

    CConn_FTPDownloadStream
    (const SConnNetInfo&  net_info,
     TFTP_Flags           flag     = 0,
     const SFTP_Callback* cmcb     = 0,
     Uint8                offset   = 0, ///< file offset to begin download at
     const STimeout*      timeout  = kDefaultTimeout,
     size_t               buf_size = kConn_DefaultBufSize
     );

protected:
    void x_InitDownload(const string& file, Uint8 offset);
};


/// CConn_FtpStream specialization (ctor) for upload
///
class NCBI_XCONNECT_EXPORT CConn_FTPUploadStream : public CConn_FtpStream
{
public:
    CConn_FTPUploadStream
    (const string&       host,
     const string&       user,
     const string&       pass,
     const string&       file    = kEmptyStr,
     const string&       path    = kEmptyStr,
     unsigned short      port    = 0, ///< 0 means default (21 for FTP)
     TFTP_Flags          flag    = 0,
     Uint8               offset  = 0, ///< file offset to start upload at
     const STimeout*     timeout = kDefaultTimeout
     );

    CConn_FTPUploadStream
    (const SConnNetInfo& net_info,
     TFTP_Flags          flag    = 0,
     Uint8               offset  = 0, ///< file offset to start upload at
     const STimeout*     timeout = kDefaultTimeout
     );

protected:
    void x_InitUpload(const string& file, Uint8 offset);
};



/////////////////////////////////////////////////////////////////////////////
///
/// Given a URL, open the data source and make it available for _reading_.
/// See <connect/ncbi_connutil.h> for supported schemes.
///
/// If "url" looks like an identifier (no scheme provided), then it will be
/// opened as a named NCBI service (using CConn_ServiceStream);  if the "url"
/// looks like a string in the form "host:port", a socket to that host and port
/// will be connected (using CConn_SocketStream);  otherwise, a stream will be
/// created according to the URL scheme (recognized are: "ftp://", "http://",
/// "https://", and "file://").
///
/// @warning
///   Writing to the resultant stream is undefined, unless it's either a
///   service or a socket stream.
///
extern NCBI_XCONNECT_EXPORT
CConn_IOStream* NcbiOpenURL(const string& url,
                            size_t        buf_size = kConn_DefaultBufSize);


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_STREAM__HPP */
