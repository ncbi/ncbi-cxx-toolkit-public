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
#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/error_codes.hpp>
#include <corelib/ncbi_safe_static.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrExcepts


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDB_Exception::
//

CDB_Exception::SParams::~SParams(void)
{
    ITERATE (TParams, it, params) {
        if (it->value != NULL) {
            delete it->value;
        }
    }
}

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


void CDB_Exception::SetFromConnection(const impl::CConnection& connection)
{
    if (GetServerName().empty()  &&  !connection.ServerName().empty()) {
        SetServerName(connection.ServerName());
        AddToMessage(" SERVER: '" + connection.ServerName() + '\'');
    }
    if (GetUserName().empty()  &&  !connection.UserName().empty()) {
        SetUserName(connection.UserName());
        AddToMessage(" USER: '" + connection.UserName() + '\'');
    }
    if ( !connection.GetDatabaseName().empty()
        &&  GetMsg().find(" DATABASE: ") == NPOS) {
        AddToMessage(" DATABASE: '" + connection.GetDatabaseName() + '\'');
    }
}


void
CDB_Exception::SetParams(const CDBParams* params)
{
    unsigned int n = params ? params->GetNum() : 0;
    if (n == 0) {
        return;
    }
    if (m_Params.Empty()) {
        m_Params.Reset(new SParams);
    }
    
    SParams::TParams& my_params = m_Params->params;
    my_params.resize(n);
    for (unsigned int i = 0;  i < n;  ++i) {
        my_params[i].value = NULL;
    }
    for (unsigned int i = 0;  i < n;  ++i) {
        SParam& p = my_params[i];
        p.name = params->GetName(i);
        try {
            const CDB_Object* v = params->GetValue(i);
            if (v != NULL) {
                p.value = v->Clone();
            }
        } catch (exception&) {
        }
    }
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
    if ( !GetExtraMsg().empty()  &&  GetMsg().find(GetExtraMsg()) == NPOS) {
        out << GetExtraMsg() << ' ';
    }
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
    if ( !m_Params.Empty()  &&  !m_Params->params.empty() ) {
        out << " [Parameters: ";
        const char* delim = kEmptyCStr;
        ITERATE (SParams::TParams, it, m_Params->params) {
            out << delim;
            if ( !it->name.empty() ) {
                out << it->name << " = ";
            }
            if (it->value) {
                out << it->value->GetLogString();
            } else {
                out << "(null)";
            }
            delim = ", ";
        }
        out << ']';
    }
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
    m_ExtraMsg = other.m_ExtraMsg;
    m_Params = other.m_Params;
}


