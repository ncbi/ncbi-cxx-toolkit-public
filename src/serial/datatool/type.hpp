#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/typeref.hpp>
#include <list>
#include <set>
#include <map>
#include "autoptr.hpp"
#include "value.hpp"

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CClassInfoTmpl;
class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNModule;
class ASNType;
class ASNChoiceType;
class CClassCode;

class CTypeStrings {
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;

    enum ETypeType {
        eStdType,
        eClassType,
        eComplexType,
        ePointerType
    };
    void SetStd(const string& c);
    void SetClass(const string& c);
    void SetComplex(const string& c, const string& m);
    void SetComplex(const string& c, const string& m,
                    const CTypeStrings& arg);
    void SetComplex(const string& c, const string& m,
                    const CTypeStrings& arg1, const CTypeStrings& arg2);

    ETypeType type;
    string cType;
    string macro;

    void ToSimple(void);
        
    string GetRef(void) const;

    void AddHPPInclude(const string& s)
        {
            m_HPPIncludes.insert(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_CPPIncludes.insert(s);
        }
    void AddForwardDeclaration(const string& s, const string& ns)
        {
            m_ForwardDeclarations[s] = ns;
        }
    void AddIncludes(const CTypeStrings& arg);

    void AddMember(CClassCode& code,
                   const string& member) const;
    void AddMember(CClassCode& code,
                   const string& name, const string& member) const;
private:
    void x_AddMember(CClassCode& code,
                     const string& name, const string& member) const;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;
};

class ASNType {
public:
    typedef void* TObjectPtr;

    ASNType(const CDataTypeContext& context);
    ASNType(const CDataTypeContext& context, const string& name);
    virtual ~ASNType();

    ASNModule& GetModule(void) const
        {
            return context.GetModule();
        }

    string ClassName(const CNcbiRegistry& registry) const;
    string FileName(const CNcbiRegistry& registry) const;
    string Namespace(const CNcbiRegistry& registry) const;
    string ParentClass(const CNcbiRegistry& registry) const;
    const ASNType* ParentType(const CNcbiRegistry& registry) const;
    const string& GetVar(const CNcbiRegistry& registry,
                         const string& value) const;

    virtual ostream& Print(ostream& out, int indent) const = 0;

    virtual bool Check(void);
    virtual bool CheckValue(const ASNValue& value) = 0;
    virtual TObjectPtr CreateDefault(const ASNValue& value) = 0;

    virtual const CTypeInfo* GetTypeInfo(void);

    static ostream& NewLine(ostream& out, int indent);

    void Warning(const string& mess) const;

    virtual void GenerateCode(CClassCode& code) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    string name; // for named type
    CDataTypeContext context;

    // tree info
    set<const ASNChoiceType*> choices;
    bool inSet;

protected:
    virtual CTypeInfo* CreateTypeInfo(void);

private:
    AutoPtr<CTypeInfo> m_CreatedTypeInfo;
};

class ASNFixedType : public ASNType {
public:
    ASNFixedType(const CDataTypeContext& context, const string& kw);

    ostream& Print(ostream& out, int indent) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultCType(void) const;

private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
public:
    ASNNullType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
};

class ASNBooleanType : public ASNFixedType {
public:
    ASNBooleanType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNRealType : public ASNFixedType {
public:
    ASNRealType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNVisibleStringType : public ASNFixedType {
public:
    ASNVisibleStringType(const CDataTypeContext& context);
    ASNVisibleStringType(const CDataTypeContext& context, const string& kw);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNStringStoreType : public ASNVisibleStringType {
public:
    ASNStringStoreType(const CDataTypeContext& context);
};

class ASNBitStringType : public ASNFixedType {
public:
    ASNBitStringType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
};

class ASNOctetStringType : public ASNFixedType {
public:
    ASNOctetStringType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNEnumeratedType : public ASNType {
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

    ostream& Print(ostream& out, int indent) const;

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    CTypeInfo* CreateTypeInfo(void);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

private:
    string keyword;
    bool isInteger;

public:
    TValues values;
};

class ASNIntegerType : public ASNFixedType {
public:
    ASNIntegerType(const CDataTypeContext& context);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNUserType : public ASNType {
public:
    ASNUserType(const CDataTypeContext& context, const string& n);

    ostream& Print(ostream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    string userTypeName;

    ASNType* Resolve(void) const;
};

class ASNOfType : public ASNType {
public:
    ASNOfType(const CDataTypeContext& context, const string& kw);

    ostream& Print(ostream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    AutoPtr<ASNType> type;

private:
    string keyword;
};

class ASNSetOfType : public ASNOfType {
public:
    ASNSetOfType(const CDataTypeContext& context);

    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNSequenceOfType : public ASNOfType {
public:
    ASNSequenceOfType(const CDataTypeContext& context);

    CTypeInfo* CreateTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
};

class ASNMember {
public:
    ASNMember()
        : optional(false)
        {
        }

    ostream& Print(ostream& out, int indent) const;

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
public:
    typedef list<AutoPtr<ASNMember> > TMembers;

    ASNMemberContainerType(const CDataTypeContext& context, const string& kw);
    
    ostream& Print(ostream& out, int indent) const;

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
