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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_align_Handle::CSeq_align_Handle(void)
{
    return;
}


CSeq_align_Handle::CSeq_align_Handle(CScope& scope,
                                     const CSeq_annot_Info& annot_info,
                                     size_t index)
    : m_Scope(scope),
      m_Annot(&annot_info),
      m_Index(index)
{
    return;
}


CSeq_align_Handle::~CSeq_align_Handle(void)
{
    return;
}


const CSeq_align& CSeq_align_Handle::x_GetSeq_align(void) const
{
    _ASSERT(m_Annot);
    return m_Annot->GetAnnotObject_Info(m_Index).GetAlign();
}


CConstRef<CSeq_align> CSeq_align_Handle::GetSeq_align(void) const
{
    return ConstRef(&x_GetSeq_align());
}


CSeq_annot_Handle CSeq_align_Handle::GetAnnot(void) const
{
    if ( m_Annot ) {
        return m_Scope.GetScope().GetSeq_annotHandle(*m_Annot->GetSeq_annotCore());
    }
    return CSeq_annot_Handle();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
