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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *
 */

#include <corelib/plugin_manager.hpp>
#include <corelib/ncbimtx.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

class NCBI_XNCBI_EXPORT CPluginManagerGetterImpl
{
public:
    typedef string TKey;
    typedef CPluginManagerBase TObject;
    
    static SSystemFastMutex& GetMutex(void);

    static TObject* GetBase(const TKey& key);
    static void PutBase(const TKey& key, TObject* pm);

    static void ReportKeyConflict(const TKey& key,
                                  const TObject* old_pm,
                                  const type_info& new_pm_type);
};

template<class Interface>
class CPluginManagerGetter
{
public:
    typedef CPluginManagerGetterImpl::TKey    TPluginManagerKey;
    typedef CPluginManagerGetterImpl::TObject TPluginManagerBase;
    typedef Interface                         TInterface;
    typedef CPluginManager<TInterface>        TPluginManager;
    
    static TPluginManager* Get(void)
        {
            return Get(CInterfaceVersion<TInterface>::GetName());
        }
    
    static TPluginManager* Get(const string& key)
        {
            TPluginManagerBase* pm_base;
            {{
                CFastMutexGuard guard(CPluginManagerGetterImpl::GetMutex());
                pm_base = CPluginManagerGetterImpl::GetBase(key);
                if ( !pm_base ) {
                    pm_base = new TPluginManager;
                    CPluginManagerGetterImpl::PutBase(key, pm_base);
                }
            }}
            TPluginManager* pm = dynamic_cast<TPluginManager*>(pm_base);
            if ( !pm ) {
                CPluginManagerGetterImpl::
                    ReportKeyConflict(key, pm_base, typeid(TPluginManager));
            }
            _ASSERT(pm);
            return pm;
        }
};


#if 1
/// deprecated interface to CPluginManager
///
/// @note
///   We need a separate class here to make sure singleton instatiates
///   only once (and in the correct DLL)
///
class CPluginManagerStore
{
public:

    /// Utility class to get plugin manager from the store
    /// If it is not there, class will create and add new instance 
    /// to the store.
    ///
    /// @note
    ///   Created plugin manager should be considered under-constructed
    ///   since it has no regisitered entry points or dll resolver.
    template<class TInterface>
    struct CPMMaker
    {
        typedef CPluginManager<TInterface> TPluginManager;

        static
        CPluginManager<TInterface>* Get(void)
        {
            return CPluginManagerGetter<TInterface>::Get();
        }
        
        /// @param pm_name
        ///    Storage name for plugin manager
        static
        CPluginManager<TInterface>* Get(const string& pm_name)
        {
            return CPluginManagerGetter<TInterface>::Get(pm_name);
        }
    };

};
#endif


template<class TInterface>
inline
void RegisterEntryPoint(typename CPluginManager<TInterface>::
                        FNCBI_EntryPoint plugin_entry_point)
{
    typedef CPluginManager<TInterface> TPluginManager;
    CRef<TPluginManager> manager(CPluginManagerGetter<TInterface>::Get());
    _ASSERT(manager);
    manager->RegisterWithEntryPoint(plugin_entry_point);
}


END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.7  2005/03/23 14:41:45  vasilche
 * Do not use non-portable CSingletonObjectStore.
 * Removed non-MT-safe "created" flag.
 * CPluginManagerStore::CPMMaker<> replaced by CPluginManagerGetter<>.
 *
 * Revision 1.6  2004/12/27 16:10:41  vasilche
 * Force linking in CSingletonObjectStore::s_obj_store on GCC.
 *
 * Revision 1.5  2004/12/22 19:25:49  grichenk
 * +RegisterEntryPoint() template
 *
 * Revision 1.4  2004/12/21 21:40:15  grichenk
 * Moved obj_store and plugin_manager_store to corelib
 *
 * Revision 1.3  2004/08/05 18:42:44  kuznets
 * +CPMMake::Get() accepting interface name
 *
 * Revision 1.2  2004/08/05 18:10:04  kuznets
 * Added plugin manager maker template
 *
 * Revision 1.1  2004/08/02 13:43:46  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

