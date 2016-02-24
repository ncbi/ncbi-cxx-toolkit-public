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
*   Data loader base class for object manager
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/annot_name.hpp>
#include <objmgr/annot_type_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CDataLoader::RegisterInObjectManager(
    CObjectManager&            om,
    CLoaderMaker_Base&         loader_maker,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    om.RegisterDataLoader(loader_maker, is_default, priority);
}


CDataLoader::CDataLoader(void)
{
    m_Name = NStr::PtrToString(this);
    return;
}


CDataLoader::CDataLoader(const string& loader_name)
    : m_Name(loader_name)
{
    if (loader_name.empty())
    {
        m_Name = NStr::PtrToString(this);
    }
}


CDataLoader::~CDataLoader(void)
{
    return;
}


void CDataLoader::SetTargetDataSource(CDataSource& data_source)
{
    m_DataSource = &data_source;
}


CDataSource* CDataLoader::GetDataSource(void) const
{
    return m_DataSource;
}


void CDataLoader::SetName(const string& loader_name)
{
    m_Name = loader_name;
}


string CDataLoader::GetName(void) const
{
    return m_Name;
}


void CDataLoader::DropTSE(CRef<CTSE_Info> /*tse_info*/)
{
}


void CDataLoader::GC(void)
{
}


CDataLoader::TTSE_LockSet
CDataLoader::GetRecords(const CSeq_id_Handle& /*idh*/,
                        EChoice /*choice*/)
{
    NCBI_THROW(CLoaderException, eNotImplemented,
               "CDataLoader::GetRecords() is not implemented in subclass");
}


CDataLoader::TTSE_LockSet
CDataLoader::GetRecordsNoBlobState(const CSeq_id_Handle& idh,
                                   EChoice choice)
{
    try {
        return GetRecords(idh, choice);
    }
    catch ( CBlobStateException& /* ignored */ ) {
        return TTSE_LockSet();
    }
}


CDataLoader::TTSE_LockSet
CDataLoader::GetDetailedRecords(const CSeq_id_Handle& idh,
                                const SRequestDetails& details)
{
    return GetRecords(idh, DetailsToChoice(details));
}


CDataLoader::TTSE_LockSet
CDataLoader::GetExternalRecords(const CBioseq_Info& bioseq)
{
    TTSE_LockSet ret;
    ITERATE ( CBioseq_Info::TId, it, bioseq.GetId() ) {
        if ( GetBlobId(*it) ) {
            // correct id is found
            TTSE_LockSet ret2 = GetRecords(*it, eExtAnnot);
            ret.swap(ret2);
            break;
        }
    }
    return ret;
}


CDataLoader::TTSE_LockSet
CDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                   const SAnnotSelector* /*sel*/)
{
    return GetRecords(idh, eOrphanAnnot);
}


CDataLoader::TTSE_LockSet
CDataLoader::GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                     const SAnnotSelector* /*sel*/)
{
    return GetRecords(idh, eExtAnnot);
}


CDataLoader::TTSE_LockSet
CDataLoader::GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                     const SAnnotSelector* sel)
{
    TTSE_LockSet ret;
    ITERATE ( CBioseq_Info::TId, it, bioseq.GetId() ) {
        if ( !CanGetBlobById() || GetBlobId(*it) ) {
            // correct id is found
            TTSE_LockSet ret2 = GetExternalAnnotRecords(*it, sel);
            if ( !ret2.empty() ) {
                ret.swap(ret2);
                break;
            }
        }
    }
    return ret;
}


bool CDataLoader::CanGetBlobById(void) const
{
    return false;
}


CDataLoader::TTSE_Lock CDataLoader::GetBlobById(const TBlobId& /*blob_id*/)
{
    NCBI_THROW(CLoaderException, eNotImplemented,
               "CDataLoader::GetBlobById() is not implemented in subclass");
}

CDataLoader::TBlobId CDataLoader::GetBlobIdFromString(const string& /*str*/) const
{
    NCBI_THROW(CLoaderException, eNotImplemented,
               "CDataLoader::GetBlobIdFromString(str) is not implemented in subclass");
}


void CDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    TTSE_LockSet locks = GetRecordsNoBlobState(idh, eBioseqCore);
    ITERATE(TTSE_LockSet, it, locks) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( bs_info ) {
            ids = bs_info->GetId();
            break;
        }
    }
}


bool CDataLoader::SequenceExists(const CSeq_id_Handle& idh)
{
    // check if sequence exists
    TIds ids;
    GetIds(idh, ids);
    return !ids.empty();
}


