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
*   Bioseq info for data source
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/seq_map.hpp>

#include <objects/seq/Bioseq.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(CBioseq& seq)
    : m_AssemblyChunk(-1),
      m_FeatureFetchPolicy(-1)
{
    x_SetObject(seq);
}


CBioseq_Info::CBioseq_Info(const CBioseq_Info& info, TObjectCopyMap* copy_map)
    : TParent(info, copy_map),
      m_Seq_dataChunks(info.m_Seq_dataChunks),
      m_AssemblyChunk(info.m_AssemblyChunk),
      m_FeatureFetchPolicy(info.m_FeatureFetchPolicy)
{
    x_SetObject(info, copy_map);
}


CBioseq_Info::~CBioseq_Info(void)
{
    x_ResetSeqMap();
}


CConstRef<CBioseq> CBioseq_Info::GetCompleteBioseq(void) const
{
    x_UpdateComplete();
    return m_Object;
}


CConstRef<CBioseq> CBioseq_Info::GetBioseqCore(void) const
{
    x_UpdateCore();
    return m_Object;
}


void CBioseq_Info::x_SetChangedSeqMap(void)
{
    x_SetNeedUpdate(fNeedUpdate_seq_data);
}


void CBioseq_Info::x_AddSeq_dataChunkId(TChunkId chunk_id)
{
    m_Seq_dataChunks.push_back(chunk_id);
    x_SetNeedUpdate(fNeedUpdate_seq_data);
}


void CBioseq_Info::x_AddAssemblyChunkId(TChunkId chunk_id)
{
    _ASSERT(m_AssemblyChunk < 0);
    _ASSERT(chunk_id >= 0);
    m_AssemblyChunk = chunk_id;
    x_SetNeedUpdate(fNeedUpdate_assembly);
}


void CBioseq_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    if ( flags & fNeedUpdate_seq_data ) {
        if ( !m_Seq_dataChunks.empty() ) {
            x_LoadChunks(m_Seq_dataChunks);
        }
        if ( m_SeqMap ) {
            m_SeqMap->x_UpdateSeq_inst(m_Object->SetInst());
        }
    }
    if ( flags & fNeedUpdate_assembly ) {
        TChunkId chunk = m_AssemblyChunk;
        if ( chunk >= 0 ) {
            x_LoadChunk(chunk);
        }
    }
    TParent::x_DoUpdate(flags);
}


void CBioseq_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    x_DSMapObject(m_Object, ds);
}


void CBioseq_Info::x_DSDetachContents(CDataSource& ds)
{
    x_DSUnmapObject(m_Object, ds);
    TParent::x_DSDetachContents(ds);
}


void CBioseq_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
}


void CBioseq_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
}


void CBioseq_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    TParent::x_TSEAttachContents(tse);
    SetBioObjectId(tse.x_IndexBioseq(this));
    /*
    x_RegisterRemovedIds(m_Object, *this);
    ITERATE ( TId, it, m_Id ) {
        tse.x_SetBioseqId(*it, this);
    }
    */
}


void CBioseq_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    ITERATE ( TId, it, m_Id ) {
        tse.x_ResetBioseqId(*it, this);
    }
    TParent::x_TSEDetachContents(tse);
}


void CBioseq_Info::x_ParentAttach(CSeq_entry_Info& parent)
{
    TParent::x_ParentAttach(parent);
    CSeq_entry& entry = parent.x_GetObject();
    entry.ParentizeOneLevel();
#ifdef _DEBUG
    _ASSERT(&entry.GetSeq() == m_Object);
    _ASSERT(m_Object->GetParentEntry() == &entry);
#endif
}


void CBioseq_Info::x_ParentDetach(CSeq_entry_Info& parent)
{
    //m_Object->ResetParentEntry();
    TParent::x_ParentDetach(parent);
}


