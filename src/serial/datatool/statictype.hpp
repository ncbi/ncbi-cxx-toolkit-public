#ifndef STATICTYPE_HPP
#define STATICTYPE_HPP

#include "type.hpp"

class CStaticDataType : public CDataType {
    typedef CDataType CParent;
public:
    void PrintASN(CNcbiOstream& out, int indent) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const = 0;
};

class CNullDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
    virtual const char* GetASNKeyword(void) const;
};

class CBoolDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CRealDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CStringDataType(const string& asnKeyword = "VisibleString");

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;

private:
    string m_ASNKeyword;
};

class CBitStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual const char* GetASNKeyword(void) const;
};

class COctetStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual const char* GetASNKeyword(void) const;
};

class CIntDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
