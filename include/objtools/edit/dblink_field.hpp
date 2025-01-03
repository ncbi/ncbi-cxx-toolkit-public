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
#ifndef _DBLINK_FIELD_H_
#define _DBLINK_FIELD_H_

#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objmgr/scope.hpp>

#include <objtools/edit/field_handler.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CDBLinkField : public CFieldHandler
{
public:
    enum EDBLinkFieldType {
        eDBLinkFieldType_Trace = 0,
        eDBLinkFieldType_BioSample,
        eDBLinkFieldType_ProbeDB,
        eDBLinkFieldType_SRA,
        eDBLinkFieldType_BioProject,
        eDBLinkFieldType_Assembly,
        eDBLinkFieldType_Unknown };

    CDBLinkField(EDBLinkFieldType field_type) : m_FieldType(field_type)
      , m_ConstraintFieldType(eDBLinkFieldType_Unknown), m_StringConstraint() {}

    vector<CConstRef<CObject>> GetObjects(CBioseq_Handle bsh) override;
    vector<CConstRef<CObject>> GetObjects(CSeq_entry_Handle seh,
                                          const string& constraint_field,
                                          CRef<CStringConstraint> string_constraint) override;
    vector<CRef<CApplyObject>> GetApplyObjects(CBioseq_Handle bsh) override;
    vector<CConstRef<CObject>> GetRelatedObjects(const CObject& object, CRef<CScope> scope) override;
    vector<CConstRef<CObject>> GetRelatedObjects(const CApplyObject& object) override;

    bool IsEmpty(const CObject& object) const override;
    string GetVal(const CObject& object) override;
    vector<string> GetVals(const CObject& object) override;
    void ClearVal(CObject& object) override;

    CSeqFeatData::ESubtype GetFeatureSubtype() override { return CSeqFeatData::eSubtype_bad; }
    CSeqdesc::E_Choice GetDescriptorSubtype() override { return CSeqdesc::e_User; }
    void SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint) override;
    bool AllowMultipleValues() override { return true; }

    bool SetVal(CObject& object, const string& newValue, EExistingText existing_text = eExistingText_replace_old) override;
    bool SetVal(CSeqdesc& object, const string& newValue, EExistingText existing_text);
    bool SetVal(CUser_object& object, const string& newValue, EExistingText existing_text);
    bool SetVal(CUser_field& field, const string& newValue, EExistingText existing_text);
    virtual string GetLabel() const;
    static EDBLinkFieldType GetTypeForLabel(const string& label);
    static const string GetLabelForType(EDBLinkFieldType field_type);
    static bool IsDBLink(const CUser_object& user);
    static CTempString GetNormalizedDBLinkFieldName(const CTempString& orig_label);
    static vector<string> GetFieldNames();
    static CRef<CUser_object> MakeUserObject();

protected:
    void _ParseAndAppend(CUser_field::C_Data::TStrs& strs, const string & newValue, EExistingText existing_text);
    EDBLinkFieldType m_FieldType;
    EDBLinkFieldType m_ConstraintFieldType;
    CRef<CStringConstraint> m_StringConstraint;
};


class NCBI_XOBJEDIT_EXPORT CDBLink
{
public:
    CDBLink();
    CDBLink(CUser_object& user);
    ~CDBLink() {};
    static CRef<CUser_object> MakeEmptyUserObject();
    static void SetBioSample(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);
    static void SetBioProject(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);
    static void SetTrace(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);
    static void SetProbeDB(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);
    static void SetSRA(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);
    static void SetAssembly(CUser_object& obj, const string& val, EExistingText existing_text = eExistingText_replace_old);

    static void MergeDBLink(CUser_object& dest, const CUser_object& src);

    static vector<string> GetBioSample(const CUser_object& obj);
    static vector<string> GetBioProject(const CUser_object& obj);
    static vector<string> GetTrace(const CUser_object& obj);
    static vector<string> GetProbeDB(const CUser_object& obj);
    static vector<string> GetSRA(const CUser_object& obj);
    static vector<string> GetAssembly(const CUser_object& obj);

    CDBLink& SetBioSample(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CDBLink& SetBioProject(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CDBLink& SetTrace(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CDBLink& SetProbeDB(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CDBLink& SetSRA(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CDBLink& SetAssembly(const string& val, EExistingText existing_text = eExistingText_replace_old);
    CRef<CUser_object> MakeUserObject();

protected:
    CRef<CUser_object> m_User;
};




END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
// _DBLINK_FIELD_H_

