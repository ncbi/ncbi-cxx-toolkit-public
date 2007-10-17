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

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/scope_transaction_impl.hpp>

#include <objmgr/edit_saver.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_ScopeTrans


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

///////////////////////////////////////////////////////////////////////////////
IEditCommand::~IEditCommand()
{
}

///////////////////////////////////////////////////////////////////////////////
CMultEditCommand::CMultEditCommand()
{
}
CMultEditCommand::~CMultEditCommand()
{
}

void CMultEditCommand::AddCommand(TCommand cmd)
{
    m_Commands.push_back(cmd);
}

void CMultEditCommand::Do(IScopeTransaction_Impl& tr)
{
    NON_CONST_ITERATE(TCommands, it, m_Commands) {
        (*it)->Do(tr);
    }
}
void CMultEditCommand::Undo()
{
    TCommands::reverse_iterator rit = m_Commands.rbegin();
    for( ; rit != m_Commands.rend(); ++rit) {
        (*rit)->Undo();
    }
}
///////////////////////////////////////////////////////////////////////////////
IScopeTransaction_Impl::~IScopeTransaction_Impl()
{
}

///////////////////////////////////////////////////////////////////////////////
CScopeTransaction_Impl::CScopeTransaction_Impl(CScope_Impl& scope, 
                                               IScopeTransaction_Impl* parent)
    : m_Parent(parent)
{
    m_CurCmd = m_Commands.begin();
    x_AddScope(scope);
}

CScopeTransaction_Impl::~CScopeTransaction_Impl()
{
    try {
        RollBack();
    } catch (exception& ex) {
        ERR_POST_X(3, Fatal << "Exception cought in ~CScopeTransaction_Impl() : " 
                            << ex.what());
    } catch (...) {
        ERR_POST_X(4, Fatal
                   << "Unknown Exception cought in ~CScopeTransaction_Impl()");
    }
}

void CScopeTransaction_Impl::AddCommand(TCommand cmd)
{
    m_Commands.erase(m_CurCmd, m_Commands.end());
    m_Commands.push_back(cmd);
    m_CurCmd = m_Commands.end();
}

void CScopeTransaction_Impl::AddEditSaver(IEditSaver* saver)
{
    if (saver) {
        if (m_Parent)
            m_Parent->AddEditSaver(saver);
        else {
            if (m_Savers.find(saver) == m_Savers.end() ) {
                saver->BeginTransaction();
                m_Savers.insert(saver);
            }
        }
    }
}
void CScopeTransaction_Impl::AddScope(CScope_Impl& scope)
{
    x_AddScope(scope);
}

void CScopeTransaction_Impl::x_AddScope(CScope_Impl& scope)
{
    if (m_Parent) 
        m_Parent->AddScope(scope);
    m_Scopes.insert(Ref(&scope));
}

bool CScopeTransaction_Impl::HasScope(CScope_Impl& scope) const
{
    if (m_Parent) 
        return m_Parent->HasScope(scope);
    return m_Scopes.find(Ref(&scope)) != m_Scopes.end();
}

bool CScopeTransaction_Impl::x_CanCommitRollBack() const
{
    //    if (m_Parent) 
    //        return m_Parent->x_CanCommitRollBack();
    ITERATE(TScopes, it, m_Scopes) {
        if ( static_cast<const IScopeTransaction_Impl*>(this)
             != &const_cast<TScope&>(*it)->GetTransaction() )
            return false;
    }
    return true;
}

