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
#include <corelib/ncbiapp.hpp>
#include "ncbi_conn_streambuf.hpp"
#include <connect/ncbi_conn_exception.hpp>
#define NCBI_CONN_STREAM_EXPERIMENTAL_API 1  // Pick up MS-Win DLL linkage
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_file_connector.h>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_util.h>
#include <stdlib.h>
#include <string.h>


BEGIN_NCBI_SCOPE


CConn_IOStream::CConn_IOStream(CONNECTOR connector, const STimeout* timeout,
                               size_t buf_size, TConn_Flags flags,
                               CT_CHAR_TYPE* ptr, size_t size)
    : CNcbiIostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(connector, timeout, buf_size, flags,
                                ptr, size));
    CONN conn = csb->GetCONN();
    if (conn) {
        SOCK s/*dummy*/;
        // CONN_Write(0 bytes) could have caused the same effect as GetSOCK
        (void) CONN_GetSOCK(conn, &s);  // Prompt CONN to actually open
        if (CONN_Status(conn, eIO_Open) == eIO_Success) {
            init(csb.get());
            m_CSb = csb.release();
            return;
        }
    }
    init(0); // according to the standard (27.4.4.1.3), badbit is set here
}


CConn_IOStream::CConn_IOStream(CONN conn, bool close, const STimeout* timeout,
                               size_t buf_size, TConn_Flags flags,
                               CT_CHAR_TYPE* ptr, size_t size)
    : CNcbiIostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(conn, close, timeout, buf_size, flags,
                                ptr, size));
    if (conn) {
        SOCK s/*dummy*/;
        // CONN_Write(0 bytes) could have caused the same effect as GetSOCK
        (void) CONN_GetSOCK(conn, &s);  // Prompt CONN to actually open
        if (CONN_Status(conn, eIO_Open) == eIO_Success) {
            init(csb.get());
            m_CSb = csb.release();
            return;
        }
    }
    init(0); // according to the standard (27.4.4.1.3), badbit is set here
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
    int n = (int) type - (int) eCONN_OnRead;
    _ASSERT(n >= 0  &&  (size_t) n < sizeof(io->m_CB) / sizeof(io->m_CB[0]));
    _ASSERT((n == 0  &&  type == eCONN_OnRead)   ||
            (n == 1  &&  type == eCONN_OnWrite)  ||
            (n == 2  &&  type == eCONN_OnFlush));
    if (!io->m_CB[n].func)
        return eIO_Success;
    return io->m_CB[n].func(conn, type, io->m_CB[n].data);
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       unsigned short  max_try,
                                       const STimeout* timeout,
                                       size_t          buf_size)
    : CConn_IOStream(SOCK_CreateConnector(host.c_str(), port, max_try),
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
                                       size_t          buf_size)
    : CConn_IOStream(SOCK_CreateConnectorEx(host.c_str(), port, max_try,
                                            data, size, flags),
                     timeout, buf_size)
{
    return;
}


