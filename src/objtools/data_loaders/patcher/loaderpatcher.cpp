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
*  Author: Maxim Didenko
*
*  File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/plugin_manager_impl.hpp>

#include <objtools/data_loaders/patcher/loaderpatcher.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_name.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/annot_finder.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <objects/seqedit/SeqEdit_Cmd.hpp>
#include <objects/seqedit/SeqEdit_Id.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddId.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveId.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetIds.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ChangeSeqAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSetAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ChangeSetAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSeqAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_SetDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddDesc.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveDesc.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSeq.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSet.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ReplaceAnnot.hpp>


//=======================================================================
// CDataLoaderPatcher Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataLoaderPatcher::TRegisterLoaderInfo 
CDataLoaderPatcher::RegisterInObjectManager(
    CObjectManager&      om,
    CRef<CDataLoader>    data_loader,
    CRef<IEditsDBEngine> db_engine,
    CRef<IEditSaver>     saver,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SParam param(data_loader, db_engine, saver);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}



string CDataLoaderPatcher::GetLoaderNameFromArgs(const SParam& param)
{
    return "PATCHER_" + param.m_DataLoader->GetName();
}


CDataLoaderPatcher::CDataLoaderPatcher(const string& loader_name,
                                       const SParam& param)
    : CDataLoader (loader_name),
      m_DataLoader(param.m_DataLoader),
      m_DBEngine  (param.m_DBEngine),
      m_EditSaver (param.m_EditSaver)
{
}

CDataLoaderPatcher::~CDataLoaderPatcher(void)
{
}

CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetRecords(const CSeq_id_Handle& idh,
                               EChoice choice)
{
    TTSE_LockSet locks;
    string sblobid;
    bool found = m_DBEngine->FindSeqId(idh, sblobid);
    if (found) {
        if (!sblobid.empty()) {
            TBlobId blobid = m_DataLoader->GetBlobIdFromString(sblobid);
            TTSE_Lock lock = m_DataLoader->GetBlobById(blobid);
            locks.insert(lock);           
        }
    } else {
        locks = m_DataLoader->GetRecords(idh, choice);
    }
    TTSE_LockSet nlocks;
    x_PatchLockSet(locks, nlocks);
    return nlocks;
}

CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetDetailedRecords(const CSeq_id_Handle& idh,
                                       const SRequestDetails& details)
{
    TTSE_LockSet locks;
    string sblobid;
    bool found = m_DBEngine->FindSeqId(idh, sblobid);
    if (found) {
        if (!sblobid.empty()) {
            TBlobId blobid = m_DataLoader->GetBlobIdFromString(sblobid);
            TTSE_Lock lock = m_DataLoader->GetBlobById(blobid);
            locks.insert(lock);
        }
    } else {
        locks = m_DataLoader->GetDetailedRecords(idh, details);
    }
    TTSE_LockSet nlocks;
    x_PatchLockSet(locks, nlocks);
    return nlocks;
}


CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetExternalRecords(const CBioseq_Info& bioseq)
{
    TTSE_LockSet locks = m_DataLoader->GetExternalRecords(bioseq);
    TTSE_LockSet nlocks;
    x_PatchLockSet(locks, nlocks);
    return nlocks;
}


CDataLoader::TBlobId 
CDataLoaderPatcher::GetBlobId(const CSeq_id_Handle& idh)
{
    string sblobid;
    bool found = m_DBEngine->FindSeqId(idh, sblobid);
    if (found) {
        if (!sblobid.empty()) {
            return m_DataLoader->GetBlobIdFromString(sblobid);
        } else {
            return TBlobId();
        }
    } else {
        return m_DataLoader->GetBlobId(idh);
    }        
}

CDataLoader::TBlobId 
CDataLoaderPatcher::GetBlobIdFromString(const string& str) const
{
    return m_DataLoader->GetBlobIdFromString(str);
}

CDataLoader::TBlobVersion 
CDataLoaderPatcher::GetBlobVersion(const TBlobId& id)
{
    return m_DataLoader->GetBlobVersion(id);
}

bool CDataLoaderPatcher::CanGetBlobById(void) const
{
    return m_DataLoader->CanGetBlobById();
}
CDataLoader::TTSE_Lock 
CDataLoaderPatcher::GetBlobById(const TBlobId& blob_id)
{
    return x_PatchLock(m_DataLoader->GetBlobById(blob_id));
}

void CDataLoaderPatcher::GetChunk(TChunk chunk_info)
{
    m_DataLoader->GetChunk(chunk_info);
}
void CDataLoaderPatcher::GetChunks(const TChunkSet& chunks)
{
    m_DataLoader->GetChunks(chunks);
}

/*
SRequestDetails CDataLoaderPatcher::ChoiceToDetails(EChoice choice) const
{
    return m_DataLoader->ChoiceToDetails(choice);
}
CDataLoaderPatcher::EChoice 
CDataLoaderPatcher::DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const
{
    return m_DataLoader->DetailsToChoice(annots);
}

CDataLoaderPatcher::EChoice 
CDataLoaderPatcher::DetailsToChoice(const SRequestDetails& details) const
{
    return m_DataLoader->DetailsToChoice(details);
}
*/
 
CDataLoaderPatcher::TTSE_Lock
CDataLoaderPatcher::x_PatchLock(const TTSE_Lock& lock)
{
    const TBlobId& blob_id = (*lock).GetBlobId();
    CTSE_LoadLock nlock = GetDataSource()->GetTSE_LoadLock(blob_id);

    if (!nlock.IsLoaded()) {
        if ( x_IsPatchNeeded(*lock) ) {
            CRef<CSeq_entry> entry;
            CConstRef<CSeq_entry> orig_entry = lock->GetSeq_entrySkeleton();
            if( orig_entry ) {
                entry.Reset(new CSeq_entry);
                entry->Assign(*orig_entry);
            }
            nlock->Assign(lock, entry);//, CRef<ITSE_Assigner>());
            x_ApplyPatches(*nlock);
        } else 
            nlock->Assign(lock);

        nlock.SetLoaded();
    }
    return TTSE_Lock(nlock);
}   
void CDataLoaderPatcher::x_PatchLockSet(const TTSE_LockSet& orig_locks, 
                                      TTSE_LockSet& new_locks)
{
    ITERATE(TTSE_LockSet, it, orig_locks) {
        const TTSE_Lock& lock = *it;
        new_locks.insert(x_PatchLock(lock)); 
    }
}   

CDataLoader::TEditSaver CDataLoaderPatcher::GetEditSaver() const
{
    return m_EditSaver;
}

bool CDataLoaderPatcher::x_IsPatchNeeded(const CTSE_Info& tse)
{
    string blob_id = tse.GetBlobId().ToString();
    return m_DBEngine->HasBlob(blob_id);
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddId& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveId& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetIds& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ChangeSeqAttr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSeqAttr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ChangeSetAttr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSetAttr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddDescr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_SetDescr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetDescr& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddDesc& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveDesc& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSeq& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSet& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSeqEntry& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSeqEntry& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveSeqEntry& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachAnnot& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveAnnot& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddAnnot& cmd);
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ReplaceAnnot& cmd);

