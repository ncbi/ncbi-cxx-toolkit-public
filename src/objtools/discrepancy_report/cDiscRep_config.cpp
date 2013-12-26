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
 *   Cpp Discrepany Report configuration
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/fasta.hpp>
#include <serial/objhook.hpp>
#include <serial/objectio.hpp>
#include <serial/objectiter.hpp>
#include <objtools/discrepancy_report/hauto_disc_class.hpp>
#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>
#include <objtools/discrepancy_report/hDiscRep_summ.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE (objects);
USING_SCOPE (DiscRepNmSpc);
USING_SCOPE (DiscRepAutoNmSpc);

// Initialization
CRef < CScope >                     CDiscRepInfo :: scope;
string				    CDiscRepInfo :: infile;
string                              CDiscRepInfo :: report;
CRef <CRepConfig>                   CDiscRepInfo :: config;
vector < CRef < CClickableItem > >  CDiscRepInfo :: disc_report_data;
Str2Strs                            CDiscRepInfo :: test_item_list;
Str2Objs                            CDiscRepInfo :: test_item_objs;
COutputConfig                       CDiscRepInfo :: output_config;
CRef <CSuspect_rule_set >           CDiscRepInfo::suspect_prod_rules(new CSuspect_rule_set);
vector < vector <string> >          CDiscRepInfo :: susrule_summ;
vector <string> 	            CDiscRepInfo :: weasels;
CRef <CSeq_submit>                  CDiscRepInfo :: seq_submit(new CSeq_submit);
string                              CDiscRepInfo :: expand_defline_on_set;
string                              CDiscRepInfo :: report_lineage;
vector <string>                     CDiscRepInfo :: strandsymbol;
bool                                CDiscRepInfo :: exclude_dirsub;
Str2UInt                            CDiscRepInfo :: rRNATerms;
Str2UInt                            CDiscRepInfo :: rRNATerms_ignore_partial;
vector <string>                     CDiscRepInfo :: bad_gene_names_contained;
vector <string>                     CDiscRepInfo :: no_multi_qual;
vector <string>                     CDiscRepInfo :: short_auth_nms;
vector <string>                     CDiscRepInfo :: spec_words_biosrc;
vector <string>                     CDiscRepInfo :: suspicious_notes;
vector <string>                     CDiscRepInfo :: trna_list;
vector <string>                     CDiscRepInfo :: rrna_standard_name;
Str2UInt                            CDiscRepInfo :: desired_aaList;
CTaxon1                             CDiscRepInfo :: tax_db_conn;
list <string>                       CDiscRepInfo :: state_abbrev;
Str2Str                             CDiscRepInfo :: cds_prod_find;
vector <string>                     CDiscRepInfo :: s_pseudoweasels;
vector <string>                     CDiscRepInfo :: suspect_rna_product_names;
vector <string>                     CDiscRepInfo :: new_exceptions;
Str2Str	                            CDiscRepInfo :: srcqual_keywords;
vector <string>                     CDiscRepInfo :: kIntergenicSpacerNames;
vector <string>                     CDiscRepInfo :: taxnm_env;
vector <string>                     CDiscRepInfo :: virus_lineage;
vector <string>                     CDiscRepInfo :: strain_tax;
Str2UInt                            CDiscRepInfo :: whole_word;
Str2Str                             CDiscRepInfo :: fix_data;
CRef <CSuspect_rule_set>            CDiscRepInfo :: orga_prod_rules (new CSuspect_rule_set);
vector <string>                     CDiscRepInfo :: skip_bracket_paren;
vector <string>                     CDiscRepInfo :: ok_num_prefix;
map <EMacro_feature_type, CSeqFeatData::ESubtype> CDiscRepInfo :: feattype_featdef;
map <EMacro_feature_type, string>   CDiscRepInfo :: feattype_name;
map <CRna_feat_type::E_Choice, CRNA_ref::EType>   CDiscRepInfo :: rnafeattp_rnareftp;
map <ERna_field, EFeat_qual_legal>  CDiscRepInfo :: rnafield_featquallegal;
map <ERna_field, string>            CDiscRepInfo :: rnafield_names;
vector <string>                     CDiscRepInfo :: rnatype_names;
map <CSeq_inst::EMol, string>       CDiscRepInfo :: mol_molname;
map <CSeq_inst::EStrand, string>    CDiscRepInfo :: strand_strname;
vector <string>                     CDiscRepInfo :: dblink_names;
map <ESource_qual, COrgMod::ESubtype>   CDiscRepInfo :: srcqual_orgmod;
map <ESource_qual, CSubSource::ESubtype>   CDiscRepInfo :: srcqual_subsrc;
map <ESource_qual, string>          CDiscRepInfo :: srcqual_names;
map <EFeat_qual_legal, string>      CDiscRepInfo :: featquallegal_name;
map <EFeat_qual_legal, unsigned>    CDiscRepInfo :: featquallegal_subfield;
map <ESequence_constraint_rnamol, CMolInfo::EBiomol>   CDiscRepInfo :: scrna_mirna;
map <string, string>                CDiscRepInfo :: pub_class_quals;
vector <string>                     CDiscRepInfo :: months;
map <EMolecule_type, CMolInfo::EBiomol>    CDiscRepInfo :: moltp_biomol;
map <EMolecule_type, string>        CDiscRepInfo :: moltp_name;
map <ETechnique_type, CMolInfo::ETech>     CDiscRepInfo :: techtp_mitech;
map <ETechnique_type, string>       CDiscRepInfo :: techtp_name;
vector <string>                     CDiscRepInfo :: s_putative_replacements;
map <ECDSGeneProt_field, string>    CDiscRepInfo :: cgp_field_name;
map <ECDSGeneProt_feature_type_constraint, string>   CDiscRepInfo :: cgp_feat_name;
vector <vector <string> >           CDiscRepInfo :: susterm_summ;
map <EFeature_strandedness_constraint, string>       CDiscRepInfo :: feat_strandedness;
map <EPublication_field, string>    CDiscRepInfo :: pubfield_label;
map <CPub_field_special_constraint_type::E_Choice,string>  CDiscRepInfo :: spe_pubfield_label;
map <EFeat_qual_legal, string>      CDiscRepInfo :: featqual_leg_name;
vector <string>                     CDiscRepInfo :: miscfield_names;
vector <string>                     CDiscRepInfo :: loctype_names;
map <EString_location, string>      CDiscRepInfo :: matloc_names;
map <EString_location, string>      CDiscRepInfo :: matloc_notpresent_names;
map <ESource_location, string>      CDiscRepInfo :: srcloc_names;
map <int, string>                   CDiscRepInfo :: genome_names;
map <ECompletedness_type, string>   CDiscRepInfo :: compl_names;
map <EMolecule_class_type, string>  CDiscRepInfo :: molclass_names;
map <ETopology_type, string>        CDiscRepInfo :: topo_names;
map <EStrand_type, string>          CDiscRepInfo :: strand_names;
map <ESource_origin, string>        CDiscRepInfo :: srcori_names;
CRef < CSuspect_rule_set>           CDiscRepInfo :: suspect_rna_rules (new CSuspect_rule_set);
vector <string>                     CDiscRepInfo :: rna_rule_summ;
vector <string>                     CDiscRepInfo :: suspect_phrases;

vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_aa;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_na;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_CFeat;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_CFeat_NotInGenProdSet;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_NotInGenProdSet;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_Bioseq_CFeat_CSeqdesc;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_SeqEntry;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_SeqEntry_feat_desc;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_4_once;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_BioseqSet;
vector < CRef <CTestAndRepData> >   CTestGrp :: tests_on_SubmitBlk;

set <string> CDiscTestInfo :: tests_run;
CSeq_entry_Handle* CRepConfig::m_TopSeqEntry=0;

static CDiscRepInfo thisInfo;
static string       strtmp, tmp;
static list <string>  strs;
static vector <string> arr;
static CDiscTestInfo thisTest;
static CTestGrp  thisGrp;

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

