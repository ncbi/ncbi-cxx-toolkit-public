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
#include "ncbi_ansi_ext.h"
#include "ncbi_conn_streambuf.hpp"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include "ncbi_socketp.h"
#include <connect/error_codes.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_file_connector.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Stream


BEGIN_NCBI_SCOPE


const STimeout CConn_IOStream::kZeroTimeout = { 0, 0 };


typedef AutoPtr< char, CDeleter<char> >  TTempCharPtr;


CConn_IOStream::CConn_IOStream(const TConnector& connector,
                               const STimeout* timeout,
                               size_t buf_size, TConn_Flags flgs,
                               CT_CHAR_TYPE* ptr, size_t size)
    : CNcbiIostream(0/*the stream is "bad" initially (27.4.4.1.3)*/),
      m_CSb(0), x_CSb(new CConn_Streambuf(connector.first, connector.second,
                                          timeout, buf_size, flgs,
                                          ptr, size))
{
    if (x_CSb->Status(eIO_Close) == eIO_Success)
        init(m_CSb = x_CSb.get());
}


CConn_IOStream::CConn_IOStream(CONN conn, bool close,
                               const STimeout* timeout,
                               size_t buf_size, TConn_Flags flgs,
                               CT_CHAR_TYPE* ptr, size_t size)
    : CNcbiIostream(0/*the stream is "bad" initially (27.4.4.1.3)*/),
      m_CSb(0), x_CSb(new CConn_Streambuf(conn, close,
                                          timeout, buf_size, flgs,
                                          ptr, size))
{
    if (x_CSb->Status(eIO_Close) == eIO_Success)
        init(m_CSb = x_CSb.get());
}


CConn_IOStream::~CConn_IOStream()
{
    x_Destroy();
}


#define GET_CONN(sb)  ((sb) ? (sb)->GetCONN() : 0)


CONN CConn_IOStream::GetCONN(void) const
{
    return GET_CONN(m_CSb);
}


string CConn_IOStream::GetType(void) const
{
    CONN        conn = GET_CONN(x_CSb);
    const char* type = conn ? CONN_GetType(conn) : 0;
    return type ? type : kEmptyStr;
}


string CConn_IOStream::GetDescription(void) const
{
    CONN         conn = GET_CONN(x_CSb);
    TTempCharPtr text(conn ? CONN_Description(conn) : 0);
    return text ? text.get() : kEmptyStr;
}


EIO_Status CConn_IOStream::SetTimeout(EIO_Event       direction,
                                      const STimeout* timeout) const
{
    CONN conn = GET_CONN(m_CSb);
    return conn ? CONN_SetTimeout(conn, direction, timeout) : eIO_NotSupported;
}


const STimeout* CConn_IOStream::GetTimeout(EIO_Event direction) const
{
    CONN conn = GET_CONN(m_CSb);
    return conn ? CONN_GetTimeout(conn, direction) : kDefaultTimeout;
}


EIO_Status CConn_IOStream::Status(EIO_Event dir) const
{
    return x_CSb ? x_CSb->Status(dir) : eIO_NotSupported;
}


EIO_Status CConn_IOStream::Fetch(const STimeout* timeout)
{
    EIO_Status status;
    CONN conn = GET_CONN(m_CSb);
    if (!conn) {
        setstate(NcbiBadbit);
        status = eIO_NotSupported;
    } else
        status = m_CSb->Fetch(timeout);
    return status;
}


EIO_Status CConn_IOStream::x_Pushback(const CT_CHAR_TYPE* data,
                                      streamsize          size,
                                      bool                push)
{
    EIO_Status status
        = m_CSb ? m_CSb->Pushback(data, size, push) : eIO_NotSupported;
    if (status != eIO_Success)
        clear(NcbiBadbit);
    return status;
}


SOCK CConn_IOStream::GetSOCK(void)
{
    SOCK sock;
    CONN conn = GET_CONN(m_CSb);
    if (!conn)
        sock = 0;
    else if (CONN_GetSOCK(conn, &sock) != eIO_Success)
        _ASSERT(!sock);
    return sock;
}


EIO_Status CConn_IOStream::Wait(EIO_Event event, const STimeout* timeout)
{
    CONN conn = GET_CONN(m_CSb);
    return conn ? CONN_Wait(conn, event, timeout) : eIO_Closed;
}


EIO_Status CConn_IOStream::Close(void)
{
    if (!m_CSb)
        return eIO_Closed;
    m_Socket.Reset(0, eNoOwnership, eCopyTimeoutsFromSOCK);
    EIO_Status status = m_CSb->Close();
    if (status != eIO_Success  &&  status != eIO_Closed)
        clear(NcbiBadbit);
    _ASSERT(!GET_CONN(m_CSb));
    m_Canceled.Reset(0);
    return status;
}


void CConn_IOStream::x_Destroy(void)
{
    // This order!  Dtor must have the stream still constructed if it was
    x_CSb.reset();
    m_CSb = 0;
}


EIO_Status CConn_IOStream::SetCanceledCallback(const ICanceled* canceled)
{
    CONN conn = GET_CONN(m_CSb);
    if (!conn)
        return eIO_NotSupported;

    bool isset = m_Canceled.NotNull() ? 1 : 0;

    if (canceled) {
        SCONN_Callback cb;
        m_Canceled = canceled;
        memset(&cb, 0, sizeof(cb));
        cb.func = sx_IsCanceled;
        cb.data = this;
        CONN_SetCallback(conn, eCONN_OnOpen,  &cb,      isset ? 0 : &m_CB[0]);
        CONN_SetCallback(conn, eCONN_OnRead,  &cb,      isset ? 0 : &m_CB[1]);
        CONN_SetCallback(conn, eCONN_OnWrite, &cb,      isset ? 0 : &m_CB[2]);
        CONN_SetCallback(conn, eCONN_OnFlush, &cb,      isset ? 0 : &m_CB[3]);
    } else if (isset) {
        CONN_SetCallback(conn, eCONN_OnFlush, &m_CB[3],         0);
        CONN_SetCallback(conn, eCONN_OnWrite, &m_CB[2],         0);
        CONN_SetCallback(conn, eCONN_OnRead,  &m_CB[1],         0);
        CONN_SetCallback(conn, eCONN_OnOpen,  &m_CB[0],         0);
        m_Canceled = 0;
    }

    return eIO_Success;
}


