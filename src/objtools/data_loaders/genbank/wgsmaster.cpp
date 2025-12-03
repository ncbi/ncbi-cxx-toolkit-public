/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: blob stream processor interface
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/error_codes.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_assigner.hpp> // for kTSE_Place_id
#include <objmgr/objmgr_exception.hpp>
#include <objects/general/general__.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Process

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const bool kAddMasterDescrToTSE = true;
static const char kMasterDescrMark[] = "WithMasterDescr";

/////////////////////////////////////////////////////////////////////////////
// Master descriptors are added in two modes, depending on kAddMasterDescrToTSE:
// false - master descriptors are added to each CBioseq in TSE.
//    The descriptors are filtered by existing descriptors on each CBioseq individually.
// true - master descriptors are added to TSE CBioseq_set only.
//    The descriptors are filtered by existing descriptors on the TSE CBioseq_set
//    and by existing descriptors on the first CBioseq within the CBioseq_set
//    (usually nucleotide).

BEGIN_LOCAL_NAMESPACE;


static
bool s_GoodLetters(CTempString s) {
    ITERATE ( CTempString, it, s ) {
        if ( !isalpha(*it & 0xff) ) {
            return false;
        }
    }
    return true;
}


static
bool s_GoodDigits(CTempString s) {
    bool have_non_zero = false;
    ITERATE ( CTempString, it, s ) {
        if ( *it != '0' ) {
            have_non_zero = true;
            if ( !isdigit(*it & 0xff) ) {
                return false;
            }
        }
    }
    return have_non_zero;
}




static const int kForceDescrMask = ((1<<CSeqdesc::e_User));

static const int kRefSeqOptionalDescrMask = ((1<<CSeqdesc::e_Pub) |
                                             (1<<CSeqdesc::e_Comment));

static const int kOptionalDescrMask = ((1<<CSeqdesc::e_Source) |
                                       (1<<CSeqdesc::e_Molinfo) |
                                       (1<<CSeqdesc::e_Create_date) |
                                       (1<<CSeqdesc::e_Update_date) |
                                       (1<<CSeqdesc::e_Genbank) |
                                       (1<<CSeqdesc::e_Embl));

static const int kGoodDescrMask = kForceDescrMask | kRefSeqOptionalDescrMask | kOptionalDescrMask;


static
bool s_IsGoodDescr(const CSeqdesc& desc, int mask, const TUserObjectTypesSet& uo_types)
{
    if ( desc.Which() == CSeqdesc::e_User ) {
        const CObject_id& type = desc.GetUser().GetType();
        if ( type.Which() == CObject_id::e_Str ) {
            string name = type.GetStr();
            // Only a few user object types are eligible to be taken from master
            if ( name == "DBLink" ||
                 name == "GenomeProjectsDB" ||
                 name == "StructuredComment" ||
                 name == "FeatureFetchPolicy" ||
                 name == "Unverified" ) {
                // For StructuredComment, extract the comment prefix and add to the name
                if (name == "StructuredComment") {
                    // This loop should normally stop on the first iteration...
                    ITERATE (CUser_object::TData, it, desc.GetUser().GetData()) {
                        if ((*it)->GetLabel().IsStr() &&
                            (*it)->GetLabel().GetStr() == "StructuredCommentPrefix") {
                            string data = ((*it)->GetData().IsStr() ?
                                           (string) (*it)->GetData().GetStr() :
                                           NStr::IntToString((*it)->GetData().GetInt()));
                            name += "|" + data;
                            break;
                        }
                    }
                }
                // Check if this user object type should be skipped because it already exists
                if (uo_types.count(name) == 0)
                    return true;
            }
        }
    }
    else if ( (1 << desc.Which()) & mask ) {
        return true;
    }
    return false;
}


bool HasWGSMasterMark(const CTSE_Info& info)
{
    if ( info.HasNoSeq_entry() ) {
        return false;
    }
    TUserObjectTypesSet uo_types;
    info.x_GetBaseInfo().x_AddExistingUserObjectTypes(uo_types);
    return uo_types.find(kMasterDescrMark) != uo_types.end();
}


