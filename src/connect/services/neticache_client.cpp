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
 * Author:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of netcache ICache client.
 *
 */

#include <ncbi_pch.hpp>

#include "netcache_rw.hpp"

#include <connect/services/neticache_client.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/cache/icache_cf.hpp>
#include <util/transmissionrw.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

class CNetICacheServerListener : public CNetCacheServerListener
{
protected:
    virtual void OnInit(CNetObject* api_impl,
        CConfig* config, const string& config_section);
};

const char* const kNetICacheDriverName = "netcache";

static const char* const s_NetICacheConfigSections[] = {
    kNetICacheDriverName,
    NULL
};

#define KEY_VERSION_SUBKEY_MAXLEN (1 + key.length() + 2 + \
    int(sizeof(version) * 1.5) + 2 + subkey.length() + 1)

static inline void AppendKeyVersionSubkey(string* outstr,
    const string& key, int version, const string& subkey)
{
    outstr->append(key);
    outstr->append("\" ", 2);

    outstr->append(NStr::UIntToString(version));

    outstr->append(" \"", 2);
    outstr->append(subkey);
    outstr->push_back('"');
}

static string s_MakeBlobID(const string& key, int version, const string& subkey)
{
    string blob_id(kEmptyStr);
    blob_id.reserve(KEY_VERSION_SUBKEY_MAXLEN);

    blob_id.push_back('"');

    AppendKeyVersionSubkey(&blob_id, key, version, subkey);

    return blob_id;
}

struct SNetICacheClientImpl : public SNetCacheAPIImpl, protected CConnIniter
{
    SNetICacheClientImpl(CConfig* config,
            const string& section,
            const string& service_name,
            const string& client_name,
            const string& cache_name) :
        SNetCacheAPIImpl(new SNetServiceImpl(service_name, client_name,
            new CNetICacheServerListener)),
        m_CacheName(cache_name)
    {
        m_Service->Init(this, config, section, s_NetICacheConfigSections);
    }

    CNetServer::SExecResult StickToServerAndExec(const string& cmd);

    void RegisterUnregisterSession(string cmd, unsigned pid);

    string MakeStdCmd(const char* cmd_base, const string& key,
        int version, const string& subkey);

    string ExecStdCmd(const char* cmd_base, const string& key,
        int version, const string& subkey);

    virtual CNetServerConnection InitiateWriteCmd(string* blob_id,
        unsigned time_to_live);

    IReader* GetReadStream(const string& key,
        int version, const string& subkey, size_t* blob_size_ptr,
        CNetCacheAPI::ECachingMode caching_mode);

    string m_CacheName;
    string m_ICacheCmdPrefix;

    CNetServer m_SelectedServer;
};

void CNetICacheServerListener::OnInit(CNetObject* api_impl,
    CConfig* config, const string& config_section)
{
    CNetCacheServerListener::OnInit(api_impl, config, config_section);

    SNetICacheClientImpl* icache_impl =
        static_cast<SNetICacheClientImpl*>(api_impl);

    if (icache_impl->m_CacheName.empty()) {
        if (config == NULL) {
            NCBI_THROW(CNetCacheException,
                eAuthenticationError, "ICache database name is not defined");
        } else {
            try {
                icache_impl->m_CacheName = config->GetString(config_section,
                    "name", CConfig::eErr_Throw, kEmptyStr);
            }
            catch (exception&) {
                icache_impl->m_CacheName = config->GetString(config_section,
                    "cache_name", CConfig::eErr_Throw, "default_cache");
            }
        }
    }

    icache_impl->m_ICacheCmdPrefix = "IC(";
    icache_impl->m_ICacheCmdPrefix.append(icache_impl->m_CacheName);
    icache_impl->m_ICacheCmdPrefix.append(") ");
}

