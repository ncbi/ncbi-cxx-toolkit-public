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
 * Author:  Denis Vakatov, Vladimir Soussov
 *   
 * File Description:  Exceptions
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>


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
    if ( m_What.empty() ) {
        try {
            x_ComposeWhat();
        } catch (exception& ex) {
            static char s_StartOfWhat[] =
                "CDB_Exception::what(): x_ComposeWhat() threw an exception: ";
            static char s_What[sizeof(s_StartOfWhat) + 128];
            strcpy(s_What, s_StartOfWhat);
            const char* ex_what = ex.what() ? ex.what() : "NULL";
            size_t len = strlen(s_StartOfWhat);
            strncpy(s_What + len, ex_what,
                    sizeof(s_What) - strlen(s_StartOfWhat) - 1);
            s_What[sizeof(s_What) - 1] = '\0';
            return s_What;
        }
    }
    return m_What.c_str();
}


const char* CDB_Exception::SeverityString(EDB_Severity sev)
{
    switch ( sev ) {
    case eDB_Info:     return "Info";
    case eDB_Warning:  return "Warning";
    case eDB_Error:    return "Error";
    case eDB_Fatal:    return "Fatal";
    default:           return "Unclear";
    }
}


void CDB_Exception::x_ComposeWhat(void) const
{
    x_StartOfWhat(&m_What);
    x_EndOfWhat(&m_What);
}


string& CDB_Exception::x_StartOfWhat(string* str) const
{
    *str += "[";
    *str += SeverityString();
    *str += " #";
    *str += NStr::IntToString(ErrCode());
    *str += ", ";
    *str += TypeString();
    *str += " in ";
    *str += OriginatedFrom();
    *str += "]  ";
    return *str;
}


string& CDB_Exception::x_EndOfWhat(string* str) const
{
    *str += "<<<";
    *str += Message();
    *str += ">>>";
    return *str;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_DSEx::
//

const char* CDB_DSEx::TypeString() const
{
    return "Data_Source";
}


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


const char* CDB_RPCEx::TypeString() const
{
    return "Remote_Procedure_Call";
}


CDB_Exception* CDB_RPCEx::Clone() const
{
    return new CDB_RPCEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message,
         m_ProcName, m_ProcLine);
}


void CDB_RPCEx::x_ComposeWhat(void) const
{
    x_StartOfWhat(&m_What);
    m_What += "Procedure '";
    m_What += ProcName();
    m_What += "', Line ";
    m_What += NStr::IntToString(ProcLine());
    x_EndOfWhat(&m_What);
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


const char* CDB_SQLEx::TypeString() const
{
    return "SQL";
}


CDB_Exception* CDB_SQLEx::Clone() const
{
    return new CDB_SQLEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message,
         m_SqlState, m_BatchLine);
}


