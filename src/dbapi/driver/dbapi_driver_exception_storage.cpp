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
 * Author:  Sergey Sikorskiy
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/error_codes.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>

#include "dbapi_driver_exception_storage.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrUtil

BEGIN_NCBI_SCOPE

namespace impl
{

/////////////////////////////////////////////////////////////////////////////
struct SNoLock
{
    void operator() (CDB_UserHandler::TExceptions& /*resource*/) const {}
};

struct SUnLock
{
    void operator() (CDB_UserHandler::TExceptions& resource) const
    {
        CDB_UserHandler::ClearExceptions(resource);
    }
};

/////////////////////////////////////////////////////////////////////////////
CDBExceptionStorage::CDBExceptionStorage(void)
    : m_ClosingConnect(false),
      m_Retriable(eRetriable_Unknown),
      m_HasTimeout(false)
{
}


CDBExceptionStorage::~CDBExceptionStorage(void) throw()
{
    try {
        NON_CONST_ITERATE(CDB_UserHandler::TExceptions, it, m_Exceptions) {
            delete *it;
        }
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


void CDBExceptionStorage::Accept(unique_ptr<CDB_Exception>& e)
{
    static const size_t     kMaxItemsToCheck = 3;

    CFastMutexGuard mg(m_Mutex);

    // Note 1:
    // Sometimes the very same exception is pushed to the storage via different
    // paths. To avoid double output in a log file the accumulated list is
    // checked if there is this exception here already.
    // Note 2:
    // There are some cases though when the exception storage is used not for
    // the exceptions but for passing data. Sometimes there a lot of data and
    // checking the whole container introduces severe performance penalties. To
    // avoid them the only limited number of the latest items is checked.
    size_t checked_items = 0;
    for (auto it = m_Exceptions.rbegin();
         checked_items < kMaxItemsToCheck && it != m_Exceptions.rend();
         ++it) {
        if ((*it)->GetDBErrCode() == e->GetDBErrCode())
            if ((*it)->GetMsg() == e->GetMsg())
                return;     // the same exception has already been registered
        ++checked_items;
    }

    CDB_Exception *  raw_pointer = e.get();
    m_Exceptions.push_back(raw_pointer);
    e.release();

    const CDB_TimeoutEx *  timeout_exc =
                            dynamic_cast<const CDB_TimeoutEx *>(raw_pointer);
    if (timeout_exc)
        m_HasTimeout = true;
}


void CDBExceptionStorage::Handle(const CDBHandlerStack& handler,
                                 const CDB_Exception::SContext* dbg_info,
                                 const CConnection* conn,
                                 const CDBParams* par)
{
    typedef CGuard<CDB_UserHandler::TExceptions, SNoLock, SUnLock> TGuard;

    if (!m_Exceptions.empty()) {
        CFastMutexGuard mg(m_Mutex);
        TGuard guard(m_Exceptions);

        ERetriable retriable = m_Retriable;
        m_Retriable = eRetriable_Unknown;

        m_HasTimeout = false;

        try {
            handler.HandleExceptions(m_Exceptions, dbg_info, conn, par);
        } catch (CException &  exc) {
            exc.SetRetriable(retriable);
            throw;
        }
    }
}

void CDBExceptionStorage::SetRetriable(ERetriable retriable)
{
    if (m_Retriable == eRetriable_No)
        return;
    m_Retriable = retriable;
}

}

void s_DelExceptionStorage(impl::CDBExceptionStorage* storage, void* /* data */)
{
    delete storage;
}

END_NCBI_SCOPE