CNetICachePasswordGuard::CNetICachePasswordGuard(
    CNetICacheClient::TInstance ic_client,
    const string& password)
{
    if (!ic_client->m_Password.empty()) {
        NCBI_THROW(CNetCacheException, eAuthenticationError,
            "Cannot reuse a password-protected CNetICacheClient object.");
    }
    if (password.empty())
        m_NetICacheClient = ic_client;
    else {
        m_NetICacheClient = new SNetICacheClientImpl(*ic_client);
        m_NetICacheClient->m_Password = password;
    }
}

CNetServerConnection SNetICacheClientImpl::InitiateWriteCmd(
    string* blob_id, unsigned time_to_live)
{
    string cmd(m_ICacheCmdPrefix + "STOR ");
    cmd.append(NStr::UIntToString(time_to_live));
    cmd.push_back(' ');
    cmd.append(*blob_id);
    AppendClientIPSessionIDPassword(&cmd);

    return StickToServerAndExec(cmd).conn;
}

CNetICacheClient::CNetICacheClient(EAppRegistry use_app_reg,
        const string& conf_section) :
    m_Impl(new SNetICacheClientImpl(NULL, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetICacheClient::CNetICacheClient(
        const string& host,
        unsigned short port,
        const string& cache_name,
        const string& client_name) :
    m_Impl(new SNetICacheClientImpl(NULL, kEmptyStr,
        host + ':' + NStr::UIntToString(port),
        client_name, cache_name))
{
}

CNetICacheClient::CNetICacheClient(
        const string& service_name,
        const string& cache_name,
        const string& client_name) :
    m_Impl(new SNetICacheClientImpl(NULL, kEmptyStr,
        service_name, client_name, cache_name))
{
}

CNetICacheClient::CNetICacheClient(CConfig* config, const string& driver_name) :
    m_Impl(new SNetICacheClientImpl(config, driver_name,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

void CNetICacheClient::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_Service.SetCommunicationTimeout(to);
}


STimeout CNetICacheClient::GetCommunicationTimeout() const
{
    return m_Impl->m_Service.GetCommunicationTimeout();
}

CNetServer::SExecResult
    SNetICacheClientImpl::StickToServerAndExec(const string& cmd)
{
    if (m_SelectedServer) {
        try {
            return m_SelectedServer.ExecWithRetry(cmd);
        }
        catch (CNetSrvConnException& ex) {
            if (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure &&
                ex.GetErrCode() != CNetSrvConnException::eServerThrottle)
                throw;
            ERR_POST(ex.what());
        }
    }

    for (CNetServerGroupIterator it =
            m_Service.DiscoverServers().Iterate(); it; ++it) {
        try {
            return (m_SelectedServer = *it).ExecWithRetry(cmd);
        }
        catch (CNetSrvConnException& ex) {
            if (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure &&
                ex.GetErrCode() != CNetSrvConnException::eServerThrottle)
                throw;
            ERR_POST(ex.what());
        }
    }

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any availbale servers for the " +
            m_Service.GetServiceName() + " service.");
}

void SNetICacheClientImpl::RegisterUnregisterSession(string cmd, unsigned pid)
{
    char hostname[256];
    if (SOCK_gethostname(hostname, sizeof(hostname)) != 0) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
            "Cannot get host name");
    }
    cmd.append(hostname);
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(pid));
    AppendClientIPSessionIDPassword(&cmd);
    StickToServerAndExec(cmd);
}

void CNetICacheClient::RegisterSession(unsigned pid)
{
    m_Impl->RegisterUnregisterSession("SMR ", pid);
}


void CNetICacheClient::UnRegisterSession(unsigned pid)
{
    m_Impl->RegisterUnregisterSession("SMU ", pid);
}


void CNetICacheClient::SetTimeStampPolicy(TTimeStampFlags policy,
                                          unsigned int    timeout,
                                          unsigned int    max_timeout)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "STSP is not implemented");
}


ICache::TTimeStampFlags CNetICacheClient::GetTimeStampPolicy() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "GTSP is not implemented");
}


int CNetICacheClient::GetTimeout() const
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "GTOU");
    m_Impl->AppendClientIPSessionIDPassword(&cmd);
    return NStr::StringToUInt(m_Impl->StickToServerAndExec(cmd).response);
}


