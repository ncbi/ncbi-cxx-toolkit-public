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

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/test_mt.hpp>
#include <connect/ncbi_http_session.hpp>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


class CTestApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);

protected:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);

private:
    string         m_Service;
    string         m_ToPost;
    string         m_Expected;
    string         m_ThreadsPassed;
    string         m_ThreadsFailed;
    CTimeout       m_Timeout;
    unsigned short m_Retries;
    int            m_CompleteThreads;
};


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddKey("service", "Service", "Name of service to use for I/O",
                CArgDescriptions::eString);

    args.AddDefaultKey("post", "ToPost", "The data to send via POST (if any)",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("expected", "Expected", "The expected output",
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
    /* Get args as passed in. */
    m_Service = GetArgs()["service"].AsString();
    if (m_Service.empty()) {
        ERR_POST(Critical << "Missing service.");
        return false;
    }
    m_ToPost   = GetArgs()["post"].AsString();
    m_Expected = GetArgs()["expected"].AsString();
    m_Retries  = static_cast<unsigned short>(GetArgs()["retries"].AsInteger());
    m_Timeout.Set(GetArgs()["timeout"].AsDouble());

    ERR_POST(Info << "Service:  '" << m_Service << "'");
    ERR_POST(Info << "ToPost:   '" << m_ToPost << "'");
    ERR_POST(Info << "Expected: '" << m_Expected << "'");
    ERR_POST(Info << "Timeout:  "  << m_Timeout.GetAsDouble());
    ERR_POST(Info << "Retries:  "  << m_Retries);

    m_CompleteThreads = 0;

    return true;
}


bool CTestApp::Thread_Run(int idx)
{
    bool    retval = false;
    string  id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    // Setup
    CHttpSession    session;
    CHttpRequest    request(session.NewRequest(CUrl(m_Service), CHttpSession::ePost));
    request.SetTimeout(m_Timeout);
    request.SetRetries(m_Retries);

    // Send
    if ( ! m_ToPost.empty()) {
        ERR_POST(Info << "Posting: '" << m_ToPost << "'");
        request.ContentStream() << m_ToPost;
    }

    // Receive
    CHttpResponse   response(request.Execute());
    if (response.GetStatusCode() != 200) {
        ERR_POST(Error << "FAIL: HTTP Status: " << response.GetStatusCode()
                       << " (" << response.GetStatusText() << ")");
    } else {
        string              data_out;
        CNcbiOstrstream     out;
        NcbiStreamCopy(out, response.ContentStream());
        data_out = CNcbiOstrstreamToString(out);

        // Process result
        if (data_out.empty()) {
            if (m_Expected.empty()) {
                ERR_POST(Info << "SUCCESS: No data expected or received.");
                retval = true;
            } else {
                ERR_POST(Error << "FAIL: Expected data not received.");
            }
        } else {
            if (m_Expected.empty()) {
                ERR_POST(Error << "FAIL: Received data unexpectedly.");
            } else {
                if (data_out == m_Expected) {
                    ERR_POST(Info << "SUCCESS: Expected data received.");
                    retval = true;
                } else {
                    ERR_POST(Error << "FAIL: Received data '" << data_out
                                   << "' didn't match expected data '"
                                   << m_Expected << "'.");
                }
            }
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
    ERR_POST(Info << "Progress:          " << ++m_CompleteThreads
                  << " out of " << s_NumThreads << " threads complete.");
    ERR_POST(Info << "Passed thread IDs: "
                  << (m_ThreadsPassed.empty() ? "(none)" : m_ThreadsPassed));
    ERR_POST(Info << "Failed thread IDs: "
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
