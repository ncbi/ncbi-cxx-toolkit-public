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
#include <corelib/ncbimisc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
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
ETestCategoryFlags                  CDiscRepInfo :: report;
CRef <CRepConfig>                   CDiscRepInfo :: config;
vector < CRef < CClickableItem > >  CDiscRepInfo :: disc_report_data;
Str2Strs                            CDiscRepInfo :: test_item_list;
Str2Objs                            CDiscRepInfo :: test_item_objs;
COutputConfig                       CDiscRepInfo :: output_config;
CConstRef <CSuspect_rule_set >      CDiscRepInfo :: suspect_prod_rules(new CSuspect_rule_set);
vector < vector <string> >          CDiscRepInfo :: susrule_summ;
vector <string> 	            CDiscRepInfo :: weasels;
CRef <CSeq_submit>             CDiscRepInfo :: seq_submit(new CSeq_submit);
bool                                CDiscRepInfo :: expand_defline_on_set;
bool                                CDiscRepInfo :: expand_srcqual_report;
string                              CDiscRepInfo :: report_lineage;
bool                                CDiscRepInfo :: exclude_dirsub;
Str2CombDt                          CDiscRepInfo :: rRNATerms;
vector <string>                     CDiscRepInfo :: bad_gene_names_contained;
vector <string>                     CDiscRepInfo :: no_multi_qual;
vector <string>                     CDiscRepInfo :: short_auth_nms;
vector <string>                     CDiscRepInfo :: spec_words_biosrc;
vector <string>                     CDiscRepInfo :: suspicious_notes;
vector <string>                     CDiscRepInfo :: trna_list;
vector <string>                     CDiscRepInfo :: rrna_standard_name;
Str2UInt                            CDiscRepInfo :: desired_aaList;
Str2Str                             CDiscRepInfo :: state_abbrev;
Str2Str                             CDiscRepInfo :: cds_prod_find;
vector <string>                     CDiscRepInfo :: new_exceptions;
Str2Str	                            CDiscRepInfo :: srcqual_keywords;
vector <string>                     CDiscRepInfo :: kIntergenicSpacerNames;
vector <string>                     CDiscRepInfo :: virus_lineage;
vector <string>                     CDiscRepInfo :: strain_tax;
Str2CombDt                          CDiscRepInfo :: fix_data;
CConstRef<CSuspect_rule_set>        CDiscRepInfo :: orga_prod_rules (new CSuspect_rule_set);
vector <string>                     CDiscRepInfo :: skip_bracket_paren;
vector <string>                     CDiscRepInfo :: ok_num_prefix;
map <EMacro_feature_type, CSeqFeatData::ESubtype> CDiscRepInfo :: feattype_featdef;
Str2Str                             CDiscRepInfo :: featkey_modified;
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
const s_SuspectProductNameData*     CDiscRepInfo :: suspect_prod_terms;
unsigned                            CDiscRepInfo :: num_suspect_prod_terms;

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