bool CNetICacheClient::IsOpen() const
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "ISOP");
    m_Impl->AppendClientIPSessionIDPassword(&cmd);
    return NStr::StringToUInt(m_Impl->StickToServerAndExec(cmd).response) != 0;
}


void CNetICacheClient::SetVersionRetention(EKeepVersions policy)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "SVRP is not implemented");
}


ICache::EKeepVersions CNetICacheClient::GetVersionRetention() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "ISOP is not implemented");
}


void CNetICacheClient::Store(const string&  key,
                             int            version,
                             const string&  subkey,
                             const void*    data,
                             size_t         size,
                             unsigned int   time_to_live,
                             const string&  /*owner*/)
{
    string blob_id(s_MakeBlobID(key, version, subkey));

    CNetCacheWriter writer(m_Impl, &blob_id, time_to_live,
        eICache_NoWait, CNetCacheAPI::eCaching_Disable);

    writer.WriteBufferAndClose(reinterpret_cast<const char*>(data), size);
}


size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    return NStr::StringToULong(
        m_Impl->ExecStdCmd("GSIZ", key, version, subkey));
}


void CNetICacheClient::GetBlobOwner(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    string*        owner)
{
    *owner = m_Impl->ExecStdCmd("GBLW", key, version, subkey);
}

IReader* SNetICacheClientImpl::GetReadStream(
    const string& key, int version, const string& subkey,
    size_t* blob_size_ptr, CNetCacheAPI::ECachingMode caching_mode)
{
    try {
        return new CNetCacheReader(this, StickToServerAndExec(
            MakeStdCmd("READ", key, version, subkey)),
                blob_size_ptr, caching_mode);
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return NULL;
        throw;
    }
}

bool CNetICacheClient::Read(const string& key,
                            int           version,
                            const string& subkey,
                            void*         buf,
                            size_t        buf_size)
{
    size_t blob_size;

    auto_ptr<IReader> rdr(m_Impl->GetReadStream(
        key, version, subkey, &blob_size, CNetCacheAPI::eCaching_Disable));

    if (rdr.get() == 0)
        return false;

    return SNetCacheAPIImpl::ReadBuffer(*rdr, (char*) buf, buf_size,
        NULL, blob_size) == CNetCacheAPI::eReadComplete;
}


void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     SBlobAccessDescr* blob_descr)
{
    size_t blob_size;
    blob_descr->reader.reset(m_Impl->GetReadStream(key, version, subkey,
        &blob_size, CNetCacheAPI::eCaching_AppDefault));

    if (blob_descr->reader.get()) {
        blob_descr->blob_size = blob_size;
        blob_descr->blob_found = true;

        if (blob_descr->buf && blob_descr->buf_size >= blob_size) {
            try {
                SNetCacheAPIImpl::ReadBuffer(*blob_descr->reader,
                    blob_descr->buf, blob_descr->buf_size,
                    NULL, blob_size);
            }
            catch (CNetServiceException&) {
                blob_descr->reader.reset(NULL);
                throw;
            }
            blob_descr->reader.reset(NULL);
        }
    } else {
        blob_descr->blob_size = 0;
        blob_descr->blob_found = false;
    }
}


IWriter* CNetICacheClient::GetWriteStream(const string&    key,
                                          int              version,
                                          const string&    subkey,
                                          unsigned int     time_to_live,
                                          const string&    owner)
{
    return GetNetCacheWriter(key, version, subkey, time_to_live, owner);
}


IEmbeddedStreamWriter* CNetICacheClient::GetNetCacheWriter(const string& key,
    int version, const string& subkey,
    unsigned time_to_live, const string& /*owner*/,
    CNetCacheAPI::ECachingMode caching_mode)
{
    string blob_id(s_MakeBlobID(key, version, subkey));

    return new CNetCacheWriter(m_Impl, &blob_id, time_to_live,
        eICache_NoWait, caching_mode);
}


