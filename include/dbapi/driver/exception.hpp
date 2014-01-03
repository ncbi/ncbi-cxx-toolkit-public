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
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>

#include <deque>

/** @addtogroup DbExceptions
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CDB_Object;
class CDBParams;

BEGIN_SCOPE(impl)
class CConnection;
class CDBHandlerStack;
END_SCOPE(impl)

////////////////////////////////////////////////////////////////////////////////
/// Helper macro for default database exception implementation.
#define NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class, db_err_code) \
    { \
        this->x_InitCDB(db_err_code); \
    } \
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
    virtual CDB_Exception* Clone(void) const \
    { \
        return new exception_class(*this); \
    } \
    NCBI_EXCEPTION_DEFAULT_THROW(exception_class) \
protected: \
    exception_class(void) {} \
    virtual const CException* x_Clone(void) const \
    { \
        return new exception_class(*this); \
    } \
private: \
    /* for the sake of semicolon at the end of macro...*/ \
    static void xx_unused_##exception_class(void)


////////////////////////////////////////////////////////////////////////////////
// DEPRECATED, Will be removed soon.
enum EDB_Severity {
    eDB_Info,
    eDB_Warning,
    eDB_Error,
    eDB_Fatal,
    eDB_Unknown
};

////////////////////////////////////////////////////////////////////////////////
///
/// CDB_Exception --
///
/// Define database exception.  CDB_Exception inherits its basic
/// functionality from CException and defines additional error codes for
/// databases.


////////////////////////////////////////////////////////////////////////////////
// class NCBI_DBAPIDRIVER_EXPORT CDB_Exception : public std::exception
class NCBI_DBAPIDRIVER_EXPORT CDB_Exception :
    EXCEPTION_VIRTUAL_BASE public CException
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
        eMulti,
        eTruncate
    };
    typedef EErrCode EType;

    struct SParam {
        string      name;
        CDB_Object* value;
    };
    struct NCBI_DBAPIDRIVER_EXPORT SParams : public CObject {
        ~SParams(void);

        typedef vector<SParam> TParams;
        TParams params;
    };

    // access
    // DEPRECATED, Will be removed soon.
    NCBI_DEPRECATED
    EDB_Severity        Severity(void) const;
    int                 GetDBErrCode(void) const { return m_DBErrCode; }

    const char*         SeverityString(void) const;
    // DEPRECATED, Will be removed soon.
    NCBI_DEPRECATED
    static const char*  SeverityString(EDB_Severity sev);
    virtual const char* GetErrCodeString(void) const;

public:
    // Duplicate methods. We need them to support the old interface.

    EType Type(void) const;
    // text representation of the exception type and severity
    virtual const char* TypeString() const { return GetType();         }
    int ErrCode(void) const { return GetDBErrCode();                   }
    const string& Message(void) const { return GetMsg();               }
    const string& OriginatedFrom() const { return GetModule();         }

    void SetServerName(const string& name) { m_ServerName = name;      }
    const string& GetServerName(void) const { return m_ServerName;     }

    void SetUserName(const string& name) { m_UserName = name;          }
    const string& GetUserName(void) const { return m_UserName;         }

    void SetFromConnection(const impl::CConnection& connection);

    void SetExtraMsg(const string& msg) { m_ExtraMsg = msg;            }
    const string& GetExtraMsg(void) const { return m_ExtraMsg;         }

    void SetParams(const CDBParams* params);
    void SetParams(const CDBParams& params) { SetParams(&params); }
    CConstRef<SParams> GetParams(void) const { return m_Params; }
    

    /// WARNING !!! Sybase severity value can be provided by Sybase/FreeTDS
    /// ctlib/dblib drivers only.
    void SetSybaseSeverity(int severity) { m_SybaseSeverity = severity;}
    int GetSybaseSeverity(void) const { return m_SybaseSeverity;       }

public:
    virtual void ReportExtra(ostream& out) const;
    virtual CDB_Exception* Clone(void) const;

public:
    // Warning: exception constructor must be "public" because MSVC 7.1 won't
    // catch parent exceptions other way.
    CDB_Exception(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  EDiagSev severity,
                  int db_err_code)
        : CException(info,
                     prev_exception,
                     (CException::EErrCode)err_code,
                     message,
                     severity )
        , m_DBErrCode(db_err_code)
        , m_SybaseSeverity(0)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_Exception, CException);

protected:
    int     m_DBErrCode;

protected:
    void x_StartOfWhat(ostream& out) const;
    void x_EndOfWhat  (ostream& out) const;
    virtual void x_Assign(const CException& src);
    void x_InitCDB(int db_error_code) { m_DBErrCode = db_error_code; }

