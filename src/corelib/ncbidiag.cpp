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
* Fixes to (first-time) compile and test-run on MSVS++
* ==========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <stdlib.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


//## DECLARE_MUTEX(s_Mutex);  // protective mutex



///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev     CDiagBuffer::sm_PostSeverity   = eDiag_Error;
#else
EDiagSev     CDiagBuffer::sm_PostSeverity   = eDiag_Warning;
#endif /* else!NDEBUG */

unsigned int CDiagBuffer::sm_PostFlags      = eDPF_Prefix | eDPF_Severity;

char*        CDiagBuffer::sm_PostPrefix     = 0;
EDiagSev     CDiagBuffer::sm_DieSeverity    = eDiag_Fatal;

EDiagTrace   CDiagBuffer::sm_TraceDefault   = eDT_Default;
bool         CDiagBuffer::sm_TraceEnabled;  // to be set on first request


const char*  CDiagBuffer::sm_SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Critical", "Fatal", "Trace"
};


static void s_DefaultHandlerFunc(const SDiagMessage& mess) {
    NcbiCerr << mess << NcbiFlush;
}

FDiagHandler CDiagBuffer::sm_HandlerFunc    = s_DefaultHandlerFunc;
void*        CDiagBuffer::sm_HandlerData    = 0;
FDiagCleanup CDiagBuffer::sm_HandlerCleanup = 0;


CDiagBuffer::CDiagBuffer(void)
    : m_Stream(new CNcbiOstrstream)
{
    m_Diag = 0;
}

CDiagBuffer::~CDiagBuffer(void)
{
    if (m_Diag  ||  dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount())
        ::abort();
    delete m_Stream;
}

void CDiagBuffer::DiagHandler(SDiagMessage& mess)
{
    if ( CDiagBuffer::sm_HandlerFunc ) {
        //## MUTEX_LOCK(s_Mutex);
        if ( CDiagBuffer::sm_HandlerFunc ) {
            mess.m_Data   = CDiagBuffer::sm_HandlerData;
            mess.m_Prefix = CDiagBuffer::sm_PostPrefix;
            CDiagBuffer::sm_HandlerFunc(mess);
        }
        //## MUTEX_UNLOCK(s_Mutex);
    }
}

bool CDiagBuffer::SetDiag(const CNcbiDiag& diag)
{
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
        SDiagMessage mess(sev, message, ostr->pcount(), 0,
                          m_Diag->GetFile(), m_Diag->GetLine(),
                          m_Diag->GetPostFlags());
        DiagHandler(mess);
        Reset(*m_Diag);
    }

    if (sev >= sm_DieSeverity  &&  sev != eDiag_Trace) {
        m_Diag = 0;
#if defined(_DEBUG)
        ::abort();
#else
        ::exit(-1);
#endif
    }
}


bool CDiagBuffer::GetTraceEnabledFirstTime(void)
{
    //## MUTEX_LOCK(s_Mutex);
    const char* str = ::getenv(DIAG_TRACE);
    if (str  &&  *str) {
        sm_TraceDefault = eDT_Enable;
    } else {
        sm_TraceDefault = eDT_Disable;
    }
    sm_TraceEnabled = (sm_TraceDefault == eDT_Enable);
    //## MUTEX_UNLOCK(s_Mutex);
    return sm_TraceEnabled;
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
    if ( IsSetDiagPostFlag(eDPF_Severity, m_Flags) )
        os << CNcbiDiag::SeverityName(m_Severity) << ": ";

    // [<prefix>]
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

extern void SetDiagPostFlag(EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    //## MUTEX_LOCK(s_Mutex);
    CDiagBuffer::sm_PostFlags |= flag;
    //## MUTEX_UNLOCK(s_Mutex);
}

extern void UnsetDiagPostFlag(EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    //## MUTEX_LOCK(s_Mutex);
    CDiagBuffer::sm_PostFlags &= ~flag;
    //## MUTEX_UNLOCK(s_Mutex);
}


extern void SetDiagPostPrefix(const char* prefix)
{
    //## MUTEX_LOCK(s_Mutex);
    delete[] CDiagBuffer::sm_PostPrefix;
    if (prefix  &&  *prefix) {
        CDiagBuffer::sm_PostPrefix = new char[::strlen(prefix) + 1];
        ::strcpy(CDiagBuffer::sm_PostPrefix, prefix);
    } else {
        CDiagBuffer::sm_PostPrefix = 0;
    }
    //## MUTEX_UNLOCK(s_Mutex);
}

extern EDiagSev SetDiagPostLevel(EDiagSev post_sev)
{
    //## MUTEX_LOCK(s_Mutex);
    EDiagSev sev = CDiagBuffer::sm_PostSeverity;
    CDiagBuffer::sm_PostSeverity = post_sev;
    //## MUTEX_UNLOCK(s_Mutex);
    return sev;
}


extern EDiagSev SetDiagDieLevel(EDiagSev die_sev)
{
    //## MUTEX_LOCK(s_Mutex);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    //## MUTEX_UNLOCK(s_Mutex);
    return sev;
}


extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt)
{
    //## MUTEX_LOCK(s_Mutex);
    (void) CDiagBuffer::GetTraceEnabled();

    if (dflt != eDT_Default)
        CDiagBuffer::sm_TraceDefault = dflt;

    if (how == eDT_Default)
        how = CDiagBuffer::sm_TraceDefault;
    CDiagBuffer::sm_TraceEnabled = (how == eDT_Enable);
    //## MUTEX_UNLOCK(s_Mutex);
}


