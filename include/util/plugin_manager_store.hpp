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
#include <util/obj_store.hpp>

BEGIN_NCBI_SCOPE

/// CPluginManager specific instantiation of CSingletonObjectStore<>
///
/// @note
///   We need a separate class here to make sure singleton instatiates
///   only once (and in the correct DLL)
///
class NCBI_XUTIL_EXPORT CPluginManagerStore : 
    public CSingletonObjectStore<string, CPluginManagerBase>
{
public:
    typedef CSingletonObjectStore<string, CPluginManagerBase> TParent;
public:
    CPluginManagerStore();

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

        /// @param created
        ///    assigned TRUE if new plugin manager was added to the store
        ///    FALSE - if it existed before
        static
        CPluginManager<TInterface>* Get(bool* created = 0)
        {
            bool pm_created = false;
            string pm_name = CInterfaceVersion<TInterface>::GetName();
            TPluginManager* pm = 
                dynamic_cast<TPluginManager*>
                    (CPluginManagerStore::GetObject(pm_name));

            if (!pm) {
                pm = new TPluginManager;
                CPluginManagerStore::PutObject(pm_name, pm);
                pm_created = true;
            }
            if (created)
                *created = pm_created;
            return pm;
        }
    };
    

};


END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.2  2004/08/05 18:10:04  kuznets
 * Added plugin manager maker template
 *
 * Revision 1.1  2004/08/02 13:43:46  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

