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

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <objtools/edit/dblink_field.hpp>
#include <objtools/edit/seqid_guesser.hpp>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(ncbi::objects);
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


static const char* kDBLink = "DBLink";


bool CDBLinkField::SetVal(CObject& object, const string & newValue, EExistingText existing_text)
{
    bool rval = false;
    CSerialObject* serial = dynamic_cast<CSerialObject*>(&object);

    if (serial)
    {
        if (serial->GetThisTypeInfo() == CSeqdesc::GetTypeInfo())
        {
            rval = SetVal(static_cast<CSeqdesc&>(*serial), newValue, existing_text);
        }
        else
        if (serial->GetThisTypeInfo() == CUser_object::GetTypeInfo())
        {
            rval = SetVal(static_cast<CUser_object&>(*serial), newValue, existing_text);
        }
    }
    return rval;
}

bool CDBLinkField::SetVal(CSeqdesc& seqdesc, const string & newValue, EExistingText existing_text)
{
    bool rval = false;
    if (seqdesc.IsUser()) {
        rval = SetVal(seqdesc.SetUser(), newValue, existing_text);
    }

    return rval;
}

bool CDBLinkField::SetVal(CUser_object& user, const string & newValue, EExistingText existing_text)
{
    bool rval = false;
    if (IsDBLink(user)) {
        bool found = false;
        if (user.IsSetData()) {
            CUser_object::TData::iterator it = user.SetData().begin();
            while (it != user.SetData().end()) {
                if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()) {
                    EDBLinkFieldType check = GetTypeForLabel((*it)->GetLabel().GetStr());
                    if (check == m_FieldType) {
                        rval |= SetVal(**it, newValue, existing_text);
                        found = true;
                    }
                }
                if (!(*it)->IsSetData()) {
                    it = user.SetData().erase(it);
                } else {
                    it++;
                }
            }
        }
        if (!found && (m_ConstraintFieldType != m_FieldType || !m_StringConstraint)) {
            CRef<CUser_field> new_field(new CUser_field());
            new_field->SetLabel().SetStr(GetLabelForType(m_FieldType));
            if (SetVal(*new_field, newValue, eExistingText_replace_old)) {
                user.SetData().push_back(new_field);
                rval = true;
            }
        }

        // if User object now has no fields, reset so it will be detected as empty
        if (user.GetData().empty()) {
            user.ResetData();
        }
    }
    return rval;
}


void CDBLinkField::_ParseAndAppend(CUser_field::C_Data::TStrs& strs, const string & newValue, EExistingText existing_text)
{
    vector<CTempStringEx> l;
    NStr::Split(newValue, ",", l);
    if (existing_text == eExistingText_replace_old || strs.empty())
    {
        strs.clear();
        for (auto v : l)
        {
            strs.push_back(v);
        }
    }
    else
    {
        for (auto v : l)
        {
            if (existing_text == eExistingText_add_qual)
            {
                strs.push_back(v);
            }
            else
            for (auto s : strs)
            {
                if (m_ConstraintFieldType != m_FieldType || !m_StringConstraint  || m_StringConstraint->DoesTextMatch(s))
                   AddValueToString(s, v, existing_text);
            }
        }
    }
}

