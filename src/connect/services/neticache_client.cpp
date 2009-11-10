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

#include "netcache_api_impl.hpp"

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

static const char* kNetICacheDriverName = "netcache";

struct SNetICacheClientImpl : public SNetCacheAPIImpl, protected CConnIniter
{
    SNetICacheClientImpl(CConfig* config,
            const string& section,
            const string& service_name,
            const string& client_name,
            const string& lbsm_affinity_name,
            const string& cache_name) :
        SNetCacheAPIImpl(config,
            section.empty() ? kNetICacheDriverName : section,
            service_name, client_name, lbsm_affinity_name)
    {
        if (m_Service->m_ClientName.length() < 3) {
            NCBI_THROW(CNetCacheException,
                eAuthenticationError, "Client name is too short or empty");
        }

        if (!cache_name.empty())
            m_CacheName = cache_name;
        else {
            if (config == NULL) {
                NCBI_THROW(CNetCacheException,
                    eAuthenticationError, "Cache name is not defined");
            } else {
                try {
                    m_CacheName = config->GetString(section,
                        "name", CConfig::eErr_Throw, kEmptyStr);
                }
                catch (exception&) {
                    m_CacheName = config->GetString(section,
                        "cache_name", CConfig::eErr_Throw, "default_cache");
                }
            }
        }

        m_ICacheCmdPrefix = "IC(";
        m_ICacheCmdPrefix.append(m_CacheName);
        m_ICacheCmdPrefix.append(") ");
    }

    void RegisterUnregisterSession(string cmd, unsigned pid);

    void AddKVS(string* out_str, const string& key,
        int version, const string& subkey) const;

    CNetServerConnection InitiateStoreCmd(const string& key,
        int version, const string& subkey, unsigned time_to_live);

    IReader* GetReadStream(const string& key,
        int version, const string& subkey, size_t* blob_size);

    string m_CacheName;
    string m_ICacheCmdPrefix;
};

CNetServerConnection SNetICacheClientImpl::InitiateStoreCmd(
    const string& key, int version, const string& subkey, unsigned time_to_live)
{
    string cmd(m_ICacheCmdPrefix + "STOR ");
    cmd.append(NStr::UIntToString(time_to_live));
    AddKVS(&cmd, key, version, subkey);

    return m_Service.FindServerAndExec(cmd).conn;
}

CNetICacheClient::CNetICacheClient(
        const string& host,
        unsigned short port,
        const string& cache_name,
        const string& client_name) :
    m_Impl(new SNetICacheClientImpl(NULL, kEmptyStr,
        host + ':' + NStr::UIntToString(port),
        client_name, kEmptyStr, cache_name))
{
}

CNetICacheClient::CNetICacheClient(
        const string& service_name,
        const string& cache_name,
        const string& client_name) :
    m_Impl(new SNetICacheClientImpl(NULL, kEmptyStr,
        service_name, client_name, kEmptyStr, cache_name))
{
}

CNetICacheClient::CNetICacheClient(
        const string& service_name,
        const string& cache_name,
        const string& client_name,
        const string& lbsm_affinity_name) :
    m_Impl(new SNetICacheClientImpl(NULL, kEmptyStr,
        service_name, client_name, lbsm_affinity_name, cache_name))
{
}

CNetICacheClient::CNetICacheClient(CConfig* config, const string& driver_name) :
    m_Impl(new SNetICacheClientImpl(config, driver_name,
        kEmptyStr, kEmptyStr, kEmptyStr, kEmptyStr))
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
    AppendClientIPSessionID(&cmd);
    m_Service.FindServerAndExec(cmd);
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
    m_Impl->AppendClientIPSessionID(&cmd);
    return NStr::StringToUInt(
        m_Impl->m_Service.FindServerAndExec(cmd).response);
}


bool CNetICacheClient::IsOpen() const
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "ISOP");
    m_Impl->AppendClientIPSessionID(&cmd);
    return NStr::StringToUInt(
        m_Impl->m_Service.FindServerAndExec(cmd).response) != 0;
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
    m_Impl->WriteBuffer(
        m_Impl->InitiateStoreCmd(key, version, subkey, time_to_live),
        CNetCacheWriter::eICache_NoWait, (const char*) data, size);
}


size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "GSIZ");
    m_Impl->AddKVS(&cmd, key, version, subkey);
    return NStr::StringToULong(
        m_Impl->m_Service.FindServerAndExec(cmd).response);
}


void CNetICacheClient::GetBlobOwner(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    string*        owner)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "GBLW");
    m_Impl->AddKVS(&cmd, key, version, subkey);
    *owner = m_Impl->m_Service.FindServerAndExec(cmd).response;
}

IReader* SNetICacheClientImpl::GetReadStream(
    const string& key, int version, const string& subkey, size_t* blob_size)
{
    string cmd(m_ICacheCmdPrefix + "READ");
    AddKVS(&cmd, key, version, subkey);

    CNetServer::SExecResult exec_result;

    try {
        exec_result = m_Service.FindServerAndExec(cmd);
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return NULL;
        throw;
    }

    return SNetCacheAPIImpl::GetReadStream(exec_result, blob_size);
}

bool CNetICacheClient::Read(const string& key,
                            int           version,
                            const string& subkey,
                            void*         buf,
                            size_t        buf_size)
{

    size_t blob_size;

    auto_ptr<IReader> rdr(m_Impl->GetReadStream(
        key, version, subkey, &blob_size));

    if (rdr.get() == 0)
        return false;

    return m_Impl->ReadBuffer(*rdr, (unsigned char*) buf, buf_size,
        NULL, blob_size) == CNetCacheAPI::eReadComplete;
}


void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     SBlobAccessDescr* blob_descr)
{
    size_t blob_size;
    blob_descr->reader.reset(
        m_Impl->GetReadStream(key, version, subkey, &blob_size));

    if (blob_descr->reader.get()) {
        blob_descr->blob_size = blob_size;
        blob_descr->blob_found = true;

        if (blob_descr->buf && blob_descr->buf_size >= blob_size) {
            try {
                m_Impl->ReadBuffer(*blob_descr->reader,
                    (unsigned char*) blob_descr->buf, blob_descr->buf_size,
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
                                          const string&    /*owner*/)
{
    return new CNetCacheWriter(
        m_Impl->InitiateStoreCmd(key, version, subkey, time_to_live),
            CNetCacheWriter::eICache_NoWait);
}


void CNetICacheClient::Remove(const string& key)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "REMK \"");
    cmd.append(key);
    cmd.push_back('"');
    m_Impl->AppendClientIPSessionID(&cmd);
    m_Impl->m_Service.FindServerAndExec(cmd);
}


void CNetICacheClient::Remove(const string&    key,
                              int              version,
                              const string&    subkey)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "REMO");
    m_Impl->AddKVS(&cmd, key, version, subkey);
    m_Impl->m_Service.FindServerAndExec(cmd);
}


time_t CNetICacheClient::GetAccessTime(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "GACT");
    m_Impl->AddKVS(&cmd, key, version, subkey);
    return NStr::StringToInt(m_Impl->m_Service.FindServerAndExec(cmd).response);
}


bool CNetICacheClient::HasBlobs(const string&  key,
                                const string&  subkey)
{
    string cmd(m_Impl->m_ICacheCmdPrefix + "HASB");
    m_Impl->AddKVS(&cmd, key, 0, subkey);
    return m_Impl->m_Service.FindServerAndExec(cmd).response == "1";
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


IReader* CNetICacheClient::GetReadStream(const string&  key,
                                         int            version,
                                         const string&  subkey)
{
    return m_Impl->GetReadStream(key, version, subkey, NULL);
}


void SNetICacheClientImpl::AddKVS(string*          out_str,
                              const string&    key,
                              int              version,
                              const string&    subkey) const
{
    out_str->append(" \"", 2);
    out_str->append(key);
    out_str->append("\" ", 2);
    out_str->append(NStr::UIntToString(version));
    out_str->append(" \"", 2);
    out_str->append(subkey);
    out_str->push_back('"');

    AppendClientIPSessionID(out_str);
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
