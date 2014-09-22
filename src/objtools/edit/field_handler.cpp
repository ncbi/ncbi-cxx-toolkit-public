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
*   CFieldHandler parent class
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objtools/edit/field_handler.hpp>
#include <objtools/edit/dblink_field.hpp>
#include <objtools/edit/gb_block_field.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

bool CFieldHandler::QualifierNamesAreEquivalent (string name1, string name2)
{
    // spaces and underscores do not count
    NStr::ReplaceInPlace (name1, " ", "");
    NStr::ReplaceInPlace (name1, "_", "");
    NStr::ReplaceInPlace (name2, " ", "");
    NStr::ReplaceInPlace (name2, "_", "");
    // ignore protein at beginning
    if (NStr::StartsWith(name1, "protein ")) {
        name1 = name1.substr(8);
    }
    if (NStr::StartsWith(name2, "protein ")) {
        name2 = name2.substr(8);
    }
    return NStr::EqualNocase(name1, name2);
}


vector<CRef<CApplyObject> > CFieldHandler::GetRelatedApplyObjects(const CObject& object, CRef<CScope> scope)
{
    vector<CRef<CApplyObject> > related = GetApplyObjectsFromRelatedObjects(GetRelatedObjects(object, scope), scope);

    return related;
}


vector<CRef<CApplyObject> > CFieldHandler::GetApplyObjectsFromRelatedObjects(vector<CConstRef<CObject> > related, CRef<CScope> scope)
{
    vector<CRef<CApplyObject> > rval;

    ITERATE(vector<CConstRef<CObject> >, it, related) {
        const CSeqdesc * obj_desc = dynamic_cast<const CSeqdesc *>((*it).GetPointer());
        const CSeq_feat * obj_feat = dynamic_cast<const CSeq_feat *>((*it).GetPointer());
        if (obj_desc) {
            CSeq_entry_Handle seh = GetSeqEntryForSeqdesc(scope, *obj_desc);
            CRef<CSeqdesc> new_desc(new CSeqdesc());
            new_desc->Assign(*obj_desc);
            CRef<CObject> editable(new_desc.GetPointer());
            CRef<CApplyObject> apply(new CApplyObject(seh, *it, editable));
            rval.push_back(apply);
        } else {
            CBioseq_Handle bsh = scope->GetBioseqHandle(obj_feat->GetLocation());
            CRef<CApplyObject> apply(new CApplyObject(bsh, *obj_feat));
            rval.push_back(apply);
        }
 
    }

    return rval;
}


CRef<CFieldHandler>
CFieldHandlerFactory::Create(const string& field_name)
{
    CDBLinkField::EDBLinkFieldType dblink_field = CDBLinkField::GetTypeForLabel(field_name);
    if (dblink_field != CDBLinkField::eDBLinkFieldType_Unknown) {
        return CRef<CFieldHandler>(new CDBLinkField(dblink_field));
    }
#if 0
    if (s_IsSequenceIDField(field_name)) {
        return CRef<CFieldHandler>(new CSeqIdField());
    }
    CPubField::EPubFieldType pub_field = CPubField::GetTypeForLabel(field_name);
    if (pub_field != CPubField::ePubFieldType_Unknown) {
        return CRef<CFieldHandler>(new CPubField(pub_field));
    }
    CMolInfoField::EMolInfoFieldType molinfo_field = CMolInfoField::GetFieldType(field_name);
    if (molinfo_field != CMolInfoField::e_Unknown) {
        return CRef<CFieldHandler>(new CMolInfoField(molinfo_field));
    }

    if (NStr::EqualNocase(field_name, kGenomeProjectID)) {
        return CRef<CFieldHandler>(new CGenomeProjectField());
    }
#endif
    if (CFieldHandler::QualifierNamesAreEquivalent(field_name, kCommentDescriptorLabel)) {
        return CRef<CFieldHandler>(new CCommentDescField());
    }
    if (CFieldHandler::QualifierNamesAreEquivalent(field_name, kDefinitionLineLabel)) {
        return CRef<CFieldHandler>( new CDefinitionLineField());
    }
    CGBBlockField::EGBBlockFieldType gbblock_field = CGBBlockField::GetTypeForLabel(field_name);
    if (gbblock_field != CGBBlockField::eGBBlockFieldType_Unknown) {
        return CRef<CFieldHandler>(new CGBBlockField(gbblock_field));
    }

    // empty
    CRef<CFieldHandler> empty;
    return empty;
}


bool CFieldHandlerFactory::s_IsSequenceIDField(const string& field)
{
    if (CFieldHandler::QualifierNamesAreEquivalent(field, kFieldTypeSeqId)
        || CFieldHandler::QualifierNamesAreEquivalent(field, kFieldTypeSeqId)) {
        return true;
    } else {
        return false;
    }
}


bool DoesObjectMatchFieldConstraint (const CObject& object, const string& field_name, CRef<CStringConstraint> string_constraint, CRef<CScope> scope)
{
    if (NStr::IsBlank(field_name) || !string_constraint) {
        return true;
    }
    
    CRef<CFieldHandler> fh = CFieldHandlerFactory::Create(field_name); 
    if (!fh) {
        return false;
    }

    vector<string> val_list; 
    vector<CConstRef<CObject> > objs = fh->GetRelatedObjects (object, scope);
    ITERATE(vector<CConstRef<CObject> >, it, objs) {
        vector<string> add = fh->GetVals(**it);
        val_list.insert(val_list.end(), add.begin(), add.end());
    }

    return string_constraint->DoesListMatch(val_list);
}


bool DoesApplyObjectMatchFieldConstraint (const CApplyObject& object, const string& field_name, CRef<CStringConstraint> string_constraint)
{
    if (NStr::IsBlank(field_name) || !string_constraint) {
        return true;
    }
    
    CRef<CFieldHandler> fh = CFieldHandlerFactory::Create(field_name); 
    if (!fh) {
        return false;
    }

    vector<string> val_list; 
    vector<CConstRef<CObject> > objs = fh->GetRelatedObjects (object);
    ITERATE(vector<CConstRef<CObject> >, it, objs) {
        vector<string> add = fh->GetVals(**it);
        val_list.insert(val_list.end(), add.begin(), add.end());
    }

    return string_constraint->DoesListMatch(val_list);
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

