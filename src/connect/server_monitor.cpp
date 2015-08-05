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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Queue monitoring
 *
 */

#include <ncbi_pch.hpp>
#include <connect/server_monitor.hpp>


BEGIN_NCBI_SCOPE

/// Server monitor
///

CServer_Monitor::CServer_Monitor() : m_Sock(0) {}

CServer_Monitor::~CServer_Monitor() 
{ 
    SendString("END");
    delete m_Sock; 
}


void CServer_Monitor::SetSocket(CSocket& socket)
{ 
    SendString("END");
    CFastMutexGuard guard(m_Lock);
    delete m_Sock;

    auto_ptr<CSocket> s(new CSocket());

    // Pass internals of socket into new one
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);
    s->Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

    m_Sock = s.release(); 
}

bool CServer_Monitor::IsMonitorActive()
{
    if (!m_Sock) return false;
    CFastMutexGuard guard(m_Lock);
    if (!m_Sock) return false;

    EIO_Status st = m_Sock->GetStatus(eIO_Open);
    if (st == eIO_Success) {
        return true;
    }
    delete m_Sock;
    m_Sock = 0; // socket closed, forget it
    return false;
}

void CServer_Monitor::SendMessage(const char* msg, size_t length)
{
    CFastMutexGuard guard(m_Lock);
    if (!m_Sock) return;
    EIO_Status st = m_Sock->Write(msg, length);
    if (st != eIO_Success) {
        delete m_Sock;
        m_Sock = 0;
    }
}

void CServer_Monitor::SendString(const string& str)
{
    SendMessage(str.data(), str.length());
}


bool CServer_Monitor::IsActive()
{
    return IsMonitorActive();
}

void CServer_Monitor::Send(const char* msg, size_t length)
{
    SendMessage(msg, length);
}

void CServer_Monitor::Send(const string& str)
{
    SendString(str);
}


END_NCBI_SCOPE
