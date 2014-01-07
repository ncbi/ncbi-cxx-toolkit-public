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
#ifndef _STRUC_COMM_FIELD_H_
#define _STRUC_COMM_FIELD_H_

#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>

#include <objmgr/scope.hpp>

#include <objtools/edit/field_handler.hpp>
#include <objtools/edit/string_constraint.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CComment_set;
class CComment_rule;
class CField_rule;
class CField_set;


BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CStructuredCommentField : public CFieldHandler
{
public:

    CStructuredCommentField (const string& prefix, const string& field_name) : m_Prefix(prefix), m_FieldName(field_name) 
            { CComment_rule::NormalizePrefix(m_Prefix);
              m_ConstraintFieldName = ""; 
              m_StringConstraint = NULL; };

    virtual vector<CConstRef<CObject> > GetObjects(CBioseq_Handle bsh);
    virtual vector<CConstRef<CObject> > GetObjects(CSeq_entry_Handle seh, const string& constraint_field, CRef<CStringConstraint> string_constraint);
    virtual vector<CRef<CApplyObject> > GetApplyObjects(CBioseq_Handle bsh);
    virtual vector<CConstRef<CObject> > GetRelatedObjects(const CObject& object, CRef<CScope> scope);
    virtual vector<CConstRef<CObject> > GetRelatedObjects(const CApplyObject& object);

    virtual bool IsEmpty(const CObject& object) const;

    virtual string GetVal(const CObject& object);
    virtual vector<string> GetVals(const CObject& object);

    virtual void ClearVal(CObject& object);

    virtual CSeqFeatData::ESubtype GetFeatureSubtype() { return CSeqFeatData::eSubtype_bad; };
    virtual CSeqdesc::E_Choice GetDescriptorSubtype() { return CSeqdesc::e_User; };
    virtual void SetConstraint(const string& field_name, CConstRef<CStringConstraint> string_constraint);
    virtual bool AllowMultipleValues() { return false; }
    virtual bool SetVal(CObject& object, const string & newValue, EExistingText existing_text = eExistingText_replace_old);
    bool SetVal(CUser_field& field, const string & newValue, EExistingText existing_text);
    virtual string GetLabel() const { return m_Prefix + " " + m_FieldName; };
    bool IsStructuredCommentForThisField (const CUser_object& user) const;
    static bool IsStructuredComment (const CUser_object& user);
    static string GetPrefix (const CUser_object& user);
    static vector<string> GetFieldNames(const string& prefix);
    static CRef<CUser_object> MakeUserObject(const string& prefix);
    static string MakePrefixFromRoot(const string& root);
    static string MakeSuffixFromRoot(const string& root);
    static string KeywordForPrefix(const string& prefix);
    static string PrefixForKeyword(const string& keyword);
    static vector<string> GetKeywordList();

protected:
    string m_Prefix;
    string m_FieldName;
    string m_ConstraintFieldName;
    CRef<CStringConstraint> m_StringConstraint;

    void x_InsertFieldAtCorrectPosition(CUser_object& user, CRef<CUser_field> field);
};


class NCBI_XOBJEDIT_EXPORT CGenomeAssemblyComment
{
public:
    CGenomeAssemblyComment();
    CGenomeAssemblyComment(CUser_object& user);
    ~CGenomeAssemblyComment() {};
    static CRef<CUser_object> MakeEmptyUserObject();
    static void SetAssemblyMethod(CUser_object& obj, string val, EExistingText existing_text = eExistingText_replace_old);
    static void SetAssemblyMethodProgram(CUser_object& obj, string val, EExistingText existing_text = eExistingText_replace_old);
    static void SetAssemblyMethodVersion(CUser_object& obj, string val, EExistingText existing_text = eExistingText_replace_old);
    static void SetGenomeCoverage(CUser_object& obj, string val, EExistingText existing_text = eExistingText_replace_old);
    static void SetSequencingTechnology(CUser_object& obj, string val, EExistingText existing_text = eExistingText_replace_old);

    static string GetAssemblyMethod(CUser_object& obj);
    static string GetAssemblyMethodProgram(CUser_object& obj);
    static string GetAssemblyMethodVersion(CUser_object& obj);
    static string GetGenomeCoverage(CUser_object& obj);
    static string GetSequencingTechnology(CUser_object& obj);

    CGenomeAssemblyComment& SetAssemblyMethod(string val, EExistingText existing_text = eExistingText_replace_old);
    CGenomeAssemblyComment& SetAssemblyMethodProgram(string val, EExistingText existing_text = eExistingText_replace_old);
    CGenomeAssemblyComment& SetAssemblyMethodVersion(string val, EExistingText existing_text = eExistingText_replace_old);
    CGenomeAssemblyComment& SetGenomeCoverage(string val, EExistingText existing_text = eExistingText_replace_old);
    CGenomeAssemblyComment& SetSequencingTechnology(string val, EExistingText existing_text = eExistingText_replace_old);
    CRef<CUser_object> MakeUserObject();

protected:
    CRef<CUser_object> m_User;

    static void x_GetAssemblyMethodProgramAndVersion(string val, string& program, string& version);
    static string x_GetAssemblyMethodFromProgramAndVersion(const string& program, const string& version);
};



END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
    // _STRUC_COMM_FIELD_H_
