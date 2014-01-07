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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 */


#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/valid/Field_rule.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Field_set.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <util/util_misc.hpp>

#include <objtools/edit/struc_comm_field.hpp>
#include <objtools/edit/seqid_guesser.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(ncbi::objects);
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


const string kStructuredComment = "StructuredComment";
const string kStructuredCommentPrefix = "StructuredCommentPrefix";
const string kStructuredCommentSuffix = "StructuredCommentSuffix";

bool CStructuredCommentField::SetVal(CObject& object, const string & newValue, EExistingText existing_text)
{
    bool rval = false;
    CSeqdesc* seqdesc = dynamic_cast<CSeqdesc*>(&object);
    CUser_object* user = dynamic_cast<CUser_object*>(&object);

    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->SetUser());
    }
    if (user && IsStructuredCommentForThisField(*user)) {
        bool found = false;
        if (user->IsSetData()) {
            CUser_object::TData::iterator it = user->SetData().begin();
            while (it != user->SetData().end()) {
                if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()
                    && NStr::Equal((*it)->GetLabel().GetStr(), m_FieldName)) {
                    rval |= SetVal(**it, newValue, existing_text);
                    found = true;
                }
                if (!(*it)->IsSetData()) {
                    it = user->SetData().erase(it);
                } else {
                    it++;
                }
            }
        }
        if (!found && (!NStr::Equal(m_ConstraintFieldName, m_FieldName) || !m_StringConstraint)) {
            CRef<CUser_field> new_field(new CUser_field());
            new_field->SetLabel().SetStr(m_FieldName);
            if (SetVal(*new_field, newValue, eExistingText_replace_old)) {
                x_InsertFieldAtCorrectPosition(*user, new_field);
                rval = true;
            }
        }
        
        // if User object now has no fields, reset so it will be detected as empty
        if (user->GetData().empty()) {
            user->ResetData();
        }
    }
    return rval;
}


/// Assumes that User-object fields are already mostly in order
void CStructuredCommentField::x_InsertFieldAtCorrectPosition(CUser_object& user, CRef<CUser_field> field)
{
    if (!field) {
        return;
    }
    if (!user.IsSetData()) {
        // no fields yet, just add the field
        user.SetData().push_back(field);
        return;
    }
    string this_field_label = field->GetLabel().GetStr();

    vector<string> field_names = CComment_set::GetFieldNames(m_Prefix);
    if (field_names.size() == 0) {
        // no information about field order, just add to end
        user.SetData().push_back(field);
        return;
    }

    vector<string>::iterator sit = field_names.begin();
    CUser_object::TData::iterator fit = user.SetData().begin();
    while (sit != field_names.end() && fit != user.SetData().end()) {
        string field_label = (*fit)->GetLabel().GetStr();
        if (NStr::EqualNocase(field_label, kStructuredCommentPrefix)
            || NStr::EqualNocase(field_label, kStructuredCommentSuffix)) {
            // skip
            fit++;
        } else if (NStr::EqualNocase(*sit, (*fit)->GetLabel().GetStr())) {
            sit++;
            fit++;
        } else if (NStr::EqualNocase(*sit, this_field_label)) {
            // insert field here
            user.SetData().insert(fit, field);
            return;
        } else {
            // field is missing
            sit++;
        }
    }
    user.SetData().push_back(field);
}


bool CStructuredCommentField::SetVal(CUser_field& field, const string & newValue, EExistingText existing_text)
{
    bool rval = false;
    
    if (field.IsSetData()) {
        if (field.GetData().IsStr()) {
            string curr_val = field.GetData().GetStr();
            if (!NStr::Equal(m_ConstraintFieldName, m_FieldName) || !m_StringConstraint
                || m_StringConstraint->DoesTextMatch(curr_val)) {
                if (AddValueToString(curr_val, newValue, existing_text)) {
                    field.SetData().SetStr(curr_val);
                    rval = true;
                }
            }
        } else if (field.GetData().Which() == CUser_field::TData::e_not_set) {
            if (!NStr::Equal(m_ConstraintFieldName, m_FieldName) || !m_StringConstraint) {
                field.SetData().SetStr(newValue);
                rval = true;
            }
        }
    } else if (!NStr::Equal(m_ConstraintFieldName, m_FieldName) || !m_StringConstraint) {
        field.SetData().SetStr(newValue);
        rval = true;
    }

    return rval;
}


string CStructuredCommentField::GetVal( const CObject& object)
{
    vector<string> vals = GetVals(object);
    if (vals.size() > 0) {
        return vals[0];
    } else {
        return "";
    }

}


vector<string> CStructuredCommentField::GetVals(const CObject& object)
{
    vector<string> vals;
    const CSeqdesc* seqdesc = dynamic_cast<const CSeqdesc*>(&object);
    const CUser_object* user = dynamic_cast<const CUser_object*>(&object);

    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->GetUser());
    }
    if (IsStructuredCommentForThisField(*user) && user->IsSetData()) {
        CUser_object::TData::const_iterator it = user->GetData().begin();
        while (it != user->GetData().end()) {
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && (*it)->IsSetData() 
                && NStr::Equal((*it)->GetLabel().GetStr(), m_FieldName)) {
                switch((*it)->GetData().Which()) {
                    case CUser_field::TData::e_Str:
                        vals.push_back((*it)->GetData().GetStr());
                        break;
                    case CUser_field::TData::e_Strs:
                        ITERATE(CUser_field::TData::TStrs, s, (*it)->GetData().GetStrs()) {
                            vals.push_back(*s);
                        }
                        break;
                    default:
                        //not handling other types of fields
                        break;
                }
            }
            ++it;
        }
    }
    return vals;
}


void CStructuredCommentField::SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint)
{
    m_ConstraintFieldName = field_name;
    if (NStr::IsBlank(field_name)) {
        string_constraint.Reset(NULL);
    } else {
        m_StringConstraint = new CStringConstraint(" ");
        m_StringConstraint->Assign(*string_constraint);
    }
}


vector<CConstRef<CObject> > CStructuredCommentField::GetObjects(CBioseq_Handle bsh)
{
    vector<CConstRef<CObject> > objects;

    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
    while (desc_ci) {
        if (IsStructuredCommentForThisField(desc_ci->GetUser())) {
            CConstRef<CObject> object;
            object.Reset(&(*desc_ci));
            objects.push_back(object);
        }
        ++desc_ci;
    }

    return objects;
}


vector<CRef<CApplyObject> > CStructuredCommentField::GetApplyObjects(CBioseq_Handle bsh)
{
    vector<CRef<CApplyObject> > objects;

    // add existing descriptors
    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
    while (desc_ci) {
        if (IsStructuredCommentForThisField(desc_ci->GetUser())) {
            CRef<CApplyObject> obj(new CApplyObject(bsh, *desc_ci));
            objects.push_back(obj);
        }
        ++desc_ci;
    }

    if (objects.empty()) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetUser(*MakeUserObject(m_Prefix));
        CRef<CApplyObject> new_obj(new CApplyObject(bsh, *desc));
        objects.push_back(new_obj);
    }        

    return objects;
}


vector<CConstRef<CObject> > CStructuredCommentField::GetObjects(CSeq_entry_Handle seh, const string& constraint_field, CRef<CStringConstraint> string_constraint)
{
    vector<CConstRef<CObject> > objs;
    CRef<CScope> scope(&seh.GetScope());

    CBioseq_CI bi (seh, objects::CSeq_inst::eMol_na);
    while (bi) {
        if (NStr::EqualNocase(constraint_field, kFieldTypeSeqId)) {
            if (CSeqIdGuesser::DoesSeqMatchConstraint(*bi, string_constraint)) {
                vector<CConstRef<CObject> > these_objs = GetObjects(*bi);
                objs.insert(objs.end(), these_objs.begin(), these_objs.end());
            }
        } else {
            vector<CConstRef<CObject> > these_objs = GetObjects(*bi);
            ITERATE (vector<CConstRef<CObject> >, it, these_objs) {
                if (DoesObjectMatchFieldConstraint (**it, constraint_field, string_constraint, scope)) {
                    objs.push_back (*it);
                }
            }
        }
        ++bi;
    }

    return objs;
}


bool CStructuredCommentField::IsEmpty(const CObject& object) const
{
    bool rval = false;
    const CSeqdesc* seqdesc = dynamic_cast<const CSeqdesc*>(&object);
    const CUser_object* user = dynamic_cast<const CUser_object*>(&object);
    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->GetUser());
    }
    if (user && IsStructuredCommentForThisField(*user)) {
        if (!user->IsSetData() || user->GetData().empty()) {
            // empty if no fields
            rval = true;
        } else {
            // empty if no fields other than prefix and/or suffix
            rval = true;
            ITERATE(CUser_object::TData, it, user->GetData()) {
                if (!(*it)->IsSetLabel()
                    || !(*it)->GetLabel().IsStr()) {
                    // field with no label or non-string label,
                    // clearly neither prefix nor suffix
                    rval = false;
                    break;
                } else {
                    string label = (*it)->GetLabel().GetStr();
                    if (!NStr::Equal(label, kStructuredCommentPrefix)
                        && !NStr::Equal(label, kStructuredCommentSuffix)) {
                        rval = false;
                        break;
                    }
                }
            }
        }
    }

    return rval;
}


void CStructuredCommentField::ClearVal(CObject& object)
{
    CSeqdesc* seqdesc = dynamic_cast<CSeqdesc*>(&object);
    CUser_object* user = dynamic_cast<CUser_object*>(&object);

    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->SetUser());
    }
    if (user && user->IsSetData()) {
        CUser_object::TData::iterator it = user->SetData().begin();
        while (it != user->SetData().end()) {
            bool do_erase = false;
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && NStr::Equal((*it)->GetLabel().GetStr(), m_FieldName)) {
                do_erase = true;
            }
            if (do_erase) {
                it = user->SetData().erase(it);
            } else {
                it++;
            }
        }
        if (user->GetData().empty()) {
            user->ResetData();
        }
    }
}


vector<CConstRef<CObject> > CStructuredCommentField::GetRelatedObjects(const CObject& object, CRef<CScope> scope)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&object);

   if (obj_feat) {
        // find closest related Structured Comment Objects
        CBioseq_Handle bsh = scope->GetBioseqHandle(obj_feat->GetLocation());
        related = GetObjects(bsh);
    } else if (obj_desc) {
        if (obj_desc->IsUser() && IsStructuredCommentForThisField(obj_desc->GetUser())) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            CSeq_entry_Handle seh = GetSeqEntryForSeqdesc(scope, *obj_desc);
            related = GetObjects(seh, m_ConstraintFieldName, m_StringConstraint);
        }
    }

    return related;
}


vector<CConstRef<CObject> > CStructuredCommentField::GetRelatedObjects(const CApplyObject& object)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&object);

   if (obj_feat) {
        related = GetObjects(object.GetSEH(), "", CRef<CStringConstraint>(NULL));
    } else if (obj_desc) {
        if (obj_desc->IsUser() && IsStructuredCommentForThisField(obj_desc->GetUser())) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            related = GetObjects(object.GetSEH(), m_ConstraintFieldName, m_StringConstraint);
        }
    }

    return related;
}


bool CStructuredCommentField::IsStructuredCommentForThisField (const CUser_object& user) const
{
    if (!CComment_rule::IsStructuredComment(user)) {
        return false;
    }
    string prefix = CComment_rule::GetStructuredCommentPrefix(user);
    CComment_rule::NormalizePrefix(prefix);
    return NStr::Equal(prefix, m_Prefix);
}


CRef<CUser_object> CStructuredCommentField::MakeUserObject(const string& prefix)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr(kStructuredComment);

    if (!NStr::IsBlank(prefix)) {
        string root = prefix;
        CComment_rule::NormalizePrefix(root);
        CRef<CUser_field> p(new CUser_field());
        p->SetLabel().SetStr(kStructuredCommentPrefix);
        string pre = CComment_rule::MakePrefixFromRoot(root);
        p->SetData().SetStr(pre);
        obj->SetData().push_back(p);
        CRef<CUser_field> s(new CUser_field());
        s->SetLabel().SetStr(kStructuredCommentSuffix);
        string suf = CComment_rule::MakeSuffixFromRoot(root);
        s->SetData().SetStr(suf);
        obj->SetData().push_back(s);
    }
    return obj;
}


const string kGenomeAssemblyData = "Genome-Assembly-Data";
const string kAssemblyMethod = "Assembly Method";
const string kGenomeCoverage = "Genome Coverage";
const string kSequencingTechnology = "Sequencing Technology";

CGenomeAssemblyComment::CGenomeAssemblyComment()
{
    m_User = MakeEmptyUserObject();
}


CGenomeAssemblyComment::CGenomeAssemblyComment(CUser_object& user)
{
    m_User.Reset(new CUser_object());
    m_User->Assign(user);
}


CRef<CUser_object> CGenomeAssemblyComment::MakeEmptyUserObject()
{
    CRef<CUser_object> obj = CStructuredCommentField::MakeUserObject(kGenomeAssemblyData);
    return obj;
}


void CGenomeAssemblyComment::SetAssemblyMethod(CUser_object& obj, string val, EExistingText existing_text)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    field.SetVal(obj, val, existing_text);
}


void CGenomeAssemblyComment::x_GetAssemblyMethodProgramAndVersion(string val, string& program, string& version)
{
    program = val;
    version = "";
    size_t pos = NStr::Find(val, " v.");
    if (pos != string::npos) {
        program = val.substr(0, pos);
        version = val.substr(pos + 3);
        NStr::TruncateSpacesInPlace(program);
        NStr::TruncateSpacesInPlace(version);
    }
}


