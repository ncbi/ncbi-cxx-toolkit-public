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
#include <corelib/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Corelib_Diag

BEGIN_NCBI_SCOPE


CRequestContext::CRequestContext(TContextFlags flags)
    : m_RequestID(0),
      m_AppState(eDiagAppState_NotSet),
      m_SubHitID(0),
      m_ReqStatus(0),
      m_ReqTimer(CStopWatch::eStop),
      m_BytesRd(0),
      m_BytesWr(0),
      m_PropSet(0),
      m_IsRunning(false),
      m_AutoIncOnPost(false),
      m_Flags(flags)
{
}


CRequestContext::~CRequestContext(void)
{
}


CRequestContext::TCount CRequestContext::GetNextRequestID(void)
{
    static CAtomicCounter s_RequestCount;
    return s_RequestCount.Add(1);
}


const string& CRequestContext::SetHitID(void)
{
    SetHitID(GetDiagContext().GetNextHitID());
    return m_HitID;
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
    m_AppState = state;
}


void CRequestContext::Reset(void)
{
    m_AppState = eDiagAppState_NotSet; // Use global AppState
    UnsetRequestID();
    UnsetClientIP();
    UnsetSessionID();
    UnsetHitID();
    UnsetRequestStatus();
    UnsetBytesRd();
    UnsetBytesWr();
    m_ReqTimer.Reset();
}


void CRequestContext::SetProperty(const string& name, const string& value)
{
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
    m_Properties.erase(name);
}


static const char* kBadIP = "0.0.0.0";


void CRequestContext::SetClientIP(const string& client)
{
    x_SetProp(eProp_ClientIP);

    // Verify IP
    if ( !NStr::IsIPAddress(client) ) {
        m_ClientIP = kBadIP;
        ERR_POST_X(25, "Bad client IP value: " << client);
        return;
    }

    m_ClientIP = client;
}


void CRequestContext::StartRequest(void)
{
    if (m_Flags & fResetOnStart) {
        UnsetRequestStatus();
        SetBytesRd(0);
        SetBytesWr(0);
    }
    GetRequestTimer().Restart();
    m_IsRunning = true;
}


void CRequestContext::StopRequest(void)
{
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


static bool IsValidHitID(const string& hit) {
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
        if (!isalnum(hit[pos])  &&  id_std.find(hit[pos]) == NPOS) {
            return false;
        }
    }
    if (sep_pos == NPOS) return true;
    // Separator found - make sure the rest of the id contains only digits and separators
    // (XYZ.1.2.34)
    for (pos = sep_pos; pos < hit.size(); pos++) {
        char c = hit[pos];
        if (c == '.') {
            // Need at least one digit between separators.
            if (pos == sep_pos + 1) return false;
            sep_pos = pos;
            continue;
        }
        if ( !isdigit(c) ) return false;
    }
    // Make sure the last char is digit.
    return pos > sep_pos + 1;
}


void CRequestContext::SetHitID(const string& hit)
{
    if ( !IsValidHitID(hit) ) {
        EOnBadHitID action = TOnBadHitId::GetDefault();
        switch ( action ) {
        case eOnBadPHID_Ignore:
            return;
        case eOnBadPHID_AllowAndReport:
        case eOnBadPHID_IgnoreAndReport:
            ERR_POST_X(27, "Bad hit ID format: " << hit);
            if (action == eOnBadPHID_IgnoreAndReport) {
                return;
            }
            break;
        case eOnBadPHID_Throw:
            NCBI_THROW(CRequestContextException, eBadHit,
                "Bad hit ID format: " + hit);
            break;
        case eOnBadPHID_Allow:
            break;
        }
    }
    x_SetProp(eProp_HitID);
    if (m_HitID != hit) {
        m_SubHitID = 0;
    }
    m_HitID = hit;
}


void CRequestContext::SetSessionID(const string& session)
{
    if ( !IsValidSessionID(session) ) {
        EOnBadSessionID action = GetBadSessionIDAction();
        switch ( action ) {
        case eOnBadSID_Ignore:
            return;
        case eOnBadSID_AllowAndReport:
        case eOnBadSID_IgnoreAndReport:
            ERR_POST_X(26, "Bad session ID format: " << session);
            if (action == eOnBadSID_IgnoreAndReport) {
                return;
            }
            break;
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
    ret->m_HitID = m_HitID;
    ret->m_SubHitID = m_SubHitID;
    ret->m_ReqStatus = m_ReqStatus;
    ret->m_ReqTimer = m_ReqTimer;
    ret->m_BytesRd = m_BytesRd;
    ret->m_BytesWr = m_BytesWr;
    ret->m_Properties = m_Properties;
    ret->m_PropSet = m_PropSet;
    ret->m_IsRunning = m_IsRunning;
    ret->m_AutoIncOnPost = m_AutoIncOnPost;
    ret->m_Flags = m_Flags;
    return ret;
}


const char* CRequestContextException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadSession: return "eBadSession";
    case eBadHit:     return "eBadHit";
    default:          return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
