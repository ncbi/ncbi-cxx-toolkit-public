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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   C++ wrapper for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *   Implementation of CSocketAPI::Poll()
 *
 */

#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE


EIO_Status CSocketAPI::Poll(vector<SPoll>&  sockets,
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    size_t n = sockets.size();
    SSOCK_Poll* polls = 0;

    if (n  &&  !(polls = new SSOCK_Poll[n]))
        return eIO_Unknown;

    for (size_t i = 0; i < n; i++) {
        CSocket& s = sockets[i].m_Socket;
        if (s.GetStatus(eIO_Open) == eIO_Success) {
            polls[i].sock = s.GetSOCK();
            polls[i].event = sockets[i].m_Event;
            polls[i].revent = eIO_Open;
        } else
            polls[i].sock = 0;
    }

    size_t x_ready;
    EIO_Status status = SOCK_Poll(n, polls, timeout, &x_ready);

    if (status == eIO_Success) {
        for (size_t i = 0; i < n; i++)
            sockets[i].m_REvent = polls[i].revent;
    }
    if ( n_ready )
        *n_ready = x_ready;

    delete[] polls;

    return status;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2002/08/12 15:15:36  lavr
 * Initial revision
 *
 * ===========================================================================
 */
