#ifndef CONN___NETCACHE_CLIENT__HPP
#define CONN___NETCACHE_CLIENT__HPP

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
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <connect/connect_export.h>
#include <connect/ncbi_types.h>
#include <connect/netservice_client.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <corelib/ncbistd.hpp>
#include <util/reader_writer.hpp>



BEGIN_NCBI_SCOPE


class CSocket;

/** @addtogroup NetCacheClient
 *
 * @{
 */

/// Client API for NetCache server
///
/// After any Put, Get transactions connection socket
/// is closed (part of NetCache protocol implemenation)
///
/// @sa NetCache_ConfigureWithLB, CNetCacheClient_LB
///
class NCBI_XCONNECT_EXPORT CNetCacheClient : public CNetServiceClient
{
public:
    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// BLOB key 
    CNetCacheClient(const string&  client_name);

    /// Construct client, working with the specified server (host and port)
    CNetCacheClient(const string&  host,
                    unsigned short port,
                    const string&  client_name);

    /// Construction.
    /// @param sock
    ///    Connected socket to the primary server. 
    ///    CNetCacheCleint does not take socket ownership and does not change 
    ///    communication parameters (like timeout)
    /// @param client_name
    ///    Identification name of the connecting client
    CNetCacheClient(CSocket*      sock,
                    const string& client_name);

    virtual ~CNetCacheClient();



    /// Put BLOB to server
    ///
    /// @param time_to_live
    ///    BLOB time to live value in seconds. 
    ///    0 - server side default is assumed.
    /// 
    /// Please note that time_to_live is controlled by the server-side
    /// parameter so if you set time_to_live higher than server-side value,
    /// server side TTL will be in effect.
    ///
    /// @return NetCache access key
    virtual 
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Put BLOB to server
    ///
    /// @param key
    ///    NetCache key, if empty new key is created
    ///    
    /// @param time_to_live
    ///    BLOB time to live value in seconds. 
    ///    0 - server side default is assumed.
    /// 
    /// @return
    ///    IReader* (caller must delete this). 
    virtual
    IWriter* PutData(string* key, unsigned int  time_to_live = 0);

    /// Update an existing BLOB
    ///
    virtual 
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    //
    /// Caller is responsible for deletion of the IReader*
    /// IReader* MUST be deleted before calling any other
    /// method and before destruction of CNetCacheClient.
    ///
    /// @note 
    ///   IReader implementation used here is TCP/IP socket 
    ///   based, so when reading the BLOB please remember to check
    ///   IReader::Read return codes, it may not be able to read
    ///   the whole BLOB in one call because of network delays.
    ///
    /// @param key
    ///    BLOB key to read (returned by PutData)
    /// @param blob_size
    ///    Size of the BLOB
    /// @return
    ///    IReader* (caller must delete this). 
    ///    NULL means that BLOB was not found (expired).
    virtual 
    IReader* GetData(const string& key, 
                     size_t*       blob_size = 0);

    /// Remove BLOB by key
    virtual 
    void Remove(const string& key);

    /// Status of GetData() call
    /// @sa GetData
    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Retrieve BLOB from server by key
    ///
    /// @note
    ///    Function waits for enough data to arrive.
    virtual 
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);


    /// Connects to server to make sure it is running.
    bool IsAlive();

    /// Return version string
    string ServerVersion();

protected:
    /// Shutdown the server daemon.
    ///
    /// @note
    ///  Protected to avoid a temptation to call it from time to time. :)
    void ShutdownServer();

    /// Turn server-side logging on(off)
    ///
    void Logging(bool on_off);

protected:

    bool IsError(const char* str);

    void SendClientName();

    /// Makes string including authentication info(client name) and
    /// command
    void MakeCommandPacket(string* out_str, const string& cmd_str);

    /// If client is not already connected to the primary server it 
    /// tries to connect to the server specified in the BLOB key
    /// (all infomation is encoded in there)
    virtual 
    void CheckConnect(const string& key);

    /// @return netcache key
    void PutInitiate(string*       key,
                     unsigned int  time_to_live);

private:
    CNetCacheClient(const CNetCacheClient&);
    CNetCacheClient& operator=(const CNetCacheClient&);
protected:
    string   m_Tmp;
};


/// Configure NetCache client using NCBI load balancer
///
/// Functions retrieves current status of netcache servers, then connects
/// netcache client to the most available netcache machine.
///
/// @note Please note that it should be done only when we place a new
/// BLOB to the netcache storage. When retriving you should directly connect
/// to the service without any load balancing 
/// (service infomation encoded in the BLOB key)

extern NCBI_XCONNECT_EXPORT
void NetCache_ConfigureWithLB(CNetCacheClient* nc_client, 
                              const string&    service_name);


/// Client API for NetCache server.
///
/// The same API as provided by CNetCacheClient, 
/// only integrated with NCBI load balancer
///
/// Rebalancing is based on a combination of rebalance parameters,
/// when rebalance parameter is not zero and that parameter has been 
/// reached client connects to NCBI load balancing service (could be a 
/// daemon or a network instance) and obtains the most available server.
/// 
/// The intended use case for this client is long living programs like
/// services or fast CGIs or any other program running a lot of NetCache
/// requests.
///
/// @sa CNetCacheClient
///
class NCBI_XCONNECT_EXPORT CNetCacheClient_LB : public CNetCacheClient
{
public:

    typedef CNetCacheClient  TParent;

