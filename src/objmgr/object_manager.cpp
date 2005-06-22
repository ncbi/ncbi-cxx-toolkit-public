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

#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const string kObjectManagerPtrName = "ObjectManagerPtr";

typedef CObjectManager TInstance;

static TInstance* s_Instance = 0;
DEFINE_STATIC_FAST_MUTEX(s_InstanceMutex);

CRef<TInstance> TInstance::GetInstance(void)
{
    CRef<TInstance> ret;
    {{
        CFastMutexGuard guard(s_InstanceMutex);
        ret.Reset(s_Instance);
        if ( !ret || ret->ReferencedOnlyOnce() ) {
            if ( ret ) {
                ret.Release();
            }
            ret.Reset(new TInstance);
            s_Instance = ret;
        }
    }}
    _ASSERT(ret == s_Instance);
    return ret;
}


static void s_ResetInstance(TInstance* instance)
{
    CFastMutexGuard guard(s_InstanceMutex);
    if ( s_Instance == instance ) {
        s_Instance = 0;
    }
}


CObjectManager::CObjectManager(void)
    : m_Seq_id_Mapper(CSeq_id_Mapper::GetInstance())
{
}


CObjectManager::~CObjectManager(void)
{
    s_ResetInstance(this);
    // delete scopes
    TWriteLockGuard guard(m_OM_Lock);

    if(!m_setScope.empty()) {
        ERR_POST("Attempt to delete Object Manager with open scopes");
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
            ERR_POST("Attempt to delete Object Manager with used datasources");
        }
        m_mapToSource.erase(m_mapToSource.begin());
    }
    // LOG_POST("~CObjectManager - delete " << this << "  done");
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
        ERR_POST(Warning <<
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
        ERR_POST("CObjectManager::RevokeDataLoader: "
                 "data loader is in use");
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
    if (priority != kPriority_NotSet  &&
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
}


void CObjectManager::RevokeScope(CScope_Impl& scope)
{
    TWriteLockGuard guard(m_OM_ScopeLock);
    _VERIFY(m_setScope.erase(&scope));
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
        lock = x_RegisterLoader(loader, kPriority_NotSet, eNonDefault, true);
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
        m_mapNameToLoader.insert(TMapNameToLoader::value_type(loader_name,0));
    if ( !ins.second ) {
        if ( ins.first->second != &loader ) {
            NCBI_THROW(CObjMgrException, eRegisterError,
                "Attempt to register different data loaders "
                "with the same name");
        }
        if ( !no_warning ) {
            ERR_POST(Warning <<
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
    if (priority != kPriority_NotSet) {
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
        ERR_POST("CObjectManager::ReleaseDataSource: "
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.48  2005/06/22 14:13:23  vasilche
* Removed obsolete methods.
* Register only shared Seq-entries.
*
* Revision 1.47  2005/01/27 17:57:29  grichenk
* Throw exception if loader can not be created.
*
* Revision 1.46  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.45  2004/12/22 15:56:10  vasilche
* ReleaseDataSource made public.
* Removed obsolete DebugDump.
*
* Revision 1.44  2004/12/13 15:19:20  grichenk
* Doxygenized comments
*
* Revision 1.43  2004/09/30 18:38:55  vasilche
* Thread safe CObjectManager::GetInstance().
*
* Revision 1.42  2004/08/24 16:42:03  vasilche
* Removed TAB symbols in sources.
*
* Revision 1.41  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.40  2004/07/30 14:23:55  ucko
* Make kObjectManagerPtrName extern (defined in object_manager.cpp) to
* ensure that it's always properly available on WorkShop.
*
* Revision 1.39  2004/07/28 14:02:57  grichenk
* Improved MT-safety of RegisterInObjectManager(), simplified the code.
*
* Revision 1.38  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.37  2004/07/21 15:51:25  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.36  2004/07/12 15:24:59  grichenk
* Reverted changes related to CObjectManager singleton
*
* Revision 1.35  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.34  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.33  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.32  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.31  2004/03/17 16:05:21  vasilche
* IRIX CC won't implicitly convert CRef<CSeq_entry> to CObject*
*
* Revision 1.30  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.29  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.28  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.27  2003/08/04 17:04:31  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.26  2003/06/19 18:34:28  vasilche
* Fixed compilation on Windows.
*
* Revision 1.25  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.23  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.22  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.21  2003/04/09 16:04:32  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.20  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.19  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.18  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.17  2002/11/08 21:07:15  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.16  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.15  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.14  2002/07/12 18:33:23  grichenk
* Fixed the bug with querying destroyed datasources
*
* Revision 1.13  2002/07/10 21:11:36  grichenk
* Destroy any data source not linked to a loader
* and not used by any scope.
*
* Revision 1.12  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.11  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.10  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.9  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.8  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.7  2002/04/22 20:04:11  grichenk
* Fixed TSE dropping
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/01/29 17:45:21  grichenk
* Removed debug output
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:53:29  gouriano
* implemented RegisterTopLevelSeqEntry
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:21  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
