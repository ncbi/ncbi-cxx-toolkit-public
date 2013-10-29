/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_desc
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/validator/validatorp.hpp>
#include "utilities.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Field_set.hpp>
#include <objects/valid/Field_rule.hpp>
#include <objects/valid/Dependent_field_set.hpp>
#include <objects/valid/Dependent_field_rule.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/format/items/comment_item.hpp>

#include <objects/misc/sequence_macros.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_desc::CValidError_desc(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_desc::~CValidError_desc(void)
{
}


/**
 * Validate descriptors as stand alone objects (no context)
 **/
void CValidError_desc::ValidateSeqDesc
(const CSeqdesc& desc,
 const CSeq_entry& ctx)
{
    m_Ctx.Reset(&ctx);

    // switch on type, e.g., call ValidateBioSource, ValidatePubdesc, ...
    switch ( desc.Which() ) {
        case CSeqdesc::e_Modif:
        {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "Modif descriptor is obsolete", *m_Ctx, desc);
            CSeqdesc::TModif::const_iterator it = desc.GetModif().begin();
            while (it != desc.GetModif().end()) {
                if (*it == eGIBB_mod_other) {
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_Unknown, "GIBB-mod = other used", ctx, desc);
                }
                ++it;
            }
        }

            break;

        case CSeqdesc::e_Mol_type:
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "MolType descriptor is obsolete", *m_Ctx, desc);
            break;

        case CSeqdesc::e_Method:           
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "Method descriptor is obsolete", *m_Ctx, desc);
            break;

        case CSeqdesc::e_Comment:
            ValidateComment(desc.GetComment(), desc);
            break;

        case CSeqdesc::e_Pub:
            m_Imp.ValidatePubdesc(desc.GetPub(), desc, &ctx);
            break;

        case CSeqdesc::e_User:
            ValidateUser(desc.GetUser(), desc);
            break;

        case CSeqdesc::e_Source:
            m_Imp.ValidateBioSource (desc.GetSource(), desc, &ctx);
            break;
        
        case CSeqdesc::e_Molinfo:
            ValidateMolInfo(desc.GetMolinfo(), desc);
            break;

        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Name:
            if (NStr::IsBlank (desc.GetName())) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                         "Name descriptor needs text", ctx, desc);
			}
            break;
        case CSeqdesc::e_Title:
            if (NStr::IsBlank (desc.GetTitle())) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                         "Title descriptor needs text", ctx, desc);
            } else {
                string title = desc.GetTitle();
                if (s_StringHasPMID (desc.GetTitle())) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TitleHasPMID, 
                             "Title descriptor has internal PMID", ctx, desc);
                }
                NStr::TruncateSpacesInPlace (title);
                char end = title.c_str()[title.length() - 1];
                
                if (end == '.' && title.length() > 4) {
                    end = title.c_str()[title.length() - 2];
                }
                if (end == ','
                    || end == '.'
                    || end == ';'
                    || end == ':') {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadPunctuation, 
                             "Title descriptor ends in bad punctuation", ctx, desc);
                }
            }
            break;
        case CSeqdesc::e_Org:
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "OrgRef descriptor is obsolete", *m_Ctx, desc);
            break;
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Region:
			if (NStr::IsBlank (desc.GetRegion())) {
				PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
						 "Region descriptor needs text", ctx, desc);
			}
            break;
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            break;
        case CSeqdesc::e_Create_date:
            {
                int rval = CheckDate (desc.GetCreate_date(), true);
                if (rval != eDateValid_valid) {
                    m_Imp.PostBadDateError (eDiag_Error, "Create date has error", rval, desc, &ctx);
                }
            }
            break;
        case CSeqdesc::e_Update_date:
            {
                int rval = CheckDate (desc.GetUpdate_date(), true);
                if (rval != eDateValid_valid) {
                    m_Imp.PostBadDateError (eDiag_Error, "Update date has error", rval, desc, &ctx);
                }
            }
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        default:
            break;
    }

    m_Ctx.Reset();
}


void CValidError_desc::ValidateComment
(const string& comment,
 const CSeqdesc& desc)
{
    if ( m_Imp.IsSerialNumberInComment(comment) ) {
        PostErr(eDiag_Info, eErr_SEQ_DESCR_SerialInComment,
            "Comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", *m_Ctx, desc);
    }
    if (NStr::IsBlank (comment)) {
        PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                 "Comment descriptor needs text", *m_Ctx, desc);    
    } else {
        if (NStr::Find (comment, "::") != string::npos) {
            PostErr (eDiag_Info, eErr_SEQ_DESCR_FakeStructuredComment, 
                     "Comment may be formatted to look like a structured comment.", *m_Ctx, desc);  
        }
    }       
}


static EDiagSev s_ErrorLevelFromFieldRuleSev (CField_rule::TSeverity severity)
{
  EDiagSev sev = eDiag_Error;
  switch (severity) {
    case eSeverity_level_none:
      sev = eDiag_Info;
      break;
    case eSeverity_level_info:
      sev = eDiag_Info;
      break;
    case eSeverity_level_warning:
      sev = eDiag_Warning;
      break;
    case eSeverity_level_error:
      sev = eDiag_Error;
      break;
    case eSeverity_level_reject:
      sev = eDiag_Critical;
      break;
    case eSeverity_level_fatal:
      sev = eDiag_Fatal;
      break;
  }
  return sev;
}


bool s_UserFieldCompare (const CRef<CUser_field>& f1, const CRef<CUser_field>& f2)
{
    if (!f1->IsSetLabel()) return true;
    if (!f2->IsSetLabel()) return false;
    return f1->GetLabel().Compare(f2->GetLabel()) < 0;
}

static bool s_IsGenomeAssembly (
    const CUser_object& usr
)

