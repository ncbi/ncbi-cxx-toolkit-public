#ifndef _HDiscRepApp_HPP
#define _HDiscRepApp_HPP

/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   main headfile of cpp discrepancy report
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexpt.hpp>

#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/taxon1/taxon1.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/validator/validatorp.hpp>

// macros
#include <objects/macro/CDSGen_featur_type_constra_.hpp>
#include <objects/macro/CDSGeneProt_field_.hpp>
#include <objects/macro/Completedness_type_.hpp>
#include <objects/macro/DBLink_field_type_.hpp>
#include <objects/macro/Feat_qual_legal_.hpp>
#include <objects/macro/Feature_stranded_constrain_.hpp>
#include <objects/macro/Macro_feature_type_.hpp>
#include <objects/macro/Molecule_type_.hpp>
#include <objects/macro/Molecule_class_type_.hpp>
#include <objects/macro/Pub_field_speci_const_type.hpp>
#include <objects/macro/Publication_field.hpp>
#include <objects/macro/Rna_feat_type.hpp>
#include <objects/macro/Rna_field_.hpp>
#include <objects/macro/Sequence_constraint_rnamol_.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Source_location_.hpp>
#include <objects/macro/Source_origin_.hpp>
#include <objects/macro/Source_qual_.hpp>
#include <objects/macro/Strand_type.hpp>
#include <objects/macro/String_location_.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Technique_type_.hpp>
#include <objects/macro/Topology_type_.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <common/test_assert.h>

using namespace ncbi;
using namespace objects;
using namespace validator;

BEGIN_NCBI_SCOPE

namespace DiscRepNmSpc{

     typedef map < string, string> Str2Str;
     typedef map < string, vector < const CSerialObject* > > Str2Objs;
     typedef map < string, vector < string > > Str2Strs;

     enum ESuspectNameType {
        eSuspectNameType_None = 0,
        eSuspectNameType_Typo = 1,
        eSuspectNameType_QuickFix,
        eSuspectNameType_NoOrganelleForProkaryote,
        eSuspectNameType_MightBeNonfunctional,
        eSuspectNameType_Database,
        eSuspectNameType_RemoveOrganismName,
        eSuspectNameType_InappropriateSymbol,
        eSuspectNameType_EvolutionaryRelationship,
        eSuspectNameType_UseProtein,
        eSuspectNameType_Max
     };
     typedef bool (*FSuspectProductNameSearchFunc) (const string& str1, const string& str2);
     typedef void (*FSuspectProductNameReplaceFunc) (const string& str, const string& str2,
                                                   const string& str3, const CSeq_feat& feat);
     struct SuspectProductNameData {
         const char* pattern;
         FSuspectProductNameSearchFunc search_func;
         ESuspectNameType fix_type;
         const char* replace_phrase;
         FSuspectProductNameReplaceFunc replace_func;
     };

     typedef struct loginfo 
     {
       ofstream fout;
       bool data_in_log;
       string display_title;
       string path;
     } LogInfo;


     typedef void (*ClickableCallback) (list <void *> item_list, void *userdata);
     typedef void (*ClickableCallbackDataFree) (void * userdata);

     class CClickableItem  : public CObject
     {
       public:
          CClickableItem () : item_list(), callback_data(0),chosen(false), 
                              subcategories(),
                              expanded(false), level(0),
                              autofix_data(0), next_sibling(false) {};
          ~CClickableItem () {};

          string                               setting_name;
          string                               description;
          vector < string >                    item_list;
          void *                               callback_data;
          bool                                 chosen;
          vector < CRef <CClickableItem > >    subcategories;
          bool                                 expanded;
          int                                  level;
          void *                               autofix_data; 
          bool                                 next_sibling;
          void (*autofix_func)(list <void *> item_list, void *userdata, 
                                                             LogInfo& lip);  
          void (*callback_func)(list <void *> item_list, void *userdata); 
          void (*datafree_func)(void *userdata); 
     };
   
     class COutputConfig {
 
        public:
           bool     use_flag;
           ofstream output_f;
           bool     summary_report;
     };

     typedef map<string, unsigned>  Str2UInt;

     class CDiscRepInfo
     {
        public:
          CDiscRepInfo ();
          ~CDiscRepInfo () {};

           static CRef < CScope >                    scope;
           static string                             infile;
           static COutputConfig                      output_config;
           static vector < CRef < CClickableItem > > disc_report_data;
           static Str2Strs                           test_item_list;
           static CRef < CSuspect_rule_set>          suspect_prod_rules;
           static vector < vector <string> >         susrule_summ;
           static vector <string> 		     weasels;
           static CRef <CSeq_submit>                 seq_submit;
           static string                             expand_defline_on_set;
           static string                             report_lineage;
           static vector <string>                    strandsymbol;
           static bool                               exclude_dirsub;
           static string                             report;

