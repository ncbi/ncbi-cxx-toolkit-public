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
*/

#include <ncbi_pch.hpp>
#include <connect/threaded_server.hpp>

#include <connect/ncbi_socket.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE


class CSocketRequest : public CStdRequest
{
public:
    CSocketRequest(CThreadedServer& server, SOCK sock)
        : m_Server(server), m_Sock(sock) {}
    virtual void Process(void)
        { m_Server.Process(m_Sock); }

private:
    CThreadedServer& m_Server;
    SOCK             m_Sock;
};


void CThreadedServer::Run(void)
{
    SetParams();

    if (m_InitThreads <= 0  ||  m_MaxThreads < m_InitThreads
        ||  m_MaxThreads > 1000  ||  m_Port > 65535) {
        NCBI_THROW(CThreadedServerException, eBadParameters,
                   "CThreadedServer::Run: Bad parameters");
    }

    CStdPoolOfThreads pool(m_MaxThreads, m_QueueSize, m_SpawnThreshold);
    pool.Spawn(m_InitThreads);

    CListeningSocket lsock(m_Port);
    if (lsock.GetStatus() != eIO_Success) {
        NCBI_THROW(CThreadedServerException, eCouldntListen,
                   "CThreadedServer::Run: Unable to create listening socket: "
                   + string(strerror(errno)));
    }

    for (;;) {
        CSocket    sock;
        EIO_Status status = lsock.Accept(sock);
        sock.SetOwnership(eNoOwnership); // Process[Overflow] will close it
        if (status == eIO_Success) {
            try {
                pool.AcceptRequest
                    (CRef<ncbi::CStdRequest>
                     (new CSocketRequest(*this, sock.GetSOCK())));
                if (pool.IsFull()  &&  m_TemporarilyStopListening) {
                    lsock.Close();
                    pool.WaitForRoom();
                    lsock.Listen(m_Port);
                }
            } catch (CBlockingQueueException) {
                _ASSERT(!m_TemporarilyStopListening);
                ProcessOverflow(sock.GetSOCK());
            }            
        } else {
            ERR_POST("accept failed: " << IO_StatusStr(status));
        }
    }
}


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 6.11  2004/05/17 20:58:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 6.10  2003/08/12 19:28:11  ucko
* Throw CThreadedServerException for abnormal exits.
*
* Revision 6.9  2002/11/04 21:29:02  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 6.8  2002/09/17 18:42:30  ucko
* Use CSocket now that SetOwnership exists.
*
* Revision 6.7  2002/09/13 17:13:27  ucko
* Use CListeningSocket instead of LSOCK, but stick with SOCK to avoid
* double closes.
*
* Revision 6.6  2002/09/13 15:16:25  ucko
* Update for new CBlockingQueue exception setup.
*
* Revision 6.5  2002/08/20 19:23:44  ucko
* Check return status from LSOCK_Create() in CThreadedServer::Run().
* Move CVS log to end of file.
*
* Revision 6.4  2002/01/25 15:39:29  ucko
* Completely reorganized threaded servers.
*
* Revision 6.3  2002/01/24 20:19:18  ucko
* Add magic TemporarilyStopListening overflow processor
* More cleanups
*
* Revision 6.2  2002/01/24 18:35:56  ucko
* Allow custom queue-overflow handling.
* Clean up SOCKs and CONNs when done with them.
*
* Revision 6.1  2001/12/11 19:55:22  ucko
* Introduce thread-pool-based servers.
*
* ===========================================================================
*/
