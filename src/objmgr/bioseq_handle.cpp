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

#include <objmgr/impl/scope_impl.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/seqmatch_info.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/seq_loc_mapper.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CBioseq_Handle
/////////////////////////////////////////////////////////////////////////////

CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& id,
                               CBioseq_ScopeInfo* bioseq_info)
    : m_Seq_id(id),
      m_Bioseq_Info(bioseq_info)
{
}


bool CBioseq_Handle::operator==(const CBioseq_Handle& h) const
{
    if ( m_Scope != m_Scope ) {
        return false;
    }
    if ( *this && h )
        return &x_GetInfo() == &h.x_GetInfo();
    // Compare by id key
    return GetSeq_id_Handle() == h.GetSeq_id_Handle();
}


bool CBioseq_Handle::operator< (const CBioseq_Handle& h) const
{
    if ( m_Scope != h.m_Scope ) {
        return m_Scope < h.m_Scope;
    }
    if ( *this && h )
        return &x_GetInfo() < &h.x_GetInfo();
    return GetSeq_id_Handle() < h.GetSeq_id_Handle();
}


const CBioseq_Info& CBioseq_Handle::x_GetInfo(void) const
{
    return x_GetScopeInfo().GetBioseq_Info();
}


CConstRef<CSeq_id> CBioseq_Handle::GetSeqId(void) const
{
    return GetSeq_id_Handle().GetSeqIdOrNull();
}


CConstRef<CBioseq> CBioseq_Handle::GetCompleteBioseq(void) const
{
    return x_GetInfo().GetCompleteBioseq();
}


const CBioseq& CBioseq_Handle::GetBioseq(void) const
{
    ERR_POST_ONCE(Warning<<
                  "CBioseq_Handle::GetBioseq() is deprecated, "
                  "use GetCompleteBioseq() or GetBioseqCore().");
    return *GetCompleteBioseq();
}


const CSeq_entry& CBioseq_Handle::GetTopLevelSeqEntry(void) const
{
    ERR_POST_ONCE(Warning<<
                  "CBioseq_Handle::GetTopLevelSeqEntry() is deprecated, "
                  "use GetTopLevelEntry().");
    return *GetTopLevelEntry().GetCompleteSeq_entry();
}


CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetInfo().GetBioseqCore();
}


CBioseq_EditHandle CBioseq_Handle::GetEditHandle(void) const
{
    return m_Scope->GetEditHandle(*this);
}


/////////////////////////////////////////////////////////////////////////////
// Bioseq members

bool CBioseq_Handle::IsSetId(void) const
{
    return x_GetInfo().IsSetId();
}


bool CBioseq_Handle::CanGetId(void) const
{
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetId();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetDescr();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Repr();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Mol();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Length();
}


CBioseq_Handle::TInst_Length CBioseq_Handle::GetInst_Length(void) const
{
    return x_GetInfo().GetInst_Length();
}


TSeqPos CBioseq_Handle::GetBioseqLength(void) const
{
    return x_GetInfo().GetBioseqLength();
}


bool CBioseq_Handle::IsSetInst_Fuzz(void) const
{
    return x_GetInfo().IsSetInst_Fuzz();
}


