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
 * Authors:  Anton Lavrentiev,  Denis Vakatov
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
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Connect_Stream


BEGIN_NCBI_SCOPE


CConn_IOStream::CConn_IOStream(CONNECTOR connector, const STimeout* timeout,
                               streamsize buf_size, bool do_tie,
                               CT_CHAR_TYPE* ptr, size_t size) :
    CNcbiIostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(connector, timeout, buf_size, do_tie,
                                ptr, size));
    if (csb->GetCONN()) {
        init(csb.get());
        m_CSb = csb.release();
    } else
        init(0); // according to the standard (27.4.4.1.3), badbit is set here
}


CConn_IOStream::~CConn_IOStream()
{
    Cleanup();
}


CONN CConn_IOStream::GetCONN(void) const
{
    return m_CSb ? m_CSb->GetCONN() : 0;
}


void CConn_IOStream::Close(void)
{
    if (m_CSb)
        m_CSb->Close();
}


void CConn_IOStream::Cleanup(void)
{
    streambuf* sb = rdbuf();
    delete sb;
    if (sb != m_CSb)
        delete m_CSb;
    m_CSb = 0;
#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif // AUTOMATIC_STREAMBUF_DESTRUCTION
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       unsigned int    max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(SOCK_CreateConnector(host.c_str(), port, max_try),
                     timeout, buf_size)
{
    return;
}


CConn_SocketStream::CConn_SocketStream(SOCK            sock,
                                       unsigned int    max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(SOCK_CreateConnectorOnTop(sock, max_try),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_HttpConnectorBuilder(const SConnNetInfo*  a_net_info,
                                        const char*          url,
                                        const char*          host,
                                        unsigned short       port,
                                        const char*          path,
                                        const char*          args,
                                        const char*          user_header,
                                        FHttpParseHTTPHeader parse_header,
                                        FHttpAdjustNetInfo   adjust_net_info,
                                        void*                adjust_data,
                                        FHttpAdjustCleanup   adjust_cleanup,
                                        THCC_Flags           flags,
                                        const STimeout*      timeout)
{
    SConnNetInfo* net_info = a_net_info ?
        ConnNetInfo_Clone(a_net_info) : ConnNetInfo_Create(0);
    if (!net_info)
        return 0;
    if (url  &&  !ConnNetInfo_ParseURL(net_info, url))
        return 0;
    if (host) {
        strncpy0(net_info->host, host, sizeof(net_info->host) - 1);
        net_info->port = port;
    }
    if (path)
        strncpy0(net_info->path, path, sizeof(net_info->path) - 1);
    if (args)
        strncpy0(net_info->args, args, sizeof(net_info->args) - 1);
    if (user_header)
        ConnNetInfo_OverrideUserHeader(net_info, user_header);
    if (timeout  &&  timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    CONNECTOR c = HTTP_CreateConnectorEx(net_info, flags,
        parse_header, adjust_net_info, adjust_data, adjust_cleanup);
    ConnNetInfo_Destroy(net_info);
    return c;
}


CConn_HttpStream::CConn_HttpStream(const string&   host,
                                   const string&   path,
                                   const string&   args,
                                   const string&   user_header,
                                   unsigned short  port,
                                   THCC_Flags      flags,
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


CConn_HttpStream::CConn_HttpStream(const string&       url,
                                   THCC_Flags          flags,
                                   const STimeout*     timeout,
                                   streamsize          buf_size)
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
                                   THCC_Flags          flags,
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


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo*  net_info,
                                   const string&        user_header,
                                   FHttpParseHTTPHeader parse_header,
                                   FHttpAdjustNetInfo   adjust_net_info,
                                   void*                adjust_data,
                                   FHttpAdjustCleanup   adjust_cleanup,
                                   THCC_Flags           flags,
                                   const STimeout*      timeout,
                                   streamsize           buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            parse_header,
                                            adjust_net_info,
                                            adjust_data,
                                            adjust_cleanup,
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
    return;
}


static CONNECTOR s_ServiceConnectorBuilder(const char*           service,
                                           TSERV_Type            types,
                                           const SConnNetInfo*   a_net_info,
                                           const SSERVICE_Extra* params,
                                           const STimeout*       timeout)
{
    SConnNetInfo* net_info = a_net_info ?
        ConnNetInfo_Clone(a_net_info) : ConnNetInfo_Create(service);
    if (!net_info)
        return 0;
    if (timeout && timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    CONNECTOR c = SERVICE_CreateConnectorEx(service, types, net_info, params);
    if (!c)
        ERR_POST_X(1, Error << "Cannot connect to service \"" << service << '\"');
    ConnNetInfo_Destroy(net_info);
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
                                               params,
                                               timeout),
                     timeout, buf_size)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(streamsize  buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(), 0, buf_size, false),
      m_Buf(0), m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(BUF         buf,
                                       EOwnership  owner,
                                       streamsize  buf_size)
    : CConn_IOStream(MEMORY_CreateConnectorEx(buf), 0, buf_size, false,
                     0, BUF_Size(buf)),
      m_Buf(owner == eTakeOwnership ? buf : 0), m_Ptr(0)
{
    return;
}


CConn_MemoryStream::CConn_MemoryStream(const void* ptr,
                                       size_t      size,
                                       EOwnership  owner,
                                       streamsize  buf_size)
    : CConn_IOStream(MEMORY_CreateConnector(), 0, buf_size, false,
                     (CT_CHAR_TYPE*) ptr, size),
      m_Buf(0), m_Ptr(owner == eTakeOwnership ? ptr : 0)
{
    return;
}


CConn_MemoryStream::~CConn_MemoryStream()
{
    Cleanup();
#ifndef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif // AUTOMATIC_STREAMBUF_DESTRUCTION
    BUF_Destroy(m_Buf);
    delete[] (char*) m_Ptr;
}


void CConn_MemoryStream::ToString(string* str)
{
    flush();
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
        str->resize(s);  // NB: this is essentially a NOP when size == s
    }
}


char* CConn_MemoryStream::ToCStr(void)
{
    flush();
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    streamsize size = sb ? (size_t)(tellp() - tellg()) : 0;
    char* str = new char[size + 1];
    if (!str) {
        NCBI_THROW(CIO_Exception, eUnknown,
                   "CConn_MemoryStream::ToCStr() cannot allocate buffer");
    }
    if (sb) {
        streamsize s = sb->sgetn(str, size);
        _ASSERT(size == s);
        size = s;
    }
    str[size] = '\0';
    return str;
}


void CConn_MemoryStream::ToVector(vector<char>* vec)
{
    flush();
    if (!vec) {
        NCBI_THROW(CIO_Exception, eInvalidArg,
                   "CConn_MemoryStream::ToVector(NULL) is not allowed");
    }
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    streamsize size = sb ? (size_t)(tellp() - tellg()) : 0;
    vec->resize(size);
    if (sb) {
        streamsize s = sb->sgetn(&(*vec)[0], size);
        _ASSERT(size == s);
        vec->resize(s);  // NB: this is essentially a NOP when size == s
    }
}


CConn_PipeStream::CConn_PipeStream(const string&         cmd,
                                   const vector<string>& args,
                                   CPipe::TCreateFlags   create_flags,
                                   const STimeout*       timeout,
                                   streamsize            buf_size)
    : CConn_IOStream(PIPE_CreateConnector(cmd, args, create_flags, &m_Pipe),
                     timeout, buf_size), m_Pipe()
{
    return;
}


CConn_PipeStream::~CConn_PipeStream()
{
    // Explicitly call Cleanup() to avoid using dead m_Pipe otherwise.
    Cleanup();
#ifndef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif // AUTOMATIC_STREAMBUF_DESTRUCTION
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


CConn_FTPDownloadStream::CConn_FTPDownloadStream(const string&   host,
                                                 const string&   file,
                                                 const string&   user,
                                                 const string&   pass,
                                                 const string&   path,
                                                 unsigned short  port,
                                                 TFCDC_Flags     flag,
                                                 streamsize      offset,
                                                 const STimeout* timeout,
                                                 streamsize      buf_size)
    : CConn_IOStream(FTP_CreateDownloadConnector(host.c_str(), port,
                                                 user.c_str(), pass.c_str(),
                                                 path.c_str(), flag),
                     timeout, buf_size)
{
    if (file != kEmptyStr) {
        if (offset != 0) {
            write("REST ", 5) << offset << endl;
        }
        if (good()) {
            write("RETR ", 5) << file   << endl;
        }
    }
}

const char* CIO_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eTimeout:      return "eIO_Timeout";
    case eClosed:       return "eIO_Closed";
    case eInterrupt:    return "eIO_Interrupt";
    case eInvalidArg:   return "eIO_InvalidArg";
    case eNotSupported: return "eIO_NotSupported";
    case eUnknown:      return "eIO_Unknown";
    default:            return  CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
