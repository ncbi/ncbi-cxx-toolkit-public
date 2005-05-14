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

#include <ncbiconf.h>

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbiexpt.hpp>
#include "ncbidiag_p.hpp"
#include <stdlib.h>
#include <time.h>
#include <stack>

#if defined(NCBI_OS_MAC)
#  include <corelib/ncbi_os_mac.hpp>
#endif



BEGIN_NCBI_SCOPE

DEFINE_STATIC_MUTEX(s_DiagMutex);

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
//  Static variables for Trace and Post filters

static CDiagFilter s_TraceFilter;
static CDiagFilter s_PostFilter;


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
      m_Parsed(false)
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
      m_Parsed(false)
{
    if (!file) {
        m_File = "";
        return;
    }
    if (!module)
        return;

    // Check for a file extension without creating of temporary string objects
    const char* cur_extension = strrchr(file, '.');
    if (cur_extension == NULL)
        return; 

    if (*(cur_extension + 1) != '\0') {
        ++cur_extension;
    } else {
        return;
    }

    if (strcmp(cur_extension, "cpp") == 0 || 
        strcmp(cur_extension, "C") == 0 || 
        strcmp(cur_extension, "c") == 0 || 
        strcmp(cur_extension, "cxx") == 0 ) {
        m_Module = module;
    }
}


CDiagCompileInfo::~CDiagCompileInfo(void)
{
    return;
}


void 
CDiagCompileInfo::ParseCurrFunctName(void) const
{
    if (m_CurrFunctName && *m_CurrFunctName) {
        // Parse curr_funct

        const char* end_str = strchr(m_CurrFunctName, '(');
        if ( end_str ) {
            // Get a function/method name
            const char* start_str = NULL;

            // Get a finction start position.
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
            size_t cur_funct_name_len = end_str - cur_funct_name;
            m_FunctName = string(cur_funct_name, cur_funct_name_len);

           // Get a class name
           if (has_class) {
               end_str = start_str - 2;
               start_str = str_rev_str(m_CurrFunctName, end_str, " ");
               const char* cur_class_name =
                   (start_str == NULL ? m_CurrFunctName : start_str + 1);
               size_t cur_class_name_len = end_str - cur_class_name;
               m_ClassName = string(cur_class_name, cur_class_name_len);
           }
        }
    }
    m_Parsed = true;
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

static CSafeStaticPtr<CDiagRecycler> s_DiagRecycler;


///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Error;
#else
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Warning;
#endif /* else!NDEBUG */

EDiagSevChange CDiagBuffer::sm_PostSeverityChange = eDiagSC_Unknown;
                                                  // to be set on first request

TDiagPostFlags CDiagBuffer::sm_PostFlags          =
    eDPF_Prefix | eDPF_Severity | eDPF_ErrCode | eDPF_ErrSubCode | 
    eDPF_ErrCodeMessage | eDPF_ErrCodeExplanation | eDPF_ErrCodeUseSeverity;
TDiagPostFlags CDiagBuffer::sm_TraceFlags         = eDPF_Trace;

EDiagSev       CDiagBuffer::sm_DieSeverity        = eDiag_Fatal;

EDiagTrace     CDiagBuffer::sm_TraceDefault       = eDT_Default;
bool           CDiagBuffer::sm_TraceEnabled;     // to be set on first request


const char*    CDiagBuffer::sm_SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Critical", "Fatal", "Trace" };

// Use s_DefaultHandler only for purposes of comparison, as installing
// another handler will normally delete it.
CDiagHandler*      s_DefaultHandler = new CStreamDiagHandler(&NcbiCerr);
CDiagHandler*      CDiagBuffer::sm_Handler = s_DefaultHandler;
bool               CDiagBuffer::sm_CanDeleteHandler = true;
CDiagErrCodeInfo*  CDiagBuffer::sm_ErrCodeInfo = 0;
bool               CDiagBuffer::sm_CanDeleteErrCodeInfo = false;


CDiagBuffer::CDiagBuffer(void)
    : m_Stream(new CNcbiOstrstream)
{
    m_Diag = 0;
}

CDiagBuffer::~CDiagBuffer(void)
{
#if (_DEBUG > 1)
    if (m_Diag  ||  dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount())
        Abort();
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

    // Check severity level change status
    if ( sm_PostSeverityChange == eDiagSC_Unknown ) {
        GetSeverityChangeEnabledFirstTime();
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
        size_t size = ostr->pcount();
        ostr->rdbuf()->freeze(false);
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
            DiagHandler(mess);
        }

#if defined(NCBI_COMPILER_KCC)
        // KCC's implementation of "freeze(false)" makes the ostrstream buffer
        // stuck.  We need to replace the frozen stream with the new one.
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
        Abort();
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


bool CDiagBuffer::GetSeverityChangeEnabledFirstTime(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    if ( sm_PostSeverityChange != eDiagSC_Unknown ) {
        return sm_PostSeverityChange == eDiagSC_Enable;
    }
    const char* str = ::getenv(DIAG_POST_LEVEL);
    EDiagSev sev;
    if (str  &&  *str  &&  CNcbiDiag::StrToSeverityLevel(str, sev)) {
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
        string src, dest;
        x_Write(ostr, fNoEndl);
        ostr.put('\0');
        src = ostr.str();
        ostr.rdbuf()->freeze(false);
        NStr::Replace(NStr::Replace(src,"\r",""),"\n","", dest);
        os << dest;
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
        NStr::strftime(datetime, sizeof(datetime), timefmt, tm);
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
        (m_Severity != eDiag_Info || !IsSetDiagPostFlag(eDPF_OmitInfoSev)))
        os << CNcbiDiag::SeverityName(m_Severity) << ": ";

    // (<err_code>.<err_subcode>) or (err_text)
    if ((m_ErrCode  ||  m_ErrSubCode || m_ErrText)  &&
        IsSetDiagPostFlag(eDPF_ErrCode, m_Flags)) {
        os << '(';
        if (m_ErrText) {
            os << m_ErrText;
        } else {
            os << m_ErrCode;
            if ( IsSetDiagPostFlag(eDPF_ErrSubCode, m_Flags)) {
                os << '.' << m_ErrSubCode;
            }
        }
        os << ") ";
    }

    // Module::Class::Function -
    bool print_location =
        ((m_Module    &&  *m_Module) ||
         (m_Class     &&  *m_Class ) ||
         (m_Function  &&  *m_Function))
        && IsSetDiagPostFlag(eDPF_Location, m_Flags);

    if (print_location) {
        // Module:: Module::Class Module::Class::Function()
        // ::Class ::Class::Function()
        // Module::Function() Function()
        bool need_double_colon = false;

        if (m_Module  &&  *m_Module) {
            os << m_Module;
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

        os << " - ";
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if (m_BufferLen)
        os.write(m_Buffer, m_BufferLen);

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
    CMutexGuard LOCK(s_DiagMutex);

    TDiagPostFlags prev_flags = flags;
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

    CMutexGuard LOCK(s_DiagMutex);
    flags |= flag;
}


static void s_UnsetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CMutexGuard LOCK(s_DiagMutex);
    flags &= ~flag;
}


extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags)
{
    return s_SetDiagPostAllFlags(CDiagBuffer::sm_PostFlags, flags);
}

extern void SetDiagPostFlag(EDiagPostFlag flag)
{
    s_SetDiagPostFlag(CDiagBuffer::sm_PostFlags, flag);
}

extern void UnsetDiagPostFlag(EDiagPostFlag flag)
{
    s_UnsetDiagPostFlag(CDiagBuffer::sm_PostFlags, flag);
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

    CMutexGuard LOCK(s_DiagMutex);
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


extern void SetDiagFixedPostLevel(const EDiagSev post_sev)
{
    SetDiagPostLevel(post_sev);
    DisableDiagPostLevelChange();
}


extern bool DisableDiagPostLevelChange(bool disable_change)
{
    CMutexGuard LOCK(s_DiagMutex);
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

    CMutexGuard LOCK(s_DiagMutex);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    return sev;
}


extern void IgnoreDiagDieLevel(bool ignore, EDiagSev* prev_sev)
{
    if (!!ignore != !!prev_sev) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "IgnoreDiagDieLevel() -- Illegal 'ignore'/'prev_sev' "
                   "combination");
    }

    CMutexGuard LOCK(s_DiagMutex);
    if ( ignore ) {
        *prev_sev = CDiagBuffer::sm_DieSeverity;
        CDiagBuffer::sm_DieSeverity = eDiag_Trace;
    } else {
        CDiagBuffer::sm_DieSeverity = eDiag_Fatal;
        CNcbiDiag diag(eDiag_Info);
    }
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


extern void SetDiagErrCodeInfo(CDiagErrCodeInfo* info, bool can_delete)
{
    CMutexGuard LOCK(s_DiagMutex);
    if ( CDiagBuffer::sm_CanDeleteErrCodeInfo )
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
    CMutexGuard LOCK(s_DiagMutex);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteErrCodeInfo);
        CDiagBuffer::sm_CanDeleteErrCodeInfo = false;
    }
    return CDiagBuffer::sm_ErrCodeInfo;
}


extern void SetDiagFilter(EDiagFilter what, const char* filter_str)
{
    CMutexGuard LOCK(s_DiagMutex);
    if (what == eDiagFilter_Trace  ||  what == eDiagFilter_All) 
        s_TraceFilter.Fill(filter_str);

    if (what == eDiagFilter_Post  ||  what == eDiagFilter_All) 
        s_PostFilter .Fill(filter_str);
}


static
bool s_CheckDiagFilter(const CException& ex, EDiagSev sev, const char* file)
{
    if (sev == eDiag_Fatal) 
        return true;

    CMutexGuard LOCK(s_DiagMutex);

    // check for trace filter
    if (sev == eDiag_Trace) {
        EDiagFilterAction action = s_TraceFilter.CheckFile(file);
        if(action == eDiagFilter_None)
            return s_TraceFilter.Check(ex, sev) == eDiagFilter_Accept;
        return action == eDiagFilter_Accept;
    }

    // check for post filter
    EDiagFilterAction action = s_PostFilter.CheckFile(file);
    if (action == eDiagFilter_None) {
        action = s_PostFilter.Check(ex, sev);
    }

    return (action == eDiagFilter_Accept);
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), 
      m_ErrCode(0), 
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), 
      m_PostFlags(post_flags),
      m_CheckFilters(true),
      m_Line(0),
      m_ValChngFlags(0)
{
}


CNcbiDiag::CNcbiDiag(const CDiagCompileInfo &info,
                     EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), 
      m_ErrCode(0), 
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), 
      m_PostFlags(post_flags),
      m_CheckFilters(true),
      m_CompileInfo(info),
      m_Line(info.GetLine()),
      m_ValChngFlags(0)
{
    SetFile(   info.GetFile()   );
    SetModule( info.GetModule() );
}

CNcbiDiag::~CNcbiDiag(void) 
{
    m_Buffer.Detach(this);
}

const CNcbiDiag& CNcbiDiag::SetFile(const char* file) const
{
    m_File = file;
    m_ValChngFlags |= fFileIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetModule(const char* module) const
{
    m_Module = module;
    m_ValChngFlags |= fModuleIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetClass(const char* nclass ) const
{
    m_Class = nclass;
    m_ValChngFlags |= fClassIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetFunction(const char* function) const
{
    m_Function = function;
    m_ValChngFlags |= fFunctionIsChanged;
    return *this;
}


bool CNcbiDiag::CheckFilters(void) const
{
    if ( !m_CheckFilters ) {
        m_CheckFilters = true;
        return true;
    }
    EDiagSev current_sev = GetSeverity();
    if (current_sev == eDiag_Fatal) 
        return true;

    CMutexGuard LOCK(s_DiagMutex);
    if (GetSeverity() == eDiag_Trace) {
        // check for trace filter
        return 
        s_TraceFilter.Check(*this, this->GetSeverity()) != eDiagFilter_Reject;
    }
    
    // check for post filter and severity
    return 
     (s_PostFilter.Check(*this, this->GetSeverity()) != eDiagFilter_Reject);
}



const CNcbiDiag& CNcbiDiag::operator<< (const CException& ex) const
{
    if ( !s_CheckDiagFilter(ex, GetSeverity(), GetFile()) ) {
        m_Buffer.Reset(*this);
        return *this;
    }

    m_CheckFilters = false;

    {
        ostrstream os;
        os << '\n' << "NCBI C++ Exception:" << '\n';
        *this << string(CNcbiOstrstreamToString(os));
    }
    const CException* pex;
    stack<const CException*> pile;
    // invert the order
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        string text(pex->GetMsg());
        {
            ostrstream os;
            pex->ReportExtra(os);
            if (os.pcount() != 0) {
                text += " (";
                text += (string)CNcbiOstrstreamToString(os);
                text += ')';
            }
        }
        string err_type(pex->GetType());
        err_type += "::";
        err_type += pex->GetErrCodeString();
        SDiagMessage diagmsg(eDiag_Error, 
                             text.c_str(), 
                             text.size(),
                             pex->GetFile().c_str(),
                             pex->GetLine(),
                             GetPostFlags(),
                             NULL,
                             0,
                             0,
                             err_type.c_str(),
                             pex->GetModule().c_str(),
                             pex->GetClass().c_str(),
                             pex->GetFunction().c_str());
        string report;
        diagmsg.Write(report);
        *this << "    "; // indentation
        *this << report;
    }
    return *this;
}


bool CNcbiDiag::StrToSeverityLevel(const char* str_sev, EDiagSev& sev)
{
    if (!str_sev || !*str_sev) {
        return false;
    } 
    // Digital value
    int nsev = NStr::StringToNumeric(str_sev);

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
    return sev >= eDiagSevMin && sev <= eDiagSevMax;
}

void CNcbiDiag::DiagFatal(const CDiagCompileInfo& info,
                          const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal) << message << Endm;
}

void CNcbiDiag::DiagTrouble(const CDiagCompileInfo& info)
{
    DiagFatal(info, "Trouble!");
}

void CNcbiDiag::DiagAssert(const CDiagCompileInfo& info,
                           const char* expression)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal, eDPF_Trace) <<
        "Assertion failed: (" << expression << ')' << Endm;
}

void CNcbiDiag::DiagValidate(const CDiagCompileInfo& info,
                             const char* _DEBUG_ARG(expression),
                             const char* message)
{
#ifdef _DEBUG
    if ( xncbi_GetValidateAction() != eValidate_Throw ) {
        DiagAssert(info, expression);
    }
#endif
    throw CCoreException(info, 0, CCoreException::eCore, message);
}

///////////////////////////////////////////////////////
//  CDiagRestorer::

CDiagRestorer::CDiagRestorer(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    const CDiagBuffer& buf  = GetDiagBuffer();
    m_PostPrefix            = buf.m_PostPrefix;
    m_PrefixList            = buf.m_PrefixList;
    m_PostFlags             = buf.sm_PostFlags;
    m_PostSeverity          = buf.sm_PostSeverity;
    m_PostSeverityChange    = buf.sm_PostSeverityChange;
    m_DieSeverity           = buf.sm_DieSeverity;
    m_TraceDefault          = buf.sm_TraceDefault;
    m_TraceEnabled          = buf.sm_TraceEnabled;
    m_Handler               = buf.sm_Handler;
    m_CanDeleteHandler      = buf.sm_CanDeleteHandler;
    m_ErrCodeInfo           = buf.sm_ErrCodeInfo;
    m_CanDeleteErrCodeInfo  = buf.sm_CanDeleteErrCodeInfo;
    // avoid premature cleanup
    buf.sm_CanDeleteHandler = false;
    buf.sm_CanDeleteErrCodeInfo = false;
}

CDiagRestorer::~CDiagRestorer(void)
{
    {{
        CMutexGuard LOCK(s_DiagMutex);
        CDiagBuffer& buf          = GetDiagBuffer();
        buf.m_PostPrefix          = m_PostPrefix;
        buf.m_PrefixList          = m_PrefixList;
        buf.sm_PostFlags          = m_PostFlags;
        buf.sm_PostSeverity       = m_PostSeverity;
        buf.sm_PostSeverityChange = m_PostSeverityChange;
        buf.sm_DieSeverity        = m_DieSeverity;
        buf.sm_TraceDefault       = m_TraceDefault;
        buf.sm_TraceEnabled       = m_TraceEnabled;
    }}
    SetDiagHandler(m_Handler, m_CanDeleteHandler);
    SetDiagErrCodeInfo(m_ErrCodeInfo, m_CanDeleteErrCodeInfo);
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
    const char* value = getenv("DIAG_SILENT_ABORT");
    if (value  &&  (*value == 'Y'  ||  *value == 'y'  ||  *value == '1')) {
        ::exit(255);
    }
    else if (value  &&  (*value == 'N'  ||  *value == 'n' || *value == '0')) {
        ::abort();
    }
    else
#define NCBI_TOTALVIEW_ABORT_WORKAROUND 1
#if defined(NCBI_TOTALVIEW_ABORT_WORKAROUND)
        // The condition in the following if statement is always 'true'.
        // It's a workaround for TotalView 6.5 (beta) to properly display
        // stacktrace at this point.
        if ( !(value && *value == 'Y') )
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
            ERR_POST("Error message file parsing: Incorrect file format " \
                     ", line " + NStr::IntToString(line));
            return false;
        }
        // Mnemonic name (skip)
        tokens.pop_front();

        // Error code
        string token = NStr::TruncateSpaces(tokens.front());
        tokens.pop_front();
        x_code = NStr::StringToInt(token);

        // Severity
        if (!tokens.empty()) { 
            token = NStr::TruncateSpaces(tokens.front());
            EDiagSev sev;
            if (CNcbiDiag::StrToSeverityLevel(token.c_str(), sev)) {
                x_severity = sev;
            } else {
                ERR_POST(Warning << "Error message file parsing: " \
                         "Incorrect severity level in the verbose " \
                         "message file, line " + NStr::IntToString(line));
            }
        } else {
            x_severity = -1;
        }
    }
    catch (CException& e) {
        ERR_POST(Warning << "Error message file parsing: " << e.GetMsg() <<
                 ", line " + NStr::IntToString(line));
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
            SDiagErrCodeDescription(err_message, err_text,
                                    err_subseverity));
    }
    return true;
}


bool CDiagErrCodeInfo::GetDescription(const ErrCode& err_code, 
                      SDiagErrCodeDescription* description) const
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


END_NCBI_SCOPE



/*
 * ==========================================================================
 * $Log$
 * Revision 1.93  2005/05/14 20:57:20  vakatov
 * SetDiagPostLevel() -- Special case:  eDiag_Trace to print all messages
 * and turn on the tracing
 *
 * Revision 1.92  2005/05/05 00:11:22  vakatov
 * Added missing 'const.
 * Plus, some cosmetics.
 *
 * Revision 1.91  2005/05/04 19:53:42  ssikorsk
 * Store internal data in std::string instead of AutoPtr within CDiagCompileInfo and CNcbiDiag. Optimized module name parsing and checking algorithm.
 *
 * Revision 1.90  2005/05/04 15:27:54  ssikorsk
 * Initialize m_File and m_Module with empty strins.
 *
 * Revision 1.89  2005/05/04 14:13:19  ssikorsk
 * Made CNcbiDiag dtor not inline. Added copy ctor and dtor to CDiagCompileInfo.
 *
 * Revision 1.88  2005/05/04 13:19:18  ssikorsk
 * Revamped CDiagCompileInfo and CNcbiDiag to use dynamically allocated buffers instead of predefined
 *
 * Revision 1.87  2005/04/14 20:25:16  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.86  2004/12/13 14:38:32  kuznets
 * Implemented severity filtering
 *
 * Revision 1.85  2004/10/21 15:27:51  vakatov
 * CDiagBuffer::Flush(), eDPF_PreMergeLines -- fixed incorrect construction
 * of C++ string from not-NUL-terminated C string (reported by V.Chetvernin)
 *
 * Revision 1.84  2004/10/05 16:42:21  shomrat
 * rollback last change
 *
 * Revision 1.83  2004/10/05 16:13:57  shomrat
 * Use in place NStr::TruncateSpaces
 *
 * Revision 1.82  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.81  2004/06/30 17:17:58  vasilche
 * Workaround for TotalView 6.5 (beta).
 *
 * Revision 1.80  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.79  2004/03/18 20:19:20  gouriano
 * make it possible to convert multi-line diagnostic message into single-line
 *
 * Revision 1.78  2003/11/12 20:30:26  ucko
 * Make extra flags for severity-trace messages tunable.
 *
 * Revision 1.77  2003/11/06 21:40:56  vakatov
 * A somewhat more natural handling of the 'eDPF_Default' flag -- replace
 * it by the current global flags, then merge these with other flags (if any)
 *
 * Revision 1.76  2003/10/31 19:38:53  lavr
 * No '\0' in exception reporting
 *
 * Revision 1.75  2003/09/26 15:54:13  vakatov
 * CNcbiDiag::DiagAssert() -- print everything (trace-like -- file, line, etc.)
 * in the message
 *
 * Revision 1.74  2003/09/17 15:58:29  vasilche
 * Allow debug abort when:
 * CObjectException is thrown - env var NCBI_ABORT_ON_COBJECT_THROW=[Yy1],
 * CNullPointerException is thrown - env var NCBI_ABORT_ON_NULL=[Yy1].
 * Allow quit abort in debug mode and coredump in release mode:
 * env var DIAG_SILENT_ABORT=[Yy1Nn0].
 *
 * Revision 1.73  2003/05/19 21:12:46  vakatov
 * CNcbiDiag::DiagValidate() -- get rid of "unused func arg" compilation warning
 *
 * Revision 1.72  2003/04/25 20:54:15  lavr
 * Introduce draft version of IgnoreDiagDieLevel()
 *
 * Revision 1.71  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.70  2003/02/21 21:08:57  vakatov
 * Minor cast to get rid of 64-bit compilation warning
 *
 * Revision 1.69  2003/01/13 20:42:50  gouriano
 * corrected the problem with ostrstream::str(): replaced such calls with
 * CNcbiOstrstreamToString(os)
 *
 * Revision 1.68  2002/09/30 16:35:16  vasilche
 * Restored mutex lock on fork().
 *
 * Revision 1.67  2002/09/24 18:28:20  vasilche
 * Fixed behavour of CNcbiDiag::DiagValidate() in release mode
 *
 * Revision 1.66  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.65  2002/08/20 19:10:39  gouriano
 * added DiagWriteFlags into SDiagMessage::Write
 *
 * Revision 1.64  2002/08/16 15:02:11  ivanov
 * Added class CDiagAutoPrefix
 *
 * Revision 1.63  2002/08/01 18:47:43  ivanov
 * Added stuff to store and output error verbose messages for error codes
 *
 * Revision 1.62  2002/07/25 15:46:08  ivanov
 * Rollback R1.60
 *
 * Revision 1.61  2002/07/25 13:35:05  ivanov
 * Changed exit code of a faild test
 *
 * Revision 1.60  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.59  2002/07/10 16:19:00  ivanov
 * Added CNcbiDiag::StrToSeverityLevel().
 * Rewrite and rename SetDiagFixedStrPostLevel() -> SetDiagFixedPostLevel()
 *
 * Revision 1.58  2002/07/09 16:37:11  ivanov
 * Added GetSeverityChangeEnabledFirstTime().
 * Fix usage forced set severity post level from environment variable
 * to work without NcbiApplication::AppMain()
 *
 * Revision 1.57  2002/07/02 18:26:22  ivanov
 * Added CDiagBuffer::DisableDiagPostLevelChange()
 *
 * Revision 1.56  2002/06/27 18:56:16  gouriano
 * added "title" parameter to report functions
 *
 * Revision 1.55  2002/06/26 18:38:04  gouriano
 * added CNcbiException class
 *
 * Revision 1.54  2002/06/18 17:07:12  lavr
 * Take advantage of NStr:strftime()
 *
 * Revision 1.53  2002/05/14 16:47:27  ucko
 * Conditionalize usage of pthread_atfork, which doesn't seem to exist at
 * all on FreeBSD.
 *
 * Revision 1.52  2002/05/03 14:29:17  ucko
 * #include <unistd.h> for pthread_atfork(); the other headers do not
 * necessarily already include it.
 *
 * Revision 1.51  2002/04/25 21:49:05  ucko
 * Made pthread_atfork callbacks extern "C".
 *
 * Revision 1.50  2002/04/25 21:29:39  ucko
 * At Yan Raytselis's suggestion, use pthread_atfork to avoid
 * inadvertently holding s_DiagMutex across fork.
 *
 * Revision 1.49  2002/04/23 19:57:29  vakatov
 * Made the whole CNcbiDiag class "mutable" -- it helps eliminate
 * numerous warnings issued by SUN Forte6U2 compiler.
 * Do not use #NO_INCLASS_TMPL anymore -- apparently all modern
 * compilers seem to be supporting in-class template methods.
 *
 * Revision 1.48  2002/04/16 18:48:42  ivanov
 * SuppressDiagPopupMessages() moved to "test/test_assert.h"
 *
 * Revision 1.47  2002/04/11 19:58:34  ivanov
 * Added function SuppressDiagPopupMessages()
 *
 * Revision 1.46  2002/04/10 14:45:27  ivanov
 * Abort() moved from static to extern and added to header file
 *
 * Revision 1.45  2002/04/01 22:35:22  ivanov
 * Added SetAbortHandler() function to set user abort handler.
 * Used call internal function Abort() vice ::abort().
 *
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
 *
 * ==========================================================================
 */
