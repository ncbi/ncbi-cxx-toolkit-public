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

#include "netservice_api_impl.hpp"

#include <connect/services/netcache_rw.hpp>

BEGIN_NCBI_SCOPE

struct SNetCacheAPIImpl : public CNetObject
{
    SNetCacheAPIImpl(const string& service,
        const string& client_name,
        const std::string& lbsm_affinity_name);

    class CNetCacheServerListener : public INetServerConnectionListener
    {
    public:
        CNetCacheServerListener(const std::string& client_name);

    private:
        virtual void OnConnected(CNetServerConnection::TPtr);
        virtual void OnError(const string& err_msg,
            SNetServerConnectionPoolImpl* pool);

    private:
        std::string m_Auth;
    };

    static CNetCacheAPI::EReadResult x_ReadBuffer(
        IReader& reader,
        void* buf,
        size_t buf_size,
        size_t* n_read,
        size_t blob_size);

    CNetServerConnection GetBestConnection();
    CNetServerConnection x_GetConnection(const string& bid);
    CNetServerConnection x_PutInitiate(
        string*  key, unsigned  time_to_live);

    string x_MakeCommand(const string& cmd) const;

    CNetService m_Service;

    CNetObjectRef<CNetCacheServerListener> m_Listener;

    bool m_NoHasBlob;
};

inline SNetCacheAPIImpl::SNetCacheAPIImpl(const std::string& service,
    const std::string& client_name,
    const std::string& lbsm_affinity_name) :
    m_Service(new SNetServiceImpl(service, client_name, lbsm_affinity_name)),
    m_NoHasBlob(false)
{
    m_Listener = new CNetCacheServerListener(m_Service.GetClientName());

    m_Service->SetListener(m_Listener);
}

struct SNetCacheAdminImpl : public CNetObject
{
    SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl);

    CNetCacheAPI m_API;
};

inline SNetCacheAdminImpl::SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl) :
    m_API(nc_api_impl)
{
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETCACHE_API_IMPL__HPP */
