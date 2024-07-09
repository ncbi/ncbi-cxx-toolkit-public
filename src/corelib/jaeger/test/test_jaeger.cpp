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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Jaeger tracer test application.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/jaeger/jaeger_tracer.hpp>

#include <common/ncbi_sanitizers.h>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CJaegerTestApp::
//

class CJaegerTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CJaegerTestApp::Init()
{
    CNcbiApplication::Init();

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Jaeger test application");

    arg_desc->AddOptionalKey("service", "service", "Jaeger service name",
        CArgDescriptions::eString);
    arg_desc->AddDefaultKey("max_depth", "max_depth", "Max sub-process depth",
        CArgDescriptions::eInteger, "2");
    arg_desc->AddDefaultKey("spawn", "spawn", "Number of processes to spawn",
        CArgDescriptions::eInteger, "2");

    arg_desc->AddOptionalKey("phid", "phid", "Parent PHID",
        CArgDescriptions::eString);
    arg_desc->AddDefaultKey("depth", "depth", "Current sub-process depth",
        CArgDescriptions::eInteger, "0");

    SetupArgDescriptions(arg_desc.release());
}


int CJaegerTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    CDiagContext::SetOldPostFormat(false);

    int max_depth = args["max_depth"].AsInteger();
    int depth = args["depth"].AsInteger();
    int spawn = args["spawn"].AsInteger();

    string service;
    if ( args["service"] ) service = args["service"].AsString();
    shared_ptr<CJaegerTracer> tracer = !service.empty() ?
        make_shared<CJaegerTracer>(service) :
        make_shared<CJaegerTracer>();

    string phid;
    if (args["phid"]) {
        phid = args["phid"].AsString();
        GetDiagContext().SetDefaultHitID(phid);
    }
    else if ( depth == 0 ) {
        phid = GetDiagContext().GetDefaultHitID();
    }

    CRequestContext::SetRequestTracer(tracer);
    GetDiagContext().PrintRequestStart();
    if (depth < max_depth) {
        for (int i = 0; i < spawn; ++i) {
            string cmd = GetProgramExecutablePath();
            if ( !service.empty() ) cmd += " -service " + service;
            cmd += " -max_depth " + NStr::NumericToString(max_depth);
            cmd += " -depth " + NStr::NumericToString(depth + 1);
            cmd += " -phid " + CDiagContext::GetRequestContext().GetHitID() + "." + NStr::NumericToString(i + 1);
            try {
                CExec::System(cmd.c_str());
            }
            catch (...) {}
        }
    }
    GetDiagContext().PrintRequestStop();
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
#if defined(NCBI_USE_TSAN)
    cout << "This test is disabled to run under thread sanitizer, "
         << "it constantly fails with memory allocation errors" 
         << endl
         << "NCBI_UNITTEST_DISABLED" 
         << endl;
    return 0;
#endif
    return CJaegerTestApp().AppMain(argc, argv);
}
