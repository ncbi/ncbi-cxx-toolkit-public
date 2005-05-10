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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/services/grid_default_factories.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//

CNetScheduleStorageFactory_NetCache::
        CNetScheduleStorageFactory_NetCache(const IRegistry& reg,
                                            bool             cache_input,
                                            bool             cache_output)
            : m_Registry(reg), m_CacheInput(cache_input), 
              m_CacheOutput(cache_output)
{
    m_PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);
    vector<string> masks;
    masks.push_back( CNetCacheNSStorage::sm_InputBlobCachePrefix + "*" );
    masks.push_back( CNetCacheNSStorage::sm_OutputBlobCachePrefix + "*" );
    CDir curr_dir(".");
    CDir::TEntries dir_entries = curr_dir.GetEntries(masks, 
                                                     CDir::eIgnoreRecursive);
    ITERATE( CDir::TEntries, it, dir_entries) {
        (*it)->Remove(CDirEntry::eNonRecursive);
    }
}

INetScheduleStorage* 
CNetScheduleStorageFactory_NetCache::CreateInstance(void)
{
    CConfig conf(m_Registry);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* netcache_tree = 
            param_tree->FindSubNode(kNetCacheDriverName);

    auto_ptr<CNetCacheClient> nc_client;
    if (netcache_tree) {
        nc_client.reset( 
            m_PM_NetCache.CreateInstance(
                    kNetCacheDriverName,
                    CVersionInfo(TPMNetCache::TInterfaceVersion::eMajor,
                                 TPMNetCache::TInterfaceVersion::eMinor,
                                 TPMNetCache::TInterfaceVersion::ePatchLevel), 
                                 netcache_tree)
                       );
    }
    if (!nc_client.get())
        NCBI_THROW(CGridFactoriesException,
                   eNCClientIsNotCreated, 
                   "Couldn't create NetCache client."
                   "Check registry.");

    return new CNetCacheNSStorage(nc_client.release(),
                                  m_CacheInput, 
                                  m_CacheOutput);
}



//////////////////////////////////////////////////////////////////////////////
//
CNetScheduleClientFactory::CNetScheduleClientFactory(const IRegistry& reg)
: m_Registry(reg)
{
    m_PM_NetSchedule.RegisterWithEntryPoint(NCBI_EntryPoint_xnetschedule);
}


CNetScheduleClient* CNetScheduleClientFactory::CreateInstance(void)
{
    auto_ptr<CNetScheduleClient> ret;

    CConfig conf(m_Registry);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* netschedule_tree = 
            param_tree->FindSubNode(kNetScheduleDriverName);

    if (netschedule_tree) {
        ret.reset( 
            m_PM_NetSchedule.CreateInstance(
                    kNetScheduleDriverName,
                    CVersionInfo(TPMNetSchedule::TInterfaceVersion::eMajor,
                                 TPMNetSchedule::TInterfaceVersion::eMinor,
                                 TPMNetSchedule::TInterfaceVersion::ePatchLevel), 
                                 netschedule_tree)
                                 );
        ret->ActivateRequestRateControl(false);
    }
    if (!ret.get())
        NCBI_THROW(CGridFactoriesException,
                   eNSClientIsNotCreated, 
                   "Couldn't create NetSchedule client."
                   "Check registry.");

    return ret.release();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2005/05/10 15:15:14  didenko
 * Added clean up procedure
 *
 * ===========================================================================
 */
 
