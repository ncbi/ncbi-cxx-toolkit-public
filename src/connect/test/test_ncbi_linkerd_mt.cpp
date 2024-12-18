/* $Id$
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
 * Authors:  David McElhany
 *
 * File Description:
 *   Test NAMERD in MT mode
 *
 */

#include <ncbi_pch.hpp>

#include <connect/ncbi_http_session.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/test_mt.hpp>
#include <util/xregexp/regexp.hpp>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


class CTestApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);

protected:
    int CompareResponse(const string& expected, const string& got);
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);

private:
    CUrl           m_Url;
    string         m_Service;
    string         m_Scheme;
    string         m_User;
    string         m_Password;
    string         m_Path;
    string         m_Args;
    string         m_ToPost;
    string         m_Expected;
    string         m_ThreadsPassed;
    string         m_ThreadsFailed;
    CTimeout       m_Timeout;
    unsigned short m_Retries;
    int            m_CompleteThreads;
};


int CTestApp::CompareResponse(const string& expected, const string& got)
{
    CRegexp re(expected, CRegexp::fCompile_dotall);
    if (re.IsMatch(got)) {
        ERR_POST(Info << "--- Response Body (STDOUT) ---");
        ERR_POST(Info << got);
        return 0;
    } else {
        ERR_POST(Error << "--- Response Body (STDOUT) ---  did not match expected value");
        size_t pos = 0, len;
        string escaped = expected;
        len = escaped.length();
        while ((pos = escaped.find("\\", pos)) != NPOS) { escaped.replace(pos, 1, "\\\\"); pos += 2; }
        while ((pos = escaped.find("\r", pos)) != NPOS) { escaped.replace(pos, 1, "\\r");  pos += 2; }
        while ((pos = escaped.find("\n", pos)) != NPOS) { escaped.replace(pos, 1, "\\n");  pos += 2; }
        ERR_POST(Info << "Escaped exp string (length " << len << "): [" << escaped << "]");
        escaped = got;
        len = escaped.length();
        while ((pos = escaped.find("\\", pos)) != NPOS) { escaped.replace(pos, 1, "\\\\"); pos += 2; }
        while ((pos = escaped.find("\r", pos)) != NPOS) { escaped.replace(pos, 1, "\\r");  pos += 2; }
        while ((pos = escaped.find("\n", pos)) != NPOS) { escaped.replace(pos, 1, "\\n");  pos += 2; }
        ERR_POST(Info << "Escaped got string (length " << len << "): [" << escaped << "]");
    }
    return 1;
}


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddKey("service", "Service", "Name of service to use for I/O",
                CArgDescriptions::eString);

    args.AddDefaultKey("scheme", "HttpScheme", "The HTTP scheme, e.g. 'https'",
                       CArgDescriptions::eString, "http");
    args.SetConstraint("scheme", &(*new CArgAllow_Strings, "http", "https"));

    args.AddDefaultKey("user", "User", "The user name",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("password", "Password", "The password",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("path", "Path", "The URL path, e.g. '/path/file.cgi'",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("args", "Args", "The URL parameters (without '?'), "
                       "e.g. 'id=1234&term=cancer'",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("post", "ToPost", "The data to send via POST (if any)",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("expected", "Expected", "The expected output "
                       "(if empty, expected = posted)",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("timeout", "Timeout", "Timeout",
                       CArgDescriptions::eDouble, "60.0");

    args.AddDefaultKey("retries", "Retries", "Max HTTP retries",
                       CArgDescriptions::eInteger, "0");

    args.SetUsageContext(GetArguments().GetProgramBasename(), "NAMERD MT test");

    return true;
}


bool CTestApp::TestApp_Init(void)
{
    // Get args as passed in.
    m_Service = GetArgs()["service"].AsString();
    if (m_Service.empty()) {
        ERR_POST(Critical << "Missing service.");
        return false;
    }
    m_ToPost = GetArgs()["post"].AsString();
    if (m_ToPost.empty()) {
        ERR_POST(Critical << "Missing value to post.");
        return false;
    }
    m_Scheme   = GetArgs()["scheme"].AsString();
    m_User     = GetArgs()["user"].AsString();
    m_Password = GetArgs()["password"].AsString();
    m_Path     = GetArgs()["path"].AsString();
    m_Args     = GetArgs()["args"].AsString();
    m_Expected = GetArgs()["expected"].AsString();
    m_Retries  = static_cast<unsigned short>(GetArgs()["retries"].AsInteger());
    m_Timeout.Set(GetArgs()["timeout"].AsDouble());

    // Set expected=posted if not given in args.
    if (m_Expected.empty()) {
        m_Expected = m_ToPost;
    }

    // Construct a URL based on supplied fields.
    string  url(m_Scheme);
    url += "+ncbilb://";
    if ( ! m_User.empty()  ||  ! m_Password.empty()) {
        if ( ! m_User.empty()) url += m_User;
        url += ":";
        if ( ! m_Password.empty()) url += m_Password;
        url += "@";
    }
    url += m_Service;
    if ( ! m_Path.empty()) {
        if (m_Path[0] != '/') url += '/';
        url += m_Path;
    }
    if ( ! m_Args.empty()) {
        if (m_Args[0] != '?') url += '?';
        url += m_Args;
    }

    m_Url.SetUrl(url);
    if ( ! m_Url.IsService()) {
        ERR_POST(Critical << "URL '" << m_Url.ComposeUrl(CUrlArgs::eAmp_Char)
            << "' is not a service URL.");
        return false;
    }
    ERR_POST(Info << "Service:  '" << m_Service << "'");
    ERR_POST(Info << "Scheme:   '" << m_Scheme << "'");
    ERR_POST(Info << "User:     '" << m_User << "'");
    ERR_POST(Info << "Password: '" << m_Password << "'");
    ERR_POST(Info << "Path:     '" << m_Path << "'");
    ERR_POST(Info << "Args:     '" << m_Args << "'");
    ERR_POST(Info << "ToPost:   '" << m_ToPost << "'");
    ERR_POST(Info << "Expected: '" << m_Expected << "'");
    ERR_POST(Info << "Timeout:  "  << m_Timeout.GetAsDouble());
    ERR_POST(Info << "Retries:  "  << m_Retries);

    ERR_POST(Info << "Calculated values:");
    ERR_POST(Info << "URL in    " << url);
    ERR_POST(Info << "URL:      " << m_Url.ComposeUrl(CUrlArgs::eAmp_Char));
    ERR_POST(Info << "Service:  " << m_Url.GetService());
    ERR_POST(Info << "Scheme:   " << m_Url.GetScheme());
    ERR_POST(Info << "Host:     " << m_Url.GetHost());

    m_CompleteThreads = 0;

    return true;
}


bool CTestApp::Thread_Run(int idx)
{
    bool    retval = false;
    string  id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    // Send / Receive
    ERR_POST(Info << "Posting: '" << m_ToPost << "'");
    CHttpResponse   response = g_HttpPost(m_Url, m_ToPost, "", m_Timeout, m_Retries);

    // Check status
    if (response.GetStatusCode() != 200) {
        ERR_POST(Error << "FAIL: HTTP Status: " << response.GetStatusCode()
                       << " (" << response.GetStatusText() << ")");
    } else {
        string          got;
        CNcbiOstrstream out;

        // Process result
        if (response.CanGetContentStream()) {
            CNcbiIstream& in = response.ContentStream();
            if (in.good()) {
                CNcbiOstrstream out;
                NcbiStreamCopy(out, in);
                got = CNcbiOstrstreamToString(out);
                retval = (CompareResponse(m_Expected, got) == 0);
            } else {
                ERR_POST(Error << "FAIL: Bad content stream.");
            }
        } else {
            ERR_POST(Error << "FAIL: Can't read content stream.");
        }
    }

    PopDiagPostPrefix();

    if (retval) {
        if ( ! m_ThreadsPassed.empty()) {
            m_ThreadsPassed += ",";
        }
        m_ThreadsPassed += id;
    } else {
        if ( ! m_ThreadsFailed.empty()) {
            m_ThreadsFailed += ",";
        }
        m_ThreadsFailed += id;
    }
    ERR_POST(Info << "Progress:           " << ++m_CompleteThreads
                  << " out of " << s_NumThreads << " threads complete.");
    ERR_POST(Info << "Passed threads IDs: "
                  << (m_ThreadsPassed.empty() ? "(none)" : m_ThreadsPassed));
    ERR_POST(Info << "Failed threads IDs: "
                  << (m_ThreadsFailed.empty() ? "(none)" : m_ThreadsFailed));

    return retval;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_Severity | eDPF_ErrorID | eDPF_Prefix);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    s_NumThreads = 2; // default is small

    return CTestApp().AppMain(argc, argv);
}
