#ifndef OBJECTS_OBJMGR_IMPL___EDIT_COMMNADS_IMPL__HPP
#define OBJECTS_OBJMGR_IMPL___EDIT_COMMNADS_IMPL__HPP

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

#include <objmgr/impl/scope_transaction_impl.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/edit_saver.hpp>

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/tse_handle.hpp>

#include <objects/seq/Seq_descr.hpp>

#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

template<typename CMD>
struct CMDReturn {
    typedef bool TReturn;
    static inline TReturn GetRet(CMD*) { return false; }
};

class NCBI_XOBJMGR_EXPORT CCommandProcessor
{
public:
    CCommandProcessor(CScope_Impl& scope);

    template<typename CMD>
    typename CMDReturn<CMD>::TReturn run(CMD* cmd)
    {
        CRef<IEditCommand> rcmd(cmd);
        CRef<IScopeTransaction_Impl> tr( &m_Scope.GetTransaction() );
        cmd->Do( *tr );
        if (tr->ReferencedOnlyOnce())
            tr->Commit();
        return CMDReturn<CMD>::GetRet(cmd);
    }
    
private:   
    CScope_Impl& m_Scope;
    CRef<IScopeTransaction_Impl> m_Transaction;

private:

    void* operator new(size_t); // only stack allocation is allowed
    void operator delete(void*);
    CCommandProcessor(const CCommandProcessor&);
    CCommandProcessor& operator=(const CCommandProcessor&);
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Handle>
inline IEditSaver* GetEditSaver(const Handle& handle)
{
    const CTSE_Info& tse = handle.GetTSE_Handle().x_GetTSE_Info();
    IEditSaver *saver = tse.GetEditSaver().GetPointer();
    return saver;
}

template<typename T, bool cref = false>
struct MemetoTrait {
    typedef T TValue;
    typedef T TStorage;
    typedef T TRef;
    typedef T TConstRef;

    static inline TStorage Store(TRef t) {return t;}
    static inline TRef     Restore(TStorage t) {return t;}
};

template<typename T>
struct MemetoTrait<T,true> {
    typedef T         TValue;
    typedef CRef<T>   TStorage;
    typedef T&        TRef;
    typedef const T&  TConstRef;

    static inline TStorage Store(TRef t) {return TStorage(&t);}
    static inline TRef     Restore(TStorage t)  {return *t;}
};

template<typename T>
struct IsCRef { enum { value = 0 }; };

#define DEFINE_CREF_TYPE(T) \
template<> struct IsCRef<T> { enum { value = 1 }; }

template<typename TEditHandle, typename T>
struct MemetoFunctions {
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;
    typedef typename TTrait::TValue         TValue;
    typedef typename TTrait::TStorage       TStorage;
    typedef typename TTrait::TRef           TRef;
    typedef typename TTrait::TConstRef      TConstRef;

    static inline bool IsSet(const TEditHandle& handle);
    static inline TStorage Get(const TEditHandle& handle);
    static inline void Set(const TEditHandle& handle, const TStorage& data);
    static inline void Reset(const TEditHandle& handle);
};

template<typename T>
class CMemeto
{
public:
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;
    typedef typename TTrait::TStorage       TStorage;
    typedef typename TTrait::TRef           TRef;
    typedef typename TTrait::TConstRef      TConstRef;

    CMemeto() : m_Storage(), m_WasSet(false) {}

    CMemeto(TRef t) : m_Storage(TTrait::Store(t)), m_WasSet(true) {}

    template<typename TEditHandle>
    CMemeto(const TEditHandle& handle) 
    {
        typedef MemetoFunctions<TEditHandle,T> TFunc;
        m_WasSet = TFunc::IsSet(handle);
        if (m_WasSet) m_Storage = TFunc::Get(handle);
    }
    template<typename TEditHandle>
    void RestoreTo(const TEditHandle& handle) 
    {
        typedef MemetoFunctions<TEditHandle,T> TFunc;
        if (m_WasSet) TFunc::Set(handle, m_Storage);
        else          TFunc::Reset(handle);
    }
    bool WasSet() const { return m_WasSet; }

