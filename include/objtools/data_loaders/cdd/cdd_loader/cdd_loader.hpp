#ifndef OBJTOOLS_DATA_LOADERS_CDD___CDD_LOADER__HPP
#define OBJTOOLS_DATA_LOADERS_CDD___CDD_LOADER__HPP

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
 * Author: Aleksey Grichenko
 *
 * File Description: CDD data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CCDDDataLoader_Impl;

class NCBI_XLOADER_CDD_EXPORT CCDDDataLoader : public CDataLoader
{
public:

    struct SLoaderParams
    {
        SLoaderParams(void);
        SLoaderParams(const TPluginManagerParamTree& params);
        ~SLoaderParams(void);

        string          m_ServiceName;
        bool            m_Compress;
        size_t          m_PoolSoftLimit;
        time_t          m_PoolAgeLimit;
        bool            m_ExcludeNucleotides;
    };


    typedef TPluginManagerParamTree TParamTree;
    typedef SRegisterLoaderInfo<CCDDDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TParamTree& param_tree,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const SLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice) override;
    TTSE_LockSet GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    TTSE_LockSet GetOrphanAnnotRecordsNA(const TSeq_idSet& ids,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    CObjectManager::TPriority GetDefaultPriority(void) const override;

private:
    typedef CParamLoaderMaker<CCDDDataLoader, SLoaderParams> TMaker;
    friend class CParamLoaderMaker<CCDDDataLoader, SLoaderParams>;

    // default constructor
    CCDDDataLoader(void);
    // parametrized constructor
    CCDDDataLoader(const string& loader_name, const SLoaderParams& params);
    ~CCDDDataLoader(void);

    static string GetLoaderNameFromArgs(const SLoaderParams& params);

    CRef<CCDDDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_CDD_EXPORT const string kDataLoader_Cdd_DriverName;

extern "C"
{

NCBI_XLOADER_CDD_EXPORT
void NCBI_EntryPoint_DataLoader_Cdd(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_CDD_EXPORT
void NCBI_EntryPoint_xloader_cdd(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_CDD___CDD_LOADER__HPP
