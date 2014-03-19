#ifndef _DiscRepConfig_HPP
#define _DiscRepConfig_HPP


/*  $Id$
 *===========================================================================
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
 *===========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Report configuration headfile for discrepancy report
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>
#include <serial/objhook.hpp>
#include <serial/serial.hpp>

#include <objects/submit/Contact_info.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objects/seqfeat/RNA_ref.hpp>

#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hDiscRep_output.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc)

   typedef map <string, vector < CConstRef <CObject> > > Str2Objs;
   
   struct s_CombData {
      int    i_val;
      string s_val;
      bool   b_val;
   };
   typedef map <string, s_CombData> Str2CombDt;

   enum ETestCategoryFlags {
      fUnknown = 0,
      fDiscrepancy = 1 << 0,
      fOncaller = 1 << 1,
      fMegaReport = 1 << 2,
      fTSA = 1 << 3,
      fAsndisc = 1 << 4
   };

   struct s_test_property {
      string setting_name;
      int category;
      string conf_name;
   };

   class CSeqEntryReadHook : public CSkipClassMemberHook
   {
     public:
       virtual ~CSeqEntryReadHook () { };
       virtual void SkipClassMember(CObjectIStream& in, const CObjectTypeInfoMI& passed_info);
   };

   class CSeqEntryChoiceHook : public CSkipChoiceVariantHook
   {
     public:
       virtual ~CSeqEntryChoiceHook () { };
       virtual void SkipChoiceVariant(CObjectIStream& in,const CObjectTypeInfoCV& passed_info);
   };

   class CTestGrp
   {
      public:
        ~CTestGrp() { };

        static vector < CRef < CTestAndRepData > > tests_on_Bioseq;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_na;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_aa;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat_NotInGenProdSet;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_NotInGenProdSet;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat_CSeqdesc;
        static vector < CRef < CTestAndRepData > > tests_on_SeqEntry;
        static vector < CRef < CTestAndRepData > > tests_on_SeqEntry_feat_desc;
        static vector < CRef < CTestAndRepData > > tests_4_once;
        static vector < CRef < CTestAndRepData > > tests_on_BioseqSet;
        static vector < CRef < CTestAndRepData > > tests_on_SubmitBlk;
   };

   class NCBI_DISCREPANCY_REPORT_EXPORT CRepConfig : public CObject 
   {
     public:
        CRepConfig() { 
            m_enabled.clear();
            m_disabled.clear();
            m_num_entry = 0;
        }
        virtual ~CRepConfig() { };

        // removed from *_app.hpp
        void InitParams(const IRWRegistry* reg);
        void ReadArgs(const CArgs& args);
        string GetDirStr(const string& src_dir);
        void ProcessArgs(Str2Str& args);
        void SetArg(const string& arg, const string& val);
        void CheckThisSeqEntry(CRef <CSeq_entry> seq_entry);
        void GetOrgModSubtpName(unsigned num1, unsigned num2,
                              map <string, COrgMod::ESubtype>& orgmodnm_subtp);
        CRef <CSearch_func> MakeSimpleSearchFunc(const string& match_text,
                                                 bool whole_word = false);
        void GetTestList();
        void CollectTests();
        virtual void Run();
        void CollectRepData();
        void CollectDefaultConfig(Str2Str& test_nm2conf_nm, const string& report_type);

        void RunMultiObjects();
        static CSeq_entry_Handle* m_TopSeqEntry;
        static CRepConfig* factory(string report_tp,CSeq_entry_Handle* tse_p=0);

        void SetTopLevelSeqEntry(CSeq_entry_Handle* tse) {
           m_TopSeqEntry = tse;
        };

        void SetMultiObjects(vector <CConstRef <CObject> >* objs) {
             m_objs = objs; 
        };
   
        void AddNumEntry() {m_num_entry ++ ;}
        unsigned GetNumEntry() {return m_num_entry; }

     protected:
        unsigned m_num_entry;
        vector <string> m_enabled, m_disabled;
        string m_outsuffix, m_outdir, m_insuffix, m_indir, m_file_tp;
        bool m_dorecurse;
        vector <CConstRef <CObject> >* m_objs;

        void x_GoGetRep(vector < CRef < CTestAndRepData> >& test_category);
   };

   class CRepConfAsndisc : public CRepConfig
   {
      public:
        virtual ~CRepConfAsndisc () {};

        virtual void Run();
     
      private:
        void x_Asn1(ESerialDataFormat datafm = eSerial_AsnText);
        void x_Fasta();
        void x_GuessFile();
        void x_BatchSet(ESerialDataFormat datafm = eSerial_AsnText);
        void x_BatchSeqSubmit(ESerialDataFormat datafm = eSerial_AsnText);
        void x_CatenatedSeqEntry();
        void x_ProcessOneFile();
        void x_ProcessDir(const CDir& dir, bool out_f);
   };

   class CRepConfDiscrepancy : public CRepConfig
   {
      public:
        virtual ~CRepConfDiscrepancy () {};
   };

   class CRepConfOncaller : public CRepConfig
   {
      public:
        virtual ~CRepConfOncaller () {};
   };

   class CRepConfMega  : public CRepConfig
   {
      public:
        virtual ~CRepConfMega () {};
   };

   class CDiscRepInfo 
   {
      public:
        CDiscRepInfo ();
        ~CDiscRepInfo () {};

        static CRef < CScope >                    scope;
        static string                             infile;
        static string                             report;
        static COutputConfig                      output_config;
        static CRef <CRepConfig>                  config;
        static vector < CRef < CClickableItem > > disc_report_data;
        static Str2Strs                           test_item_list;
        static Str2Objs                           test_item_objs;
        static CConstRef < CSuspect_rule_set>     suspect_prod_rules;
        static vector < vector <string> >         susrule_summ;
        static vector <string> 		          weasels;
        static CRef <CSeq_submit>                 seq_submit;
        static bool                               expand_defline_on_set;
        static bool                               expand_srcqual_report;
        static string                             report_lineage;
        static bool                               exclude_dirsub;

        static Str2CombDt                         rRNATerms;
        static vector <string>                    bad_gene_names_contained;
        static vector <string>                    no_multi_qual;
        static vector <string>                    rrna_standard_name; 
        static vector <string>                    short_auth_nms; 
        static vector <string>                    spec_words_biosrc; 
        static vector <string>                    suspicious_notes;
        static vector <string>                    trna_list; 
        static Str2UInt                           desired_aaList;
        static Str2Str                            state_abbrev;
        static Str2Str                            cds_prod_find;
        static vector <string>                    new_exceptions;
        static Str2Str		                  srcqual_keywords;
        static vector <string>                    kIntergenicSpacerNames;
        static vector <string>                    virus_lineage;
        static vector <string>                    strain_tax;
        static Str2CombDt                         fix_data;
        static CConstRef <CSuspect_rule_set>      orga_prod_rules;
        static vector <string>                    skip_bracket_paren;
        static vector <string>                    ok_num_prefix;
        static map <EMacro_feature_type, CSeqFeatData::ESubtype>  feattype_featdef;
        static Str2Str                            featkey_modified;
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
        static map <string, string>                     pub_class_quals; 
        static vector <string>                          months;
        static map <EMolecule_type, CMolInfo::EBiomol>  moltp_biomol;
        static map <ETechnique_type, CMolInfo::ETech>   techtp_mitech;
        static map <ETechnique_type, string>            techtp_name;
        static vector <string>                          s_putative_replacements;
//        static vector <string>                          fix_type_names;
        static map <ECDSGeneProt_field, string>         cgp_field_name;
        static map <ECDSGeneProt_feature_type_constraint, string>          cgp_feat_name;
        static map <EMolecule_type, string>             moltp_name;
        static vector < vector <string> >               susterm_summ;
        static map <EFeature_strandedness_constraint, string> feat_strandedness;
        static map <EPublication_field, string>         pubfield_label;
        static map <CPub_field_special_constraint_type::E_Choice,string> spe_pubfield_label;
        static map <EFeat_qual_legal, string>           featqual_leg_name;
        static vector <string>                          miscfield_names;
        static vector <string>                          loctype_names;
        static map <EString_location, string>           matloc_names;
        static map <EString_location, string>           matloc_notpresent_names;
        static map <ESource_location, string>           srcloc_names;
        static map <ESource_origin, string>             srcori_names;
        static map <ECompletedness_type, string>        compl_names;
        static map <EMolecule_class_type, string>       molclass_names;
        static map <ETopology_type, string>             topo_names;
        static map <EStrand_type, string>               strand_names;
        static CRef < CSuspect_rule_set>                suspect_rna_rules;
        static vector <string>                          rna_rule_summ;
        static vector <string>                          suspect_phrases;
        static map <int, string>                        genome_names;

        static const s_SuspectProductNameData *         suspect_prod_terms;
        static unsigned num_suspect_prod_terms;
   };

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE

#endif
