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
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/submit/Seq_submit.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include "hDiscRep_app.hpp"
#include "hchecking_class.hpp"
#include "hauto_disc_class.hpp"
#include "hDiscRep_config.hpp"

#include <common/test_assert.h>

USING_NCBI_SCOPE;

using namespace ncbi;
using namespace objects;
using namespace DiscRepNmSpc;
using namespace DiscRepAutoNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp;

// Initialization
CRef < CScope >                         CDiscRepInfo :: scope;
string				        CDiscRepInfo :: infile;
vector < CRef < CClickableItem > >      CDiscRepInfo :: disc_report_data;
Str2Strs                                CDiscRepInfo :: test_item_list;
COutputConfig                           CDiscRepInfo :: output_config;
CRef < CSuspect_rule_set >              CDiscRepInfo :: suspect_rules (new CSuspect_rule_set);
vector <string> 	                CDiscRepInfo :: weasels;
CConstRef <CSubmit_block>               CDiscRepInfo :: submit_block = CRef <CSubmit_block>(0);
string                                  CDiscRepInfo :: expand_defline_on_set;
string                                  CDiscRepInfo :: report_lineage;
vector <string>                         CDiscRepInfo :: strandsymbol;
bool                                    CDiscRepInfo :: exclude_dirsub;
string                                  CDiscRepInfo :: report;
Str2UInt                                CDiscRepInfo :: rRNATerms;
vector <string>                         CDiscRepInfo :: bad_gene_names_contained;
vector <string>                         CDiscRepInfo :: no_multi_qual;
vector <string>                         CDiscRepInfo :: short_auth_nms;
vector <string>                         CDiscRepInfo :: spec_words_biosrc;
vector <string>                         CDiscRepInfo :: suspicious_notes;
vector <string>                         CDiscRepInfo :: trna_list;
vector <string>                         CDiscRepInfo :: rrna_standard_name;
Str2UInt                                CDiscRepInfo :: desired_aaList;

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
    arg_desc->AddOptionalKey("report", "ReportType", 
                   "Report type: Discrepancy, Oncaller, TSA, Genome, Big Sequence, MegaReport, Include Tag, Include Tag for Superuser",
                   CArgDescriptions::eString);
