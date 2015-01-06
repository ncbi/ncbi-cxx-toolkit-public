#ifndef CLASSSTR_HPP
#define CLASSSTR_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   C++ class info: includes, used classes, C++ code etc.
*/

#include "typestr.hpp"
#include <corelib/ncbiutil.hpp>
#include "namespace.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CClassCode;
class CDataType;

class CClassTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    struct SMemberInfo {
        string externalName; // logical name
        string cName; // C++ type code
        string mName; // member name
        string tName; // typedef name
        AutoPtr<CTypeStrings> type; // value type
        string ptrType; // "*" or "NCBI_NS_NCBI::CRef"
        string valueName; // value name (mName or '*'+mName)
        bool optional;  // have OPTIONAL or DEFAULT attribute
        bool ref;       // implement member via CRef
        bool haveFlag;  // need additional boolean flag 'isSet'
        bool canBeNull; // pointer type can be NULL pointer
        bool delayed;
        int memberTag;
        string defaultValue; // DEFAULT value code
        bool noPrefix;
        bool attlist;
        bool noTag;
        bool simple;
        const CDataType* dataType;
        bool nonEmpty;
        CComments comments;
        SMemberInfo(const string& external_name,
                    const string& name, 
                    const AutoPtr<CTypeStrings>& type,
                    const string& pointerType,
                    bool optional, const string& defaultValue,
                    bool delayed, int tag,
                    bool noPrefx, bool attlst, bool noTg, bool simpl,
                    const CDataType* dataTp, bool nEmpty,
                    const CComments& comments);
    };
    typedef list<SMemberInfo> TMembers;

    CClassTypeStrings(const string& externalName, const string& className,
                      const string& namespaceName, const CDataType* dataType,
                      const CComments& comments);
    ~CClassTypeStrings(void);

    void SetClassNamespace(const CNamespace& ns);

    const string& GetExternalName(void) const
        {
            return m_ExternalName;
        }
    // DT added to avoid conflict with the standard Windows interfaces
    const string& GetClassNameDT(void) const
        {
            return m_ClassName;
        }

    void SetParentClass(const string& className, const CNamespace& ns,
                        const string& fileName);

    void AddMember(const string& external_name, const string& name,
                   const AutoPtr<CTypeStrings>& type,
                   const string& pointerType,
                   bool optional, const string& defaultValue,
                   bool delayed, int tag,
                   bool noPrefix, bool attlist, bool noTag, bool simple,
                   const CDataType* dataType, bool nonEmpty, const CComments& comments);
    void AddMember(const AutoPtr<CTypeStrings>& type, int tag, bool nonEmpty, bool noPrefix)
        {
            AddMember(NcbiEmptyString, NcbiEmptyString, type, NcbiEmptyString,
                      false, NcbiEmptyString, false, tag,
                      noPrefix,false,false,false,0,nonEmpty,CComments());
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    EKind GetKind(void) const;

    void SetObject(bool isObject)
        {
            m_IsObject = isObject;
        }

    bool HaveUserClass(void) const
        {
            return m_HaveUserClass;
        }
    void SetHaveUserClass(bool haveUserClass)
        {
            m_HaveUserClass = haveUserClass;
        }

    bool HaveTypeInfo(void) const
        {
            return m_HaveTypeInfo;
        }
    void SetHaveTypeInfo(bool haveTypeInfo)
        {
            m_HaveTypeInfo = haveTypeInfo;
        }

    void GenerateTypeCode(CClassContext& ctx) const;
    string GetResetCode(const string& var) const;

    void GenerateUserHPPCode(CNcbiOstream& out) const;
    void GenerateUserCPPCode(CNcbiOstream& out) const;

protected:
    virtual void GenerateClassCode(CClassCode& code,
                                   CNcbiOstream& getters,
                                   const string& methodPrefix,
                                   bool haveUserClass,
                                   const string& classPrefix) const;
    bool x_IsNullType(TMembers::const_iterator i) const;
    bool x_IsNullWithAttlist(TMembers::const_iterator i) const;
    bool x_IsAnyContentType(TMembers::const_iterator i) const;
    bool x_IsUniSeq(TMembers::const_iterator i) const;

private:
    bool m_IsObject;
    bool m_HaveUserClass;
    bool m_HaveTypeInfo;
    string m_ExternalName;
    string m_ClassName;
    string m_ParentClassName;
    CNamespace m_ParentClassNamespace;
    string m_ParentClassFileName;
public:
    TMembers m_Members;
};

class CClassRefTypeStrings : public CTypeStrings
{
public:
    CClassRefTypeStrings(const string& className, const CNamespace& ns,
                         const string& fileName,
                         const CComments& comments);

    string GetClassName(void) const;
    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    EKind GetKind(void) const;
    const CNamespace& GetNamespace(void) const;

    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;
    void GeneratePointerTypeCode(CClassContext& ctx) const;

private:
    string m_ClassName;
    CNamespace m_Namespace;
    string m_FileName;
};

class CWsdlTypeStrings : public CClassTypeStrings
{
    typedef CClassTypeStrings CParent;
public:
    CWsdlTypeStrings(const string& externalName, const string& className,
                      const string& namespaceName, const CDataType* dataType,
                      const CComments& comments);
    ~CWsdlTypeStrings(void);

    void GenerateTypeCode(CClassContext& ctx) const;
    void GenerateClassCode( CClassCode& code, CNcbiOstream& setters,
        const string& methodPrefix, bool haveUserClass,
        const string& classPrefix) const;
};

END_NCBI_SCOPE

#endif
