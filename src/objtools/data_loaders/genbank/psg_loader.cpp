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
#include <objtools/data_loaders/genbank/seqref.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <objtools/data_loaders/genbank/psg_loader.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;

NCBI_PARAM_ENUM_ARRAY(objects::CSeq_id::ESNPScaleLimit, PSG_LOADER, SNP_SCALE_LIMIT) {
    {"", objects::CSeq_id::eSNPScaleLimit_Default},
    {"Unit", objects::CSeq_id::eSNPScaleLimit_Unit},
    {"Contig", objects::CSeq_id::eSNPScaleLimit_Contig},
    {"Supercontig", objects::CSeq_id::eSNPScaleLimit_Supercontig},
    {"Chromosome", objects::CSeq_id::eSNPScaleLimit_Chromosome},
};
NCBI_PARAM_ENUM_DECL(objects::CSeq_id::ESNPScaleLimit, PSG_LOADER, SNP_SCALE_LIMIT);
NCBI_PARAM_ENUM_DEF_EX(objects::CSeq_id::ESNPScaleLimit, PSG_LOADER, SNP_SCALE_LIMIT,
                       objects::CSeq_id::eSNPScaleLimit_Default,
                       eParam_NoThread, PSG_LOADER_SNP_SCALE_LIMIT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, SNP_SCALE_LIMIT) TSNP_Scale_Limit;

BEGIN_NAMESPACE(objects);

/////////////////////////////////////////////////////////////////////////////
// CPsgBlobId
/////////////////////////////////////////////////////////////////////////////


CPsgBlobId::CPsgBlobId(const string& id)
    : m_Id(id)
{
}


CPsgBlobId::CPsgBlobId(const string& id, bool is_dead)
    : m_Id(id),
      m_HasBioseqIsDead(true),
      m_BioseqIsDead(is_dead)
{
}


CPsgBlobId::CPsgBlobId(const string& id, const string& id2_info)
    : m_Id(id),
      m_Id2Info(id2_info)
{
}


CPsgBlobId::~CPsgBlobId()
{
}


string CPsgBlobId::ToString(void) const
{
    return m_Id;
}


bool CPsgBlobId::operator==(const CBlobId& id_ref) const
{
    const CPsgBlobId* id = dynamic_cast<const CPsgBlobId*>(&id_ref);
    return id && m_Id == id->m_Id;
}


bool CPsgBlobId::operator<(const CBlobId& id_ref) const
{
    const CPsgBlobId* id = dynamic_cast<const CPsgBlobId*>(&id_ref);
    if ( !id ) {
        return LessByTypeId(id_ref);
    }
    return m_Id < id->m_Id;
}


bool CPsgBlobId::GetSatSatkey(int& sat, int& satkey) const
{
    string ssat, ssatkey;
    NStr::SplitInTwo(m_Id, ".", ssat, ssatkey);
    if (ssat.empty() || ssatkey.empty()) return false;
    try {
        sat = NStr::StringToNumeric<int>(ssat);
        satkey = NStr::StringToNumeric<int>(ssatkey);
    }
    catch (CStringException&) {
        return false;
    }
    return true;
}


CConstRef<CPsgBlobId> CPsgBlobId::GetPsgBlobId(const CBlobId& blob_id)
{
    if ( auto psg_blob_id = dynamic_cast<const CPsgBlobId*>(&blob_id) ) {
        return ConstRef(psg_blob_id);
    }
    const CBlob_id* gb_blob_id = dynamic_cast<const CBlob_id*>(&blob_id);
    if (!gb_blob_id) {
        NCBI_THROW(CLoaderException, eOtherError, "Incompatible blob-id: " + blob_id.ToString());
    }
    return ConstRef(new CPsgBlobId(
                        NStr::NumericToString(gb_blob_id->GetSat()) +
                        '.' +
                        NStr::NumericToString(gb_blob_id->GetSatKey())));
}


/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader
/////////////////////////////////////////////////////////////////////////////


#define PSGLOADER_NAME "GBLOADER"

const char kDataLoader_PSG_DriverName[] = "psg";


CSeq_id::ESNPScaleLimit CPSGDataLoader::GetSNP_Scale_Limit(void)
{
    return TSNP_Scale_Limit::GetDefault();
}