void CNetICacheClient::Remove(const string& key)
{
    string cmd(kEmptyStr);
    cmd.reserve(m_Impl->m_ICacheCmdPrefix.length() +
        sizeof("REMK \"") - 1 +
        key.length() + 1);
    cmd.append(m_Impl->m_ICacheCmdPrefix);
    cmd.append("REMK \"");
    cmd.append(key);
    cmd.push_back('"');
    m_Impl->AppendClientIPSessionIDPassword(&cmd);
    m_Impl->StickToServerAndExec(cmd);
}


void CNetICacheClient::Remove(const string&    key,
                              int              version,
                              const string&    subkey)
{
    m_Impl->ExecStdCmd("REMO", key, version, subkey);
}


time_t CNetICacheClient::GetAccessTime(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    return NStr::StringToInt(
        m_Impl->ExecStdCmd("GACT", key, version, subkey));
}


bool CNetICacheClient::HasBlobs(const string&  key,
                                const string&  subkey)
{
    return m_Impl->ExecStdCmd("HASB", key, 0, subkey) == "1";
}


void CNetICacheClient::Purge(time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "PRG1 is not implemented");
}


void CNetICacheClient::Purge(const string&    /*key*/,
                             const string&    /*subkey*/,
                             time_t           /*access_timeout*/,
                             EKeepVersions    /*keep_last_version*/)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "PRG1 is not implemented");
}

IReader* CNetICacheClient::GetReadStream(
    const string& key,
    int version,
    const string& subkey,
    size_t* blob_size_ptr,
    CNetCacheAPI::ECachingMode caching_mode)
{
    return m_Impl->GetReadStream(key, version, subkey,
        blob_size_ptr, caching_mode);
}

IReader* CNetICacheClient::GetReadStream(const string&  key,
                                         int            version,
                                         const string&  subkey)
{
    return GetReadStream(key, version, subkey,
        NULL, CNetCacheAPI::eCaching_AppDefault);
}

string SNetICacheClientImpl::MakeStdCmd(const char* cmd_base,
    const string& key, int version, const string& subkey)
{
    string cmd(kEmptyStr);
    cmd.reserve(m_ICacheCmdPrefix.length() + strlen(cmd_base) +
        1 + KEY_VERSION_SUBKEY_MAXLEN);

    cmd.append(m_ICacheCmdPrefix);
    cmd.append(cmd_base);

    cmd.append(" \"", 2);

    AppendKeyVersionSubkey(&cmd, key, version, subkey);

    AppendClientIPSessionIDPassword(&cmd);

    return cmd;
}

string SNetICacheClientImpl::ExecStdCmd(const char* cmd_base,
    const string& key, int version, const string& subkey)
{
    return StickToServerAndExec(MakeStdCmd(cmd_base,
        key, version, subkey)).response;
}

string CNetICacheClient::GetCacheName(void) const
{
    return m_Impl->m_CacheName;
}


bool CNetICacheClient::SameCacheParams(const TCacheParams* params) const
{
    return false;
}


/// Class factory for NetCache implementation of ICache
///
/// @internal
///
class CNetICacheCF : public CICacheCF<CNetICacheClient>
{
public:
    typedef CICacheCF<CNetICacheClient> TParent;

public:
    CNetICacheCF() : TParent(kNetICacheDriverName, 0)
    {
    }

    virtual ICache* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;
};


ICache* CNetICacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    if ((!driver.empty() && driver != m_DriverName) ||
        version.Match(NCBI_INTERFACE_VERSION(ICache)) ==
            CVersionInfo::eNonCompatible)
        return 0;

    if (!params)
        return new CNetICacheClient;

    CConfig conf(params);

    return new CNetICacheClient(&conf, driver);
}


void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CNetICacheCF>::NCBI_EntryPointImpl(info_list, method);
}


void Cache_RegisterDriver_NetCache(void)
{
    RegisterEntryPoint<ICache>( NCBI_EntryPoint_xcache_netcache );
}

END_NCBI_SCOPE
