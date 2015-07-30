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
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bio_object_id.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_edit_commands.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/seq_loc_mapper.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CBioseq_Handle
/////////////////////////////////////////////////////////////////////////////

CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& id,
                               const CBioseq_ScopeInfo& binfo)
    : m_Handle_Seq_id(id),
      m_Info(const_cast<CBioseq_ScopeInfo&>(binfo).GetLock(null))
{
}


CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& id,
                               const TLock& lock)
    : m_Handle_Seq_id(id),
      m_Info(lock)
{
}


void CBioseq_Handle::Reset(void)
{
    m_Info.Reset();
    m_Handle_Seq_id.Reset();
}


CBioseq_Handle::TBioseqStateFlags CBioseq_Handle::GetState(void) const
{
    if ( !m_Info ) {
        return fState_no_data;
    }
    TBioseqStateFlags state = x_GetScopeInfo().GetBlobState();
    if ( m_Info->HasBioseq() ) {
        state |= m_Info->GetTSE_Handle().x_GetTSE_Info().GetBlobState();
    }
    if ( state == 0 && !*this ) {
        state |= fState_not_found;
    }
    return state;
}


const CBioseq_ScopeInfo& CBioseq_Handle::x_GetScopeInfo(void) const
{
    return *m_Info;
}


const CBioseq_Info& CBioseq_Handle::x_GetInfo(void) const
{
    return m_Info->GetObjectInfo();
}


CConstRef<CSeq_id> CBioseq_Handle::GetInitialSeqIdOrNull(void) const
{
    return GetSeq_id_Handle().GetSeqIdOrNull();
}

CConstRef<CSeq_id> CBioseq_Handle::GetSeqId(void) const
{
    return GetAccessSeq_id_Handle().GetSeqId();
}

const CBioObjectId& CBioseq_Handle::GetBioObjectId(void) const
{
    return x_GetInfo().GetBioObjectId();
}

CConstRef<CBioseq> CBioseq_Handle::GetCompleteBioseq(void) const
{
    return x_GetInfo().GetCompleteBioseq();
}


CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetInfo().GetBioseqCore();
}


CBioseq_EditHandle CBioseq_Handle::GetEditHandle(void) const
{
    return x_GetScopeImpl().GetEditHandle(*this);
}


/////////////////////////////////////////////////////////////////////////////
// Bioseq members

bool CBioseq_Handle::IsSetId(void) const
{
    return x_GetInfo().IsSetId();
}


bool CBioseq_Handle::CanGetId(void) const
{
    return *this  &&  x_GetInfo().CanGetId();
}


const CBioseq_Handle::TId& CBioseq_Handle::GetId(void) const
{
    return x_GetInfo().GetId();
}


bool CBioseq_Handle::IsSetDescr(void) const
{
    return x_GetInfo().IsSetDescr();
}


bool CBioseq_Handle::CanGetDescr(void) const
{
    return *this  &&  x_GetInfo().CanGetDescr();
}


const CSeq_descr& CBioseq_Handle::GetDescr(void) const
{
    return x_GetInfo().GetDescr();
}


bool CBioseq_Handle::IsSetInst(void) const
{
    return x_GetInfo().IsSetInst();
}


bool CBioseq_Handle::CanGetInst(void) const
{
    return *this  &&  x_GetInfo().CanGetInst();
}


const CSeq_inst& CBioseq_Handle::GetInst(void) const
{
    return x_GetInfo().GetInst();
}


bool CBioseq_Handle::IsSetInst_Repr(void) const
{
    return x_GetInfo().IsSetInst_Repr();
}


bool CBioseq_Handle::CanGetInst_Repr(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Repr();
}


CBioseq_Handle::TInst_Repr CBioseq_Handle::GetInst_Repr(void) const
{
    return x_GetInfo().GetInst_Repr();
}


bool CBioseq_Handle::IsSetInst_Mol(void) const
{
    return x_GetInfo().IsSetInst_Mol();
}


