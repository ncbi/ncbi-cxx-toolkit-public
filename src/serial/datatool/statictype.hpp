#ifndef STATICTYPE_HPP
#define STATICTYPE_HPP

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
*   Predefined types: INTEGER, BOOLEAN, VisibleString etc.
*
*/

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CStaticDataType : public CDataType {
    typedef CDataType CParent;
public:
    virtual void PrintASN(CNcbiOstream& out, int indent) const override;
    virtual void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const override;
    virtual void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const override;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const override;

    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;

    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    //virtual string GetDefaultCType(void) const;
    virtual const char* GetDefaultCType(void) const = 0;
    virtual const char* GetXMLContents(void) const = 0;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const;
};

class CNullDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;

    virtual CTypeRef GetTypeInfo(void) override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const override;
};

class CBoolDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual CTypeRef GetTypeInfo(void) override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const override;

    virtual void PrintDTDExtra(CNcbiOstream& out) const override;
};

class CRealDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CRealDataType(void);
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
};

class CStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    enum EType {
        eStringTypeVisible,
        eStringTypeUTF8
    };

    CStringDataType(EType type = eStringTypeVisible);

    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
    
    EType GetStringType(void) const
    {
        return m_Type;
    }
protected:
    EType m_Type;
};

class CStringStoreDataType : public CStringDataType {
    typedef CStringDataType CParent;
public:
    CStringStoreDataType(void);

    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
};

class CBitStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CBitStringDataType(CDataType* bitenum = nullptr);
    virtual void FixTypeTree(void) const override;
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const override;
private:
    CDataType* m_BitStringEnum;
};

class COctetStringDataType : public CBitStringDataType {
    typedef CBitStringDataType CParent;
public:
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
    virtual bool IsCompressed(void) const;
protected:
    virtual bool x_AsBitString(void) const;
};

class CBase64BinaryDataType : public COctetStringDataType {
    typedef COctetStringDataType CParent;
public:
    virtual string GetSchemaTypeString(void) const override;
    virtual bool IsCompressed(void) const override;
protected:
    virtual bool x_AsBitString(void) const override;
};

class CIntDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CIntDataType(void);
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual CTypeRef GetTypeInfo(void) override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
};

class CBigIntDataType : public CIntDataType {
    typedef CIntDataType CParent;
public:
    CBigIntDataType(bool bAsnBigInt = false) : m_bAsnBigInt(bAsnBigInt) {
    }
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual CTypeRef GetTypeInfo(void) override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
    virtual string GetSchemaTypeString(void) const override;
protected:
    bool m_bAsnBigInt;
};

class CAnyContentDataType : public CStaticDataType {
public:
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual void PrintASN(CNcbiOstream& out, int indent) const override;
    virtual void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const override;
    virtual void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const override;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const override;

    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;

    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetDefaultCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
    virtual const char* GetXMLContents(void) const override;
};

END_NCBI_SCOPE

#endif
