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
 * ---------------------------------------------------------------------------
 * $Log$
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


#include <connect/ncbi_conn_stream.hpp>
#include "ncbi_conn_streambuf.hpp"


BEGIN_NCBI_SCOPE


CConn_IOStream::CConn_IOStream(CONNECTOR connector, const STimeout* timeout,
                               streamsize buf_size, bool do_tie) :
    iostream(0), m_CSb(0)
{
    auto_ptr<CConn_Streambuf>
        csb(new CConn_Streambuf(connector, timeout, buf_size, do_tie));
    init(csb.get());
    m_CSb = csb.release();
}


CONN CConn_IOStream::GetCONN() const
{
    return m_CSb->GetCONN();
}


CConn_IOStream::~CConn_IOStream(void)
{
    streambuf* sb = rdbuf();
    delete sb;
    if (sb != m_CSb)
        delete m_CSb;
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


static CONNECTOR s_HttpConnectorBuilder(const char*    host,
                                        unsigned short port,
                                        const char*    path,
                                        const char*    args,
                                        const char*    user_hdr,
                                        THCC_Flags     flags)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    if (!net_info)
        return 0;
    strncpy(net_info->host, host, sizeof(net_info->host) - 1);
    net_info->host[sizeof(net_info->host) - 1] = '\0';
    net_info->port = port;
    strncpy(net_info->path, path, sizeof(net_info->path) - 1);
    net_info->path[sizeof(net_info->path) - 1] = '\0';
    if (args) {
        strncpy(net_info->args, args, sizeof(net_info->args) - 1);
        net_info->args[sizeof(net_info->args) - 1] = '\0';
    } else {
        *net_info->args = '\0';
    }
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
    : CConn_IOStream(s_HttpConnectorBuilder(host.c_str(),
                                            port,
                                            path.c_str(),
                                            args.c_str(),
                                            user_header.c_str(),
                                            flags),
                     timeout, buf_size)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo* net_info,
                                   const string&       user_header,
                                   THCC_Flags          flags,
                                   const STimeout*     timeout,
                                   streamsize          buf_size)
    : CConn_IOStream(HTTP_CreateConnector(net_info,
                                          user_header.c_str(),
                                          flags),
                     timeout, buf_size)
{
    return;
}


CConn_ServiceStream::CConn_ServiceStream(const string&         service,
                                         TSERV_Type            types,
                                         const SConnNetInfo*   net_info,
                                         const SSERVICE_Extra* params,
                                         const STimeout*       timeout,
                                         streamsize            buf_size)
    : CConn_IOStream(SERVICE_CreateConnectorEx(service.c_str(),
                                               types,
                                               net_info,
                                               params),
                     timeout, buf_size)
{
    return;
}


END_NCBI_SCOPE
