#ifndef CONNECT___NCBI_CONN_STREAM__HPP
#define CONNECT___NCBI_CONN_STREAM__HPP

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
 * @file
 * File Description:
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
 *      connects to HTTP server and exchanges information using HTTP
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
 *      I/O stream based on PIPE connector, which is  able to exchange data
 *      to/from another child process.
 *
 *   CConn_NamedPipeStream
 *      I/O stream based on NAMEDPIPE connector, which is able to exchange
 *      data to/from another process.
 *
 *   CConn_FTPDownloadStream
 *      I/O stream based on FTP connector, which is able to retrieve files
 *      and file lists from remote FTP servers.
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_namedpipe_connector.hpp>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_pipe_connector.hpp>
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <vector>

/** @addtogroup ConnStreams
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CConn_Streambuf; // Forward declaration


const streamsize kConn_DefaultBufSize = 4096;



/////////////////////////////////////////////////////////////////////////////
///
/// Base class, inherited from "std::iostream", does both input
/// and output, using the specified CONNECTOR.  Input operations
/// can be tied to the output ones by setting 'do_tie' to 'true'
/// (default), which means that any input attempt first flushes
/// the output queue from the internal buffers.  'buf_size'
/// designates the size of the I/O buffers, which reside in between
/// the stream and underlying connector (which in turn may do
/// further buffering, if needed).
///

class NCBI_XCONNECT_EXPORT CConn_IOStream : virtual public CConnIniter,
                                            public CNcbiIostream
{
protected:
    /// Create a stream based on a CONNECTOR -- for internal use only
    /// in derived classes.
    ///
    /// @param connector
    ///  A C object of type CONNECTOR (ncbi_connector.h) on top of which
    ///  the stream is being constructed.  Used internally by individual
    /// ctors of specialized streams in this header.  May not be NULL.
    /// @param timeout
    ///  Default I/O timeout
    /// @param buf_size
    ///  Default size of underlying stream buffer's I/O arena
    /// @param do_tie
    ///  Specifies whether to tie output to input -- a tied stream flushes
    ///  all pending output prior to doing any input.
    /// @sa
    ///  CONNECTOR, ncbi_connector.h
    CConn_IOStream
    (CONNECTOR       connector,
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize,
     bool            do_tie   = true,
     CT_CHAR_TYPE*   ptr      = 0,
     size_t          size     = 0);

public:
    virtual ~CConn_IOStream();

    /// @return
    ///   CONNection handle built from the CONNECTION
    /// @sa
    ///   CONN, ncbi_connection.h
    CONN GetCONN(void) const;

    /// Close CONNection free all internal buffers and underlying structures,
    /// render stream unusable for further I/O.
    void Close(void);

protected:
    CConn_IOStream(CConn_Streambuf* sb);
    void Cleanup(void);

private:
    CConn_Streambuf* m_CSb;

    // Disable copy constructor and assignment.
    CConn_IOStream(const CConn_IOStream&);
    CConn_IOStream& operator= (const CConn_IOStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges data in a TCP channel, using socket interface.
/// The endpoint is specified as host/port pair.  The maximal
/// number of connection attempts is given as 'max_try'.
/// More details on that: <connect/ncbi_socket_connector.h>.
///
///

class NCBI_XCONNECT_EXPORT CConn_SocketStream : public CConn_IOStream
{
public:
    /// Create a direct connection to host:port.
    ///
    /// @param host
    ///  Host to connect to
    /// @param port
    ///  ... and port number
    /// @param max_try
    ///  Number of attempts
    /// @param timeout
    ///  Default I/O timeout
    /// @param buf_size
    ///  Default buffer size
    /// @sa
    ///  CConn_IOStream
    CConn_SocketStream
    (const string&   host,         /* host to connect to  */
     unsigned short  port,         /* ... and port number */
     unsigned int    max_try  = 3, /* number of attempts  */
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize);

    /// This variant uses existing socket "sock" to build the stream upon it.
    /// NOTE:  it revokes all ownership of the socket, and further assumes the
    /// socket being in exclusive use of this stream's underlying CONN.
    /// More details:  <ncbi_socket_connector.h>::SOCK_CreateConnectorOnTop().
    ///
    /// @param SOCK
    ///  Socket to build the stream on
    /// @sa
    ///  SOCK, ncbi_socket.h
    CConn_SocketStream
    (SOCK            sock,         /* socket              */
     unsigned int    max_try  = 3, /* number of attempts  */
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize);

