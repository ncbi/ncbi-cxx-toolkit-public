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
*   Seq-align handle
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_align_Handle::CSeq_align_Handle(void)
{
}


CSeq_align_Handle::CSeq_align_Handle(const CSeq_annot_Handle& annot,
                                     size_t index)
    : m_Annot(annot),
      m_Index(index)
{
}


CSeq_align_Handle::~CSeq_align_Handle(void)
{
}


void CSeq_align_Handle::Reset(void)
{
    m_Index = 0;
    m_Annot.Reset();
}


const CSeq_align& CSeq_align_Handle::x_GetSeq_align(void) const
{
    _ASSERT(m_Annot);
    return m_Annot.x_GetInfo().GetAnnotObject_Info(m_Index).GetAlign();
}


CConstRef<CSeq_align> CSeq_align_Handle::GetSeq_align(void) const
{
    return ConstRef(&x_GetSeq_align());
}


CScope& CSeq_align_Handle::GetScope(void) const
{
    return GetAnnot().GetScope();
}


const CSeq_annot_Handle& CSeq_align_Handle::GetAnnot(void) const
{
    return m_Annot;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
 * Revision 1.2  2004/05/21 21:42:12  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/04 18:06:06  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
