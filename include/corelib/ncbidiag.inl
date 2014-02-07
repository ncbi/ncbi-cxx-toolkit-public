#if defined(CORELIB___NCBIDIAG__HPP)  &&  !defined(CORELIB___NCBIDIAG__INL)
#define CORELIB___NCBIDIAG__INL

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
 */


/////////////////////////////////////////////////////////////////////////////
// WARNING -- all the beneath is for INTERNAL "ncbidiag" use only,
//            and any classes, typedefs and even "extern" functions and
//            variables declared in this file should not be used anywhere
//            but inside "ncbidiag.inl" and/or "ncbidiag.cpp"!!!
/////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
// CDiagBuffer
// (can be accessed only by "CNcbiDiag" and "CDiagRestorer"
// and created only by GetDiagBuffer())
//


class CDiagBuffer
{
    CDiagBuffer(const CDiagBuffer&);
    CDiagBuffer& operator= (const CDiagBuffer&);

    friend class CDiagContextThreadData;
    friend class CDiagContext_Extra;

    // Flags
    friend bool IsSetDiagPostFlag(EDiagPostFlag flag, TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT
    friend TDiagPostFlags         SetDiagPostAllFlags(TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT friend void SetDiagPostFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void UnsetDiagPostFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT
    friend TDiagPostFlags         SetDiagTraceAllFlags(TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT friend void SetDiagTraceFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void UnsetDiagTraceFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void SetDiagPostPrefix(const char* prefix);
    NCBI_XNCBI_EXPORT friend void PushDiagPostPrefix(const char* prefix);
    NCBI_XNCBI_EXPORT friend void PopDiagPostPrefix(void);

    //
    friend class CNcbiDiag;
    friend const CNcbiDiag& Reset(const CNcbiDiag& diag);
    friend const CNcbiDiag& Endm(const CNcbiDiag& diag);

    // Severity
    NCBI_XNCBI_EXPORT
    friend EDiagSev SetDiagPostLevel(EDiagSev post_sev);
    NCBI_XNCBI_EXPORT
    friend bool IsVisibleDiagPostLevel(EDiagSev sev);
    NCBI_XNCBI_EXPORT
    friend void SetDiagFixedPostLevel(EDiagSev post_sev);
    NCBI_XNCBI_EXPORT
    friend bool DisableDiagPostLevelChange(bool disable_change);
    NCBI_XNCBI_EXPORT
    friend EDiagSev SetDiagDieLevel(EDiagSev die_sev);
    NCBI_XNCBI_EXPORT
    friend EDiagSev GetDiagDieLevel(void);
    NCBI_XNCBI_EXPORT
    friend bool IgnoreDiagDieLevel(bool ignore);

    // Others
    NCBI_XNCBI_EXPORT
    friend void SetDiagTrace(EDiagTrace how, EDiagTrace dflt);
    NCBI_XNCBI_EXPORT friend bool IsDiagStream(const CNcbiOstream* os);

    // Handler
    NCBI_XNCBI_EXPORT friend void
    SetDiagHandler(CDiagHandler* handler, bool can_delete);
    NCBI_XNCBI_EXPORT friend CDiagHandler* GetDiagHandler(bool take_ownership);
    NCBI_XNCBI_EXPORT friend bool IsSetDiagHandler(void);

    // Error code information
    NCBI_XNCBI_EXPORT
    friend void SetDiagErrCodeInfo(CDiagErrCodeInfo* info, bool can_delete);
    NCBI_XNCBI_EXPORT
    friend CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership);
    NCBI_XNCBI_EXPORT
    friend bool IsSetDiagErrCodeInfo(void);

private:
    friend class CDiagRestorer;
    friend class CDiagContext;
    friend class CDiagCollectGuard;

    const CNcbiDiag*   m_Diag;    // present user
    CNcbiOstrstream*   m_Stream;  // storage for the diagnostic message
    IOS_BASE::fmtflags m_InitialStreamFlags;
    bool               m_InUse;   // Protection against nested posts

    // user-specified string to add to each posted message
    // (can be constructed from "m_PrefixList" after push/pop operations)
    string m_PostPrefix;

    // list of prefix strings to compose the "m_PostPrefix" from
    typedef list<string> TPrefixList;
    TPrefixList m_PrefixList;

    CDiagBuffer(void);

    //### This is a temporary workaround to allow call the destructor of
    //### static instance of "CDiagBuffer" defined in GetDiagBuffer()
public:
    ~CDiagBuffer(void);
private:
    //###

    // formatted output
    template<class X> void Put(const CNcbiDiag& diag, const X& x) {
        if ( SetDiag(diag) )
            (*m_Stream) << x;
    }

    NCBI_XNCBI_EXPORT
    void Flush  (void);
    void PrintMessage(SDiagMessage& mess, const CNcbiDiag& diag);
    void Reset  (const CNcbiDiag& diag);   // reset content of the diag.message
    void EndMess(const CNcbiDiag& diag);   // output current diag. message
    NCBI_XNCBI_EXPORT
    bool SetDiag(const CNcbiDiag& diag);

    // flush & detach the current user
    void Detach(const CNcbiDiag* diag);

    // compose the post prefix using "m_PrefixList"
    void UpdatePrefix(void);

    // the bitwise OR combination of "EDiagPostFlag"
    // Hidden inside the function to adjust default flags depending on
    // registry/environment.
    // inline version
    static TDiagPostFlags& sx_GetPostFlags(void);
    // non-inline version
    NCBI_XNCBI_EXPORT
    static TDiagPostFlags& s_GetPostFlags(void);
    // extra flags ORed in for traces
    static TDiagPostFlags sm_TraceFlags;

    // static members
    static EDiagSev       sm_PostSeverity;
    static EDiagSevChange sm_PostSeverityChange;
                                           // severity level changing status
    static bool           sm_IgnoreToDie;
    static EDiagSev       sm_DieSeverity;
    static EDiagTrace     sm_TraceDefault; // default state of tracing
    static bool           sm_TraceEnabled; // current state of tracing
                                           // (enable/disable)

    static bool GetTraceEnabled(void);     // dont access sm_TraceEnabled 
                                           // directly
    static bool GetTraceEnabledFirstTime(void);
    static bool GetSeverityChangeEnabledFirstTime(void);
    // Anything not disabled but also not printable is collectable.
    static bool SeverityDisabled(EDiagSev sev);
    static bool SeverityPrintable(EDiagSev sev);

    // call the current diagnostics handler directly
    static void DiagHandler(SDiagMessage& mess);

    // Symbolic name for the severity levels(used by CNcbiDiag::SeverityName)
    NCBI_XNCBI_EXPORT
    static const char* sm_SeverityName[eDiag_Trace+1];

    // Application-wide diagnostic handler
    static CDiagHandler* sm_Handler;
    static bool          sm_CanDeleteHandler;

    // Error codes info
    static CDiagErrCodeInfo* sm_ErrCodeInfo;
    static bool              sm_CanDeleteErrCodeInfo;

    friend NCBI_XNCBI_EXPORT
        CDiagContext_Extra g_PostPerf(int                       status,
                                      double                    timespan,
                                      SDiagMessage::TExtraArgs& args);
};

extern CDiagBuffer& GetDiagBuffer(void);


///////////////////////////////////////////////////////
//  CDiagCompileInfo

inline const char* CDiagCompileInfo::GetFile (void) const
{
    return m_File;
}

inline const char* CDiagCompileInfo::GetModule(void) const
{
    return m_Module;
}

inline int CDiagCompileInfo::GetLine(void) const
{
    return m_Line;
}

inline const string& CDiagCompileInfo::GetClass(void) const
{
    if (!m_ClassSet  &&  !m_Parsed) {
        ParseCurrFunctName();
    }
    return m_ClassName;
}

inline const string& CDiagCompileInfo::GetFunction(void) const
{
    if (!m_Parsed) {
        ParseCurrFunctName();
    }
    return m_FunctName;
}


///////////////////////////////////////////////////////
//  CNcbiDiag::

#ifdef NCBIDIAG_DEFER_GENERIC_PUT
template<class X>
inline
const CNcbiDiag& CNcbiDiag::Put(const void*, const X& x) const
{
    m_Buffer.Put(*this, x);
    return *this;
}
#endif


inline const CNcbiDiag& CNcbiDiag::operator<< (FIosbaseManip manip) const
{
    m_Buffer.Put(*this, manip);
    return *this;
}

inline const CNcbiDiag& CNcbiDiag::operator<< (FIosManip manip) const
{
    m_Buffer.Put(*this, manip);
    return *this;
}


inline const CNcbiDiag& CNcbiDiag::SetLine(size_t line) const {
    m_CompileInfo.SetLine((int)line);
    return *this;
}

inline const CNcbiDiag& CNcbiDiag::SetErrorCode(int code, int subcode) const {
    m_ErrCode = code;
    m_ErrSubCode = subcode;
    return *this;
}

inline EDiagSev CNcbiDiag::GetSeverity(void) const {
    return m_Severity;
}

inline const char* CNcbiDiag::GetModule(void) const 
{ 
    return m_CompileInfo.GetModule();
}

inline const char* CNcbiDiag::GetFile(void) const 
{ 
    return m_CompileInfo.GetFile();
}

inline const char* CNcbiDiag::GetClass(void) const 
{ 
    return m_CompileInfo.GetClass().c_str();
}

inline const char* CNcbiDiag::GetFunction(void) const 
{ 
    return m_CompileInfo.GetFunction().c_str();
}

inline size_t CNcbiDiag::GetLine(void) const {
    return m_CompileInfo.GetLine();
}

inline int CNcbiDiag::GetErrorCode(void) const {
    return m_ErrCode;
}

inline int CNcbiDiag::GetErrorSubCode(void) const {
    return m_ErrSubCode;
}

inline TDiagPostFlags CNcbiDiag::GetPostFlags(void) const {
    return (m_PostFlags & eDPF_Default) ?
        (m_PostFlags | CDiagBuffer::s_GetPostFlags()) & ~eDPF_Default :
        m_PostFlags;
}


inline
const char* CNcbiDiag::SeverityName(EDiagSev sev) {
    return CDiagBuffer::sm_SeverityName[sev];
}


///////////////////////////////////////////////////////
//  ErrCode - class for manipulator ErrCode

inline
void CNcbiDiag::x_EndMess(void) const
{
    m_Buffer.EndMess(*this);
}

inline
const CNcbiDiag& CNcbiDiag::Put(const ErrCode*, const ErrCode& err_code) const
{
    x_EndMess();
    return SetErrorCode(err_code.m_Code, err_code.m_SubCode);
}

inline
bool operator< (const ErrCode& ec1, const ErrCode& ec2)
{
    return (ec1.m_Code == ec2.m_Code)
        ? (ec1.m_SubCode < ec2.m_SubCode)
        : (ec1.m_Code < ec2.m_Code);
}


///////////////////////////////////////////////////////
//  Other CNcbiDiag:: manipulators

inline
const CNcbiDiag& CNcbiDiag::Put(const Severity*,
                                const Severity& severity) const
{
    x_EndMess();
    m_Severity = severity.m_Level;
    return *this;
}

inline
const CNcbiDiag& Reset(const CNcbiDiag& diag)  {
    diag.m_Buffer.Reset(diag);
    diag.ResetIsMessageFlag();
    diag.ResetIsConsoleFlag();
    diag.SetErrorCode(0, 0);
    return diag;
}

inline
const CNcbiDiag& Endm(const CNcbiDiag& diag)  {
    if ( !diag.m_Buffer.m_Diag
        && (diag.GetErrorCode() || diag.GetErrorSubCode()) ) {
        diag.m_Buffer.SetDiag(diag);
    }
    diag.m_Buffer.EndMess(diag);
    return diag;
}

inline
const CNcbiDiag& Info(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Info;
    return diag;
}
inline
const CNcbiDiag& Warning(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Warning;
    return diag;
}
inline
const CNcbiDiag& Error(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Error;
    return diag;
}
inline
const CNcbiDiag& Critical(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Critical;
    return diag;
}
inline
const CNcbiDiag& Fatal(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Fatal;
    return diag;
}
inline
const CNcbiDiag& Trace(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Trace;
    return diag;
}
inline
const CNcbiDiag& Message(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_PostFlags |= eDPF_IsMessage;
    return diag;
}
inline
const CNcbiDiag& Console(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_PostFlags |= eDPF_IsConsole;
    return diag;
}

inline
const CNcbiDiag& StackTrace (const CNcbiDiag& diag) {
    CStackTrace stk;
    diag.Put(NULL,stk);
    return diag;
}



///////////////////////////////////////////////////////
//  CDiagBuffer::

inline
void CDiagBuffer::Reset(const CNcbiDiag& diag) {
    if (&diag == m_Diag) {
        m_Stream->rdbuf()->PUBSEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
    }
}

inline
void CDiagBuffer::EndMess(const CNcbiDiag& diag) {
    if (&diag == m_Diag) {
        // Flush();
        Detach(&diag);
        diag.SetErrorCode(0, 0);
    }
}

inline
void CDiagBuffer::Detach(const CNcbiDiag* diag) {
    if (diag == m_Diag) {
        Flush();
        m_Diag = 0;
    }
}

inline
bool CDiagBuffer::GetTraceEnabled(void) {
    return (sm_TraceDefault == eDT_Default) ?
        GetTraceEnabledFirstTime() : sm_TraceEnabled;
}


///////////////////////////////////////////////////////
//  EDiagPostFlag::

inline
bool IsSetDiagPostFlag(EDiagPostFlag flag, TDiagPostFlags flags) {
    if (flag == eDPF_AtomicWrite) return true;
    if (flags & eDPF_Default)
        flags |= CDiagBuffer::s_GetPostFlags();
    return (flags & flag) != 0;
}


///////////////////////////////////////////////////////
//  CDiagErrCodeInfo::


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(void)
{
    return;
}


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(const string& file_name)
{
    if ( !Read(file_name) ) {
        throw runtime_error
            ("CDiagErrCodeInfo::  failed to read error descriptions from file "
             + file_name);
    }
}


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(CNcbiIstream& is)
{
    if ( !Read(is) ) {
        throw runtime_error
            ("CDiagErrCodeInfo::  failed to read error descriptions");
    }
}


inline
CDiagErrCodeInfo::~CDiagErrCodeInfo(void)
{
    Clear();
}


inline
void CDiagErrCodeInfo::Clear(void)
{
    m_Info.clear();
}


inline
void CDiagErrCodeInfo::SetDescription
(const ErrCode&                 err_code, 
 const SDiagErrCodeDescription& description)
{
    m_Info[err_code] = description;
}


inline
bool CDiagErrCodeInfo::HaveDescription(const ErrCode& err_code) const
{
    return m_Info.find(err_code) != m_Info.end();
}


/////////////////////////////////////////////////////////////////////////////
/// MDiagModuleCpp::

/*inline
const CNcbiDiag& operator<< (const CNcbiDiag&      diag,
                             const MDiagModuleCpp& module)
{
    if(module.m_Module)
        diag.SetModule(module.m_Module);
    return diag;
    }*/


/////////////////////////////////////////////////////////////////////////////
/// MDiagModule::

inline
MDiagModule::MDiagModule(const char* module)
    : m_Module(module)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagModule& module)
{
    return diag.SetModule(module.m_Module);
}



/////////////////////////////////////////////////////////////////////////////
/// MDiagClass::

inline
MDiagClass::MDiagClass(const char* nclass)
    : m_Class(nclass)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagClass& nclass)
{
    return diag.SetClass(nclass.m_Class);
}



/////////////////////////////////////////////////////////////////////////////
/// MDiagFunction::

inline
MDiagFunction::MDiagFunction(const char* function)
    : m_Function(function)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagFunction& function)
{
    return diag.SetFunction(function.m_Function);
}


// The function should not be called directly, use performance guard instead.
// The 'args' list elements will be moved to the CDiagContext_Extra.
NCBI_XNCBI_EXPORT
extern CDiagContext_Extra g_PostPerf(int                       status,
                                     double                    timespan,
                                     SDiagMessage::TExtraArgs& args);

#endif /* def CORELIB___NCBIDIAG__HPP  &&  ndef CORELIB___NCBIDIAG__INL */