{
    ITERATE (CUser_object::TData, field, usr.GetData()) {
        if ((*field)->IsSetLabel() && (*field)->GetLabel().IsStr()) {
            if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "StructuredCommentPrefix")) {
                const string& prefix = (*field)->GetData().GetStr();
                if (NStr::EqualCase(prefix, "##Genome-Assembly-Data-START##")) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool CValidError_desc::ValidateStructuredComment 
(const CUser_object& usr,
 const CSeqdesc& desc,
 const CComment_rule& rule,
 bool  report)
{
    bool is_valid = true;
    EDiagSev sev;

    CField_set::Tdata::const_iterator field_rule = rule.GetFields().Get().begin();
    CUser_object::TData::const_iterator field = usr.GetData().begin();
    while (field_rule != rule.GetFields().Get().end()
           && field != usr.GetData().end()) {
        if ((*field)->IsSetLabel()) {
            string label = "";
            if ((*field)->GetLabel().IsStr()) {
                label = (*field)->GetLabel().GetStr();
            } else {
                label = NStr::IntToString((*field)->GetLabel().GetId());
            }
            // skip suffix and prefix
            if (NStr::Equal(label, "StructuredCommentPrefix")
                || NStr::Equal(label, "StructuredCommentSuffix")) {
                ++field;
                continue;
            }
            string expected_field = (*field_rule)->GetField_name();
            if (NStr::Equal(expected_field, label)) {
                // field in correct order
                // is value correct?
                string value = "";
                if ((*field)->GetData().IsStr()) {
                    value = ((*field)->GetData().GetStr());
                } else if ((*field)->GetData().IsInt()) {
                    value = NStr::IntToString((*field)->GetData().GetInt());
                }
                if (!(*field_rule)->DoesStringMatchRuleExpression(value)) {
                    if (report) {
                        // post error about not matching format
                        sev = s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity());
                        if (NStr::EqualNocase (label, "Finishing Goal") && s_IsGenomeAssembly (usr)) {
                            sev = eDiag_Error;
                        } else if (NStr::EqualNocase (label, "Current Finishing Status") && s_IsGenomeAssembly (usr)) {
                            sev = eDiag_Error;
                        }
                        PostErr (sev,
                                 eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue, 
                                 value + " is not a valid value for " + label, *m_Ctx, desc);   
                    }
                    is_valid = false;
                }
                ++field;
                ++field_rule;
            } else {
                // find rule for this field
                try {
                    // find field for this rule and validate it
                    try {
                        const CUser_field& other_field = usr.GetField(expected_field);
                        if (rule.GetRequire_order()) {
                            if (report) {
                                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                         eErr_SEQ_DESCR_BadStrucCommFieldOutOfOrder, 
                                         expected_field + " field is out of order", *m_Ctx, desc);
                            }
                            is_valid = false;
                        }
                        string value = "";
                        if (other_field.GetData().IsStr()) {
                            value = (other_field.GetData().GetStr());
                        } else if (other_field.GetData().IsInt()) {
                            value = NStr::IntToString(other_field.GetData().GetInt());
                        }
                        if (!(*field_rule)->DoesStringMatchRuleExpression(value)) {
                            if (report) {
                                // post error about not matching format
                                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                         eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue, 
                                         value + " is not a valid value for " + expected_field, *m_Ctx, desc);  
                            }
                            is_valid = false;
                        }
                    } catch (CException ) {
                        if ((*field_rule)->IsSetRequired() && (*field_rule)->GetRequired()) {
                            if (report) {
                                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                        eErr_SEQ_DESCR_BadStrucCommMissingField,
                                        "Required field " + (*field_rule)->GetField_name() + " is missing",
                                        *m_Ctx, desc);
                            }
                            is_valid = false;
                        }
                    }
                    ++field_rule;
                } catch (CException ) {
                    if (!rule.IsSetAllow_unlisted()) {
                        if (report) {
                            PostErr (eDiag_Error, 
                                     eErr_SEQ_DESCR_BadStrucCommInvalidFieldName, 
                                     label + " is not a valid field name", *m_Ctx, desc);
                        }
                        is_valid = false;
                    }
                    ++field;
                }
            }
        } else {
            // post error about field without label
            PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStrucCommInvalidFieldName, 
                     "Structured Comment contains field without label", *m_Ctx, desc);                            
            is_valid = false;
            ++field;
        }
    }

    while (field_rule != rule.GetFields().Get().end()) {
        if ((*field_rule)->IsSetRequired() && (*field_rule)->GetRequired()) {
            if (report) {
                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                        eErr_SEQ_DESCR_BadStrucCommMissingField,
                        "Required field " + (*field_rule)->GetField_name() + " is missing",
                        *m_Ctx, desc);
            }
            is_valid = false;
        }
        ++field_rule;
    }

    while (field != usr.GetData().end()) {
        // skip suffix and prefix
        if ((*field)->IsSetLabel()) {
            string label = "";
            if ((*field)->GetLabel().IsStr()) {
                label = (*field)->GetLabel().GetStr();
            } else {
                label = NStr::IntToString((*field)->GetLabel().GetId());
            }
            if (NStr::Equal(label, "StructuredCommentPrefix")
                || NStr::Equal(label, "StructuredCommentSuffix")) {
                ++field;
                continue;
            }
            try {
                const CField_rule& field_rule = rule.FindFieldRule(label);
                const CUser_field& other_field = usr.GetField(label);
                if (&other_field != (*field)) {
                    if (report) {
                        // post error about multiple field values
                        PostErr (s_ErrorLevelFromFieldRuleSev(field_rule.GetSeverity()),
                                 eErr_SEQ_DESCR_BadStrucCommMultipleFields, 
                                 "Multiple values for " + label + " field", *m_Ctx, desc);   
                    }
                    is_valid = false;
                }
            } catch (CException ) {
                if (!rule.IsSetAllow_unlisted()) {
                    if (report) {
                        // field not found, not legitimate field name
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStrucCommInvalidFieldName, 
                                 label + " is not a valid field name", *m_Ctx, desc); 
                    }
                    is_valid = false;
                }
            }

        } else {
            if (report) {
                // post error about field without label
                PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadStrucCommInvalidFieldName, 
                         "Structured Comment contains field without label", *m_Ctx, desc);                            
            }
            is_valid = false;
        }
        ++field;
    }

    // now look at dependent rules
    ITERATE (CDependent_field_set::Tdata, depend_rule, rule.GetDependent_rules().Get()) {
        try {
            string depend_field_name = (*depend_rule)->GetMatch_name();
            const CUser_field& depend_field = usr.GetField(depend_field_name);
            string value = "";
            if (depend_field.GetData().IsStr()) {
                value = (depend_field.GetData().GetStr());
            } else if (depend_field.GetData().IsInt()) {
                value = NStr::IntToString(depend_field.GetData().GetInt());
            }
            if (((!(*depend_rule)->IsSetInvert_match() || !(*depend_rule)->GetInvert_match()) && (*depend_rule)->DoesStringMatchRuleExpression(value))
                || ((*depend_rule)->IsSetInvert_match() && (*depend_rule)->GetInvert_match() && !(*depend_rule)->DoesStringMatchRuleExpression(value))) {
                // other rules apply
                ITERATE (CField_set::Tdata, other_rule, (*depend_rule)->GetOther_fields().Get()) {
                    try {
                        string other_field_name = (*other_rule)->GetField_name();
                        const CUser_field& other_field = usr.GetField(other_field_name);
                        string other_value = "";
                        if (other_field.GetData().IsStr()) {
                            other_value = (other_field.GetData().GetStr());
                        } else if (other_field.GetData().IsInt()) {
                            other_value = NStr::IntToString(other_field.GetData().GetInt());
                        }
                        if (!(*other_rule)->DoesStringMatchRuleExpression(other_value)) {
                            if (report) {
                                // post error about not matching format
                                PostErr (s_ErrorLevelFromFieldRuleSev((*other_rule)->GetSeverity()),
                                         eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue, 
                                         other_value + " is not a valid value for " + other_field_name
                                         + " when " + depend_field_name + " has value '" + value + "'",                                             
                                         *m_Ctx, desc);
                            }
                            is_valid = false;
                        }

                    } catch (CException) {
                        // unable to find field
                        if ((*other_rule)->IsSetRequired() && (*other_rule)->GetRequired()) {
                            if (report) {
                                PostErr (s_ErrorLevelFromFieldRuleSev((*other_rule)->GetSeverity()),
                                        eErr_SEQ_DESCR_BadStrucCommMissingField,
                                        "Required field " + (*other_rule)->GetField_name() + " is missing when "
                                        + depend_field_name + " has value '" + value + "'",
                                        *m_Ctx, desc);
                            }
                            is_valid = false;
                        }
                    }
                }
                ITERATE (CField_set::Tdata, other_rule, (*depend_rule)->GetDisallowed_fields().Get()) {
                    try {
                        string other_field_name = (*other_rule)->GetField_name();
                        // found field that should not be present
                        if (report) {
                            PostErr (s_ErrorLevelFromFieldRuleSev((*other_rule)->GetSeverity()),
                                     eErr_SEQ_DESCR_BadStrucCommInvalidFieldName, 
                                     other_field_name + " is not a valid field name when " + depend_field_name + " has value '" + value + "'",                                             
                                     *m_Ctx, desc);                            
                        }
                        is_valid = false;
                    } catch (CException ) {
                        // did not find field, good
                    }
                }
            }
        } catch (CException ) {
            // field not found
        }
    }
    return is_valid;
}


