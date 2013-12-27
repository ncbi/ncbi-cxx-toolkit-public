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
 *   Example of an integrated simple asndisc
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbireg.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(DiscRepNmSpc);


/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

void GetAsndiscReport(int argc, const char* argv[])
{
    vector <string> arr;
    Str2Str args;
    for (unsigned i=1; i< argc; i++) {
      arr.clear();
      arr = NStr::Tokenize(argv[i], "=", arr);
      args[arr[0]] = arr[1];  
    }

    CRef <CRepConfig> config (CRepConfig::factory("Asndisc"));
    config->ProcessArgs(args);

    if (!CFile("disc_report.ini").Exists()) {
           NCBI_THROW(CException, eUnknown,
                   "Configuration file disc_report.ini is missing.");
    }
    CMetaRegistry:: SEntry entry = CMetaRegistry :: Load("disc_report.ini");
    CRef <IRWRegistry> reg(entry.registry); 
    config->InitParams(*reg);
    config->CollectTests();
    config->Run(config);
};

int main(int argc, const char* argv[])
{
    try {
       SetDiagPostLevel(eDiag_Error);
       GetAsndiscReport(argc, argv);
    }
    catch (CException& eu) {
       string err_msg(eu.GetMsg());
       if (err_msg == "Input path or input file must be specified") {
            err_msg = "You need to supply an input file (i=myfile).";
//            err_msg = "You need to supply at least an input file (i=) or a path in which to find input files (p=). Please see 'asndisc -help' for additional details, and use the format arg=arg_value to input arguments.";
       }
       ERR_POST(err_msg);
    }
}

