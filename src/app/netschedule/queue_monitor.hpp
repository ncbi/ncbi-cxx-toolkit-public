#ifndef NS_MONITOR__HPP
#define NS_MONITOR__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Queue monitoring
 *                   
 *
 */

#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE

/// Queue activity monitor
///
/// @internal
///
class CNetScheduleMonitor
{
public:
    CNetScheduleMonitor() : m_Sock(0) {}

    ~CNetScheduleMonitor() 
    { 
        SendString("END");
        delete m_Sock; 
    }


    void SetSocket(CSocket* sock)
    { 
        SendString("END");
        CFastMutexGuard guard(m_Lock);
        delete m_Sock;
        m_Sock = sock; 
    }

    bool IsMonitorActive()
    {
        if (m_Sock == 0) return false;
        CFastMutexGuard guard(m_Lock);

        EIO_Status st = m_Sock->GetStatus(eIO_Open);
        if (st == eIO_Success) {
            return true;
        }

        m_Sock = 0; // socket closed, forget it
        return false;
    }

    void SendMessage(const char* msg, size_t length)
    {
        if (m_Sock) {
            CFastMutexGuard guard(m_Lock);
            if (m_Sock == 0)
                return;
            EIO_Status st = m_Sock->Write(msg, length);
            if (st != eIO_Success) {
                delete m_Sock; m_Sock = 0;
                return;
            }
        }
    }
    void SendString(const string& str)
    {
        SendMessage(str.c_str(), str.length()+1);
    }


private:
    typedef CSocket*       TSocketPtr;

    CFastMutex             m_Lock; 
    volatile TSocketPtr    m_Sock;
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/02 13:59:02  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif

