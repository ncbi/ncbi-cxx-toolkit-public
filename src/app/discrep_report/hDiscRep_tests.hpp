#ifndef _DiscRep_Test_HPP
#define _DiscRep_Test_HPP

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
 *   Tests and item data collection for Cpp Discrepany Report
 */


// macros
#include <objects/macro/CDSGen_featur_type_constra_.hpp>
#include <objects/macro/CDSGeneProt_field_.hpp>
#include <objects/macro/Completedness_type_.hpp>
#include <objects/macro/Constraint_choice.hpp>
#include <objects/macro/Constraint_choice_set.hpp>
#include <objects/macro/DBLink_field_type_.hpp>
#include <objects/macro/Feat_qual_choice.hpp>
#include <objects/macro/Feat_qual_legal_.hpp>
#include <objects/macro/Feature_field.hpp>
#include <objects/macro/Feature_stranded_constrain_.hpp>
#include <objects/macro/Location_constraint.hpp>
#include <objects/macro/Macro_feature_type_.hpp>
#include <objects/macro/Molecule_type_.hpp>
#include <objects/macro/Molecule_class_type_.hpp>
#include <objects/macro/Molinfo_field.hpp>
#include <objects/macro/Pub_field_constraint.hpp>
#include <objects/macro/Pub_field_speci_const_type.hpp>
#include <objects/macro/Pub_field_special_constrai.hpp>
#include <objects/macro/Pub_type_.hpp>
#include <objects/macro/Publication_field.hpp>
#include <objects/macro/Quantity_constraint.hpp>
#include <objects/macro/Rna_feat_type.hpp>
#include <objects/macro/Rna_field_.hpp>
#include <objects/macro/Rna_qual.hpp>
#include <objects/macro/Seqtype_constraint_.hpp>
#include <objects/macro/Sequence_constraint_rnamol_.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Source_location_.hpp>
#include <objects/macro/Source_origin_.hpp>
#include <objects/macro/Source_qual_.hpp>
#include <objects/macro/Source_qual_choice.hpp>
#include <objects/macro/Strand_type.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/macro/String_location_.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Technique_type_.hpp>
#include <objects/macro/Topology_type_.hpp>

#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>      
#include <objects/pub/Pub.hpp>

#include <objmgr/align_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>

using namespace ncbi;
using namespace objects;

BEGIN_NCBI_SCOPE

namespace DiscRepNmSpc {
  typedef map < string, string> Str2Str;
  typedef map < string, vector < string > > Str2Strs;
  typedef map<string, unsigned>  Str2UInt;
  typedef map <string, vector <int> > Str2Ints;
  typedef map <string, int> Str2Int;
  typedef map <int, int> Int2Int;
  typedef map <string, Str2Strs> Str2MapStr2Strs;
  typedef map <string, vector <CConstRef <CBioseq> > > Str2Seqs;
  typedef map <string, CConstRef <CObject> > Str2Obj;

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
  struct s_SuspectProductNameData {
     const char* pattern;
     FSuspectProductNameSearchFunc search_func;
     ESuspectNameType fix_type;
     const char* replace_phrase;
     FSuspectProductNameReplaceFunc replace_func;
  };

  class CClickableItem  : public CObject
  {
     public:
       CClickableItem () : item_list(), subcategories(), expanded(false),
                           next_sibling(false) {};
       ~CClickableItem () {};

       string                               setting_name;
       string                               description;
       vector < string >                    item_list;
       vector < CRef <CClickableItem > >    subcategories;
       bool                                 expanded;
       bool                                 next_sibling;
       vector < CConstRef <CObject> >       obj_list;
  };

  class CDiscTestInfo 
  {
    public:
      static set <string> tests_run;

      static bool   is_Aa_run;
      static bool   is_AllAnnot_run;  // checked
      static bool   is_Bad_Gene_Nm; 
      static bool   is_BacPartial_run;
      static bool   is_Bases_N_run; 
      static bool   is_BASES_N_run; // may not be needed
      static bool   is_BioSet_run;
      static bool   is_Biosrc_Orgmod_run;
      static bool   is_BIOSRC_run;
      static bool   is_CDs_run;
      static bool   is_CdTransl_run;
      static bool   is_Comment_run;
      static bool   is_Defl_run; // checked
      static bool   is_DESC_user_run; // checked
      static bool   is_Genes_run;
      static bool   is_Genes_oncall_run;
      static bool   is_GP_Set_run;
      static bool   is_IncnstUser_run;
      static bool   is_LocusTag_run;
      static bool   is_MolInfo_run;
      static bool   is_MRNA_run;   // discard later
      static bool   is_mRNA_run;
      static bool   is_Prot_run;
      static bool   is_ProtFeat_run;
      static bool   is_Pub_run;
      static bool   is_Quals_run;
      static bool   is_RRna_run;
      static bool   is_Subsrc_run;
      static bool   is_SusPhrase_run;
      static bool   is_SusProd_run;
      static bool   is_TaxCflts_run;
      static bool   is_TaxDef_run;
      static bool   is_TRna_run;
  };

  template < typename T >
  struct SCompareCRefs
  {
     bool operator() (const CRef < T >& x, const CRef < T >& y) const {
         if ( !(y.GetPointer()) ) return false;
         else if ( (!x.GetPointer()) ) return true;
         else return ((*x).str) <  ((*y).str) ;
     }
  };

  // get all string type data from object
  template <class T>
  void GetStringsFromObject(const T& obj, vector <string>& strs)
  {
     CTypesConstIterator it(CStdTypeInfo<string>::GetTypeInfo(),
                          CStdTypeInfo<utf8_string_type>::GetTypeInfo());
     for (it = ConstBegin(obj);  it;  ++it)
        strs.push_back(*static_cast<const string*>(it.GetFoundPtr()));
  };

  struct qualvlu_distribute {
         Str2Strs qual_vlu2src;
         vector <string> missing_item, multi_same, multi_dup, multi_all_dif;
  };
  typedef map <string, qualvlu_distribute > Str2QualVlus;

  class GeneralDiscSubDt: public CObject
  {
     public:
        GeneralDiscSubDt (const string& new_text, 
                          const string& new_str)
                           : obj_text(1, new_text), 
                             str(new_str), 
                             sf0_added(false), 
                             biosrc(0) {};

        virtual ~GeneralDiscSubDt () {};

        vector < string > obj_text;  // e.g., seq_feat list
        vector <CConstRef <CObject> > obj_ls; // e.g., seq_feat obj list
        string str;                       // prop. of the list;
        bool  sf0_added;
        bool  isAa;
        const CBioSource* biosrc;
        map <string, int> feat_count_list;
  };

  enum ECommentTp {
     e_IsComment = 0,
     e_ContainsComment,
     e_HasComment,
     e_DoesComment,
     e_OtherComment,
     e_NoCntComment
  };

  class CSuspectRuleCheck : public CObject
  {
    public:
      CSuspectRuleCheck () {}; 
      ~CSuspectRuleCheck () {};

      class CCGPSetData : public CObject
      {
        public:
          CCGPSetData() { cds = gene = mrna = 0; 
                          prot = CConstRef <CSeq_feat> (0); mat_peptide_list.clear();};
          ~CCGPSetData() {};
          
          const CSeq_feat* cds;
          const CSeq_feat* gene;
          const CSeq_feat* mrna;
          CConstRef <CSeq_feat> prot;
          vector <const CSeq_feat*> mat_peptide_list;
      };

      static CBioseq_Handle m_bioseq_hl;  // ?? how about reference?

      bool DoesStringMatchSuspectRule(const CBioseq_Handle& bioseq_hl, const string& str, 
                                             const CSeq_feat& feat, const CSuspect_rule& rule);
      // string ~ rule/constraint
      bool MatchesSuspectProductRule(const string& str, const CSuspect_rule& rule);
      bool IsSearchFuncEmpty(const CSearch_func& func);
      bool IsStringConstraintEmpty(const CString_constraint* constraint);
      bool MatchesSearchFunc(const string& str, const CSearch_func& func);
      bool DoesStringMatchConstraint(const string& str, 
                                                      const CString_constraint* str_cons=0);
      bool DoesSingleStringMatchConstraint(const string& str, 
                                                      const CString_constraint* str_cons=0);
      string StripUnimportantCharacters(const string& str, bool strip_space, 
                                                                      bool strip_punch);
      string SkipWeasel(const string& str);
      bool AdvancedStringMatch(const string& str, 
                                               const CString_constraint* str_const = 0);
      bool AdvancedStringCompare(const string& str, const string& str_match, 
                                            const CString_constraint* str_cons, bool is_start, 
                                            unsigned* ini_target_match_len = 0);
      bool CaseNCompareEqual(string str1, string str2, unsigned len1, bool case_sensitive);
      bool IsWholeWordMatch(const string& start, const size_t& found, 
                               const unsigned& match_len, bool disallow_slash = false);
      bool DisallowCharacter(const char ch, bool disallow_slash);
      bool IsStringInSpanInList (const string& str, const string& list);
      bool GetSpanFromHyphenInString(const string& str, const size_t& hyphen, 
                                                   string& first, string& second);
      bool IsStringInSpan(const string& str, const string& first, const string& second);
      bool StringIsPositiveAllDigits(const string& str);
      bool StringMayContainPlural(const string& str);
      bool ContainsNorMoreSetsOfBracketsOrParentheses(string search, const int& n);
      bool SkipBracketOrParen(const unsigned& idx, string& start);
      char GetClose(char bp);
      bool ContainsThreeOrMoreNumbersTogether(const string& search);
      bool PrecededByOkPrefix (const string& start_str);
      bool InWordBeforeCytochromeOrCoenzyme(const string& start_str);
      bool FollowedByFamily(string& after_str);
      bool StringContainsUnderscore(const string& search);
      bool IsPrefixPlusNumbers(const string& prefix, const string& search);
      bool IsPropClose(const string& str, char open_p);
      bool StringContainsUnbalancedParentheses(const string& search);
      bool ProductContainsTerm(const string& pattern, const string& search);

      // object ~ rule/constraint
      bool DoesObjectMatchConstraintChoiceSet(const CSeq_feat& feat,
                                                    const CConstraint_choice_set& c_set);
      bool DoesObjectMatchMolinfoFieldConstraint (const CSeq_feat& seq_feat, 
                                                 const CMolinfo_field_constraint& mol_cons);
      bool DoesObjectMatchConstraint(const CSeq_feat& data, const vector <string>& strs,
                                                         const CConstraint_choice& cons);
      bool DoesObjectMatchStringConstraint(const CSeq_feat& feat, const vector <string>& strs, 
                                                        const CString_constraint& str_cons);
      bool DoesObjectMatchStringConstraint(const CBioSource& biosrc, 
                                                        const CString_constraint& str_cons);
      bool DoesObjectMatchStringConstraint(const CCGPSetData& cgp,
                                                        const CString_constraint& str_cons);
      bool IsLocationConstraintEmpty(const CLocation_constraint& loc_cons);
      bool DoesStrandMatchConstraint(const CSeq_loc& loc,const CLocation_constraint& loc_cons);
      bool DoesBioseqMatchSequenceType(const ESeqtype_constraint& seq_type);
      bool DoesLocationMatchPartialnessConstraint(const CSeq_loc& loc, 
                                                        const CLocation_constraint& loc_cons);
      bool DoesLocationMatchTypeConstraint(const CSeq_loc& loc, 
                                                       const CLocation_constraint& loc_cons);
      bool DoesPositionMatchEndConstraint(int pos, const CLocation_pos_constraint& lp_cons);
      bool DoesLocationMatchDistanceConstraint(const CSeq_loc& loc, 
                                                     const CLocation_constraint& loc_cons);
      bool DoesObjectMatchFieldConstraint(const CSeq_feat& data, 
                                                        const CField_constraint& field_cons);
      bool DoesObjectMatchRnaQualConstraint (const CSeq_feat& seq_feat, 
                               const CRna_qual& rna_qual, const CString_constraint& str_cons);
      string GetFieldValueForObjectEx (const CField_type& field_type, 
                                                          const CString_constraint& str_cons);
      string GetSrcQualValue4FieldType(const CBioSource& biosrc, 
                  const CSource_qual_choice& src_qual, const CString_constraint* str_cons = 0);
      string GetSequenceQualFromBioseq (const CMolinfo_field& mol_field);
      string GetDBLinkFieldFromUserObject(const CUser_object& user_obj, 
                          EDBLink_field_type dblink_tp, const CString_constraint& str_cons);
      string GetFirstStringMatch(const list <string>& strs,
                                                   const CString_constraint* str_cons = 0);
      string GetFirstStringMatch(const vector <string>& strs,const 
                                                             CString_constraint* str_cons = 0);
      string GetQualName(ESource_qual src_qual);
      bool IsSubsrcQual(ESource_qual src_qual);
      string GetNotTextqualSrcQualValue(const CBioSource& biosrc, 
                  const CSource_qual_choice& src_qual, const CString_constraint* constraint);
      bool DoesObjectMatchFeatureFieldConstraint(const CSeq_feat& feat, 
                        const CFeature_field& feat_field, const CString_constraint& str_cons);
      string GetIntervalString(const CSeq_interval& seq_int);
      string GetAnticodonLocString (const CTrna_ext& trna);
      string GetCodeBreakString (const CSeq_feat& seq_feat);
      string GetDbxrefString (const vector <CRef <CDbtag> >& dbxref, 
                                                 const CString_constraint* str_cons = 0);
      string GettRNACodonsRecognized (const CTrna_ext& trna, 
                                                 const CString_constraint* str_cons = 0);
      string GetQualFromFeatureAnyType(const CSeq_feat& seq_feat, 
                 const CFeat_qual_choice& feat_qual, const CString_constraint* str_cons = 0);
      string GetQualFromFeature(const CSeq_feat& seq_feat, 
                   const CFeature_field& feat_field, const CString_constraint* str_cons = 0);
      bool DoesFeatureMatchRnaType(const CSeq_feat& seq_feat, const CRna_feat_type& rna_type);
      void MakeFeatureField(CRef <CFeature_field>& f, CFeat_qual_choice& f_qual, 
                                     EMacro_feature_type f_tp, EFeat_qual_legal legal_qual);
      CRef <CFeature_field> FeatureFieldFromCDSGeneProtField (
                                             ECDSGeneProt_field cds_gene_prot_field);
      bool DoesBiosourceMatchConstraint (const CBioSource& biosrc, 
                                                         const CSource_constraint& src_cons);
      bool IsSourceQualPresent (const CBioSource& biosrc, 
                                      const CSource_qual_choice* src_cons = 0);
      bool IsNonTextSourceQualPresent(const CBioSource& biosrc, ESource_qual srcqual);
      bool AllowSourceQualMulti(const CSource_qual_choice* src_cons = 0);
      bool DoesFeatureMatchCGPQualConstraint (const CSeq_feat& feat, 
                                             const CCDSGeneProt_qual_constraint& cons);
      void GetProtFromCodingRegion (CRef <CCGPSetData>& cgp, const CSeq_feat& cd_feat);
      CConstRef <CSeq_feat> AddProtFeatForCds(const CSeq_feat& cd_feat, 
                                                            const CBioseq_Handle& protbsp);
      bool DoesCGPSetMatchQualConstraint (const CCGPSetData& c, 
                                            const CCDSGeneProt_qual_constraint& cgp_cons);
      string  GetFieldValueFromCGPSet (const CCGPSetData& c, ECDSGeneProt_field field, 
                                                      const CString_constraint* str_cons = 0);
      bool CanGetFieldString(const CCGPSetData& cgp, const ECDSGeneProt_field field, 
                                                       const CString_constraint& str_cons);
      string GetFirstGBQualMatch (const vector <CRef <CGb_qual> >& quals, 
                                       const string& qual_name, unsigned subfield = 0, 
                                       const CString_constraint* str_cons = 0);
      string GetTwoFieldSubfield(const string& str, unsigned subfield);
      string GetFieldValue(const CCGPSetData& cgp, const CSeq_feat& feat, 
                                ECDSGeneProt_field val, const CString_constraint* str_cons=0);
      bool DoesFeatureMatchCGPPseudoConstraint (const CSeq_feat& seq_feat, 
                                            const CCDSGeneProt_pseudo_constraint& cgp_p_cons);
      void ListFeaturesInLocation (const CSeq_loc& seq_loc, 
                         CSeqFeatData::ESubtype subtype, vector <const CSeq_feat*> feat_list);
      const CSeq_feat* GetmRNAforCDS (const CSeq_feat& cds); 
      bool DoesSequenceMatchSequenceConstraint ( const CSequence_constraint& seq_cons);
      bool IsSequenceConstraintEmpty (const CSequence_constraint& constraint);
      bool DoesFeatureCountMatchQuantityConstraint (CSeqFeatData::ESubtype featdef, 
                                                   const CQuantity_constraint& quantity);
      bool DoesValueMatchQuantityConstraint(int val,const CQuantity_constraint& quantity);
      bool DoesSeqIDListMeetStringConstraint (const vector <CSeq_id_Handle>& seq_ids, 
                                                       const CString_constraint& str_cons);
      bool DoesTextMatchBankItId (const CSeq_id& sid, const CString_constraint& str_cons);
      string CopyListWithoutBankIt (const string& orig);
      bool DoesObjectIdMatchStringConstraint (const CObject_id& obj_id, 
                                                       const CString_constraint& str_cons);
      bool  DoesSequenceMatchStrandednessConstraint (
                                              EFeature_strandedness_constraint strandedness);
      bool DoesPubMatchPublicationConstraint (const CPubdesc& pub_desc, 
                                                   const CPublication_constraint& pub_cons);
      bool IsPublicationConstraintEmpty (const CPublication_constraint& constraint);
      bool DoesPubFieldMatch (const CPubdesc& pub_desc, const CPub_field_constraint& field);
      string GetPubFieldFromPub (const CPub& the_pub, EPublication_field field, 
                                                      const CString_constraint* str_cons = 0);
      string GetPubclassFromPub(const CPub& the_pub);
      EPub_type GetPubMLStatus (const CPub& the_pub);
      string GetAuthorListString (const CAuth_list& auth_ls, 
                               const CString_constraint* str_cons = 0, bool use_initials = 0);
      string GetPubFieldFromAffil (const CAffil& affil, EPublication_field field, 
                                                       const CString_constraint* str_cons = 0);
      string Get1stStringMatchFromTitle(const CTitle& title, 
                                                    const CString_constraint* str_cons = 0);
      string PrintPartialOrCompleteDate(const CDate& date);
      string GetPubFieldFromCitJour(const CCit_jour& jour, EPublication_field field, 
                                                       const CString_constraint* str_cons = 0);
      string GetPubFieldFromImprint (const CImprint& imprint, EPublication_field field, 
                                                      const CString_constraint* str_cons = 0);
      string GetPubFieldFromCitBook (const CCit_book& book, EPublication_field field, 
                                                     const CString_constraint* str_cons = 0); 
      bool DoesPubFieldSpecialMatch (const CPubdesc& pdb_desc, 
                                            const CPub_field_special_constraint& field);
      string GetConstraintFieldFromObject (const CSeq_feat& seq_feat, const CField_type& field,
                                                  const CString_constraint* str_cons = 0);
      string GetRNAQualFromFeature(const CSeq_feat& seq_feat, const CRna_qual& rna_qual, 
                                              const CString_constraint* str_cons = 0);
      bool DoesCodingRegionMatchTranslationConstraint(const CSeq_feat& feat, 
                                                 const CTranslation_constraint& trans_cons);
      bool DoesFeatureMatchLocationConstraint(const CSeq_feat& feat, 
                                                  const CLocation_constraint& loc_cons);
    private:
      bool x_DoesStrContainPlural(const string& word, char last_letter, 
                                                         char second_to_last_letter);
  };

  class CDiscRepUtil
  {
    public:
      CDiscRepUtil () {};
      ~CDiscRepUtil () {};

      static bool IsWholeWord(const string& str, const string& phrase);
      static bool IsAllCaps(const string& str);
      static bool IsAllLowerCase(const string& str);
      static bool IsAllPunctuation(const string& str);
   
      static string digit_str, alpha_str;
  };

  class CTestAndRepData : public CObject
  {
    public:

      CTestAndRepData() {};
      virtual ~CTestAndRepData() {};

      virtual void TestOnObj(const CBioseq_set& bioseq_set) = 0;
      virtual void TestOnObj(const CSeq_entry& seq_entry) = 0;
      virtual void TestOnObj(const CBioseq& bioseq) = 0;
      virtual void TestOnObj(const CSeq_feat& seq_feat) = 0;

      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;

      virtual string GetName() const =0;

      static bool AllCapitalLetters(const string& pattern, const string& search);
      static bool BeginsWithPunct(const string& pattern, const string& search);
      static bool BeginsOrEndsWithQuotes(const string& pattern, const string& search);
      static bool ContainsDoubleSpace(const string& pattern, const string& search);
      static bool ContainsPseudo(const string& pattern, const list <string>& strs);
      static bool ContainsTwoSetsOfBracketsOrParentheses(const string& pattern, 
                                                                     const string& search);
      static bool ContainsWholeWord(const string& pattern, const string& search);
      static bool ContainsWholeWord(const string& pattern, const list <string>& strs);
      static bool ContainsWholeWordCaseSensitive(const string& pattern, const string& search);
      static bool ContainsUnbalancedParentheses(const string& pattern, const string& search);
      static bool ContainsUnderscore(const string& pattern, const string& search);
      static bool ContainsUnknownName(const string& pattern, const string& search);
      static bool EndsWithFold(const string& pattern, const string& search);
      static bool EndsWithPattern(const string& pattern, const string& search);
      static bool EndsWithPunct(const string& pattern, const string& search);
      static bool IsSingleWord(const string& pattern, const string& search);
      static bool IsSingleWordOrWeaselPlusSingleWord(const string& pattern, 
                                                                  const string& search);
      static bool IsTooLong(const string& pattern, const string& search);
      static bool MayContainPlural(const string& pattern, const string& search);
      static bool NormalSearch(const string& pattern, const string& search);
      static bool PrefixPlusNumbersOnly(const string& pattern, const string& search);
      static bool ProductContainsTerm(const string& pattern, const string& search);
      static bool StartsWithPattern(const string& pattern, const string& search);
      static bool StartsWithPutativeReplacement(const string& pattern, const string& search);
      static bool ThreeOrMoreNumbersTogether(const string& pattern, const string& search);