CSeq_id_Handle GetWGSMasterSeq_id(const CSeq_id_Handle& idh)
{
    CSeq_id_Handle master_idh;

    switch ( idh.Which() ) { // shortcut to exclude all non Textseq-id types
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Pdb:
        return master_idh;
    default:
        break;
    }

    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id || !text_id->IsSetAccession() ) {
        return master_idh;
    }

    CTempString acc = text_id->GetAccession();

    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    bool is_cage_ddbj = false;
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_mga: // 2019/02/08 : For now, it's just CAGE DDBJ
        is_cage_ddbj = true;
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
    case CSeq_id::eAcc_targeted:
        break;
    default:
        return master_idh;
    }

    SIZE_TYPE digits_pos = acc.find_first_of("0123456789");
    bool have_nz = NStr::StartsWith(acc, "NZ_");
    SIZE_TYPE letters_pos = (have_nz ? 3 : 0);

    // First check the prefix and suffix lengths.
    // WGS/TSA/TLS prefixes have 4 or 6 letters; CAGE DDBJ prefixes have 5 letters
    // WGS/TSA/TLS suffixes have 8-10 or 9-11 digits (including 2-digit version);
    // CAGE DDBJ suffixes have 7 digits 
    SIZE_TYPE min_digits = 0;
    SIZE_TYPE max_digits = 0;

    if (is_cage_ddbj) {
        if (digits_pos != 5)
            return master_idh;
        min_digits = 7;
        max_digits = 7;
    } else {
        if (digits_pos != letters_pos+4 && digits_pos != letters_pos+6)
            return master_idh;
        min_digits = ((digits_pos == letters_pos+4) ? 8 : 9);
        max_digits = min_digits + 2;
    }

    SIZE_TYPE digits_count = acc.size() - digits_pos;
    if (digits_count < min_digits || digits_count > max_digits)
        return master_idh;

    // Check that prefix and suffix actually consist of letters and digits respectively.
    if ( !s_GoodLetters(acc.substr(letters_pos, digits_pos-letters_pos)) ) {
        return master_idh;
    }
    if ( !s_GoodDigits(acc.substr(digits_pos)) ) {
        return master_idh;
    }

    // Exclude master accessions
    // Non-CAGE-DDBJ master accessions may also contain a 2-digit version 
    int version = 0;
    Uint8 row_id = 0;
    if (is_cage_ddbj) {
        version = 1;
        row_id = NStr::StringToNumeric<Uint8>(acc.substr(digits_pos));
    } else {
        version = NStr::StringToNumeric<int>(acc.substr(digits_pos, 2));
        row_id = NStr::StringToNumeric<Uint8>(acc.substr(digits_pos+2));
    }
    if ( !version || !row_id ) {
        return master_idh;
    }

    CSeq_id master_id;
    master_id.Assign(*id);
    CTextseq_id* master_text_id =
        const_cast<CTextseq_id*>(master_id.GetTextseq_Id());
    string master_acc = acc.substr(0, digits_pos);
    master_acc.resize(acc.size(), '0');
    master_text_id->Reset();
    master_text_id->SetAccession(master_acc);
    master_text_id->SetVersion(version);
    master_idh = CSeq_id_Handle::GetHandle(master_id);
    return master_idh;
}


enum EDescrType {
    eDescrTypeDefault,
    eDescrTypeRefSeq
};

inline EDescrType GetDescrType(const CSeq_id_Handle& master_seq_idh)
{
    return master_seq_idh.Which() == CSeq_id::e_Other? eDescrTypeRefSeq: eDescrTypeDefault;
}


int GetForceDescrMask(EDescrType type)
{
    int force_mask = kForceDescrMask;
    if ( type != eDescrTypeRefSeq ) {
        force_mask |= kRefSeqOptionalDescrMask;
    }
    return force_mask;
}


int GetOptionalDescrMask(EDescrType type)
{
    int optional_mask = kOptionalDescrMask;
    if ( type == eDescrTypeRefSeq ) {
        optional_mask |= kRefSeqOptionalDescrMask;
    }
    return optional_mask;
}


