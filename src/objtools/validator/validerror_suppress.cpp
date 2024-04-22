/* $Id$
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
 * Author:  Justin Foley, Colleen Bollin......
 *
 * File Description:
 *  Methods to facilitate runtime suppression of validation errors
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objtools/validator/validerror_suppress.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


    
static const string kSuppressFieldLabel = "Suppress";

static bool s_IsSuppressField (const CUser_field& field)
{
    return (field.IsSetLabel() && 
            field.GetLabel().IsStr() &&
            NStr::EqualNocase(field.GetLabel().GetStr(), kSuppressFieldLabel));
}


void CValidErrorSuppress::AddSuppression(CUser_object& user, TErrCode error_code)
{
    if (user.IsSetData()) {
        for (auto pUserField : user.SetData()) {
            if (s_IsSuppressField(*pUserField)) {
                if (pUserField->IsSetData()) {
                    if (pUserField->GetData().IsInt()) {
                        const auto old_val = pUserField->GetData().GetInt(); 
                        if (old_val != error_code) {
                            pUserField->SetData().SetInts().push_back(old_val);
                            pUserField->SetData().SetInts().push_back(error_code);
                        }
                        return;
                    } else if (pUserField->GetData().IsInts()) {
                        const auto& ints = pUserField->GetData().GetInts();
                        if (find(ints.cbegin(), ints.cend(), error_code) == ints.cend()) {
                            pUserField->SetData().SetInts().push_back(error_code);
                        }
                        return;
                    }
                }
            }
        }
    }

    CRef<CUser_field> pField(new CUser_field());
    pField->SetLabel().SetStr(kSuppressFieldLabel);
    pField->SetData().SetInts().push_back(error_code);
    user.SetData().push_back(pField);
}



void CValidErrorSuppress::SetSuppressedCodes(const CUser_object& user, CValidErrorSuppress::TCodes& errCodes)
{
    if (!user.IsSetData()) {
        return;
    }

    for (const auto& pUserField : user.GetData()) {
        if (pUserField->IsSetData() && s_IsSuppressField(*pUserField)) {
            if (pUserField->GetData().IsInt()) {
                errCodes.insert(pUserField->GetData().GetInt());
            } else if (pUserField->GetData().IsInts()) {
                for (const auto& code : pUserField->GetData().GetInts()) {
                    errCodes.insert(code);
                }
            } else if (pUserField->GetData().IsStr()) {
                unsigned int ec = CValidErrItem::ConvertToErrCode(pUserField->GetData().GetStr());
                if (ec != eErr_MAX) {
                    errCodes.insert(ec);
                }
            } else if (pUserField->GetData().IsStrs()) {
                for (const auto& errStr : pUserField->GetData().GetStrs()) {
                    unsigned int ec = CValidErrItem::ConvertToErrCode(errStr);
                    if (ec != eErr_MAX) {
                        errCodes.insert(ec);
                    }
                }
            }
        }
    }
}


void CValidErrorSuppress::SetSuppressedCodes(const CSeq_entry& se, CValidErrorSuppress::TCodes& errCodes)
{
    if (se.IsSeq()) {
        SetSuppressedCodes(se.GetSeq(), errCodes);
    } else if (se.IsSet()) {
        const CBioseq_set& set = se.GetSet();
        if (set.IsSetDescr()) {
            for (const auto& pDesc : set.GetDescr().Get()) {
                if (pDesc->IsUser() &&
                    pDesc->GetUser().GetObjectType() == CUser_object::eObjectType_ValidationSuppression) {
                    SetSuppressedCodes(pDesc->GetUser(), errCodes);
                }
            }
        }
        if (set.IsSetSeq_set()) {
            for (const auto& pEntry : set.GetSeq_set()) {
                SetSuppressedCodes(*pEntry, errCodes);
            }
        }
    }
}


void CValidErrorSuppress::SetSuppressedCodes(const CSeq_entry_Handle& se, CValidErrorSuppress::TCodes& errCodes)
{
    SetSuppressedCodes(*(se.GetCompleteSeq_entry()), errCodes);
}


void CValidErrorSuppress::SetSuppressedCodes(const CSeq_submit& ss, CValidErrorSuppress::TCodes& errCodes)
{
    if (ss.IsEntrys()) {
        for (const auto& pEntry : ss.GetData().GetEntrys()) {
            SetSuppressedCodes(*pEntry, errCodes);
        }
    }
}


void CValidErrorSuppress::SetSuppressedCodes(const CBioseq& seq, CValidErrorSuppress::TCodes& errCodes)
{
    if (seq.IsSetDescr()) {
        for (const auto& pDesc : seq.GetDescr().Get()) {
            if (pDesc->IsUser() &&
                pDesc->GetUser().GetObjectType() == CUser_object::eObjectType_ValidationSuppression) {
                SetSuppressedCodes(pDesc->GetUser(), errCodes);
            }
        }
    }
}




END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

