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

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/general/Int_fuzz.hpp>
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
    : m_AssemblyChunk(-1)
{
    x_SetObject(seq);
}


CBioseq_Info::CBioseq_Info(const CBioseq_Info& info)
    : m_AssemblyChunk(-1)
{
    x_SetObject(info);
}


CBioseq_Info::~CBioseq_Info(void)
{
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
        x_LoadChunks(m_Seq_dataChunks);
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
    ITERATE ( TId, it, m_Id ) {
        tse.x_SetBioseqId(*it, this);
    }
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
}


void CBioseq_Info::x_SetObject(const CBioseq_Info& info)
{
    _ASSERT(!m_Object);

    m_Object = sx_ShallowCopy(*info.m_Object);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    m_Id = info.m_Id;
    m_SeqMap = info.m_SeqMap;
    if ( info.IsSetAnnot() ) {
        x_SetAnnot(info);
    }
}


CRef<CBioseq> CBioseq_Info::sx_ShallowCopy(const TObject& src)
{        
    CRef<TObject> obj(new TObject);
    if ( src.IsSetId() ) {
        obj->SetId() = src.GetId();
    }
    if ( src.IsSetDescr() ) {
        obj->SetDescr(const_cast<TDescr&>(src.GetDescr()));
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
    m_Id.clear();
    m_Object->ResetId();
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
    m_Seq_dataChunks.clear();
    m_Object->SetInst(v);
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
    m_Object->SetInst().SetRepr(v);;
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
    m_Object->SetInst().SetMol(v);;
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
    return m_Object->GetInst().GetLength();
}


void CBioseq_Info::SetInst_Length(TInst_Length v)
{
    m_Object->SetInst().SetLength(v);;
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
    m_Object->SetInst().SetFuzz(v);;
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
    m_Object->SetInst().SetTopology(v);;
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
    m_Object->SetInst().SetStrand(v);;
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
    m_Seq_dataChunks.clear();
    m_Object->SetInst().SetSeq_data(v);;
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
    m_Seq_dataChunks.clear();
    m_Object->SetInst().SetExt(v);
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
    m_AssemblyChunk = -1;
    m_Object->SetInst().SetHist(v);;
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
    if ( m_SeqMap ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                     "CBioseq_Info::AttachMap: bioseq already has SeqMap");
    }
    m_SeqMap.Reset(&seq_map);
}


const CSeqMap& CBioseq_Info::GetSeqMap(void) const
{
    const CSeqMap* ret = m_SeqMap.GetPointer();
    if ( !ret ) {
        CFastMutexGuard guard(m_SeqMap_Mtx);
        ret = m_SeqMap.GetPointer();
        if ( !ret ) {
            m_SeqMap = CSeqMap::CreateSeqMapForBioseq(*m_Object);
            ret = m_SeqMap.GetPointer();
            _ASSERT(ret);
        }
    }
    return *ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2005/02/02 21:59:39  vasilche
* Implemented CBioseq_Handle AddId() & RemoveId().
*
* Revision 1.29  2005/01/04 16:31:07  grichenk
* Added IsNa(), IsAa()
*
* Revision 1.28  2004/12/22 15:56:29  vasilche
* Try matching Seq-ids too in x_CalcBioseqLength.
*
* Revision 1.27  2004/10/25 16:53:36  vasilche
* Lookup Bioseq by matching Seq-id.
*
* Revision 1.26  2004/10/18 13:57:04  vasilche
* Added support for split assembly.
*
* Revision 1.25  2004/08/17 15:55:37  vasilche
* Added mapping and unmapping CBioseq -> CBioseq_Info in x_SetObject().
*
* Revision 1.24  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.23  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.22  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.21  2004/05/11 18:05:03  grichenk
* include algorithm
*
* Revision 1.20  2004/05/11 17:45:51  grichenk
* Implemented HasId(), AddId() and RemoveId()
*
* Revision 1.19  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.18  2004/03/31 17:08:07  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.17  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.16  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.15  2003/12/11 17:02:50  grichenk
* Fixed CRef resetting in constructors.
*
* Revision 1.14  2003/11/19 22:18:02  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.13  2003/11/12 16:53:17  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.12  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.11  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.10  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.9  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.8  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.7  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.6  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.5  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
