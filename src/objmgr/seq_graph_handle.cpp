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


#include <ncbi_pch.hpp>
#include <objmgr/seq_graph_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/scope_impl.hpp>

#include <objmgr/impl/seq_annot_edit_commands.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_graph_Handle::CSeq_graph_Handle(const CSeq_annot_Handle& annot,
                                     TIndex index)
    : m_Annot(annot),
      m_AnnotIndex(index)
{
    _ASSERT(!IsRemoved());
    _ASSERT(annot.x_GetInfo().GetInfo(index).IsGraph());
}


void CSeq_graph_Handle::Reset(void)
{
    m_AnnotIndex = eNull;
    m_Annot.Reset();
}


const CSeq_graph& CSeq_graph_Handle::x_GetSeq_graph(void) const
{
    _ASSERT(m_Annot);
    const CAnnotObject_Info& info = m_Annot.x_GetInfo().GetInfo(m_AnnotIndex);
    if ( info.IsRemoved() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_graph_Handle: removed");
    }
    return info.GetGraph();
}


CConstRef<CSeq_graph> CSeq_graph_Handle::GetSeq_graph(void) const
{
    return ConstRef(&x_GetSeq_graph());
}


bool CSeq_graph_Handle::IsRemoved(void) const
{
    return m_Annot.x_GetInfo().GetInfo(m_AnnotIndex).IsRemoved();
}


void CSeq_graph_Handle::Remove(void) const
{
    typedef CSeq_annot_Remove_EditCommand<CSeq_graph_Handle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this));
}


void CSeq_graph_Handle::Replace(const CSeq_graph& new_obj) const
{
    typedef CSeq_annot_Replace_EditCommand<CSeq_graph_Handle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this, new_obj));
}

void CSeq_graph_Handle::Update(void) const
{
    GetAnnot().GetEditHandle().x_GetInfo().Update(m_AnnotIndex);
}

void CSeq_graph_Handle::x_RealRemove(void) const
{
    GetAnnot().GetEditHandle().x_GetInfo().Remove(m_AnnotIndex);
    _ASSERT(IsRemoved());
}


void CSeq_graph_Handle::x_RealReplace(const CSeq_graph& new_obj) const
{
    GetAnnot().GetEditHandle().x_GetInfo().Replace(m_AnnotIndex, new_obj);
    _ASSERT(!IsRemoved());
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/09/20 14:00:21  vasilche
 * Implemented user API to Update() annotation index.
 *
 * Revision 1.10  2005/11/15 19:22:08  didenko
 * Added transactions and edit commands support
 *
 * Revision 1.9  2005/09/20 15:45:36  vasilche
 * Feature editing API.
 * Annotation handles remember annotations by index.
 *
 * Revision 1.8  2005/08/23 17:03:01  vasilche
 * Use CAnnotObject_Info pointer instead of annotation index in annot handles.
 *
 * Revision 1.7  2005/04/07 16:30:42  vasilche
 * Inlined handles' constructors and destructors.
 * Optimized handles' assignment operators.
 *
 * Revision 1.6  2004/12/28 18:40:30  vasilche
 * Added GetScope() method.
 *
 * Revision 1.5  2004/12/22 15:56:12  vasilche
 * Used CSeq_annot_Handle in annotations' handles.
 *
 * Revision 1.4  2004/11/04 19:21:18  grichenk
 * Marked non-handle versions of SetLimitXXXX as deprecated
 *
 * Revision 1.3  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.2  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/04 18:06:07  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
