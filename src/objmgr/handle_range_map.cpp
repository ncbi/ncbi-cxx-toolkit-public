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
*   CHandle_Range_Map is a substitute for seq-loc to make searching
*   over locations more effective.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CHandleRangeMap::
//


CHandleRangeMap::CHandleRangeMap(void)
{
}


CHandleRangeMap::CHandleRangeMap(const CHandleRangeMap& rmap)
{
    *this = rmap;
}


CHandleRangeMap::~CHandleRangeMap(void)
{
}


CHandleRangeMap& CHandleRangeMap::operator= (const CHandleRangeMap& rmap)
{
    if (this != &rmap) {
        m_LocMap = rmap.m_LocMap;
    }
    return *this;
}


void CHandleRangeMap::clear(void)
{
    m_LocMap.clear();
}


void CHandleRangeMap::AddLocation(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    {
        return;
    }
    case CSeq_loc::e_Whole:
    {
        AddRange(loc.GetWhole(), CHandleRange::TRange::GetWhole());
        return;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& i = loc.GetInt();
        AddRange(i.GetId(),
                 i.GetFrom(),
                 i.GetTo(),
                 i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown);
        return;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& p = loc.GetPnt();
        AddRange(p.GetId(),
                 p.GetPoint(),
                 p.GetPoint(),
                 p.IsSetStrand()? p.GetStrand(): eNa_strand_unknown);
        return;
    }
    case CSeq_loc::e_Packed_int:
    {
        // extract each range
        const CPacked_seqint& pi = loc.GetPacked_int();
        ITERATE( CPacked_seqint::Tdata, ii, pi.Get() ) {
            const CSeq_interval& i = **ii;
            AddRange(i.GetId(),
                     i.GetFrom(),
                     i.GetTo(),
                     i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown);
        }
        return;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        // extract each point
        const CPacked_seqpnt& pp = loc.GetPacked_pnt();
        CSeq_id_Handle idh =
            CSeq_id_Mapper::GetSeq_id_Mapper()->GetHandle(pp.GetId());
        CHandleRange& hr = m_LocMap[idh];
        ENa_strand strand = pp.IsSetStrand()? pp.GetStrand(): eNa_strand_unknown;
        ITERATE ( CPacked_seqpnt::TPoints, pi, pp.GetPoints() ) {
            hr.AddRange(CHandleRange::TRange(*pi, *pi), strand);
        }
        return;
    }
    case CSeq_loc::e_Mix:
    {
        // extract sub-locations
        ITERATE ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
            AddLocation(**li);
        }
        return;
    }
    case CSeq_loc::e_Equiv:
    {
        // extract sub-locations
        ITERATE ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
            AddLocation(**li);
        }
        return;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& bond = loc.GetBond();
        const CSeq_point& pa = bond.GetA();
        AddRange(pa.GetId(),
                 pa.GetPoint(),
                 pa.GetPoint(),
                 pa.IsSetStrand()? pa.GetStrand(): eNa_strand_unknown);
        if ( bond.IsSetB() ) {
            const CSeq_point& pb = bond.GetB();
            AddRange(pb.GetId(),
                     pb.GetPoint(),
                     pb.GetPoint(),
                     pb.IsSetStrand()? pb.GetStrand(): eNa_strand_unknown);
        }
        return;
    }
    case CSeq_loc::e_Feat:
    {
        //### Not implemented (do we need it?)
        return;
    }
    } // switch
}


void CHandleRangeMap::AddRange(const CSeq_id_Handle& h,
                               CHandleRange::TRange range,
                               ENa_strand strand)
{
    m_LocMap[h].AddRange(range, strand);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               CHandleRange::TRange range,
                               ENa_strand strand)
{
    AddRange(CSeq_id_Mapper::GetSeq_id_Mapper()->GetHandle(id), range, strand);
}


void CHandleRangeMap::AddRanges(const CSeq_id_Handle& h,
                                const CHandleRange& hr)
{
    m_LocMap[h].AddRanges(hr);
}


CHandleRange& CHandleRangeMap::AddRanges(const CSeq_id_Handle& h)
{
    return m_LocMap[h];
}


bool CHandleRangeMap::IntersectingWithLoc(const CSeq_loc& loc) const
{
    CHandleRangeMap rmap;
    rmap.AddLocation(loc);
    return IntersectingWithMap(rmap);
}


bool CHandleRangeMap::IntersectingWithMap(const CHandleRangeMap& rmap) const
{
    if ( rmap.m_LocMap.size() > m_LocMap.size() ) {
        return rmap.IntersectingWithMap(*this);
    }
    ITERATE ( CHandleRangeMap, it1, rmap ) {
        const_iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != end() && it1->second.IntersectingWith(it2->second) ) {
            return true;
        }
    }
    return false;
}


bool CHandleRangeMap::TotalRangeIntersectingWith(const CHandleRangeMap& rmap) const
{
    if ( rmap.m_LocMap.size() > m_LocMap.size() ) {
        return rmap.TotalRangeIntersectingWith(*this);
    }
    ITERATE ( CHandleRangeMap, it1, rmap ) {
        TLocMap::const_iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != end() && it1->second.GetOverlappingRange()
             .IntersectingWith(it2->second.GetOverlappingRange()) ) {
            return true;
        }
    }
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.20  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.19  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.18  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.17  2003/06/02 16:06:38  dicuccio
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
* Revision 1.16  2003/05/21 16:03:08  vasilche
* Fixed access to uninitialized optional members.
* Added initialization of mandatory members.
*
* Revision 1.15  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.13  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.12  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.11  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.10  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.9  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.8  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.7  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.6  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.5  2002/06/12 14:40:47  grichenk
* Made some methods inline
*
* Revision 1.4  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.2  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:20  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
