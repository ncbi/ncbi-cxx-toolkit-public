#ifndef NCBIEXPT__HPP
#define NCBIEXPT__HPP

/*  $Id$
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
 * Author:  Denis Vakatov
 *
 *
 */

/// @file ncbiexpt.hpp
/// Defines NCBI C++ exception handling.
///
/// Contains support for the NCBI C++ exception handling mechanisms and
/// auxiliary ad hoc macros to "catch" certain types of errors, and macros for
/// the C++ exception specification.


#include <corelib/ncbidiag.hpp>
#include <errno.h>
#include <string.h>
#include <string>
#include <stdexcept>
#include <typeinfo>


/** @addtogroup Exception
 *
 * @{
 */


BEGIN_NCBI_SCOPE

#if (_MSC_VER >= 1200)
#undef NCBI_USE_THROW_SPEC
#endif

/// Define THROWS macros for C++ exception specification.
///
/// Define use of C++ exception specification mechanism:
///   "f(void) throw();"       <==  "f(void) THROWS_NONE;"
///   "g(void) throw(e1,e2);"  <==  "f(void) THROWS((e1,e2));"
#if defined(NCBI_USE_THROW_SPEC)
#  define THROWS_NONE throw()
#  define THROWS(x) throw x
#else
#  define THROWS_NONE
#  define THROWS(x)
#endif

/// ABORT_ON_THROW controls if program should be aborted.
#define ABORT_ON_THROW "ABORT_ON_THROW"

/// Specify whether to call "abort()" inside the DoThrowTraceAbort().
///
/// By default, this feature is not activated unless
/// -  environment variable $ABORT_ON_THROW is set (to any value), or
/// -  registry value of ABORT_ON_THROW, section DEBUG is set (to any value)
NCBI_XNCBI_EXPORT
extern void SetThrowTraceAbort(bool abort_on_throw_trace);

/// "abort()" the program if set by SetThrowTraceAbort() or $ABORT_ON_THROW.
NCBI_XNCBI_EXPORT
extern void DoThrowTraceAbort(void);

/// Print the specified debug message.
NCBI_XNCBI_EXPORT
extern void DoDbgPrint(const CDiagCompileInfo& info, const char* message);

/// Print the specified debug message.
NCBI_XNCBI_EXPORT
extern void DoDbgPrint(const CDiagCompileInfo& info, const string& message);

/// Print the specified debug messages.
NCBI_XNCBI_EXPORT
extern void DoDbgPrint(const CDiagCompileInfo& info,
                       const char* msg1, const char* msg2);

#if defined(_DEBUG)

/// Templated function for printing debug message.
///
/// Print debug message for the specified exception type.
template<typename T>
inline
const T& DbgPrint(const CDiagCompileInfo& info,
                  const T& e, const char* e_str)
{
    DoDbgPrint(info, e_str, e.what());
    return e;
}

/// Print debug message for "const char*" object.
inline
const char* DbgPrint(const CDiagCompileInfo& info,
                     const char* e, const char* )
{
    DoDbgPrint(info, e);
    return e;
}

/// Print debug message for "char*" object.
inline
char* DbgPrint(const CDiagCompileInfo& info,
               char* e, const char* )
{
    DoDbgPrint(info, e);
    return e;
}

/// Print debug message for "std::string" object.
inline
const string& DbgPrint(const CDiagCompileInfo& info,
                       const string& e, const char* )
{
    DoDbgPrint(info, e);
    return e;
}

/// Create diagnostic stream for printing specified message and "abort()" the
/// program if set by SetThrowTraceAbort() or $ABORT_ON_THROW.
///
/// @sa
///   SetThrowTraceAbort(), DoThrowTraceAbort()
template<typename T>
inline
const T& DbgPrintP(const CDiagCompileInfo& info, const T& e, const char* e_str)
{
    CNcbiDiag(info, eDiag_Trace) << e_str << ": " << e;
    DoThrowTraceAbort();
    return e;
}

/// Create diagnostic stream for printing specified message.
///
/// Similar to DbgPrintP except that "abort()" not executed.
/// @sa
///   DbgPrintP()
template<typename T>
inline
const T& DbgPrintNP(const CDiagCompileInfo& info,
                    const T& e,
                    const char* e_str)
{
    DoDbgPrint(info, e_str);
    return e;
}

