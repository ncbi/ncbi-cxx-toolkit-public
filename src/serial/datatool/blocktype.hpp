#ifndef BLOCKTYPE_HPP
#define BLOCKTYPE_HPP

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
*   Type description of compount types: SET, SEQUENCE and CHOICE
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/11/15 20:31:38  vasilche
* Fixed error on GCC
*
* Revision 1.2  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "type.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

END_NCBI_SCOPE

class CDataMember {
public:
    CDataMember(const string& name, const AutoPtr<CDataType>& type);
    ~CDataMember(void);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool Check(void) const;

    const string& GetName(void) const
        {
            return m_Name;
        }
    CDataType* GetType(void)
        {
            return m_Type.get();
        }
    const CDataType* GetType(void) const
        {
            return m_Type.get();
        }
    bool Optional() const
        {
            return m_Optional || m_Default;
        }
    const CDataValue* GetDefault(void) const
        {
            return m_Default.get();
        }

    void SetOptional(void);
    void SetDefault(const AutoPtr<CDataValue>& value);

private:
    string m_Name;
    AutoPtr<CDataType> m_Type;
    bool m_Optional;
    AutoPtr<CDataValue> m_Default;
};

class CDataMemberContainerType : public CDataType {
    typedef CDataType CParent;
public:
    typedef list< AutoPtr<CDataMember> > TMembers;

    void PrintASN(CNcbiOstream& out, int indent) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;

    void AddMember(const AutoPtr<CDataMember>& member);

    TObjectPtr CreateDefault(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const = 0;

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }

private:
    TMembers m_Members;
};

class CDataContainerType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

protected:
    virtual CClassInfoTmpl* CreateClassInfo(void);
};

class CDataSetType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;

protected:
    CClassInfoTmpl* CreateClassInfo(void);
};

class CDataSequenceType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;
};

class CChoiceDataType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;

    CTypeInfo* CreateTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
