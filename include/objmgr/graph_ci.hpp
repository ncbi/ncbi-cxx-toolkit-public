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
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/seq_graph_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CMappedGraph
{
public:
    // Original graph with unmapped location/product
    const CSeq_graph& GetOriginalGraph(void) const
        {
            return m_GraphRef->GetGraph();
        }

    // Original graph handle
    CSeq_graph_Handle GetSeq_graph_Handle(void) const;

    // Graph mapped to the master sequence.
    // WARNING! The function is rather slow and should be used with care.
    const CSeq_graph& GetMappedGraph(void) const
        {
            if ( !m_MappedGraph ) {
                MakeMappedGraph();
            }
            return *m_MappedGraph;
        }

    bool IsSetTitle(void) const
        {
            return GetOriginalGraph().IsSetTitle();
        }
    const string& GetTitle(void) const
        {
            return GetOriginalGraph().GetTitle();
        }

    bool IsSetComment(void) const
        {
            return GetOriginalGraph().IsSetComment();
        }
    const string& GetComment(void) const
        {
            return GetOriginalGraph().GetComment();
        }

    const CSeq_loc& GetLoc(void) const
        {
            if ( !m_MappedLoc ) {
                MakeMappedLoc();
            }
            return *m_MappedLoc;
        }

    bool IsSetTitle_x(void) const
        {
            return GetOriginalGraph().IsSetTitle_x();
        }
    const string& GetTitle_x(void) const
        {
            return GetOriginalGraph().GetTitle_x();
        }

    bool IsSetTitle_y(void) const
        {
            return GetOriginalGraph().IsSetTitle_y();
        }
    const string& GetTitle_y(void) const
        {
            return GetOriginalGraph().GetTitle_y();
        }

    bool IsSetComp(void) const
        {
            return GetOriginalGraph().IsSetComp();
        }
    TSeqPos GetComp(void) const
        {
            return GetOriginalGraph().GetComp();
        }

    bool IsSetA(void) const
        {
            return GetOriginalGraph().IsSetA();
        }
    double GetA(void) const
        {
            return GetOriginalGraph().GetA();
        }

    bool IsSetB(void) const
        {
            return GetOriginalGraph().IsSetB();
        }
    double GetB(void) const
        {
            return GetOriginalGraph().GetB();
        }

    TSeqPos GetNumval(void) const
        {
            return GetOriginalGraph().GetNumval();
        }

    const CSeq_graph::C_Graph& GetGraph(void) const
        {
            return GetOriginalGraph().GetGraph();
        }

private:
    friend class CGraph_CI;
    friend class CAnnot_CI;

    typedef CAnnot_Collector::TAnnotSet TAnnotSet;
    typedef TAnnotSet::const_iterator   TIterator;

    void Set(CAnnot_Collector& collector,
             const TIterator& annot);

    void MakeMappedGraph(void) const;
    void MakeMappedLoc(void) const;

    mutable CRef<CAnnot_Collector> m_Collector;
    TIterator                      m_GraphRef;

    mutable CConstRef<CSeq_graph>   m_MappedGraph;
    mutable CConstRef<CSeq_loc>     m_MappedLoc;
};


class NCBI_XOBJMGR_EXPORT CGraph_CI : public CAnnotTypes_CI
{
public:
    CGraph_CI(void);
    CGraph_CI(CScope& scope, const CSeq_loc& loc,
              const SAnnotSelector& sel);
    CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              const SAnnotSelector& sel);
    // Search all TSEs in all datasources
    CGraph_CI(CScope& scope, const CSeq_loc& loc,
              SAnnotSelector::EOverlapType overlap_type
              = SAnnotSelector::eOverlap_Intervals,
              SAnnotSelector::EResolveMethod resolve
              = SAnnotSelector::eResolve_TSE);
    // Search only in TSE, containing the bioseq
    CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              SAnnotSelector::EOverlapType overlap_type
              = SAnnotSelector::eOverlap_Intervals,
              SAnnotSelector::EResolveMethod resolve
              = SAnnotSelector::eResolve_TSE);
    
    // Iterate all graphs from the object regardless of their location
    CGraph_CI(const CSeq_annot_Handle& annot);
    CGraph_CI(const CSeq_annot_Handle& annot,
              const SAnnotSelector& sel);
    CGraph_CI(const CSeq_entry_Handle& entry);
    CGraph_CI(const CSeq_entry_Handle& entry,
              const SAnnotSelector& sel);

    virtual ~CGraph_CI(void);

    CGraph_CI& operator++ (void);
    CGraph_CI& operator-- (void);
    operator bool (void) const;
    const CMappedGraph& operator* (void) const;
    const CMappedGraph* operator-> (void) const;
private:
    CGraph_CI operator++ (int);
    CGraph_CI operator-- (int);

    CMappedGraph m_Graph; // current graph object returned by operator->()
};


inline
CGraph_CI::CGraph_CI(void)
{
}

inline
CGraph_CI::CGraph_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, scope, loc, sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, bioseq, start, stop, sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI::CGraph_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, annot, sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI::CGraph_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Graph, entry, sel)
{
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
}


inline
CGraph_CI& CGraph_CI::operator++ (void)
{
    Next();
    if ( IsValid() ) {
        m_Graph.Set(GetCollector(), GetIterator());
    }
    return *this;
}

inline
CGraph_CI& CGraph_CI::operator-- (void)
{
    Prev();
    m_Graph.Set(GetCollector(), GetIterator());
    return *this;
}

inline
CGraph_CI::operator bool (void) const
{
    return IsValid();
}


inline
const CMappedGraph& CGraph_CI::operator* (void) const
{
    return m_Graph;
}


inline
const CMappedGraph* CGraph_CI::operator-> (void) const
{
    return &m_Graph;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.36  2004/05/04 18:08:47  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.35  2004/04/07 13:20:17  grichenk
* Moved more data from iterators to CAnnot_Collector
*
* Revision 1.34  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.33  2004/03/23 19:37:41  grichenk
* Added m_Graph in constructors
*
* Revision 1.32  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.31  2004/02/11 22:19:23  grichenk
* Fixed annot type initialization in iterators
*
* Revision 1.30  2004/02/05 19:53:40  grichenk
* Fixed type matching in SAnnotSelector. Added IncludeAnnotType().
*
* Revision 1.29  2004/02/04 18:05:32  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.28  2004/01/28 20:54:34  vasilche
* Fixed mapping of annotations.
*
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
