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
* Author: Michael Kholodov
*
* File Description:
*   String representation of the database character types.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include "dbapi_advanced_features_common.hpp"


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CDbapiAdvancedFeaturesApp::

class CDbapiAdvancedFeaturesApp : public CNcbiApplication
{
private:
    virtual void Init();
    virtual int  Run();
    virtual void Exit(void) { m_Sample.Exit(); };
    
    CNcbiSample_Dbapi_Advanced_Features m_Sample;

    void ParseArgs(CNcbiSample_Dbapi_Advanced_Features& sample);
};


/////////////////////////////////////////////////////////////////////////////
//  Init - setup command-line arguments

void CDbapiAdvancedFeaturesApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DBAPI Advanced Features sample");

#if defined(NCBI_OS_MSWIN)
   arg_desc->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "MS_DEV1");
   arg_desc->AddDefaultKey("d", "string",
                           "Driver <ctlib|dblib|ftds|odbc>",
                           CArgDescriptions::eString, 
                           "odbc");
#else
	arg_desc->AddDefaultKey("s", "string",
                            "Server name",
                            CArgDescriptions::eString, "TAPER");

	arg_desc->AddDefaultKey("d", "string",
                            "Driver <ctlib|dblib|ftds>",
                            CArgDescriptions::eString, 
                            "ctlib");
#endif
    SetupArgDescriptions(arg_desc.release());
   
    // Init sample logic
    m_Sample.Init();
}


/////////////////////////////////////////////////////////////////////////////
//  Run - i.e. parse command-line arguments and demo simple operations.

int CDbapiAdvancedFeaturesApp::Run() 
{
    // Get user parameters
    ParseArgs(m_Sample);

    // Execute sample logic
    int exit_code = m_Sample.Run();

    return exit_code;
}


/////////////////////////////////////////////////////////////////////////////
//  ParseArgs - parse the command-line arguments.

void CDbapiAdvancedFeaturesApp::ParseArgs(CNcbiSample_Dbapi_Advanced_Features& sample)
{
    const CArgs& args = GetArgs();
    
    sample.m_ServerName = args["s"].AsString();
    sample.m_DriverName = args["d"].AsString();
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CDbapiAdvancedFeaturesApp().AppMain(argc, argv);
}
