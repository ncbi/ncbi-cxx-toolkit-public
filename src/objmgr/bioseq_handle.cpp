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

//#include <serial/typeinfo.hpp>

#include <objmgr/bioseq_handle.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/seqmatch_info.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_Handle::CBioseq_Handle(void)
{
}


CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& id,
                               CBioseq_ScopeInfo* bioseq_info)
    : m_Seq_id(id),
      m_Bioseq_Info(bioseq_info)
{
}


CBioseq_Handle::~CBioseq_Handle(void)
{
}


CDataSource& CBioseq_Handle::x_GetDataSource(void) const
{
    // m_TSE_Info and its m_DataSource should never be null
    if ( !m_Bioseq_Info ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Can not resolve data source for bioseq handle.");
    }
    return m_Bioseq_Info->GetDataSource();
}


CConstRef<CSeq_id> CBioseq_Handle::GetSeqId(void) const
{
    return GetSeq_id_Handle().GetSeqIdOrNull();
}


const CBioseq& CBioseq_Handle::GetBioseq(void) const
{
    return x_GetDataSource().GetBioseq(x_GetBioseq_Info());
}


const CSeq_entry& CBioseq_Handle::GetTopLevelSeqEntry(void) const
{
    // Can not use m_TSE->m_TSE since the handle may be unresolved yet
    return x_GetDataSource().GetTSE(x_GetBioseq_Info().GetTSE_Info());
}


CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetDataSource().GetBioseqCore(x_GetBioseq_Info());
}


TSeqPos CBioseq_Handle::GetBioseqLength(void) const
{
    return x_GetBioseq_Info().GetBioseqLength();
}


CSeq_inst::TMol CBioseq_Handle::GetBioseqMolType(void) const
{
    return x_GetBioseq_Info().GetBioseqMolType();
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetBioseq_Info().GetSeqMap();
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


CConstRef<CSynonymsSet> CBioseq_Handle::x_GetSynonyms(void) const
{
    if ( !*this ) {
        return CConstRef<CSynonymsSet>();
    }
    return GetScope().GetSynonyms(*this);
}


bool CBioseq_Handle::IsSynonym(const CSeq_id& id) const
{
    if ( !(*this) )
        return false;
    CConstRef<CSynonymsSet> syns = x_GetSynonyms();
    return syns->ContainsSynonym(CSeq_id_Handle::GetHandle(id));
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


const CTSE_Info& CBioseq_Handle::x_GetTSE_Info(void) const
{
    _ASSERT(*this);
    return x_GetBioseq_Info().GetTSE_Info();
}


CSeq_entry& CBioseq_Handle::x_GetTSE_NC(void)
{
    _ASSERT(*this);
    return const_cast<CSeq_entry&>(x_GetBioseq_Info().GetSeq_entry());
}


void CBioseq_Handle::AddAnnot(CSeq_annot& annot)
{
    _ASSERT(bool(m_Bioseq_Info));
    x_GetDataSource().AttachAnnot(x_GetTSE_NC(), annot);
}


void CBioseq_Handle::RemoveAnnot(CSeq_annot& annot)
{
    _ASSERT(bool(m_Bioseq_Info));
    x_GetDataSource().RemoveAnnot(x_GetTSE_NC(), annot);
}


void CBioseq_Handle::ReplaceAnnot(CSeq_annot& old_annot,
                                  CSeq_annot& new_annot)
{
    _ASSERT(bool(m_Bioseq_Info));
    x_GetDataSource().ReplaceAnnot(x_GetTSE_NC(), old_annot, new_annot);
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


const CSeq_entry&
CBioseq_Handle::GetComplexityLevel(CBioseq_set::EClass cls) const
{
    if (cls == CBioseq_set::eClass_other) {
        // adjust 255 to the correct value
        cls = CBioseq_set::EClass(sizeof(s_Complexity) - 1);
    }
    TBioseqCore bs = GetBioseqCore();
    CSeq_entry* last = bs->GetParentEntry();
    _ASSERT(last  &&  last->IsSeq());
    CSeq_entry* e = last->GetParentEntry();
    while ( e ) {
        // Found good level
        if (last->IsSet()
            &&  s_Complexity[last->GetSet().GetClass()] == s_Complexity[cls])
            break;
        // Gone too high
        if (e->IsSet()
            &&  s_Complexity[e->GetSet().GetClass()] > s_Complexity[cls])
            break;
        // Go up one level
        last = e;
        e = e->GetParentEntry();
    }
    return *last;
}


CRef<CSeq_loc> CBioseq_Handle::MapLocation(const CSeq_loc& loc) const
{
    const CSeqMap& seq_map = GetSeqMap();
    // Iterate seq-map, for each segment create conversion,
    // for each conversion try to map seq-loc to the master sequence.
    CSeq_loc_Conversion_Set conv_set;
    CHeapScope hscope(GetScope());
    conv_set.SetScope(hscope);
    CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
    master_loc_empty->SetEmpty().Assign(*GetSeqId());
    CConstRef<CSeqMap> cr_map(&seq_map);
    CSeqMap_CI it(cr_map, &GetScope(), 0, kMax_Int,
        CSeqMap::fFindRef);
    for ( ; it; ++it) {
        _ASSERT(it.GetType() == CSeqMap::eSeqRef);
        CSeq_loc_Conversion def_cvt(
            GetSeq_id_Handle(),
            *master_loc_empty,
            it,
            it.GetRefSeqid(),
            &GetScope());
        CConstRef<CSynonymsSet> syns = GetScope().GetSynonyms(it.GetRefSeqid());
        if (syns) {
            ITERATE ( CSynonymsSet, synit, *syns ) {
                CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(synit);
                CRef<CSeq_loc_Conversion> cvt(new CSeq_loc_Conversion(def_cvt));
                cvt->SetSrcId(idh);
                conv_set.Add(*cvt);
            }
        }
        else {
            CRef<CSeq_loc_Conversion> cvt(new CSeq_loc_Conversion(def_cvt));
            conv_set.Add(*cvt);
        }
    }
    CRef<CSeq_loc> dst(new CSeq_loc);
    if (!conv_set.Convert(loc, dst)) {
        dst.Reset();
    }
    return dst;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