void AddMasterDescr(CBioseq_Info& seq,
                    const CSeq_descr& src,
                    EDescrType type)
{
    int existing_mask = 0;
    CSeq_descr::Tdata& dst = seq.x_SetDescr().Set();
    ITERATE ( CSeq_descr::Tdata, it, dst ) {
        const CSeqdesc& desc = **it;
        existing_mask |= 1 << desc.Which();
    }
    int force_mask = GetForceDescrMask(type);
    int optional_mask = GetOptionalDescrMask(type);
    int mask = force_mask | (optional_mask & ~existing_mask);
    TUserObjectTypesSet uo_types;
    seq.x_AddExistingUserObjectTypes(uo_types);
    if ( uo_types.find(kMasterDescrMark) != uo_types.end() ) {
        // master descriptors are already attached
        return;
    }
    ITERATE ( CSeq_descr::Tdata, it, src.Get() ) {
        if ( s_IsGoodDescr(**it, mask, uo_types) ) {
            dst.push_back(*it);
        }
    }
}


bool s_HasMasterId(const CBioseq_Info& seq, const CSeq_id_Handle& master_idh)
{
    if ( master_idh ) {
        const CBioseq_Info::TId& ids = seq.GetId();
        ITERATE ( CBioseq_Info::TId, it, ids ) {
            if ( GetWGSMasterSeq_id(*it) == master_idh ) {
                return true;
            }
        }
    }
    return false;
}


CConstRef<CSeq_descr> GetWGSMasterDescr(CDataLoader* loader,
                                        const CSeq_id_Handle& master_idh)
{
    CDataLoader::TTSE_LockSet locks =
        loader->GetRecordsNoBlobState(master_idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        if ( CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(master_idh) ) {
            if ( bs_info->IsSetDescr() ) {
                return ConstRef(&bs_info->GetDescr());
            }
            break;
        }
    }
    return null;
}


class CWGSBioseqUpdater_Base : public CBioseqUpdater,
                               public CWGSMasterSupport
{
public:
    CWGSBioseqUpdater_Base(const CSeq_id_Handle& master_idh)
        : m_MasterId(master_idh)
        {
        }
    
    const CSeq_id_Handle& GetMasterId() const {
        return m_MasterId;
    }
    bool HasMasterId(const CBioseq_Info& seq) const {
        return s_HasMasterId(seq, GetMasterId());
    }
    
private:
    CSeq_id_Handle m_MasterId;
};


class CWGSBioseqUpdaterChunk : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterChunk(const CSeq_id_Handle& master_idh)
        : CWGSBioseqUpdater_Base(master_idh)
        {
        }

    virtual void Update(CBioseq_Info& seq) override
        {
            if ( HasMasterId(seq) ) {
                // register master descr chunk
                //ERR_POST("Adding descr chunk id to "<<seq.GetId().front());
                seq.x_AddDescrChunkId(kGoodDescrMask, kMasterWGS_ChunkId, seq.eNeedOtherDescr);
            }
        }
};


class CWGSBioseqUpdaterDescr : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterDescr(const CSeq_id_Handle& master_idh,
                           CConstRef<CSeq_descr> descr)
        : CWGSBioseqUpdater_Base(master_idh),
          m_Descr(descr)
        {
        }
    
    virtual void Update(CBioseq_Info& seq) override
        {
            if ( m_Descr &&
                 seq.x_NeedUpdate(seq.fNeedUpdate_descr) &&
                 HasMasterId(seq) ) {
                AddMasterDescr(seq, *m_Descr, GetDescrType(GetMasterId()));
            }
        }
    
private:
    CConstRef<CSeq_descr> m_Descr;
};


class CWGSMasterInfo : public CObject
{
public:
    CWGSMasterInfo(const CSeq_id_Handle& master_idh)
        : m_MasterId(master_idh),
          m_AddToTSE(false)
        {
        }

    CSeq_id_Handle m_MasterId;
    CConstRef<CSeq_descr> m_OriginalMasterDescr;
    bool m_AddToTSE;
};


class CWGSMasterChunkInfo : public CTSE_Chunk_Info
{
public:
    CWGSMasterChunkInfo(const CSeq_id_Handle& master_idh)
        : CTSE_Chunk_Info(kMasterWGS_ChunkId),
          m_MasterInfo(new CWGSMasterInfo(master_idh))
        {
        }

    CRef<CWGSMasterInfo> m_MasterInfo;
};