    /// Construct load-balanced client.
    ///
    ///
    /// @param client_name
    ///    Name of the client
    /// @param lb_service_name
    ///    Service name as listed in NCBI load balancer
    ///
    /// @param rebalance_time
    ///    Number of seconds after which client is rebalanced
    /// @param rebalance_requests
    ///    Number of requests before rebalancing. 
    /// @param rebalance_bytes
    ///    Read/Write bytes before rebalancing occures
    ///
    CNetCacheClient_LB(const string& client_name,
                       const string& lb_service_name,
                       unsigned int  rebalance_time     = 10,
                       unsigned int  rebalance_requests = 0,
                       unsigned int  rebalance_bytes    = 0);

    virtual 
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);
    virtual 
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);
    virtual
    IWriter* PutData(string* key, unsigned int  time_to_live = 0);

    virtual 
    IReader* GetData(const string& key, 
                            size_t*       blob_size = 0);
    virtual 
    void Remove(const string& key);
    virtual 
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);

protected:
    virtual 
    void CheckConnect(const string& key);
private:
    CNetCacheClient_LB(const CNetCacheClient_LB&);
    CNetCacheClient_LB& operator=(const CNetCacheClient_LB&);

private:
    string   m_LB_ServiceName;

    unsigned int  m_RebalanceTime;
    unsigned int  m_RebalanceRequests;
    unsigned int  m_RebalanceBytes;

    time_t        m_LastRebalanceTime;
    unsigned int  m_Requests;
    unsigned int  m_RWBytes;
    bool          m_StickToHost;
};



/// NetCache internal exception
///
class CNetCacheException : public CNetServiceException
{
public:
    typedef CNetServiceException TParent;
    enum EErrCode {
        eAuthenticationError,
        eKeyFormatError,
        eServerError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError: return "eAuthenticationError";
        case eKeyFormatError:      return "eKeyFormatError";
        case eServerError:         return "eServerError";
        default:                   return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheException, CNetServiceException);
};

/// Meaningful information encoded in the NetCache key
///
/// @sa CNetCache_ParseBlobKey
///
struct CNetCache_Key
{
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< BLOB id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};

/// Parse blob key string into a CNetCache_Key structure
extern NCBI_XCONNECT_EXPORT
void CNetCache_ParseBlobKey(CNetCache_Key* key, const string& key_str);

/// Generate blob key string
///
/// Please note that "id" is an integer issued by the netcache server.
/// Clients should not use this function with custom ids. 
/// Otherwise it may disrupt the interserver communication.
///
extern NCBI_XCONNECT_EXPORT
void CNetCache_GenerateBlobKey(string*        key, 
                               unsigned       id, 
                               const string&  host, 
                               unsigned short port);


/// IReader/IWriter implementation 
/// returned by CNetCacheClient::GetData(), CNetCacheClient::PutData()
///
/// @internal
///
class CNetCacheSock_RW : public CSocketReaderWriter
{
public:
    CNetCacheSock_RW(CSocket* sock);
    virtual ~CNetCacheSock_RW();

    /// Take socket ownership
    void OwnSocket();
};


/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2005/02/07 13:00:41  kuznets
 * Part of functionality moved to netservice_client.hpp(cpp)
 *
 * Revision 1.22  2005/01/28 19:25:16  kuznets
 * Exception method inlined
 *
 * Revision 1.21  2005/01/28 14:55:06  kuznets
 * GetCommunicatioTimeout() declared const
 *
 * Revision 1.20  2005/01/28 14:46:38  kuznets
 * Code clean-up, added PutData returning IWriter
 *
 * Revision 1.19  2005/01/19 12:21:10  kuznets
 * +CNetCacheClient_LB
 *
 * Revision 1.18  2005/01/05 17:45:34  kuznets
 * Removed default client name
 *
 * Revision 1.17  2004/12/27 16:30:30  kuznets
 * + logging control function
 *
 * Revision 1.16  2004/12/20 13:13:14  kuznets
 * +NetCache_ConfigureWithLB
 *
 * Revision 1.15  2004/12/16 17:32:45  kuznets
 * + methods to change comm.timeouts
 *
 * Revision 1.14  2004/12/15 19:04:40  kuznets
 * Code cleanup
 *
 * Revision 1.13  2004/11/16 19:26:39  kuznets
 * Improved documentation
 *
 * Revision 1.12  2004/11/04 21:49:51  kuznets
 * CheckConnect() fixed
 *
 * Revision 1.11  2004/11/02 17:29:55  kuznets
 * Implemented reconnection mode and no-default server mode
 *
 * Revision 1.10  2004/11/01 16:02:17  kuznets
 * GetData now returns BLOB size
 *
 * Revision 1.9  2004/11/01 14:39:30  kuznets
 * Implemented BLOB update
 *
 * Revision 1.8  2004/10/28 16:16:09  kuznets
 * +CNetCacheClient::Remove()
 *
 * Revision 1.7  2004/10/27 14:16:45  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.6  2004/10/25 14:36:03  kuznets
 * New methods IsAlive(), ServerVersion()
 *
 * Revision 1.5  2004/10/22 13:51:01  kuznets
 * Documentation chnages
 *
 * Revision 1.4  2004/10/08 12:30:34  lavr
 * Cosmetics
 *
 * Revision 1.3  2004/10/05 19:01:59  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:17:58  kuznets
 * comments, added GetData
 *
 * Revision 1.1  2004/10/04 18:44:33  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETCACHE_CLIENT__HPP */
