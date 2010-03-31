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

#include <connect/services/netcache_api_expt.hpp>

#include <connect/server.hpp>
#include <connect/server_monitor.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>

#include "nc_storage.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCServerStat;
class CNetCacheDApp;
class CNCMessageHandler;


/// Name of the cache that can be used in CNetCacheServer::GetBlobStorage()
/// to acquire instance used for NetCache (as opposed to ICache instances).
static const string kNCDefCacheName = "nccache";


struct SConstCharCompare
{
    bool operator() (const char* left, const char* right) const;
};


/// Helper class to maintain different instances of server statistics in
/// different threads.
class CNCServerStat_Getter : public CNCTlsObject<CNCServerStat_Getter,
                                                 CNCServerStat>
{
    typedef CNCTlsObject<CNCServerStat_Getter, CNCServerStat>  TBase;

public:
    CNCServerStat_Getter(void);
    ~CNCServerStat_Getter(void);

    /// Part of interface required by CNCTlsObject<>
    CNCServerStat* CreateTlsObject(void);
    static void DeleteTlsObject(void* obj_ptr);
};

/// Class collecting statistics about whole server.
class CNCServerStat
{
public:
    /// Add finished command
    ///
    /// @param cmd
    ///   Command name - should be string constant living through all time
    ///   application is running.
    /// @param cmd_span
    ///   Time command was executed
    /// @param cmd_status
    ///   Resultant status code of the command
    static void AddFinishedCmd(const char* cmd,
                               double      cmd_span,
                               int         cmd_status);
    /// Add closed connection
    ///
    /// @param conn_span
    ///   Time which connection stayed opened
    static void AddClosedConnection(double conn_span);
    /// Add opened connection
    static void AddOpenedConnection(void);
    /// Remove opened connection (after OnOverflow was called)
    static void RemoveOpenedConnection(void);
    /// Add connection that will be closed because of maximum number of
    /// connections exceeded.
    static void AddOverflowConnection(void);
    /// Add command terminated because of timeout
    static void AddTimedOutCommand(void);

    /// Print statistics
    static void Print(CPrintTextProxy& proxy);

private:
    friend class CNCServerStat_Getter;


    typedef CNCStatFigure<double>                           TSpanValue;
    typedef map<const char*, TSpanValue, SConstCharCompare> TCmdsSpansMap;
    typedef map<int, TCmdsSpansMap>                         TStatCmdsSpansMap;


    CNCServerStat(void);
    /// Get pointer to element in given span map related to given command name.
    /// If there's no such element then it's created and initialized.
    static TCmdsSpansMap::iterator x_GetSpanFigure(TCmdsSpansMap&  cmd_span_map,
                                                   const char*     cmd);
    /// Collect all statistics from this instance to another
    void x_CollectTo(CNCServerStat* other);


    /// Mutex for working with statistics
    CSpinLock             m_ObjLock;
    /// Number of connections opened
    Uint8                 m_OpenedConns;
    /// Number of connections closed because of maximum number of opened
    /// connections limit.
    Uint8                 m_OverflowConns;
    /// Sum of times all connections stayed opened
    CNCStatFigure<double> m_ConnSpan;
    /// Maximum time one command was executed
    CNCStatFigure<double> m_CmdSpan;
    /// Maximum time of each command type executed
    TStatCmdsSpansMap     m_CmdSpanByCmd;
    /// Number of commands terminated because of command timeout
    Uint8                 m_TimedOutCmds;

    /// Object differentiating statistics instances over threads
    static CNCServerStat_Getter sm_Getter;
    /// All instances of statistics used in application
    static CNCServerStat        sm_Instances[kNCMaxThreadsCnt];
};

/// Types of "request-start" and "request-stop" messages logging
enum ENCLogCmdsType {
    eLogAllCmds,    ///< Always log these messages
    eNoLogHasb,     ///< Log these messages only if command is not HASB
    eNoLogCmds      ///< Never log these messages
};

/// Policy for accepting passwords for reading and writing blobs
enum ENCBlobPassPolicy {
    eNCBlobPassAny,     ///< Both blobs with password and without are accepted
    eNCOnlyWithPass,    ///< Only blobs with password are accepted
    eNCOnlyWithoutPass  ///< Only blobs without password are accepted
};

