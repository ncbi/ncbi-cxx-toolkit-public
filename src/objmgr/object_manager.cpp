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

#include <objects/seqset/Seq_entry.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSafeStaticRef<CObjectManager> s_ObjectManager;

CObjectManager& CObjectManager::GetObjectManager(void)
{
    return s_ObjectManager.Get();
}


CObjectManager::CObjectManager(void)
    : m_Seq_id_Mapper(CSeq_id_Mapper::GetInstance())
{
    //LOG_POST("CObjectManager - new :" << this );
}


CObjectManager::~CObjectManager(void)
{
    //LOG_POST("~CObjectManager - delete " << this );
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
            ERR_POST("Attempt to delete Object Manager with used data sources");
        }
        m_mapToSource.erase(m_mapToSource.begin());
    }
    // LOG_POST("~CObjectManager - delete " << this << "  done");
}

/////////////////////////////////////////////////////////////////////////////
// configuration functions


void CObjectManager::RegisterDataLoader(CDataLoader& loader,
                                        EIsDefault   is_default,
                                        CPriorityNode::TPriority priority)
{
    TWriteLockGuard guard(m_OM_Lock);
    x_RegisterLoader(loader, priority, is_default);
}


void CObjectManager::RegisterDataLoader(CDataLoaderFactory& factory,
                                        EIsDefault is_default,
                                        CPriorityNode::TPriority priority)
{
    string loader_name = factory.GetName();
    _ASSERT(!loader_name.empty());
    // if already registered
    TWriteLockGuard guard(m_OM_Lock);
    if ( x_GetLoaderByName(loader_name) ) {
        ERR_POST(Warning <<
            "CObjectManager::RegisterDataLoader: " <<
            "data loader " << loader_name << " already registered");
        return;
    }
    // in case factory was not put in a CRef container by the caller
    // it will be deleted here
    CRef<CDataLoaderFactory> p(&factory);
    // create loader
    CDataLoader* loader = factory.Create();
    // register
    x_RegisterLoader(*loader, priority, is_default);
}


void CObjectManager::RegisterDataLoader(TFACTORY_AUTOCREATE factory,
                                        const string& loader_name,
                                        EIsDefault is_default,
                                        CPriorityNode::TPriority priority)
{
    _ASSERT(!loader_name.empty());
    // if already registered
    TWriteLockGuard guard(m_OM_Lock);
    if ( x_GetLoaderByName(loader_name) ) {
        ERR_POST(Warning <<
            "CObjectManager::RegisterDataLoader: " <<
            "data loader " << loader_name << " already registered");
        return;
    }
    // create one
    CDataLoader* loader = CREATE_AUTOCREATE(CDataLoader,factory);
    loader->SetName(loader_name);
    // register
    x_RegisterLoader(*loader, priority, is_default);
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
    return lock;
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
    return lock;
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


/*
void CObjectManager::RegisterTopLevelSeqEntry(CSeq_entry& top_entry)
{
    TWriteLockGuard guard(m_OM_Mutex);
    x_RegisterTSE(top_entry);
}
*/

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
CObjectManager::AcquireTopLevelSeqEntry(CSeq_entry& top_entry)
{
    TReadLockGuard guard(m_OM_Lock);
    TDataSourceLock lock = x_FindDataSource(&top_entry);
    if ( !lock ) {
        guard.Release();
        
        TDataSourceLock source(new CDataSource(top_entry, *this));
        source->DoDeleteThisObject();
        
        TWriteLockGuard wguard(m_OM_Lock);
        lock = m_mapToSource.insert(
            TMapToSource::value_type(&top_entry, source)).first->second;
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
        _ASSERT(it != m_mapToSource.end() && bool(it->second));
        return it->second;
    }
    ins.first->second = &loader;
    // create data source
    TDataSourceLock source(new CDataSource(loader, *this));
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


CObjectManager::TDataSourceLock
CObjectManager::x_RegisterTSE(CSeq_entry& top_entry)
{
    TDataSourceLock ret = x_FindDataSource(&top_entry);
    if ( !ret ) {
        TDataSourceLock source(new CDataSource(top_entry, *this));
        source->DoDeleteThisObject();

        ret = m_mapToSource.insert(
            TMapToSource::value_type(&top_entry, source)).first->second;
    }
    
    _ASSERT(ret);
    return ret;
}

CDataLoader* CObjectManager::x_GetLoaderByName(const string& name) const
{
    TMapNameToLoader::const_iterator itMap = m_mapNameToLoader.find(name);
    return itMap == m_mapNameToLoader.end()? 0: itMap->second;
}

bool CObjectManager::ReleaseDataSource(TDataSourceLock& pSource)
{
    CDataSource& ds = *pSource;
    _ASSERT(pSource->Referenced());
    CDataLoader* loader = ds.GetDataLoader();
    if ( loader ) {
        pSource.Reset();
        return false;
    }

    CConstRef<CSeq_entry> key = ds.GetTopEntry();
    if ( !key ) {
        ERR_POST("CObjectManager::ReleaseDataSource: "
                 "unknown data source key");
        pSource.Reset();
        return false;
    }

    TWriteLockGuard guard(m_OM_Lock);
    TMapToSource::iterator iter = m_mapToSource.find(key.GetPointer());
    if ( iter == m_mapToSource.end() ) {
        guard.Release();
        ERR_POST("CObjectManager::ReleaseDataSource: "
                 "unknown data source");
        pSource.Reset();
        return false;
    }
    _ASSERT(pSource == iter->second);
    _ASSERT(ds.Referenced() && !ds.ReferencedOnlyOnce());
    pSource.Reset();
    if ( ds.ReferencedOnlyOnce() ) {
        // Destroy data source if it's linked to an entry and is not
        // referenced by any scope.
        if ( !ds.GetDataLoader() ) {
            pSource = iter->second;
            m_mapToSource.erase(iter);
            _ASSERT(ds.ReferencedOnlyOnce());
            guard.Release();
            pSource.Reset();
            return true;
        }
    }
    return false;
}

/*
CConstRef<CBioseq> CObjectManager::GetBioseq(const CSeq_id& id)
{
    CScope* pScope = *(m_setScope.begin());
    return CConstRef<CBioseq>(&((pScope->GetBioseqHandle(id)).GetBioseq()));
}
*/

void CObjectManager::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CObjectManager");
    CObject::DebugDump( ddc, depth);

    if (depth == 0) {
        DebugDumpValue(ddc,"m_setDefaultSource.size()", m_setDefaultSource.size());
        DebugDumpValue(ddc,"m_mapNameToLoader.size()",  m_mapNameToLoader.size());
        DebugDumpValue(ddc,"m_mapToSource.size()",m_mapToSource.size());
        DebugDumpValue(ddc,"m_setScope.size()",         m_setScope.size());
    } else {
        
        DebugDumpRangePtr(ddc,"m_setDefaultSource",
            m_setDefaultSource.begin(), m_setDefaultSource.end(), depth);
        DebugDumpPairsValuePtr(ddc,"m_mapNameToLoader",
            m_mapNameToLoader.begin(), m_mapNameToLoader.end(), depth);
        DebugDumpPairsPtrPtr(ddc,"m_mapToSource",
            m_mapToSource.begin(), m_mapToSource.end(), depth);
        DebugDumpRangePtr(ddc,"m_setScope",
            m_setScope.begin(), m_setScope.end(), depth);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