EIO_Status CConn_IOStream::sx_IsCanceled(CONN           conn,
                                         TCONN_Callback type,
                                         void*          data)
{
    _ASSERT(conn  &&  data);
    CConn_IOStream* io = reinterpret_cast<CConn_IOStream*>(data);
    if (/* io && */ io->m_Canceled.NotNull()  &&  io->m_Canceled->IsCanceled())
        return eIO_Interrupt;
    int n = (int) type & (int) eIO_ReadWrite;
    _ASSERT(0 <= n  &&  (size_t) n < sizeof(io->m_CB) / sizeof(io->m_CB[0]));
    _ASSERT((n == 0  &&  type == eCONN_OnOpen)   ||
            (n == 1  &&  type == eCONN_OnRead)   ||
            (n == 2  &&  type == eCONN_OnWrite)  ||
            (n == 3  &&  type == eCONN_OnFlush));
    return io->m_CB[n].func
        ? io->m_CB[n].func(conn, type, io->m_CB[n].data)
        : eIO_Success;

}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       unsigned short  max_try,
                                       const STimeout* timeout,
                                       size_t          buf_size,
                                       TConn_Flags     flags)
    : CConn_IOStream(TConnector(SOCK_CreateConnector(host.c_str(),
                                                     port,
                                                     max_try)),
                     timeout, buf_size, flags)
{
    return;
}


static CConn_IOStream::TConnector
s_SocketConnectorBuilder(const string&  ahost,
                         unsigned short aport,
                         unsigned short max_try,
                         const void*    data,
                         size_t         size,
                         TSOCK_Flags    flgs)
{
    size_t         len = ahost.size();
    string         x_host, xx_port;
    unsigned int   x_port;
    const string*  host;
    unsigned short port;
    CONNECTOR      c;
    if ((port =  aport) != 0  &&  len) {
        host  = &ahost;
    } else if (!len  ||  NCBI_HasSpaces(ahost.c_str(), len)
               ||  !NStr::SplitInTwo(ahost, ":", x_host, xx_port)
               ||  x_host.empty()  ||  xx_port.empty()
               ||  !isdigit((unsigned char) xx_port[0])  ||  xx_port[0] == '0'
               ||  !(x_port = NStr::StringToUInt(xx_port,
                                                 NStr::fConvErr_NoThrow))
               ||  (x_port ^ (x_port & 0xFFFF))) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_SocketStream::CConn_SocketStream(\""
                   + ahost + "\", " + NStr::NumericToString(aport) + "): "
                   " Invalid/insufficient destination address");
    } else {
        port =  x_port;
        host = &x_host;
    }
    c = SOCK_CreateConnectorEx(host->c_str(), port, max_try,
                               data, size, flgs);
    return CConn_IOStream::TConnector(c);
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       const void*     data,
                                       size_t          size,
                                       TSOCK_Flags     flgs,
                                       unsigned short  max_try,
                                       const STimeout* timeout,
                                       size_t          buf_size,
                                       TConn_Flags     flags)
    : CConn_IOStream(s_SocketConnectorBuilder(host, port, max_try,
                                              data, size, flgs),
                     timeout, buf_size, flags)
{
    return;
}


CConn_SocketStream::CConn_SocketStream(SOCK            sock,
                                       EOwnership      if_to_own,
                                       const STimeout* timeout,
                                       size_t          buf_size,
                                       TConn_Flags     flags)
    : CConn_IOStream(TConnector(SOCK_CreateConnectorOnTop(sock,
                                                          if_to_own
                                                          != eNoOwnership)),
                     timeout, buf_size, flags)
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
                                       size_t          buf_size,
                                       TConn_Flags     flags)
    : CConn_IOStream(TConnector(SOCK_CreateConnectorOnTop(s_GrabSOCK(socket),
                                                          1/*own*/)),
                     timeout, buf_size, flags)
{
    return;
}


template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


static CConn_IOStream::TConnector
s_SocketConnectorBuilder(const SConnNetInfo* net_info,
                         const STimeout*     timeout,
                         const void*         data,
                         size_t              size,
                         TSOCK_Flags         flgs)
{
    EIO_Status status = eIO_Success;
    bool       proxy = false;
    SOCK       sock = 0;

    _ASSERT(net_info);
    if ((flgs & (fSOCK_LogOn | fSOCK_LogDefault)) == fSOCK_LogDefault
        &&  net_info->debug_printout == eDebugPrintout_Data) {
        flgs &= ~fSOCK_LogDefault;
        flgs |=  fSOCK_LogOn;
    }
    if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port
        &&  net_info->http_proxy_mask != fProxy_Http) {
        status = HTTP_CreateTunnel(net_info, fHTTP_NoAutoRetry, &sock);
        _ASSERT(!sock ^ !(status != eIO_Success));
        if (status == eIO_Success
            &&  (size  ||  (flgs & ~(fSOCK_LogOn | fSOCK_LogDefault)))) {
            SSOCK_Init init;
            memset(&init, 0, sizeof(init));
            init.data = data;
            init.size = size;
            init.host = net_info->host;
            init.cred = net_info->credentials;
            SOCK s;
            status = SOCK_CreateOnTopInternal(sock, 0, &s, &init, flgs);
            _ASSERT(!s ^ !(status != eIO_Success));
            SOCK_Destroy(sock);
            sock = s;
        }
        proxy = true;
    }
    if (!sock  &&  (!proxy  ||  net_info->http_proxy_leak)) {
        if (timeout == kDefaultTimeout)
            timeout  = net_info->timeout;
        if (!proxy  &&  net_info->debug_printout) {
            AutoPtr<SConnNetInfo> x_net_info(ConnNetInfo_Clone(net_info));
            if (x_net_info) {
                // NB: This is only for logging! (so no throw on NULL)
                // Manual cleanup of most fields req'd :-/
                x_net_info->scheme = eURL_Unspec;
                x_net_info->req_method = eReqMethod_Any;
                x_net_info->external = 0;
                x_net_info->firewall = 0;
                x_net_info->stateless = 0;
                x_net_info->lb_disable = 0;
                x_net_info->http_version = 0;
                x_net_info->http_push_auth = 0;
                x_net_info->http_proxy_leak = 0;
                x_net_info->http_proxy_skip = 0;
                x_net_info->http_proxy_mask = 0;
                x_net_info->user[0] = '\0';
                x_net_info->pass[0] = '\0';
                x_net_info->path[0] = '\0';
                x_net_info->http_proxy_host[0] = '\0';
                x_net_info->http_proxy_port    =   0;
                x_net_info->http_proxy_user[0] = '\0';
                x_net_info->http_proxy_pass[0] = '\0';
                ConnNetInfo_SetUserHeader(x_net_info.get(), 0);
                if (x_net_info->http_referer) {
                    free((void*) x_net_info->http_referer);
                    x_net_info->http_referer = 0;
                }
                x_net_info->timeout = timeout;
            }
            CORE_LOCK_READ;
            ConnNetInfo_Log(x_net_info.get(), eLOG_Note, CORE_GetLOG());
            CORE_UNLOCK;
        }
        SSOCK_Init init;
        memset(&init, 0, sizeof(init));
        init.data = data;
        init.size = size;
        init.host = net_info->host;
        init.cred = net_info->credentials;
        status = SOCK_CreateInternal(net_info->host, net_info->port, timeout,
                                     &sock, &init, flgs);
        _ASSERT(!sock ^ !(status != eIO_Success));
    }
    string hostport(net_info->host);
    hostport += ':';
    hostport += NStr::UIntToString(net_info->port);
    CONNECTOR c = SOCK_CreateConnectorOnTopEx(sock, 1/*own*/,hostport.c_str());
    if (!c) {
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    return CConn_IOStream::TConnector(c, status);
}


