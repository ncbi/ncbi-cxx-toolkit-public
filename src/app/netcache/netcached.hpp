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
class CCounterGuard
{
public:
    CCounterGuard(CAtomicCounter& counter) :
        m_Counter(counter)
    {
        m_Counter.Add(1);
    }
    ~CCounterGuard() {
        m_Counter.Add(-1);
    }
private:
    CAtomicCounter& m_Counter;
};


struct STraceEvent
{
    STraceEvent(double t, const char*m) :
        time_mark(t), message(m)
    { }
    double time_mark;
    string message;
};

/// @internal
///
/// Netcache server side request statistics
///
struct SNetCache_RequestStat
{
    CTime        conn_time;    ///< request incoming time in seconds
    unsigned     req_code;     ///< 'P' put, 'G' get
    SBDB_CacheUnitStatistics::EErrGetPut op_code; /// error opcode for BDB
    CStopWatch   elapsed_watch; /// watch for request elapsed time
    double       elapsed;      ///< total time in seconds to process request
    double       comm_elapsed; ///< time spent reading/sending data
    double       lock_elapsed; ///< time to wait until blob is unlocked
    double       db_elapsed;   ///< time to store/retrieve data
    unsigned     db_accesses;  ///< number of db hits
    size_t       blob_size;    ///< BLOB size
    string       peer_address; ///< Client's IP address
    unsigned     io_blocks;    ///< Number of IO blocks translated
    // fields from NetCache_RequestInfo
    string request; ///< saved request
    string type;    ///< NC or IC
    string blob_id; ///< NC blob id or IC synthetic id
    string details; ///< readable cache id, blob id etc.
#ifdef _DEBUG
    vector<STraceEvent> events;
    void AddEvent(const char* m) {
        double t = elapsed_watch.Elapsed();
        events.push_back(STraceEvent(t, m));
    }
#else
    void AddEvent(const char*) {}
#endif

    void InitSession(const CTime& t, const string& pa) {
        conn_time = t;
        peer_address = pa;
        // get only host name
        string::size_type offs = peer_address.find_first_of(":");
        if (offs != string::npos) {
            peer_address.erase(offs, peer_address.length());
        }
    }
    void InitRequest() {
        elapsed_watch.Restart();
#ifdef _DEBUG
        events.clear();
#endif
        req_code = '?';
        elapsed = 0;
        blob_size = 0;
        comm_elapsed = 0;
        lock_elapsed = 0;
        db_elapsed = 0;
        db_accesses = 0;
        io_blocks = 0;

        type = "Unknown";
        blob_id.clear();
        details.clear();
    }
    void EndRequest() {
        elapsed_watch.Stop();
        elapsed = elapsed_watch.Elapsed();
    }
};


/// @internal
///
/// Guard to record elapsed times in the presence of exceptions
class CTimeGuard {
public:
    CTimeGuard(double& elapsed, CStopWatch::EStart state = CStopWatch::eStart) :
        m_StopWatch(state), m_State(state), m_Elapsed(&elapsed), m_Stat(0)
    { }
    CTimeGuard(double& elapsed, SNetCache_RequestStat* stat,
        CStopWatch::EStart state = CStopWatch::eStart) :
        m_StopWatch(state), m_State(state), m_Elapsed(&elapsed), m_Stat(0) // DEBUG: turn off detailed stat
    {
        if (m_Stat)
            m_Stat->AddEvent("Start");
    }
    ~CTimeGuard() {
        Release();
    }
    void Start() {
        m_StopWatch.Start();
        m_State = CStopWatch::eStart;
    }
    void Stop() {
        m_StopWatch.Stop();
        if (m_State == CStopWatch::eStart) {
            if (m_Stat)
                m_Stat->AddEvent("Stop");
        }
        m_State = CStopWatch::eStop;
    }
    void Release() {
        if (!m_Elapsed) return;
        Stop();
        *m_Elapsed += m_StopWatch.Elapsed();
        m_Elapsed = NULL;
    }
private:
    CStopWatch m_StopWatch;
    CStopWatch::EStart m_State;
    double*    m_Elapsed;
    SNetCache_RequestStat*  m_Stat;
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

    ///
    CAtomicCounter& GetRequestCounter() { return m_PendingRequests; }

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

    bool IsLogForced() const;
    bool IsLog() const;

private:
    /// Read the registry for icache_XXXX entries
    void x_GetICacheNames(TLocalCacheMap* cache_map);

private:
    /// Host name and port where server runs
    string           m_Host;
    unsigned         m_Port;

    /// ID counter
    unsigned         m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>    m_UsedIds;
    CBDB_Cache*      m_Cache;
    /// Flags that server received a shutdown request
    volatile bool    m_Shutdown; 
    /// Matches signal, if server got one
    int              m_Signal;
    /// Time to wait for the client (seconds)
    unsigned         m_InactivityTimeout;
    /// Logging ON/OFF
    bool             m_LogFlag;       ///< From config file
    bool             m_LogForcedFlag; ///< Set by command

    /// Accept timeout for threaded server
    STimeout         m_ThrdSrvAcceptTimeout;
    /// Quick local timer
    CFastLocalTime   m_LocalTimer;
    /// Configuration
    const IRegistry& m_Reg;

    /// Map of local ICache instances
    TLocalCacheMap   m_LocalCacheMap;
    CFastMutex       m_LocalCacheMap_Lock;

    /// Session management
    CRef<CSessionManagementThread>
                     m_SessionMngThread;

    /// Monitor
    CServer_Monitor  m_Monitor;

    /// Pending requests
    CAtomicCounter   m_PendingRequests;
};



END_NCBI_SCOPE

#endif /* NETCACHED__HPP */