      static void HaemReplaceFunc(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void RemoveBeginningAndEndingQuotes(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void ReplaceWholeNameFunc(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void ReplaceWholeNameAddNoteFunc(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void SimpleReplaceAnywhereFunc(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void SimpleReplaceFunc(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};
      static void UsePutative(const string& str, const string& str2, 
                                             const string& str3, const CSeq_feat& feat) {};

      string GetIsComment(unsigned cnt, const string& str);
      string GetHasComment(unsigned cnt, const string& str);
      string GetDoesComment(unsigned cnt, const string& str);
      string GetContainsComment(unsigned cnt, const string& str);
      string GetOtherComment(unsigned cnt, const string& single_str,const string& plural_str);
      string GetNoCntComment(unsigned cnt, const string& single_str,const string& plural_str);
      string GetNoun(unsigned cnt, const string& str);

 //  GetDiscrepancyItemTextEx() 
      const CSeq_feat* GetCDFeatFromProtFeat(const CSeq_feat& prot);
      string SeqDescLabelContent(const CSeqdesc& sd);
      string GetDiscItemText(const CSeq_feat& obj);
      string GetDiscItemText(const CSeq_submit& seq_submit);
      string GetDiscItemText(const CBioseq& obj);
      string GetDiscItemTextForBioseqSet(const CBioseq_set& obj);
      string GetDiscItemText(const CBioseq_set& obj);
      string GetDiscItemText(const CSeqdesc& obj, const CSeq_entry& seq_entry);
      string GetDiscItemText(const CSeqdesc& obj, const CBioseq& bioseq);
      string GetDiscItemText(const CPerson_id& obj, const CSeq_entry& seq_entry);
      string GetDiscItemText(const CSeq_entry& seq_entry);

      CBioseq* GetRepresentativeBioseqFromBioseqSet(const CBioseq_set& bioseq_set);

      string ListAuthNames(const CAuth_list& auths);
      string ListAllAuths(const CPubdesc& pubdesc);

      static const CSeq_id& BioseqToBestSeqId(const CBioseq& bioseq, CSeq_id::E_Choice);
      string BioseqToBestSeqIdString(const CBioseq& bioseq, CSeq_id::E_Choice);
      static string PrintSeqInt(const CSeq_interval& seq_int, bool range_only = false);
      static string SeqLocPrintUseBestID(const CSeq_loc& seq_loc, bool range_only = false);
      string GetLocusTagForFeature(const CSeq_feat& seq_feat);
      string GetProdNmForCD(const CSeq_feat& cd_feat);

      static bool IsmRNASequenceInGenProdSet(const CBioseq& bioseq);
      static bool IsProdBiomol(const CBioseq& bioseq);

      void RmvChar (string& in_out_str, string rm_chars);

      static vector <const CSeq_feat*> mix_feat, gene_feat, cd_feat, rna_feat, prot_feat;
      static vector <const CSeq_feat*> pub_feat, biosrc_feat, biosrc_orgmod_feat;
      static vector <const CSeq_feat*> rbs_feat, biosrc_subsrc_feat, repeat_region_feat;
      static vector <const CSeq_feat*> D_loop_feat, rna_not_mrna_feat, intron_feat;
      static vector <const CSeq_feat*> all_feat, non_prot_feat, rrna_feat, miscfeat_feat;
      static vector <const CSeq_feat*> otherRna_feat, org_orgmod_feat, gap_feat;
      static vector <const CSeq_feat*> utr3_feat, utr5_feat, exon_feat, promoter_feat;
      static vector <const CSeq_feat*> mrna_feat, trna_feat, bioseq_biosrc_feat;

      static vector <const CSeqdesc*>  pub_seqdesc,comm_seqdesc, biosrc_seqdesc;
      static vector <const CSeqdesc*>  title_seqdesc, biosrc_orgmod_seqdesc;
      static vector <const CSeqdesc*>  user_seqdesc, org_orgmod_seqdesc;
      static vector <const CSeqdesc*>  molinfo_seqdesc, biosrc_subsrc_seqdesc;
      static vector <const CSeqdesc*>  bioseq_biosrc_seqdesc, bioseq_molinfo;
      static vector <const CSeqdesc*>  bioseq_title, bioseq_user, bioseq_genbank;

      static vector <const CSeq_entry*> pub_seqdesc_seqentry, comm_seqdesc_seqentry;
      static vector <const CSeq_entry*> biosrc_seqdesc_seqentry, title_seqdesc_seqentry;
      static vector <const CSeq_entry*> biosrc_orgmod_seqdesc_seqentry;
      static vector <const CSeq_entry*> user_seqdesc_seqentry;
      static vector <const CSeq_entry*> biosrc_subsrc_seqdesc_seqentry;
      static vector <const CSeq_entry*> molinfo_seqdesc_seqentry;
      static vector <const CSeq_entry*> org_orgmod_seqdesc_seqentry;

      static string FindReplaceString(const string& src, const string& search_str,
          const string& replacement_str, bool case_sensitive=true, bool whole_word=true);

      static bool DoesStringContainPhrase(const string& str, const string& phrase, 
                            bool case_sensitive=true, bool whole_word=true);
      static void GetSeqFeatLabel(const CSeq_feat& seq_feat, string& label);      
      static string GetSrcQualValue(const CBioSource& biosrc, const string& qual_name,
                              bool is_subsrc = false, const CString_constraint* str_cons = 0);
      static CConstRef <CSeq_feat> GetGeneForFeature(const CSeq_feat& seq_feat);

    protected:
      string x_GetUserObjType(const CUser_object& user_obj);
      bool CommentHasPhrase(string comment, const string& phrase);
      bool HasLineage(const CBioSource& biosrc, const string& type);
      bool IsBiosrcEukaryotic(const CBioSource& biosrc);
      bool IsBioseqHasLineage(const CBioseq& bioseq, const string& type, bool has_biosrc=true);
      bool HasTaxonomyID(const CBioSource& biosec);
      void GetProperCItem(CRef <CClickableItem>& c_item, bool* citem1_used);
      void AddSubcategory(CRef <CClickableItem>& c_item, const string& setting_name, 
           const vector <string>* itemlist, const string& desc1, const string& desc2, 
           ECommentTp comm=e_IsComment, bool copy2parent = true, const string& desc3="",
           bool halfsize = false, unsigned input_cnt = 0);
      void GetTestItemList(const vector <string>& itemlist, Str2Strs& setting2list, 
                                                                   const string& delim = "$");
      void RmvRedundancy(vector <string>& item_list); //all CSeqEntry_Feat_desc tests need this
      bool DoesStringContainPhrase(const string& str, const vector <string>& phrases, 
                            bool case_sensitive=true, bool whole_word=true);

      static string Get1OrgModValue(const CBioSource& biosrc, const string& type_name);
      static void GetOrgModValues(const COrg_ref& org, COrgMod::ESubtype subtype, 
                                                              vector <string>& strs);
      static void GetOrgModValues(const CBioSource& biosrc, const string& type_name, 
                                                              vector <string>& strs);
      static void GetOrgModValues(const CBioSource& biosrc, COrgMod::ESubtype subtype,
                                                              vector <string>& strs);
      bool IsOrgModPresent(const CBioSource& biosrc, COrgMod::ESubtype subtype);
      static string Get1SubSrcValue(const CBioSource& biosrc, const string& type_name);
      static void GetSubSrcValues(const CBioSource& biosrc, CSubSource::ESubtype subtype,
                                                              vector <string>& strs);
      static void GetSubSrcValues(const CBioSource& biosrc, const string& type_name,
                                                              vector <string>& strs);
      bool IsSubSrcPresent(const CBioSource& biosrc, CSubSource::ESubtype subtype);
      bool AllVecElesSame(const vector <string> arr);
  };


// CBioseqSet_
  class CBioseqSetTestAndRepData : public CTestAndRepData
  {
     public:
       virtual ~CBioseqSetTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) = 0;
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) {};

       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;
  };


  class CBioseqSet_on_class : public CBioseqSetTestAndRepData
  {
    public:
      CBioseqSet_on_class () 
             { m_has_rearranged = m_has_sat_feat = m_has_non_sat_feat = false;};
      virtual ~CBioseqSet_on_class () {};

      virtual void TestOnObj(const CBioseq_set& bioseq_set);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
   
    protected:
      bool m_has_rearranged, m_has_sat_feat, m_has_non_sat_feat;

      string GetName_nonwgs() const {return string("DISC_NONWGS_SETS_PRESENT"); }
      string GetName_segset() const {return string("DISC_SEGSETS_PRESENT"); }
      string GetName_wrap() const {return string("TEST_UNWANTED_SET_WRAPPER"); }

      void FindUnwantedSetWrappers (const CBioseq_set& set);
      bool IsMicrosatelliteRepeatRegion (const CSeq_feat& seq_feat);
  };

  class CBioseqSet_TEST_UNWANTED_SET_WRAPPER : public CBioseqSet_on_class
  {
    public:
      virtual ~CBioseqSet_TEST_UNWANTED_SET_WRAPPER () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string(CBioseqSet_on_class::GetName_wrap()); }
  };

  class CBioseqSet_DISC_NONWGS_SETS_PRESENT : public CBioseqSet_on_class
  {
    public:
      virtual ~CBioseqSet_DISC_NONWGS_SETS_PRESENT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string(CBioseqSet_on_class::GetName_nonwgs()); }
  };

  class CBioseqSet_DISC_SEGSETS_PRESENT : public CBioseqSet_on_class
  {
    public:
      virtual ~CBioseqSet_DISC_SEGSETS_PRESENT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string(CBioseqSet_on_class::GetName_segset()); }
  };



  // CSeqEntry
  class CSeqEntryTestAndRepData : public CTestAndRepData
  {
     public:
       CSeqEntryTestAndRepData () {};
       virtual ~CSeqEntryTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) = 0;
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) {};

       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;

     protected:
       void AddBioseqsOfSetToReport(const CBioseq_set& bioseq_set, 
                    const string& setting_name, bool be_na = true, bool be_aa = true);
       void AddBioseqsInSeqentryToReport(const CSeq_entry* seq_entry, 
                        const string& setting_name, bool be_na = true, bool be_aa=true);

//       void TestOnBiosrc(const CSeq_entry& seq_entry);
       string GetName_multi() const {return string("ONCALLER_MULTISRC"); }
       string GetName_iso() const {
                             return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"); }
       bool IsBacterialIsolate(const CBioSource& biosrc);
       bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& biosrc);
  }; // CSeqEntryTest


  enum eMultiQual {
        e_no_multi = 0,
        e_same,
        e_dup,
        e_all_dif
  };

