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
 * File Description: CDD file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/serial.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/id2/id2__.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objtools/data_loaders/cdd/cdd_loader/cdd_loader.hpp>
#include <objtools/data_loaders/cdd/cdd_loader/cdd_loader_impl.hpp>
#include <objtools/data_loaders/cdd/id2cdd_params.h>


BEGIN_NCBI_SCOPE

class CObject;

BEGIN_SCOPE(objects)

class CDataLoader;

BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CCDDDataLoader*>(&loader) != 0;
    }
};


class CRevoker {
public:
    ~CRevoker() {
        CLoaderFilter filter;
        CObjectManager::GetInstance()->RevokeDataLoaders(filter);
    }
};
static CSafeStatic<CRevoker> s_Revoker(CSafeStaticLifeSpan(
    CSafeStaticLifeSpan::eLifeLevel_AppMain,
    CSafeStaticLifeSpan::eLifeSpan_Long));

END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CCDDDataLoader
/////////////////////////////////////////////////////////////////////////////


CCDDDataLoader::SLoaderParams::SLoaderParams(void)
    : m_Compress(false),
      m_PoolSoftLimit(DEFAULT_CDD_POOL_SOFT_LIMIT),
      m_PoolAgeLimit(DEFAULT_CDD_POOL_AGE_LIMIT),
      m_ExcludeNucleotides(DEFAULT_CDD_EXCLUDE_NUCLEOTIDES)
{
}


static const char* kCDDLoaderName = "CDDDataLoader";
static const char* kCDDLoaderParamName = "CDD_Loader";


CCDDDataLoader::SLoaderParams::SLoaderParams(const TPluginManagerParamTree& params)
{
    const TParamTree* node = params.FindSubNode(kCDDLoaderParamName);

    CConfig conf(node);

    m_ServiceName = conf.GetString(kCDDLoaderParamName,
        NCBI_ID2PROC_CDD_PARAM_SERVICE_NAME,
        CConfig::eErr_NoThrow,
        DEFAULT_CDD_SERVICE_NAME);

    m_Compress = conf.GetBool(kCDDLoaderParamName,
        NCBI_ID2PROC_CDD_PARAM_COMPRESS_DATA,
        CConfig::eErr_NoThrow,
        false);

    m_PoolSoftLimit = conf.GetInt(kCDDLoaderParamName,
        NCBI_ID2PROC_CDD_PARAM_POOL_SOFT_LIMIT,
        CConfig::eErr_NoThrow, DEFAULT_CDD_POOL_SOFT_LIMIT);

    m_PoolAgeLimit = conf.GetInt(kCDDLoaderParamName,
        NCBI_ID2PROC_CDD_PARAM_POOL_AGE_LIMIT,
        CConfig::eErr_NoThrow, DEFAULT_CDD_POOL_AGE_LIMIT);

    m_ExcludeNucleotides = conf.GetBool(kCDDLoaderParamName,
        NCBI_ID2PROC_CDD_PARAM_EXCLUDE_NUCLEOTIDES,
        CConfig::eErr_NoThrow, DEFAULT_CDD_EXCLUDE_NUCLEOTIDES);

}


CCDDDataLoader::SLoaderParams::~SLoaderParams(void)
{
}


CCDDDataLoader::TRegisterLoaderInfo CCDDDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCDDDataLoader::TRegisterLoaderInfo CCDDDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params(param_tree);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCDDDataLoader::TRegisterLoaderInfo CCDDDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CCDDDataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    return kCDDLoaderName;
}


string CCDDDataLoader::GetLoaderNameFromArgs(void)
{
    SLoaderParams params;
    return GetLoaderNameFromArgs(params);
}


CCDDDataLoader::CCDDDataLoader(const string& loader_name,
                               const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    m_Impl = new CCDDDataLoader_Impl(params);
}


CCDDDataLoader::~CCDDDataLoader(void)
{
}


CDataLoader::TTSE_LockSet
CCDDDataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice choice)
{
    TTSE_LockSet locks;
    return locks;
}


CDataLoader::TTSE_LockSet
CCDDDataLoader::GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    TProcessedNAs* processed_nas)
{
    TSeq_idSet ids;
    ids.insert(idh);
    return m_Impl->GetBlobBySeq_ids(ids, *GetDataSource());
}


CDataLoader::TTSE_LockSet
CCDDDataLoader::GetOrphanAnnotRecordsNA(const TSeq_idSet& ids,
    const SAnnotSelector* sel,
    TProcessedNAs* processed_nas)
{
    return m_Impl->GetBlobBySeq_ids(ids, *GetDataSource());
}