void CScopeTransaction_Impl::Commit()
{
    if (!x_CanCommitRollBack()) {
        NCBI_THROW(CObjMgrException, eTransaction,
                       "This Transaction is not a top level transaction");
    }

    if (m_Parent) {
        if (m_Commands.size() == 1 ) {
            m_Parent->AddCommand(*m_Commands.begin());
        } else {
            auto_ptr<CMultEditCommand> cmd(new CMultEditCommand);
            cmd->AddCommands(m_Commands.begin(), m_CurCmd);
            m_Parent->AddCommand(CRef<IEditCommand>(cmd.release()));
        }
    } else {
        ITERATE(TEditSavers, saver_it, m_Savers) {
            IEditSaver* saver = *saver_it;        
            if ( saver ) {
                try {
                    saver->CommitTransaction();
                } catch (exception& ex) {
                    ERR_POST_X(5, Fatal << "Couldn't commit transaction : " 
                                        << ex.what());
                } catch (...) {
                    ERR_POST_X(6, Fatal << "Couldn't commit transaction");
                }
            }
        }
    }
    x_DoFinish(m_Parent.GetPointer());
}

void CScopeTransaction_Impl::RollBack()
{
    if (!x_CanCommitRollBack()) {
        NCBI_THROW(CObjMgrException, eTransaction,
                       "This Transaction is not a top level transaction");
    }
    m_Commands.erase(m_CurCmd, m_Commands.end());
    TCommands::reverse_iterator it;
    for( it = m_Commands.rbegin(); it != m_Commands.rend(); ++it)
        (*it)->Undo();
    if (!m_Parent) {
        ITERATE(TEditSavers, saver_it, m_Savers) {
            IEditSaver* saver = *saver_it;        
            if ( saver ) {
                try {
                    saver->RollbackTransaction();
                } catch (exception& ex) {
                    ERR_POST_X(7, Fatal << "Couldn't rollback transaction : " 
                                        << ex.what());
                } catch (...) {
                    ERR_POST_X(8, Fatal << "Couldn't rollback transaction");
                }
            }
        }
    }
    x_DoFinish(m_Parent.GetPointer());
}

void CScopeTransaction_Impl::x_DoFinish(IScopeTransaction_Impl* parent)
{
    m_Commands.clear();
    m_CurCmd = m_Commands.begin();
    NON_CONST_ITERATE(TScopes, it, m_Scopes) {
        const_cast<TScope&>(*it)->SetActiveTransaction(parent);
    }
    m_Scopes.clear();
    m_Savers.clear();
}

/*
///////////////////////////////////////////////////////////////////////////////
CScopeSubTransaction_Impl::CScopeSubTransaction_Impl(CScope_Impl& scope)
    : CScopeTransaction_Impl(scope), m_Parent(&scope.GetTransaction())
{
    m_Parent->AddScope(scope);
    scope.SetActiveTransaction(this);
}

CScopeSubTransaction_Impl::~CScopeSubTransaction_Impl()
{
}


void CScopeSubTransaction_Impl::AddScope(CScope_Impl& scope)
{
    CScopeTransaction_Impl::AddScope(scope);
    m_Parent->AddScope(scope);
}
bool CScopeSubTransaction_Impl::HasScope(CScope_Impl& scope) const
{
    if ( CScopeTransaction_Impl::HasScope(scope) )
        return true;
    return m_Parent->HasScope(scope);
}

void CScopeSubTransaction_Impl::AddEditSaver(IEditSaver* saver)
{
    m_Parent->AddEditSaver(saver);
}

void CScopeSubTransaction_Impl::Commit()
{
    if (!CanCommitRollBack()) {
        NCBI_THROW(CObjMgrException, eTransaction,
                       "This Transaction is not a top level transaction");
    }
    if (m_Commands.size() == 1 ) {
        m_Parent->AddCommand(*m_Commands.begin());
    } else {
        auto_ptr<CMultEditCommand> cmd(new CMultEditCommand);
        cmd->AddCommands(m_Commands.begin(), m_CurCmd);
        m_Parent->AddCommand(CRef<IEditCommand>(cmd.release()));
    }
    x_Finish();
}

void CScopeSubTransaction_Impl::x_Finish()
{
    x_DoFinish(&*m_Parent);
}
*/

END_SCOPE(objects)
END_NCBI_SCOPE