CSeq_id_Handle CDataLoader::GetAccVer(const CSeq_id_Handle& idh)
{
    // default implementation based on GetIds();
    TIds ids;
    GetIds(idh, ids);
    if ( ids.empty() ) {
        NCBI_THROW(CLoaderException, eNotFound,
                   "CDataLoader::GetAccVer() sequence not found");
    }
    CSeq_id_Handle acc = CScope::x_GetAccVer(ids);
    if ( !acc ) {
        NCBI_THROW(CLoaderException, eNoData,
                   "CDataLoader::GetAccVer() sequence doesn't have accession");
    }
    return acc;
}


CDataLoader::SAccVerFound
CDataLoader::GetAccVerFound(const CSeq_id_Handle& idh)
{
    // default implementation based on GetAccVer() and GetIds()
    SAccVerFound ret;
    try {
        ret.acc_ver = GetAccVer(idh);
        ret.sequence_found = ret.acc_ver || SequenceExists(idh);
    }
    catch ( CLoaderException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFound ) {
            // no sequence
        }
        else if ( exc.GetErrCode() == exc.eNoData ) {
            // sequence is known, but there is no accession
            ret.sequence_found = true;
        }
        else {
            // problem
            throw;
        }
    }
    return ret;
}


TGi CDataLoader::GetGi(const CSeq_id_Handle& idh)
{
    // default implementation based on GetIds();
    TIds ids;
    GetIds(idh, ids);
    if ( ids.empty() ) {
        NCBI_THROW(CLoaderException, eNotFound,
                   "CDataLoader::GetGi() sequence not found");
    }
    TGi gi = CScope::x_GetGi(ids);
    if ( gi == ZERO_GI ) {
        NCBI_THROW(CLoaderException, eNoData,
                   "CDataLoader::GetGi() sequence doesn't have GI");
    }
    return gi;
}


CDataLoader::SGiFound CDataLoader::GetGiFound(const CSeq_id_Handle& idh)
{
    // default implementation based on GetGi() and GetIds()
    SGiFound ret;
    try {
        ret.gi = GetGi(idh);
        ret.sequence_found = ret.gi || SequenceExists(idh);
    }
    catch ( CLoaderException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFound ) {
            // no sequence
        }
        else if ( exc.GetErrCode() == exc.eNoData ) {
            // sequence is known, but there is no GI
            ret.sequence_found = true;
        }
        else {
            // problem
            throw;
        }
    }
    return ret;
}


string CDataLoader::GetLabel(const CSeq_id_Handle& idh)
{
    // default implementation based on GetIds();
    TIds ids;
    GetIds(idh, ids);
    if ( ids.empty() ) {
        return string();
    }
    return objects::GetLabel(ids);
}


int CDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    // default implementation based on GetRecordsNoBlobState();
    TTSE_LockSet locks = GetRecordsNoBlobState(idh, eBioseqCore);
    ITERATE(TTSE_LockSet, it, locks) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( bs_info ) {
            return bs_info->GetTaxId();
        }
    }
    return -1;
}


TSeqPos CDataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    // default implementation based on GetRecordsNoBlobState()
    TTSE_LockSet locks = GetRecordsNoBlobState(idh, eBioseqCore);
    ITERATE(TTSE_LockSet, it, locks) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( bs_info ) {
            return bs_info->GetBioseqLength();
        }
    }
    return kInvalidSeqPos;
}


CSeq_inst::TMol CDataLoader::GetSequenceType(const CSeq_id_Handle& idh)
{
    // default implementation based on GetRecordsNoBlobState()
    TTSE_LockSet locks = GetRecordsNoBlobState(idh, eBioseqCore);
    ITERATE(TTSE_LockSet, it, locks) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( bs_info ) {
            CSeq_inst::TMol type = bs_info->GetInst_Mol();
            if ( type == CSeq_inst::eMol_not_set ) {
                NCBI_THROW(CLoaderException, eNoData,
                           "CDataLoader::GetSequenceType() type not set");
            }
            return type;
        }
    }
    NCBI_THROW(CLoaderException, eNotFound,
               "CDataLoader::GetSequenceType() sequence not found");
}


CDataLoader::STypeFound
CDataLoader::GetSequenceTypeFound(const CSeq_id_Handle& idh)
{
    // default implementation based on GetSequenceType() and GetIds()
    STypeFound ret;
    try {
        ret.type = GetSequenceType(idh);
        ret.sequence_found =
            ret.type != CSeq_inst::eMol_not_set || SequenceExists(idh);
    }
    catch ( CLoaderException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFound ) {
            // no sequence
        }
        else if ( exc.GetErrCode() == exc.eNoData ) {
            // sequence is known, but there is no type
            ret.sequence_found = true;
        }
        else {
            // problem
            throw;
        }
    }
    return ret;
}