bool CDBLinkField::SetVal(CUser_field& field, const string & newValue, EExistingText existing_text)
{
    bool rval = false;

    if (field.IsSetData()) {
#if 0
        if (field.GetData().IsStr()) {
            string curr_val = field.GetData().GetStr();
            if (m_ConstraintFieldType != m_FieldType || !m_StringConstraint
                || m_StringConstraint->DoesTextMatch(curr_val)) {
                CUser_field::C_Data::TStrs& strs = field.SetData().SetStrs();
                if (existing_text == eExistingText_add_qual) {
                    _ParseAndAppend(strs, curr_val);
                    _ParseAndAppend(strs, newValue);
                    rval = true;
                } else if (AddValueToString(curr_val, newValue, existing_text)) {
                    field.SetData().SetStrs().push_back(curr_val);
                    rval = true;
                }
            }
        } else if (field.GetData().IsStrs()) {
            if ((existing_text == eExistingText_add_qual || field.GetData().GetStrs().empty())
                && (m_ConstraintFieldType != m_FieldType || !m_StringConstraint)) {
                field.SetData().SetStrs().push_back(newValue);
                rval = true;
            } else {
                NON_CONST_ITERATE(CUser_field::TData::TStrs, s, field.SetData().SetStrs()) {
                    if (m_ConstraintFieldType != m_FieldType || !m_StringConstraint
                        || m_StringConstraint->DoesTextMatch(*s)) {
                        rval |= AddValueToString(*s, newValue, existing_text);
                    }
                }
            }
        } else if (field.GetData().Which() == CUser_field::TData::e_not_set) {
            if (m_ConstraintFieldType != m_FieldType || !m_StringConstraint) {
                field.SetData().SetStrs().push_back(newValue);
                rval = true;
            }
        }
#else
        // first convert older format into newer
        if (field.GetData().IsStr()) {
            string curr_val = field.GetData().GetStr();
            CUser_field::C_Data::TStrs& strs = field.SetData().SetStrs();
            _ParseAndAppend(strs, curr_val, eExistingText_replace_old);
        }
        CUser_field::C_Data::TStrs& strs = field.SetData().SetStrs();
        _ParseAndAppend(strs, newValue, existing_text);
        rval = true;
#endif
    } else if (m_ConstraintFieldType != m_FieldType || !m_StringConstraint) {
        _ParseAndAppend(field.SetData().SetStrs(), newValue, eExistingText_replace_old);
        rval = true;
    }

    if (field.IsSetData() && field.GetData().IsStrs())
    {
        field.SetNum(CUser_field::TNum(field.GetData().GetStrs().size()));
    }
    return rval;
}


string CDBLinkField::GetVal( const CObject& object)
{
    vector<string> vals = GetVals(object);
    if (vals.size() > 0) {
        return vals[0];
    } else {
        return "";
    }

}


vector<string> CDBLinkField::GetVals(const CObject& object)
{
    vector<string> vals;
    const CSeqdesc* seqdesc = dynamic_cast<const CSeqdesc*>(&object);
    const CUser_object* user = dynamic_cast<const CUser_object*>(&object);

    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->GetUser());
    }
    if (IsDBLink(*user) && user->IsSetData()) {
        CUser_object::TData::const_iterator it = user->GetData().begin();
        while (it != user->GetData().end()) {
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && (*it)->IsSetData()) {
                EDBLinkFieldType check = GetTypeForLabel((*it)->GetLabel().GetStr());
                if (check == m_FieldType) {
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
            }
            ++it;
        }
    }
    return vals;
}


string CDBLinkField::GetLabel() const
{
    return "DBLink " + GetLabelForType(m_FieldType);
}


void CDBLinkField::SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint)
{
    // constraints only apply for authors, since there could be more than one
    m_ConstraintFieldType = GetTypeForLabel(field_name);
    if (m_ConstraintFieldType == eDBLinkFieldType_Unknown || !string_constraint) {
        string_constraint.Reset();
    } else {
        m_StringConstraint = new CStringConstraint(" ");
        m_StringConstraint->Assign(*string_constraint);
    }
}



MAKE_TWOWAY_CONST_MAP(kMapTypeToLabel, CDBLinkField::EDBLinkFieldType, ct::tagStrNocase,
{
    { CDBLinkField::eDBLinkFieldType_Trace,     "Trace Assembly Archive" },
    { CDBLinkField::eDBLinkFieldType_BioSample, "BioSample" },
    { CDBLinkField::eDBLinkFieldType_ProbeDB,   "ProbeDB" },
    { CDBLinkField::eDBLinkFieldType_SRA,       "Sequence Read Archive" },
    { CDBLinkField::eDBLinkFieldType_BioProject,"BioProject" },
    { CDBLinkField::eDBLinkFieldType_Assembly,  "Assembly" },
    { CDBLinkField::eDBLinkFieldType_Unknown,   "Unknown" },
});


CDBLinkField::EDBLinkFieldType CDBLinkField::GetTypeForLabel(const string& label)
{
    auto type_it = kMapTypeToLabel.second.find(label);
    if (type_it == kMapTypeToLabel.second.end()) {
        return eDBLinkFieldType_Unknown;
    }
    return type_it->second;
}


