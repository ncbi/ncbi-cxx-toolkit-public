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
    static string           sm_Service;
    static string           sm_ToPost;
    static string           sm_Expected;
    static CTimeout         sm_Timeout;
    static unsigned short   sm_Retries;
};


string          CTestApp::sm_Service;
string          CTestApp::sm_ToPost;
string          CTestApp::sm_Expected;
CTimeout        CTestApp::sm_Timeout;
unsigned short  CTestApp::sm_Retries;


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddKey("svc", "Service", "Name of service to use for I/O",
                CArgDescriptions::eString);

    args.AddDefaultKey("post", "ToPost", "The data to send via POST (if any)",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("exp", "Expected", "The expected output",
                       CArgDescriptions::eString, "");

    args.AddDefaultKey("t", "Timeout", "Timeout",
                       CArgDescriptions::eDouble, "60.0");

    args.AddDefaultKey("r", "Retries", "Max HTTP retries",
                       CArgDescriptions::eInteger, "0");

    args.SetUsageContext(GetArguments().GetProgramBasename(), "NAMERD MT test");

    return true;
}


bool CTestApp::TestApp_Init(void)
{
    sm_Service = GetArgs()["svc"].AsString();
    ERR_POST(Info << "Service:  '" << sm_Service << "'");

    sm_ToPost = GetArgs()["post"].AsString();
    ERR_POST(Info << "ToPost:   '" << sm_ToPost << "'");

    sm_Expected = GetArgs()["exp"].AsString();
    ERR_POST(Info << "Expected: '" << sm_Expected << "'");

    sm_Timeout.Set(GetArgs()["t"].AsDouble());
    ERR_POST(Info << "Timeout:  " << sm_Timeout.GetAsDouble());

    sm_Retries = static_cast<unsigned short>(GetArgs()["r"].AsInteger());
    ERR_POST(Info << "Retries:  " << sm_Retries);

    return ! sm_Service.empty()  &&  ! sm_Expected.empty();
}


bool CTestApp::Thread_Run(int idx)
{
    bool    retval = false;
    string  id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    // Setup
    CHttpSession    session;
    CHttpRequest    request(session.NewRequest(CUrl(sm_Service),
                                               CHttpSession::ePost));
    request.SetTimeout(sm_Timeout);
    request.SetRetries(sm_Retries);

    // Send
    if ( ! sm_ToPost.empty()) {
        ERR_POST(Info << "Posting: '" << sm_ToPost << "'");
        request.ContentStream() << sm_ToPost;
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
            ERR_POST(Error << "FAIL: Did not receive any data.");
        } else if (data_out != sm_Expected) {
            ERR_POST(Error << "FAIL: Received data '" << data_out
                           << "' didn't match expected data '"
                           << sm_Expected << "'.");
        } else {
            ERR_POST(Info << "SUCCESS: Received data matched sent data.");
            retval = true;
        }
    }

    PopDiagPostPrefix();

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