bool CBioseq_Handle::CanGetInst_Fuzz(void) const
{
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Fuzz();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Topology();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Strand();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Seq_data();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Ext();
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
    return bool(m_Bioseq_Info)  &&  x_GetInfo().CanGetInst_Hist();
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
    return x_GetInfo().GetInst_Mol();
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetInfo().GetSeqMap();
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        ENa_strand strand) const
{
    return CSeqVector(GetSeqMap(), GetScope(), coding, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(ENa_strand strand) const
{
    return CSeqVector(GetSeqMap(), GetScope(), eCoding_Ncbi, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        EVectorStrand strand) const
{
    return CSeqVector(GetSeqMap(), GetScope(), coding,
                      strand == eStrand_Minus?
                      eNa_strand_minus: eNa_strand_plus);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorStrand strand) const
{
    return CSeqVector(GetSeqMap(), GetScope(), eCoding_Ncbi,
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
    CConstRef<CSynonymsSet> syns = GetSynonyms();
    return bool(syns) && syns->ContainsSynonym(CSeq_id_Handle::GetHandle(id));
}


CSeqVector CBioseq_Handle::GetSequenceView(const CSeq_loc& location,
                                           ESequenceViewMode mode,
                                           EVectorCoding coding,
                                           ENa_strand strand) const
{
    if ( mode != eViewConstructed )
        strand = eNa_strand_unknown;
    return CSeqVector(GetSeqMapByLocation(location, mode), GetScope(),
                      coding, strand);
}


CConstRef<CSeqMap>
CBioseq_Handle::GetSeqMapByLocation(const CSeq_loc& loc,
                                    ESequenceViewMode mode) const
{
    CConstRef<CSeqMap> ret;
    if ( mode == eViewConstructed ) {
        ret = CSeqMap::CreateSeqMapForSeq_loc(loc, &GetScope());
    }
    else {
        // Parse the location
        CHandleRange rlist;      // all intervals pointing to the sequence
        CSeq_loc_CI loc_it(loc);
        for ( ; loc_it; ++loc_it) {
            if ( !IsSynonym(loc_it.GetSeq_id()) )
                continue;
            rlist.AddRange(loc_it.GetRange(), loc_it.GetStrand());
        }

        // Make mode-dependent parsing of the range list
        CHandleRange mode_rlist; // processed intervals (merged, excluded)
        switch (mode) {
        case eViewMerged:
        {
            // Merge intervals from "rlist"
            ITERATE (CHandleRange, rit, rlist) {
                mode_rlist.MergeRange(rit->first, rit->second);
            }
            break;
        }
        case eViewExcluded:
        {
            // Exclude intervals from "rlist"
            TSeqPos last_from = 0;
            ITERATE (CHandleRange, rit, rlist) {
                if (last_from < rit->first.GetFrom()) {
                    mode_rlist.MergeRange(
                        CHandleRange::TRange(last_from, rit->first.GetFrom()-1),
                        eNa_strand_unknown);
                }
                if ( !rit->first.IsWholeTo() ) {
                    last_from = rit->first.GetTo()+1;
                }
                else {
                    last_from = CHandleRange::TRange::GetWholeTo();
                }
            }
            TSeqPos total_length = GetSeqMap().GetLength(&GetScope());
            if (last_from < total_length) {
                mode_rlist.MergeRange(
                    CHandleRange::TRange(last_from, total_length-1),
                    eNa_strand_unknown);
            }
            break;
        }
        default:
            break;
        }

        // Convert ranges to seq-loc
        CRef<CSeq_loc> view_loc(new CSeq_loc);
        ITERATE (CHandleRange, rit, mode_rlist) {
            CRef<CSeq_loc> seg_loc(new CSeq_loc);
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(*GetSeqId());
            seg_loc->SetInt().SetId(*id);
            seg_loc->SetInt().SetFrom(rit->first.GetFrom());
            seg_loc->SetInt().SetTo(rit->first.GetTo());
            view_loc->SetMix().Set().push_back(seg_loc);
        }
        ret = CSeqMap::CreateSeqMapForSeq_loc(*view_loc, &GetScope());
    }
    return ret;
}


CSeq_entry_Handle CBioseq_Handle::GetTopLevelEntry(void) const
{
    return CSeq_entry_Handle(GetScope(),
                             x_GetInfo().GetTSE_Info());
}


CSeq_entry_Handle CBioseq_Handle::GetParentEntry(void) const
{
    return CSeq_entry_Handle(m_Scope,
                             x_GetInfo().GetParentSeq_entry_Info());
}


CSeq_entry_Handle CBioseq_Handle::GetSeq_entry_Handle(void) const
{
    return GetParentEntry();
}



static int s_Complexity[] = {
    0, // not-set (0)
    3, // nuc-prot (1)
    2, // segset (2)
    2, // conset (3)
    1, // parts (4)
    1, // gibb (5)
    1, // gi (6)
    5, // genbank (7)
    3, // pir (8)
    4, // pub-set (9)
    4, // equiv (10)
    3, // swissprot (11)
    3, // pdb-entry (12)
    4, // mut-set (13)
    4, // pop-set (14)
    4, // phy-set (15)
    4, // eco-set (16)
    4, // gen-prod-set (17)
    4, // wgs-set (18)
    0, // other (255 - processed separately)
};


CSeq_entry_Handle
CBioseq_Handle::GetComplexityLevel(CBioseq_set::EClass cls) const
{
    if (cls == CBioseq_set::eClass_other) {
        // adjust 255 to the correct value
        cls = CBioseq_set::EClass(sizeof(s_Complexity) - 1);
    }
    CSeq_entry_Handle last = GetParentEntry();
    _ASSERT(last && last.IsSeq());
    CSeq_entry_Handle e = last.GetParentEntry();
    while ( e ) {
        _ASSERT(e.IsSet());
        // Found good level
        if ( last.IsSet()  &&
             s_Complexity[last.GetSet().GetClass()] == s_Complexity[cls] )
            break;
        // Gone too high
        if ( s_Complexity[e.GetSet().GetClass()] > s_Complexity[cls] ) {
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
    CSeq_loc_Mapper mapper(*this);
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
}


CSeq_entry_EditHandle CBioseq_EditHandle::GetParentEntry(void) const
{
    return CSeq_entry_EditHandle(m_Scope,
                                 x_GetInfo().GetParentSeq_entry_Info());
}


CSeq_annot_EditHandle
CBioseq_EditHandle::AttachAnnot(const CSeq_annot& annot) const
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


void CBioseq_EditHandle::Remove(void) const
{
    m_Scope->RemoveBioseq(*this);
}


/////////////////////////////////////////////////////////////////////////////
// Bioseq members

void CBioseq_EditHandle::SetDescr(TDescr& v) const
{
    x_GetInfo().SetDescr(v);
}


void CBioseq_EditHandle::SetInst(TInst& v) const
{
    x_GetInfo().SetInst(v);
}


void CBioseq_EditHandle::SetInst_Repr(TInst_Repr v) const
{
    x_GetInfo().SetInst_Repr(v);
}


void CBioseq_EditHandle::SetInst_Mol(TInst_Mol v) const
{
    x_GetInfo().SetInst_Mol(v);
}


void CBioseq_EditHandle::SetInst_Length(TInst_Length v) const
{
    x_GetInfo().SetInst_Length(v);
}


void CBioseq_EditHandle::SetInst_Fuzz(TInst_Fuzz& v) const
{
    x_GetInfo().SetInst_Fuzz(v);
}


void CBioseq_EditHandle::SetInst_Topology(TInst_Topology v) const
{
    x_GetInfo().SetInst_Topology(v);
}


void CBioseq_EditHandle::SetInst_Strand(TInst_Strand v) const
{
    x_GetInfo().SetInst_Strand(v);
}


void CBioseq_EditHandle::SetInst_Seq_data(TInst_Seq_data& v) const
{
    x_GetInfo().SetInst_Seq_data(v);
}


void CBioseq_EditHandle::SetInst_Ext(TInst_Ext& v) const
{
    x_GetInfo().SetInst_Ext(v);
}


void CBioseq_EditHandle::SetInst_Hist(TInst_Hist& v) const
{
    x_GetInfo().SetInst_Hist(v);
}



// end of Bioseq members
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.66  2004/05/27 14:59:49  shomrat
* Added CBioseq_EditHandle constructor implementation
*
* Revision 1.65  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.64  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.63  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.62  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.61  2004/04/20 20:34:40  shomrat
* fix GetExactComplexityLevel
*
* Revision 1.60  2004/03/31 19:54:08  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.59  2004/03/31 19:23:13  vasilche
* Fixed scope in CBioseq_Handle::GetEditHandle().
*
* Revision 1.58  2004/03/29 20:13:06  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.57  2004/03/25 19:27:44  vasilche
* Implemented MoveTo and CopyTo methods of handles.
*
* Revision 1.56  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.55  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.54  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.53  2004/02/06 16:07:27  grichenk
* Added CBioseq_Handle::GetSeq_entry_Handle()
* Fixed MapLocation()
*
* Revision 1.52  2004/01/28 22:10:10  grichenk
* Removed extra seq-loc initialization in MapLocation
*
* Revision 1.51  2004/01/23 16:14:47  grichenk
* Implemented alignment mapping
*
* Revision 1.50  2004/01/22 20:10:40  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.49  2003/11/28 15:13:26  grichenk
* Added CSeq_entry_Handle
*
* Revision 1.48  2003/11/26 17:55:57  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.47  2003/11/10 18:12:43  grichenk
* Added MapLocation()
*
* Revision 1.46  2003/09/30 16:22:02  vasilche
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
* Revision 1.45  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.44  2003/08/27 14:27:19  vasilche
* Use Reverse(ENa_strand) function.
*
* Revision 1.43  2003/07/15 16:14:08  grichenk
* CBioseqHandle::IsSynonym() made public
*
* Revision 1.42  2003/06/24 14:22:46  vasilche
* Fixed CSeqMap constructor from CSeq_loc.
*
* Revision 1.41  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.38  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.37  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.36  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.35  2003/03/27 19:39:36  grichenk
* +CBioseq_Handle::GetComplexityLevel()
*
* Revision 1.34  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.33  2003/03/12 20:09:33  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.32  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.31  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.30  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.29  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.28  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.26  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.24  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.23  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.22  2002/09/03 21:27:01  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.21  2002/07/10 16:49:29  grichenk
* Removed commented reference to old CDataSource mutex
*
* Revision 1.20  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.19  2002/06/12 14:39:02  grichenk
* Renamed enumerators
*
* Revision 1.18  2002/06/06 21:00:42  clausen
* Added include for scope.hpp
*
* Revision 1.17  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.16  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.15  2002/05/21 18:39:30  grichenk
* CBioseq_Handle::GetResolvedSeqMap() -> CreateResolvedSeqMap()
*
* Revision 1.14  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.13  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.12  2002/04/29 16:23:28  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.11  2002/04/23 19:01:07  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.10  2002/04/22 20:06:59  grichenk
* +GetSequenceView(), +x_IsSynonym()
*
* Revision 1.9  2002/04/11 12:07:30  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.8  2002/03/19 19:16:28  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.7  2002/03/15 18:10:07  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.6  2002/03/04 15:08:44  grichenk
* Improved CTSE_Info locks
*
* Revision 1.5  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