    TRef GetRefValue() const { return TTrait::Restore(m_Storage); }
private:
    TStorage m_Storage;
    bool     m_WasSet;
};

template<typename THandle, typename T>
struct DBFunc {
    typedef MemetoTrait<T,IsCRef<T>::value> TTrait;
    typedef typename TTrait::TRef           TRef;
    typedef typename TTrait::TConstRef      TConstRef;

    static inline void Set(IEditSaver&, const THandle&, 
                           TConstRef, IEditSaver::ECallMode);
    static inline void Reset(IEditSaver&, const THandle&, 
                             IEditSaver::ECallMode);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


DEFINE_CREF_TYPE(CSeq_descr);

template<typename TEditHandle>
struct MemetoFunctions<TEditHandle,CSeq_descr> { 
    typedef MemetoTrait<CSeq_descr,IsCRef<CSeq_descr>::value > TTrait;
    typedef typename TTrait::TValue                            TValue;
    typedef typename TTrait::TStorage                          TStorage;

    static inline bool IsSet(const TEditHandle& handle)
    { return handle.IsSetDescr(); }

    static inline TStorage Get(const TEditHandle& handle)
    { return TStorage( const_cast<TValue*>(&handle.GetDescr())); }

    static inline void Set(const TEditHandle& handle, TStorage& data)
    { handle.x_RealSetDescr(TTrait::Restore(data)); }
    static inline void Reset(const TEditHandle& handle) 
    { handle.x_RealResetDescr(); }
};

template<typename Handle>
struct DBFunc<Handle,CSeq_descr> {
    static inline void Set(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeq_descr& data, IEditSaver::ECallMode mode)
    { saver.SetDescr(handle, data, mode); }

    static inline void Reset(IEditSaver& saver, 
                             const Handle& handle, IEditSaver::ECallMode mode)
    { saver.ResetDescr(handle, mode); }

    static inline void Add(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeq_descr& data, IEditSaver::ECallMode mode)
    { saver.AddDescr(handle, data, mode); }
};

template<typename TEditHandle, typename T>
class CSetValue_EditCommand : public IEditCommand
{
public:
    typedef CMemeto<T>                   TMemeto;
    typedef typename TMemeto::TTrait     TTrait;
    typedef typename TMemeto::TRef       TRef;
    typedef typename TMemeto::TStorage   TStorage;
    typedef MemetoFunctions<TEditHandle,T>     TFunc;
    typedef DBFunc<TEditHandle,T>             TDBFunc;

    CSetValue_EditCommand(const TEditHandle& handle, TRef value) 
        : m_Handle(handle), m_Value(TTrait::Store(value)) {}

    virtual ~CSetValue_EditCommand() {}
    
    virtual void Do(IScopeTransaction_Impl& tr)
    {
        m_Memeto.reset(new TMemeto(m_Handle));
        TFunc::Set(m_Handle, m_Value);
        tr.AddCommand(CRef<IEditCommand>(this));
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            TDBFunc::Set(*saver, m_Handle, TTrait::Restore(m_Value), IEditSaver::eDo);
        }
    }

    virtual void Undo() 
    {
        _ASSERT(m_Memeto.get());
        m_Memeto->RestoreTo(m_Handle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            if( m_Memeto->WasSet() )
                TDBFunc::Set(*saver, m_Handle, m_Memeto->GetRefValue(), 
                             IEditSaver::eUndo);
            else
                TDBFunc::Reset(*saver, m_Handle, IEditSaver::eUndo);
        }
        m_Memeto.reset();
    }

private:

    TEditHandle       m_Handle;
    TStorage          m_Value;
    auto_ptr<TMemeto> m_Memeto;   
};

template<typename TEditHandle, typename T>
class CResetValue_EditCommand : public IEditCommand
{
public:
    typedef CMemeto<T>                   TMemeto;
    typedef typename TMemeto::TTrait     TTrait;
    typedef typename TMemeto::TRef       TRef;
    typedef MemetoFunctions<TEditHandle,T>     TFunc;
    typedef DBFunc<TEditHandle,T>             TDBFunc;

    CResetValue_EditCommand(const TEditHandle& handle) 
        : m_Handle(handle) {}

    virtual ~CResetValue_EditCommand() {}
    
