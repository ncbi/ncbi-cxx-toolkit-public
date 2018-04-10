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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Plain example of a CGI/FastCGI application.
 *
 *   USAGE:  sample.cgi?message=Some+Message
 *
 *   NOTE:
 *     1) needs HTML template file "cgi.html" in curr. dir to run,
 *     2) needs configuration file "cgi.ini",
 *     3) on most systems, you must make sure the executable's extension
 *        is '.cgi'.
 *
 */

#include <ncbi_pch.hpp>
#include <sys/time.h>
#include <sys/resource.h>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <sample/app/deployable_cgi/deployable_cgi.hpp>

using namespace ncbi;

#define NEED_SET_DEPLOYMENT_UID

/////////////////////////////////////////////////////////////////////////////
//  CCgiSampleApplication::
//

void CCgiSampleApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
}


void CCgiSampleApplication::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CGI sample application");

    arg_desc->AddOptionalKey("message",
                             "message",
                             "Message passed to CGI application",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CCgiSampleApplication::x_LookAtArgs()
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request

    if ( args["message"] ) {
        // get the first "message" argument only...
        const string& m = args["message"].AsString();
        (void) m.c_str(); // just get rid of compiler warning "unused 'm'"

        // ...or get the whole list of "message" arguments
        const CArgValue::TStringArray& values = 
            args["message"].GetStringList();

        ITERATE(CArgValue::TStringArray, it, values) {
            // do something with the message
            // (void) it->c_str(); // eg
        } 
    } else {
        // no "message" argument is present
    }
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CCgiSampleApplication().AppMain(argc, argv);
}
