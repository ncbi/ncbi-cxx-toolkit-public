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


#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_id_mapper.hpp>

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


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(const CBioseq& seq)
{
    x_SetObject(seq);
}


CBioseq_Info::CBioseq_Info(const CBioseq_Info& info)
    : TParent(info),
      m_Object(info.m_Object),
      m_Id(info.m_Id),
      m_Inst(info.m_Inst),
      m_Inst_Repr(info.m_Inst_Repr),
      m_Inst_Mol(info.m_Inst_Mol),
      m_Inst_Length(info.m_Inst_Length),
      m_Inst_Fuzz(info.m_Inst_Fuzz),
      m_Inst_Topology(info.m_Inst_Topology),
      m_Inst_Strand(info.m_Inst_Strand),
      m_Inst_Seq_data(info.m_Inst_Seq_data),
      m_Inst_Ext(info.m_Inst_Ext),
      m_Inst_Hist(info.m_Inst_Hist),
      m_SeqMap(info.m_SeqMap)
{
}


CBioseq_Info::~CBioseq_Info(void)
{
}


const char* CBioseq_Info::x_GetTypeName(void) const
{
    return "Bioseq";
}


const char* CBioseq_Info::x_GetMemberName(TMembers member) const
{
    if ( member & fMember_id ) {
        return "id";
    }
    if ( member & fMember_inst ) {
        return "inst";
    }
    return TParent::x_GetMemberName(member);
}


inline
void CBioseq_Info::x_UpdateObject(CConstRef<TObject> obj)
{
    m_Object = obj;
    if ( HaveDataSource() ) {
        x_DSMapObject(obj, GetDataSource());
    }
    x_ResetModifiedMembers();
}


inline
void CBioseq_Info::x_UpdateModifiedObject(void) const
{
    if ( x_IsModified() ) {
        const_cast<CBioseq_Info*>(this)->x_UpdateObject(x_CreateObject());
    }
}


CConstRef<CBioseq> CBioseq_Info::GetCompleteBioseq(void) const
{
    x_UpdateModifiedObject();
    _ASSERT(!x_IsModified());
    return m_Object;
}


CConstRef<CBioseq> CBioseq_Info::GetBioseqCore(void) const
{
    x_UpdateModifiedObject();
    _ASSERT(!x_IsModified());
    return m_Object;
}


void CBioseq_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    x_DSMapObject(m_Object, ds);
}


void CBioseq_Info::x_DSDetachContents(CDataSource& ds)
{
    ITERATE ( TDSMappedObjects, it, m_DSMappedObjects ) {
        x_DSUnmapObject(*it, ds);
    }
    m_DSMappedObjects.clear();
    TParent::x_DSDetachContents(ds);
}


void CBioseq_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    m_DSMappedObjects.push_back(obj);
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


