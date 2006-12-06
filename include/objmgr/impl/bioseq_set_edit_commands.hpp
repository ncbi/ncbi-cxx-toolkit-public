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
#include <objmgr/impl/edit_commands_impl.hpp>

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>

#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DEFINE_DB_FUNC(type)                                      \
template<>                                                        \
struct DBFunc<CBioseq_set_EditHandle,CBioseq_set::T##type> {      \
    typedef CBioseq_set_EditHandle          Handle;               \
    typedef CBioseq_set::T##type            T;                    \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;               \
    typedef TTrait::TRef                    TRef;                 \
    typedef TTrait::TConstRef               TConstRef;            \
                                                                  \
    static inline void Set(IEditSaver& saver,                     \
                           const Handle& handle, TConstRef data,  \
                           IEditSaver::ECallMode mode)            \
    { saver.SetBioseqSet##type(handle, data, mode); }             \
    static inline void Reset(IEditSaver& saver,                   \
                             const Handle& handle,                \
                             IEditSaver::ECallMode mode)          \
    { saver.ResetBioseqSet##type(handle, mode); }                 \
};

#define DEFINE_BIOSEQSET_SCALAR_CMD(type)                                \
template<>                                                               \
struct MemetoFunctions<CBioseq_set_EditHandle,CBioseq_set::T##type> {    \
    typedef CBioseq_set_EditHandle          TEditHandle;                 \
    typedef CBioseq_set::T##type            T;                           \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;                      \
    typedef TTrait::TValue                  TValue;                      \
    typedef TTrait::TStorage                TStorage;                    \
                                                                         \
    static inline bool IsSet(const TEditHandle& handle)                  \
    { return handle.IsSet##type(); }                                     \
    static inline TStorage Get(const TEditHandle& handle)                \
    { return handle.Get##type(); }                                       \
    static inline void Set(const TEditHandle& handle, TStorage data)     \
    { handle.x_RealSet##type(data); }                                    \
    static inline void Reset(const TEditHandle& handle)                  \
    { handle.x_RealReset##type(); }                                      \
};                                                                       \
                                                                         \
DEFINE_DB_FUNC(type)                                                     \
                                                                         \
typedef CSetValue_EditCommand<CBioseq_set_EditHandle, CBioseq_set::T##type>    \
                            CSet_BioseqSet##type##_EditCommand;          \
typedef CResetValue_EditCommand<CBioseq_set_EditHandle, CBioseq_set::T##type>  \
                            CReset_BioseqSet##type##_EditCommand; 


DEFINE_BIOSEQSET_SCALAR_CMD(Level)
DEFINE_BIOSEQSET_SCALAR_CMD(Release)
DEFINE_BIOSEQSET_SCALAR_CMD(Class)

#undef  DEFINE_BIOSEQSET_SCALAR_CMD


#define DEFINE_BIOSEQSET_REF_CMD(type)                                   \
DEFINE_CREF_TYPE(CBioseq_set::T##type);                                  \
                                                                         \
template<>                                                               \
struct MemetoFunctions<CBioseq_set_EditHandle,CBioseq_set::T##type> {    \
    typedef CBioseq_set_EditHandle          TEditHandle;                 \
    typedef CBioseq_set::T##type            T;                           \
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;                      \
    typedef TTrait::TValue                  TValue;                      \
    typedef TTrait::TStorage                TStorage;                    \
                                                                         \
    static inline bool IsSet(const TEditHandle& handle)                  \
    { return handle.IsSet##type(); }                                     \
    static inline TStorage Get(const TEditHandle& handle)                \
    { return TStorage(const_cast<TValue*>(&handle.Get##type())); }       \
    static inline void Set(const TEditHandle& handle, TStorage& data)    \
    { handle.x_RealSet##type(*data); }                                   \
    static inline void Reset(const TEditHandle& handle)                  \
    { handle.x_RealReset##type(); }                                      \
};                                                                       \
                                                                         \
DEFINE_DB_FUNC(type)                                                     \
                                                                         \
typedef CSetValue_EditCommand<CBioseq_set_EditHandle, CBioseq_set::T##type>    \
                            CSet_BioseqSet##type##_EditCommand;          \
typedef CResetValue_EditCommand<CBioseq_set_EditHandle, CBioseq_set::T##type>  \
                            CReset_BioseqSet##type##_EditCommand;      
                                                                         
DEFINE_BIOSEQSET_REF_CMD(Id)
DEFINE_BIOSEQSET_REF_CMD(Coll)
DEFINE_BIOSEQSET_REF_CMD(Date)

#undef DEFINE_BIOSEQSET_REF_CMD
#undef DEFINE_DB_FUNC

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<>
struct RemoveAction<CBioseq_set_EditHandle> {
    typedef CBioseq_set_EditHandle Handle;
    static inline void Do(CScope_Impl& scope,
                          const CSeq_entry_EditHandle& entry, 
                          const Handle&)
    { scope.SelectNone(entry); }
    static inline void Undo(CScope_Impl& scope,
                            const CSeq_entry_EditHandle& entry,
                            const Handle& handle)
    { scope.SelectSet(entry, handle); }

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


typedef CRemove_EditCommand<CBioseq_set_EditHandle>
                             CRemoveBioseq_set_EditCommand;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Entry>
class CAttachEntry_EditCommand : public IEditCommand
{
public:
    typedef CSeq_entry_EditHandle                    TReturn;
    typedef MemetoTrait<Entry,IsCRef<Entry>::value>  TTrait;
    typedef typename TTrait::TStorage                TStorage;
    typedef typename TTrait::TRef                    TRef;

    CAttachEntry_EditCommand(const CBioseq_set_EditHandle& handle, 
                                   TRef                          entry,
                                   int                           index,
                                   CScope_Impl&                  scope) 
        : m_Handle(handle), m_Entry(TTrait::Store(entry)),
          m_Index(index), m_Scope(scope)
    {}

    virtual ~CAttachEntry_EditCommand() {};

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        m_Return = m_Scope.AttachEntry(m_Handle,
                                       TTrait::Restore(m_Entry),
                                       m_Index);
        if (!m_Return) 
            return;

        tr.AddCommand(CRef<IEditCommand>(this));       
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            saver->Attach(m_Handle, m_Return, m_Index, IEditSaver::eDo);
        }
    }
    virtual void Undo()
    {
        IEditSaver* saver = GetEditSaver(m_Handle);
        m_Scope.RemoveEntry(m_Return);
        if (saver) {
            saver->Remove(m_Handle, m_Return, m_Index, IEditSaver::eUndo);
        }
    }

    TReturn GetRet() const { return m_Return; }

private:
    CBioseq_set_EditHandle m_Handle;
    TStorage               m_Entry;
    int                    m_Index;
    CScope_Impl&           m_Scope;

    TReturn                m_Return;
};

template<typename Entry>
struct CMDReturn<CAttachEntry_EditCommand<Entry> > {
    typedef CAttachEntry_EditCommand<Entry> TCMD;
    typedef typename TCMD::TReturn                        TReturn;
    static inline TReturn GetRet(TCMD* cmd) { return cmd->GetRet(); }
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2006/12/06 17:54:03  didenko
* Fixed compilation warning on MSVC
*
* Revision 1.3  2006/05/01 16:56:45  didenko
* Attach SeqEntry edit command revamp
*
* Revision 1.2  2006/01/25 18:59:03  didenko
* Redisgned bio objects edit facility
*
* Revision 1.1  2005/11/15 19:22:07  didenko
* Added transactions and edit commands support
*
* ===========================================================================
*/

#endif // OBJECTS_OBJMGR_IMPL___BIOSEQ_EDIT_COMMNADS__HPP
