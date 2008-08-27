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
#include <objtools/lds/lds_query.hpp>
#include <objtools/lds/lds_manager.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLDS_Database;
class CLDS_IStreamCache;

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

    void SetIStreamCacheSize(size_t size);

// private:
    typedef CSimpleLoaderMaker<CLDS_DataLoader>                TSimpleMaker;
    typedef CParamLoaderMaker<CLDS_DataLoader, CLDS_Database&> TDbMaker;
    friend class CSimpleLoaderMaker<CLDS_DataLoader>;
    friend class CParamLoaderMaker<CLDS_DataLoader, CLDS_Database&>;

protected:
    CRef<CSeq_entry> x_LoadTSE(const CLDS_Query::SObjectDescr& obj_descr);
    CRef<CLDS_IStreamCache> x_GetIStreamCache(void);

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

    CRef<CLDS_IStreamCache> m_IStreamCache;

private: // to prevent copying
    CLDS_DataLoader(const CLDS_DataLoader&);
    void operator==(const CLDS_DataLoader&);
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

#endif