const char*
CDB_Exception::GetErrCodeString(void) const
{
    switch ( x_GetErrCode() ) {
        case eDS:       return "eDS";
        case eRPC:      return "eRPC";
        case eSQL:      return "eSQL";
        case eDeadlock: return "eDeadlock";
        case eTimeout:  return "eTimeout";
        case eClient:   return "eClient";
        case eMulti:    return "eMulti";
        case eTruncate: return "eTruncate";
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
        while ( (pex = mex.Pop()) != NULL ) {
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

    str += "---  [Multi-Exception";
    if (!GetModule().empty()) {
        str += " in ";
        str += GetModule();
    }
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

    virtual bool HandleAll(const TExceptions& exceptions);
    virtual bool HandleIt(CDB_Exception* ex);
    virtual bool HandleMessage(int severity, int msgnum,
                               const string& message);

    virtual ~CDB_UserHandler_Wrapper();

private:
    CRef<CDB_UserHandler> m_Handler;
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

    if (h == m_Handler) {
        return NULL;
    }

    CDB_UserHandler* prev_h = m_Handler.Release();
    m_Handler = h;
    return prev_h;
}


CDB_UserHandler_Wrapper::~CDB_UserHandler_Wrapper()
{
}


bool CDB_UserHandler_Wrapper::HandleIt(CDB_Exception* ex)
{
    return m_Handler ? m_Handler->HandleIt(ex) : false;
}


bool CDB_UserHandler_Wrapper::HandleAll(const TExceptions& exceptions)
{
    return m_Handler ? m_Handler->HandleAll(exceptions) : false;
}


bool CDB_UserHandler_Wrapper::HandleMessage(int severity, int msgnum,
                                            const string& message)
{
    return m_Handler ? m_Handler->HandleMessage(severity, msgnum, message)
        : false;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler::
//

CDB_UserHandler::CDB_UserHandler(void)
{
}

CDB_UserHandler::~CDB_UserHandler(void)
{
}

void
CDB_UserHandler::ClearExceptions(TExceptions& expts)
{
    NON_CONST_ITERATE(CDB_UserHandler::TExceptions, it, expts) {
        CDB_Exception* ex = *it; // for debugging ...
        delete ex;
    }
    expts.clear();
}


//
// ATTENTION:  Should you change the following static methods, please make sure
//             to rebuild all DLLs which have this code statically linked in!
//

static CDB_UserHandler_Wrapper& GetDefaultCDBErrorHandler(void)
{
    static CSafeStatic<CDB_UserHandler_Wrapper> s_CDB_DefUserHandler;

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


bool CDB_UserHandler::HandleAll(const TExceptions& /* exceptions */)
{
    // return x_HandleAll(exceptions);
    return false;
}

bool CDB_UserHandler::x_HandleAll(const TExceptions& exceptions)
{
    bool handled_any = false;
    ITERATE (TExceptions, it, exceptions) {
        handled_any |= HandleIt(*it);
    }
    return handled_any;
}

bool CDB_UserHandler::HandleMessage(int /* severity */,
                                    int /* msgnum */,
                                    const string& /* message */)
{
    return false;
}


string CDB_UserHandler::GetExtraMsg(void) const
{
    return "Method CDB_UserHandler::GetExtraMsg is deprecated. "
           "Use CDB_Exception::GetExtraMsg instead.";
}

void CDB_UserHandler::SetExtraMsg(const string&) const
{
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
    NCBI_CATCH_ALL_X( 5, NCBI_CURRENT_FUNCTION )
}


bool CDB_UserHandler_Diag::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return true;

    if (ex->GetSeverity() == eDiag_Info) {
        if ( m_Prefix.empty() ) {
            ERR_POST_X(1, Info << ex->GetMsg());
        } else {
            ERR_POST_X(2, Info << m_Prefix << " " << ex->GetMsg());
        }
    }
    else {
        if ( m_Prefix.empty() ) {
            ERR_POST_X(3, *ex);
        } else {
            ERR_POST_X(4, Severity(ex->GetSeverity()) << m_Prefix << " " << *ex);
        }
    }

    return true;
}


bool CDB_UserHandler_Diag::HandleAll(const TExceptions& exceptions)
{
    return x_HandleAll(exceptions);
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
    if (m_OwnOutput && (m_Output == &cerr || m_Output == &cout)) {
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
    NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
}


bool CDB_UserHandler_Stream::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return true;

    if ( !m_Output )
        return false;

    CFastMutexGuard mg(m_Mtx);

    if ( !m_Prefix.empty() ) {
        *m_Output << m_Prefix << " ";
    }

    *m_Output << ex->what();
    *m_Output << endl;

    return m_Output->good();
}


bool CDB_UserHandler_Stream::HandleAll(const TExceptions& exceptions)
{
    return x_HandleAll(exceptions);
}


////////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Exception::
//

CDB_UserHandler_Exception::~CDB_UserHandler_Exception(void)
{
}

bool
CDB_UserHandler_Exception::HandleIt(CDB_Exception* ex)
{
    if (!ex  ||  ex->GetSeverity() == eDiag_Info)
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    CDB_TruncateEx* trunc_ex = dynamic_cast<CDB_TruncateEx*>(ex);

    if (trunc_ex) {
        ERR_POST_X(7, *ex);
        return true;
    }

    ex->Throw();

    return true;
}

bool
CDB_UserHandler_Exception::HandleAll(const TExceptions& exceptions)
{
    if (exceptions.empty())
        return false;

    TExceptions::const_iterator it = exceptions.end();
    CDB_Exception* outer_ex = NULL;
    bool ret_val = false;
    while (it != exceptions.begin()  &&  !outer_ex) {
        --it;
        outer_ex = *it;
        if (outer_ex) {
            if (outer_ex->GetSeverity() == eDiag_Info)
                outer_ex = NULL;
            else if (outer_ex->GetDBErrCode() == 0) {
                outer_ex = NULL;
                ret_val = true;
            }
            else {
                CDB_TruncateEx* trunc_ex = dynamic_cast<CDB_TruncateEx*>(outer_ex);
                if (trunc_ex) {
                    ERR_POST_X(7, *trunc_ex);
                    outer_ex = NULL;
                    ret_val = true;
                }
            }
        }
    }
    if (!outer_ex)
        return ret_val;

    if (it != exceptions.begin()) {
        do {
            --it;
            if (*it)
                outer_ex->AddPrevious(*it);
        }
        while (it != exceptions.begin());
    }

    outer_ex->Throw();
    return true;
}


////////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Deferred::
//

CDB_UserHandler_Deferred::CDB_UserHandler_Deferred
(const impl::CDBHandlerStack& ultimate_handlers)
    : m_UltimateHandlers(ultimate_handlers)
{
}

CDB_UserHandler_Deferred::~CDB_UserHandler_Deferred(void)
{
    if ( !m_SavedExceptions.empty() ) {
        ERR_POST_X(8, Warning << "Internal bug: Found unreported exceptions."
                   << CStackTrace());
        bool done = false;
        while ( !done ) {
            try {
                Flush();
                done = true;
            } catch (CException& ex) {
                ERR_POST(ex);
            }
        }
    }
}

bool CDB_UserHandler_Deferred::HandleIt(CDB_Exception* ex)
{
    if (ex == NULL) {
        return false;
    }

    CFastMutexGuard LOCK(m_Mutex);
    _TRACE(*ex);
    m_SavedExceptions.push_back(ex->Clone());
    return true;
}

bool CDB_UserHandler_Deferred::HandleAll(const TExceptions& exceptions)
{
    CFastMutexGuard LOCK(m_Mutex);
    ITERATE (TExceptions, ex, exceptions) {
        if (*ex == NULL) {
            continue;
        }
        _TRACE(**ex);
        m_SavedExceptions.push_back((*ex)->Clone());
    }
    return true;
}

void CDB_UserHandler_Deferred::Flush(EDiagSev max_severity)
{
    CFastMutexGuard LOCK(m_Mutex);
    ERASE_ITERATE (TExceptions, it, m_SavedExceptions) {
        auto_ptr<CDB_Exception> ex(*it);
        VECTOR_ERASE(it, m_SavedExceptions);
        if (ex->GetSeverity() > max_severity) {
            ex->SetSeverity(max_severity);
        }
        if (max_severity < eDiag_Error) {
            try {
                m_UltimateHandlers.PostMsg(ex.get());
            } catch (CDB_Exception& ex2) {
                ERR_POST(ex2);
            }
        } else { 
            m_UltimateHandlers.PostMsg(ex.get());
        }    
    }
}


////////////////////////////////////////////////////////////////////////////////
//  CDB_UserHandler_Exception_ODBC::
//

CDB_UserHandler_Exception_ODBC::~CDB_UserHandler_Exception_ODBC(void)
{
}

bool
CDB_UserHandler_Exception_ODBC::HandleIt(CDB_Exception* ex)
{
    if (!ex)
        return false;

    ex->Throw();

    return true;
}


END_NCBI_SCOPE