static string s_OfficialPrefixList[] = {
    "Assembly-Data",
    "EpifluData",
    "Evidence-Data",
    "FluData",
    "Genome-Annotation-Data",
    "Genome-Assembly-Data",
    "GISAID_EpiFlu(TM)Data",
    "HCVDataBaseData",
    "HIVDataBaseData",
    "International Barcode of Life (iBOL)Data", 
    "MIENS-Data",
    "MIGS-Data",
    "MIGS:3.0-Data",
    "MIMARKS:3.0-Data",
    "MIMS-Data",
    "MIMS:3.0-Data",
    "RefSeq-Attributes"
};

static bool s_PrefixOrSuffixInList (
  const string& val,
  const string& before,
  const string& after
)

{
    for ( size_t i = 0; 
          i < sizeof(s_OfficialPrefixList) / sizeof(string); 
          ++i ) {
        const string str = before + s_OfficialPrefixList[i] + after;
        if (NStr::EqualNocase (val, str)) {
            return true;
        }
    }
    return false;
}


bool CValidError_desc::ValidateStructuredComment
(const CUser_object& usr,
 const CSeqdesc& desc,
 bool  report)
{
    bool is_valid = true;
    if (!usr.IsSetType() || !usr.GetType().IsStr()
        || !NStr::EqualCase(usr.GetType().GetStr(), "StructuredComment")) {
        return false;
    }
    if (!usr.IsSetData() || usr.GetData().size() == 0) {
        if (report) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_UserObjectProblem, 
                     "Structured Comment user object descriptor is empty", *m_Ctx, desc);
        }
        return false;
    }

    if (! usr.HasField("StructuredCommentPrefix")) {
        if (report) {
            PostErr (eDiag_Info, eErr_SEQ_DESCR_StructuredCommentPrefixOrSuffixMissing, 
                    "Structured Comment lacks prefix", *m_Ctx, desc);
        }
        return false;
    }

    // find prefix
    try {
        const CUser_field& prefix = usr.GetField("StructuredCommentPrefix");
        if (!prefix.IsSetData() || !prefix.GetData().IsStr()) {
            return true;
        }
        const string& pfx = prefix.GetData().GetStr();
        if (! s_PrefixOrSuffixInList (pfx, "##", "-START##")) {
            PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue, 
                    pfx + " is not a valid value for StructuredCommentPrefix", *m_Ctx, desc);   
        }
        CRef<CComment_set> comment_rules = m_Imp.GetStructuredCommentRules();
        if (comment_rules) {
            try {
                const CComment_rule& rule = comment_rules->FindCommentRule(prefix.GetData().GetStr());

                if (rule.GetRequire_order()) {
                    is_valid = ValidateStructuredComment (usr, desc, rule, report);
                } else {
                    CUser_object tmp;
                    tmp.Assign(usr);
                    CUser_object::TData& fields = tmp.SetData();
                    stable_sort (fields.begin(), fields.end(), s_UserFieldCompare);
                    is_valid = ValidateStructuredComment (tmp, desc, rule, report);
                }
            } catch (CException ) {
                // no rule for this prefix, no error
            }
        }   
        try {
            const CUser_field& suffix = usr.GetField("StructuredCommentSuffix");
            if (!suffix.IsSetData() || !suffix.GetData().IsStr()) {
                return true;
            }
            const string& sfx = suffix.GetData().GetStr();
            if (! s_PrefixOrSuffixInList (sfx, "##", "-END##")) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue, 
                        sfx + " is not a valid value for StructuredCommentSuffix", *m_Ctx, desc);   
            }
        } catch (CException ) {
            // we don't care about missing suffixes
        }
    } catch (CException ) {
        // no prefix, in which case no rules
        // but it is still an error - should have prefix
        if (report) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_StructuredCommentPrefixOrSuffixMissing, 
                    "Structured Comment lacks prefix", *m_Ctx, desc);
        }
        is_valid = false;
    }
    return is_valid;
}