CObjectManager::TPriority CCDDDataLoader::GetDefaultPriority(void) const
{
    return CObjectManager::kPriority_Replace;
}


/////////////////////////////////////////////////////////////////////////////
// CCDDBlobId
/////////////////////////////////////////////////////////////////////////////


CCDDBlobId::CCDDBlobId(CTempString str_id)
{
    try {
        str_id >> Get();
    }
    catch (...) {
    }
}


CCDDBlobId::CCDDBlobId(const CID2_Blob_Id& blob_id)
{
    m_Id2BlobId.Reset(new CID2_Blob_Id);
    m_Id2BlobId->Assign(blob_id);
}


const CID2_Blob_Id& CCDDBlobId::Get(void) const
{
    if (!m_Id2BlobId) {
        m_Id2BlobId.Reset(new CID2_Blob_Id());
    }
    return *m_Id2BlobId;
}


CID2_Blob_Id& CCDDBlobId::Get(void)
{
    if (!m_Id2BlobId) {
        m_Id2BlobId.Reset(new CID2_Blob_Id());
    }
    return *m_Id2BlobId;
}


string CCDDBlobId::ToString(void) const
{
    string ret;
    if (!IsEmpty())
    {
        ret << *m_Id2BlobId;
    }
    return ret;
}


bool CCDDBlobId::operator<(const CBlobId& blob_id) const
{
    const CCDDBlobId* cdd = dynamic_cast<const CCDDBlobId*>(&blob_id);
    return cdd  &&  *this < *cdd;
}


bool CCDDBlobId::operator==(const CBlobId& blob_id) const
{
    const CCDDBlobId* cdd = dynamic_cast<const CCDDBlobId*>(&blob_id);
    return cdd  &&  *this == *cdd;
}


#define CMP_MEMBER(NAME) \
    do { \
        T##NAME v1 = Get##NAME(); \
        T##NAME v2 = blob_id.Get##NAME(); \
        if (v1 != v2) return v1 < v2; \
    } while (0)


bool CCDDBlobId::operator<(const CCDDBlobId& blob_id) const
{
    // NULL is less than any blob-id.
    if (IsEmpty()) return !blob_id.IsEmpty();

    CMP_MEMBER(Sat);
    CMP_MEMBER(SubSat);
    CMP_MEMBER(SatKey);
    CMP_MEMBER(Version);
    return false;
}


bool CCDDBlobId::operator==(const CCDDBlobId& blob_id) const
{
    if (IsEmpty()) return blob_id.IsEmpty();
    return GetSat() == blob_id.GetSat()
        && GetSubSat() == blob_id.GetSubSat()
        && GetSatKey() == blob_id.GetSatKey()
        && GetVersion() == blob_id.GetVersion();
}


/////////////////////////////////////////////////////////////////////////////
// CCDDDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CCDDDataLoader_Impl::CCDDDataLoader_Impl(const CCDDDataLoader::SLoaderParams& params)
    : m_ClientPool(
        params.m_ServiceName,
        params.m_PoolSoftLimit,
        params.m_PoolAgeLimit,
        params.m_ExcludeNucleotides)
{
}


CCDDDataLoader_Impl::~CCDDDataLoader_Impl(void)
{
}


CDataLoader::TTSE_LockSet
CCDDDataLoader_Impl::GetBlobBySeq_ids(const TSeq_idSet& ids, CDataSource& ds)
{
    CDataLoader::TTSE_LockSet ret;
    TBlob blob = m_ClientPool.GetBlobBySeq_ids(ids);
    if (!blob.data) return ret;

    CRef<CCDDBlobId> cdd_blob_id(new CCDDBlobId(blob.info->GetBlob_id()));
    CDataLoader::TBlobId blob_id(cdd_blob_id);
    CTSE_LoadLock load_lock = ds.GetTSE_LoadLock(blob_id);
    if (!load_lock.IsLoaded()) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSet().SetSeq_set();
        entry->SetAnnot().push_back(blob.data);
        load_lock->SetName("CDD");
        load_lock->SetSeq_entry(*entry);
        load_lock.SetLoaded();
    }
    if (load_lock.IsLoaded()) {
        ret.insert(load_lock);
    }
    return ret;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_CDD(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Cdd);
}


const string kDataLoader_Cdd_DriverName("cdd");

class CCDD_DataLoaderCF : public CDataLoaderFactory
{
public:
    CCDD_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Cdd_DriverName) {}
    virtual ~CCDD_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CCDD_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CCDDDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    if (params) {
        return CCDDDataLoader::RegisterInObjectManager(
            om,
            *params,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CCDDDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Cdd(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCDD_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_cdd(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Cdd(info_list, method);
}


END_NCBI_SCOPE
