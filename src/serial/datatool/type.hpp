#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <corelib/ncbistd.hpp>
#include <ostream>
#include <list>
#include <autoptr.hpp>
#include <value.hpp>

BEGIN_NCBI_SCOPE

class CTypeInfo;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNModule;

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

private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
public:
    ASNNullType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
};

class ASNBooleanType : public ASNFixedType {
public:
    ASNBooleanType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
};

class ASNRealType : public ASNFixedType {
public:
    ASNRealType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
};

class ASNVisibleStringType : public ASNFixedType {
public:
    ASNVisibleStringType(ASNModule& module);
    ASNVisibleStringType(ASNModule& module, const string& kw);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);
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
};

class ASNUserType : public ASNType {
public:
    ASNUserType(ASNModule& module, const string& n);

    ostream& Print(ostream& out, int indent) const;

    bool Check(void);
    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    const CTypeInfo* GetTypeInfo(void);

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

    AutoPtr<ASNType> type;

private:
    string keyword;
};

class ASNSetOfType : public ASNOfType {
public:
    ASNSetOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);
};

class ASNSequenceOfType : public ASNOfType {
public:
    ASNSequenceOfType(const AutoPtr<ASNType>& type);

    CTypeInfo* CreateTypeInfo(void);
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

class ASNContainerType : public ASNType {
public:
    typedef list<AutoPtr<ASNMember> > TMembers;

    ASNContainerType(ASNModule& module, const string& kw);
    
    ostream& Print(ostream& out, int indent) const;

    bool Check(void);

    TMembers members;

    CTypeInfo* CreateTypeInfo(void);

private:
    string keyword;
};

class ASNSetType : public ASNContainerType {
public:
    ASNSetType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
};

class ASNSequenceType : public ASNContainerType {
public:
    ASNSequenceType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);
};

class ASNChoiceType : public ASNContainerType {
public:
    ASNChoiceType(ASNModule& module);

    bool CheckValue(const ASNValue& value);
    TObjectPtr CreateDefault(const ASNValue& value);

    CTypeInfo* CreateTypeInfo(void);
};

#endif