           static Str2UInt                           rRNATerms;
           static Str2UInt                           rRNATerms_partial;
           static vector <string>                    bad_gene_names_contained;
           static vector <string>                    no_multi_qual;
           static vector <string>                    rrna_standard_name; 
           static vector <string>                    short_auth_nms; 
           static vector <string>                    spec_words_biosrc; 
           static vector <string>                    suspicious_notes;
           static vector <string>                    trna_list; 
           static Str2UInt                           desired_aaList;
           static CTaxon1                            tax_db_conn;
           static list <string>                      state_abbrev;
           static Str2Str                            cds_prod_find;
           static vector <string>                    s_pseudoweasels;
           static vector <string>                    suspect_rna_product_names;
           static vector <string>                    new_exceptions;
           static Str2Str		             srcqual_keywords;
           static vector <string>                    kIntergenicSpacerNames;
           static vector <string>                    taxnm_env;
           static vector <string>                    virus_lineage;
           static vector <string>                    strain_tax;
           static CRef <CComment_set>                comment_rules;
           static Str2UInt                           whole_word;
           static Str2Str                            fix_data;
           static CRef <CSuspect_rule_set>           orga_prod_rules;
           static vector <string>                    skip_bracket_paren;
           static vector <string>                    ok_num_prefix;
           static map <EMacro_feature_type, CSeqFeatData::ESubtype>  feattype_featdef;
           static map <EMacro_feature_type, string>  feattype_name;
           static map <CRna_feat_type::E_Choice, CRNA_ref::EType> rnafeattp_rnareftp;
           static map <ERna_field, EFeat_qual_legal>  rnafield_featquallegal;
           static map <ERna_field, string>            rnafield_names;
           static vector <string>                     rnatype_names;
           static map <CSeq_inst::EMol, string>       mol_molname;
           static map <CSeq_inst::EStrand, string>    strand_strname;
           static vector <string>                     dblink_names;
           static map <ESource_qual, COrgMod::ESubtype>    srcqual_orgmod;
           static map <ESource_qual, CSubSource::ESubtype> srcqual_subsrc;
           static map <ESource_qual, string>               srcqual_names;
           static map <EFeat_qual_legal, string>           featquallegal_name;
           static map <EFeat_qual_legal, unsigned>         featquallegal_subfield;
           static map <ESequence_constraint_rnamol, CMolInfo::EBiomol> scrna_mirna;
           static map <string, string>               pub_class_quals; 
           static vector <string>                    months;
           static map <EMolecule_type, CMolInfo::EBiomol>   moltp_biomol;
           static map <ETechnique_type, CMolInfo::ETech>    techtp_mitech;
           static map <ETechnique_type, string>             techtp_name;
           static vector <string>                           s_putative_replacements;
           static vector <string>                           suspect_name_category_names;
           static vector <string>                           fix_type_names;
           static map <ECDSGeneProt_field, string>          cgp_field_name;
           static map <ECDSGeneProt_feature_type_constraint, string>          cgp_feat_name;
           static map <EMolecule_type, string>              moltp_name;
           static vector < vector <string> >                susterm_summ;
           static map <EFeature_strandedness_constraint, string> feat_strandedness;
           static map <EPublication_field, string>          pubfield_label;
           static map <CPub_field_special_constraint_type::E_Choice,string> spe_pubfield_label;
           static map <EFeat_qual_legal, string>            featqual_leg_name;
           static vector <string>                           miscfield_names;
           static vector <string>                           loctype_names;
           static map <EString_location, string>            matloc_names;
           static map <EString_location, string>            matloc_notpresent_names;
           static map <ESource_location, string>            srcloc_names;
           static map <ESource_origin, string>              srcori_names;
           static map <ECompletedness_type, string>         compl_names;
           static map <EMolecule_class_type, string>        molclass_names;
           static map <ETopology_type, string>              topo_names;
           static map <EStrand_type, string>                strand_names;
           static CRef < CSuspect_rule_set>                 suspect_rna_rules;
           static vector <string>                           rna_rule_summ;
           static vector <string>                           tests_enabled;
           static vector <string>                           tests_disabled;
           static vector <string>                           suspect_phrases;

           SuspectProductNameData                    suspect_prod_terms[];
           unsigned GetSusProdTermsLen() { return sizeof(suspect_prod_terms); };
     };

     class CDiscRepApp : public CNcbiApplication
     {
       public:
         virtual void Init(void);
         virtual int  Run (void);
         virtual void Exit(void);

         void CheckThisSeqEntry(CRef <CSeq_entry> seq_entry);
         void GetOrgModSubtpName(unsigned num1, unsigned num2,
                                         map <string, COrgMod::ESubtype>& orgmodnm_subtp);
         CRef <CSearch_func> MakeSimpleSearchFunc(const string& match_text, 
                                                                 bool whole_word = false);
     };
};

END_NCBI_SCOPE

#endif
