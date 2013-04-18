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
#include <objects/valerr/ValidError.hpp>

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
#include <sstream>

#include "hDiscRep_app.hpp"
#include "hchecking_class.hpp"
#include "hauto_disc_class.hpp"
#include "hDiscRep_config.hpp"
#include "hUtilib.hpp"

#include <common/test_assert.h>

USING_NCBI_SCOPE;

using namespace DiscRepNmSpc;
using namespace DiscRepAutoNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp;
static list <string> strs;
static vector <string> arr;

// Initialization
CRef < CScope >                         CDiscRepInfo :: scope;
string				        CDiscRepInfo :: infile;
vector < CRef < CClickableItem > >      CDiscRepInfo :: disc_report_data;
Str2Strs                                CDiscRepInfo :: test_item_list;
COutputConfig                           CDiscRepInfo :: output_config;
CRef < CSuspect_rule_set >              CDiscRepInfo :: suspect_prod_rules;
vector <string> 	                CDiscRepInfo :: weasels;
CConstRef <CSeq_submit>                 CDiscRepInfo :: seq_submit = CConstRef <CSeq_submit>();
string                                  CDiscRepInfo :: expand_defline_on_set;
string                                  CDiscRepInfo :: report_lineage;
vector <string>                         CDiscRepInfo :: strandsymbol;
bool                                    CDiscRepInfo :: exclude_dirsub;
string                                  CDiscRepInfo :: report;
Str2UInt                                CDiscRepInfo :: rRNATerms;
Str2UInt                                CDiscRepInfo :: rRNATerms_partial;
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
vector <string>                         CDiscRepInfo :: strain_tax;
CRef <CComment_set>                     CDiscRepInfo :: comment_rules;
Str2UInt                                CDiscRepInfo :: spell_data;
Str2Str                                 CDiscRepInfo :: fix_data;
CRef <CSuspect_rule_set>                CDiscRepInfo :: orga_prod_rules;
vector <string>                         CDiscRepInfo :: skip_bracket_paren;
vector <string>                         CDiscRepInfo :: ok_num_prefix;
map <EMacro_feature_type, CSeqFeatData::ESubtype> CDiscRepInfo :: feattype_featdef;
map <CRna_feat_type::E_Choice, CRNA_ref::EType>  CDiscRepInfo :: rnafeattp_rnareftp;
map <ERna_field, EFeat_qual_legal>      CDiscRepInfo :: rnafield_featquallegal;
map <CSeq_inst::EMol, string>           CDiscRepInfo :: mol_molname;
map <CSeq_inst::EStrand, string>        CDiscRepInfo :: strand_strname;
map <EDBLink_field_type, string>        CDiscRepInfo :: dblink_name;
map <ESource_qual, COrgMod::ESubtype>   CDiscRepInfo :: srcqual_orgmod;
map <ESource_qual, CSubSource::ESubtype>   CDiscRepInfo :: srcqual_subsrc;
map <EFeat_qual_legal, string>          CDiscRepInfo :: featquallegal_name;
map <EFeat_qual_legal, unsigned>        CDiscRepInfo :: featquallegal_subfield;
map <ESequence_constraint_rnamol, CMolInfo::EBiomol>   CDiscRepInfo :: scrna_mirna;
map <string, string>                    CDiscRepInfo :: pubclass_qual;
vector <string>                         CDiscRepInfo :: months;
map <EMolecule_type, CMolInfo::EBiomol> CDiscRepInfo :: moltp_biomol;
map <ETechnique_type, CMolInfo::ETech>  CDiscRepInfo :: techtp_mitech;

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

    string suspect_rule_file = reg.Get("RuleFiles", "SuspectRuleFile");
    ifstream ifile(suspect_rule_file.c_str());
    
    if (!ifile) {
       ERR_POST(Warning << "Unable to read syspect prodect rules from " << suspect_rule_file);
       suspect_rule_file = "/ncbi/data/product_rules.prt";
       ifile.open(suspect_rule_file.c_str());
       if (!ifile) 
          ERR_POST(Warning<< "Unable to read syspect prodect rules from "<< suspect_rule_file);
    } 
