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
#include <string.h>
#include <algorithm>

#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>

BEGIN_NCBI_SCOPE

namespace impl
{

// All methods of CDBHandlerStack() are protected where necessary by mutexes
// in calling functions

CDBHandlerStack::CDBHandlerStack()
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
        TContainer::reverse_iterator rcit;

        rcit = find_if(m_Stack.rbegin(), m_Stack.rend(), CFunctor(h));

        if ( rcit != m_Stack.rend() ) {
            m_Stack.erase((--rcit.base()), m_Stack.end());
        }
    } else {
        TContainer::iterator cit;

        cit = find_if(m_Stack.begin(), m_Stack.end(), CFunctor(h));

        if ( cit != m_Stack.end() ) {
            m_Stack.erase(cit, m_Stack.end());
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


void CDBHandlerStack::x_AddDetails(CDB_Exception& ex, const string& extra_msg,
                                   const CConnection* conn,
                                   const CDBParams* params) const
{
    ex.SetExtraMsg(extra_msg);
    if (conn != NULL) {
        ex.SetFromConnection(*conn);
    }
    if (params != NULL  &&  ex.GetParams().Empty()) {
        ex.SetParams(params);
    }
}

void CDBHandlerStack::PostMsg(CDB_Exception* ex, const string& extra_msg,
                              const CConnection* connection,
                              const CDBParams* params) const
{
    x_AddDetails(*ex, extra_msg, connection, params);
    REVERSE_ITERATE(TContainer, cit, m_Stack) {
        if ( cit->NotNull() && cit->GetNCObject().GetHandler()->HandleIt(ex) )
        {
            break;
        }
    }
}


bool CDBHandlerStack::HandleExceptions(const CDB_UserHandler::TExceptions&  exeptions,
                                       const string& extra_msg,
                                       const CConnection* connection,
                                       const CDBParams* params) const
{
    ITERATE(CDB_UserHandler::TExceptions, it, exeptions) {
        x_AddDetails(**it, extra_msg, connection, params);
    }

    REVERSE_ITERATE(TContainer, cit, m_Stack) {
        if ( cit->NotNull() && cit->GetNCObject().GetHandler()->HandleAll(exeptions) )
        {
            return true;
        }
    }

    return false;
}

bool CDBHandlerStack::HandleMessage(int severity, int msgnum, const string& message) const
{
    REVERSE_ITERATE(TContainer, cit, m_Stack) {
        if (cit->NotNull()
            && cit->GetNCObject().GetHandler()->HandleMessage(severity, msgnum, message))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
CDBHandlerStack::CUserHandlerWrapper::CUserHandlerWrapper(
    CDB_UserHandler* handler,
    bool guard
    ) :
    m_ObjGuard(guard ? handler : NULL),
    m_UserHandler(handler)
{
}

CDBHandlerStack::CUserHandlerWrapper::~CUserHandlerWrapper(void)
{
}

CDBHandlerStack::CUserHandlerWrapper::CObjGuard::CObjGuard(CObject* const obj) :
    m_Obj(obj)
{
    if (m_Obj) {
        m_Obj->AddReference();
    }
}

CDBHandlerStack::CUserHandlerWrapper::CObjGuard::CObjGuard(const CObjGuard& other) :
    m_Obj(other.m_Obj)
{
    if (m_Obj) {
        m_Obj->AddReference();
    }
}

CDBHandlerStack::CUserHandlerWrapper::CObjGuard::~CObjGuard(void)
{
    if (m_Obj) {
        // This call doesn't delete m_Obj even if reference
        // counter is equal to 0. And with this feature CObjGuard
        // differs from CRef.
        m_Obj->ReleaseReference();
    }
}


}


END_NCBI_SCOPE