void CDataLoaderPatcher::x_ApplyPatches(CTSE_Info& tse)
{
    IEditsDBEngine::TCommands cmds;
    string blob_id = tse.GetBlobId().ToString();
    m_DBEngine->GetCommands(blob_id, cmds);
    
    ITERATE(IEditsDBEngine::TCommands, it, cmds) {
        const CSeqEdit_Cmd& cmd = **it;
        switch(cmd.Which()) {
        case CSeqEdit_Cmd::e_Add_id:
            x_ApplyCmd(tse,cmd.GetAdd_id());
            break;
        case CSeqEdit_Cmd::e_Remove_id:
            x_ApplyCmd(tse,cmd.GetRemove_id());
            break;
        case CSeqEdit_Cmd::e_Reset_ids:
            x_ApplyCmd(tse,cmd.GetReset_ids());
            break;
        case CSeqEdit_Cmd::e_Change_seqattr:
            x_ApplyCmd(tse,cmd.GetChange_seqattr());
            break;
        case CSeqEdit_Cmd::e_Reset_seqattr:
            x_ApplyCmd(tse,cmd.GetReset_seqattr());
            break;
        case CSeqEdit_Cmd::e_Change_setattr:
            x_ApplyCmd(tse,cmd.GetChange_setattr());
            break;
        case CSeqEdit_Cmd::e_Reset_setattr:
            x_ApplyCmd(tse,cmd.GetReset_setattr());
            break;
        case CSeqEdit_Cmd::e_Add_descr:
            x_ApplyCmd(tse,cmd.GetAdd_descr());
            break;
        case CSeqEdit_Cmd::e_Set_descr:
            x_ApplyCmd(tse,cmd.GetSet_descr());
            break;
        case CSeqEdit_Cmd::e_Reset_descr:
            x_ApplyCmd(tse,cmd.GetReset_descr());
            break;
        case CSeqEdit_Cmd::e_Add_desc:
            x_ApplyCmd(tse,cmd.GetAdd_desc());
            break;
        case CSeqEdit_Cmd::e_Remove_desc:
            x_ApplyCmd(tse,cmd.GetRemove_desc());
            break;
        case CSeqEdit_Cmd::e_Attach_seq:
            x_ApplyCmd(tse,cmd.GetAttach_seq());
            break;
        case CSeqEdit_Cmd::e_Attach_set:
            x_ApplyCmd(tse,cmd.GetAttach_set());
            break;
        case CSeqEdit_Cmd::e_Reset_seqentry:
            x_ApplyCmd(tse,cmd.GetReset_seqentry());
            break;
        case CSeqEdit_Cmd::e_Attach_seqentry:
            x_ApplyCmd(tse,cmd.GetAttach_seqentry());
            break;
        case CSeqEdit_Cmd::e_Remove_seqentry:
            x_ApplyCmd(tse,cmd.GetRemove_seqentry());
            break;
        case CSeqEdit_Cmd::e_Attach_annot:
            x_ApplyCmd(tse,cmd.GetAttach_annot());
            break;
        case CSeqEdit_Cmd::e_Remove_annot:
            x_ApplyCmd(tse,cmd.GetRemove_annot());
            break;
        case CSeqEdit_Cmd::e_Add_annot:
            x_ApplyCmd(tse,cmd.GetAdd_annot());
            break;
        case CSeqEdit_Cmd::e_Replace_annot:
            x_ApplyCmd(tse,cmd.GetReplace_annot());
            break;
        case CSeqEdit_Cmd::e_not_set:
            NCBI_THROW(CLoaderException, eOtherError,
                       "SeqEdit_Cmd is not set");
        }
    }
}

static inline CBioObjectId s_Convert(const CSeqEdit_Id& id)
{
    switch (id.Which()) {
    case CSeqEdit_Id::e_Bioseq_id:
        return CBioObjectId(CSeq_id_Handle::GetHandle(id.GetBioseq_id()));
    case CSeqEdit_Id::e_Bioseqset_id:
        return CBioObjectId(CBioObjectId::eSetId, id.GetBioseqset_id());
    case CSeqEdit_Id::e_Unique_num:
        return CBioObjectId(CBioObjectId::eUniqNumber, id.GetUnique_num());
    default:
        NCBI_THROW(CLoaderException, eOtherError,
               "SeqEdit_Id is not set");
    }   
}

CBioseq_Info& GetBioseq(CTSE_Info& tse, const CBioObjectId& id)
{
    CTSE_Info_Object* to = tse.x_FindBioObject(id);
    CBioseq_Info* seq = dynamic_cast<CBioseq_Info*>(to);
    if (seq)
        return *seq;
    NCBI_THROW(CLoaderException, eOtherError,
               "BioObjectId does not point to Bioseq");
        
}

CBioseq_set_Info& GetBioseq_set(CTSE_Info& tse, const CBioObjectId& id)
{
    CTSE_Info_Object* to = tse.x_FindBioObject(id);
    CBioseq_set_Info* bset = dynamic_cast<CBioseq_set_Info*>(to);
    if (bset)
        return *bset;
    NCBI_THROW(CLoaderException, eOtherError,
               "BioObjectId does not point to Bioseq_set");
        
}

