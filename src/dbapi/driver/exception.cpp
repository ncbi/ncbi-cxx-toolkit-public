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
* Author:  Vladimir Soussov, Denis Vakatov
*   
* File Description:  Exceptions
*
*
*/

#include <dbapi/driver/exception.hpp>
#include <iostream>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDB_Exception::
//

CDB_Exception::CDB_Exception(EType type, EDB_Severity severity, int err_code,
                             const string& originated_from, const string& msg)
{
    m_Type           = type;
    m_Severity       = severity;
    m_ErrCode        = err_code;
    m_OriginatedFrom = originated_from;
    m_Message        = msg;
}


CDB_Exception::~CDB_Exception() throw()
{
    return;
}


const char* CDB_Exception::what() const throw()
{
    return Message().c_str();
}


CDB_Exception* CDB_Exception::Clone() const
{
    return new CDB_Exception
        (m_Type, m_Severity, m_ErrCode, m_OriginatedFrom, m_Message);
}


const char* CDB_Exception::SeverityString(EDB_Severity sev)
{
    switch ( sev ) {
    case eDB_Info:     return "Info";
    case eDB_Warning:  return "Warning";
    case eDB_Error:    return "Error";
    case eDB_Fatal:    return "Fatal Error";
    default:           return "Unknown severity";
    }
 
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

CDB_RPCEx::CDB_RPCEx(EDB_Severity severity, int err_code,
                     const string& originated_from, const string& msg,
                     const string& proc_name, int proc_line)
    : CDB_Exception(eRPC, severity, err_code, originated_from, msg)
{
    static const string s_UnknownProcName = "Unknown";
    m_ProcName = proc_name.empty() ? s_UnknownProcName : proc_name;
    m_ProcLine = proc_line;
}

CDB_RPCEx::~CDB_RPCEx() throw()
{
    return;
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

CDB_SQLEx::CDB_SQLEx(EDB_Severity severity, int err_code,
                     const string& originated_from, const string& msg,
                     const string& sql_state, int batch_line)
    : CDB_Exception(eSQL, severity, err_code, originated_from, msg)
{
    static const string s_UnknownSqlState = "Unknown";
    m_SqlState  = sql_state.empty() ? s_UnknownSqlState : sql_state;
    m_BatchLine = batch_line;
}

CDB_SQLEx::~CDB_SQLEx() throw()
{
    return;
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

CDB_MultiEx::~CDB_MultiEx() throw()
{
    m_Bag->DelRef();
}

CDB_MultiEx::CDB_MultiExStorage::CDB_MultiExStorage(unsigned int capacity)
{
    m_NofRooms = capacity;
    m_NofExs   = 0;
    m_RefCnt   = 0;
    m_Ex       = new CDB_Exception* [m_NofRooms];
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
//  CDB_UserHandler_Default::  wrapper for the actual current handler
//
// NOTE: It is actually a singleton, but as it is a local and very simple class,
// so there is no singleton-specific sanity checks here...
//

class CDB_UserHandler_Default : public CDB_UserHandler
{
public:
    CDB_UserHandler* Set(CDB_UserHandler* h);

    virtual bool HandleIt(CDB_Exception* ex);
    virtual ~CDB_UserHandler_Default();

private:
    CDB_UserHandler* m_Handler; 
};


CDB_UserHandler* CDB_UserHandler_Default::Set(CDB_UserHandler* h)
{
    if (h == this) {
        throw runtime_error("CDB_UserHandler_Default::Reset() -- attempt "
                            "to set handle wrapper as a handler");
    }

    CDB_UserHandler* prev_h = m_Handler;
    m_Handler = h;
    return prev_h;
}


CDB_UserHandler_Default::~CDB_UserHandler_Default()
{
    delete m_Handler;
}


bool CDB_UserHandler_Default::HandleIt(CDB_Exception* ex)
{
    return m_Handler ? m_Handler->HandleIt(ex) : true;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler::
//

CDB_UserHandler::~CDB_UserHandler()
{
    return;
}


//
// ATTENTION:  Should you change the following static methods, please make sure
//             to rebuild all DLLs which have this code statically linked in!
//

static CDB_UserHandler_Default s_CDB_DefUserHandler;  // (singleton)


CDB_UserHandler& CDB_UserHandler::GetDefault()
{
    static bool s_CDB_DefUserHandler_IsSet = false;

    if ( !s_CDB_DefUserHandler_IsSet ) {
        s_CDB_DefUserHandler_IsSet = true;
        delete s_CDB_DefUserHandler.Set(new CDB_UserHandler_Stream(&cerr));
    }

    return s_CDB_DefUserHandler;
}


CDB_UserHandler* CDB_UserHandler::SetDefault(CDB_UserHandler* h)
{
    return s_CDB_DefUserHandler.Set(h);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Stream::
//


CDB_UserHandler_Stream::CDB_UserHandler_Stream(ostream*      os,
                                               const string& prefix,
                                               bool          own_os)
    : m_Output(os),
      m_Prefix(prefix),
      m_OwnOutput(own_os)
{
    return;
}


CDB_UserHandler_Stream::~CDB_UserHandler_Stream()
{
    if ( m_OwnOutput )
        delete m_Output;
}


bool CDB_UserHandler_Stream::HandleIt(CDB_Exception* ex)
{
    if ( !ex ) {
        return false;
    }

    *m_Output << m_Prefix << ' ';

    *m_Output << ex->SeverityString(ex->Severity()) << ' ';

    switch ( ex->Type() ) {
    case CDB_Exception::eDS : {
        const CDB_DSEx& e = dynamic_cast<const CDB_DSEx&> (*ex);
        *m_Output << "Message (Err.code:" << e.ErrCode() << ") Data source: '"
             << e.OriginatedFrom() << "'\n<<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eRPC : {
        const CDB_RPCEx& e = dynamic_cast<const CDB_RPCEx&> (*ex);
        *m_Output << "Message (Err code:" << e.ErrCode()
             << ") SQL/Open server: '" << e.OriginatedFrom()
             << "' Procedure: '" << e.ProcName()
             << "' Line: " << e.ProcLine() << endl
             << "<<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eSQL : {
        const CDB_SQLEx& e = dynamic_cast<const CDB_SQLEx&> (*ex);
        *m_Output << "Message (Err.code=" << e.ErrCode()
             << ") SQL server: '" << e.OriginatedFrom()
             << "' SQLstate '" << e.SqlState()
             << "' Line: " << e.BatchLine() << endl
             << " <<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eDeadlock : {
        const CDB_DeadlockEx& e = dynamic_cast<const CDB_DeadlockEx&> (*ex);
        *m_Output << "Message: SQL server: " << e.OriginatedFrom()
             << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eTimeout : {
        const CDB_TimeoutEx& e = dynamic_cast<const CDB_TimeoutEx&> (*ex);
        *m_Output << "Message: SQL server: " << e.OriginatedFrom()
             << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eClient : {
        const CDB_ClientEx& e = dynamic_cast<const CDB_ClientEx&> (*ex);
        *m_Output << "Message: Err code=" << e.ErrCode()
             <<" Source: " << e.OriginatedFrom() << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eMulti : {
        CDB_MultiEx& e = dynamic_cast<CDB_MultiEx&> (*ex);
        while ( HandleIt(e.Pop()) ) {
            continue;
        }
        break;
    }
    default: {
        *m_Output << "Message: Error code=" << ex->ErrCode()
             << " <" << ex->what() << ">";
    }
    }

    *m_Output << endl;
    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2001/10/04 20:26:45  vakatov
 * Added missing virtual destructors to CDB_RPCEx and CDB_SQLEx
 *
 * Revision 1.5  2001/10/01 20:09:29  vakatov
 * Introduced a generic default user error handler and the means to
 * alternate it. Added an auxiliary error handler class
 * "CDB_UserHandler_Stream".
 * Moved "{Push/Pop}{Cntx/Conn}MsgHandler()" to the generic code
 * (in I_DriverContext).
 *
 * Revision 1.4  2001/09/28 16:37:34  vakatov
 * Added missing "throw()" to exception classes derived from "std::exception"
 *
 * Revision 1.3  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
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
