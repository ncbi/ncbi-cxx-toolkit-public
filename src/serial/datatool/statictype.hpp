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
    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;

    TObjectPtr CreateDefault(const CDataValue& value) const;

    AutoPtr<CTypeStrings> GetFullCType(void) const;
    //virtual string GetDefaultCType(void) const;
    virtual const char* GetDefaultCType(void) const = 0;
    virtual const char* GetXMLContents(void) const = 0;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const;
};

class CNullDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const;
};

class CBoolDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const;

    void PrintDTDExtra(CNcbiOstream& out) const;
};

class CRealDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CRealDataType(void);
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetRealTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
};

class CStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    enum EType {
        eStringTypeVisible,
        eStringTypeUTF8
    };

    CStringDataType(EType type = eStringTypeVisible);

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
    
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

    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
};

class CBitStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual bool PrintXMLSchemaContents(CNcbiOstream& out, int indent, const CDataMember* mem) const;
};

class COctetStringDataType : public CBitStringDataType {
    typedef CBitStringDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
    virtual bool IsCompressed(void) const;
protected:
    virtual bool x_AsBitString(void) const;
};

class CBase64BinaryDataType : public COctetStringDataType {
    typedef COctetStringDataType CParent;
public:
    virtual string GetSchemaTypeString(void) const;
    virtual bool IsCompressed(void) const;
protected:
    virtual bool x_AsBitString(void) const;
};

class CIntDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CIntDataType(void);
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
};

class CBigIntDataType : public CIntDataType {
    typedef CIntDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
    virtual string GetSchemaTypeString(void) const;
};

class CAnyContentDataType : public CStaticDataType {
public:
    bool CheckValue(const CDataValue& value) const;
    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;

    TObjectPtr CreateDefault(const CDataValue& value) const;

    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

END_NCBI_SCOPE

#endif
