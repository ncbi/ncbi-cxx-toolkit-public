#ifndef DBAPI_DRIVER___DBAPI_DRIVER_EXCEPTION_STORAGE__HPP
#define DBAPI_DRIVER___DBAPI_DRIVER_EXCEPTION_STORAGE__HPP

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
 *
 */


#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE

namespace impl
{

/////////////////////////////////////////////////////////////////////////////
class CDBHandlerStack;

/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBExceptionStorage
{
public:
    CDBExceptionStorage(void);
    ~CDBExceptionStorage(void) throw();

public:
    void Accept(const CDB_Exception& e);
    void Handle(const CDBHandlerStack& handler);
    void Handle(const CDBHandlerStack& handler, const string& msg = kEmptyStr,
                const CConnection* conn = NULL, const CDBParams* par = NULL);
    void SetClosingConnect(bool value);
    bool IsClosingConnect(void);

private:
    CFastMutex                      m_Mutex;
    CDB_UserHandler::TExceptions    m_Exceptions;
    bool                            m_ClosingConnect;
};



inline void
CDBExceptionStorage::SetClosingConnect(bool value)
{
    m_ClosingConnect = value;
}

inline bool
CDBExceptionStorage::IsClosingConnect(void)
{
    return m_ClosingConnect;
}


}


void NCBI_DBAPIDRIVER_EXPORT
s_DelExceptionStorage(impl::CDBExceptionStorage* storage, void* data);

END_NCBI_SCOPE


#endif // DBAPI_DRIVER___DBAPI_DRIVER_EXCEPTION_STORAGE__HPP