class CWGSMasterDescrSetter : public CTSEChunkLoadListener
{
public: 
    typedef CTSE_Chunk_Info::TChunkId TChunkId;
    
    CWGSMasterDescrSetter(CRef<CWGSMasterChunkInfo> master_chunk_info,
                          const CBioseq_set_Info& bset,
                          CTSE_Split_Info& split_info,
                          int mask)
        : m_MasterInfo(master_chunk_info->m_MasterInfo),
          m_BioseqSet(&bset)
        {
            AddChunkToWait(master_chunk_info->GetChunkId(), split_info, mask);
        }

    void AddChunkToWait(TChunkId chunk_id, CTSE_Split_Info& split_info, int mask)
        {
            m_ChunksToWait.insert(chunk_id);
            if ( mask ) {
                split_info.GetChunk(chunk_id).x_AddDescInfo(mask, kTSE_Place_id);
            }
        }

    void AddChunksToWait(const CBioseq_Base_Info& info, CTSE_Split_Info& split_info, int mask)
        {
            for ( auto chunk_id : info.x_GetDescrChunkIds() ) {
                AddChunkToWait(chunk_id, split_info, mask);
            }
        }

    void RegisterCallbacks(CTSE_Split_Info& split_info)
        {
            vector<TChunkId> ids(m_ChunksToWait.begin(), m_ChunksToWait.end());
            for ( auto chunk_id : ids ) {
                //ERR_POST("CWGSMasterDescrSetter: waiting for "<<chunk_id<<" to be loaded");
                split_info.GetChunk(chunk_id).SetLoadListener(Ref(this));
            }
        }

    static int x_GetActualExistingDescrMask(const CBioseq_Base_Info& info)
        {
            //return info.x_GetExistingDescrMask();
            int mask = 0;
            if ( info.x_IsSetDescr() ) {
                // collect already set descr bits
                for ( auto& i : info.x_GetDescr().Get() ) {
                    mask |= 1 << i->Which();
                }
            }
            return mask;
        }

    virtual void Loaded(CTSE_Chunk_Info& chunk) override
        {
            //ERR_POST("CWGSMasterDescrSetter::Loaded("<<chunk.GetChunkId()<<")");
            m_ChunksToWait.erase(chunk.GetChunkId());
            if ( !m_ChunksToWait.empty() ) {
                // still needs more chunks to be loaded
                return;
            }
            // all chunks are loaded, filter and add descriptors
            //ERR_POST("CWGSMasterDescrSetter: setting descriptors");
            if ( !m_MasterInfo->m_OriginalMasterDescr ) {
                // no master descriptors found
                return;
            }
            // collect filters
            int mask = kGoodDescrMask;
            TUserObjectTypesSet existing_uo_types;
            int force_descr = GetForceDescrMask(GetDescrType(m_MasterInfo->m_MasterId));
            mask &= ~x_GetActualExistingDescrMask(*m_BioseqSet) | force_descr;
            m_BioseqSet->x_AddExistingUserObjectTypes(existing_uo_types);
            if ( existing_uo_types.find(kMasterDescrMark) != existing_uo_types.end() ) {
                // master descriptors are already attached
                return;
            }
            if ( auto first_entry = m_BioseqSet->GetFirstEntry() ) {
                mask &= ~x_GetActualExistingDescrMask(first_entry->x_GetBaseInfo()) | force_descr;
                first_entry->x_GetBaseInfo().x_AddExistingUserObjectTypes(existing_uo_types);
            }
            if ( existing_uo_types.find(kMasterDescrMark) != existing_uo_types.end() ) {
                // master descriptors are already attached
                return;
            }
            CRef<CSeq_descr> descr;
            for ( auto& ref : m_MasterInfo->m_OriginalMasterDescr->Get() ) {
                if ( s_IsGoodDescr(*ref, mask, existing_uo_types) ) {
                    if ( !descr ) {
                        descr = new CSeq_descr;
                    }
                    descr->Set().push_back(ref);
                }
            }
            chunk.x_LoadDescr(CTSE_Chunk_Info::TPlace(), *descr);
        }
    
private:
    CRef<CWGSMasterInfo> m_MasterInfo;
    CConstRef<CBioseq_set_Info> m_BioseqSet;
    set<TChunkId> m_ChunksToWait;
};