CBioseq_Base_Info& GetBase(CTSE_Info& tse, const CBioObjectId& id)
{
    CTSE_Info_Object* to = tse.x_FindBioObject(id);
    CBioseq_Base_Info* base = dynamic_cast<CBioseq_Base_Info*>(to);
    if (base)
        return *base;
    NCBI_THROW(CLoaderException, eOtherError,
               "BioObjectId does not point to Bioseq_set");
        
}
CSeq_entry_Info& GetSeq_entry(CTSE_Info& tse, const CBioObjectId& id)
{
    CTSE_Info_Object* to = tse.x_FindBioObject(id);
    CSeq_entry_Info* entry = dynamic_cast<CSeq_entry_Info*>(to);
    if (entry)
        return *entry;
    CBioseq_Base_Info* base = dynamic_cast<CBioseq_Base_Info*>(to);
    if (base)
        return base->GetParentSeq_entry_Info();

    NCBI_THROW(CLoaderException, eOtherError,
               "BioObjectId does not point to Seq_entry");
        
}



void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddId& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    const CSeq_id& seqid = cmd.GetAdd_id();
    CBioseq_Info& bioseq = GetBioseq(tse, id);
    bioseq.AddId(CSeq_id_Handle::GetHandle(seqid));
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveId& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    const CSeq_id& seqid = cmd.GetRemove_id();
    CBioseq_Info& bioseq = GetBioseq(tse, id);
    bioseq.RemoveId(CSeq_id_Handle::GetHandle(seqid));
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetIds& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_Info& bioseq = GetBioseq(tse, id);
    bioseq.ResetId();       
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ChangeSeqAttr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_Info& info = GetBioseq(tse, id);

    switch (cmd.GetData().Which()) {
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Inst:
        info.SetInst(const_cast<CSeq_inst&>(cmd.GetData().GetInst()));
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Repr:
        info.SetInst_Repr((CSeq_inst::ERepr)cmd.GetData().GetRepr());
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Mol:
        info.SetInst_Mol((CSeq_inst::EMol)cmd.GetData().GetMol());
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Length:
        info.SetInst_Length(cmd.GetData().GetLength());
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Fuzz:
        info.SetInst_Fuzz(const_cast<CInt_fuzz&>(cmd.GetData().GetFuzz()));
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Topology:
        info.SetInst_Topology((CSeq_inst::ETopology)cmd.GetData().GetTopology());
        break;      
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Strand:
        info.SetInst_Strand((CSeq_inst::EStrand)cmd.GetData().GetStrand());
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Ext:
        info.SetInst_Ext(const_cast<CSeq_ext&>(cmd.GetData().GetExt()));
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Hist:
        info.SetInst_Hist(const_cast<CSeq_hist&>(cmd.GetData().GetHist()));
        break;
    case CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Seq_data:
        info.SetInst_Seq_data(const_cast<CSeq_data&>(cmd.GetData().GetSeq_data()));
        break;       
    default:
        _ASSERT(0);
    }
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSeqAttr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_Info& info = GetBioseq(tse, id);
    switch (cmd.GetWhat()) {
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_inst:
        info.ResetInst();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_repr:
        info.ResetInst_Repr();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_mol:
        info.ResetInst_Mol();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_length:
        info.ResetInst_Length();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_fuzz:
        info.ResetInst_Fuzz();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_topology:
        info.ResetInst_Topology();
        break;      
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_strand:
        info.ResetInst_Strand();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_ext:
        info.ResetInst_Ext();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_hist:
        info.ResetInst_Hist();
        break;
    case CSeqEdit_Cmd_ResetSeqAttr::eWhat_seq_data:
        info.ResetInst_Seq_data();
        break;       
    default:
        _ASSERT(0);
    }

}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ChangeSetAttr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_set_Info& info = GetBioseq_set(tse, id);
    switch (cmd.GetData().Which()) {
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Id:
        info.SetId(const_cast<CObject_id&>(cmd.GetData().GetId()));
        break;
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Coll:
        info.SetColl(const_cast<CDbtag&>(cmd.GetData().GetColl()));
        break;
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Level:
        info.SetLevel(cmd.GetData().GetLevel());
        break;
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Class:
        info.SetClass((CBioseq_set::TClass)cmd.GetData().GetClass());
        break;
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Release:
        info.SetRelease(const_cast<string&>(cmd.GetData().GetRelease()));
        break;
    case CSeqEdit_Cmd_ChangeSetAttr::TData::e_Date:
        info.SetDate(const_cast<CDate&>(cmd.GetData().GetDate()));
        break;
    default:
        _ASSERT(0);
    }
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSetAttr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_set_Info& info = GetBioseq_set(tse, id);
    switch (cmd.GetWhat()) {
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_id:
        info.ResetId();
        break;
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_coll:
        info.ResetColl();
        break;
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_level:
        info.ResetLevel();
        break;
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_class:
        info.ResetClass();
        break;
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_release:
        info.ResetRelease();
        break;
    case CSeqEdit_Cmd_ResetSetAttr::eWhat_date:
        info.ResetDate();
        break;
    default:
        _ASSERT(0);
    }
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddDescr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    const CSeq_descr& descr = cmd.GetAdd_descr();
    CBioseq_Base_Info& base = GetBase(tse, id);
    base.AddSeq_descr(descr);   
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_SetDescr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_descr& descr = const_cast<CSeq_descr&>(cmd.GetSet_descr());
    CBioseq_Base_Info& base = GetBase(tse, id);
    base.SetDescr(descr);   
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetDescr& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_Base_Info& base = GetBase(tse, id);
    base.ResetDescr();   
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddDesc& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeqdesc& desc = const_cast<CSeqdesc&>(cmd.GetAdd_desc());
    CBioseq_Base_Info& base = GetBase(tse, id);
    base.AddSeqdesc(desc);   
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveDesc& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    const CSeqdesc& desc = cmd.GetRemove_desc();
    CBioseq_Base_Info& base = GetBase(tse, id);
    const CSeq_descr& descr = base.GetDescr();
    ITERATE(CSeq_descr::Tdata, it, descr.Get()) {
        const CSeqdesc& d = **it;
        if (d.Equals(desc)) {
            base.RemoveSeqdesc(d);
            break;
        }
    }
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSeq& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CBioseq& seq = const_cast<CBioseq&>(cmd.GetSeq());
    info.SelectSeq(seq);   
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSet& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CBioseq_set& set = const_cast<CBioseq_set&>(cmd.GetSet());
    info.SelectSet(set);
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ResetSeqEntry& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    info.Reset();
}
void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachSeqEntry& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_set_Info& info = GetBioseq_set(tse, id);
    CRef<CSeq_entry> entry;
    if (cmd.IsSetSeq_entry())
        entry.Reset(const_cast<CSeq_entry*>(&cmd.GetSeq_entry()));
    else 
        entry.Reset(new CSeq_entry);
    int index = cmd.GetIndex();
    info.AddEntry(*entry, index, true);
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveSeqEntry& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CBioseq_set_Info& info = GetBioseq_set(tse, id);
    CBioObjectId entry_id = s_Convert(cmd.GetEntry_id());
    CSeq_entry_Info& entry_info = GetSeq_entry(tse,entry_id);
    info.RemoveEntry(Ref(&entry_info));
}


