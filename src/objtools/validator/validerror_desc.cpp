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
#include "validatorp.hpp"
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
        case CSeqdesc::e_Mol_type:
        case CSeqdesc::e_Method:
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                desc.SelectionName(desc.Which()) + " descriptor is obsolete", *m_Ctx, desc);
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


void CValidError_desc::ValidateStructuredComment
(const CUser_object& usr,
 const CSeqdesc& desc)
{
    if (!usr.IsSetType() || !usr.GetType().IsStr()
        || !NStr::EqualCase(usr.GetType().GetStr(), "StructuredComment")) {
        return;
    }
    if (!usr.IsSetData() || usr.GetData().size() == 0) {
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_UserObjectProblem, 
                 "Structured Comment user object descriptor is empty", *m_Ctx, desc);
        return;
    }

    // find prefix
    try {
        const CUser_field& prefix = usr.GetField("StructuredCommentPrefix");
        if (!prefix.IsSetData() || !prefix.GetData().IsStr()) {
            return;
        }
        CRef<CComment_set> comment_rules = m_Imp.GetStructuredCommentRules();
        if (comment_rules) {
            const CComment_rule& rule = comment_rules->FindCommentRule(prefix.GetData().GetStr());
            CField_set::Tdata::const_iterator field_rule = rule.GetFields().Get().begin();
            CUser_object::TData::const_iterator field = usr.GetData().begin();
            while (field_rule != rule.GetFields().Get().end()
                   && field != usr.GetData().end()) {
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
                            // post error about not matching format
                            PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                     eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                     value + " is not a valid value for " + label, *m_Ctx, desc);                            
                        }
                        ++field;
                        ++field_rule;
                    } else {
                        // find field for this rule and validate it
                        try {
                            const CUser_field& other_field = usr.GetField(expected_field);
                            PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                     eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                     expected_field + " field is out of order", *m_Ctx, desc);
                            string value = "";
                            if (other_field.GetData().IsStr()) {
                                value = (other_field.GetData().GetStr());
                            } else if (other_field.GetData().IsInt()) {
                                value = NStr::IntToString(other_field.GetData().GetInt());
                            }
                            if (!(*field_rule)->DoesStringMatchRuleExpression(value)) {
                                // post error about not matching format
                                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                         eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                         value + " is not a valid value for " + expected_field, *m_Ctx, desc);                            
                            }

                        } catch (CException ) {
                            if ((*field_rule)->IsSetRequired() && (*field_rule)->GetRequired()) {
                                PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                                        eErr_SEQ_DESCR_BadStructuredCommentFormat,
                                        "Required field " + (*field_rule)->GetField_name() + " is missing",
                                        *m_Ctx, desc);
                            }
                        }
                        ++field_rule;
                    }
                } else {
                    // post error about field without label
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                             "Structured Comment contains field without label", *m_Ctx, desc);                            
                    ++field;
                }
            }

            while (field_rule != rule.GetFields().Get().end()) {
                if ((*field_rule)->IsSetRequired() && (*field_rule)->GetRequired()) {
                    PostErr (s_ErrorLevelFromFieldRuleSev((*field_rule)->GetSeverity()),
                            eErr_SEQ_DESCR_BadStructuredCommentFormat,
                            "Required field " + (*field_rule)->GetField_name() + " is missing",
                            *m_Ctx, desc);
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
                            // post error about multiple field values
                            PostErr (s_ErrorLevelFromFieldRuleSev(field_rule.GetSeverity()),
                                     eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                     "Multiple values for " + label + " field", *m_Ctx, desc);                            
                        }
                    } catch (CException ) {
                        // field not found, not legitimate field name
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                 label + " is not a valid field name", *m_Ctx, desc);                            
                    }

                } else {
                    // post error about field without label
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                             "Structured Comment contains field without label", *m_Ctx, desc);                            
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
                    if ((*depend_rule)->DoesStringMatchRuleExpression(value)) {
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
                                    // post error about not matching format
                                    PostErr (s_ErrorLevelFromFieldRuleSev((*other_rule)->GetSeverity()),
                                             eErr_SEQ_DESCR_BadStructuredCommentFormat, 
                                             other_value + " is not a valid value for " + other_field_name
                                             + " when " + depend_field_name + " has value '" + value + "'",                                             
                                             *m_Ctx, desc);                            
                                }

                            } catch (CException) {
                                // unable to find field
                                if ((*other_rule)->IsSetRequired() && (*other_rule)->GetRequired()) {
                                    PostErr (s_ErrorLevelFromFieldRuleSev((*other_rule)->GetSeverity()),
                                            eErr_SEQ_DESCR_BadStructuredCommentFormat,
                                            "Required field " + (*other_rule)->GetField_name() + " is missing when "
                                            + depend_field_name + " has value '" + value + "'",
                                            *m_Ctx, desc);
                                }
                            }
                        }
                    }

                } catch (CException ) {
                    // field not found
                }
            }
        }            
    } catch (CException ) {
        // might not have prefix, in which case no rules
        // might not have rule for that prefix
    }

}


void CValidError_desc::ValidateUser
(const CUser_object& usr,
 const CSeqdesc& desc)
{
    if ( !usr.CanGetType() ) {
        return;
    }
    const CObject_id& oi = usr.GetType();
    if ( !oi.IsStr() ) {
        return;
    }
    if ( NStr::EqualNocase(oi.GetStr(), "RefGeneTracking")) {
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
    } else if ( NStr::EqualCase(oi.GetStr(), "StructuredComment")) {
        ValidateStructuredComment(usr, desc);
    }
}


void CValidError_desc::ValidateMolInfo
(const CMolInfo& minfo,
 const CSeqdesc& desc)
{
    if ( !minfo.IsSetBiomol() ) {
        return;
    }

    int biomol = minfo.GetBiomol();

    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
            "Molinfo-biomol unknown used", *m_Ctx, desc);
        break;

    case CMolInfo::eBiomol_other:
        if ( !m_Imp.IsXR() ) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "Molinfo-biomol other used", *m_Ctx, desc);
        }
        break;

    default:
        break;
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
