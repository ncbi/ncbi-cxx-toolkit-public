/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver manager
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbireg.hpp>

#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <dbapi/driver/driver_mgr.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
TPluginManagerParamTree*
MakePluginManagerParamTree(const string& driver_name, const map<string, string>* attr)
{
    typedef map<string, string>::const_iterator TCIter;
    CMemoryRegistry reg;
    TCIter citer = attr->begin();
    TCIter cend = attr->end();

    for ( ; citer != cend; ++citer ) {
        reg.Set( driver_name, citer->first, citer->second );
    }

    TPluginManagerParamTree* const tr = CConfig::ConvertRegToTree(reg);

    return tr;
}

///////////////////////////////////////////////////////////////////////////////
CPluginManager_DllResolver*
CDllResolver_Getter<I_DriverContext>::operator()(void)
{
    CPluginManager_DllResolver* resolver =
        new CPluginManager_DllResolver
        (CInterfaceVersion<I_DriverContext>::GetName(),
            kEmptyStr,
            CVersionInfo::kAny,
            CDll::eNoAutoUnload);
    resolver->SetDllNamePrefix("ncbi");
    return resolver;
}

///////////////////////////////////////////////////////////////////////////////
class C_xDriverMgr
{
public:
    C_xDriverMgr(void);
    virtual ~C_xDriverMgr(void);

public:
    /// Add path for the DLL lookup
    void AddDllSearchPath(const string& path);
    /// Delete all user-installed paths for the DLL lookup (for all resolvers)
    /// @param previous_paths
    ///  If non-NULL, store the prevously set search paths in this container
    void ResetDllSearchPath(vector<string>* previous_paths = NULL);

    /// Specify which standard locations should be used for the DLL lookup
    /// (for all resolvers). If standard locations are not set explicitelly
    /// using this method CDllResolver::fDefaultDllPath will be used by default.
    CDllResolver::TExtraDllPath
    SetDllStdSearchPath(CDllResolver::TExtraDllPath standard_paths);

    /// Get standard locations which should be used for the DLL lookup.
    /// @sa SetDllStdSearchPath
    CDllResolver::TExtraDllPath GetDllStdSearchPath(void) const;

    I_DriverContext* GetDriverContext(
        const string& driver_name,
        const TPluginManagerParamTree* const attr = NULL);

    I_DriverContext* GetDriverContext(
        const string& driver_name,
        const map<string, string>* attr = NULL);

private:
    struct SDrivers {
	typedef I_DriverContext* (*FDBAPI_CreateContext)(const map<string,string>* attr);

        SDrivers(const string& name, FDBAPI_CreateContext func) :
            drv_name(name),
            drv_func(func)
        {
        }

        string               drv_name;
        FDBAPI_CreateContext drv_func;
    };
    vector<SDrivers> m_Drivers;

    mutable CFastMutex m_Mutex;

private:
    typedef CPluginManager<I_DriverContext> TContextManager;
    typedef CPluginManagerGetter<I_DriverContext> TContextManagerStore;

    CRef<TContextManager>   m_ContextManager;
};

C_xDriverMgr::C_xDriverMgr(void)
{
    m_ContextManager.Reset( TContextManagerStore::Get() );
#ifndef NCBI_COMPILER_COMPAQ
    // For some reason, Compaq's compiler thinks m_ContextManager is
    // inaccessible here!
    _ASSERT( m_ContextManager );
#endif
}


C_xDriverMgr::~C_xDriverMgr(void)
{
}


void
C_xDriverMgr::AddDllSearchPath(const string& path)
{
    CFastMutexGuard mg(m_Mutex);

    m_ContextManager->AddDllSearchPath( path );
}


void
C_xDriverMgr::ResetDllSearchPath(vector<string>* previous_paths)
{
    CFastMutexGuard mg(m_Mutex);

    m_ContextManager->ResetDllSearchPath( previous_paths );
}


CDllResolver::TExtraDllPath
C_xDriverMgr::SetDllStdSearchPath(CDllResolver::TExtraDllPath standard_paths)
{
    CFastMutexGuard mg(m_Mutex);

    return m_ContextManager->SetDllStdSearchPath( standard_paths );
}


CDllResolver::TExtraDllPath
C_xDriverMgr::GetDllStdSearchPath(void) const
{
    CFastMutexGuard mg(m_Mutex);

    return m_ContextManager->GetDllStdSearchPath();
}


