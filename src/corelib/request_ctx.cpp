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

BEGIN_NCBI_SCOPE


CRequestContext::CRequestContext(void)
    : m_RequestID(0),
      m_AppState(eDiagAppState_NotSet),
      m_ReqTimer(CStopWatch::eStop),
      m_BytesRd(0),
      m_BytesWr(0)
{
}


CRequestContext::~CRequestContext(void)
{
}


int CRequestContext::GetNextRequestID(void)
{
    static CAtomicCounter s_RequestCount;
    return s_RequestCount.Add(1);
}


const string& CRequestContext::SetHitID(void)
{
    m_HitID = GetDiagContext().GetNextHitID();
    return m_HitID;
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
    m_RequestID = 0; // No request ID is assigned to this request
    m_AppState = eDiagAppState_NotSet; // Use global AppState
    m_ClientIP.clear();
    m_SessionID.clear();
    m_HitID.clear();
    m_ReqStatus = 0;
    m_BytesRd = 0;
    m_BytesWr = 0;
    m_ReqTimer.Restart(); // Reset time to 0
    m_ReqTimer.Stop();    // Stop the timer
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


END_NCBI_SCOPE