/// Rethrow trace.
///
/// Reason for do {...} while in macro definition is to permit a natural
/// syntax usage when a user wants to write something like:
///
/// if (expression)
///     RETHROW_TRACE;
/// else do_something_else;
///
/// Example:
/// -  RETHROW_TRACE;
#  define RETHROW_TRACE do { \
    _TRACE("EXCEPTION: re-throw"); \
    NCBI_NS_NCBI::DoThrowTraceAbort(); \
    throw; \
} while(0)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Argument can be a simple string, or an exception object.
///
/// Example:
/// -  THROW0_TRACE("Throw just a string");
/// -  THROW0_TRACE(runtime_error("message"));
#  define THROW0_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrint(DIAG_COMPILE_INFO, \
        exception_object, #exception_object)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Argument can be any printable object; that is, any object with a defined
/// output operator.
///
/// Program may abort if so set by SetThrowTraceAbort() or $ABORT_ON_THROW.
///
/// Example:
/// -  THROW0p_TRACE(123);
/// -  THROW0p_TRACE(complex(1,2));
/// @sa
///   THROW0np_TRACE
#  define THROW0p_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrintP(DIAG_COMPILE_INFO, \
        exception_object, #exception_object)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Argument can be any printable object; that is, any object with a defined
/// output operator.
///
/// Similar to THROW0p_TRACE except that program is not "aborted" when
/// exception is thrown, and argument type can be an aggregate type such as
/// Vector<T> where T is a printable argument.
///
/// Example:
/// -  THROW0np_TRACE(vector<char>());
/// @sa
///   THROW0p_TRACE
#  define THROW0np_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrintNP(DIAG_COMPILE_INFO, \
        exception_object, #exception_object)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with the specified initialization
/// argument. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization argument.
///
/// Example:
/// -  THROW1_TRACE(runtime_error, "Something is weird...");
#  define THROW1_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrint(DIAG_COMPILE_INFO, \
        exception_class(exception_arg), #exception_class)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with a the specified initialization
/// argument. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization argument.
///
/// Program may abort if so set by SetThrowTraceAbort() or $ABORT_ON_THROW.
///
/// Example:
/// -  THROW1p_TRACE(int, 32);
/// @sa
///   THROW1np_TRACE
#  define THROW1p_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrintP(DIAG_COMPILE_INFO,    \
        exception_class(exception_arg), #exception_class)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with a the specified initialization
/// argument. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization argument.
///
/// Similar to THROW1p_TRACE except that program is not "aborted" when
/// exception is thrown, and argument type can be an aggregate type such as
/// Vector<T> where T is a printable argument.
///
/// Example:
/// -  THROW1np_TRACE(CUserClass, "argument");
#  define THROW1np_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrintNP(DIAG_COMPILE_INFO,    \
        exception_class(exception_arg), #exception_class)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with a the specified initialization
/// arguments. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization arguments.
///
/// Similar to THROW1_TRACE except that the exception class can have multiple
/// initialization arguments instead of just one.
///
/// Example:
/// -  THROW_TRACE(bad_alloc, ());
/// -  THROW_TRACE(runtime_error, ("Something is weird..."));
/// -  THROW_TRACE(CParseException, ("Some parse error", 123));
/// @sa
///   THROW1_TRACE
#  define THROW_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrint(DIAG_COMPILE_INFO,    \
        exception_class exception_args, #exception_class)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with a the specified initialization
/// arguments. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization arguments.
///
/// Program may abort if so set by SetThrowTraceAbort() or $ABORT_ON_THROW.
///
/// Similar to THROW1p_TRACE except that the exception class can have multiple
/// initialization arguments instead of just one.
///
/// Example:
/// - THROWp_TRACE(complex, (2, 3));
/// @sa
///   THROW1p_TRACE
#  define THROWp_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrintP(DIAG_COMPILE_INFO,    \
        exception_class exception_args, #exception_class)

/// Throw trace.
///
/// Combines diagnostic message trace and exception throwing. First the
/// diagnostic message is printed, and then exception is thrown.
///
/// Arguments can be any exception class with a the specified initialization
/// argument. The class argument need not be derived from std::exception as
/// a new class object is constructed using the specified class name and
/// initialization argument.
///
/// Argument type can be an aggregate type such as Vector<T> where T is a
/// printable argument.
///
/// Similar to THROWp_TRACE except that program is not "aborted" when
/// exception is thrown.
///
/// Example:
/// -  THROWnp_TRACE(CUserClass, (arg1, arg2));
#  define THROWnp_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrintNP(DIAG_COMPILE_INFO,    \
        exception_class exception_args, #exception_class)

