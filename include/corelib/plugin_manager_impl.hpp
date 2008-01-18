#ifndef CORELIB___PLUGIN_MANAGER__IMPL__HPP
#define CORELIB___PLUGIN_MANAGER__IMPL__HPP

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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:  Collection of classes to implement different
 *                    plugin manager paradigms.
 *
 *
 */

/// @file plugin_manager_impl.hpp
/// Helper classes and templates to implement plugins.

#include <corelib/plugin_manager.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup PluginMgr
 *
 * @{
 */


/// Template class helps to implement one driver class factory.
///
/// Class supports one driver, one version class factory
/// (the very basic one)
/// Template parameters are:
///   IFace - interface class
///   TDriver - driver class (implements IFace)

template <class IFace, class TDriver>
class CSimpleClassFactoryImpl : public IClassFactory<IFace>
{
public:

    typedef TDriver                        TImplementation;
    typedef IFace                          TInterface;
    typedef IClassFactory<IFace>           TParent;
    typedef typename TParent::SDriverInfo  TDriverInfo;
    typedef typename TParent::TDriverList  TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CSimpleClassFactoryImpl(const string& driver_name, int patch_level = -1)
        : m_DriverVersionInfo
        (TParent::GetDefaultDrvVers().GetMajor(),
         TParent::GetDefaultDrvVers().GetMinor(),
         patch_level >= 0 ?
            patch_level : TParent::GetDefaultDrvVers().GetPatchLevel()),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = TParent::GetDefaultDrvVers(),
                   const TPluginManagerParamTree* /*params*/ = 0) const
    {
        TDriver* drv = 0;
        if (driver.empty() || driver == m_DriverName) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {
                drv = new TImplementation();
            }
        }
        return drv;
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }

protected:
    /// Utility function to get an element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    string GetParam(const TPluginManagerParamTree* params,
                    const string&                  param_name,
                    bool                           mandatory,
                    const string&                  default_value) const
    {
        return
            TParent::GetParam(m_DriverName,
                              params, param_name, mandatory, default_value);
    }

    /// This version always defaults to the empty string so that it
    /// can safely return a reference.  (default_value may be
    /// temporary in some cases.)
    const string& GetParam(const TPluginManagerParamTree* params,
                           const string&                  param_name,
                           bool                           mandatory) const
    {
        return
            TParent::GetParam(m_DriverName, params, param_name, mandatory);
    }

    /// Utility function to get an integer of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    int GetParamInt(const TPluginManagerParamTree* params,
                    const string&                  param_name,
                    bool                           /* mandatory */,
                    int                            default_value) const
    {
        CConfig conf(params);
        return conf.GetInt(m_DriverName,
                           param_name,
                           CConfig::eErr_NoThrow,
                           default_value);
    }

    /// Utility function to get an integer of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    Uint8
    GetParamDataSize(const TPluginManagerParamTree* params,
                     const string&                  param_name,
                     bool                           /* mandatory */,
                     unsigned int                   default_value) const
    {
        CConfig conf(params);
        return conf.GetDataSize(m_DriverName,
                                param_name,
                                CConfig::eErr_NoThrow,
                                default_value);
    }


    /// Utility function to get an bool of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)
    bool GetParamBool(const TPluginManagerParamTree* params,
                      const string&                  param_name,
                      bool                           /* mandatory */,
                      bool                           default_value) const
    {
        CConfig conf(params);
        return conf.GetBool(m_DriverName,
                            param_name,
                            CConfig::eErr_NoThrow,
                            default_value);

    }


protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;
};


/// Template implements entry point
///
/// The actual entry point is a C callable exported function
///   delegates the functionality to
///               CHostEntryPointImpl<>::NCBI_EntryPointImpl()

template<class TClassFactory>
struct CHostEntryPointImpl
{
    typedef typename TClassFactory::TInterface                TInterface;
    typedef CPluginManager<TInterface>                        TPluginManager;
    typedef typename CPluginManager<TInterface>::SDriverInfo  TDriverInfo;

    typedef typename
    CPluginManager<TInterface>::TDriverInfoList         TDriverInfoList;
    typedef typename
    CPluginManager<TInterface>::EEntryPointRequest      EEntryPointRequest;
    typedef typename TClassFactory::SDriverInfo         TCFDriverInfo;


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
                        // We do only an exact match here.
                        // A factory cannot be matched twice.
                        _ASSERT( it1->factory == NULL );

                        typename list<TCFDriverInfo>::iterator it2 =
                            cf_info_list.begin();
                        typename list<TCFDriverInfo>::iterator it2_end =
                            cf_info_list.end();

                        for (; it2 != it2_end; ++it2) {
                            if (it1->name == it2->name) {
                                // We do only an exact match here.
                                if (it1->version.Match(it2->version) ==
                                    CVersionInfo::eFullyCompatible)
                                    {
                                        _ASSERT( it1->factory == NULL );

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


/* @} */

END_NCBI_SCOPE

#endif  /* CORELIB___PLUGIN_MANAGER__IMPL_HPP */
