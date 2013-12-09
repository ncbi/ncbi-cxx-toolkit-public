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
                user->SetData().push_back(new_field);
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
    if (!IsStructuredComment(user)) {
        return false;
    }
    string prefix = GetPrefix(user);
    NormalizePrefix(prefix);
    return NStr::Equal(prefix, m_Prefix);
}


bool CStructuredCommentField::IsStructuredComment (const CUser_object& user)
{
    if (user.IsSetType()
        && user.GetType().IsStr()
        && NStr::EqualNocase(user.GetType().GetStr(),kStructuredComment)) {
        return true;
    } else {
        return false;
    }
}


void CStructuredCommentField::NormalizePrefix(string& prefix)
{
    if (!NStr::IsBlank(prefix)) {
        while (NStr::StartsWith(prefix, "#")) {
            prefix = prefix.substr(1);
        }
        while (NStr::EndsWith(prefix, "#")) {
            prefix = prefix.substr(0, prefix.length() - 1);
        }
        if (NStr::EndsWith(prefix, "-START", NStr::eNocase)) {
            prefix = prefix.substr(0, prefix.length() - 6);
        } else if (NStr::EndsWith(prefix, "-END", NStr::eNocase)) {
            prefix = prefix.substr(0, prefix.length() - 4);
        }
    }  
}


string CStructuredCommentField::GetPrefix (const CUser_object& user)
{
    if (!IsStructuredComment(user) || !user.IsSetData()) {
        return "";
    }
    string prefix = "";
    ITERATE(CUser_object::TData, it, user.GetData()) {
        if ((*it)->IsSetData() && (*it)->GetData().IsStr()
            && (*it)->IsSetLabel() && (*it)->GetLabel().IsStr()
            && (NStr::Equal((*it)->GetLabel().GetStr(), kStructuredCommentPrefix)
                || NStr::Equal((*it)->GetLabel().GetStr(), kStructuredCommentSuffix))) {
            prefix = (*it)->GetData().GetStr();
            break;
        }
    }
    NormalizePrefix(prefix);
    return prefix;
}


static bool s_FieldRuleCompare (
    const CRef<CField_rule>& p1,
    const CRef<CField_rule>& p2
)

{
    return NStr::Compare(p1->GetField_name(), p2->GetField_name()) < 0;
}


#if 0
CRef<CComment_set> ReadCommentRules()
{
    CRef<CComment_set> rules(NULL);

    string fname = g_FindDataFile("E:/ncbi/data/validrules.prt");
    if (fname.empty()) {
        // do nothing
    } else {
        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Open(fname, eSerial_AsnText));
        string header = in->ReadFileHeader();
        rules.Reset(new CComment_set());
        in->Read(ObjectInfo(*rules), CObjectIStream::eNoFileHeader);        
        if (rules->IsSet()) {
            NON_CONST_ITERATE(CComment_set::Tdata, it, rules->Set()) {
                if (!(*it)->GetRequire_order() && (*it)->IsSetFields()) {
                    CField_set& fields = (*it)->SetFields();
                    fields.Set().sort(s_FieldRuleCompare);
                }
            }
        }       
    }
    return rules;
}
#endif


vector<string> CStructuredCommentField::GetFieldNames(const string& prefix)
{
    vector<string> options;

#if 0
    string prefix_to_use = prefix;
    NormalizePrefix(prefix_to_use);

    // look up mandatory and required field names from validator rules
    CRef<CComment_set> rules = ReadCommentRules();

    if (rules) {
        try {
            const CComment_rule& rule = rules->FindCommentRule(prefix_to_use);
            ITERATE(CComment_rule::TFields::Tdata, it, rule.GetFields().Get()) {
                options.push_back((*it)->GetField_name());
            }
        } catch (CException ) {
            // no rule for this prefix, can't list fields
        }
    }
#endif
    return options;
}


CRef<CUser_object> CStructuredCommentField::MakeUserObject(const string& prefix)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr(kStructuredComment);

    if (!NStr::IsBlank(prefix)) {
        string value = prefix;
        NormalizePrefix(value);
        CRef<CUser_field> p(new CUser_field());
        p->SetLabel().SetStr(kStructuredCommentPrefix);
        p->SetData().SetStr("##" + value + "-START##");
        obj->SetData().push_back(p);
        CRef<CUser_field> s(new CUser_field());
        s->SetLabel().SetStr(kStructuredCommentSuffix);
        s->SetData().SetStr("##" + value + "-END##");
        obj->SetData().push_back(s);
    }
    return obj;
}


const string kGenomeAssemblyData = "Genome-Assembly-Data";
CRef<CUser_object> CGenomeAssemblyComment::MakeUserObject()
{
    CRef<CUser_object> obj = CStructuredCommentField::MakeUserObject(kGenomeAssemblyData);
    return obj;
}


void CGenomeAssemblyComment::SetAssemblyMethod(CUser_object& obj, string val)
{
    CStructuredCommentField field(kGenomeAssemblyData, "Assembly Method");
    field.SetVal(obj, val);
}


void CGenomeAssemblyComment::SetGenomeCoverage(CUser_object& obj, string val)
{
    CStructuredCommentField field(kGenomeAssemblyData, "Genome Coverage");
    field.SetVal(obj, val);
}


void CGenomeAssemblyComment::SetSequencingTechnology(CUser_object& obj, string val)
{
    CStructuredCommentField field(kGenomeAssemblyData, "Sequencing Technology");
    field.SetVal(obj, val);
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
