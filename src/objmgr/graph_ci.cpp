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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CMappedGraph::Set(CAnnot_Collector& collector, const TIterator& annot)
{
    _ASSERT(annot->IsGraph());
    m_Collector.Reset(&collector);
    m_GraphRef = annot;
    m_MappedGraph.Reset();
    m_MappedLoc.Reset();
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
        m_GraphRef->GetMappingInfo().UpdateMappedSeq_loc(created_loc);
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
    CSeq_annot_Handle annot_handle;
    m_Collector->SetAnnotHandle(annot_handle, *m_GraphRef);
    return annot_handle;
}


void CMappedGraph::MakeMappedGraph(void) const
{
    if ( m_GraphRef->GetMappingInfo().IsMapped() ) {
        CSeq_loc& loc = const_cast<CSeq_loc&>(GetLoc());
        CSeq_graph* tmp;
        m_MappedGraph.Reset(tmp = new CSeq_graph);
        tmp->Assign(GetOriginalGraph());
        tmp->SetLoc(loc);
    }
    else {
        m_MappedGraph.Reset(&GetOriginalGraph());
    }
}


CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, scope, loc)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, scope, loc, &sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     range,
                     strand)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown,
                     &sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
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
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
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
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot, &sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry, &sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


CGraph_CI::~CGraph_CI(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
