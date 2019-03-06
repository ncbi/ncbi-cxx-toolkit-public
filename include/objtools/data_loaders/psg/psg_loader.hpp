#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPSGDataLoader_Impl;

struct SPSGLoaderParams
{
    SPSGLoaderParams(void);

    string m_ServiceName; // PSG service name or host:port
    bool m_NoSplit = false; // Don't use split data.
};


class NCBI_XLOADER_PSG_EXPORT CPSGDataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CPSGDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const SPSGLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& service_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const SPSGLoaderParams& params);
    static string GetLoaderNameFromArgs(const string& service_name);

    TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    TBlobId GetBlobIdFromString(const string& str) const override;

    bool CanGetBlobById(void) const override;
    TTSE_Lock GetBlobById(const TBlobId& blob_id) override;

    TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice) override;

    TTSE_LockSet GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    TTSE_LockSet GetExternalAnnotRecordsNA(const CSeq_id_Handle& idh,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    TTSE_LockSet GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;

    void GetChunk(TChunk chunk) override;
    void GetChunks(const TChunkSet& chunks) override;

    void GetIds(const CSeq_id_Handle& idh, TIds& ids) override;
    int GetTaxId(const CSeq_id_Handle& idh) override;
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh) override;
    SHashFound GetSequenceHashFound(const CSeq_id_Handle& idh) override;
    STypeFound GetSequenceTypeFound(const CSeq_id_Handle& idh) override;

    virtual CObjectManager::TPriority GetDefaultPriority(void) const override;

    void DropTSE(CRef<CTSE_Info> tse_info) override;

private:
    typedef CParamLoaderMaker<CPSGDataLoader, SPSGLoaderParams> TMaker;
    friend class CParamLoaderMaker<CPSGDataLoader, SPSGLoaderParams>;

    // default constructor
    CPSGDataLoader(void);
    // parametrized constructor
    CPSGDataLoader(const string& loader_name, const SPSGLoaderParams& params);
    ~CPSGDataLoader(void);

    CRef<CPSGDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)


extern NCBI_XLOADER_PSG_EXPORT const char kDataLoader_PSG_DriverName[];

extern "C"
{

NCBI_XLOADER_PSG_EXPORT
void NCBI_EntryPoint_DataLoader_PSG(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_PSG_EXPORT
void NCBI_EntryPoint_xloader_psg(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP
