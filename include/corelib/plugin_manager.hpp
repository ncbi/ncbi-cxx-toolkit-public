#ifndef CORELIB___PLUGIN_MANAGER__HPP
#define CORELIB___PLUGIN_MANAGER__HPP

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
 * Author:  Denis Vakatov, Anatoliy Kuznetsov
 *
 * File Description:  Plugin manager (using class factory paradigm)
 *
 */

/// @file plugin_manager.hpp
/// Plugin manager (using class factory paradigm).
///
/// Describe generic interface and provide basic functionality to advertise
/// and export a class factory. 
/// The class and class factory implementation code can be linked to
/// either statically (then, the class factory will need to be registered
/// explicitly by the user code) or dynamically (then, the DLL will be
/// searched for using plugin name, and the well-known DLL entry point
/// will be used to register the class factory, automagically).
/// 
/// - "class factory" -- An entity used to generate objects of the given class.
///                      One class factory can generate more than one version
///                      of the class.
/// 
/// - "interface"  -- Defines the implementation-independent API and expected
///                   behaviour of a class.
///                   Interface's name is provided by its class's factory,
///                   see IClassFactory::GetInterfaceName().
///                   Interfaces are versioned to track the compatibility.
/// 
/// - "driver"  -- A concrete implementation of the interface and its factory.
///                Each driver has its own name (do not confuse it with the
///                interface name!) and version.
/// 
/// - "host"    -- An entity (DLL or the EXEcutable itself) that contains
///                one or more drivers (or versions of the same driver),
///                which can implement one or more interfaces.
/// 
/// - "version" -- MAJOR (backward- and forward-incompatible changes in the
///                       interface and/or its expected behaviour);
///                MINOR (backward compatible changes in the driver code);
///                PATCH_LEVEL (100% compatible plugin or driver code changes).
/// 

#include <corelib/ncbimtx.hpp>
#include <corelib/version.hpp>

#include <set>

