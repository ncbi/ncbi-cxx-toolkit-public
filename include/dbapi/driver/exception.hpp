#ifndef DBAPI_DRIVER___EXCEPTION__HPP
#define DBAPI_DRIVER___EXCEPTION__HPP

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
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <deque>

/** @addtogroup DbExceptions
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Helper macro for default database exception implementation.
#define NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class) \
    {} \
    exception_class(const exception_class& other) \
       : base_class(other) \
    { \
        x_Assign(other); \
    } \
public: \
    virtual ~exception_class(void) throw() {} \
    virtual const char* GetType(void) const {return #exception_class;} \
    typedef int TErrCode; \
    TErrCode GetErrCode(void) const \
    { \
        return typeid(*this) == typeid(exception_class) ? \
            (TErrCode)x_GetErrCode() : (TErrCode)CException::eInvalid; \
    } \
protected: \
    exception_class(void) {} \
    virtual const CException* x_Clone(void) const \
    { \
        return new exception_class(*this); \
    } \
private: \
    /* for the sake of semicolon at the end of macro...*/ \
    static void xx_unused_##exception_class(void)


// DEPRECATED, Will be removed soon.
enum EDB_Severity {
    eDB_Info,
    eDB_Warning,
    eDB_Error,
    eDB_Fatal,
    eDB_Unknown
};

/////////////////////////////////////////////////////////////////////////////
///
/// CDB_Exception --
///
/// Define database exception.  CDB_Exception inherits its basic
/// functionality from CException and defines additional error codes for
/// databases.


// class NCBI_DBAPIDRIVER_EXPORT CDB_Exception : public std::exception
class NCBI_DBAPIDRIVER_EXPORT CDB_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
    friend class CDB_MultiEx;

public:
    /// Error types that can be generated.
    enum EErrCode {
        eDS,
        eRPC,
        eSQL,
        eDeadlock,
        eTimeout,
        eClient,
        eMulti
    };
    typedef EErrCode EType;

    // access
    // DEPRECATED, Will be removed soon.
    EDB_Severity        Severity(void) const;
    int                 GetDBErrCode(void) const { return m_DBErrCode; }

    const char*         SeverityString(void) const;
    // DEPRECATED, Will be removed soon.
    static const char*  SeverityString(EDB_Severity sev);
    virtual const char* GetErrCodeString(void) const;

public:
    // Duplicate methods. We need them to support the old interface.

    EType Type(void) const { return static_cast<EType>(GetErrCode());  }
    // text representation of the exception type and severity
    virtual const char* TypeString() const { return GetType();         }
    int ErrCode(void) const { return GetDBErrCode();                   }
    const string& Message(void) const { return GetMsg();               }
    const string& OriginatedFrom() const { return GetModule();         }

public:
    virtual void ReportExtra(ostream& out) const;
    virtual CDB_Exception* Clone(void) const;

protected:
    CDB_Exception(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  EDiagSev severity,
                  int db_err_code)
        : CException(info, prev_exception, CException::eInvalid, message)
        , m_Severity(severity)
        , m_DBErrCode(db_err_code)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_Exception, CException);

protected:
    // data

    EDiagSev    m_Severity;
    int         m_DBErrCode;

protected:
    void x_StartOfWhat(ostream& out) const;
    void x_EndOfWhat  (ostream& out) const;

protected:
    virtual void x_Assign(const CException& src);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_DSEx : public CDB_Exception
{
public:
    CDB_DSEx(const CDiagCompileInfo& info,
             const CException* prev_exception,
             const string& message,
             EDiagSev severity,
             int db_err_code)
        : CDB_Exception(info, prev_exception, CDB_Exception::eDS, message, severity, db_err_code)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_DSEx, CDB_Exception);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_RPCEx : public CDB_Exception
{
public:
    CDB_RPCEx(const CDiagCompileInfo& info,
              const CException* prev_exception,
              const string& message,
              EDiagSev severity,
              int db_err_code,
              const string& proc_name,
              int proc_line)
        : CDB_Exception(info,
                        prev_exception,
                        CDB_Exception::eRPC,
                        message,
                        severity,
                        db_err_code)
        , m_ProcName(proc_name.empty() ? "Unknown" : proc_name)
        , m_ProcLine(proc_line)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_RPCEx, CDB_Exception);

public:
    const string& ProcName()  const { return m_ProcName; }
    int           ProcLine()  const { return m_ProcLine; }

    virtual void ReportExtra(ostream& out) const;

protected:
    virtual void x_Assign(const CException& src);

