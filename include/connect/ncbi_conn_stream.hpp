#ifndef NCBI_CONN_STREAM__HPP
#define NCBI_CONN_STREAM__HPP

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
*   CConn_IOStream - base class derived from "std::iostream" to perform I/O
*                    by means of underlying CConn_Streambuf (implemented
*                    privately in ncbi_conn_streambuf.[ch]pp).
*
*   CConn_SocketStream  - I/O stream based on socket connector.
*
*   CConn_HttpStream    - I/O stream based on HTTP connector (that is,
*                         the stream, which connects to HTTP server and
*                         exchanges information using HTTP protocol).
*
*   CConn_ServiceStream - I/O stream based on service connector, which is
*                         able to exchange data to/from a named service
*                         that can be found via dispatcher/load-balancing
*                         daemon and implemented as either HTTP GCI,
*                         standalone server, or NCBID service.
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_service_connector.h>


BEGIN_NCBI_SCOPE


const streamsize kConn_DefBufSize = 4096;


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

class CConn_IOStream : public iostream
{
public:
    CConn_IOStream(CONNECTOR  connector,
                   streamsize buf_size = kConn_DefBufSize,
                   bool       do_tie   = true);
    virtual ~CConn_IOStream(void);
};



/*
 * This stream exchanges data in a TCP channel, using socket interface.
 * The endpoint is specified as host/port pair. The maximal
 * number of connection attempts is given as 'max_try'.
 * More details on that: <connect/ncbi_socket_connector.h>.
 */

class CConn_SocketStream : public CConn_IOStream
{
public:
    CConn_SocketStream(const string&  host,         /* host to connect to  */
                       unsigned short port,         /* ... and port number */
                       unsigned int   max_try  = 3, /* number of attempts  */
                       streamsize     buf_size = kConn_DefBufSize);
};



/*
 * This stream exchanges data with an HTTP server found by URL:
 * http://host[:port]/path[?args]
 *
 * Note that 'path' must include a leading slash,
 * 'args' can be empty, is that case the '?' is not appended to the path.
 *
 * 'User_header' (if not empty) should be a sequence of lines
 * in the form 'HTTP-tag: Tag value', separated by '\r\n', and
 * '\r\n'-terminated. It is included in the HTTP-header of each transaction.
 *
 * More elaborate specification of the server can be done via
 * SConnNetInfo structure, which otherwise will be created with the
 * use of a standard registry section to obtain default values
 * (details: <connect/ncbi_connutil.h>).
 *
 * THCC_Flags and other details: <connect/ncbi_http_connector.h>.
 */

class CConn_HttpStream : public CConn_IOStream
{
public:
    CConn_HttpStream(const string&  host,
                     const string&  path,
                     const string&  args        = kEmptyStr,
                     const string&  user_header = kEmptyStr,
                     unsigned short port        = 80,
                     THCC_Flags     flags       = fHCC_AutoReconnect,
                     streamsize     buf_size    = kConn_DefBufSize);
    
    CConn_HttpStream(const SConnNetInfo* info        = 0,
                     const string&       user_header = kEmptyStr,
                     THCC_Flags          flags       = fHCC_AutoReconnect,
                     streamsize          buf_size    = kConn_DefBufSize);
};



/*
 * This stream exchanges the data with a named service, in a
 * constraint that the service is implemented as one of the specified
 * server 'types' (details: <connect/ncbi_server_info.h>).
 *
 * Additional specifications can be passed in the SConnNetInfo structure,
 * otherwise created by using service name as a registry section
 * to obtain the information from (details: <connect/ncbi_connutil.h>).
 */

class CConn_ServiceStream : public CConn_IOStream
{
public:
    CConn_ServiceStream(const string&       service,
                        TSERV_Type          types    = fSERV_Any,
                        const SConnNetInfo* info     = 0,
                        streamsize          buf_size = kConn_DefBufSize);
};


END_NCBI_SCOPE

#endif  /* NCBI_CONN_STREAM__HPP */