int CBioseq_Info::GetFeatureFetchPolicy(void) const
{
    if ( m_FeatureFetchPolicy == -1 ) {
        int policy = -1;
        if ( IsSetDescr() ) {
            for ( TDesc_CI it = x_GetFirstDesc(1<<CSeqdesc::e_User);
                  policy == -1 && !x_IsEndDesc(it);
                  it = x_GetNextDesc(it, 1<<CSeqdesc::e_User) ) {
                const CSeqdesc& desc = **it;
                if ( !desc.IsUser() ) {
                    continue;
                }
                const CUser_object& user = desc.GetUser();
                const CObject_id& id = user.GetType();
                if ( !id.IsStr() || id.GetStr() != "FeatureFetchPolicy" ) {
                    continue;
                }
                ITERATE ( CUser_object::TData, fit, user.GetData() ) {
                    const CUser_field& field = **fit;
                    const CObject_id& fid = field.GetLabel();
                    if ( !fid.IsStr() || fid.GetStr() != "Policy" ) {
                        continue;
                    }
                    if ( !field.GetData().IsStr() ) {
                        continue;
                    }
                    const string& str = field.GetData().GetStr();
                    if ( str == "OnlyNearFeatures" ) {
                        policy = CBioseq_Handle::eFeatureFetchPolicy_only_near;
                    }
                    else {
                        policy = CBioseq_Handle::eFeatureFetchPolicy_default;
                    }
                    break;
                }
            }
        }
        if ( policy == -1 ) {
            policy = CBioseq_Handle::eFeatureFetchPolicy_default;
        }
        m_FeatureFetchPolicy = policy;
    }
    return m_FeatureFetchPolicy;
}


void CBioseq_Info::x_SetObject(TObject& obj)
{
    _ASSERT(!m_Object);

    m_Object.Reset(&obj);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    if ( obj.IsSetId() ) {
        ITERATE ( TObject::TId, it, obj.GetId() ) {
            m_Id.push_back(CSeq_id_Handle::GetHandle(**it));
        }
    }
    if ( obj.IsSetAnnot() ) {
        x_SetAnnot();
    }
    m_FeatureFetchPolicy = -1;
}


void CBioseq_Info::x_SetObject(const CBioseq_Info& info,
                               TObjectCopyMap* copy_map)
{
    _ASSERT(!m_Object);

    // update Seq-inst object with split data
    info.x_Update(fNeedUpdate_seq_data);
    m_Object = sx_ShallowCopy(*info.m_Object);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    m_Id = info.m_Id;
    if ( info.m_SeqMap ) {
        m_SeqMap = info.m_SeqMap->CloneFor(*m_Object);
        m_SeqMap->m_Bioseq = this;
    }
    if ( info.IsSetAnnot() ) {
        x_SetAnnot(info, copy_map);
    }
    m_FeatureFetchPolicy = info.m_FeatureFetchPolicy;
}


CRef<CBioseq> CBioseq_Info::sx_ShallowCopy(const TObject& src)
{        
    CRef<TObject> obj(new TObject);
    if ( src.IsSetId() ) {
        obj->SetId() = src.GetId();
    }
    if ( src.IsSetDescr() ) {
        obj->SetDescr().Set() = src.GetDescr().Get();
    }
    if ( src.IsSetInst() ) {
        CRef<TInst> inst = sx_ShallowCopy(src.GetInst());
        obj->SetInst(*inst);
    }
    if ( src.IsSetAnnot() ) {
        obj->SetAnnot() = src.GetAnnot();
    }
    return obj;
}


CRef<CSeq_inst> CBioseq_Info::sx_ShallowCopy(const TInst& src)
{
    CRef<TInst> obj(new TInst);
    if ( src.IsSetRepr() ) {
        obj->SetRepr(src.GetRepr());
    }
    if ( src.IsSetMol() ) {
        obj->SetMol(src.GetMol());
    }
    if ( src.IsSetLength() ) {
        obj->SetLength(src.GetLength());
    }
    if ( src.IsSetFuzz() ) {
        obj->SetFuzz(const_cast<TInst_Fuzz&>(src.GetFuzz()));
    }
    if ( src.IsSetTopology() ) {
        obj->SetTopology(src.GetTopology());
    }
    if ( src.IsSetStrand() ) {
        obj->SetStrand(src.GetStrand());
    }
    if ( src.IsSetSeq_data() ) {
        obj->SetSeq_data(const_cast<TInst_Seq_data&>(src.GetSeq_data()));
    }
    if ( src.IsSetExt() ) {
        obj->SetExt(const_cast<TInst_Ext&>(src.GetExt()));
    }
    if ( src.IsSetHist() ) {
        obj->SetHist(const_cast<TInst_Hist&>(src.GetHist()));
    }
    return obj;
}