void CRepConfig :: InitParams(const IRWRegistry& reg)
{
    int i;

    // get other parameters from *.ini
    thisInfo.expand_defline_on_set 
         = reg.Get("OncallerTool", "EXPAND_DEFLINE_ON_SET");
    if (thisInfo.expand_defline_on_set.empty()) {
      NCBI_THROW(CException, eUnknown, "Missing config parameters.");
    }

    // ini. of srcqual_keywords
    strs.clear();
    reg.EnumerateEntries("Srcqual-keywords", &strs);
    ITERATE (list <string>, it, strs) {
      thisInfo.srcqual_keywords[*it]= reg.Get("Srcqual-keywords", *it);
    }
    
    // ini. of s_pseudoweasels
    strtmp = reg.Get("StringVecIni", "SPseudoWeasels");
    thisInfo.s_pseudoweasels 
        = NStr::Tokenize(strtmp, ",", thisInfo.s_pseudoweasels);

    // ini. of suspect_rna_product_names
    strtmp = reg.Get("StringVecIni", "SuspectRnaProdNms");
    thisInfo.suspect_rna_product_names 
        = NStr::Tokenize(strtmp, ",", thisInfo.suspect_rna_product_names);

    // ini. of new_exceptions
    strtmp = reg.Get("StringVecIni", "NewExceptions");
    thisInfo.new_exceptions 
        = NStr::Tokenize(strtmp, ",", thisInfo.new_exceptions);

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
       thisInfo.rRNATerms_ignore_partial[strtmp] = NStr::StringToUInt(arr[1]);
    }

    // ini. of no_multi_qual
    strtmp = reg.Get("StringVecIni", "NoMultiQual");
    thisInfo.no_multi_qual 
        = NStr::Tokenize(strtmp, ",", thisInfo.no_multi_qual);
  
    // ini. of bad_gene_names_contained
    strtmp = reg.Get("StringVecIni", "BadGeneNamesContained");
    thisInfo.bad_gene_names_contained 
        = NStr::Tokenize(strtmp, ",", thisInfo.bad_gene_names_contained);

    // ini. of suspicious notes
    strtmp = reg.Get("StringVecIni", "Suspicious_notes");
    thisInfo.suspicious_notes 
        = NStr::Tokenize(strtmp, ",", thisInfo.suspicious_notes);

    // ini. of spec_words_biosrc;
    strtmp = reg.Get("StringVecIni", "SpecWordsBiosrc");
    thisInfo.spec_words_biosrc 
        = NStr::Tokenize(strtmp, ",", thisInfo.spec_words_biosrc);

    // ini. of trna_list:
    strtmp = reg.Get("StringVecIni", "TrnaList");
    thisInfo.trna_list 
        = NStr::Tokenize(strtmp, ",", thisInfo.trna_list);

    // ini. of rrna_standard_name
    strtmp = reg.Get("StringVecIni", "RrnaStandardName");
    thisInfo.rrna_standard_name 
        = NStr::Tokenize(strtmp,",",thisInfo.rrna_standard_name);

    // ini. of short_auth_nms
    strtmp = reg.Get("StringVecIni", "ShortAuthNms");
    thisInfo.short_auth_nms 
         = NStr::Tokenize(strtmp, ",", thisInfo.short_auth_nms);

    // ini. of desired_aaList
    reg.EnumerateEntries("Descred_aaList", &strs);
    ITERATE (list <string>, it, strs) {
       thisInfo.desired_aaList[*it] 
           = NStr::StringToUInt(reg.Get("Desired_aaList", *it));
    }

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
    thisInfo.virus_lineage 
        = NStr::Tokenize(strtmp, ",", thisInfo.virus_lineage); 

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
    if (CFile(strtmp).Exists()) {
        ReadInBlob(*thisInfo.orga_prod_rules, strtmp);
    }

    // ini. of matloc_names & matloc_notpresent_names;
    for (i = eString_location_contains; i <= eString_location_inlist; i++) {
       strtmp = ENUM_METHOD_NAME(EString_location)()->FindName(i, true);
       if (strtmp == "inlist") {
              strtmp = "is one of";
              tmp = "is not one of";
       }
       else if (strtmp == "starts" || strtmp == "ends") {
           tmp = strtmp.substr(0, strtmp.size()-1) + " with";
           strtmp += " with";
           tmp = "does not " + tmp;
       }
       else if (strtmp == "contains") {
           tmp = "does not contain";
       }
       thisInfo.matloc_names[(EString_location)i] = strtmp;
       thisInfo.matloc_notpresent_names[(EString_location)i] = tmp;
    }

    // ini. suspect rule file && susrule_fixtp_summ
    strtmp = reg.Get("RuleFiles", "SuspectRuleFile");
    if (!CFile(strtmp).Exists()) {
         strtmp = "/ncbi/data/product_rules.prt";
    }
    if (CFile(strtmp).Exists()) {
        ReadInBlob(*thisInfo.suspect_prod_rules, strtmp);
    }

    CSummarizeSusProdRule summ_susrule;
    ITERATE (list <CRef <CSuspect_rule> >, rit, 
                                       thisInfo.suspect_prod_rules->Get()) {
       arr.clear();
       EFix_type fixtp = (*rit)->GetRule_type();
       if (fixtp == eFix_type_typo) {
           strtmp = CBioseq_on_SUSPECT_RULE :: GetName_typo();
       }
       else if (fixtp == eFix_type_quickfix || (*rit)->CanGetReplace() ) {
           strtmp = CBioseq_on_SUSPECT_RULE::GetName_qfix();
       }
       else {
           strtmp = CBioseq_on_SUSPECT_RULE :: GetName_name();
       }
       arr.push_back(strtmp);  // test_name
       arr.push_back(fix_type_names[(int)fixtp]);  // fixtp_name
       arr.push_back(summ_susrule.SummarizeSuspectRuleEx(**rit));

       thisInfo.susrule_summ.push_back(arr);
    }

    // ini. of susterm_summ for suspect_prod_terms if necessary
    if ((thisInfo.suspect_prod_rules->Get()).empty()) {
       for (i=0; 
            i < (int)thisInfo.GetSusProdTermsLen(thisInfo.suspect_prod_terms); 
            i++){
          const s_SuspectProductNameData& 
                this_term = thisInfo.suspect_prod_terms[i];
          arr.clear();
          arr.push_back(this_term.pattern);

          if (this_term.fix_type  ==  eSuspectNameType_Typo) {
              strtmp = CBioseq_on_SUSPECT_RULE :: GetName_typo();
          }
          else if (this_term.fix_type == eSuspectNameType_QuickFix) {
              strtmp = CBioseq_on_SUSPECT_RULE::GetName_qfix();
          }
          else {
              strtmp = CBioseq_on_SUSPECT_RULE :: GetName_name();
          }
          arr.push_back(strtmp); // test_name

          // CTestAndRepData can't have these functions overwritten. 
          if (this_term.search_func == CTestAndRepData :: EndsWithPattern) {
              strtmp = "end";
          }
          else if (this_term.search_func ==CTestAndRepData ::StartsWithPattern){
              strtmp = "start";
          }
          else {
              strtmp = "contain";
          }
          arr.push_back(strtmp); // test_desc;
          thisInfo.susterm_summ.push_back(arr);
       }
    }

    // ini of skip_bracket_paren
    strtmp = reg.Get("StringVecIni", "SkipBracketOrParen");
    thisInfo.skip_bracket_paren 
        = NStr::Tokenize(strtmp, ",", thisInfo.skip_bracket_paren);

    // ini of ok_num_prefix
    strtmp = reg.Get("StringVecIni", "OkNumberPrefix");
    thisInfo.ok_num_prefix
        = NStr::Tokenize(strtmp, ",", thisInfo.ok_num_prefix);

    // ini of feattype_featdef & feattype_name
    for (i = eMacro_feature_type_any; i <= eMacro_feature_type_imp_CDS; i++) {
      tmp = strtmp = ENUM_METHOD_NAME(EMacro_feature_type)()->FindName(i, true);
      if (strtmp == "cds") {
          strtmp == "cdregion";
          tmp = "CDS";
      }
      else if (strtmp == "prot") {
          tmp = "Protein";
      }
      else if (strtmp == "otherRNA") {
          tmp = "misc_RNA";
      }
      else if (strtmp == "c-region") {
          strtmp = "C-region";
      }
      else if (strtmp == "caat-signal") {
          strtmp = "CAAT-signal";
      }
      else if (strtmp == "imp-CDS") {
          strtmp = "Imp-CDS";
      }
      thisInfo.feattype_featdef[(EMacro_feature_type)i]
          = CSeqFeatData :: SubtypeNameToValue(strtmp);
      arr = NStr::Tokenize(tmp, "_", arr);
      if (!arr.empty()) {
          tmp = NStr::Join(arr, "-");
          arr.clear();
      }
      thisInfo.feattype_name[(EMacro_feature_type)i] = tmp;
    }

    // skip eMocro_feature_type_conflict, why?
    for (i = eMacro_feature_type_d_loop; 
         i <= eMacro_feature_type_mobile_element; 
         i++) {
      tmp = strtmp = ENUM_METHOD_NAME(EMacro_feature_type)()->FindName(i, true);
      if (strtmp == "d-loop") {
          strtmp = "D-loop";
      }
      else if (strtmp == "d-segment") {
          strtmp = "D-segment";
      }
      else if (strtmp == "gC-signal") {
          strtmp = "GC-signal";
      }
      else if (strtmp == "j-segmrnt") {
          strtmp = "J-segment";
      }
      else if (strtmp == "ltr") {
          strtmp = tmp = "LTR";  
      }
      else if (strtmp == "misc-RNA") {
          strtmp = "otherRNA";
      }
      else if (strtmp == "precursor-RNA") {
          strtmp = "preRNA";
      }
      else if (strtmp == "rbs") {
          strtmp = "RBS";
      }
      else if (strtmp == "s-region") {
          strtmp = "S-region";
      }
      else if (strtmp == "sts") {
          strtmp = "STS";
      }
      else if (strtmp == "tata-signal") {
          strtmp = "TATA-signal";
      }
      else if (strtmp == "v-region") {
          strtmp = "V-region";
      }
      else if (strtmp == "v-segment") {
          strtmp = "V-segment";
      }
      else if (strtmp == "n35-signal") {
          strtmp = tmp = "35-signal";
      }
      else if (strtmp == "n10-signal") {
          strtmp = tmp = "10-signal";
      }
      else if (strtmp.find("n3") != string::npos 
                  || strtmp.find("n5") != string::npos) {
            strtmp = strtmp[1] + strtmp.substr(2);
            tmp = strtmp[1] + (string)"'" + strtmp.substr(2);
      }
      thisInfo.feattype_featdef[(EMacro_feature_type)i]
          = CSeqFeatData :: SubtypeNameToValue(strtmp);
      arr = NStr::Tokenize(tmp, "_", arr);
      if (!arr.empty()) {
         tmp = NStr::Join(arr, "-");
         arr.clear();
      }
      thisInfo.feattype_name[(EMacro_feature_type)i] = tmp;
    }
    
    // ini. of rnafeattp_rnareftp
    string rna_feat_tp_nm;
    for (i = CRna_feat_type::e_PreRNA; i <= CRna_feat_type::e_RRNA; i++) {
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] 
            = (CRNA_ref::EType)(i-1);
    }
    for (i = CRna_feat_type::e_NcRNA; i <= CRna_feat_type::e_MiscRNA; i++) {
        thisInfo.rnafeattp_rnareftp[(CRna_feat_type::E_Choice)i] 
           = (CRNA_ref::EType)(i+2); 
    }

    // ini of rnafield_featquallegal & rnafield_names
    string rnafield_val;
    for (i = eRna_field_product; i<= eRna_field_tag_peptide; i++) {
       strtmp = rnafield_val =ENUM_METHOD_NAME(ERna_field)()->FindName(i, true);
       if (rnafield_val == "comment") {
           rnafield_val = "note";
       }
       else if (rnafield_val == "ncrna-class") {
           rnafield_val = "ncRNA-class";
       }
       else if (rnafield_val == "gene-locus") {
           rnafield_val = "gene";
       }
       else if (rnafield_val == "gene-maploc") {
           rnafield_val = "map";
       }
       else if (rnafield_val == "gene-locus-tag") {
           rnafield_val = "locus-tag";
       }
       else if (rnafield_val == "gene-synonym") {
           rnafield_val = "synonym";
       }
       thisInfo.rnafield_featquallegal[(ERna_field)i] 
           = (EFeat_qual_legal)(ENUM_METHOD_NAME(EFeat_qual_legal)()
                 ->FindValue(rnafield_val));

       arr.clear();
       if (strtmp.find("-") != string::npos) {
           arr = NStr::Tokenize(strtmp, "-", arr);
       }
       else arr.push_back(strtmp);
       if (arr[0] == "ncrna") {
              arr[0] = "ncRNA";
       }
       else if (arr.size() >=2 && arr[1] == "id") {
             arr[1] = "ID";
       }
       thisInfo.rnafield_names[(ERna_field)i] = NStr::Join(arr, " ");
    }

    // ini. of rnatype_names
    strtmp = reg.Get("StringVecIni", "RnaTypeMap");
    thisInfo.rnatype_names 
        = NStr::Tokenize(strtmp, ",", thisInfo.rnatype_names);
    
  
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
    for (i = eSource_qual_acronym; i<= eSource_qual_altitude; i++) {
       strtmp = ENUM_METHOD_NAME(ESource_qual)()->FindName(i, true);
       if (!strtmp.empty()) {
           srcnm_qual[strtmp] = (ESource_qual)i;
       }
       thisInfo.srcqual_names[(ESource_qual)i] = strtmp;
    }

    GetOrgModSubtpName(COrgMod::eSubtype_strain, 
            COrgMod::eSubtype_metagenome_source, orgmodnm_subtp);
    GetOrgModSubtpName(COrgMod::eSubtype_old_lineage, 
            COrgMod::eSubtype_old_name, orgmodnm_subtp);

    map <string, CSubSource::ESubtype> subsrcnm_subtp;
    for (i = CSubSource::eSubtype_chromosome; 
         i<= CSubSource::eSubtype_altitude; 
         i++) {
       strtmp 
          = CSubSource::ENUM_METHOD_NAME(ESubtype)()->FindName(i, true);
       if (!strtmp.empty()) subsrcnm_subtp[strtmp] = (CSubSource::ESubtype)i;
    }
   
    typedef map <string, ESource_qual> maptp;
    ITERATE (maptp, it, srcnm_qual) {
       if ( orgmodnm_subtp.find(it->first) != orgmodnm_subtp.end() ) {
           thisInfo.srcqual_orgmod[it->second] = orgmodnm_subtp[it->first];
       }
       else if (it->first == "bio-material-INST" 
                 || it->first == "bio-material-COLL"
                 || it->first == "bio-material-SpecID") {
           thisInfo.srcqual_orgmod[it->second] 
                = orgmodnm_subtp["bio-material"]; 
       }
       else if (it->first == "culture-collection-INST" 
                  || it->first == "culture-collection-COLL" 
                  || it->first == "culture-collection-SpecID") {
           thisInfo.srcqual_orgmod[it->second] 
                 = orgmodnm_subtp["culture-collection"];
       }
       else if (subsrcnm_subtp.find(it->first) != subsrcnm_subtp.end()) {
           thisInfo.srcqual_subsrc[it->second] = subsrcnm_subtp[it->first];
       }
       else if (it->first == "subsource-note") {
           thisInfo.srcqual_subsrc[it->second] = CSubSource::eSubtype_other;
       }
    } 

    // ini featquallegal_name, featquallegal_subfield;
    for (i= eFeat_qual_legal_allele; i<= eFeat_qual_legal_pcr_conditions; i++) {
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
         else if (strtmp == "ec-number") {
             strtmp = "EC-number";
         }
         else if (strtmp == "pcr-conditions") {
             strtmp = "PCR-conditions";
         }
         else if (strtmp == "pseudo") {
             strtmp = "pseudogene";
         }
         if (!strtmp.empty()) {
             thisInfo.featquallegal_name[(EFeat_qual_legal)i] = strtmp;
         }
         if (subfield) {
             thisInfo.featquallegal_subfield[(EFeat_qual_legal)i] = subfield;
         }
    };

   // ini of scrna_mirna
   for (i = eSequence_constraint_rnamol_genomic; 
        i <= eSequence_constraint_rnamol_transfer_messenger_RNA; 
        i++) {
     strtmp= ENUM_METHOD_NAME(ESequence_constraint_rnamol)()->FindName(i, true);
     if (!strtmp.empty()) {
       strtmp = (strtmp == "precursor-RNA") ? "pre-RNA"
                  : ((strtmp == "transfer-messenger-RNA") ? "tmRNA" : strtmp);
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
   for (i = eMolecule_type_unknown; i <= eMolecule_type_macro_other; i++) {
     strtmp = ENUM_METHOD_NAME(EMolecule_type)()->FindName(i, true);
     if (!strtmp.empty()) {
        tmp = strtmp; 
        strtmp = (strtmp == "precursor-RNA") ? "pre-RNA"
                   : ((strtmp == "transfer-messenger-RNA") ? "tmRNA"
                        :((strtmp == "macro-other") ? "other": strtmp));
        thisInfo.moltp_biomol[(EMolecule_type)i]
           = (CMolInfo::EBiomol)CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindValue(strtmp);

        if (tmp == "macro-other") {
            tmp = "other-genetic";
        }
        else if (tmp == "transfer_messenger_RNA") {
            tmp = strtmp;
        }
        else if (tmp.find("-") != string::npos) {
            tmp = NStr::Replace (tmp, "-", " ");
        }
        thisInfo.moltp_name[(EMolecule_type)i] = tmp;
     }
   }

   // ini of techtp_mitech: TechFromTechniqueType(), and techtp_name
   for (i = eTechnique_type_unknown; i <= eTechnique_type_other; i++) {
     strtmp = ENUM_METHOD_NAME(ETechnique_type)()->FindName(i, true);
     if (!strtmp.empty()) {
          tmp = strtmp;

          strtmp = (strtmp == "genetic-map") ? "genemap"
                     : ((strtmp == "physical-map")? "physmap"
                          :((strtmp == "fli-cDNA") ? "fli-cdna" : strtmp));
          thisInfo.techtp_mitech[(ETechnique_type)i]
             = (CMolInfo::ETech)CMolInfo::ENUM_METHOD_NAME(ETech)()->FindValue(strtmp);

          if (tmp == "ets" || tmp == "sts" || tmp == "htgs-1" || tmp == "htgs-2"
               || tmp == "htgs-3" || tmp == "htgs-0" || tmp == "htc" 
               || tmp == "WGS" || tmp == "barcode" || tmp == "tsa") {
             tmp = NStr::ToUpper(tmp);
          }
          else if (tmp == "composite-wgs-htgs") {
              tmp = "composite-wgs-htgs";
          }
          thisInfo.techtp_name[(ETechnique_type)i] = tmp;
     }
   }

   // ini. of s_putative_replacements
   strtmp = reg.Get("StringVecIni", "SPutativeReplacements");
   if (!strtmp.empty()) {
     thisInfo.s_putative_replacements 
            = NStr::Tokenize(strtmp, ",", thisInfo.s_putative_replacements);
   }
   else {
      NCBI_THROW(CException, eUnknown, "Missing SPutativeReplacements\n");
   }

   // ini. of cgp_field_name
   for (i = eCDSGeneProt_field_cds_comment; 
        i <= eCDSGeneProt_field_codon_start; 
        i++) {
     strtmp = ENUM_METHOD_NAME(ECDSGeneProt_field)()->FindName(i, true);
     arr.clear();
     if (strtmp != "codon-start") {
         arr = NStr::Tokenize(strtmp, "-", arr);
         if (arr[0] != "mat") {
           if (arr[0] == "cds") {
               arr[0] = "CDS";
           }
           else if (arr[0] == "mrna") {
               arr[0] = "mRNA";
           }
           else if (arr[0] == "prot") {
               arr[0] = "protein";
               if (arr[1] == "ec") {
                  arr[1] == "EC"; 
               }
           }
           strtmp = NStr::Join(arr, " ");
         }
         else {
            strtmp = arr[0] + "-" + arr[1];
            for (unsigned j=2; j < arr.size(); j++) {
               strtmp += " " + ((arr[j] == "ec")? "EC" : arr[j]);
            }
         }
     }
     thisInfo.cgp_field_name[(ECDSGeneProt_field)i] = strtmp;
   }

   // ini. of cgp_feat_name
   for (i = eCDSGeneProt_feature_type_constraint_gene; 
        i <= eCDSGeneProt_feature_type_constraint_mat_peptide; i++) {
     strtmp 
        = ENUM_METHOD_NAME(ECDSGeneProt_feature_type_constraint)()->FindName(i, true);
     strtmp = (strtmp == "cds") ? "CDS"
                : ((strtmp == "prot") ? "protein" : strtmp);
     thisInfo.cgp_feat_name[(ECDSGeneProt_feature_type_constraint)i] = strtmp;
   }

   // ini. of feat_strandedness
   thisInfo.feat_strandedness[eFeature_strandedness_constraint_any] = kEmptyStr; 
   for (i = eFeature_strandedness_constraint_minus_only; 
        i <= eFeature_strandedness_constraint_no_plus; i++) {
     strtmp 
        =ENUM_METHOD_NAME(EFeature_strandedness_constraint)()->FindName(i,true);
     arr.clear();
     arr = NStr::Tokenize(strtmp, "-", arr);
     if (arr[0] == "minus" || arr[0] == "plus") {
        tmp = arr[0];
        arr[0] = arr[1];
        arr[1] = tmp;
     }
     strtmp = "sequence contains " + NStr::Join(arr, " ") + "  strand feature";
     if (strtmp.find("at least") == string::npos) {
         strtmp += "s";
      }
     thisInfo.feat_strandedness[(EFeature_strandedness_constraint)i] = strtmp;
   }

   // ini. of pubfield_label;
   for (i = ePublication_field_cit; i <= ePublication_field_pub_class; i++) {
     strtmp = ENUM_METHOD_NAME(EPublication_field)()->FindName(i, true);
     strtmp = (strtmp == "cit") ? "citation"
                :((strtmp == "sub")?"state"
                    :((strtmp == "zipcode") ? "postal code"
                        :((strtmp == "pmid") ? "PMID"
                            :((strtmp == "div") ? "department"
                               :((strtmp == "pub-class")? "class"
                                   :(strtmp == "serial-number")?"serial number"
                                      : strtmp)))));
     thisInfo.pubfield_label[(EPublication_field)i] = strtmp;
   }

   // ini. of spe_pubfield_label;
   strtmp = reg.Get("StringVecIni", "SpecPubFieldWords");
   arr.clear();
   arr = NStr::Tokenize(strtmp, ",", arr);
   for (i = CPub_field_special_constraint_type::e_Is_present; 
        i <= CPub_field_special_constraint_type::e_Is_all_punct; i++) {
       thisInfo.spe_pubfield_label[(CPub_field_special_constraint_type::E_Choice)i] = arr[i-1];

   }
   // ini. of featqual_leg_name
   for (i = eFeat_qual_legal_allele; i <= eFeat_qual_legal_pcr_conditions; i++){
      strtmp = ENUM_METHOD_NAME(EFeat_qual_legal)()->FindName(i, true);
      if (!strtmp.empty()) {
         strtmp = (strtmp == "ec-number") ? "EC number"
                    :((strtmp == "gene") ? "locus" 
                       :((strtmp == "pseudo") ? "pseudogene" : strtmp));
         thisInfo.featqual_leg_name[(EFeat_qual_legal)i] = strtmp; 
      }
   }
    
   // ini. of miscfield_names
   strtmp = reg.Get("StringVecIni", "MiscFieldNames");
   thisInfo.miscfield_names 
       = NStr::Tokenize(strtmp, ",", thisInfo.miscfield_names);

   // ini. of loctype_names;
   strtmp = reg.Get("StringVecIni", "LocationType");
   thisInfo.loctype_names 
        = NStr::Tokenize(strtmp, ",", thisInfo.loctype_names);
 
   // ini. of srcloc_names & genome_names;
   for (i = eSource_location_unknown; i <= eSource_location_chromatophore; i++){
      strtmp = ENUM_METHOD_NAME(ESource_location)()->FindName(i, true);
      strtmp = (strtmp == "unknown") ? kEmptyStr
                 : ((strtmp == "extrachrom") ? "extrachromosomal" : strtmp);
      thisInfo.srcloc_names[(ESource_location)i] = strtmp;
      thisInfo.genome_names[i] = strtmp;
   }

   // ini of srcori_names;
   for (i = eSource_origin_unknown; i<= eSource_origin_synthetic; i++) {
      strtmp = ENUM_METHOD_NAME(ESource_origin)()->FindName(i, true);
      thisInfo.srcori_names[(ESource_origin)i] = strtmp;
   }
   thisInfo.srcori_names[eSource_origin_other] = "other";

   // ini of compl_names
   for (i = eCompletedness_type_unknown; 
        i <= eCompletedness_type_has_right; i++) {
      strtmp = ENUM_METHOD_NAME(ECompletedness_type)()->FindName(i, true);
      if (strtmp.find("-") != string::npos) {
         strtmp = NStr::Replace(strtmp, "-", " ");
      }
      thisInfo.compl_names[(ECompletedness_type)i] = strtmp;
   }
  
   // ini of molclass_names
   for (i = eMolecule_class_type_unknown; i <= eMolecule_class_type_other; i++){
     strtmp = ENUM_METHOD_NAME(EMolecule_class_type)()->FindName(i, true);
     if (strtmp == "dna" || strtmp == "rna") {
         strtmp = NStr::ToUpper(strtmp);
     }
     thisInfo.molclass_names[(EMolecule_class_type)i] = strtmp;
   }

   // ini of topo_names;
   for (i = eTopology_type_unknown; i <= eTopology_type_other; i++) {
     thisInfo.topo_names[(ETopology_type)i] 
         = ENUM_METHOD_NAME(ETopology_type)()->FindName(i, true);
   }
 
   // ini of strand_names
   for (i = eStrand_type_unknown; i <= eStrand_type_other; i++) {
      thisInfo.strand_names[(EStrand_type)i] 
            = ENUM_METHOD_NAME(EStrand_type)()->FindName(i, true);
   }
   
   // ini of suspect_rna_rules;
   strtmp = reg.Get("StringVecIni", "SusRrnaProdNames");
   arr.clear();
   arr = NStr::Tokenize(strtmp, ",", arr);
   ITERATE (vector <string>, it, arr) {
     CRef <CSearch_func> sch_func = MakeSimpleSearchFunc(*it);
     CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
     this_rule->SetFind(*sch_func);
     thisInfo.suspect_rna_rules->Set().push_back(this_rule);
     thisInfo.rna_rule_summ.push_back(
                             summ_susrule.SummarizeSuspectRuleEx(*this_rule));
   }
   CRef <CSearch_func> sch_func = MakeSimpleSearchFunc("8S", true);
   CRef <CSearch_func> except = MakeSimpleSearchFunc("5.8S", true);
   CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
   this_rule->SetFind(*sch_func);
   this_rule->SetExcept(*except);
   thisInfo.suspect_rna_rules->Set().push_back(this_rule); 
   thisInfo.rna_rule_summ.push_back(
                              summ_susrule.SummarizeSuspectRuleEx(*this_rule));

   // ini. of suspect_phrases
   strtmp = reg.Get("StringVecIni", "SuspectPhrases");
   thisInfo.suspect_phrases 
       = NStr::Tokenize(strtmp, ",", thisInfo.suspect_phrases); 

   strs.clear();
   arr.clear();
};

string CRepConfig :: GetDirStr(const string& src_dir)
{
   if (src_dir.empty()) return ("./");
   else {
      size_t pos = src_dir.find_last_of('/');
      if (pos == string::npos) return (src_dir + "/");
      else {
         size_t pos2 = src_dir.find_last_not_of('/');
         if (pos2 > pos) return (src_dir + "/");
         if (pos2 < pos) return (CTempString(src_dir).substr(0, pos2+1) + "/");
      }
   }
   return kEmptyStr;
};

void CRepConfig :: ProcessArgs(Str2Str& args)
{
    // input file/path
    thisInfo.infile = (args.find("i") != args.end()) ? args["i"] : kEmptyStr;
    m_indir = (args.find("p") != args.end()) ? args["p"] : kEmptyStr;
    m_insuffix = args["x"];
    m_dorecurse = (args["u"] == "1") ? true : false;
    if ((m_indir.empty() || !CDir(m_indir).Exists()) 
         && thisInfo.infile.empty()) {
         NCBI_THROW(CException, eUnknown, 
                  "Input path or input file must be specified");
    }
    m_file_tp = args["a"];

    // report category
    thisInfo.output_config.add_output_tag 
         = (thisInfo.report == "t") ? true : false;
    thisInfo.output_config.add_extra_output_tag 
         = (thisInfo.report == "s") ? true : false;
    if (thisInfo.report == "t" || thisInfo.report == "s") {
          thisInfo.report = "Discrepancy";
    }

    // output
    m_outsuffix = args["s"];
    string output_f = (args.find("o") != args.end()) ? args["o"] : kEmptyStr;
    m_outdir= (args.find("r") != args.end()) ? GetDirStr(args["r"]) : kEmptyStr;
    if (!output_f.empty() && !m_outdir.empty()) {
        NCBI_THROW(CException, eUnknown,
            "-o and -r are incompatible: specify the output file name with the full path.");
    }

    if (output_f.empty() && !thisInfo.infile.empty()) {
       CFile file(thisInfo.infile);
       output_f = file.GetDir() + file.GetBase() + m_outsuffix;
    }
    if (m_outdir.empty() && m_indir.empty()) m_outdir = "./";
    if (!m_outdir.empty() 
               && !CDir(m_outdir).Exists() && !CDir(m_outdir).Create()) {
         NCBI_THROW(CException, eUnknown, 
                           "Unable to create output directory " + m_outdir);
   }

    thisInfo.output_config.output_f 
      = output_f.empty() ? 0 : new CNcbiOfstream((m_outdir + output_f).c_str());
    strtmp = args["S"];
    thisInfo.output_config.summary_report 
       = (NStr::EqualNocase(strtmp, "true") || NStr::EqualNocase(strtmp, "t"));

    // enabled and disabled tests
    strtmp = (args.find("e") != args.end()) ? args["e"] : kEmptyStr;
    if (!strtmp.empty()) {
        m_enabled = NStr::Tokenize(strtmp, ", ", m_enabled, NStr::eMergeDelims);
    }
    strtmp = (args.find("d") != args.end()) ? args["d"] : kEmptyStr;
    if (!strtmp.empty()) {
        m_disabled= NStr::Tokenize(strtmp,", ", m_disabled, NStr::eMergeDelims);
    }
};


void CRepConfig :: ReadArgs(const CArgs& args) 
{
    Str2Str arg_map;
    // input file
    if (args["i"]) arg_map["i"] = args["i"].AsString();
    arg_map["a"] = args["a"].AsString();

    // output
    if (args["o"]) arg_map["o"] = args["o"].AsString();
    if (args["r"]) arg_map["r"] = args["r"].AsString();
    arg_map["S"] = args["S"].AsBoolean();
    arg_map["s"] = args["s"].AsString();

    // enabled and disabled tests
    if (args["e"]) arg_map["e"] = args["e"].AsString();
    if (args["d"]) arg_map["d"] = args["d"].AsString();

    // input directory
    if (args["p"]) arg_map["p"] = args["p"].AsString();
    arg_map["x"] = args["x"].AsString();
    arg_map["u"] = args["u"].AsString();

    ProcessArgs(arg_map);
};

CRef <CSearch_func> CRepConfig :: MakeSimpleSearchFunc(const string& match_text, bool whole_word)
{
     CRef <CString_constraint> str_cons (new CString_constraint);
     str_cons->SetMatch_text(match_text);
     if (whole_word) {
         str_cons->SetWhole_word(whole_word);
     }
     CRef <CSearch_func> sch_func (new CSearch_func);
     sch_func->SetString_constraint(*str_cons);
     return sch_func;
};

void CRepConfig :: GetOrgModSubtpName(unsigned num1, unsigned num2, map <string, COrgMod::ESubtype>& orgmodnm_subtp)
{
    unsigned i;
    for (i = num1; i <= num2; i++) {
       strtmp = COrgMod::ENUM_METHOD_NAME(ESubtype)()->FindName(i, true);
       if (!strtmp.empty()) {
           orgmodnm_subtp[strtmp] = (COrgMod::ESubtype)i;
       }
    }
};

void CRepConfig :: CheckThisSeqEntry(CRef <CSeq_entry> seq_entry)
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

    CCheckingClass myChecker;
    myChecker.CheckSeqEntry(seq_entry);

    // go through tests
    CAutoDiscClass autoDiscClass( *(thisInfo.scope), myChecker );
    autoDiscClass.LookAtSeqEntry( *seq_entry );

    // collect disc report
    myChecker.CollectRepData();
};  // CheckThisSeqEntry()