#else  /* _DEBUG */

// No trace/debug versions of these macros.

#  define RETHROW_TRACE \
    throw
#  define THROW0_TRACE(exception_object) \
    throw exception_object
#  define THROW0p_TRACE(exception_object) \
    throw exception_object
#  define THROW0np_TRACE(exception_object) \
    throw exception_object
#  define THROW1_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW1p_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW1np_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW_TRACE(exception_class, exception_args) \
    throw exception_class exception_args
#  define THROWp_TRACE(exception_class, exception_args) \
    throw exception_class exception_args
#  define THROWnp_TRACE(exception_class, exception_args) \
    throw exception_class exception_args

#endif  /* else!_DEBUG */


/// Standard handling of "exception"-derived exceptions.
#define STD_CATCH(message) \
catch (NCBI_NS_STD::exception& e) { \
      NCBI_NS_NCBI::CNcbiDiag() << NCBI_NS_NCBI::Error \
           << "[" << message << "]" << "Exception: " << e.what(); \
}


/// Standard handling of "exception"-derived exceptions; catches non-standard
/// exceptiuons and generates "unknown exception" for all other exceptions.
#define STD_CATCH_ALL(message) \
STD_CATCH(message) \
    catch (...) { \
      NCBI_NS_NCBI::CNcbiDiag() << NCBI_NS_NCBI::Error \
           << "[" << message << "]" << "Unknown exception"; \
}


/////////////////////////////////////////////////////////////////////////////
// CException: useful macros

/// Generic macro to throw an exception, given the exception class,
/// error code and message string.
#define NCBI_THROW(exception_class, err_code, message) \
    throw exception_class(DIAG_COMPILE_INFO,           \
        0, exception_class::err_code, (message))

// NCBI_THROW(foo).SetModule("aaa");
/// Generic macro to make an exception, given the exception class,
/// error code and message string.
#define NCBI_EXCEPTION(exception_class, err_code, message)           \
    (exception_class(DIAG_COMPILE_INFO, 0, exception_class::err_code,\
                     (message) ))


/// Generic macro to make an exception, given the exception class,
/// previous exception , error code and message string.
#define NCBI_EXCEPTION_EX(prev_exception, exception_class, err_code, message) \
    exception_class(DIAG_COMPILE_INFO,                                        \
        &(prev_exception), exception_class::err_code, (message))

/// Generic macro to re-throw an exception.
#define NCBI_RETHROW(prev_exception, exception_class, err_code, message) \
    throw exception_class(DIAG_COMPILE_INFO, \
        &(prev_exception), exception_class::err_code, (message))

/// Generic macro to re-throw the same exception.
#define NCBI_RETHROW_SAME(prev_exception, message)              \
    do { prev_exception.AddBacklog(DIAG_COMPILE_INFO, message); \
    throw; }  while (0)

/// Generate a report on the exception.
#define NCBI_REPORT_EXCEPTION(title,ex) \
    CExceptionReporter::ReportDefault(DIAG_COMPILE_INFO,title,ex,eDPF_Default)



/////////////////////////////////////////////////////////////////////////////
// CException

// Forward declaration of CExceptionReporter.
class CExceptionReporter;


/////////////////////////////////////////////////////////////////////////////
///
/// CException --
///
/// Define an extended exception class based on the C+++ std::exception.
///
/// CException inherits its basic functionality from std::exception and
/// defines additional generic error codes for applications, and error
/// reporting capabilities.

class NCBI_XNCBI_EXPORT CException : public std::exception
{
public:
    /// Error types that an application can generate.
    ///
    /// Each derived class has its own error codes and their interpretations.
    /// Define two generic error codes "eInvalid" and "eUnknown" to be used
    /// by all NCBI applications.
    enum EErrCode {
        eInvalid = -1, ///< To be used ONLY as a return value;
                       ///< please, NEVER throw an exception with this code.
        eUnknown = 0   ///< Unknown exception.
    };
    typedef int TErrCode;

    /// Constructor.
    ///
    /// When throwing an exception initially, "prev_exception" must be 0.
    CException(const CDiagCompileInfo& info,
               const CException* prev_exception,
               EErrCode err_code,const string& message);

    /// Copy constructor.
    CException(const CException& other);

    /// Add a message to backlog (to re-throw the same exception then).
    void AddBacklog(const CDiagCompileInfo& info,const string& message);


    // ---- Reporting --------------

