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
* File Description:
*   NCBI C++ diagnostic API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.44  2002/02/07 19:45:54  ucko
* Optionally transfer ownership in GetDiagHandler.
*
* Revision 1.43  2002/02/05 22:01:36  lavr
* Minor tweak
*
* Revision 1.42  2002/01/12 22:16:47  lavr
* Eliminated GCC warning: "'%D' yields only 2 digits of year"
*
* Revision 1.41  2001/12/07 15:27:28  ucko
* Switch CDiagRecycler over to current form of SetDiagHandler.
*
* Revision 1.40  2001/12/03 22:06:31  juran
* Use 'special environment' flag to indicate that a fatal error
* must throw an exception rather than abort.  (Mac only.)
*
* Revision 1.39  2001/11/14 15:15:00  ucko
* Revise diagnostic handling to be more object-oriented.
*
* Revision 1.38  2001/10/29 15:16:13  ucko
* Preserve default CGI diagnostic settings, even if customized by app.
*
* Revision 1.37  2001/10/16 23:44:07  vakatov
* + SetDiagPostAllFlags()
*
* Revision 1.36  2001/08/24 13:48:01  grichenk
* Prevented some memory leaks
*
* Revision 1.35  2001/08/09 16:26:11  lavr
* Added handling for new eDPF_OmitInfoSev format flag
*
* Revision 1.34  2001/07/30 14:42:10  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.33  2001/07/26 21:29:00  lavr
* Remove printing DateTime stamp by default
*
* Revision 1.32  2001/07/25 19:13:55  lavr
* Added date/time stamp for message logging
*
* Revision 1.31  2001/06/13 23:19:38  vakatov
* Revamped previous revision (prefix and error codes)
*
* Revision 1.30  2001/06/13 20:48:28  ivanov
* + PushDiagPostPrefix(), PopPushDiagPostPrefix() - stack post prefix messages.
* + ERR_POST_EX, LOG_POST_EX - macros for posting with error codes.
* + ErrCode(code[,subcode]) - CNcbiDiag error code manipulator.
* + eDPF_ErrCode, eDPF_ErrSubCode - new post flags.
*
* Revision 1.29  2001/06/05 20:58:16  vakatov
* ~CDiagBuffer()::  to check for consistency and call "abort()" only
* #if (_DEBUG > 1)
*
* Revision 1.28  2001/05/17 15:04:59  lavr
* Typos corrected
*
* Revision 1.27  2001/03/30 22:49:22  grichenk
* KCC freeze() bug workaround
*
* Revision 1.26  2001/03/26 21:45:54  vakatov
* Made MT-safe (with A.Grichenko)
*
* Revision 1.25  2001/01/23 23:20:42  lavr
* MSVS++ -> MSVC++
*
* Revision 1.24  2000/11/16 23:52:41  vakatov
* Porting to Mac...
*
* Revision 1.23  2000/10/24 21:51:21  vakatov
* [DEBUG] By default, do not print file name and line into the diagnostics
*
* Revision 1.22  2000/10/24 19:54:46  vakatov
* Diagnostics to go to CERR by default (was -- disabled by default)
*
* Revision 1.21  2000/06/22 22:09:10  vakatov
* Fixed:  GetTraceEnabledFirstTime(), sm_TraceDefault
*
* Revision 1.20  2000/06/11 01:47:28  vakatov
* IsDiagSet(0) to return TRUE if the diag stream is unset
*
* Revision 1.19  2000/06/09 21:22:21  vakatov
* IsDiagStream() -- fixed
*
* Revision 1.18  2000/04/04 22:31:59  vakatov
* SetDiagTrace() -- auto-set basing on the application
* environment and/or registry
*
* Revision 1.17  2000/02/18 16:54:07  vakatov
* + eDiag_Critical
*
* Revision 1.16  2000/01/20 16:52:32  vakatov
* SDiagMessage::Write() to replace SDiagMessage::Compose()
* + operator<<(CNcbiOstream& os, const SDiagMessage& mess)
* + IsSetDiagHandler(), IsDiagStream()
*
* Revision 1.15  1999/12/29 22:30:25  vakatov
* Use "exit()" rather than "abort()" in non-#_DEBUG mode
*
* Revision 1.14  1999/12/29 21:22:30  vakatov
* Fixed "delete" to "delete[]"
*
* Revision 1.13  1999/12/27 19:44:18  vakatov
* Fixes for R1.13:
* ERR_POST() -- use eDPF_Default rather than eDPF_Trace;  forcibly flush
* ("<< Endm") the diag. stream. Get rid of the eDPF_CopyFilename, always
* make a copy of the file name.
*
* Revision 1.12  1999/12/16 17:22:51  vakatov
* Fixed "delete" to "delete[]"
*
* Revision 1.11  1999/09/27 16:23:23  vasilche
* Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
* so that they will be much easier for compilers to eat.
*
* Revision 1.10  1999/05/27 16:32:26  vakatov
* In debug-mode(#_DEBUG), set the default post severity level to
* "Warning" (yet, it is "Error" in non-debug mode)
*
* Revision 1.9  1999/04/30 19:21:04  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.8  1998/12/28 17:56:37  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.7  1998/11/06 22:42:41  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.6  1998/11/03 22:57:51  vakatov
* Use #define'd manipulators(like "NcbiFlush" instead of "flush") to
* make it compile and work with new(templated) version of C++ streams
*
* Revision 1.4  1998/11/03 22:28:35  vakatov
* Renamed Die/Post...Severity() to ...Level()
*
* Revision 1.3  1998/11/03 20:51:26  vakatov
* Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
*
* Revision 1.2  1998/10/30 20:08:37  vakatov
* Fixes to (first-time) compile and test-run on MSVC++
* ==========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <stdlib.h>
#include <time.h>

#if defined(NCBI_OS_MAC)
#include <corelib/ncbi_os_mac.hpp>
#endif


BEGIN_NCBI_SCOPE


static CMutex s_DiagMutex;


class CDiagRecycler {
public:
    CDiagRecycler(void) {};
    ~CDiagRecycler(void)
    {
        SetDiagHandler(0, false);
    }
};

static CSafeStaticPtr<CDiagRecycler> s_DiagRecycler;


///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev     CDiagBuffer::sm_PostSeverity   = eDiag_Error;
#else
EDiagSev     CDiagBuffer::sm_PostSeverity   = eDiag_Warning;
#endif /* else!NDEBUG */

TDiagPostFlags CDiagBuffer::sm_PostFlags      =
eDPF_Prefix | eDPF_Severity | eDPF_ErrCode | eDPF_ErrSubCode;

EDiagSev     CDiagBuffer::sm_DieSeverity    = eDiag_Fatal;

EDiagTrace   CDiagBuffer::sm_TraceDefault   = eDT_Default;
bool         CDiagBuffer::sm_TraceEnabled;  // to be set on first request


const char*  CDiagBuffer::sm_SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Critical", "Fatal", "Trace" };

// Use s_DefaultHandler only for purposes of comparison, as installing
// another handler will normally delete it.
CDiagHandler* s_DefaultHandler = new CStreamDiagHandler(&NcbiCerr);
CDiagHandler* CDiagBuffer::sm_Handler = s_DefaultHandler;
bool          CDiagBuffer::sm_CanDeleteHandler = true;


CDiagBuffer::CDiagBuffer(void)
    : m_Stream(new CNcbiOstrstream)
{
    m_Diag = 0;
}

CDiagBuffer::~CDiagBuffer(void)
{
#if (_DEBUG > 1)
    if (m_Diag  ||  dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount())
        ::abort();
#endif
    delete m_Stream;
    m_Stream = 0;
}

void CDiagBuffer::DiagHandler(SDiagMessage& mess)
{
    if ( CDiagBuffer::sm_Handler ) {
        CMutexGuard LOCK(s_DiagMutex);
        if ( CDiagBuffer::sm_Handler ) {
            mess.m_Prefix = GetDiagBuffer().m_PostPrefix.empty() ?
                0 : GetDiagBuffer().m_PostPrefix.c_str();
            CDiagBuffer::sm_Handler->Post(mess);
        }
    }
}

bool CDiagBuffer::SetDiag(const CNcbiDiag& diag)
{
    if ( !m_Stream ) {
        return false;
    }

    if (diag.GetSeverity() < sm_PostSeverity  ||
        (diag.GetSeverity() == eDiag_Trace  &&  !GetTraceEnabled())) {
        return false;
    }

    if (m_Diag != &diag) {
        if ( dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount() ) {
            Flush();
        }
        m_Diag = &diag;
    }

    return true;
}

void CDiagBuffer::Flush(void)
{
    if ( !m_Diag )
        return;

    CNcbiOstrstream* ostr = dynamic_cast<CNcbiOstrstream*>(m_Stream);
    EDiagSev sev = m_Diag->GetSeverity();
    if ( ostr->pcount() ) {
        const char* message = ostr->str();
        ostr->rdbuf()->freeze(0);
        SDiagMessage mess
            (sev, message, ostr->pcount(),
             m_Diag->GetFile(), m_Diag->GetLine(),
             m_Diag->GetPostFlags() | (sev == eDiag_Trace || sev == eDiag_Fatal
                                       ? eDPF_Trace : 0),
             0, m_Diag->GetErrorCode(), m_Diag->GetErrorSubCode());
        DiagHandler(mess);

#if defined(NCBI_COMPILER_KCC)
        // KCC's implementation of "freeze(0)" makes the ostrstream buffer
        // stuck. We need to replace the frozen stream with the new one.
        delete ostr;
        m_Stream = new CNcbiOstrstream;
#endif

        Reset(*m_Diag);
    }

    if (sev >= sm_DieSeverity  &&  sev != eDiag_Trace) {
        m_Diag = 0;
#if defined(NCBI_OS_MAC)
        if ( g_Mac_SpecialEnvironment ) {
            throw runtime_error("Application aborted.");
        }
#endif
#if defined(_DEBUG)
        ::abort();
#else
        ::exit(-1);
#endif
    }
}


bool CDiagBuffer::GetTraceEnabledFirstTime(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    const char* str = ::getenv(DIAG_TRACE);
    if (str  &&  *str) {
        sm_TraceDefault = eDT_Enable;
    } else {
        sm_TraceDefault = eDT_Disable;
    }
    sm_TraceEnabled = (sm_TraceDefault == eDT_Enable);
    return sm_TraceEnabled;
}


void CDiagBuffer::UpdatePrefix(void)
{
    m_PostPrefix.erase();
    iterate(TPrefixList, prefix, m_PrefixList) {
        if (prefix != m_PrefixList.begin()) {
            m_PostPrefix += "::";
        }
        m_PostPrefix += *prefix;
    }
}


///////////////////////////////////////////////////////
//  CDiagMessage::

void SDiagMessage::Write(string& str) const
{
    CNcbiOstrstream ostr;
    ostr << *this;
    ostr.put('\0');
    str = ostr.str();
    ostr.rdbuf()->freeze(0);
}


CNcbiOstream& SDiagMessage::Write(CNcbiOstream& os) const
{
    // Date & time
    if (IsSetDiagPostFlag(eDPF_DateTime, m_Flags)) {
        static const char timefmt[] = "%D %T ";
        time_t t = time(0);
        char datetime[32];
        struct tm* tm;
#ifdef HAVE_LOCALTIME_R
        struct tm temp;
        localtime_r(&t, &temp);
        tm = &temp;
#else
        tm = localtime(&t);
#endif /*HAVE_LOCALTIME_R*/
        strftime(datetime, sizeof(datetime), timefmt, tm);
        os << datetime;
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

    // <severity>:
    if (IsSetDiagPostFlag(eDPF_Severity, m_Flags)  &&
        (m_Severity != eDiag_Info || !IsSetDiagPostFlag(eDPF_OmitInfoSev)))
        os << CNcbiDiag::SeverityName(m_Severity) << ": ";

    // (<err_code>.<err_subcode>)
    if ((m_ErrCode  ||  m_ErrSubCode)  &&
        IsSetDiagPostFlag(eDPF_ErrCode, m_Flags)) {
        os << "(" << m_ErrCode;
        if ( IsSetDiagPostFlag(eDPF_ErrSubCode, m_Flags)) {
            os << "." << m_ErrSubCode;
        }
        os << ") ";
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if ( m_BufferLen )
        os.write(m_Buffer, m_BufferLen);

    // Endl
    os << NcbiEndl;

    return os;
}



///////////////////////////////////////////////////////
//  EXTERN


extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags)
{
    CMutexGuard LOCK(s_DiagMutex);

    TDiagPostFlags prev_flags = CDiagBuffer::sm_PostFlags;
    CDiagBuffer::sm_PostFlags = flags;
    return prev_flags;
}


extern void SetDiagPostFlag(EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CMutexGuard LOCK(s_DiagMutex);
    CDiagBuffer::sm_PostFlags |= flag;
}


extern void UnsetDiagPostFlag(EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CMutexGuard LOCK(s_DiagMutex);
    CDiagBuffer::sm_PostFlags &= ~flag;
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
    CMutexGuard LOCK(s_DiagMutex);
    EDiagSev sev = CDiagBuffer::sm_PostSeverity;
    CDiagBuffer::sm_PostSeverity = post_sev;
    return sev;
}


extern EDiagSev SetDiagDieLevel(EDiagSev die_sev)
{
    CMutexGuard LOCK(s_DiagMutex);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    return sev;
}


extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt)
{
    CMutexGuard LOCK(s_DiagMutex);
    (void) CDiagBuffer::GetTraceEnabled();

    if (dflt != eDT_Default)
        CDiagBuffer::sm_TraceDefault = dflt;

    if (how == eDT_Default)
        how = CDiagBuffer::sm_TraceDefault;
    CDiagBuffer::sm_TraceEnabled = (how == eDT_Enable);
}


extern void SetDiagHandler(CDiagHandler* handler, bool can_delete)
{
    CMutexGuard LOCK(s_DiagMutex);
    if ( CDiagBuffer::sm_CanDeleteHandler )
        delete CDiagBuffer::sm_Handler;
    CDiagBuffer::sm_Handler          = handler;
    CDiagBuffer::sm_CanDeleteHandler = can_delete;
}

extern bool IsSetDiagHandler(void)
{
    return (CDiagBuffer::sm_Handler != s_DefaultHandler);
}

extern CDiagHandler* GetDiagHandler(bool take_ownership)
{
    CMutexGuard LOCK(s_DiagMutex);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteHandler);
        CDiagBuffer::sm_CanDeleteHandler = false;
    }
    return CDiagBuffer::sm_Handler;
}


