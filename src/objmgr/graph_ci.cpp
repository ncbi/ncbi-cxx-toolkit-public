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
    return CSeq_graph_Handle(GetAnnot(), m_GraphRef->GetAnnotObjectIndex());
}


CSeq_annot_Handle CMappedGraph::GetAnnot(void) const
{
    return m_Collector->GetAnnot(*m_GraphRef);
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


CGraph_CI::~CGraph_CI(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.31  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.30  2004/12/22 15:56:07  vasilche
* Added method CSeq_annot_Handle GetAnnot().
*
* Revision 1.29  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.28  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.27  2004/05/04 18:08:48  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.26  2004/04/07 13:20:17  grichenk
* Moved more data from iterators to CAnnot_Collector
*
* Revision 1.25  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.24  2004/03/23 19:37:33  grichenk
* Added m_Graph in constructors
*
* Revision 1.23  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.22  2004/01/28 20:54:36  vasilche
* Fixed mapping of annotations.
*
* Revision 1.21  2004/01/23 16:14:48  grichenk
* Implemented alignment mapping
*
* Revision 1.20  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.19  2003/06/02 16:06:37  dicuccio
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
* Revision 1.18  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.17  2003/03/18 21:48:30  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.16  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.15  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.14  2003/02/10 22:04:44  grichenk
* CGraph_CI resolves to CMappedGraph instead of CSeq_graph
*
* Revision 1.13  2003/02/10 15:51:27  grichenk
* + CMappedGraph
*
* Revision 1.12  2002/12/24 15:42:45  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.11  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.10  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:55  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:19  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
