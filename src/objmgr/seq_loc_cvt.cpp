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

CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id& master_id,
                                         const CSeqMap_CI& seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope* scope)
    : m_Src_id(src_id), m_Dst_id(master_id), m_Scope(scope)
{
    SetConversion(seg);
    Reset();
}


CSeq_loc_Conversion::~CSeq_loc_Conversion(void)
{
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


bool CSeq_loc_Conversion::ConvertRange(TRange& range)
{
    TSeqPos src_from = range.GetFrom();
    if ( src_from < m_Src_from ) {
        m_Partial = true;
        src_from = m_Src_from;
    }
    TSeqPos src_to = range.GetTo();
    if ( src_to > m_Src_to ) {
        m_Partial = true;
        src_to = m_Src_to;
    }
    if ( src_from > src_to ) {
        return false;
    }
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    range.SetFrom(dst_from).SetTo(dst_to);
    return true;
}


bool CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos)
{
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return false;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        dst_pos = m_Shift + src_pos;
    }
    else {
        dst_pos = m_Shift - src_pos;
    }
    CSeq_point* dst;
    dst_pnt.Reset(dst = new CSeq_point);
    dst->SetId().Assign(m_Dst_id);
    dst->SetPoint(dst_pos);
    m_TotalRange += TRange(dst_pos, dst_pos);
    return true;
}


bool CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from, TSeqPos src_to)
{
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
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    CSeq_interval* dst;
    dst_int.Reset(dst = new CSeq_interval);
    dst->SetId().Assign(m_Dst_id);
    dst->SetFrom(dst_from);
    dst->SetTo(dst_to);
    m_TotalRange += TRange(dst_from, dst_to);
    return true;
}


bool CSeq_loc_Conversion::Convert(const CSeq_loc& src, CRef<CSeq_loc>& dst,
                                  bool always)
{
    dst.Reset();
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Feat:
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        break;
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src.GetWhole();
        // Convert to the allowed master seq interval
        if ( GoodSrcId(src_id) ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
            if ( !bh ) {
                THROW1_TRACE(runtime_error,
                             "CSeq_loc_Conversion::Convert: "
                             "cannot determine sequence length");
            }
            CBioseq_Handle::TBioseqCore core = bh.GetBioseqCore();
            if ( !core->GetInst().IsSetLength() ) {
                THROW1_TRACE(runtime_error,
                             "CSeq_loc_Conversion::Convert: "
                             "cannot determine sequence length");
            }
            if ( ConvertInterval(0, core->GetInst().GetLength()) ) {
                dst.Reset(new CSeq_loc);
                dst->SetInt(*dst_int);
            }
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        if ( ConvertInterval(src.GetInt()) ) {
            dst.Reset(new CSeq_loc);
            dst->SetInt(*dst_int);
        }
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        if ( ConvertPoint(src.GetPnt()) ) {
            dst.Reset(new CSeq_loc);
            dst->SetPnt(*dst_pnt);
        }
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
                dst_ints->push_back(dst_int);
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
                    pnts.SetId().Assign(m_Dst_id);
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
            if ( Convert(**i, dst_loc) ) {
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
            if ( Convert(**i, dst_loc) ) {
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
            dst_bond->SetA(*dst_pnt);
        }
        if ( src_bond.IsSetB() ) {
            if ( ConvertPoint(src_bond.GetB()) ) {
                if ( !dst_bond ) {
                    dst.Reset(new CSeq_loc);
                    dst_bond = &dst->SetBond();
                }
                dst_bond->SetB(*dst_pnt);
            }
        }
        break;
    }
    default:
        THROW1_TRACE(runtime_error,
                     "CSeq_loc_Conversion::Convert: "
                     "Unsupported location type");
    }
    if ( always && !dst ) {
        dst.Reset(new CSeq_loc);
        dst->SetEmpty();
    }
    return dst;
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref, int index)
{
    Reset();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    CRef<CSeq_loc> loc;
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        const CSeq_feat& feat = *obj.GetFeatFast();
        if ( index == 0 ) {
            Convert(feat.GetLocation(), loc, true);
        }
        else {
            _ASSERT(feat.IsSetProduct());
            Convert(feat.GetProduct(), loc, true);
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        const CSeq_graph& graph = *obj.GetGraphFast();
        Convert(graph.GetLoc(), loc, true);
        break;
    }
    case CSeq_annot::C_Data::e_Align:
    {
        // TODO: map align
        break;
    }
    }
    ref.SetAnnotObjectRange(m_TotalRange, IsPartial(), loc);
}


CSeq_loc* CSeq_loc_Conversion::GetDstLocWhole(void)
{
    if ( !m_DstLocWhole ) {
        m_DstLocWhole.Reset(new CSeq_loc);
        m_DstLocWhole->SetWhole(const_cast<CSeq_id&>(m_Dst_id));
    }
    return m_DstLocWhole.GetPointer();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