CConn_SocketStream::CConn_SocketStream(SOCK            sock,
                                       EOwnership      if_to_own,
                                       const STimeout* timeout,
                                       size_t          buf_size)
    : CConn_IOStream(SOCK_CreateConnectorOnTop(sock,if_to_own != eNoOwnership),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_SocketConnectorBuilder(const SConnNetInfo* net_info,
                                          const STimeout*     timeout,
                                          const void*         data,
                                          size_t              size,
                                          TSOCK_Flags         flags)
{
    EIO_Status status;
    SOCK       sock = 0;
    bool       proxy = false;

    _ASSERT(net_info);
    if ((flags & (fSOCK_LogOn | fSOCK_LogDefault)) == fSOCK_LogDefault
        &&  net_info->debug_printout == eDebugPrintout_Data) {
        flags &= ~fSOCK_LogDefault;
        flags |=  fSOCK_LogOn;
    }
    if (*net_info->http_proxy_host  &&  net_info->http_proxy_port) {
        status = HTTP_CreateTunnel(net_info, fHTTP_NoAutoRetry, &sock);
        _ASSERT(!sock ^ !(status != eIO_Success));
        if (status == eIO_Success
            &&  ((flags & ~(fSOCK_LogOn | fSOCK_LogDefault))  ||  size)) {
            SOCK s;
            status = SOCK_CreateOnTopEx(sock, 0, &s,
                                        data, size, flags);
            _ASSERT(!s ^ !(status != eIO_Success));
            SOCK_Destroy(sock);
            sock = s;
        }
        proxy = true;
    }
    if (!sock  &&  (!proxy  ||  net_info->http_proxy_leak)) {
        const char* host = (net_info->firewall  &&  *net_info->proxy_host
                            ? net_info->proxy_host : net_info->host);
        if (timeout == kDefaultTimeout)
            timeout  = net_info->timeout;
        if (!proxy  &&  net_info->debug_printout) {
            SConnNetInfo* x_net_info = ConnNetInfo_Clone(net_info);
            if (x_net_info) {
                x_net_info->req_method = eReqMethod_Any;
                x_net_info->stateless = 0;
                x_net_info->lb_disable = 0;
                x_net_info->http_proxy_host[0] = '\0';
                x_net_info->http_proxy_port    =   0;
                x_net_info->http_proxy_user[0] = '\0';
                x_net_info->http_proxy_pass[0] = '\0';
                x_net_info->proxy_host[0]      = '\0';
                ConnNetInfo_SetUserHeader(x_net_info, 0);
                if (x_net_info->http_referer) {
                    free((void*) x_net_info->http_referer);
                    x_net_info->http_referer = 0;
                }
                x_net_info->timeout = timeout;
            }
            ConnNetInfo_Log(x_net_info, eLOG_Note, CORE_GetLOG());
            ConnNetInfo_Destroy(x_net_info);
        }
        status = SOCK_CreateEx(host, net_info->port, timeout, &sock,
                               data, size, flags);
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
                                       const STimeout*     timeout,
                                       size_t              buf_size)
    : CConn_IOStream(s_SocketConnectorBuilder(&net_info, timeout,
                                              data, size, flags),
                     timeout, buf_size)
{
    return;
}


static SOCK s_GrabSOCK(CSocket& socket)
{
    SOCK sock = socket.GetSOCK();
    if (!sock) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_SocketStream::CConn_SocketStream(): "
                   " Socket may not be empty");
    }
    if (socket.SetOwnership(eNoOwnership) == eNoOwnership) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_SocketStream::CConn_SocketStream(): "
                   " Socket must be owned");
    }
    socket.Reset(0/*empty*/,
                 eNoOwnership/*irrelevant*/,
                 eCopyTimeoutsFromSOCK/*irrelevant*/);
    return sock;
}


CConn_SocketStream::CConn_SocketStream(CSocket&        socket,
                                       const STimeout* timeout,
                                       size_t          buf_size)
    : CConn_IOStream(SOCK_CreateConnectorOnTop(s_GrabSOCK(socket), 1/*own*/),
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


static CONNECTOR s_HttpConnectorBuilder(const SConnNetInfo* net_info,
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
        x_net_info(net_info
                   ? ConnNetInfo_Clone(net_info) : ConnNetInfo_Create(0));
    if (!x_net_info.get()) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_HttpStream::CConn_HttpStream():  Out of memory");
    }
    if (url  &&  !ConnNetInfo_ParseURL(x_net_info.get(), url)) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream():  Bad URL");
    }
    if (host) {
        if ((len = *host ? strlen(host) : 0) >= sizeof(x_net_info->host)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Host too long");
        }
        memcpy(x_net_info->host, host, ++len);
    }
    if (port)
        x_net_info->port = port;
    if (path) {
        if ((len = *path ? strlen(path) : 0) >= sizeof(x_net_info->path)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Path too long");
        }
        memcpy(x_net_info->path, path, ++len);
    }
    if (args) {
        if ((len = *args ? strlen(args) : 0) >= sizeof(x_net_info->args)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream():  Args too long");
        }
        memcpy(x_net_info->args, args, ++len);
    }
    if (user_header  &&  *user_header)
        ConnNetInfo_OverrideUserHeader(x_net_info.get(), user_header);
    x_SetupUserAgent(x_net_info.get());
    if (timeout != kDefaultTimeout)
        x_net_info->timeout = timeout;
    return HTTP_CreateConnectorEx(x_net_info.get(), flags,
                                  parse_header, user_data, adjust, cleanup);
}


