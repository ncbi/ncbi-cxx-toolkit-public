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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   CONN-based C++ streams
 *
 *   See file <connect/ncbi_conn_stream.hpp> for more detailed information.
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_conn_streambuf.hpp"
#include <connect/error_codes.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <corelib/ncbiapp.hpp>
#include <stdlib.h>


#define NCBI_USE_ERRCODE_X   Connect_Stream


BEGIN_NCBI_SCOPE


CConn_IOStream::CConn_IOStream(CONNECTOR connector, const STimeout* timeout,
                               streamsize buf_size, bool tie,
                               CT_CHAR_TYPE* ptr, size_t size) :
    CNcbiIostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(connector, timeout, buf_size, tie,
                                ptr, size));
    if (csb->GetCONN()) {
        init(csb.get());
        m_CSb = csb.release();
    } else
        init(0); // according to the standard (27.4.4.1.3), badbit is set here
}


CConn_IOStream::CConn_IOStream(CONN conn, bool close, const STimeout* timeout,
                               streamsize buf_size, bool tie,
                               CT_CHAR_TYPE* ptr, size_t size) :
    CNcbiIostream(0), m_CSb(0)
{
    if (conn) {
        auto_ptr<CConn_Streambuf>
            csb(new CConn_Streambuf(conn, close, timeout, buf_size, tie,
                                    ptr, size));
        init(csb.get());
        m_CSb = csb.release();
    } else
        init(0);
}


CConn_IOStream::~CConn_IOStream()
{
    x_Cleanup();
}


#define GET_CONN(sb)  ((sb) ? (sb)->GetCONN() : 0)


CONN CConn_IOStream::GetCONN(void) const
{
    return GET_CONN(m_CSb);
}


string CConn_IOStream::GetType(void) const
{
    CONN        conn = GET_CONN(m_CSb);
    const char* type = conn ? CONN_GetType(conn) : 0;
    return type ? type : kEmptyStr;
}


string CConn_IOStream::GetDescription(void) const
{
    CONN   conn = GET_CONN(m_CSb);
    char*  text = conn ? CONN_Description(conn) : 0;
    string retval(text ? text : kEmptyStr);
    if (text)
        free(text);
    return retval;
}


EIO_Status CConn_IOStream::SetTimeout(EIO_Event       direction,
                                      const STimeout* timeout) const
{
    CONN conn = GET_CONN(m_CSb);
    return conn ? CONN_SetTimeout(conn, direction, timeout) : eIO_Closed;
}


const STimeout* CConn_IOStream::GetTimeout(EIO_Event direction) const
{
    CONN conn = GET_CONN(m_CSb);
    return conn ? CONN_GetTimeout(conn, direction) : 0;
}


EIO_Status CConn_IOStream::Status(EIO_Event dir) const
{
    return m_CSb ? m_CSb->Status(dir) : eIO_NotSupported;
}


EIO_Status CConn_IOStream::Close(void)
{
    return m_CSb ? m_CSb->Close() : eIO_Closed;
}


void CConn_IOStream::x_Cleanup(void)
{
    CConn_Streambuf* sb = m_CSb;
    m_CSb = 0;
    delete sb;
}


EIO_Status CConn_IOStream::SetCanceledCallback(const ICanceled* canceled)
{
    CONN conn = GetCONN();
    if (!conn)
        return eIO_Closed;

    bool isset = m_Canceled.NotNull() ? 1 : 0;

    if (canceled) {
        SCONN_Callback cb;
        m_Canceled = canceled;
        memset(&cb, 0, sizeof(cb));
        cb.func = (FCONN_Callback) x_IsCanceled;
        cb.data = this;
        CONN_SetCallback(conn, eCONN_OnRead,  &cb,      isset ? 0 : &m_CB[0]);
        CONN_SetCallback(conn, eCONN_OnWrite, &cb,      isset ? 0 : &m_CB[1]);
        CONN_SetCallback(conn, eCONN_OnFlush, &cb,      isset ? 0 : &m_CB[2]);
    } else if (isset) {
        CONN_SetCallback(conn, eCONN_OnFlush, &m_CB[2], 0);
        CONN_SetCallback(conn, eCONN_OnWrite, &m_CB[1], 0);
        CONN_SetCallback(conn, eCONN_OnRead,  &m_CB[0], 0);
        m_Canceled = 0;
    }

    return eIO_Success;
}


