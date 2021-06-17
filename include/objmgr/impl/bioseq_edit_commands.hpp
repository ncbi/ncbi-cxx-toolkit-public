#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_EDIT_COMMNADS__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_EDIT_COMMNADS__HPP

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
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/edit_commands_impl.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/general/Int_fuzz.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CResetIds_EditCommand : public IEditCommand
{
public:
    typedef set<CSeq_id_Handle> TIds;

    CResetIds_EditCommand(const CBioseq_EditHandle& handle);
    virtual ~CResetIds_EditCommand();
    
    virtual void Do(IScopeTransaction_Impl& tr);
    virtual void Undo();

private:
    const CBioseq_EditHandle& m_Handle;
    TIds m_Ids;
        
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<bool add = true>
struct AddRemoveIdEditAction {
    static inline bool Do(const CBioseq_EditHandle& handle, 
                          const CSeq_id_Handle& id)
    {  
        return handle.x_RealAddId(id); 
    }
    static inline void Undo(const CBioseq_EditHandle& handle, 
                            const CSeq_id_Handle& id)
    { 
        handle.x_RealRemoveId(id); 
    }

    static inline void DoInDB(IEditSaver& saver,
                              const CBioseq_EditHandle& handle, 
                              const CSeq_id_Handle& id)
    { saver.AddId(handle,id,IEditSaver::eDo); }
    static inline void UndoInDB(IEditSaver& saver,
                                const CBioseq_EditHandle& handle, 
                                const CSeq_id_Handle& id)
    { saver.RemoveId(handle,id,IEditSaver::eUndo); }
};

template<>
struct AddRemoveIdEditAction<false> {
    static inline bool Do(const CBioseq_EditHandle& handle, 
                          const CSeq_id_Handle& id)
    { 
        return handle.x_RealRemoveId(id); 
    }
    static inline void Undo(const CBioseq_EditHandle& handle, 
                            const CSeq_id_Handle& id)
    { 
        handle.x_RealAddId(id); 
    }
    static inline void DoInDB(IEditSaver& saver,
                              const CBioseq_EditHandle& handle, 
                              const CSeq_id_Handle& id)
    { saver.RemoveId(handle,id,IEditSaver::eDo); }
    static inline void UndoInDB(IEditSaver& saver,
                                const CBioseq_EditHandle& handle, 
                                const CSeq_id_Handle& id)
    { saver.AddId(handle,id,IEditSaver::eUndo); }
};

template<bool add>
class CId_EditCommand : public IEditCommand
{
public: 
    typedef AddRemoveIdEditAction<add> TAction;

    CId_EditCommand(const CBioseq_EditHandle& handle, 
                    const CSeq_id_Handle& id)
        : m_Handle(handle), m_Id(id) {}

    virtual ~CId_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr)
    {
        m_Ret = TAction::Do(m_Handle, m_Id);
        if (m_Ret) {
            tr.AddCommand(CRef<IEditCommand>(this));
            IEditSaver* saver = GetEditSaver(m_Handle);
            if (saver) {
                tr.AddEditSaver(saver);
                TAction::DoInDB(*saver, m_Handle, m_Id);
            }
        }
    }

    virtual void Undo()
    {
        TAction::Undo(m_Handle, m_Id);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            TAction::UndoInDB(*saver, m_Handle, m_Id);
        }
    }

    bool GetRet() const { return m_Ret; }

private:
    CBioseq_EditHandle    m_Handle;
    const CSeq_id_Handle& m_Id;
    bool m_Ret;
};

typedef CId_EditCommand<true> CAddId_EditCommand;
typedef CId_EditCommand<false> CRemoveId_EditCommand;


template<bool add>
struct CMDReturn<CId_EditCommand<add> > {
    typedef CId_EditCommand<add> CMD;
    typedef bool TReturn;
    static inline TReturn GetRet(CMD* cmd) { return cmd->GetRet(); }
};



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_CREF_TYPE(CSeq_inst);