CConn_SocketStream::CConn_SocketStream(const SConnNetInfo& net_info,
                                       const void*         data,
                                       size_t              size,
                                       TSOCK_Flags         flgs,
                                       const STimeout*     timeout,
                                       size_t              buf_size,
                                       TConn_Flags         flags)
    : CConn_IOStream(s_SocketConnectorBuilder(&net_info, timeout,
                                              data, size, flgs),
                     timeout, buf_size, flags)
{
    return;
}


//
// WARNING: All sx_ callbacks that operate on data members of non-primitive
//          types MUST NOT be indirectly called before the respective stream's
//          constructor body, and may not be used after that stream's dtor!!!
//
// Notably, sx_ParseHeader operates on SHTTP_StatusData, or SERVICE connector's
// sx_Adjust (with failure_count == -1) gets called from that connector's Open,
// which is done by default from the CConn_Streambuf's ctor unless
// fConn_DelayOpen is set in the stream's flags.
//


EHTTP_HeaderParse SHTTP_StatusData::Parse(const char* header)
{
    Clear();
    const char* eol = strstr(header, HTTP_EOL);
    if (!eol)
        return eHTTP_HeaderError;
    int c, n;
    if (sscanf(header, "HTTP/%*[0-9.] %u%n", &c, &n) < 1  ||  eol < header + n)
        return eHTTP_HeaderError;
    m_Header = header;
    const char* str = m_Header.c_str();
    eol = str + (size_t)(eol - header);
    str += n;
    str += strspn(str, " \t");
    while (eol > str) {
        if (!isspace((unsigned char) eol[-1]))
            break;
        --eol;
    }
    m_Code = c;
    m_Text.assign(str, (size_t)(eol - str));
    return eHTTP_HeaderSuccess;
}


static CConn_IOStream::TConnector
s_HttpConnectorBuilder(const SConnNetInfo* net_info,
                       EReqMethod          method,
                       const char*         url,
                       const char*         host,
                       unsigned short      port,
                       const char*         path,
                       const char*         args,
                       const char*         user_header,
                       void*               x_data,
                       FHTTP_Adjust        x_adjust,
                       FHTTP_Cleanup       x_cleanup,
                       FHTTP_ParseHeader   x_parse_header,
                       THTTP_Flags         flgs,
                       const STimeout*     timeout,
                       void**              user_data_ptr,
                       FHTTP_Cleanup*      user_cleanup_ptr,
                       void*               user_data    = 0,
                       FHTTP_Cleanup       user_cleanup = 0)
{
    EReqMethod x_req_method;
    AutoPtr<SConnNetInfo> x_net_info(net_info
                                     ? ConnNetInfo_Clone(net_info)
                                     : ConnNetInfo_CreateInternal(0));
    if (!x_net_info) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   string("CConn_HttpStream::CConn_HttpStream(): ")
                   + (net_info ? " Out of memory"
                      : " Unable to build connection parameters"));
    }
    x_req_method = (EReqMethod)(method & ~eReqMethod_v1);
    if (x_req_method == eReqMethod_Connect) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream(): "
                   " Bad request method (CONNECT)");
    }
    if (x_req_method)
        x_net_info->req_method = method;
    else if (method/*ANY/1.1*/)
        x_net_info->http_version = 1;
    if (url  &&  !ConnNetInfo_ParseURL(x_net_info.get(), url)) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream(): "
                   " Bad URL \"" + string(url) + '"');
    }
    if (host) {
        size_t len;
        if ((len = *host ? strlen(host) : 0) >= sizeof(x_net_info->host)) {
            NCBI_THROW(CIO_Exception, eInvalidArg,
                       "CConn_HttpStream::CConn_HttpStream(): "
                       " Host too long \"" + string(host) + '"');
        }
        memcpy(x_net_info->host, host, ++len);
    }
    if (port)
        x_net_info->port = port;
    if (path  &&  !ConnNetInfo_SetPath(x_net_info.get(), path)) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream(): "
                   " Path too long \"" + string(path) + '"');
    }
    if (args  &&  !ConnNetInfo_SetArgs(x_net_info.get(), args)) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_HttpStream::CConn_HttpStream(): "
                   " Args too long \"" + string(args) + '"');
    }
    if (user_header  &&  *user_header
        &&  !ConnNetInfo_OverrideUserHeader(x_net_info.get(), user_header)) {
        int x_dynamic = 0;
        const char* x_message = NcbiMessagePlusError(&x_dynamic,
                                                     "Cannot set user header",
                                                     errno, 0);
        TTempCharPtr msg_ptr(const_cast<char*> (x_message),
                             x_dynamic ? eTakeOwnership : eNoOwnership);
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_HttpStream::CConn_HttpStream(): "
                   " " + string(msg_ptr.get()));
    }
    if (timeout != kDefaultTimeout)
        x_net_info->timeout = timeout;
    // NB: Must init these two here just in case of early CONNECTOR->destroy()
    *user_data_ptr    = user_data;
    *user_cleanup_ptr = user_cleanup;
    CONNECTOR c = HTTP_CreateConnectorEx(x_net_info.get(),
                                         flgs,
                                         x_parse_header,
                                         x_data,
                                         x_adjust,
                                         x_cleanup);
    /* NOTE: if "c" is NULL here (could not create connector -- out of memory?)
     * then "user_data" (if any) is going to potentially leak because for the caller,
     * the stream contruction appears "successful" (no exception thrown), meaning if
     * the caller arranged a unique_ptr to guard "user_data", it's now going to be
     * "released", but x_cleanup will never called to deallocate because that's done
     * from CONNECTOR -- which does not exist! */
    return CConn_IOStream::TConnector(c);
}


