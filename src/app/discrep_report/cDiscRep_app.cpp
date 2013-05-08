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
#include "hDiscRep_tests.hpp"
#include "hDiscRep_summ.hpp"
#include "hUtilib.hpp"

#include <common/test_assert.h>

USING_NCBI_SCOPE;

using namespace DiscRepNmSpc;
using namespace DiscRepAutoNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp, tmp;
static list <string> strs;
static vector <string> arr;

// Initialization
CRef < CScope >                         CDiscRepInfo :: scope;
string				        CDiscRepInfo :: infile;
vector < CRef < CClickableItem > >      CDiscRepInfo :: disc_report_data;
Str2Strs                                CDiscRepInfo :: test_item_list;
COutputConfig                           CDiscRepInfo :: output_config;
CRef < CSuspect_rule_set >              CDiscRepInfo :: suspect_prod_rules;
vector < vector <string> >              CDiscRepInfo :: susrule_summ;
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
Str2UInt                                CDiscRepInfo :: whole_word;
Str2Str                                 CDiscRepInfo :: fix_data;
CRef <CSuspect_rule_set>                CDiscRepInfo :: orga_prod_rules;
vector <string>                         CDiscRepInfo :: skip_bracket_paren;
vector <string>                         CDiscRepInfo :: ok_num_prefix;
map <EMacro_feature_type, CSeqFeatData::ESubtype> CDiscRepInfo :: feattype_featdef;
map <EMacro_feature_type, string>                 CDiscRepInfo :: feattype_name;
map <CRna_feat_type::E_Choice, CRNA_ref::EType>  CDiscRepInfo :: rnafeattp_rnareftp;
map <ERna_field, EFeat_qual_legal>      CDiscRepInfo :: rnafield_featquallegal;
map <ERna_field, string>                CDiscRepInfo :: rnafield_names;
vector <string>                         CDiscRepInfo :: rnatype_names;
map <CSeq_inst::EMol, string>           CDiscRepInfo :: mol_molname;
map <CSeq_inst::EStrand, string>        CDiscRepInfo :: strand_strname;
vector <string>                         CDiscRepInfo :: dblink_names;
map <ESource_qual, COrgMod::ESubtype>   CDiscRepInfo :: srcqual_orgmod;
map <ESource_qual, CSubSource::ESubtype>   CDiscRepInfo :: srcqual_subsrc;
map <ESource_qual, string>              CDiscRepInfo :: srcqual_names;
map <EFeat_qual_legal, string>          CDiscRepInfo :: featquallegal_name;
map <EFeat_qual_legal, unsigned>        CDiscRepInfo :: featquallegal_subfield;
map <ESequence_constraint_rnamol, CMolInfo::EBiomol>   CDiscRepInfo :: scrna_mirna;
map <string, string>                    CDiscRepInfo :: pub_class_quals;
vector <string>                         CDiscRepInfo :: months;
map <EMolecule_type, CMolInfo::EBiomol> CDiscRepInfo :: moltp_biomol;
map <EMolecule_type, string>            CDiscRepInfo :: moltp_name;
map <ETechnique_type, CMolInfo::ETech>  CDiscRepInfo :: techtp_mitech;
map <ETechnique_type, string>           CDiscRepInfo :: techtp_name;
vector <string>                         CDiscRepInfo :: s_putative_replacements;
vector <string>                         CDiscRepInfo :: suspect_name_category_names;
map <ECDSGeneProt_field, string>        CDiscRepInfo :: cgp_field_name;
map <ECDSGeneProt_feature_type_constraint, string>   CDiscRepInfo :: cgp_feat_name;
vector <vector <string> >                            CDiscRepInfo :: susterm_summ;
map <EFeature_strandedness_constraint, string>       CDiscRepInfo :: feat_strandedness;
map <EPublication_field, string>        CDiscRepInfo :: pubfield_label;
map <CPub_field_special_constraint_type::E_Choice,string>  CDiscRepInfo :: spe_pubfield_label;
map <EFeat_qual_legal, string>          CDiscRepInfo :: featqual_leg_name;
vector <string>                         CDiscRepInfo :: miscfield_names;
vector <string>                         CDiscRepInfo :: loctype_names;
map <EString_location, string>          CDiscRepInfo :: matloc_names;
map <EString_location, string>          CDiscRepInfo :: matloc_notpresent_names;
map <ESource_location, string>          CDiscRepInfo :: srcloc_names;
map <ECompletedness_type, string>       CDiscRepInfo :: compl_names;
map <EMolecule_class_type, string>      CDiscRepInfo :: molclass_names;
map <ETopology_type, string>            CDiscRepInfo :: topo_names;
map <EStrand_type, string>              CDiscRepInfo :: strand_names;
map <ESource_origin, string>            CDiscRepInfo :: srcori_names;
CRef < CSuspect_rule_set>               CDiscRepInfo :: suspect_rna_rules;
vector <string>                         CDiscRepInfo :: rna_rule_summ;


const char* fix_type_names[] = {
  "None",
  "Typo",
  "Putative Typo",
  "Quick fix",
  "Organelles not appropriate in prokaryote",
  "Suspicious phrase; should this be nonfunctional?",
  "May contain database identifier more appropriate in note; remove from product name",
  "Remove organism from product name",
  "Possible parsing error or incorrect formatting; remove inappropriate symbols",
  "Implies evolutionary relationship; change to -like protein",
  "Consider adding 'protein' to the end of the product name",
  "Correct the name or use 'hypothetical protein'",
  "Use American spelling",
  "Use short product name instead of descriptive phrase",
  "use protein instead of gene as appropriate"
};

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

    const CNcbiRegistry& reg = GetConfig();
    int i;

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
      arr = NStr::Tokenize(reg.Get("SpellFixData", *it), ",", arr);
      strtmp = ( *it == "nuclear-shutting") ? "nuclear shutting" : *it;
      thisInfo.fix_data[strtmp] = arr[0].empty()? "no" : "yes";
      thisInfo.whole_word[strtmp]= NStr::StringToUInt(arr[1]);
    }
    arr.clear();

    // ini of orga_prod_rules:
    strtmp = reg.Get("RuleFiles", "OrganelleProductRuleFile");
    if (CFile(strtmp).Exists()) ReadInBlob(*thisInfo.orga_prod_rules, strtmp);

    // ini. suspect rule file && susrule_fixtp_summ
    strtmp = reg.Get("RuleFiles", "SuspectRuleFile");
    if (!CFile(strtmp).Exists()) strtmp = "/ncbi/data/product_rules.prt";
    if (CFile(strtmp).Exists()) ReadInBlob(*thisInfo.suspect_prod_rules, strtmp);

    CSummarizeSusProdRule summ_susrule;
    ITERATE (list <CRef <CSuspect_rule> >, rit, thisInfo.suspect_prod_rules->Get()) {
       arr.clear();
       EFix_type fixtp = (*rit)->GetRule_type();
       if (fixtp == eFix_type_typo) strtmp = "DISC_PRODUCT_NAME_TYPO";
       else if (fixtp == eFix_type_quickfix) strtmp = "DISC_PRODUCT_NAME_QUICKFIX";
       else strtmp = "DISC_SUSPECT_PRODUCT_NAME";
       arr.push_back(strtmp);  // test_name
       arr.push_back(fix_type_names[(int)fixtp]);  // fixtp_name
       arr.push_back(summ_susrule.SummarizeSuspectRuleEx(**rit));
       thisInfo.susrule_summ.push_back(arr);
    }

    // ini. of susterm_summ for suspect_prod_terms if necessary
    if ((thisInfo.suspect_prod_rules->Get()).empty()) {
       for (i=0; i < (int)thisInfo.GetSusProdTermsLen(); i++) {
          const SuspectProductNameData& this_term = thisInfo.suspect_prod_terms[i];
          arr.clear();
          arr.push_back(this_term.pattern);

          if (this_term.fix_type  ==  eSuspectNameType_Typo) strtmp = "DISC_PRODUCT_NAME_TYPO";
          else if (this_term.fix_type == eSuspectNameType_QuickFix) 
                                        strtmp = "DISC_PRODUCT_NAME_QUICKFIX";
          else strtmp = "DISC_SUSPECT_PRODUCT_NAME";
          arr.push_back(strtmp); // test_name

          if (this_term.search_func == CTestAndRepData :: EndsWithPattern)
                strtmp = "end";
          else if (this_term.search_func == CTestAndRepData :: StartsWithPattern)
                 strtmp = "start";
          else strtmp = "contain";
          arr.push_back(strtmp); // test_desc;
          thisInfo.susterm_summ.push_back(arr);
       }
    }