*/
    arg_desc->AddDefaultKey("report", "ReportType",
                   "Report type: Discrepancy, Oncaller, TSA, Genome, Big Sequence, MegaReport, Include Tag, Include Tag for Superuser",
                   CArgDescriptions::eString, "Discrepancy");

    arg_desc->AddDefaultKey("o", "OutputFile", "Single output file", 
                                             CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey("S", "SummaryReport", "Summary Report?: 'T'=true, 'F' =false", CArgDescriptions::eBoolean, "F");

    SetupArgDescriptions(arg_desc.release());

    // read suspect rule file
    const CNcbiRegistry& reg = GetConfig();
    string suspect_rule_file = reg.Get("SuspectProductRule", "SuspectRuleFile");
    ifstream ifile(suspect_rule_file.c_str());
    
    if (!ifile) {
       ERR_POST(Warning << "Unable to read syspect prodect rules from " << suspect_rule_file);
       suspect_rule_file = "/ncbi/data/product_rules.prt";
       ifile.open(suspect_rule_file.c_str());
       if (!ifile) 
           ERR_POST(Warning<< "Unable to read syspect prodect rules from "<< suspect_rule_file);
    } 
if (thisInfo.suspect_rules->IsSet())
cerr << " is set\n";
if (thisInfo.suspect_rules->CanGet())
cerr << "can get\n";
       
    if (ifile) {
       auto_ptr <CObjectIStream> ois (CObjectIStream::Open(eSerial_AsnText, suspect_rule_file));
       *ois >> *thisInfo.suspect_rules;
/*
if (thisInfo.suspect_rules->IsSet())
cerr << "222 is set\n";
if (thisInfo.suspect_rules->CanGet())
cerr << "222can get\n";
*/
    }

    // get other parameters from *.ini
    thisInfo.expand_defline_on_set = reg.Get("OncallerTool", "EXPAND_DEFLINE_ON_SET");
    if (thisInfo.expand_defline_on_set.empty())
      NCBI_THROW(CException, eUnknown, "Missing config parameters.");

    // ini. of weasels
    thisInfo.weasels.reserve(11);
    thisInfo.weasels.push_back("candidate");
    thisInfo.weasels.push_back("hypothetical");
    thisInfo.weasels.push_back("novel");
    thisInfo.weasels.push_back("possible");
    thisInfo.weasels.push_back("potential");
    thisInfo.weasels.push_back("predicted"); 
    thisInfo.weasels.push_back("probable"); 
    thisInfo.weasels.push_back("putative");
    thisInfo.weasels.push_back("candidate");  
    thisInfo.weasels.push_back("uncharacterized");  
    thisInfo.weasels.push_back("unique");

    // ini. of strandsymbol
    thisInfo.strandsymbol.reserve(5);
    thisInfo.strandsymbol.push_back("");
    thisInfo.strandsymbol.push_back("");
    thisInfo.strandsymbol.push_back("c");
    thisInfo.strandsymbol.push_back("b");
    thisInfo.strandsymbol.push_back("r");

    // ini. of rRNATerms
    thisInfo.rRNATerms["16S"] = 1000;
    thisInfo.rRNATerms["18S"] = 1000;
    thisInfo.rRNATerms["23S"] = 2000;
    thisInfo.rRNATerms["25s"] = 1000;
    thisInfo.rRNATerms["26S"] = 1000;
    thisInfo.rRNATerms["28S"] = 3300;
    thisInfo.rRNATerms["small"] = 1000;
    thisInfo.rRNATerms["large"] = 1000;
    thisInfo.rRNATerms["5.8S"] = 130;
    thisInfo.rRNATerms["5S"] = 90;

    // ini. of no_multi_qual
    thisInfo.no_multi_qual.reserve(3);
    thisInfo.no_multi_qual.push_back("location");
    thisInfo.no_multi_qual.push_back("taxname");
    thisInfo.no_multi_qual.push_back("taxid");
  
    // ini. of bad_gene_names_contained
    thisInfo.bad_gene_names_contained.reserve(5);
    thisInfo.bad_gene_names_contained.push_back("putative");
    thisInfo.bad_gene_names_contained.push_back("fragment");
    thisInfo.bad_gene_names_contained.push_back("gene");
    thisInfo.bad_gene_names_contained.push_back("orf");
    thisInfo.bad_gene_names_contained.push_back("like");

    // ini. of suspicious notes
    thisInfo.suspicious_notes.reserve(15);
    thisInfo.suspicious_notes.push_back("characterised");
    thisInfo.suspicious_notes.push_back("recognised");
    thisInfo.suspicious_notes.push_back("characterisation");
    thisInfo.suspicious_notes.push_back("localisation");
    thisInfo.suspicious_notes.push_back("tumour");
    thisInfo.suspicious_notes.push_back("uncharacterised");
    thisInfo.suspicious_notes.push_back("oxydase");
    thisInfo.suspicious_notes.push_back("colour");
    thisInfo.suspicious_notes.push_back("localise");
    thisInfo.suspicious_notes.push_back("faecal");
    thisInfo.suspicious_notes.push_back("orthologue");
    thisInfo.suspicious_notes.push_back("paralogue");
    thisInfo.suspicious_notes.push_back("homolog");
    thisInfo.suspicious_notes.push_back("homologue");
    thisInfo.suspicious_notes.push_back("intronless gene");

    // ini. of spec_words_biosrc;
    thisInfo.spec_words_biosrc.reserve(4);
    thisInfo.spec_words_biosrc.push_back("institute");
    thisInfo.spec_words_biosrc.push_back("institution");
    thisInfo.spec_words_biosrc.push_back("University");
    thisInfo.spec_words_biosrc.push_back("College");

    // ini. of trna_list:
    thisInfo.trna_list.reserve(28);
    thisInfo.trna_list.push_back("tRNA-Gap");
    thisInfo.trna_list.push_back("tRNA-Ala");
    thisInfo.trna_list.push_back("tRNA-Asx");
    thisInfo.trna_list.push_back("tRNA-Cys");
    thisInfo.trna_list.push_back("tRNA-Asp");
    thisInfo.trna_list.push_back("tRNA-Glu");
    thisInfo.trna_list.push_back("tRNA-Phe");
    thisInfo.trna_list.push_back("tRNA-Gly");
    thisInfo.trna_list.push_back("tRNA-His");
    thisInfo.trna_list.push_back("tRNA-Ile");
    thisInfo.trna_list.push_back("tRNA-Xle");
    thisInfo.trna_list.push_back("tRNA-Lys");
    thisInfo.trna_list.push_back("tRNA-Leu");
    thisInfo.trna_list.push_back("tRNA-Met");
    thisInfo.trna_list.push_back("tRNA-Asn");
    thisInfo.trna_list.push_back("tRNA-Pyl");
    thisInfo.trna_list.push_back("tRNA-Pro");
    thisInfo.trna_list.push_back("tRNA-Gln");
    thisInfo.trna_list.push_back("tRNA-Arg");
    thisInfo.trna_list.push_back("tRNA-Ser");
    thisInfo.trna_list.push_back("tRNA-Thr");
    thisInfo.trna_list.push_back("tRNA-Sec");
    thisInfo.trna_list.push_back("tRNA-Val");
    thisInfo.trna_list.push_back("tRNA-Trp");
    thisInfo.trna_list.push_back("tRNA-OTHER");
    thisInfo.trna_list.push_back("tRNA-Tyr");
    thisInfo.trna_list.push_back("tRNA-Glx");
    thisInfo.trna_list.push_back("tRNA-TERM");

    // ini. of rrna_standard_name
    thisInfo.rrna_standard_name.reserve(10);
    thisInfo.rrna_standard_name.push_back("5S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("5.8S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("12S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("16S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("18S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("23S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("26S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("28S ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("large subunit ribosomal RNA");
    thisInfo.rrna_standard_name.push_back("small subunit ribosomal RNA");

    // ini. of short_auth_nms
    thisInfo.short_auth_nms.reserve(13);
    thisInfo.short_auth_nms.push_back("de la");
    thisInfo.short_auth_nms.push_back("del");
    thisInfo.short_auth_nms.push_back("de");
    thisInfo.short_auth_nms.push_back("da");
    thisInfo.short_auth_nms.push_back("du");
    thisInfo.short_auth_nms.push_back("dos");
    thisInfo.short_auth_nms.push_back("la");
    thisInfo.short_auth_nms.push_back("le");
    thisInfo.short_auth_nms.push_back("van");
    thisInfo.short_auth_nms.push_back("von");
    thisInfo.short_auth_nms.push_back("der");
    thisInfo.short_auth_nms.push_back("den");
    thisInfo.short_auth_nms.push_back("di");

    // ini. of descred_aaList
    thisInfo.desired_aaList["Ala"] = 1;
    thisInfo.desired_aaList["Asx"] = 0;
    thisInfo.desired_aaList["Cys"] = 1;
    thisInfo.desired_aaList["Asp"] = 1;
    thisInfo.desired_aaList["Glu"] = 1;
    thisInfo.desired_aaList["Phe"] = 1;
    thisInfo.desired_aaList["Gly"] = 1;
    thisInfo.desired_aaList["His"] = 1;
    thisInfo.desired_aaList["Ile"] = 1;
    thisInfo.desired_aaList["Xle"] = 0;
    thisInfo.desired_aaList["Lys"] = 1;
    thisInfo.desired_aaList["Leu"] = 2;
    thisInfo.desired_aaList["Met"] = 1;
    thisInfo.desired_aaList["Asn"] = 1;
    thisInfo.desired_aaList["Pro"] = 1;
    thisInfo.desired_aaList["Gln"] = 1;
    thisInfo.desired_aaList["Arg"] = 1;
    thisInfo.desired_aaList["Ser"] = 2;
    thisInfo.desired_aaList["Thr"] = 1;
    thisInfo.desired_aaList["Val"] = 1;
    thisInfo.desired_aaList["Trp"] = 1;
    thisInfo.desired_aaList["Xxx"] = 0;
    thisInfo.desired_aaList["Tyr"] = 1;
    thisInfo.desired_aaList["Glx"] = 0;
    thisInfo.desired_aaList["Sec"] = 0;
    thisInfo.desired_aaList["Pyl"] = 0;
    thisInfo.desired_aaList["Ter"] = 0;
}






void CDiscRepApp :: CheckThisSeqEntry(CRef <CSeq_entry> seq_entry)
{
    seq_entry->Parentize();

    // Create object manager
    // * We use CRef<> here to automatically delete the OM on exit.
    // * While the CRef<> exists GetInstance() will return the same object.
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    // Create a new scope ("attached" to our OM).
    thisInfo.scope.Reset( new CScope(*object_manager) );
    thisInfo.scope->AddTopLevelSeqEntry(*seq_entry);

    CCheckingClass myChecker;
    myChecker.CheckSeqEntry(seq_entry);

    // go through tests
    CAutoDiscClass autoDiscClass( *(thisInfo.scope), myChecker );
    autoDiscClass.LookAtSeqEntry( *seq_entry );

    // collect disc report
    myChecker.CollectRepData();
    cout << "Number of seq-feats: " << myChecker.GetNumSeqFeats() << endl;

};  // CheckThisSeqEntry()


 

int CDiscRepApp :: Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();

    // input file
    thisInfo.infile = args["i"].AsString();
    auto_ptr <CObjectIStream> ois (CObjectIStream::Open(eSerial_AsnText, 
                                                            thisInfo.infile));
    // Config discrepancy report
    thisInfo.report = args["report"].AsString();
    string output_f = args["o"].AsString();
    if (output_f.empty()) {
       vector <string> infile_path;
       infile_path = NStr::Tokenize(thisInfo.infile, "/", infile_path);
       strtmp = infile_path[infile_path.size()-1];
       size_t pos = strtmp.find(".");
       if (pos != string::npos) strtmp = strtmp.substr(0, pos);
       infile_path[infile_path.size()-1] = strtmp + ".dr";
       output_f = NStr::Join(infile_path, "/");
    }
    thisInfo.output_config.output_f.open(output_f.c_str());
    thisInfo.output_config.summary_report = args["S"].AsBoolean();

    CRepConfig* rep_config = CRepConfig::factory(thisInfo.report);
    rep_config->Init();
    rep_config->ConfigRep();

    // read input file and go tests
    CRef <CSeq_entry> seq_entry (new CSeq_entry);
    CRef <CSeq_submit> seq_submit (new CSeq_submit);
    set <const CTypeInfo*> known_tp;
    known_tp.insert(CSeq_submit::GetTypeInfo());
    set <const CTypeInfo*> matching_tp = ois->GuessDataType(known_tp);
    if (matching_tp.size() == 1 && *(matching_tp.begin()) == CSeq_submit :: GetTypeInfo()) {
         *ois >> *seq_submit;
         thisInfo.submit_block.Reset();
         thisInfo.submit_block = CConstRef <CSubmit_block> (&(seq_submit->GetSub()));
         if (seq_submit->IsEntrys()) {
             ITERATE (list <CRef <CSeq_entry> >, it, seq_submit->GetData().GetEntrys()) {
                CheckThisSeqEntry(*it);
             } 
         }
    }
    else {
        *ois >> *seq_entry;
        CheckThisSeqEntry(seq_entry);
/*
      // Create a new scope ("attached" to our OM).
      thisInfo.scope.Reset( new CScope(*object_manager) );
      thisInfo.scope->AddTopLevelSeqEntry(*seq_entry);
*/
    }

/*
CSeq_entry_Handle seq_entry_h = thisInfo.scope->GetSeq_entryHandle(*seq_entry);
unsigned i=0;
for (CFeat_CI feat_it(seq_entry_h); feat_it; ++feat_it)
  i++;
cerr << "CFeat_CI.size()   " << i << endl;
*/


/*
    CCheckingClass myChecker( thisInfo.scope );
    myChecker.CheckSeqEntry(seq_entry);

    // go through tests
    CAutoDiscClass autoDiscClass( myChecker );
    autoDiscClass.LookAtSeqEntry( *seq_entry );

    // output disc report
    myChecker.CollectRepData();
*/
    // output disc report
    rep_config->Export();

    cout << "disc_rep.size()  " << thisInfo.disc_report_data.size() << endl;

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
