/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Main() of Cpp Discrepany Report
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include "hDiscRep_app.hpp"
#include "hDiscRep_config.hpp"

USING_NCBI_SCOPE;

using namespace DiscRepNmSpc;

static string       strtmp, tmp;

void CDiscRepApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                                "Cpp Discrepancy Report");
    DisableArgDescriptions();

    // Pass argument descriptions to the application
    //
    arg_desc->AddKey("i", "InputFile", "Single input file (mandatory)", 
                                               CArgDescriptions::eString);
    // how about report->p?
/*
    arg_desc->AddOptionalKey("P", "ReportType", 
                   "Report type: Discrepancy, Oncaller, TSA, Genome, Big Sequence, MegaReport, Include Tag, Include Tag for Superuser",
                   CArgDescriptions::eString);
*/
    arg_desc->AddDefaultKey("P", "ReportType",
                   "Report type: Discrepancy, Oncaller, TSA, Genome, Big Sequence, MegaReport, Include Tag, Include Tag for Superuser",
                   CArgDescriptions::eString, "Discrepancy");

    arg_desc->AddDefaultKey("o", "OutputFile", "Single output file", 
                                             CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey("S", "SummaryReport", "Summary Report?: 'T'=true, 'F' =false", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("e", "EnableTests", "List of enabled tests, seperated by ','",
                            CArgDescriptions::eString, ""); 
    arg_desc->AddDefaultKey("d", "DisableTests", "List of disabled tests, seperated by ','",
                              CArgDescriptions::eString, "");

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};

int CDiscRepApp :: Run(void)
{
    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    
    string report = args["P"].AsString();
    if (report == "t" || report == "s") report = "Discrepancy";
    CRepConfig* config = CRepConfig::factory(report);
    config->InitParams(GetConfig());
    config->ReadArgs(args);
    config->Run();
    config->Export();
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
//  SetDiagTrace(eDT_Enable);
  SetDiagPostLevel(eDiag_Error);

  try {
    return CDiscRepApp().AppMain(argc, argv);
  } catch(CException& eu) {
     ERR_POST( "throw an error: " << eu.GetMsg());
  }
}