const string CDBLinkField::GetLabelForType(EDBLinkFieldType field_type)
{
    auto type_it = kMapTypeToLabel.first.find(field_type);
    if (type_it == kMapTypeToLabel.first.end()) {
        return kEmptyStr;
    }
    return type_it->second;
}


vector<CConstRef<CObject> > CDBLinkField::GetObjects(CBioseq_Handle bsh)
{
    vector<CConstRef<CObject> > objects;

    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
    while (desc_ci) {
        if (IsDBLink(desc_ci->GetUser())) {
            CConstRef<CObject> object;
            object.Reset(&(*desc_ci));
            objects.push_back(object);
        }
        ++desc_ci;
    }

    return objects;
}


vector<CRef<CApplyObject> > CDBLinkField::GetApplyObjects(CBioseq_Handle bsh)
{
    vector<CRef<CApplyObject> > objects;

    // add existing descriptors
    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User);
    while (desc_ci) {
        if (IsDBLink(desc_ci->GetUser())) {
            CRef<CApplyObject> obj(new CApplyObject(bsh, *desc_ci));
            objects.push_back(obj);
        }
        ++desc_ci;
    }

    if (objects.empty()) {
        CRef<CApplyObject> new_obj(new CApplyObject(bsh, kDBLink));
        objects.push_back(new_obj);
    }

    return objects;
}


vector<CConstRef<CObject> > CDBLinkField::GetObjects(CSeq_entry_Handle seh, const string& constraint_field, CRef<CStringConstraint> string_constraint)
{
    vector<CConstRef<CObject> > objs;
    CRef<CScope> scope(&seh.GetScope());

    CBioseq_CI bi (seh, CSeq_inst::eMol_na);
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


bool CDBLinkField::IsEmpty(const CObject& object) const
{
    bool rval = false;
    const CSeqdesc* seqdesc = dynamic_cast<const CSeqdesc*>(&object);
    const CUser_object* user = dynamic_cast<const CUser_object*>(&object);
    if (seqdesc && seqdesc->IsUser()) {
        user = &(seqdesc->GetUser());
    }
    if (user && IsDBLink(*user)) {
        if (!user->IsSetData() || user->GetData().empty()) {
            rval = true;
        }
    }

    return rval;
}


void CDBLinkField::ClearVal(CObject& object)
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
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()) {
                EDBLinkFieldType check = GetTypeForLabel((*it)->GetLabel().GetStr());
                if (check == m_FieldType) {
                    do_erase = true;
                }
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


vector<CConstRef<CObject> > CDBLinkField::GetRelatedObjects(const CObject& object, CRef<CScope> scope)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&object);

   if (obj_feat) {
        // find closest related DBLink User Objects
        CBioseq_Handle bsh = scope->GetBioseqHandle(obj_feat->GetLocation());
        related = GetObjects(bsh);
    } else if (obj_desc) {
        if (obj_desc->IsUser() && IsDBLink(obj_desc->GetUser())) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            CSeq_entry_Handle seh = GetSeqEntryForSeqdesc(scope, *obj_desc);
            related = GetObjects(seh, GetLabelForType(m_ConstraintFieldType), m_StringConstraint);
        }
    }

    return related;
}


vector<CConstRef<CObject> > CDBLinkField::GetRelatedObjects(const CApplyObject& object)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&object);

   if (obj_feat) {
        related = GetObjects(object.GetSEH(), "", CRef<CStringConstraint>());
    } else if (obj_desc) {
        if (obj_desc->IsUser() && IsDBLink(obj_desc->GetUser())) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            related = GetObjects(object.GetSEH(), GetLabelForType(m_ConstraintFieldType), m_StringConstraint);
        }
    }

    return related;
}


bool CDBLinkField::IsDBLink (const CUser_object& user)
{
    if (user.GetObjectType() == CUser_object::eObjectType_DBLink) {
        return true;
    } else {
        return false;
    }
}

CTempString CDBLinkField::GetNormalizedDBLinkFieldName(const CTempString& orig_label)
{
    if (NStr::StartsWith(orig_label, "DBLink ")) {
        return orig_label.substr(7);
    }
    else
        return orig_label;
}



