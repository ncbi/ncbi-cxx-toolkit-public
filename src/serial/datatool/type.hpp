#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/typeref.hpp>
#include <list>
#include <set>
#include "autoptr.hpp"
#include "typecontext.hpp"

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CClassInfoTmpl;
class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNModule;
class ASNType;
class ASNValue;
class ASNChoiceType;
class CClassCode;
class CTypeStrings;

class ASNType {
public:
    typedef void* TObjectPtr;

    ASNType(const CDataTypeContext& context);
    //    ASNType(const CDataTypeContext& context, const string& name);
    virtual ~ASNType();

    ASNModule& GetModule(void) const
        {
            return context.GetModule();
        }

    string IdName(void) const;
    bool Skipped(const CNcbiRegistry& registry) const;
    string ClassName(const CNcbiRegistry& registry) const;
    string FileName(const CNcbiRegistry& registry) const;
    string Namespace(const CNcbiRegistry& registry) const;
    string ParentClass(const CNcbiRegistry& registry) const;
    const ASNType* ParentType(const CNcbiRegistry& registry) const;
    const string& GetVar(const CNcbiRegistry& registry,
                         const string& value) const;

    bool InChoice(void) const;

    const ASNType* GetParentType(void) const
        {
            return context.GetConfigPos().GetParentType();
        }

    virtual CNcbiOstream& Print(CNcbiOstream& out, int indent) const = 0;

    virtual bool Check(void);
    virtual bool CheckValue(const ASNValue& value) = 0;
    virtual TObjectPtr CreateDefault(const ASNValue& value) = 0;

    virtual const CTypeInfo* GetTypeInfo(void);

    static CNcbiOstream& NewLine(CNcbiOstream& out, int indent);

    void Warning(const string& mess) const;

    virtual void GenerateCode(CClassCode& code) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const ASNType* Resolve(void) const;
    virtual ASNType* Resolve(void);

    CDataTypeContext context;

    // tree info
    set<const ASNChoiceType*> choices;
    bool inSet;
    bool main;      // true for types defined in main module
    bool exported;  // true for types listed in EXPORT statements
    bool checked;   // true if this type already checked

protected:
    virtual CTypeInfo* CreateTypeInfo(void);

private:
    AutoPtr<CTypeInfo> m_CreatedTypeInfo;
};

class ASNFixedType : public ASNType {
    typedef ASNType CParent;
public:
    ASNFixedType(const CDataTypeContext& context, const string& kw);

    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultCType(void) const;

private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNNullType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
};

class ASNBooleanType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNBooleanType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNRealType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNRealType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNVisibleStringType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNVisibleStringType(const CDataTypeContext& context);
    ASNVisibleStringType(const CDataTypeContext& context, const string& kw);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNStringStoreType : public ASNVisibleStringType {
    typedef ASNVisibleStringType CParent;
public:
    ASNStringStoreType(const CDataTypeContext& context);
};

class ASNBitStringType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNBitStringType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
};

class ASNOctetStringType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNOctetStringType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNEnumeratedType : public ASNType {
    typedef ASNType CParent;
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

    ASNEnumeratedType(const CDataTypeContext& context, const string& kw);

    bool IsInteger(void) const
        {
            return isInteger;
        }
    const string& Keyword(void) const
        {
            return keyword;
        }

    void AddValue(const string& name, int value);

    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    string DefaultEnumName(CClassCode& code) const;
    void GenerateCode(CClassCode& code,
                      CNcbiOstream& hpp, CNcbiOstream& cpp,
                      CTypeStrings* tType) const;

    CTypeInfo* CreateTypeInfo(void);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual void GenerateCode(CClassCode& code) const;

private:
    string keyword;
    bool isInteger;

public:
    TValues values;
};

class ASNIntegerType : public ASNFixedType {
    typedef ASNFixedType CParent;
public:
    ASNIntegerType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNUserType : public ASNType {
    typedef ASNType CParent;
public:
    ASNUserType(const CDataTypeContext& context, const string& n);

    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const ASNType* Resolve(void) const;
    virtual ASNType* Resolve(void);

    string userTypeName;
};

class ASNOfType : public ASNType {
    typedef ASNType CParent;
public:
    ASNOfType(const CDataTypeContext& context, const string& kw);

    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    AutoPtr<ASNType> type;

private:
    string keyword;
};

class ASNSetOfType : public ASNOfType {
    typedef ASNOfType CParent;
public:
    ASNSetOfType(const CDataTypeContext& context);

    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNSequenceOfType : public ASNOfType {
    typedef ASNOfType CParent;
public:
    ASNSequenceOfType(const CDataTypeContext& context);

    CTypeInfo* CreateTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNMember {
public:
    ASNMember(void);
    ~ASNMember(void);

    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    bool Check(void);

    bool Optional() const
        {
            return optional || defaultValue;
        }

    string name;
    AutoPtr<ASNType> type;
    bool optional;
    AutoPtr<ASNValue> defaultValue;
};

class ASNMemberContainerType : public ASNType {
    typedef ASNType CParent;
public:
    typedef list<AutoPtr<ASNMember> > TMembers;

    ASNMemberContainerType(const CDataTypeContext& context, const string& kw);
    
    CNcbiOstream& Print(CNcbiOstream& out, int indent) const;

    bool Check(void);

    TObjectPtr CreateDefault(const ASNValue& value);
    
    TMembers members;

private:
    string keyword;
};

class ASNContainerType : public ASNMemberContainerType {
    typedef ASNMemberContainerType CParent;
public:
    ASNContainerType(const CDataTypeContext& context, const string& kw);
    
    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GenerateCode(CClassCode& code) const;

protected:
    virtual CClassInfoTmpl* CreateClassInfo(void);
};

class ASNSetType : public ASNContainerType {
    typedef ASNContainerType CParent;
public:
    ASNSetType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);

protected:
    CClassInfoTmpl* CreateClassInfo(void);
};

class ASNSequenceType : public ASNContainerType {
    typedef ASNContainerType CParent;
public:
    ASNSequenceType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
};

class ASNChoiceType : public ASNMemberContainerType {
    typedef ASNMemberContainerType CParent;
public:
    ASNChoiceType(const CDataTypeContext& context);

    bool Check(void);
    bool CheckValue(const ASNValue& value);

    CTypeInfo* CreateTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

#endif