/** @addtogroup PluginMgr
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// CInterfaceVersion<> --
///
/// Current interface version.
///
/// It is just a boilerplate, to be hard-coded in the concrete interface header
/// @sa NCBI_PLUGIN_VERSION, CVersionInfo

template <class TClass>
class CInterfaceVersion
{
};


/// Macro to auto-setup the current interface name and version.
///
/// This macro must be "called" once per interface, usually in the
/// very header that describes that interface.
///
/// Example:
///    NCBI_INTERFACE_VERSION(IFooBar, "IFooBar", 1, 3, 8);
/// @sa CInterfaceVersion

#define NCBI_INTERFACE_VERSION(iface, iface_name, major, minor, patch_level) \
EMPTY_TEMPLATE \
class CInterfaceVersion<iface> \
{ \
public: \
    enum { \
        eMajor      = major, \
        eMinor      = minor, \
        ePatchLevel = patch_level \
    }; \
    static const char* GetName() { return iface_name; } \
}



/// IClassFactory<> --
///
/// Class factory for the given interface.
///
/// IClassFactory should be implemented for collection of drivers
/// and exported by hosts

template <class TClass>
class IClassFactory
{
public:
    typedef TClass   TInterface;

    struct SDriverInfo
    {
        string         name;        ///< Driver name
        CVersionInfo   version;     ///< Driver version

        SDriverInfo(const string& driver_name,
                    const CVersionInfo& driver_version)
        : name(driver_name),
          version(driver_version)
        {}
    };

    typedef list<SDriverInfo>  TDriverList;


    /// Create driver's instance
    ///
    /// Function creates driver by its name and version.
    /// The requirements is the drivers version should match the interface
    /// up to the patch level.
    ///
    /// @param driver
    ///  Requested driver's name (not the name of the supported interface)
    /// @param version
    ///  Requested interface version (as understood by the caller).
    ///  By default it will be passed the version which is current from
    ///  the calling code's point of view.
    /// @return
    ///  NULL on any error (not found entry point, version mismatch, etc.)
    virtual TClass* CreateInstance(const string&     driver  = kEmptyStr,
                                   CVersionInfo version = CVersionInfo
                                   (ncbi::CInterfaceVersion<TClass>::eMajor,
                                    ncbi::CInterfaceVersion<TClass>::eMinor,
                                    ncbi::CInterfaceVersion<TClass>::ePatchLevel))
        const = 0;

    // Versions of the interface exported by the factory
    virtual void GetDriverVersions(TDriverList& driver_list) const = 0;

    virtual ~IClassFactory(void) {}
};



/// CPluginManager<> --
///
/// To register (either directly, or via an "entry point") class factories
/// for the given interface.
///
/// Then, facilitate the process of instantiating the class given
/// the registered pool of drivers, and also taking into accont the driver name
/// and/or version as requested by the calling code.
///
/// Template class is protected by mutex and safe for use from diffrent threads
///

template <class TClass>
class CPluginManager
{
public:
    typedef IClassFactory<TClass> TClassFactory;

    /// Create class instance
    /// @return
    ///  Never return NULL -- always throw exception on error.
    /// @sa GetFactory()
    TClass* CreateInstance(const string&       driver  = kEmptyStr,
                           const CVersionInfo& version = CVersionInfo
                           (CInterfaceVersion<TClass>::eMajor,
                            CInterfaceVersion<TClass>::eMinor,
                            CInterfaceVersion<TClass>::ePatchLevel))
        const
    {
        return GetFactory(name, version)->CreateInstance(name, version);
    }

    /// Get class factory
    ///
    /// If more than one (of registered) class factory contain eligible
    /// driver candidates, then pick the class factory containing driver of
    /// the latest version.
    /// @param driver
    ///  Name of the driver. If passed empty, then -- any.
    /// @param version
    ///  Requested version. The returned driver can have a different (newer)
    ///  version (provided that the new implementation is backward-compatible
    ///  with the requested version.
    /// @return
    ///  Never return NULL -- always throw exception on error.
    TClassFactory* GetFactory(const string&       driver  = kEmptyStr,
                              const CVersionInfo& version = CVersionInfo
                              (CInterfaceVersion<TClass>::eMajor,
                               CInterfaceVersion<TClass>::eMinor,
                               CInterfaceVersion<TClass>::ePatchLevel))
        const
    {
        CFastMutexGuard guard(m_Mutex);

        typename 
           set<TClassFactory*>::const_iterator it = m_Factories.begin();
        typename 
           set<TClassFactory*>::const_iterator it_end = m_Factories.end();

        typename 
            set<TClassFactory*>::const_iterator it1 = FindVersion(it, it_end);
        if (it1 != it_end) {
            return *it1;
        }
    }

    /// Information about a driver, with maybe a pointer to an instantiated
    /// class factory that contains the driver.
    /// @sa FNCBI_EntryPoint
    struct SDriverInfo {
        string         name;        //!< Driver name
        CVersionInfo   version;     //!< Driver version
        // It's the plugin manager's (and not SDriverInfo) responsibility to
        // keep and then destroy class factories.
        TClassFactory* factory;     //!< Class factory (can be NULL)

        SDriverInfo(const string&        driver_name,
                    const CVersionInfo&  driver_version)
        : name(driver_name),
          version(driver_version),
          factory(0)
        {}
    };

    /// List of driver information.
    ///
    /// It is used to communicate using the entry points mechanism.
    /// @sa FNCBI_EntryPoint
    typedef list<SDriverInfo> TDriverInfoList;

    /// Register factory in the manager.
    ///  
    /// The registered factory will be owned by the manager.
    /// @sa UnregisterFactory()
    void RegisterFactory(TClassFactory& factory);

    /// Unregister and release (un-own)
    /// @sa RegisterFactory()
    bool UnregisterFactory(TClassFactory& factory);

    /// Actions performed by the entry point
    /// @sa FNCBI_EntryPoint
    enum EEntryPointRequest {
        /// Add info about all drivers exported through the entry point
        /// to the end of list.
        ///
        /// "SFactoryInfo::factory" in the added info should be assigned NULL.
        eGetFactoryInfo,

        /// Scan the driver info list passed to the entry point for the
        /// [name,version] pairs exported by the given entry point.
        ///
        /// For each pair found, if its "SDriverInfo::factory" is NULL,
        /// instantiate its class factory and assign it to the
        /// "SDriverInfo::factory".
        eInstantiateFactory
    };

    /// Entry point to get drivers' info, and (if requested) their class
    /// factori(es).
    ///
    /// This function is usually (but not necessarily) called by
    /// RegisterWithEntryPoint().
    ///
    /// Usually, it's called twice -- the first time to get the info
    /// about the drivers exported by the entry point, and then
    /// to instantiate selected factories.
    ///
    /// Caller is responsible for the proper destruction (deallocation)
    /// of the instantiated factories.
    typedef void (*FNCBI_EntryPoint)(TDriverInfoList&   info_list,
                                     EEntryPointRequest method);

    /// Register all factories exported by the plugin entry point.
    /// @sa RegisterFactory()
    void RegisterWithEntryPoint(FNCBI_EntryPoint plugin_entry_point);

    // ctors
    CPluginManager(void) {}
    virtual ~CPluginManager();

protected:
    /// Protective mutex to syncronize the access to the plugin manager
    /// from different threads
    CFastMutex m_Mutex;

private:
    /// List of factories presently registered with (and owned by)
    /// the plugin manager.
    set<TClassFactory*> m_Factories;
};



template <class TClass>
void CPluginManager<TClass>::RegisterFactory(TClassFactory& factory)
{
    CFastMutexGuard guard(m_Mutex);

    m_Factories->insert(&factory);
}

template <class TClass>
bool CPluginManager<TClass>::UnregisterFactory(TClassFactory& factory)
{
    CFastMutexGuard guard(m_Mutex);

    typename set<TClassFactory*>::iterator it = m_Factories.find(&factory);
    if (it != m_Factories.end()) {
        TClassFactory* f = *it;
        m_Factories.erase(it);
        delete f;
    }
}


template <class TClass>
void CPluginManager<TClass>::RegisterWithEntryPoint(FNCBI_EntryPoint plugin_entry_point)
{
    TDriverInfoList drv_list;
    plugin_entry_point(&drv_list, eGetFactoryInfo);

    if (!drv_list.empty()) {
        plugin_entry_point(&drv_list, eInstantiateFactory);

        typename TDriverInfoList::iterator it = drv_list.begin();
        typename TDriverInfoList::iterator it_end = drv_list.end();
        for (; it != it_end; ++it) {
            if (it->factory) {
                RegisterFactory(*(it->factory));
            }
        }
    }
}

template <class TClass>
CPluginManager<TClass>::~CPluginManager()
{
    typename set<TClassFactory*>::iterator it = m_Factories.begin();
    typename set<TClassFactory*>::iterator it_end = m_Factories.end();
    for (; it != it_end; ++it) {
        TClassFactory* f = *it;
        delete f;
    }
}


/// Template implements entry point
///
/// The actual entry point is a C callable exported function 
///   delegates the functionality to 
///               CHostEntryPointImpl<>::NCBI_EntryPointImpl()

template<class TClassFactory> struct CHostEntryPointImpl
{
    typedef typename TClassFactory::TInterface                TInterface;
    typedef CPluginManager<TInterface>                        TPluginManager;
    typedef typename CPluginManager<TInterface>::SDriverInfo  TDriverInfo;
    
    typedef typename 
      CPluginManager<TInterface>::TDriverInfoList             TDriverInfoList;
    typedef typename 
      CPluginManager<TInterface>::EEntryPointRequest        EEntryPointRequest;
    typedef typename TClassFactory::SDriverInfo             TCFDriverInfo;
    

    /// Entry point implementation. 
    ///
    /// @sa CPluginManager::FNCBI_EntryPoint
    static void NCBI_EntryPointImpl(TDriverInfoList& info_list,
                                    EEntryPointRequest method)
    {
        TClassFactory cf;
        list<TCFDriverInfo> cf_info_list;
        cf.GetDriverVersions(cf_info_list);

        switch (method)
        { 
        case TPluginManager::eGetFactoryInfo:
            {
            typename list<TCFDriverInfo>::const_iterator it =
                                                         cf_info_list.begin();
            typename list<TCFDriverInfo>::const_iterator it_end =
                                                         cf_info_list.end();
            for (; it != it_end; ++it) {
                info_list.push_back(TDriverInfo(it->name, it->version));
            }

            }
            break;
        case TPluginManager::eInstantiateFactory:
            {
            typename TDriverInfoList::iterator it1 = info_list.begin();
            typename TDriverInfoList::iterator it1_end = info_list.end();
            for(; it1 != it1_end; ++it1) {
                if (it1->factory) {    // already instantiated
                    continue;
                }
                typename list<TCFDriverInfo>::iterator it2 = 
                                                    cf_info_list.begin();
                typename list<TCFDriverInfo>::iterator it2_end = 
                                                    cf_info_list.end();
                for (; it2 != it2_end; ++it2) {
                    if (it1->name == it2->name) {
                        if (it1->version.Match(it2->version) != 
                                                 CVersionInfo::eNonCompatible)
                        {
                            TClassFactory* cg = new TClassFactory();
                            IClassFactory<TInterface>* icf = cg;
                            it1->factory = icf;
                        }
                    }
                } // for

            } // for

            }
            break;
        default:
            _ASSERT(0);
        } // switch
    }

};


/// Template class helps to implement one driver class factory.
///
/// Class supports one driver, one version class factory 
/// (the very basic one)
/// Template parameters are:
///   TIClass - interface class 
///   TDriver - driver class

template <class TIClass, class TDriver>
class CSimpleClassFactoryImpl : public IClassFactory<TIClass>
{
public:
    /// Construction
    ///
    /// @param driver_name
    ///    Driver name string
    /// @param patch_level
    ///    Patch level implemented by the driver. 
    ///    By default corresponds to interface patch level.
    CSimpleClassFactoryImpl(const string& driver_name, int patch_level = -1) 
    : m_DriverVersionInfo(
         ncbi::CInterfaceVersion<TIClass>::eMajor, 
         ncbi::CInterfaceVersion<TIClass>::eMinor, 
         patch_level >= 0 ? patch_level 
                          : ncbi::CInterfaceVersion<TIClass>::ePatchLevel),
      m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    TIClass* CreateInstance(const string&  driver  = kEmptyStr,
                            CVersionInfo version = 
                  CVersionInfo(ncbi::CInterfaceVersion<TIClass>::eMajor,
                               ncbi::CInterfaceVersion<TIClass>::eMinor,
                               ncbi::CInterfaceVersion<TIClass>::ePatchLevel)
                            ) const

    {
        if (driver.empty() || driver == m_DriverName) {
            CVersionInfo v(ncbi::CInterfaceVersion<TIClass>::eMajor,
                           ncbi::CInterfaceVersion<TIClass>::eMinor,
                           ncbi::CInterfaceVersion<TIClass>::ePatchLevel);

            if (version.Match(v) == CVersionInfo::eNonCompatible) {
                return 0;
            }
            return new TDriver();
        }
        return 0;
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(SDriverInfo(m_DriverName, m_DriverVersionInfo));
    }

protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;
};


//!!!  The above API is by-and-large worked through, all the rest of
//!!!  the file is absolutely not...

#if 0


/// CDllResolver_PluginManager<> --
///
/// Class to resolve DLL by driver name and version.
/// Also allows to filter for and get entry point for CPluginManager.

class NCBI_XNCBI_EXPORT CDllResolver_PluginManager : public CDllResolver
{
public:
    /// 
    /// Return list of absolute DLL names matching the prugin's class name,
    /// driver name, and driver version.
    virtual string<list> Resolve
    (const string&       interface,
     const string&       driver,
     const CVersionInfo& version = CVersionInfo::kAny)
        const;
  

protected:
    /// Compose a list of possible DLL names based on the plugin's class, name
    /// and version. 
    ///
    /// Legal return formats:
    ///  - Platform-independent form -- no path or dots in the DLL name, e.g.
    ///    "ncbi_plugin_dbapi_ftds_3_1_7".
    ///    In this case, the DLL will be searched for in the standard
    ///    DLL search paths, with automatic addition of any platform-specific
    ///    prefixes and suffixes.
    ///
    ///  - Platform-dependent form, with path search -- no path is specified
    ///    in the DLL name, e.g. "dbapi_plugin_dbapi_ftds.so.3.1.7".
    ///    In this case, the DLL will be searched for in the standard
    ///    DLL search paths "as is", without any automatic addition of
    ///    platform-specific prefixes and suffixes.
    ///
    ///  - Platform-dependent form, with fixed absolute path, e.g.
    ///    "/foo/bar/dir/dbapi_plugin_dbapi_ftds.so.3.1.7".
    ///
    /// Default DLL name list (depending of whether full or partial version
    /// and driver name are specified):
    ///  - "<GetDllPrefix()><plugin>_<driver>_<version>", e.g.
    ///     "ncbi_plugin_dbapi_ftds_3_1_7", "ncbi_plugin_dbapi_ftds_2"
    ///  - "<GetDllPrefix()><plugin>_<driver>.so.<version>"  (UNIX-only), e.g.
    ///     "ncbi_plugin_dbapi_ftds.so.3.1.7", "ncbi_plugin_dbapi_ftds.so.2"
    ///  - "<GetDllPrefix()><plugin>_<driver>", e.g. "ncbi_plugin_dbapi_ftds"
    ///  - "<GetDllPrefix()><plugin>", e.g. "ncbi_plugin_dbapi"
    virtual list<string> GetDllNames
    (const string&       plugin,
     const string&       driver  = kEmptyStr,
     const CVersionInfo& version = CVersionInfo::kAny)
        const;

    // default:
    //  - "NCBI_EntryPoint_<TClassFactory::GetName()>_<name>"
    virtual string GetEntryPointName
    (const string&       driver  = kEmptyStr,
     const CVersionInfo& version = CVersionInfo::kAny)
        const;

    // ctors
    CDllResolver_PluginManager(void) :
        CDllResolver() {}
    virtual ~CDllResolver_PluginManager() {}
};



/// CDllResolver --
///
/// Class to find DLLs.
///
/// The DLL search criteria include:
///  - search path(s)
///  - DLL file name mask(s)
///  - entry point name mask(s)

class NCBI_XNCBI_EXPORT CDllResolver
{
public:
    ///
    typedef list<string> TPath;

    /// 
    typedef list<string> TMask;

    /// How to set a path or a mask list
    /// @sa SetPath(), SetMask()
    enum ESet {
        eOverride,  //<! Override the existing list
        ePrepend,   //<! Add to the head of the existing list
        eAppend     //<! Add to the tail of the existing list
    };

    /// Change DLL search path.
    void SetPath(const TPath& path, ESetOrder method);

    /// Get the current DLL serach path.
    ///
    /// By default, it is set to the standard system-dependent DLL search path.
    /// @sa GetStdPath
    const TPath& GetPath(void) const;

    /// Get the standard OS- and program-environment specific DLL search path.
    static const TPath& GetStdPath(void);

    /// Change filename mask(s)
    void SetMask(const TMask& path, ESet method);

    /// Get the current filename mask(s)
    ///
    /// By default, it is set to the standard mask which can be obtained
    /// by calling GetStdMask() without arguments.
    /// @sa GetStdMask()
    const TMask& GetMask(void) const;

    /// Get the standard mask, composed according to standard rules.
    ///
    /// Default:  "ncbi_plugin_*<sfx>*",
    ///           where <sfx> is OS-dependent (such as '.dll', '.so')
    static const TMask& GetStdMask(const string& prefix  = kEmptyStr,
                                   const string& family  = kEmptyStr,
                                   const string& family  = kEmptyStr,
                                   CVersionInfo& version = CVersionInfo::kAny);

protected:
    // ctors
    CDllResolver(void);
    virtual ~CDllResolver();

private:
    /// Directories to scan for the DLLs
    TPath m_Path;
    /// DLL filename mask(s) to match
    TMask m_Mask;
};

#endif /* NOT READY YET */


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2003/11/03 17:52:00  kuznets
 * Added CSimpleClassFactoryImpl template.
 * Helps quickly implement basic PM compatible class factory.
 *
 * Revision 1.7  2003/11/03 16:32:58  kuznets
 * Cleaning the code to be compatible with GCC, WorkShop 53 and MSVC at the
 * same time...
 *
 * Revision 1.6  2003/10/31 19:53:52  kuznets
 * +CHostEntryPointImpl
 *
 * Revision 1.5  2003/10/30 20:03:49  kuznets
 * Work in progress. Added implementations of CPluginManager<> methods.
 *
 * Revision 1.4  2003/10/29 23:35:46  vakatov
 * Just starting with CDllResolver...
 *
 * Revision 1.3  2003/10/29 19:34:43  vakatov
 * Comment out unfinished defined APIs (using "#if 0")
 *
 * Revision 1.2  2003/10/28 22:29:04  vakatov
 * Draft-done with:
 *   general terminology
 *   CInterfaceVersion<>
 *   NCBI_PLUGIN_VERSION()
 *   IClassFactory<>
 *   CPluginManager<>
 * TODO:
 *   Host-related API
 *   DLL resolution
 *
 * Revision 1.1  2003/10/28 00:12:23  vakatov
 * Initial revision
 *
 * Work-in-progress, totally unfinished.
 * Note: DLL resolution shall be split and partially moved to the NCBIDLL.
 *
 * ===========================================================================
 */

#endif  /* CORELIB___PLUGIN_MANAGER__HPP */
