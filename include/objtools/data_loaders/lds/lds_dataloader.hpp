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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Object manager compliant data loader for local data
 *                   storage (LDS).
 *
 */

#ifndef LDS_DATALOADER_HPP
#define LDS_DATALOADER_HPP

#include <objmgr/data_loader.hpp>

#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_manager.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLDS_Database;

//////////////////////////////////////////////////////////////////
//
// CLDS_DataLoader.
// CDataLoader implementation for LDS.
//

// Parameter names used by loader factory

//const string kCFParam_LDS_Database = "Database"; // = CLDS_Database*
const string kCFParam_LDS_DbPath   = "DbPath";   // = string
const string kCFParam_LDS_DbAlias   = "DbAlias";   // = string
const string kCFParam_LDS_SourcePath   = "SourcePath";   // = string
const string kCFParam_LDS_RecurseSubDir  = "RecurseSubDir"; // = bool
const string kCFParam_LDS_ControlSum     = "ControlSum";    // = bool


class NCBI_XLOADER_LDS_EXPORT CLDS_DataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CLDS_DataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CLDS_Database& lds_db,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CLDS_Database& lds_db);

    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& source_path,
        const string& db_path,
        const string& db_alias,
        CLDS_Manager::ERecurse recurse = CLDS_Manager::eRecurseSubDirs,
        CLDS_Manager::EComputeControlSum csum = 
                                         CLDS_Manager::eComputeControlSum,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CLDS_DataLoader(void);

    virtual ~CLDS_DataLoader(void);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    
    void SetDatabase(CLDS_Database& lds_db, EOwnership owner,
                     const string&  dl_name);
    
    CLDS_Database& GetDatabase();
// private:
    typedef CSimpleLoaderMaker<CLDS_DataLoader>                TSimpleMaker;
    typedef CParamLoaderMaker<CLDS_DataLoader, CLDS_Database&> TDbMaker;
    friend class CSimpleLoaderMaker<CLDS_DataLoader>;
    friend class CParamLoaderMaker<CLDS_DataLoader, CLDS_Database&>;

private:
    class CLDS_LoaderMaker : public TSimpleMaker
    {
    public:
        CLDS_LoaderMaker(const string& source_path,
                         const string& db_path,
                         const string& db_alias,
                         CLDS_Manager::ERecurse           recurse,
                         CLDS_Manager::EComputeControlSum csum);

        virtual CDataLoader* CreateLoader(void) const;
    private:
       
        string m_SourcePath;
        string m_DbPath;
        string m_DbAlias;
        CLDS_Manager::ERecurse            m_Recurse;
        CLDS_Manager::EComputeControlSum  m_ControlSum;
    };
    friend class CLDS_LoaderMaker;

private:
    CLDS_DataLoader(const string& dl_name);

    // Construct dataloader, attach the external LDS database
    CLDS_DataLoader(const string& dl_name,
                    CLDS_Database& lds_db);

    // Construct dataloader, take ownership of  LDS database
    CLDS_DataLoader(const string& dl_name,
                    CLDS_Database* lds_db);

    CLDS_Database*      m_LDS_db;        // Reference on the LDS database 
    bool                m_OwnDatabase;   // "TRUE" if datalaoder owns m_LDS_db
};

END_SCOPE(objects)


extern NCBI_XLOADER_LDS_EXPORT const string kDataLoader_LDS_DriverName;

extern "C"
{

NCBI_XLOADER_LDS_EXPORT
void NCBI_EntryPoint_DataLoader_LDS(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_LDS_EXPORT
void NCBI_EntryPoint_xloader_lds(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2006/11/07 21:13:10  didenko
 * Added GetDatabase method
 *
 * Revision 1.22  2006/10/02 14:38:22  didenko
 * Cleaned up dataloader implementation
 *
 * Revision 1.21  2005/10/26 14:36:44  vasilche
 * Updated for new CBlobId interface. Fixed load lock logic.
 *
 * Revision 1.20  2005/09/22 01:22:55  ucko
 * Also make internal typedefs public for the sake of WorkShop, for
 * which the previous fix proved insufficient.
 *
 * Revision 1.19  2005/09/22 01:14:13  ucko
 * Explicitly declare CLDS_LoaderMaker to be a friend of CLDS_DataLoader,
 * as not all compilers automatically grant friendship to inner classes.
 *
 * Revision 1.18  2005/09/21 13:32:47  kuznets
 * Addedcvs diff lds_dataloader.hpp
 *
 * Revision 1.17  2005/07/15 19:52:25  vasilche
 * Use blob_id map from CDataSource.
 *
 * Revision 1.16  2004/08/10 16:56:10  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.15  2004/08/04 19:35:09  grichenk
 * Renamed entry points to be found by dll resolver.
 * GB loader uses CPluginManagerStore to get/put
 * plugin manager for readers.
 *
 * Revision 1.14  2004/08/04 14:56:34  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.13  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.12  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.11  2004/07/26 14:13:31  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.10  2004/07/21 15:51:23  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.9  2003/12/16 20:49:17  vasilche
 * Fixed compile errors - added missing includes and declarations.
 *
 * Revision 1.8  2003/12/16 20:17:25  kuznets
 * Added empty constructor prototype
 *
 * Revision 1.7  2003/11/28 13:40:40  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.6  2003/10/28 14:00:13  kuznets
 * Added constructor parameter name of the dataloader
 *
 * Revision 1.5  2003/09/30 16:36:36  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.4  2003/07/30 16:35:38  kuznets
 * Fixed export macro NCBI_XLOADER_LDS_EXPORT
 *
 * Revision 1.3  2003/06/18 18:47:53  kuznets
 * Minor change: LDS Dataloader can now own the LDS database.
 *
 * Revision 1.2  2003/06/18 15:04:05  kuznets
 * Added dll export/import specs.
 *
 * Revision 1.1  2003/06/16 15:47:53  kuznets
 * Initial revision
 *
 *
 */

#endif

