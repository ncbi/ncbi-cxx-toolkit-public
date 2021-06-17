#ifndef OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_EDIT_COMMNADS__HPP
#define OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_EDIT_COMMNADS__HPP

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
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objmgr/impl/edit_commands_impl.hpp>

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope_Impl;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Handle, typename Data>
struct SeqEntrySelectAction;

template<typename Data>
struct SeqEntrySelectAction<CBioseq_EditHandle, Data> {
    typedef MemetoTrait<Data,IsCRef<Data>::value> TTrait;
    typedef typename TTrait::TStorage             TStorage;

    static inline CBioseq_EditHandle Do(CScope_Impl&     scope, 
                            const CSeq_entry_EditHandle& handle, 
                            TStorage                     data)
    { return scope.SelectSeq(handle, TTrait::Restore(data)); }
};

template<typename Data>
struct SeqEntrySelectAction<CBioseq_set_EditHandle, Data> {
    typedef MemetoTrait<Data,IsCRef<Data>::value> TTrait;
    typedef typename TTrait::TStorage             TStorage;

    static inline CBioseq_set_EditHandle Do(CScope_Impl& scope, 
                            const CSeq_entry_EditHandle& handle, 
                            TStorage                     data)
    { return scope.SelectSet(handle, TTrait::Restore(data)); }
};

template<typename Handle, typename Data>
class CSeq_entry_Select_EditCommand : public IEditCommand
{
public:

    typedef SeqEntrySelectAction<Handle,Data>      Action;
    typedef MemetoTrait<Data,IsCRef<Data>::value>  TTrait;
    typedef typename TTrait::TStorage              TStorage;
    typedef typename TTrait::TRef                  TRef;

    CSeq_entry_Select_EditCommand(const CSeq_entry_EditHandle& handle,
                                  TRef                         data,
                                  CScope_Impl&                 scope) 
        : m_Handle(handle), m_Data(TTrait::Store(data)), m_Scope(scope) {}

    virtual ~CSeq_entry_Select_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        CBioObjectId old_id(m_Handle.GetBioObjectId());
        m_RetHandle = Action::Do(m_Scope, m_Handle, m_Data);
        if (!m_RetHandle)
            return;
        tr.AddCommand(CRef<IEditCommand>(this));       
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            saver->Attach(old_id, m_Handle, m_RetHandle, IEditSaver::eDo);
        }
        
    }
    virtual void Undo() 
    {
        m_Scope.SelectNone(m_Handle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            saver->Detach(m_Handle, m_RetHandle, IEditSaver::eUndo);
        }
    }
    Handle GetRet() const { return m_RetHandle; }

private:
    CSeq_entry_EditHandle m_Handle;
    TStorage              m_Data;
    Handle                m_RetHandle;
    CScope_Impl&          m_Scope;
};