int CDataLoader::GetSequenceState(const CSeq_id_Handle& idh)
{
    try {
        TTSE_LockSet locks = GetRecords(idh, eBioseqCore);
        ITERATE(TTSE_LockSet, it, locks) {
            CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
            if ( bs_info ) {
                return (*it)->GetBlobState();
            }
        }
        return CBioseq_Handle::fState_not_found|CBioseq_Handle::fState_no_data;
    }
    catch ( CBlobStateException& exc ) {
        return exc.GetBlobState();
    }
}


int CDataLoader::GetSequenceHash(const CSeq_id_Handle& idh)
{
    if ( SequenceExists(idh) ) {
        NCBI_THROW(CLoaderException, eNoData,
                   "CDataLoader::GetSequenceHash() sequence hash not set");
    }
    NCBI_THROW(CLoaderException, eNotFound,
               "CDataLoader::GetSequenceHash() sequence not found");
}


CDataLoader::SHashFound
CDataLoader::GetSequenceHashFound(const CSeq_id_Handle& idh)
{
    // default implementation based on GetSequenceHash() and GetIds()
    SHashFound ret;
    try {
        ret.hash = GetSequenceHash(idh);
        if ( !ret.hash ) {
            // hash = 0, we don't know what causes it:
            // absence of sequence, unknown hash, or the hash happens to be 0.
            ret.sequence_found = SequenceExists(idh);
        }
        else {
            ret.sequence_found = true;
            ret.hash_known = true;
        }
    }
    catch ( CLoaderException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFound ) {
            // no sequence found
        }
        else if ( exc.GetErrCode() == exc.eNoData ) {
            // sequence exists
            ret.sequence_found = true;
        }
        else {
            // problem
            throw;
        }
    }
    return ret;
}


void CDataLoader::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    TIds seq_ids;
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SAccVerFound data = GetAccVerFound(ids[i]);
        if ( data.sequence_found ) {
            ret[i] = data.acc_ver;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    TIds seq_ids;
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SGiFound data = GetGiFound(ids[i]);
        if ( data.sequence_found ) {
            ret[i] = data.gi;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        string label = GetLabel(ids[i]);
        if ( !label.empty() ) {
            ret[i] = label;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        
        int taxid = GetTaxId(ids[i]);
        if ( taxid != -1 ) {
            ret[i] = taxid;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                                     TSequenceLengths& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }

        TSeqPos len = GetSequenceLength(ids[i]);
        if ( len != kInvalidSeqPos ) {
            ret[i] = len;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                   TSequenceTypes& ret)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }

        STypeFound data = GetSequenceTypeFound(ids[i]);
        if ( data.sequence_found ) {
            ret[i] = data.type;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetSequenceStates(const TIds& ids, TLoaded& loaded,
                                   TSequenceStates& ret)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);

    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }

        int state = GetSequenceState(ids[i]);
        if ( state != kNotFound ) {
            ret[i] = state;
            loaded[i] = true;
        }
    }
}


void CDataLoader::GetSequenceHashes(const TIds& ids, TLoaded& loaded,
                                    TSequenceHashes& ret, THashKnown& known)
{
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }

        SHashFound data = GetSequenceHashFound(ids[i]);
        if ( data.sequence_found ) {
            ret[i] = data.hash;
            loaded[i] = true;
            known[i] = data.hash_known;
        }
    }
}


void CDataLoader::GetBlobs(TTSE_LockSets& tse_sets)
{
    NON_CONST_ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        tse_set->second = GetRecords(tse_set->first, eBlob);
    }
}


CDataLoader::EChoice
CDataLoader::DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const
{
    EChoice ret = eCore;
    ITERATE ( SRequestDetails::TAnnotSet, i, annots ) {
        ITERATE ( SRequestDetails::TAnnotTypesSet, j, i->second ) {
            EChoice cur = eCore;
            switch ( j->GetAnnotType() ) {
            case CSeq_annot::C_Data::e_Ftable:
                cur = eFeatures;
                break;
            case CSeq_annot::C_Data::e_Graph:
                cur = eGraph;
                break;
            case CSeq_annot::C_Data::e_Align:
                cur = eAlign;
                break;
            case CSeq_annot::C_Data::e_not_set:
                return eAnnot;
            default:
                break;
            }
            if ( cur != eCore && cur != ret ) {
                if ( ret != eCore ) return eAnnot;
                ret = cur;
            }
        }
    }
    return ret;
}


