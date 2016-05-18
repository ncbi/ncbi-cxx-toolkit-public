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
 * Author: Mike DiCuccio Cheinan Marks
 *
 * File Description:  AsnCache dataloader. Implementations.
 *
 */


#include <ncbi_pch.hpp>

#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>

#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache_loader.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_AsnCache_Loader

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

CAsnCache_DataLoader::SCacheInfo::SCacheInfo()
    : requests(0)
    , found(0)
{
}


CAsnCache_DataLoader::SCacheInfo::~SCacheInfo()
{
}


CAsnCache_DataLoader::TRegisterLoaderInfo CAsnCache_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& db_path,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TDbMaker maker(db_path);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CAsnCache_DataLoader::GetLoaderNameFromArgs(void)
{
    return "AsnCache_dataloader";
}


string CAsnCache_DataLoader::GetLoaderNameFromArgs(const string& db_path)
{
    return string("AsnCache_dataloader:") + db_path;
}


CAsnCache_DataLoader::CAsnCache_DataLoader(void)
    : CDataLoader(GetLoaderNameFromArgs())
{
    m_IndexMap.resize(15);
}


CAsnCache_DataLoader::CAsnCache_DataLoader(const string& dl_name)
    : CDataLoader(dl_name)
{
    m_IndexMap.resize(15);
}

CAsnCache_DataLoader::CAsnCache_DataLoader(const string& dl_name,
                                           const string& db_path)
    : CDataLoader(dl_name),
      m_DbPath(db_path)
{
    m_IndexMap.resize(15);
}


CAsnCache_DataLoader::~CAsnCache_DataLoader()
{
    /**
    size_t total_requests = 0;
    size_t total_found = 0;
    ITERATE (TIndexMap, it, m_IndexMap) {
        LOG_POST(Error << "thread=" << it->first
                 << "  requests=" << it->second.requests
                 << "  found=" << it->second.found);
        total_requests += it->second.requests;
        total_found    += it->second.found;
    }
    LOG_POST(Error << "total requests: " << total_requests);
    LOG_POST(Error << "total found:    " << total_found);
    **/
}


CAsnCache_DataLoader::TBlobId
CAsnCache_DataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    CAsnIndex::SIndexInfo info;
    TBlobId blob_id;
    if (index.cache->GetIndexEntry(idh, info)) {
        blob_id = new CBlobIdSeq_id(idh);
    }
    //LOG_POST(Error << "CAsnCache_DataLoader::GetBlobId(): " << idh);
    return blob_id;
}


bool CAsnCache_DataLoader::CanGetBlobById() const
{
    return true;
}


TGi CAsnCache_DataLoader::GetGi(const CSeq_id_Handle& idh)
{
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    CAsnIndex::TGi gi = 0;
    time_t timestamp = 0;
    if (index.cache->GetIdInfo(idh, gi, timestamp)) {
        //LOG_POST(Error << "CAsnCache_DataLoader::GetGi(): " << idh << " -> " << gi);
        return GI_FROM(CAsnIndex::TGi, gi);
    }
    return ZERO_GI;
}


TSeqPos CAsnCache_DataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    CAsnIndex::TGi gi = 0;
    time_t timestamp = 0;
    Uint4 sequence_length = 0;
    Uint4 tax_id = 0;
    CSeq_id_Handle acc;
    if (index.cache->GetIdInfo(idh, acc, gi,
                               timestamp, sequence_length, tax_id)) {
        return sequence_length;
    }
    return kInvalidSeqPos;
}




void CAsnCache_DataLoader::GetIds(const CSeq_id_Handle& idh,
                                  TIds& ids)
{
    ///
    /// okay, the contract is that we must return something if we know the
    /// sequence.  thus, if the sequence exists in the cache, we must return
    /// something. If the SeqId index is available, the cache will use it to
    /// get the ids quickly; otherwise it will use the expensive way, retrieving
    /// the entire sequence.
    ///
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    vector<CSeq_id_Handle> bioseq_ids;
    bool res = index.cache->GetSeqIds(idh, bioseq_ids, false);
    ++index.requests;
    if (res) {
        ids.swap(bioseq_ids);
    }
}