//  new comb!!
  class CSeqEntry_on_incnst_user : public CSeqEntryTestAndRepData
  {
    public:
      CSeqEntry_on_incnst_user () { m_prefix_set.clear(); 
                                     m_seq_has_dblink.clear();
                                     m_seq2prefix.clear();
                                     m_seq_no_dblink.clear();
                                  };
      virtual ~CSeqEntry_on_incnst_user () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      set <string> m_prefix_set, m_seq_has_dblink, m_seq_no_dblink;
      Str2Str m_seq2prefix;

      string GetName_comm() const {
                      return string("DISC_INCONSISTENT_STRUCTURED_COMMENTS");}
      string GetName_db() const { return string("DISC_INCONSISTENT_DBLINK");}

      void x_MakeMissingList(CConstRef <CBioseq> bsq_ref, const string& bsq_desc, 
                                       const string& setting_name, Str2Strs& key2ls);
      void x_ClassifyFlds(Str2Strs& key2ls, const string& key, const CSeqdesc& sd, 
                                          const string& setting_name, const CBioseq& bsq);
 
      void x_GetIncnstTestReport (CRef <CClickableItem>& c_item, 
               const string& setting_name, const string& title, const string& item_type);
      string x_GetStrCommPrefix(const CUser_object& user_obj);
      bool x_HasUserObjOfType(CSeqdesc_CI uci, const string& type);
  };

  class CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS : public CSeqEntry_on_incnst_user
  {
    public:
      virtual ~CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS () {};

      virtual string GetName() const { return CSeqEntry_on_incnst_user::GetName_comm();}
      virtual void GetReport(CRef <CClickableItem>& c_item)
      {
          x_GetIncnstTestReport(c_item,GetName(), "Structured Comment Report",
                                                               "structured comment");
      };
  };

  class CSeqEntry_DISC_INCONSISTENT_DBLINK  : public CSeqEntry_on_incnst_user
  {
    public:
      virtual ~CSeqEntry_DISC_INCONSISTENT_DBLINK () {};

      virtual string GetName() const { return CSeqEntry_on_incnst_user::GetName_db();}
      virtual void GetReport(CRef <CClickableItem>& c_item)
      {
          x_GetIncnstTestReport(c_item,GetName(), "DBLink Report", "");
      };
  };

  class CSeqEntry_on_biosrc_subsrc :  public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_on_biosrc_subsrc () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      string GetName_col() const {return string("ONCALLER_MORE_NAMES_COLLECTED_BY"); }
      string GetName_orgi() const {return string("ONCALLER_SUSPECTED_ORG_IDENTIFIED"); }
      string GetName_orgc() const {return string("ONCALLER_SUSPECTED_ORG_COLLECTED"); }
      string GetName_end() const {return string("END_COLON_IN_COUNTRY"); }
      string GetName_uncul() const {return string("UNCULTURED_NOTES_ONCALLER");}

      bool m_run_col, m_run_orgi, m_run_orgc, m_run_end, m_run_uncul;

      bool x_HasUnculturedNotes(const string& str, const char** notes, unsigned sz);
      bool Has3Names(const vector <string> arr);
      void RunTests(const CBioSource& biosrc, const string& desc);
  };

  class CSeqEntry_UNCULTURED_NOTES_ONCALLER : public CSeqEntry_on_biosrc_subsrc
  {
    public:
      virtual ~CSeqEntry_UNCULTURED_NOTES_ONCALLER () {};
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_biosrc_subsrc::GetName_uncul();}
  };

  class CSeqEntry_END_COLON_IN_COUNTRY : public CSeqEntry_on_biosrc_subsrc
  {
    public:
      virtual ~CSeqEntry_END_COLON_IN_COUNTRY () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_biosrc_subsrc::GetName_end();}
  };

  class CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED : public CSeqEntry_on_biosrc_subsrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_biosrc_subsrc::GetName_orgc();}
  };

  class CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED : public CSeqEntry_on_biosrc_subsrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_biosrc_subsrc::GetName_orgi();}
  };

  class CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY : public CSeqEntry_on_biosrc_subsrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_on_biosrc_subsrc::GetName_col();}
  };

  class CSeqEntry_test_on_pub : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_pub () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

      static string GetAuthNameList(const CAuthor& auth, bool use_initials = false);

    protected:
      enum E_Status {
        e_any = 0,
        e_published,
        e_unpublished,
        e_in_press,
        e_submitter_block
      };

      bool m_has_cit, m_run_dup, m_run_cap, m_run_usa, m_run_tlt, m_run_aff, m_run_unp;
      bool m_run_noaff, m_run_cons, m_run_missing;
      string GetName_dup() const {return string("DISC_CITSUB_AFFIL_DUP_TEXT");}
      string GetName_cap() const {return string("DISC_CHECK_AUTH_CAPS"); }
      string GetName_usa() const {return string("DISC_USA_STATE"); }
      string GetName_tlt() const {return string("DISC_TITLE_AUTHOR_CONFLICT"); }
      string GetName_aff() const {return string("DISC_CITSUBAFFIL_CONFLICT"); }
      string GetName_unp() const {return string("DISC_UNPUB_PUB_WITHOUT_TITLE"); }
      string GetName_noaff() const {return string("DISC_MISSING_AFFIL"); }
      string GetName_cons() const {return string("ONCALLER_CONSORTIUM"); }
      string GetName_missing() const {return string("DISC_CHECK_AUTH_NAME"); }

      bool AuthorHasConsortium(const CAuthor& author);
      bool AuthListHasConsortium(const CAuth_list& auth_ls);
      bool PubHasConsortium(const list <CRef <CPub> >& pubs);
      bool IsMissingAffil(const CAffil& affil);
      E_Status GetPubMLStatus (const CPub& pub);
      E_Status ImpStatus(const CImprint& imp, bool is_pub_sub = false); 
      string Get1stTitle(const CTitle::C_E& title);
      string GetTitleFromPub(const CPub& pub);
      bool DoesPubdescContainUnpubPubWithoutTitle(const list <CRef <CPub> >& pubs);
      void GetGroupedAffilString(const CAuth_list& authors, string& affil_str,string& grp_str);
      void RunTests(const list <CRef <CPub> >& pubs, const string& desc);
      CConstRef <CCit_sub> CitSubFromPubEquiv(const list <CRef <CPub> >& pubs);
      bool AffilStreetContainsDuplicateText(const CAffil& affil);
      bool AffilStreetEndsWith(const string& street, const string& end_str);
      bool IsNameCapitalizationOk(const string& name);
      bool IsAuthorInitialsCapitalizationOk(const string& nm_init);
      bool NameIsBad(const CRef <CAuthor>& auth_nm);
      bool HasBadAuthorName(const CAuth_list& auths);
      void CheckBadAuthCapsOrNoFirstLastNamesInPubdesc(const list <CRef <CPub> >& pubs, 
                                                                       const string& desc);
      bool AuthNoFirstLastNames(const CAuth_list& auths);
      bool CorrectUSAStates(CConstRef <CCit_sub>& cit_sub);
      void CheckTitleAndAuths(CConstRef <CCit_sub>& cit_sub, const string& desc);
  };

  class CSeqEntry_DISC_CHECK_AUTH_NAME :  public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_CHECK_AUTH_NAME () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_missing();}
  };

  class CSeqEntry_ONCALLER_CONSORTIUM :  public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_ONCALLER_CONSORTIUM () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_cons();}
  };

  class CSeqEntry_DISC_MISSING_AFFIL : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_MISSING_AFFIL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_noaff();}
  };



  class CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_unp();}
  };


  class CSeqEntry_DISC_CITSUBAFFIL_CONFLICT : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_CITSUBAFFIL_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_aff();}
  };


  class CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_tlt();}
  };


  class CSeqEntry_DISC_USA_STATE : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_USA_STATE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_usa();}
  };


  class CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_dup();}
  };


  class CSeqEntry_DISC_CHECK_AUTH_CAPS : public CSeqEntry_test_on_pub
  {
    public:
      virtual ~CSeqEntry_DISC_CHECK_AUTH_CAPS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_pub::GetName_cap();}
  };


  class CSeqEntry_test_on_defline : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_defline () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

   protected:
      set <string> m_aa_bioseqs;

      string GetName_set() const {return string("ONCALLER_DEFLINE_ON_SET");}
      string GetName_dup() const {return string("DISC_DUP_DEFLINE"); }
      string GetName_no_tlt() const {return string("DISC_MISSING_DEFLINES"); }
      string GetName_seqch() const {return string("DISC_TITLE_ENDS_WITH_SEQUENCE"); }
  };

  class CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE : public CSeqEntry_test_on_defline
  {
    public:
      virtual ~CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_defline::GetName_seqch();}
  };

  class CSeqEntry_DISC_MISSING_DEFLINES : public CSeqEntry_test_on_defline
  {
    public:
      virtual ~CSeqEntry_DISC_MISSING_DEFLINES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_defline::GetName_no_tlt();}
  };

  
  class CSeqEntry_DISC_DUP_DEFLINE : public CSeqEntry_test_on_defline
  {
    public:
      virtual ~CSeqEntry_DISC_DUP_DEFLINE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_defline::GetName_dup();}
  };


  class CSeqEntry_ONCALLER_DEFLINE_ON_SET : public CSeqEntry_test_on_defline
  {
    public:
      virtual ~CSeqEntry_ONCALLER_DEFLINE_ON_SET () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_defline::GetName_set();}
  };
 

  class CSeqEntry_test_on_tax_cflts : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_tax_cflts () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

   protected:
      string GetName_vou() const {return string("DISC_SPECVOUCHER_TAXNAME_MISMATCH"); }
      string GetName_str() const {return string("DISC_STRAIN_TAXNAME_MISMATCH"); }
      string GetName_cul() const {return string("DISC_CULTURE_TAXNAME_MISMATCH"); }
      string GetName_biom() const {return string("DISC_BIOMATERIAL_TAXNAME_MISMATCH"); }

      bool m_run_vou, m_run_str, m_run_cul, m_run_biom;
 
      bool s_StringHasVoucherSN(const string& vou_nm);
      void RunTests(const CBioSource& biosrc, const string& desc);
      void GetReport_cflts(CRef <CClickableItem>& c_item, const string& setting_name, 
                                                              const string& qual_nm);
  };

  class CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH : public CSeqEntry_test_on_tax_cflts
  {
    public:
      virtual ~CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH () {};

      virtual string GetName() const { return CSeqEntry_test_on_tax_cflts::GetName_biom();}
      virtual void GetReport(CRef <CClickableItem>& c_item) {
          GetReport_cflts(c_item, GetName(), "biomaterial");
      };
  }; 

  class CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH : public CSeqEntry_test_on_tax_cflts
  {
    public:
      virtual ~CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH () {};

      virtual string GetName() const { return CSeqEntry_test_on_tax_cflts::GetName_cul();}
      virtual void GetReport(CRef <CClickableItem>& c_item) {
          GetReport_cflts(c_item, GetName(), "culture collection");
      };
  };


  class CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH : public CSeqEntry_test_on_tax_cflts
  {
    public:
      virtual ~CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH () {};

      virtual string GetName() const { return CSeqEntry_test_on_tax_cflts::GetName_str();}
      virtual void GetReport(CRef <CClickableItem>& c_item) {
          GetReport_cflts(c_item, GetName(), "strain");
      };
  };


  class CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH : public CSeqEntry_test_on_tax_cflts
  {
    public:
      virtual ~CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH () {};

      virtual string GetName() const { return CSeqEntry_test_on_tax_cflts::GetName_vou();}
      virtual void GetReport(CRef <CClickableItem>& c_item) {
          GetReport_cflts(c_item, GetName(), "specimen voucher");
      };
  };


  class CSeqEntry_test_on_quals : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_quals () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      string GetName_asn1() const {return string("DISC_SOURCE_QUALS_ASNDISC"); }
      string GetName_asn1_oncall() const {
                                      return string("DISC_SOURCE_QUALS_ASNDISC_oncaller"); }
      string GetName_bad() const {return string("DISC_SRC_QUAL_PROBLEM"); }

      void GetQualDistribute(Str2Ints& qual2src_idx, const vector <string>& desc_ls, 
             const vector <CConstRef <CBioSource> >& src_ls, const string& setting_name,
             unsigned pre_cnt, unsigned tot_cnt);
      void GetReport_quals(CRef <CClickableItem>& c_item, const string& setting_name);
      bool GetQual2SrcIdx(const vector <CConstRef <CBioSource> >& src_ls, 
              const vector <string>& desc_ls, Str2Ints& qual2src_idx, unsigned pre_cnt, 
              unsigned tot_cnt);
      void GetMultiSubSrcVlus(const CBioSource& biosrc, const string& type_name,
             vector <string>& multi_vlus);
      void GetMultiOrgModVlus(const CBioSource& biosrc, const string& type_name,
                                                                  vector <string>& multi_vlus);
      void GetMultiPrimerVlus(const CBioSource& biosrc, const string& qual_name,
                                                                  vector <string>& multi_vlus);
      CRef <CClickableItem> MultiItem(const string& qual_name,
        const vector <string>& multi_list, const string& ext_desc, const string& setting_name);
      void CheckForMultiQual(const string& qual_name, const CBioSource& biosrc,
                                                      eMultiQual& multi_type, bool is_subsrc);
      void GetMultiQualVlus(const string& qual_name, const CBioSource& biosrc,
                                                  vector <string>& multi_vlus, bool is_subsrc);
  };


  class CSeqEntry_DISC_SRC_QUAL_PROBLEM : public CSeqEntry_test_on_quals
  {
    public:
      virtual ~CSeqEntry_DISC_SRC_QUAL_PROBLEM () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_quals::GetName_bad();}
  };


  class CSeqEntry_DISC_SOURCE_QUALS_ASNDISC : public CSeqEntry_test_on_quals
  {
    public:
      virtual ~CSeqEntry_DISC_SOURCE_QUALS_ASNDISC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_quals::GetName_asn1();}
  };


  class CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller : public CSeqEntry_test_on_quals
  {
    public:
      virtual ~CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_quals::GetName_asn1_oncall();}
  };


  class CSeqEntry_test_on_biosrc_orgmod : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_biosrc_orgmod () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;
 
    protected:
      string GetName_mism() const {return string("DISC_BACTERIAL_TAX_STRAIN_MISMATCH");}
      string GetName_cul() const {return 
                                    string("ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH");}
      string GetName_cbs() const {return string("DUP_DISC_CBS_CULTURE_CONFLICT");}
      string GetName_atcc() const {return string("DUP_DISC_ATCC_CULTURE_CONFLICT");}
      string GetName_sp_strain() const {return string("DISC_BACTERIA_MISSING_STRAIN");}
      string GetName_meta() const {return string("DISC_METAGENOME_SOURCE");}
      string GetName_auth() const {return string("ONCALLER_CHECK_AUTHORITY");}
      string GetName_mcul() const {return string("ONCALLER_MULTIPLE_CULTURE_COLLECTION");}
      string GetName_human() const {return string("DISC_HUMAN_HOST");}
      string GetName_env() const {return string("TEST_UNNECESSARY_ENVIRONMENTAL");}
      string GetName_amp() const {
                           return string("TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE");}

      bool m_run_mism, m_run_cul, m_run_cbs, m_run_atcc, m_run_sp, m_run_meta;
      bool m_run_auth, m_run_mcul, m_run_human, m_run_env, m_run_amp;

      bool AmpPrimersNoEnvSample(const CBioSource& biosrc);
      bool HasUnnecessaryEnvironmental(const CBioSource& biosrc);
      bool HasMultipleCultureCollection (const CBioSource& biosrc, string& cul_vlus);
      bool DoAuthorityAndTaxnameConflict(const CBioSource& biosrc);
      bool HasMissingBacteriaStrain(const CBioSource& biosrc);
      bool IsMissingRequiredStrain(const CBioSource& biosrc);
      bool IsStrainInCultureCollectionForBioSource(const CBioSource& biosrc, 
                        const string& strain_head, const string& culture_head);
      void RunTests(const CBioSource& biosrc, const string& desc);
      void BiosrcHasConflictingStrainAndCultureCollectionValues(const CBioSource& biosrc, 
                                                                         const string& desc);
      void BacterialTaxShouldEndWithStrain(const CBioSource& biosrc, const string& desc);

      bool HasConflict(const list <CRef <COrgMod> >& mods, const string& subname_rest, 
                               const COrgMod::ESubtype& check_type, const string& check_head);
  };

  class CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE 
                                              : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_amp();}
  };

  class CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_env();}
  };

  class CSeqEntry_DISC_HUMAN_HOST : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DISC_HUMAN_HOST () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_human();}
  };


  class CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_mcul();}
  };


  class CSeqEntry_ONCALLER_CHECK_AUTHORITY : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_ONCALLER_CHECK_AUTHORITY () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_auth();}
  };


  class CSeqEntry_DISC_METAGENOME_SOURCE : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DISC_METAGENOME_SOURCE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc_orgmod::GetName_meta();}
  };


  class CSeqEntry_DISC_BACTERIA_MISSING_STRAIN : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DISC_BACTERIA_MISSING_STRAIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { 
                            return CSeqEntry_test_on_biosrc_orgmod::GetName_sp_strain();}
  };

  class CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_atcc();}
  };

 
  class CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_cbs();}
  };

  
  class CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_mism();}
  }; 

 
  class CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_cul();}
  };



  class CSeqEntry_test_on_user : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_user () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      Str2Obj m_seqs_w_pid, m_seqs_no_pid;
      string GetName_gcomm() const {return string("MISSING_GENOMEASSEMBLY_COMMENTS");}
      string GetName_proj() const {return string("TEST_HAS_PROJECT_ID"); }
      string GetName_oncall_scomm() const {
                        return string("ONCALLER_MISSING_STRUCTURED_COMMENTS"); }
      string GetName_scomm() const {return string("MISSING_STRUCTURED_COMMENT"); }
      string GetName_mproj() const {return string("MISSING_PROJECT"); }
      string GetName_bproj() const {return string("ONCALLER_BIOPROJECT_ID"); }
      string GetName_prefix() const {
                       return string("ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX"); }

      void GroupAllBioseqs(const CBioseq_set& bioseq_set, const int& id);
      void CheckCommentCountForSet(const CBioseq_set& set, const unsigned& cnt, 
                                                                        Str2Int& bioseq2cnt);
      Str2Obj m_bioseq2geno_comm;
      const CBioseq& Get1stBioseqOfSet(const CBioseq_set& bioseq_set);
