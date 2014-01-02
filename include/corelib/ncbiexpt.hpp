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
#include <corelib/ncbi_stack.hpp>
#include <errno.h>
#include <string.h>
#include <typeinfo>

#ifdef NCBI_OS_MSWIN
#  include <corelib/ncbi_os_mswin.hpp>
#endif

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
const T& DbgPrintP(const CDiagCompileInfo& info, const T& e,const char* e_str)
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
/// This macro is deprecated - use *_X or *_XX variant instead of it.
#define STD_CATCH(message)                                    \
    catch (NCBI_NS_STD::exception& e) {                       \
        NCBI_NS_NCBI::CNcbiDiag()                             \
            << NCBI_NS_NCBI::Error                            \
            << "[" << message << "] Exception: " << e.what(); \
    }

/// Standard handling of "exception"-derived exceptions; catches non-standard
/// exceptions and generates "unknown exception" for all other exceptions.
/// This macro is deprecated - use *_X or *_XX variant instead of it.
#define STD_CATCH_ALL(message)                                \
    STD_CATCH(message)                                        \
    catch (...) {                                             \
        NCBI_NS_NCBI::CNcbiDiag()                             \
           << NCBI_NS_NCBI::Error                             \
           << "[" << message << "] Unknown exception";        \
    }

/// Catch CExceptions as well
/// This macro is deprecated - use *_X or *_XX variant instead of it.
#define NCBI_CATCH(message)                                   \
    catch (NCBI_NS_NCBI::CException& e) {                     \
        NCBI_REPORT_EXCEPTION(message, e);                    \
    }                                                         \
    STD_CATCH(message)

/// This macro is deprecated - use *_X or *_XX variant instead of it.
#define NCBI_CATCH_ALL(message)                               \
    catch (NCBI_NS_NCBI::CException& e) {                     \
        NCBI_REPORT_EXCEPTION(message, e);                    \
    }                                                         \
    STD_CATCH_ALL(message)


/// Standard handling of "exception"-derived exceptions
/// with default error code and given error subcode placed in diagnostics.
/// Default error code is used and error subcode checking for correctness
/// is made in same way as in ERR_POST_X macro.
///
/// @sa NCBI_DEFINE_ERRCODE_X, ERR_POST_X
#define STD_CATCH_X(err_subcode, message)                     \
    STD_CATCH_XX(NCBI_USE_ERRCODE_X, err_subcode, message)

/// Standard handling of "exception"-derived exceptions; catches non-standard
/// exceptions and generates "unknown exception" for all other exceptions.
/// With default error code and given error subcode placed in diagnostics
///
/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_X
#define STD_CATCH_ALL_X(err_subcode, message)                 \
    STD_CATCH_ALL_XX(NCBI_USE_ERRCODE_X, err_subcode, message)

/// Catch CExceptions as well
/// with default error code and given error subcode placed in diagnostics
///
/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_X
#define NCBI_CATCH_X(err_subcode, message)                    \
    NCBI_CATCH_XX(NCBI_USE_ERRCODE_X, err_subcode, message)

/// @sa STD_CATCH_ALL_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_X
#define NCBI_CATCH_ALL_X(err_subcode, message)                \
    NCBI_CATCH_ALL_XX(NCBI_USE_ERRCODE_X, err_subcode, message)


/// Standard handling of "exception"-derived exceptions
/// with given error code name and given error subcode placed in diagnostics
///
/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_XX
#define STD_CATCH_XX(err_name, err_subcode, message)                 \
    catch (NCBI_NS_STD::exception& e) {                              \
        NCBI_CHECK_ERR_SUBCODE_X_NAME(err_name, err_subcode);        \
        NCBI_NS_NCBI::CNcbiDiag()                                    \
            << ErrCode(NCBI_ERRCODE_X_NAME(err_name), err_subcode)   \
            << NCBI_NS_NCBI::Error                                   \
            << "[" << message << "] Exception: " << e.what();        \
    }

