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
#include <connect/services/srv_connections.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <util/cache/icache_cf.hpp>
#include <util/transmissionrw.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <memory>
#include <sstream>

#define MAX_ICACHE_CACHE_NAME_LENGTH 64
#define MAX_ICACHE_KEY_LENGTH 256
#define MAX_ICACHE_SUBKEY_LENGTH 256

#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

const char* const kNetICacheDriverName = "netcache";

static string s_CheckKeySubkey(
    const string& key, const string& subkey, string* encoded_key)
{
    encoded_key->push_back('"');
    encoded_key->append(NStr::PrintableString(key));

    string encoded_subkey(NStr::PrintableString(subkey));

    if (encoded_key->length() > (1 + MAX_ICACHE_KEY_LENGTH) ||
            encoded_subkey.length() > MAX_ICACHE_SUBKEY_LENGTH) {
        NCBI_THROW(CNetCacheException, eKeyFormatError,
            "ICache key or subkey is too long");
    }

    return encoded_subkey;
}

static string s_KeySubkeyToBlobID(const string& key, const string& subkey)
{
    string blob_id(kEmptyStr);
    blob_id.reserve(1 + key.length() + 3 + subkey.length() + 1);

    string encoded_subkey(s_CheckKeySubkey(key, subkey, &blob_id));

    blob_id.append("\" \"", 3);

    blob_id.append(encoded_subkey);

    blob_id.push_back('"');

    return blob_id;
}

static string s_KeyVersionSubkeyToBlobID(
    const string& key, int version, const string& subkey)
{
    string blob_id(kEmptyStr);
    blob_id.reserve(1 + key.length() + 2 +
        int((double)sizeof(version) * 1.5) + 2 + subkey.length() + 1);

    string encoded_subkey(s_CheckKeySubkey(key, subkey, &blob_id));

    blob_id.append("\" ", 2);
    blob_id.append(NStr::IntToString(version));
    blob_id.append(" \"", 2);

    blob_id.append(encoded_subkey);

    blob_id.push_back('"');

    return blob_id;
}

static const char s_NetICacheAPIName[] = "NetICacheClient";

struct SNetICacheClientImpl : public SNetCacheAPIImpl
{
    SNetICacheClientImpl(CSynRegistryBuilder registry_builder,
            const string& section,
            const string& service_name,
            const string& client_name,
            const string& cache_name) :
        m_CacheFlags(ICache::fBestPerformance)
    {
        m_DefaultParameters.SetCacheName(cache_name);
        SRegSynonyms sections{ section, "netcache_api", "netcache_client", kNetICacheDriverName };
        m_Service = SNetServiceImpl::Create(s_NetICacheAPIName, service_name, client_name,
                new CNetCacheServerListener,
                registry_builder, sections);
        Init(registry_builder, sections);
    }

    SNetICacheClientImpl(SNetServerInPool* server, SNetICacheClientImpl* parent) :
        SNetCacheAPIImpl(server, parent),
        m_CacheFlags(parent->m_CacheFlags)
    {
    }

    CNetServer::SExecResult ChooseServerAndExec(const string& cmd,
            const string& key,
            bool multiline_output,
            const CNetCacheAPIParameters* parameters);

    string MakeStdCmd(const char* cmd_base, const string& blob_id,
        const CNetCacheAPIParameters* parameters,
        const string& injection = kEmptyStr);

    string ExecStdCmd(const char* cmd_base, const string& key,
        int version, const string& subkey,
        const CNetCacheAPIParameters* parameters);

    virtual CNetServerConnection InitiateWriteCmd(CNetCacheWriter* nc_writer,
            const CNetCacheAPIParameters* parameters);

    IReader* ReadCurrentBlobNotOlderThan(
        const string& key, const string& subkey,
        size_t* blob_size_ptr,
        int* version,
        ICache::EBlobVersionValidity* validity,
        unsigned max_age, unsigned* actual_age,
        const CNamedParameterList* optional = NULL);