static bool x_IsBadBioSampleFormat (
    const string& str
)

{
    char  ch;
    int   i;

    if (str.length() < 5) return true;

    if (str [0] != 'S') return true;
    if (str [1] != 'A') return true;
    if (str [2] != 'M') return true;
    if (str [3] != 'E' && str [3] != 'N' && str [3] != 'D') return true;

    for (i = 4; i < str.length(); i++) {
        ch = str [i];
        if (! isdigit (ch)) return true;
    }

    return false;
}

static bool x_IsBadAltBioSampleFormat (
    const string& str
)

{
    char  ch;
    int   i;

    if (str.length() < 9) return true;

    if (str [0] != 'S') return true;
    if (str [1] != 'R') return true;
    if (str [2] != 'S') return true;

    for (i = 3; i < str.length(); i++) {
        ch = str [i];
        if (! isdigit (ch)) return true;
    }

    return false;
}

static bool x_IsBadSRAFormat (
    const string& str
)

{
    char  ch;
    int   i;

    if (str.length() < 9) return true;

    ch = str [0];
    if (ch != 'S' && ch != 'D' && ch != 'E') return true;
    ch = str [1];
    if (! isupper (ch)) return true;
    ch = str [2];
    if (! isupper (ch)) return true;

    for (i = 3; i < str.length(); i++) {
        ch = str [i];
        if (! isdigit (ch)) return true;
    }

    return false;
}

