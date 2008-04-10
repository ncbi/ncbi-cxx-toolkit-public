/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  Handlers Stack
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <string.h>
#include <algorithm>

BEGIN_NCBI_SCOPE


CDBHandlerStack::CDBHandlerStack(size_t)
{
}

CDBHandlerStack::~CDBHandlerStack()
{
}

void CDBHandlerStack::Push(CDB_UserHandler* h, EOwnership ownership)
{
    CHECK_DRIVER_ERROR(h == NULL, "An attempt to pass NULL instead of "
                       "a valid CDB_UserHandler object", 0);

    CRef<CUserHandlerWrapper>
        obj(new CUserHandlerWrapper(h, ownership == eNoOwnership));

    m_Stack.push_back(TContainer::value_type(obj));
}

namespace
{
    class CFunctor
    {
    public:
        CFunctor(CDB_UserHandler* const h) :
            m_Handler(h)
        {
        }

        bool operator()(const CDBHandlerStack::TContainer::value_type& hwrapper)
        {
            return hwrapper->GetHandler() == m_Handler;
        }

    private:
        CDB_UserHandler* const m_Handler;
    };
}

void CDBHandlerStack::Pop(CDB_UserHandler* h, bool last)
{
    CHECK_DRIVER_ERROR(h == NULL, "An attempt to pass NULL instead of "
                       "a valid CDB_UserHandler object", 0);

    if ( last ) {
        while ( !m_Stack.empty() ) {
            if (m_Stack.back().GetObject() == h) {
                m_Stack.pop_back();
                break;
            } else {
                m_Stack.pop_back();
            }
        }
    } else {
        TContainer::iterator cit;

        cit = find_if(m_Stack.begin(), m_Stack.end(), CFunctor(h));

        if ( cit != m_Stack.end() ) {
            m_Stack.erase(cit, m_Stack.end());
        }
    }
}


void CDBHandlerStack::SetExtraMsg(const string& msg)
{
    NON_CONST_ITERATE(TContainer, cit, m_Stack) {
        if ( cit->NotNull() ) {
            (*cit)->GetHandler()->SetExtraMsg(msg);
        }
    }
}

CDBHandlerStack::CDBHandlerStack(const CDBHandlerStack& s) :
m_Stack( s.m_Stack )
{
    return;
}


CDBHandlerStack& CDBHandlerStack::operator= (const CDBHandlerStack& s)
{
    if ( this != &s ) {
        m_Stack = s.m_Stack;
    }
    return *this;
}


void CDBHandlerStack::PostMsg(CDB_Exception* ex)
{
    // Attempting to use m_Stack directly on WorkShop fails because it
    // tries to call the non-const version of rbegin(), and the
    // resulting reverse_iterator can't automatically be converted to
    // a const_reverse_iterator.  (Sigh.)
    TContainer& s = m_Stack;
    NON_CONST_REVERSE_ITERATE(TContainer, cit, s) {
        if ( cit->NotNull() && (*cit)->GetHandler()->HandleIt(ex) ) {
            break;
        }
    }
}


bool CDBHandlerStack::HandleExceptions(CDB_UserHandler::TExceptions& exeptions)
{
    TContainer& s = m_Stack;
    NON_CONST_REVERSE_ITERATE(TContainer, cit, s) {
        if ( cit->NotNull() && (*cit)->GetHandler()->HandleAll(exeptions) ) {
            return true;
        }
    }

    return false;
}

END_NCBI_SCOPE