private:
    string  m_ServerName;
    string  m_UserName;
    int     m_SybaseSeverity;
    string  m_ExtraMsg;
    CRef<SParams> m_Params;
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_DSEx : public CDB_Exception
{
public:
    CDB_DSEx(const CDiagCompileInfo& info,
             const CException* prev_exception,
             const string& message,
             EDiagSev severity,
             int db_err_code)
        : CDB_Exception(info, prev_exception,
                        CDB_Exception::eDS,
                        message, severity,
                        db_err_code)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_DSEx,
                                                       CDB_Exception,
                                                       db_err_code);

};


////////////////////////////////////////////////////////////////////////////////
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
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_RPCEx,
                                                       CDB_Exception,
                                                       db_err_code);

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


////////////////////////////////////////////////////////////////////////////////
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
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_SQLEx,
                                                       CDB_Exception,
                                                       db_err_code);

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


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_DeadlockEx : public CDB_Exception
{
public:
    CDB_DeadlockEx(const CDiagCompileInfo& info,
                   const CException* prev_exception,
                   const string& message)
       : CDB_Exception(info,
                       prev_exception,
                       CDB_Exception::eDeadlock,
                       message,
                       eDiag_Error,
                       123456)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_DeadlockEx,
                                                       CDB_Exception,
                                                       123456);

};


////////////////////////////////////////////////////////////////////////////////
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
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_TimeoutEx,
                                                       CDB_Exception,
                                                       db_err_code);
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_ClientEx : public CDB_Exception
{
public:
    CDB_ClientEx(const CDiagCompileInfo& info,
                 const CException* prev_exception,
                 const string& message,
                 EDiagSev severity,
                 int db_err_code,
                 const string& dbg_info,
                 const impl::CConnection& connection,
                 const CDBParams* params)
       : CDB_Exception(info, prev_exception, CDB_Exception::eClient,
                       message, severity, db_err_code)
    {
        // Many callers already supply it.
        if ( !NStr::EndsWith(message, dbg_info) ) {
            AddToMessage(dbg_info);
        }
        SetFromConnection(connection);
        SetParams(params);
    }
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
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_ClientEx,
                                                       CDB_Exception,
                                                       db_err_code);
};



////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_TruncateEx : public CDB_Exception
{
public:
    CDB_TruncateEx(const CDiagCompileInfo& info,
                   const CException* prev_exception,
                   const string& message,
                   int db_err_code)
       : CDB_Exception(info,
                       prev_exception,
                       CDB_Exception::eTruncate,
                       message,
                       eDiag_Warning,
                       db_err_code)
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_TruncateEx,
                                                       CDB_Exception,
                                                       db_err_code);
};



////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_MultiEx : public CDB_Exception
{
public:
    // ctor/dtor
    CDB_MultiEx(const CDiagCompileInfo& info,
                const CException* prev_exception,
                unsigned int  capacity = 64)
        : CDB_Exception(info,
                        prev_exception,
                        CDB_Exception::eMulti,
                        kEmptyStr,
                        eDiag_Info,
                        0)
        , m_Bag( new CObjectFor<TExceptionStack>() )
        , m_NofRooms( capacity )
        NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDB_MultiEx,
                                                       CDB_Exception,
                                                       0 );

