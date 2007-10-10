#ifndef NETCACHED__HPP
#define NETCACHED__HPP

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
 * File Description: Network cache daemon
 *
 */

#include <util/logrotate.hpp>

#include <connect/threaded_server.hpp>
#include <connect/server_monitor.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_client.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/cache/icache.hpp>
#include <util/cache/icache_clean_thread.hpp>

#include "smng_thread.hpp"

BEGIN_NCBI_SCOPE


/// request type
///
/// @internal
typedef enum {
    eNCServer_Unknown,
    eNCServer_NetCache,
    eNCServer_Session,
    eNCServer_ICache
} EServed_RequestType;

/// NC requests
///
/// @internal
typedef enum {
    eError,
    ePut,
    ePut2,   ///< PUT v.2 transmission protocol
    ePut3,   ///< PUT v.2 transmission protocol with commit confirmation
    eGet,
    eShutdown,
    eVersion,
    eRemove,
    eRemove2, ///< Remove with confirmation
    eLogging,
    eGetConfig,
    eGetStat,
    eIsLock,
    eGetBlobOwner,
    eDropStat
} ENC_RequestType;


/// IC requests
///
/// @internal
typedef enum {
    eIC_Error,
    eIC_SetTimeStampPolicy,
    eIC_GetTimeStampPolicy,
    eIC_SetVersionRetention,
    eIC_GetVersionRetention,
    eIC_GetTimeout,
    eIC_IsOpen,
    eIC_Store,
    eIC_StoreBlob,
    eIC_GetSize,
    eIC_GetBlobOwner,
    eIC_Read,
    eIC_Remove,
    eIC_RemoveKey,
    eIC_GetAccessTime,
    eIC_HasBlobs,
    eIC_Purge1
} EIC_RequestType;



/// Parsed NetCache request 
///
/// @internal
struct SNC_Request
{
    ENC_RequestType req_type;
    unsigned int    timeout;
    string          req_id;
    string          err_msg;
    bool            no_lock;

    void Init() 
    {
        req_type = eError;
        timeout = 0;
        req_id.erase(); err_msg.erase();
        no_lock = false;
    }
};

/// Parsed IC request
///
/// @internal 
struct SIC_Request
{
    EIC_RequestType req_type;
    string          cache_name;
    string          key;
    int             version;
    string          subkey;
    string          err_msg;

    unsigned        i0;
    unsigned        i1;
    unsigned        i2;

    void Init() 
    {
        req_type = eIC_Error;
        cache_name.erase(); key.erase(); subkey.erase();
        err_msg.erase();
        version = 0;
        i0 = i1 = i2 = 0;
    }
};



/// Thread specific data for threaded server
///
/// @internal
///
struct SNC_ThreadData
{
    string      auth;                           ///< Authentication string
    AutoPtr<char, ArrayDeleter<char> > buffer; ///< operation buffer
    size_t      buffer_size;
    string      tmp;

    SNC_ThreadData(size_t size)
        : buffer(new char[size + 256]), buffer_size(size)
    {}
};


/// @internal
///
/// Netcache server side request statistics
///
struct NetCache_RequestStat
{
    CTime        conn_time;    ///< request incoming time in seconds
    unsigned     req_code;     ///< 'P' put, 'G' get
    size_t       blob_size;    ///< BLOB size
    double       elapsed;      ///< total time in seconds to process request
    double       comm_elapsed; ///< time spent reading/sending data
    string       peer_address; ///< Client's IP address
    unsigned     io_blocks;    ///< Number of IO blocks translated
};


/// @internal
class CNetCacheLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetCacheLogStream(const string&    filename, 
                       CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }
};


/// @internal
///
/// Netcache logger
///
class CNetCache_Logger
{
public:
    CNetCache_Logger(const string&    filename, 
                     CNcbiStreamoff   limit)
      : m_Log(filename, limit)
    {}

    void Put(const string&               auth, 
             const NetCache_RequestStat& stat,
             const string&               blob_key);

    void Rotate() { m_Log.Rotate(); }
private:
    CNetCacheLogStream m_Log;
};

class CBDB_Cache;

/// Netcache threaded server 
///
/// @internal
class CNetCacheServer : public CThreadedServer
{
public:
    /// Named local cache instances
    typedef map<string, ICache*>   TLocalCacheMap;
    typedef CPluginManager<ICache> TCachePM;

public:
    CNetCacheServer(unsigned int     port,
                    bool             use_hostname,
                    CBDB_Cache*      cache,
                    unsigned         max_threads,
                    unsigned         init_threads,
                    unsigned         network_timeout,
                    bool             is_log,
                    const IRegistry& reg);