//      void AddBioseqsOfSet2Map(const CBioseq_set& bioseq_set);
 //     void RmvBioseqsOfSetOutMap(const CBioseq_set& bioseq_set);
  };

  class CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX:public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_prefix();}
  };

  class CSeqEntry_MISSING_STRUCTURED_COMMENT : public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_MISSING_STRUCTURED_COMMENT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_scomm();}
  };

  class CSeqEntry_ONCALLER_BIOPROJECT_ID : public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_ONCALLER_BIOPROJECT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_bproj();}
  };


  class CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS : public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_gcomm();}
  };

  class CSeqEntry_TEST_HAS_PROJECT_ID : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_TEST_HAS_PROJECT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_user::GetName_proj();}
  };


  class CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_oncall_scomm();}
  };


  class CSeqEntry_MISSING_PROJECT : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_MISSING_PROJECT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_mproj();}
  };



  class CSeqEntry_test_on_biosrc : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_biosrc () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      vector <string> m_submit_text;
      bool m_run_trin, m_run_iso, m_run_mult, m_run_tmiss, m_run_tbad, m_run_flu, m_run_quals;
      bool m_run_iden, m_run_col, m_run_div, m_run_map, m_run_clone, m_run_meta, m_run_sp;
      bool m_run_prim, m_run_cty, m_run_pcr, m_run_strain;

      string GetName_trin() const {
                      return string("DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"); }
      string GetName_iso() const {
                        return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"); }
      string GetName_mult() const {return string("ONCALLER_MULTISRC"); }
      string GetName_tmiss() const {return string("TAX_LOOKUP_MISSING"); }
      string GetName_tbad () const {return string("TAX_LOOKUP_MISMATCH"); }
      string GetName_flu() const {return string("DISC_INFLUENZA_DATE_MISMATCH"); }
      string GetName_quals() const {return string("DISC_MISSING_VIRAL_QUALS"); }
      string GetName_iden() const {return string("MORE_OR_SPEC_NAMES_IDENTIFIED_BY"); }
      string GetName_col() const {return string("MORE_NAMES_COLLECTED_BY"); }
      string GetName_div() const {return string("DIVISION_CODE_CONFLICTS");}
      string GetName_map() const {return string("DISC_MAP_CHROMOSOME_CONFLICT");}
      string GetName_clone() const {return string("DISC_REQUIRED_CLONE");}
      string GetName_meta() const {return string("DISC_METAGENOMIC");}
      string GetName_sp() const {return string("TEST_SP_NOT_UNCULTURED");}
      string GetName_prim() const {return string("TEST_MISSING_PRIMER");}
      string GetName_cty() const {return string("ONCALLER_COUNTRY_COLON");}
      string GetName_pcr() const {return string("ONCALLER_DUPLICATE_PRIMER_SET");}
      string GetName_strain() const {return string("DISC_REQUIRED_STRAIN"); }

      bool x_IsMissingRequiredStrain(const CBioSource& biosrc);
      void IniMap(const list <CRef <CPCRPrimer> >& ls, Str2Int& map);
      bool SamePrimerList(const list <CRef <CPCRPrimer> >& ls1, 
                                               const list <CRef <CPCRPrimer> >& ls2);
      bool SamePCRReaction(const CPCRReaction& pcr1, const CPCRReaction& pcr2);
      bool MissingPrimerValue(const CBioSource& biosrc);
      void IsFwdRevDataPresent(const CRef <CPCRPrimer>& primer, 
                                                  bool& has_seq, bool& has_name);
      bool FindTrinomialWithoutQualifier(const CBioSource& biosrc);
      bool IsMissingRequiredClone (const CBioSource& biosrc);
      void AddEukaryoticBioseqsToReport(const CBioseq_set& set);
      bool HasMulSrc(const CBioSource& biosrc);
      bool IsBacterialIsolate(const CBioSource& biosrc);
      bool DoTaxonIdsMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool DoInfluenzaStrainAndCollectionDateMisMatch(const CBioSource& biosrc);
      void AddMissingViralQualsDiscrepancies(const CBioSource& biosrc, 
                                                            const string& desc);
      bool HasMoreOrSpecNames(const CBioSource& biosrc, CSubSource::ESubtype subtype, 
                                                         bool check_mul_nm_only = false);
      void GetSubmitText(const CAuth_list& authors);
      void FindSpecSubmitText();

      void RunTests(const CBioSource& biosrc, const string& desc, int idx = -1);
  };

  class CSeqEntry_DISC_REQUIRED_STRAIN : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_REQUIRED_STRAIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_strain();}
  };

  class CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_pcr();}
  };

  class CSeqEntry_ONCALLER_COUNTRY_COLON : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_COUNTRY_COLON () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_cty();}
  };

  class CSeqEntry_TEST_MISSING_PRIMER : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TEST_MISSING_PRIMER () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_prim();}
  };

  class CSeqEntry_TEST_SP_NOT_UNCULTURED : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TEST_SP_NOT_UNCULTURED () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_sp();}
  };

  class CSeqEntry_DISC_METAGENOMIC : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_METAGENOMIC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_meta();}
  };

  class CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_trin();}
  };

  class CSeqEntry_DISC_REQUIRED_CLONE : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_REQUIRED_CLONE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_clone();}
  };

  class CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT :  public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_map();}
  };

  class CSeqEntry_DIVISION_CODE_CONFLICTS :  public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DIVISION_CODE_CONFLICTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_div();}
  };

  class CSeqEntry_MORE_NAMES_COLLECTED_BY : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_MORE_NAMES_COLLECTED_BY () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_col();}
  };

  class CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_iden();}
  };

  class CSeqEntry_DISC_MISSING_VIRAL_QUALS : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_MISSING_VIRAL_QUALS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_quals();}
  };

  class CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_flu();}
  };

  class CSeqEntry_TAX_LOOKUP_MISSING : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TAX_LOOKUP_MISSING () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_tmiss();}
  };


  class CSeqEntry_TAX_LOOKUP_MISMATCH : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TAX_LOOKUP_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_tbad();}
  };


  class CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_iso();}
  };


  class CSeqEntry_ONCALLER_MULTISRC : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MULTISRC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_mult();}
  };
// new comb


/*
  class CSeqEntry_DISC_SUSPECT_RRNA_PRODUCTS : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_SUSPECT_RRNA_PRODUCTS () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SUSPECT_RRNA_PRODUCTS");}
  };
*/

  class CFlatfileTextFind: public CFlatFileConfig::CGenbankBlockCallback, CTestAndRepData
  {
    public:
       CFlatfileTextFind ( const string& setting_name) {
              m_seqdesc_sel.push_back(CSeqdesc::e_Source);
              m_seqdesc_sel.push_back(CSeqdesc::e_Molinfo);
              m_seqdesc_sel.push_back(CSeqdesc::e_Title);
              m_seqdesc_sel.push_back(CSeqdesc::e_Genbank);
              m_seqdesc_sel.push_back(CSeqdesc::e_Pub);
              m_setting_name = setting_name;
       }
       virtual ~CFlatfileTextFind () { };
    
       virtual EBioseqSkip notify_bioseq(CBioseqContext& ctx);

       virtual EAction notify (string& block_text, const CBioseqContext& ctx,
                                              const CSourceItem& source_item);
       virtual EAction notify (string& block_text, const CBioseqContext& ctx,
                                              const CFeatureItem& feature_item);
       virtual EAction unified_notify( string & block_text, 
                                 const CBioseqContext& ctx, const IFlatItem & flat_item, 
                                 CFlatFileConfig::FGenbankBlocks which_block);
       string m_block_text;

      virtual void TestOnObj(const CBioseq_set& bioseq_set) { };
      virtual void TestOnObj(const CSeq_entry& seq_entry) { };
      virtual void TestOnObj(const CBioseq& bioseq) { };
      virtual void TestOnObj(const CSeq_feat& seq_feat) { };

      virtual void GetReport(CRef <CClickableItem>& c_item) { };

      virtual string GetName() const {return kEmptyStr ;}

    protected:
       string m_taxname, m_bioseq_desc, m_setting_name; 
       string m_src_desc, m_mol_desc, m_tlt_desc, m_pub_desc, m_gbk_desc;
       vector <CSeqdesc::E_Choice> m_seqdesc_sel;
  };

  class CSeqEntry_DISC_FLATFILE_FIND_ONCALLER : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_FLATFILE_FIND_ONCALLER () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FLATFILE_FIND_ONCALLER");}
    
    private:
      Str2Strs m_fixable2ls; 
      bool m_citem1;

      void AddCItemToReport(const string& ls_type, const string& setting_name, 
                                                      CRef <CClickableItem>& c_item);
      string GetName_nofix() const {
                         return string("DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE");}
      string GetName_fix() const {return string("DISC_FLATFILE_FIND_ONCALLER_FIXABLE");}
  };

  class CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ONCALLER_STRAIN_TAXNAME_CONFLICT");}

    private:
      bool StrainConflictsTaxname(const COrg_ref& org);
  };

  class CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("TEST_SMALL_GENOME_SET_PROBLEM");}

    private:
      bool HasSmallSeqset(const CSeq_entry& seq_entry);
  };


  class CSeqEntry_DISC_SUBMITBLOCK_CONFLICT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_SUBMITBLOCK_CONFLICT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_SUBMITBLOCK_CONFLICT");}

    private:
      bool CitSubMatchExceptDate(const CCit_sub& cit1, const CCit_sub& cit2);
      bool DateMatch(const CSubmit_block& blk1, const CSubmit_block& blk2);
      string SubmitBlockMatchExceptDate(const CSubmit_block& this_blk);
  };


  class CSeqEntry_DISC_INCONSISTENT_MOLTYPES : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_INCONSISTENT_MOLTYPES () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_INCONSISTENT_MOLTYPES");}

    private:
      unsigned m_entry_no;
      void AddMolinfoToBioseqsOfSet(const CBioseq_set& set, const string& desc);
  };


  class CSeqEntry_DISC_HAPLOTYPE_MISMATCH : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_HAPLOTYPE_MISMATCH () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_HAPLOTYPE_MISMATCH");}

    private:
      Str2Seqs m_tax_hap2seqs;
      unsigned m_entry_cnt;

      void ExtractNonAaBioseqsOfSet(const string& tax_hap, const CBioseq_set& set);
      bool SeqMatch(const CConstRef <CBioseq>& seq1, int beg1, const CConstRef <CBioseq>& seq2,
                       int beg2, unsigned& len, bool Ndiff = true);
      bool SeqsMatch(const vector <CConstRef <CBioseq> >& seqs, bool Ndiff = true);
      void ReportOneHaplotypeSequenceMismatch(Str2Seqs::const_iterator& iter, bool Ndiff=true);
      void ReportHaplotypeSequenceMismatchForList();
      void MakeCitem4DiffSeqs(CRef <CClickableItem>& c_item, 
                            const vector <string> tax_hap_seqs, bool Ndiff = true);
      void MakeCitem4SameSeqs(CRef <CClickableItem>& c_item, 
                            const vector <string>& idx_seqs, bool Ndiff = true);
  };


/*
  class CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_CITSUB_AFFIL_DUP_TEXT");}

    protected:
      CConstRef <CCit_sub> CitSubFromPubEquiv(const list <CRef <CPub> >& pubs);
      bool AffilStreetContainsDuplicateText(const CAffil& affil);       
      bool AffilStreetEndsWith(const string& street, const string& end_str);
  };
*/



/*
  class CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE : public CSeqEntryTestAndRepData
  {
      friend class CSeqEntryTestAndRepData;
    public:
      virtual ~CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {
                               return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE");}
      virtual void Check(const string& desc) {cerr << "check1 " << desc << endl;}
  };
*/


  class CSeqEntry_INCONSISTENT_BIOSOURCE : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_INCONSISTENT_BIOSOURCE () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_BIOSOURCE"); }
   
    private:
      bool SynonymsMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool DbtagMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool OrgModSetMatch(const COrgName& nm1, const COrgName& nm2);
      bool OrgNameMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool OrgRefMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool SubSourceSetMatch(const CBioSource& biosrc1, const CBioSource& biosrc2);
      bool BioSourceMatch( const CBioSource& biosrc1, const CBioSource& biosrc2);
      string DescribeOrgRefDifferences(const COrg_ref& org1, const COrg_ref& org2);
      string DescribeBioSourceDifferences(const CBioSource& biosrc1,const CBioSource& biosrc2);
      string DescribeOrgNameDifferences(const COrg_ref& org1, const COrg_ref& org2);
      string OrgModSetDifferences(const COrgName& nm1, const COrgName& nm2);
      CRef <GeneralDiscSubDt> AddSeqEntry(const CSeqdesc& seqdesc,const CSeq_entry& seq_entry,
                                          const CBioseq_CI& b_ci);
  };


  class CSeqEntry_DISC_FEATURE_COUNT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_FEATURE_COUNT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEATURE_COUNT_oncaller");}
  };


// new comb
  class CSeqEntry_on_comment : public CSeqEntryTestAndRepData
  {
    public:
      CSeqEntry_on_comment () { m_all_same = true; };
      virtual ~CSeqEntry_on_comment () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      bool m_all_same;
      string GetName_has() const {return string("ONCALLER_COMMENT_PRESENT");}
      string GetName_mix() const {return string("DISC_MISMATCHED_COMMENTS");}
  };


  class CSeqEntry_ONCALLER_COMMENT_PRESENT : public CSeqEntry_on_comment
  {
    public:
      virtual ~CSeqEntry_ONCALLER_COMMENT_PRESENT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_comment::GetName_has(); }
  };

  class CSeqEntry_DISC_MISMATCHED_COMMENTS  : public CSeqEntry_on_comment
  {
    public:
      virtual ~CSeqEntry_DISC_MISMATCHED_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_on_comment::GetName_mix(); }
  };  

