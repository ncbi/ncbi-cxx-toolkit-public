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
    virtual vector<CConstRef<CObject> > GetObjects(CBioseq_Handle bsh);
    virtual vector<CConstRef<CObject> > GetObjects(CSeq_entry_Handle seh, const string& constraint_field, CRef<CStringConstraint> string_constraint);
    virtual vector<CRef<CApplyObject> > GetApplyObjects(CBioseq_Handle bsh);
    virtual vector<CConstRef<CObject> > GetRelatedObjects(const CObject& object, CRef<CScope> scope);
    virtual vector<CConstRef<CObject> > GetRelatedObjects(const CApplyObject& object);

    virtual CSeqFeatData::ESubtype GetFeatureSubtype() { return CSeqFeatData::eSubtype_bad; } ;
    virtual CSeqdesc::E_Choice GetDescriptorSubtype() { return m_Subtype; } ;


    virtual void SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint) {};
    CSeqdesc::E_Choice GetSubtype() { return m_Subtype; };
    void SetSubtype(CSeqdesc::E_Choice subtype) { m_Subtype = subtype; };
    virtual bool AllowMultipleValues() { return false; };
protected:
    CSeqdesc::E_Choice m_Subtype;
};


class NCBI_XOBJEDIT_EXPORT CCommentDescField : public CTextDescriptorField
{
public:
    CCommentDescField () { m_Subtype = CSeqdesc::e_Comment; };

    virtual string GetVal(const CObject& object);
    virtual vector<string> GetVals(const CObject& object);
    virtual bool IsEmpty(const CObject& object) const;
    virtual void ClearVal(CObject& object);
    virtual bool SetVal(CObject& object, const string& val, EExistingText existing_text);
};


class NCBI_XOBJEDIT_EXPORT CDefinitionLineField : public CTextDescriptorField
{
public:
    CDefinitionLineField () { m_Subtype = CSeqdesc::e_Title; };
    virtual string GetVal(const CObject& object);
    virtual vector<string> GetVals(const CObject& object);
    virtual bool IsEmpty(const CObject& object) const;
    virtual void ClearVal(CObject& object);
    virtual bool SetVal(CObject& object, const string& val, EExistingText existing_text);
};


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif