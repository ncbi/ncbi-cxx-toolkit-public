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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*
* File Description:
*           Object manager manages data objects,
*           provides them to Scopes when needed
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>

#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <corelib/ncbi_safe_static.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_Main

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(7);

BEGIN_SCOPE(objects)

#ifdef _DEBUG
namespace {

NCBI_PARAM_DECL(bool, OBJMGR, DEBUG_SCOPE);
NCBI_PARAM_DEF(bool, OBJMGR, DEBUG_SCOPE, false);

static bool s_DebugScope(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJMGR, DEBUG_SCOPE)> sx_Value;
    return sx_Value->Get();
}

typedef map<const CScope_Impl*, AutoPtr<CStackTrace> > TScopeRegisterMap;
static CSafeStatic<TScopeRegisterMap> s_ScopeRegisterMap;

void s_RegisterScope(const CScope_Impl& scope)
{
    if ( s_DebugScope() ) {
        AutoPtr<CStackTrace> st(new CStackTrace());
        s_ScopeRegisterMap.Get()[&scope] = st;
    }
}

void s_RevokeScope(const CScope_Impl& scope)
{
    if ( s_DebugScope() ) {
        s_ScopeRegisterMap.Get().erase(&scope);
    }
}

void s_DumpScopes(void)
{
    if ( s_DebugScope() ) {
        ITERATE ( TScopeRegisterMap, it, s_ScopeRegisterMap.Get() ) {
            ERR_POST("Scope "<<it->first<<" registered at "<<*it->second);
        }
    }
}

}
#endif


CRef<CObjectManager> CObjectManager::GetInstance(void)
{
    static CSafeStatic<CObjectManager> s_Instance;
    return Ref(&s_Instance.Get());
}


CObjectManager::CObjectManager(void)
    : m_Seq_id_Mapper(CSeq_id_Mapper::GetInstance())
{
}


CObjectManager::~CObjectManager(void)
{
    // delete scopes
    TWriteLockGuard guard(m_OM_Lock);

    if(!m_setScope.empty()) {
        ERR_POST_X(1, "Attempt to delete Object Manager with open scopes");
        while ( !m_setScope.empty() ) {
            // this will cause calling RegisterScope and changing m_setScope
            // be careful with data access synchronization
            (*m_setScope.begin())->x_DetachFromOM();
        }
    }
    // release data sources

    m_setDefaultSource.clear();
    
    while (!m_mapToSource.empty()) {
        CDataSource* pSource = m_mapToSource.begin()->second.GetPointer();
        _ASSERT(pSource);
        if ( !pSource->ReferencedOnlyOnce() ) {
            ERR_POST_X(2, "Attempt to delete Object Manager with used datasources");
        }
        m_mapToSource.erase(m_mapToSource.begin());
    }
    // LOG_POST_X(3, "~CObjectManager - delete " << this << "  done");
}

/////////////////////////////////////////////////////////////////////////////
// configuration functions


void CObjectManager::RegisterDataLoader(CLoaderMaker_Base& loader_maker,
                                        EIsDefault         is_default,
                                        TPriority          priority)
{
    TWriteLockGuard guard(m_OM_Lock);
    CDataLoader* loader = FindDataLoader(loader_maker.m_Name);
    if (loader) {
        loader_maker.m_RegisterInfo.Set(loader, false);
        return;
    }
    try {
        loader = loader_maker.CreateLoader();
        x_RegisterLoader(*loader, priority, is_default);
    }
    catch (CObjMgrException& e) {
        ERR_POST_X(4, Warning <<
            "CObjectManager::RegisterDataLoader: " << e.GetMsg());
        // This can happen only if something is wrong with the new loader.
        // loader_maker.m_RegisterInfo.Set(0, false);
        throw;
    }
    loader_maker.m_RegisterInfo.Set(loader, true);
}


CObjectManager::TPluginManager& CObjectManager::x_GetPluginManager(void)
{
    if (!m_PluginManager.get()) {
        TWriteLockGuard guard(m_OM_Lock);
        if (!m_PluginManager.get()) {
            m_PluginManager.reset(new TPluginManager);
        }
    }
    _ASSERT(m_PluginManager.get());
    return *m_PluginManager;
}


CDataLoader*
CObjectManager::RegisterDataLoader(TPluginManagerParamTree* params,
                                   const string& driver_name)
{
    // Check params, extract driver name, add pointer to self etc.
    //
    //
    typedef CInterfaceVersion<CDataLoader> TDLVersion;
    return x_GetPluginManager().CreateInstance(driver_name,
        CVersionInfo(TDLVersion::eMajor,
        TDLVersion::eMinor,
        TDLVersion::ePatchLevel),
        params);
}


bool CObjectManager::RevokeDataLoader(CDataLoader& loader)
{
    string loader_name = loader.GetName();

    TWriteLockGuard guard(m_OM_Lock);
    // make sure it is registered
    CDataLoader* my_loader = x_GetLoaderByName(loader_name);
    if ( my_loader != &loader ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
            "Data loader " + loader_name + " not registered");
    }
    TDataSourceLock lock = x_RevokeDataLoader(&loader);
    guard.Release();
    return lock.NotEmpty();
}


bool CObjectManager::RevokeDataLoader(const string& loader_name)
{
    TWriteLockGuard guard(m_OM_Lock);
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    // if not registered
    if ( !loader ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
            "Data loader " + loader_name + " not registered");
    }
    TDataSourceLock lock = x_RevokeDataLoader(loader);
    guard.Release();
    return lock.NotEmpty();
}


void CObjectManager::RevokeAllDataLoaders(void)
{
    TWriteLockGuard guard(m_OM_Lock);
    NON_CONST_ITERATE ( TMapToSource, iter, m_mapToSource ) {
        iter->second->RevokeDataLoader();
    }
    m_mapToSource.clear();
    m_setDefaultSource.clear();
}


CObjectManager::TDataSourceLock
CObjectManager::x_RevokeDataLoader(CDataLoader* loader)
{
    TMapToSource::iterator iter = m_mapToSource.find(loader);
    _ASSERT(iter != m_mapToSource.end());
    _ASSERT(iter->second->GetDataLoader() == loader);
    bool is_default = m_setDefaultSource.erase(iter->second) != 0;
    if ( !iter->second->ReferencedOnlyOnce() ) {
        // this means it is in use
        if ( is_default )
            _VERIFY(m_setDefaultSource.insert(iter->second).second);
        ERR_POST_X(5, "CObjectManager::RevokeDataLoader: "
                      "data loader is in use");
#ifdef _DEBUG
        s_DumpScopes();
#endif
        return TDataSourceLock();
    }
    // remove from the maps
    TDataSourceLock lock(iter->second);
    m_mapNameToLoader.erase(loader->GetName());
    m_mapToSource.erase(loader);
    return lock;
}


CDataLoader* CObjectManager::FindDataLoader(const string& loader_name) const
{
    TReadLockGuard guard(m_OM_Lock);
    return x_GetLoaderByName(loader_name);
}


void CObjectManager::GetRegisteredNames(TRegisteredNames& names)
{
    ITERATE(TMapNameToLoader, it, m_mapNameToLoader) {
        names.push_back(it->first);
    }
}


// Update loader's options
void CObjectManager::SetLoaderOptions(const string& loader_name,
                                      EIsDefault    is_default,
                                      TPriority     priority)
{
    TWriteLockGuard guard(m_OM_Lock);
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    if ( !loader ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
            "Data loader " + loader_name + " not registered");
    }
    TMapToSource::iterator data_source = m_mapToSource.find(loader);
    _ASSERT(data_source != m_mapToSource.end());
    TSetDefaultSource::iterator def_it =
        m_setDefaultSource.find(data_source->second);
    if (is_default == eDefault  &&  def_it == m_setDefaultSource.end()) {
        m_setDefaultSource.insert(data_source->second);
    }
    else if (is_default == eNonDefault && def_it != m_setDefaultSource.end()) {
        m_setDefaultSource.erase(def_it);
    }
    if (priority != kPriority_Default  &&
        data_source->second->GetDefaultPriority() != priority) {
        data_source->second->SetDefaultPriority(priority);
    }
}


/////////////////////////////////////////////////////////////////////////////
// functions for scopes

void CObjectManager::RegisterScope(CScope_Impl& scope)
{
    TWriteLockGuard guard(m_OM_ScopeLock);
    _VERIFY(m_setScope.insert(&scope).second);
#ifdef _DEBUG
    s_RegisterScope(scope);
#endif
}


void CObjectManager::RevokeScope(CScope_Impl& scope)
{
    TWriteLockGuard guard(m_OM_ScopeLock);
    _VERIFY(m_setScope.erase(&scope));
#ifdef _DEBUG
    s_RevokeScope(scope);
#endif
}


void CObjectManager::AcquireDefaultDataSources(TDataSourcesLock& sources)
{
    TReadLockGuard guard(m_OM_Lock);
    sources = m_setDefaultSource;
}


CObjectManager::TDataSourceLock
CObjectManager::AcquireDataLoader(CDataLoader& loader)
{
    TReadLockGuard guard(m_OM_Lock);
    TDataSourceLock lock = x_FindDataSource(&loader);
    if ( !lock ) {
        guard.Release();
        TWriteLockGuard wguard(m_OM_Lock);
        lock = x_RegisterLoader(loader, kPriority_Default, eNonDefault, true);
    }
    return lock;
}


CObjectManager::TDataSourceLock
CObjectManager::AcquireDataLoader(const string& loader_name)
{
    TReadLockGuard guard(m_OM_Lock);
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    if ( !loader ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
            "Data loader " + loader_name + " not found");
    }
    TDataSourceLock lock = x_FindDataSource(loader);
    _ASSERT(lock);
    return lock;
}


CObjectManager::TDataSourceLock
CObjectManager::AcquireSharedSeq_entry(const CSeq_entry& object)
{
    TReadLockGuard guard(m_OM_Lock);
    TDataSourceLock lock = x_FindDataSource(&object);
    if ( !lock ) {
        guard.Release();
        
        TDataSourceLock source(new CDataSource(object, object));
        source->DoDeleteThisObject();

        TWriteLockGuard wguard(m_OM_Lock);
        lock = m_mapToSource.insert(
            TMapToSource::value_type(&object, source)).first->second;
        _ASSERT(lock);
    }
    return lock;
}


CObjectManager::TDataSourceLock
CObjectManager::AcquireSharedBioseq(const CBioseq& object)
{
    TReadLockGuard guard(m_OM_Lock);
    TDataSourceLock lock = x_FindDataSource(&object);
    if ( !lock ) {
        guard.Release();
        
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSeq(const_cast<CBioseq&>(object));
        TDataSourceLock source(new CDataSource(object, *entry));
        source->DoDeleteThisObject();

        TWriteLockGuard wguard(m_OM_Lock);
        lock = m_mapToSource.insert(
            TMapToSource::value_type(&object, source)).first->second;
        _ASSERT(lock);
    }
    return lock;
}


CObjectManager::TDataSourceLock
CObjectManager::AcquireSharedSeq_annot(const CSeq_annot& object)
{
    TReadLockGuard guard(m_OM_Lock);
    TDataSourceLock lock = x_FindDataSource(&object);
    if ( !lock ) {
        guard.Release();
        
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSet().SetSeq_set(); // it's not optional
        entry->SetSet().SetAnnot()
            .push_back(Ref(&const_cast<CSeq_annot&>(object)));
        TDataSourceLock source(new CDataSource(object, *entry));
        source->DoDeleteThisObject();

        TWriteLockGuard wguard(m_OM_Lock);
        lock = m_mapToSource.insert(
            TMapToSource::value_type(&object, source)).first->second;
        _ASSERT(lock);
    }
    return lock;
}


/////////////////////////////////////////////////////////////////////////////
// private functions


CObjectManager::TDataSourceLock
CObjectManager::x_FindDataSource(const CObject* key)
{
    TMapToSource::iterator iter = m_mapToSource.find(key);
    return iter == m_mapToSource.end()? TDataSourceLock(): iter->second;
}


CObjectManager::TDataSourceLock
CObjectManager::x_RegisterLoader(CDataLoader& loader,
                                 CPriorityNode::TPriority priority,
                                 EIsDefault is_default,
                                 bool no_warning)
{
    string loader_name = loader.GetName();
    _ASSERT(!loader_name.empty());

    // if already registered
    pair<TMapNameToLoader::iterator, bool> ins =
        m_mapNameToLoader.insert(TMapNameToLoader::value_type(loader_name,nullptr));
    if ( !ins.second ) {
        if ( ins.first->second != &loader ) {
            NCBI_THROW(CObjMgrException, eRegisterError,
                "Attempt to register different data loaders "
                "with the same name");
        }
        if ( !no_warning ) {
            ERR_POST_X(6, Warning <<
                       "CObjectManager::RegisterDataLoader() -- data loader " <<
                       loader_name << " already registered");
        }
        TMapToSource::const_iterator it = m_mapToSource.find(&loader);
        _ASSERT(it != m_mapToSource.end() && it->second);
        return it->second;
    }
    ins.first->second = &loader;

    // create data source
    TDataSourceLock source(new CDataSource(loader));
    source->DoDeleteThisObject();
    if (priority != kPriority_Default) {
        source->SetDefaultPriority(priority);
    }
    _VERIFY(m_mapToSource.insert(TMapToSource::value_type(&loader,
                                                          source)).second);
    if (is_default == eDefault) {
        m_setDefaultSource.insert(source);
    }
    return source;
}


CDataLoader* CObjectManager::x_GetLoaderByName(const string& name) const
{
    TMapNameToLoader::const_iterator itMap = m_mapNameToLoader.find(name);
    return itMap == m_mapNameToLoader.end()? 0: itMap->second;
}


void CObjectManager::ReleaseDataSource(TDataSourceLock& pSource)
{
    CDataSource& ds = *pSource;
    _ASSERT(pSource->Referenced());
    CDataLoader* loader = ds.GetDataLoader();
    if ( loader ) {
        pSource.Reset();
        return;
    }

    CConstRef<CObject> key = ds.GetSharedObject();
    if ( !key ) {
        pSource.Reset();
        return;
    }

    TWriteLockGuard guard(m_OM_Lock);
    TMapToSource::iterator iter = m_mapToSource.find(key);
    if ( iter == m_mapToSource.end() ) {
        guard.Release();
        ERR_POST_X(7, "CObjectManager::ReleaseDataSource: "
                      "unknown data source");
        pSource.Reset();
        return;
    }
    _ASSERT(pSource == iter->second);
    _ASSERT(ds.Referenced() && !ds.ReferencedOnlyOnce());
    pSource.Reset();
    if ( ds.ReferencedOnlyOnce() ) {
        // Destroy data source if it's linked to an entry and is not
        // referenced by any scope.
        pSource = iter->second;
        m_mapToSource.erase(iter);
        _ASSERT(ds.ReferencedOnlyOnce());
        guard.Release();
        pSource.Reset();
        return;
    }
    return;
}


END_SCOPE(objects)
END_NCBI_SCOPE
