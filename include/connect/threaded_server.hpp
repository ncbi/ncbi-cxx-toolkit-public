#ifndef THREADED_SERVER__HPP
#define THREADED_SERVER__HPP

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
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_socket.h>

BEGIN_NCBI_SCOPE

// CThreadedServer - abstract class for network servers using thread pools.
//   This code maintains a pool of threads (initially m_InitThreads, but
//   potentially as many as m_MaxThreads) to deal with incoming connections;
//   each connection gets assigned to one of the worker threads, allowing
//   the server to handle multiple requests in parallel while still checking
//   for new requests.
//
//   You must define Process() to indicate what to do with each incoming
//   connection; .../src/connect/test_threaded_server.cpp illustrates
//   how you might do this.

class CThreadedServer
{
public:
    CThreadedServer(unsigned int port) :
        m_InitThreads(5), m_MaxThreads(10), m_QueueSize(20),
        m_SpawnThreshold(1), m_TemporarilyStopListening(false), m_Port(port)
        {}

    void Run(void);

    // Runs asynchronously (from a separate thread) for each request
    // Implementor must take care of closing socket when done
    virtual void Process(SOCK sock) = 0;

protected:
    // Runs synchronously when request queue is full
    // Implementor must take care of closing socket when done
    virtual void ProcessOverflow(SOCK sock) { SOCK_Close(sock); }

    // Called at the beginning of Run, before creating thread pool
    virtual void SetParams() {}

    // Settings for thread pool
    unsigned int m_InitThreads;     // Number of initial threads
    unsigned int m_MaxThreads;      // Maximum simultaneous threads
    unsigned int m_QueueSize;       // Maximum size of request queue
    unsigned int m_SpawnThreshold;  // Controls when to spawn more threads

    // Temporarily close listener when queue fills?
    bool         m_TemporarilyStopListening;

private:
    unsigned int m_Port; // TCP port to listen on
};

END_NCBI_SCOPE



/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.4  2002/01/25 15:39:29  ucko
* Completely reorganized threaded servers.
*
* Revision 6.3  2002/01/24 20:18:56  ucko
* Add comments
* Add magic TemporarilyStopListening overflow processor
*
* Revision 6.2  2002/01/24 18:36:07  ucko
* Allow custom queue-overflow handling.
*
* Revision 6.1  2001/12/11 19:55:21  ucko
* Introduce thread-pool-based servers.
*
*/

#endif  /* THREADED_SERVER__HPP */