CDataLoader::EChoice
CDataLoader::DetailsToChoice(const SRequestDetails& details) const
{
    EChoice ret = DetailsToChoice(details.m_NeedAnnots);
    switch ( details.m_AnnotBlobType ) {
    case SRequestDetails::fAnnotBlobNone:
        // no annotations
        ret = eCore;
        break;
    case SRequestDetails::fAnnotBlobInternal:
        // no change
        break;
    case SRequestDetails::fAnnotBlobExternal:
        // shift from internal to external annotations
        _ASSERT(ret >= eFeatures && ret <= eAnnot);
        ret = EChoice(ret + eExtFeatures - eFeatures);
        _ASSERT(ret >= eExtFeatures && ret <= eExtAnnot);
        break;
    case SRequestDetails::fAnnotBlobOrphan:
        // all orphan annots
        ret = eOrphanAnnot;
        break;
    default:
        // all other cases -> eAll
        ret = eAll;
        break;
    }
    if ( !details.m_NeedSeqMap.Empty() || !details.m_NeedSeqData.Empty() ) {
        // include sequence
        if ( ret == eCore ) {
            ret = eSequence;
        }
        else if ( ret >= eFeatures && ret <= eAnnot ) {
            // only internal annot + sequence -> whole blob
            ret = eBlob;
        }
        else {
            // all blobs
            ret = eAll;
        }
    }
    return ret;
}


SRequestDetails CDataLoader::ChoiceToDetails(EChoice choice) const
{
    SRequestDetails details;
    CSeq_annot::C_Data::E_Choice type = CSeq_annot::C_Data::e_not_set;
    bool sequence = false;
    switch ( choice ) {
    case eAll:
        sequence = true;
        // from all blobs
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobAll;
        break;
    case eBlob:
    case eBioseq:
    case eBioseqCore:
        sequence = true;
        // internal only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobInternal;
        break;
    case eSequence:
        sequence = true;
        break;
    case eAnnot:
        // internal only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobInternal;
        break;
    case eGraph:
        type = CSeq_annot::C_Data::e_Graph;
        // internal only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobInternal;
        break;
    case eFeatures:
        type = CSeq_annot::C_Data::e_Ftable;
        // internal only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobInternal;
        break;
    case eAlign:
        type = CSeq_annot::C_Data::e_Align;
        // internal only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobInternal;
        break;
    case eExtAnnot:
        // external only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobExternal;
        break;
    case eExtGraph:
        type = CSeq_annot::C_Data::e_Graph;
        // external only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobExternal;
        break;
    case eExtFeatures:
        type = CSeq_annot::C_Data::e_Ftable;
        // external only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobExternal;
        break;
    case eExtAlign:
        type = CSeq_annot::C_Data::e_Align;
        // external only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobExternal;
        break;
    case eOrphanAnnot:
        // orphan annotations only
        details.m_AnnotBlobType = SRequestDetails::fAnnotBlobOrphan;
        break;
    default:
        break;
    }
    if ( sequence ) {
        details.m_NeedSeqMap = SRequestDetails::TRange::GetWhole();
        details.m_NeedSeqData = SRequestDetails::TRange::GetWhole();
    }
    if ( details.m_AnnotBlobType != SRequestDetails::fAnnotBlobNone ) {
        details.m_NeedAnnots[CAnnotName()].insert(SAnnotTypeSelector(type));
    }
    return details;
}


void CDataLoader::GetChunk(TChunk /*chunk_info*/)
{
    NCBI_THROW(CLoaderException, eNotImplemented,
               "CDataLoader::GetChunk() is not implemented in subclass");
}


void CDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CDataLoader::ResolveConflict(const CSeq_id_Handle& /*id*/,
                             const TTSE_LockSet& /*tse_set*/)
{
    return TTSE_Lock();
}


CDataLoader::TBlobId CDataLoader::GetBlobId(const CSeq_id_Handle& /*sih*/)
{
    return TBlobId();
}


CDataLoader::TBlobVersion CDataLoader::GetBlobVersion(const TBlobId& /*id*/)
{
    return 0;
}

CDataLoader::TEditSaver CDataLoader::GetEditSaver() const 
{
    return TEditSaver();
}

/////////////////////////////////////////////////////////////////////////////
// CBlobId

CBlobId::~CBlobId(void)
{
}

bool CBlobId::LessByTypeId(const CBlobId& id2) const
{
    return typeid(*this).before(typeid(id2));
}

bool CBlobId::operator==(const CBlobId& id) const
{
    return !(*this < id || id < *this);
}


END_SCOPE(objects)
END_NCBI_SCOPE