public:
    bool              Push(const CDB_Exception& ex);
    // REsult is not owned by CDB_MultiEx
    CDB_Exception*    Pop(void);

    unsigned int NofExceptions() const {
        return static_cast<unsigned int>( m_Bag->GetData().size() );
    }
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


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler : public CObject
{
public:
    CDB_UserHandler(void);
    virtual ~CDB_UserHandler(void);

public:
    /// Exception container type
    /// @sa HandleAll()
    typedef deque<CDB_Exception*> TExceptions;
    static void ClearExceptions(TExceptions& expts);

    /// Handle all of the exceptions resulting from a native API call.
    /// @param exceptions
    ///   List of exceptions
    /// @return
    ///   TRUE if the exceptions are handled -- in this case, HandleIt() methods
    ///   will *NOT* be called.
    /// @sa HandleIt(), CException::Throw()
    virtual bool HandleAll(const TExceptions& exceptions);

    /// Handle the exceptions resulting from a native API call, one-by-one.
    /// @return
    ///   TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    /// @sa HandleAll(), CException::Throw()
    virtual bool HandleIt(CDB_Exception* ex) = 0;

    /// Handle message resulting from a native API call.
    /// Method MUST NOT throw any exceptions.
    /// 
    /// @return
    ///   TRUE if message is processed and shouldn't be saved for later
    ///   appearance as CDB_Exception, FALSE otherwise (default value is FALSE)
    virtual bool HandleMessage(int severity, int msgnum, const string& message);

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

    /// Method is deprecated. Use CDB_Exception::GetExtraMsg() instead.
    NCBI_DEPRECATED string GetExtraMsg(void) const;
    /// Method is deprecated. Use CDB_Exception::SetExtraMsg() instead.
    NCBI_DEPRECATED void SetExtraMsg(const string& msg) const;

protected:
    /// Simply call HandleIt on each exception.
    bool x_HandleAll(const TExceptions& exceptions);
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Diag : public CDB_UserHandler
{
public:
    CDB_UserHandler_Diag(const string& prefix = kEmptyStr);
    virtual ~CDB_UserHandler_Diag();

    // Print "*ex" to the standard C++ Toolkit diagnostics, with "prefix".
    // Always return TRUE (i.e. always process the "ex").
    virtual bool HandleIt(CDB_Exception* ex);

    virtual bool HandleAll(const TExceptions& exceptions);

private:
    string m_Prefix;     // string to prefix each message with
};


////////////////////////////////////////////////////////////////////////////////
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

    virtual bool HandleAll(const TExceptions& exceptions);

private:
    mutable CFastMutex  m_Mtx;

    ostream* m_Output;     // output stream to print messages to
    string   m_Prefix;     // string to prefix each message with
    bool     m_OwnOutput;  // if TRUE, then delete "m_Output" in d-tor
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Exception :
    public CDB_UserHandler
{
public:
    virtual ~CDB_UserHandler_Exception(void);

    virtual bool HandleIt(CDB_Exception* ex);
    virtual bool HandleAll(const TExceptions& exceptions);
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Deferred : public CDB_UserHandler
{
public:
    CDB_UserHandler_Deferred(const impl::CDBHandlerStack& ultimate_handlers);
    ~CDB_UserHandler_Deferred(void);
    
    bool HandleIt(CDB_Exception* ex);
    bool HandleAll(const TExceptions& exceptions);

    void Flush(EDiagSev max_severity = eDiagSevMax);

private:
    TExceptions                  m_SavedExceptions;
    const impl::CDBHandlerStack& m_UltimateHandlers;
    CFastMutex                   m_Mutex;
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_UserHandler_Exception_ODBC :
    public CDB_UserHandler
{
public:
    virtual ~CDB_UserHandler_Exception_ODBC(void);

    virtual bool HandleIt(CDB_Exception* ex);
};


////////////////////////////////////////////////////////////////////////////////
typedef CDB_UserHandler_Diag CDB_UserHandler_Default;

////////////////////////////////////////////////////////////////////////////////
/// Generic macro to throw a database exception, given the exception class,
/// database error code and message string.
#define NCBI_DATABASE_THROW( exception_class, message, err_code, severity ) \
    throw exception_class( DIAG_COMPILE_INFO, \
        0, (message), severity, err_code )
#define NCBI_DATABASE_RETHROW( prev_exception, exception_class, message, \
    err_code, severity ) \
    throw exception_class( DIAG_COMPILE_INFO, \
        &(prev_exception), (message), severity, err_code )

// Driver code typically redefines NCBI_DATABASE_(RE)THROW in terms of these.
#define NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
                                      dbg_info, conn, params) \
    throw ex_class(DIAG_COMPILE_INFO, NULL, (message), severity, err_code, \
                   dbg_info, conn, params)
#define NCBI_DATABASE_RETHROW_ANNOTATED(prev_ex, ex_class, message, err_code, \
                                        severity, dbg_info, conn, params) \
    throw ex_class(DIAG_COMPILE_INFO, &(prev_ex), (message), severity, \
                   err_code, dbg_info, conn, params)

#define DATABASE_DRIVER_ERROR( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Error )
#define DATABASE_DRIVER_ERROR_EX( prev_exception, message, err_code ) \
    NCBI_DATABASE_RETHROW( prev_exception, CDB_ClientEx, message, err_code, \
    eDiag_Error )

#define DATABASE_DRIVER_WARNING( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Warning )
#define DATABASE_DRIVER_WARNING_EX( prev_exception, message, err_code ) \
    NCBI_DATABASE_RETHROW( prev_exception, CDB_ClientEx, message, err_code, \
    eDiag_Warning )

#define DATABASE_DRIVER_FATAL( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Fatal )
#define DATABASE_DRIVER_FATAL_EX( prev_exception, message, err_code ) \
    NCBI_DATABASE_RETHROW( prev_exception, CDB_ClientEx, message, err_code, \
    eDiag_Fatal )

#define DATABASE_DRIVER_INFO( message, err_code ) \
    NCBI_DATABASE_THROW( CDB_ClientEx, message, err_code, eDiag_Info )
#define DATABASE_DRIVER_INFO_EX( prev_exception, message, err_code ) \
    NCBI_DATABASE_RETHROW( prev_exception, CDB_ClientEx, message, err_code, \
    eDiag_Info )


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


#endif  /* DBAPI_DRIVER___EXCEPTION__HPP */
