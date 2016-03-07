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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *   CApplogUrl -- class to compose Applog URL.
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_applog_url.hpp"

BEGIN_NCBI_SCOPE


/// Default CGI used by default if /log directory is not writable on current machine.
/// Can be redefined in the configuration file.
const char* kDefaultApplogURL = "http://intranet.ncbi.nlm.nih.gov/projects/applog/browser/";

// Default Applog arguments
const char* kDefaultApplogArgs = "src=applog&noBots=0&noInternal=0&random=0&fmt=split&retmax=25&run=1";


void s_AddArg(string& args, const string& name, const string& value,
              NStr::EQuoted quotes = NStr::eNotQuoted)
{
    args += '&';
    args += NStr::URLEncode(name, NStr::eUrlEnc_URIQueryName);
    args += '=';
    if (quotes == NStr::eQuoted) {
        args += NStr::URLEncode("\"" + value + "\"", NStr::eUrlEnc_URIQueryValue);
    } else {
        args += NStr::URLEncode(value, NStr::eUrlEnc_URIQueryValue);
    }
}


// Add AND logic to the search term
#define AND_TERM  \
    if (!term.empty()) { \
        term += " AND "; \
    }

inline
void s_AddTerm(string& term, const string& name, const string& value,
               NStr::EQuoted quotes = NStr::eNotQuoted)
{
    if (value.empty()) {
        return;
    }
    AND_TERM;
    if (quotes == NStr::eQuoted) {
        term += (name + "=\"" + value + "\"");
    } else {
        term += (name + "=" + value);
    }
}


string CApplogUrl::ComposeUrl(void)
{
    string url;
    url.reserve(512);
    url.assign(m_Url.empty() ? kDefaultApplogURL : m_Url);
    url.append("?");
    url.append(kDefaultApplogArgs);

    // Date/time range

    if (m_TimeStart.IsEmpty()) {
        throw "'From' date is required";
    }
    m_TimeStart.ToLocalTime();
    CTime tmax = m_TimeEnd.Round(CTime::eRound_Minute);
    if (m_TimeEnd.IsEmpty()) {
        tmax.SetCurrent();
    } else {
        tmax = m_TimeEnd.GetLocalTime();
    }
    // Round end time to the next minute.
    tmax.AddSecond(59);
    s_AddArg(url, "mindate", m_TimeStart.AsString("Y-M-D"));
    s_AddArg(url, "mintime", m_TimeStart.AsString("h:m"));
    s_AddArg(url, "maxdate", tmax.AsString("Y-M-D"));
    s_AddArg(url, "maxtime", tmax.AsString("h:m"));

    // Search term

    string term;
    term.reserve(512);

    s_AddTerm(term, "host",       m_Host,    NStr::eQuoted);
    s_AddTerm(term, "pid",        m_PID);
    s_AddTerm(term, "tid",        m_TID);
    s_AddTerm(term, "iter",       m_RID);
    s_AddTerm(term, "ncbi_phid",  m_PHID,    NStr::eQuoted);
    s_AddTerm(term, "session_id", m_Session, NStr::eQuoted);
    s_AddTerm(term, "ip",         m_Client,  NStr::eQuoted);

    // Application name (depends from logsite)
    if ( !m_AppName.empty() || !m_Logsite.empty() ) {
        if ( m_AppName.empty() ) {
            // have logsite only, use it instead of application name
            s_AddTerm(term, "app", m_Logsite, NStr::eQuoted);
        } else {
            if ( m_Logsite.empty() ) {
                s_AddTerm(term, "app", m_AppName, NStr::eQuoted);
            } else {
                AND_TERM;
                // have both:
                // (app='appname' OR (orig_appname='appname' AND app='logsite'))
                term += "(app='" + m_AppName + "' OR " \
                        "(orig_appname='" + m_AppName + "' AND app='" + m_Logsite + "'))";
            }
        }
    }
    if ( term.empty() ) {
        throw "CApplogUrl:: search term is empty, please specify at least one argument";
    }
    s_AddArg(url, "term", term);

    // Return composed URL
    return url;
}


END_NCBI_SCOPE
