#ifndef CORELIB___REQUEST_CTX__HPP
#define CORELIB___REQUEST_CTX__HPP

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

/// @file request_ctx.hpp
///
///   Defines CRequestContext class for NCBI C++ diagnostic API.
///


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/request_status.hpp>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declarations
class CRequestContext_PassThrough;


/// Helper class to hold hit id and sub-hit counter which can be shared
/// between multiple request contexts.
class NCBI_XNCBI_EXPORT CSharedHitId
{
public:
    explicit CSharedHitId(const string& hit)
        : m_HitId(hit), m_SubHitId(0), m_AppState(GetDiagContext().GetAppState()) {}
    CSharedHitId(void) : m_SubHitId(0) {}
    ~CSharedHitId(void) {}

    bool Empty(void) const { return m_HitId.empty(); }

    /// Mark this hit id as a shared one and start using shared counter.
    void SetShared(void) const
    {
        if ( IsShared() ) return; // Already shared.
        m_SharedSubHitId.Reset(new TSharedCounter());
        m_SharedSubHitId->GetData().Set(m_SubHitId);
    }

    /// Check if shared counter is used.
    bool IsShared(void) const { return !m_SharedSubHitId.Empty(); }

    /// Get hit id value.
    const string& GetHitId(void) const { return m_HitId; }

    /// Set new hit id value. This resets sub-hit counter and makes it non-shared.
    void SetHitId(const string& hit_id)
    {
        m_SharedSubHitId.Reset();
        m_SubHitId = 0;
        m_HitId = hit_id;
        m_AppState = GetDiagContext().GetAppState();
    }

    typedef unsigned int TSubHitId;

    /// Get current sub-hit id value.
    TSubHitId GetCurrentSubHitId(void) const
    {
        return IsShared() ? (TSubHitId)m_SharedSubHitId->GetData().Get() : m_SubHitId;
    }

    /// Get next sub-hit id value.
    TSubHitId GetNextSubHitId(void)
    {
        return IsShared() ? (TSubHitId)m_SharedSubHitId->GetData().Add(1) : ++m_SubHitId;
    }

    /// Check if this hit ID was set at request level.
    bool IsRequestLevel(void) const
    {
        return m_AppState == eDiagAppState_RequestBegin ||
            m_AppState == eDiagAppState_Request ||
            m_AppState == eDiagAppState_RequestEnd;
    }

private:
    typedef CObjectFor<CAtomicCounter> TSharedCounter;

    string m_HitId;
    TSubHitId m_SubHitId;
    mutable CRef<TSharedCounter> m_SharedSubHitId;
    EDiagAppState m_AppState;
};


class CMask;
class CMaskFileName;


class NCBI_XNCBI_EXPORT CRequestContext : public CObject
{
public:
    /// Request context flags.
    enum EContextFlags {
        fResetOnStart = 1, ///< Reset values when printing request-start.

        fDefault = 0
    };
    typedef int TContextFlags;

    CRequestContext(TContextFlags flags = fDefault);
    virtual ~CRequestContext(void);

    typedef SDiagMessage::TCount TCount;

    /// Get request ID (or zero if not set).
    TCount GetRequestID(void) const;
    /// Set request ID.
    void SetRequestID(TCount rid);
    /// Check if request ID was assigned a value
    bool IsSetRequestID(void) const;
    /// Reset request ID
    void UnsetRequestID(void);
    /// Assign the next available request ID to this request.
    TCount SetRequestID(void);

    /// Return the next available application-wide request ID.
    static TCount GetNextRequestID(void);

    /// Application state
    EDiagAppState GetAppState(void) const;
    void          SetAppState(EDiagAppState state);

    /// Client IP/hostname
    const string& GetClientIP(void) const;
    void          SetClientIP(const string& client);
    bool          IsSetClientIP(void) const;
    void          UnsetClientIP(void);

    /// Session ID
    string        GetSessionID(void) const;
    void          SetSessionID(const string& session);
    bool          IsSetSessionID(void) const;
    bool          IsSetExplicitSessionID(void) const; ///< Does not check default SID
    void          UnsetSessionID(void);
    /// Create and set new session ID
    const string& SetSessionID(void);
    /// Get URL-encoded session ID
    string        GetEncodedSessionID(void) const;

    /// Hit ID
    /// Allowed source of the current hit id
    /// @sa IsSetHitID
    enum EHitIDSource {
        eHitID_Any = 0,        ///< Any hit id - always return true.
        eHitID_Request = 0x01, ///< Check if per-request hit id is set.
        eHitID_Default = 0x02, ///< Check if default hit id is set.

        /// Check if any hit is already available (will not be generated
        /// on request).
        eHidID_Existing = eHitID_Default | eHitID_Request
    };

    /// Get explicit hit id or the default one (from HTTP_NCBI_PHID etc).
    /// If none of the above is available, generate a new id for the current
    /// request.
    /// If using the default hit id, it is cached in the request context and
    /// becomes local one.
    string        GetHitID(void) const
        { return x_GetHitID(CDiagContext::eHitID_Create); }
    /// Set explicit hit id. The id is reset on request end.
    void          SetHitID(const string& hit);
    /// Check if there's an explicit hit id or the default one.
    /// @param src
    ///   Allowed source(s) of hit id.
    /// @return
    ///   If 'src' is eHitID_Any, always return 'true' because GetHitID
    ///   always returns a non-empty value. For other options return true
    ///   if the selected hit id source is already not empty.
    bool          IsSetHitID(EHitIDSource src = eHitID_Any) const;
    /// Check if there's an explicit hit id.
    /// @deprecated Use IsSetHitID(eHitID_Request) instead.
    NCBI_DEPRECATED
    bool          IsSetExplicitHitID(void) const
        { return IsSetHitID(eHitID_Request); }
    /// Reset explicit hit id.
    void          UnsetHitID(void);
    /// Generate unique hit id, assign it to this request, return
    /// the hit id value.
    const string& SetHitID(void);
    /// Get current hit id appended with auto-incremented sub-hit id.
    const string& GetNextSubHitID(CTempString prefix = CTempString());
    /// Get the last generated sub-hit id.
    const string& GetCurrentSubHitID(CTempString prefix = CTempString());

    /// Dtab
    bool IsSetDtab(void) const;
    const string& GetDtab(void) const;
    void SetDtab(const string& dtab);
    void UnsetDtab(void);

    /// Request exit status
    int  GetRequestStatus(void) const;
    void SetRequestStatus(int status);
    void SetRequestStatus(CRequestStatus::ECode code);
    bool IsSetRequestStatus(void) const;
    void UnsetRequestStatus(void);

    /// Request execution timer
    const CStopWatch& GetRequestTimer(void) const { return m_ReqTimer; }
    CStopWatch&       GetRequestTimer(void) { return m_ReqTimer; }

    /// Bytes read
    Int8 GetBytesRd(void) const;
    void SetBytesRd(Int8 bytes);
    bool IsSetBytesRd(void) const;
    void UnsetBytesRd(void);

    /// Bytes written
    Int8 GetBytesWr(void) const;
    void SetBytesWr(Int8 bytes);
    bool IsSetBytesWr(void) const;
    void UnsetBytesWr(void);

    /// Reset all properties to the initial state
    void Reset(void);

    /// User-defined request properties
    typedef map<string, string> TProperties;

    /// Add/change property
    void          SetProperty(const string& name, const string& value);
    /// Get property value or empty string
    const string& GetProperty(const string& name) const;
    /// Check if the property has a value (even if it's an empty string).
    bool          IsSetProperty(const string& name) const;
    /// Remove property from the map
    void          UnsetProperty(const string& name);

    /// Get all properties (read only)
    const TProperties& GetProperties(void) const { return m_Properties; }
    /// Get all properties (non-const)
    TProperties&       GetProperties(void) { return m_Properties; }

    /// Auto-increment request ID with every posted message
    void SetAutoIncRequestIDOnPost(bool enable) { m_AutoIncOnPost = enable; }
    /// Get auto-increment state
    bool GetAutoIncRequestIDOnPost(void) const { return m_AutoIncOnPost; }
    /// Set default auto-increment flag used for each default request context.
    /// Contexts created by users do not check this flag.
    /// The flag is not MT-protected.
    static void SetDefaultAutoIncRequestIDOnPost(bool enable);
    /// Get default auto-increment flag.
    static bool GetDefaultAutoIncRequestIDOnPost(void);

    /// Session ID format
    enum ESessionIDFormat {
        eSID_Ncbi,     ///< Strict NCBI format: (UID:16)_(RqID:4+)SID
        eSID_Standard, ///< Alpanum, underscore, -.:@, (default)
        eSID_Other     ///< Any other format
    };
    /// Session ID error actions
    enum EOnBadSessionID {
        eOnBadSID_Allow,           ///< Don't validate session id.
        eOnBadSID_AllowAndReport,  ///< Accept but show warning (default).
        eOnBadSID_Ignore,          ///< Ignore bad session id.
        eOnBadSID_IgnoreAndReport, ///< Ignore and show warning.
        eOnBadSID_Throw            ///< Throw on bad session id.
    };

    /// Check if session id fits the allowed format.
    static bool IsValidSessionID(const string& session_id);

    /// Get/set session id error action.
    static EOnBadSessionID GetBadSessionIDAction(void);
    static void SetBadSessionIDAction(EOnBadSessionID action);

    /// Get/set allowed session id format.
    static ESessionIDFormat GetAllowedSessionIDFormat(void);
    static void SetAllowedSessionIDFormat(ESessionIDFormat fmt);

    /// Copy current request context to a new one. The method can be used
    /// to process a single request in several threads which share the same
    /// information (request id, session id etc.).
    /// NOTE: The new context is not linked to the parent one. No further
    /// changes in a context are copied to its clones. It's the developer's
    /// responsibility to track multiple clones and make sure that they are
    /// used properly (e.g. status is set by just one thread; only one
    /// request-stop is printed and no clones continue to log the same request
    /// after it's stopped).
    CRef<CRequestContext> Clone(void) const;

    /// Select the last hit id from the list of ids separated with commas
    /// and optional spaces.
    static string SelectLastHitID(const string& hit_ids);
    /// Select the last session id from the list of ids separated with
    /// commas and optional spaces, ignore UNK_SESSION value.
    static string SelectLastSessionID(const string& session_ids);

    /// Add pass-through value if it matches a pattern from NCBI_CONTEXT_FIELDS.
    void AddPassThroughProperty(const string& name, const string& value);

    /// Switch request context to read-only mode. A read-only context
    /// can be attached to multiple threads. The mode must be disabled
    /// before making any modifications (e.g. printing request-stop).
    /// To avoid overhead CRequestContext does not check if there are
    /// any threads currently using the same context in a different mode.
    /// Any attempts to modify request context while in read-only mode
    /// are ignored and reported as an error (once per process).
    /// Note that some const methods may still need to modify the context
    /// (e.g. GetHitID() generates a new hit id if it's not yet assigned).
    /// To prevent the above error from being logged, the method should be
    /// called at least once before switching to read-only mode.
    void SetReadOnly(bool read_only) { m_IsReadOnly = read_only; }
    /// Get current read-only flag.
    bool GetReadOnly(void) const { return m_IsReadOnly; }

    typedef CAtomicCounter::TValue TVersion;

    /// Return version increased on every context change (hit/subhit id, client ip,
    /// session id).
    TVersion GetVersion(void) const { return m_Version; }

private:
    // Prohibit copying
    CRequestContext(const CRequestContext&);
    CRequestContext& operator=(const CRequestContext&);

    // Start/Stop methods must be visible in CDiagContext
    friend class CDiagContext;

    // Mark the current request as running.
    // Reset request status, bytes rd/wr, restart timer.
    void StartRequest(void);

    // Mark the current request as finished.
    // Reset all values (call Reset()).
    void StopRequest(void);

    // Check if the request is running.
    bool IsRunning(void) const { return m_IsRunning; }

    enum EProperty {
        eProp_RequestID     = 1 << 0,
        eProp_ClientIP      = 1 << 1,
        eProp_SessionID     = 1 << 2,
        eProp_HitID         = 1 << 3,
        eProp_ReqStatus     = 1 << 4,
        eProp_BytesRd       = 1 << 5,
        eProp_BytesWr       = 1 << 6,
        eProp_Dtab          = 1 << 7
    };
    typedef int TPropSet;

    bool x_IsSetProp(EProperty prop) const;
    void x_SetProp(EProperty prop);
    void x_UnsetProp(EProperty prop);

    // Log current hit id if not yet logged and if the application state is
    // 'in request', otherwise postpone logging until StartRequest is executed.
    // If 'ignore_app_state' is set, log any available hit id anyway.
    void x_LogHitID(bool ignore_app_state = false) const;

    void x_SetHitID(const CSharedHitId& hit_id);
    string x_GetHitID(CDiagContext::EDefaultHitIDFlags flag) const;

    void x_UpdateSubHitID(bool increment, CTempString prefix);

    static bool& sx_GetDefaultAutoIncRequestIDOnPost(void);

    // Methods used by CRequestContext_PassThrough
    bool x_IsSetPassThroughProp(CTempString name, bool update) const;
    void x_SetPassThroughProp(CTempString name, CTempString value, bool update) const;
    const string& x_GetPassThroughProp(CTempString name, bool update) const;
    void x_ResetPassThroughProp(CTempString name, bool update) const;
    // Copy std properties from CRequestContext to pass-through data.
    void x_UpdateStdPassThroughProp(CTempString name) const;
    // Copy std properties from pass-through data to CRequestContext.
    void x_UpdateStdContextProp(CTempString name) const;

    static const CMask& sx_GetContextFieldsMask(void);
    static string sx_NormalizeContextPropertyName(const string& name);

    // Load environment values matching NCBI_CONTEXT_FIELDS.
    void x_LoadEnvContextProperties(void);

    friend class CDiagBuffer;
    bool x_LogHitIDOnError(void) const;

    bool x_CanModify(void) const;

    // Bumps context version.
    void x_Modify(void);
        
    enum FLoggedHitIDFlag {
        fLoggedOnRequest = 1, // Logged on creation or request start
        fLoggedOnError = 2    // Logged on ERR_POST when applog messages are disabled
    };

    TCount         m_RequestID;
    EDiagAppState  m_AppState;
    string         m_ClientIP;
    CEncodedString m_SessionID;
    CSharedHitId   m_HitID;
    string         m_Dtab;
    mutable int    m_HitIDLoggedFlag;
    int            m_ReqStatus;
    CStopWatch     m_ReqTimer;
    Int8           m_BytesRd;
    Int8           m_BytesWr;
    TProperties    m_Properties;
    TPropSet       m_PropSet;
    bool           m_IsRunning;
    bool           m_AutoIncOnPost;
    TContextFlags  m_Flags;
    string         m_SubHitIDCache;

    // For saving/checking owner TID.
    friend class CDiagContextThreadData;
    // TID of the thread currently using this context or -1.
    Uint8          m_OwnerTID;
    bool           m_IsReadOnly;
    TVersion       m_Version;

    // Name/value map for properties to be passed between requests.
    // @sa CRequestContext_PassThrough
    typedef map<string, string, PNocase> TPassThroughProperties;

    // Access to passable properties.
    friend class CRequestContext_PassThrough;
    mutable TPassThroughProperties m_PassThroughProperties;

    // Patterns from NCBI_CONTEXT_FIELDS variable.
    static unique_ptr<CMaskFileName> sm_ContextFields;

    // Context values loaded from the global environment.
    static unique_ptr<TPassThroughProperties> sm_EnvContextProperties;

    static CAtomicCounter_WithAutoInit sm_VersionCounter;
};


/// Request context properties passed between tasks.
class NCBI_XNCBI_EXPORT CRequestContext_PassThrough
{
public:
    /// Get CRequestContext_PassThrough for the current request context.
    CRequestContext_PassThrough(void);

    /// Get CRequestContext_PassThrough for the specific request context.
    CRequestContext_PassThrough(CRequestContext ctx);

    /// Supported serialization/deserialization formats.
    enum EFormat {
        eFormat_UrlEncoded ///< name=value pairs URL-encoded and separated with '&'
    };

    /// Check if the property is set.
    bool IsSet(CTempString name) const;

    /// Set or update property value.
    void Set(CTempString name, CTempString value);

    /// Get current property value or empty string if it's not set;
    const string& Get(CTempString name) const;

    /// Reset property.
    void Reset(CTempString name);

    /// Serialize current values using the specified format.
    string Serialize(EFormat format) const;

    /// Deserialize values using the specified format.
    void Deserialize(CTempString data, EFormat format);

    /// Enumerate all properties. The callback must have the following signarure:
    /// bool F(const string& name, const string& value);
    /// The function should return true to continue enumaration, false to stop.
    template<class TCallback>
    void Enumerate(TCallback callback)
    {
        m_Context->x_UpdateStdPassThroughProp("");
        ITERATE(TProperties, it, m_Context->m_PassThroughProperties) {
            if ( !callback(it->first, it->second) ) break;
        }
    }

private:
    string x_SerializeUrlEncoded(void) const;
    void x_DeserializeUrlEncoded(CTempString data);

    typedef CRequestContext::TPassThroughProperties TProperties;

    CRef<CRequestContext> m_Context;
};


/// Take guard of the current CRequestContext, handle app-state, start/stop
/// logging and request status in the dtor.
class NCBI_XNCBI_EXPORT CRequestContextGuard_Base
{
public:
    enum EFlags {
        fPrintRequestStart = 1 << 0 ///< Print request-start automatically in the
                                    ///< constructor. By default request-start is
                                    ///< not printed to allow the caller log request
                                    ///< arguments.
    };
    typedef int TFlags;

    /// Initialize guard.
    /// @param context
    ///  Request context to be used. If null, re-use current context.
    /// @param flags
    ///  Optional flags, @sa FFlags.
    CRequestContextGuard_Base(CRequestContext* context, TFlags flags = 0);

    /// Destroy guard. If released do nothing. On exception set error status.
    /// Print request-stop. Restore previous context.
    ~CRequestContextGuard_Base(void);

    /// Set request context status.
    void SetStatus(int status) { m_RequestContext->SetRequestStatus(status); }

    /// Set default error status, which will be used if an uncaught exception
    /// is detected.
    void SetDefaultErrorStatus(int status);

    /// Get the guarded request context.
    CRequestContext& GetRequestContext() const { return *m_RequestContext; }

    /// Release the guarded context, restore the saved context if any, do not
    /// perform any other actions (logging, setting status).
    void Release(void);

private:
    TFlags                        m_Flags = 0;
    int                           m_ErrorStatus = 500;
    CRef<CRequestContext>         m_SavedContext;
    mutable CRef<CRequestContext> m_RequestContext;
};


class NCBI_XNCBI_EXPORT CRequestContextException :
    EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  CRequestContext can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eBadSession,   ///< Invalid session id
        eBadHit        ///< Invalid hit id
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const override;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CRequestContextException, CException);
};


inline
CRequestContext::TCount CRequestContext::GetRequestID(void) const
{
    return x_IsSetProp(eProp_RequestID) ? m_RequestID : 0;
}

inline
void CRequestContext::SetRequestID(TCount rid)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_RequestID);
    m_RequestID = rid;
    x_Modify();
}

inline
CRequestContext::TCount CRequestContext::SetRequestID(void)
{
    if (!x_CanModify()) return m_RequestID;
    SetRequestID(GetNextRequestID());
    return m_RequestID;
}

inline
bool CRequestContext::IsSetRequestID(void) const
{
    return x_IsSetProp(eProp_RequestID);
}

inline
void CRequestContext::UnsetRequestID(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_RequestID);
    m_RequestID = 0;
    x_Modify();
}


inline
const string& CRequestContext::GetClientIP(void) const
{
    return x_IsSetProp(eProp_ClientIP) ? m_ClientIP : kEmptyStr;
}

inline
bool CRequestContext::IsSetClientIP(void) const
{
    return x_IsSetProp(eProp_ClientIP);
}

inline
void CRequestContext::UnsetClientIP(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_ClientIP);
    m_ClientIP.clear();
    x_Modify();
}


inline
string CRequestContext::GetSessionID(void) const
{
    if ( x_IsSetProp(eProp_SessionID) ) {
        return m_SessionID.GetOriginalString();
    }
    string def_sid = GetDiagContext().GetDefaultSessionID();
    if ( !def_sid.empty() ) return def_sid;
    return const_cast<CRequestContext*>(this)->SetSessionID();
}

inline
string CRequestContext::GetEncodedSessionID(void) const
{
    return x_IsSetProp(eProp_SessionID) ?
        m_SessionID.GetEncodedString()
        : GetDiagContext().GetEncodedSessionID();
}

inline
bool CRequestContext::IsSetSessionID(void) const
{
    return IsSetExplicitSessionID()  ||
        !GetDiagContext().GetDefaultSessionID().empty();
}

inline
bool CRequestContext::IsSetExplicitSessionID(void) const
{
    return x_IsSetProp(eProp_SessionID);
}

inline
void CRequestContext::UnsetSessionID(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_SessionID);
    m_SessionID.SetString(kEmptyStr);
    x_Modify();
}


inline
bool CRequestContext::IsSetHitID(EHitIDSource src) const
{
    if (src == eHitID_Any) {
        // Local, default or auto-created hit id is always available.
        return true;
    }
    if ((src & eHitID_Request)  &&  x_IsSetProp(eProp_HitID)) {
        return m_HitID.IsRequestLevel();
    }
    if ((src & eHitID_Default) && GetDiagContext().x_IsSetDefaultHitID()) {
        return true;
    }
    return false;
}


inline
void CRequestContext::UnsetHitID(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_HitID);
    m_HitID.SetHitId(kEmptyStr);
    x_Modify();
    m_HitIDLoggedFlag = 0;
    m_SubHitIDCache.clear();
}


inline
const string& CRequestContext::GetNextSubHitID(CTempString prefix)
{
    if (!x_CanModify()) return m_SubHitIDCache;
    x_UpdateSubHitID(true, prefix);
    return m_SubHitIDCache;
}


inline
const string& CRequestContext::GetCurrentSubHitID(CTempString prefix)
{
    x_UpdateSubHitID(false, prefix);
    return m_SubHitIDCache;
}


inline
bool CRequestContext::IsSetDtab(void) const
{
    return x_IsSetProp(eProp_Dtab);
}


inline
const string& CRequestContext::GetDtab(void) const
{
    return x_IsSetProp(eProp_Dtab) ? m_Dtab : kEmptyStr;
}


inline
void CRequestContext::SetDtab(const string& dtab)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_Dtab);
    m_Dtab = dtab;
}


inline
void CRequestContext::UnsetDtab(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_Dtab);
}


inline
int CRequestContext::GetRequestStatus(void) const
{
    return x_IsSetProp(eProp_ReqStatus) ? m_ReqStatus : 0;
}

inline
void CRequestContext::SetRequestStatus(int status)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_ReqStatus);
    m_ReqStatus = status;
}

inline
void CRequestContext::SetRequestStatus(CRequestStatus::ECode code)
{
    if (!x_CanModify()) return;
    SetRequestStatus((int)code);
}

inline
bool CRequestContext::IsSetRequestStatus(void) const
{
    return x_IsSetProp(eProp_ReqStatus);
}

inline
void CRequestContext::UnsetRequestStatus(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_ReqStatus);
    m_ReqStatus = 0;
}


inline
Int8 CRequestContext::GetBytesRd(void) const
{
    return x_IsSetProp(eProp_BytesRd) ? m_BytesRd : 0;
}

inline
void CRequestContext::SetBytesRd(Int8 bytes)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_BytesRd);
    m_BytesRd = bytes;
}

inline
bool CRequestContext::IsSetBytesRd(void) const
{
    return x_IsSetProp(eProp_BytesRd);
}

inline
void CRequestContext::UnsetBytesRd(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_BytesRd);
    m_BytesRd = 0;
}


inline
Int8 CRequestContext::GetBytesWr(void) const
{
    return x_IsSetProp(eProp_BytesWr) ? m_BytesWr : 0;
}

inline
void CRequestContext::SetBytesWr(Int8 bytes)
{
    if (!x_CanModify()) return;
    x_SetProp(eProp_BytesWr);
    m_BytesWr = bytes;
}

inline
bool CRequestContext::IsSetBytesWr(void) const
{
    return x_IsSetProp(eProp_BytesWr);
}

inline
void CRequestContext::UnsetBytesWr(void)
{
    if (!x_CanModify()) return;
    x_UnsetProp(eProp_BytesWr);
    m_BytesWr = 0;
}


inline
bool CRequestContext::x_IsSetProp(EProperty prop) const
{
    return m_PropSet & prop ? true : false;
}


inline
void CRequestContext::x_SetProp(EProperty prop)
{
    m_PropSet |= prop;
}

inline
void CRequestContext::x_UnsetProp(EProperty prop)
{
    m_PropSet &= ~prop;
}


inline
bool CRequestContext::x_LogHitIDOnError(void) const
{
    if (m_HitIDLoggedFlag & fLoggedOnError || m_HitID.Empty()) {
        return false;
    }
    m_HitIDLoggedFlag |= fLoggedOnError;
    return true;
}


inline
bool CRequestContext::x_CanModify(void) const
{
    if ( m_IsReadOnly ) {
        ERR_POST_ONCE("Attempt to modify a read-only request context.");
        return false;
    }
    return true;
}


inline
void CRequestContext::x_Modify(void)
{
    m_Version = sm_VersionCounter.Add(1);
}


inline
CRequestContext_PassThrough::CRequestContext_PassThrough(void)
{
    m_Context.Reset(&GetDiagContext().GetRequestContext());
}


inline
CRequestContext_PassThrough::CRequestContext_PassThrough(CRequestContext ctx)
    : m_Context(&ctx)
{
}


inline
bool CRequestContext_PassThrough::IsSet(CTempString name) const
{
    return m_Context->x_IsSetPassThroughProp(name, true);
}


inline
void CRequestContext_PassThrough::Set(CTempString name, CTempString value)
{
    m_Context->x_SetPassThroughProp(name, value, true);
}


inline
const string& CRequestContext_PassThrough::Get(CTempString name) const
{
    return m_Context->x_GetPassThroughProp(name, true);
}


inline
void CRequestContext_PassThrough::Reset(CTempString name)
{
    m_Context->x_ResetPassThroughProp(name, true);
}


END_NCBI_SCOPE


#endif  /* CORELIB___REQUEST_CTX__HPP */
