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
* Author: Eugene Vasilchenko
*
* File Description:
*   Class for mapping Seq-loc petween sequences.
*
*/

#include <objmgr/impl/seq_loc_cvt.hpp>

#include <objmgr/seq_map_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seq/Bioseq.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion
/////////////////////////////////////////////////////////////////////////////

CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id_Handle& master_id,
                                         CSeq_loc& master_loc_empty,
                                         const CSeqMap_CI& seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope* scope)
    : m_Src_id_Handle(src_id),
      m_Src_length(kInvalidSeqPos),
      m_Src_from(0),
      m_Src_to(0),
      m_Shift(0),
      m_Reverse(false),
      m_Dst_id(&master_loc_empty.SetEmpty()),
      m_Dst_loc_Empty(&master_loc_empty),
      m_Partial(false),
      m_LastType(CSeq_loc::e_not_set),
      m_LastStrand(eNa_strand_unknown),
      m_Scope(scope)
{
    SetConversion(seg);
    Reset();
}


CSeq_loc_Conversion::~CSeq_loc_Conversion(void)
{
    _ASSERT(!IsSpecialLoc());
}


void CSeq_loc_Conversion::SetConversion(const CSeqMap_CI& seg)
{
    m_Src_from = seg.GetRefPosition();
    m_Src_to = m_Src_from + seg.GetLength() - 1;
    m_Reverse = seg.GetRefMinusStrand();
    if ( !m_Reverse ) {
        m_Shift = seg.GetPosition() - m_Src_from;
    }
    else {
        m_Shift = seg.GetPosition() + m_Src_to;
    }
}


bool CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos,
                                       ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return false;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        m_LastStrand = src_strand;
        dst_pos = m_Shift + src_pos;
    }
    else {
        m_LastStrand = Reverse(src_strand);
        dst_pos = m_Shift - src_pos;
    }
    m_LastType = CSeq_loc::e_Pnt;
    m_TotalRange += m_LastRange.SetFrom(dst_pos).SetTo(dst_pos);
    return true;
}


bool CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from, TSeqPos src_to,
                                          ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
    if ( src_from < m_Src_from ) {
        m_Partial = true;
        src_from = m_Src_from;
    }
    if ( src_to > m_Src_to ) {
        m_Partial = true;
        src_to = m_Src_to;
    }
    if ( src_from > src_to ) {
        return false;
    }
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        m_LastStrand = src_strand;
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        m_LastStrand = Reverse(src_strand);
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    m_LastType = CSeq_loc::e_Int;
    m_TotalRange += m_LastRange.SetFrom(dst_from).SetTo(dst_to);
    return true;
}


inline
void CSeq_loc_Conversion::CheckDstInterval(void)
{
    if ( m_LastType != CSeq_loc::e_Int ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Wrong last location type");
    }
    m_LastType = CSeq_loc::e_not_set;
}


inline
void CSeq_loc_Conversion::CheckDstPoint(void)
{
    if ( m_LastType != CSeq_loc::e_Pnt ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Wrong last location type");
    }
    m_LastType = CSeq_loc::e_not_set;
}


CRef<CSeq_interval> CSeq_loc_Conversion::GetDstInterval(void)
{
    CheckDstInterval();
    CRef<CSeq_interval> ret(new CSeq_interval);
    CSeq_interval& interval = *ret;
    interval.SetId(GetDstId());
    interval.SetFrom(m_LastRange.GetFrom());
    interval.SetTo(m_LastRange.GetTo());
    if ( m_LastStrand != eNa_strand_unknown ) {
        interval.SetStrand(m_LastStrand);
    }
    return ret;
}


CRef<CSeq_point> CSeq_loc_Conversion::GetDstPoint(void)
{
    CheckDstPoint();
    _ASSERT(m_LastRange.GetLength() == 1);
    CRef<CSeq_point> ret(new CSeq_point);
    CSeq_point& point = *ret;
    point.SetId(GetDstId());
    point.SetPoint(m_LastRange.GetFrom());
    if ( m_LastStrand != eNa_strand_unknown ) {
        point.SetStrand(m_LastStrand);
    }
    return ret;
}


void CSeq_loc_Conversion::SetDstLoc(CRef<CSeq_loc>& dst)
{
    if ( !dst ) {
        switch ( m_LastType ) {
        case CSeq_loc::e_Int:
            dst.Reset(new CSeq_loc);
            dst->SetInt(*GetDstInterval());
            break;
        case CSeq_loc::e_Pnt:
            dst.Reset(new CSeq_loc);
            dst->SetPnt(*GetDstPoint());
            break;
        default:
            dst = m_Dst_loc_Empty;
            break;
        }
    }
    else {
        _ASSERT(!IsSpecialLoc());
    }
}


