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

BEGIN_NCBI_SCOPE


CDBHandlerStack::CDBHandlerStack(size_t n)
{
    m_Room    = (n < 8) ? 8 : (int) n;
    m_Stack   = new CDB_UserHandler* [m_Room];
    m_TopItem = 0;
}


void CDBHandlerStack::Push(CDB_UserHandler* h)
{
    if (m_TopItem >= m_Room) {
        CDB_UserHandler** t = m_Stack;
        m_Room += m_Room / 2;
        m_Stack = new CDB_UserHandler* [m_Room];
        memcpy(m_Stack, t, m_TopItem * sizeof(CDB_UserHandler*));
        delete [] t;
    }
    m_Stack[m_TopItem++] = h;
}


void CDBHandlerStack::Pop(CDB_UserHandler* h, bool last)
{
    int i;

    if ( last ) {
        for (i = m_TopItem;  --i >= 0; ) {
            if (m_Stack[i] == h) {
                m_TopItem = i;
                break;
            }
        }
    }
    else {
        for (i = 0;  i < m_TopItem;  i++) {
            if (m_Stack[i] == h) {
                m_TopItem = i;
                break;
            }
        }
    }
}


CDBHandlerStack::CDBHandlerStack(const CDBHandlerStack& s)
{
    m_Stack = new CDB_UserHandler* [s.m_Room];
    memcpy(m_Stack, s.m_Stack, s.m_TopItem * sizeof(CDB_UserHandler*));
    m_TopItem = s.m_TopItem;
    m_Room    = s.m_Room;
}


CDBHandlerStack& CDBHandlerStack::operator= (const CDBHandlerStack& s)
{
    if (m_Room < s.m_TopItem) {
        delete [] m_Stack;
        m_Stack = new CDB_UserHandler* [s.m_Room];
        m_Room = s.m_Room;
    }
    memcpy(m_Stack, s.m_Stack, s.m_TopItem * sizeof(CDB_UserHandler*));
    m_TopItem = s.m_TopItem;
    return *this;
}


void CDBHandlerStack::PostMsg(CDB_Exception* ex)
{
    for (int i = m_TopItem;  --i >= 0; ) {
        if ( m_Stack[i]->HandleIt(ex) )
            break;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.2  2001/09/27 16:46:32  vakatov
 * Non-const (was const) exception object to pass to the user handler
 *
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
