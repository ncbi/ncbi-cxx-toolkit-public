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

#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netservice_client.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/version.hpp>
#include <util/transmissionrw.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

class NCBI_XCONNECT_EXPORT CNetCacheClientBase : public CNetServiceClient
{
public:
    CNetCacheClientBase(const string& client_name);

    CNetCacheClientBase(const string&  host,
                        unsigned short port,
                        const string&  client_name);

protected:
    /// Send session registration command
    void RegisterSession(unsigned pid);
    /// Send session unregistration command
    void UnRegisterSession(unsigned pid);
    bool CheckAlive();

    virtual
    void CheckOK(string* str) const;
    virtual
    void TrimPrefix(string* str) const;
};

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
    typedef CNetServiceClient TParent;

    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// BLOB key
    ///
    /// @param client_name
    ///    local name of the client program
    ///    (used for server access logging and authentication)
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

    /// BLOB locking mode
    enum ELockMode {
        eLockWait,   ///< waits for BLOB to become available
        eLockNoWait  ///< throws an exception immediately if BLOB locked
    };

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
    /// @param lock_mode
    ///    Blob locking mode
    /// @return
    ///    IReader* (caller must delete this).
    ///    NULL means that BLOB was not found (expired).
    virtual
    IReader* GetData(const string& key,
                     size_t*       blob_size = 0,
                     ELockMode     lock_mode = eLockWait);

    /// NetCache server locks BLOB so only one client can
    /// work with one BLOB at a time. Method returns TRUE
    /// if BLOB is locked at the moment.
    ///
    /// @return TRUE if BLOB exists and locked by another client
    /// FALSE if BLOB not found or not locked
    virtual
    bool IsLocked(const string& key);

    /// Retrieve BLOB's owner information as registered by the server
    virtual
    string GetOwner(const string& key);

    /// Status of GetData() call
    /// @sa GetData
    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Structure to read BLOB in memory
    struct SBlobData
    {
        AutoPtr<unsigned char,
                ArrayDeleter<unsigned char> >  blob;      ///< blob data
        size_t                                 blob_size; ///< blob size

        SBlobData() : blob_size(0) {}
    };

    /// Retrieve BLOB from server by key
    /// This method retrives BLOB size, allocates memory and gets all
    /// the data from the server.
    ///
    /// Blob size and binary data is placed into blob_to_read structure.
    /// Do not use this method if you are not sure you have memory
    /// to load the whole BLOB.
    ///
    /// @return
    ///    eReadComplete if BLOB found (eNotFound otherwise)
    virtual
    EReadResult GetData(const string& key, SBlobData& blob_to_read);

    /// Remove BLOB by key
    virtual
    void Remove(const string& key);


    /// Returns communication error message or empty string.
    /// This method is used to deliver error message from
    /// IReader/IWriter because they sometimes cannot throw exceptions
    /// (intercepted by C++ stream library)
    ///
    /// This method can be called once to get error, repeated call
    /// returns empty string
    virtual
    string GetCommErrMsg();


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



    /// Return version string
    string ServerVersion();

    /// Return version of the server (parses output of ServerVersion())
    CVersionInfo ServerVersionInfo();

protected:
    /// Shutdown the server daemon.
    ///
    /// @note
    ///  Protected to avoid a temptation to call it from time to time. :)
    void ShutdownServer();

    /// Turn server-side logging on(off)
    ///
    void Logging(bool on_off);
    /// Connects to server to make sure it is running.
    bool IsAlive();

    /// Print ini file
    void PrintConfig(CNcbiOstream& out);

    /// Print server statistics
    void PrintStat(CNcbiOstream& out);

    /// Reinit server side statistics collector
    void DropStat();

    /// Set communication error message
    virtual
    void SetCommErrMsg(const string& msg);

    bool IsError(const char* str);

    void Monitor(CNcbiOstream & out);

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
                     unsigned      time_to_live,
                     unsigned      put_version = 0);

    void TransmitBuffer(const char* buf, size_t len);

    friend class CNetCache_WriterErrCheck;

public:
    static
    bool CheckErrTrim(string& answer) { return x_CheckErrTrim(answer); }

protected:
    /// Check if server output starts with "ERR:", throws an exception
    /// "OK:", "ERR:" prefixes are getting trimmed
    ///
    /// @return false if Blob not found error detected
    static
    bool x_CheckErrTrim(string& answer);

private:
    CNetCacheClient(const CNetCacheClient&);
    CNetCacheClient& operator=(const CNetCacheClient&);

