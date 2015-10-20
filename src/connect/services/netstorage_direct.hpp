#ifndef CONNECT_SERVICES__NETSTORAGE_DIRECT__HPP
#define CONNECT_SERVICES__NETSTORAGE_DIRECT__HPP

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
 * Authors: Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include "netstorage_direct_object.hpp"

#include <connect/services/netstorage.hpp>

BEGIN_NCBI_SCOPE

using namespace NDirectNetStorageImpl;

struct SDirectNetStorageImpl : public SNetStorageImpl
{
    SDirectNetStorageImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
    }

    CObj* OpenImpl(const string&);

    CNetStorageObject Create(TNetStorageFlags = 0);
    CNetStorageObject Open(const string&);
    string Relocate(const string&, TNetStorageFlags);
    bool Exists(const string&);
    void Remove(const string&);

    CObj* Create(TNetStorageFlags, const string&, Int8 = 0);

private:
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections() { m_Context->AllowXSiteConnections(); }
#endif

    CRef<SContext> m_Context;
};

struct SDirectNetStorageByKeyImpl : public SNetStorageByKeyImpl
{
    SDirectNetStorageByKeyImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
        if (app_domain.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "Domain name cannot be empty.");
        }
    }

    CObj* OpenImpl(const string&, TNetStorageFlags = 0);

    CNetStorageObject Open(const string&, TNetStorageFlags = 0);
    string Relocate(const string&, TNetStorageFlags, TNetStorageFlags = 0);
    bool Exists(const string&, TNetStorageFlags = 0);
    void Remove(const string&, TNetStorageFlags = 0);

private:
    CRef<SContext> m_Context;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSTORAGE_DIRECT__HPP */
