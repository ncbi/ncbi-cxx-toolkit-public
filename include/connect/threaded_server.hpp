#ifndef CONNECT___THREADED_SERVER__HPP
#define CONNECT___THREADED_SERVER__HPP

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
 * Author:  Aaron Ucko
 *
 * File Description:
 *   Framework for a multithreaded network server
 *
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>


/** @addtogroup ThreadedServer
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Exceptions thrown by CThreadedServer::Run
class NCBI_XCONNECT_EXPORT CThreadedServerException
    : EXCEPTION_VIRTUAL_BASE public CConnException
{
public:
    enum EErrCode {
        eBadParameters, ///< Out-of-range parameters given
        eCouldntListen  ///< Unable to bind listening port
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CThreadedServerException, CConnException);
};


/// CThreadedServer - abstract class for network servers using thread pools.
///   This code maintains a pool of threads (initially m_InitThreads, but
///   potentially as many as m_MaxThreads) to deal with incoming connections;
///   each connection gets assigned to one of the worker threads, allowing
///   the server to handle multiple requests in parallel while still checking
///   for new requests.
///
///   You must define Process() to indicate what to do with each incoming
///   connection; .../src/connect/test_threaded_server.cpp illustrates
///   how you might do this.
///
/// @deprecated  Use CServer instead.

NCBI_DEPRECATED_CLASS NCBI_XCONNECT_EXPORT CThreadedServer
    : protected CConnIniter
{
public:
    CThreadedServer(unsigned short port) :
        m_InitThreads(5), m_MaxThreads(10), m_QueueSize(20),
        m_SpawnThreshold(1), m_AcceptTimeout(kInfiniteTimeout),
        m_TemporarilyStopListening(false), m_Port(port) { }

    virtual ~CThreadedServer() { }

    /// Enter the main loop.
    void Run(void);

    /// Start listening immediately, or throw an exception if it is
    /// impossible to do so.  (Does nothing if *this* object is
    /// already listening on the port.)  Calling StartListening()
    /// before Run() will permit detecting port-in-use problems before
    /// the last minute.  (On the other hand, clients that attempt to
    /// connect in the interim will get no response until the main
    /// loop actually starts.)
    void StartListening(void);

    /// Runs asynchronously (from a separate thread) for each request.
    /// Implementor must take care of closing the socket when done.
    /// (Using it as the basis of a CConn_SocketStream object will do
    /// so automatically.)
    virtual void Process(SOCK sock) = 0;

    /// Get the listening port number back.
    unsigned short GetPort() const { return m_Port; }

protected:
    /// Runs synchronously when request queue is full.
    /// Implementor must take care of closing socket when done.
    virtual void ProcessOverflow(SOCK sock) { SOCK_Close(sock); }

    /// Runs synchronously when accept has timed out.
    virtual void ProcessTimeout(void) {}

    /// Runs synchronously between iterations.
    virtual bool ShutdownRequested(void) { return false; }

    /// Called at the beginning of Run, before creating thread pool.
    virtual void SetParams() {}

    /// Settings for thread pool (which is local to Run):

    unsigned int    m_InitThreads;     ///< Number of initial threads
    unsigned int    m_MaxThreads;      ///< Maximum simultaneous threads
    unsigned int    m_QueueSize;       ///< Maximum size of request queue
    unsigned int    m_SpawnThreshold;  ///< Controls when to spawn more threads
    const STimeout* m_AcceptTimeout;   ///< Maximum time between exit checks

    /// Temporarily close listener when queue fills?
    bool            m_TemporarilyStopListening;

private:
    unsigned short   m_Port;  ///< TCP port to listen on
    CListeningSocket m_LSock; ///< Listening socket
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___THREADED_SERVER__HPP */