CConn_HttpStream::CConn_HttpStream(const string&   host,
                                   const string&   path,
                                   const string&   args,
                                   const string&   user_header,
                                   unsigned short  port,
                                   THTTP_Flags     flags,
                                   const STimeout* timeout,
                                   size_t          buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(0,
                                            0,
                                            host.c_str(),
                                            port,
                                            path.c_str(),
                                            args.c_str(),
                                            user_header.c_str(),
                                            x_ParseHeader,
                                            this,
                                            0,
                                            0,
                                            flags,
                                            timeout),
                     timeout, buf_size),
      m_UserParseHeader(0), m_UserData(0), m_UserAdjust(0), m_UserCleanup(0),
      m_Code(0)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&   url,
                                   THTTP_Flags     flags,
                                   const STimeout* timeout,
                                   size_t          buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(0,
                                            url.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            x_ParseHeader,
                                            this,
                                            0,
                                            0,
                                            flags,
                                            timeout),
                     timeout, buf_size),
      m_UserParseHeader(0), m_UserData(0), m_UserAdjust(0), m_UserCleanup(0),
      m_Code(0)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&       url,
                                   const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   FHTTP_ParseHeader   parse_header,
                                   void*               user_data,
                                   FHTTP_Adjust        adjust,
                                   FHTTP_Cleanup       cleanup,
                                   THTTP_Flags         flags,
                                   const STimeout*     timeout,
                                   size_t              buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            url.c_str(),
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            x_ParseHeader,
                                            this,
                                            adjust  ? x_Adjust  : 0,
                                            cleanup ? x_Cleanup : 0,
                                            flags,
                                            timeout),
                     timeout, buf_size),
      m_UserParseHeader(parse_header), m_UserData(user_data),
      m_UserAdjust(adjust), m_UserCleanup(cleanup),
      m_Code(0)
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
                                   size_t              buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            x_ParseHeader,
                                            this,
                                            adjust  ? x_Adjust  : 0,
                                            cleanup ? x_Cleanup : 0,
                                            flags,
                                            timeout),
                     timeout, buf_size),
      m_UserParseHeader(parse_header), m_UserData(user_data),
      m_UserAdjust(adjust), m_UserCleanup(cleanup),
      m_Code(0)
{
    return;
}


EHTTP_HeaderParse CConn_HttpStream::x_ParseHeader(const char* header,
                                                  void*       data,
                                                  int         code)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    int c, n;
    if (sscanf(header, "%*s %u%n", &c, &n) < 1)
        return eHTTP_HeaderError;
    _ASSERT(!code  ||  code == c);
    const char* str = header + n;
    str += strspn(str, " \t");
    const char* eol = strchr(str, '\n');
    if (!eol)
        eol = str + strlen(str);
    while (eol > str) {
        if (!isspace((unsigned char) eol[-1]))
            break;
        --eol;
    }
    http->m_Code = c;
    http->m_Text.assign(str, (size_t)(eol - str));
    return http->m_UserParseHeader
        ? http->m_UserParseHeader(header, http->m_UserData, code)
        : eHTTP_HeaderSuccess;
}


int/*bool*/ CConn_HttpStream::x_Adjust(SConnNetInfo* net_info,
                                       void*         data,
                                       unsigned int  count)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    return http->m_UserAdjust(net_info, http->m_UserData, count);
}


void CConn_HttpStream::x_Cleanup(void* data)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    http->m_UserCleanup(http->m_UserData);
}