    /// Standard report (includes full backlog).
    virtual const char* what(void) const throw();

    /// Report the exception.
    ///
    /// Report the exception using "reporter" exception reporter.
    /// If "reporter" is not specified (value 0), then use the default
    /// reporter as set with CExceptionReporter::SetDefault.
    void Report(const CDiagCompileInfo& info,
                const string& title, CExceptionReporter* reporter = 0,
                TDiagPostFlags flags = eDPF_Trace) const;

    /// Report this exception only.
    ///
    /// Report as a string this exception only. No backlog is attached.
    string ReportThis(TDiagPostFlags flags = eDPF_Trace) const;

    /// Report all exceptions.
    ///
    /// Report as a string all exceptions. Include full backlog.
    string ReportAll (TDiagPostFlags flags = eDPF_Trace) const;

    /// Report "standard" attributes.
    ///
    /// Report "standard" attributes (file, line, type, err.code, user message)
    /// into the "out" stream (this exception only, no backlog).
    void ReportStd(ostream& out, TDiagPostFlags flags = eDPF_Trace) const;

    /// Report "non-standard" attributes.
    ///
    /// Report "non-standard" attributes (those of derived class) into the
    /// "out" stream.
    virtual void ReportExtra(ostream& out) const;

    /// Enable background reporting.
    ///
    /// If background reporting is enabled, then calling what() or ReportAll()
    /// would also report exception to the default exception reporter.
    /// @return
    ///   The previous state of the flag.
    static bool EnableBackgroundReporting(bool enable);


    // ---- Attributes ---------

    /// Get class name as a string.
    virtual const char* GetType(void) const;

    /// Get error code interpreted as text.
    virtual const char* GetErrCodeString(void) const;

    /// Get file name used for reporting.
    const string& GetFile(void) const { return m_File; }

    /// Set module name used for reporting.
    CException& SetModule(const string& module)
    { m_Module = module;  return *this; }

    /// Get module name used for reporting.
    const string& GetModule(void) const { return m_Module; }

    /// Set class name used for reporting.
    CException& SetClass(const string& nclass)
    { m_Class = nclass;  return *this; }

    /// Get class name used for reporting.
    const string& GetClass(void) const { return m_Class; }

    /// Set function name used for reporting.
    CException& SetFunction(const string& function)
    { m_Function = function;  return *this; }

    /// Get function name used for reporting.
    const string& GetFunction(void) const { return m_Function; }

    /// Get line number where error occurred.
    int GetLine(void) const { return m_Line; }

    /// Get error code.
    TErrCode GetErrCode(void) const;

    /// Get message string.
    const string& GetMsg(void) const { return m_Msg; }

    /// Get "previous" exception from the backlog.
    const CException* GetPredecessor(void) const { return m_Predecessor; }

    /// Destructor.
    virtual ~CException(void) throw();

protected:
    /// Constructor with no arguments.
    ///
    /// Required in case of multiple inheritance.
    CException(void);

    /// Helper method for reporting to the system debugger.
    virtual void x_ReportToDebugger(void) const;

    /// Helper method for cloning the exception.
    virtual const CException* x_Clone(void) const;

    /// Helper method for initializing exception data.
    virtual void x_Init(const CDiagCompileInfo& info,
                        const string& message,
                        const CException* prev_exception);

    /// Helper method for copying exception data.
    virtual void x_Assign(const CException& src);

    /// Helper method for assigning error code.
    virtual void x_AssignErrCode(const CException& src);

    /// Helper method for initializing error code.
    virtual void x_InitErrCode(CException::EErrCode err_code);

    /// Helper method for getting error code.
    virtual int  x_GetErrCode(void) const { return m_ErrCode; }

private:
    string  m_File;                  ///< File     to report on
    int     m_Line;                  ///< Line number
    int     m_ErrCode;               ///< Error code
    string  m_Msg;                   ///< Message string
    string  m_Module;                ///< Module   to report on
    string  m_Class;                 ///< Class    to report on
    string  m_Function;              ///< Function to report on

    mutable string m_What;           ///< What type of exception
    const CException* m_Predecessor; ///< Previous exception

    mutable bool m_InReporter;       ///< Reporter flag
    static  bool sm_BkgrEnabled;     ///< Background reporting enabled flag

    /// Private assignment operator to prohibit assignment.
    CException& operator= (const CException&);
};