CConn_HttpStream::CConn_HttpStream(const string&   host,
                                   const string&   path,
                                   const string&   args,
                                   const string&   user_header,
                                   unsigned short  port,
                                   THTTP_Flags     flgs,
                                   const STimeout* timeout,
                                   size_t          buf_size)
    : CConn_HttpStream_Base(s_HttpConnectorBuilder(0,
                                                   eReqMethod_Any,
                                                   0,
                                                   host.c_str(),
                                                   port,
                                                   path.c_str(),
                                                   args.c_str(),
                                                   user_header.c_str(),
                                                   this,
                                                   sx_Adjust,
                                                   0/*x_cleanup*/,
                                                   sx_ParseHeader,
                                                   flgs,
                                                   timeout,
                                                   &m_UserData,
                                                   &m_UserCleanup),
                            timeout, buf_size),
      m_UserAdjust(0), m_UserParseHeader(0)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&   url,
                                   THTTP_Flags     flgs,
                                   const STimeout* timeout,
                                   size_t          buf_size)
    : CConn_HttpStream_Base(s_HttpConnectorBuilder(0,
                                                   eReqMethod_Any,
                                                   url.c_str(),
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   this,
                                                   sx_Adjust,
                                                   0/*x_cleanup*/,
                                                   sx_ParseHeader,
                                                   flgs,
                                                   timeout,
                                                   &m_UserData,
                                                   &m_UserCleanup),
                            timeout, buf_size),
      m_UserAdjust(0), m_UserParseHeader(0)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const string&   url,
                                   EReqMethod      method,
                                   const string&   user_header,
                                   THTTP_Flags     flgs,
                                   const STimeout* timeout,
                                   size_t          buf_size)
    : CConn_HttpStream_Base(s_HttpConnectorBuilder(0,
                                                   method,
                                                   url.c_str(),
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   user_header.c_str(),
                                                   this,
                                                   sx_Adjust,
                                                   0/*x_cleanup*/,
                                                   sx_ParseHeader,
                                                   flgs,
                                                   timeout,
                                                   &m_UserData,
                                                   &m_UserCleanup),
                            timeout, buf_size),
      m_UserAdjust(0), m_UserParseHeader(0)
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
                                   THTTP_Flags         flgs,
                                   const STimeout*     timeout,
                                   size_t              buf_size)
    : CConn_HttpStream_Base(s_HttpConnectorBuilder(net_info,
                                                   eReqMethod_Any,
                                                   url.c_str(),
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   user_header.c_str(),
                                                   this,
                                                             sx_Adjust,
                                                   cleanup ? sx_Cleanup : 0,
                                                   sx_ParseHeader,
                                                   flgs,
                                                   timeout,
                                                   &m_UserData,
                                                   &m_UserCleanup,
                                                   user_data,
                                                   cleanup),
                            timeout, buf_size),
      m_UserAdjust(adjust), m_UserParseHeader(parse_header)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   FHTTP_ParseHeader   parse_header,
                                   void*               user_data,
                                   FHTTP_Adjust        adjust,
                                   FHTTP_Cleanup       cleanup,
                                   THTTP_Flags         flgs,
                                   const STimeout*     timeout,
                                   size_t              buf_size)
    : CConn_HttpStream_Base(s_HttpConnectorBuilder(net_info,
                                                   eReqMethod_Any,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   user_header.c_str(),
                                                   this,
                                                             sx_Adjust,
                                                   cleanup ? sx_Cleanup : 0,
                                                   sx_ParseHeader,
                                                   flgs,
                                                   timeout,
                                                   &m_UserData,
                                                   &m_UserCleanup,
                                                   user_data,
                                                   cleanup),
                            timeout, buf_size),
      m_UserAdjust(adjust), m_UserParseHeader(parse_header)
{
    return;
}


CConn_HttpStream::~CConn_HttpStream()
{
    // Explicitly destroy so that the callbacks are not used out of context.
    x_Destroy();
}


EHTTP_HeaderParse CConn_HttpStream::sx_ParseHeader(const char* header,
                                                   void*       data,
                                                   int         code)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    _ASSERT(http->m_CSb); // make sure the stream is fully constructed
    try {
        EHTTP_HeaderParse rv = http->m_StatusData.Parse(header);
        if (rv != eHTTP_HeaderSuccess)
            return rv;
        _ASSERT(!code  ||  code == http->m_StatusData.m_Code);
        return http->m_UserParseHeader
            ? http->m_UserParseHeader(header, http->m_UserData, code)
            : eHTTP_HeaderSuccess;
    }
    NCBI_CATCH_ALL("CConn_HttpStream::sx_ParseHeader()");
    if (http->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_HttpStream::sx_ParseHeader()");
    return eHTTP_HeaderError;
}


int CConn_HttpStream::sx_Adjust(SConnNetInfo* net_info,
                                void*         data,
                                unsigned int  failure_count)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    _ASSERT(http->m_CSb); // make sure the stream is fully constructed
    try {
        int  retval;
        bool modified;
        if (failure_count == (unsigned int)(-1)  &&  !http->m_URL.empty()) {
            http->m_StatusData.Clear();
            if (!ConnNetInfo_ParseURL(net_info, http->m_URL.c_str()))
                return 0/*failure*/;
            http->m_URL.clear();
            modified = true;
        } else
            modified = false;
        if (http->m_UserAdjust) {
            retval = http->m_UserAdjust(net_info,
                                        http->m_UserData, failure_count);
            if (retval < 0  &&  modified)
                retval = 1;
        } else
            retval = modified ? 1 : -1;
        return retval;
    }
    NCBI_CATCH_ALL("CConn_HttpStream::sx_Adjust()");
    if (http->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_HttpStream::sx_Adjust()");
    return 0/*error*/;
}