void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AttachAnnot& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CSeq_annot& annot = const_cast<CSeq_annot&>(cmd.GetAnnot());
    info.AddAnnot(annot);
}

template<typename T> static inline
void x_MakeRemove(CTSE_Info& tse, const CSeq_entry_Info& info,
                   const CAnnotName& name, const T& old_value)
{
    CSeq_annot_Finder finder(tse);
    const CAnnotObject_Info* annot_obj = finder.Find(info,name,old_value);
    if (!annot_obj) 
        NCBI_THROW(CLoaderException, eOtherError,
                   "Annotation object is not found");
    CSeq_annot_Info& annot = const_cast<CSeq_annot_Info&>
        (annot_obj->GetSeq_annot_Info());
    annot.Remove(annot_obj->GetAnnotIndex());
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_RemoveAnnot& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CAnnotName annot_name;
    if (cmd.GetNamed()) {
        annot_name.SetNamed(cmd.GetName());
    }
    
    switch (cmd.GetData().Which()) {
    case CSeqEdit_Cmd_RemoveAnnot::TData::e_Feat:
        x_MakeRemove(tse, info, annot_name, cmd.GetData().GetFeat());
        break;
    case CSeqEdit_Cmd_RemoveAnnot::TData::e_Align:
        x_MakeRemove(tse, info, annot_name, cmd.GetData().GetAlign());
        break;
    case CSeqEdit_Cmd_RemoveAnnot::TData::e_Graph:
        x_MakeRemove(tse, info, annot_name, cmd.GetData().GetGraph());
        break;
    default:
        NCBI_THROW(CLoaderException, eOtherError,
               "Annotation is not set");
    }
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_AddAnnot& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CAnnotName annot_name;
    if (cmd.GetNamed()) {
        annot_name.SetNamed(cmd.GetName());
    }

    CSeq_annot_Finder finder(tse);
    CSeq_annot_Info* annot = NULL;

    if (cmd.IsSetSearch_param() && 
        cmd.GetSearch_param().Which() == CSeqEdit_Cmd_AddAnnot::TSearch_param::e_Obj) {
        const CAnnotObject_Info* annot_obj = NULL;
        switch (cmd.GetData().Which()) {
        case CSeqEdit_Cmd_AddAnnot::TSearch_param::TObj::e_Feat:
            annot_obj = finder.Find(info, annot_name, 
                                    cmd.GetSearch_param().GetObj().GetFeat());
            break;
        case CSeqEdit_Cmd_AddAnnot::TSearch_param::TObj::e_Align:
            annot_obj = finder.Find(info, annot_name, 
                                    cmd.GetSearch_param().GetObj().GetAlign());
            break;
        case CSeqEdit_Cmd_AddAnnot::TSearch_param::TObj::e_Graph:
            annot_obj = finder.Find(info, annot_name, 
                                    cmd.GetSearch_param().GetObj().GetGraph());
            break;
        default:
            NCBI_THROW(CLoaderException, eOtherError,
                       "Annotation is not set");
        }
        if (!annot_obj) 
            NCBI_THROW(CLoaderException, eOtherError,
                       "Seq_annot object is not found");
        annot = &const_cast<CAnnotObject_Info*>(annot_obj)->GetSeq_annot_Info();
    } else if (cmd.IsSetSearch_param() && 
               cmd.GetSearch_param().Which() 
               == CSeqEdit_Cmd_AddAnnot::TSearch_param::e_Descr) {
        annot = const_cast<CSeq_annot_Info*>(finder.Find(info, annot_name, 
                                                cmd.GetSearch_param().GetDescr()));
    } else {
        annot = const_cast<CSeq_annot_Info*>(finder.Find(info, annot_name));       
    }
    if (!annot) 
        NCBI_THROW(CLoaderException, eOtherError,
                   "Seq_annot object is not found");

    switch (cmd.GetData().Which()) {
    case CSeqEdit_Cmd_AddAnnot::TData::e_Feat:
        annot->Add(cmd.GetData().GetFeat());
        break;
    case CSeqEdit_Cmd_AddAnnot::TData::e_Align:
        annot->Add(cmd.GetData().GetAlign());
        break;
    case CSeqEdit_Cmd_AddAnnot::TData::e_Graph:
        annot->Add(cmd.GetData().GetGraph());
        break;
    default:
        NCBI_THROW(CLoaderException, eOtherError,
               "Annotation is not set");
    }
}

