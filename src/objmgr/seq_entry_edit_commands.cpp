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

#include <corelib/ncbiexpt.hpp>

#include <objmgr/impl/seq_entry_edit_commands.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSeq_entry_SelectNone_EditCommand::
CSeq_entry_SelectNone_EditCommand(const CSeq_entry_EditHandle& handle,
                                  CScope_Impl& scope)
    : m_Handle(handle), m_Scope(scope)
{
}
CSeq_entry_SelectNone_EditCommand::~CSeq_entry_SelectNone_EditCommand()
{
}

void CSeq_entry_SelectNone_EditCommand::Do(IScopeTransaction_Impl& tr)
{
    if (m_Handle.IsSeq())
        m_BioseqHandle = m_Handle.SetSeq();
    else if(m_Handle.IsSet())
        m_BioseqSetHandle = m_Handle.SetSet();
    else 
        return;
    tr.AddCommand(CRef<IEditCommand>(this));       
    IEditSaver* saver = GetEditSaver(m_Handle);
    m_Scope.SelectNone(m_Handle);
    if (saver) {
        tr.AddEditSaver(saver);
        if (m_BioseqHandle.IsRemoved())
            saver->Detach(m_Handle, m_BioseqHandle, IEditSaver::eDo);
        else if(m_BioseqSetHandle.IsRemoved())
            saver->Detach(m_Handle, m_BioseqSetHandle, IEditSaver::eDo);
    }

}
void CSeq_entry_SelectNone_EditCommand::Undo()
{
    IEditSaver* saver = GetEditSaver(m_Handle);
    CBioObjectId old_id(m_Handle.GetBioObjectId());
    if (m_BioseqHandle.IsRemoved()) {
        m_Scope.SelectSeq(m_Handle, m_BioseqHandle);
        if (saver)
            saver->Attach(old_id, m_Handle, m_BioseqHandle, IEditSaver::eUndo);
    }
    else if (m_BioseqSetHandle.IsRemoved()) {
        m_Scope.SelectSet(m_Handle, m_BioseqSetHandle);
        if (saver)
            saver->Attach(old_id,m_Handle, m_BioseqSetHandle, IEditSaver::eUndo);
    }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSeq_entry_Remove_EditCommand::~CSeq_entry_Remove_EditCommand()
{
}

void CSeq_entry_Remove_EditCommand::Do(IScopeTransaction_Impl& tr)
{
    _ASSERT(m_Handle.GetParentEntry()); // Does not handle TSE
    m_ParentHandle = m_Handle.GetParentBioseq_set();
    m_Index = m_ParentHandle.GetSeq_entry_Index(m_Handle);
    if( m_Index < 0 )
        return;
    tr.AddCommand(CRef<IEditCommand>(this));       
    IEditSaver* saver = GetEditSaver(m_Handle);
    m_Scope.RemoveEntry(m_Handle);
    if (saver) {
        tr.AddEditSaver(saver);
        saver->Remove(m_ParentHandle, m_Handle, m_Index, IEditSaver::eDo);
    }
}

void CSeq_entry_Remove_EditCommand::Undo()
{
    m_Scope.AttachEntry(m_ParentHandle, m_Handle, m_Index);
    IEditSaver* saver = GetEditSaver(m_Handle);
    if (saver) {
        saver->Attach(m_ParentHandle, m_Handle, m_Index, IEditSaver::eUndo);
    }
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CRemoveTSE_EditCommand::~CRemoveTSE_EditCommand()
{
}

void CRemoveTSE_EditCommand::Do(IScopeTransaction_Impl& tr)
{
    _ASSERT(!m_Handle.GetParentEntry()); // Handles TSE only
    CTSE_Handle tse = m_Handle.GetTSE_Handle();
    // TODO entry.Reset();
    IEditSaver* saver = GetEditSaver(m_Handle);
    m_Scope.RemoveTopLevelSeqEntry(tse);
    tr.AddCommand(CRef<IEditCommand>(this));       
    if (saver) {
        tr.AddEditSaver(saver);
        saver->RemoveTSE(tse, IEditSaver::eDo);
    }

}
void CRemoveTSE_EditCommand::Undo()
{
    _ASSERT(0);
    NCBI_THROW(CException, eUnknown, 
          "CRemoveTSE_EditCommand::Undo() is not implemented yet");
}

END_SCOPE(objects)
END_NCBI_SCOPE