static void s_TlsDataCleanup(CDiagBuffer* old_value, void* /* cleanup_data */)
{
    delete old_value;
}

static bool s_TlsDestroyed; /* = false */

static void s_TlsObjectCleanup(void* /* ptr */)
{
    s_TlsDestroyed = true;
}


extern CDiagBuffer& GetDiagBuffer(void)
{
    static CSafeStaticRef< CTls<CDiagBuffer> >
        s_DiagBufferTls(s_TlsObjectCleanup);

    // Create and use dummy buffer if all real buffers are gone already
    // (on the application exit)
    if ( s_TlsDestroyed ) {
        static CDiagBuffer s_DiagBuffer;
        return s_DiagBuffer;
    }

    // Create thread-specific diag.buffer (if not created yet),
    // and store it to TLS
    CDiagBuffer* msg_buf = s_DiagBufferTls->GetValue();
    if ( !msg_buf ) {
        msg_buf = new CDiagBuffer;
        s_DiagBufferTls->SetValue(msg_buf, s_TlsDataCleanup);
    }

    return *msg_buf;
}


void CStreamDiagHandler::Post(const SDiagMessage& mess)
{
    if (m_Stream) {
        (*m_Stream) << mess;
        if (m_QuickFlush) {
            (*m_Stream) << NcbiFlush;
        }
    }
}