static bool x_IsBadBioProjectFormat (
    const string& str
)

{
    char  ch;
    int   i;

    if (str.length() < 6) return true;

    if (str [0] != 'P') return true;
    if (str [1] != 'R') return true;
    if (str [2] != 'J') return true;
    if (str [3] != 'E' && str [3] != 'N' && str [3] != 'D') return true;
    ch = str [4];
    if (! isupper (ch)) return true;

    for (i = 5; i < str.length(); i++) {
        ch = str [i];
        if (! isdigit (ch)) return true;
    }

    return false;
}

static string s_legalDblinkNames [] = {
    "Trace Assembly Archive",
    "ProbeDB",
    "Assembly",
    "BioSample",
    "Sequence Read Archive",
    "BioProject"
};

bool CValidError_desc::ValidateDblink
(const CUser_object& usr,
 const CSeqdesc& desc,
 bool  report)
{
    bool is_valid = true;
    if (!usr.IsSetType() || !usr.GetType().IsStr()
        || !NStr::EqualCase(usr.GetType().GetStr(), "DBLink")) {
        return false;
    }
    if (!usr.IsSetData() || usr.GetData().size() == 0) {
        if (report) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_UserObjectProblem, 
                     "DBLink user object descriptor is empty", *m_Ctx, desc);
        }
        return false;
    }

    FOR_EACH_USERFIELD_ON_USEROBJECT(ufd_it, usr) {
        const CUser_field& fld = **ufd_it;
        if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
            const string &label_str = GET_FIELD(fld.GetLabel(), Str);
            if (NStr::EqualNocase(label_str, "BioSample")) {
                if (fld.IsSetData() && fld.GetData().IsStrs()) {
                    const CUser_field::C_Data::TStrs& strs = fld.GetData().GetStrs();
                    ITERATE(CUser_field::C_Data::TStrs, st_itr, strs) {
                        const string& str = *st_itr;
                        if (x_IsBadBioSampleFormat (str) && x_IsBadAltBioSampleFormat (str)) {
                            PostErr(eDiag_Error, eErr_SEQ_DESCR_DBLinkProblem,
                                "Bad BioSample format - " + str, *m_Ctx, desc);
                        }
                    }
                }
            } else if (NStr::EqualNocase(label_str, "Sequence Read Archive")) {
                if (fld.IsSetData() && fld.GetData().IsStrs()) {
                    const CUser_field::C_Data::TStrs& strs = fld.GetData().GetStrs();
                    ITERATE(CUser_field::C_Data::TStrs, st_itr, strs) {
                        const string& str = *st_itr;
                        if (x_IsBadSRAFormat (str)) {
                            PostErr(eDiag_Error, eErr_SEQ_DESCR_DBLinkProblem,
                                "Bad Sequence Read Archive format - " + str, *m_Ctx, desc);
                        }
                    }
                }
            } else if (NStr::EqualNocase(label_str, "BioProject")) {
                if (fld.IsSetData() && fld.GetData().IsStrs()) {
                    const CUser_field::C_Data::TStrs& strs = fld.GetData().GetStrs();
                    ITERATE(CUser_field::C_Data::TStrs, st_itr, strs) {
                        const string& str = *st_itr;
                        if (x_IsBadBioProjectFormat (str)) {
                            PostErr(eDiag_Error, eErr_SEQ_DESCR_DBLinkProblem,
                                "Bad BioProject format - " + str, *m_Ctx, desc);
                        }
                    }
                }
            }

            for ( size_t i = 0; i < sizeof(s_legalDblinkNames) / sizeof(string); ++i) {
                if (NStr::EqualNocase (label_str, s_legalDblinkNames[i]) && ! NStr::EqualCase (label_str, s_legalDblinkNames[i])) {
                    PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                         "Bad DBLink capitalization - " + label_str, *m_Ctx, desc);
                }
            }
        }
    }

    return is_valid;
}