template<typename T> static inline
void x_MakeReplace(CTSE_Info& tse,const CSeq_entry_Info& info, 
                   const CAnnotName& name,
                   const T& old_value, const T& new_value)
{
    CSeq_annot_Finder finder(tse);
    const CAnnotObject_Info* annot_obj = finder.Find(info, name, old_value);
    if (!annot_obj) 
        NCBI_THROW(CLoaderException, eOtherError,
                   "Annotation object is not found");
    CSeq_annot_Info& annot = const_cast<CSeq_annot_Info&>
        (annot_obj->GetSeq_annot_Info());
    annot.Replace(annot_obj->GetAnnotIndex(), new_value);
}

void x_ApplyCmd(CTSE_Info& tse, const CSeqEdit_Cmd_ReplaceAnnot& cmd)
{
    CBioObjectId id = s_Convert(cmd.GetId());
    CSeq_entry_Info& info = GetSeq_entry(tse,id);
    CAnnotName annot_name;
    if (cmd.GetNamed()) {
        annot_name.SetNamed(cmd.GetName());
    }
    
    switch (cmd.GetData().Which()) {
    case CSeqEdit_Cmd_ReplaceAnnot::TData::e_Feat:
        x_MakeReplace(tse,info, annot_name, 
                      cmd.GetData().GetFeat().GetOvalue(),
                      cmd.GetData().GetFeat().GetNvalue());
        break;
    case CSeqEdit_Cmd_ReplaceAnnot::TData::e_Align:
        x_MakeReplace(tse, info, annot_name, 
                      cmd.GetData().GetAlign().GetOvalue(),
                      cmd.GetData().GetAlign().GetNvalue());
        break;
    case CSeqEdit_Cmd_ReplaceAnnot::TData::e_Graph:
        x_MakeReplace(tse, info, annot_name, 
                      cmd.GetData().GetGraph().GetOvalue(),
                      cmd.GetData().GetGraph().GetNvalue());
        break;
    default:
        NCBI_THROW(CLoaderException, eOtherError,
               "Annotation is not set");
    }
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_Patcher(void)
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager<CDataLoader>
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader>
                                ::EEntryPointRequest);
    
    RegisterEntryPoint<CDataLoader>((TArgFuncType)
                                    NCBI_EntryPoint_DataLoader_Patcher);
}

