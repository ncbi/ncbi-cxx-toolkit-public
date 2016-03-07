#ifndef MISC___NCBI_APPLOG_URL__HPP
#define MISC___NCBI_APPLOG_URL__HPP

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
 * Authors: Vladimir Ivanov
 *
 */

/// @file ncbi_applog_url.hpp
///
/// Defines classes:
///   CApplogUrl -- compose Applog URL.

#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup UTIL
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
/// CApplogUrl::
///
/// Applog URL parser/composer.
///
class NCBI_XNCBI_EXPORT CApplogUrl
{
public:
    /// Default constructor
    CApplogUrl(const string& url = kEmptyStr) : m_Url(url) {};

    /// Setters
    void SetAppName       (const string& value) { m_AppName = value; }
    void SetHost          (const string& value) { m_Host    = value; }
    void SetProcessID     (Uint8         value) { SetProcessID_Str(NStr::UInt8ToString(value)); }
    void SetProcessID_Str (const string& value) { m_PID = value; }
    void SetThreadID      (Uint8         value) { SetThreadID_Str(NStr::UInt8ToString(value)); }
    void SetThreadID_Str  (const string& value) { m_TID = value; }
    void SetRequestID     (Uint8         value) { SetRequestID_Str(NStr::UInt8ToString(value)); }
    void SetRequestID_Str (const string& value) { m_RID = value; }
    void SetSession       (const string& value) { m_Session = value; }
    void SetClient        (const string& value) { m_Client  = value; }
    void SetHitID         (const string& value) { m_PHID    = value; }
    void SetLogsite       (const string& value) { m_Logsite = value; }

    void SetDateTime      (const CTime& start, const CTime& end = CTime()) {
        m_TimeStart = start;
        m_TimeEnd   = end;
    }

    /// Compose URL.
    string ComposeUrl(void);

private:
    string  m_Url;   ///< Base Applog URL, use default if empty

    // Arguments
    string  m_AppName;
    string  m_Host;
    string  m_PID;
    string  m_TID;
    string  m_RID;
    string  m_PHID;
    string  m_Session;
    string  m_Client;
    string  m_Logsite;

    CTime   m_TimeStart;
    CTime   m_TimeEnd;    ///< Current local time if empty
};


/* @} */

END_NCBI_SCOPE


#endif  /* MISC___NCBI_APPLOG_URL__HPP */