/*
if (thisInfo.suspect_prod_rules->IsSet())
cerr << "222 is set\n";
if (thisInfo.suspect_prod_rules->CanGet())
cerr << "222can get\n";
*/

    // ini of skip_bracket_paren
    strtmp = reg.Get("StringVecIni", "SkipBracketOrParen");
    thisInfo.skip_bracket_paren 
        = NStr::Tokenize(strtmp, ",", thisInfo.skip_bracket_paren);

    // ini of ok_num_prefix
    strtmp = reg.Get("StringVecIni", "OkNumberPrefix");
    thisInfo.ok_num_prefix
        = NStr::Tokenize(strtmp, ",", thisInfo.ok_num_prefix);

    // ini of feattype_featdef & feattype_name
    struct FeatTypeFeatDefData {
             EMacro_feature_type feattype;
             CSeqFeatData::ESubtype featdef;
             const char* featname;
    };
    FeatTypeFeatDefData feattype_featdef[] = {
 { eMacro_feature_type_any , CSeqFeatData::eSubtype_any , "any" } , 
 { eMacro_feature_type_gene , CSeqFeatData::eSubtype_gene , "gene" } , 
 { eMacro_feature_type_org , CSeqFeatData::eSubtype_org , "org" } , 
 { eMacro_feature_type_cds , CSeqFeatData::eSubtype_cdregion , "CDS" } , 
 { eMacro_feature_type_prot , CSeqFeatData::eSubtype_prot , "Protein" } , 
 { eMacro_feature_type_preRNA , CSeqFeatData::eSubtype_preRNA , "preRNA" } , 
 { eMacro_feature_type_mRNA , CSeqFeatData::eSubtype_mRNA , "mRNA" } , 
 { eMacro_feature_type_tRNA , CSeqFeatData::eSubtype_tRNA , "tRNA" } , 
 { eMacro_feature_type_rRNA , CSeqFeatData::eSubtype_rRNA , "rRNA" } , 
 { eMacro_feature_type_snRNA , CSeqFeatData::eSubtype_snRNA , "snRNA" } , 
 { eMacro_feature_type_scRNA , CSeqFeatData::eSubtype_scRNA , "scRNA" } , 
 { eMacro_feature_type_otherRNA , CSeqFeatData::eSubtype_otherRNA , "misc_RNA" } , 
 { eMacro_feature_type_pub , CSeqFeatData::eSubtype_pub , "pub" } , 
 { eMacro_feature_type_seq , CSeqFeatData::eSubtype_seq , "seq" } , 
 { eMacro_feature_type_imp , CSeqFeatData::eSubtype_imp , "imp" } , 
 { eMacro_feature_type_allele , CSeqFeatData::eSubtype_allele , "allele" } , 
 { eMacro_feature_type_attenuator , CSeqFeatData::eSubtype_attenuator , "attenuator" } , 
 { eMacro_feature_type_c_region , CSeqFeatData::eSubtype_C_region , "c_region" } , 
 { eMacro_feature_type_caat_signal , CSeqFeatData::eSubtype_CAAT_signal , "caat_signal" } , 
 { eMacro_feature_type_imp_CDS , CSeqFeatData::eSubtype_Imp_CDS , "imp_CDS" } , 
 { eMacro_feature_type_d_loop , CSeqFeatData::eSubtype_D_loop , "d_loop" } , 
 { eMacro_feature_type_d_segment , CSeqFeatData::eSubtype_D_segment , "d_segment" } , 
 { eMacro_feature_type_enhancer , CSeqFeatData::eSubtype_enhancer , "enhancer" } , 
 { eMacro_feature_type_exon , CSeqFeatData::eSubtype_exon , "exon" } , 
 { eMacro_feature_type_gC_signal , CSeqFeatData::eSubtype_GC_signal , "gC_signal" } , 
 { eMacro_feature_type_iDNA , CSeqFeatData::eSubtype_iDNA , "iDNA" } , 
 { eMacro_feature_type_intron , CSeqFeatData::eSubtype_intron , "intron" } , 
 { eMacro_feature_type_j_segment , CSeqFeatData::eSubtype_J_segment , "j_segment" } , 
 { eMacro_feature_type_ltr , CSeqFeatData::eSubtype_LTR , "LTR" } , 
 { eMacro_feature_type_mat_peptide , CSeqFeatData::eSubtype_mat_peptide , "mat_peptide" } , 
 { eMacro_feature_type_misc_binding , CSeqFeatData::eSubtype_misc_binding , "misc_binding" } , 
 { eMacro_feature_type_misc_difference , CSeqFeatData::eSubtype_misc_difference , "misc_difference" } , 
 { eMacro_feature_type_misc_feature , CSeqFeatData::eSubtype_misc_feature , "misc_feature" } , 
 { eMacro_feature_type_misc_recomb , CSeqFeatData::eSubtype_misc_recomb , "misc_recomb" } , 
 { eMacro_feature_type_misc_RNA , CSeqFeatData::eSubtype_otherRNA , "misc_RNA" } , 
 { eMacro_feature_type_misc_signal , CSeqFeatData::eSubtype_misc_signal , "misc_signal" } , 
 { eMacro_feature_type_misc_structure , CSeqFeatData::eSubtype_misc_structure , "misc_structure" } , 
 { eMacro_feature_type_modified_base , CSeqFeatData::eSubtype_modified_base , "modified_base" } , 
 { eMacro_feature_type_mutation , CSeqFeatData::eSubtype_mutation , "mutation" } , 
 { eMacro_feature_type_n_region , CSeqFeatData::eSubtype_N_region , "n_region" } , 
 { eMacro_feature_type_old_sequence , CSeqFeatData::eSubtype_old_sequence , "old_sequence" } , 
 { eMacro_feature_type_polyA_signal , CSeqFeatData::eSubtype_polyA_signal , "polyA_signal" } , 
 { eMacro_feature_type_polyA_site , CSeqFeatData::eSubtype_polyA_site , "polyA_site" } , 
 { eMacro_feature_type_precursor_RNA , CSeqFeatData::eSubtype_preRNA , "precursor_RNA" } , 
 { eMacro_feature_type_prim_transcript , CSeqFeatData::eSubtype_prim_transcript , "prim_transcript" } , 
 { eMacro_feature_type_primer_bind , CSeqFeatData::eSubtype_primer_bind , "primer_bind" } , 
 { eMacro_feature_type_promoter , CSeqFeatData::eSubtype_promoter , "promoter" } , 
 { eMacro_feature_type_protein_bind , CSeqFeatData::eSubtype_protein_bind , "protein_bind" } , 
 { eMacro_feature_type_rbs , CSeqFeatData::eSubtype_RBS , "rbs" } , 
 { eMacro_feature_type_repeat_region , CSeqFeatData::eSubtype_repeat_region , "repeat_region" } , 
 { eMacro_feature_type_rep_origin , CSeqFeatData::eSubtype_rep_origin , "rep_origin" } , 
 { eMacro_feature_type_s_region , CSeqFeatData::eSubtype_S_region , "s_region" } , 
 { eMacro_feature_type_sig_peptide , CSeqFeatData::eSubtype_sig_peptide , "sig_peptide" } , 
 { eMacro_feature_type_source , CSeqFeatData::eSubtype_source , "source" } , 
 { eMacro_feature_type_stem_loop , CSeqFeatData::eSubtype_stem_loop , "stem_loop" } , 
 { eMacro_feature_type_sts , CSeqFeatData::eSubtype_STS , "sts" } , 
 { eMacro_feature_type_tata_signal , CSeqFeatData::eSubtype_TATA_signal , "tata_signal" } , 
 { eMacro_feature_type_terminator , CSeqFeatData::eSubtype_terminator , "terminator" } , 
 { eMacro_feature_type_transit_peptide , CSeqFeatData::eSubtype_transit_peptide , "transit_peptide" } , 
 { eMacro_feature_type_unsure , CSeqFeatData::eSubtype_unsure , "unsure" } , 
 { eMacro_feature_type_v_region , CSeqFeatData::eSubtype_V_region , "v_region" } , 
 { eMacro_feature_type_v_segment , CSeqFeatData::eSubtype_V_segment , "v_segment" } , 
 { eMacro_feature_type_variation , CSeqFeatData::eSubtype_variation , "variation" } , 
 { eMacro_feature_type_virion , CSeqFeatData::eSubtype_virion , "virion" } , 
 { eMacro_feature_type_n3clip , CSeqFeatData::eSubtype_3clip , "3'clip" } , 
 { eMacro_feature_type_n3UTR , CSeqFeatData::eSubtype_3UTR , "3'UTR" } , 
 { eMacro_feature_type_n5clip , CSeqFeatData::eSubtype_5clip , "5'clip" } , 
 { eMacro_feature_type_n5UTR , CSeqFeatData::eSubtype_5UTR , "5'UTR" } , 
 { eMacro_feature_type_n10_signal , CSeqFeatData::eSubtype_10_signal , "10_signal" } , 
 { eMacro_feature_type_n35_signal , CSeqFeatData::eSubtype_35_signal , "35_signal" } , 
 { eMacro_feature_type_site_ref , CSeqFeatData::eSubtype_site_ref , "site_ref" } , 
 { eMacro_feature_type_region , CSeqFeatData::eSubtype_region , "region" } , 
 { eMacro_feature_type_comment , CSeqFeatData::eSubtype_comment , "comment" } , 
 { eMacro_feature_type_bond , CSeqFeatData::eSubtype_bond , "bond" } , 
 { eMacro_feature_type_site , CSeqFeatData::eSubtype_site , "site" } , 
 { eMacro_feature_type_rsite , CSeqFeatData::eSubtype_rsite , "rsite" } , 
 { eMacro_feature_type_user , CSeqFeatData::eSubtype_user , "user" } , 
 { eMacro_feature_type_txinit , CSeqFeatData::eSubtype_txinit , "txinit" } , 
 { eMacro_feature_type_num , CSeqFeatData::eSubtype_num , "num" } , 
 { eMacro_feature_type_psec_str , CSeqFeatData::eSubtype_psec_str , "psec_str" } , 
 { eMacro_feature_type_non_std_residue , CSeqFeatData::eSubtype_non_std_residue , "non_std_residue" } , 
 { eMacro_feature_type_het , CSeqFeatData::eSubtype_het , "het" } , 
 { eMacro_feature_type_biosrc , CSeqFeatData::eSubtype_biosrc , "biosrc" } , 
 { eMacro_feature_type_preprotein , CSeqFeatData::eSubtype_preprotein , "preprotein" } , 
 { eMacro_feature_type_mat_peptide_aa , CSeqFeatData::eSubtype_mat_peptide_aa , "mat_peptide_aa" } , 
 { eMacro_feature_type_sig_peptide_aa , CSeqFeatData::eSubtype_sig_peptide_aa , "sig_peptide_aa" } , 
 { eMacro_feature_type_transit_peptide_aa , CSeqFeatData::eSubtype_transit_peptide_aa , "transit_peptide_aa" } , 
 { eMacro_feature_type_snoRNA , CSeqFeatData::eSubtype_snoRNA , "snoRNA" } , 
 { eMacro_feature_type_gap , CSeqFeatData::eSubtype_gap , "gap" } , 
 { eMacro_feature_type_operon , CSeqFeatData::eSubtype_operon , "operon" } , 
 { eMacro_feature_type_oriT , CSeqFeatData::eSubtype_oriT , "oriT" } , 
 { eMacro_feature_type_ncRNA , CSeqFeatData::eSubtype_ncRNA , "ncRNA" } , 
 { eMacro_feature_type_tmRNA , CSeqFeatData::eSubtype_tmRNA , "tmRNA" } ,
 { eMacro_feature_type_mobile_element, CSeqFeatData::eSubtype_mobile_element, "mobile_element" }
};
    for (i = 0; i < (int)(sizeof(feattype_featdef) / sizeof(FeatTypeFeatDefData)); i++) {
       thisInfo.feattype_featdef[feattype_featdef[i].feattype] = feattype_featdef[i].featdef;
       thisInfo.feattype_name[feattype_featdef[i].feattype] = feattype_featdef[i].featname;
    }

    // ini. of rnafeattp_rnareftp
    string rna_feat_tp_nm;
    for (i = 2; i <= 5; i++) 
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] = (CRNA_ref::EType)(i-1);
    for (i = 6; i < 9; i++)
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] = (CRNA_ref::EType)(i+2); 

    // ini of rnafield_featquallegal & rnafield_names
    string rnafield_val;
    for (i = (int)eRna_field_product; i<= eRna_field_tag_peptide; i++) {
       strtmp = rnafield_val = ENUM_METHOD_NAME(ERna_field)()->FindName(i, false);
       if (rnafield_val == "comment") rnafield_val = "note";
       else if (rnafield_val == "ncrna-class") rnafield_val = "ncRNA-class";
       else if (rnafield_val == "gene-locus") rnafield_val = "gene";
       else if (rnafield_val == "gene-maploc") rnafield_val = "map";
       else if (rnafield_val == "gene_locus_tag") rnafield_val = "locus_tag";
       else if (rnafield_val == "gene_synonym") rnafield_val = "synonym";
       thisInfo.rnafield_featquallegal[(ERna_field)i] 
           = (EFeat_qual_legal)(ENUM_METHOD_NAME(EFeat_qual_legal)()->FindValue(rnafield_val));

       arr.clear();
       arr = NStr::Tokenize(strtmp, "-", arr);
       if (arr[0] == "ncrna") arr[0] = "ncRNA";
       else if (arr[1] == "id") arr[1] = "ID";
       thisInfo.rnafield_names[(ERna_field)i] = NStr::Join(arr, " ");
    }

    // ini. of rnatype_names
    strtmp = reg.Get("StringVecIni", "RnaTypeMap");
    thisInfo.rnatype_names =  NStr::Tokenize(strtmp, ",", thisInfo.rnatype_names);
    
  
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
 
    // ini of dblink_names
    strtmp = reg.Get("StringVecIni", "DblinkNames");
    thisInfo.dblink_names = NStr::Tokenize(strtmp, ",", thisInfo.dblink_names);
 
    // ini of srcqual_orgmod, srcqual_subsrc, srcqual_names
    map <string, ESource_qual> srcnm_qual;
    map <string, COrgMod::ESubtype> orgmodnm_subtp;
    for (i=(int)eSource_qual_acronym; i<= eSource_qual_altitude; i++) {
       strtmp = ENUM_METHOD_NAME(ESource_qual)()->FindName(i, true);
       if (!strtmp.empty()) srcnm_qual[strtmp] = (ESource_qual)i;
       thisInfo.srcqual_names[(ESource_qual)i] = strtmp;
    }

    GetOrgModSubtpName(COrgMod::eSubtype_strain, COrgMod::eSubtype_metagenome_source, 
                                                                            orgmodnm_subtp);
    GetOrgModSubtpName(COrgMod::eSubtype_old_lineage, COrgMod::eSubtype_old_name, 
                                                                             orgmodnm_subtp);

    map <string, CSubSource::ESubtype> subsrcnm_subtp;
    for (i = CSubSource::eSubtype_chromosome; i<= CSubSource::eSubtype_altitude; i++) {
       strtmp 
          = CSubSource::ENUM_METHOD_NAME(ESubtype)()->FindName(i, true);
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
         strtmp = ENUM_METHOD_NAME(EFeat_qual_legal)()->FindName(i, true);
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
     strtmp = ENUM_METHOD_NAME(ESequence_constraint_rnamol)()->FindName(i, true);
     if (strtmp.empty()) {
       if (strtmp == "precursor_RNA") strtmp = "pre_RNA";
       else if (strtmp == "transfer_messenger_RNA") strtmp = "tmRNA";
       thisInfo.scrna_mirna[(ESequence_constraint_rnamol)i]
         = (CMolInfo::EBiomol) CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindValue(strtmp);
     }
   }

   // ini of pub_class_quals
   ostringstream output;
   output << CPub::e_Gen << ePub_type_unpublished << 0;
   thisInfo.pub_class_quals[output.str()] = "unpublished";
   output.str("");
   output << CPub::e_Sub << ePub_type_in_press << 0;
   thisInfo.pub_class_quals[output.str()] = "in-press submission";
   output.str("");
   output << CPub::e_Sub << ePub_type_published << 0;
   thisInfo.pub_class_quals[output.str()] = "submission";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 1;
   thisInfo.pub_class_quals[output.str()] = "in-press journal";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 1;
   thisInfo.pub_class_quals[output.str()] = "journal";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 2;
   thisInfo.pub_class_quals[output.str()] = "in-press book chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 2;
   thisInfo.pub_class_quals[output.str()] = "book chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_in_press << 3;
   thisInfo.pub_class_quals[output.str()] = "in-press proceedings chapter";
   output.str("");
   output << CPub::e_Article << ePub_type_published << 3;
   thisInfo.pub_class_quals[output.str()] = "proceedings chapter";
   output.str("");
   output << CPub::e_Book << ePub_type_in_press << 0;
   thisInfo.pub_class_quals[output.str()] = "in-press book";
   output.str("");
   output << CPub::e_Book << ePub_type_published << 0;
   thisInfo.pub_class_quals[output.str()] = "book";
   output.str("");
   output << CPub::e_Man << ePub_type_in_press << 0;
   thisInfo.pub_class_quals[output.str()] = "in-press thesis";
   output.str("");
   output << CPub::e_Man << ePub_type_published << 0;
   thisInfo.pub_class_quals[output.str()] = "thesis";
   output.str("");
   output << CPub::e_Proc << ePub_type_in_press << 0;
   thisInfo.pub_class_quals[output.str()] = "in-press proceedings";
   output.str("");
   output << CPub::e_Proc << ePub_type_published << 0;
   thisInfo.pub_class_quals[output.str()] = "proceedings";
   output.str("");
   output << CPub::e_Patent << ePub_type_any << 0;
   thisInfo.pub_class_quals[output.str()] = "patent";

   // ini of months
   strtmp = reg.Get("StringVecIni", "Months");
   thisInfo.months = NStr::Tokenize(strtmp, ",", thisInfo.months);

   // ini of moltp_biomol: BiomolFromMoleculeType(), and moltp_name
   for (i=(int)eMolecule_type_unknown; i<= (int)eMolecule_type_macro_other; i++) {
     strtmp = ENUM_METHOD_NAME(EMolecule_type)()->FindName(i, true);
     if (!strtmp.empty()) {
        tmp = strtmp; 
        if (strtmp == "precursor-RNA") strtmp = "pre-RNA";
        else if (strtmp == "transfer-messenger-RNA") strtmp = "tmRNA";
        else if (strtmp == "macro-other") strtmp = "other";
        thisInfo.moltp_biomol[(EMolecule_type)i]
           = (CMolInfo::EBiomol)CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindValue(strtmp);

        if (tmp == "macro-other") tmp = "other-genetic";
        else if (tmp == "transfer_messenger_RNA") tmp = strtmp;
        else if (tmp.find("-") != string::npos) tmp = NStr::Replace (tmp, "-", " ");
        thisInfo.moltp_name[(EMolecule_type)i] = tmp;
     }
   }

   // ini of techtp_mitech: TechFromTechniqueType(), and techtp_name
   for (i=0; i<=24; i++) {
     strtmp = ENUM_METHOD_NAME(ETechnique_type)()->FindName(i, true);
     if (!strtmp.empty()) {
          tmp = strtmp;

          if (strtmp == "genetic_map") strtmp = "genemap";
          else if (strtmp == "physical_map") strtmp = "physmap";
          else if (strtmp == "fli_cDNA") strtmp = "fil_cdna";
          thisInfo.techtp_mitech[(ETechnique_type)i]
             = (CMolInfo::ETech)CMolInfo::ENUM_METHOD_NAME(ETech)()->FindValue(strtmp);

          if (tmp == "ets" || tmp == "sts" || tmp == "htgs-1" || tmp == "htgs-2"
                        || tmp == "htgs-3" || tmp == "htgs-0" || tmp == "htc" 
                        || tmp == "WGS" || tmp == "barcode" || tmp == "tsa") {
             tmp = NStr::ToUpper(tmp);
          }
          else if (tmp == "composite_wgs_htgs") tmp = "composite_wgs_htgs";
          thisInfo.techtp_name[(ETechnique_type)i] = tmp;
     }
   }

   // ini. of s_putative_replacements
   strtmp = reg.Get("StringVecIni", "SPutativeReplacements");
   thisInfo.s_putative_replacements 
            = NStr::Tokenize(strtmp, ",", thisInfo.s_putative_replacements);

   // ini. of suspect_name_category_names
   strtmp = reg.Get("StringVecIni", "SuspectNameCategoryNames");
   thisInfo.suspect_name_category_names
        = NStr::Tokenize(strtmp, ",", thisInfo.suspect_name_category_names);

   // ini. of cgp_field_name
   for (i=1; i< 24; i++) {
     strtmp = ENUM_METHOD_NAME(ECDSGeneProt_field)()->FindName(i, true);
     arr.clear();
     if (strtmp != "codon-start") {
         if (arr[0] != "mat") {
           arr = NStr::Tokenize(strtmp, "-", arr);
           if (arr[0] == "cds") arr[0] = "CDS";
           else if (arr[0] == "mrna") arr[0] = "mRNA";
           else if (arr[0] == "prot") {
                  arr[0] = "protein";
                  if (arr[1] == "ec") arr[1] == "EC"; 
           }
           strtmp = NStr::Join(arr, " ");
         }
         else {
            strtmp = arr[0] + "-" + arr[1];
            for (unsigned j=2; j < arr.size(); j++)
              strtmp += " " + ((arr[j] == "ec")? "EC" : arr[j]);
         }
     }
     thisInfo.cgp_field_name[(ECDSGeneProt_field)i] = strtmp;
   }

   // ini. of cgp_feat_name
   for (i=1; i <=6; i++) {
     strtmp = ENUM_METHOD_NAME(ECDSGeneProt_feature_type_constraint)()->FindName(i, true);
     if (strtmp == "cds") strtmp = "CDS";
     else if (strtmp == "prot") strtmp = "protein";
     thisInfo.cgp_feat_name[(ECDSGeneProt_feature_type_constraint)i] = strtmp;
   }

   // ini. of feat_strandedness
   thisInfo.feat_strandedness[eFeature_strandedness_constraint_any] = kEmptyStr; 
   for (i=1; i<=6; i++) {
     strtmp = ENUM_METHOD_NAME(EFeature_strandedness_constraint)()->FindName(i, true);
     arr.clear();
     arr = NStr::Tokenize(strtmp, "-", arr);
     if (arr[0] == "minus" || arr[0] == "plus") {
        tmp = arr[0];
        arr[0] = arr[1];
        arr[1] = tmp;
     }
     strtmp = "sequence contains " + NStr::Join(arr, " ") + "  strand feature";
     if (strtmp.find("at least") == string::npos) strtmp += "s";
     thisInfo.feat_strandedness[(EFeature_strandedness_constraint)i] = strtmp;
   }

   // ini. of pubfield_label;
   for (i = (int)ePublication_field_cit; i <= (int)ePublication_field_pub_class; i++) {
     strtmp = ENUM_METHOD_NAME(EPublication_field)()->FindName(i, true);
     if (strtmp == "cit") strtmp = "citation";
     else if (strtmp == "sub") strtmp = "state";
     else if (strtmp == "zipcode") strtmp = "postal code";
     else if (strtmp == "pmid") strtmp = "PMID";
     else if (strtmp == "div") strtmp = "department";
     else if (strtmp == "pub-class") strtmp = "class";
     else if (strtmp == "serial-number") strtmp = "serial number";
     thisInfo.pubfield_label[(EPublication_field)i] = strtmp;
   }

   // ini. of spe_pubfield_label;
   strtmp = reg.Get("StringVecIni", "SpecPubFieldWords");
   arr.clear();
   arr = NStr::Tokenize(strtmp, ",", arr);
   for (i = 0; i < CPub_field_special_constraint_type::e_Is_all_punct; i++) 
     thisInfo.spe_pubfield_label[(CPub_field_special_constraint_type::E_Choice)i] = arr[i];

   // ini. of featqual_leg_name
   for (i = (int)eFeat_qual_legal_allele; i <= (int)eFeat_qual_legal_pcr_conditions; i++){
      strtmp = ENUM_METHOD_NAME(EFeat_qual_legal)()->FindName(i, true);
      if (!strtmp.empty()) {
         if (strtmp == "ec-number") strtmp = "EC number";
         else if (strtmp == "gene") strtmp = "locus";
         else if (strtmp == "pseudo") strtmp = "pseudogene";
         thisInfo.featqual_leg_name[(EFeat_qual_legal)i] = strtmp; 
      }
   }
    
   // ini. of miscfield_names
   strtmp = reg.Get("StringVecIni", "MiscFieldNames");
   thisInfo.miscfield_names = NStr::Tokenize(strtmp, ",", thisInfo.miscfield_names);

   // ini. of loctype_names;
   strtmp = reg.Get("StringVecIni", "LocationType");
   thisInfo.loctype_names = NStr::Tokenize(strtmp, ",", thisInfo.loctype_names);
 
   // ini. of matloc_names & matloc_notpresent_names;
   for (i = (int)eString_location_contains; i <= (int)eString_location_inlist; i++) {
      strtmp = ENUM_METHOD_NAME(EString_location)()->FindName(i, true);
      if (strtmp == "inlist") {
              strtmp = "is one of";
              tmp = "is not one of";
      }
      else { 
        if (strtmp == "starts" || strtmp == "ends") {
               tmp = strtmp.substr(0, strtmp.size()-1) + " with"; 
               strtmp = " with";
        }
        tmp = "does not " + tmp;
      }
      thisInfo.matloc_names[(EString_location)i] = strtmp;
      thisInfo.matloc_notpresent_names[(EString_location)i] = tmp;
   }

   // ini. of srcloc_names;
   for (i = (int)eSource_location_unknown; i <= (int)eSource_location_chromatophore; i++){
      strtmp = ENUM_METHOD_NAME(ESource_location)()->FindName(i, true);
      thisInfo.srcloc_names[(ESource_location)i] = strtmp;
   }

   // ini of srcori_names;
   for (i = (int)eSource_origin_unknown; i<= (int)eSource_origin_synthetic; i++) {
      strtmp = ENUM_METHOD_NAME(ESource_origin)()->FindName(i, true);
      thisInfo.srcori_names[(ESource_origin)i] = strtmp;
   }
   thisInfo.srcori_names[eSource_origin_other] = "other";

   // ini of compl_names
   for (i = (int)eCompletedness_type_unknown; i <= (int)eCompletedness_type_has_right; i++) {
      strtmp = ENUM_METHOD_NAME(ECompletedness_type)()->FindName(i, true);
      if (strtmp.find("-") != string::npos) strtmp = NStr::Replace(strtmp, "-", " ");
      thisInfo.compl_names[(ECompletedness_type)i] = strtmp;
   }
  
   // ini of molclass_names
   for (i = (int)eMolecule_class_type_unknown; i <= (int)eMolecule_class_type_other; i++) {
     strtmp = ENUM_METHOD_NAME(EMolecule_class_type)()->FindName(i, true);
     if (strtmp == "dna" || strtmp == "rna") strtmp = NStr::ToUpper(strtmp);
     thisInfo.molclass_names[(EMolecule_class_type)i] = strtmp;
   }

   // ini of topo_names;
   for (i = (int)eTopology_type_unknown; i <= (int)eTopology_type_other; i++)
     thisInfo.topo_names[(ETopology_type)i] 
         = ENUM_METHOD_NAME(ETopology_type)()->FindName(i, true);
 
   // ini of strand_names
   for (i = (int)eStrand_type_unknown; i <= (int)eStrand_type_other; i++)
      thisInfo.strand_names[(EStrand_type)i] 
            = ENUM_METHOD_NAME(EStrand_type)()->FindName(i, true);
   
   // ini of suspect_rna_rules;
   strtmp = reg.Get("StringVecIni", "SusRrnaProdNames");
   arr.clear();
   arr = NStr::Tokenize(strtmp, ",", arr);
   ITERATE (vector <string>, it, arr) {
     CRef <CSearch_func>& sch_func = MakeSimpleSearchFunc(*it);
     CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
     this_rule->SetFind(*sch_func);
     thisInfo.suspect_rna_rules->Set().push_back(this_rule);
     thisInfo.rna_rule_summ.push_back(summ_susrule.SummarizeSuspectRuleEx(*this_rule));    
   }
   CRef <CSearch_func>& sch_func = MakeSimpleSearchFunc("8S", true);
   CRef <CSearch_func>& except = MakeSimpleSearchFunc("5.8S", true);
   CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
   this_rule->SetFind(*sch_func);
   this_rule->SetExcept(*except);
   thisInfo.suspect_rna_rules->Set().push_back(this_rule); 
   thisInfo.rna_rule_summ.push_back(summ_susrule.SummarizeSuspectRuleEx(*this_rule));    

   strs.clear();
   arr.clear();
}

CRef <CSearch_func>& CDiscRepApp :: MakeSimpleSearchFunc(const string& match_text, bool whole_word)
{
     CRef <CString_constraint> str_cons (new CString_constraint);
     str_cons->SetMatch_text(match_text);
     if (whole_word) str_cons->SetWhole_word(whole_word);
     CRef <CSearch_func> sch_func (new CSearch_func);
     sch_func->SetString_constraint(*str_cons);
     return sch_func;
};

void CDiscRepApp :: GetOrgModSubtpName(unsigned num1, unsigned num2, map <string, COrgMod::ESubtype>& orgmodnm_subtp)
{
    unsigned i;
    for (i = num1; i <= num2; i++) {
       strtmp = COrgMod::ENUM_METHOD_NAME(ESubtype)()->FindName(i, true);
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