bool CBioseq_Handle::CanGetInst_Mol(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Mol();
}


CBioseq_Handle::TInst_Mol CBioseq_Handle::GetInst_Mol(void) const
{
    return x_GetInfo().GetInst_Mol();
}


bool CBioseq_Handle::IsSetInst_Length(void) const
{
    return x_GetInfo().IsSetInst_Length();
}


bool CBioseq_Handle::CanGetInst_Length(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Length();
}


CBioseq_Handle::TInst_Length CBioseq_Handle::GetInst_Length(void) const
{
    return x_GetInfo().GetInst_Length();
}


TSeqPos CBioseq_Handle::GetBioseqLength(void) const
{
    if ( IsSetInst_Length() ) {
        return GetInst_Length();
    }
    else {
        return GetSeqMap().GetLength(&GetScope());
    }
}


bool CBioseq_Handle::IsSetInst_Fuzz(void) const
{
    return x_GetInfo().IsSetInst_Fuzz();
}


bool CBioseq_Handle::CanGetInst_Fuzz(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Fuzz();
}


const CBioseq_Handle::TInst_Fuzz& CBioseq_Handle::GetInst_Fuzz(void) const
{
    return x_GetInfo().GetInst_Fuzz();
}


bool CBioseq_Handle::IsSetInst_Topology(void) const
{
    return x_GetInfo().IsSetInst_Topology();
}


bool CBioseq_Handle::CanGetInst_Topology(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Topology();
}


CBioseq_Handle::TInst_Topology CBioseq_Handle::GetInst_Topology(void) const
{
    return x_GetInfo().GetInst_Topology();
}


bool CBioseq_Handle::IsSetInst_Strand(void) const
{
    return x_GetInfo().IsSetInst_Strand();
}


bool CBioseq_Handle::CanGetInst_Strand(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Strand();
}


CBioseq_Handle::TInst_Strand CBioseq_Handle::GetInst_Strand(void) const
{
    return x_GetInfo().GetInst_Strand();
}


bool CBioseq_Handle::IsSetInst_Seq_data(void) const
{
    return x_GetInfo().IsSetInst_Seq_data();
}


bool CBioseq_Handle::CanGetInst_Seq_data(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Seq_data();
}


const CBioseq_Handle::TInst_Seq_data&
CBioseq_Handle::GetInst_Seq_data(void) const
{
    return x_GetInfo().GetInst_Seq_data();
}


bool CBioseq_Handle::IsSetInst_Ext(void) const
{
    return x_GetInfo().IsSetInst_Ext();
}


bool CBioseq_Handle::CanGetInst_Ext(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Ext();
}


const CBioseq_Handle::TInst_Ext& CBioseq_Handle::GetInst_Ext(void) const
{
    return x_GetInfo().GetInst_Ext();
}


bool CBioseq_Handle::IsSetInst_Hist(void) const
{
    return x_GetInfo().IsSetInst_Hist();
}


bool CBioseq_Handle::CanGetInst_Hist(void) const
{
    return *this  &&  x_GetInfo().CanGetInst_Hist();
}


const CBioseq_Handle::TInst_Hist& CBioseq_Handle::GetInst_Hist(void) const
{
    return x_GetInfo().GetInst_Hist();
}


bool CBioseq_Handle::HasAnnots(void) const
{
    return x_GetInfo().HasAnnots();
}


// end of Bioseq members
/////////////////////////////////////////////////////////////////////////////


CSeq_inst::TMol CBioseq_Handle::GetBioseqMolType(void) const
{
    return GetSequenceType();
}


bool CBioseq_Handle::IsNa(void) const
{
    return IsNucleotide();
}


bool CBioseq_Handle::IsAa(void) const
{
    return IsProtein();
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetInfo().GetSeqMap();
}


bool CBioseq_Handle::ContainsSegment(const CSeq_id& id,
                                     size_t resolve_depth,
                                     EFindSegment limit_flag) const
{
    return ContainsSegment(CSeq_id_Handle::GetHandle(id),
                           resolve_depth,
                           limit_flag);
}