/// Standard handling of "exception"-derived exceptions; catches non-standard
/// exceptions and generates "unknown exception" for all other exceptions.
/// With given error code name and given error subcode placed in diagnostics
///
/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_XX
#define STD_CATCH_ALL_XX(err_name, err_subcode, message)             \
    STD_CATCH_XX(err_name, err_subcode, message)                     \
    catch (...) {                                                    \
        NCBI_NS_NCBI::CNcbiDiag()                                    \
           << ErrCode(NCBI_ERRCODE_X_NAME(err_name), err_subcode)    \
           << NCBI_NS_NCBI::Error                                    \
           << "[" << message << "] Unknown exception";               \
    }

/// Catch CExceptions as well
/// with given error code name and given error subcode placed in diagnostics
///
/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_XX
#define NCBI_CATCH_XX(err_name, err_subcode, message)                 \
    catch (NCBI_NS_NCBI::CException& e) {                             \
        NCBI_REPORT_EXCEPTION_XX(err_name, err_subcode, message, e);  \
    }                                                                 \
    STD_CATCH_XX(err_name, err_subcode, message)

/// @sa STD_CATCH_X, NCBI_DEFINE_ERRCODE_X, ERR_POST_XX
#define NCBI_CATCH_ALL_XX(err_name, err_subcode, message)             \
    catch (NCBI_NS_NCBI::CException& e) {                             \
        NCBI_REPORT_EXCEPTION_XX(err_name, err_subcode, message, e);  \
    }                                                                 \
    STD_CATCH_ALL_XX(err_name, err_subcode, message)



/////////////////////////////////////////////////////////////////////////////
// CException: useful macros

/// Format message using iostreams library.
/// This macro returns an object convertible to std::string.
#define FORMAT(message) \
    CNcbiOstrstreamToString(static_cast<CNcbiOstrstream&>(CNcbiOstrstream().flush() << message))


/// Create an exception instance to be thrown later, given the exception
/// class, previous exception pointer, error code and message string.
#define NCBI_EXCEPTION_VAR_EX(name, prev_exception_ptr,              \
                              exception_class, err_code, message)    \
    exception_class name(DIAG_COMPILE_INFO,                          \
        prev_exception_ptr, exception_class::err_code, (message))

/// Create an instance of the exception to be thrown later.
#define NCBI_EXCEPTION_VAR(name, exception_class, err_code, message) \
    NCBI_EXCEPTION_VAR_EX(name, 0, exception_class, err_code, message)

/// Throw an existing exception object
#define NCBI_EXCEPTION_THROW(exception_var) \
    throw (exception_var)

#define NCBI_EXCEPTION_EMPTY_NAME

// NCBI_THROW(foo).SetModule("aaa");
/// Generic macro to make an exception, given the exception class,
/// error code and message string.
#define NCBI_EXCEPTION(exception_class, err_code, message)           \
    NCBI_EXCEPTION_VAR(NCBI_EXCEPTION_EMPTY_NAME,                    \
                       exception_class, err_code, message)

/// Generic macro to throw an exception, given the exception class,
/// error code and message string.
/// The err_code argument may include manipulators to set additional
/// options, e.g. eMyErrCode | Console | Severity(eDiag_Info). In any
/// case the error code value must be the first in the combination.
#define NCBI_THROW(exception_class, err_code, message)                  \
    do {                                                                \
        USING_SCOPE(NCBI_NS_NCBI::ncbi_ex_manip);                       \
        NCBI_EXCEPTION_THROW(NCBI_EXCEPTION(exception_class, err_code,  \
                                            message));                  \
    } while (0)

/// Throw a quick-and-dirty runtime exception of type 'CException' with
/// the given error message and error code 'eUnknown'.
/// This macro is intended for use only in stand-alone applications.
/// Library APIs should properly declare their specific exception types.
#define NCBI_USER_THROW(message) \
    NCBI_THROW(NCBI_NS_NCBI::CException, eUnknown, message)

/// The same as NCBI_THROW but with message processed as output to ostream.
#define NCBI_THROW_FMT(exception_class, err_code, message)  \
    NCBI_THROW(exception_class, err_code, FORMAT(message))

/// Throw a "user exception" with message processed as output to ostream.
/// See NCBI_USER_THROW for details.
#define NCBI_USER_THROW_FMT(message)  \
    NCBI_THROW_FMT(NCBI_NS_NCBI::CException, eUnknown, message)