// new comb


/*
  class CSeqEntry_DISC_CHECK_AUTH_CAPS : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_CHECK_AUTH_CAPS () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_CHECK_AUTH_CAPS");}

    protected:
      bool IsNameCapitalizationOk(const string& name);
      bool IsAuthorInitialsCapitalizationOk(const string& nm_init);
      bool NameIsBad(const CRef <CAuthor> nm_std);

      bool HasBadAuthorName(const CAuth_list& auths);
      bool AreBadAuthCapsInPubdesc(const CPubdesc& pubdesc);
      bool AreAuthCapsOkInSubmitBlock(const CSubmit_block& submit_block);
  };
*/



// class CBioseq_
  class CBioseqTestAndRepData : public CTestAndRepData
  {
     public: 
       virtual ~CBioseqTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) = 0;
       virtual void TestOnObj(const CSeq_feat& seq_feat) {}; 
         
       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;

       static string GetRNAProductString(const CSeq_feat& seq_feat);
    
    protected:
       bool x_IsShortrRNA(const CSeq_feat* seq_ft);
       bool BioseqHasKeyword(const CBioseq& bioseq, const string& keywd);
       bool StrandOk(ENa_strand strand1, ENa_strand strand2);
       bool IsUnknown(const string& known_items, const unsigned idx);
       bool IsPseudoSeqFeatOrXrefGene(const CSeq_feat* seq_feat);

       void TestOverlapping_ed_Feats(const vector <const CSeq_feat*>& feat, 
                     const string& setting_name, bool isGene = true, bool isOverlapped = true);

       void TestExtraMissingGenes(const CBioseq& bioseq);
       bool GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref);

       void TestProteinID(const CBioseq& bioseq);
       string GetName_pid () const {return string("MISSING_PROTEIN_ID"); }
       string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX"); }

       void TestOnBasesN(const CBioseq& bioseq);
       bool IsDeltaSeqWithFarpointers(const CBioseq& bioseq);
       void AddNsReport(CRef <CClickableItem>& c_item, bool is_n10=true);
       string GetName_n10() const {return string("N_RUNS"); }
       string GetName_n14() const {return string("N_RUNS_14"); }
       string GetName_0() const {return string("ZERO_BASECOUNT"); }
       string GetName_5perc() const {return string("DISC_PERCENT_N"); }
       string GetName_10perc() const {return string("DISC_10_PERCENTN"); }
       string GetName_non_nt() const {return string("TEST_UNUSUAL_NT"); }

       void TestOnMRna(const CBioseq& bioseq);
       string GetName_no_mrna() const {return string("DISC_CDS_WITHOUT_MRNA");}
       string GetName_pid_tid() const {
                             return string("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS");}
       string GetFieldValueForObject(const CSeq_feat& seq_feat, 
                                                           const CFeat_qual_choice& feat_qual);
       CSeqFeatData::ESubtype GetFeatdefFromFeatureType(EMacro_feature_type feat_field_type);
       CConstRef <CSeq_feat> GetmRNAforCDS(const CSeq_feat& cd_feat, 
                                                             const CSeq_entry& seq_entry);
       CConstRef <CProt_ref> GetProtRefForFeature(const CSeq_feat& seq_feat, 
                                                                        bool look_xref=true);
       bool IsLocationOrganelle(int genome);
       bool ProductsMatchForRefSeq(const string& feat_prod, const string& mRNA_prod);
       bool IsMrnaSequence();
       int DistanceToUpstreamGap(const unsigned& pos, const CBioseq& bioseq);
       int DistanceToDownstreamGap (const int& pos, const CBioseq& bioseq);
       bool HasUnculturedNonOrganelleName (const string& tax_nm);
  }; // CBioseqTest:


// new comb.
  class CBioseq_on_base : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_base() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_n10() const {return string("N_RUNS"); }
      string GetName_n14() const {return string("N_RUNS_14"); }
      string GetName_0() const {return string("ZERO_BASECOUNT"); }
      string GetName_5perc() const {return string("DISC_PERCENT_N"); }
      string GetName_10perc() const {return string("DISC_10_PERCENTN"); }
      string GetName_non_nt() const {return string("TEST_UNUSUAL_NT"); }

      bool x_IsDeltaSeqWithFarpointers(const CBioseq& bioseq);
      void x_AddNsReport(CRef <CClickableItem>& c_item, bool is_n10=true);
  };

  class CBioseq_N_RUNS : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_N_RUNS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_n10();}
  };

  class CBioseq_N_RUNS_14 : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_N_RUNS_14 () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_n14();}
  };

  class CBioseq_ZERO_BASECOUNT : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_ZERO_BASECOUNT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_0();}
  };

  class CBioseq_DISC_PERCENT_N : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_DISC_PERCENT_N () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_5perc();}
  };

  class CBioseq_DISC_10_PERCENTN : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_DISC_10_PERCENTN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_10perc();}
  };

  class CBioseq_TEST_UNUSUAL_NT : public CBioseq_on_base
  {
    public:
      virtual ~CBioseq_TEST_UNUSUAL_NT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_base::GetName_non_nt();}
  };

  class CBioseq_on_SUSPECT_RULE : public CBioseqTestAndRepData 
  {
    public:
      virtual ~CBioseq_on_SUSPECT_RULE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("SUSPECT_PRODUCT_NAMES");};

      static string GetName_typo() { return string("DISC_PRODUCT_NAME_TYPO");};
      static string GetName_qfix() { return string("DISC_PRODUCT_NAME_QUICKFIX");};
      static string GetName_name() { return string("SUSPECT_PRODUCT_NAMES");};

    protected:
      CBioseq_Handle m_bioseq_hl;
      Str2Strs m_feature_list;

      bool CategoryOkForBioSource(const CBioSource* biosrc_p, ESuspectNameType name_type);
      void FindSuspectProductNamesCallback();
      void FindSuspectProductNamesWithStaticList();
      void FindSuspectProductNamesWithRules();
      void GetReportForStaticList (CRef <CClickableItem>& c_item);
      void GetReportForRules (CRef <CClickableItem>& c_Item);
  };

  class CBioseq_on_tax_def :  public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_tax_def () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_inc() const {return string("INCONSISTENT_SOURCE_DEFLINE"); }
      string GetName_missing() const {return string("TEST_TAXNAME_NOT_IN_DEFLINE"); }
  };

  class CBioseq_INCONSISTENT_SOURCE_DEFLINE : public CBioseq_on_tax_def
  {
    public:
      virtual ~CBioseq_INCONSISTENT_SOURCE_DEFLINE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_tax_def::GetName_inc();}
  };

  class CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE : public CBioseq_on_tax_def
  {
    public:
      virtual ~CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_tax_def::GetName_missing();}
  };

  class CBioseq_on_Aa :  public CBioseqTestAndRepData
  {
    public:
      CBioseq_on_Aa () { m_e_exist.clear(); m_i_exist.clear(); m_check_eu_mrna = true;};
      virtual ~CBioseq_on_Aa () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      vector <unsigned> m_e_exist, m_i_exist;
      bool m_check_eu_mrna;

      string GetName_shtprt() const {return string("SHORT_PROT_SEQUENCES"); }
      string GetName_orgl() const {return string("TEST_ORGANELLE_NOT_GENOMIC"); }
      string GetName_vir() const {return string("TEST_UNNECESSARY_VIRUS_GENE"); }
      string GetName_rbs() const {return string("DISC_RBS_WITHOUT_GENE"); }
      string GetName_ei() const {return string("DISC_EXON_INTRON_CONFLICT"); }
      string GetName_pgene() const {return string("DISC_GENE_PARTIAL_CONFLICT"); }
      string GetName_contig() const {return string("SHORT_CONTIG"); }
      string GetName_200seq() const {return string("SHORT_SEQUENCES_200"); }
      string GetName_50seq() const {return string("SHORT_SEQUENCES"); }
      string GetName_dupg() const {return string("TEST_DUP_GENES_OPPOSITE_STRANDS"); }
      string GetName_unv() const {return string("TEST_COUNT_UNVERIFIED"); }
      string GetName_retro() const {return string("DISC_RETROVIRIDAE_DNA"); }
      string GetName_nonr() const {return string("NON_RETROVIRIDAE_PROVIRAL"); }
      string GetName_rnapro() const {return string("RNA_PROVIRAL"); }
      string GetName_eu_mrna() const {return string("EUKARYOTE_SHOULD_HAVE_MRNA"); }

      void CompareIntronExonList(const string& seq_id_desc, 
                                                  const vector <const CSeq_feat*>& exon_ls, 
                                                  const vector <const CSeq_feat*>& intron_ls);
      bool Is5EndInUTRList(const unsigned& start);
      bool Is3EndInUTRList(const unsigned& stop);
      void ReportPartialConflictsForFeatureType(vector <const CSeq_feat*>& seq_feats, 
                                                    string label, bool check_for_utrs = false);
      void  GetFeatureList4Gene(const CSeq_feat* gene, const vector <const CSeq_feat*> feats,
                                        vector <unsigned> exist_ls);
  };

  class CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_eu_mrna();}
  };

  class CBioseq_RNA_PROVIRAL : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_RNA_PROVIRAL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_rnapro();}
  };

  class CBioseq_DISC_RETROVIRIDAE_DNA : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_DISC_RETROVIRIDAE_DNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_retro();}
  };

  class CBioseq_NON_RETROVIRIDAE_PROVIRAL : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_NON_RETROVIRIDAE_PROVIRAL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_nonr();}
  };

  class CBioseq_TEST_COUNT_UNVERIFIED : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_TEST_COUNT_UNVERIFIED () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_unv();}
  };

  class CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS  : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_dupg();}
  };

  class CBioseq_SHORT_PROT_SEQUENCES : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_SHORT_PROT_SEQUENCES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_shtprt();}
  };


  class CBioseq_TEST_ORGANELLE_NOT_GENOMIC : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_TEST_ORGANELLE_NOT_GENOMIC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_orgl();}
  };


  class CBioseq_TEST_UNNECESSARY_VIRUS_GENE : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_TEST_UNNECESSARY_VIRUS_GENE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_vir();}
  };


  class CBioseq_DISC_RBS_WITHOUT_GENE : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_DISC_RBS_WITHOUT_GENE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_rbs();}
  };


  class CBioseq_DISC_EXON_INTRON_CONFLICT : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_DISC_EXON_INTRON_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_ei();}
  };


  class CBioseq_DISC_GENE_PARTIAL_CONFLICT : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_DISC_GENE_PARTIAL_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_pgene();}
  };


  class CBioseq_SHORT_CONTIG : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_SHORT_CONTIG () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_contig();}
  };


  class CBioseq_SHORT_SEQUENCES_200 : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_SHORT_SEQUENCES_200 () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_200seq();}
  };

 
  class CBioseq_SHORT_SEQUENCES : public CBioseq_on_Aa
  {
    public:
      virtual ~CBioseq_SHORT_SEQUENCES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_Aa::GetName_50seq();}
  };



  class CBioseq_on_mRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_mRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_exon() const {return string("TEST_EXON_ON_MRNA"); }
      string GetName_qual() const {return string("TEST_BAD_MRNA_QUAL"); }
      string GetName_str() const {return string("TEST_MRNA_SEQUENCE_MINUS_ST"); }
      string GetName_mcds() const {return string("MULTIPLE_CDS_ON_MRNA"); }
  };

  class CBioseq_MULTIPLE_CDS_ON_MRNA : public CBioseq_on_mRNA
  {
    public:
      virtual ~CBioseq_MULTIPLE_CDS_ON_MRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_mRNA::GetName_mcds();}
  };

  class CBioseq_TEST_MRNA_SEQUENCE_MINUS_ST : public CBioseq_on_mRNA
  {
    public:
      virtual ~CBioseq_TEST_MRNA_SEQUENCE_MINUS_ST () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_mRNA::GetName_str();}
  };

  class CBioseq_TEST_EXON_ON_MRNA : public CBioseq_on_mRNA
  {
    public:
      virtual ~CBioseq_TEST_EXON_ON_MRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_mRNA::GetName_exon();}
  };


  class CBioseq_TEST_BAD_MRNA_QUAL : public CBioseq_on_mRNA
  {
    public:
      virtual ~CBioseq_TEST_BAD_MRNA_QUAL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_mRNA::GetName_qual();}
  };


  class CBioseq_on_cd_feat :  public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_cd_feat () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_cdd() const {return string("TEST_CDS_HAS_CDD_XREF"); }
      string GetName_exc() const {return string("DISC_CDS_HAS_NEW_EXCEPTION"); }
  };


  class CBioseq_TEST_CDS_HAS_CDD_XREF : public CBioseq_on_cd_feat
  {
    public:
      virtual ~CBioseq_TEST_CDS_HAS_CDD_XREF () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_cd_feat::GetName_cdd();}
  };


  class CBioseq_DISC_CDS_HAS_NEW_EXCEPTION : public CBioseq_on_cd_feat
  {
    public:
      virtual ~CBioseq_DISC_CDS_HAS_NEW_EXCEPTION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_cd_feat::GetName_exc();}
  };


  class CBioseq_test_on_missing_genes : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_missing_genes () {};

      virtual void TestOnObj(const CBioseq& bioseq) = 0;
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      vector <int> m_super_idx;
      string m_no_genes;
      unsigned m_super_cnt;

      void CheckGenesForFeatureType(const vector <const CSeq_feat*>& feats, 
                                        bool makes_gene_not_superfluous = false);
      bool GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref);
  };


  class CBioseq_missing_genes_regular : public CBioseq_test_on_missing_genes
  {
    public:
      virtual ~CBioseq_missing_genes_regular () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_missing() const {return string("MISSING_GENES"); }
      string GetName_extra() const {return string("EXTRA_GENES"); } 
      bool x_HasPseudogeneQualifier (const CSeq_feat& sft);
  };

  
  class CBioseq_MISSING_GENES : public CBioseq_missing_genes_regular
  {
    public:
      virtual ~CBioseq_MISSING_GENES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_missing_genes_regular::GetName_missing();}
  };


  class CBioseq_EXTRA_GENES : public CBioseq_missing_genes_regular
  {
    public:
      virtual ~CBioseq_EXTRA_GENES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_missing_genes_regular::GetName_extra();}
  };


  class CBioseq_missing_genes_oncaller : public CBioseq_test_on_missing_genes
  {
    public:
      virtual ~CBioseq_missing_genes_oncaller () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_missing() const {return string("ONCALLER_GENE_MISSING"); }
      string GetName_extra() const {return string("ONCALLER_SUPERFLUOUS_GENE"); }

      bool IsOkSuperfluousGene (const CSeq_feat* seq_feat);
  };


  class CBioseq_ONCALLER_GENE_MISSING : public CBioseq_missing_genes_oncaller
  {
    public:
     virtual ~CBioseq_ONCALLER_GENE_MISSING () {};

     virtual void GetReport(CRef <CClickableItem>& c_item);
     virtual string GetName() const {return CBioseq_missing_genes_oncaller::GetName_missing();}
  };

  
  class CBioseq_ONCALLER_SUPERFLUOUS_GENE : public CBioseq_missing_genes_oncaller
  {
    public:
     virtual ~CBioseq_ONCALLER_SUPERFLUOUS_GENE () {};

     virtual void GetReport(CRef <CClickableItem>& c_item);
     virtual string GetName() const {return CBioseq_missing_genes_oncaller::GetName_extra();}
  };


  class CBioseq_test_on_bac_partial : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_bac_partial () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_noexc() const {
                           return string("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS");}
      string GetName_exc() const {
                           return string("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION");}
 
      bool IsNonExtendableLeft(const CBioseq& bioseq, const unsigned& pos);
      bool IsNonExtendableRight(const CBioseq& bioseq, const unsigned& pos);
  };


  class CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION 
                                              : public CBioseq_test_on_bac_partial
  {
    public:
      virtual ~CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION () {};
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_bac_partial::GetName_exc();}
  };



  class CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS 
                                              : public CBioseq_test_on_bac_partial
  {
    public:
      virtual ~CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS () {};
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_bac_partial::GetName_noexc();}
  };


  class CBioseq_test_on_rrna : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_rrna () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_nm() const {return string("RRNA_NAME_CONFLICTS"); }
      string GetName_its() const {return string("DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"); }
      string GetName_short() const {return string("DISC_SHORT_RRNA"); }

      bool NameNotStandard(const string& nm);
  };


  class CBioseq_DISC_SHORT_RRNA : public CBioseq_test_on_rrna
  {
    public:
      virtual ~CBioseq_DISC_SHORT_RRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CBioseq_test_on_rrna::GetName_short(); }
  };



  class CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA : public CBioseq_test_on_rrna
  {
    public:
      virtual ~CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CBioseq_test_on_rrna::GetName_its(); }
  };


  class CBioseq_RRNA_NAME_CONFLICTS : public CBioseq_test_on_rrna
  {
    public:
      virtual ~CBioseq_RRNA_NAME_CONFLICTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CBioseq_test_on_rrna::GetName_nm(); }
  };


  class CBioseq_test_on_suspect_phrase : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_suspect_phrase () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_rna_comm() const {return string("DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS");}
      string GetName_misc() const {return string("DISC_SUSPECT_MISC_FEATURES"); }
      
      void GetRepOfSuspPhrase(CRef <CClickableItem>& c_item, const string& setting_name, 
                             const string& phrase_loc_4_1, const string& phrase_loc_4_mul);
      void CheckForProdAndComment(const CSeq_feat& seq_feat);
      void FindBadMiscFeatures(const CSeq_feat& seq_feat);
  };


  class CBioseq_DISC_SUSPECT_MISC_FEATURES : public CBioseq_test_on_suspect_phrase
  {
    public:
      virtual ~CBioseq_DISC_SUSPECT_MISC_FEATURES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CBioseq_test_on_suspect_phrase::GetName_misc(); }
  };


  class CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS : public CBioseq_test_on_suspect_phrase
  {
    public:
      virtual ~CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {
                           return CBioseq_test_on_suspect_phrase::GetName_rna_comm(); }
  };

  class CBioseq_test_on_protfeat : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_protfeat () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
   
    protected:
      string GetName_cds() const {return string("DISC_CDS_PRODUCT_FIND");}
      string GetName_ec() const {return string("EC_NUMBER_ON_UNKNOWN_PROTEIN"); }
      string GetName_pnm() const {return string("DISC_PROTEIN_NAMES"); }

      bool EndsWithPattern(const string& pattern, const list <string>& strs);
  };

  class CBioseq_DISC_PROTEIN_NAMES : public CBioseq_test_on_protfeat
  {
    public:
      virtual ~CBioseq_DISC_PROTEIN_NAMES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_protfeat::GetName_pnm(); }
  };

  class CBioseq_DISC_CDS_PRODUCT_FIND : public CBioseq_test_on_protfeat
  {
    public:
      virtual ~CBioseq_DISC_CDS_PRODUCT_FIND () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_protfeat::GetName_cds(); }
  };


  class CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN : public CBioseq_test_on_protfeat
  {
    public:
      virtual ~CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_protfeat::GetName_ec(); }
  };


  class CBioseq_test_on_genprod_set : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_genprod_set() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_mprot() const {return string("MISSING_GENPRODSET_PROTEIN");}
      string GetName_dprot() const {return string("DUP_GENPRODSET_PROTEIN");}
      string GetName_mtid() const {return string("MISSING_GENPRODSET_TRANSCRIPT_ID");}
      string GetName_dtid() const {return string("DUP_GENPRODSET_TRANSCRIPT_ID");}

      void GetReport_dup(CRef <CClickableItem>& c_item, const string& setting_name, 
                              const string& desc1, const string& desc2, const string& desc3);
  };


  class CBioseq_MISSING_GENPRODSET_PROTEIN : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_MISSING_GENPRODSET_PROTEIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_mprot(); }
  };


  class CBioseq_DUP_GENPRODSET_PROTEIN : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_DUP_GENPRODSET_PROTEIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_dprot(); }
  };


  class CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_mtid(); }
  };


  class CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_dtid(); }
  };


  class CBioseq_test_on_prot : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_prot() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_cnt () const {return string("COUNT_PROTEINS"); }
      string GetName_id() const {return string("MISSING_PROTEIN_ID"); }
      string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX"); }
  };


  class CBioseq_COUNT_PROTEINS : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_COUNT_PROTEINS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_cnt(); }
  };

 
  class CBioseq_MISSING_PROTEIN_ID : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_MISSING_PROTEIN_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_id(); }
  };


  class CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_prefix(); }

    private:
      void MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2);
  };


  class CBioseq_test_on_cd_4_transl : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_cd_4_transl() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
    
    protected:
      string GetName_note() const {return string("TRANSL_NO_NOTE"); }
      string GetName_transl() const {return string("NOTE_NO_TRANSL"); }
      string GetName_long() const {return string("TRANSL_TOO_LONG"); }
      void  TranslExceptOfCDs (const CSeq_feat& cd, bool& has_transl, bool& too_long);
  };

  class CBioseq_TRANSL_NO_NOTE : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_TRANSL_NO_NOTE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_note(); }
  };


  class CBioseq_NOTE_NO_TRANSL : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_NOTE_NO_TRANSL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_transl(); }
  };

 
  class CBioseq_TRANSL_TOO_LONG : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_TRANSL_TOO_LONG () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_long(); }
  };


  class CBioseq_test_on_rna : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_rna() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_tcnt() const {return string("COUNT_TRNAS"); }
      string GetName_rcnt() const {return string("COUNT_RRNAS"); }
      string GetName_rdup() const {return string("FIND_DUP_RRNAS"); }
      string GetName_tdup() const {return string("FIND_DUP_TRNAS"); }  // launches COUNT_TRNAS
      string GetName_len() const {return string("FIND_BADLEN_TRNAS"); }
      string GetName_strand() const {return string("FIND_STRAND_TRNAS"); }
      string GetName_lnc() const {return string("TEST_SHORT_LNCRNA"); }

      void FindMissingRNAsInList();
      bool RRnaMatch(const CRNA_ref& rna1, const CRNA_ref& rna2);
      void FindDupRNAsInList();
      void GetReport_trna(CRef <CClickableItem>& c_item);
      void FindtRNAsOnSameStrand();

      string m_bioseq_desc, m_best_id_str;
  };

  class CBioseq_TEST_SHORT_LNCRNA : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_TEST_SHORT_LNCRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_lnc(); }
  };

  class CBioseq_FIND_STRAND_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_STRAND_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_strand(); }
  };


  class CBioseq_FIND_BADLEN_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_BADLEN_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_len(); }
  };


  class CBioseq_FIND_DUP_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_DUP_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_tdup();}
  };

  class CBioseq_COUNT_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_COUNT_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_tcnt();}
  };


  class CBioseq_COUNT_RRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_COUNT_RRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_rcnt();}
  };


  class CBioseq_FIND_DUP_RRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_DUP_RRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_rdup();}
  };


  
  class CBioseq_test_on_molinfo : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_molinfo () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_mrna() const {return string("MOLTYPE_NOT_MRNA"); }
      string GetName_tsa() const {return string("TECHNIQUE_NOT_TSA"); }
      string GetName_part() const { return string("PARTIAL_CDS_COMPLETE_SEQUENCE"); }
      string GetName_link() const { return string("DISC_POSSIBLE_LINKER"); }
  };


  class CBioseq_DISC_POSSIBLE_LINKER : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_DISC_POSSIBLE_LINKER () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_link();}
  };


  class CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_part();}
  };


  class CBioseq_MOLTYPE_NOT_MRNA : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_MOLTYPE_NOT_MRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_mrna();}
  };


  class CBioseq_TECHNIQUE_NOT_TSA : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_TECHNIQUE_NOT_TSA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_tsa();}
  };


  class CBioseq_test_on_all_annot : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_all_annot () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
    
    protected:
       string GetName_no() const {return string("NO_ANNOTATION");}
       string GetName_long_no() const {return string("DISC_LONG_NO_ANNOTATION");}
       string GetName_joined() const {return string("JOINED_FEATURES");}
       string GetName_ls() const {return string("DISC_FEATURE_LIST");}
       string GetName_loc() const {return string("ONCALLER_ORDERED_LOCATION");}
       string GetName_std() const {return string("ONCALLER_HAS_STANDARD_NAME");}
     
       bool LocationHasNullsBetween(const CSeq_feat* seq_feat);
  };

  class CBioseq_ONCALLER_HAS_STANDARD_NAME : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_ONCALLER_HAS_STANDARD_NAME () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_std();}
  };


  class CBioseq_DISC_FEATURE_LIST : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_DISC_FEATURE_LIST () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_ls();}
  };

  class CBioseq_ONCALLER_ORDERED_LOCATION : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_ONCALLER_ORDERED_LOCATION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_loc();}
  };

  class CBioseq_JOINED_FEATURES : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_JOINED_FEATURES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_joined();}
  };

 
  class CBioseq_NO_ANNOTATION : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_NO_ANNOTATION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_no();}
  };

  class CBioseq_DISC_LONG_NO_ANNOTATION: public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_DISC_LONG_NO_ANNOTATION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_long_no();}
  };

