#ifndef NETCACHE__SRV_LIB__HPP
#define NETCACHE__SRV_LIB__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *   This header contains special definitions of different classes, enums and
 *   macros which are used by sources included in srv_lib.cpp but which don't
 *   have definitions included. Many classes and macros are just stubs to make
 *   code compilable (this code should never be called in run time) and some
 *   are modified to use TaskServer infrastructure instead of corelib.
 */


BEGIN_NCBI_SCOPE


class CDiagCompileInfo;
class CRequestContext;
struct SSystemMutex;


#define xncbi_Validate(expr, msg)   \
    if (!(expr))                    \
        SRV_LOG(Fatal, msg)         \
/**/

#define xncbi_Verify(expression)    assert(expression)

#ifdef _DEBUG
# define _DEBUG_ARG(arg) arg
#else
# define _DEBUG_ARG(arg)
#endif

class CDiagCompileInfo
{
public:
    CDiagCompileInfo(void);
    CDiagCompileInfo(const char* file,
                     int line,
                     const char* func);
    CDiagCompileInfo(const string& file,
                     int           line,
                     const string& curr_funct,
                     const string& module);
    ~CDiagCompileInfo(void);

    const char* GetFile(void) const
    { return m_File; }
    int GetLine(void) const 
    { return m_Line; }
    const char* GetModule(void) const
    { return ""; }
    const string& GetClass(void) const
    { 
        if (!m_Parsed) {
            ParseCurrFunctName();
        }
        return m_ClassName;
    }
    const string& GetFunction(void) const
    { 
        if (!m_Parsed) {
            ParseCurrFunctName();
        }
        return m_FunctName;
    }

private:
    void ParseCurrFunctName(void) const;

private:
    const char*    m_File;
    int            m_Line;

    const   char*  m_CurrFunctName;
    mutable bool   m_Parsed;
    mutable string m_ClassName;
    mutable string m_FunctName;

    // Storage for data passed as strings rather than char*.
    char*          m_StrFile;
    char*          m_StrCurrFunctName;
};

#ifdef NCBI_COMPILER_GCC
# define NCBI_CURRENT_FUNCTION  __PRETTY_FUNCTION__
#else
# define NCBI_CURRENT_FUNCTION  ""
#endif

#define DIAG_COMPILE_INFO   CDiagCompileInfo(__FILE__, __LINE__,    \
                                             NCBI_CURRENT_FUNCTION) \
/**/


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

    /// Send the message to 'console' regardless of it's severity.
    /// To be set by 'Console' manipulator only.
    eDPF_IsConsole          = 0x8000000,

    /// Use global default flags (merge with).
    /// @sa SetDiagPostFlag(), UnsetDiagPostFlag(), IsSetDiagPostFlag()
    eDPF_Default            = 0x10000000,

    /// Important bits which should be taken from the globally set flags
    /// even if a user attempts to override (or forgets to set) them
    /// when calling CNcbiDiag().
    eDPF_ImportantFlagsMask = eDPF_PreMergeLines |
                              eDPF_MergeLines |
                              eDPF_OmitInfoSev |
                              eDPF_OmitSeparator |
                              eDPF_AtomicWrite,

    /// Use flags provided by user as-is, do not allow CNcbiDiag to replace
    /// "important" flags by the globally set ones.
    eDPF_UseExactUserFlags  = 0x20000000
};

typedef int TDiagPostFlags;  ///< Binary OR of "EDiagPostFlag"

/// Application execution states shown in the std prefix
enum EDiagAppState {
    eDiagAppState_NotSet,        ///< Reserved value, never used in messages
    eDiagAppState_AppBegin,      ///< AB
    eDiagAppState_AppRun,        ///< A
    eDiagAppState_AppEnd,        ///< AE
    eDiagAppState_RequestBegin,  ///< RB
    eDiagAppState_Request,       ///< R
    eDiagAppState_RequestEnd     ///< RE
};


struct SDiagMessage
{
    typedef Uint8 TCount;
    enum EFlags {
        fNoEndl = 1,
        fNoPrefix = 2
    };
    CNcbiOstream& Write(CNcbiOstream& os, int flags = 0) const;

