#ifndef CONNECT___THREADED_SERVER__HPP
#define CONNECT___THREADED_SERVER__HPP

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
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_socket.h>


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
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eBadParameters: return "eBadParameters";
        case eCouldntListen: return "eCouldntListen";
        default:             return CException::GetErrCodeString();
        }
    }
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

class NCBI_XCONNECT_EXPORT CThreadedServer
{
public:
    CThreadedServer(unsigned short port) :
        m_InitThreads(5), m_MaxThreads(10), m_QueueSize(20),
        m_SpawnThreshold(1), m_AcceptTimeout(kInfiniteTimeout),
        m_TemporarilyStopListening(false), m_Port(port)
        {}

    /// Enter the main loop.
    void Run(void);

    /// Runs asynchronously (from a separate thread) for each request.
    /// Implementor must take care of closing socket when done.
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
    unsigned short  m_Port; ///< TCP port to listen on
};


END_NCBI_SCOPE


/* @} */


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.14  2005/05/02 16:01:42  lavr
 * Proper exception class derivation
 *
 * Revision 6.13  2005/01/05 15:09:44  ucko
 * Complete "DOXYGENization"
 *
 * Revision 6.12  2004/10/08 12:41:49  lavr
 * Cosmetics
 *
 * Revision 6.11  2004/09/29 15:20:36  kuznets
 * +GetPort()
 *
 * Revision 6.10  2004/08/19 12:43:38  dicuccio
 * Dropped unnecessary export specifier
 *
 * Revision 6.9  2004/07/15 18:58:10  ucko
 * Make more versatile, per discussion with Peter Meric:
 * - Periodically check whether to keep going or gracefully bail out,
 *   based on a new callback method (ShutdownRequested).
 * - Add a timeout for accept, and corresponding callback (ProcessTimeout).
 *
 * Revision 6.8  2003/08/12 19:27:52  ucko
 * +CThreadedServerException
 *
 * Revision 6.7  2003/04/09 19:06:05  siyan
 * Added doxygen support
 *
 * Revision 6.6  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.5  2002/09/19 18:06:04  lavr
 * Header file guard macro changed
 *
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

#endif  /* CONNECT___THREADED_SERVER__HPP */