bool CBioseq_Handle::ContainsSegment(const CBioseq_Handle& part,
                                     size_t resolve_depth,
                                     EFindSegment limit_flag) const
{
    CConstRef<CSynonymsSet> syns = part.GetSynonyms();
    if ( !syns ) {
        return false;
    }
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindRef);
    if ( limit_flag == eFindSegment_LimitTSE ) {
        sel.SetLimitTSE(GetTopLevelEntry());
    }
    sel.SetResolveCount(resolve_depth);
    CSeqMap_CI it = GetSeqMap().BeginResolved(&GetScope(), sel);
    for ( ; it; ++it) {
        if ( syns->ContainsSynonym(it.GetRefSeqid()) ) {
            return true;
        }
    }
    return false;
}


bool CBioseq_Handle::ContainsSegment(CSeq_id_Handle id,
                                     size_t resolve_depth,
                                     EFindSegment limit_flag) const
{
    CBioseq_Handle h = GetScope().GetBioseqHandle(id);
    CConstRef<CSynonymsSet> syns;
    if ( h ) {
        syns = h.GetSynonyms();
    }
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindRef);
    if ( limit_flag == eFindSegment_LimitTSE ) {
        sel.SetLimitTSE(GetTopLevelEntry());
    }
    sel.SetResolveCount(resolve_depth);
    CSeqMap_CI it = GetSeqMap().BeginResolved(&GetScope(), sel);
    for ( ; it; ++it) {
        if ( syns ) {
            if ( syns->ContainsSynonym(it.GetRefSeqid()) ) {
                return true;
            }
        }
        else {
            if (it.GetRefSeqid() == id) {
                return true;
            }
        }
    }
    return false;
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        ENa_strand strand) const
{
    return CSeqVector(*this, coding, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(ENa_strand strand) const
{
    return CSeqVector(*this, eCoding_Ncbi, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        EVectorStrand strand) const
{
    return CSeqVector(*this, coding,
                      strand == eStrand_Minus?
                      eNa_strand_minus: eNa_strand_plus);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorStrand strand) const
{
    return CSeqVector(*this, eCoding_Ncbi,
                      strand == eStrand_Minus?
                      eNa_strand_minus: eNa_strand_plus);
}


CConstRef<CSynonymsSet> CBioseq_Handle::GetSynonyms(void) const
{
    if ( !*this ) {
        return CConstRef<CSynonymsSet>();
    }
    return GetScope().GetSynonyms(*this);
}


bool CBioseq_Handle::IsSynonym(const CSeq_id& id) const
{
    return IsSynonym(CSeq_id_Handle::GetHandle(id));
}


bool CBioseq_Handle::IsSynonym(const CSeq_id_Handle& idh) const
{
    CConstRef<CSynonymsSet> syns = GetSynonyms();
    return syns && syns->ContainsSynonym(idh);
}


CSeq_entry_Handle CBioseq_Handle::GetTopLevelEntry(void) const
{
    return CSeq_entry_Handle(GetTSE_Handle());
}


CSeq_entry_Handle CBioseq_Handle::GetParentEntry(void) const
{
    CSeq_entry_Handle ret;
    if ( *this ) {
        ret = CSeq_entry_Handle(x_GetInfo().GetParentSeq_entry_Info(),
                                GetTSE_Handle());
    }
    return ret;
}


CSeq_entry_Handle CBioseq_Handle::GetSeq_entry_Handle(void) const
{
    return GetParentEntry();
}


CBioseq_set_Handle CBioseq_Handle::GetParentBioseq_set(void) const
{
    CBioseq_set_Handle ret;
    const CBioseq_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        const CSeq_entry_Info& entry = info.GetParentSeq_entry_Info();
        if ( entry.HasParent_Info() ) {
            ret = CBioseq_set_Handle(entry.GetParentBioseq_set_Info(),
                                     GetTSE_Handle());
        }
    }
    return ret;
}


CSeq_entry_Handle
CBioseq_Handle::GetComplexityLevel(CBioseq_set::EClass cls) const
{
    const CBioseq_set_Handle::TComplexityTable& ctab =
        CBioseq_set_Handle::sx_GetComplexityTable();
    if (cls == CBioseq_set::eClass_other) {
        // adjust 255 to the correct value
        cls = CBioseq_set::EClass(sizeof(ctab) - 1);
    }
    CSeq_entry_Handle last = GetParentEntry();
    _ASSERT(last && last.IsSeq());
    CSeq_entry_Handle e = last.GetParentEntry();
    while ( e ) {
        _ASSERT(e.IsSet());
        // Found good level
        if ( last.IsSet()  &&
             ctab[last.GetSet().GetClass()] == ctab[cls] )
            break;
        // Gone too high
        if ( ctab[e.GetSet().GetClass()] > ctab[cls] ) {
            break;
        }
        // Go up one level
        last = e;
        e = e.GetParentEntry();
    }
    return last;
}


CSeq_entry_Handle
CBioseq_Handle::GetExactComplexityLevel(CBioseq_set::EClass cls) const
{
    CSeq_entry_Handle ret = GetComplexityLevel(cls);
    if ( ret  &&
         (!ret.IsSet()  ||  !ret.GetSet().IsSetClass()  ||
         ret.GetSet().GetClass() != cls) ) {
        ret.Reset();
    }
    return ret;
}


CRef<CSeq_loc> CBioseq_Handle::MapLocation(const CSeq_loc& loc) const
{
    CSeq_loc_Mapper mapper(*this, CSeq_loc_Mapper::eSeqMap_Up);
    return mapper.Map(loc);
}


CBioseq_EditHandle
CBioseq_Handle::CopyTo(const CSeq_entry_EditHandle& entry,
                       int index) const
{
    return entry.CopyBioseq(*this, index);
}


CBioseq_EditHandle
CBioseq_Handle::CopyTo(const CBioseq_set_EditHandle& seqset,
                       int index) const
{
    return seqset.CopyBioseq(*this, index);
}


CBioseq_EditHandle
CBioseq_Handle::CopyToSeq(const CSeq_entry_EditHandle& entry) const
{
    return entry.CopySeq(*this);
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_EditHandle
/////////////////////////////////////////////////////////////////////////////

CBioseq_EditHandle::CBioseq_EditHandle(void)
{
}


CBioseq_EditHandle::CBioseq_EditHandle(const CBioseq_Handle& h)
    : CBioseq_Handle(h)
{
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "object is not in editing mode");
    }
}


CBioseq_EditHandle::CBioseq_EditHandle(const CSeq_id_Handle& id,
                                       CBioseq_ScopeInfo& binfo)
    : CBioseq_Handle(id, binfo)
{
}


CBioseq_EditHandle::CBioseq_EditHandle(const CSeq_id_Handle& id,
                                       const TLock& lock)
    : CBioseq_Handle(id, lock)
{
}


CSeq_entry_EditHandle CBioseq_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    if ( *this ) {
        ret = CSeq_entry_EditHandle(x_GetInfo().GetParentSeq_entry_Info(),
                                    GetTSE_Handle());
    }
    return ret;
}


