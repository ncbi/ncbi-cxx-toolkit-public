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
 * Authors:  Aleksey Grichenko, Denis Vakatov
 *
 * File Description:
 *   Request context for diagnostic framework.
 *
 */


#include <ncbi_pch.hpp>

#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_strings.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbi_mask.hpp>


#define NCBI_USE_ERRCODE_X   Corelib_Diag

BEGIN_NCBI_SCOPE


static const char* kPassThrough_Sid      = "ncbi_sid";
static const char* kPassThrough_Phid     = "ncbi_phid";
static const char* kPassThrough_ClientIp = "ncbi_client_ip";
static const char* kPassThrough_Dtab     = "ncbi_dtab";


CAtomicCounter CRequestContext::sm_VersionCounter;


// What to do on a bad hit ID -- allow-and-report by default

enum EOnBadHitID {
    eOnBadPHID_Allow,
    eOnBadPHID_AllowAndReport,
    eOnBadPHID_Ignore,
    eOnBadPHID_IgnoreAndReport,
    eOnBadPHID_Throw
};

NCBI_PARAM_ENUM_DECL(EOnBadHitID, Log, On_Bad_Hit_Id);
NCBI_PARAM_ENUM_ARRAY(EOnBadHitID, Log, On_Bad_Hit_Id)
{
    {"Allow", eOnBadPHID_Allow},
    {"AllowAndReport", eOnBadPHID_AllowAndReport},
    {"Ignore", eOnBadPHID_Ignore},
    {"IgnoreAndReport", eOnBadPHID_IgnoreAndReport},
    {"Throw", eOnBadPHID_Throw}
};
NCBI_PARAM_ENUM_DEF_EX(EOnBadHitID, Log, On_Bad_Hit_Id, 
                       eOnBadPHID_AllowAndReport,
                       eParam_NoThread,
                       LOG_ON_BAD_HIT_ID);
typedef NCBI_PARAM_TYPE(Log, On_Bad_Hit_Id) TOnBadHitId;


static const char* kAllowedIdMarkchars = "_-.:@";


static bool IsValidHitID(const string& hit) 
{
    string id_std = kAllowedIdMarkchars;
    size_t pos = 0;
    size_t sep_pos = NPOS;
    // Allow aphanumeric and some markup before the first separator (dot).
    for (; pos < hit.size(); pos++) {
        char c = hit[pos];
        if (c == '.') {
            sep_pos = pos;
            break;
        }
        if (!isalnum(c)  &&  id_std.find(c) == NPOS) {
            return false;
        }
    }
    // Hit id must be present before the first separator.
    // Note that empty hit id is still allowed if there are no sub-hit ids.
    if (sep_pos == 0) return false;
    if (sep_pos == NPOS) return true;
    // Separator found - make sure the rest of the id contains only separators
    // and valid sub-hit ids: a prefix consisting of allowed chars and some
    // digits (XYZ.1.J2.T34).
    size_t last_digit = sep_pos;
    for (pos = sep_pos + 1; pos < hit.size(); pos++) {
        char c = hit[pos];
        if (c == '.') {
            // Need at least one char between separators.
            if (pos == sep_pos + 1) return false;
            // Need at least one digit before separator.
            if (last_digit + 1 != pos) return false;
            sep_pos = pos;
            continue;
        }
        if (!isalnum(c)  &&  id_std.find(c) == NPOS) {
            return false;
        }
        if ( isdigit(c) ) last_digit = pos;
    }
    // Make sure the last char is digit.
    return pos > sep_pos + 1;
}


