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
#include <stdexcept>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Set default module name based on NCBI_MODULE macro
#ifdef NCBI_MODULE
#  define NCBI_MODULE_AS_STRING(module) #module
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
                     const char* module = 0);
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

private:
    const char*    m_File;
    const char*    m_Module;
    int            m_Line; 

    const char*    m_CurrFunctName;
    mutable bool   m_Parsed;
    mutable string m_ClassName;
    mutable string m_FunctName;
};


/// Get current function name. Defined inside of either a method or a function body only.
// Based on boost's BOOST_CURRENT_FUNCTION

#ifndef NDEBUG

#define NCBI_SHOW_FUNCTION_NAME

#endif


#ifdef NCBI_SHOW_FUNCTION_NAME

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600))

# define NCBI_CURRENT_FUNCTION __PRETTY_FUNCTION__

#elif defined(__FUNCSIG__)

# define NCBI_CURRENT_FUNCTION __FUNCSIG__

#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))

# define NCBI_CURRENT_FUNCTION __FUNCTION__

#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)

# define NCBI_CURRENT_FUNCTION __FUNC__

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)

# define NCBI_CURRENT_FUNCTION __func__

#else

# define NCBI_CURRENT_FUNCTION NULL

#endif

#else

# define NCBI_CURRENT_FUNCTION NULL

#endif

/// Make compile time diagnostic information object
/// to use in CDiagInfo and CException
/// @sa
///   CDiagCompileInfo

#define DIAG_COMPILE_INFO NCBI_NS_NCBI::CDiagCompileInfo(__FILE__, __LINE__,  \
                                                 NCBI_CURRENT_FUNCTION, \
                                                 NCBI_MAKE_MODULE(NCBI_MODULE))

    


/// Error posting with file, line number information but without error codes.
///
/// @sa
///   ERR_POST_EX macro
#define ERR_POST(message)                        \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO) \
      << message                                 \
      << NCBI_NS_NCBI::Endm )


/// Log message only without severity, location, prefix information.
///
/// @sa
///   LOG_POST_EX macro
#define LOG_POST(message)                            \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log) \
      << message                                     \
      << NCBI_NS_NCBI::Endm )

/// Error posting with error codes.
///
/// @sa
///   ERR_POST
#define ERR_POST_EX(err_code, err_subcode, message)         \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO)            \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << message                                            \
      << NCBI_NS_NCBI::Endm )

/// Log posting with error codes.
///
/// @sa
///   LOG_POST
#define LOG_POST_EX(err_code, err_subcode, message)         \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log)        \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << message << NCBI_NS_NCBI::Endm )


#define LOG_POST_N_TIMES(count, message) \
    do { \
        static volatile int sx_to_show = (count);  \
        int to_show = sx_to_show; \
        if ( to_show > 0 ) { \
            sx_to_show = to_show - 1; \
            LOG_POST(message);     \
        } \
    } while ( false )


#define ERR_POST_N_TIMES(count, message) \
    do { \
        static volatile int sx_to_show = (count);  \
        int to_show = sx_to_show; \
        if ( to_show > 0 ) { \
            sx_to_show = to_show - 1; \
            ERR_POST(message);     \
        } \
    } while ( false )

#define LOG_POST_ONCE(message) LOG_POST_N_TIMES(1, message)
#define ERR_POST_ONCE(message) ERR_POST_N_TIMES(1, message)


/// Severity level for the posted diagnostics.
enum EDiagSev {
    eDiag_Info = 0, ///< Informational message
    eDiag_Warning,  ///< Warning message
    eDiag_Error,    ///< Error message
    eDiag_Critical, ///< Critical error message
    eDiag_Fatal,    ///< Fatal error -- guarantees exit(or abort)
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
    eDPF_ErrCode            = 0x20,  ///< Set by default (always)
    eDPF_ErrSubCode         = 0x40,  ///< Set by default (always)
    eDPF_ErrCodeMessage     = 0x100, ///< Set by default (always)
    eDPF_ErrCodeExplanation = 0x200, ///< Set by default (always)
    eDPF_ErrCodeUseSeverity = 0x400, ///< Set by default (always)
    eDPF_DateTime           = 0x80,  ///< Include date and time
    eDPF_Location           = 0x800, ///< Include module, class and function
                                     ///< if any, not set by default

