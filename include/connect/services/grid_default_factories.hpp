#ifndef CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP
#define CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP

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
 * Authors:  Maxim Didenko
 *
 */

#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbiexpt.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class NCBI_XCONNECT_EXPORT CGridFactoriesException : public CException
{
public:
    enum EErrCode {
        eNSClientIsNotCreated,
        eNCClientIsNotCreated
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eNSClientIsNotCreated: 
            return "eNSClientIsNotCreatedError";
        case eNCClientIsNotCreated: 
            return "eNCClientIsNotCreatedError";
        default:      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridFactoriesException, CException);
};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CNetScheduleStorageFactory_NetCache : public INetScheduleStorageFactory
{
public:
    CNetScheduleStorageFactory_NetCache(const IRegistry& reg);
    virtual ~CNetScheduleStorageFactory_NetCache() {}

    virtual INetScheduleStorage* CreateInstance(void);

private:
    typedef CPluginManager<CNetCacheClient> TPMNetCache;
    TPMNetCache               m_PM_NetCache;
    const IRegistry&          m_Registry;
};

inline
CNetScheduleStorageFactory_NetCache::
        CNetScheduleStorageFactory_NetCache(const IRegistry& reg)
: m_Registry(reg)
{
    m_PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);
}

inline 
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

    return new CNetCacheNSStorage(nc_client.release());
}



/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CNetScheduleClientFactory : public INetScheduleClientFactory
{
public:
    CNetScheduleClientFactory(const IRegistry& reg);
    virtual ~CNetScheduleClientFactory() {}

    virtual CNetScheduleClient* CreateInstance(void);

private:
    typedef CPluginManager<CNetScheduleClient> TPMNetSchedule;
    TPMNetSchedule   m_PM_NetSchedule;
    const IRegistry& m_Registry;
};

inline 
CNetScheduleClientFactory::CNetScheduleClientFactory(const IRegistry& reg)
: m_Registry(reg)
{
    m_PM_NetSchedule.RegisterWithEntryPoint(NCBI_EntryPoint_xnetschedule);
}

inline 
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
 * Revision 1.2  2005/03/28 14:38:04  didenko
 * Cosmetics
 *
 * Revision 1.1  2005/03/25 16:23:43  didenko
 * Initail version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP
