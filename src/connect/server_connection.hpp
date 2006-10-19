#ifndef CONNECT___SERVER_CONNECTION__HPP
#define CONNECT___SERVER_CONNECTION__HPP

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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 */

/// @file server_connection.hpp
/// Internal header for threaded server connections.

#include <util/thread_pool.hpp>

#include <connect/server.hpp>


/** @addtogroup ThreadedServer
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class IServer_ConnectionBase
{
public:
    virtual ~IServer_ConnectionBase() { }
    virtual EIO_Event GetEventsToPollFor(void) const
        { return eIO_Read; }
    virtual CStdRequest* CreateRequest(EIO_Event event,
                                       CServer_ConnectionPool& connPool,
                                       const STimeout* timeout) = 0;
    virtual bool IsOpen(void) { return true; }
    virtual void OnTimeout(void) { }
    virtual void OnOverflow(void) { }
    virtual void Activate(void) { }
    virtual void Passivate(void) { }
};

class CServer_Connection : public CSocket, // CPollable
                           public IServer_ConnectionBase
{
public:
    CServer_Connection(IServer_ConnectionHandler* handler)
        : m_Handler(handler), m_Open(true)
        { m_Handler->SetSocket(this); }
    virtual EIO_Event GetEventsToPollFor() 
        { return m_Handler->GetEventsToPollFor(); }
    virtual CStdRequest* CreateRequest(EIO_Event event,
                                       CServer_ConnectionPool& connPool,
                                       const STimeout* timeout);
    virtual void OnTimeout(void) { m_Handler->OnTimeout(); }
    virtual void OnOverflow(void) { m_Handler->OnOverflow(); }
    virtual bool IsOpen(void);
    // connection-specific methods
    void OnSocketEvent(EIO_Event event);
private:
    auto_ptr<IServer_ConnectionHandler> m_Handler;
    bool m_Open;
} ;

class CServer_Listener : public CListeningSocket, // CPollable
                         public IServer_ConnectionBase
{
public:
    CServer_Listener(IServer_ConnectionFactory* factory, unsigned short port)
        : m_Factory(factory), m_Port(port)
        { Listen(m_Port); }
    virtual CStdRequest* CreateRequest(EIO_Event event,
                                       CServer_ConnectionPool& connPool,
                                       const STimeout* timeout);
    virtual void Activate(void) { Listen(m_Port); }
    virtual void Passivate(void) { Close(); }
    // listener-specific methods
    virtual void OnOverflow(void);
private:
    friend class CAcceptRequest;
    auto_ptr<IServer_ConnectionFactory> m_Factory;
    unsigned short m_Port;
} ;


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2006/10/19 20:38:20  joukovv
 * Works in thread-per-request mode. Errors in BDB layer fixed.
 *
 * Revision 6.2  2006/09/27 21:26:06  joukovv
 * Thread-per-request is finally implemented. Interface changed to enable
 * streams, line-based message handler added, netscedule adapted.
 *
 * Revision 6.1  2006/09/13 18:32:21  joukovv
 * Added (non-functional yet) framework for thread-per-request thread pooling model,
 * netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
 *
 * ===========================================================================
 */

#endif  /* CONNECT___SERVER_CONNECTION__HPP */