    virtual void Do(IScopeTransaction_Impl& tr)
    {
        if (!TFunc::IsSet(m_Handle))
            return;
        m_Memeto.reset(new TMemeto(m_Handle));
        TFunc::Reset(m_Handle);
        tr.AddCommand(CRef<IEditCommand>(this));
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            TDBFunc::Reset(*saver, m_Handle, IEditSaver::eDo);
        }
    }

    virtual void Undo() 
    {
        _ASSERT(m_Memeto.get());
        m_Memeto->RestoreTo(m_Handle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            TDBFunc::Set(*saver, m_Handle, m_Memeto->GetRefValue(), 
                         IEditSaver::eUndo);
        }
        m_Memeto.reset();
    }

private:
    TEditHandle       m_Handle;
    auto_ptr<TMemeto> m_Memeto;
    
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename TEditHandle>
class CAddDescr_EditCommand : public IEditCommand
{
public:
    typedef CMemeto<CSeq_descr>          TSeq_descr_Memeto;
    typedef DBFunc<TEditHandle,CSeq_descr>    TDBFunc;

    CAddDescr_EditCommand(const TEditHandle& handle, CSeq_descr& descr)
        : m_Handle(handle), m_Descr(&descr) {}
    virtual ~CAddDescr_EditCommand() {}
    
    virtual void Do(IScopeTransaction_Impl& tr) 
    {       
        m_Memeto.reset(new TSeq_descr_Memeto(m_Handle));
        m_Handle.x_RealAddSeq_descr(*m_Descr);
        tr.AddCommand(CRef<IEditCommand>(this));
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            TDBFunc::Add(*saver, m_Handle, *m_Descr, IEditSaver::eDo);
        }
    }
    virtual void Undo() 
    {
        m_Memeto->RestoreTo(m_Handle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            if( m_Memeto->WasSet() )
                TDBFunc::Set(*saver, m_Handle, m_Memeto->GetRefValue(), 
                             IEditSaver::eUndo);
            else
                TDBFunc::Reset(*saver, m_Handle, IEditSaver::eUndo);
        }
        m_Memeto.reset();
    }

private:
    TEditHandle                 m_Handle;
    auto_ptr<TSeq_descr_Memeto> m_Memeto;
    CRef<CSeq_descr>            m_Descr;
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename Handle>
struct DescDBFunc {
    static inline void Add(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeqdesc& desc,
                           IEditSaver::ECallMode mode)
    { saver.AddDesc(handle, desc, mode); }
    static inline void Remove(IEditSaver& saver, 
                           const Handle& handle, 
                           const CSeqdesc& desc,
                           IEditSaver::ECallMode mode)
    { saver.RemoveDesc(handle, desc, mode); }
};

template<typename TEditHandle, bool add = true>
struct DescEditAction {
    typedef bool                         TReturn;

    static inline TReturn Do(const TEditHandle& handle, 
                             CSeqdesc& desc) 
    { return handle.x_RealAddSeqdesc(desc); }
    static inline void Undo(const TEditHandle& handle, 
                            CSeqdesc& desc)
    { handle.x_RealRemoveSeqdesc( desc ); }
    static inline void DoInDB(IEditSaver& saver,
                              const TEditHandle& handle, 
                              const CSeqdesc& desc)
    { DescDBFunc<TEditHandle>::Add(saver,handle, desc, IEditSaver::eDo);  }

    static inline void UndoInDB(IEditSaver& saver,
                                const TEditHandle& handle, 
                                const CSeqdesc& desc) 
    { DescDBFunc<TEditHandle>::Remove(saver,handle, desc, IEditSaver::eUndo);  }
};

template<typename TEditHandle>
struct DescEditAction<TEditHandle,false> {
    typedef CRef<CSeqdesc>               TReturn;

    static inline TReturn Do(const TEditHandle& handle, CSeqdesc& desc) 
    { return handle.x_RealRemoveSeqdesc(desc); }
    static inline void Undo(const TEditHandle& handle, CSeqdesc& desc) 
    { handle.x_RealAddSeqdesc( desc ); }
    static inline void DoInDB(IEditSaver&     saver,
                              const TEditHandle&   handle, 
                              const CSeqdesc& desc) 
    { DescDBFunc<TEditHandle>::Remove(saver,handle, desc, IEditSaver::eDo);  }
    static inline void UndoInDB(IEditSaver&     saver,
                                const TEditHandle&   handle, 
                                const CSeqdesc& desc) 
    { DescDBFunc<TEditHandle>::Add(saver,handle, desc, IEditSaver::eUndo);  }
};



template <typename TEditHandle, bool add>
class CDesc_EditCommand : public IEditCommand
{
public:
    typedef DescEditAction<TEditHandle,add>     TAction;
    typedef typename TAction::TReturn      TReturn;