CSeq_annot_EditHandle
CBioseq_EditHandle::AttachAnnot(CSeq_annot& annot) const
{
    return GetParentEntry().AttachAnnot(annot);
}


CSeq_annot_EditHandle
CBioseq_EditHandle::CopyAnnot(const CSeq_annot_Handle& annot) const
{
    return GetParentEntry().CopyAnnot(annot);
}


CSeq_annot_EditHandle
CBioseq_EditHandle::TakeAnnot(const CSeq_annot_EditHandle& annot) const
{
    return GetParentEntry().TakeAnnot(annot);
}


CBioseq_EditHandle
CBioseq_EditHandle::MoveTo(const CSeq_entry_EditHandle& entry,
                           int index) const
{
    return entry.TakeBioseq(*this, index);
}


CBioseq_EditHandle
CBioseq_EditHandle::MoveTo(const CBioseq_set_EditHandle& seqset,
                           int index) const
{
    return seqset.TakeBioseq(*this, index);
}


CBioseq_EditHandle
CBioseq_EditHandle::MoveToSeq(const CSeq_entry_EditHandle& entry) const
{
    return entry.TakeSeq(*this);
}

void CBioseq_EditHandle::Remove(CBioseq_EditHandle::ERemoveMode mode) const
{
    if (mode == eKeepSeq_entry) 
        x_Detach();
    else {
        CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
        CSeq_entry_EditHandle parent = GetParentEntry();
        x_Detach();
        parent.Remove();
        tr->Commit();
    }
}