/// Return valid pointer to uppermost derived class only if "from" is _really_
/// the object of the desired type.
///
/// Do not cast to intermediate types (return NULL if such cast is attempted).
template <class TTo, class TFrom>
const TTo* UppermostCast(const TFrom& from)
{
    return typeid(from) == typeid(TTo) ? dynamic_cast<const TTo*>(&from) : 0;
}

/// Helper macro for default exception implementation.
/// @sa
///   NCBI_EXCEPTION_DEFAULT
#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class) \
    { \
        x_Init(info, message, prev_exception); \
        x_InitErrCode((CException::EErrCode) err_code); \
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
protected: \
    exception_class(void) {} \
    virtual const CException* x_Clone(void) const \
    { \
        return new exception_class(*this); \
    } \
private: \
    /* for the sake of semicolon at the end of macro...*/ \
    static void xx_unused_##exception_class(void)


/// To help declare new exception class.
///
/// This can be used ONLY if the derived class does not have any additional
/// (non-standard) data members.
#define NCBI_EXCEPTION_DEFAULT(exception_class, base_class)         \
public:                                                             \
    exception_class(const CDiagCompileInfo& info,                   \
        const CException* prev_exception,                           \
                    EErrCode err_code,const string& message)        \
        : base_class(info, prev_exception,                          \
            (base_class::EErrCode) CException::eInvalid, (message)) \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class)