/// Netcache server
class CNetCacheServer : public CServer
{
public:
    /// Create server
    ///
    /// @param do_reinit
    ///   TRUE if reinitialization should be forced (was requested in command
    ///   line)
    CNetCacheServer(bool do_reinit);
    virtual ~CNetCacheServer();

    /// Check if server was asked to stop
    virtual bool ShutdownRequested(void);
    /// Get signal number by which server was asked to stop
    int GetSignalCode(void) const;
    /// Ask server to stop after receiving given signal
    void RequestShutdown(int signal = 0);

    /// Get monitor for the server
    CServer_Monitor* GetServerMonitor(void);
    /// Get storage for the given cache name.
    /// If cache name is empty then it should be main storage for NetCache,
    /// otherwise it's storage for ICache implementation
    CNCBlobStorage* GetBlobStorage(const string& cache_name);
    /// Get next blob id to incorporate it into generated blob key
    int GetNextBlobId(void);
    /// Get configuration registry for the server
    const IRegistry& GetRegistry(void);
    /// Get server host
    const string& GetHost(void) const;
    /// Get server port
    unsigned int GetPort(void) const;
    /// Get inactivity timeout for each connection
    unsigned GetInactivityTimeout(void) const;
    /// Get timeout for each executed command
    unsigned GetCmdTimeout(void) const;
    /// Get type of logging all commands starting and stopping
    ENCLogCmdsType GetLogCmdsType(void) const;
    /// Get name of client that should be used for administrative commands
    const string& GetAdminClient(void) const;
    /// Get policy for accepting blobs with/without passwords
    ENCBlobPassPolicy GetBlobPassPolicy(void) const;
    /// Create CTime object in fast way (via CFastTime)
    CTime GetFastTime(void);

    /// Block all storages available in the server
    void BlockAllStorages(void);
    /// Unblock all storages available in the server
    void UnblockAllStorages(void);
    /// Check if server is ready for re-reading of the configuration.
    /// The server is ready when all storages are blocked and all operations in
    /// them are stopped.
    bool CanReconfigure(void);
    /// Re-read configuration of the server and all storages
    void Reconfigure(void);

    /// Print full server statistics into stream
    void PrintServerStats(CNcbiIostream* ios);

private:
    /// Read integer configuration value from server's registry
    int    x_RegReadInt   (const IRegistry& reg,
                           const char*      value_name,
                           int              def_value);
    /// Read boolean configuration value from server's registry
    bool   x_RegReadBool  (const IRegistry& reg,
                           const char*      value_name,
                           bool             def_value);
    /// Read string configuration value from server's registry
    string x_RegReadString(const IRegistry& reg,
                           const char*      value_name,
                           const string&    def_value);

    /// Read server parameters from application's configuration file
    void x_ReadServerParams(void);
    /// Remove all old storages (that can be left from previous
    /// re-configuration) and check if some active ones were removed from
    /// list of sections in application's configuration file.
    void x_CleanExistingStorages(const list<string>& ini_sections);
    /// Read storages configuration from registry and adjust list of active
    /// storages accordingly.
    ///
    /// @param reg
    ///   Registry to read configuration for storages
    /// @param do_reinit
    ///   Flag if all storages should be forced to reinitialize
    void x_ConfigureStorages(CNcbiRegistry& reg, bool do_reinit);

    /// Print full server statistics into stream or diagnostics
    void x_PrintServerStats(CPrintTextProxy& proxy);


    typedef map<string, AutoPtr<CNCBlobStorage> >   TStorageMap;
    typedef list< AutoPtr<CNCBlobStorage> >         TStorageList;


