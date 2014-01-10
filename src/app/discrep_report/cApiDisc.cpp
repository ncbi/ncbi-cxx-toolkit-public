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
 *   Example of integrated discrepancy report
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

void GetDiscrepancyReport(int argc, const char* argv[])
{
    if (argc < 2) {
       NCBI_THROW(CException, eUnknown,
                  "Input path or input file must be specified");
    }
    vector <string> arr;
    string in_file(kEmptyStr);
    for (int i=1; i< argc; i++) {
       arr = NStr::Tokenize(argv[i], "=", arr); 
       if (arr[0] == "i") {
           in_file = arr[1];
           break;
       }
    }

    if (in_file.empty()) {
         NCBI_THROW(CException, eUnknown,
                  "Input path or input file must be specified");
    }
    auto_ptr <CObjectIStream> ois(CObjectIStream::Open(eSerial_AsnText,in_file));
    string strtmp = ois->ReadFileHeader();
    ois->SetStreamPos(0);
    CRef <CSeq_submit> seq_submit (new CSeq_submit);
    CRef <CSeq_entry> seq_entry (new CSeq_entry);
    if (strtmp == "Seq-submit") {
       *ois >> *seq_submit;
       if (seq_submit->IsEntrys()) {
         NON_CONST_ITERATE (list <CRef <CSeq_entry> >, it, 
                                       seq_submit->SetData().SetEntrys()) {
               seq_entry.Reset(&(**it));
         }
       }
    }
    else if (strtmp == "Seq-entry") {
       *ois >> *seq_entry;
    }

    ois->Close();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef <CScope> scope (new CScope(*object_manager));
    scope->AddTopLevelSeqEntry(*seq_entry);   
    CSeq_entry_Handle seq_handle = scope->GetSeq_entryHandle(*seq_entry);

    CRef <CRepConfig> 
          config (CRepConfig::factory((string)"Discrepancy", &seq_handle));
/*
    CRef <CRepConfig> 
          config (CRepConfig::factory((string)"Oncaller", &seq_handle));
*/

    CRef <IRWRegistry> reg(0); 
    if (CFile("disc_report.ini").Exists()) {
        CMetaRegistry:: SEntry entry = CMetaRegistry :: Load("disc_report.ini");
        reg.Reset(entry.registry); 
    }
    config->InitParams(reg);
    config->CollectTests();
    config->Run();
    vector <CRef <CClickableText> > item_list;
    CDiscRepOutput output_obj;
    output_obj.Export(item_list);
};

int main(int argc, const char* argv[])
{
// Usage disc_report i=data_file

   SetDiagFilter(eDiagFilter_Post, "Structured Comment");

   try {
      SetDiagPostLevel(eDiag_Error);
      GetDiscrepancyReport(argc, argv);
      cerr << "Program existed normally\n";
   }
   catch (CException& eu) {
       string err_msg(eu.GetMsg());
       if (err_msg == "Input path or input file must be specified") {
            err_msg = "You need to supply an input file (i=myfild)";
//            err_msg = "You need to supply at least an input file (i=) or a path in which to find input files (p=). Please see 'asndisc -help' for additional details, and use the format arg=arg_value to input arguments.";
       }
       ERR_POST(err_msg);
   };
}

