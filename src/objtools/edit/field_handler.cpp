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

CApplyObject::CApplyObject(CBioseq_Handle bsh, const CSeqdesc& desc)
{
    m_SEH = bsh.GetParentEntry();
    if (!desc.IsMolinfo() && !desc.IsTitle()) {
        CBioseq_set_Handle bssh = bsh.GetParentBioseq_set();
        if (bssh && bssh.IsSetClass() && bssh.GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
            m_SEH = bssh.GetParentEntry();
        }
    }

    m_Original.Reset(&desc);
    CRef<CSeqdesc> new_desc(new CSeqdesc());
    new_desc->Assign(desc);
    m_Editable = new_desc.GetPointer();    
}


CApplyObject::CApplyObject(CBioseq_Handle bsh, const CSeqdesc::E_Choice subtype)
{
    m_SEH = bsh.GetParentEntry();
    if (subtype != CSeqdesc::e_Molinfo && subtype != CSeqdesc::e_Title) {
        CBioseq_set_Handle bssh = bsh.GetParentBioseq_set();
        if (bssh && bssh.IsSetClass() && bssh.GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
            m_SEH = bssh.GetParentEntry();
        }
    }

    m_Original.Reset(NULL);
    CRef<CSeqdesc> new_desc(new CSeqdesc());
    new_desc->Select(subtype);
    m_Editable = new_desc.GetPointer();    
}


CApplyObject::CApplyObject(CBioseq_Handle bsh, const string& user_label)
{
    m_SEH = bsh.GetParentEntry();
    CBioseq_set_Handle bssh = bsh.GetParentBioseq_set();
    if (bssh && bssh.IsSetClass() && bssh.GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
        m_SEH = bssh.GetParentEntry();
    }

    m_Original.Reset(NULL);
    CRef<CSeqdesc> new_desc(new CSeqdesc());
    new_desc->SetUser().SetType().SetStr(user_label);
    m_Editable = new_desc.GetPointer();    
}


CApplyObject::CApplyObject(CBioseq_Handle bsh, const CSeq_feat& feat)
{
    m_SEH = bsh.GetParentEntry();
    m_Original.Reset(&feat);
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(feat);
    m_Editable = new_feat.GetPointer();
}


CApplyObject::CApplyObject(CBioseq_Handle bsh)
{
    m_SEH = bsh.GetParentEntry();
    m_Original.Reset(bsh.GetCompleteBioseq().GetPointer());
    CRef<CBioseq> new_seq(new CBioseq());
    new_seq->Assign(*(bsh.GetCompleteBioseq()));
    m_Editable = new_seq.GetPointer();
}


void CApplyObject::ApplyChange()
{
    CSeqdesc* desc = dynamic_cast<CSeqdesc * >(m_Editable.GetPointer());
    CSeq_feat* feat = dynamic_cast<CSeq_feat * >(m_Editable.GetPointer());
    CSeq_inst* inst = dynamic_cast<CSeq_inst * >(m_Editable.GetPointer());
    
    if (desc) {
        if (m_Original) {
            const CSeqdesc* orig_desc = dynamic_cast<const CSeqdesc* >(m_Original.GetPointer());
            CSeqdesc* change_desc = const_cast<CSeqdesc* >(orig_desc);
            change_desc->Assign(*desc);
        } else {
            CSeq_entry_EditHandle eseh = m_SEH.GetEditHandle();
            eseh.AddSeqdesc(const_cast<CSeqdesc&>(*desc));
        }
    } else if (feat) {
        if (m_Original) {
            const CSeq_feat* old_feat = dynamic_cast<const CSeq_feat * >(m_Original.GetPointer());
            CSeq_feat_Handle oh = m_SEH.GetScope().GetSeq_featHandle(*old_feat);
            // This is necessary, to make sure that we are in "editing mode"
            const CSeq_annot_Handle& annot_handle = oh.GetAnnot();
            CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

            // Now actually edit the feature
            CSeq_feat_EditHandle feh(oh);
            CSeq_entry_Handle parent_entry = feh.GetAnnot().GetParentEntry();

            feh.Replace(*feat);
            if (feh.IsSetData() && feh.GetData().IsCdregion() && feh.IsSetProduct()) {
                if (parent_entry.IsSet()
                     && parent_entry.GetSet().IsSetClass()
                     && parent_entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
                    // already on nuc-prot set, leave it alone
                } else {
                    CBioseq_set_Handle nuc_parent = parent_entry.GetParentBioseq_set();
                    if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
                        CSeq_annot_Handle ftable;
                        CSeq_entry_Handle parent_seh = nuc_parent.GetParentEntry();
                        CSeq_annot_CI annot_ci(parent_seh, CSeq_annot_CI::eSearch_entry);
                        for (; annot_ci; ++annot_ci) {
                            if ((*annot_ci).IsFtable()) {
                                ftable = *annot_ci;
                                break;
                            }
                        }

                        if (!ftable) {
                            CRef<CSeq_annot> new_annot(new CSeq_annot());
                            new_annot->SetData().SetFtable();
                            CSeq_entry_EditHandle eh = parent_seh.GetEditHandle();
                            ftable = eh.AttachAnnot(*new_annot);
                        }

                        CSeq_annot_EditHandle new_annot = ftable.GetEditHandle();
                        new_annot.TakeFeat(feh);
                    }
                }
            }
        } else {
            if (feat->IsSetData() && feat->GetData().IsCdregion() && feat->IsSetProduct()) {
                CBioseq_Handle bsh = m_SEH.GetScope().GetBioseqHandle(feat->GetProduct());
                if (bsh) {
                    CBioseq_set_Handle nuc_parent = bsh.GetParentBioseq_set();
                    if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
                        m_SEH = nuc_parent.GetParentEntry();
                    }
                }
            }
            CSeq_annot_Handle ftable;
            CSeq_annot_CI annot_ci(m_SEH, CSeq_annot_CI::eSearch_entry);
            for (; annot_ci; ++annot_ci) {
                if ((*annot_ci).IsFtable()) {
                    ftable = *annot_ci;
                    break;
                }
            }

            CSeq_entry_EditHandle eh = m_SEH.GetEditHandle();

            if (!ftable) {
                CRef<CSeq_annot> new_annot(new CSeq_annot());
                ftable = eh.AttachAnnot(*new_annot);
            }

            CSeq_annot_EditHandle aeh(ftable);
            aeh.AddFeat(*feat);

        }

    } else if (inst) {
        CBioseq_EditHandle beeh = m_SEH.GetSeq().GetEditHandle();
        beeh.SetInst(*inst);
    }
}


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


CSeq_entry_Handle GetSeqEntryForSeqdesc (CRef<CScope> scope, const CSeqdesc& seq_desc)
{
    CSeq_entry_Handle seh;

    if (!scope) {
        return seh;
    }

    CScope::TTSE_Handles tses;
    scope->GetAllTSEs(tses, CScope::eAllTSEs);
    ITERATE (CScope::TTSE_Handles, handle, tses) {
        for (CSeq_entry_CI entry_ci(*handle, CSeq_entry_CI::fRecursive | CSeq_entry_CI::fIncludeGivenEntry); entry_ci; ++entry_ci) {
            if (entry_ci->IsSetDescr()) {
                ITERATE (CBioseq::TDescr::Tdata, dit, entry_ci->GetDescr().Get()) {
                    if (dit->GetPointer() == &seq_desc) {
                        return *entry_ci;
                    }
                }
            }
        }
    }
    return seh;
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