    CDesc_EditCommand(const TEditHandle& handle, const CSeqdesc& desc)
        : m_Handle(handle), m_Desc(const_cast<CSeqdesc*>(&desc)) {}


    virtual ~CDesc_EditCommand() {}
    
    virtual void Do(IScopeTransaction_Impl& tr)
    {
        m_Ret = TAction::Do(m_Handle, *m_Desc);
        if (!m_Ret)
            return;
        tr.AddCommand(CRef<IEditCommand>(this));       
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            TAction::DoInDB(*saver, m_Handle, *m_Desc);
        }
    }

    virtual void Undo() 
    {
        TAction::Undo(m_Handle, *m_Desc);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            TAction::UndoInDB(*saver, m_Handle, *m_Desc);
        }
    }


    TReturn GetRet() const { return m_Ret; }
private:
    TEditHandle         m_Handle;
    CRef<CSeqdesc> m_Desc;
    TReturn        m_Ret;
};

template<typename TEditHandle, bool add>
struct CMDReturn<CDesc_EditCommand<TEditHandle,add> > {
    typedef CDesc_EditCommand<TEditHandle,add> TCMD;
    typedef typename TCMD::TReturn        TReturn;
    static inline TReturn GetRet(TCMD* cmd) { return cmd->GetRet(); }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename TEditHandle>
struct RemoveAction {
    static inline void Do(CScope_Impl& scope,
                          const CSeq_entry_EditHandle& entry, 
                          const TEditHandle& handle);
    static inline void Undo(CScope_Impl& scope,
                            const CSeq_entry_EditHandle& entry,
                            const TEditHandle& handle);

    static inline void DoInDB(IEditSaver& saver,
                              const CSeq_entry_EditHandle& entry,
                              const TEditHandle& handle);
    static inline void UndoInDB(IEditSaver& saver,
                                const CBioObjectId& old_id,
                                const CSeq_entry_EditHandle& entry,
                                const TEditHandle& handle);
};

template<typename TEditHandle>
class CRemove_EditCommand : public IEditCommand
{
public:
    typedef RemoveAction<TEditHandle> TAction;

    CRemove_EditCommand(const TEditHandle& handle,
                        CScope_Impl& scope)
        : m_Handle(handle), m_Scope(scope) {}


    virtual ~CRemove_EditCommand() {}

    virtual void Do(IScopeTransaction_Impl& tr) 
    {
        m_Entry = m_Handle.GetParentEntry();
        if (!m_Entry)
            return;    
        tr.AddCommand(CRef<IEditCommand>(this));
        IEditSaver* saver = GetEditSaver(m_Handle);
        TAction::Do(m_Scope, m_Entry, m_Handle);
        if (saver) {
            tr.AddEditSaver(saver);
            TAction::DoInDB(*saver, m_Entry, m_Handle);
        }
    }
    virtual void Undo() 
    {
        _ASSERT(m_Entry);
        CBioObjectId old_id(m_Entry.GetBioObjectId());
        TAction::Undo(m_Scope, m_Entry, m_Handle);
        IEditSaver* saver = GetEditSaver(m_Handle);
        if (saver) {
            TAction::UndoInDB(*saver,old_id, m_Entry, m_Handle);
        }
    }

     
private:
    CSeq_entry_EditHandle m_Entry;
    TEditHandle           m_Handle;
    CScope_Impl&          m_Scope;

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


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
* Revision 1.2  2005/11/15 21:54:07  ucko
* Portability fixes: make default CMDReturn yield a bool (false) rather
* than void, which confuses some compilers (WorkShop and MIPSpro); also
* unqualify std::size_t for MIPSpro's sake.
*
* Revision 1.1  2005/11/15 19:22:07  didenko
* Added transactions and edit commands support
*
* ===========================================================================
*/

#endif // OBJECTS_OBJMGR_IMPL___EDIT_COMMNADS_IMPL__HPP