vector<string> CDBLinkField::GetFieldNames()
{
    vector<string> options;

    for (int field_type = CDBLinkField::eDBLinkFieldType_Trace;
         field_type < CDBLinkField::eDBLinkFieldType_Unknown;
         field_type++) {
        const string& field_name = CDBLinkField::GetLabelForType((CDBLinkField::EDBLinkFieldType)field_type);
        options.push_back(field_name);
    }

    return options;
}


CRef<CUser_object> CDBLinkField::MakeUserObject()
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetObjectType(CUser_object::eObjectType_DBLink);
    return obj;
}


CDBLink::CDBLink()
{
    m_User = MakeEmptyUserObject();
}


CDBLink::CDBLink(CUser_object& user)
{
    m_User.Reset(new CUser_object());
    m_User->Assign(user);
}


CRef<CUser_object> CDBLink::MakeEmptyUserObject()
{
    CRef<CUser_object> obj = CDBLinkField::MakeUserObject();
    return obj;
}


void CDBLink::SetBioSample(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_BioSample);
    field.SetVal(obj, val, existing_text);
}


void CDBLink::SetBioProject(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_BioProject);
    field.SetVal(obj, val, existing_text);
}


void CDBLink::SetTrace(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_Trace);
    field.SetVal(obj, val, existing_text);
}


void CDBLink::SetProbeDB(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_ProbeDB);
    field.SetVal(obj, val, existing_text);
}


void CDBLink::SetSRA(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_SRA);
    field.SetVal(obj, val, existing_text);
}


void CDBLink::SetAssembly(CUser_object& obj, const string& val, EExistingText existing_text)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_Assembly);
    field.SetVal(obj, val, existing_text);
}

void CDBLink::MergeDBLink(CUser_object& dest, const CUser_object& src)
{
    for (const auto& src_field : src.GetData())
    {
        if (src_field->IsSetLabel() && src_field->GetLabel().IsStr() && src_field->IsSetData())
        {
            CDBLinkField field(CDBLinkField::GetTypeForLabel(src_field->GetLabel().GetStr()));

            if (src_field->GetData().IsStr())
            {
                field.SetVal(dest, src_field->GetData().GetStr());
            } else if (src_field->GetData().IsStrs())
            {
                for (const auto& src_value : src_field->GetData().GetStrs())
                {
                    field.SetVal(dest, src_value, eExistingText_add_qual);
                }
            }
        }

    }
}

CDBLink& CDBLink::SetBioSample(const string& val, EExistingText existing_text)
{
    SetBioSample(*m_User, val, existing_text);
    return *this;
}


CDBLink& CDBLink::SetBioProject(const string& val, EExistingText existing_text)
{
    SetBioProject(*m_User, val, existing_text);
    return *this;
}


CDBLink& CDBLink::SetTrace(const string& val, EExistingText existing_text)
{
    SetTrace(*m_User, val, existing_text);
    return *this;
}


CDBLink& CDBLink::SetProbeDB(const string& val, EExistingText existing_text)
{
    SetProbeDB(*m_User, val, existing_text);
    return *this;
}


CDBLink& CDBLink::SetSRA(const string& val, EExistingText existing_text)
{
    SetSRA(*m_User, val, existing_text);
    return *this;
}


CDBLink& CDBLink::SetAssembly(const string& val, EExistingText existing_text)
{
    SetAssembly(*m_User, val, existing_text);
    return *this;
}


vector<string> CDBLink::GetBioSample(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_BioSample);
    return field.GetVals(obj);
}


vector<string> CDBLink::GetBioProject(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_BioProject);
    return field.GetVals(obj);
}


vector<string> CDBLink::GetTrace(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_Trace);
    return field.GetVals(obj);
}


vector<string> CDBLink::GetProbeDB(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_ProbeDB);
    return field.GetVals(obj);
}


vector<string> CDBLink::GetSRA(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_SRA);
    return field.GetVals(obj);
}


vector<string> CDBLink::GetAssembly(const CUser_object& obj)
{
    CDBLinkField field(CDBLinkField::eDBLinkFieldType_Assembly);
    return field.GetVals(obj);
}


CRef<CUser_object> CDBLink::MakeUserObject()
{
    CRef<CUser_object> obj(new CUser_object());
    obj->Assign(*m_User);
    return obj;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