    /// All flags (except for the "unusual" ones!)
    eDPF_All                = 0x3FFF,

    // "Unusual" flags -- not included in "eDPF_All"
    eDPF_OmitInfoSev        = 0x4000, ///< No sev. indication if eDiag_Info 
    eDPF_PreMergeLines      = 0x10000,///< Remove EOLs before calling handler
    eDPF_MergeLines         = 0x20000,///< Ask diag.handlers to remove EOLs

    /// Default flags to use when tracing.
    eDPF_Trace              = 0x81F,

    /// Print the posted message only; without severity, location, prefix, etc.
    eDPF_Log                = 0x0,

    /// Use global default flags (merge with).
    /// @sa SetDiagPostFlag(), UnsetDiagPostFlag(), IsSetDiagPostFlag()
    eDPF_Default            = 0x8000
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


class CException;

/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiDiag --
///
/// Define the main NCBI Diagnostic class.

class CNcbiDiag
{
public:
    /// Constructor.
    NCBI_XNCBI_EXPORT
    CNcbiDiag(EDiagSev       sev        = eDiag_Error,  ///< Severity level
              TDiagPostFlags post_flags = eDPF_Default  ///< What info.
             );


    /// Constructor -- includes file and line# information.
    NCBI_XNCBI_EXPORT
    CNcbiDiag(const CDiagCompileInfo& info,  ///< File, line and module
              EDiagSev       sev          = eDiag_Error,  ///< Severity level
              TDiagPostFlags post_flags   = eDPF_Default  ///< What info.
             );

    /// Destructor.
    NCBI_XNCBI_EXPORT
    ~CNcbiDiag(void);

    /// Put object to be formatted to diagnostic stream.
    // Some compilers need to see the body right away, but others need
    // to meet CDiagBuffer first.
    template<class X> const CNcbiDiag& operator<< (const X& x) const
#ifdef NCBI_COMPILER_MSVC
    {
        m_Buffer.Put(*this, x);
        return *this;
    }
#else
      ;
#  define NCBIDIAG_DEFER_GENERIC_PUT
#endif

    /// Insert specified error code into diagnostic stream.
    ///
    /// Example:
    ///   CNcbiDiag() << ErrCode(5,3);
    const CNcbiDiag& operator<< (const ErrCode& err_code) const;

    /// Report specified exception to diagnostic stream.
    NCBI_XNCBI_EXPORT
    const CNcbiDiag& operator<< (const CException& ex) const;

    /// Function-based manipulators.
    const CNcbiDiag& operator<< (const CNcbiDiag& (*f)(const CNcbiDiag&))
        const
    {
        return f(*this);
    }

    // Output manipulators for CNcbiDiag.

    /// Reset the content of current message.
    friend const CNcbiDiag& Reset  (const CNcbiDiag& diag);