static CONNECTOR s_ServiceConnectorBuilder(const char*           service,
                                           TSERV_Type            types,
                                           const SConnNetInfo*   net_info,
                                           const char*           user_header,
                                           const SSERVICE_Extra* params,
                                           const STimeout*       timeout)
{
    AutoPtr<SConnNetInfo>
        x_net_info(net_info ?
                   ConnNetInfo_Clone(net_info) : ConnNetInfo_Create(service));
    if (!x_net_info.get()) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_ServiceStream::CConn_ServiceStream(): "
                   " Out of memory");
    }
    if (user_header  &&  *user_header)
        ConnNetInfo_OverrideUserHeader(x_net_info.get(), user_header);
    x_SetupUserAgent(x_net_info.get());
    if (timeout != kDefaultTimeout)
        x_net_info->timeout = timeout;
    return SERVICE_CreateConnectorEx(service, types, x_net_info.get(), params);
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         TSERV_Type            types,
                                         const SConnNetInfo*   net_info,
                                         const SSERVICE_Extra* params,
                                         const STimeout*       timeout,
                                         size_t                buf_size)
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
                                         size_t                buf_size)
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


CConn_MemoryStream::CConn_MemoryStream(size_t buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(),
                     kInfiniteTimeout/*0*/, buf_size),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(BUF        buf,
                                       EOwnership owner,
                                       size_t     buf_size)
    : CConn_IOStream(MEMORY_CreateConnectorEx(buf, owner == eTakeOwnership
                                              ? 1/*true*/
                                              : 0/*false*/),
                     0, buf_size, fConn_ReadBuffered | fConn_WriteBuffered,
                     0, BUF_Size(buf)),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(const void* ptr,
                                       size_t      size,
                                       EOwnership  owner,
                                       size_t      buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(),
                     0, buf_size, fConn_ReadBuffered | fConn_WriteBuffered,
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
    delete[] (CT_CHAR_TYPE*) m_Ptr;
}