void CBioseq_EditHandle::x_Detach(void) const
{
    typedef CRemoveBioseq_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, x_GetScopeImpl()));
    //    x_GetScopeImpl().RemoveBioseq(*this);
}


/////////////////////////////////////////////////////////////////////////////
// Bioseq members

void CBioseq_EditHandle::ResetId(void) const
{
    CScope_Impl::TConfWriteLockGuard guard(x_GetScopeImpl().m_ConfLock);
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new CResetIds_EditCommand(*this));
}


bool CBioseq_EditHandle::AddId(const CSeq_id_Handle& id) const
{
    CScope_Impl::TConfWriteLockGuard guard(x_GetScopeImpl().m_ConfLock);
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new CAddId_EditCommand(*this,id));
}


bool CBioseq_EditHandle::RemoveId(const CSeq_id_Handle& id) const
{
    CScope_Impl::TConfWriteLockGuard guard(x_GetScopeImpl().m_ConfLock);
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new CRemoveId_EditCommand(*this,id));
}


void CBioseq_EditHandle::ResetDescr(void) const
{
    typedef CResetValue_EditCommand<CBioseq_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
}


void CBioseq_EditHandle::SetDescr(TDescr& v) const
{
    typedef CSetValue_EditCommand<CBioseq_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
}


CBioseq_EditHandle::TDescr& CBioseq_EditHandle::SetDescr(void) const
{
    if (x_GetScopeImpl().IsTransactionActive() 
        || GetTSE_Handle().x_GetTSE_Info().GetEditSaver() ) {
        NCBI_THROW(CObjMgrException, eTransaction,
                       "TDescr& CBioseq_EditHandle::SetDescr(): "
                       "method can not be called if a transaction is required");
    }
    return x_GetInfo().SetDescr();
}


bool CBioseq_EditHandle::AddSeqdesc(CSeqdesc& d) const
{
    typedef CDesc_EditCommand<CBioseq_EditHandle,true> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, d));
}


CRef<CSeqdesc> CBioseq_EditHandle::RemoveSeqdesc(const CSeqdesc& d) const
{
    typedef CDesc_EditCommand<CBioseq_EditHandle,false> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, d));
}


void CBioseq_EditHandle::AddSeq_descr(TDescr& v) const
{
    typedef CAddDescr_EditCommand<CBioseq_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().AddSeq_descr(v);
}


void CBioseq_EditHandle::SetInst(TInst& v) const
{
    typedef CSet_SeqInst_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst(v);
}


void CBioseq_EditHandle::SetInst_Repr(TInst_Repr v) const
{
    typedef CSet_SeqInstRepr_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Repr(v);
}


void CBioseq_EditHandle::SetInst_Mol(TInst_Mol v) const
{
    typedef CSet_SeqInstMol_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Mol(v);
}


void CBioseq_EditHandle::SetInst_Length(TInst_Length v) const
{
    typedef CSet_SeqInstLength_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Length(v);
}


void CBioseq_EditHandle::SetInst_Fuzz(TInst_Fuzz& v) const
{
    typedef CSet_SeqInstFuzz_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Fuzz(v);
}


