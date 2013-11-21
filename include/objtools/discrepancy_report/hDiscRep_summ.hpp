#ifndef _DiscRep_Summ_HPP
#define _DiscRep_Summ_HPP

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
 *   summization for suspect product rules
 */

#include <objects/macro/Replace_func.hpp>
#include <objects/macro/Structured_comment_field.hpp>
#include <objects/macro/Word_substitution.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc)

   class CSummarizeSusProdRule 
   {
     public:
       CSummarizeSusProdRule (void) {};
       ~CSummarizeSusProdRule () {};

       string SummarizeSuspectRuleEx(const CSuspect_rule& rule, bool short_version = true);
       string SummarizeSearchFunc (const CSearch_func& func, bool short_version = true);
       string SummarizeConstraintSet (const CConstraint_choice_set& cons_set);
       string SummarizeConstraint (const CConstraint_choice& cons_choice);
       string SummarizeLocationConstraint (const CLocation_constraint& loc_cons);
       string SummarizePartialnessForLocationConstraint(const CLocation_constraint& loc_cons);
       string SummarizeEndDistance (const CLocation_pos_constraint& lp_cons, 
                                                               const string& end_name);
       string SummarizeSourceConstraint (const CSource_constraint& src_cons);
       string SummarizeStringConstraintEx(const CString_constraint& str_cons, 
                                                                   bool short_version = true);
       string SummarizeSourceQual(const CSource_qual_choice& src_qual);
       string SummarizeCDSGeneProtQualConstraint(const CCDSGeneProt_qual_constraint& cgp_cons);
       string SummarizeCDSGeneProtPseudoConstraint(
                                     const CCDSGeneProt_pseudo_constraint& cgp_pcons);
       string SummarizeSequenceConstraint (const CSequence_constraint& seq_cons);
       bool HasQuantity(const CQuantity_constraint& v, CQuantity_constraint::E_Choice& e_val, 
                                                                                   int& num);
       string SummarizeFeatureQuantity (const CQuantity_constraint& v, 
                                                            const string* feature_name = 0);
       string SummarizeSequenceLength (const CQuantity_constraint& v);
       string SummarizePublicationConstraint (const CPublication_constraint& pub_cons);
       string SummarizePubFieldConstraint (const CPub_field_constraint& field);
       string SummarizePubFieldSpecialConstraint(const CPub_field_special_constraint& field);
       string SummarizeFieldConstraint (const CField_constraint& field_cons);
       string SummarizeMissingFieldConstraint (const CField_type& field_tp);
       string SummarizeFieldType (const CField_type& vnp);
       string GetSequenceQualName (const CMolinfo_field& field);
       string SummarizeRnaQual (const CRna_qual& rq);
       string SummarizeStructuredCommentField (const CStructured_comment_field& field);
       string SummarizeMolinfoFieldConstraint (const CMolinfo_field_constraint& mol_cons);
       void GetStringConstraintPhrase(const list <CRef <CString_constraint> >& str_cons_set, 
                                                                           string& phrase);
       string SummarizeTranslationConstraint (const CTranslation_constraint& tras_cons);
       string SummarizeTranslationMismatches (const CQuantity_constraint& v);
       string SummarizeReplaceFunc (const CReplace_func& replace, bool short_version = false);
       string GetStringLocationPhrase (EString_location match_location, bool not_present);
       string SummarizeWordSubstitution (const CWord_substitution& word);
       string FeatureFieldLabel(const string& feature_name, const CFeat_qual_choice& field);
       string SummarizeReplaceRule (const CReplace_rule& replace);
   };

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE


#endif

