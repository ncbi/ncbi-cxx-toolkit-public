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


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XNCBI_EXPORT CRequestContext : public CObject
{
public:
    CRequestContext(void);
    virtual ~CRequestContext(void);

    /// Get request ID (or zero if not set).
    int  GetRequestID(void) const;
    /// Set request ID.
    void SetRequestID(int rid);
    /// Check if request ID was assigned a value
    bool IsSetRequestID(void) const;
    /// Reset request ID
    void UnsetRequestID(void);
    /// Assign the next available request ID to this request.
    int  SetRequestID(void);

    /// Return the next available application-wide request ID.
    static int GetNextRequestID(void);

    /// Application state
    EDiagAppState GetAppState(void) const;
    void          SetAppState(EDiagAppState state);

    /// Client IP/hostname
    const string& GetClientIP(void) const;
    void          SetClientIP(const string& client);
    bool          IsSetCleintIP(void) const;
    void          UnsetClientIP(void);

    /// Session ID
    const string& GetSessionID(void) const;
    void          SetSessionID(const string& session);
    bool          IsSetSessionID(void) const;
    void          UnsetSessionID(void);
    /// Create and set new session ID
    const string& SetSessionID(void);
    /// Get URL-encoded session ID
    const string& GetEncodedSessionID(void) const;

    /// Hit ID
    const string& GetHitID(void) const;
    void          SetHitID(const string& hit);
    bool          IsSetHitID(void) const;
    void          UnsetHitID(void);
    /// Generate unique hit id, assign it to this request, return
    /// the hit id value.
    const string& SetHitID(void);

    /// Request exit startus
    int  GetRequestStatus(void) const;
    void SetRequestStatus(int status);
    bool IsSetRequestStatus(void) const;
    void UnsetRequestStatus(void);

    /// Request execution timer
    const CStopWatch& GetRequestTimer(void) const { return m_ReqTimer; }
    CStopWatch&       GetRequestTimer(void) { return m_ReqTimer; }

    /// Bytes read
    Int8 GetBytesRd(void) const { return m_BytesRd; }
    void SetBytesRd(Int8 bytes) { m_BytesRd = bytes; }

    /// Bytes written
    Int8 GetBytesWr(void) const { return m_BytesWr; }
    void SetBytesWr(Int8 bytes) { m_BytesWr = bytes; }

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
        eProp_RequestID  = 1 << 0,
        eProp_ClientIP   = 1 << 1,
        eProp_SessionID  = 1 << 2,
        eProp_HitID      = 1 << 3,
        eProp_ReqStatus  = 1 << 4
    };
    typedef int TPropSet;

    bool x_IsSetProp(EProperty prop) const;
    void x_SetProp(EProperty prop);
    void x_UnsetProp(EProperty prop);

    int            m_RequestID;
    EDiagAppState  m_AppState;
    string         m_ClientIP;
    CEncodedString m_SessionID;
    string         m_HitID;
    int            m_ReqStatus;
    CStopWatch     m_ReqTimer;
    Int8           m_BytesRd;
    Int8           m_BytesWr;
    TProperties    m_Properties;
    TPropSet       m_PropSet;
    bool           m_IsRunning;
};


inline
int CRequestContext::GetRequestID(void) const
{
    return x_IsSetProp(eProp_RequestID) ? m_RequestID : 0;
}

inline
void CRequestContext::SetRequestID(int rid)
{
    x_SetProp(eProp_RequestID);
    m_RequestID = rid;
}

inline
int CRequestContext::SetRequestID(void)
{
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
    x_UnsetProp(eProp_RequestID);
    m_RequestID = 0;
}


inline
const string& CRequestContext::GetClientIP(void) const
{
    return x_IsSetProp(eProp_ClientIP) ? m_ClientIP : kEmptyStr;
}

inline
bool CRequestContext::IsSetCleintIP(void) const
{
    return x_IsSetProp(eProp_ClientIP);
}

inline
void CRequestContext::UnsetClientIP(void)
{
    x_UnsetProp(eProp_ClientIP);
    m_ClientIP.clear();
}


inline
const string& CRequestContext::GetSessionID(void) const
{
    return x_IsSetProp(eProp_SessionID) ?
        m_SessionID.GetOriginalString() : kEmptyStr;
}

inline
const string& CRequestContext::GetEncodedSessionID(void) const
{
    return x_IsSetProp(eProp_SessionID) ?
        m_SessionID.GetEncodedString() : kEmptyStr;
}

inline
void CRequestContext::SetSessionID(const string& session)
{
    x_SetProp(eProp_SessionID);
    m_SessionID.SetString(session);
}

inline
bool CRequestContext::IsSetSessionID(void) const
{
    return x_IsSetProp(eProp_SessionID);
}

inline
void CRequestContext::UnsetSessionID(void)
{
    x_UnsetProp(eProp_SessionID);
    m_SessionID.SetString(kEmptyStr);
}


inline
const string& CRequestContext::GetHitID(void) const
{
    return x_IsSetProp(eProp_HitID) ? m_HitID : kEmptyStr;
}

inline
void CRequestContext::SetHitID(const string& hit)
{
    x_SetProp(eProp_HitID);
    m_HitID = hit;
}

inline
bool CRequestContext::IsSetHitID(void) const
{
    return x_IsSetProp(eProp_HitID);
}

inline
void CRequestContext::UnsetHitID(void)
{
    x_UnsetProp(eProp_HitID);
    m_HitID.clear();
}


inline
int CRequestContext::GetRequestStatus(void) const
{
    return x_IsSetProp(eProp_ReqStatus) ? m_ReqStatus : 0;
}

inline
void CRequestContext::SetRequestStatus(int status)
{
    x_SetProp(eProp_ReqStatus);
    m_ReqStatus = status;
}

inline
bool CRequestContext::IsSetRequestStatus(void) const
{
    return x_IsSetProp(eProp_ReqStatus);
}

inline
void CRequestContext::UnsetRequestStatus(void)
{
    x_UnsetProp(eProp_ReqStatus);
    m_ReqStatus = 0;
}


inline
bool CRequestContext::x_IsSetProp(EProperty prop) const
{
    return m_PropSet & prop;
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


END_NCBI_SCOPE


#endif  /* CORELIB___REQUEST_CTX__HPP */