void CBioseq_EditHandle::SetInst_Topology(TInst_Topology v) const
{
    typedef CSet_SeqInstTopology_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Topology(v);
}


void CBioseq_EditHandle::SetInst_Strand(TInst_Strand v) const
{
    typedef CSet_SeqInstStrand_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Strand(v);
}


void CBioseq_EditHandle::SetInst_Seq_data(TInst_Seq_data& v) const
{
    typedef CSet_SeqInstSeq_data_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Seq_data(v);
}


void CBioseq_EditHandle::SetInst_Ext(TInst_Ext& v) const
{
    typedef CSet_SeqInstExt_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Ext(v);
}


void CBioseq_EditHandle::SetInst_Hist(TInst_Hist& v) const
{
    typedef CSet_SeqInstHist_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetInst_Hist(v);
}


CSeq_id_Handle CBioseq_Handle::GetAccessSeq_id_Handle(void) const
{
    CSeq_id_Handle id = GetSeq_id_Handle();
    // First try original id
    if ( id ) {
        return id;
    }
    // Then try to find gi
    ITERATE ( TId, it, GetId() ) {
        if ( it->IsGi() ) {
            CBioseq_Handle bh =
                GetScope().GetBioseqHandleFromTSE(*it, GetTSE_Handle());
            if ( bh == *this ) {
                id = *it;
                _ASSERT(id);
                return id;
            }
        }
    }
    // Then try to find accession
    ITERATE ( TId, it, GetId() ) {
        if ( !it->IsGi() && it->GetSeqId()->GetTextseq_Id() ) {
            CBioseq_Handle bh =
                GetScope().GetBioseqHandleFromTSE(*it, GetTSE_Handle());
            if ( bh == *this ) {
                id = *it;
                _ASSERT(id);
                return id;
            }
        }
    }
    // Then try to find any other id
    ITERATE ( TId, it, GetId() ) {
        if ( !it->IsGi() && !it->GetSeqId()->GetTextseq_Id() ) {
            CBioseq_Handle bh =
                GetScope().GetBioseqHandleFromTSE(*it, GetTSE_Handle());
            if ( bh == *this ) {
                id = *it;
                _ASSERT(id);
                return id;
            }
        }
    }
    NCBI_THROW(CObjMgrException, eOtherError,
               "CBioseq_Handle::GetAccessSeq_id_Handle "
               "can not find seq-id to access this bioseq");
}


CRef<CSeq_loc> CBioseq_Handle::GetRangeSeq_loc(TSeqPos start,
                                               TSeqPos stop,
                                               ENa_strand strand) const
{
    CSeq_id_Handle orig_id = GetAccessSeq_id_Handle();
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*orig_id.GetSeqId());
    CRef<CSeq_loc> res(new CSeq_loc);
    if (start == 0  &&  stop == 0) {
        if ( strand == eNa_strand_unknown ) {
            res->SetWhole(*id);
        }
        else {
            CRef<CSeq_interval> interval
                (new CSeq_interval(*id, 0, GetBioseqLength()-1, strand));
            res->SetInt(*interval);
        }
    }
    else {
        CRef<CSeq_interval> interval(new CSeq_interval
                                     (*id, start, stop, strand));
        res->SetInt(*interval);
    }
    return res;
}


bool CBioseq_Handle::AddUsedBioseq(const CBioseq_Handle& bh) const
{
    return GetTSE_Handle().AddUsedTSE(bh.GetTSE_Handle());
}


///////////////////////////////////////////////////////////////////////////////
void CBioseq_EditHandle::x_RealResetDescr(void) const
{
    x_GetInfo().ResetDescr();
}


void CBioseq_EditHandle::x_RealSetDescr(TDescr& v) const
{
    x_GetInfo().SetDescr(v);
}


bool CBioseq_EditHandle::x_RealAddSeqdesc(CSeqdesc& d) const
{
    return x_GetInfo().AddSeqdesc(d);
}


CRef<CSeqdesc> CBioseq_EditHandle::x_RealRemoveSeqdesc(const CSeqdesc& d) const
{
    return x_GetInfo().RemoveSeqdesc(d);
}


