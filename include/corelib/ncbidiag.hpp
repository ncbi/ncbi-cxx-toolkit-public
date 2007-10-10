#ifndef CORELIB___NCBIDIAG__HPP
#define CORELIB___NCBIDIAG__HPP

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

/// @file ncbidiag.hpp
///
///   Defines NCBI C++ diagnostic APIs, classes, and macros.
///
///   More elaborate documentation could be found in:
///     http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/
///            programming_manual/diag.html


#include <corelib/ncbistre.hpp>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Convert some value to string
#define NCBI_AS_STRING(str)   #str

/// Set default module name based on NCBI_MODULE macro
#ifdef NCBI_MODULE
#  define NCBI_MODULE_AS_STRING(module) NCBI_AS_STRING(module)
#  define NCBI_MAKE_MODULE(module) NCBI_MODULE_AS_STRING(module)
#else
#  define NCBI_MAKE_MODULE(module) NULL
#endif 


/// Incapsulate compile time information such as
/// _FILE_ _LINE NCBI_MODULE
/// NCBI_MODULE is used only in .cpp file
/// @sa
///   DIAG_COMPILE_INFO

class CDiagCompileInfo
{
public:
    // DO NOT create CDiagCompileInfo directly
    // use macro DIAG_COMPILE_INFO instead!
    NCBI_XNCBI_EXPORT
    CDiagCompileInfo(void);
    NCBI_XNCBI_EXPORT
    CDiagCompileInfo(const char* file,
                     int line,
                     const char* curr_funct = NULL,
                     const char* module = NULL);
    NCBI_XNCBI_EXPORT
    CDiagCompileInfo(const string& file,
                     int           line,
                     const string& curr_funct,
                     const string& module);
    NCBI_XNCBI_EXPORT
    ~CDiagCompileInfo(void);

    const char*   GetFile    (void) const;
    const char*   GetModule  (void) const;
    int           GetLine    (void) const;
    const string& GetClass   (void) const;
    const string& GetFunction(void) const;

private:
    NCBI_XNCBI_EXPORT
    void ParseCurrFunctName(void) const;

    // Check if module needs to be set
    bool x_NeedModule(void) const;

private:
    const char*    m_File;
    const char*    m_Module;
    int            m_Line; 

    const   char*  m_CurrFunctName;
    mutable bool   m_Parsed;
    mutable string m_ClassName;
    mutable string m_FunctName;

    // Storage for data passed as strings rather than char*.
    char*          m_StrFile;
    char*          m_StrModule;
    char*          m_StrCurrFunctName;
};

NCBI_XNCBI_EXPORT const char* g_DiagUnknownFunction(void);

/// Get current function name.
/// Defined inside of either a method or a function body only.
// Based on boost's BOOST_CURRENT_FUNCTION

#ifndef NDEBUG
#  define NCBI_SHOW_FUNCTION_NAME
#endif


#ifdef NCBI_SHOW_FUNCTION_NAME
#  if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600))
#    define NCBI_CURRENT_FUNCTION __PRETTY_FUNCTION__
#  elif defined(__FUNCSIG__)
#    define NCBI_CURRENT_FUNCTION __FUNCSIG__
#  elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#    define NCBI_CURRENT_FUNCTION __FUNCTION__
#  elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#    define NCBI_CURRENT_FUNCTION __FUNC__
#  elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#    define NCBI_CURRENT_FUNCTION __func__
#  else
#    define NCBI_CURRENT_FUNCTION NCBI_NS_NCBI::g_DiagUnknownFunction()
#  endif
#else
#  define NCBI_CURRENT_FUNCTION NCBI_NS_NCBI::g_DiagUnknownFunction()
#endif


/// Make compile time diagnostic information object
/// to use in CDiagInfo and CException
/// @sa
///   CDiagCompileInfo

#define DIAG_COMPILE_INFO                               \
  NCBI_NS_NCBI::CDiagCompileInfo(__FILE__,              \
                                 __LINE__,              \
                                 NCBI_CURRENT_FUNCTION, \
                                 NCBI_MAKE_MODULE(NCBI_MODULE))

    


/// Error posting with file, line number information but without error codes.
/// This macro is deprecated and it's strongly recomended to move
/// in all projects (except tests) to macro ERR_POST_X to make possible more
/// flexible error statistics and logging.
///
/// @sa
///   ERR_POST_EX, ERR_POST_X
#define ERR_POST(message)                                 \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO).GetRef() \
      << message                                          \
      << NCBI_NS_NCBI::Endm )


/// Log message only without severity, location, prefix information.
/// This macro is deprecated and it's strongly recomended to move
/// in all projects (except tests) to macro LOG_POST_X to make possible more
/// flexible error statistics and logging.
///
/// @sa
///   LOG_POST_EX, LOG_POST_X
#define LOG_POST(message)                          \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO,   \
      eDiag_Error,                                 \
      eDPF_Log | eDPF_IsMessage).GetRef()          \
      << message                                   \
      << NCBI_NS_NCBI::Endm )

/// Error posting with error codes.
/// This macro should be used only when you need to make non-constant
/// error subcode. In all other cases it's strongly recomended to move
/// in all projects (except tests) to macro ERR_POST_X to make possible more
/// flexible error statistics and logging.
///
/// @sa
///   ERR_POST, ERR_POST_X
#define ERR_POST_EX(err_code, err_subcode, message)         \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO).GetRef()   \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << message                                            \
      << NCBI_NS_NCBI::Endm )

/// Log posting with error codes.
/// This macro should be used only when you need to make non-constant
/// error subcode. In all other cases it's strongly recomended to move
/// in all projects (except tests) to macro LOG_POST_X to make possible more
/// flexible error statistics and logging.
///
/// @sa
///   LOG_POST, LOG_POST_X
#define LOG_POST_EX(err_code, err_subcode, message)         \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error,                  \
      eDPF_Log | eDPF_IsMessage).GetRef()                   \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << message << NCBI_NS_NCBI::Endm )


/// Defines global error code name with given value (err_code) and given
/// maximum value of error subcode within this code. To use defined error
/// code you need to define symbol NCBI_USE_ERRCODE_X with name as its value.
/// This error code is used only in macros LOG_POST_X and ERR_POST_X. Maximum
/// value of error subcode is being checked only in Debug configuration and
/// exists for developers to know what code they can use in next inserted
/// ERR_POST_X call.
/// Macro MUST be used inside ncbi scope.
///
/// Example:
/// NCBI_DEFINE_ERRCODE_X(Corelib_Util, 110, 5);
/// ...
/// #define NCBI_USE_ERRCODE_X   Corelib_Util
/// ...
/// ERR_POST_X(3, "My error message with variables " << var);
///
///
/// @sa
///   LOG_POST_X, ERR_POST_X, NCBI_ERRCODE_X, NCBI_MAX_ERR_SUBCODE_X
#define NCBI_DEFINE_ERRCODE_X(name, err_code, max_err_subcode)  \
    namespace err_code_x {                                      \
        enum enum##name {                                       \
            eErrCodeX_##name     = err_code,                    \
            eErrCodeX_Max_##name = max_err_subcode              \
        };                                                      \
    }                                                           \
    extern void err_code_x__dummy_for_semicolon(void)

/// Makes one identifier from 2 parts
/// A small trick taken from BOOST library: when we do triple redirection
/// we can use as prefix or postfix macros themselves. They will be finally
/// expanded before concatenation (see 16.3.1 in C++ standard).
#define NCBI_CONCAT_IDENTIFIER(prefix, postfix)   \
    NCBI_CONCAT_IDENTIFIER1(prefix, postfix)
#define NCBI_CONCAT_IDENTIFIER1(prefix, postfix)   \
    NCBI_CONCAT_IDENTIFIER2(prefix, postfix)
#define NCBI_CONCAT_IDENTIFIER2(prefix, postfix) prefix##postfix

/// Returns value of error code by its name defined by NCBI_DEFINE_ERRCODE_X
///
/// @sa NCBI_DEFINE_ERRCODE_X
#define NCBI_ERRCODE_X_NAME(name)   \
    NCBI_NS_NCBI::err_code_x::NCBI_CONCAT_IDENTIFIER(eErrCodeX_, name)

/// Returns currently set default error code. Default error code is set by
/// definition of NCBI_USE_ERRCODE_X with name of error code as its value.
///
/// @sa NCBI_DEFINE_ERRCODE_X
#define NCBI_ERRCODE_X   NCBI_ERRCODE_X_NAME(NCBI_USE_ERRCODE_X)

/// Returns maximum value of error subcode within error code with given name.
///
/// @sa NCBI_DEFINE_ERRCODE_X
#define NCBI_MAX_ERR_SUBCODE_X_NAME(name)   \
    NCBI_NS_NCBI::err_code_x::NCBI_CONCAT_IDENTIFIER(eErrCodeX_Max_, name)

/// Returns maximum value of error subcode within current default error code.
///
/// @sa NCBI_DEFINE_ERRCODE_X
#define NCBI_MAX_ERR_SUBCODE_X   \
    NCBI_MAX_ERR_SUBCODE_X_NAME(NCBI_USE_ERRCODE_X)