extern bool IsDiagStream(const CNcbiOstream* os)
{
    CStreamDiagHandler* sdh
        = dynamic_cast<CStreamDiagHandler*>(CDiagBuffer::sm_Handler);
    return (sdh  &&  sdh->m_Stream == os);
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), m_Line(0),  m_ErrCode(0), m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), m_PostFlags(post_flags)
{
    *m_File = '\0';
}


CNcbiDiag::CNcbiDiag(const char* file, size_t line,
                     EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), m_Line(line), m_ErrCode(0), m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), m_PostFlags(post_flags)
{
    SetFile(file);
}


CNcbiDiag& CNcbiDiag::SetFile(const char* file)
{
    if (file  &&  *file) {
        strncpy(m_File, file, sizeof(m_File));
        m_File[sizeof(m_File) - 1] = '\0';
    } else {
        *m_File = '\0';
    }
    return *this;
}


///////////////////////////////////////////////////////
//  CDiagRestorer::

CDiagRestorer::CDiagRestorer(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    const CDiagBuffer& buf = GetDiagBuffer();
    m_PostPrefix       = buf.m_PostPrefix;
    m_PrefixList       = buf.m_PrefixList;
    m_PostFlags        = buf.sm_PostFlags;
    m_PostSeverity     = buf.sm_PostSeverity;
    m_DieSeverity      = buf.sm_DieSeverity;
    m_TraceDefault     = buf.sm_TraceDefault;
    m_TraceEnabled     = buf.sm_TraceEnabled;
    m_Handler          = buf.sm_Handler;
    m_CanDeleteHandler = buf.sm_CanDeleteHandler;
    buf.sm_CanDeleteHandler = false; // avoid premature cleanup
}

CDiagRestorer::~CDiagRestorer(void)
{
    {{
        CMutexGuard LOCK(s_DiagMutex);
        CDiagBuffer& buf = GetDiagBuffer();
        buf.m_PostPrefix      = m_PostPrefix;
        buf.m_PrefixList      = m_PrefixList;
        buf.sm_PostFlags      = m_PostFlags;
        buf.sm_PostSeverity   = m_PostSeverity;
        buf.sm_DieSeverity    = m_DieSeverity;
        buf.sm_TraceDefault   = m_TraceDefault;
        buf.sm_TraceEnabled   = m_TraceEnabled;
    }}
    SetDiagHandler(m_Handler, m_CanDeleteHandler);
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
    CCompatStreamDiagHandler(CNcbiOstream* os, bool quick_flush = true,
                             FDiagCleanup cleanup = 0,
                             void* cleanup_data = 0)
        : CStreamDiagHandler(os, quick_flush),
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

extern void SetDiagStream(CNcbiOstream* os, bool quick_flush,
                          FDiagCleanup cleanup, void* cleanup_data)
{
    SetDiagHandler(new CCompatStreamDiagHandler(os, quick_flush,
                                                cleanup, cleanup_data));
}


END_NCBI_SCOPE
