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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_stream.hpp>

BEGIN_NCBI_SCOPE

// Callback types, from highest- to lowest-level

typedef void (*FConnStreamProcessor)(CConn_IOStream& stream);
typedef void (*FConnectionProcessor)(CONN conn);
typedef void (*FSockProcessor)(SOCK sock);

void TemporarilyStopListening(SOCK sock);

// Arguments to RunThreadedServer:
//  proc - What to do with each incoming connection in normal circumstances
//       - Runs asynchronously, in a separate thread from RunThreadedServer
//       - Use whichever argument type is most convenient
//       - Do NOT close anything; this happens automatically
//
//  port - TCP port to listen on
//
//  init_threads, max_threads, queue_size, spawn_threshold
//       - Passed directly to thread pool constructor
//
//  overflow_proc - What to do with each incoming connection when queue is full
//                - Runs synchronously, from within RunThreadedServer
//                - The special value TemporarilyStopListening closes the
//                  listening socket until the queue has room again
//                - Do NOT close the socket; this happens automatically
//                - Default is a no-op (aside from automatic socket closing)

void RunThreadedServer(FConnStreamProcessor proc, unsigned int port,
                       unsigned int init_threads = 5,
                       unsigned int max_threads = 10,
                       unsigned int queue_size = 20, int spawn_threshold = 1,
                       FSockProcessor overflow_proc = 0);

void RunThreadedServer(FConnectionProcessor proc, unsigned int port,
                       unsigned int init_threads = 5,
                       unsigned int max_threads = 10,
                       unsigned int queue_size = 20, int spawn_threshold = 1,
                       FSockProcessor overflow_proc = 0);

void RunThreadedServer(FSockProcessor proc, unsigned int port,
                       unsigned int init_threads = 5,
                       unsigned int max_threads = 10,
                       unsigned int queue_size = 20, int spawn_threshold = 1,
                       FSockProcessor overflow_proc = 0);

END_NCBI_SCOPE

#endif  /* THREADED_SERVER__HPP */
