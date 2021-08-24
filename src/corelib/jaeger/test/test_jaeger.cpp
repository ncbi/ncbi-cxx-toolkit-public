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
#include <corelib/jaeger/jaeger_tracer.hpp>

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

    arg_desc->AddOptionalKey("service",
                            "service",
                            "Jaeger service name",
                            CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


int CJaegerTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    CDiagContext::SetOldPostFormat(false);
    shared_ptr<CJaegerTracer> tracer = args["service"] ?
        make_shared<CJaegerTracer>(args["service"].AsString()) :
        make_shared<CJaegerTracer>();

    CDiagContext::GetRequestContext().SetRequestTracer(tracer);
    GetDiagContext().PrintRequestStart();
    GetDiagContext().PrintRequestStop();
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    return CJaegerTestApp().AppMain(argc, argv);
}