void CConn_MemoryStream::ToString(string* str)
{
    if (!str) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToString(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    size_t size = sb ? (size_t)(tellp() - tellg()) : 0;
    str->resize(size);
    if (sb) {
        size_t s = (size_t) sb->sgetn(&(*str)[0], size);
        _ASSERT(s == size);
#ifdef NCBI_COMPILER_WORKSHOP
        if (s < 0) {
            s = 0; // WS6 weirdness to sometimes return -1 from sgetn() :-/
        }
#endif //NCBI_COMPILER_WORKSHOP
        str->resize(s);  // NB: just in case, essentially NOOP when s == size
    }
}


void CConn_MemoryStream::ToVector(vector<char>* vec)
{
    if (!vec) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToVector(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    size_t size = sb ? (size_t)(tellp() - tellg()) : 0;
    vec->resize(size);
    if (sb) {
        size_t s = (size_t) sb->sgetn(&(*vec)[0], size);
        _ASSERT(s == size);
#ifdef NCBI_COMPILER_WORKSHOP
        if (s < 0) {
            s = 0;  // WS6 weirdness to sometimes return -1 from sgetn() :-/
        }
#endif //NCBI_COMPILER_WORKSHOP
        vec->resize(s);  // NB: just in case, essentially NOOP when s == size
    }
}


static CONNECTOR s_PipeConnectorBuilder(const string&         cmd,
                                        const vector<string>& args,
                                        CPipe::TCreateFlags   flags,
                                        CPipe*&               pipe)
{
    pipe = new CPipe;
    return PIPE_CreateConnector(cmd, args, flags, pipe, eNoOwnership);
}


CConn_PipeStream::CConn_PipeStream(const string&         cmd,
                                   const vector<string>& args,
                                   CPipe::TCreateFlags   flags,
                                   const STimeout*       timeout,
                                   size_t                buf_size)
    : CConn_IOStream(s_PipeConnectorBuilder(cmd, args, flags, m_Pipe),
                     timeout, buf_size)
{
    return;
}


CConn_PipeStream::~CConn_PipeStream()
{
    // Explicitly do x_Cleanup() to avoid using dead m_Pipe in base class dtor
    x_Cleanup();
    rdbuf(0);
    delete m_Pipe;
}


CConn_NamedPipeStream::CConn_NamedPipeStream(const string&   pipename,
                                             size_t          pipebufsize,
                                             const STimeout* timeout,
                                             size_t          buf_size)
    : CConn_IOStream(NAMEDPIPE_CreateConnector(pipename, pipebufsize),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_FtpConnectorBuilder(const SConnNetInfo*  net_info,
                                       TFTP_Flags           flag,
                                       const SFTP_Callback* cmcb,
                                       const STimeout*      timeout)
{
    _ASSERT(net_info);
    if (timeout == kDefaultTimeout)
        timeout  = net_info->timeout;
    const SConnNetInfo* x_net_info;
    if (timeout != net_info->timeout) {
        SConnNetInfo* xx_net_info = ConnNetInfo_Clone(net_info);
        if (xx_net_info)
            xx_net_info->timeout = timeout;
        x_net_info = xx_net_info;
    } else
        x_net_info = net_info;
    CONNECTOR c = FTP_CreateConnector(x_net_info, flag, cmcb);
    if (x_net_info != net_info)
        ConnNetInfo_Destroy((SConnNetInfo*) x_net_info);
    return c;
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
                                 const STimeout*      timeout,
                                 size_t               buf_size)
    : CConn_IOStream(FTP_CreateConnectorSimple(host.c_str(), port,
                                               user.c_str(), pass.c_str(),
                                               path.c_str(), flag, cmcb),
                     timeout, buf_size, fConn_Untie | fConn_ReadBuffered)
{
    return;
}


CConn_FtpStream::CConn_FtpStream(const SConnNetInfo&  net_info,
                                 TFTP_Flags           flag,
                                 const SFTP_Callback* cmcb,
                                 const STimeout*      timeout,
                                 size_t               buf_size)
    : CConn_IOStream(s_FtpConnectorBuilder(&net_info, flag, cmcb, timeout),
                     timeout, buf_size, fConn_Untie | fConn_ReadBuffered)
{
    return;
}


EIO_Status CConn_FtpStream::Drain(const STimeout* timeout)
{
    const STimeout* r_timeout = kInfiniteTimeout/*0*/;
    const STimeout* w_timeout = kInfiniteTimeout/*0*/;
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
                                                 const STimeout*      timeout,
                                                 size_t               buf_size)
    : CConn_FtpStream(host, user, pass, path, port, flag, cmcb,
                      timeout, buf_size)
{
    if (!file.empty())
        x_InitDownload(file, offset);
}


CConn_FTPDownloadStream::CConn_FTPDownloadStream(const SConnNetInfo&  net_info,
                                                 TFTP_Flags           flag,
                                                 const SFTP_Callback* cmcb,
                                                 Uint8                offset,
                                                 const STimeout*      timeout,
                                                 size_t               buf_size)
    : CConn_FtpStream(net_info, flag | fFTP_IgnorePath, cmcb,
                      timeout, buf_size)
{
    if (net_info.path[0])
        x_InitDownload(net_info.path, offset);
}


void CConn_FTPDownloadStream::x_InitDownload(const string& file, Uint8 offset)
{
    // Use '\n' here instead of NcbiFlush to avoid (and thus make silent)
    // flush errors on retrieval of inexistent (or bad) files / directories..
    EIO_Status status;
    if (offset) {
        write("REST ", 5) << NStr::UInt8ToString(offset) << '\n';
        status  = Status(eIO_Write);
    } else
        status  = eIO_Success;
    if (good()  &&  status == eIO_Success) {
        bool directory = NStr::EndsWith(file, '/');
        write(directory ? "NLST " : "RETR ", 5) << file << '\n';
        status  = Status(eIO_Write);
    }
    if (status != eIO_Success)
        setstate(NcbiBadbit);
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
    : CConn_FtpStream(host, user, pass, path, port, flag, 0/*cmcb*/,
                      timeout)
{
    if (!file.empty())
        x_InitUpload(file, offset);
}


CConn_FTPUploadStream::CConn_FTPUploadStream(const SConnNetInfo& net_info,
                                             TFTP_Flags          flag,
                                             Uint8               offset,
                                             const STimeout*     timeout)
    : CConn_FtpStream(net_info, flag | fFTP_IgnorePath, 0/*cmcb*/,
                      timeout)
{
    if (net_info.path[0])
        x_InitUpload(net_info.path, offset);
}


void CConn_FTPUploadStream::x_InitUpload(const string& file, Uint8 offset)
{
    EIO_Status status;
    if (offset) {
        write("REST ", 5) << NStr::UInt8ToString(offset) << NcbiFlush;
        status = Status(eIO_Write);
    } else
        status = eIO_Success;
    if (good()  &&  status == eIO_Success)
        write("STOR ", 5) << file << NcbiFlush;
}


/* non-public class */
class CConn_FileStream : public CConn_IOStream
{
public:
    CConn_FileStream(const string&   ifname,
                     const string&   ofname = kEmptyStr,
                     SFILE_ConnAttr* attr   = 0)
        : CConn_IOStream(FILE_CreateConnectorEx(ifname.c_str(),
                                                ofname.c_str(), attr),
                         0/*timeout*/, 0/*unbuffered*/, fConn_Untie)
    {
        return;
    }

private:
    // Disable copy constructor and assignment.
    CConn_FileStream(const CConn_FileStream&);
    CConn_FileStream& operator= (const CConn_FileStream&);
};


static bool x_IsIdentifier(const string& str)
{
    const char* s = str.c_str();
    if (!isalpha((unsigned char)(*s)))
        return false;
    for (++s;  *s;  ++s) {
        if (!isalnum((unsigned char)(*s))  &&  *s != '_')
            return false;
    }
    return true;
}


extern
NCBI_XCONNECT_EXPORT  // FIXME: To remove once the API is fully official
CConn_IOStream* NcbiOpenURL(const string& url, size_t buf_size)
{
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;
    }
    bool svc = x_IsIdentifier(url);

    AutoPtr<SConnNetInfo> net_info = ConnNetInfo_Create(svc ? url.c_str() : 0);

    if (svc)
        return new CConn_ServiceStream(url, fSERV_Any, net_info.get());

    unsigned int   host;
    unsigned short port;
    if (url.size() == CSocketAPI::StringToHostPort(url, &host, &port)  &&  port
        &&  net_info.get()) {
        net_info->req_method = eReqMethod_Connect;
    }

    if (ConnNetInfo_ParseURL(net_info.get(), url.c_str())) {
        _ASSERT(net_info);  // otherwise ConnNetInfo_ParseURL() would fail
        if (net_info->req_method == eReqMethod_Connect) {
            return new CConn_SocketStream(*net_info, 0, 0, fSOCK_LogDefault,
                                          net_info->timeout, buf_size);
        }
        switch (net_info->scheme) {
        case eURL_Https:
        case eURL_Http:
            return new CConn_HttpStream(net_info.get(), kEmptyStr, 0, 0, 0, 0,
                                        fHTTP_AutoReconnect,
                                        kDefaultTimeout, buf_size);
        case eURL_File:
            if (*net_info->host  ||  net_info->port)
                break; /*not supported*/
            _ASSERT(!*net_info->args);
            if (net_info->debug_printout) {
                net_info->req_method = eReqMethod_Any;
                net_info->firewall = 0;
                net_info->stateless = 0;
                net_info->lb_disable = 0;
                net_info->http_proxy_leak = 0;
                net_info->http_proxy_host[0] = '\0';
                net_info->http_proxy_port    =   0;
                net_info->http_proxy_user[0] = '\0';
                net_info->http_proxy_pass[0] = '\0';
                net_info->proxy_host[0]      = '\0';
                net_info->max_try = 0;
                net_info->timeout = kInfiniteTimeout;
                ConnNetInfo_SetUserHeader(net_info.get(), 0);
                if (net_info->http_referer) {
                    free((void*) net_info->http_referer);
                    net_info->http_referer = 0;
                }
                ConnNetInfo_Log(net_info.get(), eLOG_Note, CORE_GetLOG());
            }
            return new CConn_FileStream(net_info->path);
        case eURL_Ftp:
            return new CConn_FTPDownloadStream(*net_info, 0, 0, 0,
                                               net_info->timeout, buf_size);
        default:
            break;
        }
    }
    return 0;
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
    default:             break;
    }
    return CException::GetErrCodeString();
}


END_NCBI_SCOPE