/// Template structure that used to point out wrong error subcode in
/// ERR_POST_X and LOG_POST_X macros. When error subcode that is greater
/// than maximum defined in NCBI_DEFINE_ERRCODE_X will be used compiler
/// will give an error and in text of this error you'll see the name
/// of this structure. In parameters of template instantiation (that will be
/// also shown in error message) you'll see error code currently active,
/// error subcode used in *_POST_X macro and maximum error subcode valid for
/// active error code.
///
/// @sa NCBI_DEFINE_ERRCODE_X, LOG_POST_X, ERR_POST_X
template <int errorCode, int errorSubcode, int maxErrorSubcode, bool isWrong>
struct WRONG_ERROR_SUBCODE_IN_POST_MACRO;

/// Specialization of template: when error subcode is valid existance
/// of this specialization will be valuable for not issuing compiler error.
template <int errorCode, int errorSubcode, int maxErrorSubcode>
struct WRONG_ERROR_SUBCODE_IN_POST_MACRO
            <errorCode, errorSubcode, maxErrorSubcode, false> {
    enum {valid = 1};
};


#if defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION < 350

/// Issue compile-time error if error subcode given is not valid for given
/// error code name.
/// For early versions of gcc used a bit different design to make error
/// message more clear to understand.
///
/// @sa LOG_POST_X, ERR_POST_X
#define NCBI_CHECK_ERR_SUBCODE_X_NAME(name, subcode)                         \
    typedef char NCBI_CONCAT_IDENTIFIER(check_subcode_typedef_, __LINE__)    \
        [ NCBI_NS_NCBI::WRONG_ERROR_SUBCODE_IN_POST_MACRO<                   \
              NCBI_ERRCODE_X_NAME(name), subcode,                            \
              NCBI_MAX_ERR_SUBCODE_X_NAME(name),                             \
              ((unsigned int)subcode >                                       \
                    (unsigned int)NCBI_MAX_ERR_SUBCODE_X_NAME(name))         \
                                                         >::valid ]

#else  // if defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION < 350

/// Additional dummy function for use in NCBI_CHECK_ERR_SUBCODE_X macro
inline void CheckErrSubcodeX(int)
{}

/// Issue compile-time error if error subcode given is not valid for given
/// error code name.
/// This design is used for all compilers except early versions of gcc.
/// Though for MIPSpro and ICC it's not enough to make error message clear
/// (see addition below).
///
/// @sa LOG_POST_X, ERR_POST_X
#define NCBI_CHECK_ERR_SUBCODE_X_NAME(name, subcode)                  \
    NCBI_NS_NCBI::CheckErrSubcodeX(                                   \
        sizeof(NCBI_NS_NCBI::WRONG_ERROR_SUBCODE_IN_POST_MACRO<       \
              NCBI_ERRCODE_X_NAME(name), subcode,                     \
              NCBI_MAX_ERR_SUBCODE_X_NAME(name),                      \
              ((unsigned int)subcode >                                \
                    (unsigned int)NCBI_MAX_ERR_SUBCODE_X_NAME(name))  \
                                                              >)      \
                                  )

#endif  // if defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION < 350 else

/// Issue compile-time error if error subcode given is not valid for current
/// error code.
#define NCBI_CHECK_ERR_SUBCODE_X(subcode)   \
    NCBI_CHECK_ERR_SUBCODE_X_NAME(NCBI_USE_ERRCODE_X, subcode)

#if defined(NCBI_COMPILER_ICC) || defined(NCBI_COMPILER_MIPSPRO)

/// Additional not implemented template structure for use in
/// WRONG_ERROR_SUBCODE_IN_POST_MACRO structure specialization
template <int x>
struct WRONG_ERROR_SUBCODE_IN_POST_MACRO_2;

/// Specialization of template structure used for ICC and MIPSpro
/// If this specialization doesn't exist these compilers doesn't show name
/// of unimplemented structure in error message. But when this specialization
/// exists and uses recursively another not implemented template structure
/// then WRONG_ERROR_SUBCODE_IN_POST_MACRO appears in error message and it
/// becomes clearer.
template <int errorCode, int errorSubcode, int maxErrorSubcode>
struct WRONG_ERROR_SUBCODE_IN_POST_MACRO
            <errorCode, errorSubcode, maxErrorSubcode, true> {
    typedef char type[sizeof(WRONG_ERROR_SUBCODE_IN_POST_MACRO_2<errorCode>)];
};

#endif  // if defined(NCBI_COMPILER_ICC) || defined(NCBI_COMPILER_MIPSPRO)


/// Error posting with default error code and given error subcode. Also
/// checks subcode correctness. When error subcode is incorrect (greater than
/// defined in NCBI_DEFINE_ERRCODE_X) compile-time error is issued.
/// All calls to ERR_POST_X or LOG_POST_X under the same default error code
/// MUST be with deferent error subcodes to make possible more
/// flexible error statistics and logging.
/// If using of macro leads to compile errors containing in message strings
/// like "err_code_x" or "ErrCodeX" then you didn't defined error code name
/// with NCBI_DEFINE_ERRCODE_X macro or didn't selected current default
/// error code with valid NCBI_USE_ERRCODE_X definition.
/// This macro allows the use of only constant error subcodes
/// (integer literals or enum constants). If you need to make variable error
/// subcode you need to use macro ERR_POST_EX as follows:
///
/// NCBI_DEFINE_ERRCODE_X(Corelib_Util, 110, 5);
/// ...
/// #define NCBI_USE_ERRCODE_X   Corelib_Util
/// ...
/// NCBI_CHECK_ERR_SUBCODE_X(my_subcode);
/// ERR_POST_EX(NCBI_ERRCODE_X, my_subcode,
///             "My error message with variables " << var);
///
/// Or in more complicated way:
///
/// NCBI_DEFINE_ERRCODE_X(Corelib_Util, 110, 5);
/// ...
/// // no need to define NCBI_USE_ERRCODE_X
/// ...
/// NCBI_CHECK_ERR_SUBCODE_X_NAME(Corelib_Util, my_subcode);
/// ERR_POST_EX(NCBI_ERRCODE_X_NAME(Corelib_Util), my_subcode,
///             "My error message with variables " << var);
///
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X, ERR_POST_EX
#define ERR_POST_X(err_subcode, message)              \
    NCBI_CHECK_ERR_SUBCODE_X(err_subcode);            \
    ERR_POST_EX(NCBI_ERRCODE_X, err_subcode, message)

/// Log posting with default error code and given error subcode. See comments
/// to ERR_POST_X for clarifying the way of use and details of behaviour
/// of this macro.
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X, ERR_POST_X, LOG_POST_EX
#define LOG_POST_X(err_subcode, message)              \
    NCBI_CHECK_ERR_SUBCODE_X(err_subcode);            \
    LOG_POST_EX(NCBI_ERRCODE_X, err_subcode, message)


/// Common code for making log or error posting only given number of times
/// during program execution. This macro MUST not be used outside
/// this header.
#define NCBI_REPEAT_POST_N_TIMES(post_macro, count, params)     \
    do {                                                        \
        static volatile int sx_to_show = (count);               \
        int to_show = sx_to_show;                               \
        if ( to_show > 0 ) {                                    \
            sx_to_show = to_show - 1;                           \
            post_macro params;  /* parenthesis are in params */ \
        }                                                       \
    } while ( false )


/// Log posting only given number of times during program execution.
#define LOG_POST_N_TIMES(count, message)   \
    NCBI_REPEAT_POST_N_TIMES( LOG_POST, count, (message) )

/// Error posting only given number of times during program execution.
#define ERR_POST_N_TIMES(count, message)   \
    NCBI_REPEAT_POST_N_TIMES( ERR_POST, count, (message) )

/// Log posting only once during program execution.
#define LOG_POST_ONCE(message) LOG_POST_N_TIMES(1, message)

/// Error posting only once during program execution.
#define ERR_POST_ONCE(message) ERR_POST_N_TIMES(1, message)


/// Log posting only given number of times during program execution
/// with default error code and given error subcode.
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X
#define LOG_POST_X_N_TIMES(count, err_subcode, message)   \
    NCBI_REPEAT_POST_N_TIMES( LOG_POST_X, count, (err_subcode, message) )

/// Error posting only given number of times during program execution
/// with default error code and given error subcode.
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X
#define ERR_POST_X_N_TIMES(count, err_subcode, message)   \
    NCBI_REPEAT_POST_N_TIMES( ERR_POST_X, count, (err_subcode, message) )

/// Log posting only once during program execution with default
/// error code and given error subcode.
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X
#define LOG_POST_X_ONCE(err_subcode, message)   \
    LOG_POST_X_N_TIMES(1, err_subcode, message)

/// Error posting only once during program execution with default
/// error code and given error subcode.
///
/// @sa NCBI_DEFINE_ERRCODE_X, NCBI_ERRCODE_X
#define ERR_POST_X_ONCE(err_subcode, message)   \
    ERR_POST_X_N_TIMES(1, err_subcode, message)


/// Severity level for the posted diagnostics.
enum EDiagSev {
    eDiag_Info = 0, ///< Informational message
    eDiag_Warning,  ///< Warning message
    eDiag_Error,    ///< Error message
    eDiag_Critical, ///< Critical error message
    eDiag_Fatal,    ///< Fatal error -- guarantees exit(or abort)
    //
    eDiag_Trace,    ///< Trace message

