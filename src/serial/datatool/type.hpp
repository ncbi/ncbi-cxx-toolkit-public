#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
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

    ASNType(ASNModule& module);
    ASNType(ASNModule& module, const string& name);
    virtual ~ASNType();

    ASNModule& GetModule(void) const
        {
            return m_Module;
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

    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
    //    virtual bool SimpleType(void) const;

    int line;
    string name; // for named type

protected:
    virtual CTypeInfo* CreateTypeInfo(void);

private:
    ASNModule& m_Module;
    AutoPtr<CTypeInfo> m_CreatedTypeInfo;
};

class ASNFixedType : public ASNType {
public:
    ASNFixedType(ASNModule& module, const string& kw);

    ostream& Print(ostream& out, int indent) const;

    //    bool SimpleType(void) const;

    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
    virtual string GetDefaultCType(void) const;

private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
public:
    ASNNullType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
};

class ASNBooleanType : public ASNFixedType {
public:
    ASNBooleanType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNRealType : public ASNFixedType {
public:
    ASNRealType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNVisibleStringType : public ASNFixedType {
public:
    ASNVisibleStringType(ASNModule& module);
    ASNVisibleStringType(ASNModule& module, const string& kw);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNStringStoreType : public ASNVisibleStringType {
public:
    ASNStringStoreType(ASNModule& module);
};

class ASNBitStringType : public ASNFixedType {
public:
    ASNBitStringType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
};

class ASNOctetStringType : public ASNFixedType {
public:
    ASNOctetStringType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
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

    ASNEnumeratedType(ASNModule& module, const string& kw);

    void AddValue(const string& name, int value);

    ostream& Print(ostream& out, int indent) const;

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    CTypeInfo* CreateTypeInfo(void);
    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
    //    bool SimpleType(void) const;

private:
    string keyword;

public:
    TValues values;
};

class ASNIntegerType : public ASNFixedType {
public:
    ASNIntegerType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
};

class ASNUserType : public ASNType {
public:
    ASNUserType(ASNModule& module, const string& n);

    ostream& Print(ostream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;

    string userTypeName;

    ASNType* Resolve(void) const;
};

class ASNOfType : public ASNType {
public:
    ASNOfType(const string& kw, const AutoPtr<ASNType>& t);

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
    ASNSetOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
};

class ASNSequenceOfType : public ASNOfType {
public:
    ASNSequenceOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);

    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
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

    ASNMemberContainerType(ASNModule& module, const string& kw);
    
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
    ASNContainerType(ASNModule& module, const string& kw);
    
    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GenerateCode(CClassCode& code) const;
/*
    void GetCType(CTypeStrings& tType,
                  CClassCode& code, const string& key) const;
*/
protected:
    virtual CClassInfoTmpl* CreateClassInfo(void);
};

class ASNSetType : public ASNContainerType {
    typedef ASNContainerType CParent;
public:
    ASNSetType(ASNModule& module);

    bool CheckValue(const ASNValue& value);

protected:
    CClassInfoTmpl* CreateClassInfo(void);
};

class ASNSequenceType : public ASNContainerType {
    typedef ASNContainerType CParent;
public:
    ASNSequenceType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
};

class ASNChoiceType : public ASNMemberContainerType {
    typedef ASNMemberContainerType CParent;
public:
    ASNChoiceType(ASNModule& module);

    bool CheckValue(const ASNValue& value);

    CTypeInfo* CreateTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType,
                          CClassCode& code, const string& key) const;
};

#endif
