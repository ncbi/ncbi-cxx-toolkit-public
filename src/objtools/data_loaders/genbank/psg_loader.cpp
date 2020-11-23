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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <objtools/data_loaders/genbank/psg_loader.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader
/////////////////////////////////////////////////////////////////////////////


#define PSGLOADER_NAME "GBLOADER"
#define PSGLOADER_HUP_NAME "GBLOADER-HUP"

const char kDataLoader_PSG_DriverName[] = "psg";

CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const CGBLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    CGBLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    CGBLoaderParams params(&param_tree);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CPSGDataLoader::CPSGDataLoader(const string& loader_name,
                               const CGBLoaderParams& params)
    : CGBDataLoader(loader_name, params)
{
    m_Impl.Reset(new CPSGDataLoader_Impl(params));
}


CPSGDataLoader::~CPSGDataLoader(void)
{
}


CDataLoader::TBlobId CPSGDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(m_Impl->GetBlobId(idh).GetPointerOrNull());
}


CDataLoader::TBlobId
CPSGDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CPsgBlobId(str));
}


bool CPSGDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


CPSGDataLoader::TTSE_LockSet CPSGDataLoader::GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    TProcessedNAs* processed_nas)
{
    return CDataLoader::TTSE_LockSet();
}


CPSGDataLoader::TTSE_LockSet CPSGDataLoader::GetExternalAnnotRecordsNA(const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    TProcessedNAs* processed_nas)
{
    return m_Impl->GetAnnotRecordsNA(GetDataSource(), idh, sel, processed_nas);
}


CPSGDataLoader::TTSE_LockSet CPSGDataLoader::GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
    const SAnnotSelector* sel,
    TProcessedNAs* processed_nas)
{
    TTSE_LockSet ret;
    ITERATE(CBioseq_Info::TId, it, bioseq.GetId()) {
        TTSE_LockSet ret2 = m_Impl->GetAnnotRecordsNA(GetDataSource(), *it, sel, processed_nas);
        if (!ret2.empty()) {
            ret.swap(ret2);
            break;
        }
    }
    return ret;
}


void CPSGDataLoader::GetChunk(TChunk chunk)
{
    m_Impl->LoadChunk(GetDataSource(), *chunk);
}


void CPSGDataLoader::GetChunks(const TChunkSet& chunks)
{
    m_Impl->LoadChunks(GetDataSource(), chunks);
}


void CPSGDataLoader::GetBlobs(TTSE_LockSets& tse_sets)
{
    m_Impl->GetBlobs(GetDataSource(), tse_sets);
}


CDataLoader::TTSE_Lock
CPSGDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               dynamic_cast<const CPsgBlobId&>(*blob_id));
}


void CPSGDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Impl->GetIds(idh, ids);
}


CDataLoader::SGiFound
CPSGDataLoader::GetGiFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetGi(idh);
}


CDataLoader::SAccVerFound
CPSGDataLoader::GetAccVerFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetAccVer(idh);
}


TTaxId CPSGDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetTaxId(idh);
}


TSeqPos CPSGDataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceLength(idh);
}


CDataLoader::SHashFound
CPSGDataLoader::GetSequenceHashFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceHash(idh);
}


CDataLoader::STypeFound
CPSGDataLoader::GetSequenceTypeFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceType(idh);
}


int CPSGDataLoader::GetSequenceState(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceState(idh);
}


void CPSGDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    m_Impl->DropTSE(dynamic_cast<const CPsgBlobId&>(*tse_info->GetBlobId()));
}


void CPSGDataLoader::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    m_Impl->GetAccVers(ids, loaded, ret);
}


void CPSGDataLoader::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    m_Impl->GetGis(ids, loaded, ret);
}


CGBDataLoader::TNamedAnnotNames
CPSGDataLoader::GetNamedAnnotAccessions(const CSeq_id_Handle& sih)
{
    TNamedAnnotNames names;

    /*
    CGBReaderRequestResult result(this, sih);
    SAnnotSelector sel;
    sel.IncludeNamedAnnotAccession("NA*");
    CLoadLockBlobIds blobs(result, sih, &sel);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, &sel);
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ((blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0) {
        if (blob_ids.GetState() == CBioseq_Handle::fState_no_data) {
            // default state - return empty name set
            return names;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
            "blob state error for " + sih.AsString(),
            blob_ids.GetState());
    }

    ITERATE(CFixedBlob_ids, it, blob_ids) {
        const CBlob_Info& info = *it;
        if (!info.IsSetAnnotInfo()) {
            continue;
        }
        CConstRef<CBlob_Annot_Info> annot_info = info.GetAnnotInfo();
        ITERATE(CBlob_Annot_Info::TNamedAnnotNames, jt,
            annot_info->GetNamedAnnotNames()) {
            names.insert(*jt);
        }
    }
    */

    return names;
}


CGBDataLoader::TNamedAnnotNames
CPSGDataLoader::GetNamedAnnotAccessions(const CSeq_id_Handle& sih,
    const string& named_acc)
{
    TNamedAnnotNames names;

    /*
    CGBReaderRequestResult result(this, sih);
    SAnnotSelector sel;
    if (!ExtractZoomLevel(named_acc, 0, 0)) {
        sel.IncludeNamedAnnotAccession(CombineWithZoomLevel(named_acc, -1));
    }
    else {
        sel.IncludeNamedAnnotAccession(named_acc);
    }
    CLoadLockBlobIds blobs(result, sih, &sel);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, &sel);
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ((blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0) {
        if (blob_ids.GetState() == CBioseq_Handle::fState_no_data) {
            // default state - return empty name set
            return names;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
            "blob state error for " + sih.AsString(),
            blob_ids.GetState());
    }

    ITERATE(CFixedBlob_ids, it, blob_ids) {
        const CBlob_Info& info = *it;
        if (!info.IsSetAnnotInfo()) {
            continue;
        }
        CConstRef<CBlob_Annot_Info> annot_info = info.GetAnnotInfo();
        ITERATE(CBlob_Annot_Info::TNamedAnnotNames, jt,
            annot_info->GetNamedAnnotNames()) {
            names.insert(*jt);
        }
    }
    */

    return names;
}


CGBDataLoader::TRegisterLoaderInfo CPSGDataLoader::ConvertRegInfo(const TMaker::TRegisterInfo& info)
{
    TRegisterLoaderInfo ret;
    ret.Set(info.GetLoader(), info.IsCreated());
    return ret;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

class CPSG_DataLoaderCF : public CDataLoaderFactory
{
public:
    CPSG_DataLoaderCF(void)
        : CDataLoaderFactory(objects::kDataLoader_PSG_DriverName) {}
    virtual ~CPSG_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CPSG_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CPSGDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CPSGDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}

END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