I_DriverContext*
C_xDriverMgr::GetDriverContext(
    const string& driver_name,
    const TPluginManagerParamTree* const attr)
{
    I_DriverContext* drv = NULL;

    try {
        CFastMutexGuard mg(m_Mutex);

        drv = m_ContextManager->CreateInstance(
            driver_name,
            NCBI_INTERFACE_VERSION(I_DriverContext),
            attr
            );
    }
    catch( const CPluginManagerException& ) {
        throw;
    }
    catch ( const exception& e ) {
        DATABASE_DRIVER_ERROR( driver_name + " is not available :: " + e.what(), 300 );
    }
    catch ( ... ) {
        DATABASE_DRIVER_ERROR( driver_name + " was unable to load due an unknown error", 300 );
    }

    return drv;
}

I_DriverContext*
C_xDriverMgr::GetDriverContext(
    const string& driver_name,
    const map<string, string>* attr)
{
    auto_ptr<TPluginManagerParamTree> pt;
    const TPluginManagerParamTree* nd = NULL;

    if ( attr != NULL ) {
        pt.reset( MakePluginManagerParamTree(driver_name, attr) );
        _ASSERT(pt.get());
        nd = pt->FindNode( driver_name );
    }

    return GetDriverContext(driver_name, nd);
}

////////////////////////////////////////////////////////////////////////////////
static CSafeStaticPtr<C_xDriverMgr> s_DrvMgr;


////////////////////////////////////////////////////////////////////////////////
C_DriverMgr::C_DriverMgr(unsigned int /* nof_drivers */)
{
}


C_DriverMgr::~C_DriverMgr()
{
}

I_DriverContext* C_DriverMgr::GetDriverContext(const string&       driver_name,
                                               string*             /* err_msg */,
                                               const map<string,string>* attr)
{
    return s_DrvMgr->GetDriverContext( driver_name, attr );
}


void
C_DriverMgr::AddDllSearchPath(const string& path)
{
    s_DrvMgr->AddDllSearchPath( path );
}


void
C_DriverMgr::ResetDllSearchPath(vector<string>* previous_paths)
{
    s_DrvMgr->ResetDllSearchPath( previous_paths );
}


CDllResolver::TExtraDllPath
C_DriverMgr::SetDllStdSearchPath(CDllResolver::TExtraDllPath standard_paths)
{
    return s_DrvMgr->SetDllStdSearchPath( standard_paths );
}


CDllResolver::TExtraDllPath
C_DriverMgr::GetDllStdSearchPath(void) const
{
    return s_DrvMgr->GetDllStdSearchPath();
}


I_DriverContext*
C_DriverMgr::GetDriverContextFromTree(
    const string& driver_name,
    const TPluginManagerParamTree* const attr)
{
    return s_DrvMgr->GetDriverContext( driver_name, attr );
}


I_DriverContext*
C_DriverMgr::GetDriverContextFromMap(
    const string& driver_name,
    const map<string, string>* attr)
{
    return s_DrvMgr->GetDriverContext( driver_name, attr );
}


///////////////////////////////////////////////////////////////////////////////
I_DriverContext*
Get_I_DriverContext(const string& driver_name, const map<string, string>* attr)
{
    typedef CPluginManager<I_DriverContext> TReaderManager;
    typedef CPluginManagerGetter<I_DriverContext> TReaderManagerStore;
    I_DriverContext* drv = NULL;
    const TPluginManagerParamTree* nd = NULL;

    CRef<TReaderManager> ReaderManager(TReaderManagerStore::Get());
    _ASSERT(ReaderManager);

    try {
        auto_ptr<TPluginManagerParamTree> pt;

        if ( attr != NULL ) {
             pt.reset( MakePluginManagerParamTree(driver_name, attr) );

            _ASSERT( pt.get() );

            nd = pt->FindNode( driver_name );
        }
        drv = ReaderManager->CreateInstance(
            driver_name,
            NCBI_INTERFACE_VERSION(I_DriverContext),
            nd
            );
    }
    catch( const CPluginManagerException& ) {
        throw;
    }
    catch ( const exception& e ) {
        DATABASE_DRIVER_ERROR( driver_name + " is not available :: " + e.what(), 300 );
    }
    catch ( ... ) {
        DATABASE_DRIVER_ERROR( driver_name + " was unable to load due an unknown error", 300 );
    }

    return drv;
}

END_NCBI_SCOPE