private:
    string m_ProcName;
    int    m_ProcLine;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_SQLEx : public CDB_Exception
{
public:
    CDB_SQLEx(const CDiagCompileInfo& info,
              const CException* prev_exception,
              const string& message,
              EDiagSev severity,
              int db_err_code,
              const string& sql_state,
              int batch_line)
        : CDB_Exception(info,
                        prev_exception,
                        CDB_Exception::eSQL,
                        message,
                        severity,
                        db_err_code)
        , m_SqlState(sql_state.empty() ? "Unknown" : sql_state)
        , m_BatchLine(batch_line)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_SQLEx, CDB_Exception);

public:
    const string& SqlState()   const { return m_SqlState;  }
    int           BatchLine()  const { return m_BatchLine; }

    virtual void ReportExtra(ostream& out) const;

protected:
    virtual void x_Assign(const CException& src);

private:
    string m_SqlState;
    int    m_BatchLine;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_DeadlockEx : public CDB_Exception
{
public:
    CDB_DeadlockEx(const CDiagCompileInfo& info,
                   const CException* prev_exception,
                   const string& message)
       : CDB_Exception(info, prev_exception, CDB_Exception::eDeadlock, message, eDiag_Error, 123456)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_DeadlockEx, CDB_Exception);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_TimeoutEx : public CDB_Exception
{
public:
    CDB_TimeoutEx(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  const string& message,
                  int db_err_code)
       : CDB_Exception(info,
                       prev_exception,
                       CDB_Exception::eTimeout,
                       message,
                       eDiag_Error,
                       db_err_code)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_TimeoutEx, CDB_Exception);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_ClientEx : public CDB_Exception
{
public:
    CDB_ClientEx(const CDiagCompileInfo& info,
                 const CException* prev_exception,
                 const string& message,
                 EDiagSev severity,
                 int db_err_code)
       : CDB_Exception(info,
                       prev_exception,
                       CDB_Exception::eClient,
                       message,
                       severity,
                       db_err_code)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_ClientEx, CDB_Exception);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_MultiEx : public CDB_Exception
{
public:
    // ctor/dtor
    CDB_MultiEx(const CDiagCompileInfo& info,
                const CException* prev_exception,
                unsigned int  capacity = 64)
        : CDB_Exception(info,
                        prev_exception,
                        eMulti,
                        kEmptyStr,
                        eDiag_Info,
                        0)
        , m_Bag( new CObjectFor<TExceptionStack>() )
        , m_NofRooms( capacity )
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION( CDB_MultiEx, CDB_Exception );

public:
    bool              Push(const CDB_Exception& ex);
    // REsult is not owned by CDB_MultiEx
    CDB_Exception*    Pop(void);

    unsigned int NofExceptions() const { return m_Bag->GetData().size();    }
    unsigned int Capacity()      const { return m_NofRooms;                 }

    string WhatThis(void) const;

    virtual void ReportExtra(ostream& out) const;

protected:
    void ReportErrorStack(ostream& out) const;
    virtual void x_Assign(const CException& src);

private:
    // We use "deque" instead of "stack" here we need to iterate over all 
    // recors in the container.
    typedef deque<AutoPtr<const CDB_Exception> > TExceptionStack;

    CRef<CObjectFor<TExceptionStack> > m_Bag;
    unsigned int m_NofRooms; ///< Max number of error messages to print..
};




/////////////////////////////////////////////////////////////////////////////
//
// CDB_UserHandler::   base class for user-defined handlers
//
//   Specializations of "CDB_UserHandler" -- to print error messages to:
//
// CDB_UserHandler_Default::   default destination (now:  CDB_UserHandler_Diag)
// CDB_UserHandler_Diag::      C++ Toolkit diagnostics
// CDB_UserHandler_Stream::    std::ostream specified by the user
//


class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler
{
public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex) = 0;

    // Get current global "last-resort" error handler.
    // If not set, then the default will be "CDB_UserHandler_Default".
    // This handler is guaranteed to be valid up to the program termination,
    // and it will call the user-defined handler last set by SetDefault().
    // NOTE:  never pass it to SetDefault, like:  "SetDefault(&GetDefault())"!
    static CDB_UserHandler& GetDefault(void);

    // Alternate the default global "last-resort" error handler.
    // Passing NULL will mean to ignore all errors that reach it.
    // Return previously set (or default-default if not set yet) handler.
    // The returned handler should be delete'd by the caller; the last set
    // handler will be delete'd automagically on the program termination.
    static CDB_UserHandler* SetDefault(CDB_UserHandler* h);

    // d-tor
    virtual ~CDB_UserHandler();
};


class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Diag : public CDB_UserHandler
{
public:
    CDB_UserHandler_Diag(const string& prefix = kEmptyStr);
    virtual ~CDB_UserHandler_Diag();

    // Print "*ex" to the standard C++ Toolkit diagnostics, with "prefix".
    // Always return TRUE (i.e. always process the "ex").
    virtual bool HandleIt(CDB_Exception* ex);

private:
    string m_Prefix;     // string to prefix each message with
};


