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

#include <objmgr/scope.hpp>
#include <objmgr/scope_transaction.hpp>
#include <objmgr/impl/scope_transaction_impl.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_ScopeTrans


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CScopeTransaction::CScopeTransaction(CScope& scope)
{
    CScope_Impl& impl = scope.GetImpl();
    x_Set( *impl.CreateTransaction() );
}

CScopeTransaction::~CScopeTransaction()
{
    try {
        RollBack();
    } catch (exception& ex) {
        ERR_POST_X(1, Fatal << "Exception cought in ~CScopeTransaction() : " 
                            << ex.what());
    } catch (...) {
        ERR_POST_X(2, Fatal
                   << "Unknown Exception cought in ~CScopeTransaction()");
    }
}

void CScopeTransaction::AddScope(CScope& scope)
{
    x_GetImpl().AddScope(scope.GetImpl());
}

void CScopeTransaction::Commit()
{
    x_GetImpl().Commit();
}
void CScopeTransaction::RollBack()
{
    x_GetImpl().RollBack();
}

IScopeTransaction_Impl& CScopeTransaction::x_GetImpl()
{
    return static_cast<IScopeTransaction_Impl&>(*m_Impl);
}

void CScopeTransaction::x_Set(IScopeTransaction_Impl& impl)
{
    m_Impl.Reset(&impl);
} 
END_SCOPE(objects)
END_NCBI_SCOPE
