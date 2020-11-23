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
#include <objmgr/objmgr_exception.hpp>
#include <objects/general/general__.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Process

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const bool kAddMasterDescrToTSE = true;


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




static const int kForceDescrMask = ((1<<CSeqdesc::e_Pub) |
                                    (1<<CSeqdesc::e_Comment) |
                                    (1<<CSeqdesc::e_User));

static const int kOptionalDescrMask = ((1<<CSeqdesc::e_Source) |
                                       (1<<CSeqdesc::e_Molinfo) |
                                       (1<<CSeqdesc::e_Create_date) |
                                       (1<<CSeqdesc::e_Update_date) |
                                       (1<<CSeqdesc::e_Genbank) |
                                       (1<<CSeqdesc::e_Embl));

static const int kGoodDescrMask = kForceDescrMask | kOptionalDescrMask;


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




END_LOCAL_NAMESPACE;


CSeq_id_Handle CWGSMasterSupport::GetWGSMasterSeq_id(const CSeq_id_Handle& idh)
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
        break;
    default:
        return master_idh;
    }

    SIZE_TYPE digits_pos = acc.find_first_of("0123456789");
    bool have_nz = NStr::StartsWith(acc, "NZ_");
    SIZE_TYPE letters_pos = (have_nz ? 3 : 0);

    // First check the prefix and suffix lengths.
    // WGS/TSA/TLS prefixes have 4 or 6 letters; CAGE DDBJ prefixes have 5 letters
    // WGS/TSA/TLS suffixes have 6-8 or 7-9 digits; CAGE DDBJ suffixes have 7 digits 
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
        min_digits = ((digits_pos == letters_pos+4) ? 6 : 7);
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


void CWGSMasterSupport::AddMasterDescr(CBioseq_Info& seq, const CSeq_descr& src)
{
    int existing_mask = 0;
    CSeq_descr::Tdata& dst = seq.x_SetDescr().Set();
    ITERATE ( CSeq_descr::Tdata, it, dst ) {
        const CSeqdesc& desc = **it;
        existing_mask |= 1 << desc.Which();
    }
    ITERATE ( CSeq_descr::Tdata, it, src.Get() ) {
        int mask = 1 << (*it)->Which();
        if ( mask & kOptionalDescrMask ) {
            if ( mask & existing_mask ) {
                continue;
            }
        }
        else if ( !(mask & kForceDescrMask) ) {
            continue;
        }
        dst.push_back(*it);
    }
}


bool CWGSMasterSupport::HasMasterId(const CBioseq_Info& seq, const CSeq_id_Handle& master_idh)
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


CRef<CSeq_descr> CWGSMasterSupport::GetWGSMasterDescr(CDataLoader* loader,
                                                      const CSeq_id_Handle& master_idh,
                                                      int mask, TUserObjectTypesSet& uo_types)
{
    CRef<CSeq_descr> ret;
    CDataLoader::TTSE_LockSet locks =
        loader->GetRecordsNoBlobState(master_idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        CConstRef<CBioseq_Info> bs_info =
            (*it)->FindMatchingBioseq(master_idh);
        if ( !bs_info ) {
            continue;
        }
        if ( bs_info->IsSetDescr() ) {
            const CSeq_descr::Tdata& descr = bs_info->GetDescr().Get();
            ITERATE ( CSeq_descr::Tdata, it, descr ) {
                if ( s_IsGoodDescr(**it, mask, uo_types) ) {
                    if ( !ret ) {
                        ret = new CSeq_descr;
                    }
                    ret->Set().push_back(*it);
                }
            }
        }
        break;
    }
    return ret;
}


void CWGSMasterSupport::LoadWGSMaster(CDataLoader* loader,
                                      CRef<CTSE_Chunk_Info> chunk)
{
    CWGSMasterChunkInfo& chunk_info =
        dynamic_cast<CWGSMasterChunkInfo&>(*chunk);
    CSeq_id_Handle id = chunk_info.m_MasterId;
    int mask = chunk_info.m_DescrMask;
    CRef<CSeq_descr> descr =
        GetWGSMasterDescr(loader, id, mask, chunk_info.m_UserObjectTypes);
    if ( descr ) {
        if ( kAddMasterDescrToTSE ) {
            chunk->x_LoadDescr(CTSE_Chunk_Info::TPlace(), *descr);
        }
        else {
            CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterDescr(id, descr));
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
        if ( CSeq_id_Handle id = GetWGSMasterSeq_id(*it) ) {
            int mask = kGoodDescrMask;
            TUserObjectTypesSet existing_uo_types;
            if ( kAddMasterDescrToTSE ) {
                // exclude existing descr types except forced ones (User, Pub, Comment)
                mask &= ~lock->x_GetBaseInfo().x_GetExistingDescrMask() | kForceDescrMask;
                lock->x_GetBaseInfo().x_AddExistingUserObjectTypes(existing_uo_types);
                if ( lock->IsSet() ) {
                    if ( auto first_entry = lock->GetSet().GetFirstEntry() ) {
                        mask &= ~first_entry->x_GetBaseInfo().x_GetExistingDescrMask() | kForceDescrMask;
                        first_entry->x_GetBaseInfo().x_AddExistingUserObjectTypes(existing_uo_types);
                    }
                }
            }
            CRef<CTSE_Chunk_Info> chunk(new CWGSMasterChunkInfo(id, mask, existing_uo_types));
            lock->GetSplitInfo().AddChunk(*chunk);
            if ( kAddMasterDescrToTSE ) {
                chunk->x_AddDescInfo(mask, 0);
            }
            else {
                CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterChunk(id));
                lock->SetBioseqUpdater(upd);
            }
            break;
        }
    }
}


CWGSBioseqUpdater_Base::CWGSBioseqUpdater_Base(const CSeq_id_Handle& master_idh)
    : m_MasterId(master_idh)
{
}


CWGSBioseqUpdater_Base::~CWGSBioseqUpdater_Base()
{
}


CWGSMasterChunkInfo::CWGSMasterChunkInfo(const CSeq_id_Handle& master_idh,
                                         int mask, TUserObjectTypesSet& uo_types)
    : CTSE_Chunk_Info(kMasterWGS_ChunkId),
      m_MasterId(master_idh),
      m_DescrMask(mask),
      m_UserObjectTypes(move(uo_types))
{
}


CWGSMasterChunkInfo::~CWGSMasterChunkInfo()
{
}


CWGSBioseqUpdaterChunk::CWGSBioseqUpdaterChunk(const CSeq_id_Handle& master_idh)
    : CWGSBioseqUpdater_Base(master_idh)
{
}


CWGSBioseqUpdaterChunk::~CWGSBioseqUpdaterChunk()
{
}


void CWGSBioseqUpdaterChunk::Update(CBioseq_Info& seq)
{
    if ( HasMasterId(seq) ) {
        // register master descr chunk
        seq.x_AddDescrChunkId(kGoodDescrMask, kMasterWGS_ChunkId);
    }
}


CWGSBioseqUpdaterDescr::CWGSBioseqUpdaterDescr(const CSeq_id_Handle& master_idh,
                                               CRef<CSeq_descr> descr)
    : CWGSBioseqUpdater_Base(master_idh),
      m_Descr(descr)
{
}


CWGSBioseqUpdaterDescr::~CWGSBioseqUpdaterDescr()
{
}


void CWGSBioseqUpdaterDescr::Update(CBioseq_Info& seq)
{
    if ( m_Descr &&
         seq.x_NeedUpdate(seq.fNeedUpdate_descr) &&
         HasMasterId(seq) ) {
        AddMasterDescr(seq, *m_Descr);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
