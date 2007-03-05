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


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
///
///  CMappedGraph --
///

class NCBI_XOBJMGR_EXPORT CMappedGraph
{
public:
    /// Get original graph with unmapped location/product
    const CSeq_graph& GetOriginalGraph(void) const
        {
            return m_GraphRef->GetGraph();
        }

    /// Get containing annot handle
    CSeq_annot_Handle GetAnnot(void) const;

    /// Get original graph handle
    CSeq_graph_Handle GetSeq_graph_Handle(void) const;

    /// Graph mapped to the master sequence.
    /// WARNING! The function is rather slow and should be used with care.
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


/////////////////////////////////////////////////////////////////////////////
///
///  CGraph_CI --
///

class NCBI_XOBJMGR_EXPORT CGraph_CI : public CAnnotTypes_CI
{
public:
    /// Create an empty iterator
    CGraph_CI(void);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given bioseq
    CGraph_CI(const CBioseq_Handle& bioseq);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(const CBioseq_Handle& bioseq,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given bioseq
    CGraph_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              ENa_strand strand = eNa_strand_unknown);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              ENa_strand strand,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given seq-loc
    CGraph_CI(CScope& scope,
              const CSeq_loc& loc);

    /// Create an iterator that enumerates CSeq_graph objects 
    /// related to the given seq-loc
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(CScope& scope,
              const CSeq_loc& loc,
              const SAnnotSelector& sel);

    /// Iterate all graphs from the seq-annot regardless of their location
    CGraph_CI(const CSeq_annot_Handle& annot);

    /// Iterate all graphs from the seq-annot regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(const CSeq_annot_Handle& annot,
              const SAnnotSelector& sel);

    /// Iterate all graphs from the seq-entry regardless of their location
    CGraph_CI(const CSeq_entry_Handle& entry);

    /// Iterate all graphs from the seq-entry regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CGraph_CI(const CSeq_entry_Handle& entry,
              const SAnnotSelector& sel);

    virtual ~CGraph_CI(void);

    CGraph_CI& operator++ (void);
    CGraph_CI& operator-- (void);

    DECLARE_OPERATOR_BOOL(IsValid());

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
const CMappedGraph& CGraph_CI::operator* (void) const
{
    return m_Graph;
}


inline
const CMappedGraph* CGraph_CI::operator-> (void) const
{
    return &m_Graph;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // GRAPH_CI__HPP
