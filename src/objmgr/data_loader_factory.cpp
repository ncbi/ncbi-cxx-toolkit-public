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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Data loader factory
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/data_loader_factory.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataLoaderFactory::CDataLoaderFactory(const string& driver_name,
                                       int           patch_level) 
    : m_DriverVersionInfo(
        ncbi::CInterfaceVersion<CDataLoader>::eMajor, 
        ncbi::CInterfaceVersion<CDataLoader>::eMinor, 
        patch_level >= 0 ?
        patch_level : ncbi::CInterfaceVersion<CDataLoader>::ePatchLevel),
        m_DriverName(driver_name)
{
    _ASSERT(!m_DriverName.empty());
}


void CDataLoaderFactory::GetDriverVersions(TDriverList& info_list) const
{
    info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
}


CDataLoader* CDataLoaderFactory::CreateInstance(
    const string&                  driver,
    CVersionInfo                   version,
    const TPluginManagerParamTree* params) const
{
    CDataLoader* loader = 0;
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(CDataLoader))
                            != CVersionInfo::eNonCompatible) {
            CObjectManager* om = x_GetObjectManager(params);
            _ASSERT(om);
            loader = CreateAndRegister(*om, params);
        }
    }
    return loader;
}


CObjectManager* CDataLoaderFactory::x_GetObjectManager(
    const TPluginManagerParamTree* params) const
{
    const string& om_str =
        GetParam(m_DriverName, params, kCFParam_ObjectManagerPtr, false, "0");
    CObjectManager* om = static_cast<CObjectManager*>(
        const_cast<void*>(NStr::StringToPtr(om_str)));
    return om ? om : &*CObjectManager::GetInstance();
}


CObjectManager::EIsDefault CDataLoaderFactory::GetIsDefault(
    const TPluginManagerParamTree* params) const
{
    const string& def_str =
        GetParam(m_DriverName, params, kCFParam_DataLoader_IsDefault, false,
        "NonDefault");
    return (NStr::CompareNocase(def_str, "Default") == 0) ?
        CObjectManager::eDefault : CObjectManager::eNonDefault;
}


CObjectManager::TPriority CDataLoaderFactory::GetPriority(
    const TPluginManagerParamTree* params) const
{
    const string& priority_str =
        GetParam(m_DriverName, params, kCFParam_DataLoader_Priority, false,
        NStr::IntToString(CObjectManager::kPriority_NotSet));
    return NStr::StringToInt(priority_str);
}


bool CDataLoaderFactory::ValidParams(
    const TPluginManagerParamTree* params) const
{
    if (!params) {
        return false;
    }
    // Check param tree name
    const string& tree_id = params->GetId();
    if (NStr::CompareNocase(tree_id, m_DriverName) != 0) {
        LOG_POST(Warning
          << "CDataLoaderFactory: Top level Id does not match driver name."
          << " Id = " << tree_id << " driver=" << m_DriverName
          << " parameters ignored." );
        return false;
    }
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/08/02 17:17:41  grichenk
* Initial revision
*
*
* ===========================================================================
*/