    virtual ~CNetCacheServer();

    unsigned int GetTLS_Size() const { return m_TLS_Size; }
    void SetTLS_Size(unsigned int size) { m_TLS_Size = size; }

    /// Take request code from the socket and process the request
    virtual void Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

        
    void SetShutdownFlag() { if (!m_Shutdown) m_Shutdown = true; }
    
    /// Override some parent parameters
    virtual void SetParams();

    /// Mount local cache instances
    void MountICache(CConfig& conf, 
                     CPluginManager<ICache>& pm_cache);

    void StartSessionManagement(unsigned session_shutdown_timeout);
    void StopSessionManagement();

    /// Get server-monitor class
    CServer_Monitor& GetMonitor() { return m_Monitor; }

protected:
    virtual void ProcessOverflow(SOCK sock);

private:

    /// Process NetCache request
    void ProcessNC(CSocket&              socket,
                   string&               request,
                   SNC_Request&          req,
                   SNC_ThreadData&       tdata,
                   NetCache_RequestStat& stat);

    /// Process ICache request
    void ProcessIC(CSocket&              socket,
                   string&               request,
                   SIC_Request&          req,
                   SNC_ThreadData&       tdata,
                   NetCache_RequestStat& stat);

    void ProcessSM(CSocket& socket, string& req);

    // IC requests

    void Process_IC_SetTimeStampPolicy(ICache&               ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req,
                                       SNC_ThreadData&       tdata);

    void Process_IC_GetTimeStampPolicy(ICache&               ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req,
                                       SNC_ThreadData&       tdata);

    void Process_IC_SetVersionRetention(ICache&              ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req,
                                       SNC_ThreadData&       tdata);
    void Process_IC_GetVersionRetention(ICache&              ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req,
                                       SNC_ThreadData&       tdata);

    void Process_IC_GetTimeout(ICache&              ic,
                               CSocket&             sock, 
                               SIC_Request&         req,
                               SNC_ThreadData&      tdata);

    void Process_IC_IsOpen(ICache&              ic,
                           CSocket&             sock, 
                           SIC_Request&         req,
                           SNC_ThreadData&      tdata);

    void Process_IC_Store(ICache&              ic,
                          CSocket&             sock, 
                          SIC_Request&         req,
                          SNC_ThreadData&      tdata);
    void Process_IC_StoreBlob(ICache&              ic,
                              CSocket&             sock,
                              SIC_Request&         req,
                              SNC_ThreadData&      tdata);

    void Process_IC_GetSize(ICache&              ic,
                            CSocket&             sock, 
                            SIC_Request&         req,
                            SNC_ThreadData&      tdata);

    void Process_IC_GetBlobOwner(ICache&              ic,
                                 CSocket&             sock, 
                                 SIC_Request&         req,
                                 SNC_ThreadData&      tdata);

    void Process_IC_Read(ICache&              ic,
                         CSocket&             sock, 
                         SIC_Request&         req,
                         SNC_ThreadData&      tdata);

    void Process_IC_Remove(ICache&              ic,
                           CSocket&             sock, 
                           SIC_Request&         req,
                           SNC_ThreadData&      tdata);

    void Process_IC_RemoveKey(ICache&              ic,
                              CSocket&             sock, 
                              SIC_Request&         req,
                              SNC_ThreadData&      tdata);

    void Process_IC_GetAccessTime(ICache&              ic,
                                  CSocket&             sock, 
                                  SIC_Request&         req,
                                  SNC_ThreadData&      tdata);

    void Process_IC_HasBlobs(ICache&              ic,
                             CSocket&             sock, 
                             SIC_Request&         req,
                             SNC_ThreadData&      tdata);

    void Process_IC_Purge1(ICache&              ic,
                            CSocket&             sock, 
                            SIC_Request&         req,
                            SNC_ThreadData&      tdata);
    // NC requests


    /// Process "PUT" request
    void ProcessPut(CSocket&              sock, 
                    SNC_Request&          req,
                    SNC_ThreadData&       tdata,
                    NetCache_RequestStat& stat);

    /// Process "PUT2" request
    void ProcessPut2(CSocket&              sock, 
                     SNC_Request&          req,
                     SNC_ThreadData&       tdata,
                     NetCache_RequestStat& stat);