protected:
    string    m_Tmp;
    unsigned  m_PutVersion;
    string    m_CommErrMsg;  ///< communication err message
};



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

    virtual ~CNetCacheClient_LB();

    /// Specifies when client use backup servers instead of LB provided
    /// services
    ///
    enum EServiceBackupMode {
        ENoBackup     = (0 << 0), ///< Do not use backup servers
        ENameNotFound = (1 << 0), ///< backup if LB does not find service name
        ENoConnection = (1 << 1), ///< backup if no TCP connection
        EFullBackup   = (ENameNotFound | ENoConnection)
    };

    typedef int TServiceBackupMode;

    /// Enable client to connect to predefined backup servers if service
    /// name is not available (load balancer is not available)
    /// (enabled by default)
    ///
    void EnableServiceBackup(TServiceBackupMode backup_mode = EFullBackup);


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
                     size_t*       blob_size = 0,
                     ELockMode     lock_mode = eLockWait);
    virtual
    bool IsLocked(const string& key);

    virtual
    string GetOwner(const string& key);


    virtual
    void Remove(const string& key);
    virtual
    EReadResult GetData(const string&  key,
                        void*          buf,
                        size_t         buf_size,
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);
    virtual
    EReadResult GetData(const string& key, SBlobData& blob_to_read);

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

    time_t               m_LastRebalanceTime;
    unsigned int         m_Requests;
    unsigned int         m_RWBytes;
    bool                 m_StickToHost;
    TServiceBackupMode   m_ServiceBackup;
};

NCBI_DECLARE_INTERFACE_VERSION(CNetCacheClient,  "xnetcache", 1, 1, 0);


/// Meaningful information encoded in the NetCache key
///
/// @sa CNetCache_ParseBlobKey
///
class NCBI_XCONNECT_EXPORT CNetCache_Key
{
public:
    /// Create the key out of string
    /// @sa ParseBlobKey()
    CNetCache_Key(const string& key_str);

    /// Parse blob key string into a CNetCache_Key structure
    void ParseBlobKey(const string& key_str);

    /// Generate blob key string
    ///
    /// Please note that "id" is an integer issued by the NetCache server.
    /// Clients should not use this function with custom ids.
    /// Otherwise it may disrupt the interserver communication.
    static
    void GenerateBlobKey(string*        key,
                         unsigned int   id,
                         const string&  host,
                         unsigned short port);

    /// Parse blob key, extract id
    static
    unsigned int GetBlobId(const string& key_str);

public:
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< BLOB id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};



/// IReader/IWriter implementation
///
/// @internal
///
class NCBI_XCONNECT_EXPORT CNetCacheSock_RW : public CSocketReaderWriter
{
public:
    typedef CSocketReaderWriter TParent;

    explicit CNetCacheSock_RW(CSocket* sock);
    explicit CNetCacheSock_RW(CSocket* sock, size_t blob_size);

    virtual ~CNetCacheSock_RW();

    /// Take socket ownership
    void OwnSocket();

    /// Set pointer on parent object responsible for socket pooling
    /// If parent set RW will return socket to its parent for reuse.
    /// If parent not set, socket is closed.
    void SetSocketParent(CNetServiceClient* parent);

    /// Access to CSocketReaderWriter::m_Sock
    CSocket& GetSocket() { _ASSERT(m_Sock); return *m_Sock; }

    /*
    /// Turn ON blob size control.
    /// When turned on IReader returns EOF if all BLOB bytes being
    /// read from the socket (without expecting the server to close it)
    void SetBlobSize(size_t blob_size);
    */

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

    void FinishTransmission();

    /// Set BLOB diagnostic comments
    void SetBlobComment(const string& comment)
    {
        m_BlobComment = comment;
    }
private:
    CNetCacheSock_RW(const CNetCacheSock_RW&);
    CNetCacheSock_RW& operator=(const CNetCacheSock_RW&);

protected:
    CNetServiceClient*  m_Parent;
    /// Flag that EOF is controlled not on the socket level (sock close)
    /// but based on the BLOB size
    bool                m_BlobSizeControl;
    /// Remaining BLOB size to be read
    size_t              m_BlobBytesToRead;
    /// diagnostic comments (logging)
    string              m_BlobComment;
};


/// IWriter with error checking
///
/// @internal
class NCBI_XCONNECT_EXPORT CNetCache_WriterErrCheck
					: public CTransmissionWriter
{
public:
    typedef CTransmissionWriter TParent;

    CNetCache_WriterErrCheck(CNetCacheSock_RW* wrt,
                             EOwnership        own_writer,
                             CNetCacheClient*  parent,
                             CTransmissionWriter::ESendEofPacket send_eof =
                                 CTransmissionWriter::eDontSendEofPacket);
    virtual ~CNetCache_WriterErrCheck();

    virtual
    ERW_Result Write(const void* buf,
                     size_t      count,
                     size_t*     bytes_written = 0);

    virtual
    ERW_Result Flush(void);

protected:
    void CheckInputMessage();

protected:
    CNetCacheSock_RW*  m_RW;
    CNetCacheClient*   m_NC_Client;
};


extern NCBI_XCONNECT_EXPORT const char* kNetCacheDriverName;


//extern "C" {

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcache(
     CPluginManager<CNetCacheClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetCacheClient>::EEntryPointRequest method);


//} // extern C


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_CLIENT__HPP */