/////////////////////////////////////////////////////////////////////////////
// id
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetId(void) const
{
    return m_Object->IsSetId();
}


bool CBioseq_Info::CanGetId(void) const
{
    return m_Object->CanGetId();
}


const CBioseq_Info::TId& CBioseq_Info::GetId(void) const
{
    return m_Id;
}


void CBioseq_Info::ResetId(void)
{
    ITERATE(TId, id, m_Id) {
        GetTSE_Info().x_ResetBioseqId(*id,this);        
    }
    m_Id.clear();
    m_Object->ResetId();
    SetBioObjectId(GetTSE_Info().x_RegisterBioObject(*this));
}


bool CBioseq_Info::HasId(const CSeq_id_Handle& id) const
{
    return find(m_Id.begin(), m_Id.end(), id) != m_Id.end();
}


bool CBioseq_Info::AddId(const CSeq_id_Handle& id)
{
    TId::iterator found = find(m_Id.begin(), m_Id.end(), id);
    if ( found != m_Id.end() ) {
        return false;
    }
    m_Id.push_back(id);
    CRef<CSeq_id> seq_id(new CSeq_id);
    seq_id->Assign(*id.GetSeqId());
    m_Object->SetId().push_back(seq_id);
    GetTSE_Info().x_SetBioseqId(id,this);
    return true;
}


bool CBioseq_Info::RemoveId(const CSeq_id_Handle& id)
{
    TId::iterator found = find(m_Id.begin(), m_Id.end(), id);
    if ( found == m_Id.end() ) {
        return false;
    }
    m_Id.erase(found);
    NON_CONST_ITERATE ( TObject::TId, it, m_Object->SetId() ) {
        if ( CSeq_id_Handle::GetHandle(**it) == id ) {
            m_Object->SetId().erase(it);
            break;
        }
    }
    GetTSE_Info().x_ResetBioseqId(id,this);
    if (GetBioObjectId() == CBioObjectId(id)) {
        SetBioObjectId(GetTSE_Info().x_RegisterBioObject(*this));
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
// descr
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::x_IsSetDescr(void) const
{
    return m_Object->IsSetDescr();
}


bool CBioseq_Info::x_CanGetDescr(void) const
{
    return m_Object->CanGetDescr();
}


const CSeq_descr& CBioseq_Info::x_GetDescr(void) const
{
    return m_Object->GetDescr();
}


CSeq_descr& CBioseq_Info::x_SetDescr(void)
{
    return m_Object->SetDescr();
}


void CBioseq_Info::x_SetDescr(TDescr& v)
{
    m_Object->SetDescr(v);
}


void CBioseq_Info::x_ResetDescr(void)
{
    m_Object->ResetDescr();
}


/////////////////////////////////////////////////////////////////////////////
// annot
/////////////////////////////////////////////////////////////////////////////

CBioseq::TAnnot& CBioseq_Info::x_SetObjAnnot(void)
{
    return m_Object->SetAnnot();
}


void CBioseq_Info::x_ResetObjAnnot(void)
{
    m_Object->ResetAnnot();
}


/////////////////////////////////////////////////////////////////////////////
// inst
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst(void) const
{
    return m_Object->IsSetInst();
}


bool CBioseq_Info::CanGetInst(void) const
{
    return m_Object->CanGetInst();
}


const CBioseq_Info::TInst& CBioseq_Info::GetInst(void) const
{
    x_Update(fNeedUpdate_seq_data|fNeedUpdate_assembly);
    return m_Object->GetInst();
}


void CBioseq_Info::SetInst(TInst& v)
{
    x_ResetSeqMap();
    m_Seq_dataChunks.clear();
    m_Object->SetInst(v);
}

void CBioseq_Info::ResetInst()
{
    if (IsSetInst()) {
        x_ResetSeqMap();
        m_Seq_dataChunks.clear();
        m_Object->ResetInst();
    }
}


void CBioseq_Info::x_ResetSeqMap(void)
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap ) {
        m_SeqMap->m_Bioseq = 0;
        m_SeqMap.Reset();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.repr
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Repr(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetRepr();
}


bool CBioseq_Info::CanGetInst_Repr(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetRepr();
}


CBioseq_Info::TInst_Repr CBioseq_Info::GetInst_Repr(void) const
{
    return m_Object->GetInst().GetRepr();
}


void CBioseq_Info::SetInst_Repr(TInst_Repr v)
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap ) {
        m_SeqMap->SetRepr(v);
    }
    m_Object->SetInst().SetRepr(v);
}

