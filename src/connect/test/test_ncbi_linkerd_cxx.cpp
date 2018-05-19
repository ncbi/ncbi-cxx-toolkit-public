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
 *   Test Linkerd via C++ classes
 *
 */

#include <ncbi_pch.hpp>     // This header must go first

#include <corelib/ncbiapp.hpp>

#include "test_assert.h"    // This header must go last


USING_NCBI_SCOPE;


class CTestNcbiLinkerdCxxApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CTestNcbiLinkerdCxxApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test Linkerd via C++ classes");

    SetupArgDescriptions(arg_desc.release());
}


int CTestNcbiLinkerdCxxApp::Run(void)
{
    return 0;
}


int main(int argc, char* argv[])
{
    return CTestNcbiLinkerdCxxApp().AppMain(argc, argv);
}