    SDiagMessage(EDiagSev severity, const char* buf, size_t len,
                 const char* file = 0, size_t line = 0,
                 TDiagPostFlags flags = eDPF_Default, const char* prefix = 0,
                 int err_code = 0, int err_subcode = 0,
                 const char* err_text  = 0,
                 const char* module    = 0,
                 const char* nclass    = 0,
                 const char* func      = 0);

    EDiagSev severity;
    const char* buf;
    size_t len;
    const char* file;
    size_t line;
    TDiagPostFlags flags;
    const char* prefix;
    int err_code;
    int err_subcode;
    const char* err_text;
    const char* module;
    const char* nclass;
    const char* func;
};


class CDiagContext {
public:
    static void StartCtxRequest(CRequestContext* ctx);
    static void StopCtxRequest(CRequestContext* ctx);
    static bool IsCtxRunning(CRequestContext* ctx);

public:
    enum EDefaultHitIDFlag {
        eHitID_NoCreate, // Do not create new hit id.
        eHitID_Create    // Create new hit id if it's not yet set.
    };
    enum FOnForkAction {
        fOnFork_PrintStart = 1 << 0,   ///< Log app-start.
        fOnFork_ResetTimer = 1 << 1    ///< Reset execution timer.
    };
    typedef int TOnForkFlags;
    static void UpdateOnFork(TOnForkFlags flags);

    static void UpdatePID(void);

    static const string& GetDefaultSessionID(void);
    static const string& GetEncodedSessionID(void);
    static string GetNextHitID(void) { return string(); }
    static Int8 UpdateUID(void) { return 0; }
    static string GetStringUID(Int8) { return string(); }
    static EDiagAppState GetGlobalAppState(void) { return eDiagAppState_AppRun; }

    CDiagContext& Extra(void) {
        return *this;
    }
    CDiagContext& Print( const string& s1, const string& s2) {
        return *this;
    }
    void PrintStop(void) {
    }
    const string& GetDefaultHitID(void) { return m_hid; }
    const string& x_GetDefaultHitID(CDiagContext::EDefaultHitIDFlag) { return m_hid; }
private:
    string m_hid;
};

CDiagContext& GetDiagContext(void);
inline int CompareDiagPostLevel(EDiagSev, EDiagSev)
{ return -1; }


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

enum EOldStyleSeverity {
    Trace,
    Info,
    Warning,
    Error,
    Critical,
    Fatal
};

class CStackTrace
{
public:
    CStackTrace(void)
    {}
    bool Empty(void)
    { return true; }
};

inline CStackTrace* StackTrace(void)
{
    return NULL;
}

inline CStackTrace* Endm(void)
{
    return NULL;
}


#define NCBI_DEFINE_ERRCODE_X(name, err_code, max_err_subcode)  \
    namespace err_code_x {                                      \
        enum {                                                  \
            eErrCodeX_##name     = err_code                     \
        };                                                      \
    }                                                           \
    extern void err_code_x__dummy_for_semicolon(void)           \
/**/

#define NCBI_ERRCODE_X_NAME(name)   \
    NCBI_NS_NCBI::err_code_x::NCBI_NAME2(eErrCodeX_, name)

#define NCBI_ERRCODE_X   NCBI_ERRCODE_X_NAME(NCBI_USE_ERRCODE_X)

#define NCBI_CHECK_ERR_SUBCODE_X_NAME(err_name, err_subcode)

#define ERR_POST(msg)   \
    CSrvDiagMsg().StartOldStyle(__FILE__, __LINE__, NCBI_CURRENT_FUNCTION) << msg

#define ERR_FATAL(msg)   \
    do { ERR_POST(Fatal<<msg); abort(); } while(0)

#define LOG_POST(msg)   ERR_POST(msg)

#define ERR_POST_ONCE(msg)  ERR_POST(msg)

#define ERR_POST_EX(err_code, err_subcode, msg)                             \
    CSrvDiagMsg().StartOldStyle(__FILE__, __LINE__, NCBI_CURRENT_FUNCTION)  \
                  << ErrCode(err_code, err_subcode) << msg                  \
/**/

#define ERR_FATAL_EX(err_code, err_subcode, msg)        \
    do { ERR_POST_EX(err_code, err_subcode, Fatal<<msg); abort(); } while(0)

#define LOG_POST_EX(err_code, err_subcode, msg)     \
    ERR_POST_EX(err_code, err_subcode, msg)