void CPSGDataLoader::SetSNP_Scale_Limit(CSeq_id::ESNPScaleLimit value)
{
    TSNP_Scale_Limit::SetDefault(value);
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    return RegisterInObjectManager(om, CGBLoaderParams(), is_default, priority);
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const CGBLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    CGBLoaderMaker<CPSGDataLoader> maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
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


CDataLoader::TBlobId CPSGDataLoader::GetBlobIdFromSatSatKey(int sat,
                                                            int sat_key,
                                                            int sub_sat) const
{
    string str = NStr::NumericToString(sat)+'.'+NStr::NumericToString(sat_key);
    if ( sub_sat != CSeqref::eSubSat_main ) {
        str += '.'+NStr::NumericToString(sub_sat);
    }
    return TBlobId(new CPsgBlobId(str));
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


namespace {
    struct SBetterId
    {
        int GetScore(const CSeq_id_Handle& id1) const
        {
            if ( id1.IsGi() ) {
                return 100;
            }
            if ( !id1 ) {
                return -1;
            }
            CConstRef<CSeq_id> seq_id = id1.GetSeqId();
            const CTextseq_id* text_id = seq_id->GetTextseq_Id();
            if ( text_id ) {
                int score;
                if ( text_id->IsSetAccession() ) {
                    if ( text_id->IsSetVersion() ) {
                        score = 99;
                    }
                    else {
                        score = 50;
                    }
                }
                else {
                    score = 0;
                }
                return score;
            }
            if ( seq_id->IsGeneral() ) {
                return 10;
            }
            if ( seq_id->IsLocal() ) {
                return 0;
            }
            return 1;
        }
        bool operator()(const CSeq_id_Handle& id1,
                        const CSeq_id_Handle& id2) const
        {
            int score1 = GetScore(id1);
            int score2 = GetScore(id2);
            if ( score1 != score2 ) {
                return score1 > score2;
            }
            return id1 < id2;
        }
    };
}


CPSGDataLoader::TTSE_LockSet CPSGDataLoader::GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
                                                                       const SAnnotSelector* sel,
                                                                       TProcessedNAs* processed_nas)
{
    TIds ids = bioseq.GetId();
    sort(ids.begin(), ids.end(), SBetterId());
    return m_Impl->GetAnnotRecordsNA(GetDataSource(), ids, sel, processed_nas);
}


CPSGDataLoader::TTSE_LockSet CPSGDataLoader::GetOrphanAnnotRecordsNA(const TSeq_idSet& seq_ids,
                                                                     const SAnnotSelector* sel,
                                                                     TProcessedNAs* processed_nas)
{
    TIds ids(seq_ids.begin(), seq_ids.end());
    sort(ids.begin(), ids.end(), SBetterId());
    return m_Impl->GetAnnotRecordsNA(GetDataSource(), ids, sel, processed_nas);
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


void CPSGDataLoader::GetCDDAnnots(const TBioseq_InfoSet& seq_set, TLoaded& loaded, TCDD_Locks& ret)
{
    m_Impl->GetCDDAnnots(GetDataSource(), seq_set, loaded, ret);
}


CDataLoader::TTSE_Lock
CPSGDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               *CPsgBlobId::GetPsgBlobId(*blob_id));
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
    auto taxid = m_Impl->GetTaxId(idh);
    return taxid != INVALID_TAX_ID ? taxid : CDataLoader::GetTaxId(idh);
}


void CPSGDataLoader::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    m_Impl->GetTaxIds(ids, loaded, ret);
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
    return m_Impl->GetSequenceState(GetDataSource(), idh);
}


void CPSGDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    m_Impl->DropTSE(dynamic_cast<const CPsgBlobId&>(*tse_info->GetBlobId()));
}


void CPSGDataLoader::GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    m_Impl->GetBulkIds(ids, loaded, ret);
}


void CPSGDataLoader::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    m_Impl->GetAccVers(ids, loaded, ret);
}


void CPSGDataLoader::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    m_Impl->GetGis(ids, loaded, ret);
}


void CPSGDataLoader::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    m_Impl->GetLabels(ids, loaded, ret);
}


void CPSGDataLoader::GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret)
{
    m_Impl->GetSequenceLengths(ids, loaded, ret);
}


void CPSGDataLoader::GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret)
{
    m_Impl->GetSequenceTypes(ids, loaded, ret);
}


void CPSGDataLoader::GetSequenceStates(const TIds& ids, TLoaded& loaded, TSequenceStates& ret)
{
    m_Impl->GetSequenceStates(GetDataSource(), ids, loaded, ret);
}


void CPSGDataLoader::GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known)
{
    m_Impl->GetSequenceHashes(ids, loaded, ret, known);
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


CGBDataLoader::TRealBlobId
CPSGDataLoader::x_GetRealBlobId(const TBlobId& blob_id) const
{
    const CPsgBlobId* psg_blob_id = dynamic_cast<const CPsgBlobId*>(&*blob_id);
    if (psg_blob_id) {
        int sat, satkey;
        if (psg_blob_id->GetSatSatkey(sat, satkey)) {
            CBlob_id gb_blob_id;
            gb_blob_id.SetSat(sat);
            gb_blob_id.SetSatKey(satkey);
            return gb_blob_id;
        }
    }
    const CBlob_id* gb_blob_id = dynamic_cast<const CBlob_id*>(&*blob_id);
    if (gb_blob_id) return *gb_blob_id;
    return CBlob_id();
}


END_NAMESPACE(objects);

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

END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