string CGenomeAssemblyComment::x_GetAssemblyMethodFromProgramAndVersion(const string& program, const string& version)
{
    string new_val = program;
    if (!NStr::IsBlank(version)) {
        if (!NStr::IsBlank(program)) {
            new_val += " ";
        }
        new_val += "v. ";
        new_val += version;
    }
    return new_val;
}


void CGenomeAssemblyComment::SetAssemblyMethodProgram(CUser_object& obj, string val, EExistingText existing_text)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    string previous = field.GetVal(obj);
    string program = "";
    string version = "";
    x_GetAssemblyMethodProgramAndVersion(previous, program, version);
    if (AddValueToString(program, val, existing_text)) {
        string new_val = x_GetAssemblyMethodFromProgramAndVersion(program, version);
        field.SetVal(obj, new_val, eExistingText_replace_old);
    }
}


void CGenomeAssemblyComment::SetAssemblyMethodVersion(CUser_object& obj, string val, EExistingText existing_text)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    string previous = field.GetVal(obj);
    string program = "";
    string version = "";
    x_GetAssemblyMethodProgramAndVersion(previous, program, version);
    if (AddValueToString(version, val, existing_text)) {
        string new_val = x_GetAssemblyMethodFromProgramAndVersion(program, version);
        field.SetVal(obj, new_val, eExistingText_replace_old);
    }
}


void CGenomeAssemblyComment::SetGenomeCoverage(CUser_object& obj, string val, EExistingText existing_text)
{
    CStructuredCommentField field(kGenomeAssemblyData, kGenomeCoverage);
    field.SetVal(obj, val, existing_text);
}


void CGenomeAssemblyComment::SetSequencingTechnology(CUser_object& obj, string val, EExistingText existing_text)
{
    CStructuredCommentField field(kGenomeAssemblyData, kSequencingTechnology);
    field.SetVal(obj, val, existing_text);
}


string CGenomeAssemblyComment::GetAssemblyMethod(CUser_object& obj)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    return field.GetVal(obj);
}


string CGenomeAssemblyComment::GetAssemblyMethodProgram(CUser_object& obj)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    string method = field.GetVal(obj);
    string program = "";
    string version = "";
    x_GetAssemblyMethodProgramAndVersion(method, program, version);
    return program;
}


string CGenomeAssemblyComment::GetAssemblyMethodVersion(CUser_object& obj)
{
    CStructuredCommentField field(kGenomeAssemblyData, kAssemblyMethod);
    string method = field.GetVal(obj);
    string program = "";
    string version = "";
    x_GetAssemblyMethodProgramAndVersion(method, program, version);
    return version;
}


string CGenomeAssemblyComment::GetGenomeCoverage(CUser_object& obj)
{
    CStructuredCommentField field(kGenomeAssemblyData, kGenomeCoverage);
    return field.GetVal(obj);
}


string CGenomeAssemblyComment::GetSequencingTechnology(CUser_object& obj)
{
    CStructuredCommentField field(kGenomeAssemblyData, kSequencingTechnology);
    return field.GetVal(obj);
}


CGenomeAssemblyComment& CGenomeAssemblyComment::SetAssemblyMethod(string val, EExistingText existing_text)
{
    SetAssemblyMethod(*m_User, val, existing_text);
    return *this;
}


CGenomeAssemblyComment& CGenomeAssemblyComment::SetAssemblyMethodProgram(string val, EExistingText existing_text)
{
    SetAssemblyMethodProgram(*m_User, val, existing_text);
    return *this;
}


CGenomeAssemblyComment& CGenomeAssemblyComment::SetAssemblyMethodVersion(string val, EExistingText existing_text)
{
    SetAssemblyMethodVersion(*m_User, val, existing_text);
    return *this;
}


CGenomeAssemblyComment& CGenomeAssemblyComment::SetGenomeCoverage(string val, EExistingText existing_text)
{
    SetGenomeCoverage(*m_User, val, existing_text);
    return *this;
}


CGenomeAssemblyComment& CGenomeAssemblyComment::SetSequencingTechnology(string val, EExistingText existing_text)
{
    SetSequencingTechnology(*m_User, val, existing_text);
    return *this;
}


CRef<CUser_object> CGenomeAssemblyComment::MakeUserObject()
{
    CRef<CUser_object> obj(new CUser_object());
    obj->Assign(*m_User);
    return obj;
}




END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