extern void SetDiagHandler(FDiagHandler func, void* data,
                           FDiagCleanup cleanup)
{
    //## MUTEX_LOCK(s_Mutex);
    if ( CDiagBuffer::sm_HandlerCleanup )
        CDiagBuffer::sm_HandlerCleanup(CDiagBuffer::sm_HandlerData);
    CDiagBuffer::sm_HandlerFunc    = func;
    CDiagBuffer::sm_HandlerData    = data;
    CDiagBuffer::sm_HandlerCleanup = cleanup;
    //## MUTEX_UNLOCK(s_Mutex);
}

extern bool IsSetDiagHandler(void)
{
    return (CDiagBuffer::sm_HandlerFunc != s_DefaultHandlerFunc);
}


//## static void s_TlsCleanup(TTls TLS, void* old_value)
//## {
//##     delete (CDiagBuffer *)old_value;
//## }

extern CDiagBuffer& GetDiagBuffer(void)
{
    //## TLS_DECLARE(s_DiagBufferTls);
    //## CDiagBuffer* msg_buf = TLS_GET_VALUE(s_DiagBufferTls);
    //## if ( !msg_buf ) {
    //##    msg_buf = new CDiagBuffer;
    //##    TLS_SET_VALUE(s_DiagBufferTls, msg_buf, s_TlsCleanup);
    //## }
    //## return *msg_buf;

    static CDiagBuffer s_DiagBuffer;
    return s_DiagBuffer;
}



/////////////////////////////////////////////////////////////////////////////
// A common-use case:  direct all messages to an output stream (C++ "ostream")
//

struct SToStream_Data {
    CNcbiOstream* os;
    bool          quick_flush;
    FDiagCleanup  cleanup;
    void*         cleanup_data;
};

static void s_ToStream_Handler(const SDiagMessage& mess)
{ // (it is MT-protected by the caller, "CDiagBuffer::DiagHandler()")
    SToStream_Data* x_data = static_cast<SToStream_Data*>(mess.m_Data);
    if ( !x_data )
        return;

    CNcbiOstream& os = *x_data->os;
    os << mess;
    if ( x_data->quick_flush ) {
        os << NcbiFlush;
    }
}

static void s_ToStream_Cleanup(void* data)
{
    SToStream_Data* x_data = static_cast<SToStream_Data*>(data);
    if ( x_data->cleanup ) {
        x_data->cleanup(x_data->cleanup_data);
    }
    delete static_cast<SToStream_Data*>(data);
}


extern void SetDiagStream
(CNcbiOstream* os,
 bool          quick_flush,
 FDiagCleanup  cleanup,
 void*         cleanup_data)
{
    if ( !os ) {
        SetDiagHandler(0, 0, 0);
        return;
    }

    SToStream_Data* data = new SToStream_Data;
    data->os           = os;
    data->quick_flush  = quick_flush;
    data->cleanup      = cleanup;
    data->cleanup_data = cleanup_data;
    SetDiagHandler(s_ToStream_Handler, data, s_ToStream_Cleanup);
}


extern bool IsDiagStream(const CNcbiOstream* os)
{
    bool res;
    //## MUTEX_LOCK(s_Mutex);
    if ( !CDiagBuffer::sm_HandlerData ) {
        res = (os == 0);
    } else {
        res =
            CDiagBuffer::sm_HandlerFunc == s_ToStream_Handler  &&
            ((SToStream_Data*) CDiagBuffer::sm_HandlerData)->os == os;
    }
    //## MUTEX_UNLOCK(s_Mutex);
    return res;
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, unsigned int post_flags)
    : m_Severity(sev), m_Line(0),
      m_Buffer(GetDiagBuffer()), m_PostFlags(post_flags)
{
    *m_File = '\0';
}


CNcbiDiag::CNcbiDiag(const char* file, size_t line,
                     EDiagSev sev, unsigned int post_flags)
    : m_Severity(sev), m_Line(line),
      m_Buffer(GetDiagBuffer()), m_PostFlags(post_flags)
{
    SetFile(file);
}


CNcbiDiag& CNcbiDiag::SetFile(const char* file)
{
    if (file  &&  *file) {
        ::strncpy(m_File, file, sizeof(m_File));
        m_File[sizeof(m_File) - 1] = '\0';
    } else {
        *m_File = '\0';
    }
    return *this;
}

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