    /// Host name where server runs
    string                         m_Host;
    /// Port where server runs
    unsigned                       m_Port;
    /// Time when this server instance was started
    CTime                          m_StartTime;
    // Some variable that should be here because of CServer requirements
    STimeout                       m_ServerAcceptTimeout;
    /// Flag that server received a shutdown request
    bool                           m_Shutdown;
    /// Signal which caused the shutdown request
    int                            m_Signal;
    /// Time to wait for the client on the connection (seconds)
    unsigned                       m_InactivityTimeout;
    /// Maximum time span which each command can work in
    unsigned                       m_CmdTimeout;
    /// Type of logging all commands starting and stopping
    ENCLogCmdsType                 m_LogCmdsType;
    /// Policy for accepting blobs with/without passwords
    ENCBlobPassPolicy              m_PassPolicy;
    /// Flag showing if configuration parameter to reinitialize broken storages
    /// was set.
    bool                           m_IsReinitBadDB;
    /// Name of client that should be used for administrative commands
    string                         m_AdminClient;
    /// Quick local timer
    CFastLocalTime                 m_FastTime;
    /// Map of strings to blob storages
    TStorageMap                    m_StorageMap;
    /// List of storages that were excluded from configuration file and that
    /// will be deleted on next re-configuration attempt.
    TStorageList                   m_OldStorages;
    /// Counter for blob id
    CAtomicCounter                 m_BlobIdCounter;
    /// Server monitor
    CServer_Monitor                m_Monitor;
};


/// NetCache daemon application
class CNetCacheDApp : public CNcbiApplication
{
protected:
    virtual void Init(void);
    virtual int  Run (void);
};



inline bool
SConstCharCompare::operator() (const char* left, const char* right) const
{
    return NStr::strcmp(left, right) < 0;
}


inline
CNCServerStat_Getter::CNCServerStat_Getter(void)
{
    TBase::Initialize();
}

inline
CNCServerStat_Getter::~CNCServerStat_Getter(void)
{
    TBase::Finalize();
}

inline CNCServerStat*
CNCServerStat_Getter::CreateTlsObject(void)
{
    return &CNCServerStat::sm_Instances[
                                     g_GetNCThreadIndex() % kNCMaxThreadsCnt];
}

inline void
CNCServerStat_Getter::DeleteTlsObject(void*)
{}


inline void
CNCServerStat::AddClosedConnection(double conn_span)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_ConnSpan.AddValue(conn_span);
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddOpenedConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OpenedConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::RemoveOpenedConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    --stat->m_OpenedConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddOverflowConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OverflowConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddTimedOutCommand(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_TimedOutCmds;
    stat->m_ObjLock.Unlock();
}



inline bool
CNetCacheServer::ShutdownRequested(void)
{
    return m_Shutdown;
}

inline int
CNetCacheServer::GetSignalCode(void) const
{
    return m_Signal;
}

inline int
CNetCacheServer::GetNextBlobId(void)
{
    return int(m_BlobIdCounter.Add(1));
}

inline unsigned int
CNetCacheServer::GetCmdTimeout(void) const
{
    return m_CmdTimeout;
}

inline void
CNetCacheServer::RequestShutdown(int sig)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_Signal = sig;
    }
}

inline CServer_Monitor*
CNetCacheServer::GetServerMonitor(void)
{
    return &m_Monitor;
}

inline const IRegistry&
CNetCacheServer::GetRegistry(void)
{
    return CNcbiApplication::Instance()->GetConfig();
}

inline const string&
CNetCacheServer::GetHost(void) const
{
    return m_Host;
}

inline unsigned int
CNetCacheServer::GetPort(void) const
{
    return m_Port;
}

inline unsigned int
CNetCacheServer::GetInactivityTimeout(void) const
{
    return m_InactivityTimeout;
}

inline ENCLogCmdsType
CNetCacheServer::GetLogCmdsType(void) const
{
    return m_LogCmdsType;
}

inline ENCBlobPassPolicy
CNetCacheServer::GetBlobPassPolicy(void) const
{
    return m_PassPolicy;
}

inline const string&
CNetCacheServer::GetAdminClient(void) const
{
    return m_AdminClient;
}

inline CTime
CNetCacheServer::GetFastTime(void)
{
    return m_FastTime.GetLocalTime();
}

inline CNCBlobStorage*
CNetCacheServer::GetBlobStorage(const string& cache_name)
{
    TStorageMap::iterator it = m_StorageMap.find(cache_name);
    if (it == m_StorageMap.end()) {
        return NULL;
    }
    return it->second.get();
}

inline void
CNetCacheServer::PrintServerStats(CNcbiIostream* ios)
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintStream, ios);
    x_PrintServerStats(proxy);
}

END_NCBI_SCOPE

#endif /* NETCACHED__HPP */