    // Limits
    eDiagSevMin = eDiag_Info,  ///< Verbosity level for min. severity
    eDiagSevMax = eDiag_Trace  ///< Verbosity level for max. severity
};


/// Severity level change state.
enum EDiagSevChange {
    eDiagSC_Unknown, ///< Status of changing severity is unknown (first call)
    eDiagSC_Disable, ///< Disable change severity level 
    eDiagSC_Enable   ///< Enable change severity level 
};


/// Which parts of the diagnostic context should be posted.
///
/// Generic appearance of the posted message is as follows:
///
///   "<file>", line <line>: <severity>: (<err_code>.<err_subcode>)
///    [<prefix1>::<prefix2>::<prefixN>] <message>\n
///    <err_code_message>\n
///    <err_code_explanation>
///
/// Example: 
///
/// - If all flags are set, and prefix string is set to "My prefix", and
///   ERR_POST(eDiag_Warning, "Take care!"):
///   "/home/iam/myfile.cpp", line 33: Warning: (2.11)
///   Module::Class::Function() - [My prefix] Take care!
///
/// @sa
///   SDiagMessage::Compose()
enum EDiagPostFlag {
    eDPF_File               = 0x1, ///< Set by default #if _DEBUG; else not set
    eDPF_LongFilename       = 0x2, ///< Set by default #if _DEBUG; else not set
    eDPF_Line               = 0x4, ///< Set by default #if _DEBUG; else not set
    eDPF_Prefix             = 0x8, ///< Set by default (always)
    eDPF_Severity           = 0x10,  ///< Set by default (always)
    eDPF_ErrorID            = 0x20,  ///< Module, error code and subcode
    eDPF_DateTime           = 0x80,  ///< Include date and time
    eDPF_ErrCodeMessage     = 0x100, ///< Set by default (always)
    eDPF_ErrCodeExplanation = 0x200, ///< Set by default (always)
    eDPF_ErrCodeUseSeverity = 0x400, ///< Set by default (always)
    eDPF_Location           = 0x800, ///< Include class and function
                                     ///< if any, not set by default
    eDPF_PID                = 0x1000,  ///< Process ID
    eDPF_TID                = 0x2000,  ///< Thread ID
    eDPF_SerialNo           = 0x4000,  ///< Serial # of the post, process-wide
    eDPF_SerialNo_Thread    = 0x8000,  ///< Serial # of the post, in the thread
    eDPF_RequestId          = 0x10000, ///< fcgi iteration number or request ID
    eDPF_Iteration          = 0x10000, ///< @deprecated
    eDPF_UID                = 0x20000, ///< UID of the log

    eDPF_ErrCode            = eDPF_ErrorID,  ///< @deprecated
    eDPF_ErrSubCode         = eDPF_ErrorID,  ///< @deprecated
    /// All flags (except for the "unusual" ones!)
    eDPF_All                = 0xFFFFF,

    /// Default flags to use when tracing.
#if defined(NCBI_THREADS)
    eDPF_Trace              = 0xF81F,
#else
    eDPF_Trace              = 0x581F,
#endif

    /// Print the posted message only; without severity, location, prefix, etc.
    eDPF_Log                = 0x0,

    // "Unusual" flags -- not included in "eDPF_All"
    eDPF_PreMergeLines      = 0x100000, ///< Remove EOLs before calling handler
    eDPF_MergeLines         = 0x200000, ///< Ask diag.handlers to remove EOLs
    eDPF_OmitInfoSev        = 0x400000, ///< No sev. indication if eDiag_Info 
    eDPF_OmitSeparator      = 0x800000, ///< No '---' separator before message

    eDPF_AppLog             = 0x1000000, ///< Post message to application log
    eDPF_IsMessage          = 0x2000000, ///< Print "Message" severity name.

    /// Hint for the current handler to make message output as atomic as
    /// possible (e.g. for stream and file handlers).
    eDPF_AtomicWrite        = 0x4000000,

    /// Use global default flags (merge with).
    /// @sa SetDiagPostFlag(), UnsetDiagPostFlag(), IsSetDiagPostFlag()
    eDPF_Default            = 0x10000000
};

typedef int TDiagPostFlags;  ///< Binary OR of "EDiagPostFlag"


// Forward declaration of some classes.
class CDiagBuffer;
class CDiagErrCodeInfo;



/////////////////////////////////////////////////////////////////////////////
///
/// ErrCode --
///
/// Define composition of error code.
///
/// Currently the error code is an ordered pair of <code, subcode> numbers.

class ErrCode
{
public:
    /// Constructor.
    ErrCode(int code, int subcode = 0)
        : m_Code(code), m_SubCode(subcode)
    { }
    int m_Code;         ///< Major error code number
    int m_SubCode;      ///< Minor error code number
};


/////////////////////////////////////////////////////////////////////////////
///
/// Severity --
///
/// Set post severity to a given level.

class Severity
{
public:
    Severity(EDiagSev sev)
        : m_Level(sev) {}
    EDiagSev m_Level;         ///< Severity level
};


class CNcbiDiag;

/////////////////////////////////////////////////////////////////////////////
///
/// MDiagModule --
///
/// Manipulator to set Module for CNcbiDiag

class MDiagModule 
{
public:
    MDiagModule(const char* module);
    friend const CNcbiDiag& operator<< (const CNcbiDiag&   diag,
                                        const MDiagModule& module);
private:
    const char* m_Module;
};



/////////////////////////////////////////////////////////////////////////////
///
/// MDiagClass --
///
/// Manipulator to set Class for CNcbiDiag

class MDiagClass 
{
public:
    MDiagClass(const char* nclass);
    friend const CNcbiDiag& operator<< (const CNcbiDiag&  diag,
                                        const MDiagClass& nclass);
private:
    const char* m_Class;
};



/////////////////////////////////////////////////////////////////////////////
///
/// MDiagFunction --
///
/// Manipulator to set Function for CNcbiDiag

class MDiagFunction 
{
public:
    MDiagFunction(const char* function);
    friend const CNcbiDiag& operator<< (const CNcbiDiag&     diag,
                                        const MDiagFunction& function);
private:
    const char* m_Function;
};


//
class CException;
class CStackTrace;


/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiDiag --
///
/// Define the main NCBI Diagnostic class.


class CNcbiDiag
{
public:
    /// Constructor.
    NCBI_XNCBI_EXPORT  CNcbiDiag
    (EDiagSev       sev        = eDiag_Error,  ///< Severity level
     TDiagPostFlags post_flags = eDPF_Default  ///< What to post
     );


    /// Constructor -- includes the file and line number info
    NCBI_XNCBI_EXPORT  CNcbiDiag
    (const CDiagCompileInfo& info,                      ///< File, line, module
     EDiagSev                sev        = eDiag_Error,  ///< Severity level
     TDiagPostFlags          post_flags = eDPF_Default  ///< What to post
     );

    /// Destructor.
    NCBI_XNCBI_EXPORT ~CNcbiDiag(void);

    /// Some compilers (e.g. GCC 3.4.0) fail to use temporary objects as
    /// function arguments if there's no public copy constructor.
    /// Rather than using the temporary, get a reference from this method.
    const CNcbiDiag& GetRef(void) const { return *this; }

    /// Generic method to post to diagnostic stream.
    // Some compilers need to see the body right away, but others need
    // to meet CDiagBuffer first.
    template<class X> const CNcbiDiag& Put(const void*, const X& x) const
#ifdef NCBI_COMPILER_MSVC
    {
        m_Buffer.Put(*this, x);
        return *this;
    }
#else
      ;
#  define NCBIDIAG_DEFER_GENERIC_PUT
#endif

    /// Diagnostic stream manipulator
    /// @sa Reset(), Endm()
    /// @sa Info(), Warning(), Error(), Critical(), Fatal(), Trace()
    typedef const CNcbiDiag& (*FManip)(const CNcbiDiag&);
    typedef IOS_BASE& (*FIosManip)(IOS_BASE&);

    /// Helper method to post error code and subcode to diagnostic stream.
    ///
    /// Example:
    ///   CNcbiDiag() << ErrCode(5,3) << "My message";
    const CNcbiDiag& Put(const ErrCode*, const ErrCode& err_code) const;

    /// Helper method to set severity level.
    ///
    /// Example:
    ///   CNcbiDiag() << Severity(eDiag_Error) << "My message";
    const CNcbiDiag& Put(const Severity*, const Severity& severity) const;

    /// Helper method to post an exception to diagnostic stream.
    ///
    /// Example:
    ///   CNcbiDiag() << ex;
    template<class X> inline
    const CNcbiDiag& Put(const CException* e, const X& x) const {
        return x_Put(x);
    }

    /// Helper method to post stack trace to diagnostic stream using
    /// standard stack trace formatting.
    ///
    /// Example:
    ///   CNcbiDiag() << "My message" << CStackTrace();
    NCBI_XNCBI_EXPORT
        const CNcbiDiag& Put(const CStackTrace*,
                             const CStackTrace& stacktrace) const;