int CAsnCache_DataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    CAsnIndex::TGi gi = 0;
    time_t timestamp = 0;
    Uint4 sequence_length = 0;
    Uint4 tax_id = 0;
    CSeq_id_Handle acc;
    if (index.cache->GetIdInfo(idh, acc, gi,
                               timestamp, sequence_length, tax_id)) {
        return tax_id;
    }
    return -1;
}


#if NCBI_PRODUCTION_VER > 20110000
/// not yet in SC-6.0...
void CAsnCache_DataLoader::GetGis(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    SCacheInfo& index = x_GetIndex();
    CFastMutexGuard LOCK(index.cache_mtx);

    ret.clear();
    ret.resize(ids.size());

    loaded.clear();
    loaded.resize(ids.size());
    for (size_t i = 0;  i < ids.size();  ++i) {
        CAsnIndex::TGi gi = 0;
        time_t timestamp = 0;
        if (index.cache->GetIdInfo(ids[i], gi, timestamp)) {
            ret[i] = CSeq_id_Handle::GetHandle(GI_FROM(CAsnIndex::TGi, gi));
            loaded[i] = true;
        }
    }
}
#endif

CAsnCache_DataLoader::TTSE_Lock
CAsnCache_DataLoader::GetBlobById(const TBlobId& blob_id)
{
    CSeq_id_Handle idh =
        dynamic_cast<const CBlobIdSeq_id&>(*blob_id).GetValue();

    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !lock.IsLoaded() ) {
        SCacheInfo& index = x_GetIndex();
        CFastMutexGuard LOCK(index.cache_mtx);

        CRef<CSeq_entry> entry = index.cache->GetEntry(idh);
        ++index.requests;

        if (entry) {
            ++index.found;
            lock->SetSeq_entry(*entry);
            lock.SetLoaded();

            /**
            LOG_POST(Error
                     << "CAsnCache_DataLoader::GetBlobById(): loaded seq-entry for "
                     << idh);
        } else {

            LOG_POST(Error
                     << "CAsnCache_DataLoader::GetBlobById(): failed to load seq-entry for "
                     << idh);
                     **/
        }
        /**
    } else {
        LOG_POST(Error
                 << "CAsnCache_DataLoader::GetBlobById(): already loaded: "
                 << idh);
                 **/
    }
    return lock;
}


CDataLoader::TTSE_LockSet
CAsnCache_DataLoader::GetRecords(const CSeq_id_Handle& idh,
                                 EChoice choice)
{
    TTSE_LockSet locks;

    switch ( choice ) {
    case eBlob:
    case eBioseq:
    case eCore:
    case eBioseqCore:
    case eSequence:
    case eAll:
        {{
             TBlobId blob_id = GetBlobId(idh);
             if (blob_id) {
                 locks.insert(GetBlobById(blob_id));
             }
         }}
        break;

    default:
        break;
    }

    return locks;
}

CAsnCache_DataLoader::SCacheInfo& CAsnCache_DataLoader::x_GetIndex()
{
    if (m_IndexMap.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "setup failure: no cache objects available");
    }

    CFastMutexGuard LOCK(m_Mutex);

    // hash to a pool of cache objects based on thread ID
    int id = CThread::GetSelf();
    id %= m_IndexMap.size();

    TIndexMap::iterator iter = m_IndexMap.begin() + id;
    if ( !iter->get() ) {
        iter->reset(new SCacheInfo);
        (*iter)->cache.Reset(new CAsnCache(m_DbPath));
    }
    return **iter;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_AsnCache(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_AsnCache);
}


const string kDataLoader_AsnCache_DriverName("asncache");

class CAsnCache_DataLoaderCF : public CDataLoaderFactory
{
public:
    CAsnCache_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_AsnCache_DriverName) {}
    virtual ~CAsnCache_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CAsnCache_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    string db_path =
        GetParam(GetDriverName(), params,
                 "DbPath", false);

    // IsDefault and Priority arguments may be specified
    return CAsnCache_DataLoader::RegisterInObjectManager(om, db_path).GetLoader();
}


void NCBI_EntryPoint_DataLoader_AsnCache(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CAsnCache_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_asncache(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_AsnCache(info_list, method);
}


END_NCBI_SCOPE
