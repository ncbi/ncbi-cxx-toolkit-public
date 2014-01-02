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
 * Authors:  Denis Vakatov, Andrei Gourianov,
 *           Eugene Vasilchenko, Anton Lavrentiev
 *
 * File Description:
 *   CException
 *   CExceptionReporter
 *   CExceptionReporterStream
 *   CErrnoException
 *   CParseException
 *   + initialization for the "unexpected"
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/error_codes.hpp>
#include "ncbisys.hpp"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stack>


#define NCBI_USE_ERRCODE_X   Corelib_Diag


BEGIN_NCBI_SCOPE

/////////////////////////////////
// SetThrowTraceAbort
// DoThrowTraceAbort

static bool s_DoThrowTraceAbort = false; //if to abort() in DoThrowTraceAbort()
static bool s_DTTA_Initialized  = false; //if s_DoThrowTraceAbort is init'd

extern void SetThrowTraceAbort(bool abort_on_throw_trace)
{
    s_DTTA_Initialized = true;
    s_DoThrowTraceAbort = abort_on_throw_trace;
}

extern void DoThrowTraceAbort(void)
{
    if ( !s_DTTA_Initialized ) {
        const TXChar* str = NcbiSys_getenv(_T_XCSTRING(ABORT_ON_THROW));
        if (str  &&  *str)
            s_DoThrowTraceAbort = true;
        s_DTTA_Initialized  = true;
    }

    if ( s_DoThrowTraceAbort )
        abort();
}

