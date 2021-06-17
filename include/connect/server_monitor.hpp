#ifndef CONNECT___SERVER_MONITOR__HPP
#define CONNECT___SERVER_MONITOR__HPP

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
 * File Description: Server monitoring
 *
 */

#include <corelib/ncbimtx.hpp>
#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup ThreadedServer
 *
 * @{
 */

/// Base interface for monitoring
///
struct IServer_Monitor
{
    virtual ~IServer_Monitor() {}

    /// Check if monitoring is active
    virtual bool IsActive() = 0;
    /// Send message
    virtual void Send(const char* msg, size_t length) = 0;
    /// Send message
    virtual void Send(const string& str) = 0;
};



/// Server monitor
///
class NCBI_XCONNECT_EXPORT CServer_Monitor : public IServer_Monitor
{
public:
    CServer_Monitor();

    virtual ~CServer_Monitor();
    /// Pass open socket for monitor output. The original socket is
    /// empty afterwards, ownership is handled by monitor. It activates
    /// the monitor.
    void SetSocket(CSocket& socket);
    bool IsMonitorActive();
    void SendMessage(const char* msg, size_t length);
    void SendString(const string& str);

    /// @name IServer_Monitor interface
    /// @{
    virtual bool IsActive();
    virtual void Send(const char* msg, size_t length);
    virtual void Send(const string& str);
    ///@}

private:
    CServer_Monitor(const CServer_Monitor&);
    CServer_Monitor& operator=(const CServer_Monitor&);
private:
    CFastMutex m_Lock; 
    CSocket*   m_Sock;
};

/* @} */


END_NCBI_SCOPE

#endif /* CONNECT___SERVER_MONITOR__HPP */
