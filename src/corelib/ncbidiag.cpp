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
#include <common/ncbi_source_ver.h>
#include <common/ncbi_package_ver.h>
#include <common/ncbi_sanitizers.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/version.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/syslog.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/request_control.hpp>
#include <corelib/ncbi_strings.h>
#include <corelib/impl/ncbi_param_impl.hpp>
#include <corelib/ncbiapp_api.hpp>
#include "ncbidiag_p.hpp"
#include "ncbisys.hpp"
#include <fcntl.h>
#include <stdlib.h>
#include <stack>
#include <atomic>
#include <thread>
#include <unordered_set>
#include <sstream>

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/utsname.h>
#endif

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined (NCBI_OS_DARWIN)
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Diag


BEGIN_NCBI_SCOPE

static bool s_DiagUseRWLock = true;
DEFINE_STATIC_MUTEX(s_DiagMutex);
DEFINE_STATIC_MUTEX(s_DiagPostMutex);
static CSafeStatic<CRWLock> s_DiagRWLock(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 1));
static CSafeStatic<CAtomicCounter_WithAutoInit> s_ReopenEntered;

DEFINE_STATIC_MUTEX(s_ApproveMutex);


void g_Diag_Use_RWLock(bool enable)
{
    if (s_DiagUseRWLock == enable) return; // already switched
    // Try to detect at least some dangerous situations.
    // This does not guarantee safety since some thread may
    // be waiting for lock while we switch to the new method.
    if ( enable ) {
        bool diag_unlocked = s_DiagMutex.TryLock();
        // Mutex is locked - fail
        _ASSERT(diag_unlocked);
        if (!diag_unlocked) {
            _TROUBLE;
            NCBI_THROW(CCoreException, eCore,
                       "Cannot switch diagnostic to RW-lock - mutex is locked.");
        }
        s_DiagMutex.Unlock();
    }
    else {
        // Switch from RW-lock to mutex
        bool diag_unlocked = s_DiagRWLock->TryWriteLock();
        // RW-lock is locked - fail
        _ASSERT(diag_unlocked);
        if (!diag_unlocked) {
            _TROUBLE;
            NCBI_THROW(CCoreException, eCore,
                       "Cannot switch diagnostic to mutex - RW-lock is locked.");
        }
        s_DiagRWLock->Unlock();
    }
    s_DiagUseRWLock = enable;
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
        if (m_LockType == ePost) {
            s_DiagPostMutex.Lock();
        }
        else {
            s_DiagMutex.Lock();
        }
    }

    ~CDiagLock(void)
    {
        if (m_UsedRWLock) {
            s_DiagRWLock->Unlock();
        }
        else {
            if (m_LockType == ePost) {
                s_DiagPostMutex.Unlock();
            }
            else {
                s_DiagMutex.Unlock();
            }
        }
    }

private:
    bool      m_UsedRWLock;
    ELockType m_LockType;
};


static const char* kLogName_None     = "NONE";
static const char* kLogName_Unknown  = "UNKNOWN";
static const char* kLogName_Stdout   = "STDOUT";
static const char* kLogName_Stderr   = "STDERR";
static const char* kLogName_Stream   = "STREAM";
static const char* kLogName_Memory   = "MEMORY";
static const char* kLogName_Tee      = "STDERR-TEE";


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

    virtual void PostToConsole(const SDiagMessage& mess)
    {
        // Console manipulator ignores severity, so we have to always print
        // the message to console and set NoTee flag to avoid duplicates.
        CDiagHandler::PostToConsole(mess);
        const_cast<SDiagMessage&>(mess).m_NoTee = true;
    }

    virtual string GetLogName(void)
    {
        return m_OrigHandler.get() ?
            m_OrigHandler->GetLogName() : kLogName_Tee;
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
static CSafeStatic<NCBI_PARAM_TYPE(Diag, Old_Post_Format)> s_OldPostFormat(
    CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 2));

// Auto-print context properties on set/change.
NCBI_PARAM_DECL(bool, Diag, AutoWrite_Context);
NCBI_PARAM_DEF_EX(bool, Diag, AutoWrite_Context, false, eParam_NoThread,
                  DIAG_AUTOWRITE_CONTEXT);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, AutoWrite_Context)> s_AutoWrite_Context;

// Print system TID rather than CThread::GetSelf()
NCBI_PARAM_DECL(bool, Diag, Print_System_TID);
NCBI_PARAM_DEF_EX(bool, Diag, Print_System_TID, false, eParam_NoThread,
                  DIAG_PRINT_SYSTEM_TID);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, Print_System_TID)> s_PrintSystemTID;

// Use assert() instead of abort() when printing fatal errors
// to show the assertion dialog and allow to choose the action
// (stop/debug/continue).
NCBI_PARAM_DECL(bool, Diag, Assert_On_Abort);
NCBI_PARAM_DEF_EX(bool, Diag, Assert_On_Abort, false, eParam_NoThread,
                  DIAG_ASSERT_ON_ABORT);

// Limit log file size, rotate log when it reaches the limit.
NCBI_PARAM_DECL(long, Diag, Log_Size_Limit);
NCBI_PARAM_DEF_EX(long, Diag, Log_Size_Limit, 0, eParam_NoThread,
                  DIAG_LOG_SIZE_LIMIT);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, Log_Size_Limit)> s_LogSizeLimit;

// Limit max line length, zero = no limit (default).
NCBI_PARAM_DECL(size_t, Diag, Max_Line_Length);
NCBI_PARAM_DEF_EX(size_t, Diag, Max_Line_Length, 0, eParam_NoThread,
    DIAG_MAX_LINE_LENGTH);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, Max_Line_Length)> s_MaxLineLength;


///////////////////////////////////////////////////////
//  Output rate control parameters

class CLogRateLimit
{
public:
    typedef unsigned int TValue;
    CLogRateLimit(void) : m_Value(kMax_UInt) {}
    CLogRateLimit(TValue val) : m_Value(val) {}

    operator TValue(void) const
    {
        return m_Value;
    }

    void Set(TValue val)
    {
        m_Value = val;
    }

private:
    TValue m_Value;
};


CNcbiIstream& operator>>(CNcbiIstream& in, CLogRateLimit& lim)
{
    lim.Set(kMax_UInt);
    string s;
    getline(in, s);
    if ( !NStr::EqualNocase(s, "off") ) {
        lim.Set(NStr::StringToNumeric<CLogRateLimit::TValue>(s));
    }
    return in;
}


// Need CSafeStatic_Proxy<> instance for CLogRateLimit to allow static
// initialization of default values with unsigned int.
NCBI_PARAM_STATIC_PROXY(CLogRateLimit, CLogRateLimit::TValue);


// AppLog limit per period
NCBI_PARAM_DECL(CLogRateLimit, Diag, AppLog_Rate_Limit);
NCBI_PARAM_DEF_EX(CLogRateLimit, Diag, AppLog_Rate_Limit, 50000,
                  eParam_NoThread, DIAG_APPLOG_RATE_LIMIT);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, AppLog_Rate_Limit)> s_AppLogRateLimit;

// AppLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, AppLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, AppLog_Rate_Period, 10, eParam_NoThread,
                  DIAG_APPLOG_RATE_PERIOD);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, AppLog_Rate_Period)> s_AppLogRatePeriod;

// ErrLog limit per period
NCBI_PARAM_DECL(CLogRateLimit, Diag, ErrLog_Rate_Limit);
NCBI_PARAM_DEF_EX(CLogRateLimit, Diag, ErrLog_Rate_Limit, 5000,
                  eParam_NoThread, DIAG_ERRLOG_RATE_LIMIT);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, ErrLog_Rate_Limit)> s_ErrLogRateLimit;

// ErrLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, ErrLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, ErrLog_Rate_Period, 1, eParam_NoThread,
                  DIAG_ERRLOG_RATE_PERIOD);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, ErrLog_Rate_Period)> s_ErrLogRatePeriod;

// TraceLog limit per period
NCBI_PARAM_DECL(CLogRateLimit, Diag, TraceLog_Rate_Limit);
NCBI_PARAM_DEF_EX(CLogRateLimit, Diag, TraceLog_Rate_Limit, 5000,
                  eParam_NoThread, DIAG_TRACELOG_RATE_LIMIT);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, TraceLog_Rate_Limit)> s_TraceLogRateLimit;

// TraceLog period, sec
NCBI_PARAM_DECL(unsigned int, Diag, TraceLog_Rate_Period);
NCBI_PARAM_DEF_EX(unsigned int, Diag, TraceLog_Rate_Period, 1, eParam_NoThread,
                  DIAG_TRACELOG_RATE_PERIOD);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, TraceLog_Rate_Period)> s_TraceLogRatePeriod;

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


NCBI_PARAM_DECL(size_t, Diag, Collect_Limit);
NCBI_PARAM_DEF_EX(size_t, Diag, Collect_Limit, 1000, eParam_NoThread,
                  DIAG_COLLECT_LIMIT);


NCBI_PARAM_DECL(bool, Log, Truncate);
NCBI_PARAM_DEF_EX(bool, Log, Truncate, false, eParam_NoThread, LOG_TRUNCATE);


NCBI_PARAM_DECL(bool, Log, NoCreate);
NCBI_PARAM_DEF_EX(bool, Log, NoCreate, false, eParam_NoThread, LOG_NOCREATE);


// Logging of environment variables: space separated list of names which
// should be logged after app start and after each request start.
NCBI_PARAM_DECL(string, Log, LogEnvironment);
NCBI_PARAM_DEF_EX(string, Log, LogEnvironment, "",
                  eParam_NoThread,
                  DIAG_LOG_ENVIRONMENT);


// Logging of registry values: space separated list of 'section:name' strings.
NCBI_PARAM_DECL(string, Log, LogRegistry);
NCBI_PARAM_DEF_EX(string, Log, LogRegistry, "",
                  eParam_NoThread,
                  DIAG_LOG_REGISTRY);


// Turn off all applog messages (start/stop, request start/stop, extra).
NCBI_PARAM_DECL(bool, Diag, Disable_AppLog_Messages);
NCBI_PARAM_DEF_EX(bool, Diag, Disable_AppLog_Messages, false, eParam_NoThread,
    DIAG_DISABLE_APPLOG_MESSAGES);
static CSafeStatic<NCBI_PARAM_TYPE(Diag, Disable_AppLog_Messages)> s_DisableAppLog;

// Set applog event types to disable with DIAG_DISABLE_APPLOG_MESSAGES.
// By default all events are disabled.
NCBI_PARAM_ENUM_DECL(CDiagContext::EDisabledAppLogEvents, Diag, Disabled_Applog_Events);
NCBI_PARAM_ENUM_ARRAY(CDiagContext::EDisabledAppLogEvents, Diag, Disabled_Applog_Events)
{
    {"All", CDiagContext::eDisable_All},
    {"Enable_App", CDiagContext::eEnable_App}
};
NCBI_PARAM_ENUM_DEF_EX(CDiagContext::EDisabledAppLogEvents, Diag, Disabled_Applog_Events,
    CDiagContext::eDisable_All, eParam_NoThread, DIAG_DISABLED_APPLOG_EVENTS);


bool CDiagContext::GetDisabledAppLog(void)
{
    return s_DisableAppLog->Get();
}


CDiagContext::EDisabledAppLogEvents CDiagContext::GetDisabledAppLogEvents(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(Diag, Disabled_Applog_Events)> s_DisabledAppLogEvents;
    return s_DisabledAppLogEvents->Get();
}


static bool s_IsDisabledAppLogEvent(const CRequestContext& rctx, SDiagMessage::EEventType event_type)
{
    if (!rctx.GetDisabledAppLog()) return false;
    auto disabled_events = rctx.GetDisabledAppLogEvents();
    switch (disabled_events) {
    case CDiagContext::eEnable_App:
        // Allow only app-start and app-stop.
        return event_type != SDiagMessage::eEvent_Start &&
            event_type != SDiagMessage::eEvent_Stop;
    default:
        return true;
    }
}


static bool s_FinishedSetupDiag = false;


///////////////////////////////////////////////////////
//  CDiagContextThreadData::


/// Thread local context data stored in TLS
class CDiagContextThreadData
{
public:
    CDiagContextThreadData(void);
    ~CDiagContextThreadData(void);

    /// Get current request context.
    CRequestContext& GetRequestContext(void);
    /// Set request context. If NULL, switches the current thread
    /// to its default request context.
    void SetRequestContext(CRequestContext* ctx);

    typedef SDiagMessage::TCount TCount;

    /// Request id
    NCBI_DEPRECATED TCount GetRequestId(void);
    NCBI_DEPRECATED void SetRequestId(TCount id);
    NCBI_DEPRECATED void IncRequestId(void);

    /// Get request timer, create if not exist yet
    NCBI_DEPRECATED CStopWatch* GetOrCreateStopWatch(void) { return NULL; }
    /// Get request timer or null
    NCBI_DEPRECATED CStopWatch* GetStopWatch(void) { return NULL; }
    /// Delete request timer
    NCBI_DEPRECATED void ResetStopWatch(void) {}

    /// Diag buffer
    CDiagBuffer& GetDiagBuffer(void) { return *m_DiagBuffer; }

    /// Get diag context data for the current thread
    static CDiagContextThreadData& GetThreadData(void);

    /// Thread ID
    typedef Uint8 TTID;

    /// Get cached thread ID
    TTID GetTID(void) const { return m_TID; }

    /// Get thread post number
    TCount GetThreadPostNumber(EPostNumberIncrement inc);

    void AddCollectGuard(CDiagCollectGuard* guard);
    void RemoveCollectGuard(CDiagCollectGuard* guard);
    CDiagCollectGuard* GetCollectGuard(void);

    void CollectDiagMessage(const SDiagMessage& mess);

private:
    CDiagContextThreadData(const CDiagContextThreadData&);
    CDiagContextThreadData& operator=(const CDiagContextThreadData&);

    // Guards override the global post level and define severity
    // for collecting messages.
    typedef list<CDiagCollectGuard*> TCollectGuards;

    // Collected diag messages
    typedef list<SDiagMessage>       TDiagCollection;

    unique_ptr<CDiagBuffer> m_DiagBuffer;       // Thread's diag buffer
    TTID                  m_TID;              // Cached thread ID
    TCount                m_ThreadPostNumber; // Number of posted messages
    TCollectGuards        m_CollectGuards;
    TDiagCollection       m_DiagCollection;
    size_t                m_DiagCollectionSize; // cached size of m_DiagCollection
    CRef<CRequestContext> m_RequestCtx;        // Request context
    CRef<CRequestContext> m_DefaultRequestCtx; // Default request context
};


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
        psev = CDiagBuffer::sm_PostSeverity;
        csev = psev;
    }
    psev = CompareDiagPostLevel(psev, print_severity) > 0
        ? psev : print_severity;
    csev = CompareDiagPostLevel(csev, collect_severity) < 0
        ? csev : collect_severity;

    m_StartingPoint
        = CDiagContext::GetThreadPostNumber(ePostNumber_NoIncrement);
    m_PrintSev = psev;
    m_CollectSev = csev;
    m_SeverityCap = csev;
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
    if ( 0 != strcmp(module, "NCBI_MODULE") && x_NeedModule() ) {
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


inline Uint8 s_GetThreadId(void)
{
    if (s_PrintSystemTID->Get()) {
        return (Uint8)GetCurrentThreadSystemID(); // GCC 3.4.6 gives warning - ignore it.
    } else {
        return CThread::GetSelf();
    }
}

namespace {

enum EThreadDataState {
    eUninitialized = 0,
    eInitializing,
    eInitialized,
    eDeinitialized,
    eReinitializing
};

thread_local EThreadDataState s_ThreadDataState(eUninitialized);
thread_local CDiagContextThreadData* s_ThreadDataCache;

const auto main_thread_id = std::this_thread::get_id();

template<typename _T, typename _MainCleanup = void, typename _ThreadCleanup = _MainCleanup>
class CTrueTlsData
{
// Some notes for order of destruction
// 1. ncbi::CStaticTls destroyed by CThread (ThreadWrapperCallerImpl)
// 2. standard thread_local of a thread
// 3. standard thread_local of a main thread
// 4. global static variables

// main thread tls data is destructed before other static variables destructed
// this needs to be delayed, so main thread tls data is not used, instead static data employed
public:
    CTrueTlsData() = default;
    ~CTrueTlsData()
    {
    }

    using thread_storage = std::unique_ptr<_T,
        std::conditional_t<std::is_void_v<_ThreadCleanup>,
            std::default_delete<_T>, _ThreadCleanup>>;
    using main_storage = std::unique_ptr<_T,
        std::conditional_t<std::is_void_v<_MainCleanup>,
        std::default_delete<_T>, _MainCleanup>>;

    operator _T*() { return GetData(); }
    operator _T&() { return *GetData(); }

    _T* GetData() {
        if (std::this_thread::get_id() == main_thread_id) {
            return m_data.get();
        } else {
            thread_local thread_storage local_data{new _T};
            return local_data.get();
        }
    }
private:
    main_storage m_data{new _T};
};

struct CDiagContextThreadData_Cleanup
{
    void operator()(CDiagContextThreadData* data) const
    {
        {
            // TODO: this is the last thread, do you need a mutex?
            // TODO: why CDiagContextThreadData cares about Stop message?

            // Copy properties from the main thread's TLS to the global properties.
            CDiagLock lock(CDiagLock::eWrite);
            // Print stop message.
            if (!CDiagContext::IsSetOldPostFormat()  &&  s_FinishedSetupDiag) {
                GetDiagContext().PrintStop();
            }
        }
        std::default_delete<CDiagContextThreadData>{}(data);
    }
};


CTrueTlsData<CDiagContextThreadData, CDiagContextThreadData_Cleanup, void> g_thread_data;

}


bool CDiagContext::IsMainThreadDataInitialized(void)
{
    return CThread::IsMain() && s_ThreadDataState == eInitialized;
}


CDiagContextThreadData::CDiagContextThreadData(void)
    : m_DiagBuffer(new CDiagBuffer),
      m_TID(s_GetThreadId()),
      m_ThreadPostNumber(0),
      m_DiagCollectionSize(0)
{
    // Default context should auto-reset on request start.
    m_RequestCtx = m_DefaultRequestCtx = new CRequestContext(CRequestContext::fResetOnStart);
    m_RequestCtx->SetAutoIncRequestIDOnPost(CRequestContext::GetDefaultAutoIncRequestIDOnPost());
    s_ThreadDataState = eInitialized; // re-enable protection
}


CDiagContextThreadData::~CDiagContextThreadData(void)
{
    if ( s_ThreadDataCache == this ) {
        s_ThreadDataCache = 0;
        s_ThreadDataState = eDeinitialized; // re-enable protection
    }
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

    if ( CDiagContextThreadData* data = s_ThreadDataCache ) {
        return *data;
    }


    if (s_ThreadDataState != eInitialized) {
        // Avoid false positives, while also taking care not to call
        // anything that might itself produce diagnostics.
        EThreadDataState thread_data_state = s_ThreadDataState; // for Clang10
        switch (thread_data_state) {
        case eInitialized:
            break;

        case eUninitialized:
            s_ThreadDataState = eInitializing;
            break;

        case eInitializing:
            cerr << "FATAL ERROR: inappropriate recursion initializing NCBI"
                    " diagnostic framework.\n";
            Abort();
            break;

        case eDeinitialized:
            s_ThreadDataState = eReinitializing;
            cerr << "Reinitializing NCBI diagnostic framework\n";
            break;

        case eReinitializing:
            cerr << "FATAL ERROR: NCBI diagnostic framework no longer"
                    " initialized.\n";
            Abort();
            break;
        }
    }

    s_ThreadDataCache = g_thread_data;
    return *s_ThreadDataCache;
}


CRequestContext& CDiagContextThreadData::GetRequestContext(void)
{
    _ASSERT(m_RequestCtx);
    return *m_RequestCtx;
}


static const Uint8 kOwnerTID_None = Uint8(-1);

void CDiagContextThreadData::SetRequestContext(CRequestContext* ctx)
{
    if (m_RequestCtx) {
        // If pointers are the same (e.g. consecutive calls with the same ctx)
        if (ctx == m_RequestCtx.GetPointer()) {
            return;
        }

        // Reset TID in the context.
        m_RequestCtx->m_OwnerTID = kOwnerTID_None;
    }

    if (!ctx) {
        m_RequestCtx = m_DefaultRequestCtx;
        return;
    }

    m_RequestCtx = ctx;
    if (!m_RequestCtx->GetReadOnly()) {
        if (m_RequestCtx->m_OwnerTID == kOwnerTID_None) {
            // Save current TID in the context.
            m_RequestCtx->m_OwnerTID = m_TID;
        }
        else if (m_RequestCtx->m_OwnerTID != m_TID) {
            ERR_POST_X_ONCE(29,
                "Using the same CRequestContext in multiple threads is unsafe!"
                << CStackTrace());
            _TROUBLE;
        }
    }
    else {
        // Read-only contexts should not remember owner thread.
        m_RequestCtx->m_OwnerTID = kOwnerTID_None;
    }
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
    auto action = guard->GetAction();
    unique_ptr<CDiagLock> lock;
    if (action == CDiagCollectGuard::ePrintCapped) {
        lock.reset(new CDiagLock(CDiagLock::eRead));
        EDiagSev cap = guard->GetSeverityCap();
        auto start = guard->GetStartingPoint();
        NON_CONST_ITERATE(TDiagCollection, itc, m_DiagCollection) {
            if (itc->m_ThrPost >= start
                &&  CompareDiagPostLevel(itc->m_Severity, cap) > 0) {
                itc->m_Severity = cap;
            }
        }
        action = CDiagCollectGuard::ePrint;
    }
    if ( !m_CollectGuards.empty() ) {
        return;
        // Previously printing was done for each guard, discarding - only for
        // the last guard.
    }
    // If this is the last guard, perform its action
    if (lock.get() == nullptr) {
        lock.reset(new CDiagLock(CDiagLock::eRead));
    }
    if (action == CDiagCollectGuard::ePrint) {
        CDiagHandler* handler = GetDiagHandler();
        if ( handler ) {
            ITERATE(TDiagCollection, itc, m_DiagCollection) {
                if ((itc->m_Flags & eDPF_IsConsole) != 0) {
                    // Print all messages to console
                    handler->PostToConsole(*itc);
                }
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
    static CSafeStatic<NCBI_PARAM_TYPE(Diag, Collect_Limit)> s_DiagCollectLimit;
    if (m_DiagCollectionSize >= s_DiagCollectLimit->Get()) {
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


static CTime s_GetFastTime(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(Diag, UTC_Timestamp)> s_UtcTimestamp;
    return (s_UtcTimestamp->Get() && !CDiagContext::IsApplogSeverityLocked()) ?
        CTime(CTime::eCurrent, CTime::eGmt) : GetFastLocalTime();
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
      m_AppNameSet(false),
      m_LoggedHitId(false),
      m_ExitCode(0),
      m_ExitCodeSet(false),
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
    CMutexGuard lock(s_ApproveMutex);
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
        return s_AppLogRateLimit->Get();
    case eLogRate_Err:
        return s_ErrLogRateLimit->Get();
    case eLogRate_Trace:
    default:
        return s_TraceLogRateLimit->Get();
    }
}

void CDiagContext::SetLogRate_Limit(ELogRate_Type type, unsigned int limit)
{
    CMutexGuard lock(s_ApproveMutex);
    switch ( type ) {
    case eLogRate_App:
        s_AppLogRateLimit->Set(limit);
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
        s_ErrLogRateLimit->Set(limit);
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
        s_TraceLogRateLimit->Set(limit);
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
        return s_AppLogRatePeriod->Get();
    case eLogRate_Err:
        return s_ErrLogRatePeriod->Get();
    case eLogRate_Trace:
    default:
        return s_TraceLogRatePeriod->Get();
    }
}

void CDiagContext::SetLogRate_Period(ELogRate_Type type, unsigned int period)
{
    CMutexGuard lock(s_ApproveMutex);
    switch ( type ) {
    case eLogRate_App:
        s_AppLogRatePeriod->Set(period);
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
        s_ErrLogRatePeriod->Set(period);
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
        s_TraceLogRatePeriod->Set(period);
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
            CMutexGuard lock(s_ApproveMutex);
            approved = m_AppLogRC->Approve();
        }
        if ( approved ) {
            m_AppLogSuspended = false;
        }
        else {
            *show_warning = !m_AppLogSuspended.exchange(true);
        }
    }
    else {
        switch ( msg.m_Severity ) {
        case eDiag_Info:
        case eDiag_Trace:
            if ( m_TraceLogRC->IsEnabled() ) {
                CMutexGuard lock(s_ApproveMutex);
                approved = m_TraceLogRC->Approve();
            }
            if ( approved ) {
                m_TraceLogSuspended = false;
            }
            else {
                *show_warning = !m_TraceLogSuspended.exchange(true);
            }
            break;
        default:
            if ( m_ErrLogRC->IsEnabled() ) {
                CMutexGuard lock(s_ApproveMutex);
                approved = m_ErrLogRC->Approve();
            }
            if ( approved ) {
                m_ErrLogSuspended = false;
            }
            else {
                *show_warning = !m_ErrLogSuspended.exchange(true);
            }
        }
    }
    return approved;
}


CDiagContext::TPID CDiagContext::sm_PID = 0;

CDiagContext::TPID CDiagContext::GetPID(void)
{
    if ( !sm_PID ) {
        sm_PID = CCurrentProcess::GetPid();
    }
    return sm_PID;
}


bool CDiagContext::UpdatePID(void)
{
    TPID old_pid = sm_PID;
    TPID new_pid = CCurrentProcess::GetPid();
    if (sm_PID == new_pid) {
        return false;
    }
    sm_PID = new_pid;
    CDiagContext& ctx = GetDiagContext();
    TUID old_uid = ctx.GetUID();
    // Update GUID to match the new PID
    ctx.x_CreateUID();
    ctx.Extra().
        Print("action", "fork").
        Print("parent_guid", ctx.GetStringUID(old_uid)).
        Print("parent_pid", NStr::NumericToString(old_pid));
    return true;
}


bool CDiagContext::UpdatePID_AsyncSafe(void)
{
    TPID new_pid = CCurrentProcess::GetPid();
    if (sm_PID == new_pid) {
        return false;
    }
    sm_PID = new_pid;
    // Update GUID to match the new PID.
    // GetDiagContext() call is not async safe in common, but should be safe
    // after Diag API initialization, it's just doing a pointer type cast.
    GetDiagContext().x_CreateUID_AsyncSafe();
    return true;
}


void CDiagContext::UpdateOnFork(TOnForkFlags flags)
{
    if (flags & fOnFork_AsyncSafe) {
        UpdatePID_AsyncSafe();
        return;
    }
    // not async safe:

    // Do not perform any actions in the parent process
    if ( !UpdatePID() ) return;

    if (flags & fOnFork_ResetTimer) {
        GetDiagContext().m_StopWatch->Restart();
    }
    if (flags & fOnFork_PrintStart) {
        GetDiagContext().PrintStart(kEmptyStr);
    }
}


const CDiagContext::TUID kUID_InitBase =  212;
const CDiagContext::TUID kUID_Mult     = 1265;

CDiagContext::TUID s_CreateUID(CDiagContext::TUID base)
{
    // This method expected to be async-signal-safe
    // https://man7.org/linux/man-pages/man7/signal-safety.7.html
    // All non-async-safe code should go to x_CreateUID().

    typedef CDiagContext::TPID TPID;
    typedef CDiagContext::TUID TUID;

    TPID pid = CDiagContext::GetPID();
    time_t t = time(0);

    base &= 0xFFFF;

    // The low 4 bits are reserved for GUID generator version number.
    // As of October 2017 the following versions are used:
    //   0 - very old C++ Toolkit
    //   1 - contemporary C++ Toolkit
    //   2 - ?
    //   3 - "C" client (CLOG API); "Python" module 'python-applog'
    //   4 - ?
    //   5 - Java
    //   6 - Scala
    //  13 - Perl (NcbiApplog.pm)

    return (TUID(base) << 48) |
           ((TUID(pid) & 0xFFFF) << 32) |
           ((TUID(t) & 0xFFFFFFF) << 4) |
           1; // version #1 - fixed type conversion bug
}

void CDiagContext::x_CreateUID(void) const
{
    TUID base = kUID_InitBase;
    const string& host = GetHost();
    for (auto c : host) {
        base = base * kUID_Mult + c;
    }
    m_UID = s_CreateUID(base);
}

void CDiagContext::x_CreateUID_AsyncSafe(void) const
{
    // This method expected to be async-signal-safe
    // https://man7.org/linux/man-pages/man7/signal-safety.7.html
    // Implemented for Unix only, on other platforms works the same
    // as a regular x_CreateUID().

#if defined(NCBI_OS_UNIX)
    TUID base = kUID_InitBase;
    struct utsname buf;
    if (uname(&buf) >= 0) {
        const char* s = buf.nodename;
        while (*s) {
            base = base * kUID_Mult + *s;
            s++;
        }
    }
    else {
        // Rough workaround in case of uname() error
        base = base * kUID_Mult + (TUID)GetPID();
    }
    m_UID = s_CreateUID(base);
#else
    x_CreateUID();
#endif
}


DEFINE_STATIC_MUTEX(s_CreateGUIDMutex);

CDiagContext::TUID CDiagContext::GetUID(void) const
{
    if ( !m_UID ) {
        CMutexGuard guard(s_CreateGUIDMutex);
        if ( !m_UID ) {
            x_CreateUID();
        }
    }
    return m_UID;
}


void CDiagContext::GetStringUID(TUID uid, char* buf, size_t buflen) const
{
    _ASSERT(buflen > 16);
    int hi = int((uid >> 32) & 0xFFFFFFFF);
    int lo = int(uid & 0xFFFFFFFF);
    snprintf(buf, buflen, "%08X%08X", hi, lo);
}


void CDiagContext::GetStringUID(TUID uid, char* buf) const
{
    GetStringUID(uid, buf, 17);
}


string CDiagContext::GetStringUID(TUID uid) const
{
    char buf[17];
    if (uid == 0) {
        uid = GetUID();
    }
    GetStringUID(uid, buf, 17);
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
    return x_GetNextHitID(false);
}


string CDiagContext::x_GetNextHitID(bool is_default) const
{
    static CAtomicCounter s_HitIdCounter;

    Uint8 hi = GetUID();
    Uint4 b3 = Uint4((hi >> 32) & 0xFFFFFFFF);
    Uint4 b2 = Uint4(hi & 0xFFFFFFFF);

    CDiagContextThreadData& thr_data = CDiagContextThreadData::GetThreadData();
    Uint8 tid = (thr_data.GetTID() & 0xFFFFFF) << 40;
    Uint8 rid;
    if ( !is_default ) {
        rid = ((Uint8)thr_data.GetRequestContext().GetRequestID() & 0xFFFFFF) << 16;
    }
    else {
        rid = ((Uint8)0xFFFFFF) << 16;
    }
    Uint8 us = (Uint8)(s_HitIdCounter.Add(1) & 0xFFFF);
    Uint8 lo = tid | rid | us;
    Uint4 b1 = Uint4((lo >> 32) & 0xFFFFFFFF);
    Uint4 b0 = Uint4(lo & 0xFFFFFFFF);
    char buf[33];
    snprintf(buf, 33, "%08X%08X%08X%08X", b3, b2, b1, b0);
    return string(buf);
}


string CDiagContext::GetDefaultHitID(void) const
{
    return x_GetDefaultHitID(eHitID_Create).GetHitId();
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

    // All platforms - check NCBI_HOST first.
    const TXChar* ncbi_host = NcbiSys_getenv(_TX("NCBI_HOST"));
    if (ncbi_host  &&  *ncbi_host) {
        m_Host->SetString(_T_STDSTRING(ncbi_host));
        return m_Host->GetOriginalString();
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


DEFINE_STATIC_MUTEX(s_AppNameMutex);

const string& CDiagContext::GetAppName(void) const
{
    if ( !m_AppNameSet ) {
        CMutexGuard guard(s_AppNameMutex);
        if ( !m_AppNameSet ) {
            m_AppName->SetString(CNcbiApplication::GetAppName());
            // Cache appname if CNcbiApplication instance exists only,
            // the Diag API can be used before instance initialization and appname
            //  could be set to a binary name, and changes later (CXX-5076).
            if (CNcbiApplication::Instance()  &&  !m_AppName->IsEmpty()) {
                m_AppNameSet = true;
            }
        }
    }
    return m_AppName->GetOriginalString();
}


const string& CDiagContext::GetEncodedAppName(void) const
{
    if ( !m_AppNameSet ) {
        GetAppName(); // Initialize app name
    }
    return m_AppName->GetEncodedString();
}


void CDiagContext::SetAppName(const string& app_name)
{
    if ( m_AppNameSet ) {
        // AppName can be set only once
        ERR_POST("Application name cannot be changed.");
        return;
    }
    CMutexGuard guard(s_AppNameMutex);
    m_AppName->SetString(app_name);
    m_AppNameSet = true;
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
    s_AutoWrite_Context->Set(value);
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
        catch (const CException&) {
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
    if ( sm_Instance  &&  s_AutoWrite_Context->Get() ) {
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

    // Check global properties
    CDiagLock lock(CDiagLock::eRead);
    TProperties::const_iterator gprop = m_Properties.find(name);
    return gprop != m_Properties.end() ? gprop->second : kEmptyStr;
}


void CDiagContext::DeleteProperty(const string& name,
                                    EPropertyMode mode)
{
    // Check global properties
    CDiagLock lock(CDiagLock::eRead);
    TProperties::iterator gprop = m_Properties.find(name);
    if (gprop != m_Properties.end()) {
        m_Properties.erase(gprop);
    }
}


void CDiagContext::PrintProperties(void)
{
    CDiagLock lock(CDiagLock::eRead);
    ITERATE(TProperties, gprop, m_Properties) {
        x_PrintMessage(SDiagMessage::eEvent_Extra,
            gprop->first + "=" + gprop->second);
    }
}


void CDiagContext::PrintStart(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Start, message);
    // Print extra message. If ncbi_role and/or ncbi_location are set,
    // they will be logged by this extra. Otherwise nothing will be printed.
    Extra().PrintNcbiRoleAndLocation().PrintNcbiAppInfoOnStart().PrintNcbiAppInfoOnRequest().Flush();

    static const char* kCloudIdFile = "/etc/ncbi/cloudid";
    CFile cloudid(kCloudIdFile);
    if ( cloudid.Exists() ) {
        CDiagContext_Extra ex = Extra();
        CNcbiIfstream in(kCloudIdFile);
        while (!in.eof() && in.good()) {
            string s;
            getline(in, s);
            size_t sep = s.find('\t');
            if (sep == NPOS) continue;
            string name = NStr::TruncateSpaces(s.substr(0, sep));
            string value = s.substr(sep + 1);
            ex.Print(name, value);
        }
        ex.Flush();
    }

    x_LogEnvironment();

    // Log NCBI_LOG_FIELDS
    map<string, string> env_map;
    for (char **env = environ; *env; ++env) {
        string n, v;
        NStr::SplitInTwo(*env, "=", n, v, NStr::fSplit_Tokenize);
        NStr::ToLower(n);
        NStr::ReplaceInPlace(n, "_", "-");
        env_map[n] = v;
    }

    CNcbiLogFields f("env");
    f.LogFields(env_map);

    // Log hit id if already available.
    x_GetDefaultHitID(eHitID_NoCreate);
}


void CDiagContext::PrintStop(void)
{
    // If no hit id has been logged until app-stop,
    // try to force logging now.
    if (x_IsSetDefaultHitID()) {
        x_LogHitID_WithLock();
    }
    else {
        CRequestContext& ctx = GetRequestContext();
        if (ctx.IsSetHitID(CRequestContext::eHitID_Request)) {
            ctx.x_LogHitID(true);
        }
    }
    x_PrintMessage(SDiagMessage::eEvent_Stop, kEmptyStr);
}


void CDiagContext::PrintExtra(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Extra, message);
}


CDiagContext_Extra CDiagContext::PrintRequestStart(void)
{
    return CDiagContext_Extra(SDiagMessage::eEvent_RequestStart);
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
    m_AllowBadNames = true;
    return *this;
}


const TDiagPostFlags kApplogDiagPostFlags =
        eDPF_OmitInfoSev | eDPF_OmitSeparator | eDPF_AppLog;


CDiagContext_Extra& CDiagContext_Extra::PrintNcbiRoleAndLocation(void)
{
    const string& role = CDiagContext::GetHostRole();
    const string& loc = CDiagContext::GetHostLocation();
    if ( !role.empty() ) {
        Print("ncbi_role", role);
    }
    if ( !loc.empty() ) {
        Print("ncbi_location", loc);
    }
    return *this;
}

CDiagContext_Extra& CDiagContext_Extra::PrintNcbiAppInfoOnStart(void)
{
    Print("ncbi_app_username", CSystemInfo::GetUserName());
    CNcbiApplication* ins = CNcbiApplication::Instance();
    if (ins) {
        Print("ncbi_app_path", ins->GetProgramExecutablePath());
        const CVersionAPI& ver = ins->GetFullVersion();
        if (!ver.GetBuildInfo().date.empty()) {
            Print("ncbi_app_build_date", ver.GetBuildInfo().date);
        }
#if NCBI_PACKAGE
        Print("ncbi_app_package_name", ver.GetPackageName());
        string pkv = NStr::NumericToString(ver.GetPackageVersion().GetMajor())
            + "." +  NStr::NumericToString(ver.GetPackageVersion().GetMinor())
            + "." +  NStr::NumericToString(ver.GetPackageVersion().GetPatchLevel());
        Print("ncbi_app_package_version", pkv);
        Print("ncbi_app_package_date", NCBI_SBUILDINFO_DEFAULT().date);
#endif
        const SBuildInfo& bi = ver.GetBuildInfo();
        initializer_list<SBuildInfo::EExtra> bi_num = {
            SBuildInfo::eTeamCityProjectName,
            SBuildInfo::eTeamCityBuildConf,
            SBuildInfo::eTeamCityBuildNumber,
            SBuildInfo::eBuildID,
            SBuildInfo::eBuiltAs
        };
        for(SBuildInfo::EExtra key : bi_num) {
            string value = bi.GetExtraValue(key);
            if (!value.empty()) {
                Print( SBuildInfo::ExtraNameAppLog(key), value);
            }
        }
        return *this;
    }
#if defined(NCBI_TEAMCITY_PROJECT_NAME)
    Print("ncbi_app_tc_project", NCBI_TEAMCITY_PROJECT_NAME);
#endif
#if defined(NCBI_TEAMCITY_BUILDCONF_NAME)
    Print("ncbi_app_tc_conf", NCBI_TEAMCITY_BUILDCONF_NAME);
#endif
#if defined(NCBI_TEAMCITY_BUILD_NUMBER)
    Print("ncbi_app_tc_build", NStr::NumericToString<Uint8>(NCBI_TEAMCITY_BUILD_NUMBER));
#endif
#if defined(NCBI_TEAMCITY_BUILD_ID)
    Print("ncbi_app_build_id", NCBI_TEAMCITY_BUILD_ID);
#endif

    return *this;
}

CDiagContext_Extra& CDiagContext_Extra::PrintNcbiAppInfoOnRequest(void)
{
    CNcbiApplication* ins = CNcbiApplication::Instance();
    if (ins) {
        const CVersionAPI& ver = ins->GetFullVersion();
        const CVersionInfo& vi = ver.GetVersionInfo();
//#if defined (NCBI_SC_VERSION) && NCBI_SC_VERSION <= 21
#if 1
        string str = NStr::NumericToString(vi.GetMajor()) + "." +
                     NStr::NumericToString(vi.GetMinor()) + "." +
                     NStr::NumericToString(vi.GetPatchLevel());
        Print("ncbi_app_version", str);
#else
        initializer_list<int> vi_num = {vi.GetMajor(), vi.GetMinor(), vi.GetPatchLevel()};
        Print("ncbi_app_version", NStr::JoinNumeric(vi_num.begin(), vi_num.end(), "."));
#endif

        const SBuildInfo& bi = ver.GetBuildInfo();
        initializer_list<SBuildInfo::EExtra> bi_num =
            {   SBuildInfo::eProductionVersion,       SBuildInfo::eDevelopmentVersion,
                SBuildInfo::eStableComponentsVersion, SBuildInfo::eSubversionRevision,
                SBuildInfo::eRevision, SBuildInfo::eGitBranch};
        for(SBuildInfo::EExtra key : bi_num) {
            string value = bi.GetExtraValue(key);
            if (!value.empty()) {
                Print( SBuildInfo::ExtraNameAppLog(key), value);
            }
        }
        return *this;
    }
#if defined(NCBI_PRODUCTION_VER)
    Print("ncbi_app_prod_version", NStr::NumericToString<Uint8>(NCBI_PRODUCTION_VER));
#elif defined(NCBI_DEVELOPMENT_VER)
    Print("ncbi_app_dev_version", NStr::NumericToString<Uint8>(NCBI_DEVELOPMENT_VER));
#endif
#if defined(NCBI_SC_VERSION)
    Print("ncbi_app_sc_version", NStr::NumericToString<Uint8>(NCBI_SC_VERSION));
#endif
#if defined(NCBI_SUBVERSION_REVISION)
    Print("ncbi_app_vcs_revision", NStr::NumericToString<Uint8>(NCBI_SUBVERSION_REVISION));
#endif
    return *this;
}


void CDiagContext_Extra::Flush(void)
{
    if (m_Flushed  ||  CDiagContext::IsSetOldPostFormat()) {
        return;
    }

    // Add ncbi-role and ncbi-location just before setting m_Flushed flag.
    if (m_EventType == SDiagMessage::eEvent_RequestStart) {
        PrintNcbiRoleAndLocation().PrintNcbiAppInfoOnRequest();
    }
    // Prevent double-flush
    m_Flushed = true;

    // Ignore extra messages without arguments. Allow request-start/request-stop
    // and stop without arguments. Empty start messages are printed only if
    // ncbi_role/ncbi_location are available (see above).
    if ((m_EventType == SDiagMessage::eEvent_Extra  ||
        m_EventType == SDiagMessage::eEvent_Start)  &&
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

    string s;
    if (m_EventType == SDiagMessage::eEvent_PerfLog) {
        s.append(to_string(m_PerfStatus)).append(1, ' ')
            .append(NStr::DoubleToString(m_PerfTime, -1, NStr::fDoubleFixed));
    }

    if (!s_IsDisabledAppLogEvent(CDiagContext::GetRequestContext(), m_EventType)) {
        SDiagMessage mess(eDiag_Info,
                          s.data(), s.size(),
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


static const char* kNcbiApplogKeywordStrings[] = {
    // ncbi_phid, self_url and http_referrer are reported by corelib and cgi
    // and should not be renamed.
    /*
    "ncbi_phid",
    "self_url",
    "http_referrer",
    "prev_phid",
    */
    "app",
    "host",
    "ip",
    "ip_addr",
    "session_id",
    "date",
    "datetime",
    "time",
    "guid",
    "acc_time_secs",
    "bytes_in_bit",
    "bytes_out_bit",
    "day",
    "day_of_week",
    "day_of_year",
    "exec_time_msec_bit",
    "start_time_msec",
    "second",
    "minute",
    "hour",
    "iter",
    "month",
    "pid",
    "status",
    "tid",
    "year",
    "geo_metro_code",
    "geo_latitude_hundredths",
    "geo_longitude_hundredths",
    "session_pageviews",
    "ping_count",
    "session_duration",
    "visit_duration",
    "time_on_page",
    "time_to_last_ping",
    "bytes_in",
    "bytes_out",
    "exec_time_msec",
    "self_url_path",
    "self_url_host",
    "self_url_file",
    "self_url_file_ext",
    "self_url_top_level_dir",
    "self_url_directory",
    "referrer",
    "ref_url",
    "ref_host",
    "http_referer",
    "referer",
    "interactive_hit",
    "statusuidlookup",
    "usageuidlookup",
    "all",
    "has_app_data",
    "has_critical",
    "has_duplicate_nexus",
    "has_duplicate_phid",
    "has_error",
    "has_fatal",
    "has_info",
    "has_multiple_phid",
    "has_ping",
    "has_start",
    "has_stop",
    "has_warning",
    "is_app_hit",
    "is_perf",
    "multiple_session_hit",
    "multiple_session_ip",
    "ncbi_phid_first_render",
    "phid_from_other_request",
    "phid_stack_ambiguous_relationship",
    "phid_stack_has_fake_child",
    "phid_stack_has_fake_parent",
    "phid_stack_missing_child",
    "phid_stack_missing_parent",
    "phid_stack_multivalued",
    "session_bot",
    "session_bot_by_ip",
    "session_bot_by_rate",
    "session_bot_by_user_agent",
    "session_bot_ip",
    "session_has_ping",
    "browser_name",
    "browser_version",
    "browser_major_version",
    "browser_platform",
    "browser_engine",
    "is_mobile",
    "is_tablet",
    "is_phone",
    "is_browser_bot",
    "applog_db_path",
    "applog_db_size",
    "applog_db_size_hit_fraction",
    "applog_db_machine",
    "applog_db_vol_id",
    "applog_db_type",
    "applog_db_date_tag",
    "applog_db_date_range",
    "applog_db_stream",
    "applog_db_create_time",
    "applog_db_modify_time",
    "applog_db_state",
    "applog_db_uid",
    "applog_db_agent_host",
    "applog_db_agent_pid",
    "applog_db_agent_tid",
    "applog_db_volume_served_time",
    "phid_stack_missing_parent",
    "phid_stack_missing_child",
    "phid_stack_has_fake_child",
    "phid_stack_has_fake_parent",
    "phid_stack_ambiguous_relationship",
    "phid_stack_multivalued",
    "ncbi_phid_first_render",
    "multiple_session_hit",
    "has_ping",
    "session_has_ping",
    "has_duplicate_nexus",
    "has_app_data",
    "has_multiple_phid",
    "multi_hit_phid",
    "session_bot_by_rate",
    "session_bot_by_user_agent",
    "session_bot",
    "session_bot_by_ip",
    "session_bot_ip",
    "multiple_session_ip",
    "collapsed_jsevent"
};


struct SNcbiApplogKeywordsInit
{
    typedef unordered_set<string> TKeywords;

    TKeywords* Create(void)
    {
        TKeywords* kw = new TKeywords;
        int sz = sizeof(kNcbiApplogKeywordStrings) / sizeof(kNcbiApplogKeywordStrings[0]);
        for (int i = 0; i < sz; ++i) {
            kw->insert(kNcbiApplogKeywordStrings[i]);
        }
        return kw;
    }

    void Cleanup(TKeywords& /*keywords*/) {}
};


static CSafeStatic<SNcbiApplogKeywordsInit::TKeywords, SNcbiApplogKeywordsInit> s_NcbiApplogKeywords;

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

    // If arg name is a reserved applog keywork, rename it and log warning.
    SNcbiApplogKeywordsInit::TKeywords& kw = s_NcbiApplogKeywords.Get();
    if (kw.find(name) == kw.end()) {
        m_Args->rbegin()->first.assign(name);
    }
    else {
        string renamed = "auto_renamed_applog_keyword__" + name;
        m_Args->rbegin()->first.assign(renamed);
        ERR_POST("'" << name
            << "' is a reserved NCBI AppLog keyword, so it has been renamed to "
            << renamed);
    }
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
        // Now back at application level check if a default hit id
        // needs to be logged.
        x_LogHitID_WithLock();
    }
}


EDiagAppState CDiagContext::GetGlobalAppState(void) const
{
    return m_AppState;
}


EDiagAppState CDiagContext::GetAppState(void) const
{
    // This checks thread's state first, then calls GetAppState if necessary.
    return GetRequestContext().GetAppState();
}


void CDiagContext::SetGlobalAppState(EDiagAppState state)
{
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


// Session id passed through HTTP
NCBI_PARAM_DECL(string, Log, Http_Session_Id);
NCBI_PARAM_DEF_EX(string, Log, Http_Session_Id, "", eParam_NoThread,
                  HTTP_NCBI_SID);
static CSafeStatic<NCBI_PARAM_TYPE(Log, Http_Session_Id)> s_HttpSessionId;

// Session id set in the environment
NCBI_PARAM_DECL(string, Log, Session_Id);
NCBI_PARAM_DEF_EX(string, Log, Session_Id, "", eParam_NoThread,
                  NCBI_LOG_SESSION_ID);
static CSafeStatic<NCBI_PARAM_TYPE(Log, Session_Id)> s_DefaultSessionId;


DEFINE_STATIC_MUTEX(s_DefaultSidMutex);

string CDiagContext::GetDefaultSessionID(void) const
{
    CMutexGuard lock(s_DefaultSidMutex);
    if (m_DefaultSessionId.get() && !m_DefaultSessionId->IsEmpty()) {
        return m_DefaultSessionId->GetOriginalString();
    }

    // Needs initialization.
    if ( !m_DefaultSessionId.get() ) {
        m_DefaultSessionId.reset(new CEncodedString);
    }
    if ( m_DefaultSessionId->IsEmpty() ) {
        string sid = CRequestContext::SelectLastSessionID(
            s_HttpSessionId->Get());
        if ( sid.empty() ) {
            sid = CRequestContext::SelectLastSessionID(
                s_DefaultSessionId->Get());
        }
        m_DefaultSessionId->SetString(sid);
    }
    return m_DefaultSessionId->GetOriginalString();
}


void CDiagContext::SetDefaultSessionID(const string& session_id)
{
    CMutexGuard lock(s_DefaultSidMutex);
    if ( !m_DefaultSessionId.get() ) {
        m_DefaultSessionId.reset(new CEncodedString);
    }
    m_DefaultSessionId->SetString(session_id);
}


string CDiagContext::GetSessionID(void) const
{
    CRequestContext& rctx = GetRequestContext();
    if ( rctx.IsSetExplicitSessionID() ) {
        return rctx.GetSessionID();
    }
    return GetDefaultSessionID();
}


string CDiagContext::GetEncodedSessionID(void) const
{
    CRequestContext& rctx = GetRequestContext();
    if ( rctx.IsSetExplicitSessionID() ) {
        return rctx.GetEncodedSessionID();
    }
    GetDefaultSessionID(); // Make sure the default value is initialized.
    CMutexGuard lock(s_DefaultSidMutex);
    _ASSERT(m_DefaultSessionId.get());
    return m_DefaultSessionId->GetEncodedString();
}


NCBI_PARAM_DECL(string, Log, Client_Ip);
NCBI_PARAM_DEF_EX(string, Log, Client_Ip, "", eParam_NoThread,
                  NCBI_LOG_CLIENT_IP);
static CSafeStatic<NCBI_PARAM_TYPE(Log, Client_Ip)> s_DefaultClientIp;


string CDiagContext::GetDefaultClientIP(void)
{
    return s_DefaultClientIp->Get();
}


void CDiagContext::SetDefaultClientIP(const string& client_ip)
{
    s_DefaultClientIp->Set(client_ip);
}


// Hit id passed through HTTP
NCBI_PARAM_DECL(string, Log, Http_Hit_Id);
NCBI_PARAM_DEF_EX(string, Log, Http_Hit_Id, "", eParam_NoThread,
                  HTTP_NCBI_PHID);
static CSafeStatic<NCBI_PARAM_TYPE(Log, Http_Hit_Id)> s_HttpHitId;

// Hit id set in the environment or registry
NCBI_PARAM_DECL(string, Log, Hit_Id);
NCBI_PARAM_DEF_EX(string, Log, Hit_Id, "", eParam_NoThread,
                  NCBI_LOG_HIT_ID);
static CSafeStatic<NCBI_PARAM_TYPE(Log, Hit_Id)> s_HitId;


DEFINE_STATIC_MUTEX(s_DefaultHidMutex);

bool CDiagContext::x_DiagAtApplicationLevel(void) const
{
    EDiagAppState state = GetAppState();
    return (state == eDiagAppState_AppBegin) ||
        (state == eDiagAppState_AppRun) ||
        (state == eDiagAppState_AppEnd);
}


void CDiagContext::x_LogHitID(void) const
{
    // NOTE: The method must be always called with s_DefaultHidMutex locked.

    // Log the default hit id only when at application level. Otherwise
    // pospone it untill request-stop/app-stop.
    if (m_LoggedHitId || !m_DefaultHitId.get() || m_DefaultHitId->Empty() ||
        !x_DiagAtApplicationLevel()) {
        return;
    }
    Extra().Print(g_GetNcbiString(eNcbiStrings_PHID), m_DefaultHitId->GetHitId());
    m_LoggedHitId = true;
}


void CDiagContext::x_LogHitID_WithLock(void) const
{
    CMutexGuard guard(s_DefaultHidMutex);
    x_LogHitID();
}


bool CDiagContext::x_IsSetDefaultHitID(void) const
{
    CMutexGuard guard(s_DefaultHidMutex);
    return m_DefaultHitId.get() && !m_DefaultHitId->Empty();
}


CSharedHitId CDiagContext::x_GetDefaultHitID(EDefaultHitIDFlags flag) const
{
    CMutexGuard guard(s_DefaultHidMutex);
    if (m_DefaultHitId.get()  &&  !m_DefaultHitId->Empty()) {
        return *m_DefaultHitId;
    }

    if ( !m_DefaultHitId.get() ) {
        m_DefaultHitId.reset(new CSharedHitId());
    }
    if ( m_DefaultHitId->Empty() ) {
        m_DefaultHitId->SetHitId(CRequestContext::SelectLastHitID(
            s_HttpHitId->Get()));
        if ( m_DefaultHitId->Empty() ) {
            string phid = CRequestContext::SelectLastHitID(
                s_HitId->Get());
            if ( !phid.empty() ) {
                const char* c_env_job_id = getenv("JOB_ID");
                string env_job_id = c_env_job_id ? string(c_env_job_id): "";
                const char* c_env_task_id = getenv("SGE_TASK_ID");
                string env_task_id = c_env_task_id ? string(c_env_task_id) : "";
                if (env_task_id.find_first_not_of("0123456789") != NPOS) {
                    // If task id is 'undefined', set it to 1 - it's a job with
                    // just one task.
                    env_task_id = "1";
                }
                if (!env_job_id.empty()  &&  !env_task_id.empty()) {
                    // FIX LATER: It's better to use J and T prefix
                    // to indicate job/task, but currently CRequestContext
                    // logs errors if sub-hit id contains non-digits.
                    // Using leading 0s is a temporary solution. Later,
                    // when most apps are recompiled with the new
                    // CRequestContext, we can reconsider using J/T.
                    string jid = ".000" + env_job_id;
                    string tid = ".00" + env_task_id;
                    size_t jid_pos = phid.find(jid);
                    if (jid_pos == NPOS) {
                        // No current job id in the phid.
                        phid += jid + tid;
                    }
                    else {
                        // Job id is already in the phid. Check task id.
                        if (phid.find(tid, jid_pos + jid.size()) == NPOS) {
                            // New task in the same job.
                            phid += tid;
                        }
                        // else - task id already in the phid. Ignore.
                    }
                }
            }
            m_DefaultHitId->SetHitId(phid);
        }
        if (m_DefaultHitId->Empty()  &&  (flag == eHitID_Create)) {
            m_DefaultHitId->SetHitId(x_GetNextHitID(true));
        }
    }
    // Default hit id always uses shared sub-hit counter.
    m_DefaultHitId->SetShared();
    _ASSERT(!m_LoggedHitId);
    // Log hit id if at application level.
    x_LogHitID();
    return *m_DefaultHitId;
}


void CDiagContext::SetDefaultHitID(const string& hit_id)
{
    CMutexGuard guard(s_DefaultHidMutex);
    if ( !m_DefaultHitId.get() ) {
        m_DefaultHitId.reset(new CSharedHitId());
    }
    m_DefaultHitId->SetHitId(hit_id);
    // Default hit id always uses shared sub-hit counter.
    m_DefaultHitId->SetShared();
    // Log new hit id when at application level.
    m_LoggedHitId = false;
    x_LogHitID();
}


inline string s_ReadString(const char* filename)
{
    string ret;
    CNcbiIfstream in(filename);
    if ( in.good() ) {
        getline(in, ret);
    }
    return ret;
}


static CSafeStatic< unique_ptr<string> > s_HostRole;
static CSafeStatic< unique_ptr<string> > s_HostLocation;

const string& CDiagContext::GetHostRole(void)
{
    if ( !s_HostRole->get() ) {
        CDiagLock lock(CDiagLock::eWrite);
        if ( !s_HostRole->get() ) {
            unique_ptr<string> role(new string);
            const TXChar* env_role = NcbiSys_getenv(_TX("NCBI_ROLE"));
            if (env_role && *env_role) {
                *role = string(_T_CSTRING(env_role));
            }
            else {
                *role = s_ReadString("/etc/ncbi/role");
            }
            s_HostRole->reset(role.release());
        }
    }
    return **s_HostRole;
}


const string& CDiagContext::GetHostLocation(void)
{
    if ( !s_HostLocation->get() ) {
        CDiagLock lock(CDiagLock::eWrite);
        if ( !s_HostLocation->get() ) {
            unique_ptr<string> loc(new string);
            const TXChar* env_loc = NcbiSys_getenv(_TX("NCBI_LOCATION"));
            if (env_loc && *env_loc) {
                *loc = string(_T_CSTRING(env_loc));
            }
            else {
                *loc = s_ReadString("/etc/ncbi/location");
            }
            s_HostLocation->reset(loc.release());
        }
    }
    return **s_HostLocation;
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
    char uid[17];
    GetStringUID(msg.GetUID(), uid, 17);
    const string& host = msg.GetHost();
    string client = msg.GetClient();
    string session = msg.GetSession();
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
    if ( !ctx.IsSetExplicitClientIP() ) {
        string ip = GetDefaultClientIP();
        if ( !ip.empty() ) {
            ctx.SetClientIP(ip);
        }
    }

    ctx.StartRequest();
    x_LogEnvironment();
}


void CDiagContext::x_LogEnvironment(void)
{
    // Print selected environment and registry values.
    static CSafeStatic<NCBI_PARAM_TYPE(Log, LogEnvironment)> s_LogEnvironment;
    string log_args = s_LogEnvironment->Get();
    if ( !log_args.empty() ) {
        list<string> log_args_list;
        NStr::Split(log_args, " ", log_args_list,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogEnvironment", "true");
        {{
            // The guard must be released before flushing the extra -
            // otherwise there may be a deadlock when accessing CParam-s
            // from other threads.
            CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
            if ( app ) {
                const CNcbiEnvironment& env = app->GetEnvironment();
                ITERATE(list<string>, it, log_args_list) {
                    const string& val = env.Get(*it);
                    extra.Print(*it, val);
                }
            }
        }}
        extra.Flush();
    }
    static CSafeStatic<NCBI_PARAM_TYPE(Log, LogRegistry)> s_LogRegistry;
    log_args = s_LogRegistry->Get();
    if ( !log_args.empty() ) {
        list<string> log_args_list;
        NStr::Split(log_args, " ", log_args_list,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogRegistry", "true");
        {{
            // The guard must be released before flushing the extra -
            // see above.
            CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
            if ( app ) {
                const CNcbiRegistry& reg = app->GetConfig();
                ITERATE(list<string>, it, log_args_list) {
                    string section, name;
                    NStr::SplitInTwo(*it, ":", section, name);
                    const string& val = reg.Get(section, name);
                    extra.Print(*it, val);
                }
            }
        }}
        extra.Flush();
    }
}


void CDiagContext::x_PrintMessage(SDiagMessage::EEventType event,
                                  const string&            message)
{
    if ( IsSetOldPostFormat() ) {
        return;
    }
    string str;
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
        str.append(to_string(GetExitCode())).append(1, ' ').append(m_StopWatch->AsString());
        if (GetExitSignal() != 0) {
            str.append(" SIG=").append(to_string(GetExitSignal()));
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
            str.append(to_string(ctx.GetRequestStatus())).append(1, ' ')
                .append(ctx.GetRequestTimer().AsString()).append(1, ' ')
                .append(to_string(ctx.GetBytesRd())).append(1, ' ')
                .append(to_string(ctx.GetBytesWr()));
            need_space = true;
            break;
        }
    default:
        return; // Prevent warning about other event types.
    }
    if ( !message.empty() ) {
        if (need_space) {
            str.append(1, ' ');
        }
        str.append(message);
    }

    if (!s_IsDisabledAppLogEvent(ctx, event)) {
        SDiagMessage mess(eDiag_Info,
            str.data(), str.size(),
            0, 0, // file, line
            CNcbiDiag::ForceImportantFlags(kApplogDiagPostFlags),
            NULL,
            0, 0, // err code/subcode
            NULL,
            0, 0, 0); // module/class/function
        mess.m_Event = event;
        CDiagBuffer::DiagHandler(mess);
    }

    if (event == SDiagMessage::eEvent_RequestStop) {
        // Reset request context after stopping the request.
        ctx.StopRequest();
    }
}


bool CDiagContext::IsSetOldPostFormat(void)
{
    return s_OldPostFormat->Get();
}


void CDiagContext::SetOldPostFormat(bool value)
{
    s_OldPostFormat->Set(value);
}


bool CDiagContext::IsUsingSystemThreadId(void)
{
     return s_PrintSystemTID->Get();
}


void CDiagContext::UseSystemThreadId(bool value)
{
    s_PrintSystemTID->Set(value);
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
    unique_ptr<TMessages> tmp(m_Messages.release());
    //ERR_POST_X(1, Note << "***** BEGIN COLLECTED MESSAGES *****");
    NON_CONST_ITERATE(TMessages, it, *tmp.get()) {
        it->m_NoTee = true; // Do not tee duplicate messages to console.
        handler.Post(*it);
        if (it->m_Flags & eDPF_IsConsole) {
            handler.PostToConsole(*it);
        }
    }
    //ERR_POST_X(2, Note << "***** END COLLECTED MESSAGES *****");
    m_Messages.reset(tmp.release());
}


void CDiagContext::DiscardMessages(void)
{
    m_Messages.reset();
}


// Diagnostics setup

static const char* kRootLog = "/log/";

string GetDefaultLogLocation(CNcbiApplication& app)
{
    static const char* kToolkitRcPath = "/etc/toolkitrc";
    static const char* kWebDirToPort = "Web_dir_to_port";

    string log_path = kRootLog;

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


typedef NCBI_PARAM_TYPE(Log, Truncate) TLogTruncateParam;

bool CDiagContext::GetLogTruncate(void)
{
    return TLogTruncateParam::GetDefault();
}


void CDiagContext::SetLogTruncate(bool value)
{
    TLogTruncateParam::SetDefault(value);
}


bool OpenLogFileFromConfig(const string& logname)
{
    if ( !logname.empty() ) {
        if (NCBI_PARAM_TYPE(Log, NoCreate)::GetDefault()  &&  !CDirEntry(logname).Exists() ) {
            return false;
        }
        return SetLogFile(logname, eDiagFile_All, true);
    }
    return false;
}


void CDiagContext::SetUseRootLog(void)
{
    if (!s_FinishedSetupDiag) {
        SetupDiag();
    }
}


void CDiagContext::x_FinalizeSetupDiag(void)
{
    _ASSERT(!s_FinishedSetupDiag);
    s_FinishedSetupDiag = true;
}


// Load string value from config if not null, or from the environment.
static string s_GetLogConfigString(const CTempString name,
                                   const CTempString defval,
                                   CNcbiRegistry*    config)
{
    if ( config ) {
        return config->GetString("LOG", name, defval);
    }
    string envname = "NCBI_CONFIG__LOG__";
    envname += name;
    const TXChar* val = NcbiSys_getenv(_T_XCSTRING(envname));
    return val ? CTempString(_T_STDSTRING(val)) : defval;
}


// Load bool value from config if not null, or from the environment.
static bool s_GetLogConfigBool(const CTempString name,
                               bool              defval,
                               CNcbiRegistry*    config)
{
    if ( config ) {
        return config->GetBool("LOG", name, defval);
    }
    string envname = "NCBI_CONFIG__LOG__";
    envname += name;
    const TXChar* val = NcbiSys_getenv(_T_XCSTRING(envname));
    if ( val ) {
        try {
            return NStr::StringToBool(_T_STDSTRING(val));
        }
        catch (const CStringException&) {
        }
    }
    return defval;
}


void CDiagContext::SetupDiag(EAppDiagStream       ds,
                             CNcbiRegistry*       config,
                             EDiagCollectMessages collect,
                             const char*          cmd_logfile)
{
    CDiagLock lock(CDiagLock::eWrite);

    CParamBase::EnableConfigDump(false);

    // Check severity level change status now.
    // This used to be done in SetDiag(), but if the first logging event is an
    // Extra(), new post format is enabled and post level is Trace, the initialization
    // results in infinite recursion. To prevent this, call this as soon as possible,
    // before any logging is done.
    if ( CDiagBuffer::sm_PostSeverityChange == eDiagSC_Unknown ) {
        CDiagBuffer::GetSeverityChangeEnabledFirstTime();
    }

    CDiagContext& ctx = GetDiagContext();
    // Initialize message collecting
    if (collect == eDCM_Init) {
        ctx.InitMessages();
    }
    else if (collect == eDCM_InitNoLimit) {
        ctx.InitMessages(size_t(-1));
    }

    /*
    The default order of checking possible log locations is:

      1. CMD - '-logfile <filename>' command line arg
      2. ENV - NCBI_CONFIG__LOG__FILE (or [Log]/File)
      3. LOG - /log/ if writable
      4. Other locations depending on 'ds' flag (e.g. current directory)

      IgnoreEnvArg: if CMD should be checked before (true) or after (false) ENV/LOG.
      TryRootLogFirst: if LOG should be checked berofe (true) or after (false) ENV.

      The resulting order is:

           IgnoreEnvArg: | true              | false
      -------------------+-------------------+---------------
      TryRootLogFirst:   |                   |
                    true | CMD, LOG, ENV     | LOG, ENV, CMD
                   false | CMD, ENV, LOG     | ENV, LOG, CMD

      Successfull opening of CMD/ENV/LOG also overrides any eDS_* flags except eDS_User.
    */

    bool log_set = false;
    bool to_applog = false;
    string old_log_name;
    string new_log_name;

    CDiagHandler* old_handler = GetDiagHandler();
    if ( old_handler ) {
        old_log_name = old_handler->GetLogName();
    }

    string config_logfile = s_GetLogConfigString("FILE", kEmptyStr, config);
    if ( config_logfile.empty() ) {
        // Try the old-style name
        config_logfile = s_GetLogConfigString("File", kEmptyStr, config);
    }
    bool cmdline_first = s_GetLogConfigBool("IgnoreEnvArg", true, config);
    bool applog_first = s_GetLogConfigBool("TryRootLogFirst", false, config);

    if (ds != eDS_User) {
        // If cmdline_first is not set, it will be checked later, after
        // checking all other possibilities.
        if (cmdline_first  &&  cmd_logfile) {
            if ( SetLogFile(cmd_logfile, eDiagFile_All) ) {
                log_set = true;
                new_log_name = cmd_logfile;
            }
        }
        // If applog_first is set, config will be checked later for eDS_ToStdlog
        // and eDS_Default but only if /log is not available.
        if (!log_set  &&  !applog_first  &&  !config_logfile.empty()) {
            if ( OpenLogFileFromConfig(config_logfile) ) {
                log_set = true;
                new_log_name = config_logfile;
            }
        }
        // The following three eDS_* options should check /log/* before using -logfile.
        if (!log_set  &&  !cmdline_first  &&  cmd_logfile  &&
            ds != eDS_Default  &&  ds != eDS_ToStdlog  &&  ds != eDS_ToSyslog) {
            if ( SetLogFile(cmd_logfile, eDiagFile_All) ) {
                log_set = true;
                new_log_name = cmd_logfile;
            }
        }
    }

    if ( !log_set ) {
        switch ( ds ) {
        case eDS_ToStdout:
            if (old_log_name != kLogName_Stdout) {
                NCBI_LSAN_DISABLE_GUARD;
                SetDiagHandler(new CStreamDiagHandler(&cout, true, kLogName_Stdout), true);
                log_set = true;
                new_log_name = kLogName_Stdout;
            }
            break;
        case eDS_ToStderr:
            if (old_log_name != kLogName_Stderr) {
                NCBI_LSAN_DISABLE_GUARD;
                SetDiagHandler(new CStreamDiagHandler(&cerr, true, kLogName_Stderr), true);
                log_set = true;
                new_log_name = kLogName_Stderr;
            }
            break;
        case eDS_ToMemory:
            if (old_log_name != kLogName_Memory) {
                ctx.InitMessages(size_t(-1));
                SetDiagStream(0, false, 0, 0, kLogName_Memory);
                log_set = true;
                new_log_name = kLogName_Memory;
            }
            collect = eDCM_NoChange; // prevent flushing to memory
            break;
        case eDS_Disable:
            if (old_log_name != kLogName_None) {
                SetDiagStream(0, false, 0, 0, kLogName_None);
                log_set = true;
                new_log_name = kLogName_None;
            }
            break;
        case eDS_User:
            collect = eDCM_Discard;
            break;
        case eDS_AppSpecific:
            collect = eDCM_Discard;
            break;
        case eDS_ToSyslog:
            if (old_log_name != CSysLog::kLogName_Syslog) {
                try {
                    NCBI_LSAN_DISABLE_GUARD;
                    SetDiagHandler(new CSysLog);
                    log_set = true;
                    new_log_name = CSysLog::kLogName_Syslog;
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
                // When using applog, create separate log file for each
                // user to avoid permission problems.
                string euid;
#if defined(NCBI_OS_UNIX)
                euid = "." + NStr::NumericToString(geteuid());
#endif
                string log_base;
                string def_log_dir;
                {{
                    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
                    if ( app ) {
                        log_base = app->GetProgramExecutablePath();
                        def_log_dir = GetDefaultLogLocation(*app);
                    }
                }}
                if ( !log_base.empty() ) {
                    log_base = CFile(log_base).GetBase() + euid + ".log";
                    string log_name;
                    // Try /log/<port>
                    if ( !def_log_dir.empty() ) {
                        log_name = CFile::ConcatPath(def_log_dir, log_base);
                        if ( SetLogFile(log_name) ) {
                            log_set = true;
                            new_log_name = log_name;
                            to_applog = true;
                            break;
                        }
                    }
                    // Try /log/srv if port is unknown or not writable
                    log_name = CFile::ConcatPath(CFile::ConcatPath(kRootLog, "srv"), log_base);
                    if ( SetLogFile(log_name) ) {
                        log_set = true;
                        new_log_name = log_name;
                        to_applog = true;
                        break;
                    }
                    // Have we skipped config_logfile above?
                    if (applog_first &&
                        OpenLogFileFromConfig(config_logfile)) {
                        log_set = true;
                        new_log_name = config_logfile;
                        break;
                    }
                    // Try to switch to /log/fallback/
                    log_name = CFile::ConcatPath(CFile::ConcatPath(kRootLog, "fallback"), log_base);
                    if ( SetLogFile(log_name) ) {
                        log_set = true;
                        new_log_name = log_name;
                        to_applog = true;
                        break;
                    }
                    // Try cwd/ for eDS_ToStdlog only
                    if (ds == eDS_ToStdlog) {
                        // Don't include euid in file name when using cwd.
                        log_name = CFile::ConcatPath(".", CFile(log_base).GetBase() + ".log");
                        if ( SetLogFile(log_name, eDiagFile_All) ) {
                            log_set = true;
                            new_log_name = log_name;
                            break;
                        }
                    }
                    _TRACE_X(3, "Failed to set log file to " +
                        CFile::NormalizePath(log_name));
                }
                // Have we skipped cmd_logfile above?
                if (!log_set  &&  !cmdline_first  &&  cmd_logfile) {
                    if ( SetLogFile(cmd_logfile, eDiagFile_All) ) {
                        log_set = true;
                        new_log_name = cmd_logfile;
                        break;
                    }
                }
                // No command line and no base name.
                // Try to switch to /log/fallback/UNKNOWN.<euid>
                if (!log_set  &&  log_base.empty()) {
                    string default_fallback = CFile::ConcatPath(kRootLog, "fallback/UNKNOWN.log");
                    if ( SetLogFile(default_fallback + euid) ) {
                        log_set = true;
                        new_log_name = default_fallback;
                        to_applog = true;
                        break;
                    }
                    _TRACE_X(4, "Failed to set log file to " <<
                        CFile::NormalizePath(default_fallback));
                }
                const char* log_name = TTeeToStderr::GetDefault() ?
                    kLogName_Tee : kLogName_Stderr;
                if (!log_set  &&  old_log_name != log_name) {
                    NCBI_LSAN_DISABLE_GUARD;
                    SetDiagHandler(new CStreamDiagHandler(&cerr,
                        true, kLogName_Stderr), true);
                    log_set = true;
                    new_log_name = log_name;
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
        s_LogSizeLimit->Set(0); // No log size limit
        SetDiagPostLevel(eDiag_Warning);
        // Lock severity level
        SetApplogSeverityLocked(true);
    }
    else {
        // Disable throttling
        ctx.SetLogRate_Limit(eLogRate_App, CRequestRateControl::kNoLimit);
        ctx.SetLogRate_Limit(eLogRate_Err, CRequestRateControl::kNoLimit);
        ctx.SetLogRate_Limit(eLogRate_Trace, CRequestRateControl::kNoLimit);
    }

    CDiagHandler* new_handler = GetDiagHandler();
    // Flush collected messages to the log if either its name has changed
    // or log file truncation is enabled.
    if (log_set  &&  new_handler  &&  new_handler->GetLogName() == old_log_name) {
        bool is_file = dynamic_cast<CFileHandleDiagHandler*>(new_handler)  ||
            dynamic_cast<CFileDiagHandler*>(new_handler);
        log_set = log_set  &&  is_file  &&  GetLogTruncate();
    }

    if (collect == eDCM_Flush) {
        // Flush and discard
        if ( log_set  &&  new_handler ) {
            ctx.FlushMessages(*new_handler);
        }
        collect = eDCM_Discard;
    }
    else if (collect == eDCM_NoChange) {
        // Flush but don't discard
        if ( log_set  &&  new_handler ) {
            ctx.FlushMessages(*new_handler);
        }
    }
    if (collect == eDCM_Discard) {
        ctx.DiscardMessages();
    }

    // Refresh rate controls
    ctx.ResetLogRates();

    CParamBase::EnableConfigDump(true);
}


CDiagContext::TCount CDiagContext::GetProcessPostNumber(EPostNumberIncrement inc)
{
    static CAtomicCounter s_ProcessPostCount;
    return (TCount)(inc == ePostNumber_Increment ?
        s_ProcessPostCount.Add(1) : s_ProcessPostCount.Get());
}


CDiagContext::TCount CDiagContext::GetThreadPostNumber(EPostNumberIncrement inc)
{
    return CDiagContextThreadData::GetThreadData().GetThreadPostNumber(inc);
}


CDiagContext::TTID CDiagContext::GetTID(void)
{
    return CDiagContextThreadData::GetThreadData().GetTID();
}


bool CDiagContext::IsUsingRootLog(void)
{
    return GetLogFile().substr(0, 5) == kRootLog;
}


CDiagContext& GetDiagContext(void)
{
    // Make the context live longer than other diag safe-statics
    // and +1 longer than TLS safe statics which print app stop
    // from their cleanup.
    static CSafeStatic<CDiagContext> s_DiagContext(
        CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 1));

    return s_DiagContext.Get();
}


///////////////////////////////////////////////////////
//  CNcbiLogFields::

#ifdef NCBI_LOG_FIELDS_CUSTOM
NCBI_PARAM_DECL(string, Log, Ncbi_Log_Fields_Custom);
NCBI_PARAM_DEF_EX(string, Log, Ncbi_Log_Fields_Custom, "",
    eParam_NoThread, NCBI_LOG_FIELDS_CUSTOM);
typedef NCBI_PARAM_TYPE(Log, Ncbi_Log_Fields_Custom) TCustomLogFields;
#endif


CNcbiLogFields::CNcbiLogFields(const string& source)
    : m_Source(source)
{
    const TXChar* env_fields = NcbiSys_getenv(_TX("NCBI_LOG_FIELDS"));
    if ( env_fields ) {
        string fields = _T_CSTRING(env_fields);
        NStr::ToLower(fields);
        NStr::ReplaceInPlace(fields, "_", "-");
        NStr::Split(fields, " ", m_Fields, NStr::fSplit_Tokenize);
    }
#ifdef NCBI_LOG_FIELDS_CUSTOM
    string custom_fields = TCustomLogFields::GetDefault();
    NStr::ToLower(custom_fields);
    NStr::ReplaceInPlace(custom_fields, "_", "-");
    NStr::Split(custom_fields, " ", m_Fields, NStr::fSplit_Tokenize);
#endif
}


CNcbiLogFields::~CNcbiLogFields(void)
{
}


void CNcbiLogFields::x_Match(const string& name, const string& value, CDiagContext_Extra& extra) const
{
    ITERATE(TFields, it, m_Fields) {
        if ( it->empty() ) continue; // Skip empty entries since they match anything.
        if (NStr::MatchesMask(name, *it, NStr::eNocase)) {
            extra.Print((m_Source.empty() ? name : m_Source + "." + name), value);
            break; // Stop if a match is found.
        }
    }
}


///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Error;
#else
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Warning;
#endif

EDiagSevChange CDiagBuffer::sm_PostSeverityChange = eDiagSC_Unknown;
                                                  // to be set on first request

static const TDiagPostFlags s_DefaultPostFlags =
    eDPF_Prefix | eDPF_Severity | eDPF_ErrorID |
    eDPF_ErrCodeMessage | eDPF_ErrCodeExplanation |
    eDPF_ErrCodeUseSeverity;
static TDiagPostFlags s_PostFlags = 0;
static bool s_DiagPostFlagsInitialized = false;

inline
TDiagPostFlags& CDiagBuffer::sx_GetPostFlags(void)
{
    if (!s_DiagPostFlagsInitialized) {
        s_PostFlags = s_DefaultPostFlags;
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


NCBI_PARAM_ENUM_DECL(EDiagSev, DEBUG, Stack_Trace_Level);
NCBI_PARAM_ENUM_ARRAY(EDiagSev, DEBUG, Stack_Trace_Level)
{
    {"Trace", eDiag_Trace},
    { "Info",     eDiag_Info },
    { "Warning",  eDiag_Warning },
    { "Error",    eDiag_Error },
    { "Critical", eDiag_Critical },
    { "Fatal",    eDiag_Fatal }
};
NCBI_PARAM_ENUM_DEF_EX(EDiagSev,
    DEBUG,
    Stack_Trace_Level,
    eDiag_Fatal,
    eParam_NoThread, // No per-thread values
    DEBUG_STACK_TRACE_LEVEL);


void* InitDiagHandler(void)
{
    NCBI_LSAN_DISABLE_GUARD;

    static bool s_DiagInitialized = false;
    if (!s_DiagInitialized) {
        CDiagContext::SetupDiag(eDS_Default, 0, eDCM_Init);
#ifdef NCBI_OS_MSWIN
        // Check environment variable for silent exit, suppress popup messages if enabled.
        // _ASSERT uses NCBI Diag API, and this allow it to work properly.
        const TXChar* value = NcbiSys_getenv(_TX("DIAG_SILENT_ABORT"));
        if (value && (*value == _TX('Y') || *value == _TX('y') || *value == _TX('1'))) {
            SuppressSystemMessageBox(fSuppress_All);
        }
#endif
        s_DiagInitialized = true;
    }
    return 0;
}


// For initialization only
void* s_DiagHandlerInitializer = InitDiagHandler();


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
            const CRequestContext& rctx = ctx.GetRequestContext();
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
                if (mess.m_Severity >= eDiag_Error &&
                    mess.m_Severity != eDiag_Trace &&
                    s_IsDisabledAppLogEvent(rctx, mess.m_Event) &&
                    rctx.x_LogHitIDOnError()) {
                    const CNcbiDiag diag(DIAG_COMPILE_INFO);
                    SDiagMessage phid_msg(eDiag_Error,
                        0, 0,
                        diag.GetFile(),
                        diag.GetLine(),
                        diag.GetPostFlags() | eDPF_AppLog,
                        NULL,
                        0, // Error code
                        0,                                 // Err subcode
                        NULL,
                        diag.GetModule(),
                        diag.GetClass(),
                        diag.GetFunction());
                    phid_msg.m_Event = SDiagMessage::eEvent_Extra;
                    phid_msg.m_ExtraArgs.push_back(SDiagMessage::TExtraArg(
                        g_GetNcbiString(eNcbiStrings_PHID),
                        rctx.GetHitID()));
                    CDiagBuffer::sm_Handler->Post(phid_msg);
                }
                CDiagBuffer::sm_Handler->Post(mess);
                if (auto span = rctx.GetTracerSpan()) {
                    span->PostEvent(mess);
                }
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

    EDiagSev sev = diag.GetSeverity();
    bool is_console = (diag.GetPostFlags() & eDPF_IsConsole) > 0;
    if (!is_console  &&  SeverityDisabled(sev)) {
        return false;
    }

    if (m_Diag != &diag) {
        if ( !IsOssEmpty(*m_Stream) ) {
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


struct SThreadsInSTBuild
{
    static bool Check();
    static SDiagMessage Report(EDiagSev& sev);

private:
    static atomic<thread::id> sm_ThreadID;
    static atomic<bool> sm_Reported;
};


atomic<thread::id> SThreadsInSTBuild::sm_ThreadID;
atomic<bool> SThreadsInSTBuild::sm_Reported;


bool SThreadsInSTBuild::Check()
{
#ifndef NCBI_THREADS
    thread::id stored_thread_id;
    thread::id this_thread_id = this_thread::get_id();

    // If this thread has just initialized sm_ThreadID
    if (sm_ThreadID.compare_exchange_strong(stored_thread_id, this_thread_id)) return false;

    // If sm_ThreadID contains same thread ID
    if (stored_thread_id == this_thread_id) return false;

    bool reported = false;

    // Whether to report this (or it has already been reported)
    if (sm_Reported.compare_exchange_strong(reported, true)) return true;
#endif

    return false;
}


SDiagMessage SThreadsInSTBuild::Report(EDiagSev& sev)
{
#ifdef _DEBUG
    sev = eDiag_Fatal;
#else
    sev = eDiag_Critical;
#endif

    const auto msg = "Detected different threads using C++ Toolkit built in single thread mode."sv;
    const CNcbiDiag diag(DIAG_COMPILE_INFO);
    return SDiagMessage(
            sev,
            msg.data(),
            msg.length(),
            diag.GetFile(),
            diag.GetLine(),
            diag.GetPostFlags(),
            nullptr,
            0,
            0,
            nullptr,
            diag.GetModule(),
            diag.GetClass(),
            diag.GetFunction());
}


void CDiagBuffer::Flush(void)
{
    if ( m_InUse || !m_Diag ) {
        if ( !m_InUse  &&  m_Stream  &&  !IsOssEmpty(*m_Stream) ) {
            string message = CNcbiOstrstreamToString(*m_Stream);
            // Can not use Reset() without CNcbiDiag.
            m_Stream->rdbuf()->PUBSEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
            // _TRACE("Discarding junk data from CDiagBuffer: " << message);
        }
        return;
    }
    CRecursionGuard guard(m_InUse);

    EDiagSev sev = m_Diag->GetSeverity();
    bool is_console = (m_Diag->GetPostFlags() & eDPF_IsConsole) != 0;
    bool is_disabled = SeverityDisabled(sev);

    // Do nothing if diag severity is lower than allowed.
    if (!is_console  &&  is_disabled) {
        return;
    }

    string message = CNcbiOstrstreamToString(*m_Stream);

    TDiagPostFlags flags = m_Diag->GetPostFlags();
    if (sev == eDiag_Trace) {
        flags |= sm_TraceFlags;
    } else if (sev == eDiag_Fatal) {
        // normally only happens once, so might as well pull everything
        // in for the record...
        flags |= sm_TraceFlags | eDPF_Trace;
    }

    if ( m_Diag->CheckFilters() ) {
        SDiagMessage mess(sev, message.data(), message.size(),
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

    if (SThreadsInSTBuild::Check()) {
        // Change severity and print error message
        auto mess = SThreadsInSTBuild::Report(sev);
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
#  ifdef NCBI_SHUN_OSTRSTREAM
    // m_Stream->rdbuf()->PUBSEEKOFF(0, IOS_BASE::beg);
    m_Stream->str(kEmptyStr);
#  endif
#endif

    Reset(*m_Diag);

    if (sev >= sm_DieSeverity  &&  sev != eDiag_Trace  &&  !sm_IgnoreToDie) {
        m_Diag = 0;

#ifdef NCBI_COMPILER_MSVC
        if (NCBI_PARAM_TYPE(Diag, Assert_On_Abort)::GetDefault() ) {
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
    if ( !diag.GetOmitStackTrace() ) {
        static CSafeStatic<NCBI_PARAM_TYPE(DEBUG, Stack_Trace_Level)> s_StackTraceLevel;
        EDiagSev stack_sev = s_StackTraceLevel->Get();
        mess.m_PrintStackTrace = (sev == stack_sev) || (sev > stack_sev && sev != eDiag_Trace);
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
      m_PrintStackTrace(false),
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
      m_PrintStackTrace(false),
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
      m_PrintStackTrace(false),
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
    virtual string Decode(const CTempString src, EStringType stype) const;
};


string CExtraDecoder::Decode(const CTempString src, EStringType stype) const
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
            str[dst] = char((n1 << 4) | n2);
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
    catch (const CStringException&) {
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
        catch (const CStringException&) {
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
            catch (const CTimeException&) {
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
            // Support both old (LOG_POST -> message) and new (NOTE_POST -> note) styles.
            size_t sev_pos = NPOS;
            if (tmp.length() == 10  &&  tmp.find("Message[") == 0) {
                sev_pos = 8;
            }
            else if (tmp.length() == 7  &&  tmp.find("Note[") == 0) {
                sev_pos = 5;
            }

            if (sev_pos != NPOS) {
                // Get the real severity
                switch ( tmp[sev_pos] ) {
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
                m_Flags |= eDPF_IsNote;
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
                    catch (const CStringException&) {
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
    catch (const CException&) {
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
    unique_ptr<SDiagMessage> msg;
    unique_ptr<SDiagMessage> last_msg;
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
    stringstream ostr;
    Write(ostr, flags);
    str = ostr.str();
}


CNcbiOstream& SDiagMessage::Write(CNcbiOstream&   os,
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

    virtual string Encode(const CTempString src, EStringType stype) const;

private:
    bool m_AllowBadNames;
};


string CExtraEncoder::Encode(const CTempString src, EStringType stype) const
{
    static const char* s_BadSymbolPrefix = "[INVALID_APPLOG_SYMBOL:";
    static const char* s_BadSymbolSuffix = "]";
    static const size_t s_BadSymbolPrefixLen = strlen(s_BadSymbolPrefix);
    static const CTempString s_EncodedSpace = "%20";

    vector<CTempString> parts;
    parts.resize(src.length() + 2); // adjust for possible invalid symbol message
    size_t part_idx = 0;
    const char* src_data = src.data();
    const bool warn_bad_name = (stype == eName && !m_AllowBadNames);
    size_t good_start = 0;
    size_t total_len = 0;
    for (size_t pos = 0; pos < src.size(); ++pos) {
        char c = src[pos];
        const char* enc = s_ExtraEncodeChars[(unsigned char)c];
        if (enc[0] == c && enc[1] == 0) continue;

        // Save good chars, if any.
        if (good_start < pos) {
            CTempString& good_part = parts[part_idx++];
            good_part.assign(src_data + good_start, pos - good_start);
            total_len += good_part.size();
        }

        if (warn_bad_name) {
            CTempString& warn_part = parts[part_idx++];
            warn_part.assign(s_BadSymbolPrefix, s_BadSymbolPrefixLen);
            total_len += s_BadSymbolPrefixLen;
        }
        CTempString& enc_part = parts[part_idx++];
        enc_part.assign((c == ' ' && warn_bad_name) ? s_EncodedSpace : enc);
        total_len += enc_part.size();
        if (warn_bad_name) {
            CTempString& warn2_part = parts[part_idx++];
            warn2_part.assign(s_BadSymbolSuffix, 1);
            total_len++;
        }
        good_start = pos + 1;

        if (part_idx + 3 >= parts.size()) parts.resize(parts.size() * 2);
    }

    if (good_start < src.size()) {
        CTempString& good_part = parts[part_idx++];
        good_part.assign(src_data + good_start, src.size() - good_start);
        total_len += good_part.size();
    }

    char* buf = new char[total_len];
    char* pos = buf;
    for (size_t i = 0; i < part_idx; ++i) {
        const CTempString& part = parts[i];
        strncpy(pos, part.data(), part.size());
        pos += part.size();
    }
    string ret(buf, total_len);
    delete[] buf;
    return ret;
}


string SDiagMessage::FormatExtraMessage(void) const
{
    return CStringPairs<TExtraArgs>::Merge(m_ExtraArgs,
        "&", "=", new CExtraEncoder(m_AllowBadExtraNames));
}


// Merge lines in-place:
// \n -> \v
// \v -> 0xFF + \v
// 0xFF -> 0xFF 0xFF
void SDiagMessage::s_EscapeNewlines(string& buf)
{
    size_t p = buf.find_first_of("\n\v\377");
    if (p == NPOS) return;
    for (; p < buf.size(); p++) {
        switch (buf[p]) {
        case '\377':
        case '\v':
            buf.insert(p, 1, '\377');
            p++;
            break;
        case '\n':
            buf[p] = '\v';
            break;
        }
    }
}


void SDiagMessage::s_UnescapeNewlines(string& buf)
{
    if (buf.find_first_of("\v\377") == NPOS) return;
    size_t src = 0, dst = 0;
    for (; src < buf.size(); src++, dst++) {
        switch (buf[src]) {
        case '\377':
            if (src < buf.size() - 1  &&
                (buf[src + 1] == '\377' || buf[src + 1] == '\v')) {
                    src++;   // skip escape char
            }
            break;
        case '\v':
            // non-escaped VT
            buf[dst] = '\n';
            continue;
        }
        if (dst != src) {
            buf[dst] = buf[src];
        }
    }
    buf.resize(dst);
}


enum EDiagMergeLines {
    eDiagMergeLines_Default, // Do not force line merging
    eDiagMergeLines_Off,     // Force line breaks
    eDiagMergeLines_On       // Escape line breaks
};


NCBI_PARAM_ENUM_DECL(EDiagMergeLines, Diag, Merge_Lines);
NCBI_PARAM_ENUM_ARRAY(EDiagMergeLines, Diag, Merge_Lines)
{
    {"Default", eDiagMergeLines_Default},
    {"Off", eDiagMergeLines_Off},
    {"On", eDiagMergeLines_On}
};

NCBI_PARAM_ENUM_DEF_EX(EDiagMergeLines, Diag, Merge_Lines,
                       eDiagMergeLines_Default,
                       eParam_NoThread, DIAG_MERGE_LINES);


// Formatted output of stack trace
void s_FormatStackTrace(CNcbiOstream& os, const CStackTrace& trace)
{
    string old_prefix = trace.GetPrefix();
    trace.SetPrefix("      ");
    os << "\n     Stack trace:\n" << trace;
    trace.SetPrefix(old_prefix);
}


CNcbiOstream& SDiagMessage::x_OldWrite(CNcbiOstream& out_str,
                                       TDiagWriteFlags flags) const
{
    // Temp stream - the result will be passed to line merging.
    // Error text, module, prefix etc. can have linebreaks which need to
    // be escaped.
    stringstream os;

    // Date & time
    if (IsSetDiagPostFlag(eDPF_DateTime, m_Flags)) {
        os << CFastLocalTime().GetLocalTime().AsString("M/D/y h:m:s ");
    }
#ifdef NCBI_THREADS
    if (IsSetDiagPostFlag(eDPF_TID, m_Flags)) {
        os << 'T' << CThread::GetSelf() << ' ';
    }
#endif
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
        string sev = CNcbiDiag::SeverityName(m_Severity);
        if ( IsSetDiagPostFlag(eDPF_IsNote, m_Flags) ) {
            os << "Note[" << sev[0] << "]";
        }
        else {
            os << sev;
        }
        os << ": ";
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

    if ( m_PrintStackTrace ) {
        s_FormatStackTrace(os, CStackTrace());
    }

    string buf = os.str();
    bool merge_lines = IsSetDiagPostFlag(eDPF_MergeLines, m_Flags);
    static CSafeStatic<NCBI_PARAM_TYPE(Diag, Merge_Lines)> s_DiagMergeLines;
    switch (s_DiagMergeLines->Get()) {
    case eDiagMergeLines_On:
        merge_lines = true;
        break;
    case eDiagMergeLines_Off:
        merge_lines = false;
        break;
    default:
        break;
    }
    if (merge_lines) {
        NStr::ReplaceInPlace(buf, "\n", ";");
    }
    out_str << buf;

    // Endl
    if ((flags & fNoEndl) == 0) {
        out_str << NcbiEndl;
    }

    return out_str;
}


CNcbiOstream& SDiagMessage::x_NewWrite(CNcbiOstream& out_str,
                                       TDiagWriteFlags flags) const
{
    // Temp stream - the result will be passed to line merging.
    // Error text, module, prefix etc. can have linebreaks which need to
    // be escaped.
    stringstream os;

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
        if ( IsSetDiagPostFlag(eDPF_IsNote, m_Flags) ) {
            os << "Note[" << sev[0] << "]:";
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
            os << '\n' << description.m_Message << ' ';
        if (IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags) &&
            !description.m_Explanation.empty())
            os << '\n' << description.m_Explanation;
    }

    if ( m_PrintStackTrace ) {
        s_FormatStackTrace(os, CStackTrace());
    }

    string buf = os.str();
    // Line merging in new (applog) format is unconditional.
    s_EscapeNewlines(buf);
    auto max_len = s_MaxLineLength->Get();
    if (max_len > 0 && buf.size() > max_len) {
        buf.resize(max_len);
    }
    out_str << buf;

    // Endl
    if ((flags & fNoEndl) == 0) {
        // In applog format always use \n only. The stream must be in binary mode.
        out_str << '\n';
    }
    return out_str;
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


string SDiagMessage::GetClient(void) const
{
    return m_Data ? m_Data->m_Client
        : CDiagContext::GetRequestContext().GetClientIP();
}


string SDiagMessage::GetSession(void) const
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
}


static void s_UnsetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CDiagLock lock(CDiagLock::eWrite);
    flags &= ~flag;
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


extern EDiagSev GetDiagPostLevel(void)
{
    return CDiagBuffer::sm_PostSeverity;
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


extern bool GetDiagTrace(void)
{
    return CDiagBuffer::GetTraceEnabled();
}


CTeeDiagHandler::CTeeDiagHandler(CDiagHandler* orig, bool own_orig)
    : m_MinSev(NCBI_PARAM_TYPE(Diag, Tee_Min_Severity)::GetDefault()),
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

    stringstream str_os;
    mess.x_OldWrite(str_os);
    CDiagLock lock(CDiagLock::ePost);
    string str = str_os.str();
    cerr.write(str.data(), str.size());
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
    // Do not delete old handler if it's reinstalled (possibly just to change
    // the ownership).
    if (CDiagBuffer::sm_CanDeleteHandler  &&  CDiagBuffer::sm_Handler != handler)
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

extern CDiagHandler* GetDiagHandler(bool take_ownership,
                                    bool* current_ownership)
{
    CDiagLock lock(CDiagLock::eRead);
    if ( current_ownership ) {
        *current_ownership = CDiagBuffer::sm_CanDeleteHandler;
    }
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
    stringstream str_os;
    str_os << mess;
    string str = str_os.str();
    cerr.write(str.data(), str.size());
    cerr << NcbiFlush;
}


string CDiagHandler::GetLogName(void)
{
    string name = typeid(*this).name();
    return name.empty() ? kLogName_Unknown
        : string(kLogName_Unknown) + "(" + name + ")";
}


bool CDiagHandler::AllowAsyncWrite(const SDiagMessage& /*msg*/) const
{
    return false;
}


string CDiagHandler::ComposeMessage(const SDiagMessage&, EDiagFileType*) const
{
    _ASSERT(0);
    return kEmptyStr;
}


void CDiagHandler::WriteMessage(const char*, size_t, EDiagFileType)
{
    _ASSERT(0);
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
    if (m_Stream->bad()) return;
    m_Stream->clear();
    stringstream str_os;
    str_os << mess;
    string str = str_os.str();
    m_Stream->write(str.data(), str.size());
    if (!m_Stream->good()) return;
    if (m_QuickFlush) {
        *m_Stream << NcbiFlush;
    }
}


CDiagFileHandleHolder::CDiagFileHandleHolder(const string& fname,
                                             CDiagHandler::TReopenFlags flags)
    : m_Handle(-1)
{
#if defined(NCBI_OS_MSWIN)
    int mode = O_WRONLY | O_APPEND | O_CREAT | O_BINARY | O_NOINHERIT;
#else
    int mode = O_WRONLY | O_APPEND | O_CREAT;
#endif

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
#if defined(NCBI_OS_UNIX)
    fcntl(m_Handle, F_SETFD, fcntl(m_Handle, F_GETFD, 0) | FD_CLOEXEC);
#endif
}

CDiagFileHandleHolder::~CDiagFileHandleHolder(void)
{
    if (m_Handle >= 0) {
        NcbiSys_close(m_Handle);
    }
}


// CFileDiagHandler

CFileHandleDiagHandler::CFileHandleDiagHandler(const string& fname, EDiagFileType file_type)
    : m_FileType(file_type),
      m_HavePosts(false),
      m_LowDiskSpace(false),
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
    if (m_FileType == eDiagFile_Perf && !m_HavePosts) {
        s_ReopenEntered->Add(-1);
        return;
    }
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
        long pos = NcbiSys_lseek(m_Handle->GetHandle(), 0, SEEK_CUR);
        long limit = s_LogSizeLimit->Get();
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
        catch (const CException&) {
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
        // Flush the collected messages, if any, once the handle if available.
        // If the process was forked, ignore messages collected by the parent.
        CDiagContext::UpdatePID();
        SDiagMessage::TPID pid = CDiagContext::GetPID();
        ITERATE(TMessages, it, *m_Messages) {
            if (it->m_PID != pid) {
                continue;
            }
            string str = ComposeMessage(*it, 0);
            if (NcbiSys_write(new_handle->GetHandle(), str.data(), (unsigned)str.size()))
                {/*dummy*/}
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
            m_HavePosts = true;
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
        string str = ComposeMessage(mess, 0);
        if (NcbiSys_write(handle->GetHandle(), str.data(), (unsigned)str.size()))
            {/*dummy*/}

        handle->RemoveReference();
    }
}


bool CFileHandleDiagHandler::AllowAsyncWrite(const SDiagMessage& /*msg*/) const
{
    return true;
}


string CFileHandleDiagHandler::ComposeMessage(const SDiagMessage& msg,
                                              EDiagFileType*) const
{
    stringstream str_os;
    str_os << msg;
    return str_os.str();
}


void CFileHandleDiagHandler::WriteMessage(const char*   buf,
                                          size_t        len,
                                          EDiagFileType /*file_type*/)
{
    // Period is longer than for CFileDiagHandler to prevent double-reopening
    // In async mode only one thread is writing messages and there's no need
    // to use locks, but it's still necessary to check for nested reopening.
    if (!m_ReopenTimer->IsRunning()  ||
        m_ReopenTimer->Elapsed() >= kLogReopenDelay + 5)
    {
        if (s_ReopenEntered->Add(1) == 1) {
            Reopen(fDefault);
        }
        s_ReopenEntered->Add(-1);
    }

    if (NcbiSys_write(m_Handle->GetHandle(), buf, (unsigned)len))
        {/*dummy*/}

    // Skip collecting m_Messages - we don't have the original SDiagMessage
    // here and can not store it. If for some reason Reopen() fails in async
    // mode, some messages can be lost.
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
        ||  name == "/dev/null"
        ||  name == "/dev/stdout"
        ||  name == "/dev/stderr";
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
    }
}


static bool
s_CreateHandler(const string& fname,
                unique_ptr<CStreamDiagHandler_Base>& handler,
                EDiagFileType file_type)
{
    if ( fname.empty()  ||  fname == "/dev/null") {
        handler.reset();
        return true;
    }
    if (fname == "-") {
        handler.reset(new CStreamDiagHandler(&NcbiCerr, true, kLogName_Stderr));
        return true;
    }
    unique_ptr<CFileHandleDiagHandler> fh(new CFileHandleDiagHandler(fname, file_type));
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
    unique_ptr<CStreamDiagHandler_Base> err_handler, log_handler,
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

            if ( s_SplitLogFile ) {
                if (!s_CreateHandler(err_name, err_handler, eDiagFile_Err))
                    return false;
                if (!s_CreateHandler(log_name, log_handler, eDiagFile_Log))
                    return false;
                if (!s_CreateHandler(trace_name, trace_handler, eDiagFile_Trace))
                    return false;
                if (!s_CreateHandler(perf_name, perf_handler, eDiagFile_Perf))
                    return false;

                x_SetHandler(&m_Err, &m_OwnErr, err_handler.release(), true);
                x_SetHandler(&m_Log, &m_OwnLog, log_handler.release(), true);
                x_SetHandler(&m_Trace, &m_OwnTrace, trace_handler.release(), true);
                x_SetHandler(&m_Perf, &m_OwnPerf, perf_handler.release(), true);
            }
            else {
                if (!s_CreateHandler(file_name, err_handler, eDiagFile_All))
                    return false;
                if (!s_CreateHandler(perf_name, perf_handler, eDiagFile_Perf))
                    return false;

                x_SetHandler(&m_Err, &m_OwnErr, err_handler.get(), true);
                x_SetHandler(&m_Log, &m_OwnLog, err_handler.get(), true);
                x_SetHandler(&m_Trace, &m_OwnTrace, err_handler.release(), true);
                x_SetHandler(&m_Perf, &m_OwnPerf, perf_handler.release(), true);
            }

            m_ReopenTimer->Restart();
            break;
        }
    case eDiagFile_Err:
        if (!s_CreateHandler(file_name, err_handler, eDiagFile_Err))
            return false;
        x_SetHandler(&m_Err, &m_OwnErr, err_handler.release(), true);
        break;
    case eDiagFile_Log:
        if (!s_CreateHandler(file_name, log_handler, eDiagFile_Log))
            return false;
        x_SetHandler(&m_Log, &m_OwnLog, log_handler.release(), true);
        break;
    case eDiagFile_Trace:
        if (!s_CreateHandler(file_name, trace_handler, eDiagFile_Trace))
            return false;
        x_SetHandler(&m_Trace, &m_OwnTrace, trace_handler.release(), true);
        break;
    case eDiagFile_Perf:
        if (!s_CreateHandler(file_name, perf_handler, eDiagFile_Perf))
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
        break;
    case eDiagFile_Log:
        handler = m_Log;
        break;
    case eDiagFile_Trace:
        handler = m_Trace;
        break;
    case eDiagFile_Perf:
        handler = m_Perf;
        break;
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
    if ( m_Log  &&  m_Log != m_Err ) {
        m_Log->Reopen(flags);
    }
    if ( m_Trace  &&  m_Trace != m_Log  &&  m_Trace != m_Err ) {
        m_Trace->Reopen(flags);
    }
    if ( m_Perf ) {
        m_Perf->Reopen(flags);
    }
    m_ReopenTimer->Restart();

    s_ReopenEntered->Add(-1);
}


EDiagFileType CFileDiagHandler::x_GetDiagFileType(const SDiagMessage& msg) const
{
    if ( IsSetDiagPostFlag(eDPF_AppLog, msg.m_Flags) ) {
        return msg.m_Event == SDiagMessage::eEvent_PerfLog
            ? eDiagFile_Perf : eDiagFile_Log;
    }
    else {
        switch ( msg.m_Severity ) {
        case eDiag_Info:
        case eDiag_Trace:
            return eDiagFile_Trace;
            break;
        default:
            return eDiagFile_Err;
        }
    }
    // Never gets here anyway.
    return eDiagFile_All;
}


CStreamDiagHandler_Base* CFileDiagHandler::x_GetHandler(EDiagFileType file_type) const
{
    switch ( file_type ) {
    case eDiagFile_Err: return m_Err;
    case eDiagFile_Log: return m_Log;
    case eDiagFile_Trace: return m_Trace;
    case eDiagFile_Perf: return m_Perf;
    default: return 0;
    }
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
    CStreamDiagHandler_Base* handler = x_GetHandler(x_GetDiagFileType(mess));
    if (handler)
        handler->Post(mess);
}


bool CFileDiagHandler::AllowAsyncWrite(const SDiagMessage& msg) const
{
    CStreamDiagHandler_Base* handler = x_GetHandler(x_GetDiagFileType(msg));
    return handler  &&  handler->AllowAsyncWrite(msg);
}


string CFileDiagHandler::ComposeMessage(const SDiagMessage& msg,
                                        EDiagFileType*      file_type) const
{
    EDiagFileType ft = x_GetDiagFileType(msg);
    if ( file_type ) {
        *file_type = ft;
    }
    CStreamDiagHandler_Base* handler = x_GetHandler(ft);
    return handler ? handler->ComposeMessage(msg, file_type) : kEmptyStr;
}


void CFileDiagHandler::WriteMessage(const char*   buf,
                                    size_t        len,
                                    EDiagFileType file_type)
{
    // In async mode only one thread is writing messages and there's no need
    // to use locks, but it's still necessary to check for nested reopening.
    if (!m_ReopenTimer->IsRunning()  ||
        m_ReopenTimer->Elapsed() >= kLogReopenDelay)
    {
        if (s_ReopenEntered->Add(1) == 1) {
            Reopen(fDefault);
        }
        s_ReopenEntered->Add(-1);
    }

    CStreamDiagHandler_Base* handler = x_GetHandler(file_type);
    if ( handler ) {
        handler->WriteMessage(buf, len, file_type);
    }
}


struct SAsyncDiagMessage
{
    SAsyncDiagMessage(void)
        : m_Message(nullptr), m_Composed(nullptr), m_FileType(eDiagFile_All) {}

    SDiagMessage* m_Message;
    string*       m_Composed;
    EDiagFileType m_FileType;
};


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
    deque<SAsyncDiagMessage> m_MsgQueue;
    string m_ThreadSuffix;
};


/// Maximum number of messages that allowed to be in the queue for
/// asynchronous processing.
NCBI_PARAM_DECL(Uint4, Diag, Max_Async_Queue_Size);
NCBI_PARAM_DEF_EX(Uint4, Diag, Max_Async_Queue_Size, 10000, eParam_NoThread,
                  DIAG_MAX_ASYNC_QUEUE_SIZE);


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
    catch (const CThreadException&) {
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
    SAsyncDiagMessage async;
    if (thr->m_SubHandler->AllowAsyncWrite(mess)) {
        async.m_Composed = new string(thr->m_SubHandler->
            ComposeMessage(mess, &async.m_FileType));
    }
    else {
        async.m_Message = new SDiagMessage(mess);
    }

    static CSafeStatic<NCBI_PARAM_TYPE(Diag, Max_Async_Queue_Size)> s_MaxAsyncQueueSizeParam;
    if (mess.m_Severity < GetDiagDieLevel()) {
        CFastMutexGuard guard(thr->m_QueueLock);
        while (Uint4(thr->m_MsgsInQueue.Get()) >= s_MaxAsyncQueueSizeParam->Get())
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
        thr->m_MsgQueue.push_back(async);
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
        thr->m_SubHandler->Post(mess);
        if (async.m_Composed) delete async.m_Composed;
        if (async.m_Message) delete async.m_Message;
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


NCBI_PARAM_DECL(size_t, Diag, Async_Buffer_Size);
NCBI_PARAM_DEF_EX(size_t, Diag, Async_Buffer_Size, 32768,
    eParam_NoThread, DIAG_ASYNC_BUFFER_SIZE);

NCBI_PARAM_DECL(size_t, Diag, Async_Buffer_Max_Lines);
NCBI_PARAM_DEF_EX(size_t, Diag, Async_Buffer_Max_Lines, 100,
    eParam_NoThread, DIAG_ASYNC_BUFFER_MAX_LINES);


struct SMessageBuffer
{
    char* data;
    size_t size;
    size_t pos;
    size_t lines;
    size_t max_lines;

    SMessageBuffer(void)
        : data(0), size(0), pos(0), lines(0)
    {
        size = NCBI_PARAM_TYPE(Diag, Async_Buffer_Size)::GetDefault();
        if ( size ) {
            data = new char[size];
        }
        max_lines = NCBI_PARAM_TYPE(Diag, Async_Buffer_Max_Lines)::GetDefault();
    }

    ~SMessageBuffer(void)
    {
        if ( data ) {
            delete[](data);
        }
    }

    bool IsEmpty(void)
    {
        return pos != 0;
    }

    void Clear(void)
    {
        pos = 0;
        lines = 0;
    }

    bool Append(const string& str)
    {
        if (!size  ||  pos + str.size() >= size  ||  lines >= max_lines) {
            return false;
        }
        memcpy(&data[pos], str.data(), str.size());
        pos += str.size();
        lines++;
        return true;
    }
};


/// Number of messages processed as a single batch by the asynchronous
/// handler.
NCBI_PARAM_DECL(int, Diag, Async_Batch_Size);
NCBI_PARAM_DEF_EX(int, Diag, Async_Batch_Size, 10, eParam_NoThread,
                  DIAG_ASYNC_BATCH_SIZE);


void*
CAsyncDiagThread::Main(void)
{
    if (!m_ThreadSuffix.empty()) {
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        string thr_name = app ? app->GetProgramDisplayName() : "";
        thr_name += m_ThreadSuffix;
        SetCurrentThreadName(thr_name);
    }

    const int batch_size = NCBI_PARAM_TYPE(Diag, Async_Batch_Size)::GetDefault();

    const size_t buf_count = size_t(eDiagFile_All) + 1;
    SMessageBuffer* buffers[buf_count];
    for (size_t i = 0; i < buf_count; ++i) {
        buffers[i] = 0;
    }

    deque<SAsyncDiagMessage> save_msgs;
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
        int queue_counter = 0;
        while (!save_msgs.empty()) {
            SAsyncDiagMessage msg = save_msgs.front();
            save_msgs.pop_front();
            if ( msg.m_Composed ) {
                SMessageBuffer* buf = buffers[msg.m_FileType];
                if ( !buf ) {
                    buf = new SMessageBuffer;
                    buffers[msg.m_FileType] = buf;
                }
                if ( !buf->size ) {
                    // Do not use buffering.
                    m_SubHandler->WriteMessage(msg.m_Composed->data(),
                        msg.m_Composed->size(), msg.m_FileType);
                }
                else if ( !buf->Append(*msg.m_Composed) ) {
                    // Not enough space in the buffer or no waiters,
                    // try to flush if not empty.
                    if ( !buf->IsEmpty() ) {
                        m_SubHandler->WriteMessage(buf->data, buf->pos, msg.m_FileType);
                        buf->Clear();
                    }
                    if ( !buf->Append(*msg.m_Composed) ) {
                        // The message is too long to fit in the buffer.
                        m_SubHandler->WriteMessage(msg.m_Composed->data(),
                            msg.m_Composed->size(), msg.m_FileType);
                    }
                }
                delete msg.m_Composed;
            }
            else {
                _ASSERT(msg.m_Message);
                m_SubHandler->Post(*msg.m_Message);
                delete msg.m_Message;
            }
            if (++queue_counter >= batch_size  ||  save_msgs.empty()) {
                m_MsgsInQueue.Add(-queue_counter);
                queue_counter = 0;
                if (m_CntWaiters != 0) {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
                    m_DequeueCond.SignalSome();
#else
                    m_DequeueSem.Post();
#endif
                }
            }
        }
        // Flush all buffers when the queue is empty and there are no waiters.
        if (m_CntWaiters == 0) {
            for (size_t i = 0; i < buf_count; ++i) {
                if ( !buffers[i] ) {
                    continue;
                }
                if ( !buffers[i]->IsEmpty() ) {
                    m_SubHandler->WriteMessage(buffers[i]->data,
                        buffers[i]->pos, EDiagFileType(i));
                    buffers[i]->Clear();
                }
            }
        }
    }
    if (m_MsgQueue.size() != 0) {
        save_msgs.swap(m_MsgQueue);
        goto drain_messages;
    }

    for (size_t i = 0; i < buf_count; ++i) {
        if ( !buffers[i] ) {
            continue;
        }
        if ( !buffers[i]->IsEmpty() ) {
            m_SubHandler->WriteMessage(buffers[i]->data,
                buffers[i]->pos, EDiagFileType(i));
        }
        delete buffers[i];
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
    catch (const CException& ex) {
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
    if ( !s_SplitLogFile ) {
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
            unique_ptr<CFileDiagHandler> fhandler(new CFileDiagHandler());
            if ( !fhandler->SetLogFile(file_name, eDiagFile_All, quick_flush) ) {
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
            bool old_ownership = false;
            CStreamDiagHandler_Base* sub_handler =
                dynamic_cast<CStreamDiagHandler_Base*>(GetDiagHandler(false, &old_ownership));
            if ( !sub_handler ) {
                old_ownership = false;
            }
            // Install new handler, try to re-use the old one
            unique_ptr<CFileDiagHandler> fhandler(new CFileDiagHandler());
            if ( sub_handler  &&  file_type != eDiagFile_All) {
                // If we are going to set all handlers, no need to save the old one.
                if ( old_ownership ) {
                    GetDiagHandler(true); // Take ownership!
                }
                // Set all three handlers to the old one.
                fhandler->SetSubHandler(sub_handler, eDiagFile_All, old_ownership);
            }
            if ( fhandler->SetLogFile(file_name, file_type, quick_flush) ) {
                // This will delete the old handler in case of eDiagFile_All.
                // Otherwise the new handler will remember the ownership.
                SetDiagHandler(fhandler.release());
                return true;
            }
            else {
                // Restore the old handler's ownership if necessary.
                if ( old_ownership ) {
                    SetDiagHandler(sub_handler, true);
                }
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


extern string GetDiagFilter(EDiagFilter what)
{
    CDiagLock lock(CDiagLock::eWrite);
    if (what == eDiagFilter_Trace)
        return s_TraceFilter->GetFilterStr();

    if (what == eDiagFilter_Post)
        return s_PostFilter->GetFilterStr();

    return kEmptyStr;
}


extern void AppendDiagFilter(EDiagFilter what, const char* filter_str)
{
    CDiagLock lock(CDiagLock::eWrite);
    if (what == eDiagFilter_Trace || what == eDiagFilter_All)
        s_TraceFilter->Append(filter_str);

    if (what == eDiagFilter_Post || what == eDiagFilter_All)
        s_PostFilter->Append(filter_str);
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()),
      m_PostFlags(ForceImportantFlags(post_flags)),
      m_OmitStackTrace(false)
{
}


CNcbiDiag::CNcbiDiag(const CDiagCompileInfo &info,
                     EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()),
      m_PostFlags(ForceImportantFlags(post_flags)),
      m_OmitStackTrace(false),
      m_CompileInfo(info)
{
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


bool CNcbiDiag::CheckFilters(const CException* ex) const
{
    EDiagSev current_sev = GetSeverity();
    if (current_sev == eDiag_Fatal) {
        return true;
    }
    CDiagLock lock(CDiagLock::eRead);
    if (GetSeverity() == eDiag_Trace) {
        // check for trace filter
        return  s_TraceFilter->Check(*this, ex) != eDiagFilter_Reject;
    }
    // check for post filter
    return  s_PostFilter->Check(*this, ex) != eDiagFilter_Reject;
}


const CNcbiDiag& CNcbiDiag::Put(const CStackTrace*,
                                const CStackTrace& stacktrace) const
{
    if ( !stacktrace.Empty() ) {
        stacktrace.SetPrefix("      ");
        stringstream os;
        s_FormatStackTrace(os, stacktrace);
        *this << os.str();
    }
    return *this;
}

static string
s_GetExceptionText(const CException* pex)
{
    string text(pex->GetMsg());
    stringstream os;
    pex->ReportExtra(os);
    string s = os.str();
    if ( !s.empty() ) {
        text += " (";
        text += s;
        text += ')';
    }
    return text;
}

const CNcbiDiag& CNcbiDiag::x_Put(const CException& ex) const
{
    if (m_Buffer.SeverityDisabled(GetSeverity()) || !CheckFilters(&ex)) {
        return *this;
    }
    CDiagContextThreadData& thr_data = CDiagContextThreadData::GetThreadData();
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
    if ( !IsOssEmpty(*m_Buffer.m_Stream) ) {
        *this << "(" << main_pex->GetType() << "::"
                     << main_pex->GetErrCodeString() << ") "
              << s_GetExceptionText(main_pex);
    }
    else {
        *this << main_pex->GetType() << "::"
            << main_pex->GetErrCodeString() << " "
            << s_GetExceptionText(main_pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        string text(s_GetExceptionText(pex));
        const CStackTrace* stacktrace = pex->GetStackTrace();
        if ( stacktrace ) {
            stringstream os;
            s_FormatStackTrace(os, *stacktrace);
            m_OmitStackTrace = true;
            text += os.str();
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
    // DiagFatal is non-returnable, so force aborting even if the above
    // call has returned.
    Abort();
}

void CNcbiDiag::DiagTrouble(const CDiagCompileInfo& info,
                            const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal) << message << Endm;
}

void CNcbiDiag::DiagAssert(const CDiagCompileInfo& info,
                           const char* expression,
                           const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal, eDPF_Trace) <<
        "Assertion failed: (" <<
        (expression ? expression : "") << ") " <<
        (message ? message : "") << Endm;
    Abort();
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
    NCBI_LSAN_DISABLE_GUARD; // new CCompatStreamDiagHandler
    SetDiagHandler(
        new CCompatStreamDiagHandler(os, quick_flush, cleanup, cleanup_data, str_name)
    );
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
//  Abort handler


static FAbortHandler s_UserAbortHandler = 0;

extern void SetAbortHandler(FAbortHandler func)
{
    s_UserAbortHandler = func;
}


extern void Abort(void)
{
    // If there is a user abort handler then call it
    if ( s_UserAbortHandler )
        s_UserAbortHandler();

    // If there's no handler or application is still running

    // Check environment variable for silent exit
    const TXChar* value = NcbiSys_getenv(_TX("DIAG_SILENT_ABORT"));
    if (value  &&  (*value == _TX('Y')  ||  *value == _TX('y')  ||  *value == _TX('1'))) {
        ::fflush(0);
        ::_exit(255);
    }
    else if (value  &&  (*value == _TX('N')  ||  *value == _TX('n') || *value == _TX('0'))) {
        ::abort();
    }
    else {
#if defined(NDEBUG)
        ::fflush(0);
        ::_exit(255);
#else
        ::abort();
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
        NStr::Split(str, ",", tokens,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
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
    catch (const CException& e) {
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
    const CRequestContext& rctx = GetDiagContext().GetRequestContext();
    CDiagContext_Extra perf(status, timespan, args);
    if (rctx.IsSetHitID(CRequestContext::eHidID_Existing)) {
        perf.Print("ncbi_phid", rctx.GetHitID());
    }
    return perf;
}


void EndmFatal(const CNcbiDiag& diag)
{
    Endm(diag);
    Abort();
}


END_NCBI_SCOPE