const string kDataLoader_Patcher_DriverName("dlpatcher");

/// Data Loader Factory for BlastDbDataLoader
///
/// This class provides an interface which builds an instance of the
/// BlastDbDataLoader and registers it with the object manager.

class CDLPatcher_DataLoaderCF : public CDataLoaderFactory
{
public:
    /// Constructor
    CDLPatcher_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Patcher_DriverName) {}
    
    /// Destructor
    virtual ~CDLPatcher_DataLoaderCF(void) {}
    
protected:
    /// Create and register a data loader
    /// @param om
    ///   A reference to the object manager
    /// @param params
    ///   Arguments for the data loader constructor
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CDLPatcher_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        return NULL;
    }
    //Parse params, select constructor
    const string& data_loader =
        GetParam(GetDriverName(), params,
                 kCFParam_DLP_DataLoader, false);
    const string& db_engine =
        GetParam(GetDriverName(), params,
                 kCFParam_DLP_EditsDBEngine, false);

    if ( !data_loader.empty() && !db_engine.empty() ) {
        const TPluginManagerParamTree* dl_tree = 
            params->FindNode(data_loader);
        
        typedef CPluginManager<CDataLoader> TDLManager;
        TDLManager dl_manager;
        CRef<CDataLoader> dl(dl_manager.CreateInstance(data_loader,
                                                      TDLManager::GetDefaultDrvVers(),
                                                      dl_tree));
        const TPluginManagerParamTree* db_tree = 
            params->FindNode(db_engine);
        
        typedef CPluginManager<IEditsDBEngine> TDBManager;
        TDBManager db_manager;
        CRef<IEditsDBEngine> db(db_manager.CreateInstance(db_engine,
                                                          TDBManager::GetDefaultDrvVers(),
                                                          db_tree));
        const string& edit_saver =
            GetParam(GetDriverName(), params,
                     kCFParam_DLP_EditSaver, false);

        CRef<IEditSaver> es;
        if ( !edit_saver.empty() ) {
            const TPluginManagerParamTree* es_tree = 
                params->FindNode(edit_saver);

            typedef CPluginManager<IEditSaver> TESManager;
            TESManager es_manager;
            es = es_manager.CreateInstance(edit_saver,
                                           TESManager::GetDefaultDrvVers(),
                                           es_tree);
        }

        if (dl && db) {
            return CDataLoaderPatcher::RegisterInObjectManager(
                             om,
                             dl,
                             db,
                             es,
                             GetIsDefault(params),
                             GetPriority(params)).GetLoader();
        }
    }
    return NULL;
}


void NCBI_EntryPoint_DataLoader_Patcher(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDLPatcher_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_Patcher(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Patcher(info_list, method);
}


END_NCBI_SCOPE
