#ifndef CONNECT___NCBI_SOCKET_UNIX__HPP
#define CONNECT___NCBI_SOCKET_UNIX__HPP

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   TCP/IP socket API extension for UNIX
 *
 */

#include <connect/ncbi_socket.hpp>


/** @addtogroup Sockets
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CUNIXSocket : public CSocket
{
public:
    // Create unconnected socket
    CUNIXSocket(void) { }

    CUNIXSocket(const string&   filename,
                const STimeout* timeout = kInfiniteTimeout,
                TSOCK_Flags     flags   = fSOCK_LogDefault);

    // May be called on a socket, which is not connected yet
    EIO_Status Connect(const string&   filename,
                       const STimeout* timeout = kDefaultTimeout,
                       TSOCK_Flags     flags   = fSOCK_LogDefault);
};


class NCBI_XCONNECT_EXPORT CUNIXListeningSocket : public CListeningSocket
{
public:
    // Create unbound socket
    CUNIXListeningSocket(void) { }

    CUNIXListeningSocket(const string&  filename,
                         unsigned short backlog = 64,
                         TSOCK_Flags    flags   = fSOCK_LogDefault);

    EIO_Status Listen(const string&  filename,
                      unsigned short backlog = 64,
                      TSOCK_Flags    flags   = fSOCK_LogDefault);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
///  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
/// CUNIXListeningSocket::
///

inline CUNIXListeningSocket::CUNIXListeningSocket(const string&  path,
                                                  unsigned short backlog,
                                                  TSOCK_Flags    flags)
{
    LSOCK_CreateUNIX(path.c_str(), backlog, &m_Socket, flags);
}


inline EIO_Status CUNIXListeningSocket::Listen(const string&  path,
                                               unsigned short backlog,
                                               TSOCK_Flags    flags)
{
    return m_Socket
        ? eIO_Unknown
        : LSOCK_CreateUNIX(path.c_str(), backlog, &m_Socket, flags);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

#endif /* CONNECT___NCBI_SOCKET_UNIX__H */