END_LOCAL_NAMESPACE;


void CWGSMasterSupport::LoadWGSMaster(CDataLoader* loader,
                                      CRef<CTSE_Chunk_Info> chunk)
{
    CWGSMasterInfo& master_info = *dynamic_cast<CWGSMasterChunkInfo&>(*chunk).m_MasterInfo;
    //ERR_POST("LoadWGSMaster: loading master descr "<<master_info.m_MasterId);
    if ( auto descr0 = GetWGSMasterDescr(loader, master_info.m_MasterId) ) {
        //ERR_POST("LoadWGSMaster: loaded master descr "<<master_info.m_MasterId);
        // save loaded descriptors for future extra filtering
        master_info.m_OriginalMasterDescr = descr0;
        if ( master_info.m_AddToTSE ) {
            // the descriptors will be added by chunk load listener CWGSMasterDescrSetter
            //ERR_POST("LoadWGSMaster: waiting for all descr chunks for master id "<<master_info.m_MasterId);
        }
        else {
            // add descriptors to each bioseq, already loaded or future
            //ERR_POST("LoadWGSMaster: individual seqs with master id "<<master_info.m_MasterId);
            CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterDescr(master_info.m_MasterId, descr0));
            const_cast<CTSE_Split_Info&>(chunk->GetSplitInfo()).x_SetBioseqUpdater(upd);
        }
    }
    chunk->SetLoaded();
}


void CWGSMasterSupport::AddWGSMaster(CTSE_LoadLock& lock)
{
    CTSE_Info::TSeqIds ids;
    lock->GetBioseqsIds(ids);
    ITERATE ( CTSE_Info::TSeqIds, it, ids ) {
        if ( CSeq_id_Handle master_id = GetWGSMasterSeq_id(*it) ) {
            // first check if WGS master descriptors are added already (WithMasterDescr mark)
            if ( HasWGSMasterMark(*lock) ) {
                return;
            }
            
            auto& split_info = lock->GetSplitInfo();
            int mask = kGoodDescrMask;
            
            // add chunk with master sequence
            CRef<CWGSMasterChunkInfo> chunk(new CWGSMasterChunkInfo(master_id));
            split_info.AddChunk(*chunk);
            
            if ( kAddMasterDescrToTSE && lock->IsSet() ) {
                //ERR_POST("AddWGSMaster: nuc-prot set with master id "<<master_id);
                // master descriptors are added to the top-level Bioseq-set only
                // but they need to be filtered by the first sequence in the set
                chunk->m_MasterInfo->m_AddToTSE = true;

                // register master chunk for descriptors on top-level object
                int force_descr = GetForceDescrMask(GetDescrType(master_id));
                mask &= ~lock->x_GetBaseInfo().x_GetExistingDescrMask() | force_descr;
                
                // collect all chunks that needs to be loaded
                const CBioseq_set_Info& bset = lock->GetSet();
                CRef<CWGSMasterDescrSetter> setter(new CWGSMasterDescrSetter(chunk, bset, split_info, mask));
                // first exclude existing descr types except forced ones (User, Pub, Comment)
                setter->AddChunksToWait(lock->x_GetBaseInfo(), split_info, 0);
                if ( auto first_entry = bset.GetFirstEntry() ) {
                    // first sequence is loaded so simply apply the filter
                    setter->AddChunksToWait(first_entry->x_GetBaseInfo(), split_info, mask);
                }
                else if ( !bset.x_GetBioseqChunkIds().empty() ) {
                    // first sequence is split out, we need to update filter when the sequence is loaded
                    // request loading of the chunk with the first sequence for applying the filter
                    auto& seq_chunk = lock->GetSplitInfo().GetChunk(bset.x_GetBioseqChunkIds().front());
                    setter->AddChunkToWait(seq_chunk.GetChunkId(), split_info, mask);
                }
                setter->RegisterCallbacks(lock->GetSplitInfo()); // wait for all chunks to be loaded
            }
            else {
                //ERR_POST("AddWGSMaster: individual seqs with master id "<<master_id);
                // add descr chunk to each bioseq, already loaded or future
                CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterChunk(master_id));
                lock->SetBioseqUpdater(upd);
            }
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
