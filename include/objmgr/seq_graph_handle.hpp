#ifndef SEQ_GRAPH_HANDLE__HPP
#define SEQ_GRAPH_HANDLE__HPP

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
*   Seq-graph handle
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>
#include <objects/seqres/Seq_graph.hpp>
#include <objmgr/seq_annot_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


class CSeq_annot_Handle;
class CMappedGraph;
class CAnnotObject_Info;

/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_graph_Handle --
///
///  Proxy to access seq-graph objects data
///

template<typename Handle>
class CSeq_annot_Add_EditCommand;
template<typename Handle>
class CSeq_annot_Replace_EditCommand;
template<typename Handle>
class CSeq_annot_Remove_EditCommand;

class NCBI_XOBJMGR_EXPORT CSeq_graph_Handle
{
public:
    CSeq_graph_Handle(void);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Annot && m_AnnotIndex != eNull && !IsRemoved());

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get an annotation handle for the current seq-graph
    const CSeq_annot_Handle& GetAnnot(void) const;

    /// Get constant reference to the current seq-graph
    CConstRef<CSeq_graph> GetSeq_graph(void) const;

    // Mappings for CSeq_graph methods
    bool IsSetTitle(void) const;
    const string& GetTitle(void) const;
    bool IsSetComment(void) const;
    const string& GetComment(void) const;
    bool IsSetLoc(void) const;
    const CSeq_loc& GetLoc(void) const;
    bool IsSetTitle_x(void) const;
    const string& GetTitle_x(void) const;
    bool IsSetTitle_y(void) const;
    const string& GetTitle_y(void) const;
    bool IsSetComp(void) const;
    TSeqPos GetComp(void) const;
    bool IsSetA(void) const;
    double GetA(void) const;
    bool IsSetB(void) const;
    double GetB(void) const;
    TSeqPos GetNumval(void) const;
    const CSeq_graph::TGraph& GetGraph(void) const;

    /// Return true if this Seq-graph was removed already
    bool IsRemoved(void) const;
    /// Remove the Seq-graph from Seq-annot
    void Remove(void) const;
    /// Replace the Seq-graph with new Seq-graph object.
    /// All indexes are updated correspondingly.
    void Replace(const CSeq_graph& new_obj) const;
    /// Update index after manual modification of the object
    void Update(void) const;

private:
    friend class CMappedGraph;
    friend class CSeq_annot_EditHandle;
    typedef Int4 TIndex;
    enum {
        eNull = kMax_I4
    };

    const CSeq_graph& x_GetSeq_graph(void) const;

    CSeq_graph_Handle(const CSeq_annot_Handle& annot, TIndex index);

    CSeq_annot_Handle          m_Annot;
    TIndex                     m_AnnotIndex;

private:
    friend class CSeq_annot_Add_EditCommand<CSeq_graph_Handle>;
    friend class CSeq_annot_Replace_EditCommand<CSeq_graph_Handle>;
    friend class CSeq_annot_Remove_EditCommand<CSeq_graph_Handle>;

    /// Remove the Seq-graph from Seq-annot
    void x_RealRemove(void) const;
    /// Replace the Seq-graph with new Seq-graph object.
    /// All indexes are updated correspondingly.
    void x_RealReplace(const CSeq_graph& new_obj) const;

};


inline
CSeq_graph_Handle::CSeq_graph_Handle(void)
    : m_AnnotIndex(eNull)
{
}


inline
const CSeq_annot_Handle& CSeq_graph_Handle::GetAnnot(void) const
{
    return m_Annot;
}


inline
CScope& CSeq_graph_Handle::GetScope(void) const
{
    return GetAnnot().GetScope();
}


inline
bool CSeq_graph_Handle::IsSetTitle(void) const
{
    return x_GetSeq_graph().IsSetTitle();
}


inline
const string& CSeq_graph_Handle::GetTitle(void) const
{
    return x_GetSeq_graph().GetTitle();
}


inline
bool CSeq_graph_Handle::IsSetComment(void) const
{
    return x_GetSeq_graph().IsSetComment();
}


inline
const string& CSeq_graph_Handle::GetComment(void) const
{
    return x_GetSeq_graph().GetComment();
}


inline
bool CSeq_graph_Handle::IsSetLoc(void) const
{
    return x_GetSeq_graph().IsSetLoc();
}


inline
const CSeq_loc& CSeq_graph_Handle::GetLoc(void) const
{
    return x_GetSeq_graph().GetLoc();
}


inline
bool CSeq_graph_Handle::IsSetTitle_x(void) const
{
    return x_GetSeq_graph().IsSetTitle_x();
}


inline
const string& CSeq_graph_Handle::GetTitle_x(void) const
{
    return x_GetSeq_graph().GetTitle_x();
}


inline
bool CSeq_graph_Handle::IsSetTitle_y(void) const
{
    return x_GetSeq_graph().IsSetTitle_y();
}


inline
const string& CSeq_graph_Handle::GetTitle_y(void) const
{
    return x_GetSeq_graph().GetTitle_y();
}


inline
bool CSeq_graph_Handle::IsSetComp(void) const
{
    return x_GetSeq_graph().IsSetComp();
}


inline
TSeqPos CSeq_graph_Handle::GetComp(void) const
{
    return x_GetSeq_graph().GetComp();
}


inline
bool CSeq_graph_Handle::IsSetA(void) const
{
    return x_GetSeq_graph().IsSetA();
}


inline
double CSeq_graph_Handle::GetA(void) const
{
    return x_GetSeq_graph().GetA();
}


inline
bool CSeq_graph_Handle::IsSetB(void) const
{
    return x_GetSeq_graph().IsSetB();
}


inline
double CSeq_graph_Handle::GetB(void) const
{
    return x_GetSeq_graph().GetB();
}


inline
TSeqPos CSeq_graph_Handle::GetNumval(void) const
{
    return x_GetSeq_graph().GetNumval();
}


inline
const CSeq_graph::TGraph& CSeq_graph_Handle::GetGraph(void) const
{
    return x_GetSeq_graph().GetGraph();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2006/09/20 14:00:19  vasilche
* Implemented user API to Update() annotation index.
*
* Revision 1.12  2005/11/15 19:22:06  didenko
* Added transactions and edit commands support
*
* Revision 1.11  2005/09/20 15:45:35  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.10  2005/08/23 17:03:01  vasilche
* Use CAnnotObject_Info pointer instead of annotation index in annot handles.
*
* Revision 1.9  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.8  2005/03/29 19:22:12  jcherry
* Added export specifiers
*
* Revision 1.7  2005/01/26 19:11:57  grichenk
* Added IsSetLoc()
*
* Revision 1.6  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.5  2004/12/28 18:40:30  vasilche
* Added GetScope() method.
*
* Revision 1.4  2004/12/22 15:56:12  vasilche
* Used CSeq_annot_Handle in annotations' handles.
*
* Revision 1.3  2004/09/29 19:49:37  kononenk
* Added doxygen formatting
*
* Revision 1.2  2004/08/25 20:44:52  grichenk
* Added operator bool() and operator !()
*
* Revision 1.1  2004/05/04 18:06:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_GRAPH_HANDLE__HPP