void CConn_HttpStream::sx_Cleanup(void* data)
{
    CConn_HttpStream* http = reinterpret_cast<CConn_HttpStream*>(data);
    try {
        http->m_UserCleanup(http->m_UserData);
    }
    NCBI_CATCH_ALL("CConn_HttpStream::sx_Cleanup()");
}


static CConn_IOStream::TConnector
s_ServiceConnectorBuilder(const char*           service,
                          TSERV_Type            types,
                          const SConnNetInfo*   net_info,
                          const char*           user_header,
                          const SSERVICE_Extra* extra,
                          void*                 x_data,
                          SSERVICE_Extra*       x_extra,
                          FSERVICE_Reset        x_reset,
                          FHTTP_Adjust          x_adjust,
                          FSERVICE_Cleanup      x_cleanup,
                          FHTTP_ParseHeader     x_parse_header,
                          FSERVICE_GetNextInfo  x_get_next_info,
                          const STimeout*       timeout)
{
    AutoPtr<SConnNetInfo> x_net_info(net_info
                                     ? ConnNetInfo_Clone(net_info)
                                     : ConnNetInfo_Create(service));
    if (!x_net_info) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   string("CConn_ServiceStream::CConn_ServiceStream(): ")
                   + (net_info ? " Out of memory"
                      : " Unable to build connection parameters"));
    }
    if (user_header  &&  *user_header
        &&  !ConnNetInfo_OverrideUserHeader(x_net_info.get(), user_header)) {
        int x_dynamic = 0;
        const char* x_message = NcbiMessagePlusError(&x_dynamic,
                                                     "Cannot set user header",
                                                     errno, 0);
        TTempCharPtr msg_ptr(const_cast<char*> (x_message),
                             x_dynamic ? eTakeOwnership : eNoOwnership);
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_ServiceStream::CConn_ServiceStream(): "
                   " " + string(msg_ptr.get()));
    }
    if (timeout != kDefaultTimeout)
        x_net_info->timeout = timeout;

    if (extra)
        memcpy(x_extra, extra, sizeof(*x_extra));
    else
        memset(x_extra, 0,     sizeof(*x_extra));
    _ASSERT(x_parse_header);
    _ASSERT(!x_reset          ||  (extra  &&  extra->reset));
    _ASSERT(!x_adjust         ||  (extra  &&  extra->adjust));
    _ASSERT(!x_cleanup        ||  (extra  &&  extra->cleanup));
    _ASSERT(!x_get_next_info  ||  (extra  &&  extra->get_next_info));
    SSERVICE_Extra xx_extra;
    memset(&xx_extra, 0, sizeof(xx_extra));
    xx_extra.data          = x_data;
    xx_extra.reset         = x_reset;
    xx_extra.adjust        = x_adjust;
    xx_extra.cleanup       = x_cleanup;
    xx_extra.parse_header  = x_parse_header;
    xx_extra.get_next_info = x_get_next_info;
    xx_extra.flags         = extra ? extra->flags : 0;
    CONNECTOR c = SERVICE_CreateConnectorEx(service,
                                            types,
                                            x_net_info.get(),
                                            &xx_extra);
    /* NB: See NOTE under the HTTP_CreateConnectorEx() call above! */
    return CConn_IOStream::TConnector(c);
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         TSERV_Type            types,
                                         const SConnNetInfo*   net_info,
                                         const SSERVICE_Extra* extra,
                                         const STimeout*       timeout,
                                         size_t                buf_size)
    : CConn_HttpStream_Base(s_ServiceConnectorBuilder(service.c_str(),
                                                      types,
                                                      net_info,
                                                      0, // user_header
                                                      extra,
                                                      this,
                                                      &m_Extra,
                                                      extra  &&  extra->reset
                                                      ? sx_Reset       : 0,
                                                      extra  &&  extra->adjust
                                                      ? sx_Adjust      : 0,
                                                      extra  &&  extra->cleanup
                                                      ? sx_Cleanup     : 0,
                                                      sx_ParseHeader,
                                                      extra  &&  extra->get_next_info
                                                      ? sx_GetNextInfo : 0,
                                                      timeout),
                            timeout, buf_size,
                            types & fSERV_DelayOpen ? fConn_DelayOpen : 0)
{
    return;
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         const string&         user_header,
                                         TSERV_Type            types,
                                         const SSERVICE_Extra* extra,
                                         const STimeout*       timeout,
                                         size_t                buf_size)
    : CConn_HttpStream_Base(s_ServiceConnectorBuilder(service.c_str(),
                                                      types,
                                                      0, // net_info
                                                      user_header.c_str(),
                                                      extra,
                                                      this,
                                                      &m_Extra,
                                                      extra  &&  extra->reset
                                                      ? sx_Reset       : 0,
                                                      extra  &&  extra->adjust
                                                      ? sx_Adjust      : 0,
                                                      extra  &&  extra->cleanup
                                                      ? sx_Cleanup     : 0,
                                                      sx_ParseHeader,
                                                      extra  &&  extra->get_next_info
                                                      ? sx_GetNextInfo : 0,
                                                      timeout),
                            timeout, buf_size,
                            types & fSERV_DelayOpen ? fConn_DelayOpen : 0)
{
    return;
}


CConn_ServiceStream::~CConn_ServiceStream()
{
    // Explicitly destroy so that the callbacks are not used out of context.
    x_Destroy();
}


