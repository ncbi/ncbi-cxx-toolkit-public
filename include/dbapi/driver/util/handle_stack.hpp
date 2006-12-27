#ifndef DBAPI_DRIVER_UTIL___HANDLE_STACK__HPP
#define DBAPI_DRIVER_UTIL___HANDLE_STACK__HPP

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
 * File Description:  Stack of handlers
 *
 */

#include <dbapi/driver/exception.hpp>
#include <deque>


BEGIN_NCBI_SCOPE


class NCBI_DBAPIDRIVER_EXPORT CDBHandlerStack
{
public:
    CDBHandlerStack(size_t n = 8);
    CDBHandlerStack(const CDBHandlerStack& s);
    ~CDBHandlerStack(void);

    CDBHandlerStack& operator= (const CDBHandlerStack& s);

public:
    void Push(CDB_UserHandler* h, EOwnership ownership = eNoOwnership);
    void Pop (CDB_UserHandler* h, bool last = true);
    void SetExtraMsg(const string& msg);

    void PostMsg(CDB_Exception* ex);
    // Return TRUE if exceptions have been successfully processed.
    bool HandleExceptions(CDB_UserHandler::TExceptions& exeptions);


public:
    class CUserHandlerWrapper : public CObject
    {
    public:
        CUserHandlerWrapper(CDB_UserHandler* handler, bool guard = false) :
            m_ObjGuard(guard ? handler : NULL),
            m_UserHandler(handler)
        {
        }
        ~CUserHandlerWrapper(void)
        {
        }

        bool operator==(CDB_UserHandler* handler)
        {
            return m_UserHandler.GetPointer() == handler;
        }

        const CDB_UserHandler* GetHandler(void) const
        {
            return m_UserHandler.GetPointer();
        }
        CDB_UserHandler* GetHandler(void)
        {
            return m_UserHandler.GetPointer();
        }

    private:
        class CObjGuard
        {
        public:
            CObjGuard(CObject* const obj) :
                m_Obj(obj)
            {
                if (m_Obj) {
                    m_Obj->AddReference();
                }
            }
            CObjGuard(const CObjGuard& other) :
                m_Obj(other.m_Obj)
            {
                if (m_Obj) {
                    m_Obj->AddReference();
                }
            }
            ~CObjGuard(void)
            {
                if (m_Obj) {
                    m_Obj->ReleaseReference();
                }
            }

            CObjGuard& operator=(const CObjGuard& other)
            {
                if (this != &other) {
                    if (m_Obj) {
                        m_Obj->ReleaseReference();
                    }

                    m_Obj = other.m_Obj;

                    if (m_Obj) {
                        m_Obj->AddReference();
                    }
                }

                return *this;
            }

        private:
            CObject* m_Obj;
        };

        const CObjGuard         m_ObjGuard;
        CRef<CDB_UserHandler>   m_UserHandler;
    };

    typedef deque<CRef<CUserHandlerWrapper> > TContainer;

private:
    TContainer m_Stack;
};

END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_UTIL___HANDLE_STACK__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/12/27 21:02:17  ssikorsk
 * Added method SetExtraMsg() to CDBHandlerStack.
 *
 * Revision 1.10  2006/06/22 18:30:37  ssikorsk
 * Added copy ctor and copy operator to CDBHandlerStack::CUserHandlerWrapper::CObjGuard.
 *
 * Revision 1.9  2006/06/19 19:03:57  ssikorsk
 * Improved CDBHandlerStack::TContainer.
 *
 * Revision 1.8  2006/05/15 19:28:11  ssikorsk
 * Added EOwnership argument to the method Push.
 *
 * Revision 1.7  2006/05/10 14:42:34  ssikorsk
 * Added method HandleExceptions to CDBHandlerStack.
 *
 * Revision 1.6  2005/10/31 17:18:27  ssikorsk
 * Revamp CDBHandlerStack to use std::deque
 *
 * Revision 1.5  2003/02/13 15:40:50  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_EXPORT
 *
 * Revision 1.4  2001/11/06 17:58:07  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/09/27 20:08:31  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.2  2001/09/27 16:46:31  vakatov
 * Non-const (was const) exception object to pass to the user handler
 *
 * Revision 1.1  2001/09/21 23:39:54  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