void CValidError_desc::ValidateUser
(const CUser_object& usr,
 const CSeqdesc& desc)
{
    if ( !usr.CanGetType() ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_UserObjectProblem,
                "User object with no type", *m_Ctx, desc);
        return;
    }
    const CObject_id& oi = usr.GetType();
    if ( !oi.IsStr() && !oi.IsId() ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_UserObjectProblem,
                "User object with no type", *m_Ctx, desc);
        return;
    }
    if ( !usr.IsSetData() || usr.GetData().size() == 0) {
        if (! NStr::EqualNocase(oi.GetStr(), "NcbiAutofix") && ! NStr::EqualNocase(oi.GetStr(), "Unverified")) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_UserObjectProblem,
                    "User object with no data", *m_Ctx, desc);
            return;
        }
    }
    if ( oi.IsStr() && NStr::EqualNocase(oi.GetStr(), "RefGeneTracking")) {
        bool has_ref_track_status = false;
        ITERATE(CUser_object::TData, field, usr.GetData()) {
            if ( (*field)->CanGetLabel() ) {
                const CObject_id& obj_id = (*field)->GetLabel();
                if ( !obj_id.IsStr() ) {
                    continue;
                }
                if ( NStr::CompareNocase(obj_id.GetStr(), "Status") == 0 ) {
                    has_ref_track_status = true;
					          if ((*field)->IsSetData() && (*field)->GetData().IsStr()) {
						            if (CCommentItem::GetRefTrackStatus(usr) == CCommentItem::eRefTrackStatus_Unknown) {
							              PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingIllegalStatus, 
									              "RefGeneTracking object has illegal Status '" 
									              + (*field)->GetData().GetStr() + "'",
									              *m_Ctx, desc);
						            }
					          }
                }
            }
        }
        if ( !has_ref_track_status ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingWithoutStatus,
                "RefGeneTracking object needs to have Status set", *m_Ctx, desc);
        }
    } else if ( oi.IsStr() && NStr::EqualCase(oi.GetStr(), "StructuredComment")) {
        ValidateStructuredComment(usr, desc);
    } else if ( oi.IsStr() && NStr::EqualCase(oi.GetStr(), "DBLink")) {
        ValidateDblink(usr, desc);
    }
}


// for MolInfo validation that does not rely on contents of sequence
void CValidError_desc::ValidateMolInfo
(const CMolInfo& minfo,
 const CSeqdesc& desc)
{
    if ( !minfo.IsSetBiomol() || minfo.GetBiomol() == CMolInfo::eBiomol_unknown) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
            "Molinfo-biomol unknown used", *m_Ctx, desc);
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
