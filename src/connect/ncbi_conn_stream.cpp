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
#include "ncbi_core_cxxp.hpp"
#include <connect/ncbi_conn_stream.hpp>


BEGIN_NCBI_SCOPE


CConn_IOStreamBase::CConn_IOStreamBase()
{
    CONNECT_InitInternal();
}


CConn_IOStream::CConn_IOStream(CONNECTOR connector, const STimeout* timeout,
                               streamsize buf_size, bool do_tie) :
    CNcbiIostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(connector, timeout, buf_size, do_tie));
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
#endif
}


CConn_SocketStream::CConn_SocketStream(const string&   host,
                                       unsigned short  port,
                                       unsigned int    max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(SOCK_CreateConnector(host.c_str(), port, max_try),
                     timeout, buf_size)
{
}


CConn_SocketStream::CConn_SocketStream(SOCK            sock,
                                       unsigned int    max_try,
                                       const STimeout* timeout,
                                       streamsize      buf_size)
    : CConn_IOStream(SOCK_CreateConnectorOnTop(sock, max_try),
                     timeout, buf_size)
{
}


static CONNECTOR s_HttpConnectorBuilder(const SConnNetInfo* net_info_in,
                                        const char*         url,
                                        const char*         host,
                                        unsigned short      port,
                                        const char*         path,
                                        const char*         args,
                                        const char*         user_hdr,
                                        THCC_Flags          flags,
                                        const STimeout*     timeout)
{
    SConnNetInfo* net_info = net_info_in ?
        ConnNetInfo_Clone(net_info_in) : ConnNetInfo_Create(0);
    if (!net_info)
        return 0;
    if (url && !ConnNetInfo_ParseURL(net_info, url))
        return 0;
    if (host) {
        strncpy0(net_info->host, host, sizeof(net_info->host) - 1);
        net_info->port = port;
    }
    if (path)
        strncpy0(net_info->path, path, sizeof(net_info->path) - 1);
    if (args)
        strncpy0(net_info->args, args, sizeof(net_info->args) - 1);
    if (timeout && timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    CONNECTOR c = HTTP_CreateConnector(net_info, user_hdr, flags);
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
                                            user_header.empty()
                                            ? 0 : user_header.c_str(),
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
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
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
}


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   THCC_Flags          flags,
                                   const STimeout*     timeout,
                                   streamsize          buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(net_info,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            user_header.c_str(),
                                            flags,
                                            timeout),
                     timeout, buf_size)
{
}


static CONNECTOR s_ServiceConnectorBuilder(const char*           service,
                                           TSERV_Type            types,
                                           const SConnNetInfo*   net_info_in,
                                           const SSERVICE_Extra* params,
                                           const STimeout*       timeout)
{
    SConnNetInfo* net_info = net_info_in ?
        ConnNetInfo_Clone(net_info_in) : ConnNetInfo_Create(service);
    if (!net_info)
        return 0;
    if (timeout && timeout != kDefaultTimeout) {
        net_info->tmo     = *timeout;
        net_info->timeout = &net_info->tmo;
    } else if (!timeout)
        net_info->timeout = 0;
    CONNECTOR c = SERVICE_CreateConnectorEx(service, types, net_info, params);
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
}


static CONNECTOR s_MemoryConnectorBuilder(BUF        buf,
                                          CRWLock*   lk,
                                          EOwnership lk_owner,
                                          MT_LOCK*   lock)
{
    *lock = MT_LOCK_cxx2c(lk,
                          lk_owner == eTakeOwnership ? 1/*true*/ : 0/*false*/);
    return MEMORY_CreateConnectorEx(buf, *lock);
}


CConn_MemoryStream::CConn_MemoryStream(CRWLock*   lk,
                                       EOwnership lk_owner,
                                       streamsize buf_size)
    : CConn_IOStream(s_MemoryConnectorBuilder(0, lk, lk_owner, &m_Lock),
                     0, buf_size), m_Buf(0)
{
}


CConn_MemoryStream::CConn_MemoryStream(BUF        buf,
                                       CRWLock*   lk,
                                       EOwnership lk_owner,
                                       streamsize buf_size)
    : CConn_IOStream(s_MemoryConnectorBuilder(buf, lk, lk_owner, &m_Lock),
                     0, buf_size), m_Buf(buf)
{
}


CConn_MemoryStream::~CConn_MemoryStream()
{
    Cleanup();
#ifndef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif
    BUF_Destroy(m_Buf);
    MT_LOCK_Delete(m_Lock);
}


string& CConn_MemoryStream::ToString(string& str)
{
    flush();
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    size_t size = sb ? (size_t)(tellp() - tellg()) : 0;
    str.resize(size);
    if (sb) {
        if (CONN_Read(sb->GetCONN(), &str[0], size, &size, eIO_ReadPersist)
            != eIO_Success) {
            str.resize(size);
        }
    }
    return str;
}


char* CConn_MemoryStream::ToCStr(void)
{
    flush();
    CConn_Streambuf* sb = dynamic_cast<CConn_Streambuf*>(rdbuf());
    size_t size = sb ? (size_t)(tellp() - tellg()) : 0;
    char* str = new char[size + 1];
    if (sb)
        CONN_Read(sb->GetCONN(), str, size, &size, eIO_ReadPersist);
    str[size] = '\0';
    return str;
}


CConn_PipeStream::CConn_PipeStream(const string&         cmd,
                                   const vector<string>& args,
                                   CPipe::TCreateFlags   create_flags,
                                   const STimeout*       timeout,
                                   streamsize            buf_size)
    : CConn_IOStream(PIPE_CreateConnector(cmd, args, create_flags, &m_Pipe),
                     timeout, buf_size), m_Pipe()
{
}


CConn_PipeStream::~CConn_PipeStream()
{
    // Explicitly call Cleanup() to avoid using dead m_Pipe otherwise.
    Cleanup();
#ifndef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif
}


CConn_NamedPipeStream::CConn_NamedPipeStream(const string&   pipename,
                                             size_t          pipebufsize,
                                             const STimeout* timeout,
                                             streamsize      buf_size)
    : CConn_IOStream(NAMEDPIPE_CreateConnector(pipename, pipebufsize),
                     timeout, buf_size)
{
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
            *this << "REST " << offset << endl;
        }
        if (good()) {
            *this << "RETR " << file << endl;
        }
    }
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.46  2005/05/18 18:14:56  lavr
 * Add flag parameter to FTP download stream ctor
 *
 * Revision 6.45  2005/03/15 21:28:22  lavr
 * +CConn_IOStream::Close()
 *
 * Revision 6.44  2005/02/25 17:20:54  lavr
 * CConn_MemoryStream::"string conversions" to do flush first
 *
 * Revision 6.43  2005/02/25 16:50:50  lavr
 * CConn_MemoryStream::dtor() to explicitly call Cleanup() to avoid using dead members
 *
 * Revision 6.42  2004/12/09 13:36:04  lavr
 * MSVC compilation fix
 *
 * Revision 6.40  2004/12/08 21:01:42  lavr
 * +CConn_FTPDownloadStream
 *
 * Revision 6.39  2004/10/28 12:49:33  lavr
 * Memory stream lock ownership -> EOwnership, MT_LOCK cleanup in dtor()
 *
 * Revision 6.38  2004/10/27 20:59:37  vasilche
 * Initialize m_Buf in all constructors.
 *
 * Revision 6.37  2004/10/27 18:53:23  lavr
 * +CConn_MemoryStream(BUF buf,...)
 *
 * Revision 6.36  2004/10/27 15:49:45  lavr
 * +CConn_MemoryStream::ToCStr()
 *
 * Revision 6.35  2004/10/27 14:16:38  ucko
 * CConn_MemoryStream::ToString: use erase() rather than clear(), which
 * GCC 2.95 doesn't support.
 *
 * Revision 6.34  2004/10/26 20:30:52  lavr
 * +CConn_MemoryStream::ToString()
 *
 * Revision 6.33  2004/09/09 16:44:22  lavr
 * Introduce virtual helper base CConn_IOStreamBase for implicit CONNECT_Init()
 *
 * Revision 6.32  2004/06/22 17:00:12  lavr
 * Handle empty user_header as no-op
 *
 * Revision 6.31  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.30  2004/03/22 16:54:32  ivanov
 * Cosmetic changes
 *
 * Revision 6.29  2004/01/20 20:36:05  lavr
 * Cease using HAVE_BUGGY_IOS_CALLBACKS in this file
 *
 * Revision 6.28  2003/11/12 17:45:08  lavr
 * Change log fixed to reflect real changes in previous commit
 *
 * Revision 6.27  2003/11/12 16:37:50  ivanov
 * CConn_PipeStream:: dtor added
 *
 * Revision 6.26  2003/10/23 12:16:27  lavr
 * CConn_IOStream:: base class is now CNcbiIostream
 *
 * Revision 6.25  2003/09/23 21:05:23  lavr
 * +CConn_PipeStream, +CConn_NamedPipeStream
 *
 * Revision 6.24  2003/08/25 14:40:43  lavr
 * Employ new k..Timeout constants
 *
 * Revision 6.23  2003/05/20 19:08:28  lavr
 * Do not use clear() in constructor ('cause it may throw an exception)
 *
 * Revision 6.22  2003/05/20 16:57:56  lavr
 * Fix typo in log
 *
 * Revision 6.21  2003/05/20 16:47:19  lavr
 * GetCONN() to check for NULL; constructor to init(0) if connection is bad
 *
 * Revision 6.20  2003/05/12 18:32:27  lavr
 * Modified not to throw exceptions from stream buffer; few more improvements
 *
 * Revision 6.19  2003/04/29 19:58:24  lavr
 * Constructor taking a URL added in CConn_HttpStream
 *
 * Revision 6.18  2003/04/14 21:08:15  lavr
 * Take advantage of HAVE_BUGGY_IOS_CALLBACKS
 *
 * Revision 6.17  2003/04/11 17:57:29  lavr
 * Take advantage of HAVE_IOS_XALLOC
 *
 * Revision 6.16  2003/03/25 22:17:18  lavr
 * Set timeouts from ctors in SConnNetInfo, too, for display unambigously
 *
 * Revision 6.15  2002/10/28 15:42:18  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.14  2002/06/06 19:02:32  lavr
 * Some housekeeping: log moved to the end
 *
 * Revision 6.13  2002/02/21 18:04:25  lavr
 * +class CConn_MemoryStream
 *
 * Revision 6.12  2002/02/05 22:04:12  lavr
 * Included header files rearranged
 *
 * Revision 6.11  2002/02/05 16:05:26  lavr
 * List of included header files revised
 *
 * Revision 6.10  2002/01/28 21:29:25  lavr
 * Use iostream(0) as a base class constructor
 *
 * Revision 6.9  2002/01/28 20:19:11  lavr
 * Clean destruction of streambuf sub-object; no exception throw in GetCONN()
 *
 * Revision 6.8  2001/12/10 19:41:18  vakatov
 * + CConn_SocketStream::CConn_SocketStream(SOCK sock, ....)
 *
 * Revision 6.7  2001/12/07 22:58:01  lavr
 * GetCONN(): throw exception if the underlying streambuf is not CONN-based
 *
 * Revision 6.6  2001/09/24 20:26:17  lavr
 * +SSERVICE_Extra* parameter for CConn_ServiceStream::CConn_ServiceStream()
 *
 * Revision 6.5  2001/01/12 23:49:19  lavr
 * Timeout and GetCONN method added
 *
 * Revision 6.4  2001/01/11 23:04:06  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.3  2001/01/11 17:22:41  thiessen
 * fix args copy in s_HttpConnectorBuilder()
 *
 * Revision 6.2  2001/01/10 21:41:10  lavr
 * Added classes: CConn_SocketStream, CConn_HttpStream, CConn_ServiceStream.
 * Everything is now wordly documented.
 *
 * Revision 6.1  2001/01/09 23:35:25  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */
