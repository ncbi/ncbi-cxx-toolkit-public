#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <list>
#include <set>
#include "autoptr.hpp"
#include "value.hpp"

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CClassInfoTmpl;
class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNModule;
class CCode;

struct CTypeStrings {
    enum ETypeType {
        eStdType,
        eClassType,
        eComplexType
    };
    CTypeStrings(const string& c)
        : type(eStdType), cType(c)
        {}
    CTypeStrings(ETypeType t, const string& c)
        : type(t), cType(c)
        {}
    CTypeStrings(const string& c, const string& m)
        : type(eComplexType), cType(c), macro(m)
        {}
    ETypeType type;
    string cType;
    string macro;

    bool IsSimple(void) const
        { return type == eStdType; }

    CTypeStrings ToSimple(void) const;
        
    string GetRef(void) const;

    void AddMember(CNcbiOstream& header, CNcbiOstream& code,
                   const string& member) const;
    void AddMember(CNcbiOstream& header, CNcbiOstream& code,
                   const string& name, const string& member) const;
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

    virtual ostream& Print(ostream& out, int indent) const = 0;

    virtual bool Check(void);
    virtual bool CheckValue(const ASNValue& value) = 0;
    virtual TObjectPtr CreateDefault(const ASNValue& value) = 0;

    virtual const CTypeInfo* GetTypeInfo(void);

    static ostream& NewLine(ostream& out, int indent);

    void Warning(const string& mess) const;

    virtual void GenerateCode(CCode& code) const;
    virtual void CollectUserTypes(set<string>& types) const;
    virtual void GenerateCode(ostream& header, ostream& code,
                              const string& name,
                              const CNcbiRegistry& def) const;

    virtual CTypeStrings GetCType(const CNcbiRegistry& def,
                                  const string& section,
                                  const string& key) const;
    virtual string GetDefaultCType(void) const;
    virtual bool SimpleType(void) const;

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

    bool SimpleType(void) const;
private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
public:
    ASNNullType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
    void GenerateCode(ostream& , ostream& ,
                      const string& ,
                      const CNcbiRegistry& ) const;
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
    CTypeStrings GetCType(const CNcbiRegistry& def,
                          const string& section,
                          const string& key) const;
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
    string GetDefaultCType(void) const;
    bool SimpleType(void) const;

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

    virtual void CollectUserTypes(set<string>& types) const;
    CTypeStrings GetCType(const CNcbiRegistry& def,
                          const string& section,
                          const string& key) const;

    string userTypeName;

    ASNType* Resolve(void);
};

class ASNOfType : public ASNType {
public:
    ASNOfType(const string& kw, const AutoPtr<ASNType>& t);

    ostream& Print(ostream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    virtual void CollectUserTypes(set<string>& types) const;

    AutoPtr<ASNType> type;

private:
    string keyword;
};

class ASNSetOfType : public ASNOfType {
public:
    ASNSetOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);
    
    CTypeStrings GetCType(const CNcbiRegistry& def,
                          const string& section,
                          const string& key) const;
};

class ASNSequenceOfType : public ASNOfType {
public:
    ASNSequenceOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);

    CTypeStrings GetCType(const CNcbiRegistry& def,
                          const string& section,
                          const string& key) const;
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
    
    virtual void CollectUserTypes(set<string>& types) const;

    TMembers members;

private:
    string keyword;
};

class ASNContainerType : public ASNMemberContainerType {
    typedef ASNMemberContainerType CParent;
public:
    ASNContainerType(ASNModule& module, const string& kw);
    
    CTypeInfo* CreateTypeInfo(void);
    
    void GenerateCode(ostream& header, ostream& code,
                      const string& name,
                      const CNcbiRegistry& def) const;

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
    void GenerateCode(ostream& header, ostream& code,
                      const string& name,
                      const CNcbiRegistry& def) const;
};

#endif