private:
    // Disable copy constructor and assignment.
    CConn_SocketStream(const CConn_SocketStream&);
    CConn_SocketStream& operator= (const CConn_SocketStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges data with an HTTP server found by URL:
/// http://host[:port]/path[?args]
///
/// Note that 'path' must include a leading slash,
/// 'args' can be empty, in which case the '?' is not appended to the path.
///
/// 'User_header' (if not empty) should be a sequence of lines
/// in the form 'HTTP-tag: Tag value', separated by '\r\n', and
/// '\r\n'-terminated. It is included in the HTTP-header of each transaction.
///
/// More elaborate specification of the server can be done via
/// SConnNetInfo structure, which otherwise will be created with the
/// use of a standard registry section to obtain default values from
/// (details: <connect/ncbi_connutil.h>).  No user header is added if
/// the argument is passed as default (or empty string).  To make
/// sure the user header is passed empty, delete it from net_info
/// by ConnNetInfo_DeleteUserHeader(net_info).
///
/// THCC_Flags and other details: <connect/ncbi_http_connector.h>.
///
/// Provided 'timeout' is set at connection level, and if different from
/// CONN_DEFAULT_TIMEOUT, it overrides a value supplied by HTTP connector
/// (the latter value is kept in SConnNetInfo::timeout).
///

class NCBI_XCONNECT_EXPORT CConn_HttpStream : public CConn_IOStream
{
public:
    CConn_HttpStream
    (const string&        host,
     const string&        path,
     const string&        args            = kEmptyStr,
     const string&        user_header     = kEmptyStr,
     unsigned short       port            = 80,
     THCC_Flags           flags           = fHCC_AutoReconnect,
     const STimeout*      timeout         = kDefaultTimeout,
     streamsize           buf_size        = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&        url,
     THCC_Flags           flags           = fHCC_AutoReconnect,
     const STimeout*      timeout         = kDefaultTimeout,
     streamsize           buf_size        = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&        url,
     const SConnNetInfo*  net_info,
     const string&        user_header     = kEmptyStr,
     THCC_Flags           flags           = fHCC_AutoReconnect,
     const STimeout*      timeout         = kDefaultTimeout,
     streamsize           buf_size        = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const SConnNetInfo*  net_info        = 0,
     const string&        user_header     = kEmptyStr,
     FHttpParseHTTPHeader parse_header    = 0,
     FHttpAdjustNetInfo   adjust_net_info = 0,
     void*                adjust_data     = 0,
     FHttpAdjustCleanup   adjust_cleanup  = 0,
     THCC_Flags           flags           = fHCC_AutoReconnect,
     const STimeout*      timeout         = kDefaultTimeout,
     streamsize           buf_size        = kConn_DefaultBufSize
     );

private:
    // Disable copy constructor and assignment.
    CConn_HttpStream(const CConn_HttpStream&);
    CConn_HttpStream& operator= (const CConn_HttpStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// This stream exchanges the data with a named service, in a
/// constraint that the service is implemented as one of the specified
/// server 'types' (details: <connect/ncbi_server_info.h>).
///
/// Additional specifications can be passed in the SConnNetInfo structure,
/// otherwise created by using service name as a registry section
/// to obtain the information from (details: <connect/ncbi_connutil.h>).
///
/// Provided 'timeout' is set at connection level, and if different from
/// CONN_DEFAULT_TIMEOUT, it overrides a value supplied by underlying
/// connector (the latter value is kept in SConnNetInfo::timeout).
///

class NCBI_XCONNECT_EXPORT CConn_ServiceStream : public CConn_IOStream
{
public:
    CConn_ServiceStream
    (const string&         service,
     TSERV_Type            types    = fSERV_Any,
     const SConnNetInfo*   net_info = 0,
     const SSERVICE_Extra* params   = 0,
     const STimeout*       timeout  = kDefaultTimeout,
     streamsize            buf_size = kConn_DefaultBufSize);

private:
    // Disable copy constructor and assignment.
    CConn_ServiceStream(const CConn_ServiceStream&);
    CConn_ServiceStream& operator= (const CConn_ServiceStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// In-memory stream
///

class NCBI_XCONNECT_EXPORT CConn_MemoryStream : public CConn_IOStream
{
public:
    CConn_MemoryStream(streamsize  buf_size = kConn_DefaultBufSize);

    /// Build a stream on top of an NCBI buffer (which in turn
    /// could have been built over a memory area of a specified size).
    /// BUF's ownership is assumed by the stream as specified in "owner".
    CConn_MemoryStream(BUF         buf,
                       EOwnership  owner    = eTakeOwnership,
                       streamsize  buf_size = kConn_DefaultBufSize);

    /// Build a stream on top of an existing data area of a specified size.
    /// The contents of the area is what will be read first from the stream.
    /// Writing to the stream will _not_ modify the contents of the area.
    /// Ownership of the area pointed to by "ptr" is controlled by "owner"
    /// parameter, and if the ownership is passed to the stream the area will
    /// be deleted by "delete[] (char*)" at the stream dtor.  That is,
    /// if there are any requirements to be considered for deleting the area
    /// (like deleting an object or an array of objects), then the
    /// ownership must not be passed to the stream.
    /// Note that the area pointed to by "ptr" should not be changed
    /// while it is still holding the data yet to be read from the stream.
    CConn_MemoryStream(const void* ptr,
                       size_t      size,
                       EOwnership  owner/*no default for satefy*/,
                       streamsize  buf_size = kConn_DefaultBufSize);

    virtual ~CConn_MemoryStream();

    /// The CConnMemoryStream::To* methods allow to obtain unread portion of
    /// the stream in a single container (a string, a vector, or a character
    /// array) so that all data is kept in sequential memory locations.

    NCBI_DEPRECATED
    string& ToString(string&); ///< fill in the data, return the argument

    void    ToString(string*); ///< fill in the data, NULL is not accepted
    void    ToVector(vector<char>*);///< fill in the data, NULL is not accepted
    char*   ToCStr(void);      ///< '\0'-terminated; delete when done using it 

protected:
    BUF         m_Buf;         ///< Underlying buffer (if owned)
    const void* m_Ptr;         ///< Pointer to read memory area (if owned)

private:
    // Disable copy constructor and assignment.
    CConn_MemoryStream(const CConn_MemoryStream&);
    CConn_MemoryStream& operator= (const CConn_MemoryStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_PipeStream
///

class NCBI_XCONNECT_EXPORT CConn_PipeStream : public CConn_IOStream
{
public:
    CConn_PipeStream
    (const string&         cmd,
     const vector<string>& args,
     CPipe::TCreateFlags   create_flags = 0,
     const STimeout*       timeout      = kDefaultTimeout,
     streamsize            buf_size     = kConn_DefaultBufSize
     );
    virtual ~CConn_PipeStream();

    CPipe& GetPipe(void) { return m_Pipe; }

protected:
    CPipe m_Pipe; ///< Underlying pipe.

private:
    // Disable copy constructor and assignment.
    CConn_PipeStream(const CConn_PipeStream&);
    CConn_PipeStream& operator= (const CConn_PipeStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_NamedPipeStream
///

class NCBI_XCONNECT_EXPORT CConn_NamedPipeStream : public CConn_IOStream
{
public:
    CConn_NamedPipeStream
    (const string&   pipename,
     size_t          pipebufsize = 0 /* default buffer size */,
     const STimeout* timeout     = kDefaultTimeout,
     streamsize      buf_size    = kConn_DefaultBufSize
     );

private:
    // Disable copy constructor and assignment.
    CConn_NamedPipeStream(const CConn_NamedPipeStream&);
    CConn_NamedPipeStream& operator= (const CConn_NamedPipeStream&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConn_FTPDownloadStream
///

class NCBI_XCONNECT_EXPORT CConn_FTPDownloadStream : public CConn_IOStream
{
public:
    CConn_FTPDownloadStream
    (const string&   host,
     const string&   file     = kEmptyStr,
     const string&   user     = "ftp",
     const string&   pass     = "-none",  // "-" often helps make login quieter
     const string&   path     = kEmptyStr,
     unsigned short  port     = 0,
     TFCDC_Flags     flag     = 0,
     streamsize      offset   = 0,
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize
     );

private:
    // Disable copy constructor and assignment.
    CConn_FTPDownloadStream(const CConn_FTPDownloadStream&);
    CConn_FTPDownloadStream& operator= (const CConn_FTPDownloadStream&);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_STREAM__HPP */
