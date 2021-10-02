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


#ifndef _TEXT_DESC_FIELD_H_
#define _TEXT_DESC_FIELD_H_

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/edit/field_handler.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CTextDescriptorField : public CFieldHandler
{
public:
    vector<CConstRef<CObject>> GetObjects(CBioseq_Handle bsh) override;
    vector<CConstRef<CObject>> GetObjects(CSeq_entry_Handle seh,
                                          const string& constraint_field,
                                          CRef<CStringConstraint> string_constraint) override;
    vector<CRef<CApplyObject>> GetApplyObjects(CBioseq_Handle bsh) override;
    vector<CConstRef<CObject>> GetRelatedObjects(const CObject& object, CRef<CScope> scope) override;
    vector<CConstRef<CObject>> GetRelatedObjects(const CApplyObject& object) override;

    CSeqFeatData::ESubtype GetFeatureSubtype() override { return CSeqFeatData::eSubtype_bad; }
    CSeqdesc::E_Choice GetDescriptorSubtype() override { return m_Subtype; }
    void SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint) override {}
    bool AllowMultipleValues() override { return false; }

    CSeqdesc::E_Choice GetSubtype() { return m_Subtype; }
    void SetSubtype(CSeqdesc::E_Choice subtype) { m_Subtype = subtype; }
protected:
    CSeqdesc::E_Choice m_Subtype;
};


class NCBI_XOBJEDIT_EXPORT CCommentDescField : public CTextDescriptorField
{
public:
    CCommentDescField() { m_Subtype = CSeqdesc::e_Comment; }

    string GetVal(const CObject& object) override;
    vector<string> GetVals(const CObject& object) override;
    bool IsEmpty(const CObject& object) const override;
    void ClearVal(CObject& object) override;
    bool SetVal(CObject& object, const string& val, EExistingText existing_text) override;
};


class NCBI_XOBJEDIT_EXPORT CDefinitionLineField : public CTextDescriptorField
{
public:
    CDefinitionLineField() { m_Subtype = CSeqdesc::e_Title; }
    string GetVal(const CObject& object) override;
    vector<string> GetVals(const CObject& object) override;
    bool IsEmpty(const CObject& object) const override;
    void ClearVal(CObject& object) override;
    bool SetVal(CObject& object, const string& val, EExistingText existing_text) override;
};


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

