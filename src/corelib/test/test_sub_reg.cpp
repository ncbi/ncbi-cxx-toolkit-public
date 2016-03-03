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
*   Test to ensure that composite registries work as expected, with no
*   strange corner cases.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


class CTestSubRegApp : public CNcbiApplication
{
    void Init();
    int  Run();
};


void CTestSubRegApp::Init()
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "test of proper subregistry handling");

    arg_desc->AddOptionalKey
        ("overrides", "IniFile", "source of overriding extra configuration",
         CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("defaults", "IniFile", "source of non-overriding extra configuration",
         CArgDescriptions::eInputFile);

    // For historical reasons, the registry system explicitly writes
    // out platform-specific newlines; forcing "binary" output avoids
    // doubled CRs on Windows (and consequent test failures).
    arg_desc->AddDefaultKey
        ("out", "IniDump", "destination for merged registries",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);
    arg_desc->AddDefaultKey
        ("out2", "IniDump", "destination for 2nd run output",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);

    SetupArgDescriptions(arg_desc.release());
}


int CTestSubRegApp::Run()
{
    const CArgs& args = GetArgs();
    SetDiagTrace(eDT_Enable, eDT_Enable);
    SetDiagPostLevel(eDiagSevMin);
    CNcbiRegistry& reg = GetConfig();
    
    // At the first run we read from all the different sources. At the second
    // run we only read the file that was this test's output at the first run
    // (this behavior is defined in the test script, not here)
    if (args["overrides"] && args["defaults"]) {
        LoadConfig(reg, &args["overrides"].AsString(), 0);
        reg.Read(args["defaults"].AsInputFile(), IRegistry::fNoOverride);
        reg.Set("Log", "my_entry", "my_value", 0,
        "This is a comment for \"my_entry\" entry");
        reg.SetComment("This is a hardcoded comment for \"Log\" section", 
                       "Log");
        reg.SetComment("This is a hardcoded comment for \"Diag\" section", 
                        "Diag", "");
        reg.SetComment("This is a hardcoded inner comment for \"Diag\" "
                       "section", "Diag", IRegistry::sm_InSectionCommentName);
        string inherits = reg.Get("NCBI", ".Inherits");
        reg.Unset("NCBI", ".Inherits"); // For the 2nd run not to 
                                        // read files again
        inherits = reg.Get("NCBI", ".Inherits");
    }

    reg.Write(args["out"].AsOutputFile(),
              IRegistry::fCoreLayers | IRegistry::fCountCleared);
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, char** argv)
{
    return CTestSubRegApp().AppMain(argc, argv, 0, eDS_ToStderr);
}
