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
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDB_Exception::
//

EDB_Severity
CDB_Exception::Severity(void) const
{
    EDB_Severity result = eDB_Unknown;

    switch ( GetSeverity() ) {
    case eDiag_Info:
        result = eDB_Info;
        break;
    case eDiag_Warning:
        result = eDB_Warning;
        break;
    case eDiag_Error:
        result = eDB_Error;
        break;
    case eDiag_Critical:
        result = eDB_Fatal;
        break;
    case eDiag_Fatal:
        result = eDB_Fatal;
        break;
    case eDiag_Trace:
        result = eDB_Fatal;
        break;
    }

    return result;
}

const char*
CDB_Exception::SeverityString(void) const
{
    return CNcbiDiag::SeverityName( GetSeverity() );
}

const char* CDB_Exception::SeverityString(EDB_Severity sev)
{
    EDiagSev dsev = eDiag_Info;

    switch ( sev ) {
    case eDB_Info:
        dsev = eDiag_Info;
        break;
    case eDB_Warning:
        dsev = eDiag_Warning;
        break;
    case eDB_Error:
        dsev = eDiag_Error;
        break;
    case eDB_Fatal:
        dsev = eDiag_Fatal;
        break;
    case eDB_Unknown:
        dsev = eDiag_Info;
        break;
    }
    return CNcbiDiag::SeverityName( dsev );
}


void
CDB_Exception::ReportExtra(ostream& out) const
{
    x_StartOfWhat( out );
    x_EndOfWhat( out );
}

void
CDB_Exception::x_StartOfWhat(ostream& out) const
{
    out << "[";
    out << SeverityString();
    out << " #";
    out << NStr::IntToString( GetDBErrCode() );
    out << ", ";
    out << GetType();
    out << "]";
}


void
CDB_Exception::x_EndOfWhat(ostream& out) const
{
//     out << "<<<";
//     out << GetMsg();
//     out << ">>>";
}

void
CDB_Exception::x_Assign(const CException& src)
{
    const CDB_Exception& other = dynamic_cast<const CDB_Exception&>(src);

    CException::x_Assign(src);

    m_DBErrCode = other.m_DBErrCode;
    m_ServerName = other.m_ServerName;
    m_UserName = other.m_UserName;
    m_SybaseSeverity = other.m_SybaseSeverity;
}


const char*
CDB_Exception::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eDS:       return "eDS";
    case eRPC:      return "eRPC";
    case eSQL:      return "eSQL";
    case eDeadlock: return "eDeadlock";
    case eTimeout:  return "eTimeout";
    case eClient:   return "eClient";
    case eMulti:    return "eMulti";
    default:        return CException::GetErrCodeString();
    }
}

CDB_Exception*
CDB_Exception::Clone(void) const
{
    return new CDB_Exception(*this);
}

