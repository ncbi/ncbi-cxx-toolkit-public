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
* Author:  Aaron Ucko
*
* File Description:
*   Framework for a multithreaded network server
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.1  2001/12/11 19:55:22  ucko
* Introduce thread-pool-based servers.
*
* ===========================================================================
*/

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.h>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// GENERIC CODE:

/* abstract */ class ISockRequestFactory
{
public:
    virtual CStdRequest* New(SOCK sock) const = 0;
};


void s_RunThreadedServer(const ISockRequestFactory& factory, unsigned int port,
                         unsigned int init_threads, unsigned int max_threads,
                         unsigned int queue_size, int spawn_threshold)
{
    LSOCK             lsock;
    CStdPoolOfThreads pool(max_threads, queue_size, spawn_threshold);

    pool.Spawn(init_threads);
    LSOCK_Create(port, 5, &lsock);

    for (;;) {
        SOCK sock;
        EIO_Status status = LSOCK_Accept(lsock, NULL, &sock);
        if (status == eIO_Success) {
            pool.AcceptRequest(factory.New(sock));
        } else {
            ERR_POST("accept failed: " << IO_StatusStr(status));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// CONNECTION STREAMS:

class CConnStreamRequest : public CStdRequest
{
public:
    CConnStreamRequest(SOCK sock, FConnStreamProcessor proc)
        : m_Sock(sock), m_Proc(proc) {}

protected:
    virtual void Process(void);

private:
    SOCK                 m_Sock;
    FConnStreamProcessor m_Proc;
};


void CConnStreamRequest::Process(void)
{
    CConn_SocketStream stream(m_Sock, 0);
    m_Proc(stream);
}


class CConnStreamRequestFactory : public ISockRequestFactory
{
public:
    CConnStreamRequestFactory(FConnStreamProcessor proc) : m_Proc(proc) {}
    virtual CStdRequest* New(SOCK sock) const
        { return new CConnStreamRequest(sock, m_Proc); }

private:
    FConnStreamProcessor m_Proc;
};


void RunThreadedServer(FConnStreamProcessor proc, unsigned int port,
                       unsigned int init_threads, unsigned int max_threads,
                       unsigned int queue_size, int spawn_threshold)
{
    s_RunThreadedServer(CConnStreamRequestFactory(proc), port, init_threads,
                        max_threads, queue_size, spawn_threshold);
}


/////////////////////////////////////////////////////////////////////////////
//
// CONNECTIONS:

class CConnectionRequest : public CStdRequest
{
public:
    CConnectionRequest(SOCK sock, FConnectionProcessor proc)
        : m_Sock(sock), m_Proc(proc) {}

protected:
    virtual void Process(void);

private:
    SOCK                 m_Sock;
    FConnectionProcessor m_Proc;
};


void CConnectionRequest::Process(void)
{
    CONN conn;
    CONN_Create(SOCK_CreateConnectorOnTop(m_Sock, 0), &conn);
    m_Proc(conn);
}


class CConnectionRequestFactory : public ISockRequestFactory
{
public:
    CConnectionRequestFactory(FConnectionProcessor proc) : m_Proc(proc) {}
    virtual CStdRequest* New(SOCK sock) const
        { return new CConnectionRequest(sock, m_Proc); }

private:
    FConnectionProcessor m_Proc;
};


void RunThreadedServer(FConnectionProcessor proc, unsigned int port,
                       unsigned int init_threads, unsigned int max_threads,
                       unsigned int queue_size, int spawn_threshold)
{
    s_RunThreadedServer(CConnectionRequestFactory(proc), port, init_threads,
                        max_threads, queue_size, spawn_threshold);
}


/////////////////////////////////////////////////////////////////////////////
//
// SOCKETS:

class CSockRequest : public CStdRequest
{
public:
    CSockRequest(SOCK sock, FSockProcessor proc)
        : m_Sock(sock), m_Proc(proc) {}

protected:
    virtual void Process(void)
        { m_Proc(m_Sock); }

private:
    SOCK           m_Sock;
    FSockProcessor m_Proc;
};


class CSockRequestFactory : public ISockRequestFactory
{
public:
    CSockRequestFactory(FSockProcessor proc) : m_Proc(proc) {}
    virtual CStdRequest* New(SOCK sock) const
        { return new CSockRequest(sock, m_Proc); }

private:
    FSockProcessor m_Proc;
};


void RunThreadedServer(FSockProcessor proc, unsigned int port,
                       unsigned int init_threads, unsigned int max_threads,
                       unsigned int queue_size, int spawn_threshold)
{
    s_RunThreadedServer(CSockRequestFactory(proc), port, init_threads,
                        max_threads, queue_size, spawn_threshold);
}

END_NCBI_SCOPE