/// Generic macro to make an exception, given the exception class,
/// previous exception, error code and message string.
#define NCBI_EXCEPTION_EX(prev_exception, exception_class, err_code, message) \
    NCBI_EXCEPTION_VAR_EX(NCBI_EXCEPTION_EMPTY_NAME, &(prev_exception), \
                          exception_class, err_code, message)

/// Generic macro to re-throw an exception.
/// The err_code argument may include manipulators to set additional
/// options, e.g. eMyErrCode | Console | Severity(eDiag_Info). In any
/// case the error code value must be the first in the combination.
#define NCBI_RETHROW(prev_exception, exception_class, err_code, message) \
    do {                                                                \
        USING_SCOPE(NCBI_NS_NCBI::ncbi_ex_manip);                       \
        throw NCBI_EXCEPTION_EX(prev_exception, exception_class,        \
                                err_code, message);                     \
    } while (0)

/// The same as NCBI_RETHROW but with message processed as output to ostream.
#define NCBI_RETHROW_FMT(prev_exception, exception_class, err_code, message) \
    NCBI_RETHROW(prev_exception, exception_class, err_code, FORMAT(message))

/// Generic macro to re-throw the same exception.
#define NCBI_RETHROW_SAME(prev_exception, message)                \
    do { prev_exception.AddBacklog(DIAG_COMPILE_INFO, message,    \
                                   prev_exception.GetSeverity()); \
    throw; }  while (0)

/// Generate a report on the exception.
#define NCBI_REPORT_EXCEPTION(title, ex) \
    NCBI_NS_NCBI::CExceptionReporter::ReportDefault \
        (DIAG_COMPILE_INFO, title, ex, NCBI_NS_NCBI::eDPF_Default)

/// Generate a report on the exception with default error code and
/// given subcode.
#define NCBI_REPORT_EXCEPTION_X(err_subcode, title, ex)                  \
    NCBI_REPORT_EXCEPTION_XX(NCBI_USE_ERRCODE_X, err_subcode, title, ex)

/// Generate a report on the exception with default error code and
/// given subcode.
#define NCBI_REPORT_EXCEPTION_XX(err_name, err_subcode, title, ex)   \
    NCBI_CHECK_ERR_SUBCODE_X_NAME(err_name, err_subcode);            \
    NCBI_NS_NCBI::CExceptionReporter::ReportDefaultEx                \
    (NCBI_ERRCODE_X_NAME(err_name), err_subcode,                     \
     DIAG_COMPILE_INFO, title, ex, NCBI_NS_NCBI::eDPF_Default)



/////////////////////////////////////////////////////////////////////////////
// CException

// Forward declaration of CExceptionReporter.
class CExceptionReporter;


// Base helper class for passing options to exceptions.
class CExceptionArgs_Base
{
public:
    typedef int      TErrCodeVal;
    typedef int      TFlags;
    typedef EDiagSev TSeverity;

    CExceptionArgs_Base(void) : m_ErrCode(0), m_Flags(0), m_Severity(eDiag_Error) {}

    void SetErrCodeVal(TErrCodeVal err_code) { m_ErrCode = err_code; }
    TErrCodeVal GetErrCodeVal(void) const { return m_ErrCode; }

    void SetFlags(TFlags flags) { m_Flags = flags; }
    TFlags GetFlags(void) const { return m_Flags; }

    void SetSeverity(TSeverity severity) { m_Severity = severity; }
    TSeverity GetSeverity(void) const { return m_Severity; }

    void SetModule(const string& module) { m_Module = module; }
    const string& GetModule(void) const { return m_Module; }
    bool IsSetModule(void) const { return !m_Module.empty(); }

private:
    TErrCodeVal m_ErrCode;
    TFlags      m_Flags;
    TSeverity   m_Severity;
    string      m_Module;
};


class CExceptionArgsManip
{
public:
    virtual ~CExceptionArgsManip(void) {}

    virtual void operator()(CExceptionArgs_Base& args) const = 0;
};

typedef void (*FExceptionArgsManip)(CExceptionArgs_Base&);

/// Manipulator function wrapper.
class CExceptionArgsManip_Wrapper : public CExceptionArgsManip
{
public:
    CExceptionArgsManip_Wrapper(FExceptionArgsManip f) : m_Func(f) { }

