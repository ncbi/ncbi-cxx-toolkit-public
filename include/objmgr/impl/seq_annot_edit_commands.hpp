#ifndef OBJECTS_OBJMGR_IMPL___SEQ_ANNOT_EDIT_COMMNADS__HPP
#define OBJECTS_OBJMGR_IMPL___SEQ_ANNOT_EDIT_COMMNADS__HPP

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

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/seq_graph_handle.hpp>

#include <objmgr/impl/edit_commands_impl.hpp>

#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<>
struct RemoveAction<CSeq_annot_EditHandle> {
    typedef CSeq_annot_EditHandle Handle;
    static inline void Do(CScope_Impl& scope,
                          const CSeq_entry_EditHandle& , 
                          const Handle& handle)
    {
        scope.RemoveAnnot(handle);
    }
    static inline void Undo(CScope_Impl& scope,
                            const CSeq_entry_EditHandle& entry,
                            const Handle& handle)
    {
        scope.AttachAnnot(entry, handle);
    }
    static inline void DoInDB(IEditSaver& saver,
                              const CSeq_entry_EditHandle& entry,
                              const Handle& handle )
    { saver.Remove(entry, handle, IEditSaver::eDo); }
    static inline void UndoInDB(IEditSaver& saver,
                                const CBioObjectId& ,
                                const CSeq_entry_EditHandle& entry,
                                const Handle& handle)
    { saver.Attach(entry, handle, IEditSaver::eUndo); }
};


typedef CRemove_EditCommand<CSeq_annot_EditHandle>
                             CRemoveAnnot_EditCommand;





///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename Handle>
struct AnnotDataResolver;

template<>
struct AnnotDataResolver<CSeq_feat_EditHandle> {
    typedef CSeq_feat_EditHandle THandle;
    typedef CSeq_feat        TData;
    static inline CConstRef<TData> GetData(const THandle& handle)
    { return handle.GetSeq_feat(); }
};

template<>
struct AnnotDataResolver<CSeq_align_Handle> {
    typedef CSeq_align_Handle THandle;
    typedef CSeq_align TData;
    static inline CConstRef<TData> GetData(const THandle& handle)
    { return handle.GetSeq_align(); }
};

template<>
struct AnnotDataResolver<CSeq_graph_Handle> {
    typedef CSeq_graph_Handle THandle;
    typedef CSeq_graph TData;
    static inline CConstRef<TData> GetData(const THandle& handle)
    { return handle.GetSeq_graph(); }
};
    
template<typename Handle>
class CSeq_annot_Replace_EditCommand : public IEditCommand
{
public:

    typedef AnnotDataResolver<Handle> TResolver;
    typedef typename TResolver::TData TData;

    CSeq_annot_Replace_EditCommand(const Handle& handle,
                                   const TData& data) 
        : m_Handle(handle), m_Data(&data),
          m_WasRemoved(handle.IsRemoved())
    {}

    virtual ~CSeq_annot_Replace_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        IEditSaver* saver = GetEditSaver(m_Handle.GetAnnot());
        if (!m_WasRemoved) {
            m_OldData = TResolver::GetData(m_Handle);
        }
        m_Handle.x_RealReplace(*m_Data);
        
        tr.AddCommand(CRef<IEditCommand>(this));       
        if (saver) {
            tr.AddEditSaver(saver);
            if( !m_WasRemoved )
                saver->Replace(m_Handle, *m_OldData, IEditSaver::eDo);
            else
                saver->Add(m_Handle.GetAnnot(),
                           *m_Data, IEditSaver::eDo);
        }
        
    }
    virtual void Undo() 
    {
        if (!m_WasRemoved)
            m_Handle.x_RealReplace(*m_OldData);
        else
            m_Handle.x_RealRemove();
        IEditSaver* saver = GetEditSaver(m_Handle.GetAnnot());
        if (saver) {
            if (!m_WasRemoved)
                saver->Replace(m_Handle, *m_Data, IEditSaver::eUndo);
            else
                saver->Remove(m_Handle.GetAnnot(), *m_Data, IEditSaver::eUndo);
        }
    }

private:
    Handle           m_Handle;
    CConstRef<TData> m_Data;
    bool             m_WasRemoved;

    CConstRef<TData> m_OldData;   
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename Handle>
class CSeq_annot_Add_EditCommand : public IEditCommand
{
public:

    typedef AnnotDataResolver<Handle> TResolver;
    typedef typename TResolver::TData TData;

    CSeq_annot_Add_EditCommand(const CSeq_annot_EditHandle& handle,
                               const TData& data)
        : m_Handle(handle), m_Data(&data)
    {}
    
    virtual ~CSeq_annot_Add_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        m_Ret = m_Handle.x_RealAdd(*m_Data);
        tr.AddCommand(CRef<IEditCommand>(this));       
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            saver->Add(m_Handle, *m_Data, IEditSaver::eDo);
        }
    }    
    virtual void Undo() 
    {
        IEditSaver* saver = GetEditSaver(m_Handle);        
        m_Ret.x_RealRemove();
        if (saver) {
            saver->Remove(m_Handle, *m_Data, IEditSaver::eUndo);
        }
    }

    Handle GetRet() const { return m_Ret; }

private:
    CSeq_annot_EditHandle m_Handle;
    CConstRef<TData>      m_Data;

    Handle                m_Ret;
};


template<typename Handle>
struct CMDReturn<CSeq_annot_Add_EditCommand<Handle> > {
    typedef CSeq_annot_Add_EditCommand<Handle> TCMD;
    typedef Handle                             TReturn;
    static inline TReturn GetRet(TCMD* cmd) { return cmd->GetRet(); }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename Handle>
class CSeq_annot_Remove_EditCommand : public IEditCommand
{
public:

    typedef AnnotDataResolver<Handle> TResolver;
    typedef typename TResolver::TData TData;

    CSeq_annot_Remove_EditCommand(const Handle& handle)
        : m_Handle(handle) {}
    
    virtual ~CSeq_annot_Remove_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        IEditSaver* saver = GetEditSaver(m_Handle.GetAnnot());
        m_Data = TResolver::GetData(m_Handle);
        m_Handle.x_RealRemove();
        tr.AddCommand(CRef<IEditCommand>(this));       
        if (saver) {
            tr.AddEditSaver(saver);
            saver->Remove(m_Handle.GetAnnot(), *m_Data, IEditSaver::eDo);
        }
    }    
    virtual void Undo() 
    {
        m_Handle.x_RealReplace(*m_Data);
        IEditSaver* saver = GetEditSaver(m_Handle.GetAnnot());
        if (saver) {
            saver->Add(m_Handle.GetAnnot(),
                       *m_Data, IEditSaver::eUndo);
            //saver->Replace(m_Handle, *m_Data, IEditSaver::eUndo);
        }
    }

private:
    Handle           m_Handle;
    CConstRef<TData> m_Data;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECTS_OBJMGR_IMPL___SEQ_ANNOT_EDIT_COMMNADS__HPP
