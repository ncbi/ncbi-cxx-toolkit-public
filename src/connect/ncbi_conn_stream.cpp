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


CConn_IOStream::CConn_IOStream(CONNECTOR connector,
                               streamsize buf_size, bool do_tie)
    : iostream(new CConn_Streambuf(connector, buf_size, do_tie))
{
    return;
}


CConn_IOStream::~CConn_IOStream(void)
{
    delete const_cast<streambuf*> (rdbuf());
}


CConn_SocketStream::CConn_SocketStream(const string&  host,
                                       unsigned short port,
                                       unsigned int   max_try,
                                       streamsize     buf_size)
    : CConn_IOStream(SOCK_CreateConnector(host.c_str(), port, max_try),
                     buf_size)
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
    SConnNetInfo* info = ConnNetInfo_Create(0);
    if (!info)
        return 0;

    strncpy(info->host, host, sizeof(info->host) - 1);
    info->host[sizeof(info->host) - 1] = '\0';
    info->port = port;
    strncpy(info->path, path, sizeof(info->path) - 1);
    info->path[sizeof(info->path) - 1] = '\0';
    if (args) {
        strncpy(info->args, args, sizeof(info->args) - 1);
        info->args[sizeof(info->args) - 1] = '\0';
    } else {
        *info->args = '\0';
    }
    CONNECTOR c = HTTP_CreateConnector(info, user_hdr, flags);
    ConnNetInfo_Destroy(info);
    return c;
}


CConn_HttpStream::CConn_HttpStream(const string&  host,
                                   const string&  path,
                                   const string&  args,
                                   const string&  user_header,
                                   unsigned short port,
                                   THCC_Flags     flags,
                                   streamsize     buf_size)
    : CConn_IOStream(s_HttpConnectorBuilder(host.c_str(),
                                            port,
                                            path.c_str(),
                                            args.c_str(),
                                            user_header.c_str(),
                                            flags),
                     buf_size)
{
    return;
}


CConn_HttpStream::CConn_HttpStream(const SConnNetInfo* info,
                                   const string&       user_header,
                                   THCC_Flags          flags,
                                   streamsize          buf_size)
    : CConn_IOStream(HTTP_CreateConnector(info, user_header.c_str(), flags),
                     buf_size)
{
    return;
}


CConn_ServiceStream::CConn_ServiceStream(const string&       service,
                                         TSERV_Type          types,
                                         const SConnNetInfo* info,
                                         streamsize          buf_size)
    : CConn_IOStream(SERVICE_CreateConnectorEx(service.c_str(),
                                               types, info),
                     buf_size)
{
    return;
}


END_NCBI_SCOPE