    void operator()(CExceptionArgs_Base& args) const { (*m_Func)(args); }

private:
    FExceptionArgsManip m_Func;
};


template<class TErrCode>
class CExceptionArgs : public CExceptionArgs_Base
{
public:
    CExceptionArgs(TErrCode err_code) { SetErrCodeVal(err_code); }

    TErrCode GetErrCode(void) const { return TErrCode(GetErrCodeVal()); }

    CExceptionArgs<TErrCode>& operator|(const CExceptionArgsManip& manip)
    {
        manip(*this);
        return *this;
    }

    CExceptionArgs<TErrCode>& operator|(const CExceptionArgsManip_Wrapper& manip)
    {
        manip(*this);
        return *this;
    }
};


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

    /// Miscellaneous generic hints, flags and attributes
    enum EFlags {
        /// Mark the exception with this flag if the exception is supposed
        /// to be extra-visible, such as to go to a console or a dialog
        /// that's looked at by the end user. Naturally, the message should be
        /// clear and informative enough to be actually helpful for the user.
        /// If such "console" exception is passed to the diagnostic (e.g.
        /// "ERR_POST(ex);", then the posting will be automatically marked as
        /// "console" too (so it'll work as "ERR_POST(Console << ex);").
        fConsole = (1 << 0)
    };
    typedef int TFlags;  //< Bit-wise OR of "EFlags"

    /// Constructor.
    ///
    /// When throwing an exception initially, "prev_exception" must be 0.
    CException(const CDiagCompileInfo& info,
               const CException* prev_exception,
               EErrCode err_code,
               const string& message,
               EDiagSev severity = eDiag_Error,
               TFlags flags = 0);

    CException(const CDiagCompileInfo& info,
               const CException* prev_exception,
               const CExceptionArgs<EErrCode>& args,
               const string& message);

    /// Copy constructor.
    CException(const CException& other);

    /// Add a message to backlog (to re-throw the same exception then).
    void AddBacklog(const CDiagCompileInfo& info,
                    const string& message,
                    EDiagSev severity = eDiag_Error);

    void AddPrevious(const CException* prev_exception);
    void AddToMessage(const string& add_msg);

    /// Polymorphically (re)throw an exception whose exact type is
    /// uncertain.
    ///
    /// NB: for best results, *EVERY* concrete derived class in the
    /// hierarchy must implement its *OWN* version of Throw().  (Using
    /// NCBI_EXCEPTION_DEFAULT or a related macro will take care of
    /// this for you.)
    ///
    /// Simply invoking the throw keyword with no arguments is a
    /// better option when available (within a catch block), but there
    /// are circumstances in which it is not.
    NCBI_NORETURN virtual void Throw(void) const;

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

    /// Get the saved stack trace if available or NULL.
    const CStackTrace* GetStackTrace(void) const;

    /// Enable background reporting.
    ///
    /// If background reporting is enabled, then calling what() or ReportAll()
    /// would also report exception to the default exception reporter.
    /// @return
    ///   The previous state of the flag.
    static bool EnableBackgroundReporting(bool enable);

    /// Set severity level for saving and printing stack trace
    static void SetStackTraceLevel(EDiagSev level);

    /// Get current severity level for saving and printing stack trace
    static EDiagSev GetStackTraceLevel(void);

    // ---- Attributes ---------

    /// Get exception severity.
    EDiagSev GetSeverity(void) const { return m_Severity; }

    /// Set exception severity.
    CException& SetSeverity(EDiagSev severity);

    /// Get class name as a string.
    virtual const char* GetType(void) const;

    /// Get error code interpreted as text.
    virtual const char* GetErrCodeString(void) const;

    /// Get file name used for reporting.
    const string& GetFile(void) const { return m_File; }

    /// Set module name used for reporting.
    void SetModule(const string& module) { m_Module = module; }

    /// Get module name used for reporting.
    const string& GetModule(void) const { return m_Module; }

    /// Set class name used for reporting.
    void SetClass(const string& nclass) { m_Class = nclass; }

    /// Get class name used for reporting.
    const string& GetClass(void) const { return m_Class; }

    /// Set function name used for reporting.
    void SetFunction(const string& function) { m_Function = function; }

    /// Get function name used for reporting.
    const string& GetFunction(void) const { return m_Function; }

    /// Get line number where error occurred.
    int GetLine(void) const { return m_Line; }

    /// Get error code.
    TErrCode GetErrCode(void) const;

    /// Get message string.
    const string& GetMsg(void) const;

    /// Get "previous" exception from the backlog.
    const CException* GetPredecessor(void) const { return m_Predecessor; }

    /// Check if exception has main text in the chain
    bool HasMainText(void) const { return m_MainText; }

    /// Destructor.
    virtual ~CException(void) throw();

    /// Check if the flag is set
    bool IsSetFlag(EFlags flag) const { return (m_Flags & flag) != 0; }

    /// Set flag (add to other flags)
    CException& SetFlag(EFlags flag) { m_Flags |= flag; return *this; }

    /// Unset flag (other flags are left as is)
    CException& UnsetFlag(EFlags flag) { m_Flags &= ~flag; return *this; }

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
                        const string&           message,
                        const CException*       prev_exception,
                        EDiagSev                severity);

    /// Process additional arguments. Derived classes may need
    /// to cast args to CExceptionArgs<CDerivedException::EErrCode>.
    virtual void x_InitArgs(const CExceptionArgs_Base& args);

    /// Helper method for copying exception data.
    virtual void x_Assign(const CException& src);

    /// Helper method for assigning error code.
    virtual void x_AssignErrCode(const CException& src);

    /// Helper method for initializing error code.
    virtual void x_InitErrCode(CException::EErrCode err_code);

    /// Helper method for getting error code.
    virtual int  x_GetErrCode(void) const { return m_ErrCode; }

    /// Get and store current stack trace.
    void x_GetStackTrace(void);

    /// Warn if Throw() will end up slicing its invocant.
    void x_ThrowSanityCheck(const type_info& expected_type,
                            const char* human_name) const;

