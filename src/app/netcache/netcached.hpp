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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */

#include <util/logrotate.hpp>

#include <connect/server.hpp>
#include <connect/server_monitor.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_client.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/cache/icache.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <bdb/bdb_blobcache.hpp>

#include "smng_thread.hpp"

#include <connect/server.hpp>

BEGIN_NCBI_SCOPE


/// @internal
///
/// Netcache request info for logging and monitoring
///
struct NetCache_RequestInfo
{
    string type;    ///< NC or IC
    string blob_id; ///< NC blob id or IC synthetic id
    string details; ///< readable cache id, blob id etc.
    void Init() {
        type = "Unknown";
        blob_id = "";
        details = "";
    }
};

/// @internal
///
/// Netcache server side request statistics
///
struct NetCache_RequestStat
{
    CTime        conn_time;    ///< request incoming time in seconds
    unsigned     req_code;     ///< 'P' put, 'G' get
    SBDB_CacheUnitStatistics::EErrGetPut op_code; /// error opcode for BDB
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
class CNetCacheServer : public CServer
{
public:
    /// Named local cache instances
    typedef map<string, ICache*>   TLocalCacheMap;
    typedef CPluginManager<ICache> TCachePM;

public:
    CNetCacheServer(unsigned int     port,
                    bool             use_hostname,
                    CBDB_Cache*      cache,
                    unsigned         network_timeout,
                    bool             is_log,
                    const IRegistry& reg);

    virtual ~CNetCacheServer();

    /// Take request code from the socket and process the request
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

    int GetSignalCode() { return m_Signal; }
        
    void SetShutdownFlag(int sig=0);
    
    void SetBufferSize(unsigned buf_size);
    /// Mount local cache instances
    void MountICache(CConfig& conf, 
                     CPluginManager<ICache>& pm_cache);

    // Session management control
    void StartSessionManagement(unsigned session_shutdown_timeout);
    void StopSessionManagement();

    // Session management
    bool IsManagingSessions();
    /// @returns true if session has been registered,
    ///          false if session management cannot do it (shutdown)
    bool RegisterSession(const string& host, unsigned pid);
    void UnRegisterSession(const string& host, unsigned pid);

    void SetMonitorSocket(CSocket& socket);
    bool IsMonitoring();
    void MonitorPost(const string& msg);

    /// Get server-monitor class
    CServer_Monitor& GetMonitor() { return m_Monitor; }

    ///////////////////////////////////////////////////////////////////
    // Service for handlers
    ///
    static
    void WriteBuf(CSocket& sock,
                  char*    buf,
                  size_t   bytes);
    /// Reply to the client

    static
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    void SwitchLog(bool on);

    ICache* GetLocalCache(const string& cache_name);
    unsigned GetTimeout();

    /// Register protocol error (statistics)
    void RegisterProtocolErr(SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth);
    void RegisterInternalErr(SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth);
    void RegisterNoBlobErr(SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth);
    /// Register exception error (statistics)
    void RegisterException(SBDB_CacheUnitStatistics::EErrGetPut op,
                             const string&             auth,
                             const CNetServiceException& ex);

    /// Register exception error (statistics)
    void RegisterException(SBDB_CacheUnitStatistics::EErrGetPut op,
                             const string&             auth,
                             const CNetCacheException& ex);


    const IRegistry& GetRegistry();

    CBDB_Cache* GetCache();

    const string& GetHost();
    unsigned GetPort();

    unsigned GetInactivityTimeout(void);

    CFastLocalTime& GetTimer();

    /// Get logger instance
    CNetCache_Logger* GetLogger();
    /// TRUE if logging is ON
    bool IsLog() const;

private:
    void x_CreateLog();

    /// Read the registry for icache_XXXX entries
    void x_GetICacheNames(TLocalCacheMap* cache_map);

private:
    /// Host name and port where server runs
    string             m_Host;
    unsigned           m_Port;

    /// ID counter
    unsigned           m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>      m_UsedIds;
    CBDB_Cache*        m_Cache;
    /// Flags that server received a shutdown request
    volatile bool      m_Shutdown; 
    /// Matches signal, if server got one
    int                m_Signal;
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    /// Log writer
    auto_ptr<CNetCache_Logger>  m_Logger;
    /// Logging ON/OFF
    bool                        m_LogFlag;
    
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

#endif /* NETCACHED__HPP */