CDB_Exception::EType
CDB_Exception::Type(void) const
{
    int err_code = x_GetErrCode();

    if (err_code > eMulti) {
        err_code = eInvalid;
    }

    return static_cast<EType>(err_code);
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_RPCEx::
//

void
CDB_RPCEx::ReportExtra(ostream& out) const
{
    string extra_value;

    x_StartOfWhat( out );

    out << " Procedure '";
    out << ProcName();
    out << "', Line ";
    out << NStr::IntToString( ProcLine() );

    x_EndOfWhat( out );
}


void
CDB_RPCEx::x_Assign(const CException& src)
{
    const CDB_RPCEx& other = dynamic_cast<const CDB_RPCEx&>(src);

    CDB_Exception::x_Assign(src);
    m_ProcName = other.m_ProcName;
    m_ProcLine = other.m_ProcLine;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_SQLEx::
//

void
CDB_SQLEx::ReportExtra(ostream& out) const
{
    x_StartOfWhat( out );

    out << " Procedure '";
    out << SqlState();
    out << "', Line ";
    out << NStr::IntToString(BatchLine());

    x_EndOfWhat( out );
}

void
CDB_SQLEx::x_Assign(const CException& src)
{
    const CDB_SQLEx& other = dynamic_cast<const CDB_SQLEx&>(src);

    CDB_Exception::x_Assign(src);
    m_SqlState = other.m_SqlState;
    m_BatchLine = other.m_BatchLine;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_MultiEx::
//

void
CDB_MultiEx::x_Assign(const CException& src)
{
    const CDB_MultiEx& other = dynamic_cast<const CDB_MultiEx&>(src);

    CDB_Exception::x_Assign(src);
    m_Bag = other.m_Bag;
    m_NofRooms = other.m_NofRooms;
}

bool
CDB_MultiEx::Push(const CDB_Exception& ex)
{
    if ( ex.GetErrCode() == eMulti ) {
        CDB_MultiEx& mex =
            const_cast<CDB_MultiEx&> ( dynamic_cast<const CDB_MultiEx&> (ex) );

        CDB_Exception* pex = NULL;
        while ( pex = mex.Pop() ) {
            m_Bag->GetData().push_back( pex );
        }
    } else {
        // this is an ordinary exception
        // Clone it ...
        const CException* tmp_ex = ex.x_Clone();
        const CDB_Exception* except_copy = dynamic_cast<const CDB_Exception*>(tmp_ex);

        // Store it ...
        if ( except_copy ) {
            m_Bag->GetData().push_back( except_copy );
        } else {
            delete tmp_ex;
            return false;
        }
    }
    return true;
}

CDB_Exception*
CDB_MultiEx::Pop(void)
{
    if ( m_Bag->GetData().empty() ) {
        return NULL;
    }

    const CDB_Exception* result = m_Bag->GetData().back().release();
    m_Bag->GetData().pop_back();

    // Remove "const" from the object because we do not own it any more ...
    return const_cast<CDB_Exception*>(result);
}

string CDB_MultiEx::WhatThis(void) const
{
    string str;
    str += "---  [Multi-Exception in ";
    str += GetModule();
    str += "]   Contains a backtrace of ";
    str += NStr::UIntToString( NofExceptions() );
    str += " exception";
    str += NofExceptions() == 1 ? "" : "s";
    str += "  ---";
    return str;
}


void
CDB_MultiEx::ReportExtra(ostream& out) const
{
    out
        << WhatThis()
        << Endl();

    ReportErrorStack(out);

    out
        << Endl()
        << "---  [Multi-Exception]  End of backtrace  ---";
}

void
CDB_MultiEx::ReportErrorStack(ostream& out) const
{
    size_t record_num = m_Bag->GetData().size();

    if ( record_num == 0 ) {
        return;
    }
    if ( record_num > m_NofRooms ) {
        out << " *** Too many exceptions -- the last ";
        out << NStr::UInt8ToString( record_num - m_NofRooms );
        out << " exceptions are not shown ***";
    }

    TExceptionStack::const_reverse_iterator criter = m_Bag->GetData().rbegin();
    TExceptionStack::const_reverse_iterator eriter = m_Bag->GetData().rend();
    for ( unsigned int i = 0; criter != eriter && i < m_NofRooms; ++criter, ++i ) {
        out << Endl();
        out << "  ";
        out << (*criter)->what();
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Wrapper::  wrapper for the actual current handler
//
//    NOTE:  it is a singleton
//


class CDB_UserHandler_Wrapper : public CDB_UserHandler
{
public:
    CDB_UserHandler_Wrapper(void);

    CDB_UserHandler* Set(CDB_UserHandler* h);

    virtual bool HandleIt(CDB_Exception* ex);
    virtual ~CDB_UserHandler_Wrapper();

private:
    CDB_UserHandler* m_Handler;
};


CDB_UserHandler_Wrapper::CDB_UserHandler_Wrapper(void) :
m_Handler(new CDB_UserHandler_Default)
{
}

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
    try {
        delete m_Handler;
        m_Handler = 0;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CDB_UserHandler_Wrapper::HandleIt(CDB_Exception* ex)
{
    return m_Handler ? m_Handler->HandleIt(ex) : true;
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler::
//

CDB_UserHandler::~CDB_UserHandler(void)
{
    return;
}


//
// ATTENTION:  Should you change the following static methods, please make sure
//             to rebuild all DLLs which have this code statically linked in!
//

static CDB_UserHandler_Wrapper& GetDefaultCDBErrorHandler(void)
{
    static CSafeStaticRef<CDB_UserHandler_Wrapper> s_CDB_DefUserHandler;

    return s_CDB_DefUserHandler.Get();
}

CDB_UserHandler& CDB_UserHandler::GetDefault(void)
{
    return GetDefaultCDBErrorHandler();
}


CDB_UserHandler* CDB_UserHandler::SetDefault(CDB_UserHandler* h)
{
    return GetDefaultCDBErrorHandler().Set(h);
}


bool CDB_UserHandler::HandleAll(const TExceptions& exceptions)
{
    return false;
}


string CDB_UserHandler::GetExtraMsg(void) const
{
    CFastMutexGuard mg(m_Mtx);

    return m_ExtraMsg;
}


void CDB_UserHandler::SetExtraMsg(const string& msg)
{
    CFastMutexGuard mg(m_Mtx);

    m_ExtraMsg = msg;
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
    try {
        m_Prefix.erase();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


bool CDB_UserHandler_Diag::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return true;

    if (ex->GetSeverity() == eDiag_Info) {
        if ( m_Prefix.empty() ) {
            LOG_POST(Severity(ex->GetSeverity()) << ex->GetMsg());
        } else {
            LOG_POST(Severity(ex->GetSeverity()) << m_Prefix << ' ' <<
                     ex->GetMsg());
        }
    } else {
        if ( m_Prefix.empty() ) {
            LOG_POST(Severity(ex->GetSeverity()) << ex->what() <<
                     " SERVER: '" << ex->GetServerName() <<
                     "' USER: '" << ex->GetUserName() << "'" <<
                     (GetExtraMsg().empty() ? "" : " CONTEXT: '" +
                      GetExtraMsg()) << "'"
                     );
        } else {
            LOG_POST(Severity(ex->GetSeverity()) << m_Prefix << ' ' <<
                     ex->what() <<
                     " SERVER: '" << ex->GetServerName() <<
                     "' USER: '" << ex->GetUserName() << "'" <<
                     (GetExtraMsg().empty() ? "" : " CONTEXT: '" +
                      GetExtraMsg()) << "'"
                     );
        }
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
    try {
        if ( m_OwnOutput ) {
            delete m_Output;
            m_OwnOutput = false;
            m_Output = 0;
        }
        m_Prefix.erase();
    }
    NCBI_CATCH_ALL( kEmptyStr )
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
 * Revision 1.34  2006/12/29 20:42:04  ssikorsk
 * Temporary mt fix for memory corruption.
 *
 * Revision 1.33  2006/12/27 21:16:19  ssikorsk
 * Report context info with CDB_UserHandler_Diag::HandleIt.
 *
 * Revision 1.32  2006/09/14 18:52:26  ssikorsk
 * CDB_Exception: fixed methods Type and Clone.
 *
 * Revision 1.31  2006/07/28 14:58:40  ssikorsk
 * Added GetSybaseSeverity/SetSybaseSeverity to CDB_Exception.
 *
 * Revision 1.30  2006/06/19 19:07:05  ssikorsk
 * Revamp CDB_UserHandler::~CDB_UserHandler to work with new CHandlerStack.
 *
 * Revision 1.29  2006/06/06 20:37:14  ssikorsk
 * Use "Severity" manipulator to put a severity of an exception into log.
 *
 * Revision 1.28  2006/06/06 19:55:46  ssikorsk
 * Put severity of an exception into log within CDB_UserHandler_Diag::HandleIt.
 *
 * Revision 1.27  2006/06/02 19:26:38  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.26  2006/05/30 18:46:07  ssikorsk
 * Added server and user names to the CDB_Exception class;
 * Revamp CDB_UserHandler_Diag to report server and user names;
 *
 * Revision 1.25  2006/05/16 21:18:38  ssikorsk
 * Improved methods CDB_UserHandler::GetDefault and CDB_UserHandler::SetDefault.
 *
 * Revision 1.24  2006/05/15 19:26:30  ssikorsk
 * Fixed CDB_UserHandler::~CDB_UserHandler to work correctly with CRef in case
 * of manual adjusting of reference number.
 *
 * Revision 1.23  2006/05/10 14:41:54  ssikorsk
 * Added default implementation of CDB_UserHandler::HandleAll.
 *
 * Revision 1.22  2006/04/20 22:16:31  ssikorsk
 * Replaced NStr::UIntToString with NStr::UInt8ToString for x64 sake.
 *
 * Revision 1.21  2006/03/01 19:50:13  ssikorsk
 * Replaced static variable with CSafeStaticPtr.
 *
 * Revision 1.20  2006/02/08 17:23:01  ssikorsk
 * For exceptions with informational severity print only a message.
 *
 * Revision 1.19  2005/11/02 14:32:39  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.18  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.17  2005/05/16 10:58:22  ssikorsk
 * Using severity level from the CException class
 *
 * Revision 1.16  2005/04/11 19:23:59  ssikorsk
 * Added method Clone to the CDB_Exception class
 *
 * Revision 1.15  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.14  2004/10/22 20:19:04  lavr
 * Do not print "1 exceptions" -- reworked
 *
 * Revision 1.13  2004/10/22 20:16:42  lavr
 * Do not print "1 exceptions"
 *
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
