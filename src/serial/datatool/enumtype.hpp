#ifndef ENUMTYPE_HPP
#define ENUMTYPE_HPP

#include "type.hpp"
#include <list>

class CEnumDataType : public CDataType {
    typedef CDataType CParent;
public:
    class Value {
    public:
        Value()
            {
            }
        Value(const string& i, int v)
            : id(i), value(v)
            {
            }
        string id;
        int value;
    };
    typedef list<Value> TValues;

    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;

    void AddValue(const string& name, long value);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

private:
    struct SEnumCInfo {
        string enumName;
        string cType;
        string valuePrefix;
        
        SEnumCInfo(const string& name, const string& type, const string& prefix)
            : enumName(name), cType(type), valuePrefix(prefix)
            {
            }
    };
    
    string DefaultEnumName(void) const;
    SEnumCInfo GetEnumCInfo(void) const;
public:

    void GenerateCode(CClassCode& code,
                      CTypeStrings* tType) const;

    CTypeInfo* CreateTypeInfo(void);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual void GenerateCode(CClassCode& code) const;

private:
    TValues m_Values;
};

class CIntEnumDataType : public CEnumDataType {
    typedef CEnumDataType CParent;
public:
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