    IReader* GetReadStreamPart(const string& key,
        int version, const string& subkey,
        size_t offset, size_t part_size,
        size_t* blob_size_ptr,
        const CNamedParameterList* optional);

    void Init(CSynRegistry& registry, const SRegSynonyms& sections);

    ICache::TFlags m_CacheFlags;
};

void SNetICacheClientImpl::Init(CSynRegistry& registry, const SRegSynonyms& sections)
{
    SNetCacheAPIImpl::Init(registry, sections);

    auto cache_name = m_DefaultParameters.GetCacheName();

    if (cache_name.empty()) cache_name = registry.Get(sections, { "name", "cache_name" }, "default_cache");

    if (cache_name.length() > MAX_ICACHE_CACHE_NAME_LENGTH) {
        NCBI_THROW(CNetCacheException, eAuthenticationError, "NetICache: cache name is too long");
    }

    m_DefaultParameters.SetCacheName(cache_name);
    m_DefaultParameters.SetTryAllServers(registry.Get(sections, "try_all_servers", false));
}

CNetServerConnection SNetICacheClientImpl::InitiateWriteCmd(
    CNetCacheWriter* nc_writer, const CNetCacheAPIParameters* parameters)
{
    string cmd("IC(" + NStr::PrintableString(parameters->GetCacheName()));
    cmd.append(") STOR ");

    cmd.append(NStr::UIntToString(parameters->GetTTL()));
    cmd.push_back(' ');
    cmd.append(nc_writer->GetBlobID());
    if (nc_writer->GetResponseType() == eNetCache_Wait)
        cmd.append(" confirm=1");
    AppendClientIPSessionIDPasswordAgeHitID(&cmd, parameters);
    if (m_FlagsOnWrite) cmd.append(" flags=").append(to_string(m_FlagsOnWrite));

    return ChooseServerAndExec(cmd, nc_writer->GetKey(),
            false, parameters).conn;
}

CNetICacheClient::CNetICacheClient(EAppRegistry,
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

CNetICacheClient::CNetICacheClient(const IRegistry& reg, const string& section) :
    m_Impl(new SNetICacheClientImpl(reg, section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

void CNetICacheClient::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_Service->m_ServerPool.SetCommunicationTimeout(to);
}


STimeout CNetICacheClient::GetCommunicationTimeout() const
{
    return m_Impl->m_Service->m_ServerPool.GetCommunicationTimeout();
}

class SWeightedServiceTraversal : public IServiceTraversal
{
public:
    SWeightedServiceTraversal(CNetService::TInstance service,
            const string& key) :
        m_Service(service),
        m_Key(key)
    {
    }

    virtual CNetServer BeginIteration();
    virtual CNetServer NextServer();

private:
    CNetService m_Service;
    const string& m_Key;
    CNetServiceIterator m_Iterator;
};

CNetServer SWeightedServiceTraversal::BeginIteration()
{
    return *(m_Iterator = m_Service.IterateByWeight(m_Key));
}

CNetServer SWeightedServiceTraversal::NextServer()
{
    return ++m_Iterator ? *m_Iterator : CNetServer();
}

CNetServer::SExecResult SNetICacheClientImpl::ChooseServerAndExec(
        const string& cmd,
        const string& key,
        bool multiline_output,
        const CNetCacheAPIParameters* parameters)
{
    CNetServer selected_server(parameters->GetServerToUse());
    CNetServer* server_last_used_ptr(parameters->GetServerLastUsedPtr());

    const auto try_all_servers = parameters->GetTryAllServers();
    CNetServer::SExecResult exec_result;

    if (!try_all_servers && selected_server) {
        exec_result = selected_server.ExecWithRetry(cmd, multiline_output);

    } else if (selected_server) {
        ESwitch server_check = eDefault;
        parameters->GetServerCheck(&server_check);

        SNetCacheMirrorTraversal mirror_traversal(m_Service,
                selected_server, server_check);

        m_Service->IterateUntilExecOK(cmd, multiline_output, exec_result,
                &mirror_traversal, SNetServiceImpl::eIgnoreServerErrors);
    } else {
        SWeightedServiceTraversal service_traversal(m_Service, key);

        m_Service->IterateUntilExecOK(cmd, multiline_output, exec_result,
                &service_traversal, try_all_servers ?
                SNetServiceImpl::eIgnoreServerErrors : SNetServiceImpl::eRethrowAllServerErrors);
    }

    if (server_last_used_ptr != NULL)
        *server_last_used_ptr = exec_result.conn->m_Server;

    return exec_result;
}

void CNetICacheClient::RegisterSession(unsigned)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "SMR is not implemented");
}


