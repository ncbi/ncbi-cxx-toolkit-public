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
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqres/seqres__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CAnnotObject_Ref;


void CMappedGraph::Set(CAnnot_Collector& collector,
                       const CAnnotObject_Ref& annot_ref)
{
    _ASSERT(annot_ref.IsGraph());
    m_Collector.Reset(&collector);
    m_GraphRef = &annot_ref;
    m_MappedGraph.Reset();
    m_MappedLoc.Reset();
}


void CMappedGraph::Reset(void)
{
    m_MappedLoc.Reset();
    m_MappedGraph.Reset();
    m_GraphRef = 0;
    m_Collector.Reset();
}


void CMappedGraph::MakeMappedLoc(void) const
{
    if ( m_GraphRef->GetMappingInfo().MappedSeq_locNeedsUpdate() ) {
        m_MappedGraph.Reset();
        m_MappedLoc.Reset();
        CRef<CSeq_loc> created_loc;
        if ( !m_Collector->m_CreatedMapped ) {
            m_Collector->m_CreatedMapped.Reset(new CCreatedFeat_Ref);
        }
        m_Collector->m_CreatedMapped->ReleaseRefsTo(0, &created_loc, 0, 0);
        CRef<CSeq_point>    created_pnt;
        CRef<CSeq_interval> created_int;
        m_GraphRef->GetMappingInfo().UpdateMappedSeq_loc(created_loc,
                                                         created_pnt,
                                                         created_int,
                                                         0);
        m_MappedLoc = created_loc;
        m_Collector->m_CreatedMapped->ResetRefsFrom(0, &created_loc, 0, 0);
    }
    else if ( m_GraphRef->GetMappingInfo().IsMapped() ) {
        m_MappedLoc.Reset(&m_GraphRef->GetMappingInfo().GetMappedSeq_loc());
    }
    else {
        m_MappedLoc.Reset(&GetOriginalGraph().GetLoc());
    }
}


CSeq_graph_Handle CMappedGraph::GetSeq_graph_Handle(void) const
{
    return CSeq_graph_Handle(GetAnnot(), m_GraphRef->GetAnnotIndex());
}


CSeq_annot_Handle CMappedGraph::GetAnnot(void) const
{
    return m_GraphRef->GetSeq_annot_Handle();
}


void CMappedGraph::MakeMappedGraph(void) const
{
    if ( m_GraphRef->GetMappingInfo().IsMapped() ) {
        CSeq_loc& loc = const_cast<CSeq_loc&>(GetLoc());
        CSeq_graph* tmp;
        m_MappedGraph.Reset(tmp = new CSeq_graph);
        tmp->Assign(GetOriginalGraph());
        MakeMappedGraphData(*tmp);
        tmp->SetLoc(loc);
    }
    else {
        m_MappedGraph.Reset(&GetOriginalGraph());
    }
}


template<class TData> void CopyGraphData(const TData& src,
                                         TData&       dst,
                                         TSeqPos      from,
                                         TSeqPos      to)
{
    _ASSERT(from < src.size()  &&  to <= src.size());
    dst.insert(dst.end(), src.begin() + from, src.begin() + to);
}


void CMappedGraph::MakeMappedGraphData(CSeq_graph& dst) const
{
    const TGraphRanges& ranges = GetMappedGraphRanges();
    CSeq_graph::TGraph& dst_data = dst.SetGraph();
    dst_data.Reset();
    const CSeq_graph& src = GetOriginalGraph();
    const CSeq_graph::TGraph& src_data = src.GetGraph();

    TSeqPos comp = (src.IsSetComp()  &&  src.GetComp()) ? src.GetComp() : 1;
    TSeqPos numval = 0;

    switch ( src_data.Which() ) {
    case CSeq_graph::TGraph::e_Byte:
        dst_data.SetByte().SetMin(src_data.GetByte().GetMin());
        dst_data.SetByte().SetMax(src_data.GetByte().GetMax());
        dst_data.SetByte().SetAxis(src_data.GetByte().GetAxis());
        dst_data.SetByte().SetValues();
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp;
            TSeqPos to = it->GetTo()/comp + 1;
            CopyGraphData(src_data.GetByte().GetValues(),
                          dst_data.SetByte().SetValues(),
                          from, to);
            numval += to - from;
        }
        break;
    case CSeq_graph::TGraph::e_Int:
        dst_data.SetInt().SetMin(src_data.GetInt().GetMin());
        dst_data.SetInt().SetMax(src_data.GetInt().GetMax());
        dst_data.SetInt().SetAxis(src_data.GetInt().GetAxis());
        dst_data.SetInt().SetValues();
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp;
            TSeqPos to = it->GetTo()/comp + 1;
            CopyGraphData(src_data.GetInt().GetValues(),
                          dst_data.SetInt().SetValues(),
                          from, to);
            numval += to - from;
        }
        break;
    case CSeq_graph::TGraph::e_Real:
        dst_data.SetReal().SetMin(src_data.GetReal().GetMin());
        dst_data.SetReal().SetMax(src_data.GetReal().GetMax());
        dst_data.SetReal().SetAxis(src_data.GetReal().GetAxis());
        dst_data.SetReal().SetValues();
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp;
            TSeqPos to = it->GetTo()/comp + 1;
            CopyGraphData(src_data.GetReal().GetValues(),
                          dst_data.SetReal().SetValues(),
                          from, to);
            numval += to - from;
        }
        break;
    default:
        break;
    }
    dst.SetNumval(numval);
}


const CSeq_graph::C_Graph& CMappedGraph::GetGraph(void) const
{
    if ( !m_GraphRef->GetMappingInfo().IsPartial() ) {
        return GetOriginalGraph().GetGraph();
    }
    MakeMappedGraph();
    return m_MappedGraph->GetGraph();
}


const CMappedGraph::TRange&
CMappedGraph::GetMappedGraphTotalRange(void) const
{
    const CGraphRanges* rgs = m_GraphRef->GetMappingInfo().GetGraphRanges();
    _ASSERT(rgs);
    return rgs->GetTotalRange();
}


const CMappedGraph::TGraphRanges&
CMappedGraph::GetMappedGraphRanges(void) const
{
    const CGraphRanges* rgs = m_GraphRef->GetMappingInfo().GetGraphRanges();
    _ASSERT(rgs);
    return rgs->GetRanges();
}


CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, scope, loc)
{
    x_Update();
}


CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, scope, loc, &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     range,
                     strand)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown,
                     &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     range,
                     eNa_strand_unknown,
                     &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     range,
                     strand,
                     &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot, &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_loc& loc,
                     const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     loc,
                     annot)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_loc& loc,
                     const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     loc,
                     annot,
                     &sel)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry)
{
    x_Update();
}


CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry, &sel)
{
    x_Update();
}


CGraph_CI::~CGraph_CI(void)
{
}


CGraph_CI& CGraph_CI::operator= (const CGraph_CI& iter)
{
    if ( this != &iter ) {
        CAnnotTypes_CI::operator=(iter);
        x_Update();
    }
    return *this;
}


void CGraph_CI::x_Update(void)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), *GetIterator());
    }
    else {
        m_Graph.Reset();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
