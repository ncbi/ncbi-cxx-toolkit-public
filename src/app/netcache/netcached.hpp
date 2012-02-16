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
#include <util/thread_pool.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>

#include "nc_utils.hpp"
#include "periodic_sync.hpp"

#ifndef NCBI_OS_MSWIN
# include <sys/time.h>
#endif



BEGIN_NCBI_SCOPE


class CNetCacheDApp;
class CNCMessageHandler;


static const char* const kNCPeerClientName = "nc_peer";


/// Policy for accepting passwords for reading and writing blobs
enum ENCBlobPassPolicy {
    eNCBlobPassAny,     ///< Both blobs with password and without are accepted
    eNCOnlyWithPass,    ///< Only blobs with password are accepted
    eNCOnlyWithoutPass  ///< Only blobs without password are accepted
};


struct SNCSpecificParams : public CObject
{
    bool  disable;
    bool  prolong_on_read;
    bool  srch_on_read;
    bool  fast_on_main;
    ENCBlobPassPolicy pass_policy;
    Uint4 conn_timeout;
    Uint4 cmd_timeout;
    Uint4 blob_ttl;
    Uint4 ver_ttl;
    Uint2 ttl_unit;
    Uint1 quorum;

    virtual ~SNCSpecificParams(void);
};

///
typedef  map<string, string>  TStringMap;


/// Netcache server
class CNetCacheServer : public CServer
{
public:
    CNetCacheServer(void);
    virtual ~CNetCacheServer();

    /// Initialize server
    ///
    /// @param do_reinit
    ///   TRUE if reinitialization should be forced (was requested in command
    ///   line)
    bool Initialize(bool do_reinit);
    void Finalize(void);

    /// Check if server was asked to stop
    virtual bool ShutdownRequested(void);
    /// Get signal number by which server was asked to stop
    int GetSignalCode(void) const;
    /// Ask server to stop after receiving given signal
    void RequestShutdown(int signal = 0);

    ///
    const SNCSpecificParams* GetAppSetup(const TStringMap& client_params);
    /// Get inactivity timeout for each connection
    unsigned GetDefConnTimeout(void) const;
    int GetDefBlobTTL(void) const;
    /// Get type of logging all commands starting and stopping
    bool IsLogCmds(void) const;
    /// Get name of client that should be used for administrative commands
    const string& GetAdminClient(void) const;

    /// Get total number of seconds the server is running
    int GetUpTime(void);
    /// Get maximum number of connections allowed in the server
    int GetMaxConnections(void);
    /// Get amount of space available on the disk
    Uint8 GetDiskFree(void);

    static Uint8 GetPreciseTime(void);
    static void CachingCompleted(void);
    static bool IsInitiallySynced(void);
    static void InitialSyncComplete(void);
    static void UpdateLastRecNo(void);
    static bool IsCachingComplete(void);
    static bool IsDebugMode(void);

    /// Re-read configuration of the server and all storages
    //void Reconfigure(void);
    /// Print full server statistics into stream
    void PrintServerStats(CNcbiIostream* ios);

private:
    ///
    typedef set<unsigned int>   TPortsList;
    typedef vector<string>      TSpecKeysList;
    ///
    struct SSpecParamsEntry {
        string        key;
        CRef<CObject> value;

        SSpecParamsEntry(const string& key, CObject* value);
    };
    ///
    struct SSpecParamsSet : public CObject {
        vector<SSpecParamsEntry>  entries;

        virtual ~SSpecParamsSet(void);
    };


    virtual void Init(void);


    /// Read server parameters from application's configuration file
    bool x_ReadServerParams(void);
    ///
    void x_ReadSpecificParams(const IRegistry&   reg,
                              const string&      section,
                              SNCSpecificParams* params);
    ///
    void x_PutNewParams(SSpecParamsSet*         params_set,
                        unsigned int            best_index,
                        const SSpecParamsEntry& entry);
    ///
    SSpecParamsSet* x_FindNextParamsSet(const SSpecParamsSet* cur_set,
                                        const string&         key,
                                        unsigned int&         best_index);
    void x_ReadPerClientConfig(const CNcbiRegistry& reg);
    void x_CheckDefClientConfig(SSpecParamsSet* cur_set,
                                SSpecParamsSet* prev_set,
                                Uint1 depth,
                                SSpecParamsSet* deflt);


    /// Print full server statistics into stream or diagnostics
    void x_PrintServerStats(CPrintTextProxy& proxy);



    ///
    string                         m_PortsConfStr;
    /// Port where server runs
    TPortsList                     m_Ports;
    Uint4                          m_CtrlPort;
    /// Type of logging all commands starting and stopping
    bool                           m_LogCmds;
    /// Name of client that should be used for administrative commands
    string                         m_AdminClient;
    ///
    TSpecKeysList                  m_SpecPriority;
    ///
    CRef<SSpecParamsSet>           m_SpecParams;
    ///
    CRef<SSpecParamsSet>           m_OldSpecParams;
    ///
    unsigned int                   m_DefConnTimeout;
    int                            m_DefBlobTTL;
    /// Time when this server instance was started
    CTime                          m_StartTime;
    // Some variable that should be here because of CServer requirements
    STimeout                       m_ServerAcceptTimeout;
    /// Flag that server received a shutdown request
    bool                           m_Shutdown;
    /// Signal which caused the shutdown request
    int                            m_Signal;
    bool                           m_DebugMode;
    bool                           m_InitiallySynced;
    bool                           m_CachingComplete;
};


///
extern CNetCacheServer* g_NetcacheServer;


/// NetCache daemon application
class CNetCacheDApp : public CNcbiApplication
{
protected:
    virtual void Init(void);
    virtual int  Run (void);
};



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

inline void
CNetCacheServer::RequestShutdown(int sig /* = 0 */)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_Signal = sig;
    }
}

inline unsigned int
CNetCacheServer::GetDefConnTimeout(void) const
{
    return m_DefConnTimeout;
}

inline int
CNetCacheServer::GetDefBlobTTL(void) const
{
    return m_DefBlobTTL;
}

inline bool
CNetCacheServer::IsLogCmds(void) const
{
    return m_LogCmds;
}

inline const string&
CNetCacheServer::GetAdminClient(void) const
{
    return m_AdminClient;
}

inline void
CNetCacheServer::PrintServerStats(CNcbiIostream* ios)
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintStream, ios);
    x_PrintServerStats(proxy);
}

inline int
CNetCacheServer::GetUpTime(void)
{
    return int(CTime(CTime::eCurrent).GetTimeT() - m_StartTime.GetTimeT());
}

inline Uint8
CNetCacheServer::GetPreciseTime(void)
{
#ifdef NCBI_OS_MSWIN
    // Conversion from FILETIME to time since Epoch is taken from MSVC's
    // implementation of time().
    union {
        Uint8    ft_int8;
        FILETIME ft_struct;
    } ft;
    GetSystemTimeAsFileTime(&ft.ft_struct);
    Uint8 epoch_time = ft.ft_int8 - 116444736000000000i64;
    return epoch_time / 10;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return Uint8(tv.tv_sec) * kNCTimeTicksInSec + tv.tv_usec;
#endif
}

END_NCBI_SCOPE

#endif /* NETCACHED__HPP */
