#ifndef CONNECT_SERVICES___NETCACHE_API_IMPL__HPP
#define CONNECT_SERVICES___NETCACHE_API_IMPL__HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 */

#include "netcache_rw.hpp"
#include "netservice_api_impl.hpp"
#include <connect/services/netcache_search.hpp>

BEGIN_NCBI_SCOPE

struct SNetCacheServerProperties : public INetServerProperties
{
    SNetCacheServerProperties() :
        mirroring_checked(false),
        mirrored(false)
    {
    }

    CFastMutex m_Mutex;

    bool mirroring_checked;
    bool mirrored;
};

class NCBI_XCONNECT_EXPORT CNetCacheServerListener : public INetServerConnectionListener
{
public:
    void SetAuthString(const string& auth) { m_Auth = auth; }

    TPropCreator GetPropCreator() const override;
    INetServerConnectionListener* Clone() override;

    void OnConnected(CNetServerConnection& connection) override;

private:
    void OnErrorImpl(const string& err_msg, CNetServer& server) override;
    void OnWarningImpl(const string& warn_msg, CNetServer& server) override;

    string m_Auth;
};

struct NCBI_XCONNECT_EXPORT SNetCacheAPIImpl : public CObject
{
    SNetCacheAPIImpl(CSynRegistryBuilder registry_builder, const string& section,
            const string& service, const string& client_name,
            CNetScheduleAPI::TInstance ns_api);

    // For use by SNetICacheClientImpl
    SNetCacheAPIImpl();

    // Special constructor for CNetCacheAPI::GetServer().
    SNetCacheAPIImpl(SNetServerInPool* server, SNetCacheAPIImpl* parent);

    CNetCacheServerListener* GetListener()
    {
        return static_cast<CNetCacheServerListener*>(m_Service->m_Listener.GetPointer());
    }

    CNetCacheReader* GetPartReader(
        const string& blob_id,
        size_t offset,
        size_t part_size,
        size_t* blob_size,
        const CNamedParameterList* optional);

    static CNetCacheAPI::EReadResult ReadBuffer(
        IReader& reader,
        char* buf_ptr,
        size_t buf_size,
        size_t* n_read,
        size_t blob_size);

    virtual CNetServerConnection InitiateWriteCmd(CNetCacheWriter* nc_writer,
            const CNetCacheAPIParameters* parameters);

    void AppendClientIPSessionID(string* cmd, CRequestContext& req);
    void AppendHitID(string* cmd, CRequestContext& req);
    void AppendClientIPSessionIDHitID(string* cmd);
    void AppendClientIPSessionIDPasswordAgeHitID(string* cmd,
            const CNetCacheAPIParameters* parameters);
    string MakeCmd(const char* cmd_base, const CNetCacheKey& key,
            const CNetCacheAPIParameters* parameters);

    unsigned x_ExtractBlobAge(const CNetServer::SExecResult& exec_result,
            const char* cmd_name);

    CNetServer::SExecResult ExecMirrorAware(
        const CNetCacheKey& key, const string& cmd,
        bool multiline_output,
        const CNetCacheAPIParameters* parameters,
        SNetServiceImpl::EServerErrorHandling error_handling =
            SNetServiceImpl::eRethrowServerErrors);

    void Init(CSynRegistry& registry, const SRegSynonyms& sections);

    CNetService m_Service;

    SNetServiceMap m_ServiceMap;

    string m_TempDir;
    bool m_CacheInput;
    bool m_CacheOutput;

    CNetScheduleAPI m_NetScheduleAPI;

    CNetCacheAPIParameters m_DefaultParameters;

    CCompoundIDPool m_CompoundIDPool;

    size_t m_FlagsOnWrite = 0;
};

struct SNetCacheAdminImpl : public CObject
{
    SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl) : m_API(nc_api_impl) {}

    void ExecOnAllServers(string cmd);
    void PrintCmdOutput(string cmd, CNcbiOstream& output_stream, bool multiline_output = true);

    CNetCacheAPI m_API;
};

class NCBI_XCONNECT_EXPORT SNetCacheMirrorTraversal : public IServiceTraversal
{
public:
    SNetCacheMirrorTraversal(CNetService::TInstance service,
            CNetServer::TInstance primary_server, ESwitch server_check) :
        m_Service(service),
        m_PrimaryServer(primary_server),
        m_PrimaryServerCheck(server_check != eOff)
    {
    }

    virtual CNetServer BeginIteration();
    virtual CNetServer NextServer();

    CNetService m_Service;
    CNetServiceIterator m_Iterator;
    CNetServer::TInstance m_PrimaryServer;
    bool m_PrimaryServerCheck;
};

namespace grid {
namespace netcache {
namespace search {

NCBI_XCONNECT_EXPORT CExpression operator+(CExpression l, CFields r);
NCBI_XCONNECT_EXPORT ostream& operator<<(ostream& os, CExpression expression);
NCBI_XCONNECT_EXPORT void operator<<(CBlobInfo& blob_info, string data);

}
}
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETCACHE_API_IMPL__HPP */