bool CSeq_loc_Conversion::Convert(const CSeq_loc& src, CRef<CSeq_loc>& dst,
                                  bool always)
{
    dst.Reset();
    _ASSERT(!IsSpecialLoc());
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Feat:
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        _ASSERT("this cannot happen" || 0);
        break;
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src.GetWhole();
        // Convert to the allowed master seq interval
        if ( GoodSrcId(src_id) ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
            ConvertInterval(0, bh.GetBioseqLength()-1, eNa_strand_unknown);
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        ConvertInterval(src.GetInt());
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        ConvertPoint(src.GetPnt());
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
        CPacked_seqint::Tdata* dst_ints = 0;
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            if ( ConvertInterval(**i) ) {
                if ( !dst_ints ) {
                    dst.Reset(new CSeq_loc);
                    dst_ints = &dst->SetPacked_int().Set();
                }
                dst_ints->push_back(GetDstInterval());
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
        if ( !GoodSrcId(src_pack_pnts.GetId()) ) {
            break;
        }
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        CPacked_seqpnt::TPoints* dst_pnts = 0;
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            TSeqPos dst_pos = ConvertPos(*i);
            if ( dst_pos != kInvalidSeqPos ) {
                if ( !dst_pnts ) {
                    dst.Reset(new CSeq_loc);
                    CPacked_seqpnt& pnts = dst->SetPacked_pnt();
                    pnts.SetId(GetDstId());
                    dst_pnts = &pnts.SetPoints();
                    if ( src_pack_pnts.IsSetStrand() ) {
                        pnts.SetStrand(ConvertStrand(src_pack_pnts.GetStrand()));
                    }
                }
                dst_pnts->push_back(dst_pos);
                m_TotalRange += TRange(dst_pos, dst_pos);
            }
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
        CSeq_loc_mix::Tdata* dst_mix = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            if ( Convert(**i, dst_loc, true) ) {
                if ( !dst_mix ) {
                    dst.Reset(new CSeq_loc);
                    dst_mix = &dst->SetMix().Set();
                }
                dst_mix->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
        CSeq_loc_equiv::Tdata* dst_equiv = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            if ( Convert(**i, dst_loc, true) ) {
                if ( !dst_equiv ) {
                    dst.Reset(new CSeq_loc);
                    dst_equiv = &dst->SetEquiv().Set();
                }
                dst_equiv->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& src_bond = src.GetBond();
        CSeq_bond* dst_bond = 0;
        if ( ConvertPoint(src_bond.GetA()) ) {
            dst.Reset(new CSeq_loc);
            dst_bond = &dst->SetBond();
            dst_bond->SetA(*GetDstPoint());
            if ( src_bond.IsSetB() ) {
                if ( ConvertPoint(src_bond.GetB()) ) {
                    if ( !dst_bond ) {
                        dst.Reset(new CSeq_loc);
                        dst_bond = &dst->SetBond();
                    }
                    dst_bond->SetB(*GetDstPoint());
                }
            }
        }
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
    if ( always && IsSpecialLoc() ) {
        SetDstLoc(dst);
    }
    return dst;
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref, int index)
{
    Reset();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        if ( index == 0 ) {
            Convert(obj.GetFeatFast()->GetLocation(), ref.m_MappedLocation, false);
        }
        else {
            Convert(obj.GetFeatFast()->GetProduct(), ref.m_MappedLocation, false);
        }
        break;
    case CSeq_annot::C_Data::e_Graph:
        Convert(obj.GetGraphFast()->GetLoc(), ref.m_MappedLocation, false);
        break;
    case CSeq_annot::C_Data::e_Align:
        // TODO: map align
        break;
    }
    SetMappedLocation(ref, index);
}


void CSeq_loc_Conversion::SetMappedLocation(CAnnotObject_Ref& ref, int index)
{
    ref.m_MappedIndex = index;
    ref.m_Partial |= m_Partial;
    ref.m_TotalRange = m_TotalRange;
    ref.m_MappedType = m_LastType;
    if ( IsSpecialLoc() ) {
        _ASSERT(!ref.m_MappedLocation);
        // special interval or point
        ref.m_MappedLocation = m_Dst_loc_Empty;
        ref.m_MappedStrand = m_LastStrand;
        m_LastType = CSeq_loc::e_not_set;
    }
    else {
        _ASSERT(ref.m_MappedLocation);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2003/10/29 19:55:47  vasilche
* Avoid making 'whole' features on 'whole' segment partial (by Aleksey Grichenko)
*
* Revision 1.5  2003/09/30 16:22:03  vasilche
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
* Revision 1.4  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.3  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.2  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.1  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* ===========================================================================
*/