EHTTP_HeaderParse CConn_ServiceStream::sx_ParseHeader(const char* header,
                                                      void*       data,
                                                      int         code)
{
    CConn_ServiceStream* svc = reinterpret_cast<CConn_ServiceStream*>(data);
    _ASSERT(svc->m_CSb); // make sure the stream is fully constructed
    try {
        EHTTP_HeaderParse rv = svc->m_StatusData.Parse(header);
        if (rv != eHTTP_HeaderSuccess)
            return rv;
        _ASSERT(!code  ||  code == svc->m_StatusData.m_Code);
        return svc->m_Extra.parse_header
            ? svc->m_Extra.parse_header(header, svc->m_Extra.data, code)
            : eHTTP_HeaderSuccess;
    }
    NCBI_CATCH_ALL("CConn_ServiceStream::sx_ParseHeader()");
    if (svc->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_ServiceStream::sx_ParseHeader()");
    return eHTTP_HeaderError;
}


int/*bool*/ CConn_ServiceStream::sx_Adjust(SConnNetInfo* net_info,
                                           void*         data,
                                           unsigned int  failure_count)
{
    CConn_ServiceStream* svc = reinterpret_cast<CConn_ServiceStream*>(data);
    try {
        return svc->m_Extra.adjust(net_info, svc->m_Extra.data, failure_count);
    }
    NCBI_CATCH_ALL("CConn_ServiceStream::sx_Adjust()");
    if (svc->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_ServiceStream::sx_Adjust()");
    return 0/*error*/;
}


const SSERV_Info* CConn_ServiceStream::sx_GetNextInfo(void*     data,
                                                      SERV_ITER iter)
{
    CConn_ServiceStream* svc = reinterpret_cast<CConn_ServiceStream*>(data);
    try {
        return svc->m_Extra.get_next_info(svc->m_Extra.data, iter);
    }
    NCBI_CATCH_ALL("CConn_ServiceStream::sx_GetNextInfo()");
    if (svc->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_ServiceStream::sx_GetNextInfo()");
    return 0;
}


void CConn_ServiceStream::sx_Reset(void* data)
{
    CConn_ServiceStream* svc = reinterpret_cast<CConn_ServiceStream*>(data);
    try {
        return svc->m_Extra.reset(svc->m_Extra.data);
    }
    NCBI_CATCH_ALL("CConn_ServiceStream::sx_Reset()");
    if (svc->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_ServiceStream::sx_Reset()");
}


void CConn_ServiceStream::sx_Cleanup(void* data)
{
    CConn_ServiceStream* svc = reinterpret_cast<CConn_ServiceStream*>(data);
    try {
        svc->m_Extra.cleanup(svc->m_Extra.data);
    }
    NCBI_CATCH_ALL("CConn_ServiceStream::sx_Cleanup()");
}


CConn_MemoryStream::CConn_MemoryStream(size_t buf_size)
    : CConn_IOStream(TConnector(MEMORY_CreateConnector()),
                     kInfiniteTimeout/*0*/, buf_size),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(BUF        buf,
                                       EOwnership owner,
                                       size_t     buf_size)
    : CConn_IOStream(TConnector(MEMORY_CreateConnectorEx(buf,
                                                         owner
                                                         == eTakeOwnership
                                                         ? 1/*true*/
                                                         : 0/*false*/)),
                     kInfiniteTimeout/*0*/, buf_size, 0/*flags*/,
                     0, BUF_Size(buf)),
      m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(const void* ptr,
                                       size_t      size,
                                       EOwnership  owner,
                                       size_t      buf_size)
    : CConn_IOStream(TConnector(MEMORY_CreateConnector()),
                     kInfiniteTimeout/*0*/, buf_size, 0/*flags*/,
                     (CT_CHAR_TYPE*) ptr, size),
      m_Ptr(owner == eTakeOwnership ? ptr : 0)
{
    return;
}


CConn_MemoryStream::~CConn_MemoryStream()
{
    // Explicitly call x_Destroy() to avoid using deleted m_Ptr otherwise.
    x_Destroy();
    delete[] (CT_CHAR_TYPE*) m_Ptr;
}


void CConn_MemoryStream::ToString(string* str)
{
    if (!str) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToString(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    size_t size = sb  &&  good() ? (size_t)(tellp() - tellg()) : 0;
    str->resize(size);
    if (sb) {
        // Proceed with read even with size == 0
        size_t s = (size_t) sb->sgetn(&(*str)[0], size);
#ifdef NCBI_COMPILER_WORKSHOP
        if (s < 0) {
            s = 0; // WS6 weirdness to sometimes return -1 from sgetn() :-/
        } else
#endif //NCBI_COMPILER_WORKSHOP
        _ASSERT(s == size);
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
    size_t size = sb  &&  good() ? (size_t)(tellp() - tellg()) : 0;
    vec->resize(size);
    if (sb) {
        // Proceed with read even with size == 0
        size_t s = (size_t) sb->sgetn(&(*vec)[0], size);
#ifdef NCBI_COMPILER_WORKSHOP
        if (s < 0) {
            s = 0;  // WS6 weirdness to sometimes return -1 from sgetn() :-/
        } else
#endif //NCBI_COMPILER_WORKSHOP
        _ASSERT(s == size);
        vec->resize(s);  // NB: just in case, essentially NOOP when s == size
    }
}


// HACK * HACK * HACK
BUF CConn_MemoryStream::GetBUF(void)
{
    SMetaConnector* meta = !flush() ? 0 : (SMetaConnector*) GetCONN();
    return meta ? *((BUF*) meta->list->handle) : 0;
}


static CConn_IOStream::TConnector
s_PipeConnectorBuilder(const string&         cmd,
                       const vector<string>& args,
                       CPipe::TCreateFlags   flgs,
                       size_t                pipe_size,
                       CPipe*&               pipe)
{
    unique_ptr<CPipe> p(new CPipe(pipe_size));
    CONNECTOR c = PIPE_CreateConnector(cmd, args, flgs, p.get(), eNoOwnership);
    pipe = c ? p.release() : 0;
    return CConn_IOStream::TConnector(c);
}


CConn_PipeStream::CConn_PipeStream(const string&         cmd,
                                   const vector<string>& args,
                                   CPipe::TCreateFlags   flgs,
                                   size_t                pipe_size,
                                   const STimeout*       timeout,
                                   size_t                buf_size)
    : CConn_IOStream(s_PipeConnectorBuilder(cmd, args, flgs, pipe_size,
                                            m_Pipe),
                     timeout, buf_size),
      m_ExitCode(-1)
{
    return;
}


CConn_PipeStream::~CConn_PipeStream()
{
    // Explicitly x_Destroy() to avoid using dead m_Pipe in the base class dtor
    x_Destroy();
    delete m_Pipe;
}


EIO_Status CConn_PipeStream::Close(void)
{
    if (!flush())
        return Status(eIO_Write);
    // NB:  This can lead to wrong order for a close callback, if any fired by
    // CConn_IOStream::Close():  the callback will be late and actually coming
    // _after_ the pipe gets closed here.  There's no easy way to avoid this...
    EIO_Status status = m_Pipe->Close(&m_ExitCode);
    (void) CConn_IOStream::Close();
    return status;
}


CConn_NamedPipeStream::CConn_NamedPipeStream(const string&   pipename,
                                             size_t          pipesize,
                                             const STimeout* timeout,
                                             size_t          buf_size)
    : CConn_IOStream(TConnector(NAMEDPIPE_CreateConnector(pipename, pipesize)),
                     timeout, buf_size)
{
    return;
}


static CConn_IOStream::TConnector
s_FtpConnectorBuilder(const char*          host,
                      unsigned short       port,
                      const char*          user,
                      const char*          pass,
                      const char*          path,
                      const SConnNetInfo*  net_info,
                      TFTP_Flags           flgs,
                      const SFTP_Callback* cmcb,
                      void*                x_data,
                      SFTP_Callback*       x_cmcb,
                      FFTP_Callback        x_func,
                      const STimeout*      timeout)
{
    SFTP_Callback xx_cmcb;
    if (cmcb) {
        memcpy(  x_cmcb, cmcb, sizeof(*x_cmcb));
        memset(&xx_cmcb, 0,    sizeof(xx_cmcb));
        xx_cmcb.data = x_data;
        xx_cmcb.func = x_func;
        x_cmcb = &xx_cmcb;
    } else {
        memset(  x_cmcb, 0,    sizeof(*x_cmcb));
        x_cmcb = 0;
    }

    CONNECTOR c;
    if (net_info) {
        if (timeout == kDefaultTimeout)
            timeout  = net_info->timeout;
        const SConnNetInfo* x_net_info;
        if (timeout != net_info->timeout) {
            SConnNetInfo* xx_net_info = ConnNetInfo_Clone(net_info);
            if (!xx_net_info) {
                NCBI_THROW(CIO_Exception, eUnknown,
                           "CConn_FtpStream::CConn_FtpStream(): "
                           " Out of memory");
            }
            xx_net_info->timeout = timeout;
            x_net_info  = xx_net_info;
        } else
            x_net_info  =    net_info;
        c = FTP_CreateConnector(x_net_info, flgs, x_cmcb);
        if (x_net_info !=    net_info)
            ConnNetInfo_Destroy((SConnNetInfo*) x_net_info);
    } else {
        c = FTP_CreateConnectorSimple(host,
                                      port,
                                      user,
                                      pass,
                                      path,
                                      flgs,
                                      x_cmcb);
    }
    return CConn_IOStream::TConnector(c);
}
 

/* For data integrity and unambigous interpretation, FTP streams are not
 * buffered at the level of the C++ STL streambuf because of the way they
 * execute read / write operations on the mix of FTP commands and data.
 * There should be a little impact on performance of byte-by-byte I/O (such as
 * formatted input, which is not expected very often for this kind of streams,
 * anyways), and almost none for block I/O (such as read / readsome / write).
 */
CConn_FtpStream::CConn_FtpStream(const string&        host,
                                 const string&        user,
                                 const string&        pass,
                                 const string&        path,
                                 unsigned short       port,
                                 TFTP_Flags           flgs,
                                 const SFTP_Callback* cmcb,
                                 const STimeout*      timeout,
                                 size_t               buf_size)
    : CConn_IOStream(s_FtpConnectorBuilder(host.c_str(),
                                           port,
                                           user.c_str(),
                                           pass.c_str(),
                                           path.c_str(),
                                           0/*net_info*/,
                                           flgs,
                                           cmcb,
                                           this,
                                           &m_Cmcb,
                                           sx_FtpCallback,
                                           timeout),
                     timeout, buf_size,
                     fConn_Untie | fConn_WriteUnbuffered)
{
    return;
}


CConn_FtpStream::CConn_FtpStream(const SConnNetInfo&  net_info,
                                 TFTP_Flags           flgs,
                                 const SFTP_Callback* cmcb,
                                 const STimeout*      timeout,
                                 size_t               buf_size)
    : CConn_IOStream(s_FtpConnectorBuilder(0/*host*/,
                                           0/*port*/,
                                           0/*user*/,
                                           0/*pass*/,
                                           0/*path*/,
                                           &net_info,
                                           flgs,
                                           cmcb,
                                           this,
                                           &m_Cmcb,
                                           sx_FtpCallback,
                                           timeout),
                     timeout, buf_size,
                     fConn_Untie | fConn_WriteUnbuffered)
{
    return;
}


CConn_FtpStream::~CConn_FtpStream()
{
    // Explicitly destroy so that the callback is not used out of context.
    x_Destroy();
}


EIO_Status CConn_FtpStream::sx_FtpCallback(void*       data,
                                           const char* cmd,
                                           const char* arg)
{
    CConn_FtpStream* ftp = reinterpret_cast<CConn_FtpStream*>(data);
    try {
        return ftp->m_Cmcb.func(ftp->m_Cmcb.data, cmd, arg);
    }
    NCBI_CATCH_ALL("CConn_FtpStream::sx_FtpCallback()");
    if (ftp->exceptions())
        THROW1_TRACE(IOS_BASE::failure, "CConn_FtpStream::sx_FtpCallback()");
    return eIO_Unknown;
}


EIO_Status CConn_FtpStream::Drain(const STimeout* timeout)
{
    const STimeout* r_timeout = kInfiniteTimeout/*0*/;
    const STimeout* w_timeout = kInfiniteTimeout/*0*/;
    static char sink[16384]; /*NB: shared sink*/
    CONN conn = GetCONN();
    size_t n;
    if (conn) {
        r_timeout = CONN_GetTimeout(conn, eIO_Read);
        w_timeout = CONN_GetTimeout(conn, eIO_Write);
        _VERIFY(CONN_SetTimeout(conn, eIO_ReadWrite, timeout) == eIO_Success);
    }
    clear();
    flush();
    if (conn) {
        // Cause any upload-in-progress to abort
        CONN_Read(conn, sink, sizeof(sink), &n, eIO_ReadPlain);
        // Cause any command-in-progress to abort
        CONN_Write(conn, "\n", 1, &n, eIO_WritePersist);
    }
    clear();
    while (read(sink, sizeof(sink)))
        continue;
    if (!conn)
        return eIO_Closed;
    EIO_Status status;
    do {
        status = CONN_Read(conn, sink, sizeof(sink), &n, eIO_ReadPersist);
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
                                                 TFTP_Flags           flgs,
                                                 const SFTP_Callback* cmcb,
                                                 Uint8                offset,
                                                 const STimeout*      timeout,
                                                 size_t               buf_size)
    : CConn_FtpStream(host, user, pass, path, port, flgs, cmcb,
                      timeout, buf_size)
{
    if (!file.empty())
        x_InitDownload(file, offset);
}


CConn_FTPDownloadStream::CConn_FTPDownloadStream(const SConnNetInfo&  net_info,
                                                 TFTP_Flags           flgs,
                                                 const SFTP_Callback* cmcb,
                                                 Uint8                offset,
                                                 const STimeout*      timeout,
                                                 size_t               buf_size)
    : CConn_FtpStream(net_info, flgs | fFTP_IgnorePath, cmcb,
                      timeout, buf_size)
{
    if (net_info.path[0])
        x_InitDownload(net_info.path, offset);
}


void CConn_FTPDownloadStream::x_InitDownload(const string& file, Uint8 offset)
{
    // Use '\n' here instead of NcbiFlush to avoid (and thus make silent)
    // flush errors on retrieval of nonexistent (or bad) files / directories...
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
        clear(NcbiBadbit);
}


CConn_FTPUploadStream::CConn_FTPUploadStream(const string&   host,
                                             const string&   user,
                                             const string&   pass,
                                             const string&   file,
                                             const string&   path,
                                             unsigned short  port,
                                             TFTP_Flags      flgs,
                                             Uint8           offset,
                                             const STimeout* timeout)
    : CConn_FtpStream(host, user, pass, path, port, flgs, 0/*cmcb*/,
                      timeout)
{
    if (!file.empty())
        x_InitUpload(file, offset);
}


CConn_FTPUploadStream::CConn_FTPUploadStream(const SConnNetInfo& net_info,
                                             TFTP_Flags          flgs,
                                             Uint8               offset,
                                             const STimeout*     timeout)
    : CConn_FtpStream(net_info, flgs | fFTP_IgnorePath, 0/*cmcb*/,
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
        status  = Status(eIO_Write);
    } else
        status  = eIO_Success;
    if (good()  &&  status == eIO_Success) {
        write("STOR ", 5) << file << NcbiFlush;
        status  = Status(eIO_Write);
    }
    if (status != eIO_Success)
        clear(NcbiBadbit);
}


/* non-public class */
class CConn_FileStream : public CConn_IOStream
{
public:
    CConn_FileStream(const string&   ifname,
                     const string&   ofname = kEmptyStr,
                     SFILE_ConnAttr* attr   = 0)
        : CConn_IOStream(TConnector(FILE_CreateConnectorEx(ifname.c_str(),
                                                           ofname.c_str(),
                                                           attr)),
                         0/*timeout*/, 0/*unbuffered*/,
                         fConn_Untie)
    {
        return;
    }
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


extern CConn_IOStream* NcbiOpenURL(const string& url, size_t buf_size)
{
    size_t len = url.size();
    if (!len)
        return 0;
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;  /*NCBI_FAKE_WARNING*/
    }
    bool svc = x_IsIdentifier(url);

    AutoPtr<SConnNetInfo> net_info
        (ConnNetInfo_CreateInternal
         (svc
          ? make_c_unique(SERV_ServiceName(url.c_str())).get()
          : NStr::StartsWith(url, "ftp://", NStr::eNocase)
          ? "_FTP" : 0));
    if (svc)
        return new CConn_ServiceStream(url, fSERV_Any, net_info.get());

    if (net_info  &&  !NCBI_HasSpaces(url.c_str(), len)) {
        unsigned int   host;
        unsigned short port;
        SIZE_TYPE      pos = NStr::Find(url, ":");
        if (0 < pos  &&  pos < len - 1
            &&  url[pos - 1] != '/'  &&  (pos == 1  ||  url[pos - 2] != '/')
            &&  CSocketAPI::StringToHostPort(url, &host, &port) == len
            &&  host  &&  port) {
            net_info->req_method = eReqMethod_Connect;
        }
    }

    if (ConnNetInfo_ParseURL(net_info.get(), url.c_str())) {
        _ASSERT(net_info);  // otherwise ConnNetInfo_ParseURL() would've failed
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
            if (*net_info->host  ||  net_info->port) {
                ERR_POST_X(18, Error << "The limited implementation of the file:// scheme does not"
                           " support non-empty authority portion of the URL (local files only!)");
                break; // not supported
            }
            _ASSERT(!*net_info->user  &&  !*net_info->pass);
            if (net_info->debug_printout) {
                // manual cleanup of most fields req'd
                net_info->req_method = eReqMethod_Any;
                net_info->external = 0;
                net_info->firewall = 0;
                net_info->stateless = 0;
                net_info->lb_disable = 0;
                net_info->http_version = 0;
                net_info->http_push_auth = 0;
                net_info->http_proxy_leak = 0;
                net_info->http_proxy_skip = 0;
                net_info->http_proxy_mask = 0;
                net_info->http_proxy_host[0] = '\0';
                net_info->http_proxy_port    =   0;
                net_info->http_proxy_user[0] = '\0';
                net_info->http_proxy_pass[0] = '\0';
                net_info->max_try = 0;
                net_info->timeout = kInfiniteTimeout/*0*/;
                ConnNetInfo_SetUserHeader(net_info.get(), 0);
                if (net_info->http_referer) {
                    free((void*) net_info->http_referer);
                    net_info->http_referer = 0;
                }
                CORE_LOCK_READ;
                ConnNetInfo_Log(net_info.get(), eLOG_Note, CORE_GetLOG());
                CORE_UNLOCK;
            }
            return new CConn_FileStream(net_info->path);
        case eURL_Ftp:
            if (!net_info->user[0]) {
                strcpy(net_info->user, "ftp");
                if (!net_info->pass[0])
                    strcpy(net_info->pass, "-none@");
            }
            return new CConn_FTPDownloadStream(*net_info, 0, 0, 0,
                                               net_info->timeout, buf_size);
        default:
            break;
        }
    }
    return 0;
}


END_NCBI_SCOPE