class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Stream : public CDB_UserHandler
{
public:
    CDB_UserHandler_Stream(ostream*      os     = 0 /*cerr*/,
                           const string& prefix = kEmptyStr,
                           bool          own_os = false);
    virtual ~CDB_UserHandler_Stream();

    // Print "*ex" to the output stream "os", with "prefix" (as set by  c-tor).
    // Return TRUE (i.e. process the "ex") unless write to "os" failed.
    virtual bool HandleIt(CDB_Exception* ex);

private:
    ostream* m_Output;     // output stream to print messages to
    string   m_Prefix;     // string to prefix each message with
    bool     m_OwnOutput;  // if TRUE, then delete "m_Output" in d-tor
};



typedef CDB_UserHandler_Diag CDB_UserHandler_Default;

/// Generic macro to throw a database exception, given the exception class,
/// database error code and message string.
#define NCBI_DATABASE_THROW( exception_class, message, err_code, severity ) \
    throw exception_class( DIAG_COMPILE_INFO, \
        0, (message), severity, err_code )

#define DATABASE_DRIVER_ERROR( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Error )

#define DATABASE_DRIVER_WARNING( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Warning )

#define DATABASE_DRIVER_FATAL( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Fatal )

#define DATABASE_DRIVER_INFO( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Info )


#define CHECK_DRIVER_ERROR( failed, message, err_code ) \
    if ( ( failed ) ) { DATABASE_DRIVER_ERROR( message, err_code ); }
    
#define CHECK_DRIVER_WARNING( failed, message, err_code ) \
    if ( ( failed ) ) { DATABASE_DRIVER_WARNING( message, err_code ); }
    
#define CHECK_DRIVER_FATAL( failed, message, err_code ) \
    if ( ( failed ) ) { DATABASE_DRIVER_FATAL( message, err_code ); }
    
#define CHECK_DRIVER_INFO( failed, message, err_code ) \
    if ( ( failed ) ) { DATABASE_DRIVER_INFO( message, err_code ); }
    
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.18  2005/04/11 19:23:31  ssikorsk
 * Added method Clone to the CDB_Exception class
 *
 * Revision 1.17  2005/04/04 13:03:02  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.16  2003/11/04 22:22:01  vakatov
 * Factorize the code (especially the reporting one) through better inheritance.
 * +CDB_Exception::TypeString()
 * CDB_MultiEx to become more CDB_Exception-like.
 * Improve the user-defined handlers' API, add CDB_UserHandler_Diag.
 *
 * Revision 1.15  2003/08/01 20:33:03  vakatov
 * Explicitly qualify "exception" with "std::" to avoid a silly name conflict
 * with <math.h> for SUN Forte6u2 compiler
 *
 * Revision 1.14  2003/04/11 17:46:05  siyan
 * Added doxygen support
 *
 * Revision 1.13  2003/02/12 22:07:44  coremake
 * Added export specifier NCBI_DBAPIDRIVER_EXPORT to the CDB_MultiExStorage
 * class declaration
 *
 * Revision 1.12  2002/12/26 19:29:12  dicuccio
 * Added Win32 export specifier for base DBAPI library
 *
 * Revision 1.11  2002/09/26 14:24:20  soussov
 * raises the severity of deadlock and timeout from warning to error
 *
 * Revision 1.10  2002/09/04 21:45:54  vakatov
 * Added missing 'const' to CDB_Exception::SeverityString()
 *
 * Revision 1.9  2002/07/11 18:19:19  ucko
 * Simplify #includes down to <corelib/ncbistd.hpp>.
 *
 * Revision 1.8  2001/11/06 17:58:03  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.7  2001/10/04 20:28:08  vakatov
 * Added missing virtual destructors to CDB_RPCEx and CDB_SQLEx
 *
 * Revision 1.6  2001/10/01 20:09:27  vakatov
 * Introduced a generic default user error handler and the means to
 * alternate it. Added an auxiliary error handler class
 * "CDB_UserHandler_Stream".
 * Moved "{Push/Pop}{Cntx/Conn}MsgHandler()" to the generic code
 * (in I_DriverContext).
 *
 * Revision 1.5  2001/09/28 16:37:32  vakatov
 * Added missing "throw()" to exception classes derived from "std::exception"
 *
 * Revision 1.4  2001/09/27 20:08:29  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.3  2001/09/27 16:46:29  vakatov
 * Non-const (was const) exception object to pass to the user handler
 *
 * Revision 1.2  2001/09/24 20:52:18  vakatov
 * Fixed args like "string& s = 0" to "string& s = kEmptyStr"
 *
 * Revision 1.1  2001/09/21 23:39:52  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___EXCEPTION__HPP */
