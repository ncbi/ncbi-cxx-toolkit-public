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
 *   Main() of Cpp Discrepany Report, stand-alone asndisc application
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(DiscRepNmSpc);

static string       strtmp, tmp;

class CDiscRepApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};

void CDiscRepApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                                "C++ Discrepancy Report");
    DisableArgDescriptions();

    // Pass argument descriptions to the application
    //
    arg_desc->AddOptionalKey("p", "InPath", "Path to ASN.1 Files", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("i", "InputFile", "Single input file (mandatory)", 
                                               CArgDescriptions::eString);
    arg_desc->AddOptionalKey("o", "OutputFile","Single output file",CArgDescriptions::eString);
    arg_desc->AddDefaultKey("x", "Suffix", "File Selection Substring", 
                                                        CArgDescriptions::eString, ".sqn");
    arg_desc->AddDefaultKey("u", "Recurse", "Recurse", CArgDescriptions::eString, "0");
    arg_desc->AddOptionalKey("e", "EnableTests", "List of enabled tests, seperated by ','",
                              CArgDescriptions::eString); 
    arg_desc->AddOptionalKey("d", "DisableTests", "List of disabled tests, seperated by ','",
                              CArgDescriptions::eString);
    arg_desc->AddDefaultKey("s", "OutputFileSuffix", "Output File Suffix", 
                              CArgDescriptions::eString, ".dr");
    arg_desc->AddOptionalKey("r", "OutPath", "Output Directory", CArgDescriptions::eString);
 
    arg_desc->AddDefaultKey("a", "Asn1Type", 
                 "Asn.1 Type: a: Any, e: Seq-entry, b: Bioseq, s: Bioseq-set, m: Seq-submit, t: Batch Bioseq-set, u: Batch Seq-submit, c: Catenated seq-entry",
                 CArgDescriptions::eString, "a");
    arg_desc->AddDefaultKey("b", "BatchBinary", "Batch File is Binary: 'T' = true, 'F' = false", 
                              CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("P", "ReportType",
                   "Report type: Asndisc, Discrepancy, Oncaller, TSA, Genome, Big Sequence, MegaReport, Include Tag, Include Tag for Superuser",
                   CArgDescriptions::eString, "Asndisc");

    arg_desc->AddDefaultKey("S", "SummaryReport", 
                            "Summary Report: 'T'=true, 'F' =false", 
                            CArgDescriptions::eBoolean, "F");

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};

int CDiscRepApp :: Run(void)
{
    // Crocess command line args:  get GI to load
    const CArgs& args = GetArgs();
    
    try {
       string report = args["P"].AsString();
       if (report == "t" || report == "s") report = "Asndisc";
       CRef <DiscRepNmSpc::CRepConfig> 
           config( DiscRepNmSpc::CRepConfig :: factory(report) );
       CMetaRegistry:: SEntry entry = CMetaRegistry :: Load("disc_report.ini");
       CRef <IRWRegistry> reg(entry.registry);
       config->InitParams(*reg);
       config->ReadArgs(args);
       config->CollectTests();
       config->Run(config);

       return 0;
    }
    catch (CException& eu) {
       string err_msg(eu.GetMsg());
       if (err_msg == "Input path or input file must be specified") {
          err_msg = "You need to supply at least an input file (-i) or a path in which to find input files (-p). Please see 'asndisc -help' for additional details.";
       }
       ERR_POST(err_msg);
    } 
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
     ERR_POST(eu.GetMsg());
  }
}