if (thisInfo.suspect_prod_rules->IsSet())
cerr << " is set\n";
if (thisInfo.suspect_prod_rules->CanGet())
cerr << "can get\n";
       
    if (ifile) {
       auto_ptr <CObjectIStream> ois (CObjectIStream::Open(eSerial_AsnText, suspect_rule_file));
       *ois >> *thisInfo.suspect_prod_rules;
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
    strs.clear();
    reg.EnumerateEntries("RRna-terms", &strs);
    ITERATE (list <string>, it, strs) {
       arr.clear();
       arr = NStr::Tokenize(reg.Get("RRna-terms", *it), ",", arr);
       strtmp = (*it == "5-8S") ? "5.8S" : *it;
       thisInfo.rRNATerms[strtmp] = NStr::StringToUInt(arr[0]);
       thisInfo.rRNATerms_partial[strtmp] = NStr::StringToUInt(arr[1]);
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
    thisInfo.rrna_standard_name = NStr::Tokenize(strtmp,",",thisInfo.rrna_standard_name);

    // ini. of short_auth_nms
    strtmp = reg.Get("StringVecIni", "ShortAuthNms");
    thisInfo.short_auth_nms = NStr::Tokenize(strtmp, ",", thisInfo.short_auth_nms);

    // ini. of desired_aaList
    reg.EnumerateEntries("Descred_aaList", &strs);
    ITERATE (list <string>, it, strs)
       thisInfo.desired_aaList[*it] = NStr::StringToUInt(reg.Get("Desired_aaList", *it));

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

    // ini. of strain_tax
    strtmp = reg.Get("StringVecIni", "StrainsConflictTax");
    thisInfo.strain_tax = NStr::Tokenize(strtmp, ",", thisInfo.strain_tax);

    // ini. of spell_data for flat file text check
    strs.clear();
    reg.EnumerateEntries("SpellFixData", &strs);
    ITERATE (list <string>, it, strs) {
      arr.clear();
      arr = NStr::Tokenize(reg.Get("SpellFixData-spell", *it), ",", arr);
      strtmp = ( *it == "nuclear-shutting") ? "nuclear shutting" : *it;
      thisInfo.fix_data[strtmp] = arr[0].empty()? "no" : "yes";
      thisInfo.spell_data[strtmp]= NStr::StringToUInt(arr[1]);
    }

    // ini of orga_prod_rules:
    CSuspect_rule_set rules;
    ReadInBlob(rules, reg.Get("RuleFiles", "OrganelleProductRuleFile"));
    thisInfo.orga_prod_rules.Reset(&rules);
 
    // ini of suspect_prod_rules:
    CSuspect_rule_set sus_rules;
    ReadInBlob(rules, reg.Get("RuleFiles", "PRODUCT_RULES_LIST"));
    thisInfo.suspect_prod_rules.Reset(&sus_rules);

    // ini of skip_bracket_paren
    strtmp = reg.Get("StringVecIni", "SkipBracketOrParen");
    thisInfo.skip_bracket_paren 
        = NStr::Tokenize(strtmp, ",", thisInfo.skip_bracket_paren);

    // ini of ok_num_prefix
    strtmp = reg.Get("StringVecIni", "OkNumberPrefix");
    thisInfo.ok_num_prefix
        = NStr::Tokenize(strtmp, ",", thisInfo.ok_num_prefix);

    // ini of feattype_featdef
/*
    for (i=0; i<= 94; i++) {
      strtmp = ENUM_METHOD_NAME(EMacro_feature_type)()->FindName((EMacro_feature_type)i, true);
      if (strtmp.empty()) {
        if (strtmp == "cds") strtmp = "cdregion";
        else if (strtmp == "c_region") strtmp = "C_region";
        else if (strtmp == "caat_signal") strtmp = "CAAT_signal";
        else if (strtmp == "imp_CDS") strtmp = "Imp_CDS";
        else if (strtmp == "d_loop") strtmp = "D_loop";
        else if (strtmp == "d_segment") strtmp = "D_segment";
        else if (strtmp == "gC_signal") strtmp = "GC_signal";
        else if (strtmp == "j_segment") strtmp = "J_segment";
        else if (strtmp == "ltr") strtmp = "LTR";
        else if (strtmp == "n_region") strtmp = "N_region";
        else if (strtmp == "rbs") strtmp = "RBS";
        else if (strtmp == "s_region") strtmp = "S_region";
        else if (strtmp == "sts") strtmp = "STS";
        else if (strtmp == "tata_signal") strtmp = "TATA_signal";
        else if (strtmp == "v_region") strtmp = "V_region";
        else if (strtmp == "v_segment") strtmp = "V_segment";
        else if (strtmp == "n3clip") strtmp = "3clip";
        else if (strtmp == "n3UTR") strtmp = "3UTR";
        else if (strtmp == "n5clip") strtmp = "5clip";
        else if (strtmp == "n5UTR") strtmp = "5UTR";
        else if (strtmp == "n10_signal") strtmp = "10_signal";
        else if (strtmp == "n35_signal") strtmp = "35_signal";
        thisInfo.feattype_featdef[(EMacro_feature_type)i]
           = CSeqFeatData::ENUM_METHOD_####

      }
    }
*/

    thisInfo.feattype_featdef[eMacro_feature_type_any] = CSeqFeatData::eSubtype_any;
    thisInfo.feattype_featdef[eMacro_feature_type_gene] = CSeqFeatData::eSubtype_gene;
    thisInfo.feattype_featdef[eMacro_feature_type_org] = CSeqFeatData::eSubtype_org;
    thisInfo.feattype_featdef[eMacro_feature_type_cds] = CSeqFeatData::eSubtype_cdregion;
    thisInfo.feattype_featdef[eMacro_feature_type_prot] = CSeqFeatData::eSubtype_prot; 
    thisInfo.feattype_featdef[eMacro_feature_type_preRNA] = CSeqFeatData::eSubtype_preRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_mRNA] = CSeqFeatData::eSubtype_mRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_tRNA] = CSeqFeatData::eSubtype_tRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_rRNA] = CSeqFeatData::eSubtype_rRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_snRNA] = CSeqFeatData::eSubtype_snRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_scRNA] = CSeqFeatData::eSubtype_scRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_otherRNA] = CSeqFeatData::eSubtype_otherRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_pub] = CSeqFeatData::eSubtype_pub;
    thisInfo.feattype_featdef[eMacro_feature_type_seq] = CSeqFeatData::eSubtype_seq;
    thisInfo.feattype_featdef[eMacro_feature_type_imp] = CSeqFeatData::eSubtype_imp;
    thisInfo.feattype_featdef[eMacro_feature_type_allele] = CSeqFeatData::eSubtype_allele;
    thisInfo.feattype_featdef[eMacro_feature_type_attenuator] 
                                                     = CSeqFeatData::eSubtype_attenuator;
    thisInfo.feattype_featdef[eMacro_feature_type_c_region] = CSeqFeatData::eSubtype_C_region;
    thisInfo.feattype_featdef[eMacro_feature_type_caat_signal] 
                                 = CSeqFeatData::eSubtype_CAAT_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_imp_CDS] = CSeqFeatData::eSubtype_Imp_CDS;
    thisInfo.feattype_featdef[eMacro_feature_type_d_loop] = CSeqFeatData::eSubtype_D_loop;
    thisInfo.feattype_featdef[eMacro_feature_type_d_segment] = CSeqFeatData::eSubtype_D_segment;
    thisInfo.feattype_featdef[eMacro_feature_type_enhancer] = CSeqFeatData::eSubtype_enhancer;
    thisInfo.feattype_featdef[eMacro_feature_type_exon] = CSeqFeatData::eSubtype_exon;
    thisInfo.feattype_featdef[eMacro_feature_type_gC_signal] = CSeqFeatData::eSubtype_GC_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_iDNA] = CSeqFeatData::eSubtype_iDNA;
    thisInfo.feattype_featdef[eMacro_feature_type_intron] = CSeqFeatData::eSubtype_intron;
    thisInfo.feattype_featdef[eMacro_feature_type_j_segment] = CSeqFeatData::eSubtype_J_segment;
    thisInfo.feattype_featdef[eMacro_feature_type_ltr] = CSeqFeatData::eSubtype_LTR;
    thisInfo.feattype_featdef[eMacro_feature_type_mat_peptide] 
                                                  = CSeqFeatData::eSubtype_mat_peptide;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_binding] 
                                                  = CSeqFeatData::eSubtype_misc_binding;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_difference] 
                                                  = CSeqFeatData::eSubtype_misc_difference;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_feature] 
                                                  = CSeqFeatData::eSubtype_misc_feature;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_recomb] 
                                                 = CSeqFeatData::eSubtype_misc_recomb;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_RNA] = CSeqFeatData::eSubtype_otherRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_signal] 
                                                 = CSeqFeatData::eSubtype_misc_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_misc_structure] 
                                                 = CSeqFeatData::eSubtype_misc_structure;
    thisInfo.feattype_featdef[eMacro_feature_type_modified_base] 
                                                 = CSeqFeatData::eSubtype_modified_base;
    thisInfo.feattype_featdef[eMacro_feature_type_mutation] = CSeqFeatData::eSubtype_mutation;
    thisInfo.feattype_featdef[eMacro_feature_type_n_region] = CSeqFeatData::eSubtype_N_region;
    thisInfo.feattype_featdef[eMacro_feature_type_old_sequence] 
                                                 = CSeqFeatData::eSubtype_old_sequence;
    thisInfo.feattype_featdef[eMacro_feature_type_polyA_signal] 
                                                 = CSeqFeatData::eSubtype_polyA_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_polyA_site] 
                                                 = CSeqFeatData::eSubtype_polyA_site;
    thisInfo.feattype_featdef[eMacro_feature_type_precursor_RNA] =CSeqFeatData::eSubtype_preRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_prim_transcript] 
                                                 = CSeqFeatData::eSubtype_prim_transcript;
    thisInfo.feattype_featdef[eMacro_feature_type_primer_bind] 
                                                 = CSeqFeatData::eSubtype_primer_bind;
    thisInfo.feattype_featdef[eMacro_feature_type_promoter] 
                                                 = CSeqFeatData::eSubtype_promoter;
    thisInfo.feattype_featdef[eMacro_feature_type_protein_bind] 
                                                 = CSeqFeatData::eSubtype_protein_bind;
    thisInfo.feattype_featdef[eMacro_feature_type_rbs] 
                                                 = CSeqFeatData::eSubtype_RBS;
    thisInfo.feattype_featdef[eMacro_feature_type_repeat_region] 
                                                 = CSeqFeatData::eSubtype_repeat_region;
    thisInfo.feattype_featdef[eMacro_feature_type_rep_origin] 
                                                 = CSeqFeatData::eSubtype_rep_origin;
    thisInfo.feattype_featdef[eMacro_feature_type_s_region] 
                                                 = CSeqFeatData::eSubtype_S_region;
    thisInfo.feattype_featdef[eMacro_feature_type_sig_peptide] 
                                                 = CSeqFeatData::eSubtype_sig_peptide;
    thisInfo.feattype_featdef[eMacro_feature_type_source] = CSeqFeatData::eSubtype_source;
    thisInfo.feattype_featdef[eMacro_feature_type_stem_loop] = CSeqFeatData::eSubtype_stem_loop;
    thisInfo.feattype_featdef[eMacro_feature_type_sts] = CSeqFeatData::eSubtype_STS;
    thisInfo.feattype_featdef[eMacro_feature_type_tata_signal] 
                                                 = CSeqFeatData::eSubtype_TATA_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_terminator] 
                                                 = CSeqFeatData::eSubtype_terminator;
    thisInfo.feattype_featdef[eMacro_feature_type_transit_peptide] 
                                                 = CSeqFeatData::eSubtype_transit_peptide;
    thisInfo.feattype_featdef[eMacro_feature_type_unsure] = CSeqFeatData::eSubtype_unsure;
    thisInfo.feattype_featdef[eMacro_feature_type_v_region] = CSeqFeatData::eSubtype_V_region;
    thisInfo.feattype_featdef[eMacro_feature_type_v_segment]= CSeqFeatData::eSubtype_V_segment;
    thisInfo.feattype_featdef[eMacro_feature_type_variation]= CSeqFeatData::eSubtype_variation;
    thisInfo.feattype_featdef[eMacro_feature_type_virion] = CSeqFeatData::eSubtype_virion;
    thisInfo.feattype_featdef[eMacro_feature_type_n3clip] = CSeqFeatData::eSubtype_3clip;
    thisInfo.feattype_featdef[eMacro_feature_type_n3UTR] = CSeqFeatData::eSubtype_3UTR;
    thisInfo.feattype_featdef[eMacro_feature_type_n5clip] = CSeqFeatData::eSubtype_5clip;
    thisInfo.feattype_featdef[eMacro_feature_type_n5UTR] = CSeqFeatData::eSubtype_5UTR;
    thisInfo.feattype_featdef[eMacro_feature_type_n10_signal] 
                                                 = CSeqFeatData::eSubtype_10_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_n35_signal] 
                                                 = CSeqFeatData::eSubtype_35_signal;
    thisInfo.feattype_featdef[eMacro_feature_type_site_ref] = CSeqFeatData::eSubtype_site_ref;
    thisInfo.feattype_featdef[eMacro_feature_type_region] = CSeqFeatData::eSubtype_region;
    thisInfo.feattype_featdef[eMacro_feature_type_comment] = CSeqFeatData::eSubtype_comment;
    thisInfo.feattype_featdef[eMacro_feature_type_bond] = CSeqFeatData::eSubtype_bond;
    thisInfo.feattype_featdef[eMacro_feature_type_site] = CSeqFeatData::eSubtype_site;
    thisInfo.feattype_featdef[eMacro_feature_type_rsite] = CSeqFeatData::eSubtype_rsite;
    thisInfo.feattype_featdef[eMacro_feature_type_user] = CSeqFeatData::eSubtype_user;
    thisInfo.feattype_featdef[eMacro_feature_type_txinit] = CSeqFeatData::eSubtype_txinit;
    thisInfo.feattype_featdef[eMacro_feature_type_num] = CSeqFeatData::eSubtype_num;
    thisInfo.feattype_featdef[eMacro_feature_type_psec_str] = CSeqFeatData::eSubtype_psec_str;
    thisInfo.feattype_featdef[eMacro_feature_type_non_std_residue] 
                                                 = CSeqFeatData::eSubtype_non_std_residue;
    thisInfo.feattype_featdef[eMacro_feature_type_het] = CSeqFeatData::eSubtype_het;
    thisInfo.feattype_featdef[eMacro_feature_type_biosrc] = CSeqFeatData::eSubtype_biosrc;
    thisInfo.feattype_featdef[eMacro_feature_type_preprotein] 
                                                 = CSeqFeatData::eSubtype_preprotein;
    thisInfo.feattype_featdef[eMacro_feature_type_mat_peptide_aa] 
                                                 = CSeqFeatData::eSubtype_mat_peptide_aa;
    thisInfo.feattype_featdef[eMacro_feature_type_sig_peptide_aa] 
                                                 = CSeqFeatData::eSubtype_sig_peptide_aa;
    thisInfo.feattype_featdef[eMacro_feature_type_transit_peptide_aa] 
                                                 = CSeqFeatData::eSubtype_transit_peptide_aa;
    thisInfo.feattype_featdef[eMacro_feature_type_snoRNA] = CSeqFeatData::eSubtype_snoRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_gap] = CSeqFeatData::eSubtype_gap;
    thisInfo.feattype_featdef[eMacro_feature_type_operon] = CSeqFeatData::eSubtype_operon;
    thisInfo.feattype_featdef[eMacro_feature_type_oriT] = CSeqFeatData::eSubtype_oriT;
    thisInfo.feattype_featdef[eMacro_feature_type_ncRNA] = CSeqFeatData::eSubtype_ncRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_tmRNA] = CSeqFeatData::eSubtype_tmRNA;
    thisInfo.feattype_featdef[eMacro_feature_type_mobile_element] 
                  = CSeqFeatData::eSubtype_mobile_element;

    // ini. of rnafeattp_rnareftp
    int i;
    string rna_feat_tp_nm;
    for (i = 2; i <= 5; i++) 
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] = (CRNA_ref::EType)(i-1);
    for (i = 6; i < 9; i++)
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] = (CRNA_ref::EType)(i+2); 

    // ini of rnafield_featquallegal
    string rnafield_val;
    for (i = 1; i< 14; i++) {
       rnafield_val = ENUM_METHOD_NAME(ERna_field)()->FindName((ERna_field)i, false);
       if (rnafield_val == "comment") rnafield_val = "note";
       else if (rnafield_val == "ncrna-class") rnafield_val = "ncRNA-class";
       else if (rnafield_val == "gene-locus") rnafield_val = "gene";
       else if (rnafield_val == "gene-maploc") rnafield_val = "map";
       else if (rnafield_val == "gene_locus_tag") rnafield_val = "locus_tag";
       else if (rnafield_val == "gene_synonym") rnafield_val = "synonym";
       thisInfo.rnafield_featquallegal[(ERna_field)i] 
           = (EFeat_qual_legal)(ENUM_METHOD_NAME(EFeat_qual_legal)()->FindValue(rnafield_val));
    }
  
    // ini of mol_molname
    thisInfo.mol_molname[CSeq_inst::eMol_not_set] = kEmptyStr;
    thisInfo.mol_molname[CSeq_inst::eMol_dna] = "DNA";
    thisInfo.mol_molname[CSeq_inst::eMol_rna] = "RNA";
    thisInfo.mol_molname[CSeq_inst::eMol_aa] = "protein";
    thisInfo.mol_molname[CSeq_inst::eMol_na] = "nucleotide";
    thisInfo.mol_molname[CSeq_inst::eMol_other] = "other";
 
    // ini of strand_strname
    thisInfo.strand_strname[CSeq_inst::eStrand_not_set] = kEmptyStr;
    thisInfo.strand_strname[CSeq_inst::eStrand_ss] = "single";
    thisInfo.strand_strname[CSeq_inst::eStrand_ds] = "double";
    thisInfo.strand_strname[CSeq_inst::eStrand_mixed] = "mixed";
    thisInfo.strand_strname[CSeq_inst::eStrand_other] = "other";
 
    // ini of dblink_name
    thisInfo.dblink_name[eDBLink_field_type_trace_assembly] = "Trace Assembly Archive";
    thisInfo.dblink_name[eDBLink_field_type_bio_sample] = "BioSample";
    thisInfo.dblink_name[eDBLink_field_type_probe_db] = "ProbeDB";
    thisInfo.dblink_name[eDBLink_field_type_sequence_read_archve] = "Sequence Read Archive";
    thisInfo.dblink_name[eDBLink_field_type_bio_project] = "BioProject"; 
 
    // ini of srcqual_orgmod, srcqual_subsrc
    map <string, ESource_qual> srcnm_qual;
    map <string, COrgMod::ESubtype> orgmodnm_subtp;
    for (i=0; i< 100; i++) {
       strtmp = ENUM_METHOD_NAME(ESource_qual)()->FindName((ESource_qual)i, true);
       if (!strtmp.empty()) srcnm_qual[strtmp] = (ESource_qual)i;
    }

    GetOrgModSubtpName(2, 37, orgmodnm_subtp);
    GetOrgModSubtpName(253, 254, orgmodnm_subtp);

    map <string, CSubSource::ESubtype> subsrcnm_subtp;
    for (i = 1; i<= 43; i++) {
       strtmp 
          = CSubSource::ENUM_METHOD_NAME(ESubtype)()
                                      ->FindName((CSubSource::ESubtype)i, true);
       if (!strtmp.empty()) subsrcnm_subtp[strtmp] = (CSubSource::ESubtype)i;
    }
   
    typedef map <string, ESource_qual> maptp;
    ITERATE (maptp, it, srcnm_qual) {
       if ( orgmodnm_subtp.find(it->first) != orgmodnm_subtp.end() )
         thisInfo.srcqual_orgmod[it->second] = orgmodnm_subtp[it->first];
       else if (it->first == "bio_material_INST" || it->first == "bio_material_COLL"
                                                  || it->first == "bio_material_SpecID")
               thisInfo.srcqual_orgmod[it->second] = orgmodnm_subtp["bio_material"]; 
       else if (it->first == "culture_collection_INST" 
                                                || it->first == "culture_collection_COLL" 
                                                || it->first == "culture_collection_SpecID")
                thisInfo.srcqual_orgmod[it->second] = orgmodnm_subtp["culture_collection"];
       else if (subsrcnm_subtp.find(it->first) != subsrcnm_subtp.end())
                    thisInfo.srcqual_subsrc[it->second] = subsrcnm_subtp[it->first];
       else if (it->first == "subsource_note")
              thisInfo.srcqual_subsrc[it->second] = CSubSource::eSubtype_other;
    } 

    // ini featquallegal_name, featquallegal_subfield;
      for (i= 1; i<= 72; i++) {
         strtmp = ENUM_METHOD_NAME(EFeat_qual_legal)()->FindName((EFeat_qual_legal)i, true);
         unsigned subfield=0;
         if (strtmp == "mobile-element-type-type") {
              strtmp = "mobile-element-type";
              subfield = 1;
         }
         else if (strtmp == "mobile-element-name") {
               strtmp = "mobile-element-type";
               subfield = 2;
         }
         else if (strtmp == "satellite-type") {
             strtmp = "satellite";
             subfield = 1;
         }
         else if (strtmp == "satellite-name") {
             strtmp = "satellite";
             subfield = 2;
         }
         else if (strtmp == "ec-number") strtmp = "EC-number"; // ???
         else if (strtmp == "pcr-conditions") strtmp = "PCR-conditions"; // ???
         else if (strtmp == "pseudo") strtmp = "pseudogene"; // ???
         if (!strtmp.empty()) thisInfo.featquallegal_name[(EFeat_qual_legal)i] = strtmp;
         if (subfield) thisInfo.featquallegal_subfield[(EFeat_qual_legal)i] = subfield;
      };

   // ini of scrna_mirna
   for (i=0; i<=10; i++) {
     strtmp = ENUM_METHOD_NAME(ESequence_constraint_rnamol)()
                              ->FindName((ESequence_constraint_rnamol)i, true);
     if (strtmp.empty()) {
       if (strtmp == "precursor_RNA") strtmp = "pre_RNA";
       else if (strtmp == "transfer_messenger_RNA") strtmp = "tmRNA";
       thisInfo.scrna_mirna[(ESequence_constraint_rnamol)i]
         = (CMolInfo::EBiomol) CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindValue(strtmp);
     }
   }

   // ini of pubclass_qual
   ostringstream output;
   output << CPub::e_Gen << ePub_type_unpublished << 0;
   thisInfo.pubclass_qual[output.str()] = "unpublished";
   output.str("");
   output << CPub::e_Sub << ePub_type_in_press << 0;
   thisInfo.pubclass_qual[output.str()] = "in-press submission";
   output.str("");
   output << CPub::e_Sub << ePub_type_published << 0;
   thisInfo.pubclass_qual[output.str()] = "submission";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 1;
   thisInfo.pubclass_qual[output.str()] = "in-press journal";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 1;
   thisInfo.pubclass_qual[output.str()] = "journal";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 2;
   thisInfo.pubclass_qual[output.str()] = "in-press book chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 2;
   thisInfo.pubclass_qual[output.str()] = "book chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 3;
   thisInfo.pubclass_qual[output.str()] = "in-press proceedings chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 3;
   thisInfo.pubclass_qual[output.str()] = "proceedings chapter";
   output.str("");
   output << CPub::e_Book << ePub_type_in_press << 0;
   thisInfo.pubclass_qual[output.str()] = "in-press book";
   output.str("");
   output << CPub::e_Book << ePub_type_published << 0;
   thisInfo.pubclass_qual[output.str()] = "book";
   output.str("");
   output << CPub::e_Man << ePub_type_in_press << 0;
   thisInfo.pubclass_qual[output.str()] = "in-press thesis";
   output.str("");
   output << CPub::e_Man << ePub_type_published << 0;
   thisInfo.pubclass_qual[output.str()] = "thesis";
   output.str("");
   output << CPub::e_Proc << ePub_type_in_press << 0;
   thisInfo.pubclass_qual[output.str()] = "in-press proceedings";
   output.str("");
   output << CPub::e_Proc << ePub_type_published << 0;
   thisInfo.pubclass_qual[output.str()] = "proceedings";
   output.str("");
   output << CPub::e_Patent << ePub_type_any << 0;
   thisInfo.pubclass_qual[output.str()] = "patent";

   // ini of months
   strtmp = reg.Get("StringVecIni", "Months");
   thisInfo.months = NStr::Tokenize(strtmp, ",", thisInfo.months);

   // ini of moltp_biomol: BiomolFromMoleculeType()
   for (i=0; i<= 11; i++) {
     strtmp = ENUM_METHOD_NAME(EMolecule_type)()->FindName((EMolecule_type)i, true);
     if (!strtmp.empty()) {
        if (strtmp == "precursor_RNA") strtmp = "ptr_RNA";
        else if (strtmp == "transfer_messenger_RNA") strtmp = "tmRNA";
        else if (strtmp == "macro_other") strtmp = "other_genetic";
        thisInfo.moltp_biomol[(EMolecule_type)i]
           = (CMolInfo::EBiomol)CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindValue(strtmp);
     }
   }

   // ini of techtp_mitech: TechFromTechniqueType()
   for (i=0; i<=24; i++) {
     strtmp = ENUM_METHOD_NAME(ETechnique_type)()->FindName((ETechnique_type)i, true);
     if (!strtmp.empty()) {
          if (strtmp == "genetic_map") strtmp = "genemap";
          else if (strtmp == "physical_map") strtmp = "physmap";
          else if (strtmp == "fli_cDNA") strtmp = "fil_cdna";
          thisInfo.techtp_mitech[(ETechnique_type)i]
             = (CMolInfo::ETech)CMolInfo::ENUM_METHOD_NAME(ETech)()->FindValue(strtmp);
     }
   }
}

void CDiscRepApp :: GetOrgModSubtpName(unsigned num1, unsigned num2, map <string, COrgMod::ESubtype>& orgmodnm_subtp)
{
    unsigned i;
    for (i = num1; i <= num2; i++) {
       strtmp = COrgMod::ENUM_METHOD_NAME(ESubtype)()->FindName((COrgMod::ESubtype)i, true);
       if (!strtmp.empty()) orgmodnm_subtp[strtmp] = (COrgMod::ESubtype)i;
    }
};




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

    // Register GenBank data loader and added to scope
    CGBDataLoader::RegisterInObjectManager(*object_manager);
    thisInfo.scope->AddDefaults();

    // ini. of comment_rules/validrules.prt for ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX
    CValidError errors;
    CValidError_imp imp(thisInfo.scope->GetObjectManager(),&errors);
    thisInfo.comment_rules = imp.GetStructuredCommentRules();

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