#define ERR_POST_XX(error_name, err_subcode, message)   \
    ERR_POST_EX(NCBI_ERRCODE_X_NAME(error_name), err_subcode, message)

#define ERR_FATAL_XX(error_name, err_subcode, message)   \
    ERR_FATAL_EX(NCBI_ERRCODE_X_NAME(error_name), err_subcode, message)

#define LOG_POST_XX(error_name, err_subcode, message)   \
    ERR_POST_XX(error_name, err_subcode, message)

#define ERR_POST_X(err_subcode, message)    \
    ERR_POST_XX(NCBI_USE_ERRCODE_X, err_subcode, message)

#define ERR_FATAL_X(err_subcode, message)    \
    ERR_FATAL_XX(NCBI_USE_ERRCODE_X, err_subcode, message)

#define LOG_POST_X(err_subcode, message)    \
    ERR_POST_X(err_subcode, message)

#define ERR_POST_X_ONCE(err_subcode, msg)  ERR_POST_X(err_subcode, msg)


#define _TRACE(msg) do {} while (0)
#define NCBI_TROUBLE(msg) SRV_FATAL(msg)
#define _TROUBLE          SRV_FATAL("")
#define Abort       abort


class CSrvDiagMsg;
class CStackTrace;

const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, EOldStyleSeverity sev);
const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, ErrCode err_code);
inline
const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, CStackTrace* (*)(void))
{
    return msg;
}
inline
const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, const CStackTrace&)
{
    return msg;
}
inline
const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, const CSrvDiagMsg& (*manip)(const CSrvDiagMsg&))
{
    return manip(msg);
}


class CNcbiDiag : public CSrvDiagMsg
{
public:
    CNcbiDiag(const CDiagCompileInfo& info, EDiagSev sev, TDiagPostFlags flags = 0)
    {
        StartOldStyle(info.GetFile(), info.GetLine(), info.GetFunction().c_str());
        if (sev == eDiag_Trace)
            operator<< (Trace);
        else
            operator<< (EOldStyleSeverity(int(sev) - 1));
    }
    CNcbiDiag(void)
    {
        StartOldStyle(__FILE__, __LINE__, NCBI_CURRENT_FUNCTION);
    }

    bool CheckFilters(void)
    { return true; }
};

inline void Reset(const CNcbiDiag&)
{}


class CNcbiEnvironment;
class CNcbiArguments;


struct SDummyConfig
{
    string Get(const char*, const char*)
    { return string(); }
};

class CNcbiApplication
{
public:
    static CNcbiApplication* Instance(void)
    { return NULL; }
    bool HasLoadedConfig(void)
    { return false; }
    bool FinishedLoadingConfig(void)
    { return false; }
    SDummyConfig GetConfig(void)
    { return SDummyConfig(); }
    CNcbiEnvironment& SetEnvironment(void)
    { return *(CNcbiEnvironment*)Instance(); }
    const CNcbiArguments& GetArguments(void)
    { return *(CNcbiArguments*)Instance(); }

    enum ENameType {
        eFullName,
        eRealName
    };
    static string GetAppName(ENameType)
    { return string(); }
    static SSystemMutex& GetInstanceMutex(void);
};


class CObject;

class CObjectMemoryPool
{
public:
    void* Allocate(size_t size)
    { return NULL; }
    void Deallocate(void* ptr)
    {}
    static void Delete(CObject* obj)
    {}
};

#define DECLARE_TLS_VAR(type, var) NCBI_TLS_VAR type var



template <typename T>
class CStaticTls
{
public:
    T* GetValue(void)
    { return NULL; }
    typedef void (*FCleanup)(T* value, void* cleanup_data);
    void SetValue(T* value, FCleanup cleanup = NULL)
    {}
};

#ifdef NCBI_OS_MSWIN

class CWinSecurity
{
public:
    static bool GetFilePermissions(const string& path, ACCESS_MASK* permissions)
    { return false; }
    static bool GetFileOwner(const string& filename,
                             string* owner, string* group = 0,
                             unsigned int* uid = 0, unsigned int* gid = 0)
    { return false; }
    static bool SetFileOwner(const string& filename, const string& owner,
                             const string& group = "",
                             unsigned int* uid = 0, unsigned int* gid = 0)
    { return false; }
};

#endif

END_NCBI_SCOPE

#endif /* NETCACHE__SRV_LIB__HPP */
