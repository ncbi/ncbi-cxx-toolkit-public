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
* Author: Eugene Vasilchenko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Definition CGI application class and its context class.
*/

#include <ncbi_pch.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <cgi/cgiapp_cached.hpp>

#include <util/cache/icache.hpp>

BEGIN_NCBI_SCOPE

NCBI_PARAM_DEF(string, CGI, ResultCacheSectionName, "result_cache");

CCgiApplicationCached::CCgiApplicationCached(void)
    : m_CacheTreeParams(NULL)
{
}

CCgiApplicationCached::~CCgiApplicationCached(void)
{
}


void CCgiApplicationCached::Init(void)
{
    CCgiApplication::Init();

    const TPluginManagerParamTree* params = CConfig::ConvertRegToTree(GetConfig());
    if (params) {
        const TPluginManagerParamTree* cache_tree = 
            params->FindSubNode(TCGI_ResultCacheSectionName::GetDefault());
        if (cache_tree) {
            const TPluginManagerParamTree* driver_tree = 
                cache_tree->FindSubNode("driver");
            if (driver_tree  && !driver_tree->GetValue().value.empty()) {
                m_CacheDriverName = driver_tree->GetValue().value;
                m_CacheTreeParams = params->FindSubNode(m_CacheDriverName);
            }
        }
    }
}


ICache* CCgiApplicationCached::GetCacheStorage() const
{ 
    if (!m_CacheTreeParams || m_CacheDriverName.empty() )
        return NULL;
    
    typedef CPluginManager<ICache> TCacheManager; 
    typedef CPluginManagerGetter<ICache> TCacheManagerStore;
 
     CRef<TCacheManager> cache_manager( TCacheManagerStore::Get() );
    
     _ASSERT( cache_manager );
      
     return cache_manager->CreateInstance(
         m_CacheDriverName,
         NCBI_INTERFACE_VERSION(ICache),
         m_CacheTreeParams
         );
}

END_NCBI_SCOPE
