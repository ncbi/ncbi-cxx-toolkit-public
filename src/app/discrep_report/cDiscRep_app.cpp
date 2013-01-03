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
static list <string> strs;

// Initialization
CRef < CScope >                         CDiscRepInfo :: scope;
string				        CDiscRepInfo :: infile;
vector < CRef < CClickableItem > >      CDiscRepInfo :: disc_report_data;
Str2Strs                                CDiscRepInfo :: test_item_list;
COutputConfig                           CDiscRepInfo :: output_config;
CRef < CSuspect_rule_set >              CDiscRepInfo :: suspect_rules (new CSuspect_rule_set);
vector <string> 	                CDiscRepInfo :: weasels;
CConstRef <CSeq_submit>                 CDiscRepInfo :: seq_submit = CConstRef <CSeq_submit>();
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
CTaxon1                                 CDiscRepInfo :: tax_db_conn;
list <string>                           CDiscRepInfo :: state_abbrev;
Str2Str                                 CDiscRepInfo :: cds_prod_find;
vector <string>                         CDiscRepInfo :: s_pseudoweasels;
vector <string>                         CDiscRepInfo :: suspect_rna_product_names;
string                                  CDiscRepInfo :: kNonExtendableException;
vector <string>                         CDiscRepInfo :: new_exceptions;
Str2Str	                                CDiscRepInfo :: srcqual_keywords;
vector <string>                         CDiscRepInfo :: kIntergenicSpacerNames;
vector <string>                         CDiscRepInfo :: taxnm_env;
vector <string>                         CDiscRepInfo :: virus_lineage;

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

    // ini. of srcqual_keywords
    strs.clear();
    reg.EnumerateEntries("Srcqual-keywords", &strs);
    ITERATE (list <string>, it, strs) {
      thisInfo.srcqual_keywords[*it]= reg.Get("Srcqual-keywords", *it);
    }
    
    // ini. of s_pseudoweasels
    strtmp = reg.Get("StringVecIni", "SPseudoWeasels");
    thisInfo.s_pseudoweasels = NStr::Tokenize(strtmp, ",", thisInfo.s_pseudoweasels);

    // ini. of suspect_rna_product_names
    strtmp = reg.Get("StringVecIni", "SuspectRnaProdNms");
    thisInfo.suspect_rna_product_names 
        = NStr::Tokenize(strtmp, ",", thisInfo.suspect_rna_product_names);

    // ini. of kNonExtendableException
    thisInfo.kNonExtendableException = reg.Get("SingleDataIni", "KNonExtExc");

    // ini. of new_exceptions
    strtmp = reg.Get("StringVecIni", "NewExceptions");
    thisInfo.new_exceptions = NStr::Tokenize(strtmp, ",", thisInfo.new_exceptions);

    // ini. of kIntergenicSpacerNames
    strtmp = reg.Get("StringVecIni", "KIntergenicSpacerNames");
    thisInfo.kIntergenicSpacerNames 
                  = NStr::Tokenize(strtmp, ",", thisInfo.kIntergenicSpacerNames);
    
    // ini. of weasels
    strtmp = reg.Get("StringVecIni", "Weasels");
    thisInfo.weasels = NStr::Tokenize(strtmp, ",", thisInfo.weasels);

    // ini. of strandsymbol
    thisInfo.strandsymbol.reserve(5);
    thisInfo.strandsymbol.push_back("");
    thisInfo.strandsymbol.push_back("");
    thisInfo.strandsymbol.push_back("c");
    thisInfo.strandsymbol.push_back("b");
    thisInfo.strandsymbol.push_back("r");

    // ini. of rRNATerms
    reg.EnumerateEntries("RRna-terms", &strs);
    ITERATE (list <string>, it, strs) {
       strtmp = (*it == "5-8S") ? "5.8S" : *it;
       thisInfo.rRNATerms[strtmp] = NStr::StringToUInt(reg.Get("RRna-terms", *it));
    }

    // ini. of no_multi_qual
    strtmp = reg.Get("StringVecIni", "NoMultiQual");
    thisInfo.no_multi_qual = NStr::Tokenize(strtmp, ",", thisInfo.no_multi_qual);
  
    // ini. of bad_gene_names_contained
    strtmp = reg.Get("StringVecIni", "BadGeneNamesContained");
    thisInfo.bad_gene_names_contained 
                    = NStr::Tokenize(strtmp, ",", thisInfo.bad_gene_names_contained);

    // ini. of suspicious notes
    strtmp = reg.Get("StringVecIni", "Suspicious_notes");
    thisInfo.suspicious_notes = NStr::Tokenize(strtmp, ",", thisInfo.suspicious_notes);

    // ini. of spec_words_biosrc;
    strtmp = reg.Get("StringVecIni", "SpecWordsBiosrc");
    thisInfo.spec_words_biosrc = NStr::Tokenize(strtmp, ",", thisInfo.spec_words_biosrc);

    // ini. of trna_list:
    strtmp = reg.Get("StringVecIni", "TrnaList");
    thisInfo.trna_list = NStr::Tokenize(strtmp, ",", thisInfo.trna_list);

    // ini. of rrna_standard_name
    strtmp = reg.Get("StringVecIni", "RrnaStandardName");
    thisInfo.rrna_standard_name = NStr::Tokenize(strtmp, ",", thisInfo.rrna_standard_name);

    // ini. of short_auth_nms
    strtmp = reg.Get("StringVecIni", "ShortAuthNms");
    thisInfo.short_auth_nms = NStr::Tokenize(strtmp, ",", thisInfo.short_auth_nms);

    // ini. of descred_aaList
    reg.EnumerateEntries("Descred_aaList", &strs);
    ITERATE (list <string>, it, strs)
       thisInfo.desired_aaList[*it] = NStr::StringToUInt(reg.Get("Descred_aaList", *it));

    // ini. of tax_db_conn: taxonomy db connection
    thisInfo.tax_db_conn.Init();

    // ini. of usa_state_abbv;
    reg.EnumerateEntries("US-state-abbrev-fixes", &(thisInfo.state_abbrev));

    // ini. of cds_prod_find
    strs.clear();
    reg.EnumerateEntries("Cds-product-find", &strs);
    ITERATE (list <string>, it, strs) {
      strtmp = ( *it == "related-to") ? "related to" : *it;
      thisInfo.cds_prod_find[strtmp]= reg.Get("Cds-product-find", *it);
    }

    // ini. of taxnm_env
    strtmp = reg.Get("StringVecIni", "TaxnameEnv");
    thisInfo.taxnm_env = NStr::Tokenize(strtmp, ",", thisInfo.taxnm_env);

    // ini. of virus_lineage
    strtmp = reg.Get("StringVecIni", "VirusLineage");
    thisInfo.virus_lineage = NStr::Tokenize(strtmp, ",", thisInfo.virus_lineage); 
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
    CSeq_submit tmp_seq_submit;
    set <const CTypeInfo*> known_tp;
    known_tp.insert(CSeq_submit::GetTypeInfo());
    set <const CTypeInfo*> matching_tp = ois->GuessDataType(known_tp);
    if (matching_tp.size() == 1 && *(matching_tp.begin()) == CSeq_submit :: GetTypeInfo()) {
         *ois >> tmp_seq_submit;
         thisInfo.seq_submit = CConstRef <CSeq_submit> (&tmp_seq_submit);
         if (thisInfo.seq_submit->IsEntrys()) {
             ITERATE (list <CRef <CSeq_entry> >,it,thisInfo.seq_submit->GetData().GetEntrys())
                CheckThisSeqEntry(*it);
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


void CDiscRepApp::Exit(void)
{
   thisInfo.tax_db_conn.Fini();
};


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
