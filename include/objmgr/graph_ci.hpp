#ifndef GRAPH_CI__HPP
#define GRAPH_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objmgr/annot_types_ci.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CMappedGraph
{
public:
    CMappedGraph(void)
        : m_Graph(0),
          m_MappedGraph(0),
          m_Partial(false),
          m_MappedLoc(0)
        { return; }
    CMappedGraph(const CMappedGraph& graph)
        : m_Graph(graph.m_Graph),
          m_MappedGraph(graph.m_MappedGraph),
          m_Partial(graph.m_Partial),
          m_MappedLoc(graph.m_MappedLoc)
        { return; }
    CMappedGraph& operator= (const CMappedGraph& graph)
        {
            if (this != &graph) {
                m_Graph = graph.m_Graph;
                m_MappedGraph = graph.m_MappedGraph;
                m_Partial = graph.m_Partial;
                m_MappedLoc = graph.m_MappedLoc;
            }
            return *this;
        }

    // Original graph with unmapped location/product
    const CSeq_graph& GetOriginalGraph(void) const
        {
            return *m_Graph;
        }
    // Graph mapped to the master sequence.
    // WARNING! The function is rather slow and should be used with care.
    const CSeq_graph& GetMappedGraph(void) const
        {
            if ( !m_MappedGraph ) {
                if ( m_MappedLoc ) {
                    CSeq_graph* tmp = new CSeq_graph;
                    m_MappedGraph = tmp;
                    tmp->Assign(*m_Graph);
                    tmp->SetLoc().Assign(*m_MappedLoc);
                }
                else {
                    m_MappedGraph = m_Graph;
                }
            }
            return *m_MappedGraph;
        }

    bool IsSetTitle(void) const
        {
            return m_Graph->IsSetTitle();
        }
    const string& GetTitle(void) const
        {
            return m_Graph->GetTitle();
        }

    bool IsSetComment(void) const
        {
            return m_Graph->IsSetComment();
        }
    const string& GetComment(void) const
        {
            return m_Graph->GetComment();
        }

    const CSeq_loc& GetLoc(void) const
        {
            return m_MappedLoc ? *m_MappedLoc : m_Graph->GetLoc();
        }

    bool IsSetTitle_x(void) const
        {
            return m_Graph->IsSetTitle_x();
        }
    const string& GetTitle_x(void) const
        {
            return m_Graph->GetTitle_x();
        }

    bool IsSetTitle_y(void) const
        {
            return m_Graph->IsSetTitle_y();
        }
    const string& GetTitle_y(void) const
        {
            return m_Graph->GetTitle_y();
        }

    bool IsSetComp(void) const
        {
            return m_Graph->IsSetComp();
        }
    TSeqPos GetComp(void) const
        {
            return m_Graph->GetComp();
        }

    bool IsSetA(void) const
        {
            return m_Graph->IsSetA();
        }
    double GetA(void) const
        {
            return m_Graph->GetA();
        }

    bool IsSetB(void) const
        {
            return m_Graph->IsSetB();
        }
    double GetB(void) const
        {
            return m_Graph->GetB();
        }

    TSeqPos GetNumval(void) const
        {
            return m_Graph->GetNumval();
        }

    const CSeq_graph::C_Graph& GetGraph(void) const
        {
            return m_Graph->GetGraph();
        }

private:
    friend class CGraph_CI;
    friend class CAnnot_CI;
    CMappedGraph& Set(const CAnnotObject_Ref& annot);

    CConstRef<CSeq_graph>         m_Graph;
    mutable CConstRef<CSeq_graph> m_MappedGraph;
    bool                          m_Partial;
    CConstRef<CSeq_loc>           m_MappedLoc;
};


class NCBI_XOBJMGR_EXPORT CGraph_CI : public CAnnotTypes_CI
{
public:
    CGraph_CI(void);
    CGraph_CI(CScope& scope, const CSeq_loc& loc,
              SAnnotSelector sel);
    CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              SAnnotSelector sel);
    // Search all TSEs in all datasources
    CGraph_CI(CScope& scope, const CSeq_loc& loc,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);
    // Search only in TSE, containing the bioseq
    CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);
    
    // Iterate all graphs from the object regardless of their location
    CGraph_CI(const CSeq_annot_Handle& annot);
    CGraph_CI(const CSeq_annot_Handle& annot,
              SAnnotSelector sel);
    CGraph_CI(CScope& scope, const CSeq_entry& entry);
    CGraph_CI(CScope& scope, const CSeq_entry& entry,
              SAnnotSelector sel);

    CGraph_CI(const CGraph_CI& iter);
    virtual ~CGraph_CI(void);
    CGraph_CI& operator= (const CGraph_CI& iter);

    CGraph_CI& operator++ (void);
    CGraph_CI& operator-- (void);
    operator bool (void) const;
    const CMappedGraph& operator* (void) const;
    const CMappedGraph* operator-> (void) const;
private:
    CGraph_CI operator++ (int);
    CGraph_CI operator-- (int);

    mutable CMappedGraph m_Graph; // current graph object returned by operator->()
};


inline
CGraph_CI::CGraph_CI(void)
{
    return;
}

inline
CGraph_CI::CGraph_CI(const CGraph_CI& iter)
    : CAnnotTypes_CI(iter)
{
    return;
}

inline
CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(scope, loc,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq,
                     TSeqPos start, TSeqPos stop,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(bioseq, start, stop,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(annot,
                     SAnnotSelector(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot,
                    SAnnotSelector sel)
    : CAnnotTypes_CI(annot,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI::CGraph_CI(CScope& scope, const CSeq_entry& entry)
    : CAnnotTypes_CI(scope, entry,
                     SAnnotSelector(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI::CGraph_CI(CScope& scope, const CSeq_entry& entry,
                    SAnnotSelector sel)
    : CAnnotTypes_CI(scope, entry,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_Graph))
{
}


inline
CGraph_CI& CGraph_CI::operator= (const CGraph_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    return *this;
}

inline
CGraph_CI& CGraph_CI::operator++ (void)
{
    Next();
    return *this;
}

inline
CGraph_CI& CGraph_CI::operator-- (void)
{
    Prev();
    return *this;
}

inline
CGraph_CI::operator bool (void) const
{
    return IsValid();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.27  2003/08/15 15:22:41  grichenk
* +CAnnot_CI
*
* Revision 1.26  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.25  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.24  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.23  2003/03/21 14:50:51  vasilche
* Added constructors of CAlign_CI and CGraph_CI taking generic
* SAnnotSelector parameters.
*
* Revision 1.22  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.21  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.20  2003/02/24 18:57:20  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.19  2003/02/13 14:57:36  dicuccio
* Changed interface to match new 'dataool'-generated code (built-in types
* returned by value, not reference).
*
* Revision 1.18  2003/02/13 14:34:31  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.17  2003/02/10 22:04:42  grichenk
* CGraph_CI resolves to CMappedGraph instead of CSeq_graph
*
* Revision 1.16  2003/02/10 15:51:26  grichenk
* + CMappedGraph
*
* Revision 1.15  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.14  2002/12/24 15:42:44  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.13  2002/12/06 15:35:57  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.12  2002/12/05 19:28:30  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.11  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.10  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.9  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.8  2002/04/30 14:30:42  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.7  2002/04/17 20:57:50  grichenk
* Added full type for "resolve" argument
*
* Revision 1.6  2002/04/05 21:26:17  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:12  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:46  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:02  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // GRAPH_CI__HPP