void CBioseq_Info::ResetInst_Repr()
{
    if (IsSetInst_Repr()) {
        CFastMutexGuard guard(m_SeqMap_Mtx);
        if ( m_SeqMap ) {
            m_SeqMap->ResetRepr();
        }
        m_Object->SetInst().ResetRepr();
    }
}

/////////////////////////////////////////////////////////////////////////////
// inst.mol
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Mol(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetMol();
}


bool CBioseq_Info::CanGetInst_Mol(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetMol();
}


CBioseq_Info::TInst_Mol CBioseq_Info::GetInst_Mol(void) const
{
    return m_Object->GetInst().GetMol();
}


void CBioseq_Info::SetInst_Mol(TInst_Mol v)
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap ) {
        m_SeqMap->SetMol(v);
    }
    m_Object->SetInst().SetMol(v);
}

void CBioseq_Info::ResetInst_Mol()
{
    if (IsSetInst_Mol()) {
        CFastMutexGuard guard(m_SeqMap_Mtx);
        if ( m_SeqMap ) {
            m_SeqMap->ResetMol();
        }
        m_Object->SetInst().ResetMol();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.length
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Length(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetLength();
}


bool CBioseq_Info::CanGetInst_Length(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetLength();
}


CBioseq_Info::TInst_Length CBioseq_Info::GetInst_Length(void) const
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap ) {
        return m_SeqMap->GetLength(0);
    }
    else {
        return m_Object->GetInst().GetLength();
    }
}


void CBioseq_Info::SetInst_Length(TInst_Length v)
{
    x_Update(fNeedUpdate_seq_data);
    x_ResetSeqMap();
    m_Object->SetInst().SetLength(v);
}

void CBioseq_Info::ResetInst_Length()
{
    if (IsSetInst_Length()) {
        x_Update(fNeedUpdate_seq_data);
        x_ResetSeqMap();
        m_Object->SetInst().ResetLength();
    }
}