    /// Process "PUT3" request
    void ProcessPut3(CSocket&              sock, 
                     SNC_Request&          req,
                     SNC_ThreadData&       tdata,
                     NetCache_RequestStat& stat);

    /// Process "GET" request
    void ProcessGet(CSocket&              sock, 
                    const SNC_Request&    req,
                    SNC_ThreadData&       tdata,
                    NetCache_RequestStat& stat);

    /// Process "GET2" request
    void ProcessGet2(CSocket&              sock,
                     const SNC_Request&    req,
                     SNC_ThreadData&       tdata,
                     NetCache_RequestStat& stat);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const SNC_Request& req);

    /// Process "LOG" request
    void ProcessLog(CSocket& sock, const SNC_Request& req);

    /// Process "REMOVE" request
    void ProcessRemove(CSocket& sock, const SNC_Request& req);

    /// Process "REMOVE2" request
    void ProcessRemove2(CSocket& sock, const SNC_Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown(CSocket& sock);

    /// Process "GETCONF" request
    void ProcessGetConfig(CSocket& sock);

    /// Process "DROPSTAT" request
    void ProcessDropStat(CSocket& sock);

    /// Process "GBOW" request
    void ProcessGetBlobOwner(CSocket& sock, const SNC_Request& req);

    /// Process "GETSTAT" request
    void ProcessGetStat(CSocket& sock, const SNC_Request& req);

    /// Process "ISLK" request
    void ProcessIsLock(CSocket& sock, const SNC_Request& req);


    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);

    /// Read buffer from the socket.
    // bool ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock,
                    IReader* rdr, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    bool ReadBuffer(CSocket& sock,
                    IReader* rdr, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length,
                    size_t   expected_size);

    void ParseRequestNC(const string& reqstr, SNC_Request* req);

    void ParseRequestIC(const string& reqstr, SIC_Request* req);


    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    /// Generate unique system-wide request id
    void GenerateRequestId(const SNC_Request& req, 
                           string*            id_str,
                           unsigned int*      transaction_id);


    /// Get logger instance
    CNetCache_Logger* GetLogger();
    /// TRUE if logging is ON
    bool IsLog() const;

    ICache* GetLocalCache(const string& cache_name);
    static
    bool WaitForReadSocket(CSocket& sock, unsigned time_to_wait);


private:
    bool x_CheckBlobId(CSocket&       sock,
                       CNetCache_Key* blob_id, 
                       const string&  blob_key);

    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    void x_CreateLog();

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SNC_ThreadData* x_GetThreadData(); 

    /// Register protocol error (statistics)
    void x_RegisterProtocolErr(ENC_RequestType   req_type,
                               const string&     auth);
    void x_RegisterInternalErr(ENC_RequestType   req_type,
                               const string&     auth);
    void x_RegisterNoBlobErr(ENC_RequestType   req_type,
                               const string&     auth);

    /// Register exception error (statistics)
    void x_RegisterException(ENC_RequestType           req_type,
                             const string&             auth,
                             const CNetCacheException& ex);

    /// Register exception error (statistics)
    void x_RegisterException(ENC_RequestType           req_type,
                             const string&             auth,
                             const CNetServiceException& ex);

    /// Read the registry for icache_XXXX entries
    void x_GetICacheNames(TLocalCacheMap* cache_map);

private:
    /// Host name where server runs
    string             m_Host;
    /// ID counter
    unsigned           m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>      m_UsedIds;
    CBDB_Cache*        m_Cache;
    /// Flags that server received a shutdown request
    volatile bool      m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    /// Log writer
    auto_ptr<CNetCache_Logger>  m_Logger;
    /// Logging ON/OFF
    bool                        m_LogFlag;
    unsigned int                m_TLS_Size;
    
    /// Accept timeout for threaded server
    STimeout                     m_ThrdSrvAcceptTimeout;
    /// Quick local timer
    CFastLocalTime               m_LocalTimer;
    /// Configuration
    const IRegistry&             m_Reg;

    /// Map of local ICache instances
    TLocalCacheMap                  m_LocalCacheMap;
    CFastMutex                      m_LocalCacheMap_Lock;

    /// Session management
    CRef<CSessionManagementThread>  m_SessionMngThread;

    /// Monitor
    CServer_Monitor                 m_Monitor;
};



END_NCBI_SCOPE

#endif
