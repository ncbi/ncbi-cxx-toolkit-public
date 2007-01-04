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
class C_xDriverMgr : public I_DriverMgr
{
public:
    C_xDriverMgr(unsigned int nof_drivers = 16);

    FDBAPI_CreateContext GetDriver(const string& driver_name,
                                   string*       err_msg = 0);

    virtual void RegisterDriver(const string&        driver_name,
                                FDBAPI_CreateContext driver_ctx_func);

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

protected:
    bool LoadDriverDll(const string& driver_name, string* err_msg);

private:
    typedef void            (*FDriverRegister) (I_DriverMgr& mgr);
    typedef FDriverRegister (*FDllEntryPoint)  (void);

    struct SDrivers {
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

C_xDriverMgr::C_xDriverMgr( unsigned int /*nof_drivers*/ )
{
    ///////////////////////////////////

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

void C_xDriverMgr::RegisterDriver(const string&        driver_name,
                                  FDBAPI_CreateContext driver_ctx_func)
{
    CFastMutexGuard mg(m_Mutex);

    NON_CONST_ITERATE(vector<SDrivers>, it, m_Drivers) {
        if (it->drv_name == driver_name) {
            it->drv_func = driver_ctx_func;

            return;
        }
    }

    m_Drivers.push_back(SDrivers(driver_name, driver_ctx_func));
}

FDBAPI_CreateContext C_xDriverMgr::GetDriver(const string& driver_name,
                                             string*       err_msg)
{
    CFastMutexGuard mg(m_Mutex);

    ITERATE(vector<SDrivers>, it, m_Drivers) {
        if (it->drv_name == driver_name) {
            return it->drv_func;
        }
    }

    if (!LoadDriverDll(driver_name, err_msg)) {
        return 0;
    }

    ITERATE(vector<SDrivers>, it, m_Drivers) {
        if (it->drv_name == driver_name) {
            return it->drv_func;
        }
    }

    DATABASE_DRIVER_ERROR( "internal error", 200 );
}

bool C_xDriverMgr::LoadDriverDll(const string& driver_name, string* err_msg)
{
    try {
        CDll drv_dll("ncbi_xdbapi_" + driver_name);

        FDllEntryPoint entry_point;
        if ( !drv_dll.GetEntryPoint_Func("DBAPI_E_" + driver_name,
                                         &entry_point) ) {
            drv_dll.Unload();
            return false;
        }

        FDriverRegister reg = entry_point();

        if(!reg) {
            DATABASE_DRIVER_ERROR( "driver reports an unrecoverable error "
                               "(e.g. conflict in libraries)", 300 );
        }

        reg(*this);
        return true;
    }
    catch (exception& e) {
        if(err_msg) *err_msg= e.what();
        return false;
    }
}

static CSafeStaticPtr<C_xDriverMgr> s_DrvMgr;

C_DriverMgr::C_DriverMgr(unsigned int nof_drivers)
{
}


C_DriverMgr::~C_DriverMgr()
{
}

FDBAPI_CreateContext C_DriverMgr::GetDriver(const string& driver_name,
                                            string* err_msg)
{
    return s_DrvMgr->GetDriver(driver_name, err_msg);
}

void C_DriverMgr::RegisterDriver(const string&        driver_name,
                                 FDBAPI_CreateContext driver_ctx_func)
{
    s_DrvMgr->RegisterDriver(driver_name, driver_ctx_func);
}

I_DriverContext* C_DriverMgr::GetDriverContext(const string&       driver_name,
                                               string*             err_msg,
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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2007/01/04 22:26:26  ssikorsk
 * Made C_xDriverMgr more thread-safe.
 *
 * Revision 1.33  2006/03/09 16:52:37  ssikorsk
 * Added methods ResetDllSearchPath,  SetDllStdSearchPath,
 * GetDllStdSearchPath to the C_DriverMgr class.
 *
 * Revision 1.32  2006/03/02 16:01:11  ssikorsk
 * Use CSafeStaticPtr to manage lifetime of static C_xDriverMgr s_DrvMgr.
 *
 * Revision 1.31  2006/02/22 16:05:35  ssikorsk
 * DATABASE_DRIVER_FALAL --> DATABASE_DRIVER_ERROR
 *
 * Revision 1.30  2005/11/02 14:32:39  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.29  2005/10/20 14:58:44  ssikorsk
 * Do not hide CPluginManagerException exception during driver loading
 *
 * Revision 1.28  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.27  2005/06/07 16:19:39  ssikorsk
 * Moved a definition of CDllResolver_Getter<I_DriverContext>::operator()(void) into cpp (again).
 *
 * Revision 1.26  2005/06/06 22:23:58  vakatov
 * Rollback previous revision as it caused linking errors in static MSVC++
 *
 * Revision 1.24  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.23  2005/03/23 14:45:42  vasilche
 * Removed non-MT-safe "created" flag.
 * CPluginManagerStore::CPMMaker<> replaced by CPluginManagerGetter<>.
 *
 * Revision 1.22  2005/03/09 17:06:33  ssikorsk
 * Implemented C_DriverMgr::GetDriver
 *
 * Revision 1.21  2005/03/08 21:06:17  ucko
 * Work around a bizarre bug in Compaq's compiler.
 *
 * Revision 1.20  2005/03/08 17:12:58  ssikorsk
 * Allow to add a driver search path for the driver manager
 *
 * Revision 1.19  2005/03/02 17:45:58  ssikorsk
 * Handle attr == NULL in Get_I_DriverContext
 *
 * Revision 1.18  2005/03/01 16:24:54  ssikorsk
 * Restored the "GetDriver" method
 *
 * Revision 1.17  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.16  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.15  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.14  2003/11/19 15:33:55  ucko
 * Adjust for CDll API change.
 *
 * Revision 1.13  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.12  2002/09/19 18:47:31  soussov
 * LoadDriverDll now allows driver to report an unrecoverable error through NULL return from entry_point function
 *
 * Revision 1.11  2002/04/23 16:46:17  soussov
 * GetDriverContext added
 *
 * Revision 1.10  2002/04/12 18:48:30  soussov
 * makes driver_mgr working properly in mt environment
 *
 * Revision 1.9  2002/04/09 22:18:15  vakatov
 * Moved code from the header
 *
 * Revision 1.8  2002/04/04 23:59:37  soussov
 * bug in RegisterDriver fixed
 *
 * Revision 1.7  2002/04/04 23:57:39  soussov
 * return of error message from dlopen added
 *
 * Revision 1.6  2002/02/14 00:59:42  vakatov
 * Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
 *
 * Revision 1.5  2002/01/23 21:29:48  soussov
 * replaces map with array
 *
 * Revision 1.4  2002/01/20 07:26:58  vakatov
 * Fool-proofed to compile func_ptr/void_ptr type casts on all compilers.
 * Added virtual destructor.
 * Temporarily get rid of the MT-safety features -- they need to be
 * implemented here more carefully (in the nearest future).
 *
 * Revision 1.3  2002/01/20 05:24:42  vakatov
 * identation
 *
 * Revision 1.2  2002/01/17 23:19:13  soussov
 * makes gcc happy
 *
 * Revision 1.1  2002/01/17 22:17:25  soussov
 * adds driver manager
 *
 * ===========================================================================
 */