template<typename Handle, typename Data>
struct CMDReturn<CSeq_entry_Select_EditCommand<Handle,Data> > {
    typedef CSeq_entry_Select_EditCommand<Handle,Data> CMD;
    typedef Handle                                     TReturn;
    static inline TReturn GetRet(CMD* cmd) { return cmd->GetRet(); }
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class NCBI_XOBJMGR_EXPORT CSeq_entry_SelectNone_EditCommand : public IEditCommand
{
public:
    CSeq_entry_SelectNone_EditCommand(const CSeq_entry_EditHandle& handle,
                                      CScope_Impl& scope);
    virtual ~CSeq_entry_SelectNone_EditCommand();

    virtual void Do(IScopeTransaction_Impl& tr);
    virtual void Undo();

private:
    CSeq_entry_EditHandle m_Handle;
    CScope_Impl&          m_Scope;
    CBioseq_EditHandle        m_BioseqHandle;
    CBioseq_set_EditHandle    m_BioseqSetHandle;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CSeq_entry_Remove_EditCommand : public IEditCommand
{
public:
    
    CSeq_entry_Remove_EditCommand(const CSeq_entry_EditHandle& handle,
                                  CScope_Impl& scope)
        : m_Handle(handle), m_Scope(scope), m_Index(-1) {}

    virtual ~CSeq_entry_Remove_EditCommand();

    virtual void Do(IScopeTransaction_Impl& tr);
    virtual void Undo();

private:
    CSeq_entry_EditHandle  m_Handle;
    CBioseq_set_EditHandle m_ParentHandle;
    CScope_Impl&           m_Scope;
    int                    m_Index;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CRemoveTSE_EditCommand : public IEditCommand
{
public:
    
    CRemoveTSE_EditCommand(const CSeq_entry_EditHandle& handle,
                           CScope_Impl& scope)
        : m_Handle(handle), m_Scope(scope) {}

    virtual ~CRemoveTSE_EditCommand();

    virtual void Do(IScopeTransaction_Impl& tr);
    virtual void Undo();

private:
    CSeq_entry_EditHandle  m_Handle;
    CScope_Impl&           m_Scope;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<>
struct DBFunc<CSeq_entry_EditHandle,CSeq_descr> {
    typedef CSeq_entry_EditHandle Handle;
    static inline void Set(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeq_descr& data, 
                           IEditSaver::ECallMode mode)
    { 
        if (handle.IsSeq()) 
            saver.SetDescr(handle.GetSeq(), data, mode); 
        else if (handle.IsSet())
            saver.SetDescr(handle.GetSet(), data, mode); 
    }

    static inline void Reset(IEditSaver& saver, 
                             const Handle& handle, 
                             IEditSaver::ECallMode mode)
    { 
        if (handle.IsSeq()) 
            saver.ResetDescr(handle.GetSeq(), mode); 
        else if (handle.IsSet()) 
            saver.ResetDescr(handle.GetSet(), mode); 
    }

    static inline void Add(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeq_descr& data, 
                           IEditSaver::ECallMode mode)
    { 
        if (handle.IsSeq()) 
            saver.AddDescr(handle.GetSeq(), data, mode); 
        else if (handle.IsSet()) 
            saver.AddDescr(handle.GetSet(), data, mode); 
    }
};

template<>
struct DescDBFunc<CSeq_entry_EditHandle> {
    typedef CSeq_entry_EditHandle Handle;
    static inline void Add(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeqdesc& desc,
                           IEditSaver::ECallMode mode)
    { 
        
        if (handle.IsSeq()) 
            saver.AddDesc(handle.GetSeq(),desc,mode); 
        else if (handle.IsSet()) 
            saver.AddDesc(handle.GetSet(),desc,mode); 
    }
    static inline void Remove(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeqdesc& desc,
                           IEditSaver::ECallMode mode)
    { 
        if (handle.IsSeq()) 
            saver.RemoveDesc(handle.GetSeq(),desc,mode); 
        else if (handle.IsSet()) 
            saver.RemoveDesc(handle.GetSet(),desc,mode); 
    }
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Annot>
class CAttachAnnot_EditCommand : public IEditCommand
{
public:
    typedef MemetoTrait<Annot,IsCRef<Annot>::value> TTrait;
    typedef typename TTrait::TStorage               TStorage;
    typedef typename TTrait::TRef                   TRef;

    CAttachAnnot_EditCommand(const CSeq_entry_EditHandle& handle, 
                             TRef                         annot,
                             CScope_Impl&                 scope) 
        : m_Handle(handle), m_Annot(TTrait::Store(annot)), m_Scope(scope)
    {}

    virtual ~CAttachAnnot_EditCommand() {};

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        m_AnnotHandle = m_Scope.AttachAnnot(m_Handle, 
                                            TTrait::Restore(m_Annot));
        if (!m_AnnotHandle)
            return;
        tr.AddCommand(CRef<IEditCommand>(this));       
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            saver->Attach(m_Handle, m_AnnotHandle, IEditSaver::eDo);
        }

    }
    virtual void Undo()
    {
        _ASSERT(m_AnnotHandle);
        m_Scope.RemoveAnnot(m_AnnotHandle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            saver->Remove(m_Handle, m_AnnotHandle, IEditSaver::eUndo);
        }
    }

    CSeq_annot_EditHandle GetRet() const { return m_AnnotHandle; }

private:
    CSeq_entry_EditHandle m_Handle;
    TStorage              m_Annot;
    CScope_Impl&          m_Scope;

    CSeq_annot_EditHandle m_AnnotHandle;

};

template<typename Annot>
struct CMDReturn<CAttachAnnot_EditCommand<Annot> > {
    typedef CAttachAnnot_EditCommand<Annot> CMD;
    typedef CSeq_annot_EditHandle           TReturn;
    static inline TReturn GetRet(CMD* cmd) { return cmd->GetRet(); }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_EDIT_COMMNADS__HPP