void CNetICacheClient::UnRegisterSession(unsigned)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "SMU is not implemented");
}


ICache::TFlags CNetICacheClient::GetFlags()
{
    return m_Impl->m_CacheFlags;
}


void CNetICacheClient::SetFlags(ICache::TFlags flags)
{
    m_Impl->m_CacheFlags = flags;
}


void CNetICacheClient::SetTimeStampPolicy(TTimeStampFlags, unsigned, unsigned)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "STSP is not implemented");
}


ICache::TTimeStampFlags CNetICacheClient::GetTimeStampPolicy() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "GTSP is not implemented");
}


int CNetICacheClient::GetTimeout() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "GTOU is not implemented");
}


bool CNetICacheClient::IsOpen() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "ISOP is not implemented");
}


void CNetICacheClient::SetVersionRetention(EKeepVersions)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "SVRP is not implemented");
}


ICache::EKeepVersions CNetICacheClient::GetVersionRetention() const
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "GVRP is not implemented");
}


void CNetICacheClient::Store(const string&  key,
                             int            version,
                             const string&  subkey,
                             const void*    data,
                             size_t         size,
                             unsigned int   time_to_live,
                             const string&  /*owner*/)
{
    string blob_id(s_KeyVersionSubkeyToBlobID(key, version, subkey));

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.SetTTL(time_to_live);
    parameters.SetCachingMode(CNetCacheAPI::eCaching_Disable);

    CNetCacheWriter writer(m_Impl, &blob_id, key,
        m_Impl->m_CacheFlags & ICache::fBestReliability ?
            eNetCache_Wait : eICache_NoWait, &parameters);

    writer.WriteBufferAndClose(reinterpret_cast<const char*>(data), size);
}


size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    return GetBlobSize(key, version, subkey);
}

size_t CNetICacheClient::GetBlobSize(const string& key,
        int version, const string& subkey,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return CheckBlobSize(NStr::StringToUInt8(m_Impl->ExecStdCmd(
            "GSIZ", key, version, subkey, &parameters)));
}


void CNetICacheClient::GetBlobOwner(const string&, int, const string&,
        string* owner)
{
    ERR_POST("NetCache command 'GBLW' has been phased out");
    *owner = kEmptyStr;
}

IReader* SNetICacheClientImpl::GetReadStreamPart(
    const string& key, int version, const string& subkey,
    size_t offset, size_t part_size,
    size_t* blob_size_ptr, const CNamedParameterList* optional)
{
    try {
        string blob_id(s_KeyVersionSubkeyToBlobID(key, version, subkey));

        CNetCacheAPIParameters parameters(&m_DefaultParameters);

        parameters.LoadNamedParameters(optional);

        const char* cmd_name;
        string cmd;

        if (offset == 0 && part_size == 0) {
            cmd_name = "READ";
            cmd = MakeStdCmd(cmd_name, blob_id, &parameters);
        } else {
            cmd_name = "READPART";
            cmd = MakeStdCmd(cmd_name, blob_id, &parameters,
                    ' ' + NStr::UInt8ToString((Uint8) offset) +
                    ' ' + NStr::UInt8ToString((Uint8) part_size));
        }

        CNetServer::SExecResult exec_result(
                ChooseServerAndExec(cmd, key, false, &parameters));

        unsigned* actual_age_ptr = parameters.GetActualBlobAgePtr();
        if (parameters.GetMaxBlobAge() > 0 && actual_age_ptr != NULL)
            *actual_age_ptr = x_ExtractBlobAge(exec_result, cmd_name);

        return new CNetCacheReader(this, blob_id,
            exec_result, blob_size_ptr, &parameters);
    }
    catch (CNetCacheBlobTooOldException&) {
        return NULL;
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() != CNetCacheException::eBlobNotFound)
            throw;
        return NULL;
    }
}

