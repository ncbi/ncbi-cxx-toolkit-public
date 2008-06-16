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

    /// Get request ID. 0 = not set.
    int GetRequestID(void) const { return m_RequestID; }
    /// Set request ID.
    void SetRequestID(int rid) { m_RequestID = rid; }
    /// Assign the next available request ID to this request.
    int SetRequestID(void) { return m_RequestID = GetNextRequestID(); }

    /// Return the next available application-wide request ID.
    static int GetNextRequestID(void);

    /// Application state
    EDiagAppState GetAppState(void) const;
    void SetAppState(EDiagAppState state);

    /// Client IP/hostname
    const string& GetClientIP(void) const { return m_ClientIP; }
    void SetClientIP(const string& client) { m_ClientIP = client; }

    /// Session ID
    const string& GetSessionID(void) const { return m_SessionID; }
    void SetSessionID(const string& session) { m_SessionID = session; }
    /// Create and set new session ID
    const string& SetSessionID(void);

    /// Hit ID
    const string& GetHitID(void) const { return m_HitID; }
    void SetHitID(const string& hit) { m_HitID = hit; }
    /// Generate unique hit id, assign it to this request, return
    /// the hit id value.
    const string& SetHitID(void);

    /// Request exit startus
    bool IsSetRequestStatus(void) const;
    int GetRequestStatus(void) const { return m_ReqStatus; }
    void SetRequestStatus(int status) { m_ReqStatus = status; }
    void ResetRequestStatus(void);

    /// Request execution timer
    const CStopWatch& GetRequestTimer(void) const { return m_ReqTimer; }
    CStopWatch& GetRequestTimer(void) { return m_ReqTimer; }

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
    void SetProperty(const string& name, const string& value);
    /// Get property value or empty string
    const string& GetProperty(const string& name) const;
    /// Get all properties (read only)
    const TProperties& GetProperties(void) const { return m_Properties; }
    /// Get all properties (non-const)
    TProperties& GetProperties(void) { return m_Properties; }

private:
    // Prohibit copying
    CRequestContext(const CRequestContext&);
    CRequestContext& operator=(const CRequestContext&);

    int           m_RequestID;
    EDiagAppState m_AppState;
    string        m_ClientIP;
    string        m_SessionID;
    string        m_HitID;
    int           m_ReqStatus;
    CStopWatch    m_ReqTimer;
    Int8          m_BytesRd;
    Int8          m_BytesWr;
    TProperties   m_Properties;
};


END_NCBI_SCOPE


#endif  /* CORELIB___REQUEST_CTX__HPP */
