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
#include <util/resource_pool.hpp>


BEGIN_NCBI_SCOPE


/// Client API for NCBI NetService server
///
/// 
class NCBI_XCONNECT_EXPORT CNetServiceClient
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
    void ReturnSocket(CSocket* sock);

    /// Get socket out of the socket pool (if there are sockets available)
    /// @note thread sync. method
    CSocket* GetPoolSocket();

    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }

protected:
    bool ReadStr(CSocket& sock, string* str);
    void WriteStr(const char* str, size_t len);
    void CreateSocket(const string& hostname, unsigned port);
    void WaitForServer(unsigned wait_sec=20);
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

private:
    CResourcePool<CSocket>  m_SockPool;
    CFastMutex              m_SockPool_Lock;
};


/// Net Service exception
///
class CNetServiceException : public CException
{
public:
    enum EErrCode {
        eTimeout,
        eCommunicationError,
        eProtocolError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eTimeout:            return "eTimeout";
        case eCommunicationError: return "eCommunicationError";
        case eProtocolError:      return "eProtocolError";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetServiceException, CException);
};


/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2006/12/07 21:26:06  joukovv
 * Error processing fixed.
 *
 * Revision 1.19  2006/12/07 16:22:10  joukovv
 * Transparent server-to-client exception report implemented. Version control
 * bug fixed.
 *
 * Revision 1.18  2006/12/06 14:57:07  didenko
 * Added methods for retrieving connection info
 *
 * Revision 1.17  2006/02/17 17:05:27  kuznets
 * Added explicit timeout to WaitForServer()
 *
 * Revision 1.16  2006/02/15 18:36:01  lavr
 * Remove inclusion of unnecessary header files
 *
 * Revision 1.15  2006/01/11 17:57:23  kuznets
 * Fixed race condition in socket pooling
 *
 * Revision 1.14  2006/01/10 15:00:39  kuznets
 * #include ncbimtx.hpp
 *
 * Revision 1.13  2006/01/10 14:45:24  kuznets
 * Save sockets: + connection pool
 *
 * Revision 1.12  2006/01/03 15:35:57  kuznets
 * Added network ICache client
 *
 * Revision 1.11  2005/11/01 14:56:21  kuznets
 * Compilation warning fixed
 *
 * Revision 1.10  2005/09/21 16:11:18  kuznets
 * Added support for global application wide client names
 *
 * Revision 1.9  2005/08/17 15:14:39  kuznets
 * Forget() to Release()
 *
 * Revision 1.8  2005/08/17 14:25:47  kuznets
 * CSockGuard added Forget()
 *
 * Revision 1.7  2005/08/15 13:28:05  kuznets
 * Added protocol error
 *
 * Revision 1.6  2005/07/27 18:12:53  kuznets
 * Added PrintServerOut()
 *
 * Revision 1.5  2005/03/28 15:31:37  didenko
 * Made destructors virtual
 *
 * Revision 1.4  2005/03/21 16:42:45  didenko
 * +GetClientName()
 *
 * Revision 1.3  2005/03/17 17:16:59  kuznets
 * +Connect()
 *
 * Revision 1.2  2005/02/09 18:58:33  kuznets
 * +TrimErr() method
 *
 * Revision 1.1  2005/02/07 12:58:36  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETSERVICE_CLIENT__HPP */