#define DEFINE_DB_FUNC(type)                                      \
template<>                                                        \
struct DBFunc<CBioseq_EditHandle,CSeq_inst::T##type> {            \
    typedef CBioseq_EditHandle              Handle;               \
    typedef CSeq_inst::T##type              T;                    \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;               \
    typedef TTrait::TRef                    TRef;                 \
    typedef TTrait::TConstRef               TConstRef;            \
                                                                  \
    static inline void Set(IEditSaver& saver,                     \
                           const Handle& handle, TConstRef data,  \
                           IEditSaver::ECallMode mode)            \
    { saver.SetSeqInst##type(handle, data, mode); }               \
    static inline void Reset(IEditSaver& saver,                   \
                             const Handle& handle,                \
                             IEditSaver::ECallMode mode)          \
    { saver.ResetSeqInst##type(handle, mode); }                   \
};


template<>
struct MemetoFunctions<CBioseq_EditHandle,CSeq_inst> {
    typedef CBioseq_EditHandle              TEditHandle;
    typedef CSeq_inst                       T;
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;
    typedef TTrait::TValue     TValue;
    typedef TTrait::TStorage   TStorage;

    static inline bool IsSet(const TEditHandle& handle)
    { return handle.IsSetInst(); }
    static inline TStorage Get(const TEditHandle& handle)
    { return TStorage(const_cast<TValue*>(&handle.GetInst())); }
    static inline void Set(const TEditHandle& handle, TStorage& data)
    { handle.x_RealSetInst(TTrait::Restore(data)); }   
    static inline void Reset(const TEditHandle& handle)
    { handle.x_RealResetInst(); }

};

template<>
struct DBFunc<CBioseq_EditHandle,CSeq_inst> {
    typedef CBioseq_EditHandle              Handle;
    typedef CSeq_inst                       T;
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;
    typedef TTrait::TRef                    TRef;
    typedef TTrait::TConstRef               TConstRef;

    static inline void Set(IEditSaver& saver,
                           const Handle& handle, TConstRef data,
                           IEditSaver::ECallMode mode)
    { saver.SetSeqInst(handle, data, mode); }
    static inline void Reset(IEditSaver& saver,
                             const Handle& handle,
                             IEditSaver::ECallMode mode)
    { saver.ResetSeqInst(handle, mode); }
};

typedef CSetValue_EditCommand<CBioseq_EditHandle, 
                              CSeq_inst> CSet_SeqInst_EditCommand;
typedef CResetValue_EditCommand<CBioseq_EditHandle, 
                                CSeq_inst> CReset_SeqInst_EditCommand;

#define DEFINE_SEQINST_SCALAR_CMD(type)                           \
template<>                                                        \
struct MemetoFunctions<CBioseq_EditHandle,CSeq_inst::T##type> {   \
    typedef CBioseq_EditHandle              TEditHandle;          \
    typedef CSeq_inst::T##type              T;                    \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;               \
    typedef TTrait::TValue                  TValue;               \
    typedef TTrait::TStorage                TStorage;             \
                                                                  \
    static inline bool IsSet(const TEditHandle& handle)           \
    { return handle.IsSetInst_##type(); }                         \
    static inline TStorage Get(const TEditHandle& handle)         \
    { return handle.GetInst_##type(); }                           \
    static inline void Set(const TEditHandle& handle, TStorage data)   \
    { handle.x_RealSetInst_##type(data); }                        \
    static inline void Reset(const TEditHandle& handle)           \
    { handle.x_RealResetInst_##type(); }                          \
};                                                                \
                                                                  \
DEFINE_DB_FUNC(type)                                              \
                                                                  \
typedef CSetValue_EditCommand<CBioseq_EditHandle, CSeq_inst::T##type>   \
                            CSet_SeqInst##type##_EditCommand;     \
typedef CResetValue_EditCommand<CBioseq_EditHandle, CSeq_inst::T##type> \
                            CReset_SeqInst##type##_EditCommand; 

DEFINE_SEQINST_SCALAR_CMD(Repr)
DEFINE_SEQINST_SCALAR_CMD(Mol)
DEFINE_SEQINST_SCALAR_CMD(Strand)
DEFINE_SEQINST_SCALAR_CMD(Length)
DEFINE_SEQINST_SCALAR_CMD(Topology)

#undef  DEFINE_SEQINST_SCALAR_CMD

#define DEFINE_SEQINST_REF_CMD(type)                                    \
DEFINE_CREF_TYPE(CSeq_inst::T##type);                                   \
                                                                        \
template<>                                                              \
struct MemetoFunctions<CBioseq_EditHandle,CSeq_inst::T##type> {         \
    typedef CBioseq_EditHandle              TEditHandle;                \
    typedef CSeq_inst::T##type              T;                          \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;                     \
    typedef TTrait::TValue                  TValue;                     \
    typedef TTrait::TStorage                TStorage;                   \
                                                                        \
    static inline bool IsSet(const TEditHandle& handle)                 \
    { return handle.IsSetInst_##type(); }                               \
    static inline TStorage Get(const TEditHandle& handle)               \
    { return  TStorage(const_cast<TValue*>(&handle.GetInst_##type())); }  \
    static inline void Set(const TEditHandle& handle, TStorage& data)   \
    { handle.x_RealSetInst_##type(*data); }                             \
    static inline void Reset(const TEditHandle& handle)                 \
    { handle.x_RealResetInst_##type(); }                                \
};                                                                      \
                                                                        \
DEFINE_DB_FUNC(type)                                                    \
                                                                        \
typedef CSetValue_EditCommand<CBioseq_EditHandle, CSeq_inst::T##type>         \
                            CSet_SeqInst##type##_EditCommand;           \
typedef CResetValue_EditCommand<CBioseq_EditHandle, CSeq_inst::T##type>       \
                            CReset_SeqInst##type##_EditCommand; 

DEFINE_SEQINST_REF_CMD(Fuzz)
DEFINE_SEQINST_REF_CMD(Seq_data)
DEFINE_SEQINST_REF_CMD(Ext)
DEFINE_SEQINST_REF_CMD(Hist)

#undef DEFINE_SEQINST_REF_CMD
#undef DEFINE_DB_FUNC


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<>
struct RemoveAction<CBioseq_EditHandle> {
    typedef CBioseq_EditHandle Handle;
    static inline void Do(CScope_Impl& scope,
                          const CSeq_entry_EditHandle& entry,
                          const Handle&)
    { scope.SelectNone(entry); }
    static inline void Undo(CScope_Impl& scope,
                            const CSeq_entry_EditHandle& entry,
                            const Handle& handle)
    { scope.SelectSeq(entry, handle);  }

    static inline void DoInDB(IEditSaver& saver,
                              const CSeq_entry_EditHandle& entry,
                              const Handle& handle)
    { saver.Detach(entry, handle, IEditSaver::eDo); }
    static inline void UndoInDB(IEditSaver& saver,
                                const CBioObjectId& old_id,
                                const CSeq_entry_EditHandle& entry,
                                const Handle& handle)
    { saver.Attach(old_id, entry, handle, IEditSaver::eUndo); }

};

typedef CRemove_EditCommand<CBioseq_EditHandle>
                             CRemoveBioseq_EditCommand;

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECTS_OBJMGR_IMPL___BIOSEQ_EDIT_COMMNADS__HPP