void CBioseq_Info::x_SetObject(const TObject& obj)
{
    _ASSERT(!m_Object);
    m_Object.Reset(&obj);

    if ( obj.IsSetId() ) {
        ITERATE ( TObject::TId, it, obj.GetId() ) {
            m_Id.push_back(CSeq_id_Handle::GetHandle(**it));
        }
        x_SetSetMembers(fMember_id);
    }
    if ( obj.IsSetDescr() ) {
        x_SetDescr(obj.GetDescr());
    }
    if ( obj.IsSetInst() ) {
        const TInst& inst = obj.GetInst();
        if ( inst.IsSetRepr() ) {
            m_Inst_Repr = inst.GetRepr();
            x_SetSetMembers(fMember_inst_repr);
        }
        if ( inst.IsSetMol() ) {
            m_Inst_Mol = inst.GetMol();
            x_SetSetMembers(fMember_inst_mol);
        }
        if ( inst.IsSetLength() ) {
            m_Inst_Length = inst.GetLength();
            x_SetSetMembers(fMember_inst_length);
        }
        else {
            m_Inst_Length = kInvalidSeqPos;
            LOG_POST(Warning << "CBioseq_Info::x_InitBioseqInfo: "
                     "Seq-inst.length is not set");
        }
        if ( inst.IsSetFuzz() ) {
            m_Inst_Fuzz.Reset(&inst.GetFuzz());
            x_SetSetMembers(fMember_inst_fuzz);
        }
        if ( inst.IsSetTopology() ) {
            m_Inst_Topology = inst.GetTopology();
            x_SetSetMembers(fMember_inst_topology);
        }
        if ( inst.IsSetStrand() ) {
            m_Inst_Strand = inst.GetStrand();
            x_SetSetMembers(fMember_inst_strand);
        }
        if ( inst.IsSetSeq_data() ) {
            m_Inst_Seq_data.Reset(&inst.GetSeq_data());
            x_SetSetMembers(fMember_inst_seq_data);
        }
        if ( inst.IsSetExt() ) {
            m_Inst_Ext.Reset(&inst.GetExt());
            x_SetSetMembers(fMember_inst_ext);
        }
        if ( inst.IsSetHist() ) {
            m_Inst_Hist.Reset(&inst.GetHist());
            x_SetSetMembers(fMember_inst_hist);
        }
        x_SetSetMembers(fMember_inst);
    }
    if ( obj.IsSetAnnot() ) {
        x_SetAnnot(obj.GetAnnot());
    }
}


CRef<CBioseq> CBioseq_Info::x_CreateObject(void) const
{        
    CRef<TObject> obj(new TObject);
    if ( IsSetId() ) {
        TObject::TId& id = obj->SetId();
        if ( x_IsModifiedMember(fMember_id) ) {
            ITERATE ( TId, it, GetId() ) {
                CConstRef<CSeq_id> seq_id = it->GetSeqId();
                id.push_back(Ref(const_cast<CSeq_id*>(&*seq_id)));
            }
        }
        else {
            id = m_Object->GetId();
        }
    }
    if ( IsSetDescr() ) {
        obj->SetDescr(const_cast<TDescr&>(GetDescr()));
    }
    if ( IsSetInst() ) {
        if ( x_IsModifiedMember(fMember_inst_all) ) {
            CRef<TInst> inst = x_CreateInst();
            obj->SetInst(*inst);
        }
        else {
            obj->SetInst(const_cast<TInst&>(GetInst()));
        }
    }
    if ( IsSetAnnot() ) {
        TObject::TAnnot& annot = obj->SetAnnot();
        if ( x_IsModifiedMember(fMember_annot) ) {
            x_FillAnnot(annot);
        }
        else {
            annot = m_Object->GetAnnot();
        }
    }
    return obj;
}


CRef<CSeq_inst> CBioseq_Info::x_CreateInst(void) const
{
    CRef<TInst> obj(new TInst);
    if ( IsSetInst_Repr() ) {
        obj->SetRepr(GetInst_Repr());
    }
    if ( IsSetInst_Mol() ) {
        obj->SetMol(GetInst_Mol());
    }
    if ( IsSetInst_Length() ) {
        obj->SetLength(GetInst_Length());
    }
    if ( IsSetInst_Fuzz() ) {
        obj->SetFuzz(const_cast<TInst_Fuzz&>(GetInst_Fuzz()));
    }
    if ( IsSetInst_Topology() ) {
        obj->SetTopology(GetInst_Topology());
    }
    if ( IsSetInst_Strand() ) {
        obj->SetStrand(GetInst_Strand());
    }
    if ( IsSetInst_Seq_data() ) {
        obj->SetSeq_data(const_cast<TInst_Seq_data&>(GetInst_Seq_data()));
    }
    if ( IsSetInst_Ext() ) {
        obj->SetExt(const_cast<TInst_Ext&>(GetInst_Ext()));
    }
    if ( IsSetInst_Hist() ) {
        obj->SetHist(const_cast<TInst_Hist&>(GetInst_Hist()));
    }
    return obj;
}


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
        GetTSE_Info().FindBioseq(CSeq_id_Handle::GetHandle(whole));
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