void CDB_SQLEx::x_ComposeWhat(void) const
{
    x_StartOfWhat(&m_What);
    m_What += "Procedure '";
    m_What += SqlState();
    m_What += "', Line ";
    m_What += NStr::IntToString(BatchLine());
    x_EndOfWhat(&m_What);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_DeadlockEx::
//

const char* CDB_DeadlockEx::TypeString() const
{
    return "Deadlock";
}


CDB_Exception* CDB_DeadlockEx::Clone() const
{
    return new CDB_DeadlockEx
        (m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_TimeoutEx::
//

const char* CDB_TimeoutEx::TypeString() const
{
    return "Timeout";
}


CDB_Exception* CDB_TimeoutEx::Clone() const
{
    return new CDB_TimeoutEx
        (m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_ClientEx::
//

const char* CDB_ClientEx::TypeString() const
{
    return "Client";
}


CDB_Exception* CDB_ClientEx::Clone() const
{
    return
        new CDB_ClientEx
        (m_Severity, m_ErrCode, m_OriginatedFrom, m_Message);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_MultiEx::
//

CDB_MultiEx::CDB_MultiEx(const string& originated_from, unsigned int capacity)
    : CDB_Exception(eMulti, eDB_Unknown, 0, originated_from, kEmptyStr)
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


const char* CDB_MultiEx::TypeString() const
{
    return "MultiException";
}


CDB_Exception* CDB_MultiEx::Clone() const
{
    return new CDB_MultiEx(*this);
}


string CDB_MultiEx::WhatThis(void) const
{
    string str;
    str += "---  [Multi-Exception in ";
    str += OriginatedFrom();
    str += "]   Contains a backtrace of ";
    str += NStr::UIntToString( NofExceptions() );
    str += " exceptions  ---";
    return str;
}


void CDB_MultiEx::x_ComposeWhat(void) const
{
    m_What  = WhatThis();
    m_What += Endl();
    m_What += m_Bag->What();
    m_What += Endl();
    m_What += "---  [Multi-Exception]  End of backtrace  ---";
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_MultiEx::CDB_MultiExStorage::
//



CDB_MultiEx::CDB_MultiExStorage::CDB_MultiExStorage(unsigned int capacity)
{
    m_NofRooms = capacity;
    m_NofExs   = 0;
    m_RefCnt   = 0;
    m_Ex       = new CDB_Exception* [m_NofRooms];
}


string CDB_MultiEx::CDB_MultiExStorage::What()
{
    string str;

    if (m_NofExs == 0)
        return str;

    if (m_NofExs > m_NofRooms) {
        str += " *** Too many exceptions -- the last ";
        str += NStr::UIntToString(m_NofExs - m_NofRooms);
        str += " exceptions are not shown ***";
    }

    
    for (size_t i = 0;  i < min(m_NofExs, m_NofRooms);  i++) {
        str += Endl();
        str += "  ";
        str += m_Ex[i]->what();
    }

    return str;
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
    if (m_NofExs > m_NofRooms) {
        m_NofExs = m_NofRooms;
    }

    while (m_NofExs != 0) {
        delete m_Ex[--m_NofExs];
    }

    delete [] m_Ex;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Wrapper::  wrapper for the actual current handler
//
//    NOTE:  it is a singleton
//


class CDB_UserHandler_Wrapper : public CDB_UserHandler
{
public:
    CDB_UserHandler* Set(CDB_UserHandler* h);

    virtual bool HandleIt(CDB_Exception* ex);
    virtual ~CDB_UserHandler_Wrapper();

private:
    CDB_UserHandler* m_Handler; 
};


static CDB_UserHandler_Wrapper s_CDB_DefUserHandler;  // (singleton)
static bool                    s_CDB_DefUserHandler_IsSet = false;


CDB_UserHandler* CDB_UserHandler_Wrapper::Set(CDB_UserHandler* h)
{
    if (h == this) {
        throw runtime_error("CDB_UserHandler_Wrapper::Reset() -- attempt "
                            "to set handle wrapper as a handler");
    }

    CDB_UserHandler* prev_h = m_Handler;
    m_Handler = h;
    return prev_h;
}


CDB_UserHandler_Wrapper::~CDB_UserHandler_Wrapper()
{
    delete m_Handler;
    m_Handler = 0;
    s_CDB_DefUserHandler_IsSet = false;
}


bool CDB_UserHandler_Wrapper::HandleIt(CDB_Exception* ex)
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

CDB_UserHandler& CDB_UserHandler::GetDefault(void)
{
    if ( !s_CDB_DefUserHandler_IsSet ) {
        s_CDB_DefUserHandler_IsSet = true;
        s_CDB_DefUserHandler.Set(new CDB_UserHandler_Default);
    }

    return s_CDB_DefUserHandler;
}


CDB_UserHandler* CDB_UserHandler::SetDefault(CDB_UserHandler* h)
{
    s_CDB_DefUserHandler_IsSet = true;
    return s_CDB_DefUserHandler.Set(h);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Diag::
//


CDB_UserHandler_Diag::CDB_UserHandler_Diag(const string& prefix)
    : m_Prefix(prefix)
{
    return;
}


CDB_UserHandler_Diag::~CDB_UserHandler_Diag()
{
    m_Prefix.erase();
}


bool CDB_UserHandler_Diag::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return true;
        
    if ( m_Prefix.empty() ) {
        LOG_POST( ex->what() );
    } else {
        LOG_POST(m_Prefix << ' ' << ex->what());
    } 

    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Stream::
//


CDB_UserHandler_Stream::CDB_UserHandler_Stream(ostream*      os,
                                               const string& prefix,
                                               bool          own_os)
    : m_Output(os ? os : &cerr),
      m_Prefix(prefix),
      m_OwnOutput(own_os)
{
    if (m_OwnOutput  &&  (m_Output == &cerr  ||  m_Output == &cout)) {
        m_OwnOutput = false;
    }
}


CDB_UserHandler_Stream::~CDB_UserHandler_Stream()
{
    if ( m_OwnOutput ) {
        delete m_Output;
        m_OwnOutput = false;
        m_Output = 0;
    }
    m_Prefix.erase();
}


bool CDB_UserHandler_Stream::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return true;

    if ( !m_Output )
        return false;

    if ( !m_Prefix.empty() ) {
        *m_Output << m_Prefix << ' ';
    }
    *m_Output << ex->what();
    *m_Output << endl;
    return m_Output->good();
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.11  2003/11/04 22:22:01  vakatov
 * Factorize the code (especially the reporting one) through better inheritance.
 * +CDB_Exception::TypeString()
 * CDB_MultiEx to become more CDB_Exception-like.
 * Improve the user-defined handlers' API, add CDB_UserHandler_Diag.
 *
 * Revision 1.8  2002/09/04 21:46:12  vakatov
 * Added missing 'const' to CDB_Exception::SeverityString()
 *
 * Revision 1.7  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
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
