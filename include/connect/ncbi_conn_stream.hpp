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
#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_namedpipe_connector.hpp>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_pipe_connector.hpp>
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>


/** @addtogroup ConnStreams
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CConn_Streambuf; // Forward declaration


const streamsize kConn_DefaultBufSize = 4096;


/*
 * Helper hook-up class that installs default logging/registry/locking
 * (if have not yet installed explicitly).
 */

class NCBI_XCONNECT_EXPORT CConn_IOStreamBase
{
protected:
    CConn_IOStreamBase();
};


/*
 * Base class, derived from "std::iostream", does both input
 * and output, using the specified CONNECTOR. Input operations
 * can be tied to the output ones by setting 'do_tie' to 'true'
 * (default), which means that any input attempt first flushes
 * the output queue from the internal buffers. 'buf_size'
 * designates the size of the I/O buffers, which reside in between
 * the stream and underlying connector (which in turn may do
 * further buffering, if needed).
 */

class NCBI_XCONNECT_EXPORT CConn_IOStream : virtual public CConn_IOStreamBase,
                                            public CNcbiIostream
{
public:
    CConn_IOStream
    (CONNECTOR       connector,
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize,
     bool            do_tie   = true);
    virtual ~CConn_IOStream();

    CONN GetCONN(void) const;

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



/*
 * This stream exchanges data in a TCP channel, using socket interface.
 * The endpoint is specified as host/port pair. The maximal
 * number of connection attempts is given as 'max_try'.
 * More details on that: <connect/ncbi_socket_connector.h>.
 */

class NCBI_XCONNECT_EXPORT CConn_SocketStream : public CConn_IOStream
{
public:
    CConn_SocketStream
    (const string&   host,         /* host to connect to  */
     unsigned short  port,         /* ... and port number */
     unsigned int    max_try  = 3, /* number of attempts  */
     const STimeout* timeout  = kDefaultTimeout,
     streamsize      buf_size = kConn_DefaultBufSize);

    // This variant uses existing socket "sock" to build the stream upon it.
    // NOTE:  it revokes all ownership of the socket, and further assumes the
    // socket being in exclusive use of this stream's underlying CONN.
    // More details:  <ncbi_socket_connector.h>::SOCK_CreateConnectorOnTop().
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



/*
 * This stream exchanges data with an HTTP server found by URL:
 * http://host[:port]/path[?args]
 *
 * Note that 'path' must include a leading slash,
 * 'args' can be empty, in which case the '?' is not appended to the path.
 *
 * 'User_header' (if not empty) should be a sequence of lines
 * in the form 'HTTP-tag: Tag value', separated by '\r\n', and
 * '\r\n'-terminated. It is included in the HTTP-header of each transaction.
 *
 * More elaborate specification of the server can be done via
 * SConnNetInfo structure, which otherwise will be created with the
 * use of a standard registry section to obtain default values
 * (details: <connect/ncbi_connutil.h>).  No user header is added if
 * the argument is passed as default (or empty string).  To make
 * sure the user header is passed empty, delete it from net_info
 * by ConnNetInfo_DeleteUserHeader(net_info).
 *
 * THCC_Flags and other details: <connect/ncbi_http_connector.h>.
 *
 * Provided 'timeout' is set at connection level, and if different from
 * CONN_DEFAULT_TIMEOUT, it overrides value supplied by HTTP connector
 * (the latter value is kept in SConnNetInfo::timeout).
 */

class NCBI_XCONNECT_EXPORT CConn_HttpStream : public CConn_IOStream
{
public:
    CConn_HttpStream
    (const string&   host,
     const string&   path,
     const string&   args        = kEmptyStr,
     const string&   user_header = kEmptyStr,
     unsigned short  port        = 80,
     THCC_Flags      flags       = fHCC_AutoReconnect,
     const STimeout* timeout     = kDefaultTimeout,
     streamsize      buf_size    = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const string&   url,
     THCC_Flags      flags       = fHCC_AutoReconnect,
     const STimeout* timeout     = kDefaultTimeout,
     streamsize      buf_size    = kConn_DefaultBufSize
     );

    CConn_HttpStream
    (const SConnNetInfo* net_info    = 0,
     const string&       user_header = kEmptyStr,
     THCC_Flags          flags       = fHCC_AutoReconnect,
     const STimeout*     timeout     = kDefaultTimeout,
     streamsize          buf_size    = kConn_DefaultBufSize
     );

private:
    // Disable copy constructor and assignment.
    CConn_HttpStream(const CConn_HttpStream&);
    CConn_HttpStream& operator= (const CConn_HttpStream&);
};



/*
 * This stream exchanges the data with a named service, in a
 * constraint that the service is implemented as one of the specified
 * server 'types' (details: <connect/ncbi_server_info.h>).
 *
 * Additional specifications can be passed in the SConnNetInfo structure,
 * otherwise created by using service name as a registry section
 * to obtain the information from (details: <connect/ncbi_connutil.h>).
 *
 * Provided 'timeout' is set at connection level, and if different from
 * CONN_DEFAULT_TIMEOUT, it overrides value supplied by underlying connector
 * (the latter value is kept in SConnNetInfo::timeout).
 */

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



class CRWLock; // Forward declaration

/*
 * In-memory stream.
 */

class NCBI_XCONNECT_EXPORT CConn_MemoryStream : public CConn_IOStream
{
public:
    CConn_MemoryStream(CRWLock*   lk = 0,
                       EOwnership lk_owner = eTakeOwnership,
                       streamsize buf_size = kConn_DefaultBufSize);
    // Build a stream on top of NCBI buffer (which could in turn
    // be built over a memory area of a specified size).
    CConn_MemoryStream(BUF        buf,
                       CRWLock*   lk = 0,
                       EOwnership lk_owner= eTakeOwnership,
                       streamsize buf_size = kConn_DefaultBufSize);
    virtual ~CConn_MemoryStream();

    string& ToString(string&); ///< fill in the data, return the argument
    char*   ToCStr(void);      ///< '\0'-terminated; delete when done using it 

protected:
    MT_LOCK m_Lock;            ///< I/O interlock
    BUF     m_Buf;             ///< Underlying buffer (if used)

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
    CPipe  m_Pipe; ///< Underlying pipe.

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
     const string&   user     = kEmptyStr,
     const string&   pass     = kEmptyStr,
     const string&   path     = kEmptyStr,
     unsigned short  port     = 0,
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


/*
 * ===========================================================================
 * $Log$
 * Revision 6.34  2005/03/15 21:28:22  lavr
 * +CConn_IOStream::Close()
 *
 * Revision 6.33  2004/12/08 21:01:20  lavr
 * +CConn_FTPDownloadStream
 *
 * Revision 6.32  2004/10/28 12:48:26  lavr
 * Memory stream lock ownership -> EOwnership, MT_LOCK cleanup in dtor()
 *
 * Revision 6.31  2004/10/27 18:53:22  lavr
 * +CConn_MemoryStream(BUF buf,...)
 *
 * Revision 6.30  2004/10/27 16:09:32  lavr
 * +CConn_MemoryStream::ToCStr()
 *
 * Revision 6.29  2004/10/26 20:30:41  lavr
 * +CConn_MemoryStream::ToString()
 *
 * Revision 6.28  2004/09/09 16:43:47  lavr
 * Introduce virtual helper base CConn_IOStreamBase for implicit CONNECT_Init()
 *
 * Revision 6.27  2004/06/22 16:51:22  lavr
 * Note handling of user_header in HTTP stream ctor's
 *
 * Revision 6.26  2004/03/22 16:54:05  ivanov
 * Cosmetic changes
 *
 * Revision 6.25  2003/11/12 17:42:40  lavr
 * Formal (non-functional) changes
 *
 * Revision 6.24  2003/11/12 16:36:12  ivanov
 * Added CConn_IOStream::Cleanup(), removed CConn_PipeStream::SetReadHandle()
 *
 * Revision 6.23  2003/10/23 12:16:48  lavr
 * CConn_IOStream:: base class is now CNcbiIostream
 *
 * Revision 6.22  2003/09/23 21:00:02  lavr
 * +CConn_PipeStream, +CConn_NamedPipeStream, +disabled copy ctors and assigns
 *
 * Revision 6.21  2003/08/25 14:47:00  lavr
 * Employ new k..Timeout constants
 *
 * Revision 6.20  2003/04/29 19:58:04  lavr
 * Constructor taking a URL added in CConn_HttpStream
 *
 * Revision 6.19  2003/04/11 17:55:30  lavr
 * Proper indentation of some fragments
 *
 * Revision 6.18  2003/04/09 17:58:42  siyan
 * Added doxygen support
 *
 * Revision 6.17  2003/01/17 19:44:20  lavr
 * Reduce dependencies
 *
 * Revision 6.16  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.15  2002/08/12 15:05:15  lavr
 * Included header files reordered
 *
 * Revision 6.14  2002/06/12 19:19:25  lavr
 * Guard macro name standardized
 *
 * Revision 6.13  2002/06/06 19:01:31  lavr
 * Take advantage of CConn_Exception class
 * Some housekeeping: guard macro name changed, log moved to the end
 *
 * Revision 6.12  2002/02/21 18:04:24  lavr
 * +class CConn_MemoryStream
 *
 * Revision 6.11  2002/01/28 20:17:43  lavr
 * +Forward declaration of CConn_Streambuf and a private member pointer
 * of this type (for clean destruction of a streambuf sub-object)
 *
 * Revision 6.10  2001/12/10 19:41:16  vakatov
 * + CConn_SocketStream::CConn_SocketStream(SOCK sock, ....)
 *
 * Revision 6.9  2001/12/07 22:55:41  lavr
 * More comments added
 *
 * Revision 6.8  2001/09/24 20:25:57  lavr
 * +SSERVICE_Extra* parameter for CConn_ServiceStream::CConn_ServiceStream()
 *
 * Revision 6.7  2001/04/24 21:18:41  lavr
 * Default timeout is set as a special value CONN_DEFAULT_TIMEOUT.
 * Removed wrong log for R6.6.
 *
 * Revision 6.5  2001/02/09 17:38:16  lavr
 * Typo fixed in comments
 *
 * Revision 6.4  2001/01/12 23:48:51  lavr
 * GetCONN method added
 *
 * Revision 6.3  2001/01/11 23:04:04  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.2  2001/01/10 21:41:08  lavr
 * Added classes: CConn_SocketStream, CConn_HttpStream, CConn_ServiceStream.
 * Everything is now wordly documented.
 *
 * Revision 6.1  2001/01/09 23:35:24  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CONN_STREAM__HPP */