static const s_test_property test_list[] = {
// tests_on_SubmitBlk
   {"DISC_SUBMITBLOCK_CONFLICT", fAsndisc | fOncaller},

// tests_on_Bioseq
   {"DISC_MISSING_DEFLINES", fAsndisc | fOncaller},
   {"DISC_QUALITY_SCORES", fDiscrepancy | fAsndisc},
   {"TEST_TERMINAL_NS", fDiscrepancy | fAsndisc},
   
// tests_on_Bioseq_aa
   {"COUNT_PROTEINS", fAsndisc},
   {"MISSING_PROTEIN_ID", fDiscrepancy | fAsndisc},
   {"INCONSISTENT_PROTEIN_ID", fDiscrepancy | fAsndisc},

// tests_on_Bioseq_na
   {"DISC_COUNT_NUCLEOTIDES", fAsndisc | fOncaller},
   {"TEST_DEFLINE_PRESENT", fDiscrepancy | fAsndisc},
   {"N_RUNS", fDiscrepancy | fAsndisc},
   {"N_RUNS_14", fAsndisc},
   {"ZERO_BASECOUNT", fDiscrepancy | fAsndisc},
   {"TEST_LOW_QUALITY_REGION", fDiscrepancy | fAsndisc},
   {"DISC_PERCENT_N", fDiscrepancy | fAsndisc},
   {"DISC_10_PERCENTN", fAsndisc},
   {"TEST_UNUSUAL_NT", fDiscrepancy | fAsndisc},

// tests_on_Bioseq_CFeat
   {"SUSPECT_PHRASES", fDiscrepancy | fAsndisc},
   {"DISC_SUSPECT_RRNA_PRODUCTS", fDiscrepancy | fAsndisc},
   {"SUSPECT_PRODUCT_NAMES", fDiscrepancy | fAsndisc | fOncaller},
   {"DISC_PRODUCT_NAME_TYPO", fDiscrepancy | fAsndisc},
   {"DISC_PRODUCT_NAME_QUICKFIX", fDiscrepancy | fAsndisc},
   {"TEST_ORGANELLE_PRODUCTS", fAsndisc | fOncaller},
   {"DISC_GAPS", fDiscrepancy | fAsndisc},
   {"TEST_MRNA_OVERLAPPING_PSEUDO_GENE", fAsndisc | fOncaller},
   {"ONCALLER_HAS_STANDARD_NAME", fAsndisc | fOncaller},
   {"ONCALLER_ORDERED_LOCATION", fAsndisc | fOncaller},
   {"DISC_FEATURE_LIST", fDiscrepancy | fAsndisc},
   {"TEST_CDS_HAS_CDD_XREF", fDiscrepancy | fAsndisc},
   {"DISC_CDS_HAS_NEW_EXCEPTION", fAsndisc | fOncaller},
   {"DISC_MICROSATELLITE_REPEAT_TYPE", fAsndisc | fOncaller},
   {"DISC_SUSPECT_MISC_FEATURES", fDiscrepancy | fAsndisc},
   {"DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS", fAsndisc | fOncaller},
   {"DISC_FEATURE_MOLTYPE_MISMATCH", fAsndisc | fOncaller},
   {"ADJACENT_PSEUDOGENES", fDiscrepancy | fAsndisc},
   {"MISSING_GENPRODSET_PROTEIN", fDiscrepancy | fAsndisc},
   {"DUP_GENPRODSET_PROTEIN", fDiscrepancy | fAsndisc},
   {"MISSING_GENPRODSET_TRANSCRIPT_ID", fDiscrepancy | fAsndisc},
   {"DISC_DUP_GENPRODSET_TRANSCRIPT_ID", fDiscrepancy | fAsndisc},
   {"DISC_FEAT_OVERLAP_SRCFEAT", fDiscrepancy | fAsndisc},
   {"CDS_TRNA_OVERLAP", fAsndisc},
   {"TRANSL_NO_NOTE", fAsndisc},
   {"NOTE_NO_TRANSL", fAsndisc},
   {"TRANSL_TOO_LONG", fAsndisc},
   {"TEST_SHORT_LNCRNA", fDiscrepancy | fOncaller},
   {"FIND_STRAND_TRNAS", fDiscrepancy | fAsndisc},
   {"FIND_BADLEN_TRNAS", fAsndisc | fOncaller},
   {"COUNT_TRNAS", fAsndisc},
   {"FIND_DUP_TRNAS", fAsndisc},
   {"COUNT_RRNAS", fAsndisc},
   {"FIND_DUP_RRNAS", fAsndisc},
   {"PARTIAL_CDS_COMPLETE_SEQUENCE", fDiscrepancy | fAsndisc},
   {"CONTAINED_CDS", fDiscrepancy | fAsndisc},
   {"PSEUDO_MISMATCH", fDiscrepancy | fAsndisc | fOncaller},
   {"EC_NUMBER_NOTE", fDiscrepancy | fAsndisc},
   {"NON_GENE_LOCUS_TAG", fDiscrepancy | fAsndisc | fOncaller},
   {"JOINED_FEATURES", fDiscrepancy | fAsndisc},
   {"SHOW_TRANSL_EXCEPT", fDiscrepancy | fAsndisc},
   {"MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS", fDiscrepancy | fAsndisc},
   {"RRNA_NAME_CONFLICTS", fDiscrepancy | fAsndisc},
   {"ONCALLER_GENE_MISSING", fAsndisc | fOncaller},
   {"ONCALLER_SUPERFLUOUS_GENE", fAsndisc},
   {"MISSING_GENES", fDiscrepancy | fAsndisc},
   {"EXTRA_GENES", fDiscrepancy | fAsndisc},
   //tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES"));
   {"OVERLAPPING_CDS", fDiscrepancy | fAsndisc},
   {"RNA_CDS_OVERLAP", fDiscrepancy | fAsndisc},
   {"FIND_OVERLAPPED_GENES", fDiscrepancy | fAsndisc},
   {"OVERLAPPING_GENES", fDiscrepancy | fAsndisc},
   {"DISC_PROTEIN_NAMES", fDiscrepancy | fAsndisc},
   {"DISC_CDS_PRODUCT_FIND", fAsndisc | fOncaller},
   {"EC_NUMBER_ON_UNKNOWN_PROTEIN", fDiscrepancy | fAsndisc},
   {"RNA_NO_PRODUCT", fDiscrepancy | fAsndisc | fOncaller},
   {"DISC_SHORT_INTRON", fDiscrepancy | fAsndisc | fOncaller},
   {"DISC_BAD_GENE_STRAND", fAsndisc | fOncaller},
   {"DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA", fAsndisc | fOncaller},
   {"DISC_SHORT_RRNA", fDiscrepancy | fAsndisc | fOncaller},
   {"TEST_OVERLAPPING_RRNAS", fDiscrepancy | fAsndisc},
   {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", fDiscrepancy | fAsndisc},
   {"DISC_SUSPICIOUS_NOTE_TEXT", fAsndisc | fOncaller},
   {"NO_ANNOTATION", fDiscrepancy | fAsndisc | fOncaller},
   {"DISC_LONG_NO_ANNOTATION", fDiscrepancy | fAsndisc},
   {"DISC_PARTIAL_PROBLEMS", fDiscrepancy | fAsndisc},
   {"TEST_UNUSUAL_MISC_RNA", fDiscrepancy | fAsndisc},
   {"GENE_PRODUCT_CONFLICT", fDiscrepancy | fAsndisc},
   {"DISC_CDS_WITHOUT_MRNA", fAsndisc | fOncaller},

// tests_on_Bioseq_CFeat_NotInGenProdSet
   {"DUPLICATE_GENE_LOCUS", fDiscrepancy | fAsndisc},
   {"MISSING_LOCUS_TAGS", fDiscrepancy | fAsndisc},
   {"DUPLICATE_LOCUS_TAGS", fDiscrepancy | fAsndisc},
   {"INCONSISTENT_LOCUS_TAG_PREFIX", fDiscrepancy | fAsndisc},
   {"BAD_LOCUS_TAG_FORMAT", fDiscrepancy | fAsndisc},
   {"FEATURE_LOCATION_CONFLICT", fDiscrepancy | fAsndisc},

// tests_on_Bioseq_CFeat_CSeqdesc
   {"DISC_FEATURE_COUNT_oncaller", fOncaller},  // oncaller tool version
   {"DISC_FEATURE_COUNT", fAsndisc}, // asndisc version   
   {"DISC_BAD_BGPIPE_QUALS", fDiscrepancy | fAsndisc},
   {"DISC_INCONSISTENT_MOLINFO_TECH", fDiscrepancy | fAsndisc},
   {"SHORT_CONTIG", fDiscrepancy | fAsndisc},
   {"SHORT_SEQUENCES", fDiscrepancy | fAsndisc},
   {"SHORT_SEQUENCES_200", fAsndisc},
   {"TEST_UNWANTED_SPACER", fDiscrepancy | fAsndisc | fOncaller},
   {"TEST_UNNECESSARY_VIRUS_GENE", fAsndisc | fOncaller},
   {"TEST_ORGANELLE_NOT_GENOMIC", fDiscrepancy | fAsndisc | fOncaller},
   {"MULTIPLE_CDS_ON_MRNA", fAsndisc | fOncaller},
   {"TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES", fAsndisc | fOncaller},
   {"TEST_BAD_MRNA_QUAL", fAsndisc | fOncaller},
   {"TEST_EXON_ON_MRNA", fAsndisc | fOncaller},
   {"ONCALLER_HIV_RNA_INCONSISTENT", fAsndisc | fOncaller},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION", fDiscrepancy | fAsndisc},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", fDiscrepancy | fAsndisc},
   {"DISC_MITOCHONDRION_REQUIRED", fAsndisc | fOncaller},
   {"EUKARYOTE_SHOULD_HAVE_MRNA", fDiscrepancy | fAsndisc},
   {"RNA_PROVIRAL", fAsndisc | fOncaller},
   {"NON_RETROVIRIDAE_PROVIRAL", fAsndisc | fOncaller},
   {"DISC_RETROVIRIDAE_DNA", fAsndisc | fOncaller},
   {"DISC_mRNA_ON_WRONG_SEQUENCE_TYPE", fAsndisc | fOncaller},
   {"DISC_RBS_WITHOUT_GENE", fAsndisc | fOncaller},
   {"DISC_EXON_INTRON_CONFLICT", fAsndisc | fOncaller},
   {"TEST_TAXNAME_NOT_IN_DEFLINE", fAsndisc | fOncaller},
   {"INCONSISTENT_SOURCE_DEFLINE", fDiscrepancy | fAsndisc},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", fAsndisc | fOncaller},
   {"DISC_BAD_BACTERIAL_GENE_NAME", fDiscrepancy | fAsndisc},
   {"TEST_BAD_GENE_NAME", fDiscrepancy | fAsndisc},
   {"MOLTYPE_NOT_MRNA", fAsndisc},
   {"TECHNIQUE_NOT_TSA", fAsndisc},
   {"DISC_POSSIBLE_LINKER", fAsndisc | fOncaller},
   {"SHORT_PROT_SEQUENCES", fDiscrepancy | fAsndisc},
   {"TEST_COUNT_UNVERIFIED", fAsndisc | fOncaller},
   {"TEST_DUP_GENES_OPPOSITE_STRANDS", fDiscrepancy | fAsndisc},
   {"DISC_GENE_PARTIAL_CONFLICT", fAsndisc | fOncaller},

// tests_on_SeqEntry
   {"DISC_FLATFILE_FIND_ONCALLER", fAsndisc | fOncaller},
   {"DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE", fAsndisc | fOncaller},
   {"DISC_FLATFILE_FIND_ONCALLER_FIXABLE", fAsndisc | fOncaller},
   {"TEST_ALIGNMENT_HAS_SCORE", fDiscrepancy | fAsndisc},

// tests_on_SeqEntry_feat_desc: all CSeqEntry_Feat_desc tests need RmvRedundancy
   {"DISC_INCONSISTENT_STRUCTURED_COMMENTS", fDiscrepancy | fAsndisc},
   {"DISC_INCONSISTENT_DBLINK", fDiscrepancy | fAsndisc},
   {"END_COLON_IN_COUNTRY", fAsndisc | fOncaller},
   {"ONCALLER_SUSPECTED_ORG_COLLECTED", fAsndisc | fOncaller},
   {"ONCALLER_SUSPECTED_ORG_IDENTIFIED", fAsndisc | fOncaller},
   {"UNCULTURED_NOTES_ONCALLER", fAsndisc | fOncaller}, 
   {"ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY", fAsndisc | fOncaller},
   {"ONCALLER_MORE_NAMES_COLLECTED_BY", fAsndisc | fOncaller},
   {"ONCALLER_STRAIN_TAXNAME_CONFLICT", fAsndisc | fOncaller},
   {"TEST_SMALL_GENOME_SET_PROBLEM", fAsndisc | fOncaller},
   {"DISC_INCONSISTENT_MOLTYPES", fAsndisc | fOncaller},
   {"DISC_BIOMATERIAL_TAXNAME_MISMATCH", fAsndisc | fOncaller},
   {"DISC_CULTURE_TAXNAME_MISMATCH", fAsndisc | fOncaller},
   {"DISC_STRAIN_TAXNAME_MISMATCH", fAsndisc | fOncaller},
   {"DISC_SPECVOUCHER_TAXNAME_MISMATCH", fAsndisc | fOncaller},
   {"DISC_HAPLOTYPE_MISMATCH", fAsndisc | fOncaller},
   {"DISC_MISSING_VIRAL_QUALS", fAsndisc | fOncaller},
   {"DISC_INFLUENZA_DATE_MISMATCH", fDiscrepancy | fAsndisc | fOncaller},
   {"TAX_LOOKUP_MISSING", fDiscrepancy | fAsndisc},
   {"TAX_LOOKUP_MISMATCH", fDiscrepancy | fAsndisc},
   {"MISSING_STRUCTURED_COMMENT", fAsndisc},
   {"ONCALLER_MISSING_STRUCTURED_COMMENTS", fAsndisc | fOncaller},
   {"DISC_MISSING_AFFIL", fAsndisc | fOncaller},
   {"DISC_CITSUBAFFIL_CONFLICT", fAsndisc | fOncaller},
   {"DISC_TITLE_AUTHOR_CONFLICT", fAsndisc | fOncaller},
   {"DISC_USA_STATE", fAsndisc | fOncaller},
   {"DISC_CITSUB_AFFIL_DUP_TEXT", fAsndisc | fOncaller},
   {"DISC_REQUIRED_CLONE", fAsndisc | fOncaller},
   {"ONCALLER_MULTISRC", fAsndisc | fOncaller},
   {"DISC_SRC_QUAL_PROBLEM", fAsndisc | fOncaller},
   {"DISC_SOURCE_QUALS_ASNDISC", fAsndisc}, // asndisc version
   {"DISC_SOURCE_QUALS_ASNDISC_oncaller", fAsndisc | fOncaller},
   {"DISC_UNPUB_PUB_WITHOUT_TITLE", fAsndisc | fOncaller},
   {"ONCALLER_CONSORTIUM", fAsndisc | fOncaller},
   {"DISC_CHECK_AUTH_NAME", fAsndisc | fOncaller},
   {"DISC_CHECK_AUTH_CAPS", fAsndisc | fOncaller},
   {"DISC_MISMATCHED_COMMENTS", fDiscrepancy | fAsndisc},
   {"ONCALLER_COMMENT_PRESENT", fAsndisc | fOncaller},
   {"DUP_DISC_ATCC_CULTURE_CONFLICT", fAsndisc | fOncaller},
   {"ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH", fAsndisc | fOncaller},
   {"DISC_DUP_DEFLINE", fAsndisc | fOncaller},
   {"DISC_TITLE_ENDS_WITH_SEQUENCE", fDiscrepancy | fAsndisc},
   {"ONCALLER_DEFLINE_ON_SET", fAsndisc | fOncaller},
   {"DISC_BACTERIAL_TAX_STRAIN_MISMATCH", fDiscrepancy | fAsndisc},
   {"DUP_DISC_CBS_CULTURE_CONFLICT", fAsndisc | fOncaller},
   {"INCONSISTENT_BIOSOURCE", fDiscrepancy | fAsndisc},
   {"ONCALLER_BIOPROJECT_ID", fAsndisc | fOncaller},
   {"ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX", fAsndisc | fOncaller},
   {"MISSING_GENOMEASSEMBLY_COMMENTS", fDiscrepancy | fAsndisc},
   {"TEST_HAS_PROJECT_ID", fAsndisc | fOncaller},
   {"DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER", fAsndisc | fOncaller},
   {"ONCALLER_DUPLICATE_PRIMER_SET", fAsndisc | fOncaller},
   {"ONCALLER_COUNTRY_COLON", fAsndisc | fOncaller},
   {"TEST_MISSING_PRIMER", fAsndisc | fOncaller},
   {"TEST_SP_NOT_UNCULTURED", fAsndisc | fOncaller},
   {"DISC_METAGENOMIC", fAsndisc | fOncaller},
   {"DISC_MAP_CHROMOSOME_CONFLICT", fAsndisc | fOncaller},
   {"DIVISION_CODE_CONFLICTS", fAsndisc | fOncaller},
   {"TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE", fAsndisc | fOncaller},
   {"TEST_UNNECESSARY_ENVIRONMENTAL", fAsndisc | fOncaller},
   {"DISC_HUMAN_HOST", fAsndisc | fOncaller},
   {"ONCALLER_MULTIPLE_CULTURE_COLLECTION", fAsndisc | fOncaller},
   {"ONCALLER_CHECK_AUTHORITY", fAsndisc | fOncaller},
   {"DISC_METAGENOME_SOURCE", fAsndisc | fOncaller},
   {"DISC_BACTERIA_MISSING_STRAIN", fAsndisc | fOncaller},
   {"DISC_REQUIRED_STRAIN", fDiscrepancy | fAsndisc},
   {"MISSING_PROJECT", fAsndisc},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE", fAsndisc | fOncaller},

// tests_on_BioseqSet   // redundant because of nested set?
   {"DISC_SEGSETS_PRESENT", fDiscrepancy | fAsndisc},
   {"TEST_UNWANTED_SET_WRAPPER", fAsndisc | fOncaller},
   {"DISC_NONWGS_SETS_PRESENT", fDiscrepancy | fAsndisc}
};

