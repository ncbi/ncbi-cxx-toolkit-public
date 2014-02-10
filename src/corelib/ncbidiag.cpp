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
 * Authors:  Denis Vakatov et al.
 *
 * File Description:
 *   NCBI C++ diagnostic API
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/syslog.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/request_control.hpp>
#include "ncbidiag_p.hpp"
#include "ncbisys.hpp"
#include <fcntl.h>
#include <stdlib.h>
#include <stack>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/utsname.h>
#endif

#ifdef NCBI_OS_LINUX
# include <sys/prctl.h>
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Diag


BEGIN_NCBI_SCOPE

static bool s_DiagUseRWLock = false;
DEFINE_STATIC_MUTEX(s_DiagMutex);
static CSafeStatic<CRWLock> s_DiagRWLock;
static CSafeStatic<CAtomicCounter_WithAutoInit> s_ReopenEntered;

DEFINE_STATIC_FAST_MUTEX(s_ApproveMutex);


void g_Diag_Use_RWLock(void)
{
    if (s_DiagUseRWLock) return; // already switched
    // Try to detect at least some dangerous situations.
    // This does not guarantee safety since some thread may
    // be waiting for the mutex while we switch to RW-lock.
    bool diag_mutex_unlocked = s_DiagMutex.TryLock();
    // Mutex is locked - fail
    _ASSERT(diag_mutex_unlocked);
    if (!diag_mutex_unlocked) {
        _TROUBLE;
        NCBI_THROW(CCoreException, eCore,
                   "Cannot switch diagnostic to RW-lock - mutex is locked.");
    }
    s_DiagUseRWLock = true;
    s_DiagMutex.Unlock();
}


class CDiagLock
{
public:
    enum ELockType {
        eRead,   // read lock
        eWrite,  // write lock (modifying flags etc.)
        ePost    // lock used by diag handlers to lock stream in Post()
    };

    CDiagLock(ELockType locktype)
        : m_UsedRWLock(false), m_LockType(locktype)
    {
        if (s_DiagUseRWLock) {
            if (m_LockType == eRead) {
                m_UsedRWLock = true;
                s_DiagRWLock->ReadLock();
                return;
            }
            if (m_LockType == eWrite) {
                m_UsedRWLock = true;
                s_DiagRWLock->WriteLock();
                return;
            }
            // For ePost use normal mutex below.
        }
        s_DiagMutex.Lock();
    }

    ~CDiagLock(void)
    {
        if (m_UsedRWLock) {
            s_DiagRWLock->Unlock();
        }
        else {
            s_DiagMutex.Unlock();
        }
    }

private:
    bool      m_UsedRWLock;
    ELockType m_LockType;
};


class CDiagFileHandleHolder : public CObject
{
public:
    CDiagFileHandleHolder(const string& fname, CDiagHandler::TReopenFlags flags);
    virtual ~CDiagFileHandleHolder(void);

    int GetHandle(void) { return m_Handle; }

private:
    int m_Handle;
};


// Special diag handler for duplicating error log messages on stderr.
class CTeeDiagHandler : public CDiagHandler
{
public:
    CTeeDiagHandler(CDiagHandler* orig, bool own_orig);
    virtual void Post(const SDiagMessage& mess);

    // Don't post duplicates to console.
    virtual void PostToConsole(const SDiagMessage& mess) {}

    virtual string GetLogName(void)
    {
        return m_OrigHandler.get() ?
            m_OrigHandler->GetLogName() : "STDERR-TEE";
    }
    virtual void Reopen(TReopenFlags flags)
    {
        if ( m_OrigHandler.get() ) {
            m_OrigHandler->Reopen(flags);
        }
    }

    CDiagHandler* GetOriginalHandler(void) const
    {
        return m_OrigHandler.get();
    }

private:
    EDiagSev              m_MinSev;
    AutoPtr<CDiagHandler> m_OrigHandler;
};


#if defined(NCBI_POSIX_THREADS) && defined(HAVE_PTHREAD_ATFORK)

#include <unistd.h> // for pthread_atfork()

extern "C" {
    static void s_NcbiDiagPreFork(void)
    {
        s_DiagMutex.Lock();
    }
    static void s_NcbiDiagPostFork(void)
    {
        s_DiagMutex.Unlock();
    }
}

#endif


///////////////////////////////////////////////////////
//  Output format parameters

// Use old output format if the flag is set
NCBI_PARAM_DECL(bool, Diag, Old_Post_Format);
NCBI_PARAM_DEF_EX(bool, Diag, Old_Post_Format, true, eParam_NoThread,
                  DIAG_OLD_POST_FORMAT);
typedef NCBI_PARAM_TYPE(Diag, Old_Post_Format) TOldPostFormatParam;

// Auto-print context properties on set/change.
NCBI_PARAM_DECL(bool, Diag, AutoWrite_Context);
NCBI_PARAM_DEF_EX(bool, Diag, AutoWrite_Context, false, eParam_NoThread,
                  DIAG_AUTOWRITE_CONTEXT);
typedef NCBI_PARAM_TYPE(Diag, AutoWrite_Context) TAutoWrite_Context;

// Print system TID rather than CThread::GetSelf()
NCBI_PARAM_DECL(bool, Diag, Print_System_TID);
NCBI_PARAM_DEF_EX(bool, Diag, Print_System_TID, false, eParam_NoThread,
                  DIAG_PRINT_SYSTEM_TID);
typedef NCBI_PARAM_TYPE(Diag, Print_System_TID) TPrintSystemTID;

// Use assert() instead of abort() when printing fatal errors
// to show the assertion dialog and allow to choose the action
// (stop/debug/continue).
NCBI_PARAM_DECL(bool, Diag, Assert_On_Abort);
NCBI_PARAM_DEF_EX(bool, Diag, Assert_On_Abort, false, eParam_NoThread,
                  DIAG_ASSERT_ON_ABORT);
typedef NCBI_PARAM_TYPE(Diag, Assert_On_Abort) TAssertOnAbortParam;

// Limit log file size, rotate log when it reaches the limit.
NCBI_PARAM_DECL(long, Diag, Log_Size_Limit);
NCBI_PARAM_DEF_EX(long, Diag, Log_Size_Limit, 0, eParam_NoThread,
                  DIAG_LOG_SIZE_LIMIT);
typedef NCBI_PARAM_TYPE(Diag, Log_Size_Limit) TLogSizeLimitParam;


///////////////////////////////////////////////////////
//  Output rate control parameters

// AppLog limit per period
NCBI_PARAM_DECL(unsigned int, Diag, AppLog_Rate_Limit);
NCBI_PARAM_DEF_EX(unsigned int, Diag, AppLog_Rate_Limit, 50000,
                  eParam_NoThread, DIAG_APPLOG_RATE_LIMIT);
typedef NCBI_PARAM_TYPE(Diag, AppLog_Rate_Limit) TAppLogRateLimitParam;

// AppLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, AppLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, AppLog_Rate_Period, 10, eParam_NoThread,
                  DIAG_APPLOG_RATE_PERIOD);
typedef NCBI_PARAM_TYPE(Diag, AppLog_Rate_Period) TAppLogRatePeriodParam;

// ErrLog limit per period
NCBI_PARAM_DECL(unsigned int, Diag, ErrLog_Rate_Limit);
NCBI_PARAM_DEF_EX(unsigned int, Diag, ErrLog_Rate_Limit, 5000,
                  eParam_NoThread, DIAG_ERRLOG_RATE_LIMIT);
typedef NCBI_PARAM_TYPE(Diag, ErrLog_Rate_Limit) TErrLogRateLimitParam;

// ErrLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, ErrLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, ErrLog_Rate_Period, 1, eParam_NoThread,
                  DIAG_ERRLOG_RATE_PERIOD);
typedef NCBI_PARAM_TYPE(Diag, ErrLog_Rate_Period) TErrLogRatePeriodParam;

// TraceLog limit per period
NCBI_PARAM_DECL(unsigned int, Diag, TraceLog_Rate_Limit);
NCBI_PARAM_DEF_EX(unsigned int, Diag, TraceLog_Rate_Limit, 5000,
                  eParam_NoThread, DIAG_TRACELOG_RATE_LIMIT);
typedef NCBI_PARAM_TYPE(Diag, TraceLog_Rate_Limit) TTraceLogRateLimitParam;

// TraceLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, TraceLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, TraceLog_Rate_Period, 1, eParam_NoThread,
                  DIAG_TRACELOG_RATE_PERIOD);
typedef NCBI_PARAM_TYPE(Diag, TraceLog_Rate_Period) TTraceLogRatePeriodParam;

// Duplicate messages to STDERR
NCBI_PARAM_DECL(bool, Diag, Tee_To_Stderr);
NCBI_PARAM_DEF_EX(bool, Diag, Tee_To_Stderr, false, eParam_NoThread,
                  DIAG_TEE_TO_STDERR);
typedef NCBI_PARAM_TYPE(Diag, Tee_To_Stderr) TTeeToStderr;

// Minimum severity of the messages duplicated to STDERR
NCBI_PARAM_ENUM_DECL(EDiagSev, Diag, Tee_Min_Severity);
NCBI_PARAM_ENUM_ARRAY(EDiagSev, Diag, Tee_Min_Severity)
{
    {"Info", eDiag_Info},
    {"Warning", eDiag_Warning},
    {"Error", eDiag_Error},
    {"Critical", eDiag_Critical},
    {"Fatal", eDiag_Fatal},
    {"Trace", eDiag_Trace}
};

const EDiagSev kTeeMinSeverityDef =
#if defined(NDEBUG)
    eDiag_Error;
#else
    eDiag_Warning;
#endif

NCBI_PARAM_ENUM_DEF_EX(EDiagSev, Diag, Tee_Min_Severity,
                       kTeeMinSeverityDef,
                       eParam_NoThread, DIAG_TEE_MIN_SEVERITY);
typedef NCBI_PARAM_TYPE(Diag, Tee_Min_Severity) TTeeMinSeverity;


NCBI_PARAM_DECL(size_t, Diag, Collect_Limit);
NCBI_PARAM_DEF_EX(size_t, Diag, Collect_Limit, 1000, eParam_NoThread,
                  DIAG_COLLECT_LIMIT);
typedef NCBI_PARAM_TYPE(Diag, Collect_Limit) TDiagCollectLimit;


NCBI_PARAM_DECL(bool, Log, Truncate);
NCBI_PARAM_DEF_EX(bool, Log, Truncate, false, eParam_NoThread, LOG_TRUNCATE);
typedef NCBI_PARAM_TYPE(Log, Truncate) TLogTruncateParam;


NCBI_PARAM_DECL(bool, Log, NoCreate);
NCBI_PARAM_DEF_EX(bool, Log, NoCreate, false, eParam_NoThread, LOG_NOCREATE);
typedef NCBI_PARAM_TYPE(Log, NoCreate) TLogNoCreate;


// Logging of environment variables: space separated list of names which
// should be logged after each request start.
NCBI_PARAM_DECL(string, Log, LogEnvironment);
NCBI_PARAM_DEF_EX(string, Log, LogEnvironment, kEmptyStr,
                  eParam_NoThread,
                  DIAG_LOG_ENVIRONMENT);
typedef NCBI_PARAM_TYPE(Log, LogEnvironment) TLogEnvironment;


// Logging of registry values: space separated list of 'section:name' strings.
NCBI_PARAM_DECL(string, Log, LogRegistry);
NCBI_PARAM_DEF_EX(string, Log, LogRegistry, kEmptyStr,
                  eParam_NoThread,
                  DIAG_LOG_REGISTRY);
typedef NCBI_PARAM_TYPE(Log, LogRegistry) TLogRegistry;


static bool s_UseRootLog = true;
static bool s_FinishedSetupDiag = false;
static bool s_MergeLinesSetBySetupDiag = false;


CDiagCollectGuard::CDiagCollectGuard(void)
{
    // the severities will be adjusted by x_Init()
    x_Init(eDiag_Critical, eDiag_Fatal, eDiscard);
}


CDiagCollectGuard::CDiagCollectGuard(EDiagSev print_severity)
{
    // the severities will be adjusted by x_Init()
    x_Init(eDiag_Critical, print_severity, eDiscard);
}


CDiagCollectGuard::CDiagCollectGuard(EDiagSev print_severity,
                                     EDiagSev collect_severity,
                                     EAction  action)
{
    // the severities will be adjusted by x_Init()
    x_Init(print_severity, collect_severity, action);
}


void CDiagCollectGuard::x_Init(EDiagSev print_severity,
                               EDiagSev collect_severity,
                               EAction  action)
{
    // Get current print severity
    EDiagSev psev, csev;
    CDiagContextThreadData& thr_data =
        CDiagContextThreadData::GetThreadData();
    if ( thr_data.GetCollectGuard() ) {
        psev = thr_data.GetCollectGuard()->GetPrintSeverity();
        csev = thr_data.GetCollectGuard()->GetCollectSeverity();
    }
    else {
        CDiagLock lock(CDiagLock::eRead);
        psev = CDiagBuffer::sm_PostSeverity;
        csev = CDiagBuffer::sm_PostSeverity;
    }
    psev = CompareDiagPostLevel(psev, print_severity) > 0
        ? psev : print_severity;
    csev = CompareDiagPostLevel(csev, collect_severity) < 0
        ? csev : collect_severity;

    m_PrintSev = psev;
    m_CollectSev = csev;
    m_Action = action;
    thr_data.AddCollectGuard(this);
}


CDiagCollectGuard::~CDiagCollectGuard(void)
{
    Release();
}


void CDiagCollectGuard::Release(void)
{
    CDiagContextThreadData& thr_data = CDiagContextThreadData::GetThreadData();
    thr_data.RemoveCollectGuard(this);
}


void CDiagCollectGuard::Release(EAction action)
{
    SetAction(action);
    Release();
}


void CDiagCollectGuard::SetPrintSeverity(EDiagSev sev)
{
    if ( CompareDiagPostLevel(m_PrintSev, sev) < 0 ) {
        m_PrintSev = sev;
    }
}


void CDiagCollectGuard::SetCollectSeverity(EDiagSev sev)
{
    if ( CompareDiagPostLevel(m_CollectSev, sev) < 0 ) {
        m_CollectSev = sev;
    }
}


///////////////////////////////////////////////////////
//  Static variables for Trace and Post filters

static CSafeStatic<CDiagFilter> s_TraceFilter;
static CSafeStatic<CDiagFilter> s_PostFilter;


// Analogue to strstr.
// Returns a pointer to the last occurrence of a search string in a string
const char* 
str_rev_str(const char* begin_str, const char* end_str, const char* str_search)
{
    if (begin_str == NULL)
        return NULL;
    if (end_str == NULL)
        return NULL;
    if (str_search == NULL)
        return NULL;

    const char* search_char = str_search + strlen(str_search);
    const char* cur_char = end_str;

    do {
        --search_char;
        do {
            --cur_char;
        } while(*cur_char != *search_char && cur_char != begin_str);
        if (*cur_char != *search_char)
            return NULL; 
    }
    while (search_char != str_search);
    
    return cur_char;
}



/////////////////////////////////////////////////////////////////////////////
/// CDiagCompileInfo::

CDiagCompileInfo::CDiagCompileInfo(void)
    : m_File(""),
      m_Module(""),
      m_Line(0),
      m_CurrFunctName(0),
      m_Parsed(false),
      m_ClassSet(false)
{
}

CDiagCompileInfo::CDiagCompileInfo(const char* file, 
                                   int         line, 
                                   const char* curr_funct, 
                                   const char* module)
    : m_File(file),
      m_Module(""),
      m_Line(line),
      m_CurrFunctName(curr_funct),
      m_Parsed(false),
      m_ClassSet(false)
{
    if (!file) {
        m_File = "";
        return;
    }
    if (!module)
        return;
    if ( x_NeedModule() && 0 != strcmp(module, "NCBI_MODULE") ) {
        m_Module = module;
    }
}


CDiagCompileInfo::CDiagCompileInfo(const string& file,
                                   int           line,
                                   const string& curr_funct,
                                   const string& module)
    : m_File(""),
      m_Module(""),
      m_Line(line),
      m_CurrFunctName(""),
      m_Parsed(false),
      m_ClassSet(false)
{
    SetFile(file);
    if ( m_File  &&  !module.empty()  &&  x_NeedModule() ) {
        SetModule(module);
    }
    SetFunction(curr_funct);
}


bool CDiagCompileInfo::x_NeedModule(void) const
{
    // Check for a file extension without creating of temporary string objects
    const char* cur_extension = strrchr(m_File, '.');
    if (cur_extension == NULL)
        return false; 

    if (*(cur_extension + 1) != '\0') {
        ++cur_extension;
    } else {
        return false;
    }

    return strcmp(cur_extension, "cpp") == 0 ||
        strcmp(cur_extension, "C") == 0 ||
        strcmp(cur_extension, "c") == 0 ||
        strcmp(cur_extension, "cxx") == 0;
}


CDiagCompileInfo::~CDiagCompileInfo(void)
{
}


void CDiagCompileInfo::SetFile(const string& file)
{
    m_StrFile = file;
    m_File = m_StrFile.c_str();
}


void CDiagCompileInfo::SetModule(const string& module)
{
    m_StrModule = module;
    m_Module = m_StrModule.c_str();
}


void CDiagCompileInfo::SetLine(int line)
{
    m_Line = line;
}


void CDiagCompileInfo::SetFunction(const string& func)
{
    m_Parsed = false;
    m_StrCurrFunctName = func;
    if (m_StrCurrFunctName.find(')') == NPOS) {
        m_StrCurrFunctName += "()";
    }
    m_CurrFunctName = m_StrCurrFunctName.c_str();
    m_FunctName.clear();
    if ( !m_ClassSet ) {
        m_ClassName.clear();
    }
}


void CDiagCompileInfo::SetClass(const string& cls)
{
    m_ClassName = cls;
    m_ClassSet = true;
}


// Skip matching l/r separators, return the last char before them
// or null if the separators are unbalanced.
const char* find_match(char        lsep,
                       char        rsep,
                       const char* start,
                       const char* stop)
{
    if (*(stop - 1) != rsep) return stop;
    int balance = 1;
    const char* pos = stop - 2;
    for (; pos > start; pos--) {
        if (*pos == rsep) {
            balance++;
        }
        else if (*pos == lsep) {
            if (--balance == 0) break;
        }
    }
    return (pos <= start) ? NULL : pos;
}


void
CDiagCompileInfo::ParseCurrFunctName(void) const
{
    m_Parsed = true;
    if (!m_CurrFunctName  ||  !(*m_CurrFunctName)) {
        return;
    }
    // Parse curr_funct

    // Skip function arguments
    size_t len = strlen(m_CurrFunctName);
    const char* end_str = find_match('(', ')',
                                     m_CurrFunctName,
                                     m_CurrFunctName + len);
    if (end_str == m_CurrFunctName + len) {
        // Missing '('
        return;
    }
    if ( end_str ) {
        // Skip template arguments
        end_str = find_match('<', '>', m_CurrFunctName, end_str);
    }
    if ( !end_str ) {
        return;
    }
    // Get a function/method name
    const char* start_str = NULL;

    // Get a function start position.
    const char* start_str_tmp =
        str_rev_str(m_CurrFunctName, end_str, "::");
    bool has_class = start_str_tmp != NULL;
    if (start_str_tmp != NULL) {
        start_str = start_str_tmp + 2;
    } else {
        start_str_tmp = str_rev_str(m_CurrFunctName, end_str, " ");
        if (start_str_tmp != NULL) {
            start_str = start_str_tmp + 1;
        } 
    }

    const char* cur_funct_name =
        (start_str == NULL ? m_CurrFunctName : start_str);
    while (cur_funct_name  &&  *cur_funct_name  &&
        (*cur_funct_name == '*'  ||  *cur_funct_name == '&')) {
        ++cur_funct_name;
    }
    size_t cur_funct_name_len = end_str - cur_funct_name;
    m_FunctName = string(cur_funct_name, cur_funct_name_len);

    // Get a class name
    if (has_class  &&  !m_ClassSet) {
        end_str = find_match('<', '>', m_CurrFunctName, start_str - 2);
        start_str = str_rev_str(m_CurrFunctName, end_str, " ");
        const char* cur_class_name =
            (start_str == NULL ? m_CurrFunctName : start_str + 1);
        while (cur_class_name  &&  *cur_class_name  &&
            (*cur_class_name == '*'  ||  *cur_class_name == '&')) {
            ++cur_class_name;
        }
        size_t cur_class_name_len = end_str - cur_class_name;
        m_ClassName = string(cur_class_name, cur_class_name_len);
    }
}



///////////////////////////////////////////////////////
//  CDiagRecycler::

class CDiagRecycler {
public:
    CDiagRecycler(void)
    {
#if defined(NCBI_POSIX_THREADS) && defined(HAVE_PTHREAD_ATFORK)
        pthread_atfork(s_NcbiDiagPreFork,   // before
                       s_NcbiDiagPostFork,  // after in parent
                       s_NcbiDiagPostFork); // after in child
#endif
    }
    ~CDiagRecycler(void)
    {
        SetDiagHandler(0, false);
        SetDiagErrCodeInfo(0, false);
    }
};

static CSafeStatic<CDiagRecycler> s_DiagRecycler;


// Helper function to check if applog severity lock is set and
// return the proper printable severity.

EDiagSev AdjustApplogPrintableSeverity(EDiagSev sev)
{
    if ( !CDiagContext::IsApplogSeverityLocked() ) return sev;
    return CompareDiagPostLevel(sev, eDiag_Warning) < 0
        ? sev : eDiag_Warning;
}


///////////////////////////////////////////////////////
//  CDiagContextThreadData::


struct SRequestCtxWrapper
{
    CRef<CRequestContext> m_Ctx;
};


inline Uint8 s_GetThreadId(void)
{
    if (TPrintSystemTID::GetDefault()) {
        return (Uint8)(CThreadSystemID::GetCurrent().m_ID); // GCC 3.4.6 gives warning - ignore it.
    } else {
        return CThread::GetSelf();
    }
}


enum EThreadDataState {
    eInitialized,
    eUninitialized,
    eInitializing,
    eDeinitialized,
    eReinitializing
};

static volatile EThreadDataState s_ThreadDataState = eUninitialized;

static void s_ThreadDataSafeStaticCleanup(void*)
{
    s_ThreadDataState = eDeinitialized; // re-enable protection
}


bool CDiagContextThreadData::IsInitialized(void)
{
    return s_ThreadDataState == eInitialized;
}


CDiagContextThreadData::CDiagContextThreadData(void)
    : m_Properties(NULL),
      m_DiagBuffer(new CDiagBuffer),
      m_TID(s_GetThreadId()),
      m_ThreadPostNumber(0),
      m_DiagCollectionSize(0),
      m_RequestCtx(new SRequestCtxWrapper),
      m_DefaultRequestCtx(new SRequestCtxWrapper)
{
    // Default context should auto-reset on request start.
    m_RequestCtx->m_Ctx = m_DefaultRequestCtx->m_Ctx =
        new CRequestContext(CRequestContext::fResetOnStart);
    m_RequestCtx->m_Ctx->SetAutoIncRequestIDOnPost(
        CRequestContext::GetDefaultAutoIncRequestIDOnPost());
}


CDiagContextThreadData::~CDiagContextThreadData(void)
{
}


void CDiagContext::sx_ThreadDataTlsCleanup(CDiagContextThreadData* value,
                                           void* cleanup_data)
{
    if ( cleanup_data ) {
        // Copy properties from the main thread's TLS to the global properties.
        CDiagLock lock(CDiagLock::eWrite);
        CDiagContextThreadData::TProperties* props =
            value->GetProperties(CDiagContextThreadData::eProp_Get); /* NCBI_FAKE_WARNING */
        if ( props ) {
            GetDiagContext().m_Properties.insert(props->begin(),
                                                 props->end());
        }
        // Print stop message.
        if (!CDiagContext::IsSetOldPostFormat()  &&  s_FinishedSetupDiag) {
            GetDiagContext().PrintStop();
        }
        s_ThreadDataState = eDeinitialized; // re-enable protection
    }
    delete value;
}


CDiagContextThreadData& CDiagContextThreadData::GetThreadData(void)
{
    // If any of this method's direct or indirect callees attempted to
    // report a (typically fatal) error, the result would ordinarily
    // be infinite recursion resulting in an undignified crash.  The
    // business with s_ThreadDataState allows the program to exit
    // (relatively) gracefully in such cases.
    //
    // In principle, such an event could happen at any stage; in
    // practice, however, the first call involves a superset of most
    // following calls' actions, at least until deep program
    // finalization.  Moreover, attempting to catch bad calls
    // mid-execution would both add overhead and open up uncatchable
    // opportunities for inappropriate recursion.

    static volatile CThreadSystemID s_LastThreadID
        = THREAD_SYSTEM_ID_INITIALIZER;

    if (s_ThreadDataState != eInitialized) {
        // Avoid false positives, while also taking care not to call
        // anything that might itself produce diagnostics.
        CThreadSystemID thread_id = CThreadSystemID::GetCurrent();
        switch (s_ThreadDataState) {
        case eInitialized:
            break;

        case eUninitialized:
            s_ThreadDataState = eInitializing;
            s_LastThreadID.Set(thread_id);
            break;

        case eInitializing:
            if (s_LastThreadID.Is(thread_id)) {
                cerr << "FATAL ERROR: inappropriate recursion initializing NCBI"
                        " diagnostic framework." << endl;
                Abort();
            }
            break;

        case eDeinitialized:
            s_ThreadDataState = eReinitializing;
            s_LastThreadID.Set(thread_id);
            break;

        case eReinitializing:
            if (s_LastThreadID.Is(thread_id)) {
                cerr << "FATAL ERROR: NCBI diagnostic framework no longer"
                        " initialized." << endl;
                Abort();
            }
            break;
        }
    }

    static CStaticTls<CDiagContextThreadData>
        s_ThreadData(s_ThreadDataSafeStaticCleanup,
        CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 1));
    CDiagContextThreadData* data = s_ThreadData.GetValue();
    if ( !data ) {
        // Cleanup data set to null for any thread except the main one.
        // This value is used as a flag to copy threads' properties to global
        // upon TLS cleanup.
        data = new CDiagContextThreadData;
        s_ThreadData.SetValue(data, CDiagContext::sx_ThreadDataTlsCleanup,
            CThread::GetSelf() ? 0 : (void*)(1));
    }

    s_ThreadDataState = eInitialized;

    return *data;
}


CRequestContext& CDiagContextThreadData::GetRequestContext(void)
{
    _ASSERT(m_RequestCtx.get()  &&  m_RequestCtx->m_Ctx);
    return *m_RequestCtx->m_Ctx;
}


void CDiagContextThreadData::SetRequestContext(CRequestContext* ctx)
{
    m_RequestCtx->m_Ctx = ctx ? ctx : m_DefaultRequestCtx->m_Ctx;
}


CDiagContextThreadData::TProperties*
CDiagContextThreadData::GetProperties(EGetProperties flag)
{
    if ( !m_Properties.get()  &&  flag == eProp_Create ) {
        m_Properties.reset(new TProperties);
    }
    return m_Properties.get();
}


CDiagContextThreadData::TCount
CDiagContextThreadData::GetThreadPostNumber(EPostNumberIncrement inc)
{
    return inc == ePostNumber_Increment ?
        ++m_ThreadPostNumber : m_ThreadPostNumber;
}


void CDiagContextThreadData::AddCollectGuard(CDiagCollectGuard* guard)
{
    m_CollectGuards.push_front(guard);
}


void CDiagContextThreadData::RemoveCollectGuard(CDiagCollectGuard* guard)
{
    TCollectGuards::iterator itg = find(
        m_CollectGuards.begin(), m_CollectGuards.end(), guard);
    if (itg == m_CollectGuards.end()) {
        return; // The guard has been already released
    }
    m_CollectGuards.erase(itg);
    if ( !m_CollectGuards.empty() ) {
        return;
        // Previously printing was done for each guard, discarding - only for
        // the last guard.
    }
    // If this is the last guard, perform its action
    CDiagLock lock(CDiagLock::eWrite);
    if (guard->GetAction() == CDiagCollectGuard::ePrint) {
        CDiagHandler* handler = GetDiagHandler();
        if ( handler ) {
            ITERATE(TDiagCollection, itc, m_DiagCollection) {
                if ((itc->m_Flags & eDPF_IsConsole) != 0) {
                    // Print all messages to console
                    handler->PostToConsole(*itc);
                    // Make sure only messages with the severity above allowed
                    // are printed to normal log.
                    EDiagSev post_sev = AdjustApplogPrintableSeverity(
                                            guard->GetCollectSeverity());
                    bool allow_trace = post_sev == eDiag_Trace;
                    if (itc->m_Severity == eDiag_Trace  &&  !allow_trace) {
                        continue; // trace is disabled
                    }
                    if (itc->m_Severity < post_sev) {
                        continue;
                    }
                }
                handler->Post(*itc);
            }
            size_t discarded = m_DiagCollectionSize - m_DiagCollection.size();
            if (discarded > 0) {
                ERR_POST_X(18, Warning << "Discarded " << discarded <<
                    " messages due to collection limit. Set "
                    "DIAG_COLLECT_LIMIT to increase the limit.");
            }
        }
    }
    m_DiagCollection.clear();
    m_DiagCollectionSize = 0;
}


CDiagCollectGuard* CDiagContextThreadData::GetCollectGuard(void)
{
    return m_CollectGuards.empty() ? NULL : m_CollectGuards.front();
}


void CDiagContextThreadData::CollectDiagMessage(const SDiagMessage& mess)
{
    if (m_DiagCollectionSize >= TDiagCollectLimit::GetDefault()) {
        m_DiagCollection.erase(m_DiagCollection.begin());
    }
    m_DiagCollection.push_back(mess);
    m_DiagCollectionSize++;
}


CDiagContextThreadData::TCount
CDiagContextThreadData::GetRequestId(void)
{
    return GetRequestContext().GetRequestID();
}


void CDiagContextThreadData::SetRequestId(TCount id)
{
    GetRequestContext().SetRequestID(id);
}


void CDiagContextThreadData::IncRequestId(void)
{
    GetRequestContext().SetRequestID();
}


extern Uint8 GetDiagRequestId(void)
{
    return GetDiagContext().GetRequestContext().GetRequestID();
}


extern void SetDiagRequestId(Uint8 id)
{
    GetDiagContext().GetRequestContext().SetRequestID(id);
}


///////////////////////////////////////////////////////
//  CDiagContext::


// AppState formatting/parsing
static const char* s_AppStateStr[] = {
    "NS", "PB", "P", "PE", "RB", "R", "RE"
};

static const char* s_LegacyAppStateStr[] = {
    "AB", "A", "AE"
};

const char* s_AppStateToStr(EDiagAppState state)
{
    return s_AppStateStr[state];
}

EDiagAppState s_StrToAppState(const string& state)
{
    for (int st = (int)eDiagAppState_AppBegin;
        st <= eDiagAppState_RequestEnd; st++) {
        if (state == s_AppStateStr[st]) {
            return (EDiagAppState)st;
        }
    }
    // Backward compatibility - allow to use 'A' instead of 'P'
    for (int st = (int)eDiagAppState_AppBegin;
        st <= eDiagAppState_AppEnd; st++) {
        if (state == s_LegacyAppStateStr[st - 1]) {
            return (EDiagAppState)st;
        }
    }

    // Throw to notify caller about invalid app state.
    NCBI_THROW(CException, eUnknown, "Invalid EDiagAppState value");
    /*NOTREACHED*/
    return eDiagAppState_NotSet;
}


NCBI_PARAM_DECL(bool, Diag, UTC_Timestamp);
NCBI_PARAM_DEF_EX(bool, Diag, UTC_Timestamp, false,
                  eParam_NoThread, DIAG_UTC_TIMESTAMP);
typedef NCBI_PARAM_TYPE(Diag, UTC_Timestamp) TUtcTimestamp;


static CTime s_GetFastTime(void)
{
    const static bool s_UtcTimestamp = TUtcTimestamp::GetDefault();
    bool use_gmt = s_UtcTimestamp  &&  !CDiagContext::IsApplogSeverityLocked();
    return use_gmt ? CTime(CTime::eCurrent, CTime::eGmt) : GetFastLocalTime();
}


struct SDiagMessageData
{
    SDiagMessageData(void);
    ~SDiagMessageData(void) {}

    string m_Message;
    string m_File;
    string m_Module;
    string m_Class;
    string m_Function;
    string m_Prefix;
    string m_ErrText;

    CDiagContext::TUID m_UID;
    CTime              m_Time;

    // If the following properties are not set, take them from DiagContext.
    string m_Host;
    string m_Client;
    string m_Session;
    string m_AppName;
    EDiagAppState m_AppState;
};


SDiagMessageData::SDiagMessageData(void)
    : m_UID(0),
      m_Time(s_GetFastTime()),
      m_AppState(eDiagAppState_NotSet)
{
}


CDiagContext* CDiagContext::sm_Instance = NULL;
bool CDiagContext::sm_ApplogSeverityLocked = false;


CDiagContext::CDiagContext(void)
    : m_UID(0),
      m_Host(new CEncodedString),
      m_Username(new CEncodedString),
      m_AppName(new CEncodedString),
      m_ExitCode(0),
      m_ExitSig(0),
      m_AppState(eDiagAppState_AppBegin),
      m_StopWatch(new CStopWatch(CStopWatch::eStart)),
      m_MaxMessages(100), // limit number of collected messages to 100
      m_AppLogRC(new CRequestRateControl(
          GetLogRate_Limit(eLogRate_App),
          CTimeSpan((long)GetLogRate_Period(eLogRate_App)),
          CTimeSpan((long)0),
          CRequestRateControl::eErrCode,
          CRequestRateControl::eDiscrete)),
      m_ErrLogRC(new CRequestRateControl(
          GetLogRate_Limit(eLogRate_Err),
          CTimeSpan((long)GetLogRate_Period(eLogRate_Err)),
          CTimeSpan((long)0),
          CRequestRateControl::eErrCode,
          CRequestRateControl::eDiscrete)),
      m_TraceLogRC(new CRequestRateControl(
          GetLogRate_Limit(eLogRate_Trace),
          CTimeSpan((long)GetLogRate_Period(eLogRate_Trace)),
          CTimeSpan((long)0),
          CRequestRateControl::eErrCode,
          CRequestRateControl::eDiscrete)),
      m_AppLogSuspended(false),
      m_ErrLogSuspended(false),
      m_TraceLogSuspended(false)
{
    sm_Instance = this;
}


CDiagContext::~CDiagContext(void)
{
    sm_Instance = NULL;
}


void CDiagContext::ResetLogRates(void)
{
    CFastMutexGuard lock(s_ApproveMutex);
    m_AppLogRC->Reset(GetLogRate_Limit(eLogRate_App),
        CTimeSpan((long)GetLogRate_Period(eLogRate_App)),
        CTimeSpan((long)0),
        CRequestRateControl::eErrCode,
        CRequestRateControl::eDiscrete);
    m_ErrLogRC->Reset(GetLogRate_Limit(eLogRate_Err),
        CTimeSpan((long)GetLogRate_Period(eLogRate_Err)),
        CTimeSpan((long)0),
        CRequestRateControl::eErrCode,
        CRequestRateControl::eDiscrete);
    m_TraceLogRC->Reset(GetLogRate_Limit(eLogRate_Trace),
        CTimeSpan((long)GetLogRate_Period(eLogRate_Trace)),
        CTimeSpan((long)0),
        CRequestRateControl::eErrCode,
        CRequestRateControl::eDiscrete);
    m_AppLogSuspended = false;
    m_ErrLogSuspended = false;
    m_TraceLogSuspended = false;
}


unsigned int CDiagContext::GetLogRate_Limit(ELogRate_Type type) const
{
    switch ( type ) {
    case eLogRate_App:
        return TAppLogRateLimitParam::GetDefault();
    case eLogRate_Err:
        return TErrLogRateLimitParam::GetDefault();
    case eLogRate_Trace:
    default:
        return TTraceLogRateLimitParam::GetDefault();
    }
}

void CDiagContext::SetLogRate_Limit(ELogRate_Type type, unsigned int limit)
{
    CFastMutexGuard lock(s_ApproveMutex);
    switch ( type ) {
    case eLogRate_App:
        TAppLogRateLimitParam::SetDefault(limit);
        if ( m_AppLogRC.get() ) {
            m_AppLogRC->Reset(limit,
                CTimeSpan((long)GetLogRate_Period(type)),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_AppLogSuspended = false;
        break;
    case eLogRate_Err:
        TErrLogRateLimitParam::SetDefault(limit);
        if ( m_ErrLogRC.get() ) {
            m_ErrLogRC->Reset(limit,
                CTimeSpan((long)GetLogRate_Period(type)),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_ErrLogSuspended = false;
        break;
    case eLogRate_Trace:
    default:
        TTraceLogRateLimitParam::SetDefault(limit);
        if ( m_TraceLogRC.get() ) {
            m_TraceLogRC->Reset(limit,
                CTimeSpan((long)GetLogRate_Period(type)),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_TraceLogSuspended = false;
        break;
    }
}

unsigned int CDiagContext::GetLogRate_Period(ELogRate_Type type) const
{
    switch ( type ) {
    case eLogRate_App:
        return TAppLogRatePeriodParam::GetDefault();
    case eLogRate_Err:
        return TErrLogRatePeriodParam::GetDefault();
    case eLogRate_Trace:
    default:
        return TTraceLogRatePeriodParam::GetDefault();
    }
}

void CDiagContext::SetLogRate_Period(ELogRate_Type type, unsigned int period)
{
    CFastMutexGuard lock(s_ApproveMutex);
    switch ( type ) {
    case eLogRate_App:
        TAppLogRatePeriodParam::SetDefault(period);
        if ( m_AppLogRC.get() ) {
            m_AppLogRC->Reset(GetLogRate_Limit(type),
                CTimeSpan((long)period),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_AppLogSuspended = false;
        break;
    case eLogRate_Err:
        TErrLogRatePeriodParam::SetDefault(period);
        if ( m_ErrLogRC.get() ) {
            m_ErrLogRC->Reset(GetLogRate_Limit(type),
                CTimeSpan((long)period),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_ErrLogSuspended = false;
        break;
    case eLogRate_Trace:
    default:
        TTraceLogRatePeriodParam::SetDefault(period);
        if ( m_TraceLogRC.get() ) {
            m_TraceLogRC->Reset(GetLogRate_Limit(type),
                CTimeSpan((long)period),
                CTimeSpan((long)0),
                CRequestRateControl::eErrCode,
                CRequestRateControl::eDiscrete);
        }
        m_TraceLogSuspended = false;
        break;
    }
}


bool CDiagContext::ApproveMessage(SDiagMessage& msg,
                                  bool*         show_warning)
{
    bool approved = true;
    if ( IsSetDiagPostFlag(eDPF_AppLog, msg.m_Flags) ) {
        if ( m_AppLogRC->IsEnabled() ) {
            CFastMutexGuard lock(s_ApproveMutex);
            approved = m_AppLogRC->Approve();
        }
        if ( approved ) {
            m_AppLogSuspended = false;
        }
        else {
            *show_warning = !m_AppLogSuspended;
            m_AppLogSuspended = true;
        }
    }
    else {
        switch ( msg.m_Severity ) {
        case eDiag_Info:
        case eDiag_Trace:
            if ( m_TraceLogRC->IsEnabled() ) {
                CFastMutexGuard lock(s_ApproveMutex);
                approved = m_TraceLogRC->Approve();
            }
            if ( approved ) {
                m_TraceLogSuspended = false;
            }
            else {
                *show_warning = !m_TraceLogSuspended;
                m_TraceLogSuspended = true;
            }
            break;
        default:
            if ( m_ErrLogRC->IsEnabled() ) {
                CFastMutexGuard lock(s_ApproveMutex);
                approved = m_ErrLogRC->Approve();
            }
            if ( approved ) {
                m_ErrLogSuspended = false;
            }
            else {
                *show_warning = !m_ErrLogSuspended;
                m_ErrLogSuspended = true;
            }
        }
    }
    return approved;
}


CDiagContext::TPID CDiagContext::sm_PID = 0;

CDiagContext::TPID CDiagContext::GetPID(void)
{
    if ( !sm_PID ) {
        sm_PID = CProcess::GetCurrentPid();
    }
    return sm_PID;
}


bool CDiagContext::UpdatePID(void)
{
    TPID new_pid = CProcess::GetCurrentPid();
    if (sm_PID == new_pid) {
        // Parent process does not need to update pid/guid
        return false;
    }
    sm_PID = new_pid;
    CDiagContext& ctx = GetDiagContext();
    TUID old_uid = ctx.GetUID();
    // Update GUID to match the new PID
    ctx.x_CreateUID();
    ctx.Extra().
        Print("action", "fork").
        Print("parent_guid", ctx.GetStringUID(old_uid));
    //ctx.PrintExtra("New process created by fork(), "
    //    "parent GUID=" + );
    return true;
}


void CDiagContext::UpdateOnFork(TOnForkFlags flags)
{
    CDiagContext& ctx = GetDiagContext();
    // Do not perform any actions in the parent process.
    if ( !UpdatePID() ) return;
    if (flags && fOnFork_ResetTimer) {
        ctx.m_StopWatch->Restart();
    }
    if (flags && fOnFork_PrintStart) {
        ctx.PrintStart(kEmptyStr);
    }
}


void CDiagContext::x_CreateUID(void) const
{
    Int8 pid = GetPID();
    time_t t = time(0);
    const string& host = GetHost();
    TUID h = 212;
    ITERATE(string, s, host) {
        h = h*1265 + *s;
    }
    h &= 0xFFFF;
    // The low 4 bits are reserved as GUID generator version number.
    m_UID = (TUID(h) << 48) |
        ((TUID(pid) & 0xFFFF) << 32) |
        ((TUID(t) & 0xFFFFFFF) << 4) |
        1; // version #1 - fixed type conversion bug
}


CDiagContext::TUID CDiagContext::GetUID(void) const
{
    if ( !m_UID ) {
        CDiagLock lock(CDiagLock::eWrite);
        if ( !m_UID ) {
            x_CreateUID();
        }
    }
    return m_UID;
}


string CDiagContext::GetStringUID(TUID uid) const
{
    char buf[18];
    if (uid == 0) {
        uid = GetUID();
    }
    int hi = int((uid >> 32) & 0xFFFFFFFF);
    int lo = int(uid & 0xFFFFFFFF);
    sprintf(buf, "%08X%08X", hi, lo);
    return string(buf);
}


CDiagContext::TUID CDiagContext::UpdateUID(TUID uid) const
{
    if (uid == 0) {
        uid = GetUID();
    }
    time_t t = time(0);
    // Clear old timestamp
    uid &= ~((TUID)0xFFFFFFF << 4);
    // Add current timestamp
    return uid | ((TUID(t) & 0xFFFFFFF) << 4);
}


string CDiagContext::GetNextHitID(void) const
{
    Uint8 hi = GetUID();
    Uint4 b3 = Uint4((hi >> 32) & 0xFFFFFFFF);
    Uint4 b2 = Uint4(hi & 0xFFFFFFFF);

    CDiagContextThreadData& thr_data = CDiagContextThreadData::GetThreadData();
    Uint8 tid = (thr_data.GetTID() & 0xFFFFFF) << 40;
    Uint8 rid = Uint8(thr_data.GetRequestContext().GetRequestID() & 0xFFFFFF) << 16;
    Uint8 us = (GetFastLocalTime().MicroSecond()/16) & 0xFFFF;
    Uint8 lo = tid | rid | us;
    Uint4 b1 = Uint4((lo >> 32) & 0xFFFFFFFF);
    Uint4 b0 = Uint4(lo & 0xFFFFFFFF);
    char buf[40];
    sprintf(buf, "%08X%08X%08X%08X", b3, b2, b1, b0);
    return string(buf);
}


const string& CDiagContext::GetUsername(void) const
{
    return m_Username->GetOriginalString();
}


void CDiagContext::SetUsername(const string& username)
{
    m_Username->SetString(username);
}


const string& CDiagContext::GetHost(void) const
{
    // Check context properties
    if ( !m_Host->IsEmpty() ) {
        return m_Host->GetOriginalString();
    }
    if ( !m_HostIP.empty() ) {
        return m_HostIP;
    }

#if defined(NCBI_OS_UNIX)
    // UNIX - use uname()
    {{
        struct utsname buf;
        if (uname(&buf) >= 0) {
            m_Host->SetString(buf.nodename);
            return m_Host->GetOriginalString();
        }
    }}
#endif

#if defined(NCBI_OS_MSWIN)
    // MSWIN - use COMPUTERNAME
    const TXChar* compname = NcbiSys_getenv(_TX("COMPUTERNAME"));
    if ( compname  &&  *compname ) {
        m_Host->SetString(_T_STDSTRING(compname));
        return m_Host->GetOriginalString();
    }
#endif

    // Server env. - use SERVER_ADDR
    const TXChar* servaddr = NcbiSys_getenv(_TX("SERVER_ADDR"));
    if ( servaddr  &&  *servaddr ) {
        m_Host->SetString(_T_STDSTRING(servaddr));
    }
    return m_Host->GetOriginalString();
}


const string& CDiagContext::GetEncodedHost(void) const
{
    if ( !m_Host->IsEmpty() ) {
        return m_Host->GetEncodedString();
    }
    if ( !m_HostIP.empty() ) {
        return m_HostIP;
    }
    // Initialize m_Host, this does not change m_HostIP
    GetHost();
    return m_Host->GetEncodedString();
}


const string& CDiagContext::GetHostname(void) const
{
    return m_Host->GetOriginalString();
}


const string& CDiagContext::GetEncodedHostname(void) const
{
    return m_Host->GetEncodedString();
}


void CDiagContext::SetHostname(const string& hostname)
{
    m_Host->SetString(hostname);
}


void CDiagContext::SetHostIP(const string& ip)
{
    if ( !NStr::IsIPAddress(ip) ) {
        m_HostIP.clear();
        ERR_POST("Bad host IP value: " << ip);
        return;
    }
    m_HostIP = ip;
}


const string& CDiagContext::GetAppName(void) const
{
    if ( m_AppName->IsEmpty() ) {
        m_AppName->SetString(CNcbiApplication::GetAppName());
    }
    return m_AppName->GetOriginalString();
}


const string& CDiagContext::GetEncodedAppName(void) const
{
    return m_AppName->GetEncodedString();
}


void CDiagContext::SetAppName(const string& app_name)
{
    if ( !m_AppName->IsEmpty() ) {
        // AppName can be set only once
        ERR_POST("Application name cannot be changed.");
        return;
    }
    m_AppName->SetString(app_name);
    if ( m_AppName->IsEncoded() ) {
        ERR_POST("Illegal characters in application name: '" << app_name <<
            "', using URL-encode.");
    }
}


CRequestContext& CDiagContext::GetRequestContext(void)
{
    return CDiagContextThreadData::GetThreadData().GetRequestContext();
}


void CDiagContext::SetRequestContext(CRequestContext* ctx)
{
    CDiagContextThreadData::GetThreadData().SetRequestContext(ctx);
}


void CDiagContext::SetAutoWrite(bool value)
{
    TAutoWrite_Context::SetDefault(value);
}


inline bool IsGlobalProperty(const string& name)
{
    return
        name == CDiagContext::kProperty_UserName  ||
        name == CDiagContext::kProperty_HostName  ||
        name == CDiagContext::kProperty_HostIP    ||
        name == CDiagContext::kProperty_AppName   ||
        name == CDiagContext::kProperty_ExitSig   ||
        name == CDiagContext::kProperty_ExitCode;
}


void CDiagContext::SetProperty(const string& name,
                               const string& value,
                               EPropertyMode mode)
{
    // Global properties
    if (name == kProperty_UserName) {
        SetUsername(value);
        return;
    }
    if (name == kProperty_HostName) {
        SetHostname(value);
        return;
    }
    if (name == kProperty_HostIP) {
        SetHostIP(value);
        return;
    }
    if (name == kProperty_AppName) {
        SetAppName(value);
        return;
    }
    if (name == kProperty_ExitCode) {
        SetExitCode(NStr::StringToInt(value, NStr::fConvErr_NoThrow));
        return;
    }
    if (name == kProperty_ExitSig) {
        SetExitSignal(NStr::StringToInt(value, NStr::fConvErr_NoThrow));
        return;
    }

    // Request properties
    if (name == kProperty_AppState) {
        try {
            SetAppState(s_StrToAppState(value));
        }
        catch (CException) {
        }
        return;
    }
    if (name == kProperty_ClientIP) {
        GetRequestContext().SetClientIP(value);
        return;
    }
    if (name == kProperty_SessionID) {
        GetRequestContext().SetSessionID(value);
        return;
    }
    if (name == kProperty_ReqStatus) {
        if ( !value.empty() ) {
            GetRequestContext().SetRequestStatus(
                NStr::StringToInt(value, NStr::fConvErr_NoThrow));
        }
        else {
            GetRequestContext().UnsetRequestStatus();
        }
        return;
    }
    if (name == kProperty_BytesRd) {
        GetRequestContext().SetBytesRd(
            NStr::StringToInt8(value, NStr::fConvErr_NoThrow));
        return;
    }
    if (name == kProperty_BytesWr) {
        GetRequestContext().SetBytesWr(
            NStr::StringToInt8(value, NStr::fConvErr_NoThrow));
        return;
    }
    if (name == kProperty_ReqTime) {
        // Cannot set this property
        return;
    }

    if ( mode == eProp_Default ) {
        mode = IsGlobalProperty(name) ? eProp_Global : eProp_Thread;
    }

    if ( mode == eProp_Global ) {
        CDiagLock lock(CDiagLock::eWrite);
        m_Properties[name] = value;
    }
    else {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Create); /* NCBI_FAKE_WARNING */
        _ASSERT(props);
        (*props)[name] = value;
    }
    if ( sm_Instance  &&  TAutoWrite_Context::GetDefault() ) {
        CDiagLock lock(CDiagLock::eRead);
        x_PrintMessage(SDiagMessage::eEvent_Extra, name + "=" + value);
    }
}


string CDiagContext::GetProperty(const string& name,
                                 EPropertyMode mode) const
{
    // Global properties
    if (name == kProperty_UserName) {
        return GetUsername();
    }
    if (name == kProperty_HostName) {
        return GetHostname();
    }
    if (name == kProperty_HostIP) {
        return GetHostIP();
    }
    if (name == kProperty_AppName) {
        return GetAppName();
    }
    if (name == kProperty_ExitCode) {
        return NStr::IntToString(m_ExitCode);
    }
    if (name == kProperty_ExitSig) {
        return NStr::IntToString(m_ExitSig);
    }

    // Request properties
    if (name == kProperty_AppState) {
        return s_AppStateToStr(GetAppState());
    }
    if (name == kProperty_ClientIP) {
        return GetRequestContext().GetClientIP();
    }
    if (name == kProperty_SessionID) {
        return GetSessionID();
    }
    if (name == kProperty_ReqStatus) {
        return GetRequestContext().IsSetRequestStatus() ?
            NStr::IntToString(GetRequestContext().GetRequestStatus())
            : kEmptyStr;
    }
    if (name == kProperty_BytesRd) {
        return NStr::Int8ToString(GetRequestContext().GetBytesRd());
    }
    if (name == kProperty_BytesWr) {
        return NStr::Int8ToString(GetRequestContext().GetBytesWr());
    }
    if (name == kProperty_ReqTime) {
        return GetRequestContext().GetRequestTimer().AsString();
    }

    if (mode == eProp_Thread  ||
        (mode == eProp_Default  &&  !IsGlobalProperty(name))) {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get); /* NCBI_FAKE_WARNING */
        if ( props ) {
            TProperties::const_iterator tprop = props->find(name);
            if ( tprop != props->end() ) {
                return tprop->second;
            }
        }
        if (mode == eProp_Thread) {
            return kEmptyStr;
        }
    }
    // Check global properties
    CDiagLock lock(CDiagLock::eRead);
    TProperties::const_iterator gprop = m_Properties.find(name);
    return gprop != m_Properties.end() ? gprop->second : kEmptyStr;
}


void CDiagContext::DeleteProperty(const string& name,
                                    EPropertyMode mode)
{
    if (mode == eProp_Thread  ||
        (mode ==  eProp_Default  &&  !IsGlobalProperty(name))) {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get); /* NCBI_FAKE_WARNING */
        if ( props ) {
            TProperties::iterator tprop = props->find(name);
            if ( tprop != props->end() ) {
                props->erase(tprop);
                return;
            }
        }
        if (mode == eProp_Thread) {
            return;
        }
    }
    // Check global properties
    CDiagLock lock(CDiagLock::eRead);
    TProperties::iterator gprop = m_Properties.find(name);
    if (gprop != m_Properties.end()) {
        m_Properties.erase(gprop);
    }
}


void CDiagContext::PrintProperties(void)
{
    {{
        CDiagLock lock(CDiagLock::eRead);
        ITERATE(TProperties, gprop, m_Properties) {
            x_PrintMessage(SDiagMessage::eEvent_Extra,
                gprop->first + "=" + gprop->second);
        }
    }}
    TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get); /* NCBI_FAKE_WARNING */
    if ( !props ) {
        return;
    }
    ITERATE(TProperties, tprop, *props) {
        x_PrintMessage(SDiagMessage::eEvent_Extra,
            tprop->first + "=" + tprop->second);
    }
}


void CDiagContext::PrintStart(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Start, message);
}


void CDiagContext::PrintStop(void)
{
    x_PrintMessage(SDiagMessage::eEvent_Stop, kEmptyStr);
}


void CDiagContext::PrintExtra(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Extra, message);
}


CDiagContext_Extra::CDiagContext_Extra(SDiagMessage::EEventType event_type)
    : m_EventType(event_type),
      m_Args(0),
      m_Counter(new int(1)),
      m_Typed(false),
      m_PerfStatus(0),
      m_PerfTime(0),
      m_Flushed(false),
      m_AllowBadNames(false)
{
}


CDiagContext_Extra::CDiagContext_Extra(int         status,
                                       double      timespan,
                                       TExtraArgs& args)
    : m_EventType(SDiagMessage::eEvent_PerfLog),
      m_Args(0),
      m_Counter(new int(1)),
      m_Typed(false),
      m_PerfStatus(status),
      m_PerfTime(timespan),
      m_Flushed(false),
      m_AllowBadNames(false)
{
    if (args.empty()) return;
    m_Args = new TExtraArgs;
    m_Args->splice(m_Args->end(), args);
}


CDiagContext_Extra::CDiagContext_Extra(const CDiagContext_Extra& args)
    : m_EventType(const_cast<CDiagContext_Extra&>(args).m_EventType),
      m_Args(const_cast<CDiagContext_Extra&>(args).m_Args),
      m_Counter(const_cast<CDiagContext_Extra&>(args).m_Counter),
      m_Typed(args.m_Typed),
      m_PerfStatus(args.m_PerfStatus),
      m_PerfTime(args.m_PerfTime),
      m_Flushed(args.m_Flushed),
      m_AllowBadNames(args.m_AllowBadNames)
{
    (*m_Counter)++;
}


CDiagContext_Extra& CDiagContext_Extra::AllowBadSymbolsInArgNames(void)
{
    if (!m_Args  ||  m_Args->empty()) {
        m_AllowBadNames = true;
    }
    return *this;
}


const TDiagPostFlags kApplogDiagPostFlags =
        eDPF_OmitInfoSev | eDPF_OmitSeparator | eDPF_AppLog;

void CDiagContext_Extra::Flush(void)
{
    if (m_Flushed  ||  CDiagContext::IsSetOldPostFormat()) {
        return;
    }

    // Prevent double-flush
    m_Flushed = true;

    // Ignore extra messages without arguments. Allow start/stop,
    // request-start/request-stop without arguments.
    if (m_EventType == SDiagMessage::eEvent_Extra  &&
        (!m_Args  ||  m_Args->empty()) ) {
        return;
    }

    CDiagContext& ctx = GetDiagContext();
    EDiagAppState app_state = ctx.GetAppState();
    bool app_state_updated = false;
    if (m_EventType == SDiagMessage::eEvent_RequestStart) {
        if (app_state != eDiagAppState_RequestBegin  &&
            app_state != eDiagAppState_Request) {
            ctx.SetAppState(eDiagAppState_RequestBegin);
            app_state_updated = true;
        }
        CDiagContext::x_StartRequest();
    }
    else if (m_EventType == SDiagMessage::eEvent_RequestStop) {
        if (app_state != eDiagAppState_RequestEnd) {
            ctx.SetAppState(eDiagAppState_RequestEnd);
            app_state_updated = true;
        }
    }

    auto_ptr<CNcbiOstrstream> ostr;
    char* buf = 0;
    size_t buflen = 0;
    if (m_EventType == SDiagMessage::eEvent_PerfLog) {
        ostr.reset(new CNcbiOstrstream);
        *ostr << m_PerfStatus << " " <<
            NStr::DoubleToString(m_PerfTime, -1, NStr::fDoubleFixed);
        buf = ostr->str();
        buflen = size_t(ostr->pcount());
    }

    SDiagMessage mess(eDiag_Info,
                      buf, buflen,
                      0, 0, // file, line
                      CNcbiDiag::ForceImportantFlags(kApplogDiagPostFlags),
                      NULL,
                      0, 0, // err code/subcode
                      NULL,
                      0, 0, 0); // module/class/function
    mess.m_Event = m_EventType;
    if (m_Args  &&  !m_Args->empty()) {
        mess.m_ExtraArgs.splice(mess.m_ExtraArgs.end(), *m_Args);
    }
    mess.m_TypedExtra = m_Typed;
    mess.m_AllowBadExtraNames = m_AllowBadNames;

    GetDiagBuffer().DiagHandler(mess);
    if ( ostr.get() ) {
        ostr->rdbuf()->freeze(false);
    }

    if ( app_state_updated ) {
        if (m_EventType == SDiagMessage::eEvent_RequestStart) {
            ctx.SetAppState(eDiagAppState_Request);
        }
        else if (m_EventType == SDiagMessage::eEvent_RequestStop) {
            ctx.SetAppState(eDiagAppState_AppRun);
        }
    }
}


void CDiagContext_Extra::x_Release(void)
{
    if ( m_Counter  &&  --(*m_Counter) == 0) {
        Flush();
        delete m_Args;
        m_Args = 0;
    }
}


CDiagContext_Extra&
CDiagContext_Extra::operator=(const CDiagContext_Extra& args)
{
    if (this != &args) {
        x_Release();
        m_Args = const_cast<CDiagContext_Extra&>(args).m_Args;
        m_Counter = const_cast<CDiagContext_Extra&>(args).m_Counter;
        m_Typed = args.m_Typed;
        m_PerfStatus = args.m_PerfStatus;
        m_PerfTime = args.m_PerfTime;
        m_Flushed = args.m_Flushed;
        m_AllowBadNames = args.m_AllowBadNames;
        (*m_Counter)++;
    }
    return *this;
}


CDiagContext_Extra::~CDiagContext_Extra(void)
{
    x_Release();
    if ( *m_Counter == 0) {
        delete m_Counter;
    }
}

bool CDiagContext_Extra::x_CanPrint(void)
{
    // Only allow extra events to be printed/flushed multiple times
    if (m_Flushed  &&  m_EventType != SDiagMessage::eEvent_Extra) {
        ERR_POST_ONCE(
            "Attempt to set request start/stop arguments after flushing");
        return false;
    }

    // For extra messages reset flushed state.
    m_Flushed = false;
    return true;
}


CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, const string& value)
{
    if ( !x_CanPrint() ) {
        return *this;
    }

    if ( !m_Args ) {
        m_Args = new TExtraArgs;
    }
    // Optimize inserting new pair into the args list, it is the same as:
    //     m_Args->push_back(TExtraArg(name, value));
    m_Args->push_back(TExtraArg(kEmptyStr, kEmptyStr));
    m_Args->rbegin()->first.assign(name);
    m_Args->rbegin()->second.assign(value);
    return *this;
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, const char* value)
{
    return Print(name, string(value)); 
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, int value)
{
    return Print(name, NStr::IntToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, unsigned int value)
{
    return Print(name, NStr::UIntToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, long value)
{
    return Print(name, NStr::LongToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, unsigned long value)
{
    return Print(name, NStr::ULongToString(value));
}

#if !NCBI_INT8_IS_LONG
CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, Int8 value)
{
    return Print(name, NStr::Int8ToString(value));
}
CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, Uint8 value)
{
    return Print(name, NStr::UInt8ToString(value));
}
#elif SIZEOF_LONG_LONG
CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, long long value)
{
    return Print(name, NStr::Int8ToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, unsigned long long value)
{
    return Print(name, NStr::UInt8ToString(value));
}
#endif

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, char value)
{
    return Print(name, string(1,value)); 
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, signed char value)
{
    return Print(name, string(1,value)); 
}

CDiagContext_Extra& 
CDiagContext_Extra::Print(const string& name, unsigned char value)
{
    return Print(name, string(1,value)); 
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, double value)
{
    return Print(name, NStr::DoubleToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(const string& name, bool value)
{
    return Print(name, NStr::BoolToString(value));
}

CDiagContext_Extra&
CDiagContext_Extra::Print(TExtraArgs& args)
{
    if ( !x_CanPrint() ) {
        return *this;
    }

    if ( !m_Args ) {
        m_Args = new TExtraArgs;
    }
    m_Args->splice(m_Args->end(), args);
    return *this;
}


static const char* kExtraTypeArgName = "NCBIEXTRATYPE";

CDiagContext_Extra& CDiagContext_Extra::SetType(const string& type)
{
    m_Typed = true;
    Print(kExtraTypeArgName, type);
    return *this;
}


void CDiagContext::PrintRequestStart(const string& message)
{
    EDiagAppState app_state = GetAppState();
    bool app_state_updated = false;
    if (app_state != eDiagAppState_RequestBegin  &&
        app_state != eDiagAppState_Request) {
        SetAppState(eDiagAppState_RequestBegin);
        app_state_updated = true;
    }
    x_PrintMessage(SDiagMessage::eEvent_RequestStart, message);
    if ( app_state_updated ) {
        SetAppState(eDiagAppState_Request);
    }
}


void CDiagContext::PrintRequestStop(void)
{
    EDiagAppState app_state = GetAppState();
    bool app_state_updated = false;
    if (app_state != eDiagAppState_RequestEnd) {
        SetAppState(eDiagAppState_RequestEnd);
        app_state_updated = true;
    }
    x_PrintMessage(SDiagMessage::eEvent_RequestStop, kEmptyStr);
    if ( app_state_updated ) {
        SetAppState(eDiagAppState_AppRun);
    }
}


EDiagAppState CDiagContext::GetGlobalAppState(void) const
{
    CDiagLock lock(CDiagLock::eRead);
    return m_AppState;
}


EDiagAppState CDiagContext::GetAppState(void) const
{
    // This checks thread's state first, then calls GetAppState if necessary.
    return GetRequestContext().GetAppState();
}


void CDiagContext::SetGlobalAppState(EDiagAppState state)
{
    CDiagLock lock(CDiagLock::eWrite);
    m_AppState = state;
}


void CDiagContext::SetAppState(EDiagAppState state)
{
    CRequestContext& ctx = GetRequestContext();
    switch ( state ) {
    case eDiagAppState_AppBegin:
    case eDiagAppState_AppRun:
    case eDiagAppState_AppEnd:
        {
            ctx.SetAppState(eDiagAppState_NotSet);
            CDiagLock lock(CDiagLock::eWrite);
            m_AppState = state;
            break;
        }
    case eDiagAppState_RequestBegin:
    case eDiagAppState_Request:
    case eDiagAppState_RequestEnd:
        ctx.SetAppState(state);
        break;
    default:
        ERR_POST_X(17, Warning << "Invalid EDiagAppState value");
    }
}


void CDiagContext::SetAppState(EDiagAppState state, EPropertyMode mode)
{
    switch ( mode ) {
    case eProp_Default:
        SetAppState(state);
        break;
    case eProp_Global:
        SetGlobalAppState(state);
        break;
    case eProp_Thread:
        GetRequestContext().SetAppState(state);
        break;
    }
}


NCBI_PARAM_DECL(string, Log, Session_Id);
NCBI_PARAM_DEF_EX(string, Log, Session_Id, kEmptyStr, eParam_NoThread,
                  NCBI_LOG_SESSION_ID);
typedef NCBI_PARAM_TYPE(Log, Session_Id) TParamDefaultSessionId;


const string& CDiagContext::GetDefaultSessionID(void) const
{
    CDiagLock lock(CDiagLock::eRead);
    if ( !m_DefaultSessionId.get() ) {
        m_DefaultSessionId.reset(new CEncodedString);
    }
    if ( m_DefaultSessionId->IsEmpty() ) {
        m_DefaultSessionId->SetString(TParamDefaultSessionId::GetDefault());
    }
    return m_DefaultSessionId->GetOriginalString();
}


void CDiagContext::SetDefaultSessionID(const string& session_id)
{
    CDiagLock lock(CDiagLock::eWrite);
    if ( !m_DefaultSessionId.get() ) {
        m_DefaultSessionId.reset(new CEncodedString);
    }
    m_DefaultSessionId->SetString(session_id);
}


const string& CDiagContext::GetSessionID(void) const
{
    CRequestContext& rctx = GetRequestContext();
    if ( rctx.IsSetExplicitSessionID() ) {
        return rctx.GetSessionID();
    }
    return GetDefaultSessionID();
}


const string& CDiagContext::GetEncodedSessionID(void) const
{
    CRequestContext& rctx = GetRequestContext();
    if ( rctx.IsSetExplicitSessionID() ) {
        return rctx.GetEncodedSessionID();
    }
    GetDefaultSessionID(); // Make sure the default value is initialized.
    _ASSERT(m_DefaultSessionId.get());
    return m_DefaultSessionId->GetEncodedString();
}


NCBI_PARAM_DECL(string, Log, Client_Ip);
NCBI_PARAM_DEF_EX(string, Log, Client_Ip, kEmptyStr, eParam_NoThread,
                  NCBI_LOG_CLIENT_IP);
typedef NCBI_PARAM_TYPE(Log, Client_Ip) TParamDefaultClientIp;


const string CDiagContext::GetDefaultClientIP(void)
{
    return TParamDefaultClientIp::GetDefault();
}


void CDiagContext::SetDefaultClientIP(const string& client_ip)
{
    TParamDefaultClientIp::SetDefault(client_ip);
}


const char* CDiagContext::kProperty_UserName    = "user";
const char* CDiagContext::kProperty_HostName    = "host";
const char* CDiagContext::kProperty_HostIP      = "host_ip_addr";
const char* CDiagContext::kProperty_ClientIP    = "client_ip";
const char* CDiagContext::kProperty_SessionID   = "session_id";
const char* CDiagContext::kProperty_AppName     = "app_name";
const char* CDiagContext::kProperty_AppState    = "app_state";
const char* CDiagContext::kProperty_ExitSig     = "exit_signal";
const char* CDiagContext::kProperty_ExitCode    = "exit_code";
const char* CDiagContext::kProperty_ReqStatus   = "request_status";
const char* CDiagContext::kProperty_ReqTime     = "request_time";
const char* CDiagContext::kProperty_BytesRd     = "bytes_rd";
const char* CDiagContext::kProperty_BytesWr     = "bytes_wr";

static const char* kDiagTimeFormat = "Y-M-DTh:m:s.rZ";
// Fixed fields' widths
static const int   kDiagW_PID      = 5;
static const int   kDiagW_TID      = 3;
static const int   kDiagW_RID      = 4;
static const int   kDiagW_AppState = 2;
static const int   kDiagW_SN       = 4;
static const int   kDiagW_UID      = 16;
static const int   kDiagW_Host     = 15;
static const int   kDiagW_Client   = 15;
static const int   kDiagW_Session  = 24;

static const char* kUnknown_Host    = "UNK_HOST";
static const char* kUnknown_Client  = "UNK_CLIENT";
static const char* kUnknown_Session = "UNK_SESSION";
static const char* kUnknown_App     = "UNK_APP";


void CDiagContext::WriteStdPrefix(CNcbiOstream& ostr,
                                  const SDiagMessage& msg) const
{
    string uid = GetStringUID(msg.GetUID());
    const string& host = msg.GetHost();
    const string& client = msg.GetClient();
    const string& session = msg.GetSession();
    const string& app = msg.GetAppName();
    const char* app_state = s_AppStateToStr(msg.GetAppState());

    // Print common fields
    ostr << setfill('0') << setw(kDiagW_PID) << msg.m_PID << '/'
         << setw(kDiagW_TID) << msg.m_TID << '/'
         << setw(kDiagW_RID) << msg.m_RequestId
         << "/"
         << setfill(' ') << setw(kDiagW_AppState) << setiosflags(IOS_BASE::left)
         << app_state << resetiosflags(IOS_BASE::left)
         << ' ' << setw(0) << setfill(' ') << uid << ' '
         << setfill('0') << setw(kDiagW_SN) << msg.m_ProcPost << '/'
         << setw(kDiagW_SN) << msg.m_ThrPost << ' '
         << setw(0) << msg.GetTime().AsString(kDiagTimeFormat) << ' '
         << setfill(' ') << setiosflags(IOS_BASE::left)
         << setw(kDiagW_Host)
         << (host.empty() ? kUnknown_Host : host.c_str()) << ' '
         << setw(kDiagW_Client)
         << (client.empty() ? kUnknown_Client : client.c_str()) << ' '
         << setw(kDiagW_Session)
         << (session.empty() ? kUnknown_Session : session.c_str()) << ' '
         << resetiosflags(IOS_BASE::left) << setw(0)
         << (app.empty() ? kUnknown_App : app.c_str()) << ' ';
}


void RequestStopWatchTlsCleanup(CStopWatch* value, void* /*cleanup_data*/)
{
    delete value;
}


void CDiagContext::x_StartRequest(void)
{
    // Reset properties
    CRequestContext& ctx = GetRequestContext();
    if ( ctx.IsRunning() ) {
        // The request is already running -
        // duplicate request start or missing request stop
        ERR_POST_ONCE(
            "Duplicate request-start or missing request-stop");
    }

    // Use the default client ip if no other value is set.
    if ( !ctx.IsSetClientIP() ) {
        string ip = GetDefaultClientIP();
        if ( !ip.empty() ) {
            ctx.SetClientIP(ip);
        }
    }

    ctx.StartRequest();

    // Print selected environment and registry values.
    CNcbiApplication* app = CNcbiApplication::Instance();
    if ( !app ) return;
    string log_args = TLogEnvironment::GetDefault();
    if ( !log_args.empty() ) {
        list<string> log_args_list;
        NStr::Split(log_args, " ", log_args_list);
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogEnvironment", "true");
        const CNcbiEnvironment& env = app->GetEnvironment();
        ITERATE(list<string>, it, log_args_list) {
            const string& val = env.Get(*it);
            extra.Print(*it, val);
        }
        extra.Flush();
    }
    log_args = TLogRegistry::GetDefault();
    if ( !log_args.empty() ) {
        list<string> log_args_list;
        NStr::Split(log_args, " ", log_args_list);
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogRegistry", "true");
        const CNcbiRegistry& reg = app->GetConfig();
        ITERATE(list<string>, it, log_args_list) {
            string section, name;
            NStr::SplitInTwo(*it, ":", section, name);
            const string& val = reg.Get(section, name);
            extra.Print(*it, val);
        }
        extra.Flush();
    }
}


void CDiagContext::x_PrintMessage(SDiagMessage::EEventType event,
                                  const string&            message)
{
    if ( IsSetOldPostFormat() ) {
        return;
    }
    CNcbiOstrstream ostr;
    string prop;
    bool need_space = false;
    CRequestContext& ctx = GetRequestContext();

    switch ( event ) {
    case SDiagMessage::eEvent_Start:
    case SDiagMessage::eEvent_Extra:
        break;
    case SDiagMessage::eEvent_RequestStart:
        {
            x_StartRequest();
            break;
        }
    case SDiagMessage::eEvent_Stop:
        ostr << NStr::IntToString(GetExitCode())
            << " " << m_StopWatch->AsString();
        if (GetExitSignal() != 0) {
            ostr << " SIG=" << GetExitSignal();
        }
        need_space = true;
        break;
    case SDiagMessage::eEvent_RequestStop:
        {
            if ( !ctx.IsRunning() ) {
                // The request is not running -
                // duplicate request stop or missing request start
                ERR_POST_ONCE(
                    "Duplicate request-stop or missing request-start");
            }
            ostr << ctx.GetRequestStatus() << " "
                << ctx.GetRequestTimer().AsString() << " "
                << ctx.GetBytesRd() << " "
                << ctx.GetBytesWr();
            need_space = true;
            break;
        }
    default:
        return; // Prevent warning about other event types.
    }
    if ( !message.empty() ) {
        if (need_space) {
            ostr << " ";
        }
        ostr << message;
    }
    SDiagMessage mess(eDiag_Info,
                      ostr.str(), size_t(ostr.pcount()),
                      0, 0, // file, line
                      CNcbiDiag::ForceImportantFlags(kApplogDiagPostFlags),
                      NULL,
                      0, 0, // err code/subcode
                      NULL,
                      0, 0, 0); // module/class/function
    mess.m_Event = event;
    CDiagBuffer::DiagHandler(mess);
    ostr.rdbuf()->freeze(false);
    // Now it's safe to reset the request context
    if (event == SDiagMessage::eEvent_RequestStop) {
        ctx.StopRequest();
    }
}


bool CDiagContext::IsSetOldPostFormat(void)
{
     return TOldPostFormatParam::GetDefault();
}


void CDiagContext::SetOldPostFormat(bool value)
{
    TOldPostFormatParam::SetDefault(value);
}


bool CDiagContext::IsUsingSystemThreadId(void)
{
     return TPrintSystemTID::GetDefault();
}


void CDiagContext::UseSystemThreadId(bool value)
{
    TPrintSystemTID::SetDefault(value);
}


void CDiagContext::InitMessages(size_t max_size)
{
    if ( !m_Messages.get() ) {
        m_Messages.reset(new TMessages);
    }
    m_MaxMessages = max_size;
}


void CDiagContext::PushMessage(const SDiagMessage& message)
{
    if ( m_Messages.get()  &&  m_Messages->size() < m_MaxMessages) {
        m_Messages->push_back(message);
    }
}


void CDiagContext::FlushMessages(CDiagHandler& handler)
{
    if ( !m_Messages.get()  ||  m_Messages->empty() ) {
        return;
    }
    CTeeDiagHandler* tee = dynamic_cast<CTeeDiagHandler*>(&handler);
    if (tee  &&  !tee->GetOriginalHandler()) {
        // Tee over STDERR - flushing will create duplicate messages
        return;
    }
    auto_ptr<TMessages> tmp(m_Messages.release());
    //ERR_POST_X(1, Message << "***** BEGIN COLLECTED MESSAGES *****");
    NON_CONST_ITERATE(TMessages, it, *tmp.get()) {
        it->m_NoTee = true; // Do not tee duplicate messages to console.
        handler.Post(*it);
        if (it->m_Flags & eDPF_IsConsole) {
            handler.PostToConsole(*it);
        }
    }
    //ERR_POST_X(2, Message << "***** END COLLECTED MESSAGES *****");
    m_Messages.reset(tmp.release());
}


void CDiagContext::DiscardMessages(void)
{
    m_Messages.reset();
}


// Diagnostics setup

static const char* kLogName_None     = "NONE";
static const char* kLogName_Unknown  = "UNKNOWN";
static const char* kLogName_Stdout   = "STDOUT";
static const char* kLogName_Stderr   = "STDERR";
static const char* kLogName_Stream   = "STREAM";
static const char* kLogName_Memory   = "MEMORY";

string GetDefaultLogLocation(CNcbiApplication& app)
{
    static const char* kToolkitRcPath = "/etc/toolkitrc";
    static const char* kWebDirToPort = "Web_dir_to_port";

    string log_path = "/log/";

    string exe_path = CFile(app.GetProgramExecutablePath()).GetDir();
    CNcbiIfstream is(kToolkitRcPath, ios::binary);
    CNcbiRegistry reg(is);
    list<string> entries;
    reg.EnumerateEntries(kWebDirToPort, &entries);
    size_t min_pos = exe_path.length();
    string web_dir;
    // Find the first dir name corresponding to one of the entries
    ITERATE(list<string>, it, entries) {
        if (!it->empty()  &&  (*it)[0] != '/') {
            // not an absolute path
            string mask = "/" + *it;
            if (mask[mask.length() - 1] != '/') {
                mask += "/";
            }
            size_t pos = exe_path.find(mask);
            if (pos < min_pos) {
                min_pos = pos;
                web_dir = *it;
            }
        }
        else {
            // absolute path
            if (exe_path.substr(0, it->length()) == *it) {
                web_dir = *it;
                break;
            }
        }
    }
    if ( !web_dir.empty() ) {
        return log_path + reg.GetString(kWebDirToPort, web_dir, kEmptyStr);
    }
    // Could not find a valid web-dir entry, use /log/port or empty string
    // to try /log/srv later.
    const TXChar* port = NcbiSys_getenv(_TX("SERVER_PORT"));
    return port ? log_path + string(_T_CSTRING(port)) : kEmptyStr;
}


bool CDiagContext::GetLogTruncate(void)
{
    return TLogTruncateParam::GetDefault();
}


void CDiagContext::SetLogTruncate(bool value)
{
    TLogTruncateParam::SetDefault(value);
}


ios::openmode s_GetLogOpenMode(void)
{
    return ios::out |
        (CDiagContext::GetLogTruncate() ? ios::trunc : ios::app);
}


bool OpenLogFileFromConfig(CNcbiRegistry* config, string* new_name)
{
    string logname;
    if ( !config ) {
        const TXChar* env_logname =
            NcbiSys_getenv(_TX("NCBI_CONFIG__LOG__FILE"));
        if (env_logname) {
            logname = _T_STDSTRING(env_logname);
        }
    }
    else {
        logname = config->GetString("LOG", "File", kEmptyStr);
    }
    // In eDS_User mode do not use config unless IgnoreEnvArg
    // is set to true.
    if ( !logname.empty() ) {
        if ( TLogNoCreate::GetDefault()  &&  !CDirEntry(logname).Exists() ) {
            return false;
        }
        if ( new_name ) {
            *new_name = logname;
        }
        return SetLogFile(logname, eDiagFile_All, true);
    }
    return false;
}


void CDiagContext::SetUseRootLog(void)
{
    if (s_FinishedSetupDiag) {
        return;
    }
    s_UseRootLog = true;
    // Try to switch to /log/ if available.
    SetupDiag();
}


void CDiagContext::x_FinalizeSetupDiag(void)
{
    _ASSERT(!s_FinishedSetupDiag);
    s_FinishedSetupDiag = true;
}


// Helper function to set log file with forced splitting.
bool SetApplogFile(const string& file_name)
{
    bool old_split = GetSplitLogFile();
    SetSplitLogFile(true);
    bool res = SetLogFile(file_name);
    if ( !res ) {
        SetSplitLogFile(old_split);
    }
    return res;
}


void CDiagContext::SetupDiag(EAppDiagStream       ds,
                             CNcbiRegistry*       config,
                             EDiagCollectMessages collect)
{
    CDiagContext& ctx = GetDiagContext();
    // Initialize message collecting
    if (collect == eDCM_Init) {
        ctx.InitMessages();
    }
    else if (collect == eDCM_InitNoLimit) {
        ctx.InitMessages(size_t(-1));
    }

    bool log_switched = false;
    bool name_changed = true; // By default consider it's a new name
    bool to_applog = false;
    bool try_root_log_first = false;

    // If env.var 'NCBI_CONFIG__LOG__FILE' is set, use it and ignore all other
    // locations.
    const TXChar* env_ignore =
        NcbiSys_getenv(_TX("NCBI_CONFIG__LOG__IgnoreEnvArg"));
    bool env_ignore_bool = false;
    if (env_ignore) {
        try {
            env_ignore_bool = NStr::StringToBool(_T_STDSTRING(env_ignore));
        }
        catch (CStringException) {
        }
    }
    if (ds != eDS_User  &&  !env_ignore_bool) {
        log_switched = OpenLogFileFromConfig(NULL, NULL);
    }

    if ( !log_switched  &&  config ) {
        try_root_log_first = config->GetBool("LOG", "TryRootLogFirst", false)
            &&  (ds == eDS_ToStdlog  ||  ds == eDS_Default);
        bool force_config = config->GetBool("LOG", "IgnoreEnvArg", false);
        if ( force_config ) {
            try_root_log_first = false;
        }
        if (force_config  ||  (ds != eDS_User  &&  !try_root_log_first)) {
            log_switched = OpenLogFileFromConfig(config, NULL);
        }
    }

    if ( !log_switched ) {
        string old_log_name;
        CDiagHandler* handler = GetDiagHandler();
        if ( handler ) {
            old_log_name = handler->GetLogName();
        }
        CNcbiApplication* app = CNcbiApplication::Instance();

        switch ( ds ) {
        case eDS_ToStdout:
            if (old_log_name != kLogName_Stdout) {
                SetDiagHandler(new CStreamDiagHandler(&cout,
                    true, kLogName_Stdout), true);
                log_switched = true;
            }
            break;
        case eDS_ToStderr:
            if (old_log_name != kLogName_Stderr) {
                SetDiagHandler(new CStreamDiagHandler(&cerr,
                    true, kLogName_Stderr), true);
                log_switched = true;
            }
            break;
        case eDS_ToMemory:
            if (old_log_name != kLogName_Memory) {
                ctx.InitMessages(size_t(-1));
                SetDiagStream(0, false, 0, 0, kLogName_Memory);
                log_switched = true;
            }
            collect = eDCM_NoChange; // prevent flushing to memory
            break;
        case eDS_Disable:
            if (old_log_name != kLogName_None) {
                SetDiagStream(0, false, 0, 0, kLogName_None);
                log_switched = true;
            }
            break;
        case eDS_User:
            // log_switched = true;
            collect = eDCM_Discard;
            break;
        case eDS_AppSpecific:
            if ( app ) {
                app->SetupDiag_AppSpecific(); /* NCBI_FAKE_WARNING */
            }
            collect = eDCM_Discard;
            break;
        case eDS_ToSyslog:
            if (old_log_name != CSysLog::kLogName_Syslog) {
                try {
                    SetDiagHandler(new CSysLog);
                    log_switched = true;
                    break;
                } catch (...) {
                    // fall through
                }
            } else {
                break;
            }
        case eDS_ToStdlog:
        case eDS_Default:
            {
                string log_base = app ?
                    app->GetProgramExecutablePath() : kEmptyStr;
                if ( !log_base.empty() ) {
                    log_base = CFile(log_base).GetBase() + ".log";
                    string log_name;
                    if ( s_UseRootLog ) {
                        string def_log_dir = GetDefaultLogLocation(*app);
                        // Try /log/<port>
                        if ( !def_log_dir.empty() ) {
                            log_name = CFile::ConcatPath(def_log_dir, log_base);
                            if ( SetApplogFile(log_name) ) {
                                log_switched = true;
                                name_changed = log_name != old_log_name;
                                to_applog = true;
                                break;
                            }
                        }
                        // Try /log/srv if port is unknown or not writable
                        log_name = CFile::ConcatPath("/log/srv", log_base);
                        if ( SetApplogFile(log_name) ) {
                            log_switched = true;
                            name_changed = log_name != old_log_name;
                            to_applog = true;
                            break;
                        }
                        if (try_root_log_first &&
                            OpenLogFileFromConfig(config, &log_name)) {
                            log_switched = true;
                            name_changed = log_name != old_log_name;
                            break;
                        }
                        // Try to switch to /log/fallback/
                        log_name = CFile::ConcatPath("/log/fallback/", log_base);
                        if ( SetApplogFile(log_name) ) {
                            log_switched = true;
                            name_changed = log_name != old_log_name;
                            to_applog = true;
                            break;
                        }
                    }
                    // Try cwd/ for eDS_ToStdlog only
                    if (ds == eDS_ToStdlog) {
                        log_name = CFile::ConcatPath(".", log_base);
                        log_switched = SetLogFile(log_name, eDiagFile_All);
                        name_changed = log_name != old_log_name;
                    }
                    if ( !log_switched ) {
                        ERR_POST_X(3, Info << "Failed to set log file to " +
                            CFile::NormalizePath(log_name));
                    }
                }
                else {
                    static const char* kDefaultFallback = "/log/fallback/UNKNOWN";
                    // Try to switch to /log/fallback/UNKNOWN
                    if ( s_UseRootLog ) {
                        if ( SetApplogFile(kDefaultFallback) ) {
                            log_switched = true;
                            name_changed = kDefaultFallback != old_log_name;
                            to_applog = true;
                        }
                        else {
                            ERR_POST_X_ONCE(4, Info <<
                                "Failed to set log file to " <<
                                CFile::NormalizePath(kDefaultFallback));
                        }
                    }
                }
                if (!log_switched  &&  old_log_name != kLogName_Stderr) {
                    SetDiagHandler(new CStreamDiagHandler(&cerr,
                        true, kLogName_Stderr), true);
                    log_switched = true;
                }
                break;
            }
        default:
            ERR_POST_X(5, Warning << "Unknown EAppDiagStream value");
            _ASSERT(0);
            break;
        }
    }

    // Unlock severity level
    SetApplogSeverityLocked(false);
    if ( to_applog ) {
        ctx.SetOldPostFormat(false);
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
        s_MergeLinesSetBySetupDiag = true;
        TLogSizeLimitParam::SetDefault(0); // No log size limit
        SetDiagPostLevel(eDiag_Warning);
        // Lock severity level
        SetApplogSeverityLocked(true);
    }
    else {
        if ( s_MergeLinesSetBySetupDiag ) {
            UnsetDiagPostFlag(eDPF_PreMergeLines);
            UnsetDiagPostFlag(eDPF_MergeLines);
        }
        // Disable throttling
        ctx.SetLogRate_Limit(eLogRate_App, CRequestRateControl::kNoLimit);
        ctx.SetLogRate_Limit(eLogRate_Err, CRequestRateControl::kNoLimit);
        ctx.SetLogRate_Limit(eLogRate_Trace, CRequestRateControl::kNoLimit);
    }
    log_switched &= name_changed;
    CDiagHandler* handler = GetDiagHandler();
    if (collect == eDCM_Flush) {
        // Flush and discard
        if ( log_switched  &&  handler ) {
            ctx.FlushMessages(*handler);
        }
        collect = eDCM_Discard;
    }
    else if (collect == eDCM_NoChange) {
        // Flush but don't discard
        if ( log_switched  &&  handler ) {
            ctx.FlushMessages(*handler);
        }
    }
    if (collect == eDCM_Discard) {
        ctx.DiscardMessages();
    }

    // Refresh rate controls
    ctx.ResetLogRates();
}


CDiagContext::TCount CDiagContext::GetProcessPostNumber(EPostNumberIncrement inc)
{
    static CAtomicCounter s_ProcessPostCount;
    return (TCount)(inc == ePostNumber_Increment ?
        s_ProcessPostCount.Add(1) : s_ProcessPostCount.Get());
}


bool CDiagContext::IsUsingRootLog(void)
{
    return GetLogFile().substr(0, 5) == "/log/";
}


CDiagContext& GetDiagContext(void)
{
    // Make the context live longer than other diag safe-statics
    static CSafeStatic<CDiagContext> s_DiagContext(
        CSafeStaticLifeSpan::eLifeSpan_Long);

    return s_DiagContext.Get();
}


///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Error;
#else
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Warning;
#endif /* else!NDEBUG */

EDiagSevChange CDiagBuffer::sm_PostSeverityChange = eDiagSC_Unknown;
                                                  // to be set on first request

static const TDiagPostFlags s_OldDefaultPostFlags =
    eDPF_Prefix | eDPF_Severity | eDPF_ErrorID | 
    eDPF_ErrCodeMessage | eDPF_ErrCodeExplanation |
    eDPF_ErrCodeUseSeverity;
static const TDiagPostFlags s_NewDefaultPostFlags =
    s_OldDefaultPostFlags |
#if defined(NCBI_THREADS)
    eDPF_TID | eDPF_SerialNo_Thread |
#endif
    eDPF_PID | eDPF_SerialNo;
static TDiagPostFlags s_PostFlags = 0;
static bool s_DiagPostFlagsInitialized = false;

inline
TDiagPostFlags& CDiagBuffer::sx_GetPostFlags(void)
{
    if (!s_DiagPostFlagsInitialized) {
        s_PostFlags = TOldPostFormatParam::GetDefault() ?
            s_OldDefaultPostFlags : s_NewDefaultPostFlags;
        s_DiagPostFlagsInitialized = true;
    }
    return s_PostFlags;
}


TDiagPostFlags& CDiagBuffer::s_GetPostFlags(void)
{
    return sx_GetPostFlags();
}


TDiagPostFlags CDiagBuffer::sm_TraceFlags         = eDPF_Trace;

bool           CDiagBuffer::sm_IgnoreToDie        = false;
EDiagSev       CDiagBuffer::sm_DieSeverity        = eDiag_Fatal;

EDiagTrace     CDiagBuffer::sm_TraceDefault       = eDT_Default;
bool           CDiagBuffer::sm_TraceEnabled;     // to be set on first request


const char*    CDiagBuffer::sm_SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Critical", "Fatal", "Trace" };


void* InitDiagHandler(void)
{
    static bool s_DiagInitialized = false;
    if ( !s_DiagInitialized ) {
        CDiagContext::SetupDiag(eDS_Default, 0, eDCM_Init);
        s_DiagInitialized = true;
    }
    return 0;
}


// MT-safe initialization of the default handler
static CDiagHandler* s_CreateDefaultDiagHandler(void);


// Use s_DefaultHandler only for purposes of comparison, as installing
// another handler will normally delete it.
// NOTE: If SetDiagHandler is never called, the default handler will not
// be destroyed on application termination. This is a tradeoff for having
// the handler always available.
CDiagHandler*      s_DefaultHandler = s_CreateDefaultDiagHandler();
CDiagHandler*      CDiagBuffer::sm_Handler = s_DefaultHandler;
bool               CDiagBuffer::sm_CanDeleteHandler = true;
CDiagErrCodeInfo*  CDiagBuffer::sm_ErrCodeInfo = 0;
bool               CDiagBuffer::sm_CanDeleteErrCodeInfo = false;

// For initialization only
void* s_DiagHandlerInitializer = InitDiagHandler();


static CDiagHandler* s_CreateDefaultDiagHandler(void)
{
    CDiagLock lock(CDiagLock::eWrite);
    static bool s_DefaultDiagHandlerInitialized = false;
    if ( !s_DefaultDiagHandlerInitialized ) {
        s_DefaultDiagHandlerInitialized = true;
        CDiagHandler* handler = new CStreamDiagHandler(&NcbiCerr, true, kLogName_Stderr);
        if ( TTeeToStderr::GetDefault() ) {
            // Need to tee?
            handler = new CTeeDiagHandler(handler, true);
        }
        return handler;
    }
    return s_DefaultHandler;
}



// Note: Intel Thread Checker detects a memory leak at the line:
//       m_Stream(new CNcbiOstrstream) below
//       This is not a fault of the toolkit code as soon as a code like:
//       int main() {
//           ostrstream *  s = new ostrstream;
//           delete s;
//           return 0;
//       }
//       will also report memory leaks.
// Test environment:
// - Intel Thread Checker for Linux 3.1
// - gcc 4.0.1, gcc 4.1.2, icc 10.1
// - Linux64
CDiagBuffer::CDiagBuffer(void)
    : m_Stream(new CNcbiOstrstream),
      m_InitialStreamFlags(m_Stream->flags()),
      m_InUse(false)
{
    m_Diag = 0;
}

CDiagBuffer::~CDiagBuffer(void)
{
#if (_DEBUG > 1)
    if (m_Diag  ||  m_Stream->pcount())
        Abort();
#endif
    delete m_Stream;
    m_Stream = 0;
}

void CDiagBuffer::DiagHandler(SDiagMessage& mess)
{
    bool is_console = (mess.m_Flags & eDPF_IsConsole) > 0;
    bool applog = (mess.m_Flags & eDPF_AppLog) > 0;
    bool is_printable = applog  ||  SeverityPrintable(mess.m_Severity);
    if (!is_console  &&  !is_printable) {
        return;
    }
    if ( CDiagBuffer::sm_Handler ) {
        CDiagLock lock(CDiagLock::eRead);
        if ( CDiagBuffer::sm_Handler ) {
            // The mutex must be locked before approving.
            CDiagBuffer& diag_buf = GetDiagBuffer();
            bool show_warning = false;
            CDiagContext& ctx = GetDiagContext();
            mess.m_Prefix = diag_buf.m_PostPrefix.empty() ?
                0 : diag_buf.m_PostPrefix.c_str();
            if (is_console) {
                // No throttling for console
                CDiagBuffer::sm_Handler->PostToConsole(mess);
                if ( !is_printable ) {
                    return;
                }
            }
            if ( ctx.ApproveMessage(mess, &show_warning) ) {
                CDiagBuffer::sm_Handler->Post(mess);
            }
            else if ( show_warning ) {
                // Substitute the original message with the error.
                // ERR_POST cannot be used here since nested posts
                // are blocked. Have to create the message manually.
                string limit_name = "error";
                CDiagContext::ELogRate_Type limit_type =
                    CDiagContext::eLogRate_Err;
                if ( IsSetDiagPostFlag(eDPF_AppLog, mess.m_Flags) ) {
                    limit_name = "applog";
                    limit_type = CDiagContext::eLogRate_App;
                }
                else if (mess.m_Severity == eDiag_Info ||
                    mess.m_Severity == eDiag_Trace) {
                        limit_name = "trace";
                        limit_type = CDiagContext::eLogRate_Trace;
                }
                string txt = "Maximum logging rate for " + limit_name + " ("
                    + NStr::UIntToString(ctx.GetLogRate_Limit(limit_type))
                    + " messages per "
                    + NStr::UIntToString(ctx.GetLogRate_Period(limit_type))
                    + " sec) exceeded, suspending the output.";
                const CNcbiDiag diag(DIAG_COMPILE_INFO);
                SDiagMessage err_msg(eDiag_Error,
                    txt.c_str(), txt.length(),
                    diag.GetFile(),
                    diag.GetLine(),
                    diag.GetPostFlags(),
                    NULL,
                    err_code_x::eErrCodeX_Corelib_Diag, // Error code
                    23,                                 // Err subcode
                    NULL,
                    diag.GetModule(),
                    diag.GetClass(),
                    diag.GetFunction());
                CDiagBuffer::sm_Handler->Post(err_msg);
                return;
            }
        }
    }
    GetDiagContext().PushMessage(mess);
}


inline
bool CDiagBuffer::SeverityDisabled(EDiagSev sev)
{
    CDiagContextThreadData& thr_data =
        CDiagContextThreadData::GetThreadData();
    CDiagCollectGuard* guard = thr_data.GetCollectGuard();
    EDiagSev post_sev = AdjustApplogPrintableSeverity(sm_PostSeverity);
    bool allow_trace = GetTraceEnabled();
    if ( guard ) {
        post_sev = guard->GetCollectSeverity();
        allow_trace = post_sev == eDiag_Trace;
    }
    if (sev == eDiag_Trace  &&  !allow_trace) {
        return true; // trace is disabled
    }
    if (post_sev == eDiag_Trace  &&  allow_trace) {
        return false; // everything is enabled
    }
    return (sev < post_sev)  &&  (sev < sm_DieSeverity  ||  sm_IgnoreToDie);
}


inline
bool CDiagBuffer::SeverityPrintable(EDiagSev sev)
{
    CDiagContextThreadData& thr_data =
        CDiagContextThreadData::GetThreadData();
    CDiagCollectGuard* guard = thr_data.GetCollectGuard();
    EDiagSev post_sev = AdjustApplogPrintableSeverity(sm_PostSeverity);
    bool allow_trace = GetTraceEnabled();
    if ( guard ) {
        post_sev = AdjustApplogPrintableSeverity(guard->GetPrintSeverity());
        allow_trace = post_sev == eDiag_Trace;
    }
    if (sev == eDiag_Trace  &&  !allow_trace) {
        return false; // trace is disabled
    }
    if (post_sev == eDiag_Trace  &&  allow_trace) {
        return true; // everything is enabled
    }
    return !((sev < post_sev)  &&  (sev < sm_DieSeverity  ||  sm_IgnoreToDie));
}


bool CDiagBuffer::SetDiag(const CNcbiDiag& diag)
{
    if ( m_InUse  ||  !m_Stream ) {
        return false;
    }

    // Check severity level change status
    if ( sm_PostSeverityChange == eDiagSC_Unknown ) {
        GetSeverityChangeEnabledFirstTime();
    }

    EDiagSev sev = diag.GetSeverity();
    bool is_console = (diag.GetPostFlags() & eDPF_IsConsole) > 0;
    if (!is_console  &&  SeverityDisabled(sev)) {
        return false;
    }

    if (m_Diag != &diag) {
        if ( m_Stream->pcount() ) {
            Flush();
        }
        m_Diag = &diag;
    }

    return true;
}


class CRecursionGuard
{
public:
    CRecursionGuard(bool& flag) : m_Flag(flag) { m_Flag = true; }
    ~CRecursionGuard(void) { m_Flag = false; }
private:
    bool& m_Flag;
};


void CDiagBuffer::Flush(void)
{
    if ( m_InUse || !m_Diag ) {
        if (!m_InUse  &&  m_Stream  &&  m_Stream->pcount()) {
            string message(m_Stream->str(), size_t(m_Stream->pcount()));
            m_Stream->rdbuf()->freeze(false);
            // Can not use Reset() without CNcbiDiag.
            m_Stream->rdbuf()->PUBSEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
            _TRACE("Discarding junk data from CDiagBuffer: " << message);
        }
        return;
    }
    CRecursionGuard guard(m_InUse);

    EDiagSev sev = m_Diag->GetSeverity();
    bool is_console = (m_Diag->GetPostFlags() & eDPF_IsConsole) != 0;
    bool is_disabled = SeverityDisabled(sev);
    // Do nothing if diag severity is lower than allowed
    if ((!is_console  &&  is_disabled)  ||  !m_Stream->pcount()) {
        return;
    }

    const char* message = m_Stream->str();
    size_t size = size_t(m_Stream->pcount());
    m_Stream->rdbuf()->freeze(false);

    TDiagPostFlags flags = m_Diag->GetPostFlags();
    if (sev == eDiag_Trace) {
        flags |= sm_TraceFlags;
    } else if (sev == eDiag_Fatal) {
        // normally only happens once, so might as well pull everything
        // in for the record...
        flags |= sm_TraceFlags | eDPF_Trace;
    }

    if (  m_Diag->CheckFilters()  ) {
        string dest;
        if (IsSetDiagPostFlag(eDPF_PreMergeLines, flags)) {
            string src(message, size);
            NStr::Replace(NStr::Replace(src,"\r",""),"\n",";", dest);
            message = dest.c_str();
            size = dest.length();
        }
        SDiagMessage mess(sev, message, size,
                          m_Diag->GetFile(),
                          m_Diag->GetLine(),
                          flags,
                          NULL,
                          m_Diag->GetErrorCode(),
                          m_Diag->GetErrorSubCode(),
                          NULL,
                          m_Diag->GetModule(),
                          m_Diag->GetClass(),
                          m_Diag->GetFunction());
        PrintMessage(mess, *m_Diag);
    }

#if defined(NCBI_COMPILER_KCC)
    // KCC's implementation of "freeze(false)" makes the ostrstream buffer
    // stuck.  We need to replace the frozen stream with the new one.
    delete ostr;
    m_Stream = new CNcbiOstrstream;
#else
    // reset flags to initial value
    m_Stream->flags(m_InitialStreamFlags);
#endif

    Reset(*m_Diag);

    if (sev >= sm_DieSeverity  &&  sev != eDiag_Trace  &&  !sm_IgnoreToDie) {
        m_Diag = 0;

#ifdef NCBI_COMPILER_MSVC
        if ( TAssertOnAbortParam::GetDefault() ) {
            int old_mode = _set_error_mode(_OUT_TO_MSGBOX);
            _ASSERT(false); // Show assertion dialog
            _set_error_mode(old_mode);
        }
        else {
            Abort();
        }
#else  // NCBI_COMPILER_MSVC
        Abort();
#endif // NCBI_COMPILER_MSVC
    }
}


void CDiagBuffer::PrintMessage(SDiagMessage& mess, const CNcbiDiag& diag)
{
    EDiagSev sev = diag.GetSeverity();
    if (!SeverityPrintable(sev)) {
        CDiagContextThreadData& thr_data =
            CDiagContextThreadData::GetThreadData();
        bool can_collect = thr_data.GetCollectGuard() != NULL;
        bool is_console = (diag.GetPostFlags() & eDPF_IsConsole) != 0;
        bool is_disabled = SeverityDisabled(sev);
        if (!is_disabled  ||  (is_console  &&  can_collect)) {
            thr_data.CollectDiagMessage(mess);
            Reset(diag);
            // The message has been collected, don't print to
            // the console now.
            return;
        }
    }
    DiagHandler(mess);
}


bool CDiagBuffer::GetTraceEnabledFirstTime(void)
{
    CDiagLock lock(CDiagLock::eWrite);
    const TXChar* str = NcbiSys_getenv(_T_XCSTRING(DIAG_TRACE));
    if (str  &&  *str) {
        sm_TraceDefault = eDT_Enable;
    } else {
        sm_TraceDefault = eDT_Disable;
    }
    sm_TraceEnabled = (sm_TraceDefault == eDT_Enable);
    return sm_TraceEnabled;
}


bool CDiagBuffer::GetSeverityChangeEnabledFirstTime(void)
{
    CDiagLock lock(CDiagLock::eWrite);
    if ( sm_PostSeverityChange != eDiagSC_Unknown ) {
        return sm_PostSeverityChange == eDiagSC_Enable;
    }
    const TXChar* str = NcbiSys_getenv(_T_XCSTRING(DIAG_POST_LEVEL));
    EDiagSev sev;
    if (str  &&  *str  &&  CNcbiDiag::StrToSeverityLevel(_T_CSTRING(str), sev)) {
        SetDiagFixedPostLevel(sev);
    } else {
        sm_PostSeverityChange = eDiagSC_Enable;
    }
    return sm_PostSeverityChange == eDiagSC_Enable;
}


void CDiagBuffer::UpdatePrefix(void)
{
    m_PostPrefix.erase();
    ITERATE(TPrefixList, prefix, m_PrefixList) {
        if (prefix != m_PrefixList.begin()) {
            m_PostPrefix += "::";
        }
        m_PostPrefix += *prefix;
    }
}


///////////////////////////////////////////////////////
//  CDiagMessage::


SDiagMessage::SDiagMessage(EDiagSev severity,
                           const char* buf, size_t len,
                           const char* file, size_t line,
                           TDiagPostFlags flags, const char* prefix,
                           int err_code, int err_subcode,
                           const char* err_text,
                           const char* module, 
                           const char* nclass, 
                           const char* function)
    : m_Event(eEvent_Start),
      m_TypedExtra(false),
      m_NoTee(false),
      m_Data(0),
      m_Format(eFormat_Auto),
      m_AllowBadExtraNames(false)
{
    m_Severity   = severity;
    m_Buffer     = buf;
    m_BufferLen  = len;
    m_File       = file;
    m_Line       = line;
    m_Flags      = flags;
    m_Prefix     = prefix;
    m_ErrCode    = err_code;
    m_ErrSubCode = err_subcode;
    m_ErrText    = err_text;
    m_Module     = module;
    m_Class      = nclass;
    m_Function   = function;

    CDiagContextThreadData& thr_data =
        CDiagContextThreadData::GetThreadData();
    CRequestContext& rq_ctx = thr_data.GetRequestContext();
    m_PID = CDiagContext::GetPID();
    m_TID = thr_data.GetTID();
    EDiagAppState app_state = GetAppState();
    switch (app_state) {
    case eDiagAppState_RequestBegin:
    case eDiagAppState_Request:
    case eDiagAppState_RequestEnd:
        if ( rq_ctx.GetAutoIncRequestIDOnPost() ) {
            rq_ctx.SetRequestID();
        }
        m_RequestId = rq_ctx.GetRequestID();
        break;
    default:
        m_RequestId = 0;
    }
    m_ProcPost = CDiagContext::GetProcessPostNumber(ePostNumber_Increment);
    m_ThrPost = thr_data.GetThreadPostNumber(ePostNumber_Increment);
}


SDiagMessage::SDiagMessage(const string& message, bool* result)
    : m_Severity(eDiagSevMin),
      m_Buffer(0),
      m_BufferLen(0),
      m_File(0),
      m_Module(0),
      m_Class(0),
      m_Function(0),
      m_Line(0),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Flags(0),
      m_Prefix(0),
      m_ErrText(0),
      m_PID(0),
      m_TID(0),
      m_ProcPost(0),
      m_ThrPost(0),
      m_RequestId(0),
      m_Event(eEvent_Start),
      m_TypedExtra(false),
      m_NoTee(false),
      m_Data(0),
      m_Format(eFormat_Auto),
      m_AllowBadExtraNames(false)
{
    bool res = ParseMessage(message);
    if ( result ) {
        *result = res;
    }
}


SDiagMessage::~SDiagMessage(void)
{
    if ( m_Data ) {
        delete m_Data;
    }
}


SDiagMessage::SDiagMessage(const SDiagMessage& message)
    : m_Severity(eDiagSevMin),
      m_Buffer(0),
      m_BufferLen(0),
      m_File(0),
      m_Module(0),
      m_Class(0),
      m_Function(0),
      m_Line(0),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Flags(0),
      m_Prefix(0),
      m_ErrText(0),
      m_PID(0),
      m_TID(0),
      m_ProcPost(0),
      m_ThrPost(0),
      m_RequestId(0),
      m_Event(eEvent_Start),
      m_TypedExtra(false),
      m_NoTee(false),
      m_Data(0),
      m_Format(eFormat_Auto),
      m_AllowBadExtraNames(false)
{
    *this = message;
}


SDiagMessage& SDiagMessage::operator=(const SDiagMessage& message)
{
    if (&message != this) {
        m_Format = message.m_Format;
        m_AllowBadExtraNames = message.m_AllowBadExtraNames;
        if ( message.m_Data ) {
            m_Data = new SDiagMessageData(*message.m_Data);
            m_Data->m_Host = message.m_Data->m_Host;
            m_Data->m_Client = message.m_Data->m_Client;
            m_Data->m_Session = message.m_Data->m_Session;
            m_Data->m_AppName = message.m_Data->m_AppName;
            m_Data->m_AppState = message.m_Data->m_AppState;
        }
        else {
            x_SaveContextData();
            if (message.m_Buffer) {
                m_Data->m_Message =
                    string(message.m_Buffer, message.m_BufferLen);
            }
            if ( message.m_File ) {
                m_Data->m_File = message.m_File;
            }
            if ( message.m_Module ) {
                m_Data->m_Module = message.m_Module;
            }
            if ( message.m_Class ) {
                m_Data->m_Class = message.m_Class;
            }
            if ( message.m_Function ) {
                m_Data->m_Function = message.m_Function;
            }
            if ( message.m_Prefix ) {
                m_Data->m_Prefix = message.m_Prefix;
            }
            if ( message.m_ErrText ) {
                m_Data->m_ErrText = message.m_ErrText;
            }
        }
        m_Severity = message.m_Severity;
        m_Line = message.m_Line;
        m_ErrCode = message.m_ErrCode;
        m_ErrSubCode = message.m_ErrSubCode;
        m_Flags = message.m_Flags;
        m_PID = message.m_PID;
        m_TID = message.m_TID;
        m_ProcPost = message.m_ProcPost;
        m_ThrPost = message.m_ThrPost;
        m_RequestId = message.m_RequestId;
        m_Event = message.m_Event;
        m_TypedExtra = message.m_TypedExtra;
        m_ExtraArgs.assign(message.m_ExtraArgs.begin(),
            message.m_ExtraArgs.end());

        m_Buffer = m_Data->m_Message.empty() ? 0 : m_Data->m_Message.c_str();
        m_BufferLen = m_Data->m_Message.empty() ? 0 : m_Data->m_Message.length();
        m_File = m_Data->m_File.empty() ? 0 : m_Data->m_File.c_str();
        m_Module = m_Data->m_Module.empty() ? 0 : m_Data->m_Module.c_str();
        m_Class = m_Data->m_Class.empty() ? 0 : m_Data->m_Class.c_str();
        m_Function = m_Data->m_Function.empty()
            ? 0 : m_Data->m_Function.c_str();
        m_Prefix = m_Data->m_Prefix.empty() ? 0 : m_Data->m_Prefix.c_str();
        m_ErrText = m_Data->m_ErrText.empty() ? 0 : m_Data->m_ErrText.c_str();
    }
    return *this;
}


Uint8 s_ParseInt(const string& message,
                 size_t&       pos,    // start position
                 size_t        width,  // fixed width or 0
                 char          sep)    // trailing separator (throw if not found)
{
    if (pos >= message.length()) {
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    Uint8 ret = 0;
    if (width > 0) {
        if (message[pos + width] != sep) {
            NCBI_THROW(CException, eUnknown,
                "Missing separator after integer");
        }
    }
    else {
        width = message.find(sep, pos);
        if (width == NPOS) {
            NCBI_THROW(CException, eUnknown,
                "Missing separator after integer");
        }
        width -= pos;
    }

    ret = NStr::StringToUInt8(CTempString(message.c_str() + pos, width));
    pos += width + 1;
    return ret;
}


CTempString s_ParseStr(const string& message,
                       size_t&       pos,              // start position
                       char          sep,              // separator
                       bool          optional = false) // do not throw if not found
{
    if (pos >= message.length()) {
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    size_t pos1 = pos;
    pos = message.find(sep, pos1);
    if (pos == NPOS) {
        if ( !optional ) {
            NCBI_THROW(CException, eUnknown,
                "Failed to parse diagnostic message");
        }
        pos = pos1;
        return kEmptyStr;
    }
    if ( pos == pos1 + 1  &&  !optional ) {
        // The separator is in the next position, no empty string allowed
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    // remember end position of the string, skip separators
    size_t pos2 = pos;
    pos = message.find_first_not_of(sep, pos);
    if (pos == NPOS) {
        pos = message.length();
    }
    return CTempString(message.c_str() + pos1, pos2 - pos1);
}


static const char s_ExtraEncodeChars[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "\"",  "#",   "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   ";",   "<",   "%3D", ">",   "?",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",
    "`",   "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};


inline
bool x_IsEncodableChar(char c)
{
    return s_ExtraEncodeChars[(unsigned char)c][0] != c  ||
        s_ExtraEncodeChars[(unsigned char)c][1] != 0;
}


class CExtraDecoder : public IStringDecoder
{
public:
    virtual string Decode(const CTempString& src, EStringType stype) const;
};


string CExtraDecoder::Decode(const CTempString& src, EStringType stype) const
{
    string str = src; // NStr::TruncateSpaces(src);
    size_t len = str.length();
    if ( !len  &&  stype == eName ) {
        NCBI_THROW2(CStringException, eFormat,
            "Empty name in extra-arg", 0);
    }

    size_t dst = 0;
    for (size_t p = 0;  p < len;  dst++) {
        switch ( str[p] ) {
        case '%': {
            if (p + 2 > len) {
                NCBI_THROW2(CStringException, eFormat,
                    "Inavild char in extra arg", p);
            }
            int n1 = NStr::HexChar(str[p+1]);
            int n2 = NStr::HexChar(str[p+2]);
            if (n1 < 0 || n2 < 0) {
                NCBI_THROW2(CStringException, eFormat,
                    "Inavild char in extra arg", p);
            }
            str[dst] = (n1 << 4) | n2;
            p += 3;
            break;
        }
        case '+':
            str[dst] = ' ';
            p++;
            break;
        default:
            str[dst] = str[p++];
            if ( x_IsEncodableChar(str[dst]) ) {
                NCBI_THROW2(CStringException, eFormat,
                    "Unencoded special char in extra arg", p);
            }
        }
    }
    if (dst < len) {
        str[dst] = '\0';
        str.resize(dst);
    }
    return str;
}


bool SDiagMessage::x_ParseExtraArgs(const string& str, size_t pos)
{
    m_ExtraArgs.clear();
    if (str.find('&', pos) == NPOS  &&  str.find('=', pos) == NPOS) {
        return false;
    }
    CStringPairs<TExtraArgs> parser("&", "=", new CExtraDecoder());
    try {
        parser.Parse(CTempString(str.c_str() + pos));
    }
    catch (CStringException) {
        string n, v;
        NStr::SplitInTwo(CTempString(str.c_str() + pos), "=", n, v);
        // Try to decode only the name, leave the value as-is.
        try {
            n = parser.GetDecoder()->Decode(n, CExtraDecoder::eName);
            if (n == kExtraTypeArgName) {
                m_TypedExtra = true;
            }
            m_ExtraArgs.push_back(TExtraArg(n, v));
            return true;
        }
        catch (CStringException) {
            return false;
        }
    }
    ITERATE(TExtraArgs, it, parser.GetPairs()) {
        if (it->first == kExtraTypeArgName) {
            m_TypedExtra = true;
        }
        m_ExtraArgs.push_back(TExtraArg(it->first, it->second));
    }
    return true;
}


bool SDiagMessage::ParseMessage(const string& message)
{
    m_Severity = eDiagSevMin;
    m_Buffer = 0;
    m_BufferLen = 0;
    m_File = 0;
    m_Module = 0;
    m_Class = 0;
    m_Function = 0;
    m_Line = 0;
    m_ErrCode = 0;
    m_ErrSubCode = 0;
    m_Flags = 0;
    m_Prefix = 0;
    m_ErrText = 0;
    m_PID = 0;
    m_TID = 0;
    m_ProcPost = 0;
    m_ThrPost = 0;
    m_RequestId = 0;
    m_Event = eEvent_Start;
    m_TypedExtra = false;
    m_Format = eFormat_Auto;
    if ( m_Data ) {
        delete m_Data;
        m_Data = 0;
    }
    m_Data = new SDiagMessageData;

    size_t pos = 0;
    try {
        // Fixed prefix
        m_PID = s_ParseInt(message, pos, 0, '/');
        m_TID = s_ParseInt(message, pos, 0, '/');
        size_t sl_pos = message.find('/', pos);
        size_t sp_pos = message.find(' ', pos);
        if (sl_pos < sp_pos) {
            // Newer format, app state is present.
            m_RequestId = s_ParseInt(message, pos, 0, '/');
            m_Data->m_AppState =
                s_StrToAppState(s_ParseStr(message, pos, ' ', true));
        }
        else {
            // Older format, no app state.
            m_RequestId = s_ParseInt(message, pos, 0, ' ');
            m_Data->m_AppState = eDiagAppState_AppRun;
        }

        if (message[pos + kDiagW_UID] != ' ') {
            return false;
        }
        m_Data->m_UID = NStr::StringToUInt8(
            CTempString(message.c_str() + pos, kDiagW_UID), 0, 16);
        pos += kDiagW_UID + 1;
        
        m_ProcPost = s_ParseInt(message, pos, 0, '/');
        m_ThrPost = s_ParseInt(message, pos, 0, ' ');

        // Date and time. Try all known formats.
        CTempString tmp = s_ParseStr(message, pos, ' ');
        static const char* s_TimeFormats[4] = {
            "Y/M/D:h:m:s", "Y-M-DTh:m:s", "Y-M-DTh:m:s.l", kDiagTimeFormat
        };
        if (tmp.find('T') == NPOS) {
            m_Data->m_Time = CTime(tmp, s_TimeFormats[0]);
        }
        else if (tmp.find('.') == NPOS) {
            m_Data->m_Time = CTime(tmp, s_TimeFormats[1]);
        }
        else {
            try {
                m_Data->m_Time = CTime(tmp, s_TimeFormats[2]);
            }
            catch (CTimeException) {
                m_Data->m_Time = CTime(tmp, s_TimeFormats[3]);
            }
        }

        // Host
        m_Data->m_Host = s_ParseStr(message, pos, ' ');
        if (m_Data->m_Host == kUnknown_Host) {
            m_Data->m_Host.clear();
        }
        // Client
        m_Data->m_Client = s_ParseStr(message, pos, ' ');
        if (m_Data->m_Client == kUnknown_Client) {
            m_Data->m_Client.clear();
        }
        // Session ID
        m_Data->m_Session = s_ParseStr(message, pos, ' ');
        if (m_Data->m_Session == kUnknown_Session) {
            m_Data->m_Session.clear();
        }
        // Application name
        m_Data->m_AppName = s_ParseStr(message, pos, ' ');
        if (m_Data->m_AppName == kUnknown_App) {
            m_Data->m_AppName.clear();
        }

        // Severity or event type
        bool have_severity = false;
        size_t severity_pos = pos;
        tmp = s_ParseStr(message, pos, ':', true);
        if ( !tmp.empty() ) {
            if (tmp.length() == 10  &&  tmp.find("Message[") == 0) {
                // Get the real severity
                switch ( tmp[8] ) {
                case 'T':
                    m_Severity = eDiag_Trace;
                    break;
                case 'I':
                    m_Severity = eDiag_Info;
                    break;
                case 'W':
                    m_Severity = eDiag_Warning;
                    break;
                case 'E':
                    m_Severity = eDiag_Error;
                    break;
                case 'C':
                    m_Severity = eDiag_Critical;
                    break;
                case 'F':
                    m_Severity = eDiag_Fatal;
                    break;
                default:
                    return false;
                }
                m_Flags |= eDPF_IsMessage;
                have_severity = true;
            }
            else {
                have_severity =
                    CNcbiDiag::StrToSeverityLevel(string(tmp).c_str(), m_Severity);
            }
        }
        if ( have_severity ) {
            pos = message.find_first_not_of(' ', pos);
            if (pos == NPOS) {
                pos = message.length();
            }
        }
        else {
            // Check event type rather than severity level
            pos = severity_pos;
            tmp = s_ParseStr(message, pos, ' ', true);
            if (tmp.empty()  &&  severity_pos < message.length()) {
                tmp = CTempString(message.c_str() + severity_pos);
                pos = message.length();
            }
            if (tmp == GetEventName(eEvent_Start)) {
                m_Event = eEvent_Start;
            }
            else if (tmp == GetEventName(eEvent_Stop)) {
                m_Event = eEvent_Stop;
            }
            else if (tmp == GetEventName(eEvent_RequestStart)) {
                m_Event = eEvent_RequestStart;
                if (pos < message.length()) {
                    if ( x_ParseExtraArgs(message, pos) ) {
                        pos = message.length();
                    }
                }
            }
            else if (tmp == GetEventName(eEvent_RequestStop)) {
                m_Event = eEvent_RequestStop;
            }
            else if (tmp == GetEventName(eEvent_Extra)) {
                m_Event = eEvent_Extra;
                if (pos < message.length()) {
                    if ( x_ParseExtraArgs(message, pos) ) {
                        pos = message.length();
                    }
                }
            }
            else if (tmp == GetEventName(eEvent_PerfLog)) {
                m_Event = eEvent_PerfLog;
                if (pos < message.length()) {
                    // Put status and time to the message,
                    // parse all the rest as extra.
                    size_t msg_end = message.find_first_not_of(' ', pos);
                    msg_end = message.find_first_of(' ', msg_end);
                    msg_end = message.find_first_not_of(' ', msg_end);
                    msg_end = message.find_first_of(' ', msg_end);
                    size_t extra_pos = message.find_first_not_of(' ', msg_end);
                    m_Data->m_Message = string(message.c_str() + pos).
                        substr(0, msg_end - pos);
                    m_BufferLen = m_Data->m_Message.length();
                    m_Buffer = m_Data->m_Message.empty() ?
                        0 : &m_Data->m_Message[0];
                    if ( x_ParseExtraArgs(message, extra_pos) ) {
                        pos = message.length();
                    }
                }
            }
            else {
                return false;
            }
            m_Flags |= eDPF_AppLog;
            // The rest is the message (do not parse status, bytes etc.)
            if (pos < message.length()) {
                m_Data->m_Message = message.c_str() + pos;
                m_BufferLen = m_Data->m_Message.length();
                m_Buffer = m_Data->m_Message.empty() ?
                    0 : &m_Data->m_Message[0];
            }
            m_Format = eFormat_New;
            return true;
        }

        // Find message separator
        size_t sep_pos = message.find(" --- ", pos);

        // <module>, <module>(<err_code>.<err_subcode>) or <module>(<err_text>)
        if (pos < sep_pos  &&  message[pos] != '"') {
            size_t mod_pos = pos;
            tmp = s_ParseStr(message, pos, ' ');
            size_t lbr = tmp.find("(");
            if (lbr != NPOS) {
                if (tmp[tmp.length() - 1] != ')') {
                    // Space(s) inside the error text, try to find closing ')'
                    int open_br = 1;
                    while (open_br > 0  &&  pos < message.length()) {
                        if (message[pos] == '(') {
                            open_br++;
                        }
                        else if (message[pos] == ')') {
                            open_br--;
                        }
                        pos++;
                    }
                    if (message[pos] != ' '  ||  pos >= message.length()) {
                        return false;
                    }
                    tmp = CTempString(message.c_str() + mod_pos, pos - mod_pos);
                    // skip space(s)
                    pos = message.find_first_not_of(' ', pos);
                    if (pos == NPOS) {
                        pos = message.length();
                    }
                }
                m_Data->m_Module = tmp.substr(0, lbr);
                tmp = tmp.substr(lbr + 1, tmp.length() - lbr - 2);
                size_t dot_pos = tmp.find('.');
                if (dot_pos != NPOS) {
                    // Try to parse error code/subcode
                    try {
                        m_ErrCode = NStr::StringToInt(tmp.substr(0, dot_pos));
                        m_ErrSubCode = NStr::StringToInt(tmp.substr(dot_pos + 1));
                    }
                    catch (CStringException) {
                        m_ErrCode = 0;
                        m_ErrSubCode = 0;
                    }
                }
                if (!m_ErrCode  &&  !m_ErrSubCode) {
                    m_Data->m_ErrText = tmp;
                    m_ErrText = m_Data->m_ErrText.empty() ?
                        0 : m_Data->m_ErrText.c_str();
                }
            }
            else {
                m_Data->m_Module = tmp;
            }
            if ( !m_Data->m_Module.empty() ) {
                m_Module = m_Data->m_Module.c_str();
            }
        }

        if (pos < sep_pos  &&  message[pos] == '"') {
            // ["<file>", ][line <line>][:]
            pos++; // skip "
            tmp = s_ParseStr(message, pos, '"');
            m_Data->m_File = tmp;
            m_File = m_Data->m_File.empty() ? 0 : m_Data->m_File.c_str();
            if (CTempString(message.c_str() + pos, 7) != ", line ") {
                return false;
            }
            pos += 7;
            m_Line = (size_t)s_ParseInt(message, pos, 0, ':');
            pos = message.find_first_not_of(' ', pos);
            if (pos == NPOS) {
                pos = message.length();
            }
        }

        if (pos < sep_pos) {
            // Class:: Class::Function() ::Function()
            if (message.find("::", pos) != NPOS) {
                size_t tmp_pos = sep_pos;
                while (tmp_pos > pos  &&  message[tmp_pos - 1] == ' ')
                    --tmp_pos;
                tmp.assign(message.data() + pos, tmp_pos - pos);
                size_t dcol = tmp.find("::");
                if (dcol == NPOS) {
                    goto parse_unk_func;
                }
                pos = sep_pos + 1;
                if (dcol > 0) {
                    m_Data->m_Class = tmp.substr(0, dcol);
                    m_Class = m_Data->m_Class.empty() ?
                        0 : m_Data->m_Class.c_str();
                }
                dcol += 2;
                if (dcol < tmp.length() - 2) {
                    // Remove "()"
                    if (tmp[tmp.length() - 2] != '(' || tmp[tmp.length() - 1] != ')') {
                        return false;
                    }
                    m_Data->m_Function = tmp.substr(dcol,
                        tmp.length() - dcol - 2);
                    m_Function = m_Data->m_Function.empty() ?
                        0 : m_Data->m_Function.c_str();
                }
            }
            else {
parse_unk_func:
                size_t unkf = message.find("UNK_FUNC", pos);
                if (unkf == pos) {
                    pos += 9;
                }
            }
        }

        if (CTempString(message.c_str() + pos, 4) == "--- ") {
            pos += 4;
        }

        // All the rest goes to message - no way to parse prefix/error code.
        // [<prefix1>::<prefix2>::.....]
        // <message>
        // <err_code_message> and <err_code_explanation>
        m_Data->m_Message = message.c_str() + pos;
        m_BufferLen = m_Data->m_Message.length();
        m_Buffer = m_Data->m_Message.empty() ? 0 : &m_Data->m_Message[0];
    }
    catch (CException) {
        return false;
    }

    m_Format = eFormat_New;
    return true;
}


void SDiagMessage::ParseDiagStream(CNcbiIstream& in,
                                   INextDiagMessage& func)
{
    string msg_str, line, last_msg_str;
    bool res = false;
    auto_ptr<SDiagMessage> msg;
    auto_ptr<SDiagMessage> last_msg;
    while ( in.good() ) {
        getline(in, line);
        // Dirty check for PID/TID/RID
        if (line.size() < 15) {
            if ( !line.empty() ) {
                msg_str += "\n" + line;
                line.erase();
            }
            continue;
        }
        else {
            for (size_t i = 0; i < 15; i++) {
                if (line[i] != '/'  &&  (line[i] < '0'  ||  line[i] > '9')) {
                    // Not a valid prefix - append to the previous message
                    msg_str += "\n" + line;
                    line.erase();
                    break;
                }
            }
            if ( line.empty() ) {
                continue;
            }
        }
        if ( msg_str.empty() ) {
            msg_str = line;
            continue;
        }
        msg.reset(new SDiagMessage(msg_str, &res));
        if ( res ) {
            if ( last_msg.get() ) {
                func(*last_msg);
            }
            last_msg_str = msg_str;
            last_msg.reset(msg.release());
        }
        else if ( !last_msg_str.empty() ) {
            last_msg_str += "\n" + msg_str;
            last_msg.reset(new SDiagMessage(last_msg_str, &res));
            if ( !res ) {
                ERR_POST_X(19,
                    Error << "Failed to parse message: " << last_msg_str);
            }
        }
        else {
            ERR_POST_X(20, Error << "Failed to parse message: " << msg_str);
        }
        msg_str = line;
    }
    if ( !msg_str.empty() ) {
        msg.reset(new SDiagMessage(msg_str, &res));
        if ( res ) {
            if ( last_msg.get() ) {
                func(*last_msg);
            }
            func(*msg);
        }
        else if ( !last_msg_str.empty() ) {
            last_msg_str += "\n" + msg_str;
            msg.reset(new SDiagMessage(last_msg_str, &res));
            if ( res ) {
                func(*msg);
            }
            else {
                ERR_POST_X(21,
                    Error << "Failed to parse message: " << last_msg_str);
            }
        }
        else {
            ERR_POST_X(22,
                Error << "Failed to parse message: " << msg_str);
        }
    }
}


string SDiagMessage::GetEventName(EEventType event)
{
    switch ( event ) {
    case eEvent_Start:
        return "start";
    case eEvent_Stop:
        return "stop";
    case eEvent_Extra:
        return "extra";
    case eEvent_RequestStart:
        return "request-start";
    case eEvent_RequestStop:
        return "request-stop";
    case eEvent_PerfLog:
        return "perf";
    }
    return kEmptyStr;
}


CDiagContext::TUID SDiagMessage::GetUID(void) const
{
    return m_Data ? m_Data->m_UID : GetDiagContext().GetUID();
}


CTime SDiagMessage::GetTime(void) const
{
    return m_Data ? m_Data->m_Time : s_GetFastTime();
}


inline
bool SDiagMessage::x_IsSetOldFormat(void) const
{
    return m_Format == eFormat_Auto ? GetDiagContext().IsSetOldPostFormat()
        : m_Format == eFormat_Old;
}


void SDiagMessage::Write(string& str, TDiagWriteFlags flags) const
{
    CNcbiOstrstream ostr;
    Write(ostr, flags);
    ostr.put('\0');
    str = ostr.str();
    ostr.rdbuf()->freeze(false);
}


CNcbiOstream& SDiagMessage::Write(CNcbiOstream&   os,
                                  TDiagWriteFlags flags) const
{
    if (IsSetDiagPostFlag(eDPF_MergeLines, m_Flags)) {
        CNcbiOstrstream ostr;
        string str;
        x_Write(ostr, fNoEndl);
        ostr.put('\0');
        str = ostr.str();
        ostr.rdbuf()->freeze(false);
        if (str.find_first_of("\r\n") != NPOS) {
            list<string> lines;
            NStr::Split(str, "\r\n", lines);
            str = NStr::Join(lines, " ");
        }
        os << str;
        if ((flags & fNoEndl) == 0) {
            os << NcbiEndl;
        }
        return os;
    } else {
        return x_Write(os, flags);
    }
}


CNcbiOstream& SDiagMessage::x_Write(CNcbiOstream& os,
                                    TDiagWriteFlags flags) const
{
    CNcbiOstream& res =
        x_IsSetOldFormat() ? x_OldWrite(os, flags) : x_NewWrite(os, flags);
    return res;
}


string SDiagMessage::x_GetModule(void) const
{
    if ( m_Module && *m_Module ) {
        return string(m_Module);
    }
    if ( x_IsSetOldFormat() ) {
        return kEmptyStr;
    }
    if ( !m_File || !(*m_File) ) {
        return kEmptyStr;
    }
    char sep_chr = CDirEntry::GetPathSeparator();
    const char* mod_start = 0;
    const char* mod_end = m_File;
    const char* c = strchr(m_File, sep_chr);
    while (c  &&  *c) {
        if (c > mod_end) {
            mod_start = mod_end;
            mod_end = c;
        }
        c = strchr(c + 1, sep_chr);
    }
    if ( !mod_start ) {
        mod_start = m_File;
    }
    while (*mod_start == sep_chr) {
        mod_start++;
    }
    if (mod_end < mod_start + 1) {
        return kEmptyStr;
    }
    string ret(mod_start, mod_end - mod_start);
    NStr::ToUpper(ret);
    return ret;
}


class CExtraEncoder : public IStringEncoder
{
public:
    CExtraEncoder(bool allow_bad_names = false) : m_AllowBadNames(allow_bad_names) {}

    virtual string Encode(const CTempString& src, EStringType stype) const;

private:
    bool m_AllowBadNames;
};


string CExtraEncoder::Encode(const CTempString& src, EStringType stype) const
{
    if (stype == eName  &&  !m_AllowBadNames) {
        // Just check the source string, it may contain only valid chars
        ITERATE(CTempString, c, src) {
            const char* enc = s_ExtraEncodeChars[(unsigned char)(*c)];
            if (enc[1] != 0  ||  enc[0] != *c) {
                NCBI_THROW(CCoreException, eInvalidArg,
                    "Invalid char in extra args name: " + string(src));
            }
        }
        return src;
    }
    // Encode value (or name if m_AllowBadNames is set).
    string dst;
    ITERATE(CTempString, c, src) {
        dst += s_ExtraEncodeChars[(unsigned char)(*c)];
    }
    return dst;
}


string SDiagMessage::FormatExtraMessage(void) const
{
    return CStringPairs<TExtraArgs>::Merge(m_ExtraArgs,
        "&", "=", new CExtraEncoder(m_AllowBadExtraNames));
}


CNcbiOstream& SDiagMessage::x_OldWrite(CNcbiOstream& os,
                                       TDiagWriteFlags flags) const
{
    // Date & time
    if (IsSetDiagPostFlag(eDPF_DateTime, m_Flags)) {
        os << CFastLocalTime().GetLocalTime().AsString("M/D/y h:m:s ");
    }
    // "<file>"
    bool print_file = (m_File  &&  *m_File  &&
                       IsSetDiagPostFlag(eDPF_File, m_Flags));
    if ( print_file ) {
        const char* x_file = m_File;
        if ( !IsSetDiagPostFlag(eDPF_LongFilename, m_Flags) ) {
            for (const char* s = m_File;  *s;  s++) {
                if (*s == '/'  ||  *s == '\\'  ||  *s == ':')
                    x_file = s + 1;
            }
        }
        os << '"' << x_file << '"';
    }

    // , line <line>
    bool print_line = (m_Line  &&  IsSetDiagPostFlag(eDPF_Line, m_Flags));
    if ( print_line )
        os << (print_file ? ", line " : "line ") << m_Line;

    // :
    if (print_file  ||  print_line)
        os << ": ";

    // Get error code description
    bool have_description = false;
    SDiagErrCodeDescription description;
    if ((m_ErrCode  ||  m_ErrSubCode)  &&
        (IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags)  || 
         IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags)  ||
         IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags))  &&
         IsSetDiagErrCodeInfo()) {

        CDiagErrCodeInfo* info = GetDiagErrCodeInfo();
        if ( info  && 
             info->GetDescription(ErrCode(m_ErrCode, m_ErrSubCode), 
                                  &description) ) {
            have_description = true;
            if (IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags) && 
                description.m_Severity != -1 )
                m_Severity = (EDiagSev)description.m_Severity;
        }
    }

    // <severity>:
    if (IsSetDiagPostFlag(eDPF_Severity, m_Flags)  &&
        (m_Severity != eDiag_Info || !IsSetDiagPostFlag(eDPF_OmitInfoSev))) {
        if ( IsSetDiagPostFlag(eDPF_IsMessage, m_Flags) ) {
            os << "Message: ";
        }
        else {
            os << CNcbiDiag::SeverityName(m_Severity) << ": ";
        }
    }

    // (<err_code>.<err_subcode>) or (err_text)
    if ((m_ErrCode  ||  m_ErrSubCode || m_ErrText)  &&
        IsSetDiagPostFlag(eDPF_ErrorID, m_Flags)) {
        os << '(';
        if (m_ErrText) {
            os << m_ErrText;
        } else {
            os << m_ErrCode << '.' << m_ErrSubCode;
        }
        os << ") ";
    }

    // Module::Class::Function -
    bool have_module = (m_Module && *m_Module);
    bool print_location =
        ( have_module ||
         (m_Class     &&  *m_Class ) ||
         (m_Function  &&  *m_Function))
        && IsSetDiagPostFlag(eDPF_Location, m_Flags);

    bool need_separator = false;
    if (print_location) {
        // Module:: Module::Class Module::Class::Function()
        // ::Class ::Class::Function()
        // Module::Function() Function()
        bool need_double_colon = false;

        if ( have_module ) {
            os << x_GetModule();
            need_double_colon = true;
        }

        if (m_Class  &&  *m_Class) {
            if (need_double_colon)
                os << "::";
            os << m_Class;
            need_double_colon = true;
        }

        if (m_Function  &&  *m_Function) {
            if (need_double_colon)
                os << "::";
            need_double_colon = false;
            os << m_Function << "()";
        }

        if( need_double_colon )
            os << "::";

        os << " ";
        need_separator = true;
    }

    bool err_text_prefix = (IsSetDiagPostFlag(eDPF_ErrCodeMsgInFront));
    if (err_text_prefix  &&  have_description  &&
        IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags) &&
        !description.m_Message.empty()) {
        os << "{" << description.m_Message << "} ";
        need_separator = true;
    }

    if (need_separator) {
        os << "- ";
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if (m_BufferLen)
        os.write(m_Buffer, m_BufferLen);

    // <err_code_message> and <err_code_explanation>
    if (have_description) {
        if (!err_text_prefix  &&
            IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags) &&
            !description.m_Message.empty())
            os << NcbiEndl << description.m_Message;
        if (IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags) &&
            !description.m_Explanation.empty())
            os << NcbiEndl << description.m_Explanation;
    }

    // Endl
    if ((flags & fNoEndl) == 0) {
        os << NcbiEndl;
    }

    return os;
}


CNcbiOstream& SDiagMessage::x_NewWrite(CNcbiOstream& os,
                                       TDiagWriteFlags flags) const
{
    if ((flags & fNoPrefix) == 0) {
        GetDiagContext().WriteStdPrefix(os, *this);
    }

    // Get error code description
    bool have_description = false;
    SDiagErrCodeDescription description;
    if ((m_ErrCode  ||  m_ErrSubCode)  &&
        IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags)  &&
        IsSetDiagErrCodeInfo()) {

        CDiagErrCodeInfo* info = GetDiagErrCodeInfo();
        if ( info  && 
             info->GetDescription(ErrCode(m_ErrCode, m_ErrSubCode), 
                                  &description) ) {
            have_description = true;
            if (description.m_Severity != -1)
                m_Severity = (EDiagSev)description.m_Severity;
        }
    }

    // <severity>:
    if ( IsSetDiagPostFlag(eDPF_AppLog, m_Flags) ) {
        os << setfill(' ') << setw(13) << setiosflags(IOS_BASE::left)
            << GetEventName(m_Event) << resetiosflags(IOS_BASE::left)
            << setw(0);
    }
    else {
        string sev = CNcbiDiag::SeverityName(m_Severity);
        os << setfill(' ') << setw(13) // add 1 for space
            << setiosflags(IOS_BASE::left) << setw(0);
        if ( IsSetDiagPostFlag(eDPF_IsMessage, m_Flags) ) {
            os << "Message[" << sev[0] << "]:";
        }
        else {
            os << sev << ':';
        }
        os << resetiosflags(IOS_BASE::left);
    }
    os << ' ';

    // <module>-<err_code>.<err_subcode> or <module>-<err_text>
    bool have_module = (m_Module && *m_Module) || (m_File && *m_File);
    bool print_err_id = have_module || m_ErrCode || m_ErrSubCode || m_ErrText;

    if (print_err_id) {
        os << (have_module ? x_GetModule() : "UNK_MODULE");
        if (m_ErrCode  ||  m_ErrSubCode || m_ErrText) {
            if (m_ErrText) {
                os << '(' << m_ErrText << ')';
            } else {
                os << '(' << m_ErrCode << '.' << m_ErrSubCode << ')';
            }
        }
        os << ' ';
    }

    // "<file>"
    if ( !IsSetDiagPostFlag(eDPF_AppLog, m_Flags) ) {
        bool print_file = m_File  &&  *m_File;
        if ( print_file ) {
            const char* x_file = m_File;
            if ( !IsSetDiagPostFlag(eDPF_LongFilename, m_Flags) ) {
                for (const char* s = m_File;  *s;  s++) {
                    if (*s == '/'  ||  *s == '\\'  ||  *s == ':')
                        x_file = s + 1;
                }
            }
            os << '"' << x_file << '"';
        }
        else {
            os << "\"UNK_FILE\"";
        }
        // , line <line>
        os << ", line " << m_Line;
        os << ": ";

        // Class::Function
        bool print_loc = (m_Class && *m_Class ) || (m_Function && *m_Function);
        if (print_loc) {
            // Class:: Class::Function() ::Function()
            if (m_Class  &&  *m_Class) {
                os << m_Class;
            }
            os << "::";
            if (m_Function  &&  *m_Function) {
                os << m_Function << "() ";
            }
        }
        else {
            os << "UNK_FUNC ";
        }

        if ( !IsSetDiagPostFlag(eDPF_OmitSeparator, m_Flags)  &&
            !IsSetDiagPostFlag(eDPF_AppLog, m_Flags) ) {
            os << "--- ";
        }
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if (m_BufferLen) {
        os.write(m_Buffer, m_BufferLen);
    }

    if ( IsSetDiagPostFlag(eDPF_AppLog, m_Flags) ) {
        if ( !m_ExtraArgs.empty() ) {
            if ( m_BufferLen ) {
                os << ' ';
            }
            os << FormatExtraMessage();
        }
    }

    // <err_code_message> and <err_code_explanation>
    if (have_description) {
        if (IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags) &&
            !description.m_Message.empty())
            os << NcbiEndl << description.m_Message << ' ';
        if (IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags) &&
            !description.m_Explanation.empty())
            os << NcbiEndl << description.m_Explanation;
    }

    // Endl
    if ((flags & fNoEndl) == 0) {
        os << NcbiEndl;
    }

    return os;
}


void SDiagMessage::x_InitData(void) const
{
    if ( !m_Data ) {
        m_Data = new SDiagMessageData;
    }
    if (m_Data->m_Message.empty()  &&  m_Buffer) {
        m_Data->m_Message = string(m_Buffer, m_BufferLen);
    }
    if (m_Data->m_File.empty()  &&  m_File) {
        m_Data->m_File = m_File;
    }
    if (m_Data->m_Module.empty()  &&  m_Module) {
        m_Data->m_Module = m_Module;
    }
    if (m_Data->m_Class.empty()  &&  m_Class) {
        m_Data->m_Class = m_Class;
    }
    if (m_Data->m_Function.empty()  &&  m_Function) {
        m_Data->m_Function = m_Function;
    }
    if (m_Data->m_Prefix.empty()  &&  m_Prefix) {
        m_Data->m_Prefix = m_Prefix;
    }
    if (m_Data->m_ErrText.empty()  &&  m_ErrText) {
        m_Data->m_ErrText = m_ErrText;
    }

    if ( !m_Data->m_UID ) {
        m_Data->m_UID = GetDiagContext().GetUID();
    }
    if ( m_Data->m_Time.IsEmpty() ) {
        m_Data->m_Time = s_GetFastTime();
    }
}


void SDiagMessage::x_SaveContextData(void) const
{
    if ( m_Data ) {
        return;
    }
    x_InitData();
    CDiagContext& dctx = GetDiagContext();
    m_Data->m_Host = dctx.GetEncodedHost();
    m_Data->m_AppName = dctx.GetEncodedAppName();
    m_Data->m_AppState = dctx.GetAppState();

    CRequestContext& rctx = dctx.GetRequestContext();
    m_Data->m_Client = rctx.GetClientIP();
    m_Data->m_Session = dctx.GetEncodedSessionID();
}


const string& SDiagMessage::GetHost(void) const
{
    if ( m_Data ) {
        return m_Data->m_Host;
    }
    return GetDiagContext().GetEncodedHost();
}


const string& SDiagMessage::GetClient(void) const
{
    return m_Data ? m_Data->m_Client
        : CDiagContext::GetRequestContext().GetClientIP();
}


const string& SDiagMessage::GetSession(void) const
{
    return m_Data ? m_Data->m_Session
        : GetDiagContext().GetEncodedSessionID();
}


const string& SDiagMessage::GetAppName(void) const
{
    if ( m_Data ) {
        return m_Data->m_AppName;
    }
    return GetDiagContext().GetEncodedAppName();
}


EDiagAppState SDiagMessage::GetAppState(void) const
{
    return m_Data ? m_Data->m_AppState : GetDiagContext().GetAppState();
}


///////////////////////////////////////////////////////
//  CDiagAutoPrefix::

CDiagAutoPrefix::CDiagAutoPrefix(const string& prefix)
{
    PushDiagPostPrefix(prefix.c_str());
}

CDiagAutoPrefix::CDiagAutoPrefix(const char* prefix)
{
    PushDiagPostPrefix(prefix);
}

CDiagAutoPrefix::~CDiagAutoPrefix(void)
{
    PopDiagPostPrefix();
}


///////////////////////////////////////////////////////
//  EXTERN


static TDiagPostFlags s_SetDiagPostAllFlags(TDiagPostFlags& flags,
                                            TDiagPostFlags  new_flags)
{
    CDiagLock lock(CDiagLock::eWrite);

    TDiagPostFlags prev_flags = flags;
    new_flags &= ~eDPF_AtomicWrite;
    if (new_flags & eDPF_Default) {
        new_flags |= prev_flags;
        new_flags &= ~eDPF_Default;
    }
    flags = new_flags;
    return prev_flags;
}


static void s_SetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CDiagLock lock(CDiagLock::eWrite);
    flags |= flag;
    // Assume flag is set by user
    s_MergeLinesSetBySetupDiag = false;
}


static void s_UnsetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CDiagLock lock(CDiagLock::eWrite);
    flags &= ~flag;
    // Assume flag is set by user
    s_MergeLinesSetBySetupDiag = false;
}


extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags)
{
    return s_SetDiagPostAllFlags(CDiagBuffer::sx_GetPostFlags(), flags);
}

extern void SetDiagPostFlag(EDiagPostFlag flag)
{
    s_SetDiagPostFlag(CDiagBuffer::sx_GetPostFlags(), flag);
}

extern void UnsetDiagPostFlag(EDiagPostFlag flag)
{
    s_UnsetDiagPostFlag(CDiagBuffer::sx_GetPostFlags(), flag);
}


extern TDiagPostFlags SetDiagTraceAllFlags(TDiagPostFlags flags)
{
    return s_SetDiagPostAllFlags(CDiagBuffer::sm_TraceFlags, flags);
}

extern void SetDiagTraceFlag(EDiagPostFlag flag)
{
    s_SetDiagPostFlag(CDiagBuffer::sm_TraceFlags, flag);
}

extern void UnsetDiagTraceFlag(EDiagPostFlag flag)
{
    s_UnsetDiagPostFlag(CDiagBuffer::sm_TraceFlags, flag);
}


extern void SetDiagPostPrefix(const char* prefix)
{
    CDiagBuffer& buf = GetDiagBuffer();
    if ( prefix ) {
        buf.m_PostPrefix = prefix;
    } else {
        buf.m_PostPrefix.erase();
    }
    buf.m_PrefixList.clear();
}


extern void PushDiagPostPrefix(const char* prefix)
{
    if (prefix  &&  *prefix) {
        CDiagBuffer& buf = GetDiagBuffer();
        buf.m_PrefixList.push_back(prefix);
        buf.UpdatePrefix();
    }
}


extern void PopDiagPostPrefix(void)
{
    CDiagBuffer& buf = GetDiagBuffer();
    if ( !buf.m_PrefixList.empty() ) {
        buf.m_PrefixList.pop_back();
        buf.UpdatePrefix();
    }
}


extern EDiagSev SetDiagPostLevel(EDiagSev post_sev)
{
    if (post_sev < eDiagSevMin  ||  post_sev > eDiagSevMax) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "SetDiagPostLevel() -- Severity must be in the range "
                   "[eDiagSevMin..eDiagSevMax]");
    }

    CDiagLock lock(CDiagLock::eWrite);
    EDiagSev sev = CDiagBuffer::sm_PostSeverity;
    if ( CDiagBuffer::sm_PostSeverityChange != eDiagSC_Disable) {
        if (post_sev == eDiag_Trace) {
            // special case
            SetDiagTrace(eDT_Enable);
            post_sev = eDiag_Info;
        }
        CDiagBuffer::sm_PostSeverity = post_sev;
    }
    return sev;
}


extern int CompareDiagPostLevel(EDiagSev sev1, EDiagSev sev2)
{
    if (sev1 == sev2) return 0;
    if (sev1 == eDiag_Trace) return -1;
    if (sev2 == eDiag_Trace) return 1;
    return sev1 - sev2;
}


extern bool IsVisibleDiagPostLevel(EDiagSev sev)
{
    if (sev == eDiag_Trace) {
        return CDiagBuffer::GetTraceEnabled();
    }
    EDiagSev sev2;
    {{
        CDiagLock lock(CDiagLock::eRead);
        sev2 = AdjustApplogPrintableSeverity(CDiagBuffer::sm_PostSeverity);
    }}
    return CompareDiagPostLevel(sev, sev2) >= 0;
}


extern void SetDiagFixedPostLevel(const EDiagSev post_sev)
{
    SetDiagPostLevel(post_sev);
    DisableDiagPostLevelChange();
}


extern bool DisableDiagPostLevelChange(bool disable_change)
{
    CDiagLock lock(CDiagLock::eWrite);
    bool prev_status = (CDiagBuffer::sm_PostSeverityChange == eDiagSC_Enable);
    CDiagBuffer::sm_PostSeverityChange = disable_change ? eDiagSC_Disable : 
                                                          eDiagSC_Enable;
    return prev_status;
}


extern EDiagSev SetDiagDieLevel(EDiagSev die_sev)
{
    if (die_sev < eDiagSevMin  ||  die_sev > eDiag_Fatal) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "SetDiagDieLevel() -- Severity must be in the range "
                   "[eDiagSevMin..eDiag_Fatal]");
    }

    CDiagLock lock(CDiagLock::eWrite);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    return sev;
}


extern EDiagSev GetDiagDieLevel(void)
{
    return CDiagBuffer::sm_DieSeverity;
}


extern bool IgnoreDiagDieLevel(bool ignore)
{
    CDiagLock lock(CDiagLock::eWrite);
    bool retval = CDiagBuffer::sm_IgnoreToDie;
    CDiagBuffer::sm_IgnoreToDie = ignore;
    return retval;
}


extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt)
{
    CDiagLock lock(CDiagLock::eWrite);
    (void) CDiagBuffer::GetTraceEnabled();

    if (dflt != eDT_Default)
        CDiagBuffer::sm_TraceDefault = dflt;

    if (how == eDT_Default)
        how = CDiagBuffer::sm_TraceDefault;
    CDiagBuffer::sm_TraceEnabled = (how == eDT_Enable);
}


CTeeDiagHandler::CTeeDiagHandler(CDiagHandler* orig, bool own_orig)
    : m_MinSev(TTeeMinSeverity::GetDefault()),
      m_OrigHandler(orig, own_orig ? eTakeOwnership : eNoOwnership)
{
    // Prevent recursion
    CTeeDiagHandler* tee = dynamic_cast<CTeeDiagHandler*>(m_OrigHandler.get());
    if ( tee ) {
        m_OrigHandler = tee->m_OrigHandler;
    }
    CStreamDiagHandler* str = dynamic_cast<CStreamDiagHandler*>(m_OrigHandler.get());
    if (str  &&  str->GetLogName() == kLogName_Stderr) {
        m_OrigHandler.reset();
    }
}


void CTeeDiagHandler::Post(const SDiagMessage& mess)
{
    if ( m_OrigHandler.get() ) {
        m_OrigHandler->Post(mess);
    }

    if ( mess.m_NoTee ) {
        // The message has been printed.
        return;
    }

    // Ignore posts below the min severity and applog messages
    if ((mess.m_Flags & eDPF_AppLog)  ||
        CompareDiagPostLevel(mess.m_Severity, m_MinSev) < 0) {
        return;
    }

    CNcbiOstrstream str_os;
    mess.x_OldWrite(str_os);
    if (IsSetDiagPostFlag(eDPF_PreMergeLines, mess.m_Flags)) {
        str_os.put('\0');
        string str = str_os.str();
        str_os.rdbuf()->freeze(false);
        if (str.find_first_of("\r\n") != NPOS) {
            list<string> lines;
            NStr::Split(str, "\r\n", lines);
            str = NStr::Join(lines, " ");
        }
        // Re-use the stream for the merged message.
        str_os.seekp(0);
        str_os << str;
        if ((mess.m_Flags & SDiagMessage::fNoEndl) == 0) {
            str_os << endl;
        }
    }
    CDiagLock lock(CDiagLock::ePost);
    cerr.write(str_os.str(), str_os.pcount());
    str_os.rdbuf()->freeze(false);
    cerr << NcbiFlush;
}


extern void SetDiagHandler(CDiagHandler* handler, bool can_delete)
{
    CDiagLock lock(CDiagLock::eWrite);
    CDiagContext& ctx = GetDiagContext();
    bool report_switch = ctx.IsSetOldPostFormat()  &&
        CDiagContext::GetProcessPostNumber(ePostNumber_NoIncrement) > 0;
    string old_name, new_name;

    if ( CDiagBuffer::sm_Handler ) {
        old_name = CDiagBuffer::sm_Handler->GetLogName();
    }
    if ( handler ) {
        new_name = handler->GetLogName();
        if (report_switch  &&  new_name != old_name) {
            ctx.Extra().Print("switch_diag_to", new_name);
        }
    }
    if ( CDiagBuffer::sm_CanDeleteHandler )
        delete CDiagBuffer::sm_Handler;
    if ( TTeeToStderr::GetDefault() ) {
        // Need to tee?
        handler = new CTeeDiagHandler(handler, can_delete);
        can_delete = true;
    }
    CDiagBuffer::sm_Handler          = handler;
    CDiagBuffer::sm_CanDeleteHandler = can_delete;
    if (report_switch  &&  !old_name.empty()  &&  new_name != old_name) {
        ctx.Extra().Print("switch_diag_from", old_name);
    }
    // Unlock severity
    CDiagContext::SetApplogSeverityLocked(false);
}


extern bool IsSetDiagHandler(void)
{
    return (CDiagBuffer::sm_Handler != s_DefaultHandler);
}

extern CDiagHandler* GetDiagHandler(bool take_ownership)
{
    CDiagLock lock(CDiagLock::eRead);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteHandler);
        CDiagBuffer::sm_CanDeleteHandler = false;
    }
    return CDiagBuffer::sm_Handler;
}


extern void DiagHandler_Reopen(void)
{
    CDiagHandler* handler = GetDiagHandler();
    if ( handler ) {
        handler->Reopen(CDiagHandler::fCheck);
    }
}


extern CDiagBuffer& GetDiagBuffer(void)
{
    return CDiagContextThreadData::GetThreadData().GetDiagBuffer();
}


void CDiagHandler::PostToConsole(const SDiagMessage& mess)
{
    if (GetLogName() == kLogName_Stderr  &&
        IsVisibleDiagPostLevel(mess.m_Severity)) {
        // Already posted to console.
        return;
    }
    CDiagLock lock(CDiagLock::ePost);
    CNcbiOstrstream str_os;
    str_os << mess;
    cerr.write(str_os.str(), str_os.pcount());
    str_os.rdbuf()->freeze(false);
    cerr << NcbiFlush;
}


string CDiagHandler::GetLogName(void)
{
    string name = typeid(*this).name();
    return name.empty() ? kLogName_Unknown
        : string(kLogName_Unknown) + "(" + name + ")";
}


CStreamDiagHandler_Base::CStreamDiagHandler_Base(void)
{
    SetLogName(kLogName_Stream);
}


string CStreamDiagHandler_Base::GetLogName(void)
{
    return m_LogName;
}


void CStreamDiagHandler_Base::SetLogName(const string& log_name)
{
    size_t len = min(log_name.length(), sizeof(m_LogName) - 1);
    memcpy(m_LogName, log_name.data(), len);
    m_LogName[len] = '\0';
}


CStreamDiagHandler::CStreamDiagHandler(CNcbiOstream* os,
                                       bool          quick_flush,
                                       const string& stream_name)
    : m_Stream(os),
      m_QuickFlush(quick_flush)
{
    if ( !stream_name.empty() ) {
        SetLogName(stream_name);
    }
}


void CStreamDiagHandler::Post(const SDiagMessage& mess)
{
    if ( !m_Stream ) {
        return;
    }
    CDiagLock lock(CDiagLock::ePost);
    m_Stream->clear();
    CNcbiOstrstream str_os;
    str_os << mess;
    m_Stream->write(str_os.str(), str_os.pcount());
    str_os.rdbuf()->freeze(false);
    if (m_QuickFlush) {
        *m_Stream << NcbiFlush;
    }
}


CDiagFileHandleHolder::CDiagFileHandleHolder(const string& fname,
                                             CDiagHandler::TReopenFlags flags)
    : m_Handle(-1)
{
    int mode = O_WRONLY | O_APPEND | O_CREAT;
    if (flags & CDiagHandler::fTruncate) {
        mode |= O_TRUNC;
    }

    mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        0);
    m_Handle = NcbiSys_open(
        _T_XCSTRING(CFile::ConvertToOSPath(fname)),
        mode, perm);
}

CDiagFileHandleHolder::~CDiagFileHandleHolder(void)
{
    if (m_Handle >= 0) {
        close(m_Handle);
    }
}


// CFileDiagHandler

CFileHandleDiagHandler::CFileHandleDiagHandler(const string& fname)
    : m_LowDiskSpace(false),
      m_Handle(NULL),
      m_HandleLock(new CSpinLock()),
      m_ReopenTimer(new CStopWatch())
{
    SetLogName(fname);
    Reopen(CDiagContext::GetLogTruncate() ? fTruncate : fDefault);
}


CFileHandleDiagHandler::~CFileHandleDiagHandler(void)
{
    delete m_ReopenTimer;
    delete m_HandleLock;
    if (m_Handle)
        m_Handle->RemoveReference();
}


void CFileHandleDiagHandler::SetLogName(const string& log_name)
{
    string abs_name = CDirEntry::IsAbsolutePath(log_name) ? log_name
        : CDirEntry::CreateAbsolutePath(log_name);
    TParent::SetLogName(abs_name);
}


const int kLogReopenDelay = 60; // Reopen log every 60 seconds

void CFileHandleDiagHandler::Reopen(TReopenFlags flags)
{
    s_ReopenEntered->Add(1);
    CDiagLock lock(CDiagLock::ePost);
    // Period is longer than for CFileDiagHandler to prevent double-reopening
    if (flags & fCheck  &&  m_ReopenTimer->IsRunning()) {
        if (m_ReopenTimer->Elapsed() < kLogReopenDelay + 5) {
            s_ReopenEntered->Add(-1);
            return;
        }
    }

    if (m_Handle) {
        // This feature of automatic log rotation will work correctly only on
        // Unix with only one CFileHandleDiagHandler for each physical file.
        // This is how it was requested to work by Denis Vakatov.
        long pos = lseek(m_Handle->GetHandle(), 0, SEEK_CUR);
        long limit = TLogSizeLimitParam::GetDefault();
        if (limit > 0  &&  pos > limit) {
            CFile f(GetLogName());
            f.Rename(GetLogName() + "-backup", CDirEntry::fRF_Overwrite);
        }
    }

    m_LowDiskSpace = false;
    CDiagFileHandleHolder* new_handle;
    new_handle = new CDiagFileHandleHolder(GetLogName(), flags);
    new_handle->AddReference();
    if (new_handle->GetHandle() == -1) {
        new_handle->RemoveReference();
        new_handle = NULL;
    }
    else {
        // Need at least 20K of free space to write logs
        try {
            CDirEntry entry(GetLogName());
            m_LowDiskSpace = CFileUtil::GetFreeDiskSpace(entry.GetDir()) < 1024*20;
        }
        catch (CException) {
            // Ignore error - could not check free space for some reason.
            // Try to open the file anyway.
        }
        if (m_LowDiskSpace) {
            new_handle->RemoveReference();
            new_handle = NULL;
        }
    }

    CDiagFileHandleHolder* old_handle;
    {{
        CSpinGuard guard(*m_HandleLock);
        // Restart the timer even if failed to reopen the file.
        m_ReopenTimer->Restart();
        old_handle = m_Handle;
        m_Handle = new_handle;
    }}

    if (old_handle)
        old_handle->RemoveReference();

    if (!new_handle) {
        if ( !m_Messages.get() ) {
            m_Messages.reset(new TMessages);
        }
    }
    else if ( m_Messages.get() ) {
        // Flush the collected messages, if any, once the handle if available
        ITERATE(TMessages, it, *m_Messages) {
            CNcbiOstrstream str_os;
            str_os << *it;
            if (write(new_handle->GetHandle(), str_os.str(),
                      size_t(str_os.pcount()))) {/*dummy*/};
            str_os.rdbuf()->freeze(false);
        }
        m_Messages.reset();
    }

    s_ReopenEntered->Add(-1);
}


void CFileHandleDiagHandler::Post(const SDiagMessage& mess)
{
    // Period is longer than for CFileDiagHandler to prevent double-reopening
    if (!m_ReopenTimer->IsRunning()  ||
        m_ReopenTimer->Elapsed() >= kLogReopenDelay + 5)
    {
        if (s_ReopenEntered->Add(1) == 1  ||  !m_ReopenTimer->IsRunning()) {
            CDiagLock lock(CDiagLock::ePost);
            if (!m_ReopenTimer->IsRunning()  ||
                m_ReopenTimer->Elapsed() >= kLogReopenDelay + 5)
            {
                Reopen(fDefault);
            }
        }
        s_ReopenEntered->Add(-1);
    }

    // If the handle is not available, collect the messages until they
    // can be written.
    if ( m_Messages.get() ) {
        CDiagLock lock(CDiagLock::ePost);
        // Check again to make sure m_Messages still exists.
        if ( m_Messages.get() ) {
            // Limit number of stored messages to 1000
            if ( m_Messages->size() < 1000 ) {
                m_Messages->push_back(mess);
            }
            return;
        }
    }

    CDiagFileHandleHolder* handle;
    {{
        CSpinGuard guard(*m_HandleLock);
        handle = m_Handle;
        if (handle)
            handle->AddReference();
    }}

    if (handle) {
        CNcbiOstrstream str_os;
        str_os << mess;
        if (write(handle->GetHandle(), str_os.str(),
                  size_t(str_os.pcount()))) {/*dummy*/};
        str_os.rdbuf()->freeze(false);

        handle->RemoveReference();
    }
}


// CFileDiagHandler

static bool s_SplitLogFile = false;

extern void SetSplitLogFile(bool value)
{
    s_SplitLogFile = value;
}


extern bool GetSplitLogFile(void)
{
    return s_SplitLogFile;
}


bool s_IsSpecialLogName(const string& name)
{
    return name.empty()
        ||  name == "-"
        ||  name == "/dev/null";
}


CFileDiagHandler::CFileDiagHandler(void)
    : m_Err(0),
      m_OwnErr(false),
      m_Log(0),
      m_OwnLog(false),
      m_Trace(0),
      m_OwnTrace(false),
      m_Perf(0),
      m_OwnPerf(false),
      m_ReopenTimer(new CStopWatch())
{
    SetLogFile("-", eDiagFile_All, true);
}


CFileDiagHandler::~CFileDiagHandler(void)
{
    x_ResetHandler(&m_Err, &m_OwnErr);
    x_ResetHandler(&m_Log, &m_OwnLog);
    x_ResetHandler(&m_Trace, &m_OwnTrace);
    x_ResetHandler(&m_Perf, &m_OwnPerf);
    delete m_ReopenTimer;
}


void CFileDiagHandler::SetLogName(const string& log_name)
{
    string abs_name = CDirEntry::IsAbsolutePath(log_name) ? log_name
        : CDirEntry::CreateAbsolutePath(log_name);
    TParent::SetLogName(abs_name);
}


void CFileDiagHandler::x_ResetHandler(CStreamDiagHandler_Base** ptr,
                                      bool*                     owned)
{
    if (!ptr  ||  !(*ptr)) {
        return;
    }
    _ASSERT(owned);
    if ( *owned ) {
        if (ptr != &m_Err  &&  *ptr == m_Err) {
            // The handler is also used by m_Err
            _ASSERT(!m_OwnErr);
            m_OwnErr = true; // now it's owned as m_Err
            *owned = false;
        }
        else if (ptr != &m_Log  &&  *ptr == m_Log) {
            _ASSERT(!m_OwnLog);
            m_OwnLog = true;
            *owned = false;
        }
        else if (ptr != &m_Trace  &&  *ptr == m_Trace) {
            _ASSERT(!m_OwnTrace);
            m_OwnTrace = true;
            *owned = false;
        }
        else if (ptr != &m_Perf  &&  *ptr == m_Perf) {
            _ASSERT(!m_OwnPerf);
            m_OwnPerf = true;
            *owned = false;
        }
        if (*owned) {
            delete *ptr;
        }
    }
    *owned = false;
    *ptr = 0;
}


void CFileDiagHandler::x_SetHandler(CStreamDiagHandler_Base** member,
                                    bool*                     own_member,
                                    CStreamDiagHandler_Base*  handler,
                                    bool                      own)
{
    if (*member == handler) {
        *member = 0;
        *own_member = false;
    }
    else {
        x_ResetHandler(member, own_member);
    }
    if (handler  &&  own) {
        // Check if the handler is already owned
        if (member != &m_Err) {
            if (handler == m_Err  &&  m_OwnErr) {
                own = false;
            }
        }
        if (member != &m_Log) {
            if (handler == m_Log  &&  m_OwnLog) {
                own = false;
            }
        }
        if (member != &m_Trace) {
            if (handler == m_Trace  &&  m_OwnTrace) {
                own = false;
            }
        }
        if (member != &m_Perf) {
            if (handler == m_Perf  &&  m_OwnPerf) {
                own = false;
            }
        }
    }
    *member = handler;
    *own_member = own;
}


void CFileDiagHandler::SetOwnership(CStreamDiagHandler_Base* handler, bool own)
{
    if (!handler) {
        return;
    }
    if (m_Err == handler) {
        m_OwnErr = own;
        own = false;
    }
    if (m_Log == handler) {
        m_OwnLog = own;
        own = false;
    }
    if (m_Trace == handler) {
        m_OwnTrace = own;
        own = false;
    }
    if (m_Perf == handler) {
        m_OwnPerf = own;
        own = false;
    }
}


static bool
s_CreateHandler(const string& fname,
                auto_ptr<CStreamDiagHandler_Base>& handler)
{
    if ( fname.empty()  ||  fname == "/dev/null") {
        handler.reset();
        return true;
    }
    if (fname == "-") {
        handler.reset(new CStreamDiagHandler(&NcbiCerr, true, kLogName_Stderr));
        return true;
    }
    auto_ptr<CFileHandleDiagHandler> fh(new CFileHandleDiagHandler(fname));
    if ( !fh->Valid() ) {
        ERR_POST_X(7, Info << "Failed to open log file: " << fname);
        return false;
    }
    handler.reset(fh.release());
    return true;
}


bool CFileDiagHandler::SetLogFile(const string& file_name,
                                  EDiagFileType file_type,
                                  bool          /*quick_flush*/)
{
    bool special = s_IsSpecialLogName(file_name);
    auto_ptr<CStreamDiagHandler_Base> err_handler, log_handler,
                                      trace_handler, perf_handler;
    switch ( file_type ) {
    case eDiagFile_All:
        {
            // Remove known extension if any
            string adj_name = file_name;
            if ( !special ) {
                CDirEntry entry(file_name);
                string ext = entry.GetExt();
                if (ext == ".log"  ||
                    ext == ".err"  ||
                    ext == ".trace"  ||
                    ext == ".perf") {
                    adj_name = entry.GetDir() + entry.GetBase();
                }
            }
            string err_name = special ? adj_name : adj_name + ".err";
            string log_name = special ? adj_name : adj_name + ".log";
            string trace_name = special ? adj_name : adj_name + ".trace";
            string perf_name = special ? adj_name : adj_name + ".perf";

            if (!s_CreateHandler(err_name, err_handler))
                return false;
            if (!s_CreateHandler(log_name, log_handler))
                return false;
            if (!s_CreateHandler(trace_name, trace_handler))
                return false;
            if (!s_CreateHandler(perf_name, perf_handler))
                return false;

            x_SetHandler(&m_Err, &m_OwnErr, err_handler.release(), true);
            x_SetHandler(&m_Log, &m_OwnLog, log_handler.release(), true);
            x_SetHandler(&m_Trace, &m_OwnTrace, trace_handler.release(), true);
            x_SetHandler(&m_Perf, &m_OwnPerf, perf_handler.release(), true);
            m_ReopenTimer->Restart();
            break;
        }
    case eDiagFile_Err:
        if (!s_CreateHandler(file_name, err_handler))
            return false;
        x_SetHandler(&m_Err, &m_OwnErr, err_handler.release(), true);
        break;
    case eDiagFile_Log:
        if (!s_CreateHandler(file_name, log_handler))
            return false;
        x_SetHandler(&m_Log, &m_OwnLog, log_handler.release(), true);
        break;
    case eDiagFile_Trace:
        if (!s_CreateHandler(file_name, trace_handler))
            return false;
        x_SetHandler(&m_Trace, &m_OwnTrace, trace_handler.release(), true);
        break;
    case eDiagFile_Perf:
        if (!s_CreateHandler(file_name, perf_handler))
            return false;
        x_SetHandler(&m_Perf, &m_OwnPerf, perf_handler.release(), true);
        break;
    }
    if (file_name == "") {
        SetLogName(kLogName_None);
    }
    else if (file_name == "-") {
        SetLogName(kLogName_Stderr);
    }
    else {
        SetLogName(file_name);
    }
    return true;
}


string CFileDiagHandler::GetLogFile(EDiagFileType file_type) const
{
    switch ( file_type ) {
    case eDiagFile_Err:
        return m_Err->GetLogName();
    case eDiagFile_Log:
        return m_Log->GetLogName();
    case eDiagFile_Trace:
        return m_Trace->GetLogName();
    case eDiagFile_Perf:
        return m_Perf->GetLogName();
    case eDiagFile_All:
        break;  // kEmptyStr
    }
    return kEmptyStr;
}


CNcbiOstream* CFileDiagHandler::GetLogStream(EDiagFileType file_type)
{
    CStreamDiagHandler_Base* handler = 0;
    switch ( file_type ) {
    case eDiagFile_Err:
        handler = m_Err;
    case eDiagFile_Log:
        handler = m_Log;
    case eDiagFile_Trace:
        handler = m_Trace;
    case eDiagFile_Perf:
        handler = m_Perf;
    case eDiagFile_All:
        return 0;
    }
    return handler ? handler->GetStream() : 0;
}


void CFileDiagHandler::SetSubHandler(CStreamDiagHandler_Base* handler,
                                     EDiagFileType            file_type,
                                     bool                     own)
{
    switch ( file_type ) {
    case eDiagFile_All:
        // Must set all handlers
    case eDiagFile_Err:
        x_SetHandler(&m_Err, &m_OwnErr, handler, own);
        if (file_type != eDiagFile_All) break;
    case eDiagFile_Log:
        x_SetHandler(&m_Log, &m_OwnLog, handler, own);
        if (file_type != eDiagFile_All) break;
    case eDiagFile_Trace:
        x_SetHandler(&m_Trace, &m_OwnTrace, handler, own);
        if (file_type != eDiagFile_All) break;
    case eDiagFile_Perf:
        x_SetHandler(&m_Perf, &m_OwnPerf, handler, own);
        if (file_type != eDiagFile_All) break;
    }
}


void CFileDiagHandler::Reopen(TReopenFlags flags)
{
    s_ReopenEntered->Add(1);

    if (flags & fCheck  &&  m_ReopenTimer->IsRunning()) {
        if (m_ReopenTimer->Elapsed() < kLogReopenDelay) {
            s_ReopenEntered->Add(-1);
            return;
        }
    }
    if ( m_Err ) {
        m_Err->Reopen(flags);
    }
    if ( m_Log ) {
        m_Log->Reopen(flags);
    }
    if ( m_Trace ) {
        m_Trace->Reopen(flags);
    }
    if ( m_Perf ) {
        m_Perf->Reopen(flags);
    }
    m_ReopenTimer->Restart();

    s_ReopenEntered->Add(-1);
}


void CFileDiagHandler::Post(const SDiagMessage& mess)
{
    // Check time and re-open the streams
    if (!m_ReopenTimer->IsRunning()  ||
        m_ReopenTimer->Elapsed() >= kLogReopenDelay)
    {
        if (s_ReopenEntered->Add(1) == 1  ||  !m_ReopenTimer->IsRunning()) {
            CDiagLock lock(CDiagLock::ePost);
            if (!m_ReopenTimer->IsRunning()  ||
                m_ReopenTimer->Elapsed() >= kLogReopenDelay)
            {
                Reopen(fDefault);
            }
        }
        s_ReopenEntered->Add(-1);
    }

    // Output the message
    CStreamDiagHandler_Base* handler = 0;
    if ( IsSetDiagPostFlag(eDPF_AppLog, mess.m_Flags) ) {
        handler = mess.m_Event == SDiagMessage::eEvent_PerfLog
            ? m_Perf : m_Log;
    }
    else {
        switch ( mess.m_Severity ) {
        case eDiag_Info:
        case eDiag_Trace:
            handler = m_Trace;
            break;
        default:
            handler = m_Err;
        }
    }
    if (handler)
        handler->Post(mess);
}


class CAsyncDiagThread : public CThread
{
public:
    CAsyncDiagThread(const string& thread_suffix);
    virtual ~CAsyncDiagThread(void);

    virtual void* Main(void);
    void Stop(void);


    bool m_NeedStop;
    Uint2 m_CntWaiters;
    CAtomicCounter m_MsgsInQueue;
    CDiagHandler* m_SubHandler;
    CFastMutex m_QueueLock;
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
    CConditionVariable m_QueueCond;
    CConditionVariable m_DequeueCond;
#else
    CSemaphore m_QueueSem;
    CSemaphore m_DequeueSem;
#endif
    deque<SDiagMessage*> m_MsgQueue;
    string m_ThreadSuffix;
};


/// Maximum number of messages that allowed to be in the queue for
/// asynchronous processing.
NCBI_PARAM_DECL(Uint4, Diag, Max_Async_Queue_Size);
NCBI_PARAM_DEF_EX(Uint4, Diag, Max_Async_Queue_Size, 10000, eParam_NoThread,
                  DIAG_MAX_ASYNC_QUEUE_SIZE);
typedef NCBI_PARAM_TYPE(Diag, Max_Async_Queue_Size) TMaxAsyncQueueSizeParam;


CAsyncDiagHandler::CAsyncDiagHandler(void)
    : m_AsyncThread(NULL)
{}

CAsyncDiagHandler::~CAsyncDiagHandler(void)
{
    _ASSERT(!m_AsyncThread);
}

void
CAsyncDiagHandler::SetCustomThreadSuffix(const string& suffix)
{
    m_ThreadSuffix = suffix;
}

void
CAsyncDiagHandler::InstallToDiag(void)
{
    m_AsyncThread = new CAsyncDiagThread(m_ThreadSuffix);
    m_AsyncThread->AddReference();
    try {
        m_AsyncThread->Run();
    }
    catch (CThreadException&) {
        m_AsyncThread->RemoveReference();
        m_AsyncThread = NULL;
        throw;
    }
    m_AsyncThread->m_SubHandler = GetDiagHandler(true);
    SetDiagHandler(this, false);
}

void
CAsyncDiagHandler::RemoveFromDiag(void)
{
    if (!m_AsyncThread)
        return;

    _ASSERT(GetDiagHandler(false) == this);
    SetDiagHandler(m_AsyncThread->m_SubHandler);
    m_AsyncThread->Stop();
    m_AsyncThread->RemoveReference();
    m_AsyncThread = NULL;
}

string
CAsyncDiagHandler::GetLogName(void)
{
    return m_AsyncThread->m_SubHandler->GetLogName();
}

void
CAsyncDiagHandler::Reopen(TReopenFlags flags)
{
    m_AsyncThread->m_SubHandler->Reopen(flags);
}

void
CAsyncDiagHandler::Post(const SDiagMessage& mess)
{
    CAsyncDiagThread* thr = m_AsyncThread;
    SDiagMessage* save_msg = new SDiagMessage(mess);

    if (save_msg->m_Severity < GetDiagDieLevel()) {
        CFastMutexGuard guard(thr->m_QueueLock);
        while (Uint4(thr->m_MsgsInQueue.Get()) >= TMaxAsyncQueueSizeParam::GetDefault())
        {
            ++thr->m_CntWaiters;
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
            thr->m_DequeueCond.WaitForSignal(thr->m_QueueLock);
#else
            guard.Release();
            thr->m_QueueSem.Wait();
            guard.Guard(thr->m_QueueLock);
#endif
            --thr->m_CntWaiters;
        }
        thr->m_MsgQueue.push_back(save_msg);
        if (thr->m_MsgsInQueue.Add(1) == 1) {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
            thr->m_QueueCond.SignalSome();
#else
            thr->m_QueueSem.Post();
#endif
        }
    }
    else {
        thr->Stop();
        thr->m_SubHandler->Post(*save_msg);
    }
}


CAsyncDiagThread::CAsyncDiagThread(const string& thread_suffix)
    : m_NeedStop(false),
      m_CntWaiters(0),
      m_SubHandler(NULL),
#ifndef NCBI_HAVE_CONDITIONAL_VARIABLE
      m_QueueSem(0, 100),
      m_DequeueSem(0, 10000000),
#endif
      m_ThreadSuffix(thread_suffix)
{
    m_MsgsInQueue.Set(0);
}

CAsyncDiagThread::~CAsyncDiagThread(void)
{}

void*
CAsyncDiagThread::Main(void)
{
    if (!m_ThreadSuffix.empty()) {
        string thr_name = CNcbiApplication::Instance()->GetProgramDisplayName();
        thr_name += m_ThreadSuffix;
#if defined(NCBI_OS_LINUX)  &&  defined(PR_SET_NAME)
        prctl(PR_SET_NAME, (unsigned long)thr_name.c_str(), 0, 0, 0);
#endif
    }

    deque<SDiagMessage*> save_msgs;
    while (!m_NeedStop) {
        {{
            CFastMutexGuard guard(m_QueueLock);
            while (m_MsgQueue.size() == 0  &&  !m_NeedStop) {
                if (m_MsgsInQueue.Get() != 0)
                    abort();
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
                m_QueueCond.WaitForSignal(m_QueueLock);
#else
                guard.Release();
                m_QueueSem.Wait();
                guard.Guard(m_QueueLock);
#endif
            }
            save_msgs.swap(m_MsgQueue);
        }}

drain_messages:
        while (!save_msgs.empty()) {
            SDiagMessage* msg = save_msgs.front();
            save_msgs.pop_front();
            m_SubHandler->Post(*msg);
            delete msg;
            m_MsgsInQueue.Add(-1);
            if (m_CntWaiters != 0) {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
                m_DequeueCond.SignalSome();
#else
                m_DequeueSem.Post();
#endif
            }
        }
    }
    if (m_MsgQueue.size() != 0) {
        save_msgs.swap(m_MsgQueue);
        goto drain_messages;
    }
    return NULL;
}

void
CAsyncDiagThread::Stop(void)
{
    m_NeedStop = true;
    try {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
        m_QueueCond.SignalAll();
#else
        m_QueueSem.Post(10);
#endif
        Join();
    }
    catch (CException& ex) {
        ERR_POST_X(24, Critical
                   << "Error while stopping thread for AsyncDiagHandler: " << ex);
    }
}


extern bool SetLogFile(const string& file_name,
                       EDiagFileType file_type,
                       bool quick_flush)
{
    // Check if a non-existing dir is specified
    if ( !s_IsSpecialLogName(file_name) ) {
        string dir = CFile(file_name).GetDir();
        if ( !dir.empty()  &&  !CDir(dir).Exists() ) {
            return false;
        }
    }

    if (file_type != eDiagFile_All) {
        // Auto-split log file
        SetSplitLogFile(true);
    }
    bool no_split = !s_SplitLogFile;
    if ( no_split ) {
        if (file_type != eDiagFile_All) {
            ERR_POST_X(8, Info <<
                "Failed to set log file for the selected event type: "
                "split log is disabled");
            return false;
        }
        // Check special filenames
        if ( file_name.empty()  ||  file_name == "/dev/null" ) {
            // no output
            SetDiagStream(0, quick_flush, 0, 0, kLogName_None);
        }
        else if (file_name == "-") {
            // output to stderr
            SetDiagStream(&NcbiCerr, quick_flush, 0, 0, kLogName_Stderr);
        }
        else {
            // output to file
            auto_ptr<CFileHandleDiagHandler> fhandler(
                new CFileHandleDiagHandler(file_name));
            if ( !fhandler->Valid() ) {
                ERR_POST_X(9, Info << "Failed to initialize log: " << file_name);
                return false;
            }
            SetDiagHandler(fhandler.release());
        }
    }
    else {
        CFileDiagHandler* handler =
            dynamic_cast<CFileDiagHandler*>(GetDiagHandler());
        if ( !handler ) {
            CStreamDiagHandler_Base* sub_handler =
                dynamic_cast<CStreamDiagHandler_Base*>(GetDiagHandler());
            // Install new handler, try to re-use the old one
            auto_ptr<CFileDiagHandler> fhandler(new CFileDiagHandler());
            // If we are going to set all three handlers, no need to save
            // the old one.
            if ( sub_handler  &&  file_type != eDiagFile_All) {
                GetDiagHandler(true); // Take ownership!
                // Set all three handlers to the old one.
                fhandler->SetSubHandler(sub_handler, eDiagFile_All, false);
            }
            if ( fhandler->SetLogFile(file_name, file_type, quick_flush) ) {
                SetDiagHandler(fhandler.release());
                return true;
            }
            else {
                return false;
            }
        }
        // Update the existing handler
        CDiagContext::SetApplogSeverityLocked(false);
        return handler->SetLogFile(file_name, file_type, quick_flush);
    }
    return true;
}


extern string GetLogFile(EDiagFileType file_type)
{
    CDiagHandler* handler = GetDiagHandler();
    CFileDiagHandler* fhandler =
        dynamic_cast<CFileDiagHandler*>(handler);
    if ( fhandler ) {
        return fhandler->GetLogFile(file_type);
    }
    CFileHandleDiagHandler* fhhandler =
        dynamic_cast<CFileHandleDiagHandler*>(handler);
    if ( fhhandler ) {
        return fhhandler->GetLogName();
    }
    return kEmptyStr;
}


extern string GetLogFile(void)
{
    CDiagHandler* handler = GetDiagHandler();
    return handler ? handler->GetLogName() : kEmptyStr;
}


extern bool IsDiagStream(const CNcbiOstream* os)
{
    CStreamDiagHandler_Base* sdh
        = dynamic_cast<CStreamDiagHandler_Base*>(CDiagBuffer::sm_Handler);
    return (sdh  &&  sdh->GetStream() == os);
}


extern void SetDiagErrCodeInfo(CDiagErrCodeInfo* info, bool can_delete)
{
    CDiagLock lock(CDiagLock::eWrite);
    if ( CDiagBuffer::sm_CanDeleteErrCodeInfo  &&
         CDiagBuffer::sm_ErrCodeInfo )
        delete CDiagBuffer::sm_ErrCodeInfo;
    CDiagBuffer::sm_ErrCodeInfo = info;
    CDiagBuffer::sm_CanDeleteErrCodeInfo = can_delete;
}

extern bool IsSetDiagErrCodeInfo(void)
{
    return (CDiagBuffer::sm_ErrCodeInfo != 0);
}

extern CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership)
{
    CDiagLock lock(CDiagLock::eRead);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteErrCodeInfo);
        CDiagBuffer::sm_CanDeleteErrCodeInfo = false;
    }
    return CDiagBuffer::sm_ErrCodeInfo;
}


extern void SetDiagFilter(EDiagFilter what, const char* filter_str)
{
    CDiagLock lock(CDiagLock::eWrite);
    if (what == eDiagFilter_Trace  ||  what == eDiagFilter_All) 
        s_TraceFilter->Fill(filter_str);

    if (what == eDiagFilter_Post  ||  what == eDiagFilter_All) 
        s_PostFilter->Fill(filter_str);
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), 
      m_ErrCode(0), 
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), 
      m_PostFlags(ForceImportantFlags(post_flags))
{
}


CNcbiDiag::CNcbiDiag(const CDiagCompileInfo &info,
                     EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()),
      m_PostFlags(ForceImportantFlags(post_flags)),
      m_CompileInfo(info)
{
    SetFile(   info.GetFile()   );
    SetModule( info.GetModule() );
}

CNcbiDiag::~CNcbiDiag(void) 
{
    m_Buffer.Detach(this);
}

TDiagPostFlags CNcbiDiag::ForceImportantFlags(TDiagPostFlags flags)
{
    if ( !IsSetDiagPostFlag(eDPF_UseExactUserFlags, flags) ) {
        flags = (flags & (~eDPF_ImportantFlagsMask)) |
            (CDiagBuffer::s_GetPostFlags() & eDPF_ImportantFlagsMask);
    }
    return flags;
}

const CNcbiDiag& CNcbiDiag::SetFile(const char* file) const
{
    m_CompileInfo.SetFile(file);
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetModule(const char* module) const
{
    m_CompileInfo.SetModule(module);
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetClass(const char* nclass) const
{
    m_CompileInfo.SetClass(nclass);
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetFunction(const char* function) const
{
    m_CompileInfo.SetFunction(function);
    return *this;
}


bool CNcbiDiag::CheckFilters(void) const
{
    EDiagSev current_sev = GetSeverity();
    if (current_sev == eDiag_Fatal) 
        return true;

    CDiagLock lock(CDiagLock::eRead);
    if (GetSeverity() == eDiag_Trace) {
        // check for trace filter
        return  s_TraceFilter->Check(*this, this->GetSeverity())
                != eDiagFilter_Reject;
    }
    
    // check for post filter and severity
    return  s_PostFilter->Check(*this, this->GetSeverity())
            != eDiagFilter_Reject;
}


// Formatted output of stack trace
void s_FormatStackTrace(CNcbiOstream& os, const CStackTrace& trace)
{
    string old_prefix = trace.GetPrefix();
    trace.SetPrefix("      ");
    os << "\n     Stack trace:\n" << trace;
    trace.SetPrefix(old_prefix);
}


const CNcbiDiag& CNcbiDiag::Put(const CStackTrace*,
                                const CStackTrace& stacktrace) const
{
    if ( !stacktrace.Empty() ) {
        stacktrace.SetPrefix("      ");
        ostrstream os;
        s_FormatStackTrace(os, stacktrace);
        *this << (string) CNcbiOstrstreamToString(os);
    }
    return *this;
}

static string
s_GetExceptionText(const CException* pex)
{
    string text(pex->GetMsg());
    ostrstream os;
    pex->ReportExtra(os);
    if (os.pcount()) {
        text += " (";
        text += (string) CNcbiOstrstreamToString(os);
        text += ')';
    }
    return text;
}

const CNcbiDiag& CNcbiDiag::x_Put(const CException& ex) const
{
    if (m_Buffer.SeverityDisabled(GetSeverity())  ||  !CheckFilters())
        return *this;

    CDiagContextThreadData& thr_data =
        CDiagContextThreadData::GetThreadData();
    CDiagCollectGuard* guard = thr_data.GetCollectGuard();
    EDiagSev print_sev = AdjustApplogPrintableSeverity(CDiagBuffer::sm_PostSeverity);
    EDiagSev collect_sev = print_sev;
    if ( guard ) {
        print_sev = AdjustApplogPrintableSeverity(guard->GetPrintSeverity());
        collect_sev = guard->GetCollectSeverity();
    }

    const CException* pex;
    const CException* main_pex = NULL;
    stack<const CException*> pile;
    // invert the order
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
        if (!main_pex  &&  pex->HasMainText())
            main_pex = pex;
    }
    if (!main_pex)
        main_pex = pile.top();
    if (m_Buffer.m_Stream->pcount()) {
        *this << "(" << main_pex->GetType() << "::"
                     << main_pex->GetErrCodeString() << ") "
              << s_GetExceptionText(main_pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        string text(s_GetExceptionText(pex));
        const CStackTrace* stacktrace = pex->GetStackTrace();
        if ( stacktrace ) {
            ostrstream os;
            s_FormatStackTrace(os, *stacktrace);
            text += (string) CNcbiOstrstreamToString(os);
        }
        string err_type(pex->GetType());
        err_type += "::";
        err_type += pex->GetErrCodeString();

        EDiagSev pex_sev = pex->GetSeverity();
        if (CompareDiagPostLevel(GetSeverity(), print_sev) < 0) {
            if (CompareDiagPostLevel(pex_sev, collect_sev) < 0)
                pex_sev = collect_sev;
        }
        else {
            if (CompareDiagPostLevel(pex_sev, print_sev) < 0)
                pex_sev = print_sev;
        }
        if (CompareDiagPostLevel(GetSeverity(), pex_sev) < 0)
            pex_sev = GetSeverity();

        SDiagMessage diagmsg
            (pex_sev,
            text.c_str(),
            text.size(),
            pex->GetFile().c_str(),
            pex->GetLine(),
            GetPostFlags(),
            NULL,
            pex->GetErrCode(),
            0,
            err_type.c_str(),
            pex->GetModule().c_str(),
            pex->GetClass().c_str(),
            pex->GetFunction().c_str());

        if ( pex->IsSetFlag(CException::fConsole) ) {
            diagmsg.m_Flags |= eDPF_IsConsole;
        }

        m_Buffer.PrintMessage(diagmsg, *this);
    }
    

    return *this;
}


bool CNcbiDiag::StrToSeverityLevel(const char* str_sev, EDiagSev& sev)
{
    if (!str_sev || !*str_sev) {
        return false;
    } 
    // Digital value
    int nsev = NStr::StringToNonNegativeInt(str_sev);

    if (nsev > eDiagSevMax) {
        nsev = eDiagSevMax;
    } else if ( nsev == -1 ) {
        // String value
        for (int s = eDiagSevMin; s <= eDiagSevMax; s++) {
            if (NStr::CompareNocase(CNcbiDiag::SeverityName(EDiagSev(s)),
                                    str_sev) == 0) {
                nsev = s;
                break;
            }
        }
    }
    sev = EDiagSev(nsev);
    // Unknown value
    return sev >= eDiagSevMin  &&  sev <= eDiagSevMax;
}

void CNcbiDiag::DiagFatal(const CDiagCompileInfo& info,
                          const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal) << message << Endm;
}

void CNcbiDiag::DiagTrouble(const CDiagCompileInfo& info,
                            const char* message)
{
    DiagFatal(info, message);
}

void CNcbiDiag::DiagAssert(const CDiagCompileInfo& info,
                           const char* expression,
                           const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal, eDPF_Trace) <<
        "Assertion failed: (" <<
        (expression ? expression : "") << ") " <<
        (message ? message : "") << Endm;
}

void CNcbiDiag::DiagAssertIfSuppressedSystemMessageBox(
    const CDiagCompileInfo& info,
    const char* expression,
    const char* message)
{
    if ( IsSuppressedDebugSystemMessageBox() ) {
        DiagAssert(info, expression, message);
    }
}

void CNcbiDiag::DiagValidate(const CDiagCompileInfo& info,
                             const char* _DEBUG_ARG(expression),
                             const char* message)
{
#ifdef _DEBUG
    if ( xncbi_GetValidateAction() != eValidate_Throw ) {
        DiagAssert(info, expression, message);
    }
#endif
    throw CCoreException(info, 0, CCoreException::eCore, message);
}

///////////////////////////////////////////////////////
//  CDiagRestorer::

CDiagRestorer::CDiagRestorer(void)
{
    CDiagLock lock(CDiagLock::eWrite);
    const CDiagBuffer& buf  = GetDiagBuffer();
    m_PostPrefix            = buf.m_PostPrefix;
    m_PrefixList            = buf.m_PrefixList;
    m_PostFlags             = buf.sx_GetPostFlags();
    m_PostSeverity          = buf.sm_PostSeverity;
    m_PostSeverityChange    = buf.sm_PostSeverityChange;
    m_IgnoreToDie           = buf.sm_IgnoreToDie;
    m_DieSeverity           = buf.sm_DieSeverity;
    m_TraceDefault          = buf.sm_TraceDefault;
    m_TraceEnabled          = buf.sm_TraceEnabled;
    m_Handler               = buf.sm_Handler;
    m_CanDeleteHandler      = buf.sm_CanDeleteHandler;
    m_ErrCodeInfo           = buf.sm_ErrCodeInfo;
    m_CanDeleteErrCodeInfo  = buf.sm_CanDeleteErrCodeInfo;
    m_ApplogSeverityLocked  = CDiagContext::IsApplogSeverityLocked();

    // avoid premature cleanup
    buf.sm_CanDeleteHandler     = false;
    buf.sm_CanDeleteErrCodeInfo = false;
}

CDiagRestorer::~CDiagRestorer(void)
{
    {{
        CDiagLock lock(CDiagLock::eWrite);
        CDiagBuffer& buf          = GetDiagBuffer();
        buf.m_PostPrefix          = m_PostPrefix;
        buf.m_PrefixList          = m_PrefixList;
        buf.sx_GetPostFlags()     = m_PostFlags;
        buf.sm_PostSeverity       = m_PostSeverity;
        buf.sm_PostSeverityChange = m_PostSeverityChange;
        buf.sm_IgnoreToDie        = m_IgnoreToDie;
        buf.sm_DieSeverity        = m_DieSeverity;
        buf.sm_TraceDefault       = m_TraceDefault;
        buf.sm_TraceEnabled       = m_TraceEnabled;
    }}
    SetDiagHandler(m_Handler, m_CanDeleteHandler);
    SetDiagErrCodeInfo(m_ErrCodeInfo, m_CanDeleteErrCodeInfo);
    CDiagContext::SetApplogSeverityLocked(m_ApplogSeverityLocked);
}


//////////////////////////////////////////////////////
//  internal diag. handler classes for compatibility:

class CCompatDiagHandler : public CDiagHandler
{
public:
    CCompatDiagHandler(FDiagHandler func, void* data, FDiagCleanup cleanup)
        : m_Func(func), m_Data(data), m_Cleanup(cleanup)
        { }
    ~CCompatDiagHandler(void)
        {
            if (m_Cleanup) {
                m_Cleanup(m_Data);
            }
        }
    virtual void Post(const SDiagMessage& mess) { m_Func(mess); }

private:
    FDiagHandler m_Func;
    void*        m_Data;
    FDiagCleanup m_Cleanup;
};


extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup)
{
    SetDiagHandler(new CCompatDiagHandler(func, data, cleanup));
}


class CCompatStreamDiagHandler : public CStreamDiagHandler
{
public:
    CCompatStreamDiagHandler(CNcbiOstream* os,
                             bool          quick_flush  = true,
                             FDiagCleanup  cleanup      = 0,
                             void*         cleanup_data = 0,
                             const string& stream_name = kEmptyStr)
        : CStreamDiagHandler(os, quick_flush, stream_name),
          m_Cleanup(cleanup), m_CleanupData(cleanup_data)
        {
        }

    ~CCompatStreamDiagHandler(void)
        {
            if (m_Cleanup) {
                m_Cleanup(m_CleanupData);
            }
        }

private:
    FDiagCleanup m_Cleanup;
    void*        m_CleanupData;
};


extern void SetDiagStream(CNcbiOstream* os,
                          bool          quick_flush,
                          FDiagCleanup  cleanup,
                          void*         cleanup_data,
                          const string& stream_name)
{
    string str_name = stream_name;
    if ( str_name.empty() ) {
        if (os == &cerr) {
            str_name = kLogName_Stderr;
        }
        else if (os == &cout) {
            str_name = kLogName_Stdout;
        }
        else {
            str_name =  kLogName_Stream;
        }
    }
    SetDiagHandler(new CCompatStreamDiagHandler(os, quick_flush,
        cleanup, cleanup_data, str_name));
}


extern CNcbiOstream* GetDiagStream(void)
{
    CDiagHandler* diagh = GetDiagHandler();
    if ( !diagh ) {
        return 0;
    }
    CStreamDiagHandler_Base* sh =
        dynamic_cast<CStreamDiagHandler_Base*>(diagh);
    // This can also be CFileDiagHandler, check it later
    if ( sh  &&  sh->GetStream() ) {
        return sh->GetStream();
    }
    CFileDiagHandler* fh =
        dynamic_cast<CFileDiagHandler*>(diagh);
    if ( fh ) {
        return fh->GetLogStream(eDiagFile_Err);
    }
    return 0;
}


extern void SetDoubleDiagHandler(void)
{
    ERR_POST_X(10, Error << "SetDoubleDiagHandler() is not implemented");
}


//////////////////////////////////////////////////////
//  abort handler


static FAbortHandler s_UserAbortHandler = 0;

extern void SetAbortHandler(FAbortHandler func)
{
    s_UserAbortHandler = func;
}


extern void Abort(void)
{
    // If defined user abort handler then call it 
    if ( s_UserAbortHandler )
        s_UserAbortHandler();
    
    // If don't defined handler or application doesn't still terminated

    // Check environment variable for silent exit
    const TXChar* value = NcbiSys_getenv(_TX("DIAG_SILENT_ABORT"));
    if (value  &&  (*value == _TX('Y')  ||  *value == _TX('y')  ||  *value == _TX('1'))) {
        ::exit(255);
    }
    else if (value  &&  (*value == _TX('N')  ||  *value == _TX('n') || *value == _TX('0'))) {
        ::abort();
    }
    else
#define NCBI_TOTALVIEW_ABORT_WORKAROUND 1
#if defined(NCBI_TOTALVIEW_ABORT_WORKAROUND)
        // The condition in the following if statement is always 'true'.
        // It's a workaround for TotalView 6.5 (beta) to properly display
        // stacktrace at this point.
//        if ( !(value && *value == 'Y') )
#endif
            {
#if defined(_DEBUG)
                ::abort();
#else
                ::exit(255);
#endif
            }
}


///////////////////////////////////////////////////////
//  CDiagErrCodeInfo::
//

SDiagErrCodeDescription::SDiagErrCodeDescription(void)
        : m_Message(kEmptyStr),
          m_Explanation(kEmptyStr),
          m_Severity(-1)
{
    return;
}


bool CDiagErrCodeInfo::Read(const string& file_name)
{
    CNcbiIfstream is(file_name.c_str());
    if ( !is.good() ) {
        return false;
    }
    return Read(is);
}


// Parse string for CDiagErrCodeInfo::Read()

bool s_ParseErrCodeInfoStr(string&          str,
                           const SIZE_TYPE  line,
                           int&             x_code,
                           int&             x_severity,
                           string&          x_message,
                           bool&            x_ready)
{
    list<string> tokens;    // List with line tokens

    try {
        // Get message text
        SIZE_TYPE pos = str.find_first_of(':');
        if (pos == NPOS) {
            x_message = kEmptyStr;
        } else {
            x_message = NStr::TruncateSpaces(str.substr(pos+1));
            str.erase(pos);
        }

        // Split string on parts
        NStr::Split(str, ",", tokens);
        if (tokens.size() < 2) {
            ERR_POST_X(11, "Error message file parsing: Incorrect file format "
                           ", line " + NStr::UInt8ToString(line));
            return false;
        }
        // Mnemonic name (skip)
        tokens.pop_front();

        // Error code
        string token = NStr::TruncateSpaces(tokens.front());
        tokens.pop_front();
        x_code = NStr::StringToInt(token);

        // Severity
        if ( !tokens.empty() ) { 
            token = NStr::TruncateSpaces(tokens.front());
            EDiagSev sev;
            if (CNcbiDiag::StrToSeverityLevel(token.c_str(), sev)) {
                x_severity = sev;
            } else {
                ERR_POST_X(12, Warning << "Error message file parsing: "
                               "Incorrect severity level in the verbose "
                               "message file, line " + NStr::UInt8ToString(line));
            }
        } else {
            x_severity = -1;
        }
    }
    catch (CException& e) {
        ERR_POST_X(13, Warning << "Error message file parsing: " << e.GetMsg() <<
                       ", line " + NStr::UInt8ToString(line));
        return false;
    }
    x_ready = true;
    return true;
}

  
bool CDiagErrCodeInfo::Read(CNcbiIstream& is)
{
    string       str;                      // The line being parsed
    SIZE_TYPE    line;                     // # of the line being parsed
    bool         err_ready       = false;  // Error data ready flag 
    int          err_code        = 0;      // First level error code
    int          err_subcode     = 0;      // Second level error code
    string       err_message;              // Short message
    string       err_text;                 // Error explanation
    int          err_severity    = -1;     // Use default severity if  
                                           // has not specified
    int          err_subseverity = -1;     // Use parents severity if  
                                           // has not specified

    for (line = 1;  NcbiGetlineEOL(is, str);  line++) {
        
        // This is a comment or empty line
        if (!str.length()  ||  NStr::StartsWith(str,"#")) {
            continue;
        }
        // Add error description
        if (err_ready  &&  str[0] == '$') {
            if (err_subseverity == -1)
                err_subseverity = err_severity;
            SetDescription(ErrCode(err_code, err_subcode), 
                SDiagErrCodeDescription(err_message, err_text,
                                        err_subseverity));
            // Clean
            err_subseverity = -1;
            err_text     = kEmptyStr;
            err_ready    = false;
        }

        // Get error code
        if (NStr::StartsWith(str,"$$")) {
            if (!s_ParseErrCodeInfoStr(str, line, err_code, err_severity, 
                                       err_message, err_ready))
                continue;
            err_subcode = 0;
        
        } else if (NStr::StartsWith(str,"$^")) {
        // Get error subcode
            s_ParseErrCodeInfoStr(str, line, err_subcode, err_subseverity,
                                  err_message, err_ready);
      
        } else if (err_ready) {
        // Get line of explanation message
            if (!err_text.empty()) {
                err_text += '\n';
            }
            err_text += str;
        }
    }
    if (err_ready) {
        if (err_subseverity == -1)
            err_subseverity = err_severity;
        SetDescription(ErrCode(err_code, err_subcode), 
            SDiagErrCodeDescription(err_message, err_text, err_subseverity));
    }
    return true;
}


bool CDiagErrCodeInfo::GetDescription(const ErrCode& err_code, 
                                      SDiagErrCodeDescription* description)
    const
{
    // Find entry
    TInfo::const_iterator find_entry = m_Info.find(err_code);
    if (find_entry == m_Info.end()) {
        return false;
    }
    // Get entry value
    const SDiagErrCodeDescription& entry = find_entry->second;
    if (description) {
        *description = entry;
    }
    return true;
}


const char* g_DiagUnknownFunction(void)
{
    return kEmptyCStr;
}


CDiagContext_Extra g_PostPerf(int                       status,
                              double                    timespan,
                              SDiagMessage::TExtraArgs& args)
{
    return CDiagContext_Extra(status, timespan, args);
}


END_NCBI_SCOPE