    /// Helper method to handle various diagnostic stream manipulators.
    ///
    /// For example, to set the message severity level to INFO:
    ///   CNcbiDiag() << Info << "My message";
    const CNcbiDiag& Put(const FManip, const FManip& manip) const
    {
        return manip(*this);
    }
    inline const CNcbiDiag& operator<< (FManip manip) const
    {
        return manip(*this);
    }
    const CNcbiDiag& operator<< (FIosManip manip) const;

    /// Post the arguments
    /// @sa Put()
    template<class X> inline const CNcbiDiag& operator<< (const X& x) const
    {
        return Put(&x, x);
    }

    // Output manipulators for CNcbiDiag.

    /// Reset the content of current message.
    friend const CNcbiDiag& Reset  (const CNcbiDiag& diag);

    /// Flush current message, start new one.
    friend const CNcbiDiag& Endm   (const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to INFO
    friend const CNcbiDiag& Info    (const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to WARNING
    friend const CNcbiDiag& Warning (const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to ERROR
    friend const CNcbiDiag& Error   (const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to CRITICAL ERROR
    friend const CNcbiDiag& Critical(const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to FATAL
    friend const CNcbiDiag& Fatal   (const CNcbiDiag& diag);

    /// Flush current message, then set a severity for the next diagnostic
    /// message to TRACE
    friend const CNcbiDiag& Trace   (const CNcbiDiag& diag);

    /// Set IsMessage flag to indicate that the current post is a message.
    /// Do not flush current post or change the severity. The flag is reset
    /// by the next Flush().
    friend const CNcbiDiag& Message (const CNcbiDiag& diag);

    /// Get a common symbolic name for the severity levels.
    static const char* SeverityName(EDiagSev sev);

    /// Get severity from string.
    ///
    /// @param str_sev
    ///   Can be the numeric value or a symbolic name (see
    ///   CDiagBuffer::sm_SeverityName[]).
    /// @param sev
    ///   Severity level. 
    /// @return
    ///   Return TRUE if severity level known; FALSE, otherwise.
    NCBI_XNCBI_EXPORT
    static bool StrToSeverityLevel(const char* str_sev, EDiagSev& sev);

    /// Set file name to post.
    NCBI_XNCBI_EXPORT
    const CNcbiDiag& SetFile(const char* file) const;

    /// Set module name.
    NCBI_XNCBI_EXPORT
    const CNcbiDiag& SetModule(const char* module) const;

    /// Set class name.
    NCBI_XNCBI_EXPORT
    const CNcbiDiag& SetClass(const char* nclass) const;

    /// Set function name.
    NCBI_XNCBI_EXPORT
    const CNcbiDiag& SetFunction(const char* function) const;

    /// Set line number for post.
    const CNcbiDiag& SetLine(size_t line) const;

    /// Set error code and subcode numbers.
    const CNcbiDiag& SetErrorCode(int code = 0, int subcode = 0) const;

    /// Get severity of the current message.
    EDiagSev GetSeverity(void) const;

    /// Get file used for the current message.
    const char* GetFile(void) const;

    /// Get line number for the current message.
    size_t GetLine(void) const;

    /// Get error code of the current message.
    int GetErrorCode(void) const;

    /// Get error subcode of the current message.
    int GetErrorSubCode(void) const;

    /// Get module name of the current message.
    const char* GetModule(void) const;

    /// Get class name of the current message.
    const char* GetClass(void) const;

    /// Get function name of the current message.
    const char* GetFunction(void) const;

    /// Check if filters are passed
    NCBI_XNCBI_EXPORT
    bool CheckFilters(void) const;

    /// Get post flags for the current message.
    /// If the post flags have "eDPF_Default" set, then in the returned flags
    /// it will be reset and substituted by current default flags.
    TDiagPostFlags GetPostFlags(void) const;

    /// Display fatal error message.
    NCBI_XNCBI_EXPORT
    static void DiagFatal(const CDiagCompileInfo& info,
                          const char* message);
    /// Display trouble error message.
    NCBI_XNCBI_EXPORT
    static void DiagTrouble(const CDiagCompileInfo& info,
                            const char* message = NULL);

    /// Assert specfied expression and report results.
    NCBI_XNCBI_EXPORT
    static void DiagAssert(const CDiagCompileInfo& info,
                           const char* expression,
                           const char* message = NULL);

    /// Display validation message.
    NCBI_XNCBI_EXPORT
    static void DiagValidate(const CDiagCompileInfo& info,
                             const char* expression,
                             const char* message);

    /// Reset IsMessage flag.
    void ResetIsMessageFlag(void) const { m_PostFlags &= ~eDPF_IsMessage; }

private:
    enum EValChngFlags {
        fFileIsChanged     = 0x1,
        fLineIsChanged     = 0x2,
        fModuleIsChanged   = 0x4,
        fClassIsChanged    = 0x8,
        fFunctionIsChanged = 0x10
    };
    typedef int TValChngFlags;

    mutable EDiagSev       m_Severity;     ///< Severity level of current msg
    mutable int            m_ErrCode;      ///< Error code
    mutable int            m_ErrSubCode;   ///< Error subcode
            CDiagBuffer&   m_Buffer;       ///< This thread's error msg. buffer
    mutable TDiagPostFlags m_PostFlags;    ///< Bitwise OR of "EDiagPostFlag"
    mutable bool           m_CheckFilters; ///< Whether to apply filters

    CDiagCompileInfo       m_CompileInfo;

    mutable string         m_File;         ///< File name
    mutable size_t         m_Line;         ///< Line number
    mutable string         m_Module;       ///< Module name
    mutable string         m_Class;        ///< Class name
    mutable string         m_Function;     ///< Function name
    mutable TValChngFlags  m_ValChngFlags;

    /// Private replacement for Endm called from manipulators. Unlike Endm,
    /// does not reset ErrCode if buffer is not set.
    void x_EndMess(void) const;

    /// Helper func for the exception-related Put()
    /// @sa Put()
    NCBI_XNCBI_EXPORT const CNcbiDiag& x_Put(const CException& ex) const;

    /// Private copy constructor to prohibit copy.
    CNcbiDiag(const CNcbiDiag&);

    /// Private assignment operator to prohibit assignment.
    CNcbiDiag& operator= (const CNcbiDiag&);
};



/////////////////////////////////////////////////////////////////////////////
// ATTENTION:  the following functions are application-wide, i.e they
//             are not local for a particular thread
/////////////////////////////////////////////////////////////////////////////


/// Check if a specified flag is set.
///
/// @param flag
///   Flag to check
/// @param flags
///   If eDPF_Default is set for "flags" then use the current global flags on
///   its place (merged with other flags from "flags").
/// @return
///   "TRUE" if the specified "flag" is set in global "flags" that describes
///   the post settings.
/// @sa SetDiagPostFlag(), UnsetDiagPostFlag()
inline bool IsSetDiagPostFlag(EDiagPostFlag  flag, 
                              TDiagPostFlags flags = eDPF_Default);

/// Set global post flags to "flags".
/// If "flags" have flag eDPF_Default set, it will be replaced by the
/// current global post flags.
/// @return
///   Previously set flags
NCBI_XNCBI_EXPORT
extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags);

/// Set the specified flag (globally).
NCBI_XNCBI_EXPORT
extern void SetDiagPostFlag(EDiagPostFlag flag);

/// Unset the specified flag (globally).
NCBI_XNCBI_EXPORT
extern void UnsetDiagPostFlag(EDiagPostFlag flag);

/// Versions of the above for extra trace flags.
/// ATTENTION:  Thus set trace flags will be ADDED to the regular
///             posting flags.

NCBI_XNCBI_EXPORT
extern TDiagPostFlags SetDiagTraceAllFlags(TDiagPostFlags flags);

NCBI_XNCBI_EXPORT
extern void SetDiagTraceFlag(EDiagPostFlag flag);

NCBI_XNCBI_EXPORT
extern void UnsetDiagTraceFlag(EDiagPostFlag flag);


/// Action to perform in StopTraceCollect()
enum ETraceCollectAction {
    eTrace_Print,   ///< Print all collected trace messages
    eTrace_Discard  ///< Discard collected trace messages
};

/// Start collecting trace messages even if trace is turned off.
/// Nested starts are allowed, see note to StopTraceCollect().
NCBI_XNCBI_EXPORT
void StartTraceCollect(void);

/// Stop collecting trace messages and perform the specified action.
/// On eTrace_Print all collected messages are printed and removed
/// from the collection. On eTrace_Discard the messages are silently
/// discarded only if the call matches the first call to StartTraceCollect,
/// otherwise the messages are still kept in the collection and can be
/// printed by another StopTraceMessages().
NCBI_XNCBI_EXPORT
void StopTraceCollect(ETraceCollectAction action);


/// Specify a string to prefix all subsequent error postings with.
NCBI_XNCBI_EXPORT
extern void SetDiagPostPrefix(const char* prefix);

/// Push a string to the list of message prefixes.
NCBI_XNCBI_EXPORT
extern void PushDiagPostPrefix(const char* prefix);

/// Pop a string from the list of message prefixes.
NCBI_XNCBI_EXPORT
extern void PopDiagPostPrefix(void);

/// Get iteration number/request ID for the current thread.
NCBI_XNCBI_EXPORT
extern int GetDiagRequestId(void);

/// Set iteration number/request ID for the current thread.
NCBI_XNCBI_EXPORT
extern void SetDiagRequestId(int id);


NCBI_DEPRECATED
inline int GetFastCGIIteration(void)
{
    return GetDiagRequestId();
}


NCBI_DEPRECATED
inline void SetFastCGIIteration(int id)
{
    SetDiagRequestId(id);
}


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagAutoPrefix --
///
/// Define the auxiliary class to temporarily add a prefix.

class NCBI_XNCBI_EXPORT CDiagAutoPrefix
{
public:
    /// Constructor.
    CDiagAutoPrefix(const string& prefix);

    /// Constructor.
    CDiagAutoPrefix(const char*   prefix);

    /// Remove the prefix automagically, when the object gets out of scope.
    ~CDiagAutoPrefix(void);
};


/// Diagnostic post severity level.
///
/// The value of DIAG_POST_LEVEL can be a digital value (0-9) or 
/// string value from CDiagBuffer::sm_SeverityName[].
#define DIAG_POST_LEVEL "DIAG_POST_LEVEL"

/// Set the threshold severity for posting the messages.
///
/// This function has effect only if:
///   - Environment variable $DIAG_POST_LEVEL is not set, and
///   - Registry value of DIAG_POST_LEVEL, section DEBUG is not set 
///
/// Another way to do filtering is to call SetDiagFilter
///
/// @param  post_sev
///   Post only messages with severity greater or equal to "post_sev".
///
///   Special case:  eDiag_Trace -- print all messages and turn on the tracing.
/// @return
///   Return previous post-level.
/// @sa SetDiagFilter(), SetDiagTrace()
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagPostLevel(EDiagSev post_sev = eDiag_Error);

/// Compare two severities.
/// @return
///   The return value is negative if the first value is lower than
/// the second one, positive if it's higher than the second one,
/// 0 if the severities are equal.
NCBI_XNCBI_EXPORT
extern int CompareDiagPostLevel(EDiagSev sev1, EDiagSev sev2);

/// Check if the specified severity is higher or equal to the currently
/// selected post level and will be printed by LOG_POST/ERR_POST.
NCBI_XNCBI_EXPORT
extern bool IsVisibleDiagPostLevel(EDiagSev sev);

/// Disable change the diagnostic post level.
///
/// Consecutive using SetDiagPostLevel() will not have effect.
NCBI_XNCBI_EXPORT
extern bool DisableDiagPostLevelChange(bool disable_change = true);

/// Sets and locks the level, combining the previous two calls.
NCBI_XNCBI_EXPORT
extern void SetDiagFixedPostLevel(EDiagSev post_sev);

/// Set the "die" (abort) level for the program.
///
/// Abort the application if severity is >= "die_sev".
/// Throw an exception if die_sev is not in the range
/// [eDiagSevMin..eDiag_Fatal].
/// @return
///   Return previous die-level.
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagDieLevel(EDiagSev die_sev = eDiag_Fatal);

/// Ignore the die level settings.  Return previous setting.
///
/// WARNING!!! -- not recommended for use unless you are real desperate:
/// By passing TRUE to this function you can make your application
/// never exit/abort regardless of the level set by SetDiagDieLevel().
/// But be warned this is usually a VERY BAD thing to do!
/// -- because any library code counts on at least "eDiag_Fatal" to exit
/// unconditionally, and thus what happens once "eDiag_Fatal" has been posted,
/// is, in general, totally unpredictable!  Therefore, use it on your own risk.
NCBI_XNCBI_EXPORT
extern bool IgnoreDiagDieLevel(bool ignore);

/// Abort handler function type.
typedef void (*FAbortHandler)(void);

/// Set/unset abort handler.
///
/// If "func"==0 use default handler.
NCBI_XNCBI_EXPORT
extern void SetAbortHandler(FAbortHandler func = 0);

/// Smart abort function.
///
/// Processes user abort handler and does not popup assert windows
/// if specified (environment variable DIAG_SILENT_ABORT is "Y" or "y").
NCBI_XNCBI_EXPORT
extern void Abort(void);

/// Diagnostic trace setting.
#define DIAG_TRACE "DIAG_TRACE"

/// Which setting disables/enables posting of "eDiag_Trace" messages.
///
/// By default, trace messages are disabled unless:
/// - Environment variable $DIAG_TRACE is set (to any value), or
/// - Registry value of DIAG_TRACE, section DEBUG is set (to any value)
enum EDiagTrace {
    eDT_Default = 0,  ///< Restores the default tracing context
    eDT_Disable,      ///< Ignore messages of severity "eDiag_Trace"
    eDT_Enable        ///< Enable messages of severity "eDiag_Trace"
};


/// Set the diagnostic trace settings.
NCBI_XNCBI_EXPORT
extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt = eDT_Default);



/// Forward declarations
class CTime;

/// Internal structure to hold diag message string data.
struct SDiagMessageData;


/////////////////////////////////////////////////////////////////////////////
///
/// SDiagMessage --
///
/// Diagnostic message structure.
///
/// Defines structure of the "data" message that is used with message handler
/// function("func"),  and destructor("cleanup").
/// The "func(..., data)" to be called when any instance of "CNcbiDiagBuffer"
/// has a new diagnostic message completed and ready to post.
/// "cleanup(data)" will be called whenever this hook gets replaced and
/// on the program termination.
/// NOTE 1:  "func()", "cleanup()" and "g_SetDiagHandler()" calls are
///          MT-protected, so that they would never be called simultaneously
///          from different threads.
/// NOTE 2:  By default, the errors will be written to standard error stream.

struct NCBI_XNCBI_EXPORT SDiagMessage {
    typedef Uint8 TPID;  ///< Process ID
    typedef Uint8 TTID;  ///< Thread ID
    typedef Int8  TUID;  ///< Unique process ID

    /// Initalize SDiagMessage fields.
    SDiagMessage(EDiagSev severity, const char* buf, size_t len,
                 const char* file = 0, size_t line = 0,
                 TDiagPostFlags flags = eDPF_Default, const char* prefix = 0,
                 int err_code = 0, int err_subcode = 0,
                 const char* err_text  = 0,
                 const char* module    = 0,
                 const char* nclass    = 0, 
                 const char* function  = 0,
                 TPID        pid       = 0,
                 TTID        tid       = 0,
                 int         proc_post = 0,
                 int         thr_post  = 0,
                 int         iter      = 0);

    /// Copy constructor required to store the messages and flush them when
    /// the diagnostics setup is finished.
    SDiagMessage(const SDiagMessage& message);

    /// Assignment of messages
    SDiagMessage& operator=(const SDiagMessage& message);

    /// Parse a string back into SDiagMessage
    SDiagMessage(const string& message);
    ~SDiagMessage(void);

    /// Type of event to report
    enum EEventType {
        eEvent_Start,        ///< Application start
        eEvent_Stop,         ///< Application exit
        eEvent_Extra,        ///< Other application events
        eEvent_RequestStart, ///< Start processing request
        eEvent_RequestStop   ///< Finish processing request
    };

    static string GetEventName(EEventType event);

    mutable EDiagSev m_Severity;   ///< Severity level
    const char*      m_Buffer;     ///< Not guaranteed to be '\0'-terminated!
    size_t           m_BufferLen;  ///< Length of m_Buffer
    const char*      m_File;       ///< File name
    const char*      m_Module;     ///< Module name
    const char*      m_Class;      ///< Class name
    const char*      m_Function;   ///< Function name
    size_t           m_Line;       ///< Line number in file
    int              m_ErrCode;    ///< Error code
    int              m_ErrSubCode; ///< Sub Error code
    TDiagPostFlags   m_Flags;      ///< Bitwise OR of "EDiagPostFlag"
    const char*      m_Prefix;     ///< Prefix string
    const char*      m_ErrText;    ///< Sometimes 'error' has no numeric code,
                                   ///< but can be represented as text
    TPID             m_PID;        ///< Process ID
    TTID             m_TID;        ///< Thread ID
    int              m_ProcPost;   ///< Number of the post in the process
    int              m_ThrPost;    ///< Number of the post in the thread
    int              m_RequestId;  ///< FastCGI iteration or request ID

    /// If the severity is eDPF_AppLog, m_Event contains event type.
    EEventType       m_Event;

    /// Get UID from current context or parsed from a string
    TUID GetUID(void) const;
    /// Get time and date - current or parsed.
    CTime GetTime(void) const;

    /// Compose a message string in the standard format(see also "flags"):
    ///    "<file>", line <line>: <severity>: [<prefix>] <message> [EOL]
    /// and put it to string "str", or write to an output stream "os".

    /// Which write flags should be output in diagnostic message.
    enum EDiagWriteFlags {
        fNone     = 0x0,      ///< No flags
        fNoEndl   = 0x01,     ///< No end of line
        fNoPrefix = 0x02      ///< No std prefix
    };

    typedef int TDiagWriteFlags; /// Binary OR of "EDiagWriteFlags"

    /// Write to string.
    void Write(string& str, TDiagWriteFlags flags = fNone) const;
    
    /// Write to stream.
    CNcbiOstream& Write  (CNcbiOstream& os, TDiagWriteFlags fl = fNone) const;

    /// For compatibility x_Write selects old or new message formatting
    /// depending on DIAG_OLD_POST_FORMAT parameter.
    CNcbiOstream& x_Write(CNcbiOstream& os, TDiagWriteFlags fl = fNone) const;
    CNcbiOstream& x_OldWrite(CNcbiOstream& os, TDiagWriteFlags fl = fNone) const;
    CNcbiOstream& x_NewWrite(CNcbiOstream& os, TDiagWriteFlags fl = fNone) const;
    string x_GetModule(void) const;

private:
    enum EFormatFlag {
        eFormat_Old,  // Force old post format
        eFormat_New,  // Force new post format
        eFormat_Auto  // Get post format from CDiagContext, default
    };
    void x_SetFormat(EFormatFlag fmt) const { m_Format = fmt; }
    bool x_IsSetOldFormat(void) const;
    friend class CDiagContext;

    // Initialize data with the current values
    void x_InitData(void) const;
    // Save current context properties
    void x_SaveProperties(const string& host,
                          const string& client,
                          const string& session,
                          const string& app_name) const;

    mutable SDiagMessageData* m_Data;
    mutable EFormatFlag       m_Format;
};

/// Insert message in output stream.
inline CNcbiOstream& operator<< (CNcbiOstream& os, const SDiagMessage& mess) {
    return mess.Write(os);
}



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagContext --
///
/// NCBI diagnostic context. Storage for application-wide properties.

class CStopWatch;
class CDiagHandler;
class CNcbiRegistry;

/// Where to write the application's diagnostics to.
enum EAppDiagStream {
    eDS_ToStdout,    ///< To standard output stream
    eDS_ToStderr,    ///< To standard error stream
    eDS_ToStdlog,    ///< Try standard log file (app.name + ".log") in /log/
                     ///< and current directory, use stderr if both fail.
    eDS_ToMemory,    ///< Keep in a temp.memory buffer, see FlushMessages()
    eDS_Disable,     ///< Dont write it anywhere
    eDS_User,        ///< Leave as was previously set (or not set) by user
    eDS_AppSpecific, ///< Call the application's SetupDiag_AppSpecific()
                     ///< @deprecated
    eDS_Default,     ///< Try standard log file (app.name + ".log") in /log/,
                     ///< use stderr on failure.
    eDS_ToSyslog     ///< To system log daemon
};


/// Flags to control collecting messages and flushing them to the new
/// destination when switching diag handlers.
enum EDiagCollectMessages {
    eDCM_Init,        ///< Start collecting messages (with limit), do nothing
                      ///< if already initialized.
    eDCM_InitNoLimit, ///< Start collecting messages without limit (must stop
                      ///< collecting later using eDCM_Flush or eDCM_Discard).
    eDCM_NoChange,    ///< Continue collecting messages if already started.
    eDCM_Flush,       ///< Flush the collected messages and stop collecting.
    eDCM_Discard      ///< Discard the collected messages without flushing.
};


/// Post nubmer increment flag for GetProcessPostNumber() and
/// GetThreadPostNumber().
enum EPostNumberIncrement {
    ePostNumber_NoIncrement,  ///< Get post number without incrementing it
    ePostNumber_Increment     ///< Increment and return the new post number
};


/// Thread local context data stored in TLS
class NCBI_XNCBI_EXPORT CDiagContextThreadData
{
public:
    CDiagContextThreadData(void);
    ~CDiagContextThreadData(void);

    /// CDiagContext properties
    typedef map<string, string> TProperties;
    enum EGetProperties {
        eProp_Get,    ///< Do not create properties if not exist yet
        eProp_Create  ///< Auto-create properties if not exist
    };
    TProperties* GetProperties(EGetProperties flag);

    /// Request id
    int GetRequestId(void) { return m_RequestId; }
    void SetRequestId(int id) { m_RequestId = id; }
    void IncRequestId(void) { m_RequestId++; }

    /// Get request timer, create if not exist yet
    CStopWatch* GetOrCreateStopWatch(void);
    /// Get request timer or null
    CStopWatch* GetStopWatch(void) { return m_StopWatch.get(); }
    /// Delete request timer
    void ResetStopWatch(void);

    /// Diag buffer
    CDiagBuffer& GetDiagBuffer(void) { return *m_DiagBuffer; }

    /// Get diag context data for the current thread
    static CDiagContextThreadData& GetThreadData(void);

    /// Thread ID
    typedef Uint8 TTID;

    /// Get cached thread ID
    TTID GetTID(void) const { return m_TID; }

    /// Get thread post number
    int GetThreadPostNumber(EPostNumberIncrement inc);

private:
    CDiagContextThreadData(const CDiagContextThreadData&);
    CDiagContextThreadData& operator=(const CDiagContextThreadData&);

    auto_ptr<TProperties> m_Properties;       // Per-thread properties
    int                   m_RequestId;        // Per-thread request ID
    auto_ptr<CStopWatch>  m_StopWatch;        // Request timer
    auto_ptr<CDiagBuffer> m_DiagBuffer;       // Thread's diag buffer
    TTID                  m_TID;              // Cached thread ID
    int                   m_ThreadPostNumber; // Number of posted messages
};


class NCBI_XNCBI_EXPORT CDiagContext
{
public:
    CDiagContext(void);
    ~CDiagContext(void);

    typedef Uint8 TPID;
    // Get cached PID (read real PID if not cached yet).
    static TPID GetPID(void);
    // Reset PID cache (e.g. after fork).
    static void UpdatePID(void);

    typedef SDiagMessage::TUID TUID;

    /// Return (create if not created yet) unique diagnostic ID.
    TUID GetUID(void) const;
    /// Return string representation of UID
    string GetStringUID(TUID uid = 0) const;

    /// Set AutoWrite flag. If set, each property is posted to the current
    /// app-log stream when a new value is set.
    void SetAutoWrite(bool value);

    /// Property visibility flag.
    enum EPropertyMode {
        eProp_Default,  ///< Auto-mode for known properties, local for others
        eProp_Global,   ///< The property is global for the application
        eProp_Thread    ///< The property has separate value in each thread
    };

    /// Set application context property by name.
    /// Write property to the log if AutoPrint flag is set.
    /// Property mode defines if the property is a global or a
    /// per-thread one. By default unknown properties are set as
    /// thread-local.
    void SetProperty(const string& name,
                     const string& value,
                     EPropertyMode mode = eProp_Default);

    /// Get application context property by name, return empty string if the
    /// property is not set. If mode is eProp_Default and the property is
    /// not a known one, check thread-local properties first.
    string GetProperty(const string& name,
                       EPropertyMode mode = eProp_Default) const;

    /// Delete a property by name. If mode is eProp_Default and the property
    /// is not a known one, check thread-local properties first.
    void DeleteProperty(const string& name,
                        EPropertyMode mode = eProp_Default);

    /// Forced dump of all set properties.
    void PrintProperties(void);

    /// Global properties
    static const char* kProperty_UserName;
    static const char* kProperty_HostName;
    static const char* kProperty_HostIP;
    static const char* kProperty_AppName;
    static const char* kProperty_ExitSig;
    static const char* kProperty_ExitCode;
    /// Per-thread properties
    static const char* kProperty_AppState;
    static const char* kProperty_ClientIP;
    static const char* kProperty_SessionID;
    static const char* kProperty_ReqStatus;
    static const char* kProperty_ReqTime;
    static const char* kProperty_BytesRd;
    static const char* kProperty_BytesWr;

    /// Print start/stop etc. message. If the following values are set as
    /// properties, they will be dumped before the message:
    ///   host | host_ip_addr
    ///   client_ip
    ///   session_id
    ///   app_name
    /// All messages have the following prefix:
    ///   PID/TID/ITER UID TIME HOST CLIENT SESSION_ID APP_NAME
    /// Depending on its type, a message can be prefixed with the following
    /// properties if they are set:
    ///   start
    ///   stop [SIG] [EXIT_CODE] ELAPSED_TIME
    ///   extra
    ///   request-start
    ///   request-stop [STATUS] [REQ_ELAPSED_TIME] [BYTES_RD] [BYTES_WR]
    void PrintStart(const string& message);
    /// Print exit message.
    void PrintStop(void);
    /// Print extra message.
    void PrintExtra(const string& message);
    /// Print request start message (for request-driven applications)
    void PrintRequestStart(const string& message);
    /// Print request stop message (for request-driven applications)
    void PrintRequestStop(void);

    /// Application execution states shown in the std prefix
    enum EAppState {
        eState_AppBegin,      ///< AB
        eState_AppRun,        ///< A
        eState_AppEnd,        ///< AE
        eState_RequestBegin,  ///< RB
        eState_Request,       ///< R
        eState_RequestEnd     ///< RE
    };

    /// Set application state property corresponding to the flag.
    /// By default the property is set as global if the flag indicates an
    /// application state and as thread local for request states.
    void SetAppState(EAppState state, EPropertyMode mode = eProp_Default);

    /// Check old/new format flag (for compatibility only)
    static bool IsSetOldPostFormat(void);
    /// Set old/new format flag
    static void SetOldPostFormat(bool value);

    /// Check if system TID is printed instead of CThread::GetSelf()
    static bool IsUsingSystemThreadId(void);
    /// Switch printing system TID (rather than CThread::GetSelf()) on/off
    static void UseSystemThreadId(bool value = true);

    /// Set username property
    /// @sa SetDiagUserAndHost
    void SetUsername(const string& username);
    /// Set hostname property
    /// @sa SetDiagUserAndHost
    void SetHostname(const string& hostname);

    /// Write standard prefix to the stream. If SDiagMessage is provided,
    /// take values from the message (PID/TID/RID etc.), otherwise use
    /// other sources (e.g. current diag buffer).
    void WriteStdPrefix(CNcbiOstream& ostr,
                        const SDiagMessage* msg) const;

    /// Start collecting all messages (the collected messages can be flushed
    /// to a new destination later). Stop collecting messages when max_size
    /// is reached.
    void InitMessages(size_t max_size = 100);
    /// Save new message
    void PushMessage(const SDiagMessage& message);
    /// Flush the collected messages to the current diag handler.
    /// Does not clear the collected messages.
    void FlushMessages(CDiagHandler& handler);
    /// Discard the collected messages without printing them.
    void DiscardMessages(void);

    /// Get log file truncation flag
    static bool GetLogTruncate(void);
    /// Set log file truncation flag
    static void SetLogTruncate(bool value);

    /// Enable creating log files in /log directory. The function has no
    /// effect if called after final SetupDiag() by AppMain(). Otherwise
    /// it will try to switch logging to /log/fallback/UNKNOWN.log until
    /// the real log name is known.
    static void SetUseRootLog(void);

    /// Application-wide diagnostics setup. Attempts to create log files
    /// or diag streams according to the 'ds' flag. If 'config' is set,
    /// gets name of the log file from the registry.
    static void SetupDiag(EAppDiagStream       ds = eDS_Default,
                          CNcbiRegistry*       config = NULL,
                          EDiagCollectMessages collect = eDCM_NoChange);

    /// Return process post number (incrementing depends on the flag).
    static int GetProcessPostNumber(EPostNumberIncrement inc);

    /// Internal function, should be used only by CNcbiApplication.
    static void x_FinalizeSetupDiag(void);

private:
    CDiagContext(const CDiagContext&);
    CDiagContext& operator=(const CDiagContext&);

    // Initialize UID
    void x_CreateUID(void) const;
    // Write message to the log using current handler
    void x_PrintMessage(SDiagMessage::EEventType event,
                        const string&            message);

    const string x_GetHost(void) const;

    typedef map<string, string> TProperties;
    friend void ThreadDataTlsCleanup(CDiagContextThreadData* value,
                                     void* cleanup_data);

    // Saved messages to be flushed after setting up log files
    typedef list<SDiagMessage> TMessages;

    // Cached process ID
    static TPID            sm_PID;

    mutable TUID           m_UID;
    mutable string         m_Host;
    TProperties            m_Properties;
    auto_ptr<CStopWatch>   m_StopWatch;
    auto_ptr<TMessages>    m_Messages;
    size_t                 m_MaxMessages;
};


/// Get diag context instance
NCBI_XNCBI_EXPORT CDiagContext& GetDiagContext(void);


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagHandler --
///
/// Base diagnostic handler class.

class NCBI_XNCBI_EXPORT CDiagHandler
{
public:
    /// Destructor.
    virtual ~CDiagHandler(void) {}

    /// Post message to handler.
    virtual void Post(const SDiagMessage& mess) = 0;

    /// Get current diag posts destination
    virtual string GetLogName(void);
};

/// Diagnostic handler function type.
typedef void (*FDiagHandler)(const SDiagMessage& mess);

/// Diagnostic cleanup function type.
typedef void (*FDiagCleanup)(void* data);

/// Set the diagnostic handler using the specified diagnostic handler class.
NCBI_XNCBI_EXPORT
extern void SetDiagHandler(CDiagHandler* handler,
                           bool can_delete = true);

/// Get the currently set diagnostic handler class.
NCBI_XNCBI_EXPORT
extern CDiagHandler* GetDiagHandler(bool take_ownership = false);

/// Set the diagnostic handler using the specified diagnostic handler
/// and cleanup functions.
NCBI_XNCBI_EXPORT
extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup);

/// Check if diagnostic handler is set.
///
/// @return 
///   Return TRUE if user has ever set (or unset) diag. handler.
NCBI_XNCBI_EXPORT
extern bool IsSetDiagHandler(void);


/////////////////////////////////////////////////////////////////////////////
//
// Diagnostic Filter Functionality
//

/// Diag severity types to put the filter on
///
/// @sa SetDiagFilter
enum EDiagFilter {
    eDiagFilter_Trace,  ///< for TRACEs only
    eDiagFilter_Post,   ///< for all non-TRACE, non-FATAL
    eDiagFilter_All     ///< for all non-FATAL
};


/// Set diagnostic filter
///
/// Diagnostic filter acts as a second level filtering mechanism
/// (the primary established by global error post level)
/// @sa SetDiagPostLevel
///
///
/// @param what
///    Filter is set for
/// @param filter_str
///    Filter string
NCBI_XNCBI_EXPORT
extern void SetDiagFilter(EDiagFilter what, const char* filter_str);


/////////////////////////////////////////////////////////////////////////////
///
/// CStreamDiagHandler_Base --
///
/// Base class for stream and file based handlers

class NCBI_XNCBI_EXPORT CStreamDiagHandler_Base : public CDiagHandler
{
public:
    CStreamDiagHandler_Base(void);

    virtual string GetLogName(void);
    virtual CNcbiOstream* GetStream(void) { return 0; }

    // Reopen file to enable log rotation.
    virtual void Reopen(bool truncate = false);

protected:
    void SetLogName(const string& log_name) { m_LogName = log_name; }

private:
    string m_LogName;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStreamDiagHandler --
///
/// Specialization of "CDiagHandler" for the stream-based diagnostics.

class NCBI_XNCBI_EXPORT CStreamDiagHandler : public CStreamDiagHandler_Base
{
public:
    /// Constructor.
    ///
    /// This does *not* own the stream; users will need to clean it up
    /// themselves if appropriate. 
    /// @param os
    ///   Output stream.
    /// @param quick_flush
    ///   Do stream flush after every message. 
    CStreamDiagHandler(CNcbiOstream* os,
                       bool          quick_flush = true,
                       const string& stream_name = "");

    /// Post message to the handler.
    virtual void Post(const SDiagMessage& mess);
    virtual CNcbiOstream* GetStream(void) { return m_Stream; }

protected:
    CNcbiOstream* m_Stream;         ///< Diagnostic stream

private:
    bool          m_QuickFlush;     ///< Quick flush of stream flag
};


/////////////////////////////////////////////////////////////////////////////
///
/// CFileHandleDiagHandler --
///
/// Specialization of "CDiagHandler" for the file-handle based diagnostics.
/// Writes messages using system write rather than stream to make the
/// operation really atomic. Re-opens file periodically to make rotation
/// possible.

class NCBI_XNCBI_EXPORT CFileHandleDiagHandler : public CStreamDiagHandler_Base
{
public:
    /// Constructor.
    ///
    /// Open file handle.
    /// themselves if appropriate. 
    /// @param fname
    ///   Output file name.
    CFileHandleDiagHandler(const string& fname);
    /// Close file handle
    ~CFileHandleDiagHandler(void);

    /// Post message to the handler.
    virtual void Post(const SDiagMessage& mess);

    bool Valid(void) { return m_Handle != -1; }

    // Reopen file to enable log rotation.
    virtual void Reopen(bool truncate = false);

private:
    int    m_Handle;      ///< File handle
    CTime* m_LastReopen;  ///< Last reopen time

    /// Save messages if the handle is unavailable
    typedef list<SDiagMessage> TMessages;
    auto_ptr<TMessages> m_Messages;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CFileDiagHandler --
///
/// Specialization of "CDiagHandler" for the file-based diagnostics.
/// Splits output into three files: .err (severity higher than the
/// threshold), .trace (severity below the threshold) and .log
/// (application access log). Re-opens the files periodically
/// to allow safe log rotation.

/// Type of file for the output
enum EDiagFileType
{
    eDiagFile_Err,    ///< Error log file
    eDiagFile_Log,    ///< Access log file
    eDiagFile_Trace,  ///< Trace log file
    eDiagFile_All     ///< All log files
};

class NCBI_XNCBI_EXPORT CFileDiagHandler : public CStreamDiagHandler_Base
{
public:
    /// Constructor. initializes log file(s) with the arguments.
    /// @sa SetLogFile
    CFileDiagHandler(void);
    ~CFileDiagHandler(void);

    /// Post message to the handler. Info and Trace messages are sent
    /// to file_name.trace file, all others go to file_name.err file.
    /// Application access messages go to file_name.log file.
    virtual void Post(const SDiagMessage& mess);

    /// Set new log file.
    ///
    /// @param file_name
    ///   File name. If file_type is eDiagFile_All, the output will be written
    ///   to file_name.(err|log|trace). Otherwise the filename is used as-is.
    ///   Special filenames are:
    ///     ""          - disable diag messages;
    ///     "-"         - print to stderr
    ///     "/dev/null" - never add .(err|log|trace) to the name.
    /// @param file_type
    ///   Type of log file to set - error, trace or application log.
    /// @param quick_flush
    ///   Do stream flush after every message.
    bool SetLogFile(const string& file_name,
                    EDiagFileType file_type,
                    bool          quick_flush);

    /// Get current log file name. If file_type is eDiagFile_All, always
    /// returns empty string.
    string GetLogFile(EDiagFileType file_type) const;

    /// Get current log stream. Return NULL if the selected destination
    /// is not a stream.
    CNcbiOstream* GetLogStream(EDiagFileType file_type);

    // Reopen all files to enable log rotation.
    virtual void Reopen(bool truncate = false);

private:
    auto_ptr<CStreamDiagHandler_Base> m_Err;
    auto_ptr<CStreamDiagHandler_Base> m_Log;
    auto_ptr<CStreamDiagHandler_Base> m_Trace;
    CTime*                            m_LastReopen;
};


/// Output diagnostics using both old and new style handlers.
NCBI_DEPRECATED
NCBI_XNCBI_EXPORT extern void SetDoubleDiagHandler(void); ///< @deprecated


/// Set diagnostic stream.
///
/// Error diagnostics are written to output stream "os".
/// This uses the SetDiagHandler() functionality.
NCBI_XNCBI_EXPORT
extern void SetDiagStream
(CNcbiOstream* os,
 bool          quick_flush  = true,///< Do stream flush after every message
 FDiagCleanup  cleanup      = 0,   ///< Call "cleanup(cleanup_data)" if diag.
 void*         cleanup_data = 0,   ///< Stream is changed (see SetDiagHandler)
 const string& stream_name  = "STREAM" ///< Stream name (e.g. STDERR, file.log)
 );

// Return TRUE if "os" is the current diag. stream.
NCBI_XNCBI_EXPORT 
extern bool IsDiagStream(const CNcbiOstream* os);

/// Get current diagnostic stream (if it was set by SetDiagStream) or NULL.
NCBI_XNCBI_EXPORT
extern CNcbiOstream* GetDiagStream(void);

/// Split log files flag. If set, the output is sent to different
/// log files depending on the severity level.
NCBI_XNCBI_EXPORT extern void SetSplitLogFile(bool value = true);
/// Get split log files flag.
NCBI_XNCBI_EXPORT extern bool GetSplitLogFile(void);

/// Set log files.
/// Send output to file_name or to file_name.(err|log|trace) depending
/// on the split log file flag and file_type. If a single file type
/// is selected, other types remain the same or are switched to
/// stderr if their files have not been assigned yet.
/// If split log flag is off, any file type except eDiagFile_All
/// will be ignored.
/// If the file_name contains one of the extensions .log, .err or .trace
/// and the file type is eDiagFile_All, the extension will be removed
/// before adding the new one.
/// Return true on success, false if the file could not be open.
NCBI_XNCBI_EXPORT
extern bool SetLogFile(const string& file_name,
                       EDiagFileType file_type = eDiagFile_All,
                       bool          quick_flush = true);

/// Get log file name for the given log type. Return empty string for
/// eDiagFile_All or if the log file handler is not installed.
NCBI_XNCBI_EXPORT
extern string GetLogFile(EDiagFileType file_type);

/// Get log file name or diag handler name.
NCBI_XNCBI_EXPORT
extern string GetLogFile(void);


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagFactory --
///
/// Diagnostic handler factory.

class NCBI_XNCBI_EXPORT CDiagFactory
{
public:
    virtual ~CDiagFactory() { }
    /// Factory method interface.
    virtual CDiagHandler* New(const string& s) = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagRestorer --
///
/// Auxiliary class to limit the duration of changes to diagnostic settings.

class NCBI_XNCBI_EXPORT CDiagRestorer
{
public:
    CDiagRestorer (void); ///< Captures current settings
    ~CDiagRestorer(void); ///< Restores captured settings
private:
    /// Private new operator.
    ///
    /// Prohibit dynamic allocation because there's no good reason to allow
    /// it, and out-of-order destruction is problematic.
    void* operator new      (size_t)  { throw runtime_error("forbidden"); }

    /// Private new[] operator.
    ///
    /// Prohibit dynamic allocation because there's no good reason to allow
    /// it, and out-of-order destruction is problematic.
    void* operator new[]    (size_t)  { throw runtime_error("forbidden"); }

    /// Private delete operator.
    ///
    /// Prohibit dynamic deallocation (and allocation) because there's no
    /// good reason to allow it, and out-of-order destruction is problematic.
    void  operator delete   (void*)   { throw runtime_error("forbidden"); }

    /// Private delete[] operator.
    ///
    /// Prohibit dynamic deallocation (and allocation) because there's no
    /// good reason to allow it, and out-of-order destruction is problematic.
    void  operator delete[] (void*)   { throw runtime_error("forbidden"); }

    string            m_PostPrefix;            ///< Message prefix
    list<string>      m_PrefixList;            ///< List of prefixs
    TDiagPostFlags    m_PostFlags;             ///< Post flags
    EDiagSev          m_PostSeverity;          ///< Post severity
    EDiagSevChange    m_PostSeverityChange;    ///< Severity change
    bool              m_IgnoreToDie;           ///< Ignore to die on die sev
    EDiagSev          m_DieSeverity;           ///< Die level severity
    EDiagTrace        m_TraceDefault;          ///< Default trace setting
    bool              m_TraceEnabled;          ///< Trace enabled?
    CDiagHandler*     m_Handler;               ///< Class handler
    bool              m_CanDeleteHandler;      ///< Can handler be deleted?
    CDiagErrCodeInfo* m_ErrCodeInfo;           ///< Error code information
    bool              m_CanDeleteErrCodeInfo;  ///< Can delete err code info?
};



/////////////////////////////////////////////////////////////////////////////
///
/// SDiagErrCodeDescription --
///
/// Structure used to store the errors code and subcode description.

struct NCBI_XNCBI_EXPORT SDiagErrCodeDescription {
    /// Constructor.
    SDiagErrCodeDescription(void);

    /// Destructor.
    SDiagErrCodeDescription(const string& message,     ///< Message
                            const string& explanation, ///< Explanation of msg.
                            int           severity = -1
                                                       ///< Do not override
                                                       ///< if set to -1
                           )
        : m_Message(message),
          m_Explanation(explanation),
          m_Severity(severity)
    {
        return;
    }

public:
    string m_Message;     ///< Error message (short)
    string m_Explanation; ///< Error message (with detailed explanation)
    int    m_Severity;    
                          ///< Message severity (if less that 0, then use
                          ///< current diagnostic severity level)
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagErrCodeInfo --
///
/// Stores mapping of error codes and their descriptions.

class NCBI_XNCBI_EXPORT CDiagErrCodeInfo
{
public:
    /// Constructor.
    CDiagErrCodeInfo(void);

    /// Constructor -- can throw runtime_error.
    CDiagErrCodeInfo(const string& file_name);  

    /// Constructor -- can throw runtime_error.
    CDiagErrCodeInfo(CNcbiIstream& is);

    /// Destructor.
    ~CDiagErrCodeInfo(void);

    /// Read error description from specified file.
    ///
    /// Read error descriptions from the specified file,
    /// store it in memory.
    bool Read(const string& file_name);

    /// Read error description from specified stream.
    ///
    /// Read error descriptions from the specified stream,
    /// store it in memory.
    bool Read(CNcbiIstream& is);
    
    /// Delete all stored error descriptions from memory.
    void Clear(void);

    /// Get description for specified error code.
    ///
    /// Get description message for the error by its code.
    /// @return
    ///   TRUE if error description exists for this code;
    ///   return FALSE otherwise.
    bool GetDescription(const ErrCode&           err_code, 
                        SDiagErrCodeDescription* description) const;

    /// Set error description for specified error code.
    ///
    /// If description for this code already exist, then it
    /// will be overwritten.
    void SetDescription(const ErrCode&                 err_code, 
                        const SDiagErrCodeDescription& description);

    /// Check if error description exists.
    ///
    ///  Return TRUE if description for specified error code exists, 
    /// otherwise return FALSE.
    bool HaveDescription(const ErrCode& err_code) const;

private:

    /// Define map for error messages.
    typedef map<ErrCode, SDiagErrCodeDescription> TInfo;

    /// Map storing error codes and descriptions.
    TInfo m_Info;
};



/// Diagnostic message file.
#define DIAG_MESSAGE_FILE "MessageFile"

/// Set handler for processing error codes.
///
/// By default this handler is unset. 
/// NcbiApplication can init itself only if registry key DIAG_MESSAGE_FILE
/// section DEBUG) is specified. The value of this key should be a name 
/// of the file with the error codes explanations.
NCBI_XNCBI_EXPORT
extern void SetDiagErrCodeInfo(CDiagErrCodeInfo* info,
                               bool              can_delete = true);

/// Indicates whether an error-code processing handler has been set.
NCBI_XNCBI_EXPORT
extern bool IsSetDiagErrCodeInfo();

/// Get handler for processing error codes.
NCBI_XNCBI_EXPORT
extern CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership = false);


/* @} */


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file

#include <corelib/ncbidiag.inl>


END_NCBI_SCOPE


#endif  /* CORELIB___NCBIDIAG__HPP */