private:
    EDiagSev    m_Severity;          ///< Severity level for the exception
    string      m_File;              ///< File     to report on
    int         m_Line;              ///< Line number
    int         m_ErrCode;           ///< Error code
    string      m_Msg;               ///< Message string
    string      m_Module;            ///< Module   to report on
    string      m_Class;             ///< Class    to report on
    string      m_Function;          ///< Function to report on

    mutable string m_What;           ///< What type of exception
    typedef const CException* TExceptionPtr;
    mutable TExceptionPtr m_Predecessor; ///< Previous exception

    mutable bool m_InReporter;       ///< Reporter flag
    mutable bool m_MainText;         ///< Exception has main text
    static  bool sm_BkgrEnabled;     ///< Background reporting enabled flag

    auto_ptr<CStackTrace> m_StackTrace; ///< Saved stack trace

    TFlags       m_Flags;            ///< Flags, hints, attributes

    /// Private assignment operator to prohibit assignment.
    CException& operator= (const CException&);
};


/// Exception manipulators. Can be combined with error code using operator|().

BEGIN_SCOPE(ncbi_ex_manip)

template<class TErrCode>
CExceptionArgs<TErrCode> operator|(TErrCode                   err_code,
                                   const CExceptionArgsManip& manip)
{
    CExceptionArgs<TErrCode> args(err_code);
    manip(args);
    return args;
}


template<class TErrCode>
CExceptionArgs<TErrCode> operator|(TErrCode                           err_code,
                                   const CExceptionArgsManip_Wrapper& manip)
{
    CExceptionArgs<TErrCode> args(err_code);
    manip(args);
    return args;
}


inline
void Console(CExceptionArgs_Base& args)
{
    args.SetFlags(args.GetFlags() | CException::fConsole);
}

inline
void Trace(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Trace);
}

inline
void Info(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Info);
}

inline
void Warning(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Warning);
}

inline
void Error(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Error);
}

inline
void Critical(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Critical);
}

inline
void Fatal(CExceptionArgs_Base& args)
{
    args.SetSeverity(eDiag_Fatal);
}


/// Module manipulator

class Module : public CExceptionArgsManip
{
public:
    Module(const string& module) : m_Module(module) {}
    Module(const char* module) : m_Module(module) {}
    virtual ~Module(void) {}

