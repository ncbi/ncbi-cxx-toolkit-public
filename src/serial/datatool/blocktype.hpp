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
*   Type description of compound types: SET, SEQUENCE and CHOICE
*
*/

#include "type.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CClassTypeInfo;

class CDataMember {
public:
    CDataMember(const string& name, const AutoPtr<CDataType>& type);
    ~CDataMember(void);

    void PrintASN(CNcbiOstream& out, int indent, bool last) const;
    void PrintSpecDump(CNcbiOstream& out, int indent, const char* tag) const;
    void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTD(CNcbiOstream& out) const;

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
    bool Optional(void) const
        {
            return m_NoPrefix ? m_Optional : (m_Optional || m_Default);
        }
    bool NoPrefix(void) const
        {
            return m_NoPrefix;
        }
    bool Attlist(void) const
        {
            return m_Attlist;
        }
    bool Notag(void) const
        {
            return m_Notag;
        }
    bool SimpleType(void) const
        {
            return m_SimpleType;
        }
    const CDataValue* GetDefault(void) const
        {
            return m_Default.get();
        }
    bool Nillable(void) const
        {
            return m_Nillable;
        }

    void SetOptional(void);
    void SetNoPrefix(void);
    void SetAttlist(void);
    void SetNotag(void);
    void SetSimpleType(void);
    void SetDefault(const AutoPtr<CDataValue>& value);
    void SetNillable(void);

    CComments& Comments(void)
        {
            return m_Comments;
        }
    const CComments& GetComments(void) const
        {
            return m_Comments;
        }
    void SetRestrictions(const list<CMemberFacet>& c) {
        m_Restrictions = c;
    }
    const list<CMemberFacet>& GetRestrictions(void) const {
        return m_Restrictions;
    }

private:
    string m_Name;
    AutoPtr<CDataType> m_Type;
    bool m_Optional;
    bool m_NoPrefix;
    bool m_Attlist;
    bool m_Notag;
    bool m_SimpleType;
    bool m_Nillable;
    AutoPtr<CDataValue> m_Default;
    CComments m_Comments;
    list<CMemberFacet> m_Restrictions;
};

class CDataMemberContainerType : public CDataType {
    typedef CDataType CParent;
public:
    typedef list< AutoPtr<CDataMember> > TMembers;

    virtual void PrintASN(CNcbiOstream& out, int indent) const override;
    virtual void PrintSpecDumpExtra(CNcbiOstream& out, int indent) const override;
    virtual void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const override;
    virtual void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const override;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const override;
    virtual void PrintDTDExtra(CNcbiOstream& out) const override;

    virtual void FixTypeTree(void) const override;
    virtual bool CheckType(void) const override;

    void AddMember(const AutoPtr<CDataMember>& member);

    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;

    virtual const char* XmlMemberSeparator(void) const = 0;

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }

    CComments& LastComments(void)
        {
            return m_LastComments;
        }
    bool UniElementNameExists(const string& name) const;

protected:
    TMembers m_Members;
    CComments m_LastComments;
};

class CDataContainerType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    virtual CTypeInfo* CreateTypeInfo(void) override;
    
    virtual const char* XmlMemberSeparator(void) const override;

    virtual AutoPtr<CTypeStrings> GenerateCode(void) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual AutoPtr<CTypeStrings> GetRefCType(void) const override;
    virtual string      GetSpecKeyword(void) const override;

protected:
    AutoPtr<CTypeStrings> AddMembers(AutoPtr<CClassTypeStrings>& code) const;
    virtual CClassTypeInfo* CreateClassInfo(void);
};

class CDataSetType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    virtual bool CheckValue(const CDataValue& value) const override;

    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;

protected:
    virtual CClassTypeInfo* CreateClassInfo(void) override;
};

class CDataSequenceType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    virtual bool CheckValue(const CDataValue& value) const override;

    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
};

class CWsdlDataType : public CDataContainerType {
    typedef CDataContainerType CParent;

public:
    enum EType {
        eWsdlService,
        eWsdlEndpoint,
        eWsdlOperation,
        eWsdlHeaderInput,
        eWsdlInput,
        eWsdlHeaderOutput,
        eWsdlOutput,
        eWsdlMessage
    };
    void SetWsdlType(EType type)
    {
        m_Type = type;
    }
    EType GetWsdlType(void) const
    {
        return m_Type;
    }

    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;

    virtual void PrintASN(CNcbiOstream&, int)     const override { }
    virtual void PrintJSONSchema(CNcbiOstream&, int, list<string>&, bool) const override { }
    virtual void PrintXMLSchema(CNcbiOstream&, int, bool) const override { }
    virtual void PrintDTDElement(CNcbiOstream&, bool) const override { }
    virtual bool CheckValue(const CDataValue&) const override { return false; }
    virtual TObjectPtr CreateDefault(const CDataValue&) const override { return 0; }

private:
    EType m_Type;
};

END_NCBI_SCOPE

#endif