// new comb: CBioseq_


  class CBioseq_DISC_BAD_BGPIPE_QUALS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_BAD_BGPIPE_QUALS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_BAD_BGPIPE_QUALS"); }

    private:
      bool x_CodeBreakIsStopCodon(const list <CRef <CCode_break> >& code_brk);
      bool x_IsBGPipe(const CSeqdesc* sdp); 
      bool x_HasFieldStrNocase(const CUser_object& uobj, const string& field, 
                                                                         const string& str);
  };

  class CBioseq_SUSPECT_PHRASES : public CBioseqTestAndRepData 
  {
    public:
      virtual ~CBioseq_SUSPECT_PHRASES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SUSPECT_PHRASES"); }

    private:
      void FindSuspectPhrases(const string& check_str, const CSeq_feat& seq_feat);
  };

  class CBioseq_DISC_SUSPECT_RRNA_PRODUCTS  : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SUSPECT_RRNA_PRODUCTS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SUSPECT_RRNA_PRODUCTS"); }
  };

  class CBioseq_TEST_ORGANELLE_PRODUCTS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_ORGANELLE_PRODUCTS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_ORGANELLE_PRODUCTS"); }
    
    private:
      SAnnotSelector m_annot_sel;
  }; 

  class CBioseq_DISC_GAPS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_GAPS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_GAPS"); }
  };

  class CBioseq_DISC_INCONSISTENT_MOLINFO_TECH : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_INCONSISTENT_MOLINFO_TECH () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_INCONSISTENT_MOLINFO_TECH"); }
  };

  class CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {
                         return string("TEST_MRNA_OVERLAPPING_PSEUDO_GENE"); }
  };

  class CBioseq_TEST_UNWANTED_SPACER : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_UNWANTED_SPACER () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_UNWANTED_SPACER"); }
 
    private:
      bool HasIntergenicSpacerName (const string& comm);
  };


  class CBioseq_ONCALLER_HIV_RNA_INCONSISTENT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_ONCALLER_HIV_RNA_INCONSISTENT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ONCALLER_HIV_RNA_INCONSISTENT"); }
  };


  class CBioseq_DISC_MITOCHONDRION_REQUIRED : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_MITOCHONDRION_REQUIRED () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_MITOCHONDRION_REQUIRED"); }
  };


  class CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_MICROSATELLITE_REPEAT_TYPE"); }
  };


  class CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"); }
  };


  class CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH  : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEATURE_MOLTYPE_MISMATCH"); }

    private:
      bool IsGenomicDNASequence (const CBioseq& bioseq);
  };

 
  class CBioseq_ADJACENT_PSEUDOGENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_ADJACENT_PSEUDOGENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ADJACENT_PSEUDOGENES"); }

    private:
      string GetGeneStringMatch (const string& str1, const string& str2);
  };


  class CBioseq_DISC_FEAT_OVERLAP_SRCFEAT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_FEAT_OVERLAP_SRCFEAT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEAT_OVERLAP_SRCFEAT"); }
  };


  class CBioseq_CDS_TRNA_OVERLAP : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_CDS_TRNA_OVERLAP () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("CDS_TRNA_OVERLAP"); }
  };


/*
  class CBioseq_INCONSISTENT_SOURCE_DEFLINE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_INCONSISTENT_SOURCE_DEFLINE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_SOURCE_DEFLINE"); }
  };
*/


  class CBioseq_CONTAINED_CDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_CONTAINED_CDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("CONTAINED_CDS"); }

    private:
      bool x_IgnoreContainedCDS(const CSeq_feat* sft);
  };

  class CBioseq_PSEUDO_MISMATCH : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_PSEUDO_MISMATCH () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("PSEUDO_MISMATCH"); }

    private:
      void FindPseudoDiscrepancies(const CSeq_feat& seq_feat);
  };


  class CBioseq_EC_NUMBER_NOTE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EC_NUMBER_NOTE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EC_NUMBER_NOTE"); }
  };



  class CBioseq_NON_GENE_LOCUS_TAG : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_NON_GENE_LOCUS_TAG () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("NON_GENE_LOCUS_TAG"); }
  };


  class CBioseq_SHOW_TRANSL_EXCEPT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHOW_TRANSL_EXCEPT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHOW_TRANSL_EXCEPT");}
  };


  class CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA");}
  };


  class CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {
                          return string("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS");}
  };



/*
  class CBioseq_RRNA_NAME_CONFLICTS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RRNA_NAME_CONFLICTS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RRNA_NAME_CONFLICTS");}

    protected:
      bool NameNotStandard(const string& nm);
  };
*/
  


  class CBioseq_DISC_CDS_WITHOUT_MRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_CDS_WITHOUT_MRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_CDS_WITHOUT_MRNA");}
  };



  class CBioseq_GENE_PRODUCT_CONFLICT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_GENE_PRODUCT_CONFLICT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("GENE_PRODUCT_CONFLICT");}
  };


  class CBioseq_TEST_UNUSUAL_MISC_RNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_UNUSUAL_MISC_RNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_UNUSUAL_MISC_RNA");}

    private:
      string GetTrnaProductString(const CTrna_ext& trna_ext);
      string GetRnaRefProductString(const CRNA_ref& rna_ref);
  };


  class CBioseq_DISC_PARTIAL_PROBLEMS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_PARTIAL_PROBLEMS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_PARTIAL_PROBLEMS");}

    private:
      bool CouldExtendLeft(const CBioseq& bioseq, const unsigned& pos);
      bool CouldExtendRight(const CBioseq& bioseq, const int& pos);
  };


  class CBioseq_DISC_SUSPICIOUS_NOTE_TEXT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SUSPICIOUS_NOTE_TEXT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SUSPICIOUS_NOTE_TEXT");}

    private:
      bool HasSuspiciousStr(const string& str, string& sus_str);
  };



  class CBioseq_SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME");}
  };


  
  class CBioseq_TEST_OVERLAPPING_RRNAS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_OVERLAPPING_RRNAS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_OVERLAPPING_RRNAS");}
  };



  class CBioseq_TEST_LOW_QUALITY_REGION : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_LOW_QUALITY_REGION () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_LOW_QUALITY_REGION");}
  };

 
  class CBioseq_on_bad_gene_name : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_bad_gene_name () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      bool GeneNameHas4Numbers(const string& locus);
      string GetName_gnm() const {return string("TEST_BAD_GENE_NAME");}
      string GetName_bgnm() const {return string("DISC_BAD_BACTERIAL_GENE_NAME"); }
  };

  class CBioseq_TEST_BAD_GENE_NAME : public CBioseq_on_bad_gene_name
  {
    public:
      virtual ~CBioseq_TEST_BAD_GENE_NAME () {};
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_bad_gene_name::GetName_gnm(); }
  };

  class CBioseq_DISC_BAD_BACTERIAL_GENE_NAME : public CBioseq_on_bad_gene_name
  {
    public:
      virtual ~CBioseq_DISC_BAD_BACTERIAL_GENE_NAME () {};
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_on_bad_gene_name::GetName_bgnm(); }
  };


/*
  class CBioseq_DISC_SHORT_RRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SHORT_RRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SHORT_RRNA");}
  };
*/


  class CBioseq_DISC_BAD_GENE_STRAND : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_BAD_GENE_STRAND () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_BAD_GENE_STRAND");}

    private:
      bool AreIntervalStrandsOk(const CSeq_loc& g_loc, const CSeq_loc& f_loc);
  };



  class CBioseq_DISC_SHORT_INTRON : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SHORT_INTRON () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SHORT_INTRON");}

    private:
      bool PosIsAt3End(const unsigned pos, CConstRef <CBioseq>& bioseq);
      bool PosIsAt5End(unsigned pos, CConstRef <CBioseq>& bioseq);
  };


/*
  class CBioseq_TEST_UNUSUAL_NT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_UNUSUAL_NT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_UNUSUAL_NT");}
  };


  class CBioseq_N_RUNS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_N_RUNS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("N_RUNS");}
  };


  class CBioseq_N_RUNS_14 : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_N_RUNS_14 () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("N_RUNS_14");}
  };



  class CBioseq_ZERO_BASECOUNT : public CBioseqTestAndRepData
  {   
    public:
      virtual ~CBioseq_ZERO_BASECOUNT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ZERO_BASECOUNT");}
  };


  class CBioseq_DISC_PERCENT_N : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_PERCENT_N () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_PERCENT_N");}
  };


  class CBioseq_DISC_10_PERCENTN : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_10_PERCENTN () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_10_PERCENTN");}
  };
*/


  class CBioseq_RNA_NO_PRODUCT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RNA_NO_PRODUCT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RNA_NO_PRODUCT");}
  };



/*
  class CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EC_NUMBER_ON_UNKNOWN_PROTEIN");}
  };
*/



  class CBioseq_FEATURE_LOCATION_CONFLICT :  public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_FEATURE_LOCATION_CONFLICT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("FEATURE_LOCATION_CONFLICT");}

    private:
      void CheckFeatureTypeForLocationDiscrepancies(const vector <const CSeq_feat*>& seq_feat,
                                                                const string& feat_type);
      bool IsGeneLocationOk(const CSeq_feat* seq_feat, const CSeq_feat* gene);
      bool IsMixStrand(const CSeq_feat* seq_feat);
      bool IsMixedStrandGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc);
      bool GeneRefMatch(const CGene_ref& gene1, const CGene_ref& gene2);
      bool StrandOk(const int& strand1, const int& strand2);
  };

  class CBioseq_MISSING_LOCUS_TAGS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_MISSING_LOCUS_TAGS() { };

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MISSING_LOCUS_TAGS"); }

    private:
      bool x_IsLocationDirSub(const CSeq_loc& seq_location);
  };


  class CBioseq_on_locus_tags : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_on_locus_tags () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("LOCUS_TAGS"); };

    protected:
      string GetName_dup() const { return string("DUPLICATE_LOCUS_TAGS"); }
      string GetName_incons() const { return string("INCONSISTENT_LOCUS_TAG_PREFIX"); }
      string GetName_badtag() const { return string("BAD_LOCUS_TAG_FORMAT"); }
  };

  class CBioseq_DUPLICATE_LOCUS_TAGS : public CBioseq_on_locus_tags
  {
    public:
      virtual ~CBioseq_DUPLICATE_LOCUS_TAGS() { };

      virtual string GetName() const {return CBioseq_on_locus_tags::GetName_dup();}
  };

  class CBioseq_INCONSISTENT_LOCUS_TAG_PREFIX : public CBioseq_on_locus_tags
  {
    public:
      virtual ~CBioseq_INCONSISTENT_LOCUS_TAG_PREFIX() { };

      virtual string GetName() const {return CBioseq_on_locus_tags::GetName_incons();}
  };

  class CBioseq_BAD_LOCUS_TAG_FORMAT : public CBioseq_on_locus_tags
  {
    public:
      virtual ~CBioseq_BAD_LOCUS_TAG_FORMAT() { };

      virtual string GetName() const {return CBioseq_on_locus_tags::GetName_badtag();}
  };

  class CBioseq_DISC_FEATURE_COUNT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_FEATURE_COUNT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEATURE_COUNT");}
  };



  class CBioseq_OVERLAPPING_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_OVERLAPPING_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("OVERLAPPING_GENES");}
  };


  class CBioseq_FIND_OVERLAPPED_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_FIND_OVERLAPPED_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("FIND_OVERLAPPED_GENES");}
  };



/*
  class CBioseq_MISSING_PROTEIN_ID : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_MISSING_PROTEIN_ID () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MISSING_PROTEIN_ID");}

    private:
      string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX");}
  };
*/


/*
  class CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX");}

   private:
      string GetName_pid() const {return string("MISSING_PROTEIN_ID"); }
      void MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2);
  };

*/


  class CBioseq_TEST_DEFLINE_PRESENT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_DEFLINE_PRESENT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_DEFLINE_PRESENT");}
  };

 

  class CBioseq_DISC_QUALITY_SCORES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_QUALITY_SCORES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_QUALITY_SCORES");}
  };


  class CBioseq_RNA_CDS_OVERLAP : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RNA_CDS_OVERLAP () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RNA_CDS_OVERLAP");}
  };


  class CBioseq_DISC_COUNT_NUCLEOTIDES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_COUNT_NUCLEOTIDES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_COUNT_NUCLEOTIDES");}
unsigned m_cnt;
  };



/*
  class CBioseq_EXTRA_MISSING_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EXTRA_MISSING_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EXTRA_MISSING_GENES");}

    protected:
      bool GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref);
      string GetName_extra() const {return string("EXTRA_GENES"); }
      string GetName_extra_pseudodp() const {return string("EXTRA_GENES_pseudodp"); }
      string GetName_extra_frameshift() const {return string("EXTRA_GENES_frameshift"); }
      string GetName_extra_nonshift() const {return string("EXTRA_GENES_nonshift"); }
      string GetName_missing() const {return string("MISSING_GENES"); }
  };
*/



  class CBioseq_DUPLICATE_GENE_LOCUS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DUPLICATE_GENE_LOCUS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DUPLICATE_GENE_LOCUS");}

      CRef <GeneralDiscSubDt> AddLocus(const CSeq_feat& seq_feat);
  };


  class CBioseq_OVERLAPPING_CDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_OVERLAPPING_CDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("OVERLAPPING_CDS");}

    private:
      bool OverlappingProdNmSimilar(const string& prod_nm1, const string& prod_nm2);
      void AddToDiscRep(const CSeq_feat* seq_feat);
      bool HasNoSuppressionWords(const CSeq_feat* seq_feat);
  };


// CSeqFeat
  class CSeqFeatTestAndRepData : public CTestAndRepData
  {
      friend class CTestAndRepData;
  
    public:
       virtual ~CSeqFeatTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) = 0; 

       virtual void GetReport(CRef <CClickableItem>& c_item)=0; 

       virtual string GetName() const  = 0;
  };
};


END_NCBI_SCOPE

#endif
