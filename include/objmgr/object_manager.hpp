#ifndef OBJECT_MANAGER__HPP
#define OBJECT_MANAGER__HPP

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
*/

/// @file object_manager.hpp
/// The Object manager core.
///
/// Handles data loaders, provides them to CScope objects.

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CDataSource;
class CDataLoader;
class CLoaderMaker_Base;
class CDataLoaderFactory;
class CSeq_entry;
class CBioseq;
class CSeq_annot;
class CSeq_id;
class CScope;
class CScope_Impl;


class CSeq_id_Mapper;

/////////////////////////////////////////////////////////////////////////////
///
///  SRegisterLoaderInfo --
///
/// Structure returned by RegisterInObjectManager() method
///

template<class TLoader>
struct SRegisterLoaderInfo
{
    /// Get pointer to the loader. The loader can be just created or
    /// already registered in the object manager. NULL if the operation
    /// failed.
    TLoader* GetLoader(void) const { return m_Loader; }
    /// Return true if the loader was just created, false if already
    /// registered or if the operation failed.
    bool     IsCreated(void) const { return m_Created; }

private:
    TLoader* m_Loader;  // pointer to the loader (created or existing)
    bool     m_Created; // true only if the loader was just created

public:
    SRegisterLoaderInfo()
        : m_Loader(0)
    {
    }

    // Used internally to populate the structure
    void Set(CDataLoader* loader, bool created)
    {
        // Check loader type
        m_Loader = dynamic_cast<TLoader*>(loader);
        if (loader  &&  !m_Loader) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "Loader name already registered for another loader type");
        }
        m_Created = created;
    }
};


/////////////////////////////////////////////////////////////////////////////
///
///  CObjectManager --
///
/// Core Class for ObjectManager Library.
/// Handles data loaders, provides them to scopes.

class NCBI_XOBJMGR_EXPORT CObjectManager : public CObject
{
public:
    /// Return the existing object manager or create one.
    static CRef<CObjectManager> GetInstance(void);
    virtual ~CObjectManager(void);

public:
    typedef CRef<CDataSource> TDataSourceLock;

// configuration functions
// this data is always available to scopes -
// by name - in case of data loader
// or by address - in case of Seq_entry

    /// Flag defining if the data loader is included in the "default" group.
    /// Default data loaders can be added to a scope using
    /// CScope::AddDefaults().
    /// @sa
    ///   CScope::AddDefaults()
    enum EIsDefault {
        eDefault,
        eNonDefault
    };

    typedef int TPriority;
    /// Default data source priority.
    enum EPriority {
        kPriority_Default = -1, ///< Use default priority for added data
        kPriority_NotSet = -1   ///< Deprecated: use kPriority_Default instead
    };

    /// Add data loader using plugin manager.
    /// @param params
    ///   Param tree containing the data loader settings.
    /// @param driver_name
    ///   Name of the driver to be used as the data loader.
    /// @return
    ///   The new data loader created by the plugin manager.
    CDataLoader* RegisterDataLoader(TPluginManagerParamTree* params = 0,
                                    const string& driver_name = kEmptyStr);

    /// Try to find a registered data loader by name.
    /// Return NULL if the name is not registered.
    CDataLoader* FindDataLoader(const string& loader_name) const;

    typedef vector<string> TRegisteredNames;
    /// Get names of all registered data loaders.
    /// @param names
    ///   A vector of strings to be filled with the known names.
    void GetRegisteredNames(TRegisteredNames& names);
    /// Update loader's default-ness and priority.
    void SetLoaderOptions(const string& loader_name,
                          EIsDefault    is_default,
                          TPriority     priority = kPriority_Default);

    /// Revoke previously registered data loader.
    /// Return FALSE if the loader is still in use (by some scope).
    /// Throw an exception if the loader is not registered with this ObjMgr.
    bool RevokeDataLoader(CDataLoader& loader);
    bool RevokeDataLoader(const string& loader_name);


    /// Revoke all registered data loaders, even if they were still used.
    /// This method should be called at application's end to release
    /// data loader plugins for unloading of their shared library.
    void RevokeAllDataLoaders(void);

    typedef SRegisterLoaderInfo<CDataLoader> TRegisterLoaderInfo;

    void ReleaseDataSource(TDataSourceLock& data_source);

protected:
    // functions for data loaders
    // Register an existing data loader.
    // NOTE:  data loader must be created in the heap (ie using operator new).
    void RegisterDataLoader(CLoaderMaker_Base& loader_maker,
                            EIsDefault         is_default = eNonDefault,
                            TPriority          priority = kPriority_Default);

    // functions for scopes
    void RegisterScope(CScope_Impl& scope);
    void RevokeScope  (CScope_Impl& scope);

    typedef set<TDataSourceLock> TDataSourcesLock;

    TDataSourceLock AcquireDataLoader(CDataLoader& loader);
    TDataSourceLock AcquireDataLoader(const string& loader_name);
    TDataSourceLock AcquireSharedSeq_entry(const CSeq_entry& object);
    TDataSourceLock AcquireSharedBioseq(const CBioseq& object);
    TDataSourceLock AcquireSharedSeq_annot(const CSeq_annot& object);
    void AcquireDefaultDataSources(TDataSourcesLock& sources);

private:
    CObjectManager(void);
    friend class CSafeStatic_Allocator<CObjectManager>;

    // these are for Object Manager itself
    // nobody else should use it
    TDataSourceLock x_RegisterLoader(CDataLoader& loader,
                                     TPriority priority,
                                     EIsDefault   is_default = eNonDefault,
                                     bool         no_warning = false);
    CDataLoader* x_GetLoaderByName(const string& loader_name) const;
    TDataSourceLock x_FindDataSource(const CObject* key);
    TDataSourceLock x_RevokeDataLoader(CDataLoader* loader);
    
    typedef CPluginManager<CDataLoader> TPluginManager;
    TPluginManager& x_GetPluginManager(void);

private:

    typedef set< TDataSourceLock >                  TSetDefaultSource;
    typedef map< string, CDataLoader* >             TMapNameToLoader;
    typedef map< const CObject* , TDataSourceLock > TMapToSource;
    typedef set< CScope_Impl* >                     TSetScope;

    TSetDefaultSource   m_setDefaultSource;
    TMapNameToLoader    m_mapNameToLoader;
    TMapToSource        m_mapToSource;
    TSetScope           m_setScope;
    
    typedef CMutex      TRWLock;
    typedef CMutexGuard TReadLockGuard;
    typedef CMutexGuard TWriteLockGuard;

    mutable TRWLock     m_OM_Lock;
    mutable TRWLock     m_OM_ScopeLock;

    // CSeq_id_Mapper lock to provide a single mapper while OM is running
    CRef<CSeq_id_Mapper> m_Seq_id_Mapper;

    auto_ptr<TPluginManager> m_PluginManager;
    friend class CScope_Impl;
    friend class CDataSource; // To get id-mapper
    friend class CDataLoader; // To register data loaders
};

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECT_MANAGER__HPP