CRepConfig* CRepConfig :: factory(string report_tp, CSeq_entry_Handle* tse_p)
{
   thisInfo.report = report_tp;   // arg_map["P"]
   if (report_tp == "Asndisc") {
         thisInfo.output_config.use_flag = true; // output flags
         return new CRepConfAsndisc();
   }
   else if (report_tp ==  "Discrepancy" || report_tp == "Oncaller") {
       if (tse_p) {
           m_TopSeqEntry = tse_p;
       }
       else {
            NCBI_THROW(CException, eUnknown, "Missing top seq-entry-handle");
       }
       if (report_tp ==  "Discrepancy") {
          return new  CRepConfDiscrepancy();
       }
       else  {
          return new  CRepConfOncaller();
       }
   }
   else {
      NCBI_THROW(CException, eUnknown, "Unrecognized report type.");
      return 0;
   }
};  // CRepConfig::factory()

void CRepConfig :: GetTestList() 
{
   unsigned sz = thisTest.tests_run.size(), i=0;
   set <string>::iterator end_it = thisTest.tests_run.end();
   if (i >= sz) return;
   if (thisTest.tests_run.find("DISC_SUBMITBLOCK_CONFLICT") != end_it) {
        thisGrp.tests_on_SubmitBlk.push_back(
            CRef <CTestAndRepData>(new CSeqEntry_DISC_SUBMITBLOCK_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_DEFLINES") != end_it) {
       thisGrp.tests_on_Bioseq.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_MISSING_DEFLINES));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_COUNT_NUCLEOTIDES") != end_it) {
        thisGrp.tests_on_Bioseq_na.push_back(
                  CRef <CTestAndRepData>(new CBioseq_DISC_COUNT_NUCLEOTIDES));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_QUALITY_SCORES") != end_it) {
        thisGrp.tests_on_Bioseq.push_back(
                 CRef <CTestAndRepData>(new CBioseq_DISC_QUALITY_SCORES));
        thisInfo.test_item_list["DISC_QUALITY_SCORES"].clear();
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("TEST_TERMINAL_NS") != end_it) {
        thisGrp.tests_on_Bioseq.push_back(
                 CRef <CTestAndRepData>(new CBioseq_TEST_TERMINAL_NS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_COUNT_oncaller") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>
                                     (new CBioseq_DISC_FEATURE_COUNT_oncaller));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_FEATURE_COUNT") != end_it) {
        thisGrp.tests_on_Bioseq_CFeat.push_back(
                  CRef <CTestAndRepData>(new CBioseq_DISC_FEATURE_COUNT)); 
        if (++i >= sz) return;
   } 
   if ( thisTest.tests_run.find("COUNT_PROTEINS") != end_it) {
        thisGrp.tests_on_Bioseq_aa.push_back(
              CRef <CTestAndRepData>(new CBioseq_COUNT_PROTEINS));
        if (++i >= sz) return;
   } 
   if ( thisTest.tests_run.find("MISSING_PROTEIN_ID") != end_it) {
        thisGrp.tests_on_Bioseq_aa.push_back(
            CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_PROTEIN_ID") != end_it) {
        thisGrp.tests_on_Bioseq_aa.push_back(
            CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_DEFLINE_PRESENT") != end_it) {
         thisGrp.tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_DEFLINE_PRESENT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("N_RUNS") != end_it) {
        thisGrp.tests_on_Bioseq_na.push_back(
            CRef <CTestAndRepData>(new CBioseq_N_RUNS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("N_RUNS_14") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_N_RUNS_14));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ZERO_BASECOUNT") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_ZERO_BASECOUNT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_LOW_QUALITY_REGION") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_LOW_QUALITY_REGION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PERCENT_N") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_PERCENT_N));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_10_PERCENTN") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_10_PERCENTN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNUSUAL_NT") != end_it) {
       thisGrp.tests_on_Bioseq_na.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_UNUSUAL_NT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SUSPECT_PHRASES") != end_it) {
        thisGrp.tests_on_Bioseq_CFeat.push_back(
            CRef <CTestAndRepData>(new CBioseq_SUSPECT_PHRASES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPECT_RRNA_PRODUCTS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_RRNA_PRODUCTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SUSPECT_PRODUCT_NAMES") != end_it
        || thisTest.tests_run.find("DISC_PRODUCT_NAME_TYPO") != end_it
        || thisTest.tests_run.find("DISC_PRODUCT_NAME_QUICKFIX") != end_it) {
        
        if (thisTest.tests_run.find("SUSPECT_PRODUCT_NAMES") != end_it) {
            i++;
        }
        if (thisTest.tests_run.find("DISC_PRODUCT_NAME_TYPO") != end_it) {
             i++;
        }
        if (thisTest.tests_run.find("DISC_PRODUCT_NAME_QUICKFIX") != end_it) {
             i++;
        }
        thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
            CRef <CTestAndRepData>(new CBioseq_on_SUSPECT_RULE()));
        if (i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_ORGANELLE_PRODUCTS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_ORGANELLE_PRODUCTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_GAPS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData> (new CBioseq_DISC_GAPS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MRNA_OVERLAPPING_PSEUDO_GENE") != end_it){
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> 
           (new CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_HAS_STANDARD_NAME") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData> (new CBioseq_ONCALLER_HAS_STANDARD_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_ORDERED_LOCATION") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData> (new CBioseq_ONCALLER_ORDERED_LOCATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_LIST") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
                    CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_LIST));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_CDS_HAS_CDD_XREF") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TEST_CDS_HAS_CDD_XREF));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_HAS_NEW_EXCEPTION") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_DISC_CDS_HAS_NEW_EXCEPTION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MICROSATELLITE_REPEAT_TYPE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
          CRef <CTestAndRepData> (new CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPECT_MISC_FEATURES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_MISC_FEATURES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS") 
            != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> 
           (new CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_MOLTYPE_MISMATCH") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ADJACENT_PSEUDOGENES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_ADJACENT_PSEUDOGENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENPRODSET_PROTEIN") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUP_GENPRODSET_PROTEIN") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_DUP_GENPRODSET_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENPRODSET_TRANSCRIPT_ID") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
         CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_DUP_GENPRODSET_TRANSCRIPT_ID") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData> 
             (new CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEAT_OVERLAP_SRCFEAT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
            CRef <CTestAndRepData> (new CBioseq_DISC_FEAT_OVERLAP_SRCFEAT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("CDS_TRNA_OVERLAP") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CDS_TRNA_OVERLAP));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TRANSL_NO_NOTE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_NO_NOTE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NOTE_NO_TRANSL") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_NOTE_NO_TRANSL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TRANSL_TOO_LONG") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_TOO_LONG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_SHORT_LNCRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TEST_SHORT_LNCRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_STRAND_TRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_STRAND_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_BADLEN_TRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_BADLEN_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("COUNT_TRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_TRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("COUNT_RRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_RRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("PARTIAL_CDS_COMPLETE_SEQUENCE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("CONTAINED_CDS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CONTAINED_CDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("PSEUDO_MISMATCH") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_PSEUDO_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EC_NUMBER_NOTE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_EC_NUMBER_NOTE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NON_GENE_LOCUS_TAG") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_NON_GENE_LOCUS_TAG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("JOINED_FEATURES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_JOINED_FEATURES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHOW_TRANSL_EXCEPT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>( new CBioseq_SHOW_TRANSL_EXCEPT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>
            ( new CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RRNA_NAME_CONFLICTS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_RRNA_NAME_CONFLICTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_GENE_MISSING") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_ONCALLER_GENE_MISSING));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUPERFLUOUS_GENE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_ONCALLER_SUPERFLUOUS_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
              CRef <CTestAndRepData>(new CBioseq_MISSING_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EXTRA_GENES") != end_it) { 
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_EXTRA_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("OVERLAPPING_CDS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_CDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_CDS_OVERLAP") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_RNA_CDS_OVERLAP));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_OVERLAPPED_GENES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_FIND_OVERLAPPED_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("OVERLAPPING_GENES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PROTEIN_NAMES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_PROTEIN_NAMES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_PRODUCT_FIND") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_CDS_PRODUCT_FIND));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EC_NUMBER_ON_UNKNOWN_PROTEIN") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>( new CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_NO_PRODUCT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>(new CBioseq_RNA_NO_PRODUCT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SHORT_INTRON") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_INTRON));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BAD_GENE_STRAND") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_BAD_GENE_STRAND));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>
           (new CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SHORT_RRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_RRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_OVERLAPPING_RRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_OVERLAPPING_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>
           ( new CBioseq_SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPICIOUS_NOTE_TEXT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData>( new CBioseq_DISC_SUSPICIOUS_NOTE_TEXT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NO_ANNOTATION") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
                           CRef <CTestAndRepData>(new CBioseq_NO_ANNOTATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_LONG_NO_ANNOTATION") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_LONG_NO_ANNOTATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PARTIAL_PROBLEMS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_PARTIAL_PROBLEMS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNUSUAL_MISC_RNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_TEST_UNUSUAL_MISC_RNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("GENE_PRODUCT_CONFLICT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData>( new CBioseq_GENE_PRODUCT_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_WITHOUT_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back( 
           CRef <CTestAndRepData>( new CBioseq_DISC_CDS_WITHOUT_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUPLICATE_GENE_LOCUS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
           CRef <CTestAndRepData>(new CBioseq_DUPLICATE_GENE_LOCUS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_LOCUS_TAGS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
           CRef <CTestAndRepData>(new CBioseq_MISSING_LOCUS_TAGS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUPLICATE_LOCUS_TAGS") != end_it
        || thisTest.tests_run.find("BAD_LOCUS_TAG_FORMAT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
           CRef <CTestAndRepData>(new CBioseq_on_locus_tags));
        if (thisTest.tests_run.find("DUPLICATE_LOCUS_TAGS") != end_it) i++;
        if (thisTest.tests_run.find("BAD_LOCUS_TAG_FORMAT") != end_it) i++;
        if (i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_LOCUS_TAG_PREFIX") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
            CRef <CTestAndRepData>(new  CBioseq_INCONSISTENT_LOCUS_TAG_PREFIX));
       if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FEATURE_LOCATION_CONFLICT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
           CRef <CTestAndRepData>(new CBioseq_FEATURE_LOCATION_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_MOLINFO_TECH") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef<CTestAndRepData>(new CBioseq_DISC_INCONSISTENT_MOLINFO_TECH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_CONTIG") != end_it) {
       thisGrp.tests_on_Bioseq_NotInGenProdSet.push_back(
           CRef<CTestAndRepData>(new CBioseq_SHORT_CONTIG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_SEQUENCES") != end_it) {
       thisGrp.tests_on_Bioseq_NotInGenProdSet.push_back(
           CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_SEQUENCES_200") != end_it) {
       thisGrp.tests_on_Bioseq_NotInGenProdSet.push_back(
           CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES_200));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BAD_BGPIPE_QUALS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_DISC_BAD_BGPIPE_QUALS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNWANTED_SPACER") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_UNWANTED_SPACER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNNECESSARY_VIRUS_GENE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_UNNECESSARY_VIRUS_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_ORGANELLE_NOT_GENOMIC") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_ORGANELLE_NOT_GENOMIC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MULTIPLE_CDS_ON_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>(new CBioseq_MULTIPLE_CDS_ON_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                       new CBioseq_TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_BAD_MRNA_QUAL") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_BAD_MRNA_QUAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_EXON_ON_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_EXON_ON_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_HIV_RNA_INCONSISTENT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>( new CBioseq_ONCALLER_HIV_RNA_INCONSISTENT));
        if (++i >= sz) return;
   }
   if(thisTest.tests_run.find("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION")
                                                             != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>( 
           new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS")
                                                             != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
           new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MITOCHONDRION_REQUIRED") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>( new CBioseq_DISC_MITOCHONDRION_REQUIRED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EUKARYOTE_SHOULD_HAVE_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>( new CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_PROVIRAL") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData>( new CBioseq_RNA_PROVIRAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NON_RETROVIRIDAE_PROVIRAL") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>( new CBioseq_NON_RETROVIRIDAE_PROVIRAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_RETROVIRIDAE_DNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>( new CBioseq_DISC_RETROVIRIDAE_DNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_mRNA_ON_WRONG_SEQUENCE_TYPE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
         CRef <CTestAndRepData>( new CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_RBS_WITHOUT_GENE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_RBS_WITHOUT_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_EXON_INTRON_CONFLICT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_EXON_INTRON_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_TAXNAME_NOT_IN_DEFLINE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_SOURCE_DEFLINE") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_SOURCE_DEFLINE));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA") != end_it){
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>
          (new CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BAD_BACTERIAL_GENE_NAME") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_DISC_BAD_BACTERIAL_GENE_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_BAD_GENE_NAME") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_TEST_BAD_GENE_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_POSSIBLE_LINKER") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_DISC_POSSIBLE_LINKER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MOLTYPE_NOT_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_MOLTYPE_NOT_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TECHNIQUE_NOT_TSA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_TECHNIQUE_NOT_TSA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_PROT_SEQUENCES") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData> (new CBioseq_SHORT_PROT_SEQUENCES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_COUNT_UNVERIFIED") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_TEST_COUNT_UNVERIFIED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_DUP_GENES_OPPOSITE_STRANDS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>
           (new CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_GENE_PARTIAL_CONFLICT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
               CRef <CTestAndRepData>(new CBioseq_DISC_GENE_PARTIAL_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER") != end_it
          || thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE")
                  != end_it
          || thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER_FIXABLE") 
                  != end_it) {
       thisGrp.tests_on_SeqEntry.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_FLATFILE_FIND_ONCALLER));
       if (thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER") != end_it) {
             i++;
       }
       if (thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE")
              != end_it) {
          i++;
       }
       if (thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER_FIXABLE")
               != end_it) {
          i++;
        }
        if (i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_ALIGNMENT_HAS_SCORE") != end_it) {
       thisGrp.tests_on_SeqEntry.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_TEST_ALIGNMENT_HAS_SCORE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_STRUCTURED_COMMENTS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_DBLINK") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_DBLINK));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("END_COLON_IN_COUNTRY") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_END_COLON_IN_COUNTRY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUSPECTED_ORG_COLLECTED") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
            (new CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUSPECTED_ORG_IDENTIFIED") != end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("UNCULTURED_NOTES_ONCALLER") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CSeqEntry_UNCULTURED_NOTES_ONCALLER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MORE_NAMES_COLLECTED_BY")!= end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
            (new CSeqEntry_ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY));
        if (++i >= sz) return;
   }

   if ( thisTest.tests_run.find("ONCALLER_STRAIN_TAXNAME_CONFLICT")!=end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
            (new CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_SMALL_GENOME_SET_PROBLEM") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_MOLTYPES") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_MOLTYPES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BIOMATERIAL_TAXNAME_MISMATCH") != end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CULTURE_TAXNAME_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_STRAIN_TAXNAME_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
            CRef <CTestAndRepData>(new CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SPECVOUCHER_TAXNAME_MISMATCH") != end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_HAPLOTYPE_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_HAPLOTYPE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_VIRAL_QUALS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_VIRAL_QUALS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INFLUENZA_DATE_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TAX_LOOKUP_MISSING") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISSING));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TAX_LOOKUP_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_STRUCTURED_COMMENT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_MISSING_STRUCTURED_COMMENT));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("ONCALLER_MISSING_STRUCTURED_COMMENTS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
            new CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_AFFIL") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_AFFIL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CITSUBAFFIL_CONFLICT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUBAFFIL_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TITLE_AUTHOR_CONFLICT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_USA_STATE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_USA_STATE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CITSUB_AFFIL_DUP_TEXT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_REQUIRED_CLONE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_CLONE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MULTISRC") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTISRC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SRC_QUAL_PROBLEM") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_SRC_QUAL_PROBLEM));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_SOURCE_QUALS_ASNDISC") != end_it) { //asndisc version
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_SOURCE_QUALS_ASNDISC_oncaller") != end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                             new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_UNPUB_PUB_WITHOUT_TITLE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_CONSORTIUM") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CONSORTIUM));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_AUTH_NAME") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_AUTH_CAPS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_CAPS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISMATCHED_COMMENTS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_MISMATCHED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_COMMENT_PRESENT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COMMENT_PRESENT));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DUP_DISC_ATCC_CULTURE_CONFLICT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
          CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH") 
                                                           != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
           new CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_DUP_DEFLINE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_DEFLINE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TITLE_ENDS_WITH_SEQUENCE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_DEFLINE_ON_SET") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DEFLINE_ON_SET));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIAL_TAX_STRAIN_MISMATCH") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUP_DISC_CBS_CULTURE_CONFLICT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_BIOSOURCE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_INCONSISTENT_BIOSOURCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_BIOPROJECT_ID") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_BIOPROJECT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
           new CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENOMEASSEMBLY_COMMENTS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
            (new CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_HAS_PROJECT_ID") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_TEST_HAS_PROJECT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_DUPLICATE_PRIMER_SET") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_COUNTRY_COLON") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COUNTRY_COLON));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MISSING_PRIMER") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_TEST_MISSING_PRIMER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_SP_NOT_UNCULTURED") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_TEST_SP_NOT_UNCULTURED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_METAGENOMIC") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOMIC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MAP_CHROMOSOME_CONFLICT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DIVISION_CODE_CONFLICTS") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DIVISION_CODE_CONFLICTS));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE")
           != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back( CRef <CTestAndRepData>(
          new CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNNECESSARY_ENVIRONMENTAL") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
          CRef <CTestAndRepData>(new CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_HUMAN_HOST") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DISC_HUMAN_HOST));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MULTIPLE_CULTURE_COLLECTION") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>
           (new CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_CHECK_AUTHORITY") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CHECK_AUTHORITY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_METAGENOME_SOURCE") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOME_SOURCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIA_MISSING_STRAIN") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIA_MISSING_STRAIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_REQUIRED_STRAIN") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_STRAIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_PROJECT") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MISSING_PROJECT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE") 
                                                            != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
           new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE)); // not tested
       if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SEGSETS_PRESENT") != end_it) {
       thisGrp.tests_on_BioseqSet.push_back(
           CRef <CTestAndRepData>(new CBioseqSet_DISC_SEGSETS_PRESENT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNWANTED_SET_WRAPPER") != end_it) {
       thisGrp.tests_on_BioseqSet.push_back(
           CRef <CTestAndRepData>(new CBioseqSet_TEST_UNWANTED_SET_WRAPPER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_NONWGS_SETS_PRESENT") != end_it) {
       thisGrp.tests_on_BioseqSet.push_back(
           CRef <CTestAndRepData>(new CBioseqSet_DISC_NONWGS_SETS_PRESENT));
        if (++i >= sz) return;
   }

   NCBI_THROW(CException, eUnknown, "Some requested tests are unrecognizable.");
};

void CRepConfAsndisc :: x_ReadAsn1(ESerialDataFormat datafm)
{
    auto_ptr <CObjectIStream> 
        ois (CObjectIStream::Open(datafm, thisInfo.infile));
    CRef <CSeq_entry> seq_entry (new CSeq_entry);
    strtmp = ois->ReadFileHeader();
    ois->SetStreamPos(0);
    if (strtmp == "Seq-submit") {
       *ois >> thisInfo.seq_submit.GetObject();
       if (thisInfo.seq_submit->IsEntrys()) {
         ITERATE (list <CRef <CSeq_entry> >, it, 
                                  thisInfo.seq_submit->GetData().GetEntrys()) {
            CheckThisSeqEntry(*it);
         }
       }
    }
    else if (strtmp == "Seq-entry") {
       *ois >> *seq_entry;
       CheckThisSeqEntry(seq_entry);
    }
    else if (strtmp == "Bioseq") {
       CRef <CBioseq> bioseq (new CBioseq);
       *ois >> *bioseq;
       seq_entry->SetSeq(*bioseq);
       CheckThisSeqEntry(seq_entry);
    }
    else if (strtmp == "Bioseq-set") {
       CRef <CBioseq_set> seq_set(new CBioseq_set);
       *ois >> *seq_set;
       seq_entry->SetSet(*seq_set);
       CheckThisSeqEntry(seq_entry);
    }
    else {
        NCBI_THROW(CException, eUnknown, "Couldn't read type [" + strtmp + "]");
    }
    
    ois->Close();
};

void CRepConfAsndisc :: x_ReadFasta()
{
   auto_ptr <CReaderBase> reader (CReaderBase::GetReader(CFormatGuess::eFasta)); // flags?
   if (!reader.get()) {
       NCBI_THROW(CException, eUnknown, "File format not supported");
   }
   CRef <CSerialObject> 
       obj= reader->ReadObject(*(new CNcbiIfstream(thisInfo.infile.c_str())));
   if (!obj) {
      NCBI_THROW(CException, eUnknown, "Couldn't read file " + thisInfo.infile);
   }
   CSeq_entry& seq_entry = dynamic_cast <CSeq_entry&>(obj.GetObject());
   seq_entry.ResetParentEntry();
   CheckThisSeqEntry(CRef <CSeq_entry> (&seq_entry));
};

void CRepConfAsndisc :: x_GuessFile()
{
   CFormatGuess::EFormat f_fmt = CFormatGuess::Format(thisInfo.infile);
   switch (f_fmt) {
      case CFormatGuess :: eBinaryASN:
          x_ReadAsn1(eSerial_AsnBinary);
          break;
      case CFormatGuess :: eTextASN:
          x_ReadAsn1(); 
          break;
      case CFormatGuess :: eFlatFileSequence:
          break;
      case CFormatGuess :: eFasta:
          x_ReadFasta();
          break;
      default:
         NCBI_THROW(CException, eUnknown,
              "File format: " + NStr::UIntToString((unsigned)f_fmt) + " not supported");
   }
};

void CSeqEntryReadHook :: SkipClassMember(CObjectIStream& in, const CObjectTypeInfoMI& passed_info)
{
   CRef <CSeq_entry> entry (new CSeq_entry);

   // Iterate through the set of Seq-entry's in container Bioseq-set
   CIStreamContainerIterator iter(in, passed_info.GetMemberType());
   for ( ; iter; ++iter ) {
      iter >> *entry; // Read a single Seq-entry from the stream.
      CRepConfig::CheckThisSeqEntry(entry);  // Process the Seq-entry
      thisInfo.config->Export(); 
      thisInfo.disc_report_data.clear();
      thisInfo.test_item_list.clear();
   }
};

void CRepConfAsndisc :: x_BatchSet(ESerialDataFormat datafm)
{
   auto_ptr<CObjectIStream> ois (CObjectIStream::Open(datafm, thisInfo.infile));
   ois->SetPathSkipMemberHook("Bioseq-set.seq-set", new CSeqEntryReadHook);
   ois->Skip(CType<CBioseq_set>()); 
};

void CSeqEntryChoiceHook :: SkipChoiceVariant(CObjectIStream& in,const CObjectTypeInfoCV& passed_info)
{
   CRef <CSeq_entry> entry (new CSeq_entry);

   // Iterate through the set of Seq-entry's in container Seq-submit
   CIStreamContainerIterator iter(in, passed_info.GetVariantType());
   for ( ; iter; ++iter ) {
     iter >> *entry; // Read a single Seq-entry from the stream.
     CRepConfig::CheckThisSeqEntry(entry);  // Process the Seq-entry
   }
};

void CRepConfAsndisc :: x_BatchSeqSubmit(ESerialDataFormat datafm)
{
   auto_ptr<CObjectIStream> ois (CObjectIStream::Open(datafm, thisInfo.infile));
   ois->SetPathSkipVariantHook("Seq-submit.entrys-set",new CSeqEntryChoiceHook);
   ois->Skip(CType<CBioseq_set>()); 
};

void CRepConfAsndisc :: x_CatenatedSeqEntry()
{
   auto_ptr <CObjectIStream> 
      ois (CObjectIStream::Open(eSerial_AsnText, thisInfo.infile));
   while (!ois->EndOfData()) {
        CRef <CSeq_entry> seq_entry;
        *ois >> *seq_entry;
        CheckThisSeqEntry(seq_entry); 
   }
};

void CRepConfAsndisc :: x_ProcessOneFile()
{
   if (!CFile(thisInfo.infile).Exists())
      NCBI_THROW(CException, eUnknown, "Can't read file " + thisInfo.infile);
   switch (m_file_tp[0]) {
      case 'a': x_GuessFile(); 
                break;
      case 'e':
      case 'b': 
      case 's':
      case 'm':
         x_ReadAsn1(); 
         break;
// binary, compressed & batch ??? ### 
      case 't': x_BatchSet(); 
                break;
      case 'u': x_BatchSeqSubmit(); 
                break;
      case 'c': x_CatenatedSeqEntry(); 
               break;
      default: x_GuessFile();
   }
};

void CRepConfAsndisc :: x_ProcessDir(const CDir& dir, bool one_ofile)
{
   CDir::TGetEntriesFlags flag = m_dorecurse ? 0 : CDir::fIgnoreRecursive ;
   list <AutoPtr <CDirEntry> > entries = dir.GetEntries(kEmptyStr, flag);
   ITERATE (list <AutoPtr <CDirEntry> >, it, entries) {
     if ((*it)->GetExt() == m_insuffix) {
        thisInfo.infile = (*it)->GetDir() + (*it)->GetName();
        x_ProcessOneFile();
        if (!one_ofile) { 
          strtmp = (!m_outdir.empty()) ? m_outdir : (*it)->GetDir();
          thisInfo.output_config.output_f 
             = new CNcbiOfstream(
                    ( strtmp + (*it)->GetBase() + m_outsuffix ).c_str());     
          thisInfo.config->Export();
          thisInfo.disc_report_data.clear();
          thisInfo.test_item_list.clear(); 
        }
     }
   }
};

void CRepConfig :: CollectTests()
{
   ETestCategoryFlags cate_flag;
   if (thisInfo.report == "Asndisc") {
       cate_flag = fAsndisc;
       thisInfo.output_config.use_flag = true;
   }
   else if (thisInfo.report == "Discrepancy") {
       cate_flag = fDiscrepancy;
       thisInfo.output_config.use_flag = true;
   }
   else if(thisInfo.report ==  "Oncaller") {
       cate_flag = fOncaller;
   }
   else {
       NCBI_THROW(CException, eUnknown, "Unrecognized report type.");
   }

   for (unsigned i=0; i< sizeof(test_list)/sizeof(s_test_property); i++) {
      if (test_list[i].category & cate_flag) {
                thisTest.tests_run.insert(test_list[i].name);
      }
   }
   
   ITERATE (vector <string>, it, m_enabled) {
      if (thisTest.tests_run.find(*it) == thisTest.tests_run.end()) {
            thisTest.tests_run.insert(*it);
      }
   }
   ITERATE (vector <string>, it, m_disabled) {
      if (thisTest.tests_run.find(*it) != thisTest.tests_run.end()) {
           thisTest.tests_run.erase(*it);
      }
   }
  
   GetTestList();
};

void CRepConfAsndisc :: Run(CRef <CRepConfig> config)
{
   thisInfo.config  = config;

   // read input file/path and go tests
   CDir dir(m_indir);
   if (!dir.Exists()) {
       x_ProcessOneFile();
       config->Export();
   }
   else {
       bool one_ofile = (bool)thisInfo.output_config.output_f; 
       x_ProcessDir(dir, one_ofile);
       if (one_ofile) {
          config->Export(); // run a global check 
       }
   }
   thisInfo.disc_report_data.clear();
   thisInfo.test_item_list.clear();

    // Exit
    thisInfo.tax_db_conn.Fini();
}; // Run()

void CRepConfig :: ProcessArgs()
{
    m_enabled.clear();
    m_disabled.clear();
};

// void CRepConfDiscrepancy :: Run(CRef <CRepConfig> config)
void CRepConfig :: Run(CRef <CRepConfig> config)
{
  CRef <CSeq_entry> 
    seq_ref ((CSeq_entry*)(m_TopSeqEntry->GetCompleteSeq_entry().GetPointer()));
  CheckThisSeqEntry(seq_ref);
};

END_NCBI_SCOPE