    /// Flush current message, start new one.
    friend const CNcbiDiag& Endm   (const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Info.
    friend const CNcbiDiag& Info    (const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Warning.
    friend const CNcbiDiag& Warning (const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Error.
    friend const CNcbiDiag& Error   (const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Critical.
    friend const CNcbiDiag& Critical(const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Fatal.
    friend const CNcbiDiag& Fatal   (const CNcbiDiag& diag);

    /// Flush current message, then  set a severity for the next diagnostic
    /// message to Trace.
    friend const CNcbiDiag& Trace   (const CNcbiDiag& diag);

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
    static void DiagTrouble(const CDiagCompileInfo& info);

    /// Assert specfied expression and report results.
    NCBI_XNCBI_EXPORT
    static void DiagAssert(const CDiagCompileInfo& info,
                           const char* expression);

    /// Display validation message.
    NCBI_XNCBI_EXPORT
    static void DiagValidate(const CDiagCompileInfo& info,
                             const char* expression, const char* message);

private:
    enum EValChngFlags {
        fFileIsChanged = 0x1,
        fLineIsChanged = 0x2,
        fModuleIsChanged = 0x4,
        fClassIsChanged = 0x8,
        fFunctionIsChanged = 0x10
    };
    typedef int TValChngFlags;

    mutable EDiagSev       m_Severity;       ///< Severity level of current msg.
    mutable int            m_ErrCode;        ///< Error code
    mutable int            m_ErrSubCode;     ///< Error subcode
            CDiagBuffer&   m_Buffer;         ///< This thread's error msg. buffer
    mutable TDiagPostFlags m_PostFlags;      ///< Bitwise OR of "EDiagPostFlag"
    mutable bool           m_CheckFilters;   ///< Is it necessary to check filters

    CDiagCompileInfo       m_CompileInfo;

    mutable string         m_File;           ///< File name
    mutable size_t         m_Line;           ///< Line number
    mutable string         m_Module;         ///< Module name
    mutable string         m_Class;          ///< Class name
    mutable string         m_Function;       ///< Function name
    mutable TValChngFlags  m_ValChngFlags;

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

NCBI_XNCBI_EXPORT
extern TDiagPostFlags SetDiagTraceAllFlags(TDiagPostFlags flags);

NCBI_XNCBI_EXPORT
extern void SetDiagTraceFlag(EDiagPostFlag flag);

NCBI_XNCBI_EXPORT
extern void UnsetDiagTraceFlag(EDiagPostFlag flag);

/// Specify a string to prefix all subsequent error postings with.
NCBI_XNCBI_EXPORT
extern void SetDiagPostPrefix(const char* prefix);

/// Push a string to the list of message prefixes.
NCBI_XNCBI_EXPORT
extern void PushDiagPostPrefix(const char* prefix);

/// Pop a string from the list of message prefixes.
NCBI_XNCBI_EXPORT
extern void PopDiagPostPrefix(void);



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

/// Set severity of post messages. 
///
/// This function has effect only if:
///   - Environment variable $DIAG_POST_LEVEL is not set, and
///   - Registry value of DIAG_POST_LEVEL, section DEBUG is not set 
///
/// Do not post messages where severity is less than "min_sev"
/// Another way to do filtering is to call SetDiagFilter
/// @return
///   Return previous post-level.
///
/// @sa SetDiagFilter
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagPostLevel(EDiagSev post_sev = eDiag_Error);

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
/// Abort the application if severity is >= "max_sev".
/// Throw an exception if die_sev is not in the range
/// [eDiagSevMin..eDiag_Fatal].
/// @return
///   Return previous die-level.
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagDieLevel(EDiagSev die_sev = eDiag_Fatal);

/// Ignore the die level settings.
///
/// WARNING!!! -- not recommended for use unless you are real desperate:
/// By passing TRUE to this function you can make your application
/// never exit/abort regardless of the level set by SetDiagDieLevel().
/// But be warned this is usually a VERY BAD thing to do!
/// -- because any library code counts on at least "eDiag_Fatal" to exit
/// unconditionally, and thus what happens after "eDiag_Fatal" is posted is
/// in general totally unpredictable! Therefore, use it on your own risk.
NCBI_XNCBI_EXPORT
extern void IgnoreDiagDieLevel(bool ignore, EDiagSev* prev_sev = 0);

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
    /// Initalize SDiagMessage fields.
    SDiagMessage(EDiagSev severity, const char* buf, size_t len,
                 const char* file = 0, size_t line = 0,
                 TDiagPostFlags flags = eDPF_Default, const char* prefix = 0,
                 int err_code = 0, int err_subcode = 0,
                 const char* err_text = 0,
                 const char* module   = 0,
                 const char* nclass   = 0, 
                 const char* function = 0);

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

    // Compose a message string in the standard format(see also "flags"):
    //    "<file>", line <line>: <severity>: [<prefix>] <message> [EOL]
    // and put it to string "str", or write to an output stream "os".

    /// Which write flags should be output in diagnostic message.
    enum EDiagWriteFlags {
        fNone   = 0x0,      ///< No flags
        fNoEndl = 0x01      ///< No end of line
    };

    typedef int TDiagWriteFlags; /// Binary OR of "EDiagWriteFlags"

    /// Write to string.
    void Write(string& str, TDiagWriteFlags flags = fNone) const;
    
    /// Write to stream.
    CNcbiOstream& Write(CNcbiOstream& os, TDiagWriteFlags flags = fNone) const;
    CNcbiOstream& x_Write(CNcbiOstream& os, TDiagWriteFlags flags = fNone) const;
};

/// Insert message in output stream.
inline CNcbiOstream& operator<< (CNcbiOstream& os, const SDiagMessage& mess) {
    return mess.Write(os);
}



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
    eDiagFilter_Trace,  //< for TRACEs only
    eDiagFilter_Post,   //< for all non-TRACE, non-FATAL
    eDiagFilter_All     //< for all non-FATAL
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
/// CStreamDiagHandler --
///
/// Specialization of "CDiagHandler" for the stream-based diagnostics.

class NCBI_XNCBI_EXPORT CStreamDiagHandler : public CDiagHandler
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
    CStreamDiagHandler(CNcbiOstream* os, bool quick_flush = true)
        : m_Stream(os), m_QuickFlush(quick_flush) {}

    /// Post message to the handler.
    virtual void Post(const SDiagMessage& mess);

    NCBI_XNCBI_EXPORT friend bool IsDiagStream(const CNcbiOstream* os);

protected:
    CNcbiOstream* m_Stream;         ///< Diagnostic stream

private:
    bool          m_QuickFlush;     ///< Quick flush of stream flag
};



/// Set diagnostic stream.
///
/// Error diagnostics are written to output stream "os".
/// This uses the SetDiagHandler() functionality.
NCBI_XNCBI_EXPORT
extern void SetDiagStream
(CNcbiOstream* os,
 bool          quick_flush  = true,///< Do stream flush after every message
 FDiagCleanup  cleanup      = 0,   ///< Call "cleanup(cleanup_data)" if diag.
 void*         cleanup_data = 0    ///< Stream is changed (see SetDiagHandler)
 );

// Return TRUE if "os" is the current diag. stream.
NCBI_XNCBI_EXPORT 
extern bool IsDiagStream(const CNcbiOstream* os);



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagFactory --
///
/// Diagnostic handler factory.

class NCBI_XNCBI_EXPORT CDiagFactory
{
public:
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

    string            m_PostPrefix;         ///< Message prefix
    list<string>      m_PrefixList;         ///< List of prefixs
    TDiagPostFlags    m_PostFlags;          ///< Post flags
    EDiagSev          m_PostSeverity;       ///< Post severity
    EDiagSevChange    m_PostSeverityChange; ///< Severity change
    EDiagSev          m_DieSeverity;        ///< Die level severity
    EDiagTrace        m_TraceDefault;       ///< Default trace setting
    bool              m_TraceEnabled;       ///< Trace enabled?
    CDiagHandler*     m_Handler;            ///< Class handler
    bool              m_CanDeleteHandler;   ///< Can handler be deleted?
    CDiagErrCodeInfo* m_ErrCodeInfo;        ///< Error code information
    bool              m_CanDeleteErrCodeInfo; 
                                       ///< Can error code info. be deleted?
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
/// @sa
///   http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/programming_manual/diag.html#errcodes
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




/*
 * ==========================================================================
 *
 * $Log$
 * Revision 1.85  2005/05/04 19:54:13  ssikorsk
 * Store internal data in std::string instead of AutoPtr within CDiagCompileInfo and CNcbiDiag.
 *
 * Revision 1.84  2005/05/04 15:26:13  ssikorsk
 * Moved getters into inl-file. Initialised m_File and m_Module with empty
 * string in ctor instead of checking for NULL in getters every time.
 *
 * Revision 1.83  2005/05/04 14:14:15  ssikorsk
 * Made CNcbiDiag dtor not inline. Added copy ctor and dtor to CDiagCompileInfo.
 *
 * Revision 1.82  2005/05/04 13:18:54  ssikorsk
 * Revamped CDiagCompileInfo and CNcbiDiag to use dynamically allocated buffers instead of predefined
 *
 * Revision 1.81  2005/04/18 14:25:58  ssikorsk
 * Report a method/function name within an error message
 *
 * Revision 1.80  2005/04/14 20:24:31  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.79  2005/03/28 20:41:43  jcherry
 * Added export specifiers
 *
 * Revision 1.78  2004/12/13 19:39:18  kuznets
 * Improved doxy comments for filtering functions
 *
 * Revision 1.77  2004/09/22 14:59:49  kononenk
 * Fix for VC++ export
 *
 * Revision 1.76  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.75  2004/09/07 18:10:49  vakatov
 * Comment fix
 *
 * Revision 1.74  2004/08/17 14:34:08  dicuccio
 * Export StrToSeverityLevel
 *
 * Revision 1.73  2004/06/15 14:12:42  vasilche
 * Fixed order of commands in LOGPOST_ONCE to reduce number of messages.
 *
 * Revision 1.72  2004/04/26 19:28:24  ucko
 * Make previous change compiler-dependent due to MSVC bugginess.
 *
 * Revision 1.71  2004/04/26 14:35:47  ucko
 * Move CNcbiDiag::operator<< to ncbidiag.inl to make GCC 3.4 happy.
 *
 * Revision 1.70  2004/03/18 20:19:48  gouriano
 * make it possible to convert multi-line diagnostic message into single-line
 *
 * Revision 1.69  2004/03/16 15:35:19  vasilche
 * Added LOG_POST_ONCE and ERR_POST_ONCE macros
 *
 * Revision 1.68  2004/03/12 21:16:56  gorelenk
 * Added NCBI_XNCBI_EXPORT to member-function SetFile of class CNcbiDiag.
 *
 * Revision 1.67  2004/03/10 18:40:21  gorelenk
 * Added/Removed NCBI_XNCBI_EXPORT prefixes.
 *
 * Revision 1.66  2004/03/10 17:34:05  gorelenk
 * Removed NCBI_XNCBI_EXPORT prefix for classes members-functions
 * that are implemented as a inline functions.
 *
 * Revision 1.65  2004/01/28 14:49:04  ivanov
 * Added export specifier to SDiagErrCodeDescription
 *
 * Revision 1.64  2004/01/27 17:04:10  ucko
 * Don't attempt to declare references as mutable -- the referents are fixed,
 * and one can always change their contents through any non-const reference.
 * (IBM's VisualAge compiler forbids such declarations.)
 * Add missing declarations for SetDiagFixedPostLevel and IsSetDiagErrCodeInfo.
 *
 * Revision 1.63  2003/11/12 20:30:25  ucko
 * Make extra flags for severity-trace messages tunable.
 *
 * Revision 1.62  2003/11/07 13:04:45  ivanov
 * Added export specifier for SDiagMessage
 *
 * Revision 1.61  2003/11/06 21:40:34  vakatov
 * A somewhat more natural handling of the 'eDPF_Default' flag -- replace
 * it by the current global flags, then merge these with other flags (if any)
 *
 * Revision 1.60  2003/08/01 14:16:27  siyan
 * Fixed error in CDiagFactory documentation.
 *
 * Revision 1.59  2003/07/21 18:42:29  siyan
 * Documentation changes.
 *
 * Revision 1.58  2003/04/25 20:52:34  lavr
 * Introduce draft version of IgnoreDiagDieLevel()
 *
 * Revision 1.57  2003/04/25 16:00:03  vakatov
 * Document an ugly, absolutely not recommended for use, very special case
 * for SetDiagDieLevel() which prevents the application from exiting when
 * an error with FATAL severity is posted.
 *
 * Revision 1.56  2003/04/11 17:55:44  lavr
 * Proper indentation of some fragments
 *
 * Revision 1.55  2003/03/31 15:36:51  siyan
 * Added doxygen support
 *
 * Revision 1.54  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.53  2002/09/24 18:28:20  vasilche
 * Fixed behavour of CNcbiDiag::DiagValidate() in release mode
 *
 * Revision 1.52  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.51  2002/09/16 20:49:06  vakatov
 * Cosmetics and comments
 *
 * Revision 1.50  2002/08/20 19:13:09  gouriano
 * added DiagWriteFlags into SDiagMessage::Write
 *
 * Revision 1.49  2002/08/16 15:02:50  ivanov
 * Added class CDiagAutoPrefix
 *
 * Revision 1.48  2002/08/01 18:48:07  ivanov
 * Added stuff to store and output error verbose messages for error codes
 *
 * Revision 1.47  2002/07/15 18:17:50  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.46  2002/07/10 16:18:42  ivanov
 * Added CNcbiDiag::StrToSeverityLevel().
 * Rewrite and rename SetDiagFixedStrPostLevel() -> SetDiagFixedPostLevel()
 *
 * Revision 1.45  2002/07/09 16:38:00  ivanov
 * Added GetSeverityChangeEnabledFirstTime().
 * Fix usage forced set severity post level from environment variable
 * to work without NcbiApplication::AppMain()
 *
 * Revision 1.44  2002/07/02 18:26:08  ivanov
 * Added CDiagBuffer::DisableDiagPostLevelChange()
 *
 * Revision 1.43  2002/06/26 18:36:37  gouriano
 * added CNcbiException class
 *
 * Revision 1.42  2002/04/23 19:57:25  vakatov
 * Made the whole CNcbiDiag class "mutable" -- it helps eliminate
 * numerous warnings issued by SUN Forte6U2 compiler.
 * Do not use #NO_INCLASS_TMPL anymore -- apparently all modern
 * compilers seem to be supporting in-class template methods.
 *
 * Revision 1.41  2002/04/16 18:38:02  ivanov
 * SuppressDiagPopupMessages() moved to "test/test_assert.h"
 *
 * Revision 1.40  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.39  2002/04/11 19:58:03  ivanov
 * Added function SuppressDiagPopupMessages()
 *
 * Revision 1.38  2002/04/10 14:45:04  ivanov
 * Abort() moved from static to extern and added to header file
 *
 * Revision 1.37  2002/04/01 22:34:35  ivanov
 * Added SetAbortHandler() function to set user abort handler
 *
 * Revision 1.36  2002/02/07 19:45:53  ucko
 * Optionally transfer ownership in GetDiagHandler.
 *
 * Revision 1.35  2001/11/14 15:14:58  ucko
 * Revise diagnostic handling to be more object-oriented.
 *
 * Revision 1.34  2001/10/29 15:37:29  ucko
 * Rewrite dummy new/delete bodies to avoid GCC warnings.
 *
 * Revision 1.33  2001/10/29 15:25:55  ucko
 * Give (prohibited) CDiagRestorer::new/delete dummy bodies.
 *
 * Revision 1.32  2001/10/29 15:16:11  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.31  2001/10/16 23:44:04  vakatov
 * + SetDiagPostAllFlags()
 *
 * Revision 1.30  2001/08/09 16:24:05  lavr
 * Added eDPF_OmitInfoSev to log message format flags
 *
 * Revision 1.29  2001/07/26 21:28:49  lavr
 * Remove printing DateTime stamp by default
 *
 * Revision 1.28  2001/07/25 19:12:41  lavr
 * Added date/time stamp for message logging
 *
 * Revision 1.27  2001/06/14 03:37:28  vakatov
 * For the ErrCode manipulator, use CNcbiDiag:: method rather than a friend
 *
 * Revision 1.26  2001/06/13 23:19:36  vakatov
 * Revamped previous revision (prefix and error codes)
 *
 * Revision 1.25  2001/06/13 20:51:52  ivanov
 * + PushDiagPostPrefix(), PopPushDiagPostPrefix() - stack post prefix messages.
 * + ERR_POST_EX, LOG_POST_EX - macros for posting with error codes.
 * + ErrCode(code[,subcode]) - CNcbiDiag error code manipulator.
 * + eDPF_ErrCode, eDPF_ErrSubCode - new post flags.
 *
 * Revision 1.24  2001/05/17 14:50:58  lavr
 * Typos corrected
 *
 * Revision 1.23  2000/11/29 16:58:23  vakatov
 * Added LOG_POST() macro -- to print the posted message only
 *
 * Revision 1.22  2000/10/24 19:54:44  vakatov
 * Diagnostics to go to CERR by default (was -- disabled by default)
 *
 * Revision 1.21  2000/04/18 19:50:10  vakatov
 * Get rid of the old-fashioned C-like "typedef enum {...} E;"
 *
 * Revision 1.20  2000/04/04 22:31:57  vakatov
 * SetDiagTrace() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.19  2000/03/10 14:17:40  vasilche
 * Added missing namespace specifier to macro.
 *
 * Revision 1.18  2000/02/18 16:54:02  vakatov
 * + eDiag_Critical
 *
 * Revision 1.17  2000/01/20 16:52:29  vakatov
 * SDiagMessage::Write() to replace SDiagMessage::Compose()
 * + operator<<(CNcbiOstream& os, const SDiagMessage& mess)
 * + IsSetDiagHandler(), IsDiagStream()
 *
 * Revision 1.16  1999/12/29 22:30:22  vakatov
 * Use "exit()" rather than "abort()" in non-#_DEBUG mode
 *
 * Revision 1.15  1999/12/28 18:55:24  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.14  1999/12/27 19:44:15  vakatov
 * Fixes for R1.13:
 * ERR_POST() -- use eDPF_Default rather than eDPF_Trace;  forcibly flush
 * ("<< Endm") the diag. stream. Get rid of the eDPF_CopyFilename, always
 * make a copy of the file name.
 *
 * Revision 1.13  1999/09/27 16:23:20  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.12  1999/05/04 00:03:06  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.11  1999/04/30 19:20:57  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.10  1999/03/15 16:08:09  vakatov
 * Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
 *
 * Revision 1.9  1999/03/12 18:04:06  vakatov
 * Added ERR_POST macro to perform a plain "standard" error posting
 *
 * Revision 1.8  1998/12/30 21:52:16  vakatov
 * Fixed for the new SunPro 5.0 beta compiler that does not allow friend
 * templates and member(in-class) templates
 *
 * Revision 1.7  1998/12/28 17:56:27  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.6  1998/11/06 22:42:37  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * Revision 1.5  1998/11/04 23:46:35  vakatov
 * Fixed the "ncbidbg/diag" header circular dependencies
 *
 * Revision 1.4  1998/11/03 20:51:24  vakatov
 * Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
 *
 * Revision 1.3  1998/10/30 20:08:20  vakatov
 * Fixes to (first-time) compile and test-run on MSVS++
 *
 * Revision 1.2  1998/10/27 23:06:58  vakatov
 * Use NCBI C++ interface to iostream's
 * ==========================================================================
 */

#endif  /* CORELIB___NCBIDIAG__HPP */
