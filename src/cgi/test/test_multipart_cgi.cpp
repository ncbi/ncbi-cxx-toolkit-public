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
* Author:  Aaron Ucko
*
* File Description:
*   testing multipart CGI responses
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiargs.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

USING_NCBI_SCOPE;

class CTestMultipartCgiApplication : public CCgiApplication
{
public:
    void Init(void);
    int  ProcessRequest(CCgiContext& ctx);
};

void CTestMultipartCgiApplication::Init()
{
    CCgiApplication::Init();

    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test of multipart CGI response implementation");
        
    arg_desc->AddDefaultKey("mode", "mode", "Multipart mode",
                            CArgDescriptions::eString, "none");
    arg_desc->SetConstraint
        ("mode",
         &(*new CArgAllow_Strings, "none", "mixed", "related", "replace"));

    SetupArgDescriptions(arg_desc.release());
}

int CTestMultipartCgiApplication::ProcessRequest(CCgiContext& ctx)
{
    const CArgs& args = GetArgs();
    CCgiResponse& response = ctx.GetResponse();
    string mode = args["mode"].AsString();

    if (mode == "mixed") {
        response.SetMultipartMode(CCgiResponse::eMultipart_mixed);
    } else if (mode == "related") {
        response.SetMultipartMode(CCgiResponse::eMultipart_related);
    } else if (mode == "replace") {
        response.SetMultipartMode(CCgiResponse::eMultipart_replace);
    }

    response.WriteHeader();

    response.out() << "main document" << endl;

    if (mode == "mixed") {
        response.BeginPart("attachment.foo", "application/x-foo-bar");
        response.out() << "attachment" << endl;
    } else if (mode == "related") {
        response.BeginPart("more-content.foo", "application/x-foo-bar");
        response.out() << "more content" << endl;
    } else if (mode == "replace") {
        response.EndPart();
        SleepSec(1);
        response.BeginPart(kEmptyStr, "text/html");
        response.out() << "updated document" << endl;
    }
    if (mode != "none") {
        response.EndLastPart();
    }

    return 0;
}

int main(int argc, char** argv)
{
    return CTestMultipartCgiApplication().AppMain(argc, argv);
}
