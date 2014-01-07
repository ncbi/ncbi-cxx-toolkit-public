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
*   CGBBlockField class
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/edit/gb_block_field.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

string CGBBlockField::GetVal(const CObject& object)
{
    vector<string> vals = GetVals(object);
    if (vals.size() > 0) {
        return vals[0];
    } else {
        return "";
    }
}

        
vector<string> CGBBlockField::GetVals(const CObject& object)
{
    vector<string> rval;

    const CSeqdesc* desc = dynamic_cast<const CSeqdesc *>(&object);
    if (desc && desc->IsGenbank()) {
        switch (m_FieldType) {
            case eGBBlockFieldType_Keyword:
                if (desc->GetGenbank().IsSetKeywords()) {
                    ITERATE(CGB_block::TKeywords, it, desc->GetGenbank().GetKeywords()) {
                        rval.push_back(*it);
                    }
                }
                break;
            case eGBBlockFieldType_ExtraAccession:
                if (desc->GetGenbank().IsSetExtra_accessions()) {
                    ITERATE(CGB_block::TKeywords, it, desc->GetGenbank().GetExtra_accessions()) {
                        rval.push_back(*it);
                    }
                }
                break;
            default:
                break;
        }
    }
    return rval;
}


bool CGBBlockField::IsEmpty(const CObject& object) const
{
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc *>(&object);
    if (!desc || !desc->IsGenbank()) {
        return false;
    }
    const CGB_block& block = desc->GetGenbank();
    return block.IsEmpty();
}


void CGBBlockField::ClearVal(CObject& object)
{
    CSeqdesc* desc = dynamic_cast<CSeqdesc *>(&object);
    if (!desc || !desc->IsGenbank()) {
        return;
    }
    switch (m_FieldType) {
        case eGBBlockFieldType_Keyword:
            desc->SetGenbank().ResetKeywords();
            break;
        case eGBBlockFieldType_ExtraAccession:
            desc->SetGenbank().ResetExtra_accessions();
            break;
        default:
            break;
    }
}


bool CGBBlockField::SetVal(CObject& object, const string& val, EExistingText existing_text)
{
    bool rval = false;

    CSeqdesc* desc = dynamic_cast<CSeqdesc *>(&object);
    if (!desc) {
        return false;
    }
    switch (m_FieldType) {
        case eGBBlockFieldType_Keyword:
            if (!desc->IsGenbank()
                || !desc->GetGenbank().IsSetKeywords()
                || desc->GetGenbank().GetKeywords().empty()
                || existing_text == eExistingText_add_qual) {
                desc->SetGenbank().SetKeywords().push_back(val);
                rval = true;
            } else {
                CGB_block::TKeywords::iterator it = desc->SetGenbank().SetKeywords().begin();
                while (it != desc->SetGenbank().SetKeywords().end()) {
                    string curr_val = *it;
                    if (!m_StringConstraint || m_StringConstraint->DoesTextMatch(curr_val)) {
                        if (AddValueToString(curr_val, val, existing_text)) {
                            *it = curr_val;
                            rval = true;
                        }
                    }
                    if(NStr::IsBlank(*it)) {
                        it = desc->SetGenbank().SetKeywords().erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            break;
        case eGBBlockFieldType_ExtraAccession:
            if (!desc->IsGenbank()
                || !desc->GetGenbank().IsSetExtra_accessions()
                || desc->GetGenbank().GetExtra_accessions().empty()
                || existing_text == eExistingText_add_qual) {
                desc->SetGenbank().SetExtra_accessions().push_back(val);
                rval = true;
            } else {
                CGB_block::TExtra_accessions::iterator it = desc->SetGenbank().SetExtra_accessions().begin();
                while (it != desc->SetGenbank().SetExtra_accessions().end()) {
                    string curr_val = *it;
                    if (!m_StringConstraint || m_StringConstraint->DoesTextMatch(curr_val)) {
                        if (AddValueToString(curr_val, val, existing_text)) {
                            *it = curr_val;
                            rval = true;
                        }
                    }
                    if(NStr::IsBlank(*it)) {
                        it = desc->SetGenbank().SetExtra_accessions().erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            break;
        default:
            break;
    }
    return rval;
}


void CGBBlockField::SetConstraint(const string& field, CConstRef<CStringConstraint> string_constraint)
{
    EGBBlockFieldType field_type = GetTypeForLabel(field);
    if (field_type == m_FieldType && string_constraint) {
        m_StringConstraint = new CStringConstraint(" ");
        m_StringConstraint->Assign(*string_constraint);
    } else {
        m_StringConstraint.Reset(NULL);
    }
}


bool CGBBlockField::AllowMultipleValues()
{
    bool rval = false;
    switch (m_FieldType) {
        case eGBBlockFieldType_Keyword:
        case eGBBlockFieldType_ExtraAccession:
            rval = true;
            break;
        default:
            break;
    }

    return rval;
}


CGBBlockField::EGBBlockFieldType CGBBlockField::GetTypeForLabel(string label)
{
    for (int i = eGBBlockFieldType_Keyword; i < eGBBlockFieldType_Unknown; i++) {
        string match = GetLabelForType((EGBBlockFieldType)i);
        if (NStr::EqualNocase(label, match)) {
            return (EGBBlockFieldType)i;
        }
    }
    return eGBBlockFieldType_Unknown;
}


string CGBBlockField::GetLabelForType(EGBBlockFieldType field_type)
{
    string rval = "";
    switch (field_type) {
        case eGBBlockFieldType_Keyword:
            rval = kGenbankBlockKeyword;
            break;
        case eGBBlockFieldType_ExtraAccession:
            rval = "Extra Accession";
            break;
        case eGBBlockFieldType_Unknown:
            break;
    }
    return rval;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

