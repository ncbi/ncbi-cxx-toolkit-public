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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   HTTP session test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/request_status.hpp>
#include <connect/ncbi_http_session.hpp>

#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


class CNCBITestHttpSessionApp : public CNcbiApplication
{
public:
    CNCBITestHttpSessionApp(void);

public:
    int      Run (void);

private:
    istream* GetStream(const string& url);
    void     UseStream(istream* stream);

    CHttpSession        m_Session;
    CRef<CHttpResponse> m_Response;
};


CNCBITestHttpSessionApp::CNCBITestHttpSessionApp(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
}


int CNCBITestHttpSessionApp::Run(void)
{
    {{
        CHttpSession  session;
        CHttpRequest  request
            = session.NewRequest("https://www.ncbi.nlm.nih.gov/Service/404");
        CHttpResponse response = request.Execute();
        _ASSERT(response.GetStatusCode() == CRequestStatus::e404_NotFound);
        NcbiStreamCopyThrow(cout, response.ErrorStream());
    }}
    {{
        CHttpSession  session;
        CHttpRequest  request
            = session.NewRequest("https://no_such_server.ncbi.nlm.nih.gov");
        CHttpResponse response = request.Execute();
        CNcbiIstream& in = response.ErrorStream();
        _ASSERT(!in.good());
    }}
    {{
        CHttpSession  session;
        CHttpRequest  request
            = session.NewRequest("http://www.ncbi.nlm.nih.gov/Service/dispd.cgi?service=test");
        CHttpResponse response = request.Execute();
        _ASSERT(response.GetStatusCode() == CRequestStatus::e200_Ok);
        CNcbiIstream& in = response.ContentStream();
        NcbiStreamCopyThrow(cout, in);
    }}
    {{
        CHttpSession  session;
        CHttpRequest  request
            = session.NewRequest("http://www.ncbi.nlm.nih.gov/Service/index.html");
        CHttpResponse response = request.Execute();
        _ASSERT(response.GetStatusCode() == CRequestStatus::e200_Ok);
        CNcbiIstream& in = response.ContentStream();
        NcbiStreamCopyThrow(cout, in);
    }}

    istream* stream = GetStream("http://www.ncbi.nlm.nih.gov");
    UseStream(stream);

    return 0;
}


istream* CNCBITestHttpSessionApp::GetStream(const string& url)
{
    CHttpRequest request = m_Session.NewRequest(url);

    m_Response = new CHttpResponse(request.Execute());
    _ASSERT(m_Response->GetStatusCode() == CRequestStatus::e200_Ok);

    return &m_Response->ContentStream();
}


void CNCBITestHttpSessionApp::UseStream(istream* stream)
{
    // Make sure the stack gets clobbered enough
    volatile char a[2 * sizeof(CHttpRequest)];
    memset((void*) a, '\xA5', sizeof(a));

    stream->exceptions(IOS_BASE::failbit);
    NcbiStreamCopyThrow(cout, *stream);
}


int main(int argc, const char* argv[])
{
    return CNCBITestHttpSessionApp().AppMain(argc, argv);
}