void CBioseq_EditHandle::x_RealAddSeq_descr(TDescr& v) const
{
    x_GetInfo().AddSeq_descr(v);
}

void CBioseq_EditHandle::x_RealResetId(void) const
{
    x_GetScopeInfo().ResetId();
}


bool CBioseq_EditHandle::x_RealAddId(const CSeq_id_Handle& id) const
{
    return x_GetScopeInfo().AddId(id);
}

bool CBioseq_EditHandle::x_RealRemoveId(const CSeq_id_Handle& id) const
{
    return x_GetScopeInfo().RemoveId(id);
}


void CBioseq_EditHandle::x_RealSetInst(TInst& v) const
{
    x_GetInfo().SetInst(v);
}


void CBioseq_EditHandle::x_RealSetInst_Repr(TInst_Repr v) const
{
    x_GetInfo().SetInst_Repr(v);
}


void CBioseq_EditHandle::x_RealSetInst_Mol(TInst_Mol v) const
{
    x_GetInfo().SetInst_Mol(v);
}


void CBioseq_EditHandle::x_RealSetInst_Length(TInst_Length v) const
{
    x_GetInfo().SetInst_Length(v);
}


void CBioseq_EditHandle::x_RealSetInst_Fuzz(TInst_Fuzz& v) const
{
    x_GetInfo().SetInst_Fuzz(v);
}


void CBioseq_EditHandle::x_RealSetInst_Topology(TInst_Topology v) const
{
    x_GetInfo().SetInst_Topology(v);
}


void CBioseq_EditHandle::x_RealSetInst_Strand(TInst_Strand v) const
{
    x_GetInfo().SetInst_Strand(v);
}


void CBioseq_EditHandle::x_RealSetInst_Seq_data(TInst_Seq_data& v) const
{
    x_GetInfo().SetInst_Seq_data(v);
}


void CBioseq_EditHandle::x_RealSetInst_Ext(TInst_Ext& v) const
{
    x_GetInfo().SetInst_Ext(v);
}


void CBioseq_EditHandle::x_RealSetInst_Hist(TInst_Hist& v) const
{
    x_GetInfo().SetInst_Hist(v);
}
void CBioseq_EditHandle::x_RealResetInst() const
{
    x_GetInfo().ResetInst();
}
void CBioseq_EditHandle::x_RealResetInst_Repr() const
{
    x_GetInfo().ResetInst_Repr();
}
void CBioseq_EditHandle::x_RealResetInst_Mol() const
{
    x_GetInfo().ResetInst_Mol();
}
void CBioseq_EditHandle::x_RealResetInst_Length() const
{
    x_GetInfo().ResetInst_Length();
}
void CBioseq_EditHandle::x_RealResetInst_Fuzz() const
{
    x_GetInfo().ResetInst_Fuzz();
}
void CBioseq_EditHandle::x_RealResetInst_Topology() const
{
    x_GetInfo().ResetInst_Topology();
}
void CBioseq_EditHandle::x_RealResetInst_Strand() const
{
    x_GetInfo().ResetInst_Strand();
}
void CBioseq_EditHandle::x_RealResetInst_Seq_data() const
{
    x_GetInfo().ResetInst_Seq_data();
}
void CBioseq_EditHandle::x_RealResetInst_Ext() const
{
    x_GetInfo().ResetInst_Ext();
}
void CBioseq_EditHandle::x_RealResetInst_Hist() const
{
    x_GetInfo().ResetInst_Hist();
}


CSeqMap& CBioseq_EditHandle::SetSeqMap(void) const
{
    return const_cast<CSeqMap&>(GetSeqMap());
}


CBioseq_Handle::EFeatureFetchPolicy
CBioseq_Handle::GetFeatureFetchPolicy(void) const
{
    return EFeatureFetchPolicy(x_GetInfo().GetFeatureFetchPolicy());
}


// end of Bioseq members
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE
