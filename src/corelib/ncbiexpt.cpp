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
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <stack>
#ifdef NCBI_OS_MSWIN
#  include <corelib/ncbi_os_mswin.hpp>
#endif


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
        const char* str = getenv(ABORT_ON_THROW);
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
// CException implementation

bool CException::sm_BkgrEnabled = false;


CException::CException(const CDiagCompileInfo& info,
                       const CException* prev_exception,
                       EErrCode err_code, 
                       const string& message,
                       EDiagSev severity )
: m_Severity(severity)
, m_ErrCode(err_code)
, m_Predecessor(0)
, m_InReporter(false)
{
    x_Init(info, message,prev_exception);
}


CException::CException(const CException& other)
: m_Predecessor(0)
{
    x_Assign(other);
}

CException::CException(void)
: m_Severity(eDiag_Error)
, m_Line(-1)
, m_ErrCode(CException::eInvalid)
, m_Predecessor(0)
, m_InReporter(false)
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
                            const string& message)
{
    const CException* prev = m_Predecessor;
    m_Predecessor = x_Clone();
    if (prev) {
        delete prev;
    }
    x_Init(info, message,0);
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
    diagmsg.Write(out, SDiagMessage::fNoEndl);
}

void CException::ReportExtra(ostream& /*out*/) const
{
    return;
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


void CException::x_ReportToDebugger(void) const
{
#ifdef NCBI_OS_MSWIN
    ostrstream os;
    os << "NCBI C++ Exception:" << '\n';
    os <<
        GetFile() << "(" << GetLine() << ") : " <<
        GetType() << "::" << GetErrCodeString() << " : \"" <<
        GetMsg() << "\" ";
    ReportExtra(os);
    os << '\n';
    OutputDebugString(((string)CNcbiOstrstreamToString(os)).c_str());
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
                        const CException* prev_exception)
{
    m_File     = info.GetFile();
    m_Line     = info.GetLine();
    m_Module   = info.GetModule();
    m_Class    = info.GetClass();
    m_Function = info.GetFunction();
    m_Msg      = message;
    if (!m_Predecessor && prev_exception) {
        m_Predecessor = prev_exception->x_Clone();
    }
}


void CException::x_Assign(const CException& src)
{
    m_Severity = src.m_Severity;
    m_InReporter = false;
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


void CExceptionReporter::ReportDefault(const CDiagCompileInfo& info,
    const string& title,const CException& ex, TDiagPostFlags flags)
{
    if ( !sm_DefEnabled )
        return;

    if ( sm_DefHandler ) {
        sm_DefHandler->Report(info.GetFile(), 
                              info.GetLine(), 
                              title, 
                              ex, 
                              flags);
    } else {
        CNcbiDiag(info, ex.GetSeverity(), flags) << title << ex;
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


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.45  2005/05/16 10:56:06  ssikorsk
 * Added m_Severity to the CException class
 *
 * Revision 1.44  2005/05/04 13:19:18  ssikorsk
 * Revamped CDiagCompileInfo and CNcbiDiag to use dynamically allocated buffers instead of predefined
 *
 * Revision 1.43  2005/04/14 20:25:16  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.42  2004/10/04 20:50:01  gouriano
 * Corrected copying predecessor info in CException::x_Assign
 *
 * Revision 1.41  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.40  2004/08/25 21:26:26  vakatov
 * By default, disable the background exception reporting.
 *
 * Revision 1.39  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.38  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.37  2004/05/11 15:55:31  gouriano
 * Change GetErrCode method prototype to return TErrCode - to be able to
 * safely cast EErrCode to an eInvalid
 *
 * Revision 1.36  2004/04/26 14:41:23  ucko
 * Drop the obsolete INLINE_CSTRERRADAPT hack.
 *
 * Revision 1.35  2003/10/31 19:39:46  lavr
 * Elaborate R1.34 change log to include ends removal
 *
 * Revision 1.34  2003/10/24 13:24:55  vasilche
 * Moved body of virtual method to *.cpp file; ends removed everywhere...
 *
 * Revision 1.33  2003/06/02 15:25:14  lavr
 * Replace some endl's with '\n's; and all '\0's with ends's
 *
 * Revision 1.32  2003/04/29 19:47:10  ucko
 * KLUDGE: avoid inlining CStrErrAdapt::strerror with GCC 2.95 on OSF/1,
 * due to a weird, apparently platform-specific, bug.
 *
 * Revision 1.31  2003/02/24 19:56:05  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.30  2003/02/20 17:55:19  gouriano
 * added the possibility to abort a program on an attempt to throw CException
 *
 * Revision 1.29  2003/01/13 20:42:50  gouriano
 * corrected the problem with ostrstream::str(): replaced such calls with
 * CNcbiOstrstreamToString(os)
 *
 * Revision 1.28  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.27  2002/08/20 19:11:30  gouriano
 * added DiagPostFlags into CException reporting functions
 *
 * Revision 1.26  2002/07/29 19:29:42  gouriano
 * changes to allow multiple inheritance in CException classes
 *
 * Revision 1.24  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.23  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.22  2002/06/27 18:56:16  gouriano
 * added "title" parameter to report functions
 *
 * Revision 1.21  2002/06/26 18:38:04  gouriano
 * added CNcbiException class
 *
 * Revision 1.20  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.19  2001/07/30 14:42:10  lavr
 * eDiag_Trace and eDiag_Fatal always print as much as possible
 *
 * Revision 1.18  2001/05/21 21:44:00  vakatov
 * SIZE_TYPE --> string::size_type
 *
 * Revision 1.17  2001/05/17 15:04:59  lavr
 * Typos corrected
 *
 * Revision 1.16  2000/11/16 23:52:41  vakatov
 * Porting to Mac...
 *
 * Revision 1.15  2000/04/04 22:30:26  vakatov
 * SetThrowTraceAbort() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.14  1999/12/29 13:58:39  vasilche
 * Added THROWS_NONE.
 *
 * Revision 1.13  1999/12/28 21:04:18  vasilche
 * Removed three more implicit virtual destructors.
 *
 * Revision 1.12  1999/11/18 20:12:43  vakatov
 * DoDbgPrint() -- prototyped in both _DEBUG and NDEBUG
 *
 * Revision 1.11  1999/10/04 16:21:04  vasilche
 * Added full set of macros THROW*_TRACE
 *
 * Revision 1.10  1999/09/27 16:23:24  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.9  1999/05/04 00:03:13  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.8  1999/04/14 19:53:29  vakatov
 * + <stdio.h>
 *
 * Revision 1.7  1999/01/04 22:41:43  vakatov
 * Do not use so-called "hardware-exceptions" as these are not supported
 * (on the signal level) by UNIX
 * Do not "set_unexpected()" as it works differently on UNIX and MSVC++
 *
 * Revision 1.6  1998/12/28 17:56:37  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.3  1998/11/13 00:17:13  vakatov
 * [UNIX] Added handler for the unexpected exceptions
 *
 * Revision 1.2  1998/11/10 17:58:42  vakatov
 * [UNIX] Removed extra #define's (POSIX... and EXTENTIONS...)
 * Allow adding strings in CNcbiErrnoException(must have used "CC -xar"
 * instead of just "ar" when building a library;  otherwise -- link error)
 *
 * Revision 1.1  1998/11/10 01:20:01  vakatov
 * Initial revision(derived from former "ncbiexcp.cpp")
 *
 * ===========================================================================
 */