extern void DoDbgPrint(const CDiagCompileInfo &info, const char* message)
{
    CNcbiDiag(info, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const CDiagCompileInfo &info, const string& message)
{
    CNcbiDiag(info, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const CDiagCompileInfo &info,
                       const char* msg1, const char* msg2)
{
    CNcbiDiag(info, eDiag_Trace) << msg1 << ": " << msg2;
    DoThrowTraceAbort();
}


/////////////////////////////////////////////////////////////////////////////
// Stack trace control

NCBI_PARAM_ENUM_ARRAY(EDiagSev, EXCEPTION, Stack_Trace_Level)
{
    {"Trace",    eDiag_Trace},
    {"Info",     eDiag_Info},
    {"Warning",  eDiag_Warning},
    {"Error",    eDiag_Error},
    {"Critical", eDiag_Critical},
    {"Fatal",    eDiag_Fatal}
};


NCBI_PARAM_ENUM_DECL(EDiagSev, EXCEPTION, Stack_Trace_Level);
NCBI_PARAM_ENUM_DEF_EX(EDiagSev,
                       EXCEPTION,
                       Stack_Trace_Level,
                       eDiag_Critical,
                       eParam_NoThread, // No per-thread values
                       EXCEPTION_STACK_TRACE_LEVEL);
typedef NCBI_PARAM_TYPE(EXCEPTION, Stack_Trace_Level) TStackTraceLevelParam;

static TStackTraceLevelParam s_StackTraceLevel;

void CException::SetStackTraceLevel(EDiagSev level)
{
    s_StackTraceLevel.Set(level);
}


EDiagSev CException::GetStackTraceLevel(void)
{
    return s_StackTraceLevel.Get();
}


NCBI_PARAM_DECL(bool, EXCEPTION, Abort_If_Critical);
NCBI_PARAM_DEF_EX(bool,
                  EXCEPTION,
                  Abort_If_Critical,
                  false,
                  eParam_NoThread,
                  EXCEPTION_ABORT_IF_CRITICAL);
typedef NCBI_PARAM_TYPE(EXCEPTION, Abort_If_Critical) TAbortIfCritical;

static TAbortIfCritical s_AbortIfCritical;


/////////////////////////////////////////////////////////////////////////////
// CException implementation

bool CException::sm_BkgrEnabled = false;


CException::CException(const CDiagCompileInfo& info,
                       const CException* prev_exception,
                       EErrCode err_code,
                       const string& message,
                       EDiagSev severity,
                       TFlags flags)
: m_Severity(severity),
  m_ErrCode(err_code),
  m_Predecessor(0),
  m_InReporter(false),
  m_MainText(true),
  m_Flags(0)
{
    if (CompareDiagPostLevel(severity, eDiag_Critical) >= 0  &&
        s_AbortIfCritical.Get()) {
        abort();
    }
    x_Init(info, message, prev_exception, severity);
    if (prev_exception)
        prev_exception->m_MainText = false;
}


CException::CException(const CDiagCompileInfo& info,
                       const CException* prev_exception,
                       const CExceptionArgs<EErrCode>& args,
                       const string& message)
: m_Severity(args.GetSeverity()),
  m_ErrCode(args.GetErrCode()),
  m_Predecessor(0),
  m_InReporter(false),
  m_MainText(true),
  m_Flags(args.GetFlags())
{
    if (CompareDiagPostLevel(m_Severity, eDiag_Critical) >= 0  &&
        s_AbortIfCritical.Get()) {
        abort();
    }
    x_Init(info, message, prev_exception, m_Severity);
    x_InitArgs(args);

    if (prev_exception)
        prev_exception->m_MainText = false;
}


CException::CException(const CException& other)
: m_Predecessor(0)
{
    x_Assign(other);
}

CException::CException(void)
: m_Severity(eDiag_Error),
  m_Line(-1),
  m_ErrCode(CException::eInvalid),
  m_Predecessor(0),
  m_InReporter(false),
  m_MainText(true),
  m_Flags(0)
{
// this only is called in case of multiple inheritance
}


CException::~CException(void) throw()
{
    if (m_Predecessor) {
        delete m_Predecessor;
        m_Predecessor = 0;
    }
}


const char* CException::GetType(void) const
{
    return "CException";
}


void CException::AddBacklog(const CDiagCompileInfo& info,
                            const string& message,
                            EDiagSev severity)
{
    const CException* prev = m_Predecessor;
    m_Predecessor = x_Clone();
    if (prev) {
        delete prev;
    }
    x_Init(info, message, 0, severity);
    m_MainText = false;
}


void CException::AddToMessage(const string& add_msg)
{
    if (add_msg.empty()) {
        return;
    }

    if (m_Msg.empty()  &&  m_Predecessor != NULL) {
        m_Msg = m_Predecessor->GetMsg();
    }
    m_Msg += add_msg;
}


void CException::AddPrevious(const CException* prev_exception)
{
    if (m_Predecessor) {
        const CException* prev = m_Predecessor;
        const CException* next = prev->m_Predecessor;
        while (next) {
            prev = next;
            next = prev->m_Predecessor;
        }
        prev->m_Predecessor = prev_exception->x_Clone();
    }
    else {
        m_Predecessor = prev_exception->x_Clone();
    }

    for (const CException* pex = prev_exception; pex; pex = pex->m_Predecessor)
        pex->m_MainText = false;
}


CException& CException::SetSeverity(EDiagSev severity)
{
    if (CompareDiagPostLevel(severity, eDiag_Critical) >= 0  &&
        s_AbortIfCritical.Get()) {
        abort();
    }
    m_Severity = severity;
    x_GetStackTrace(); // May need stack trace with the new severity
    return *this;
}


void CException::Throw(void) const
{
    x_ThrowSanityCheck(typeid(CException), "CException");
    throw *this;
}


// ---- report --------------

const char* CException::what(void) const throw()
{
    m_What = ReportAll();
    return m_What.c_str();
}


void CException::Report(const CDiagCompileInfo& info,
                        const string& title,CExceptionReporter* reporter,
                        TDiagPostFlags flags) const
{
    if (reporter) {
        reporter->Report(info.GetFile(), info.GetLine(), title, *this, flags);
    }
    // unconditionally ...
    // that is, there will be two reports
    CExceptionReporter::ReportDefault(info, title, *this, flags);
}


string CException::ReportAll(TDiagPostFlags flags) const
{
    // invert the order
    stack<const CException*> pile;
    const CException* pex;
    for (pex = this; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    ostrstream os;
    os << "NCBI C++ Exception:" << '\n';
    for (; !pile.empty(); pile.pop()) {
        //indentation
        os << "    ";
        os << pile.top()->ReportThis(flags) << '\n';
    }
    if (sm_BkgrEnabled && !m_InReporter) {
        m_InReporter = true;
        CExceptionReporter::ReportDefault(CDiagCompileInfo(0, 0,
                                           NCBI_CURRENT_FUNCTION),
                                          "(background reporting)",
                                          *this, eDPF_Trace);
        m_InReporter = false;
    }
    return CNcbiOstrstreamToString(os);
}


string CException::ReportThis(TDiagPostFlags flags) const
{
    ostrstream os, osex;
    ReportStd(os, flags);
    ReportExtra(osex);
    if (osex.pcount() != 0) {
        os << " (" << (string)CNcbiOstrstreamToString(osex) << ')';
    }
    return CNcbiOstrstreamToString(os);
}


void CException::ReportStd(ostream& out, TDiagPostFlags flags) const
{
    string text(GetMsg());
    string err_type(GetType());
    err_type += "::";
    err_type += GetErrCodeString();
    SDiagMessage diagmsg(
        GetSeverity(),
        text.c_str(),
        text.size(),
        GetFile().c_str(),
        GetLine(),
        flags, NULL, 0, 0, err_type.c_str(),
        GetModule().c_str(),
        GetClass().c_str(),
        GetFunction().c_str());
    diagmsg.Write(out, SDiagMessage::fNoEndl | SDiagMessage::fNoPrefix);
}

void CException::ReportExtra(ostream& /*out*/) const
{
    return;
}


const CStackTrace* CException::GetStackTrace(void) const
{
    if (!m_StackTrace.get()  ||  m_StackTrace->Empty()  ||
        CompareDiagPostLevel(m_Severity, GetStackTraceLevel()) < 0) {
        return NULL;
    }
    return m_StackTrace.get();
}


const char* CException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eUnknown: return "eUnknown";
    default:       return "eInvalid";
    }
}


CException::TErrCode CException::GetErrCode (void) const
{
    return typeid(*this) == typeid(CException) ?
        (TErrCode) x_GetErrCode() :
        (TErrCode) CException::eInvalid;
}


const string& CException::GetMsg(void) const
{
    for (const CException* ex = this;  ex != NULL;  ex = ex->m_Predecessor) {
        if ( !ex->m_Msg.empty() ) {
            return ex->m_Msg;
        }
    }
    return kEmptyStr;
}


void CException::x_ReportToDebugger(void) const
{
#if defined(NCBI_OS_MSWIN)  &&  defined(_DEBUG)
    // On MS Windows print out reported information into debug output window
    ostrstream os;
    os << "NCBI C++ Exception:" << '\n';
    os <<
        GetFile() << "(" << GetLine() << ") : " <<
        GetType() << "::" << GetErrCodeString() << " : \"" <<
        GetMsg() << "\" ";
    ReportExtra(os);
    os << '\n';
    ::OutputDebugStringA(((string)CNcbiOstrstreamToString(os)).c_str());
#endif
    DoThrowTraceAbort();
}


bool CException::EnableBackgroundReporting(bool enable)
{
    bool prev = sm_BkgrEnabled;
    sm_BkgrEnabled = enable;
    return prev;
}

const CException* CException::x_Clone(void) const
{
    return new CException(*this);
}


void CException::x_Init(const CDiagCompileInfo& info,const string& message,
                        const CException* prev_exception, EDiagSev severity)
{
    m_Severity = severity;
    m_File     = info.GetFile();
    m_Line     = info.GetLine();
    m_Module   = info.GetModule();
    m_Class    = info.GetClass();
    m_Function = info.GetFunction();
    m_Msg      = message;
    if (!m_Predecessor && prev_exception) {
        m_Predecessor = prev_exception->x_Clone();
    }
    x_GetStackTrace();
}


void CException::x_InitArgs(const CExceptionArgs_Base& args)
{
    if ( args.IsSetModule() ) {
        m_Module = args.GetModule();
    }
}


void CException::x_Assign(const CException& src)
{
    m_Severity = src.m_Severity;
    m_InReporter = false;
    m_MainText = src.m_MainText;
    m_File     = src.m_File;
    m_Line     = src.m_Line;
    m_Msg      = src.m_Msg;
    x_AssignErrCode(src);
    m_Module   = src.m_Module;
    m_Class    = src.m_Class;
    m_Function = src.m_Function;
    if (!m_Predecessor && src.m_Predecessor) {
        m_Predecessor = src.m_Predecessor->x_Clone();
    }
    if ( src.m_StackTrace.get() ) {
        m_StackTrace.reset(new CStackTrace(*src.m_StackTrace));
    }
    m_Flags = src.m_Flags;
}


void CException::x_AssignErrCode(const CException& src)
{
    m_ErrCode = typeid(*this) == typeid(src) ?
        src.m_ErrCode : CException::eInvalid;
}


void CException::x_InitErrCode(EErrCode err_code)
{
    m_ErrCode = err_code;
    if (m_ErrCode != eInvalid && !m_Predecessor) {
        x_ReportToDebugger();
    }
}


void CException::x_GetStackTrace(void)
{
    if ( m_StackTrace.get() ) {
        return;
    }
    if (CompareDiagPostLevel(m_Severity, GetStackTraceLevel()) < 0) {
        return;
    }
    m_StackTrace.reset(new CStackTrace);
}


void CException::x_ThrowSanityCheck(const type_info& expected_type,
                                    const char* human_name) const
{
    const type_info& actual_type = typeid(*this);
    if (actual_type != expected_type) {
        ERR_POST_X(14, Warning << "CException::Throw(): throwing object of type "
                       << actual_type.name() << " as " << expected_type.name()
                       << " [" << human_name << ']');
    }
}


/////////////////////////////////////////////////////////////////////////////
// CExceptionReporter

const CExceptionReporter* CExceptionReporter::sm_DefHandler = 0;

bool CExceptionReporter::sm_DefEnabled = true;


CExceptionReporter::CExceptionReporter(void)
{
    return;
}


CExceptionReporter::~CExceptionReporter(void)
{
    return;
}


void CExceptionReporter::SetDefault(const CExceptionReporter* handler)
{
    sm_DefHandler = handler;
}


const CExceptionReporter* CExceptionReporter::GetDefault(void)
{
    return sm_DefHandler;
}


bool CExceptionReporter::EnableDefault(bool enable)
{
    bool prev = sm_DefEnabled;
    sm_DefEnabled = enable;
    return prev;
}


class CExceptionWrapper : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    CExceptionWrapper(const CDiagCompileInfo& info,
                      const exception&        ex);

    virtual const char* GetErrCodeString(void) const;
    virtual const char* GetType(void) const;
};


CExceptionWrapper::CExceptionWrapper(const CDiagCompileInfo& info,
                                     const exception&        ex)
    : CException(info, NULL, eUnknown, ex.what())
{
}


const char* CExceptionWrapper::GetErrCodeString(void) const
{
    return kEmptyCStr;
}


const char* CExceptionWrapper::GetType(void) const
{
    return "exception";
}


void CExceptionReporter::ReportDefault(const CDiagCompileInfo& info,
    const string& title,const exception& ex, TDiagPostFlags flags)
{
    ReportDefaultEx(0, 0, info, title, ex, flags);
}

void CExceptionReporter::ReportDefaultEx(int err_code, int err_subcode,
    const CDiagCompileInfo& info, const string& title,const exception& ex,
    TDiagPostFlags flags)
{
    if ( !sm_DefEnabled )
        return;

    const CException* cex = dynamic_cast<const CException*>(&ex);
    auto_ptr<CException> wrapper;
    if ( !cex ) {
        wrapper.reset(new CExceptionWrapper(info, ex));
        cex = wrapper.get();
    }
    if ( sm_DefHandler ) {
        sm_DefHandler->Report(info.GetFile(),
                              info.GetLine(),
                              title,
                              *cex,
                              flags);
    } else {
        CNcbiDiag d(info, cex->GetSeverity(), flags);
        d << ErrCode(err_code, err_subcode);
        if ((err_code==0 && err_subcode==0) || d.CheckFilters()) {
            d << title << *cex;
        } else {
            Reset(d);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CExceptionReporterStream


CExceptionReporterStream::CExceptionReporterStream(ostream& out)
    : m_Out(out)
{
    return;
}


CExceptionReporterStream::~CExceptionReporterStream(void)
{
    return;
}


void CExceptionReporterStream::Report(const char* file, int line,
    const string& title, const CException& ex, TDiagPostFlags flags) const
{
    SDiagMessage diagmsg(ex.GetSeverity(),
                         title.c_str(),
                         title.size(),
                         file,
                         line,
                         flags,
                         NULL,
                         0, 0,
                         ex.GetModule().c_str(),
                         ex.GetClass().c_str(),
                         ex.GetFunction().c_str());
    diagmsg.Write(m_Out);

    m_Out << "NCBI C++ Exception:" << endl;
    // invert the order
    stack<const CException*> pile;
    const CException* pex;
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        m_Out << "    ";
        m_Out << pex->ReportThis(flags) << endl;
    }
}


/////////////////////////////////////////////////////////////////////////////
// Core exceptions
/////////////////////////////////////////////////////////////////////////////


const char* CCoreException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eCore:       return "eCore";
    case eNullPtr:    return "eNullPtr";
    case eDll:        return "eDll";
    case eDiagFilter: return "eDiagFilter";
    case eInvalidArg: return "eInvalidArg";
    default:          return CException::GetErrCodeString();
    }
}

#if (defined(NCBI_OS_MSWIN) && defined(_UNICODE)) || \
        (NCBI_COMPILER_MSVC && (_MSC_VER >= 1400) && __STDC_WANT_SECURE_LIB__)
// MT: Store pointer to the strerror message in TLS
static CStaticTls<char> s_TlsStrerrorMessage;
static void s_TlsStrerrorMessageCleanup(char* msg, void* /* data */)
{
    delete [] msg;
}
#endif

extern const char*  Ncbi_strerror(int errnum)
{
#if (defined(NCBI_OS_MSWIN) && defined(_UNICODE)) || \
        (NCBI_COMPILER_MSVC && (_MSC_VER >= 1400) && __STDC_WANT_SECURE_LIB__)
    string tmp;
#  if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400) && __STDC_WANT_SECURE_LIB__
    TXChar xbuf[256];
    NcbiSys_strerror_s(xbuf,sizeof(xbuf)/sizeof(TXChar),errnum);
    tmp = _T_STDSTRING(xbuf);
#  else
    tmp = _T_STDSTRING( NcbiSys_strerror(errnum) );
#  endif
    char* ptr = new char[ tmp.size() + 1];
    strcpy(ptr, tmp.c_str());
    s_TlsStrerrorMessage.SetValue(ptr, s_TlsStrerrorMessageCleanup);
    return ptr;
#else
    return NcbiSys_strerror(errnum);
#endif
}

#if defined(NCBI_OS_MSWIN)

// MT: Store pointer to the last error message in TLS
static CStaticTls<char> s_TlsErrorMessage;
static void s_TlsErrorMessageCleanup(char* msg, void* /* data */)
{
    LocalFree(msg);
}

const char* CLastErrorAdapt::GetErrCodeString(int errnum)
{
    TXChar* xptr = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM     |
                  FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  "%0", errnum,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (TXChar*)&xptr, 0, NULL);
#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
    CStringUTF8 tmp( CUtf8::AsUTF8(xptr));
    char* ptr = (char*)LocalAlloc( LPTR, tmp.size() + 1);
    strcpy(ptr, tmp.c_str());
    LocalFree(xptr);
#else
    char* ptr = xptr;
#endif
    // Remove trailing dots and spaces
    size_t pos = strlen(ptr);
    if ( pos ) {
        while (--pos  &&  (ptr[pos] == '.' || ptr[pos] == ' ')) {
            ptr[pos] = '\0';
        }
    }
    // Save pointer
    s_TlsErrorMessage.SetValue(ptr, s_TlsErrorMessageCleanup);
    return ptr;
}

#endif


END_NCBI_SCOPE