/// Helper macro added to support templatized exceptions.
///
/// GCC starting from 3.2.2 warns about implicit typenames - this macro fixes
/// the warning.
#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL(exception_class, base_class) \
    { \
        this->x_Init(info, message, prev_exception); \
        this->x_InitErrCode((typename CException::EErrCode) err_code); \
    } \
    exception_class(const exception_class& other) \
       : base_class(other) \
    { \
        x_Assign(other); \
    } \
    virtual ~exception_class(void) throw() {} \
    virtual const char* GetType(void) const {return #exception_class;} \
    typedef int TErrCode; \
    TErrCode GetErrCode(void) const \
    { \
        return typeid(*this) == typeid(exception_class) ? \
            (TErrCode) this->x_GetErrCode() : \
            (TErrCode) CException::eInvalid; \
    } \
protected: \
    exception_class(void) {} \
    virtual const CException* x_Clone(void) const \
    { \
        return new exception_class(*this); \
    } \
private: \
    /* for the sake of semicolon at the end of macro...*/ \
   // static void xx_unused_##exception_class(void)



/// Exception bug workaround for GCC version less than 3.00.
///
/// GCC compiler v.2.95 has a bug: one should not use virtual base class in
/// exception declarations - a program crashes when deleting such an exception
/// (this is fixed in newer versions of the compiler).
#if defined(NCBI_COMPILER_GCC)
#  if NCBI_COMPILER_VERSION < 300
#    define EXCEPTION_BUG_WORKAROUND
#  endif
#endif

#if defined(EXCEPTION_BUG_WORKAROUND)
#  define EXCEPTION_VIRTUAL_BASE
#else
#  define EXCEPTION_VIRTUAL_BASE virtual
#endif



/////////////////////////////////////////////////////////////////////////////
///
/// CExceptionReporter --
///
/// Define exception reporter.

class NCBI_XNCBI_EXPORT CExceptionReporter
{
public:
    /// Constructor.
    CExceptionReporter(void);

    /// Destructor.
    virtual ~CExceptionReporter(void);

    /// Set default reporter.
    static void SetDefault(const CExceptionReporter* handler);

    /// Get default reporter.
    static const CExceptionReporter* GetDefault(void);

    /// Enable/disable using default reporter.
    ///
    /// @return
    ///   Previous state of this flag.
    static bool EnableDefault(bool enable);

    /// Report exception using default reporter.
    static void ReportDefault(const CDiagCompileInfo& info,
                              const string& title, const CException& ex,
                              TDiagPostFlags flags = eDPF_Trace);

    /// Report exception with _this_ reporter
    virtual void Report(const char* file, int line,
                        const string& title, const CException& ex,
                        TDiagPostFlags flags = eDPF_Trace) const = 0;

private:
    static const CExceptionReporter* sm_DefHandler; ///< Default handler
    static bool                      sm_DefEnabled; ///< Default enable flag
};



/////////////////////////////////////////////////////////////////////////////
///
/// CExceptionReporterStream --
///
/// Define exception reporter stream.

class NCBI_XNCBI_EXPORT CExceptionReporterStream : public CExceptionReporter
{
public:
    /// Constructor.
    CExceptionReporterStream(ostream& out);

    /// Destructor.
    virtual ~CExceptionReporterStream(void);

    /// Report specified exception on output stream.
    virtual void Report(const char* file, int line,
                        const string& title, const CException& ex,
                        TDiagPostFlags flags = eDPF_Trace) const;
private:
    ostream& m_Out;   ///< Output stream
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCoreException --
///
/// Define corelib exception.  CCoreException inherits its basic
/// functionality from CException and defines additional error codes for
/// applications.

class NCBI_XNCBI_EXPORT CCoreException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  corelib can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eCore,          ///< Generic corelib error
        eNullPtr,       ///< Null pointer error
        eDll,           ///< Dll error
        eDiagFilter,    ///< Illegal syntax of the diagnostics filter string
        eInvalidArg     ///< Invalid argument error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CCoreException, CException);
};



// Some implementations return char*, so strict compilers may refuse
// to let them satisfy TStrerror without a wrapper.  However, they
// don't all agree on what form the wrapper should take. :-/
#ifdef NCBI_COMPILER_GCC
inline
const char* NcbiStrerror(int errnum) { return ::strerror(errnum); }
#  define NCBI_STRERROR_WRAPPER NCBI_NS_NCBI::NcbiStrerror
#else
class CStrErrAdapt
{
public:
    static const char* strerror(int errnum) { return ::strerror(errnum); }
};
#  define NCBI_STRERROR_WRAPPER NCBI_NS_NCBI::CStrErrAdapt::strerror
#endif


/////////////////////////////////////////////////////////////////////////////
// Auxiliary exception classes:
//   CErrnoException
//   CParseException
//


/// Define function type for "strerror" function.
typedef const char* (*TStrerror)(int errnum);


/////////////////////////////////////////////////////////////////////////////
///
/// CErrnoTemplExceptionEx --
///
/// Define template class for easy generation of Errno-like exception classes.

template <class TBase, TStrerror PErrstr=NCBI_STRERROR_WRAPPER >
class CErrnoTemplExceptionEx : EXCEPTION_VIRTUAL_BASE public TBase
{
public:
    /// Error type that an application can generate.
    enum EErrCode {
        eErrno          ///< Error code
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErrno: return "eErrno";
        default:     return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    CErrnoTemplExceptionEx(const CDiagCompileInfo& info,
                           const CException* prev_exception,
                           EErrCode err_code, const string& message)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message)
    {
        m_Errno = errno;
        this->x_Init(info, message + ": " + PErrstr(m_Errno),
                     prev_exception);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Constructor.
    CErrnoTemplExceptionEx(const CDiagCompileInfo& info,
                           const CException* prev_exception,
                           EErrCode err_code, const string& message,
                           int errnum)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message),
            m_Errno(errnum)
    {
        this->x_Init(info, message + ": " + PErrstr(m_Errno),
                     prev_exception);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Copy constructor.
    CErrnoTemplExceptionEx(const CErrnoTemplExceptionEx<TBase, PErrstr>& other)
        : TBase( other)
    {
        m_Errno = other.m_Errno;
        x_Assign(other);
    }

    /// Destructor.
    virtual ~CErrnoTemplExceptionEx(void) throw() {}

    /// Report error number on stream.
    virtual void ReportExtra(ostream& out) const
    {
        out << "m_Errno = " << m_Errno;
    }

    // Attributes.

    /// Get type of class.
    virtual const char* GetType(void) const { return "CErrnoTemplException"; }

    typedef int TErrCode;
    /// Get error code.
    TErrCode GetErrCode(void) const
    {
        return typeid(*this) ==
            typeid(CErrnoTemplExceptionEx<TBase, PErrstr>) ?
               (TErrCode) this->x_GetErrCode() :
               (TErrCode) CException::eInvalid;
    }

    /// Get error number.
    int GetErrno(void) const throw() { return m_Errno; }

protected:
    /// Constructor.
    CErrnoTemplExceptionEx(void) { m_Errno = errno; }

    /// Helper clone method.
    virtual const CException* x_Clone(void) const
    {
        return new CErrnoTemplExceptionEx<TBase, PErrstr>(*this);
    }

private:
    int m_Errno;        ///< Error number
};



/////////////////////////////////////////////////////////////////////////////
///
/// CErrnoTemplException --
///
/// Define template class for easy generation of Errno-like exception classes.

template<class TBase> class CErrnoTemplException :
                        public CErrnoTemplExceptionEx<TBase, NCBI_STRERROR_WRAPPER>
{
public:
    /// Parent class type.
    typedef CErrnoTemplExceptionEx<TBase, NCBI_STRERROR_WRAPPER> CParent;

    /// Constructor.
    CErrnoTemplException<TBase>(const CDiagCompileInfo& info,
        const CException* prev_exception,
        typename CParent::EErrCode err_code,const string& message)
        : CParent(info, prev_exception,
                 (typename CParent::EErrCode) CException::eInvalid, message)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL(CErrnoTemplException<TBase>,
                                                CParent)
};


/// Throw exception with extra parameter.
///
/// Required to throw exceptions with one additional parameter
/// (e.g. positional information for CParseException).
#define NCBI_THROW2(exception_class, err_code, message, extra) \
    throw exception_class(DIAG_COMPILE_INFO, \
        0,exception_class::err_code, (message), (extra))

/// Re-throw exception with extra parameter.
///
/// Required to re-throw exceptions with one additional parameter
/// (e.g. positional information for CParseException).
#define NCBI_RETHROW2(prev_exception,exception_class,err_code,message,extra) \
    throw exception_class(DIAG_COMPILE_INFO, \
        &(prev_exception), exception_class::err_code, (message), (extra))


/// Define exception default with one additional parameter.
///
/// Required to define exception default with one additional parameter
/// (e.g. derived from CParseException).
#define NCBI_EXCEPTION_DEFAULT2(exception_class, base_class, extra_type) \
public: \
    exception_class(const CDiagCompileInfo &info, \
        const CException* prev_exception, \
        EErrCode err_code,const string& message, \
        extra_type extra_param) \
        : base_class(info, prev_exception, \
            (base_class::EErrCode) CException::eInvalid, \
            (message), extra_param) \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class)

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.58  2005/03/30 14:40:32  ssikorsk
 * Made destructor and getters explicitly public for default exception implementation
 *
 * Revision 1.57  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.56  2004/08/17 14:34:38  dicuccio
 * Added export specifiers
 *
 * Revision 1.55  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.54  2004/05/11 15:55:47  gouriano
 * Change GetErrCode method prototype to return TErrCode - to be able to
 * safely cast EErrCode to an eInvalid
 *
 * Revision 1.53  2004/04/30 11:26:06  kuznets
 * THROW spec macro disabled for MSVC (fixed some compiler warnings)
 *
 * Revision 1.52  2004/04/26 19:17:06  ucko
 * Some compilers specifically preferred wrapping strerror with a class,
 * so conditionalize use of a simple function on NCBI_COMPILER_GCC.
 *
 * Revision 1.51  2004/04/26 14:43:56  ucko
 * Handle GCC 3.4's stricter treatment of templates:
 * - Wrap strerror with an ordinary function rather than a static method.
 * - Qualify dependent names with "this->" as needed.
 * - Move CParseTemplException to ncbistr.hpp.
 *
 * Revision 1.50  2004/03/10 19:57:40  gorelenk
 * Added NCBI_XNCBI_EXPORT prefix to function DoThrowTraceAbort.
 *
 * Revision 1.49  2003/12/22 20:20:12  ivanov
 * Made all CException protected methods virtual
 *
 * Revision 1.48  2003/10/24 13:24:54  vasilche
 * Moved body of virtual method to *.cpp file.
 *
 * Revision 1.47  2003/08/01 15:17:21  siyan
 * Documentation changes. Removed superfluous (extra)
 * "public" keyword in CErrnoTemplException.
 *
 * Revision 1.46  2003/05/27 15:19:55  kuznets
 * Included <string.h> (declaration of 'strerror')
 *
 * Revision 1.45  2003/04/29 19:47:02  ucko
 * KLUDGE: avoid inlining CStrErrAdapt::strerror with GCC 2.95 on OSF/1,
 * due to a weird, apparently platform-specific, bug.
 *
 * Revision 1.44  2003/04/25 20:53:53  lavr
 * Add eInvalidArg type of CCoreException
 *
 * Revision 1.43  2003/04/24 16:25:32  kuznets
 * Farther templatefication of CErrnoTemplException,
 * added CErrnoTemplExceptionEx.
 * This will allow easy creation of Errno-like exception classes.
 * Added NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL macro
 * (fixes some warning with templates compilation (gcc 3.2.2)).
 *
 * Revision 1.42  2003/03/31 16:40:21  siyan
 * Added doxygen support
 *
 * Revision 1.41  2003/02/24 19:54:52  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.40  2003/02/14 19:31:23  gouriano
 * added definition of templates for errno and parse exceptions
 *
 * Revision 1.39  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.38  2002/08/20 19:13:47  gouriano
 * added DiagPostFlags into CException reporting functions
 *
 * Revision 1.37  2002/08/06 14:08:30  gouriano
 * introduced EXCEPTION_VIRTUAL_BASE macro to make doc++ happy
 *
 * Revision 1.36  2002/07/31 18:32:04  gouriano
 * fix for virtual base classes
 *
 * Revision 1.35  2002/07/29 19:30:43  gouriano
 * changes to allow multiple inheritance in CException classes
 *
 * Revision 1.33  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.32  2002/07/11 19:33:11  gouriano
 * minor fix in NCBI_EXCEPTION_DEFAULT definition
 *
 * Revision 1.31  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.30  2002/06/27 18:55:32  gouriano
 * added "title" parameter to report functions
 *
 * Revision 1.29  2002/06/27 18:26:23  vakatov
 * Explicitly qualify "exception" with "std::" to avoid a silly name conflict
 * with <math.h> for SUN Forte6u2 compiler
 *
 * Revision 1.28  2002/06/26 18:36:36  gouriano
 * added CNcbiException class
 *
 * Revision 1.27  2002/04/11 20:39:17  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.26  2001/12/03 22:24:14  vakatov
 * Rollback R1.25
 *
 * Revision 1.25  2001/12/03 22:03:37  juran
 * #include <corelib/ncbistl.hpp>
 *
 * Revision 1.24  2001/07/30 14:40:57  lavr
 * eDiag_Trace and eDiag_Fatal always print as much as possible
 *
 * Revision 1.23  2001/05/21 21:44:43  vakatov
 * SIZE_TYPE --> string::size_type
 *
 * Revision 1.22  2001/05/17 14:53:41  lavr
 * Typos corrected
 *
 * Revision 1.21  2000/04/04 22:30:25  vakatov
 * SetThrowTraceAbort() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.20  1999/12/29 13:58:37  vasilche
 * Added THROWS_NONE.
 *
 * Revision 1.19  1999/12/28 21:04:14  vasilche
 * Removed three more implicit virtual destructors.
 *
 * Revision 1.18  1999/12/28 18:55:25  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.17  1999/11/18 20:12:40  vakatov
 * DoDbgPrint() -- prototyped in both _DEBUG and NDEBUG
 *
 * Revision 1.16  1999/10/04 16:20:56  vasilche
 * Added full set of macros THROW*_TRACE
 *
 * Revision 1.15  1999/09/27 16:23:20  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.14  1999/09/23 21:15:48  vasilche
 * Added namespace modifiers.
 *
 * Revision 1.13  1999/06/11 16:33:10  vasilche
 * Fixed THROWx_TRACE
 *
 * Revision 1.12  1999/06/11 16:20:22  vasilche
 * THROW_TRACE fixed for not very smart compiler like Sunpro
 *
 * Revision 1.11  1999/06/11 02:48:03  vakatov
 * [_DEBUG] Refined the output from THROW*_TRACE macro
 *
 * Revision 1.10  1999/05/04 00:03:07  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.9  1999/01/07 16:15:09  vakatov
 * Explicitly specify "NCBI_NS_NCBI::" in the preprocessor macros
 *
 * Revision 1.8  1999/01/04 22:41:41  vakatov
 * Do not use so-called "hardware-exceptions" as these are not supported
 * (on the signal level) by UNIX
 * Do not "set_unexpected()" as it works differently on UNIX and MSVC++
 *
 * Revision 1.7  1998/12/28 17:56:27  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.6  1998/12/01 01:19:47  vakatov
 * Moved <string> and <stdexcept> after the <ncbidiag.hpp> to avoid weird
 * conflicts under MSVC++ 6.0
 *
 * Revision 1.5  1998/11/24 17:51:26  vakatov
 * + CParseException
 * Removed "Ncbi" sub-prefix from the NCBI exception class names
 *
 * Revision 1.4  1998/11/10 01:17:36  vakatov
 * Cleaned, adopted to the standard NCBI C++ framework and incorporated
 * the "hardware exceptions" code and tests(originally written by
 * V.Sandomirskiy).
 * Only tested for MSVC++ compiler yet -- to be continued for SunPro...
 *
 * Revision 1.3  1998/11/06 22:42:38  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * ==========================================================================
 */

#endif  /* NCBIEXPT__HPP */
