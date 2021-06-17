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
* Author: Maxim Didenko
*
* File Description:
*   Scope transaction
*
*/


#include <ncbi_pch.hpp>

#include <objmgr/impl/bioseq_edit_commands.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CResetIds_EditCommand::CResetIds_EditCommand(const CBioseq_EditHandle& handle)
    : m_Handle(handle)
{
}
CResetIds_EditCommand::~CResetIds_EditCommand() 
{
}

void CResetIds_EditCommand::Do(IScopeTransaction_Impl& tr)
{
    if (m_Handle.IsSetId()) {
        const CBioseq_EditHandle::TId& ids = m_Handle.GetId();
        m_Ids.insert(ids.begin(), ids.end());
        m_Handle.x_RealResetId();
        tr.AddCommand(CRef<IEditCommand>(this));
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            saver->ResetIds(m_Handle, m_Ids, IEditSaver::eDo);
        }
    }   
}
void CResetIds_EditCommand::Undo()
{
    ITERATE(TIds, it, m_Ids) {
        m_Handle.x_RealAddId(*it);
    }
    IEditSaver* saver = GetEditSaver(m_Handle);
    if (saver) {
        ITERATE(TIds, it, m_Ids) {
            saver->AddId(m_Handle, *it, IEditSaver::eUndo);
        }
    }             
}

END_SCOPE(objects)
END_NCBI_SCOPE