void CRepConfig :: InitParams(const IRWRegistry* reg)
{
    int i;
    string strtmp, tmp;

    // get other parameters from *.ini
    CSuspect_rule_set rule_set, rule_set2;
    if (reg) {
        thisInfo.expand_defline_on_set 
           = reg->GetBool("OncallerTool", "EXPAND_DEFLINE_ON_SET", true);
        thisInfo.expand_srcqual_report
           = reg->GetBool("OncallerTool", "EXPAND_SRCQUAL_REPORT", true);

        strtmp = reg->Get("RuleFiles", "OrganelleProductRuleFile");
        CRef <CSuspect_rule_set> rule_set(new CSuspect_rule_set);
        if (CFile(strtmp).Exists()) {
            ReadInBlob<CSuspect_rule_set>(*rule_set, strtmp);
            thisInfo.orga_prod_rules.Reset(rule_set.GetPointer());
        }
        else {
            thisInfo.orga_prod_rules.Reset(
                        CSuspect_rule_set::GetOrganelleProductRules());
        }

        // ini. suspect rule file
        strtmp = reg->Get("RuleFiles", "PRODUCT_RULES_LIST");
        if (CFile(strtmp).Exists()) {
            rule_set.Reset(new CSuspect_rule_set);
            ReadInBlob<CSuspect_rule_set>(*rule_set, strtmp);
            thisInfo.suspect_prod_rules.Reset(rule_set.GetPointer());
        }
        else {
            thisInfo.suspect_prod_rules.Reset(
                                 CSuspect_rule_set::GetProductRules());
        }
    }
    else { // default
       thisInfo.expand_defline_on_set = thisInfo.expand_srcqual_report = false;

       // ini of orga_prod_rules:
       thisInfo.orga_prod_rules.Reset(CSuspect_rule_set::GetOrganelleProductRules());
       // ini. suspect rule file
       thisInfo.suspect_prod_rules.Reset(CSuspect_rule_set::GetProductRules());
    }

    // ini. of srcqual_keywords
    thisInfo.srcqual_keywords["forma-specialis"] = " f. sp.";
    thisInfo.srcqual_keywords["forma"] = " f.";
    thisInfo.srcqual_keywords["sub-species"] = " subsp.";
    thisInfo.srcqual_keywords["variety"] = " var.";
    thisInfo.srcqual_keywords["pathovar"] = " pv.";

    // ini. of new_exceptions
    strtmp = "annotated by transcript or proteomic data,heterogeneous population sequenced,low-quality sequence region,unextendable partial coding region";
    thisInfo.new_exceptions 
        = NStr::Tokenize(strtmp, ",", thisInfo.new_exceptions);

    // ini. of kIntergenicSpacerNames
    strtmp = "trnL-trnF intergenic spacer,trnH-psbA intergenic spacer,trnS-trnG intergenic spacer,trnF-trnL intergenic spacer,psbA-trnH intergenic spacer,trnG-trnS intergenic spacer";
    thisInfo.kIntergenicSpacerNames 
         = NStr::Tokenize(strtmp, ",", thisInfo.kIntergenicSpacerNames);
    
    // ini. of weasels
    strtmp = "candidate,hypothetical,novel,possible,potential,predicted,probable,putative,candidate,uncharacterized,unique";
    thisInfo.weasels = NStr::Tokenize(strtmp, ",", thisInfo.weasels);

    // ini. of rRNATerms
    const char* rRnaTerms[] = {
        "16S,1000,0",
        "18S,1000,0",
        "23S,2000,0",
        "25s,1000,0",
        "26S,1000,0",
        "28S,3300,0",
        "small,1000,0",
        "large,1000,0",
        "5.8S,130,1",
        "5S,90,1"
    };
    for (i=0; i< (int)ArraySize(rRnaTerms); i++) {
        arr.clear();
        arr = NStr::Tokenize(rRnaTerms[i], ",", arr);
        thisInfo.rRNATerms[arr[0]].i_val = NStr::StringToInt(arr[1]);
        thisInfo.rRNATerms[arr[0]].b_val = (arr[2] == "1");
    }

    // ini. of no_multi_qual
    strtmp = "location,taxname,taxid";
    thisInfo.no_multi_qual =NStr::Tokenize(strtmp, ",", thisInfo.no_multi_qual);
  
    // ini. of bad_gene_names_contained
    strtmp = "putative,fragment,gene,orf,like";
    thisInfo.bad_gene_names_contained 
        = NStr::Tokenize(strtmp, ",", thisInfo.bad_gene_names_contained);

    // ini. of suspicious notes
    strtmp = "characterised,recognised,characterisation,localisation,tumour,uncharacterised,oxydase,colour,localise,faecal,orthologue,paralogue,homolog,homologue,intronless gene";
    thisInfo.suspicious_notes 
        = NStr::Tokenize(strtmp, ",", thisInfo.suspicious_notes);

    // ini. of spec_words_biosrc;
    strtmp = "institute,institution,University,College";
    thisInfo.spec_words_biosrc 
        = NStr::Tokenize(strtmp, ",", thisInfo.spec_words_biosrc);

    // ini. of trna_list:
    strtmp = "tRNA-Gap,tRNA-Ala,tRNA-Asx,tRNA-Cys,tRNA-Asp,tRNA-Glu,tRNA-Phe,tRNA-Gly,tRNA-His,tRNA-Ile,tRNA-Xle,tRNA-Lys,tRNA-Leu,tRNA-Met,tRNA-Asn,tRNA-Pyl,tRNA-Pro,tRNA-Gln,tRNA-Arg,tRNA-Ser,tRNA-Thr,tRNA-Sec,tRNA-Val,tRNA-Trp,tRNA-OTHER,tRNA-Tyr,tRNA-Glx,tRNA-TERM";
    thisInfo.trna_list = NStr::Tokenize(strtmp, ",", thisInfo.trna_list);

    // ini. of rrna_standard_name
    strtmp = "5S ribosomal RNA,5.8S ribosomal RNA,12S ribosomal RNA,16S ribosomal RNA,18S ribosomal RNA,23S ribosomal RNA,26S ribosomal RNA,28S ribosomal RNA,large subunit ribosomal RNA,small subunit ribosomal RNA";
    thisInfo.rrna_standard_name 
        = NStr::Tokenize(strtmp,",",thisInfo.rrna_standard_name);

    // ini. of short_auth_nms
    strtmp = "de la,del,de,da,du,dos,la,le,van,von,der,den,di";
    thisInfo.short_auth_nms 
         = NStr::Tokenize(strtmp, ",", thisInfo.short_auth_nms);

    // ini. of desired_aaList
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

    // ini. of usa_state_abbv;
    thisInfo.state_abbrev["AL"] = "Alabama";
    thisInfo.state_abbrev["AK"] = "Alaska";
    thisInfo.state_abbrev["AZ"] = "Arizona";
    thisInfo.state_abbrev["AR"] = "Arkansas";
    thisInfo.state_abbrev["CA"] = "California";
    thisInfo.state_abbrev["CO"] = "Colorado";
    thisInfo.state_abbrev["CT"] = "Connecticut";
    thisInfo.state_abbrev["DC"] = "Washington DC";
    thisInfo.state_abbrev["DE"] = "Delaware";
    thisInfo.state_abbrev["FL"] = "Florida";
    thisInfo.state_abbrev["GA"] = "Georgia";
    thisInfo.state_abbrev["HI"] = "Hawaii";
    thisInfo.state_abbrev["ID"] = "Idaho";
    thisInfo.state_abbrev["IL"] = "Illinois";
    thisInfo.state_abbrev["IN"] = "Indiana";
    thisInfo.state_abbrev["IA"] = "Iowa";
    thisInfo.state_abbrev["KS"] = "Kansas";
    thisInfo.state_abbrev["KY"] = "Kentucky";
    thisInfo.state_abbrev["LA"] = "Louisiana";
    thisInfo.state_abbrev["ME"] = "Maine";
    thisInfo.state_abbrev["MD"] = "Maryland";
    thisInfo.state_abbrev["MA"] = "Massachusetts";
    thisInfo.state_abbrev["MI"] = "Michigan";
    thisInfo.state_abbrev["MN"] = "Minnesota";
    thisInfo.state_abbrev["MS"] = "Mississippi";
    thisInfo.state_abbrev["MO"] = "Missouri";
    thisInfo.state_abbrev["MT"] = "Montana";
    thisInfo.state_abbrev["NE"] = "Nebraska";
    thisInfo.state_abbrev["NV"] = "Nevada";
    thisInfo.state_abbrev["NH"] = "New Hampshire";
    thisInfo.state_abbrev["NJ"] = "New Jersey";
    thisInfo.state_abbrev["NM"] = "New Mexico";
    thisInfo.state_abbrev["NY"] = "New York";
    thisInfo.state_abbrev["NC"] = "North Carolina";
    thisInfo.state_abbrev["ND"] = "North Dakota";
    thisInfo.state_abbrev["OH"] = "Ohio";
    thisInfo.state_abbrev["OK"] = "Oklahoma";
    thisInfo.state_abbrev["OR"] = "Oregon";
    thisInfo.state_abbrev["PA"] = "Pennsylvania";
    thisInfo.state_abbrev["PR"] = "Puerto Rico";
    thisInfo.state_abbrev["RI"] = "Rhode Island";
    thisInfo.state_abbrev["SC"] = "South Carolina";
    thisInfo.state_abbrev["SD"] = "South Dakota";
    thisInfo.state_abbrev["TN"] = "Tennessee";
    thisInfo.state_abbrev["TX"] = "Texas";
    thisInfo.state_abbrev["UT"] = "Utah";
    thisInfo.state_abbrev["VT"] = "Vermont";
    thisInfo.state_abbrev["VA"] = "Virginia";
    thisInfo.state_abbrev["WA"] = "Washington";
    thisInfo.state_abbrev["WV"] = "West Virginia";
    thisInfo.state_abbrev["WI"] = "Wisconsin";
    thisInfo.state_abbrev["WY"] = "Wyoming";

    // ini. of cds_prod_find
    thisInfo.cds_prod_find["-like"] = "EndsWithPattern";
    thisInfo.cds_prod_find["pseudo"] = "ContainsPseudo";
    thisInfo.cds_prod_find["fragment"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["similar"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["frameshift"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["partial"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["homolog"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["homologue"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["paralog"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["paralogue"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["ortholog"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["orthologue"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["gene"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["genes"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["related to"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["terminus"]  = "ContainsWholeWord";
    thisInfo.cds_prod_find["N-terminus"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["C-terminus"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["characterised"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["recognised"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["characterisation"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["localisation"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["tumour"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["uncharacterised"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["oxydase"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["colour"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["localise"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["faecal"] = "ContainsWholeWord";
    thisInfo.cds_prod_find["frame"] = "Empty";
    thisInfo.cds_prod_find["related"] = "EndsWithPattern";

    // ini. of virus_lineage
    strtmp = "Picornaviridae,Potyviridae,Flaviviridae,Togaviridae";
    thisInfo.virus_lineage 
        = NStr::Tokenize(strtmp, ",", thisInfo.virus_lineage); 

    // ini. of strain_tax
    strtmp = "type strain of,holotype strain of,paratype strain of,isotype strain of";
    thisInfo.strain_tax = NStr::Tokenize(strtmp, ",", thisInfo.strain_tax);

    // ini. of spell_data for flat file text check
    const char* fix_data[] = {
         "Agricultutral,agricultural,0",
         "Bacilllus,Bacillus,0",
         "Enviromental,Environmental,0",
         "Insitiute,institute,0",
         "Instutite,institute,0",
         "Instutute,Institute,0",
         "Instutute,Institute,0",
         "P.R.Chian,P.R. China,0",
         "PRChian,PR China,0",
         "Scieces,Sciences,0",
         "agricultral,agricultural,0",
         "agriculturral,agricultural,0",
         "biotechnlogy,biotechnology,0",
         "biotechnolgy,biotechnology,0",
         "biotechology,biotechnology,0",
         "caputre,capture,1",
         "casette,cassette,1",
         "catalize,catalyze,0",
         "charaterization,characterization,1",
         "charaterization,characterization,0",
         "clonging,cloning,0",
         "consevered,conserved,0",
         "cotaining,containing,0",
         "cytochome,cytochrome,1",
         "diveristy,diversity,1",
         "enivronment,environment,0",
         "enviroment,environment,0",
         "genone,genome,1",
         "homologue,homolog,1",
         "hypotethical,hypothetical,0",
         "hypotetical,hypothetical,0",
         "hypothetcial,hypothetical,0",
         "hypothteical,hypothetical,0",
         "indepedent,independent,0",
         "insititute,institute,0",
         "insitute,institute,0",
         "institue,institute,0",
         "instute,institute,0",
         "muesum,museum,1",
         "musuem,museum,1",
         "nuclear-shutting,nuclear shuttling,1",
         "phylogentic,phylogenetic,0",
         "protien,protein,0",
         "puatative,putative,0",
         "putaitve,putative,0",
         "putaive,putative,0",
         "putataive,putative,0",
         "putatitve,putative,0",
         "putatuve,putative,0",
         "putatvie,putative,0",
         "pylogeny,phylogeny,0",
         "resaerch,research,0",
         "reseach,research,0",
         "reserach,research,1",
         "reserch,research,0",
         "ribosoml,ribosomal,0",
         "ribossomal,ribosomal,0",
         "scencies,sciences,0",
         "scinece,science,0",
         "simmilar,similar,0",
         "structual,structural,0",
         "subitilus,subtilis,0",
         "sulfer,sulfur,0",
         "technlogy,technology,0",
         "technolgy,technology,0",
         "transcirbed,transcribed,0",
         "transcirption,transcription,1",
         "uiniversity,university,0",
         "uinversity,university,0",
         "univercity,university,0",
         "univerisity,university,0",
         "univeristy,university,0",
         "univesity,university,0",
         "unversity,university,1",
         "uviversity,university,0",
         "anaemia,,0",
         "haem,,0",
         "haemagglutination,,0",
         "heam,,0",
         "mithocon,,0"
    };
    for (i=0; i < (int)ArraySize(fix_data); i++) {
        arr.clear();
        arr = NStr::Tokenize(fix_data[i], ",", arr);
        thisInfo.fix_data[arr[0]].s_val = arr[1].empty()? "no" : "yes";
        thisInfo.fix_data[arr[0]].b_val = (arr[0] == "1");
    }
    arr.clear();

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

    // ini. summ_susrule
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
            i < (int)thisInfo.num_suspect_prod_terms;
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
    strtmp = "(NAD(P)H),(NAD(P)),(I),(II),(III),(NADPH),(NAD+),(NAPPH/NADH),(NADP+),[acyl-carrier protein],[acyl-carrier-protein],(acyl carrier protein)";
    thisInfo.skip_bracket_paren 
        = NStr::Tokenize(strtmp, ",", thisInfo.skip_bracket_paren);

    // ini of ok_num_prefix
    strtmp = "DUF,UPF,IS,TIGR,UCP,PUF,CHP";
    thisInfo.ok_num_prefix
        = NStr::Tokenize(strtmp, ",", thisInfo.ok_num_prefix);

    // ini of featkey_modified;
    thisInfo.featkey_modified["precursor_RNA"] = "preRNA";
    thisInfo.featkey_modified["C_region"] = "c_region";
    thisInfo.featkey_modified["J_segment"] = "j_segment";
    thisInfo.featkey_modified["V_segment"] = "v_segment";
    thisInfo.featkey_modified["D-loop"] = "d_loop";

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
    strtmp = "preRNA,mRNA,tRNA,rRNA,ncRNA,tmRNA,misc_RNA";
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
    strtmp = "Trace Assembly Archive,BioSample,ProbeDB,Sequence Read Archive,BioProject,Assembly";
    thisInfo.dblink_names = NStr::Tokenize(strtmp, ",", thisInfo.dblink_names);
 
    // ini of srcqual_orgmod, srcqual_subsrc, srcqual_names
    map <string, ESource_qual> srcnm_qual;
    map <string, COrgMod::ESubtype> orgmodnm_subtp;
    for (i = eSource_qual_acronym; i <= eSource_qual_type_material; i++) {
       strtmp = ENUM_METHOD_NAME(ESource_qual)()->FindName(i, true);
       if (!strtmp.empty()) {
         srcnm_qual[strtmp] = (ESource_qual)i;

         if (strtmp == "bio-material-INST") {
             strtmp = "bio-material-inst";
         }
         else if (strtmp == "bio-material-COLL") {
             strtmp = "bio-material-coll";
         }
         else if (strtmp == "bio-material-SpecID") {
             strtmp = "bio-material-specid";
         }
         else if (strtmp == "common-name") {
               strtmp = "common name";
         }
         else if (strtmp == "culture-collection-INST") {
             strtmp = "culture-collection-inst";
         }
         else if (strtmp == "culture-collection-COLL") {
             strtmp = "culture-collection-coll";
         }
         else if (strtmp == "culture-collection-SpecID") {
             strtmp = "culture-collection-specid";
         }
         else if (strtmp == "orgmod-note") {
               strtmp = "note-orgmod";
         }
         else if (strtmp == "nat-host") {
              strtmp = "host";
         }
         else if (strtmp == "subsource-note") {
             strtmp = "sub-species";
         }
         else if (strtmp == "all-notes") {
             strtmp = "All Notes";
         }
         else if (strtmp == "all-quals") {
             strtmp = "All";
         }
         else if (strtmp == "all-primers") {
             strtmp = "All Primers";
         }
         thisInfo.srcqual_names[(ESource_qual)i] = strtmp;
       }
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
   strtmp = "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec";
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
   strtmp = "possible,potential,predicted,probable";
   thisInfo.s_putative_replacements 
            = NStr::Tokenize(strtmp, ",", thisInfo.s_putative_replacements);

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
   strtmp = "is present,is not present,is all caps,is all lowercase,is all punctuation";
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
   strtmp = "Genome Project ID,Comment Descriptor,Definition Line,Keyword";
   thisInfo.miscfield_names 
       = NStr::Tokenize(strtmp, ",", thisInfo.miscfield_names);

   // ini. of loctype_names;
   strtmp 
       = " ,with single interval,with joined intervals,with ordered intervals";
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
   strtmp = "domain,partial,5s_rRNA,16s_rRNA,23s_rRNA";
   arr.clear();
   arr = NStr::Tokenize(strtmp, ",", arr);
   ITERATE (vector <string>, it, arr) {
     CRef <CSearch_func> sch_func = MakeSimpleSearchFunc(*it);
     CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
     this_rule->SetFind(*sch_func);
     thisInfo.suspect_rna_rules->Set().push_back(this_rule);
     thisInfo.rna_rule_summ 
                .push_back(summ_susrule.SummarizeSuspectRuleEx(*this_rule));
   }
   CRef <CSearch_func> sch_func = MakeSimpleSearchFunc("8S", true);
   CRef <CSearch_func> except = MakeSimpleSearchFunc("5.8S", true);
   CRef <CSuspect_rule> this_rule ( new CSuspect_rule);
   this_rule->SetFind(*sch_func);
   this_rule->SetExcept(*except);
   thisInfo.suspect_rna_rules->Set().push_back(this_rule); 
   thisInfo.rna_rule_summ
               .push_back(summ_susrule.SummarizeSuspectRuleEx(*this_rule));

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


// only consider enables/disables
void CRepConfig :: SetArg(const string& arg, const string& vlu)
{
   if (arg == "e") {
       m_enabled.clear();
       m_enabled = NStr::Tokenize(vlu, ", ", m_enabled, NStr::eMergeDelims);
   }
   else if (arg == "d") {
        m_disabled.clear();
        m_disabled= NStr::Tokenize(vlu,", ", m_disabled, NStr::eMergeDelims);
   }
};

void CRepConfig :: ProcessArgs(Str2Str& args)
{
    string strtmp;
    // input file/path
    thisInfo.infile = (args.find("i") != args.end()) ? args["i"] : kEmptyStr;
    m_indir = (args.find("p") != args.end()) ? args["p"] : kEmptyStr;
    m_insuffix = args["x"];
    m_dorecurse = (args["u"] == "1") ? true : false;
    if ((m_indir.empty() || !CDir(m_indir).Exists()) 
         && thisInfo.infile.empty()) {
        if (m_indir.empty() || !CDir(m_indir).Exists()) {
          NCBI_USER_THROW("input path does not exist: " + m_indir);
        }
        NCBI_USER_THROW("Input path or input file must be specified");
    }
    m_file_tp = args["a"];

    bool big_sequence_report = false;
    // report category
    if (args.find("P") != args.end()) {
      strtmp == args["P"];
      thisInfo.output_config.add_output_tag 
             = ( (strtmp == "t") || (strtmp == "bt"));
      thisInfo.output_config.add_extra_output_tag = (strtmp == "s");
      if (args["P"] == "t" || args["P"] == "s") {
          thisInfo.report = fAsndisc;
      }
      else if (strtmp == "bt") {
          thisInfo.report = fBigSequence;
      }
    }

    // output
    m_outsuffix = args["s"];
    string output_f = (args.find("o") != args.end()) ? args["o"] : kEmptyStr;
    m_outdir= (args.find("r") != args.end()) ? GetDirStr(args["r"]) : kEmptyStr;
    if (!output_f.empty() && !m_outdir.empty()) {
        NCBI_USER_THROW("-o and -r are incompatible: specify the output file name with the full path.");
    }

    if (output_f.empty() && !thisInfo.infile.empty()) {
       CFile file(thisInfo.infile);
       output_f = file.GetDir() + file.GetBase() + m_outsuffix;
    }
    if (m_outdir.empty() && m_indir.empty()) m_outdir = "./";
    if (!m_outdir.empty() 
               && !CDir(m_outdir).Exists() && !CDir(m_outdir).Create()) {
         NCBI_USER_THROW("Unable to create output directory " + m_outdir);
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

    strtmp = (args.find("X") != args.end())? args["X"] : kEmptyStr;
    if (strtmp == "ALL") {
       m_all_expanded = true;     
    }
    else if (!strtmp.empty()) {
       vector <string> arr;
       arr = NStr::Tokenize(strtmp,", ", m_disabled, NStr::eMergeDelims);
       ITERATE (vector <string>, it, arr) {
          m_expanded.insert(*it); 
       }
    }

    if (!big_sequence_report) {
        big_sequence_report = (args["B"] == "true");
    }
    if (m_enabled.empty()) {
      if ( big_sequence_report) {
          thisInfo.report = fBigSequence; 
      }
      else if (thisInfo.report == fAsndisc) {
         thisInfo.report = fGenomes;
      }
    }

    m_genbank_loader = (args["R"] == "true");
};


void CRepConfig :: ReadArgs(const CArgs& args) 
{
    Str2Str arg_map;
    // input file
    if (args["i"]) {
      arg_map["i"] = args["i"].AsString();
    }
    arg_map["a"] = args["a"].AsString();

    // report_tp
    if (args["P"]) {
      arg_map["P"] = args["P"].AsString();
    }

    // output
    if (args["o"]) {
      arg_map["o"] = args["o"].AsString();
    }
    if (args["r"]) {
      arg_map["r"] = args["r"].AsString();
    }
    arg_map["S"] = NStr::BoolToString(args["S"].AsBoolean());
    arg_map["s"] = args["s"].AsString();

    // enabled and disabled tests
    if (args["e"]) {
      arg_map["e"] = args["e"].AsString();
    }
    if (args["d"]) {
      arg_map["d"] = args["d"].AsString();
    }

    // input directory
    if (args["p"]) {
      arg_map["p"] = args["p"].AsString();
    }
    arg_map["x"] = args["x"].AsString();
    arg_map["u"] = args["u"].AsString();

    // expand
    if (args["X"]) {
       arg_map["X"] = args["X"].AsString();
    }

    // Big sequence report
    arg_map["B"] = NStr::BoolToString(args["B"].AsBoolean());

    // GenBank data loader
    arg_map["R"] = NStr::BoolToString(args["R"].AsBoolean());

    ProcessArgs(arg_map);
};

CRef <CSearch_func> CRepConfig :: MakeSimpleSearchFunc(const string& match_text, bool whole_word)
{
     CRef <CString_constraint> str_cons (new CString_constraint);
     str_cons->SetMatch_text(match_text);
     str_cons->SetMatch_location(eString_location_contains);
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
    string strtmp;
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
    if (m_genbank_loader) {
       CGBDataLoader::RegisterInObjectManager(*object_manager);
       thisInfo.scope->AddDefaults();
    }

    CCheckingClass myChecker;
    myChecker.SetNumEntry(GetNumEntry());
    myChecker.CheckSeqEntry(seq_entry);

    // go through tests
    CAutoDiscClass autoDiscClass( *(thisInfo.scope), myChecker );
    autoDiscClass.LookAtSeqEntry( *seq_entry );
};  // CheckThisSeqEntry()

static const s_test_property test_list[] = {
// tests_on_SubmitBlk
   {"DISC_SUBMITBLOCK_CONFLICT", 
     fOncaller | fMegaReport, "Records should have identical submit-blocks"},

// tests_on_Bioseq
   {"DISC_MISSING_DEFLINES", 
     fOncaller | fMegaReport, "Missing definition lines"},
   {"DISC_QUALITY_SCORES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Check for Quality Scores"},
   {"TEST_TERMINAL_NS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Ns at end of sequences"},
   
// tests_on_Bioseq_aa
   {"COUNT_PROTEINS", 
     fTRNA | fMegaReport | fDiscrepancy, "Count Proteins"},
   {"MISSING_PROTEIN_ID", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Missing Protein ID" },
   {"INCONSISTENT_PROTEIN_ID", fGenomes | fAsndisc | fDiscrepancy | fMegaReport, "Inconsistent Protein ID"},

// tests_on_Bioseq_na
   {"DISC_COUNT_NUCLEOTIDES", fGenomes | fBigSequence | fAsndisc | fOncaller| fMegaReport, "Count nucleotide sequences"},
   {"TEST_DEFLINE_PRESENT", fGenomes | fDiscrepancy | fAsndisc| fMegaReport, "Test defline existence"},
   {"N_RUNS", fGenomes | fBigSequence | fDiscrepancy | fAsndisc| fMegaReport, "Runs of 10 or more Ns"},
   {"N_RUNS_14", 
     fDiscrepancy | fTSA | fMegaReport, "Runs of more than 14 Ns"},
   {"ZERO_BASECOUNT", fGenomes | fBigSequence | fDiscrepancy | fMegaReport, "Zero Base Counts"},
   {"TEST_LOW_QUALITY_REGION", fGenomes | fDiscrepancy | fAsndisc| fMegaReport, "Sequence contains regions of low quality"},
   {"DISC_PERCENT_N", fGenomes | fBigSequence | fDiscrepancy | fAsndisc| fMegaReport, "Greater than 5 percent Ns"},
   {"DISC_10_PERCENTN", 
     fDiscrepancy | fTSA | fMegaReport, "Greater than 10 percent Ns"},
   {"TEST_UNUSUAL_NT", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Sequence contains unusual nucleotides"},

// tests_on_Bioseq_CFeat
   {"SUSPECT_PHRASES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Suspect Phrases"},
   {"DISC_SUSPECT_RRNA_PRODUCTS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "rRNA product names should not contain 'partial' or 'domain'"},
   {"SUSPECT_PRODUCT_NAMES", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Suspect Product Name"},
   {"DISC_PRODUCT_NAME_TYPO", fGenomes | fDiscrepancy | fMegaReport, "Suspect Product Name Typo"},
   {"DISC_PRODUCT_NAME_QUICKFIX", fGenomes | fDiscrepancy | fMegaReport, "Suspect Product Name QuickFix"},
   {"TEST_ORGANELLE_PRODUCTS", 
     fOncaller | fMegaReport, 
     "Organelle products on non-organelle sequence: on when neither bacteria nor virus"},
   {"DISC_GAPS", fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fMegaReport, "Sequences with gaps"},
   {"TEST_MRNA_OVERLAPPING_PSEUDO_GENE", fGenomes | fAsndisc | fOncaller | fMegaReport, "Remove mRNA overlapping a pseudogene"},
   {"ONCALLER_HAS_STANDARD_NAME", fGenomes | fAsndisc | fOncaller | fMegaReport, "Feature has standard_name qualifier"},
   {"ONCALLER_ORDERED_LOCATION", fGenomes | fAsndisc | fOncaller | fMegaReport, "Location is ordered (intervals interspersed with gaps)"},
   {"DISC_FEATURE_LIST", fDiscrepancy | fMegaReport, "Feature List"},
   {"TEST_CDS_HAS_CDD_XREF", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "CDS has CDD Xref"},
   {"DISC_CDS_HAS_NEW_EXCEPTION", fGenomes | fAsndisc | fOncaller | fMegaReport, "Coding region has new exception"},
   {"DISC_MICROSATELLITE_REPEAT_TYPE", fGenomes | fAsndisc | fOncaller | fMegaReport, "Microsatellites must have repeat type of tandem"},
   {"DISC_SUSPECT_MISC_FEATURES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Suspect misc_feature comments"},
   {"DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS", fGenomes | fAsndisc | fOncaller | fMegaReport, "Check for gene or genes in rRNA and tRNA products and comments"},
   {"DISC_FEATURE_MOLTYPE_MISMATCH", 
     fOncaller | fMegaReport, 
     "Sequences with rRNA or misc_RNA features should be genomic DNA"},
   {"ADJACENT_PSEUDOGENES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Adjacent PseudoGenes with Identical Text"},
   {"MISSING_GENPRODSET_PROTEIN", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "CDS on GenProdSet without protein"},
   {"DUP_GENPRODSET_PROTEIN", fGenomes | fDiscrepancy | fMegaReport, "Multiple CDS on GenProdSet, same protein"},
   {"MISSING_GENPRODSET_TRANSCRIPT_ID", fGenomes | fDiscrepancy | fMegaReport, "mRNA on GenProdSet without transcript ID"},
   {"DISC_DUP_GENPRODSET_TRANSCRIPT_ID", fGenomes | fMegaReport | fDiscrepancy | fAsndisc, "mRNA on GenProdSet with duplicate ID"},
   {"DISC_FEAT_OVERLAP_SRCFEAT", 
     fDiscrepancy | fAsndisc | fMegaReport, 
     "Features Intersecting Source Features"},
   {"CDS_TRNA_OVERLAP", 
     fTRNA | fMegaReport | fDiscrepancy, "CDS tRNA overlaps"},
   {"TRANSL_NO_NOTE", 
     fTRNA | fDiscrepancy | fMegaReport, "Transl_except without Note"},
   {"NOTE_NO_TRANSL", 
     fTRNA | fDiscrepancy | fMegaReport, "Note without Transl_except"},
   {"TRANSL_TOO_LONG", 
     fTRNA | fDiscrepancy | fMegaReport, 
     "Transl_except longer than 3"},
   {"TEST_SHORT_LNCRNA", fGenomes | fDiscrepancy | fOncaller | fMegaReport, "Short lncRNA sequences"},
   {"FIND_STRAND_TRNAS", 
     fDiscrepancy | fMegaReport, "Find tRNAs on the same strand"},
   {"FIND_BADLEN_TRNAS", 
     fTRNA | fDiscrepancy | fOncaller | fMegaReport, 
     "Find short and long tRNAs"},
   {"COUNT_TRNAS", fTRNA | fMegaReport | fDiscrepancy, "Count tRNAs"},
   {"FIND_DUP_TRNAS", 
     fTRNA | fDiscrepancy | fMegaReport, "Find Duplicate tRNAs"},
   {"COUNT_RRNAS", fTRNA | fMegaReport | fDiscrepancy, "Count rRNAs"},
   {"FIND_DUP_RRNAS", 
     fTRNA | fDiscrepancy | fMegaReport, "Find Duplicate rRNAs"},
   {"PARTIAL_CDS_COMPLETE_SEQUENCE", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Partial CDSs in Complete Sequences"},
   {"CONTAINED_CDS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Contained CDS"},
   {"PSEUDO_MISMATCH", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Pseudo Mismatch"},
   {"EC_NUMBER_NOTE", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "EC Number Note"},
   {"NON_GENE_LOCUS_TAG", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Nongene Locus Tag"},
   {"JOINED_FEATURES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Joined Features: on when non-eukaryote"},
   {"SHOW_TRANSL_EXCEPT", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Show translation exception"},
   {"MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "mRNA should have both protein_id and transcript_id"},
   {"RRNA_NAME_CONFLICTS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "rRNA Standard name conflicts found"},
   {"ONCALLER_GENE_MISSING", fOncaller | fMegaReport, "Missing genes"},
   {"ONCALLER_SUPERFLUOUS_GENE", fOncaller | fMegaReport, "Superfluous genes"},
   {"MISSING_GENES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Missing Genes"},
   {"EXTRA_GENES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Extra Genes"},
   //tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES"));
   {"OVERLAPPING_CDS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Overlapping CDS"},
   {"RNA_CDS_OVERLAP", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "CDS RNA Overlap"},
   {"FIND_OVERLAPPED_GENES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Find completely overlapped genes"},
   {"OVERLAPPING_GENES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Overlapping Genes"},
   {"DISC_PROTEIN_NAMES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Frequently appearing proteins"},
   {"DISC_CDS_PRODUCT_FIND", 
     fOncaller | fMegaReport, "Coding region product contains suspect text"},
   {"EC_NUMBER_ON_UNKNOWN_PROTEIN", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Hypothetical or Unknown Protein with EC Number"},
   {"RNA_NO_PRODUCT", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Find RNAs without Products"},
   {"DISC_SHORT_INTRON", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Introns shorter than 10 nt"},
   {"DISC_BAD_GENE_STRAND", fGenomes | fAsndisc | fOncaller | fMegaReport | fMegaReport, "Genes and features that share endpoints should be on the same strand"},
   {"DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA", fGenomes | fAsndisc | fOncaller | fMegaReport, "rRNA product names should not contain 'internal', 'transcribed', or 'spacer'"},
   {"DISC_SHORT_RRNA", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Short rRNA Features"},
   {"TEST_OVERLAPPING_RRNAS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Overlapping rRNA features"},
   {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Show hypothetic protein having a gene name"},
   {"DISC_SUSPICIOUS_NOTE_TEXT", fGenomes | fAsndisc | fOncaller | fMegaReport, "Find Suspicious Phrases in Note Text"},
   {"NO_ANNOTATION", fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Bioseqs longer than 5000nt without Annotations"},
   {"DISC_LONG_NO_ANNOTATION", fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fMegaReport, "Bioseqs longer than 5000nt without Annotations"},
   {"DISC_PARTIAL_PROBLEMS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Find partial feature ends on sequences that could be extended"},
   {"TEST_UNUSUAL_MISC_RNA", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Unexpected misc_RNA features"},
   {"GENE_PRODUCT_CONFLICT", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Gene Product Conflict"},
   {"DISC_CDS_WITHOUT_MRNA", fGenomes | fAsndisc | fOncaller| fMegaReport, "Coding regions on eukaryotic genomic DNA should have mRNAs with matching products"},

// tests_on_Bioseq_CFeat_NotInGenProdSet
   {"DUPLICATE_GENE_LOCUS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Duplicate Gene Locus"},
   {"MISSING_LOCUS_TAGS", fGenomes | fDiscrepancy | fMegaReport, "Missing Locus Tags"},
   {"DUPLICATE_LOCUS_TAGS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Duplicate Locus Tags"},
   {"DUPLICATE_LOCUS_TAGS_global", fGenomes | fAsndisc, "Duplicate Locus Tags"},
   {"INCONSISTENT_LOCUS_TAG_PREFIX", fGenomes | fAsndisc | fDiscrepancy | fMegaReport, "Inconsistent Locus Tag Prefix"},
   {"BAD_LOCUS_TAG_FORMAT", fGenomes | fMegaReport | fDiscrepancy | fAsndisc, "Bad Locus Tag Format"},
   {"FEATURE_LOCATION_CONFLICT", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Feature Location Conflict"},

// tests_on_Bioseq_CFeat_CSeqdesc
     // oncaller tool version
   {"DISC_FEATURE_COUNT_oncaller", 
     fOncaller, 
     "Count features present or missing from sequences"}, 
   {"DISC_FEATURE_COUNT", fGenomes | fAsndisc | fMegaReport, "Count features present or missing from sequences"}, // asndisc version   
   {"DISC_BAD_BGPIPE_QUALS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Bad BGPIPE qualifiers"},
   {"DISC_INCONSISTENT_MOLINFO_TECH", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Inconsistent Molinfo Techniqueq"},
   {"SHORT_CONTIG", fGenomes | fDiscrepancy | fAsndisc | fMegaReport | fBigSequence, "Short Contig"},
   {"SHORT_SEQUENCES", 
     fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fMegaReport, 
     "Find Short Sequences"},
   {"SHORT_SEQUENCES_200", 
     fDiscrepancy | fTSA | fMegaReport, 
     "Find sequences Less Than 200 bp"},
   {"TEST_UNWANTED_SPACER", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Intergenic spacer without plastid location"},
   {"TEST_UNNECESSARY_VIRUS_GENE", fGenomes | fAsndisc | fOncaller | fMegaReport, "Unnecessary gene features on virus: on when lineage is not Picornaviridae,Potyviridae,Flaviviridae and Togaviridae"},
   {"TEST_ORGANELLE_NOT_GENOMIC", fGenomes | fDiscrepancy | fAsndisc | fOncaller | fMegaReport, "Organelle location should have genomic moltype"},
   {"MULTIPLE_CDS_ON_MRNA", fGenomes | fAsndisc | fOncaller | fMegaReport, "Multiple CDS on mRNA"},
   {"TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES", fGenomes | fAsndisc | fOncaller | fMegaReport, "mRNA sequences have CDS/gene on the complement strand"},
   {"TEST_BAD_MRNA_QUAL", fGenomes | fAsndisc | fOncaller | fMegaReport, "mRNA sequence contains rearranged or germline"},
   {"TEST_EXON_ON_MRNA", fGenomes | fAsndisc | fOncaller | fMegaReport, "mRNA sequences should not have exons"},
   {"ONCALLER_HIV_RNA_INCONSISTENT", fGenomes | fAsndisc | fOncaller | fMegaReport, "HIV RNA location or molecule type inconsistent"},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote"},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote"},
   {"DISC_MITOCHONDRION_REQUIRED", fGenomes | fAsndisc | fOncaller | fMegaReport, "If D-loop or control region misc_feat is present, source must be mitochondrial"},
   {"EUKARYOTE_SHOULD_HAVE_MRNA", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Eukaryote should have mRNA"},
   {"RNA_PROVIRAL", fGenomes | fAsndisc | fOncaller | fMegaReport, "RNA bioseqs are proviral"},
   {"NON_RETROVIRIDAE_PROVIRAL", fGenomes | fAsndisc | fOncaller | fMegaReport, "Non-Retroviridae biosources are proviral"},
   {"DISC_RETROVIRIDAE_DNA", 
     fOncaller | fMegaReport, 
     "When the organism lineage contains 'Retroviridae' and the molecule type is 'DNA', the location should be set as 'proviral'"},
   {"DISC_mRNA_ON_WRONG_SEQUENCE_TYPE", 
     fOncaller | fMegaReport, 
     "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features"},
   {"DISC_RBS_WITHOUT_GENE", fGenomes | fAsndisc | fOncaller | fMegaReport, "RBS features should have an overlapping gene"},
   {"DISC_EXON_INTRON_CONFLICT", fGenomes | fAsndisc | fOncaller | fMegaReport, "Exon and intron locations should abut (unless gene is trans-spliced)"},
   {"TEST_TAXNAME_NOT_IN_DEFLINE", 
     fOncaller | fMegaReport, 
     "Complete taxname should be present in definition line"},
   {"INCONSISTENT_SOURCE_DEFLINE", 
     fDiscrepancy | fMegaReport, 
     "Inconsistent Source And Definition Line"},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", fGenomes | fAsndisc | fOncaller | fMegaReport, "Bacterial sequences should not have mRNA features"},
   {"DISC_BAD_BACTERIAL_GENE_NAME", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Genes on bacterial sequences should start with lowercase letters: on when non-eukaryote"},
   {"TEST_BAD_GENE_NAME", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Bad gene names"},
   {"MOLTYPE_NOT_MRNA", 
     fDiscrepancy | fTSA | fMegaReport, "Moltype not mRNA"},
   {"TECHNIQUE_NOT_TSA", 
     fDiscrepancy | fTSA | fMegaReport, "Technique not set as TSA"},
   {"DISC_POSSIBLE_LINKER", 
     fOncaller | fMegaReport, "Possible linker sequence after poly-A tail"},
   {"SHORT_PROT_SEQUENCES", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Protein sequences should be at least 50 aa, unless they are partial"},
   {"TEST_COUNT_UNVERIFIED", fGenomes | fAsndisc | fOncaller | fMegaReport, "Count number of unverified sequences"},
   {"TEST_DUP_GENES_OPPOSITE_STRANDS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Duplicate genes on opposite strands"},
   {"DISC_GENE_PARTIAL_CONFLICT", fGenomes | fOncaller | fAsndisc | fMegaReport, "Feature partialness should agree with gene partialness if endpoints match"},

// tests_on_SeqEntry
   {"DISC_FLATFILE_FIND_ONCALLER", 
     fAsndisc | fOncaller | fMegaReport, 
     "Flatfile representation of object contains suspect text"},
   {"DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE", fGenomes | fMegaReport | fOncaller, "Flatfile representation of object contains unfixable suspect text"},
   {"DISC_FLATFILE_FIND_ONCALLER_FIXABLE", fGenomes | fMegaReport | fOncaller, "Flatfile representation of object contains suspect text"},
   {"TEST_ALIGNMENT_HAS_SCORE", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Alignment has score attribute"},

// tests_on_SeqEntry_feat_desc: all CSeqEntry_Feat_desc tests need RmvRedundancy
   {"DISC_INCONSISTENT_STRUCTURED_COMMENTS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Inconsistent structured comments"},
   {"DISC_INCONSISTENT_DBLINK", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Inconsistent DBLink fields"},
   {"END_COLON_IN_COUNTRY", fGenomes | fAsndisc | fOncaller | fMegaReport, "Country name end with colon"},
   {"ONCALLER_SUSPECTED_ORG_COLLECTED", fGenomes | fAsndisc | fOncaller | fMegaReport, "Suspected organism in collected-by SubSource"},
   {"ONCALLER_SUSPECTED_ORG_IDENTIFIED", fGenomes | fAsndisc | fOncaller | fMegaReport, "Suspected organism in identified-by SubSource"},
   {"UNCULTURED_NOTES_ONCALLER", fGenomes | fAsndisc | fOncaller | fMegaReport, "Uncultured Notes"}, 
   {"ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY", fGenomes | fAsndisc | fOncaller | fMegaReport, "SubSource identified-by contains more than 3 names"},
   {"ONCALLER_MORE_NAMES_COLLECTED_BY", fGenomes | fAsndisc | fOncaller | fMegaReport, "SubSource collected-by contains more than 3 names"},
   {"ONCALLER_STRAIN_TAXNAME_CONFLICT", fGenomes | fAsndisc | fOncaller | fMegaReport, "Type strain comment in OrgMod does not agree with organism name"},
   {"DISC_INCONSISTENT_MOLTYPES", 
     fOncaller | fMegaReport, 
     "All non-protein sequences in a set should have the same moltype"},
   {"DISC_BIOMATERIAL_TAXNAME_MISMATCH", fGenomes | fAsndisc | fOncaller | fMegaReport, "Test BioSources with the same biomaterial but different taxname"},
   {"DISC_CULTURE_TAXNAME_MISMATCH", fGenomes | fAsndisc | fOncaller | fMegaReport, "Test BioSources with the same culture collection but different taxname"},
   {"DISC_STRAIN_TAXNAME_MISMATCH", 
     fOncaller | fMegaReport, 
     "BioSources with the same strain should have the same taxname"},
   {"DISC_SPECVOUCHER_TAXNAME_MISMATCH", 
     fOncaller | fMegaReport, 
     "BioSources with the same specimen voucher should have the same taxname"},
   {"DISC_HAPLOTYPE_MISMATCH", 
     fOncaller | fMegaReport, "Sequences with the same haplotype should match"},
   {"DISC_MISSING_VIRAL_QUALS", 
     fOncaller | fMegaReport, 
     "Viruses should specify collection-date, country, and specific-host"},
   {"DISC_INFLUENZA_DATE_MISMATCH", 
     fDiscrepancy | fOncaller | fMegaReport, 
     "Influenza Strain/Collection Date Mismatch"},
   {"TAX_LOOKUP_MISSING", 
     fDiscrepancy | fMegaReport, "Find Missing Tax Lookups"},
   {"TAX_LOOKUP_MISMATCH", 
     fDiscrepancy | fMegaReport, "Find Tax Lookup Mismatches"},
   {"MISSING_STRUCTURED_COMMENT", 
     fDiscrepancy | fTSA | fMegaReport, "Structured comment not included"},
   {"ONCALLER_MISSING_STRUCTURED_COMMENTS", fGenomes | fOncaller | fMegaReport, "Missing structured comments"},
   {"DISC_MISSING_AFFIL", fGenomes | fAsndisc | fOncaller | fMegaReport, "Missing affiliation"},
   {"DISC_CITSUBAFFIL_CONFLICT", 
     fOncaller | fMegaReport, 
     "All Cit-subs should have identical affiliations"},
   {"DISC_TITLE_AUTHOR_CONFLICT", 
     fOncaller | fMegaReport, 
     "Publications with the same titles should have the same authors"},
   {"DISC_USA_STATE", fGenomes | fAsndisc | fOncaller | fMegaReport, "For country USA, state should be present and abbreviated"},
   {"DISC_CITSUB_AFFIL_DUP_TEXT", fGenomes | fAsndisc | fOncaller | fMegaReport, "Cit-sub affiliation street contains text from other affiliation fields"},
   {"DISC_REQUIRED_CLONE", 
     fOncaller | fMegaReport, 
     "Uncultured or environmental sources should have clone"},
   {"ONCALLER_MULTISRC", fGenomes | fAsndisc | fOncaller | fMegaReport, "Comma or semicolon appears in strain or isolate"},
   {"DISC_DUP_SRC_QUAL", fGenomes | fOncaller | fMegaReport, "Each source in a record should have unique values for qualifiers"}, 
   {"DISC_DUP_SRC_QUAL_DATA", 
     fOncaller | fMegaReport, 
     "Each qualifier on a source should have different values"},
   {"DISC_MISSING_SRC_QUAL", 
     fOncaller | fMegaReport, 
     "All sources in a record should have the same qualifier set"},
   {"DISC_SRC_QUAL_PROBLEM", 
     fOncaller | fMegaReport, 
     "Source Qualifier Report"},  // oncaller =DISC_SOURCE_QUALS_ASNDISC
   {"DISC_SOURCE_QUALS_ASNDISC", fGenomes | fAsndisc | fMegaReport, "Source Qualifier test for Asndisc"}, // asndisc version of *_PROBLEM
//   {"DISC_SOURCE_QUALS_ASNDISC_oncaller", fGenomes | fAsndisc | fOncaller | fMegaReport}, // needed?
   {"DISC_UNPUB_PUB_WITHOUT_TITLE", fGenomes | fAsndisc | fOncaller | fMegaReport, "Unpublished pubs should have titles"},
   {"ONCALLER_CONSORTIUM", 
     fOncaller | fMegaReport, 
     "Submitter blocks and publications have consortiums"},
   {"DISC_CHECK_AUTH_NAME", fGenomes | fAsndisc | fOncaller | fMegaReport, "Test author names missing first and/or last names"},
   {"DISC_CHECK_AUTH_CAPS", fGenomes | fAsndisc | fOncaller | fMegaReport, "Check for correct capitalization in author names"},
   {"DISC_MISMATCHED_COMMENTS", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Mismatched Comments"},
   {"ONCALLER_COMMENT_PRESENT", fGenomes | fAsndisc | fOncaller | fMegaReport, "Comment descriptor present"},
   {"DUP_DISC_ATCC_CULTURE_CONFLICT", fGenomes | fAsndisc | fOncaller | fMegaReport, "ATCC strain should also appear in culture collection"},
   {"ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH", fGenomes | fAsndisc | fOncaller | fMegaReport, "Strain and culture-collection values conflict"},
   {"DISC_DUP_DEFLINE", 
     fOncaller | fMegaReport, "Definition lines should be unique"},
   {"DISC_TITLE_ENDS_WITH_SEQUENCE", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Sequence characters at end of defline"},
   {"ONCALLER_DEFLINE_ON_SET", fGenomes | fAsndisc | fOncaller | fMegaReport, "Titles on sets"},
   {"DISC_BACTERIAL_TAX_STRAIN_MISMATCH", fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Bacterial taxnames should end with strain"},
   {"DUP_DISC_CBS_CULTURE_CONFLICT", fGenomes | fAsndisc | fOncaller | fMegaReport, "CBS strain should also appear in culture collection"},
   {"INCONSISTENT_BIOSOURCE", fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fMegaReport, "Inconsistent BioSource"},
   {"ONCALLER_BIOPROJECT_ID", fGenomes | fAsndisc | fOncaller | fMegaReport, "Sequences with BioProject IDs"},
   {"ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX", fGenomes | fAsndisc | fOncaller | fMegaReport, "Suspicious structured comment prefix"},
   {"MISSING_GENOMEASSEMBLY_COMMENTS", fGenomes | fBigSequence | fDiscrepancy | fAsndisc | fMegaReport, "Bioseqs should have GenomeAssembly structured comments"},
   {"TEST_HAS_PROJECT_ID", fGenomes | fAsndisc | fOncaller | fMegaReport, "Sequences with project IDs"},
   {"DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER", fGenomes | fAsndisc | fOncaller | fMegaReport, "Trinomial sources should have corresponding qualifier"},
   {"ONCALLER_DUPLICATE_PRIMER_SET", fGenomes | fAsndisc | fOncaller | fMegaReport, "Duplicate PCR primer pair"},
   {"ONCALLER_COUNTRY_COLON", fGenomes | fAsndisc | fOncaller | fMegaReport, "Country discription should only have 1 colon"},
   {"TEST_MISSING_PRIMER", fGenomes | fAsndisc | fOncaller | fMegaReport, "Missing values in primer set"},
   {"TEST_SP_NOT_UNCULTURED", 
     fOncaller | fMegaReport, "Organism ending in sp. needs tax consult"},
   {"DISC_METAGENOMIC", fGenomes | fAsndisc | fOncaller | fMegaReport, "Source has metagenomic qualifier"},
   {"DISC_MAP_CHROMOSOME_CONFLICT", 
     fOncaller | fMegaReport, 
     "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier"},
   {"DIVISION_CODE_CONFLICTS", fGenomes | fAsndisc | fOncaller | fMegaReport, "Division code conflicts found"},
   {"TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Species-specific primers, no environmental sample"},
   {"TEST_UNNECESSARY_ENVIRONMENTAL", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Unnecessary environmental qualifier present"},
   {"DISC_HUMAN_HOST", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "'Human' in host should be 'Homo sapiens'"},
   {"ONCALLER_MULTIPLE_CULTURE_COLLECTION", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Multiple culture-collection quals"},
   {"ONCALLER_CHECK_AUTHORITY", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Authority and Taxname should match first two words"},
   {"DISC_METAGENOME_SOURCE", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Source has metagenome_source qualifier"},
   {"DISC_BACTERIA_MISSING_STRAIN", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Missing strain on bacterial 'Genus sp. strain'"},
   {"DISC_REQUIRED_STRAIN", 
     fGenomes | fDiscrepancy | fAsndisc | fMegaReport, 
     "Bacteria should have strain"},
   {"MISSING_PROJECT",
     fDiscrepancy | fTSA | fMegaReport, "Project not included"},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Bacterial sources should not have isolate"},

// tests_on_BioseqSet   // redundant because of nested set?
   {"TEST_SMALL_GENOME_SET_PROBLEM", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Problems with small genome sets"},
   {"DISC_SEGSETS_PRESENT", 
     fGenomes | fDiscrepancy | fAsndisc | fMegaReport, "Segsets present"},
   {"TEST_UNWANTED_SET_WRAPPER", 
     fGenomes | fAsndisc | fOncaller | fMegaReport, 
     "Set wrapper on microsatellites or rearranged genes"},
   {"DISC_NONWGS_SETS_PRESENT", 
     fGenomes | fDiscrepancy | fAsndisc | fMegaReport, 
     "Eco, mut, phy or pop sets present"}
};

CRepConfig* CRepConfig :: factory(ETestCategoryFlags report_tp, CSeq_entry_Handle* tse_p)
{
   thisInfo.report = report_tp;   // arg_map["P"]
   if (report_tp & (fAsndisc | fBigSequence | fGenomes) ) {
         thisInfo.output_config.use_flag = true; // output flags
         return new CRepConfAsndisc();
   }
   else if (report_tp & (fDiscrepancy | fOncaller | fMegaReport) ) {
       if (tse_p) {
           m_TopSeqEntry = tse_p;
       }
       else {
            NCBI_USER_THROW("Missing top seq-entry-handle");
       }
       if (report_tp ==  fDiscrepancy) {
          return new  CRepConfDiscrepancy();
       }
       else  if (report_tp == fOncaller) {
          return new  CRepConfOncaller();
       }
       else {
          return new CRepConfMega();
       }
   }
   else {
      NCBI_USER_THROW("Unrecognized report type.");
      return 0;
   }
};  // CRepConfig::factory()


void CRepConfig :: GetTestList() 
{
   thisGrp.tests_on_Bioseq.clear();   
   thisGrp.tests_on_Bioseq_na.clear();   
   thisGrp.tests_on_Bioseq_aa.clear();   
   thisGrp.tests_on_Bioseq_CFeat.clear();   
   thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.clear();   
   thisGrp.tests_on_Bioseq_NotInGenProdSet.clear();   
   thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.clear();   
   thisGrp.tests_on_SeqEntry.clear();   
   thisGrp.tests_on_SeqEntry_feat_desc.clear();   
   thisGrp.tests_4_once.clear();   
   thisGrp.tests_on_BioseqSet.clear();   
   thisGrp.tests_on_SubmitBlk.clear();   

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
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_TRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("COUNT_RRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_RRNAS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
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
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>
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
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
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
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back( 
           CRef <CTestAndRepData>( new CBioseq_GENE_PRODUCT_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_WITHOUT_MRNA") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back( 
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
   if ( thisTest.tests_run.find("DUPLICATE_LOCUS_TAGS") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet
           .push_back(CRef <CTestAndRepData>(new CBioseq_DUPLICATE_LOCUS_TAGS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUPLICATE_LOCUS_TAGS_global") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet
            .push_back(CRef <CTestAndRepData>(new CBioseq_DUPLICATE_LOCUS_TAGS_global));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("BAD_LOCUS_TAG_FORMAT") != end_it) {
       thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
           CRef <CTestAndRepData>(new CBioseq_BAD_LOCUS_TAG_FORMAT));
        if (++i >= sz) return;
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
       //thisGrp.tests_on_SeqEntry_feat_desc.push_back( 
       thisGrp.tests_on_BioseqSet.push_back( 
          CRef <CTestAndRepData>(new CBioseqSet_TEST_SMALL_GENOME_SET_PROBLEM));
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
   if ( thisTest.tests_run.find("DISC_DUP_SRC_QUAL") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_SRC_QUAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_DUP_SRC_QUAL_DATA") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_SRC_QUAL_DATA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_SRC_QUAL") != end_it) {
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(
           CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_SRC_QUAL));
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
/*
   if (thisTest.tests_run.find("DISC_SOURCE_QUALS_ASNDISC_oncaller") != end_it){
       thisGrp.tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                             new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller));
        if (++i >= sz) return;
   }
*/
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
       thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.push_back(
           CRef <CTestAndRepData>(new CBioseq_DISC_DUP_DEFLINE));
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

   NCBI_USER_THROW("Some requested tests are unrecognizable.");
};

void CRepConfAsndisc :: x_Asn1(ESerialDataFormat datafm)
{
    string strtmp;
    auto_ptr <CObjectIStream> 
        ois (CObjectIStream::Open(datafm, thisInfo.infile));
    CRef <CSeq_entry> seq_entry (new CSeq_entry);
    strtmp = ois->ReadFileHeader();
    ois->SetStreamPos(0);
    if (strtmp == "Seq-submit") {
       thisInfo.seq_submit.Reset(new CSeq_submit);
       *ois >> thisInfo.seq_submit.GetObject();
OutBlob(*thisInfo.seq_submit, "sub1");
       if (thisInfo.seq_submit->IsEntrys()) {
         ITERATE (list <CRef <CSeq_entry> >, it, 
                                  thisInfo.seq_submit->GetData().GetEntrys()) {
            AddNumEntry(); 
            CheckThisSeqEntry(*it);
         }
       }
    }
    else {
      thisInfo.seq_submit.Reset(0);
      if (strtmp == "Seq-entry") {
         *ois >> *seq_entry;
         AddNumEntry();
         CheckThisSeqEntry(seq_entry);
      }
      else if (strtmp == "Bioseq") {
         CRef <CBioseq> bioseq (new CBioseq);
         *ois >> *bioseq;
         seq_entry->SetSeq(*bioseq);
         AddNumEntry();
         CheckThisSeqEntry(seq_entry);
      }
      else if (strtmp == "Bioseq-set") {
         CRef <CBioseq_set> seq_set(new CBioseq_set);
         *ois >> *seq_set;
         seq_entry->SetSet(*seq_set);
         AddNumEntry();
         CheckThisSeqEntry(seq_entry);
      }
      else {
         NCBI_USER_THROW("Couldn't read type [" + strtmp + "]");
      }
    }
    
    ois->Close();
};

void CRepConfAsndisc :: x_Fasta()
{
   auto_ptr <CReaderBase> reader (CReaderBase::GetReader(CFormatGuess::eFasta)); // flags?
   if (!reader.get()) {
       NCBI_USER_THROW("File format not supported");
   }
   CRef <CSerialObject> 
       obj= reader->ReadObject(*(new CNcbiIfstream(thisInfo.infile.c_str())));
   if (!obj) {
      NCBI_USER_THROW("Couldn't read file " + thisInfo.infile);
   }
   CSeq_entry& seq_entry = dynamic_cast <CSeq_entry&>(obj.GetObject());
   seq_entry.ResetParentEntry();
   AddNumEntry();
   CheckThisSeqEntry(CRef <CSeq_entry> (&seq_entry));
};

void CRepConfAsndisc :: x_GuessFile()
{
   CFormatGuess::EFormat f_fmt = CFormatGuess::Format(thisInfo.infile);
   switch (f_fmt) {
      case CFormatGuess :: eBinaryASN:
          x_Asn1(eSerial_AsnBinary);
          break;
      case CFormatGuess :: eTextASN:
          x_Asn1(); 
          break;
      case CFormatGuess :: eFlatFileSequence:
          break;
      case CFormatGuess :: eFasta:
          x_Fasta();
          break;
      default:
         NCBI_USER_THROW("File format: " + NStr::UIntToString((unsigned)f_fmt) + " not supported");
   }
};

void CSeqEntryReadHook :: SkipClassMember(CObjectIStream& in, const CObjectTypeInfoMI& passed_info)
{
   CRef <CSeq_entry> entry (new CSeq_entry);

   // Iterate through the set of Seq-entry's in container Bioseq-set
   CDiscRepOutput output_obj;
   CRepConfig config;
   CIStreamContainerIterator iter(in, passed_info.GetMemberType());
   for ( ; iter; ++iter ) {
      iter >> *entry; // Read a single Seq-entry from the stream.
      config.AddNumEntry();
      config.CheckThisSeqEntry(entry);  // Process the Seq-entry
      config.CollectRepData();
      output_obj.Export(); 
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

   CDiscRepOutput output_obj;
   CRepConfig config;
   // Iterate through the set of Seq-entry's in container Seq-submit
   CIStreamContainerIterator iter(in, passed_info.GetVariantType());
   for ( ; iter; ++iter ) {
     iter >> *entry; // Read a single Seq-entry from the stream.
     config.AddNumEntry();
     config.CheckThisSeqEntry(entry);  // Process the Seq-entry

     config.CollectRepData(); 
     output_obj.Export(); 
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
        AddNumEntry();
        CheckThisSeqEntry(seq_entry); 
   }
};

void CRepConfAsndisc :: x_ProcessOneFile()
{
   if (!CFile(thisInfo.infile).Exists()) {
      NCBI_USER_THROW("File not found: " + thisInfo.infile);
   }
   switch (m_file_tp[0]) {
      case 'a': x_GuessFile(); 
                break;
      case 'e':
      case 'b': 
      case 's':
      case 'm':
         x_Asn1(); 
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
   CDiscRepOutput output_obj;
   string strtmp;

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
          CollectRepData();
          output_obj.Export();
        }
     }
   }
};


// for Gbench
// void CRepConfig :: CollectDefaultConfig(Str2Str& test_nm2conf_nm, const string& report)
void CRepConfig :: CollectDefaultConfig(Str2Str& test_nm2conf_nm, ETestCategoryFlags report)
{
/*
   ETestCategoryFlags cate_flag;
   if (report== "Asndisc") {
       cate_flag = fAsndisc;
   }
   else if (report == "Discrepancy") {
       cate_flag = fDiscrepancy;
   }
   else if(report ==  "Oncaller") {
       cate_flag = fOncaller;
   }
   else if (report ==  "Mega") {
       cate_flag = fMegaReport;
   }
   else {
       cate_flag = fUnknown;
   }
*/

   for (unsigned i=0; i< ArraySize(test_list); i++) {
      //if (test_list[i].category & cate_flag) {
      if (test_list[i].category & report) {
          test_nm2conf_nm[test_list[i].setting_name] = test_list[i].conf_name;
      }
   }

};


void CRepConfig :: CollectTests()
{
   thisTest.tests_run.clear();

   if (!m_enabled.empty()) {  // only run enabled tests.
      thisTest.tests_run.clear();
      ITERATE (vector <string>, it, m_enabled) {
            thisTest.tests_run.insert(*it);
      }
   }
   else {
      if (thisInfo.report & (fAsndisc | fDiscrepancy)) {
          thisInfo.output_config.use_flag = true;
      }

      string strtmp;
      for (unsigned i=0; i< ArraySize(test_list); i++) {
        if (test_list[i].category & thisInfo.report) {
          strtmp = test_list[i].setting_name;
          if (thisInfo.report & (fDiscrepancy | fMegaReport) ) {
                if ( !(test_list[i].category & (fTSA | fUnknown))) {
                   if (thisInfo.report != fDiscrepancy
                             || strtmp != "DISC_PROTEIN_NAMES"){
                      thisTest.tests_run.insert(test_list[i].setting_name);
                   }
                }
          }
          else thisTest.tests_run.insert(strtmp);
        }
      }
   
      if (!m_disabled.empty() && !thisTest.tests_run.empty()) {
        ITERATE (vector <string>, it, m_disabled) {
           if (thisTest.tests_run.find(*it) != thisTest.tests_run.end()) {
              thisTest.tests_run.erase(*it);
           }
        }
      }
   }

   GetTestList();
};

bool CRepConfig :: x_IsExpandable(const string& setting_name)
{
   if (m_all_expanded) {
      return true;
   }
   else if (!m_expanded.empty()
                 && m_expanded.find(setting_name) != m_expanded.end()) {
      return true; 
   }
   else return false;
};

void CRepConfig :: x_GoGetRep(vector < CRef < CTestAndRepData> >& test_category)
{
   string strtmp;
   NON_CONST_ITERATE (vector <CRef <CTestAndRepData> >, it, test_category) {
       CRef < CClickableItem > c_item (new CClickableItem);
       strtmp = (*it)->GetName();
       if (thisInfo.test_item_list.find(strtmp)
                                    != thisInfo.test_item_list.end()) {
 cerr << "GoGetRep " << strtmp << endl;
            c_item->setting_name = strtmp;
            c_item->item_list = thisInfo.test_item_list[strtmp];
            c_item->expanded = x_IsExpandable(strtmp);
            if ( strtmp != "LOCUS_TAGS"
                        && strtmp != "INCONSISTENT_PROTEIN_ID_PREFIX") {
                  thisInfo.disc_report_data.push_back(c_item);
            }
            (*it)->GetReport(c_item);
       }
       else if ( (*it)->GetName() == "DISC_FEATURE_COUNT") {
           c_item->expanded = x_IsExpandable(strtmp);
           (*it)->GetReport(c_item);
 cerr << "GoGetRep " << (*it)->GetName() << endl;
       }
   }
};

void CRepConfig :: CollectRepData()
{
  x_GoGetRep(thisGrp.tests_on_SubmitBlk);
  x_GoGetRep(thisGrp.tests_on_Bioseq);
  x_GoGetRep(thisGrp.tests_on_Bioseq_aa);
  x_GoGetRep(thisGrp.tests_on_Bioseq_na);
  x_GoGetRep(thisGrp.tests_on_Bioseq_CFeat);
  x_GoGetRep(thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet);
  x_GoGetRep(thisGrp.tests_on_Bioseq_NotInGenProdSet);
  x_GoGetRep(thisGrp.tests_on_SeqEntry);
  x_GoGetRep(thisGrp.tests_on_SeqEntry_feat_desc);
  x_GoGetRep(thisGrp.tests_4_once);
  x_GoGetRep(thisGrp.tests_on_BioseqSet);
  x_GoGetRep(thisGrp.tests_on_Bioseq_CFeat_CSeqdesc);
};

void CRepConfAsndisc :: Run()
{
   CDiscRepOutput output_obj;

   // read input file/path and go tests
   CDir dir(m_indir);
   if (!dir.Exists()) {
       x_ProcessOneFile();
       CollectRepData();
       output_obj.Export();
   }
   else {
       bool one_ofile = (bool)thisInfo.output_config.output_f; 
       x_ProcessDir(dir, one_ofile);
       if (one_ofile) {
          CollectRepData();
          output_obj.Export(); // run a global check 
       }
   }
   thisInfo.disc_report_data.clear();
   thisInfo.test_item_list.clear();
}; // Run()

void CRepConfig :: Run()
{
  thisInfo.seq_submit.Reset(0);
  CRef <CSeq_entry> 
    seq_ref ((CSeq_entry*)(m_TopSeqEntry->GetCompleteSeq_entry().GetPointer()));
  AddNumEntry();
  CheckThisSeqEntry(seq_ref);
  CollectRepData();
};

void CRepConfig :: RunMultiObjects()
{
   ITERATE (vector <CConstRef <CObject> >, it, *m_objs) {
      const CObject* ptr = (*it).GetPointer();

      const CSeq_submit* seq_submit = dynamic_cast<const CSeq_submit*>(ptr);
      const CSeq_entry* entry = dynamic_cast<const CSeq_entry*>(ptr);
      const CBioseq* bioseq = dynamic_cast<const CBioseq*>(ptr);
      const CBioseq_set* seq_set = dynamic_cast<const CBioseq_set*>(ptr);
      
      if (seq_submit) {
         thisInfo.seq_submit = CRef <CSeq_submit> ((CSeq_submit*)seq_submit);
         if (thisInfo.seq_submit->IsEntrys()) {
           ITERATE (list <CRef <CSeq_entry> >, it,
                                  thisInfo.seq_submit->GetData().GetEntrys()) {
              AddNumEntry();
              CheckThisSeqEntry(*it);
           }
         }
      }
      else {
         thisInfo.seq_submit.Reset(0);
         CRef <CSeq_entry> entry_ref (new CSeq_entry);
         if (entry) {
            entry_ref.Reset((CSeq_entry*)entry);
            AddNumEntry();
            CheckThisSeqEntry(CRef <CSeq_entry>(entry_ref));
         }
         else if (bioseq) {
            entry_ref->SetSeq(* ((CBioseq*)bioseq));
            AddNumEntry();
            CheckThisSeqEntry(CRef <CSeq_entry>(entry_ref));
         }
         else if (seq_set) {
            entry_ref->SetSet(* ((CBioseq_set*)seq_set));
            AddNumEntry();
            CheckThisSeqEntry(CRef <CSeq_entry>(entry_ref));
         }
      }
   }

   CollectRepData();
};

END_NCBI_SCOPE
