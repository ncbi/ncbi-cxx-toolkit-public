#ifndef DATA_LOADER_FACTORY__HPP
#define DATA_LOADER_FACTORY__HPP

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
* Author:
*
* File Description:
*
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/plugin_manager.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/data_loader.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

/////////////////////////////////////////////////////////////////////////////

// Parameters used by all factories

const string kCFParam_ObjectManagerPtr     = "ObjectManagerPtr";     // pointer
const string kCFParam_DataLoader_Priority  = "DataLoader_Priority";  // int

// string: any value except "Default" results in eNonDefault
const string kCFParam_DataLoader_IsDefault = "DataLoader_IsDefault";


class NCBI_XOBJMGR_EXPORT CDataLoaderFactory
    : public IClassFactory<CDataLoader>
{
public:
    typedef IClassFactory<CDataLoader>     TParent;
    typedef TParent::SDriverInfo           TDriverInfo;
    typedef TParent::TDriverList           TDriverList;

    CDataLoaderFactory(const string& driver_name, int patch_level = -1);
    virtual ~CDataLoaderFactory() {}

    const string& GetDriverName(void) const { return m_DriverName; }
    void GetDriverVersions(TDriverList& info_list) const;

    CDataLoader* 
    CreateInstance(const string& driver = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(CDataLoader),
                   const TPluginManagerParamTree* params = 0) const;

protected:
    // True if params != 0 and node name corresponds to expected name
    bool ValidParams(const TPluginManagerParamTree* params) const;

    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const = 0;

    CObjectManager::EIsDefault GetIsDefault(
        const TPluginManagerParamTree* params) const;
    CObjectManager::TPriority GetPriority(
        const TPluginManagerParamTree* params) const;

private:
    CObjectManager* x_GetObjectManager(
        const TPluginManagerParamTree* params) const;

    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;
};


template  <class TDataLoader>
class CSimpleDataLoaderFactory : public CDataLoaderFactory
{
public:
    CSimpleDataLoaderFactory(const string& name)
        : CDataLoaderFactory(name)
    {
        return;
    }
    virtual ~CSimpleDataLoaderFactory() {
        return;
    }

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const
    {
        return TDataLoader::RegisterInObjectManager(
            om,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/08/02 17:34:43  grichenk
* Added data_loader_factory.cpp.
* Renamed xloader_cdd to ncbi_xloader_cdd.
* Implemented data loader factories for all loaders.
*
* Revision 1.6  2004/07/28 14:02:56  grichenk
* Improved MT-safety of RegisterInObjectManager(), simplified the code.
*
* Revision 1.5  2004/07/26 19:15:39  friedman
* Templates can not be exported. As per Mike D.
*
* Revision 1.4  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.3  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.2  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif // DATA_LOADER_FACTORY__HPP
