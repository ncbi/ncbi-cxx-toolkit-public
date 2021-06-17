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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// CHeapScope
//
/////////////////////////////////////////////////////////////////////////////


void CHeapScope::Set(CScope* scope)
{
    if ( scope ) {
        m_Scope.Reset(scope->m_Impl->m_HeapScope);
        _ASSERT(m_Scope);
    }
    else {
        m_Scope.Reset();
    }
}


CScope& CHeapScope::GetScope(void) const
{
    return static_cast<CScope&>(m_Scope.GetNCObject());
}


CScope* CHeapScope::GetScopeOrNull(void) const
{
    return static_cast<CScope*>(m_Scope.GetNCPointerOrNull());
}


CScope_Impl* CHeapScope::GetImpl(void) const
{
    return GetScope().m_Impl.GetPointer();
}


END_SCOPE(objects)
END_NCBI_SCOPE
