#ifndef CONN___NETSERVICE_CLIENT__HPP
#define CONN___NETSERVICE_CLIENT__HPP

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
 * File Description:
 *   Base classes for net services (NetCache, NetSchedule)
 *
 */

/// @file netservice_client.hpp
/// Network service utilities.
///

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/resource_pool.hpp>

#include <connect/services/netservice_api.hpp> // for CNetServiceException


BEGIN_NCBI_SCOPE


/// Client API for NCBI NetService server
///
///
class NCBI_XCONNECT_EXPORT CNetServiceClient : virtual protected CConnIniter
{
public:
    /// Construct the client without connecting to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// job key
    ///
    /// @param client_name
    ///    Name of the client program(project)
    ///
    CNetServiceClient(const string& client_name);

    CNetServiceClient(const string&  host,
                      unsigned short port,
                      const string&  client_name);

    /// Contruction based on existing TCP/IP socket
    /// @param sock
    ///    Connected socket to the primary server.
    ///    Client does not take socket ownership
    ///    and does not change communication parameters (like timeout)
    ///
    CNetServiceClient(CSocket*      sock,
                      const string& client_name);

    virtual ~CNetServiceClient();

    /// Client Name composition rules
    enum EUseName {
        eUseName_Global, ///< Only global name is used; client_name ignored
        eUseName_Both    ///< The name is composed of: "<global>::<local>"
    };

    /// Set an application-wide name for NetCache clients, and if to use
    /// instance specific name too
    ///
    /// @note
    ///   Global name should be established before creating service clients
    static
    void SetGlobalName(const string&  global_name,
                 EUseName       use_name  = eUseName_Both);

    static
    const string& GetGlobalName();

    static
    EUseName GetNameUse();

    /// Set communication timeout default for all new connections
    static
    void SetDefaultCommunicationTimeout(const STimeout& to);

    /// Set communication timeout (ReadWrite)
    void SetCommunicationTimeout(const STimeout& to);
    STimeout& SetCommunicationTimeout();
    STimeout  GetCommunicationTimeout() const;

    static
    void SetDefaultCreateSocketMaxReties(unsigned int retires);

    void SetCreateSocketMaxRetries(unsigned int retries) { m_MaxRetries = retries; }
    unsigned int GetCreateSocketMaxRetries() const { return m_MaxRetries; }

    void RestoreHostPort();

    /// Set socket (connected to the server)
    ///
    /// @param sock
    ///    Connected socket to the server.
    ///    Communication timeouts of the socket won't be changed
    /// @param own
    ///    Socket ownership
    ///
    void SetSocket(CSocket* sock, EOwnership own = eTakeOwnership);

    /// Connect using specified address (addr - network bo, port - host bo)
    EIO_Status Connect(unsigned int addr, unsigned short port);

    /// Detach and return current socket.
    /// Caller is responsible for deletion.
    CSocket* DetachSocket();

    /// Set client name comment (like LB service name).
    /// May be sent to server for better logging.
    void SetClientNameComment(const string& comment)
    {
        m_ClientNameComment = comment;
    }

    const string& GetClientNameComment() const
    {
        return m_ClientNameComment;
    }
    const string& GetClientName() const { return m_ClientName; }

    /// Return socket to the socket pool
    /// @note thread sync. method
	virtual
    void ReturnSocket(CSocket* sock, const string& blob_comments);

    /// Get socket out of the socket pool (if there are sockets available)
    /// @note thread sync. method
    CSocket* GetPoolSocket();

    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }

protected:
    bool ReadStr(CSocket& sock, string* str);
    void WriteStr(const char* str, size_t len);
    void CreateSocket(const string& hostname, unsigned port);
    void WaitForServer(unsigned wait_sec=0);
    /// Remove "ERR:" prefix
    static
    void TrimErr(string* err_msg);
    void PrintServerOut(CNcbiOstream & out);
    /// Error processing
    void CheckServerOK(string* response);
    enum ETrimErr {
        eNoTrimErr,
        eTrimErr
    };
    virtual void ProcessServerError(string* response, ETrimErr trim_err);

    /// @internal
    class CSockGuard
    {
    public:
        CSockGuard(CSocket& sock) : m_Sock(&sock) {}
        CSockGuard(CSocket* sock) : m_Sock(sock) {}
        ~CSockGuard() { if (m_Sock) m_Sock->Close(); }
        /// Dismiss the guard (no disconnect)
        void Release() { m_Sock = 0; }
    private:
        CSockGuard(const CSockGuard&);
        CSockGuard& operator=(const CSockGuard&);
    private:
        CSocket*    m_Sock;
    };

private:
    CNetServiceClient(const CNetServiceClient&);
    CNetServiceClient& operator=(const CNetServiceClient&);

protected:
    CSocket*                m_Sock;
    string                  m_Host;
    unsigned short          m_Port;
    EOwnership              m_OwnSocket;
    string                  m_ClientName;
    STimeout                m_Timeout;
    string                  m_ClientNameComment;
    string                  m_Tmp;                 ///< Temporary string
    unsigned int            m_MaxRetries;

private:
    CResourcePool<CSocket>  m_SockPool;
    CFastMutex              m_SockPool_Lock;
};


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETSERVICE_CLIENT__HPP */
