#ifndef OBJECTS_OBJMGR_IMPL___SCOP_TRANSACTION_IMPL__HPP
#define OBJECTS_OBJMGR_IMPL___SCOP_TRANSACTION_IMPL__HPP

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

#include <objmgr/impl/scope_impl.hpp>

#include <list>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class IEditSaver;
class IScopeTransaction_Impl;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class NCBI_XOBJMGR_EXPORT IEditCommand : public CObject
{
public:

    virtual ~IEditCommand();

    virtual void Do(IScopeTransaction_Impl&) = 0;
    virtual void Undo() = 0;
};


///////////////////////////////////////////////////////////////////////////////
class NCBI_XOBJMGR_EXPORT CMultEditCommand : public IEditCommand
{
public:
    typedef CRef<IEditCommand>   TCommand;
    typedef list<TCommand>       TCommands;

    CMultEditCommand();
    virtual ~CMultEditCommand();

    void AddCommand(TCommand cmd);

    template<typename Iter>
    void AddCommands(Iter begin, Iter end)
    {
        m_Commands.insert(m_Commands.end(), begin, end);
    }

    virtual void Do(IScopeTransaction_Impl&);
    virtual void Undo();

private:

    TCommands m_Commands;
    TCommands m_NoOpCommands;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT IScopeTransaction_Impl : public CObject
{
public:

    typedef CRef<IEditCommand>   TCommand;

    virtual ~IScopeTransaction_Impl();
    
    virtual void AddCommand(TCommand) = 0;
    virtual void AddEditSaver(IEditSaver*) = 0;

    virtual void Commit() = 0;
    virtual void RollBack() = 0;

    virtual void AddScope(CScope_Impl&) = 0;
    virtual bool HasScope(CScope_Impl&) const = 0;

protected:
    virtual bool x_CanCommitRollBack() const = 0;
    //    virtual void x_Finish() = 0;
};

///////////////////////////////////////////////////////////////////////////////
class NCBI_XOBJMGR_EXPORT CScopeTransaction_Impl : public IScopeTransaction_Impl
{
public:

    typedef CRef<IEditCommand>   TCommand;
    typedef list<TCommand>       TCommands;
    typedef CRef<CScope_Impl>    TScope;
    typedef set<TScope>          TScopes;
    typedef set<IEditSaver*>     TEditSavers;

    CScopeTransaction_Impl(CScope_Impl& scope, IScopeTransaction_Impl* parent);
    virtual ~CScopeTransaction_Impl();
    
    virtual void AddCommand(TCommand cmd);
    virtual void AddEditSaver(IEditSaver*);

    virtual void Commit();
    virtual void RollBack();

    virtual void AddScope(CScope_Impl& scope);
    virtual bool HasScope(CScope_Impl& scope) const;

protected:

    virtual bool x_CanCommitRollBack() const;

protected:

    TCommands           m_Commands;
    TCommands::iterator m_CurCmd;
    TScopes             m_Scopes;
    TEditSavers         m_Savers;
    CRef<IScopeTransaction_Impl> m_Parent;

    void x_AddScope(CScope_Impl& scope);
    void x_DoFinish(IScopeTransaction_Impl*);
    
private:
    CScopeTransaction_Impl(const CScopeTransaction_Impl&);
    CScopeTransaction_Impl& operator=(const CScopeTransaction_Impl&);

};
/*
///////////////////////////////////////////////////////////////////////////////
class NCBI_XOBJMGR_EXPORT CScopeSubTransaction_Impl : public CScopeTransaction_Impl
{
public:

    CScopeSubTransaction_Impl(CScope_Impl& scope);

    virtual ~CScopeSubTransaction_Impl();

    virtual void Commit();

    virtual void AddScope(CScope_Impl& scope);
    virtual bool HasScope(CScope_Impl& scope) const;

    virtual void AddEditSaver(IEditSaver*);

protected:
    virtual void x_Finish();

private:
    CRef<IScopeTransaction_Impl> m_Parent;


private:
    CScopeSubTransaction_Impl(const CScopeTransaction_Impl&);
    CScopeSubTransaction_Impl& operator=(const CScopeTransaction_Impl&);
};
*/

END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJECTS_OBJMGR_IMPL___SCOP_TRANSACTION_IMPL__HPP
