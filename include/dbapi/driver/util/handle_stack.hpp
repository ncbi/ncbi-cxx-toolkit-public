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
* File Description:  Handlers Stack
*
*
*/

#include <dbapi/driver/exception.hpp>


BEGIN_NCBI_SCOPE


class CDBHandlerStack
{
public:
    CDBHandlerStack(size_t n = 8);
    CDBHandlerStack(const CDBHandlerStack& s);
    CDBHandlerStack& operator= (const CDBHandlerStack& s);

    void Push(CDB_UserHandler* h);
    void Pop (CDB_UserHandler* h, bool last = true);

    void PostMsg(CDB_Exception* ex);

    ~CDBHandlerStack() {
        delete [] m_Stack;
    }
    
private:
    CDB_UserHandler** m_Stack;
    int               m_TopItem;
    int               m_Room;
};


END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_UTIL___HANDLE_STACK__HPP */



/*
 * ===========================================================================
 * $Log$
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