CBioseq_Info::TInst_Length CBioseq_Info::GetBioseqLength(void) const
{
    if ( IsSetInst_Length() ) {
        return GetInst_Length();
    }
    else {
        return x_CalcBioseqLength();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.fuzz
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Fuzz(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetFuzz();
}


bool CBioseq_Info::CanGetInst_Fuzz(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetFuzz();
}


const CBioseq_Info::TInst_Fuzz& CBioseq_Info::GetInst_Fuzz(void) const
{
    return m_Object->GetInst().GetFuzz();
}


void CBioseq_Info::SetInst_Fuzz(TInst_Fuzz& v)
{
    m_Object->SetInst().SetFuzz(v);
}

void CBioseq_Info::ResetInst_Fuzz()
{
    if (IsSetInst_Fuzz()) {
        m_Object->SetInst().ResetFuzz();
    }
}

/////////////////////////////////////////////////////////////////////////////
// inst.topology
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Topology(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetTopology();
}


bool CBioseq_Info::CanGetInst_Topology(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetTopology();
}


CBioseq_Info::TInst_Topology CBioseq_Info::GetInst_Topology(void) const
{
    return m_Object->GetInst().GetTopology();
}


void CBioseq_Info::SetInst_Topology(TInst_Topology v)
{
    m_Object->SetInst().SetTopology(v);
}

void CBioseq_Info::ResetInst_Topology()
{
    if (IsSetInst_Topology()) {
        m_Object->SetInst().ResetTopology();
    }
}

/////////////////////////////////////////////////////////////////////////////
// inst.strand
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Strand(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetStrand();
}


bool CBioseq_Info::CanGetInst_Strand(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetStrand();
}


CBioseq_Info::TInst_Strand CBioseq_Info::GetInst_Strand(void) const
{
    return m_Object->GetInst().GetStrand();
}


void CBioseq_Info::SetInst_Strand(TInst_Strand v)
{
    m_Object->SetInst().SetStrand(v);
}

void CBioseq_Info::ResetInst_Strand()
{
    if (IsSetInst_Strand()) {
        m_Object->SetInst().ResetStrand();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.seq-data
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Seq_data(void) const
{
    return IsSetInst() && GetInst().IsSetSeq_data();
}


bool CBioseq_Info::CanGetInst_Seq_data(void) const
{
    return CanGetInst() && GetInst().CanGetSeq_data();
}


const CBioseq_Info::TInst_Seq_data& CBioseq_Info::GetInst_Seq_data(void) const
{
    return GetInst().GetSeq_data();
}


void CBioseq_Info::SetInst_Seq_data(TInst_Seq_data& v)
{
    x_Update(fNeedUpdate_seq_data);
    x_ResetSeqMap();
    m_Seq_dataChunks.clear();
    m_Object->SetInst().SetSeq_data(v);
}

void CBioseq_Info::ResetInst_Seq_data()
{
    if (IsSetInst_Seq_data()) {
        x_Update(fNeedUpdate_seq_data);
        x_ResetSeqMap();
        m_Seq_dataChunks.clear();
        m_Object->SetInst().ResetSeq_data();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.ext
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Ext(void) const
{
    return IsSetInst() && GetInst().IsSetExt();
}


bool CBioseq_Info::CanGetInst_Ext(void) const
{
    return CanGetInst() && GetInst().CanGetExt();
}


const CBioseq_Info::TInst_Ext& CBioseq_Info::GetInst_Ext(void) const
{
    return GetInst().GetExt();
}


void CBioseq_Info::SetInst_Ext(TInst_Ext& v)
{
    x_Update(fNeedUpdate_seq_data);
    x_ResetSeqMap();
    m_Seq_dataChunks.clear();
    m_Object->SetInst().SetExt(v);
}

void CBioseq_Info::ResetInst_Ext()
{
    if (IsSetInst_Ext()) {
        x_Update(fNeedUpdate_seq_data);
        x_ResetSeqMap();
        m_Seq_dataChunks.clear();
        m_Object->SetInst().ResetExt();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.hist
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Hist(void) const
{
    return IsSetInst() && m_Object->GetInst().IsSetHist();
}


bool CBioseq_Info::CanGetInst_Hist(void) const
{
    return CanGetInst() && m_Object->GetInst().CanGetHist();
}


const CBioseq_Info::TInst_Hist& CBioseq_Info::GetInst_Hist(void) const
{
    x_Update(fNeedUpdate_assembly);
    return m_Object->GetInst().GetHist();
}


void CBioseq_Info::SetInst_Hist(TInst_Hist& v)
{
    x_Update(fNeedUpdate_assembly);
    m_AssemblyChunk = -1;
    m_Object->SetInst().SetHist(v);
}


void CBioseq_Info::ResetInst_Hist()
{
    if (IsSetInst_Hist()) {
        x_Update(fNeedUpdate_assembly);
        m_AssemblyChunk = -1;
        m_Object->SetInst().ResetHist();
    }
}


/////////////////////////////////////////////////////////////////////////////
// inst.hist.assembly
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Hist_Assembly(void) const
{
    return IsSetInst_Hist() &&
        (m_AssemblyChunk >= 0||m_Object->GetInst().GetHist().IsSetAssembly());
}


bool CBioseq_Info::CanGetInst_Hist_Assembly(void) const
{
    return CanGetInst_Hist();
}


const CBioseq_Info::TInst_Hist_Assembly&
CBioseq_Info::GetInst_Hist_Assembly(void) const
{
    x_Update(fNeedUpdate_assembly);
    return m_Object->GetInst().GetHist().GetAssembly();
}


void CBioseq_Info::SetInst_Hist_Assembly(const TInst_Hist_Assembly& v)
{
    x_Update(fNeedUpdate_assembly);
    m_AssemblyChunk = -1;
    m_Object->SetInst().SetHist().SetAssembly() = v;
}


/////////////////////////////////////////////////////////////////////////////
// inst.hist.replaces
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Hist_Replaces(void) const
{
    return IsSetInst_Hist() && m_Object->GetInst().GetHist().IsSetReplaces();
}


bool CBioseq_Info::CanGetInst_Hist_Replaces(void) const
{
    return CanGetInst_Hist() && m_Object->GetInst().GetHist().CanGetReplaces();
}


const CBioseq_Info::TInst_Hist_Replaces&
CBioseq_Info::GetInst_Hist_Replaces(void) const
{
    return m_Object->GetInst().GetHist().GetReplaces();
}


void CBioseq_Info::SetInst_Hist_Replaces(TInst_Hist_Replaces& v)
{
    m_Object->SetInst().SetHist().SetReplaces(v);
}


/////////////////////////////////////////////////////////////////////////////
// inst.hist.replaced-by
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Hist_Replaced_by(void) const
{
    return IsSetInst_Hist() &&
        m_Object->GetInst().GetHist().IsSetReplaced_by();
}


bool CBioseq_Info::CanGetInst_Hist_Replaced_by(void) const
{
    return CanGetInst_Hist() &&
        m_Object->GetInst().GetHist().CanGetReplaced_by();
}


const CBioseq_Info::TInst_Hist_Replaced_by&
CBioseq_Info::GetInst_Hist_Replaced_by(void) const
{
    return m_Object->GetInst().GetHist().GetReplaced_by();
}


void CBioseq_Info::SetInst_Hist_Replaced_by(TInst_Hist_Replaced_by& v)
{
    m_Object->SetInst().SetHist().SetReplaced_by(v);
}


/////////////////////////////////////////////////////////////////////////////
// inst.hist.deleted
/////////////////////////////////////////////////////////////////////////////

bool CBioseq_Info::IsSetInst_Hist_Deleted(void) const
{
    return IsSetInst_Hist() && m_Object->GetInst().GetHist().IsSetDeleted();
}


bool CBioseq_Info::CanGetInst_Hist_Deleted(void) const
{
    return CanGetInst_Hist() && m_Object->GetInst().GetHist().CanGetDeleted();
}


const CBioseq_Info::TInst_Hist_Deleted&
CBioseq_Info::GetInst_Hist_Deleted(void) const
{
    return m_Object->GetInst().GetHist().GetDeleted();
}


void CBioseq_Info::SetInst_Hist_Deleted(TInst_Hist_Deleted& v)
{
    m_Object->SetInst().SetHist().SetDeleted(v);
}


bool CBioseq_Info::IsNa(void) const
{
    return m_Object->IsNa();
}


bool CBioseq_Info::IsAa(void) const
{
    return m_Object->IsAa();
}


/////////////////////////////////////////////////////////////////////////////
// calculate bioseq length if inst.length field is not set
/////////////////////////////////////////////////////////////////////////////

TSeqPos CBioseq_Info::x_CalcBioseqLength(void) const
{
    return x_CalcBioseqLength(GetInst());
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_inst& inst) const
{
    if ( !inst.IsSetExt() ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CBioseq_Info::x_CalcBioseqLength: "
                   "failed: Seq-inst.ext is not set");
    }
    switch ( inst.GetExt().Which() ) {
    case CSeq_ext::e_Seg:
        return x_CalcBioseqLength(inst.GetExt().GetSeg());
    case CSeq_ext::e_Ref:
        return x_CalcBioseqLength(inst.GetExt().GetRef().Get());
    case CSeq_ext::e_Delta:
        return x_CalcBioseqLength(inst.GetExt().GetDelta());
    default:
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CBioseq_Info::x_CalcBioseqLength: "
                   "failed: bad Seg-ext type");
    }
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_id& whole) const
{
    CConstRef<CBioseq_Info> ref =
        GetTSE_Info().FindMatchingBioseq(CSeq_id_Handle::GetHandle(whole));
    if ( !ref ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CBioseq_Info::x_CalcBioseqLength: "
                   "failed: external whole reference");
    }
    return ref->GetBioseqLength();
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CPacked_seqint& ints) const
{
    TSeqPos ret = 0;
    ITERATE ( CPacked_seqint::Tdata, it, ints.Get() ) {
        ret += (*it)->GetLength();
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc& seq_loc) const
{
    switch ( seq_loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return 0;
    case CSeq_loc::e_Whole:
        return x_CalcBioseqLength(seq_loc.GetWhole());
    case CSeq_loc::e_Int:
        return seq_loc.GetInt().GetLength();
    case CSeq_loc::e_Pnt:
        return 1;
    case CSeq_loc::e_Packed_int:
        return x_CalcBioseqLength(seq_loc.GetPacked_int());
    case CSeq_loc::e_Packed_pnt:
        return seq_loc.GetPacked_pnt().GetPoints().size();
    case CSeq_loc::e_Mix:
        return x_CalcBioseqLength(seq_loc.GetMix());
    case CSeq_loc::e_Equiv:
        return x_CalcBioseqLength(seq_loc.GetEquiv());
    default:
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CBioseq_Info::x_CalcBioseqLength: "
                   "failed: bad Seq-loc type");
    }
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc_mix& seq_mix) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeq_loc_mix::Tdata, it, seq_mix.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc_equiv& seq_equiv) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeq_loc_equiv::Tdata, it, seq_equiv.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeg_ext& seg_ext) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeg_ext::Tdata, it, seg_ext.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CDelta_ext& delta) const
{
    TSeqPos ret = 0;
    ITERATE ( CDelta_ext::Tdata, it, delta.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CDelta_seq& delta_seq) const
{
    switch ( delta_seq.Which() ) {
    case CDelta_seq::e_Loc:
        return x_CalcBioseqLength(delta_seq.GetLoc());
    case CDelta_seq::e_Literal:
        return delta_seq.GetLiteral().GetLength();
    default:
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CBioseq_Info::x_CalcBioseqLength: "
                   "failed: bad Delta-seq type");
    }
}


string CBioseq_Info::IdString(void) const
{
    CNcbiOstrstream os;
    ITERATE ( TId, it, m_Id ) {
        if ( it != m_Id.begin() )
            os << " | ";
        os << it->AsString();
    }
    return CNcbiOstrstreamToString(os);
}


void CBioseq_Info::x_AttachMap(CSeqMap& seq_map)
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap || seq_map.m_Bioseq ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                     "CBioseq_Info::AttachMap: bioseq already has SeqMap");
    }
    m_SeqMap.Reset(&seq_map);
    seq_map.m_Bioseq = this;
}


const CSeqMap& CBioseq_Info::GetSeqMap(void) const
{
    const CSeqMap* ret = m_SeqMap.GetPointer();
    if ( !ret ) {
        CFastMutexGuard guard(m_SeqMap_Mtx);
        ret = m_SeqMap.GetPointer();
        if ( !ret ) {
            m_SeqMap = CSeqMap::CreateSeqMapForBioseq(*m_Object);
            m_SeqMap->m_Bioseq = const_cast<CBioseq_Info*>(this);
            ret = m_SeqMap.GetPointer();
            _ASSERT(ret);
        }
    }
    return *ret;
}


int CBioseq_Info::GetTaxId(void) const
{
    const COrg_ref* org_ref = 0;
    if ( const CSeqdesc* desc = x_SearchFirstDesc(1<<CSeqdesc::e_Source) ) {
        org_ref = &desc->GetSource().GetOrg();
    }
    else if ( const CSeqdesc* desc = x_SearchFirstDesc(1<<CSeqdesc::e_Org) ) {
        org_ref = &desc->GetOrg();
    }
    else {
        return 0;
    }
    try {
        return org_ref->GetTaxId();
    }
    catch ( CException& /*ignored*/ ) {
        return 0;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
