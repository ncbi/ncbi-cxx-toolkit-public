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

class NCBI_XCONNECT_EXPORT CNetCacheServerListener :
    public INetServerConnectionListener
{
public:
    virtual CRef<INetServerProperties> AllocServerProperties();

public:
    virtual CConfig* OnPreInit(CObject* api_impl,
        CConfig* config, string* config_section, string& client_name);
    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection& connection);
    virtual void OnError(const string& err_msg, CNetServer& server);
    virtual void OnWarning(const string& warn_msg, CNetServer& server);

    static CRef<SNetCacheServerProperties> x_GetServerProperties(
            SNetServerImpl* server_impl);

    string m_Auth;
    CRef<INetEventHandler> m_EventHandler;
};

struct NCBI_XCONNECT_EXPORT SNetCacheAPIImpl : public CObject
{
    SNetCacheAPIImpl(CConfig* config, const string& section,
            const string& service, const string& client_name,
            CNetScheduleAPI::TInstance ns_api);

    // For use by SNetICacheClientImpl
    SNetCacheAPIImpl(const string& api_name, const string& client_name,
            INetServerConnectionListener* listener);

    // Special constructor for CNetCacheAPI::GetServer().
    SNetCacheAPIImpl(SNetServerInPool* server, SNetCacheAPIImpl* parent);

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

    void AppendClientIPSessionID(string* cmd);
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
            SNetServiceImpl::eRethrowServerErrors,
        INetServerConnectionListener* conn_listener = NULL);

    CNetCacheServerListener* GetListener()
    {
        return static_cast<CNetCacheServerListener*>(
                m_Service->m_Listener.GetPointer());
    }

    CNetService m_Service;

    SNetServiceMap m_ServiceMap;

    string m_TempDir;
    bool m_CacheInput;
    bool m_CacheOutput;

    CNetScheduleAPI m_NetScheduleAPI;

    CNetCacheAPIParameters m_DefaultParameters;

    CCompoundIDPool m_CompoundIDPool;
};

struct SNetCacheAdminImpl : public CObject
{
    SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl);

    string MakeAdminCmd(const char* cmd);

    CNetCacheAPI m_API;
};

inline SNetCacheAdminImpl::SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl) :
    m_API(nc_api_impl)
{
}

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

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETCACHE_API_IMPL__HPP */