    virtual void operator()(CExceptionArgs_Base& args) const
    {
        args.SetModule(m_Module);
    }

private:
    string m_Module;
};


END_SCOPE(ncbi_ex_manip)


/// Return valid pointer to uppermost derived class only if "from" is _really_
/// the object of the desired type.
///
/// Do not cast to intermediate types (return NULL if such cast is attempted).
template <class TTo, class TFrom>
const TTo* UppermostCast(const TFrom& from)
{
    return typeid(from) == typeid(TTo) ? dynamic_cast<const TTo*>(&from) : 0;
}

#define NCBI_EXCEPTION_DEFAULT_THROW(exception_class) \
    NCBI_NORETURN virtual void Throw(void) const \
    { \
        this->x_ThrowSanityCheck(typeid(exception_class), #exception_class); \
        throw *this; \
    }

#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_COMMON(exception_class, base_class) \
    exception_class(const exception_class& other) \
       : base_class(other) \
    { \
        this->x_Assign(other); \
    } \
public: \
    virtual ~exception_class(void) throw() {} \
    virtual const char* GetType(void) const {return #exception_class;} \
    typedef int TErrCode; \
    TErrCode GetErrCode(void) const \
    { \
        return typeid(*this) == typeid(exception_class) ? \
            (TErrCode) this->x_GetErrCode() : \
            (TErrCode) CException::eInvalid; \
    } \
    NCBI_EXCEPTION_DEFAULT_THROW(exception_class) \
protected: \
    exception_class(void) {} \
    virtual const CException* x_Clone(void) const \
    { \
        return new exception_class(*this); \
    } \


/// Helper macro for default exception implementation.
/// @sa
///   NCBI_EXCEPTION_DEFAULT
#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class) \
    { \
        x_Init(info, message, prev_exception, severity); \
        x_InitErrCode((CException::EErrCode) err_code); \
    } \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_COMMON(exception_class, base_class) \
private: \
    /* for the sake of semicolon at the end of macro...*/ \
    static void xx_unused_##exception_class(void)


/// To help declare new exception class.
///
/// This can be used ONLY if the derived class does not have any additional
/// (non-standard) data members.
#define NCBI_EXCEPTION_DEFAULT(exception_class, base_class)             \
public:                                                                 \
    exception_class(const CDiagCompileInfo& info,                       \
        const CException* prev_exception, EErrCode err_code,            \
        const string& message, EDiagSev severity = eDiag_Error)         \
        : base_class(info, prev_exception,                              \
            (base_class::EErrCode) CException::eInvalid, (message))     \
    {                                                                   \
        x_Init(info, message, prev_exception, severity);                \
        x_InitErrCode((CException::EErrCode) err_code);                 \
    }                                                                   \
    exception_class(const CDiagCompileInfo& info,                       \
        const CException* prev_exception,                               \
        const CExceptionArgs<EErrCode>& args,                           \
        const string& message)                                          \
        : base_class(info, prev_exception,                              \
            (base_class::EErrCode) CException::eInvalid, (message))     \
    {                                                                   \
        x_Init(info, message, prev_exception, args.GetSeverity());      \
        x_InitArgs(args);                                               \
        x_InitErrCode((CException::EErrCode) args.GetErrCode());        \
    }                                                                   \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_COMMON(exception_class,       \
        base_class)                                                     \
private:                                                                \
    /* for the sake of semicolon at the end of macro...*/               \
    static void xx_unused_##exception_class(void)


/// Helper macro added to support templatized exceptions.
///
/// GCC starting from 3.2.2 warns about implicit typenames - this macro fixes
/// the warning.
#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL(exception_class, base_class) \
    { \
        this->x_Init(info, message, prev_exception, severity); \
        this->x_InitErrCode((typename CException::EErrCode) err_code); \
    } \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_COMMON(exception_class, base_class)


/// Helper macro added to support errno based templatized exceptions.
#define NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL_ERRNO(exception_class, base_class) \
    { \
        this->x_Init(info, message, prev_exception, severity); \
        this->x_InitErrCode((typename CException::EErrCode) err_code); \
    } \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_COMMON(exception_class, base_class) \
public: \
    virtual const char* GetErrCodeString(void) const \
    { \
        switch (GetErrCode()) { \
        case CParent::eErrno: return "eErrno"; \
        default:              return CException::GetErrCodeString(); \
        } \
    }


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
                              const string& title, const std::exception& ex,
                              TDiagPostFlags flags = eDPF_Trace);

    /// Report exception using default reporter and particular error code and
    /// subcode when writing to diagnostics.
    static void ReportDefaultEx(int err_code, int err_subcode,
                                const CDiagCompileInfo& info,
                                const string& title, const std::exception& ex,
                                TDiagPostFlags flags = eDPF_Trace);

    /// Report CException with _this_ reporter
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
// to let them satisfy TErrorStr without a wrapper.  However, they
// don't all agree on what form the wrapper should take. :-/
NCBI_XNCBI_EXPORT
extern const char*  Ncbi_strerror(int errnum);

#ifdef NCBI_COMPILER_GCC
inline int         NcbiErrnoCode(void)      { return errno; }
inline const char* NcbiErrnoStr(int errnum) { return ::strerror(errnum); }
#  define NCBI_ERRNO_CODE_WRAPPER NCBI_NS_NCBI::NcbiErrnoCode
#  define NCBI_ERRNO_STR_WRAPPER  NCBI_NS_NCBI::NcbiErrnoStr
#else
class CErrnoAdapt
{
public:
    static int GetErrCode(void)
        { return errno; }
    static const char* GetErrCodeString(int errnum) 
        {
            return Ncbi_strerror(errnum);
        }
};
#  define NCBI_ERRNO_CODE_WRAPPER NCBI_NS_NCBI::CErrnoAdapt::GetErrCode
#  define NCBI_ERRNO_STR_WRAPPER  NCBI_NS_NCBI::CErrnoAdapt::GetErrCodeString
#endif

// MS Windows API errors
#ifdef NCBI_OS_MSWIN
class NCBI_XNCBI_EXPORT CLastErrorAdapt
{
public:
    static int GetErrCode(void)
        { return GetLastError(); }
    static const char* GetErrCodeString(int errnum);
};
#  define NCBI_LASTERROR_CODE_WRAPPER \
    NCBI_NS_NCBI::CLastErrorAdapt::GetErrCode
#  define NCBI_LASTERROR_STR_WRAPPER  \
    NCBI_NS_NCBI::CLastErrorAdapt::GetErrCodeString
#endif


/////////////////////////////////////////////////////////////////////////////
// Auxiliary exception classes:
//   CErrnoException
//   CErrnoException_Win
//   CParseException
//

/// Define function type for "error code" function.
typedef int (*TErrorCode)(void);

/// Define function type for "error str" function.
typedef const char* (*TErrorStr)(int errnum);


/////////////////////////////////////////////////////////////////////////////
///
/// CErrnoTemplExceptionEx --
///
/// Define template class for easy generation of Errno-like exception classes.

template <class TBase, 
          TErrorCode PErrCode=NCBI_ERRNO_CODE_WRAPPER,
          TErrorStr  PErrStr=NCBI_ERRNO_STR_WRAPPER >
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
                           EErrCode err_code, const string& message, 
                           EDiagSev severity = eDiag_Error)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message)
     {
        m_Errno = PErrCode();
        this->x_Init(info, message, prev_exception, severity);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Constructor.
    CErrnoTemplExceptionEx(const CDiagCompileInfo& info,
                           const CException* prev_exception,
                           EErrCode err_code, const string& message,
                           int errnum, EDiagSev severity = eDiag_Error)
          : TBase(info, prev_exception,
                 (typename TBase::EErrCode)(CException::eInvalid),
                  message),
            m_Errno(errnum)
    {
        this->x_Init(info, message, prev_exception, severity);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Copy constructor.
    CErrnoTemplExceptionEx(
        const CErrnoTemplExceptionEx<TBase, PErrCode, PErrStr>& other)
        : TBase( other)
    {
        m_Errno = other.m_Errno;
        this->x_Assign(other);
    }

    /// Destructor.
    virtual ~CErrnoTemplExceptionEx(void) throw() {}

    /// Report error number on stream.
    virtual void ReportExtra(ostream& out) const
    {
        out << "errno = " << m_Errno <<  ": " << PErrStr(m_Errno);
    }

    // Attributes.

    /// Get type of class.
    virtual const char* GetType(void) const { return "CErrnoTemplException"; }

    typedef int TErrCode;
    /// Get error code.

    TErrCode GetErrCode(void) const
    {
        return typeid(*this) ==
            typeid(CErrnoTemplExceptionEx<TBase, PErrCode, PErrStr>) ?
               (TErrCode) this->x_GetErrCode() :
               (TErrCode) CException::eInvalid;
    }

    /// Get error number.
    int GetErrno(void) const throw() { return m_Errno; }

protected:
    /// Constructor.
    CErrnoTemplExceptionEx(void) { m_Errno = PErrCode(); }

    /// Helper clone method.
    virtual const CException* x_Clone(void) const
    {
        return new CErrnoTemplExceptionEx<TBase, PErrCode, PErrStr>(*this);
    }

private:
    int m_Errno;  ///< Error number
};



/////////////////////////////////////////////////////////////////////////////
///
/// CErrnoTemplException --
///
/// Define template class for easy generation of Errno-like exception classes.

template<class TBase> class CErrnoTemplException :
    public CErrnoTemplExceptionEx<TBase,
                                  NCBI_ERRNO_CODE_WRAPPER,
                                  NCBI_ERRNO_STR_WRAPPER>
{
public:
    /// Parent class type.
    typedef CErrnoTemplExceptionEx<TBase, NCBI_ERRNO_CODE_WRAPPER, NCBI_ERRNO_STR_WRAPPER> CParent;

    /// Constructor.
    CErrnoTemplException<TBase>(const CDiagCompileInfo&    info,
                                const CException*          prev_exception,
                                typename CParent::EErrCode err_code,
                                const string&              message,
                                EDiagSev                   severity = eDiag_Error)
        : CParent(info, prev_exception,
                 (typename CParent::EErrCode)CException::eInvalid, message)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL_ERRNO(CErrnoTemplException<TBase>, CParent)
};


#ifdef NCBI_OS_MSWIN
template<class TBase> class CErrnoTemplException_Win :
    public CErrnoTemplExceptionEx<TBase,
                                  NCBI_LASTERROR_CODE_WRAPPER,
                                  NCBI_LASTERROR_STR_WRAPPER>
{
public:
    /// Parent class type.
    typedef CErrnoTemplExceptionEx<TBase, NCBI_LASTERROR_CODE_WRAPPER, NCBI_LASTERROR_STR_WRAPPER> CParent;

    /// Constructor.
    CErrnoTemplException_Win<TBase>(const CDiagCompileInfo&    info,
                                    const CException*          prev_exception,
                                    typename CParent::EErrCode err_code,
                                    const string&              message,
                                    EDiagSev                   severity = eDiag_Error)
        : CParent(info, prev_exception,
                 (typename CParent::EErrCode)CException::eInvalid, message)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION_TEMPL_ERRNO(CErrnoTemplException_Win<TBase>, CParent)
};
#endif

/////////////////////////////////////////////////////////////////////////////


/// Create an instance of the exception with one additional parameter.
#define NCBI_EXCEPTION2_VAR(name, exception_class, err_code, message, extra) \
    exception_class name(DIAG_COMPILE_INFO, 0,                               \
    exception_class::err_code, (message), (extra) )

/// Generic macro to make an exception with one additional parameter,
/// given the exception class, error code and message string.
#define NCBI_EXCEPTION2(exception_class, err_code, message, extra)   \
    NCBI_EXCEPTION2_VAR(NCBI_EXCEPTION_EMPTY_NAME,                   \
    exception_class, err_code, message, extra)

/// Throw exception with extra parameter.
///
/// Required to throw exceptions with one additional parameter
/// (e.g. positional information for CParseException).
#define NCBI_THROW2(exception_class, err_code, message, extra) \
    throw NCBI_EXCEPTION2(exception_class, err_code, message, extra)

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
        extra_type extra_param, EDiagSev severity = eDiag_Error) \
        : base_class(info, prev_exception, \
            (base_class::EErrCode) CException::eInvalid, \
            (message), extra_param, severity) \
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(exception_class, base_class)

END_NCBI_SCOPE


/* @} */

#endif  /* NCBIEXPT__HPP */
