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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   CTextDescriptorField class
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/edit/text_desc_field.hpp>
#include <objtools/edit/seqid_guesser.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

vector<CConstRef<CObject> > CTextDescriptorField::GetObjects(CBioseq_Handle bsh)
{
    vector< CConstRef<CObject> > rval;
    CSeqdesc_CI desc_ci(bsh, m_Subtype);
    while (desc_ci) {
        CConstRef<CObject> o(&(*desc_ci));
        rval.push_back(o);
        ++desc_ci;
    }
    return rval;
}


vector<CConstRef<CObject> > CTextDescriptorField::GetObjects(CSeq_entry_Handle seh, const string& constraint_field, CRef<CStringConstraint> string_constraint)
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


vector<CRef<CApplyObject> > CTextDescriptorField::GetApplyObjects(CBioseq_Handle bsh)
{
    vector<CRef<CApplyObject> > objects;

    // add existing descriptors
    CSeqdesc_CI desc_ci(bsh, m_Subtype);
    while (desc_ci) {
        CRef<CApplyObject> new_obj(new CApplyObject(bsh, *desc_ci));
        objects.push_back(new_obj);
        ++desc_ci;
    }

    if (objects.empty()) {
        CRef<CApplyObject> new_obj(new CApplyObject(bsh, m_Subtype));
        objects.push_back(new_obj);
    }        

    return objects;
}


vector<CConstRef<CObject> > CTextDescriptorField::GetRelatedObjects(const CObject& object, CRef<CScope> scope)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&object);

   if (obj_feat) {
        // find closest related DBLink User Objects
        CBioseq_Handle bsh = scope->GetBioseqHandle(obj_feat->GetLocation());
        related = GetObjects(bsh);
    } else if (obj_desc) {
        if (obj_desc->Which() == m_Subtype) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            CSeq_entry_Handle seh = GetSeqEntryForSeqdesc(scope, *obj_desc);
            related = GetObjects(seh, "", CRef<CStringConstraint>(NULL));
        }
    }

    return related;
}


vector<CConstRef<CObject> > CTextDescriptorField::GetRelatedObjects(const CApplyObject& object)
{
    vector<CConstRef<CObject> > related;

    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&(object.GetObject()));
    const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>(&(object.GetObject()));
    const CSeq_inst * inst = dynamic_cast<const CSeq_inst *>(&(object.GetObject()));
    const CBioseq * bioseq = dynamic_cast<const CBioseq *>(&(object.GetObject()));

    if (obj_feat) {
        related = GetObjects(object.GetSEH(), "", CRef<CStringConstraint>(NULL));
    } else if (obj_desc) {
        if (obj_desc->Which() == m_Subtype) {
            CConstRef<CObject> obj(obj_desc);
            related.push_back(obj);
        } else {
            related = GetObjects(object.GetSEH(), "", CRef<CStringConstraint>(NULL));
        }
    } else if (bioseq || inst) {
        related = GetObjects(object.GetSEH(), "", CRef<CStringConstraint>(NULL));
    }

    return related;
}


string CCommentDescField::GetVal(const CObject& object)
{
    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    if (obj_desc && obj_desc->IsComment()) {
        return obj_desc->GetComment();
    } else {
        return "";
    }
}


vector<string> CCommentDescField::GetVals(const CObject& object)
{
    vector<string> vals;
    vals.push_back(GetVal(object));
    return vals;
}


bool CCommentDescField::IsEmpty(const CObject& object) const
{
    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    if (obj_desc && obj_desc->IsComment() && !NStr::IsBlank(obj_desc->GetComment())) {
        return true;
    } else {
        return false;
    }
}


void CCommentDescField::ClearVal(CObject& object)
{
    CSeqdesc * obj_desc = dynamic_cast<CSeqdesc *>(&object);
    if (obj_desc) {
        obj_desc->SetComment(" ");
    }
}


bool CCommentDescField::SetVal(CObject& object, const string& val, EExistingText existing_text)
{
    bool rval = false;
    CSeqdesc * obj_desc = dynamic_cast<CSeqdesc *>(&object);
    if (!obj_desc) {
        return false;
    }
    string curr_val = "";
    if (obj_desc->IsComment()) {
        curr_val = obj_desc->GetComment();
    }
    if (AddValueToString(curr_val, val, existing_text)) {
        obj_desc->SetComment(curr_val);
        rval = true;
    }
    return rval;
}


string CDefinitionLineField::GetVal(const CObject& object)
{
    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    if (obj_desc && obj_desc->IsTitle()) {
        return obj_desc->GetTitle();
    } else {
        return "";
    }
}


vector<string> CDefinitionLineField::GetVals(const CObject& object)
{
    vector<string> vals;
    vals.push_back(GetVal(object));
    return vals;
}


bool CDefinitionLineField::IsEmpty(const CObject& object) const
{
    const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>(&object);
    if (obj_desc && obj_desc->IsTitle() && !NStr::IsBlank(obj_desc->GetTitle())) {
        return true;
    } else {
        return false;
    }
}


void CDefinitionLineField::ClearVal(CObject& object)
{
    CSeqdesc * obj_desc = dynamic_cast<CSeqdesc *>(&object);
    if (obj_desc) {
        obj_desc->SetTitle(" ");
    }
}


bool CDefinitionLineField::SetVal(CObject& object, const string& val, EExistingText existing_text)
{
    bool rval = false;
    CSeqdesc * obj_desc = dynamic_cast<CSeqdesc *>(&object);
    if (!obj_desc) {
        return false;
    }
    string curr_val = "";
    if (obj_desc->IsTitle()) {
        curr_val = obj_desc->GetTitle();
    }
    if (AddValueToString(curr_val, val, existing_text)) {
        obj_desc->SetTitle(curr_val);
        rval = true;
    }
    return rval;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