bool CNetICacheClient::Read(const string& key,
                            int           version,
                            const string& subkey,
                            void*         buf,
                            size_t        buf_size)
{
    return ReadPart(key, version, subkey, 0, 0, buf, buf_size);
}

bool CNetICacheClient::ReadPart(const string& key,
    int version,
    const string& subkey,
    size_t offset,
    size_t part_size,
    void* buf,
    size_t buf_size)
{
    size_t blob_size;

    unique_ptr<IReader> rdr(m_Impl->GetReadStreamPart(
        key, version, subkey, offset, part_size,
            &blob_size, nc_caching_mode = CNetCacheAPI::eCaching_Disable));

    if (rdr.get() == NULL)
        return false;

    return SNetCacheAPIImpl::ReadBuffer(*rdr, (char*) buf, buf_size,
        NULL, blob_size) == CNetCacheAPI::eReadComplete;
}


void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     SBlobAccessDescr* blob_descr)
{
    if (blob_descr->return_current_version) {
        blob_descr->return_current_version_supported = true;
        blob_descr->reader.reset(m_Impl->ReadCurrentBlobNotOlderThan(
                key,
                subkey,
                &blob_descr->blob_size,
                &blob_descr->current_version,
                &blob_descr->current_version_validity,
                blob_descr->maximum_age,
                &blob_descr->actual_age));
    } else if (blob_descr->maximum_age > 0) {
        blob_descr->reader.reset(m_Impl->GetReadStreamPart(key, version, subkey,
                0, 0, &blob_descr->blob_size,
                (nc_caching_mode = CNetCacheAPI::eCaching_AppDefault,
                nc_max_age = blob_descr->maximum_age,
                nc_actual_age = &blob_descr->actual_age)));
    } else {
        blob_descr->reader.reset(m_Impl->GetReadStreamPart(key, version, subkey,
                0, 0, &blob_descr->blob_size,
                nc_caching_mode = CNetCacheAPI::eCaching_AppDefault));
    }

    if (blob_descr->reader.get() != NULL) {
        blob_descr->blob_found = true;

        if (blob_descr->buf && blob_descr->buf_size >= blob_descr->blob_size) {
            try {
                SNetCacheAPIImpl::ReadBuffer(*blob_descr->reader,
                    blob_descr->buf, blob_descr->buf_size,
                        NULL, blob_descr->blob_size);
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
    return GetNetCacheWriter(key, version, subkey, nc_blob_ttl = time_to_live);
}


IEmbeddedStreamWriter* CNetICacheClient::GetNetCacheWriter(const string& key,
        int version, const string& subkey, const CNamedParameterList* optional)
{
    string blob_id(s_KeyVersionSubkeyToBlobID(key, version, subkey));

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return new CNetCacheWriter(m_Impl, &blob_id, key,
        m_Impl->m_CacheFlags & ICache::fBestReliability ?
            eNetCache_Wait : eICache_NoWait, &parameters);
}


void CNetICacheClient::Remove(const string&    key,
                              int              version,
                              const string&    subkey)
{
    RemoveBlob(key, version, subkey);
}

void CNetICacheClient::RemoveBlob(const string& key,
        int version, const string& subkey,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    m_Impl->ExecStdCmd("REMO", key, version, subkey, &parameters);
}

time_t CNetICacheClient::GetAccessTime(const string&, int, const string&)
{
    NCBI_THROW(CNetCacheException, eNotImplemented, "GACT is not implemented");
}


bool CNetICacheClient::HasBlobs(const string&  key,
                                const string&  subkey)
{
    return HasBlob(key, subkey);
}

bool CNetICacheClient::HasBlob(const string& key, const string& subkey,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    string response(m_Impl->ExecStdCmd("HASB", key, 0, subkey, &parameters));
    return (response[0] == '1'|| NStr::StartsWith(response, "0, VER="));
}

void CNetICacheClient::Purge(time_t access_timeout)
{
    Purge(kEmptyStr, kEmptyStr, access_timeout);
}


void CNetICacheClient::Purge(const string&    key,
                             const string&    subkey,
                             time_t           access_timeout)
{
    if (access_timeout) {
        NCBI_THROW(CNetCacheException, eNotImplemented, "Not implemented");
    }

    if (!subkey.empty()) {
        if (key.empty()) {
            NCBI_THROW(CNetCacheException, eNotImplemented, "Not implemented");
        }

        return RemoveBlob(key, 0, subkey);
    }

    const auto cmd = m_Impl->MakeStdCmd("PURGE2", "'" + key + "'", &m_Impl->m_DefaultParameters);
    m_Impl->ChooseServerAndExec(cmd, key, false, &m_Impl->m_DefaultParameters);
}

IReader* CNetICacheClient::GetReadStream(
    const string& key,
    int version,
    const string& subkey,
    size_t* blob_size_ptr,
    const CNamedParameterList* optional)
{
    return GetReadStreamPart(key, version, subkey,
        0, 0, blob_size_ptr, optional);
}

IReader* CNetICacheClient::GetReadStreamPart(
    const string& key,
    int version,
    const string& subkey,
    size_t offset,
    size_t part_size,
    size_t* blob_size_ptr,
    const CNamedParameterList* optional)
{
    return m_Impl->GetReadStreamPart(key, version, subkey,
        offset, part_size, blob_size_ptr, optional);
}

IReader* CNetICacheClient::GetReadStream(const string&  key,
                                         int            version,
                                         const string&  subkey)
{
    return GetReadStream(key, version, subkey, NULL,
            nc_caching_mode = CNetCacheAPI::eCaching_AppDefault);
}

IReader* CNetICacheClient::GetReadStream(const string& key,
        const string& subkey, int* version,
        size_t* blob_size_ptr, const CNamedParameterList* optional)
{
    ICache::EBlobVersionValidity not_used;
    return m_Impl->ReadCurrentBlobNotOlderThan(key, subkey, blob_size_ptr,
            version, &not_used, 0, NULL, optional);
}

IReader* CNetICacheClient::GetReadStream(const string& key,
        const string& subkey, int* version,
        ICache::EBlobVersionValidity* validity)
{
    return m_Impl->ReadCurrentBlobNotOlderThan(key, subkey, NULL,
            version, validity, 0, NULL);
}

IReader* SNetICacheClientImpl::ReadCurrentBlobNotOlderThan(const string& key,
        const string& subkey,
        size_t* blob_size_ptr,
        int* version,
        ICache::EBlobVersionValidity* validity,
        unsigned max_age, unsigned* actual_age,
        const CNamedParameterList* optional)
{
    try {
        string blob_id(s_KeySubkeyToBlobID(key, subkey));
        CNetCacheAPIParameters parameters(&m_DefaultParameters);

        parameters.LoadNamedParameters(optional);

        if (max_age != 0) {
            parameters.SetMaxBlobAge(max_age);
        }

        string cmd = MakeStdCmd("READLAST", blob_id, &parameters);

        CNetServer::SExecResult exec_result(ChooseServerAndExec(cmd,
                key, false, &m_DefaultParameters));

        string::size_type pos = exec_result.response.find("VER=");

        if (pos == string::npos) {
            NCBI_THROW(CNetCacheException, eInvalidServerResponse,
                "No VER field in READLAST output");
        }

        *version = (int) NStr::StringToUInt(
            exec_result.response.c_str() + pos + sizeof("VER=") - 1,
            NStr::fAllowTrailingSymbols);

        pos = exec_result.response.find("VALID=");

        if (pos == string::npos) {
            NCBI_THROW(CNetCacheException, eInvalidServerResponse,
                "No VALID field in READLAST output");
        }

        switch (exec_result.response[pos + sizeof("VALID=") - 1]) {
        case 't': case 'T': case 'y': case 'Y':
            *validity = ICache::eCurrent;
            break;
        case 'f': case 'F': case 'n': case 'N':
            *validity = ICache::eExpired;
            break;
        default:
            NCBI_THROW(CNetCacheException, eInvalidServerResponse,
                "Invalid format of the VALID field in READLAST output");
        }

        if (max_age > 0)
            *actual_age = x_ExtractBlobAge(exec_result, "READLAST");

        return new CNetCacheReader(this, blob_id, exec_result,
                blob_size_ptr, &m_DefaultParameters);
    } catch (CNetCacheBlobTooOldException& e) {
        if (actual_age != NULL)
            *actual_age = e.GetAge();
        *version = e.GetVersion();

        return NULL;
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() != CNetCacheException::eBlobNotFound)
            throw;
        return NULL;
    }
}

class CSetValidWarningSuppressor
{
public:
    CSetValidWarningSuppressor(
            INetServerConnectionListener* listener,
            const string& key,
            const string& subkey,
            int version) :
        m_Listener(listener),
        m_Key(key),
        m_Subkey(subkey),
        m_Version(version)
    {
        _ASSERT(m_Listener);

        auto warning_handler = [&](const string& m, CNetServer s) {
            return OnWarning(m, s);
        };

        m_Listener->SetWarningHandler(warning_handler);
    }

    ~CSetValidWarningSuppressor()
    {
        m_Listener->SetWarningHandler(nullptr);
    }

private:
    bool OnWarning(const string& warn_msg, CNetServer server);

    CRef<INetServerConnectionListener> m_Listener;
    const string m_Key;
    const string m_Subkey;
    const int m_Version;
};

bool CSetValidWarningSuppressor::OnWarning(const string& warn_msg, CNetServer)
{
    SIZE_TYPE ver_pos = NStr::FindCase(warn_msg,
            CTempString("VER=", sizeof("VER=") - 1));

    if (ver_pos == NPOS)
        return false;
    else {
        int version = atoi(warn_msg.c_str() + ver_pos + sizeof("VER=") - 1);
        if (version < m_Version) {
            ERR_POST("Cache actualization error (key \"" << m_Key <<
                    "\", subkey \"" << m_Subkey <<
                    "\"): the cached blob version downgraded from " <<
                    m_Version << " to " << version);
        }
    }

    return true;
}

void CNetICacheClient::SetBlobVersionAsCurrent(const string& key,
        const string& subkey, int version)
{
    CSetValidWarningSuppressor warning_suppressor(m_Impl->m_Service->m_Listener, key, subkey, version);

    CNetServer::SExecResult exec_result(
            m_Impl->ChooseServerAndExec(
                    m_Impl->MakeStdCmd("SETVALID",
                            s_KeyVersionSubkeyToBlobID(key, version, subkey),
                            &m_Impl->m_DefaultParameters),
                    key,
                    false,
                    &m_Impl->m_DefaultParameters));

    if (!exec_result.response.empty()) {
        ERR_POST("SetBlobVersionAsCurrent(\"" << key << "\", " <<
            version << ", \"" << subkey << "\"): " << exec_result.response);
    }
}

CNetServerMultilineCmdOutput CNetICacheClient::GetBlobInfo(const string& key,
        int version, const string& subkey,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    CNetServerMultilineCmdOutput output(
            m_Impl->ChooseServerAndExec(
                    m_Impl->MakeStdCmd("GETMETA",
                            s_KeyVersionSubkeyToBlobID(key, version, subkey),
                            &parameters),
                    key,
                    true,
                    &parameters));

    output->SetNetCacheCompatMode();

    return output;
}

void CNetICacheClient::PrintBlobInfo(const string& key,
        int version, const string& subkey)
{
    CNetServerMultilineCmdOutput output(GetBlobInfo(key, version, subkey));

    string line;

    if (output.ReadLine(line)) {
        if (!NStr::StartsWith(line, "SIZE="))
            NcbiCout << line << NcbiEndl;
        while (output.ReadLine(line))
            NcbiCout << line << NcbiEndl;
    }
}

CNetService CNetICacheClient::GetService()
{
    return m_Impl->m_Service;
}

string SNetICacheClientImpl::MakeStdCmd(const char* cmd_base,
    const string& blob_id, const CNetCacheAPIParameters* parameters,
    const string& injection)
{
    string cmd("IC(" + NStr::PrintableString(parameters->GetCacheName()));
    cmd.append(") ");

    cmd.append(cmd_base);

    cmd.push_back(' ');

    cmd.append(blob_id);

    if (!injection.empty())
        cmd.append(injection);

    AppendClientIPSessionIDPasswordAgeHitID(&cmd, parameters);

    return cmd;
}

string SNetICacheClientImpl::ExecStdCmd(const char* cmd_base,
    const string& key, int version, const string& subkey,
    const CNetCacheAPIParameters* parameters)
{
    return ChooseServerAndExec(
            MakeStdCmd(cmd_base,
                    s_KeyVersionSubkeyToBlobID(key, version, subkey),
                    parameters),
            key,
            false,
            parameters).response;
}

string CNetICacheClient::GetCacheName(void) const
{
    return m_Impl->m_DefaultParameters.GetCacheName();
}


bool CNetICacheClient::SameCacheParams(const TCacheParams*) const
{
    return false;
}

CNetICacheClientExt CNetICacheClientExt::GetServer(CNetServer::TInstance server)
{
    return new SNetICacheClientImpl(server->m_ServerInPool, m_Impl);
}

vector<CNetICacheClient::CBlobInfo> CNetICacheClient::Search(
        CNetICacheClient::CExpression expression,
        CNetICacheClient::CFields filter)
{
    const auto parameters = &m_Impl->m_DefaultParameters;
    const auto cache_name = NStr::PrintableString(parameters->GetCacheName());
    string ids;
    m_Impl->AppendClientIPSessionIDPasswordAgeHitID(&ids, parameters);
    ostringstream oss;
    oss << "IC(" << cache_name << ") BLIST2" << expression + filter << ids;

    CNetServerMultilineCmdOutput output(
            m_Impl->ChooseServerAndExec(
                oss.str(),
                kEmptyStr,
                true,
                &m_Impl->m_DefaultParameters));

    output->SetNetCacheCompatMode();
    string line;
    vector<CNetICacheClient::CBlobInfo> result;

    while (output.ReadLine(line) && !line.empty()) {
        CBlobInfo blob_info;
        blob_info << line;
        result.push_back(blob_info);
    }

    return result;
}

void CNetICacheClientExt::ProlongBlobLifetime(const string& key,
        const CTimeout& ttl, const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);
    parameters.LoadNamedParameters(optional);

    string cmd("PROLONG \"");
    cmd += NStr::PrintableString(parameters.GetCacheName());
    cmd += "\" \"";
    cmd += key;
    cmd += "\" \"\" ttl=";
    cmd += NStr::NumericToString((unsigned)ttl.GetAsDouble());

    m_Impl->AppendClientIPSessionIDHitID(&cmd);
    m_Impl->ChooseServerAndExec(cmd, key, false, &m_Impl->m_DefaultParameters);
}

SNetCacheAPIImpl* CNetICacheClientExt::GetNetCacheAPI()
{
    return new SNetCacheAPIImpl(m_Impl.GetObject());
}

const SNetCacheAPIImpl* CNetICacheClientExt::GetNetCacheAPI() const
{
    return new SNetCacheAPIImpl(m_Impl.GetObject());
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

private:
    virtual ICache* x_CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;
};


ICache* CNetICacheCF::x_CreateInstance(
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