EIO_Status CConn_IOStream::x_IsCanceled(CONN           conn,
                                        ECONN_Callback type,
                                        void*          data)
{
    _ASSERT(conn  &&  data);
    CConn_IOStream* io = reinterpret_cast<CConn_IOStream*>(data);
    if (/* io && */ io->m_Canceled.NotNull()  &&  io->m_Canceled->IsCanceled())
        return eIO_Interrupt;
    int n = (int) type - (int) eIO_Read;
    _ASSERT(n >= 0  &&  (size_t) n < sizeof(io->m_CB) / sizeof(io->m_CB[0]));
    _ASSERT((n == 0  &&  type == eCONN_OnRead)   ||
            (n == 1  &&  type == eCONN_OnWrite)  ||
            (n == 2  &&  type == eCONN_OnFlush));
    if (!io->m_CB[n].func)
        return eIO_Success;
    return io->m_CB[n].func(conn, type, io->m_CB[n].data);
}


static CONNECTOR x_SC(CONNECTOR socket_connector, SOCK*& sockptr)
{
    // HACK * HACK * HACK
    sockptr = socket_connector ? (SOCK*) socket_connector->handle : 0;
    return socket_connector;
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       unsigned short  max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(x_SC(SOCK_CreateConnector(host.c_str(), port, max_try),
                          m_SockPtr),
                     timeout, buf_size)
{
    return;
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       const void*     data,
                                       size_t          size,
                                       TSOCK_Flags     flags,
                                       unsigned short  max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(x_SC(SOCK_CreateConnectorEx(host.c_str(), port, max_try,
                                                 data, size, flags),
                          m_SockPtr),
                     timeout, buf_size)
{
    return;
}


CConn_SocketStream::CConn_SocketStream(SOCK            sock,
                                       EOwnership      if_to_own,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(x_SC(SOCK_CreateConnectorOnTop(sock,
                                                    if_to_own != eNoOwnership),
                          m_SockPtr),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_TunneledSocketConnector(const SConnNetInfo* net_info,
                                           const void*         init_data,
                                           size_t              init_size,
                                           TSOCK_Flags         flags)
{
    EIO_Status status;
    SOCK       sock = 0;

    _ASSERT(net_info);
    if (*net_info->http_proxy_host  &&  net_info->http_proxy_port) {
        status = HTTP_CreateTunnel(net_info, fHTTP_DetachableTunnel
                                   | fHTTP_NoAutoRetry, &sock);
        if (status == eIO_Success) {
            size_t handle_size = SOCK_OSHandleSize();
            char*  handle      = new char[handle_size];
            status = SOCK_GetOSHandle(sock, handle, handle_size);
            if (status == eIO_Success) {
                SOCK_Close(sock);
                status = SOCK_CreateOnTopEx(handle, handle_size, &sock,
                                            init_data, init_size, flags);
                if (status != eIO_Success) {
                    SOCK_CloseOSHandle(handle, handle_size);
                    _ASSERT(!sock);
                } else
                    _ASSERT(sock);
            } else {
                SOCK_Abort(sock);
                SOCK_Close(sock);
            }
            delete[] handle;
        } else
            _ASSERT(!sock);
        if (!sock  &&  !net_info->http_proxy_leak)
            return 0;
    }
    if (!sock) {
        const char* host = (net_info->firewall  &&  *net_info->proxy_host
                            ? net_info->proxy_host : net_info->host);
        status = SOCK_CreateEx(host, net_info->port, net_info->timeout, &sock,
                               init_data, init_size, flags);
        _ASSERT(!sock ^ !(status != eIO_Success));
    }
    string hostport(net_info->host);
    hostport += ':';
    hostport += NStr::UIntToString(net_info->port);
    CONNECTOR c;
    if (!(c = SOCK_CreateConnectorOnTopEx(sock, 1/*own*/, hostport.c_str()))) {
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    return c;
}


CConn_SocketStream::CConn_SocketStream(const SConnNetInfo& net_info,
                                       const void*         data,
                                       size_t              size,
                                       TSOCK_Flags         flags,
                                       streamsize          buf_size)
    : CConn_IOStream(x_SC(s_TunneledSocketConnector(&net_info,
                                                    data, size, flags),
                          m_SockPtr),
                     net_info.timeout, buf_size)
{
    return;
}


static SOCK s_GrabSOCK(CSocket& socket)
{
    SOCK sock = socket.GetSOCK();
    if (!sock) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_SocketStream::CConn_SocketStream(): "
                   "Socket may not be empty");
    }
    if (socket.SetOwnership(eNoOwnership) == eNoOwnership) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_SocketStream::CConn_SocketStream(): "
                   "Socket must be owned");
    }
    socket.Reset(0/*empty*/,
                 eNoOwnership/*irrelevant*/,
                 eCopyTimeoutsFromSOCK/*irrelevant*/);
    return sock;
}


CConn_SocketStream::CConn_SocketStream(CSocket&        socket,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(x_SC(SOCK_CreateConnectorOnTop(s_GrabSOCK(socket),
                                                    1/*own*/),
                          m_SockPtr),
                     timeout, buf_size)
{
    return;
}


static void x_SetupUserAgent(SConnNetInfo* net_info)
{
    CNcbiApplication* theApp = CNcbiApplication::Instance();
    if (theApp) {
        string user_agent("User-Agent: ");
        user_agent += theApp->GetProgramDisplayName();
        ConnNetInfo_ExtendUserHeader(net_info, user_agent.c_str());
    }
}


template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


static CONNECTOR s_HttpConnectorBuilder(const SConnNetInfo* x_net_info,
                                        const char*         url,
                                        const char*         host,
                                        unsigned short      port,
                                        const char*         path,
                                        const char*         args,
                                        const char*         user_header,
                                        FHTTP_ParseHeader   parse_header,
                                        void*               user_data,
                                        FHTTP_Adjust        adjust,
                                        FHTTP_Cleanup       cleanup,
                                        THTTP_Flags         flags,
                                        const STimeout*     timeout)
{
    size_t len;
    AutoPtr<SConnNetInfo>
        net_info(x_net_info
                 ? ConnNetInfo_Clone(x_net_info) : ConnNetInfo_Create(0));
    if (!net_info.get()) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_HttpStream::CConn_HttpStream():  Out of memory");
    }
    if (url  &&  !ConnNetInfo_ParseURL(net_info.get(), url)) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream():  Bad URL");
    }
    if (host) {
        if ((len = *host ? strlen(host) : 0) >= sizeof(net_info->host)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Host too long");
        }
        memcpy(net_info->host, host, ++len);
    }
    if (port)
        net_info->port = port;
    if (path) {
        if ((len = *path ? strlen(path) : 0) >= sizeof(net_info->path)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Path too long");
        }
        memcpy(net_info->path, path, ++len);
    }
    if (args) {
        if ((len = *args ? strlen(args) : 0) >= sizeof(net_info->args)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Args too long");
        }
        memcpy(net_info->args, args, ++len);
    }
    if (user_header  &&  *user_header)
        ConnNetInfo_OverrideUserHeader(net_info.get(), user_header);
    x_SetupUserAgent(net_info.get());
    if (timeout  &&  timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    return HTTP_CreateConnectorEx(net_info.get(), flags,
                                  parse_header, user_data, adjust, cleanup);
}


CConn_HttpStream::CConn_HttpStream(const string&   host,
                                   const string&   path,
                                   const string&   args,
                                   const string&   user_header,
                                   unsigned short  port,
                                   THTTP_Flags     flags,
                                   const STimeout* timeout,
                                   streamsize      buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(0,
                                            0,
                                            host.c_str(),
                                            port,
                                            path.c_str(),
                                            args.c_str(),
                                            user_header.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&   url,
                                   THTTP_Flags     flags,
                                   const STimeout* timeout,
                                   streamsize      buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(0,
                                            url.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&       url,
                                   const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   THTTP_Flags         flags,
                                   const STimeout*     timeout,
                                   streamsize          buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            url.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   FHTTP_ParseHeader   parse_header,
                                   void*               user_data,
                                   FHTTP_Adjust        adjust,
                                   FHTTP_Cleanup       cleanup,
                                   THTTP_Flags         flags,
                                   const STimeout*     timeout,
                                   streamsize          buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            parse_header,
                                            user_data,
                                            adjust,
                                            cleanup,
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_ServiceConnectorBuilder(const char*           service,
                                           TSERV_Type            types,
                                           const SConnNetInfo*   x_net_info,
                                           const char*           user_header,
                                           const SSERVICE_Extra* params,
                                           const STimeout*       timeout)
{
    AutoPtr<SConnNetInfo>
        net_info(x_net_info ?
                 ConnNetInfo_Clone(x_net_info) : ConnNetInfo_Create(service));
    if (!net_info.get()) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_ServiceStream::CConn_ServiceStream(): "
                   " Out of memory");
    }
    if (user_header  &&  *user_header)
        ConnNetInfo_OverrideUserHeader(net_info.get(), user_header);
    x_SetupUserAgent(net_info.get());
    if (timeout  &&  timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    CONNECTOR c = SERVICE_CreateConnectorEx(service, types,
                                            net_info.get(), params);
    if (!c) {
        ERR_POST_X(1,
                   Error << "Cannot connect to service \"" << service << '\"');
    }
    return c;
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         TSERV_Type            types,
                                         const SConnNetInfo*   net_info,
                                         const SSERVICE_Extra* params,
                                         const STimeout*       timeout,
                                         streamsize            buf_size)
    : CConn_IOStream(s_ServiceConnectorBuilder(service.c_str(),
                                               types,
                                               net_info,
                                               0,
                                               params,
                                               timeout),
                     timeout, buf_size)
{
    return;
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         const string&         user_header,
                                         TSERV_Type            types,
                                         const SSERVICE_Extra* params,
                                         const STimeout*       timeout,
                                         streamsize            buf_size)
    : CConn_IOStream(s_ServiceConnectorBuilder(service.c_str(),
                                               types,
                                               0,
                                               user_header.c_str(),
                                               params,
                                               timeout),
                     timeout, buf_size)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(streamsize buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(), 0, buf_size, true),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(BUF        buf,
                                       EOwnership owner,
                                       streamsize buf_size)
    : CConn_IOStream(MEMORY_CreateConnectorEx(buf, owner == eTakeOwnership
                                              ? 1/*true*/
                                              : 0/*false*/), 0, buf_size, true,
                     0, BUF_Size(buf)),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(const void* ptr,
                                       size_t      size,
                                       EOwnership  owner,
                                       streamsize  buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(), 0, buf_size, true,
                     (CT_CHAR_TYPE*) ptr, size),
      m_Ptr(owner == eTakeOwnership ? ptr : 0)
{
    return;
}


CConn_MemoryStream::~CConn_MemoryStream()
{
    // Explicitly call x_Cleanup() to avoid using deleted m_Ptr otherwise.
    x_Cleanup();
    rdbuf(0);
    delete[] (char*) m_Ptr;
}


void CConn_MemoryStream::ToString(string* str)
{
    if (!str) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToString(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    streamsize size = sb ? (size_t)(tellp() - tellg()) : 0;
    str->resize(size);
    if (sb) {
        streamsize s = sb->sgetn(&(*str)[0], size);
        _ASSERT(size == s);
        str->resize(s);  // NB: this is essentially a NOOP when size == s
    }
}


void CConn_MemoryStream::ToVector(vector<char>* vec)
{
    if (!vec) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToVector(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    streamsize size = sb ? (streamsize)(tellp() - tellg()) : 0;
    vec->resize(size);
    if (sb) {
        streamsize s = sb->sgetn(&(*vec)[0], size);
        _ASSERT(size == s);
        vec->resize(s);  // NB: this is essentially a NOOP when size == s
    }
}


char* CConn_MemoryStream::ToCStr(void)
{
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    streamsize size = sb ? (size_t)(tellp() - tellg()) : 0;
    char* str = (char*) malloc(size + 1);
    if (!str) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_MemoryStream::ToCStr():  Out of memory");
    }
    if (sb) {
        streamsize s = sb->sgetn(str, size);
        _ASSERT(size == s);
        size = s;
    }
    str[size] = '\0';
    return str;
}


CConn_PipeStream::CConn_PipeStream(const string&         cmd,
                                   const vector<string>& args,
                                   CPipe::TCreateFlags   create_flags,
                                   const STimeout*       timeout,
                                   streamsize            buf_size)
    : CConn_IOStream(PIPE_CreateConnector(cmd, args, create_flags,
                                          &m_Pipe, eNoOwnership),
                     timeout, buf_size)
{
    return;
}


CConn_PipeStream::~CConn_PipeStream()
{
    // Explicitly call x_Cleanup() to avoid using dead m_Pipe otherwise.
    x_Cleanup();
    rdbuf(0);
}


CConn_NamedPipeStream::CConn_NamedPipeStream(const string&   pipename,
                                             size_t          pipebufsize,
                                             const STimeout* timeout,
                                             streamsize      buf_size)
    : CConn_IOStream(NAMEDPIPE_CreateConnector(pipename, pipebufsize),
                     timeout, buf_size)
{
    return;
}


/* For data integrity and unambigous interpretation, FTP streams are not
 * buffered at the level of C++ STL streambuf because of the way they execute
 * read / write operations on the mix of FTP commands and data.
 * There should be a little impact on performance of byte-by-byte I/O (such as
 * formatted input, which is not expected very often for this kind of streams,
 * anyways), and almost none for block I/O (such as read / readsome / write).
 */
CConn_FtpStream::CConn_FtpStream(const string&        host,
                                 const string&        user,
                                 const string&        pass,
                                 const string&        path,
                                 unsigned short       port,
                                 TFTP_Flags           flag,
                                 const SFTP_Callback* cmcb,
                                 const STimeout*      timeout)
    : CConn_IOStream(FTP_CreateConnectorSimple(host.c_str(), port,
                                               user.c_str(), pass.c_str(),
                                               path.c_str(), flag, cmcb),
                     timeout, 0/*must be unbuffered*/, false/*thus,untied*/)
{
    return;
}


EIO_Status CConn_FtpStream::Drain(const STimeout* timeout)
{
    const STimeout* r_timeout = 0;
    const STimeout* w_timeout = 0;
    CONN conn = GetCONN();
    char block[1024];
    if (conn) {
        size_t n;
        r_timeout = CONN_GetTimeout(conn, eIO_Read);
        w_timeout = CONN_GetTimeout(conn, eIO_Write);
        _VERIFY(SetTimeout(eIO_Read,  timeout) == eIO_Success);
        _VERIFY(SetTimeout(eIO_Write, timeout) == eIO_Success);
        // Cause any upload-in-progress to abort
        CONN_Read (conn, block, sizeof(block), &n, eIO_ReadPlain);
        // Cause any command-in-progress to abort
        CONN_Write(conn, "NOOP\n", 5, &n, eIO_WritePersist);
    }
    clear();
    while (read(block, sizeof(block)))
        ;
    if (!conn)
        return eIO_Closed;
    EIO_Status status;
    do {
        size_t n;
        status = CONN_Read(conn, block, sizeof(block), &n, eIO_ReadPersist);
    } while (status == eIO_Success);
    _VERIFY(CONN_SetTimeout(conn, eIO_Read,  r_timeout) == eIO_Success);
    _VERIFY(CONN_SetTimeout(conn, eIO_Write, w_timeout) == eIO_Success);
    clear();
    return status == eIO_Closed ? eIO_Success : status;
}


CConn_FTPDownloadStream::CConn_FTPDownloadStream(const string&        host,
                                                 const string&        file,
                                                 const string&        user,
                                                 const string&        pass,
                                                 const string&        path,
                                                 unsigned short       port,
                                                 TFTP_Flags           flag,
                                                 const SFTP_Callback* cmcb,
                                                 Uint8                offset,
                                                 const STimeout*      timeout)
    : CConn_FtpStream(host, user, pass, path, port, flag, cmcb, timeout)
{
    if (!file.empty()) {
        EIO_Status status;
        if (offset) {
            write("REST ", 5) << NStr::UInt8ToString(offset) << NcbiFlush;
            status = Status(eIO_Write);
        } else
            status = eIO_Success;
        if (good()  &&  status == eIO_Success) {
            write("RETR ", 5) << file << NcbiFlush;
        }
    }
}


CConn_FTPUploadStream::CConn_FTPUploadStream(const string&   host,
                                             const string&   user,
                                             const string&   pass,
                                             const string&   file,
                                             const string&   path,
                                             unsigned short  port,
                                             TFTP_Flags      flag,
                                             Uint8           offset,
                                             const STimeout* timeout)
    : CConn_FtpStream(host, user, pass, path, port, flag, 0/*cmcb*/, timeout)
{
    if (!file.empty()) {
        EIO_Status status;
        if (offset) {
            write("REST ", 5) << NStr::UInt8ToString(offset) << NcbiFlush;
            status = Status(eIO_Write);
        } else
            status = eIO_Success;
        if (good()  &&  status == eIO_Success) {
            write("STOR ", 5) << file << NcbiFlush;
        }
    }
}


const char* CIO_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eTimeout:       return "eIO_Timeout";
    case eClosed:        return "eIO_Closed";
    case eInterrupt:     return "eIO_Interrupt";
    case eInvalidArg:    return "eIO_InvalidArg";
    case eNotSupported:  return "eIO_NotSupported";
    case eUnknown:       return "eIO_Unknown";
    default:             return  CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