void CSharedHitId::x_SetHitId(const string& hit_id)
{
    if (GetHitId() == hit_id) {
        return;
    }
    if (IsValidHitID(hit_id)) {
        m_HitId = hit_id;
    } else {
        static CSafeStatic<TOnBadHitId> action;
        switch (action->Get()) {
        case eOnBadPHID_Allow:
        case eOnBadPHID_AllowAndReport:
            // Regardless of the allow status lets sanitize bad value, 
            // allow letters, digits and selected marks only, remlace all bad chars with '_'.
            // Resulting PHID still can be invalid, see CXX-12891
            m_HitId = NStr::Sanitize(hit_id, kAllowedIdMarkchars, "", '_', NStr::fSS_alnum | NStr::fSS_NoMerge);
            if (action->Get() == eOnBadPHID_AllowAndReport) {
                ERR_POST_X(27, Warning << "Bad hit ID format: " << NStr::PrintableString(hit_id)
                                       << ", sanitized value will be used: " << m_HitId);
            }
            break;
        case eOnBadPHID_Ignore:
            return;
        case eOnBadPHID_IgnoreAndReport:
            ERR_POST_X(27, Warning << "Bad hit ID format: " << NStr::PrintableString(hit_id));
            return;
        case eOnBadPHID_Throw:
            NCBI_THROW(CRequestContextException, eBadHit, "Bad hit ID format: " + NStr::PrintableString(hit_id));
            break;
        }
    }
}



CRequestContext::CRequestContext(TContextFlags flags)
    : m_RequestID(0),
      m_AppState(eDiagAppState_NotSet),
      m_HitIDLoggedFlag(0),
      m_ReqStatus(0),
      m_ReqTimer(CStopWatch::eStop),
      m_BytesRd(0),
      m_BytesWr(0),
      m_PropSet(0),
      m_IsRunning(false),
      m_AutoIncOnPost(false),
      m_Flags(flags),
      m_DisabledAppLog(CDiagContext::GetDisabledAppLog()),
      m_DisabledAppLogEvents(CDiagContext::GetDisabledAppLogEvents()),
      m_OwnerTID(-1),
      m_IsReadOnly(false),
      m_Version(sm_VersionCounter.Add(1)),
      m_SpanKind(ITracerSpan::eKind_NotSet)
{
    x_LoadEnvContextProperties();
}


CRequestContext::~CRequestContext(void)
{
}


CRequestContext::TCount CRequestContext::GetNextRequestID(void)
{
    static CAtomicCounter s_RequestCount;
    return s_RequestCount.Add(1);
}


void CRequestContext::x_LogHitID(bool ignore_app_state) const
{
    if ((m_HitIDLoggedFlag & fLoggedOnRequest) || m_HitID.Empty()) return;

    // ignore_app_state can be set by CDiagContext in case if hit-id
    // was set for request context only (no default one), but request
    // start/stop never happened and the hit id may be lost. If this
    // happens, CDiagContext may force logging of the request's hit id
    // on application stop.

    if (!ignore_app_state  &&
        m_AppState != eDiagAppState_Request  &&
        m_AppState != eDiagAppState_RequestBegin  &&
        m_AppState != eDiagAppState_RequestEnd) return;
    GetDiagContext().Extra().Print(g_GetNcbiString(eNcbiStrings_PHID),
        m_HitID.GetHitId());
    m_HitIDLoggedFlag |= fLoggedOnRequest;
}


const string& CRequestContext::SetHitID(void)
{
    if (x_CanModify()) {
        SetHitID(GetDiagContext().GetNextHitID());
    }
    return m_HitID.GetHitId();
}


string CRequestContext::x_GetHitID(CDiagContext::EDefaultHitIDFlags flag) const
{
    if ( x_IsSetProp(eProp_HitID) ) {
        x_LogHitID();
        return m_HitID.GetHitId();
    }
    if (!x_CanModify()) return kEmptyStr;
    CSharedHitId phid = GetDiagContext().x_GetDefaultHitID(CDiagContext::eHitID_NoCreate);
    if (!phid.Empty()) {
        const_cast<CRequestContext*>(this)->x_SetHitID(phid);
        return phid.GetHitId();
    }
    if (flag != CDiagContext::eHitID_NoCreate) {
        // If there's no hit id available, create (and log) a new one.
        return const_cast<CRequestContext*>(this)->SetHitID();
    }
    return kEmptyStr;
}


const string& CRequestContext::SetSessionID(void)
{
    CNcbiOstrstream oss;
    CDiagContext& ctx = GetDiagContext();
    oss << ctx.GetStringUID(ctx.UpdateUID()) << '_' << setw(4) << setfill('0')
        << GetRequestID() << "SID";
    SetSessionID(CNcbiOstrstreamToString(oss));
    return m_SessionID.GetOriginalString();
}


EDiagAppState CRequestContext::GetAppState(void) const
{
    return m_AppState != eDiagAppState_NotSet
        ? m_AppState : GetDiagContext().GetGlobalAppState();
}


void CRequestContext::SetAppState(EDiagAppState state)
{
    if (!x_CanModify()) return;
    m_AppState = state;
}


void CRequestContext::Reset(void)
{
    if (!x_CanModify()) return;
    m_AppState = eDiagAppState_NotSet; // Use global AppState
    UnsetRequestID();
    UnsetClientIP();
    UnsetSessionID();
    UnsetHitID();
    UnsetRequestStatus();
    UnsetBytesRd();
    UnsetBytesWr();
    m_ReqTimer.Reset();
    m_PassThroughProperties.clear();
    x_LoadEnvContextProperties();
}


void CRequestContext::SetProperty(const string& name, const string& value)
{
    if (!x_CanModify()) return;
    m_Properties[name] = value;
}


const string& CRequestContext::GetProperty(const string& name) const
{
    TProperties::const_iterator it = m_Properties.find(name);
    return it != m_Properties.end() ? it->second : kEmptyStr;
}


bool CRequestContext::IsSetProperty(const string& name) const
{
    return m_Properties.find(name) != m_Properties.end();
}


void CRequestContext::UnsetProperty(const string& name)
{
    if (!x_CanModify()) return;
    m_Properties.erase(name);
}


static const char* kBadIP = "0.0.0.0";


void CRequestContext::SetClientIP(const string& client)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_ClientIP);

    // Verify IP (tolerating optional enclosing square brackets, [...])
    CTempStringEx ip = NStr::TruncateSpaces_Unsafe(client);
    size_t len = ip.size();
    bool bad;
    if (!len) {
        bad = true;
    } else if (ip[0] != '[') {
        bad = false;
    } else if (len < 3  ||  ip[len - 1] != ']') {
        bad = true;
    } else {
        ip = ip.substr(1, len - 2);
        bad = false;
    }
    if (!bad  &&  !NStr::IsIPAddress(ip)) {
        bad = true;
    }
    if ( bad ) {
        m_ClientIP = kBadIP;
        x_Modify();
        ERR_POST_X(25, Warning << "Bad client IP value: " << ip);
        return;
    }

    m_ClientIP = ip;
    x_Modify();
}


void CRequestContext::StartRequest(void)
{
    if (!x_CanModify()) return;
    x_LoadEnvContextProperties();
    if (m_Flags & fResetOnStart) {
        UnsetRequestStatus();
        SetBytesRd(0);
        SetBytesWr(0);
    }
    GetRequestTimer().Restart();
    m_IsRunning = true;
    x_LogHitID();
    // Save the tracer to make sure the same one is used to stop request.
    m_Tracer = sm_Tracer;
    if (m_Tracer) {
        m_Tracer->OnRequestStart(*this);
    }
}


void CRequestContext::StopRequest(void)
{
    if (!x_CanModify()) return;
    if (m_Tracer) {
        m_Tracer->OnRequestStop(*this);
        m_Tracer.reset();
    }
    if ((m_HitIDLoggedFlag & fLoggedOnRequest) == 0) {
        // Hit id has not been set or logged yet. Try to log the default one.
        x_GetHitID(CDiagContext::eHitID_NoCreate);
    }
    Reset();
    m_IsRunning = false;
}


bool& CRequestContext::sx_GetDefaultAutoIncRequestIDOnPost(void)
{
    static bool s_DefaultAutoIncRequestIDOnPostFlag = false;
    return s_DefaultAutoIncRequestIDOnPostFlag;
}


void CRequestContext::SetDefaultAutoIncRequestIDOnPost(bool enable)
{
    sx_GetDefaultAutoIncRequestIDOnPost() = enable;
}


bool CRequestContext::GetDefaultAutoIncRequestIDOnPost(void)
{
    return sx_GetDefaultAutoIncRequestIDOnPost();
}





void CRequestContext::x_SetHitID(const CSharedHitId& hit_id)
{
    if (!x_CanModify()) return;

    const string& hit = hit_id.GetHitId();
    if (m_HitIDLoggedFlag & fLoggedOnRequest) {
        // Show warning when changing hit id after is has been logged.
        ERR_POST_X(28, Warning << "Changing hit ID after one has been logged. New hit id is: " << hit);
    }
    x_SetProp(eProp_HitID);
    m_SubHitIDCache.clear();
    m_HitID = hit_id;
    x_Modify();
    m_HitIDLoggedFlag = 0;
    x_LogHitID();
}


void CRequestContext::SetHitID(const string& hit)
{
    if (!x_CanModify()) return;
    x_SetHitID(CSharedHitId(hit));
}


// Use old output format if the flag is set
NCBI_PARAM_DECL(unsigned int, Log, Issued_SubHit_Limit);
NCBI_PARAM_DEF_EX(unsigned int, Log, Issued_SubHit_Limit, 200, eParam_NoThread,
    LOG_ISSUED_SUBHIT_LIMIT);
typedef NCBI_PARAM_TYPE(Log, Issued_SubHit_Limit) TIssuedSubHitLimitParam;


void CRequestContext::x_UpdateSubHitID(bool increment, CTempString prefix)
{
    _ASSERT(IsSetHitID());

    // Use global sub-hit counter for default hit id to prevent
    // duplicate phids in different threads.
    string hit_id = GetHitID();

    unsigned int sub_hit_id;
    if (increment) {
        sub_hit_id = m_HitID.GetNextSubHitId();
        x_Modify();
    }
    else {
        sub_hit_id = m_HitID.GetCurrentSubHitId();
    }

    // Cache the string so that C code can use it.
    string subhit = prefix + NStr::NumericToString(sub_hit_id);
    hit_id += "." + subhit;
    m_SubHitIDCache = hit_id;
    if (increment  &&  sub_hit_id <= TIssuedSubHitLimitParam::GetDefault()) {
        GetDiagContext().Extra().Print("issued_subhit", subhit);
    }
}


void CRequestContext::SetSessionID(const string& session)
{
    if (!x_CanModify()) return;
    if ( !IsValidSessionID(session) ) {
        EOnBadSessionID action = GetBadSessionIDAction();
        switch ( action ) {
        case eOnBadSID_Ignore:
            return;
        case eOnBadSID_AllowAndReport:
            ERR_POST_X(26, Warning << "Bad session ID format: " << session);
            break;
        case eOnBadSID_IgnoreAndReport:
            ERR_POST_X(26, Warning << "Bad session ID format: " << session);
            return;
        case eOnBadSID_Throw:
            NCBI_THROW(CRequestContextException, eBadSession,
                "Bad session ID format: " + session);
            break;
        case eOnBadSID_Allow:
            break;
        }
    }
    x_SetProp(eProp_SessionID);
    m_SessionID.SetString(session);
    x_Modify();
}


bool CRequestContext::IsValidSessionID(const string& session_id)
{
    switch ( GetAllowedSessionIDFormat() ) {
    case eSID_Ncbi:
        {
            if (session_id.size() < 24) return false;
            if (session_id[16] != '_') return false;
            if ( !NStr::EndsWith(session_id, "SID") ) return false;
            CTempString uid(session_id, 0, 16);
            if (NStr::StringToUInt8(uid, NStr::fConvErr_NoThrow, 16) == 0  &&  errno !=0) {
                return false;
            }
            CTempString rqid(session_id, 17, session_id.size() - 20);
            if (NStr::StringToUInt(rqid, NStr::fConvErr_NoThrow) == 0  &&  errno != 0) {
                return false;
            }
            break;
        }
    case eSID_Standard:
        {
            if ( session_id.empty() ) {
                return false;
            }
            string id_std = kAllowedIdMarkchars;
            ITERATE (string, c, session_id) {
                if (!isalnum(*c)  &&  id_std.find(*c) == NPOS) {
                    return false;
                }
            }
            break;
        }
    case eSID_Other:
        return true;
    }
    return true;
}


NCBI_PARAM_ENUM_DECL(CRequestContext::EOnBadSessionID, Log, On_Bad_Session_Id);
NCBI_PARAM_ENUM_ARRAY(CRequestContext::EOnBadSessionID, Log, On_Bad_Session_Id)
{
    {"Allow", CRequestContext::eOnBadSID_Allow},
    {"AllowAndReport", CRequestContext::eOnBadSID_AllowAndReport},
    {"Ignore", CRequestContext::eOnBadSID_Ignore},
    {"IgnoreAndReport", CRequestContext::eOnBadSID_IgnoreAndReport},
    {"Throw", CRequestContext::eOnBadSID_Throw}
};
NCBI_PARAM_ENUM_DEF_EX(CRequestContext::EOnBadSessionID, Log, On_Bad_Session_Id,
                       CRequestContext::eOnBadSID_AllowAndReport,
                       eParam_NoThread,
                       LOG_ON_BAD_SESSION_ID);
typedef NCBI_PARAM_TYPE(Log, On_Bad_Session_Id) TOnBadSessionId;


NCBI_PARAM_ENUM_DECL(CRequestContext::ESessionIDFormat, Log, Session_Id_Format);
NCBI_PARAM_ENUM_ARRAY(CRequestContext::ESessionIDFormat, Log, Session_Id_Format)
{
    {"Ncbi", CRequestContext::eSID_Ncbi},
    {"Standard", CRequestContext::eSID_Standard},
    {"Other", CRequestContext::eSID_Other}
};
NCBI_PARAM_ENUM_DEF_EX(CRequestContext::ESessionIDFormat, Log, Session_Id_Format,
                       CRequestContext::eSID_Standard,
                       eParam_NoThread,
                       LOG_SESSION_ID_FORMAT);
typedef NCBI_PARAM_TYPE(Log, Session_Id_Format) TSessionIdFormat;


CRequestContext::EOnBadSessionID CRequestContext::GetBadSessionIDAction(void)
{
    return TOnBadSessionId::GetDefault();
}


void CRequestContext::SetBadSessionIDAction(EOnBadSessionID action)
{
    TOnBadSessionId::SetDefault(action);
}


CRequestContext::ESessionIDFormat CRequestContext::GetAllowedSessionIDFormat(void)
{
    return TSessionIdFormat::GetDefault();
}


void CRequestContext::SetAllowedSessionIDFormat(ESessionIDFormat fmt)
{
    TSessionIdFormat::SetDefault(fmt);
}


CRef<CRequestContext> CRequestContext::Clone(void) const
{
    CRef<CRequestContext> ret(new CRequestContext);
    ret->m_RequestID = m_RequestID;
    ret->m_AppState = m_AppState;
    ret->m_ClientIP = m_ClientIP;
    ret->m_SessionID.SetString(m_SessionID.GetOriginalString());
    m_HitID.SetShared();
    ret->m_HitID = m_HitID;
    ret->m_HitIDLoggedFlag = m_HitIDLoggedFlag;
    ret->m_SubHitIDCache = m_SubHitIDCache;
    ret->m_Dtab = m_Dtab;
    ret->m_ReqStatus = m_ReqStatus;
    ret->m_ReqTimer = m_ReqTimer;
    ret->m_BytesRd = m_BytesRd;
    ret->m_BytesWr = m_BytesWr;
    ret->m_Properties = m_Properties;
    ret->m_PropSet = m_PropSet;
    ret->m_IsRunning = m_IsRunning;
    ret->m_AutoIncOnPost = m_AutoIncOnPost;
    ret->m_Flags = m_Flags;
    ret->m_DisabledAppLog = m_DisabledAppLog;
    ret->m_DisabledAppLogEvents = m_DisabledAppLogEvents;
    ret->m_IsReadOnly = m_IsReadOnly;
    // NOTE: Do not clone m_Version - the new context gets its own unique version.
    return ret;
}


string CRequestContext::SelectLastHitID(const string& hit_ids)
{
    // Empty string or single value - return as-is.
    if (hit_ids.empty()  ||  hit_ids.find_first_of(", ") == NPOS) {
        return hit_ids;
    }
    list<string> ids;
    // '+' is a temporary workaround for mismatched encoding/decoding of spaces.
    NStr::Split(hit_ids, ", +", ids,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    return ids.empty() ? kEmptyStr : ids.back();
}


string CRequestContext::SelectLastSessionID(const string& session_ids)
{
    // Empty string or single value - return as-is.
    if (session_ids.empty()  ||  session_ids.find_first_of(", ") == NPOS) {
        return session_ids;
    }
    list<string> ids;
    NStr::Split(session_ids, ", ", ids,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    REVERSE_ITERATE(list<string>, it, ids) {
        if (*it != "UNK_SESSION") {
            return *it;
        }
    }
    return kEmptyStr;
}


bool CRequestContext::x_IsSetPassThroughProp(CTempString name, bool update) const
{
    if ( update ) x_UpdateStdPassThroughProp(name);
    TPassThroughProperties::const_iterator found = m_PassThroughProperties.find(name);
    return found != m_PassThroughProperties.end();
}


const string& CRequestContext::x_GetPassThroughProp(CTempString name, bool update) const
{
    if ( update ) x_UpdateStdPassThroughProp(name);
    TPassThroughProperties::const_iterator found = m_PassThroughProperties.find(name);
    return found != m_PassThroughProperties.end() ? found->second : kEmptyStr;
}


void CRequestContext::x_SetPassThroughProp(CTempString name,
                                           CTempString value,
                                           bool update) const
{
    if (!x_CanModify()) return;
    m_PassThroughProperties[name] = value;
    if ( update ) x_UpdateStdContextProp(name);
}


void CRequestContext::x_ResetPassThroughProp(CTempString name, bool update) const
{
    if (!x_CanModify()) return;
    TPassThroughProperties::iterator found = m_PassThroughProperties.find(name);
    if (found != m_PassThroughProperties.end()) {
        m_PassThroughProperties.erase(found);
        if ( update ) x_UpdateStdContextProp(name);
    }
}


void CRequestContext::x_UpdateStdPassThroughProp(CTempString name) const
{
    if (name.empty()  ||  NStr::EqualNocase(name, kPassThrough_Sid)) {
        if ( IsSetSessionID() ) {
            x_SetPassThroughProp(kPassThrough_Sid, GetSessionID(), false);
        }
        else {
            x_ResetPassThroughProp(kPassThrough_Sid, false);
        }
    }
    if (name.empty()  ||  NStr::EqualNocase(name, kPassThrough_ClientIp)) {
        if ( IsSetClientIP() ) {
            x_SetPassThroughProp(kPassThrough_ClientIp, GetClientIP(), false);
        }
        else {
            x_ResetPassThroughProp(kPassThrough_ClientIp, false);
        }
    }
    if (name.empty()  ||  NStr::EqualNocase(name, kPassThrough_Dtab)) {
        if ( IsSetDtab() ) {
            x_SetPassThroughProp(kPassThrough_Dtab, GetDtab(), false);
        }
        else {
            x_ResetPassThroughProp(kPassThrough_Dtab, false);
        }
    }
    if (name.empty()  ||  NStr::EqualNocase(name, kPassThrough_Phid)) {
        if ( IsSetHitID() ) {
            string sub_phid = const_cast<CRequestContext&>(*this).GetCurrentSubHitID();
            if ( sub_phid.empty() ) {
                sub_phid = const_cast<CRequestContext&>(*this).GetNextSubHitID();
            }
            x_SetPassThroughProp(kPassThrough_Phid, sub_phid, false);
        }
        else {
            x_ResetPassThroughProp(kPassThrough_Phid, false);
        }
    }
}


void CRequestContext::x_UpdateStdContextProp(CTempString name) const
{
    CRequestContext& ctx = const_cast<CRequestContext&>(*this);

    bool match = NStr::EqualNocase(name, kPassThrough_Sid);
    if (name.empty()  ||  match) {
        if (x_IsSetPassThroughProp(kPassThrough_Sid, false)) {
            ctx.SetSessionID(x_GetPassThroughProp(kPassThrough_Sid, false));
        }
        // Reset property only if explicit name is provided
        else if ( match ) {
            ctx.UnsetSessionID();
        }
        // Explicit name provided - skip other checks.
        if ( match ) return;
    }

    match = NStr::EqualNocase(name, kPassThrough_Phid);
    if (name.empty()  ||  match) {
        if (x_IsSetPassThroughProp(kPassThrough_Phid, false)) {
            ctx.SetHitID(x_GetPassThroughProp(kPassThrough_Phid, false));
        }
        // Reset property only if explicit name is provided
        else if ( match ) {
            ctx.UnsetHitID();
        }
        // Explicit name provided - skip other checks.
        if ( match ) return;
    }

    match = NStr::EqualNocase(name, kPassThrough_ClientIp);
    if (name.empty()  ||  match) {
        if (x_IsSetPassThroughProp(kPassThrough_ClientIp, false)) {
            ctx.SetClientIP(x_GetPassThroughProp(kPassThrough_ClientIp, false));
        }
        // Reset property only if explicit name is provided
        else if ( match ) {
            ctx.UnsetClientIP();
        }
        // Explicit name provided - skip other checks.
        if ( match ) return;
    }

    match = NStr::EqualNocase(name, kPassThrough_Dtab);
    if (name.empty()  ||  match) {
        if (x_IsSetPassThroughProp(kPassThrough_Dtab, false)) {
            ctx.SetDtab(x_GetPassThroughProp(kPassThrough_Dtab, false));
        }
        // Reset property only if explicit name is provided
        else if ( match ) {
            ctx.UnsetDtab();
        }
        // Explicit name provided - skip other checks.
        if ( match ) return;
    }
}



shared_ptr<IRequestTracer> CRequestContext::sm_Tracer;

void CRequestContext::SetRequestTracer(const shared_ptr<IRequestTracer>& tracer)
{
    sm_Tracer = tracer;
}


NCBI_PARAM_DECL(string, Context, Fields);
NCBI_PARAM_DEF_EX(string, Context, Fields, "",
                  eParam_NoThread,
                  NCBI_CONTEXT_FIELDS);
typedef NCBI_PARAM_TYPE(Context, Fields) TNcbiContextFields;

DEFINE_STATIC_MUTEX(s_ContextFieldsMutex);
unique_ptr<CMaskFileName> CRequestContext::sm_ContextFields;
unique_ptr<CRequestContext::TPassThroughProperties> CRequestContext::sm_EnvContextProperties;


const CMask& CRequestContext::sx_GetContextFieldsMask(void)
{
    if ( !sm_ContextFields.get() ) {
        CMutexGuard guard(s_ContextFieldsMutex);
        if ( !sm_ContextFields.get() ) {
            sm_ContextFields.reset(new CMaskFileName());
            string fields_var = TNcbiContextFields::GetDefault();
            if ( !fields_var.empty() ) {
                list<string> fields;
                NStr::Split(fields_var, " ", fields, NStr::fSplit_MergeDelimiters);
                ITERATE(list<string>, field, fields) {
                    string norm_field = sx_NormalizeContextPropertyName(*field);
                    sm_ContextFields->Add(norm_field);
                }
            }
            else {
                // By default exclude everything.
                sm_ContextFields->AddExclusion("*");
            }
        }
    }
    return *sm_ContextFields;
}


string CRequestContext::sx_NormalizeContextPropertyName(const string& name)
{
    return NStr::Replace(name, "_", "-");
}


void CRequestContext::x_LoadEnvContextProperties(void)
{
    if (!x_CanModify()) return;
    // Parse environment only once.
    if ( !sm_EnvContextProperties.get() ) {
        TPassThroughProperties props;
        {
            CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
            if ( !app ) return;
            const CNcbiEnvironment& env = app->GetEnvironment();
            list<string> names;
            env.Enumerate(names);
            ITERATE(list<string>, it, names) {
                props[*it] = env.Get(*it);
            }
        }

        CMutexGuard guard(s_ContextFieldsMutex);
        if ( !sm_EnvContextProperties.get() ) {
            sm_EnvContextProperties.reset(new TPassThroughProperties);
            const CMask& mask = sx_GetContextFieldsMask();
            ITERATE(TPassThroughProperties, it, props) {
                string norm_prop = sx_NormalizeContextPropertyName(it->first);
                if ( mask.Match(norm_prop, NStr::eNocase) ) {
                    (*sm_EnvContextProperties)[norm_prop] = it->second;
                }
            }
        }
    }
    m_PassThroughProperties.insert(sm_EnvContextProperties->begin(), sm_EnvContextProperties->end());
}


void CRequestContext::AddPassThroughProperty(const string& name, const string& value)
{
    if (!x_CanModify()) return;
    const CMask& mask = sx_GetContextFieldsMask();
    string norm_prop = sx_NormalizeContextPropertyName(name);
    if ( mask.Match(norm_prop, NStr::eNocase) ) {
        m_PassThroughProperties[norm_prop] = value;
    }
}


string CRequestContext_PassThrough::Serialize(EFormat format) const
{
    m_Context->x_UpdateStdPassThroughProp("");

    switch (format) {
    case eFormat_UrlEncoded:
        return x_SerializeUrlEncoded();
    }
    return kEmptyStr;
}


void CRequestContext_PassThrough::Deserialize(CTempString data, EFormat format)
{
    switch (format) {
    case eFormat_UrlEncoded:
        x_DeserializeUrlEncoded(data);
        break;
    }

    m_Context->x_UpdateStdContextProp("");
}


string CRequestContext_PassThrough::x_SerializeUrlEncoded(void) const
{
    return CStringPairs<TProperties>::Merge(m_Context->m_PassThroughProperties,
        "&", "=", new CStringEncoder_Url(NStr::eUrlEnc_Cookie));
}


void CRequestContext_PassThrough::x_DeserializeUrlEncoded(CTempString data)
{
    CStringPairs<TProperties>::Parse(
        m_Context->m_PassThroughProperties, data,
        "&", "=", new CStringDecoder_Url());
}


const char* CRequestContextException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadSession: return "eBadSession";
    case eBadHit:     return "eBadHit";
    default:          return CException::GetErrCodeString();
    }
}


CRequestContextGuard_Base::CRequestContextGuard_Base(CRequestContext* context, TFlags flags)
    : m_Flags(flags)
{
    m_OriginatesFromThrow = uncaught_exceptions();
    CDiagContext& ctx = GetDiagContext();
    if ( context ) {
        m_SavedContext.Reset(&ctx.GetRequestContext());
        m_RequestContext.Reset(context);
        ctx.SetRequestContext(context);
    }
    else {
        m_RequestContext.Reset(&ctx.GetRequestContext());
    }

    if (m_Flags & fPrintRequestStart) {
        // This also updates app state to RB/R
        ctx.PrintRequestStart();
    }
}


CRequestContextGuard_Base::~CRequestContextGuard_Base(void)
{
    // If released, do not perform any actions.
    if ( !m_RequestContext ) return;

    if ( uncaught_exceptions()  &&  !m_OriginatesFromThrow ) {
        SetStatus(m_ErrorStatus);
    }
    else if ( !m_RequestContext->IsSetRequestStatus() ) {
        m_RequestContext->SetRequestStatus(CRequestStatus::e200_Ok);
    }

    CDiagContext& ctx = GetDiagContext();
    // This also updates app state to RE/P
    ctx.PrintRequestStop();

    if ( m_SavedContext ) {
        ctx.SetRequestContext(m_SavedContext.GetNonNullPointer());
    }
}


void CRequestContextGuard_Base::Release(void)
{
    if ( m_SavedContext ) {
        GetDiagContext().SetRequestContext(m_SavedContext.GetNonNullPointer());
        m_SavedContext.Reset();
    }
    m_RequestContext.Reset();
}


void CRequestContextGuard_Base::SetDefaultErrorStatus(int status)
{
    // If the guard has been released throw null pointer exception.
    m_RequestContext.GetNonNullPointer();
    m_ErrorStatus = status;
}


END_NCBI_SCOPE
