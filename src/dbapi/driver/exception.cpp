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
* File Description:  Exceptions
*
*
*/

#include <dbapi/driver/exception.hpp>



BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDB_Exception::
//

CDB_Exception::CDB_Exception(EType type, EDBSeverity severity, int err_code,
                             const string& originated_from, const string& msg)
{
    m_Type           = type;
    m_Severity       = severity;
    m_ErrCode        = err_code;
    m_OriginatedFrom = originated_from;
    m_Message        = msg;
}


CDB_Exception::~CDB_Exception()
{
    return;
}


const char* CDB_Exception::what() const
{
    return Message().c_str();
}


CDB_Exception* CDB_Exception::Clone() const
{
    return new CDB_Exception
        (m_Type, m_Severity, m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_DSEx::
//

CDB_Exception* CDB_DSEx::Clone() const
{
    return new CDB_DSEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_RPCEx::
//

CDB_RPCEx::CDB_RPCEx(EDBSeverity severity, int err_code,
                     const string& originated_from, const string& msg,
                     const string& proc_name, int proc_line)
    : CDB_Exception(eRPC, severity, err_code, originated_from, msg)
{
    static const string s_UnknownProcName = "Unknown";
    m_ProcName = proc_name.empty() ? s_UnknownProcName : proc_name;
    m_ProcLine = proc_line;
}

CDB_Exception* CDB_RPCEx::Clone() const
{
    return new CDB_RPCEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message,
         m_ProcName, m_ProcLine);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_SQLEx::
//

CDB_SQLEx::CDB_SQLEx(EDBSeverity severity, int err_code,
                     const string& originated_from, const string& msg,
                     const string& sql_state, int batch_line)
    : CDB_Exception(eSQL, severity, err_code, originated_from, msg)
{
    static const string s_UnknownSqlState = "Unknown";
    m_SqlState  = sql_state.empty() ? s_UnknownSqlState : sql_state;
    m_BatchLine = batch_line;
}

CDB_Exception* CDB_SQLEx::Clone() const
{
    return new CDB_SQLEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message,
         m_SqlState, m_BatchLine);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_DeadlockEx::
//

CDB_Exception* CDB_DeadlockEx::Clone() const
{
    return new CDB_DeadlockEx(m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_TimeoutEx::
//

CDB_Exception* CDB_TimeoutEx::Clone() const
{
    return new CDB_TimeoutEx(m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_ClientEx::
//

CDB_Exception* CDB_ClientEx::Clone() const
{
    return new CDB_ClientEx(m_Severity, m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_MultiEx::
//

CDB_MultiEx::CDB_MultiEx(const string& originated_from, unsigned int capacity)
    : CDB_Exception(eMulti, eDB_Unknown, 0, originated_from, 0)
{
    m_Bag = new CDB_MultiExStorage(capacity);
}

CDB_MultiEx::CDB_MultiEx(const CDB_MultiEx& mex) :
    CDB_Exception(eMulti, eDB_Unknown, 0, mex.m_OriginatedFrom, mex.m_Message)
{
    m_Bag = mex.m_Bag;
    m_Bag->AddRef();
}

CDB_MultiEx::~CDB_MultiEx()
{
    m_Bag->DelRef();
}

CDB_MultiEx::CDB_MultiExStorage::CDB_MultiExStorage(unsigned int capacity)
{
    m_NofRooms = capacity;
    m_NofExs   = 0;
    m_RefCnt   = 0;
    m_Ex       = new CDB_Exception*[m_NofRooms];
}

bool CDB_MultiEx::CDB_MultiExStorage::Push(const CDB_Exception* ex)
{
    if (ex->Type() == eMulti) { // this is a multi exception
        CDB_MultiEx& mex =
            const_cast<CDB_MultiEx&> (dynamic_cast<const CDB_MultiEx&> (*ex));

        for (CDB_Exception* pex = mex.Pop();  pex;  pex = mex.Pop()) {
            if (m_NofExs < m_NofRooms) {
                m_Ex[m_NofExs] = pex;
            }
            else {
                delete pex;
            }
            ++m_NofExs;
        }
    }
    else { // this is an ordinary exception
        if (m_NofExs < m_NofRooms) {
            m_Ex[m_NofExs] = ex->Clone();
        }
        ++m_NofExs;
    }

    return (m_NofExs <= m_NofRooms);
}

CDB_Exception* CDB_MultiEx::CDB_MultiExStorage::Pop(void)
{
    if (m_NofExs == 0)
        return 0;

    if (m_NofExs > m_NofRooms) {
        CDB_ClientEx* pex = new CDB_ClientEx
            (eDB_Warning, m_NofExs,
             "CDB_MultiEx::CDB_MultiExStorage",
             "Too many exceptions. The error code shows the count.");
        m_NofExs = m_NofRooms;
        return pex;
    }

    return m_Ex[--m_NofExs];
}

CDB_MultiEx::CDB_MultiExStorage::~CDB_MultiExStorage()
{
    if(m_NofExs > m_NofRooms) {
        m_NofExs = m_NofRooms;
    }

    while(m_NofExs != 0) {
        delete m_Ex[--m_NofExs];
    }

    delete [] m_Ex;
}



/////////////////////////////////////////////////////////////////////////////
//  CDBUserHandler::
//

CDBUserHandler::~CDBUserHandler()
{
    return;
}



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/09/24 19:18:38  vakatov
 * Fixed a couple of typos
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

