#ifndef ASNTYPE_HPP
#define ASNTYPE_HPP

#include <ostream>
#include <list>
#include "autoptr.hpp"

using namespace std;

class ASNValue;
class ASNModule;

class ASNType {
public:
    ASNType();
    virtual ~ASNType();

    virtual ostream& Print(ostream& out, int indent) const = 0;

    virtual bool Check(const ASNModule& module);
    virtual bool CheckValue(const ASNModule& module,
                            const ASNValue& value) = 0;

    static ostream& NewLine(ostream& out, int indent);

    void Warning(const string& mess) const;

    int line;
};

class ASNFixedType : public ASNType {
public:
    ASNFixedType(const string& kw);

    ostream& Print(ostream& out, int indent) const;

private:
    string keyword;
};

class ASNNullType : public ASNFixedType {
public:
    ASNNullType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNBooleanType : public ASNFixedType {
public:
    ASNBooleanType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNRealType : public ASNFixedType {
public:
    ASNRealType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNVisibleStringType : public ASNFixedType {
public:
    ASNVisibleStringType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNStringStoreType : public ASNFixedType {
public:
    ASNStringStoreType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNBitStringType : public ASNFixedType {
public:
    ASNBitStringType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNOctetStringType : public ASNFixedType {
public:
    ASNOctetStringType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNEnumeratedType : public ASNType {
public:
    class Value {
    public:
        Value()
            : hasValue(false)
            {
            }
        Value(const string& i)
            : id(i), hasValue(false)
            {
            }
        Value(const string& i, long v)
            : id(i), hasValue(true), value(v)
            {
            }
        string id;
        bool hasValue;
        long value;
    };
    typedef list<Value> TValues;

    ASNEnumeratedType(const string& kw);

    void AddValue(const string& name);
    void AddValue(const string& name, long value);

    ostream& Print(ostream& out, int indent) const;

    bool CheckValue(const ASNModule& module, const ASNValue& value);

private:
    string keyword;

public:
    TValues values;
};

class ASNIntegerType : public ASNFixedType {
public:
    ASNIntegerType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNUserType : public ASNType {
public:
    ASNUserType(const string& n);

    ostream& Print(ostream& out, int indent) const;

    bool Check(const ASNModule& module);
    bool CheckValue(const ASNModule& module, const ASNValue& value);

    string name;
};

class ASNOfType : public ASNType {
public:
    ASNOfType(const string& kw, const AutoPtr<ASNType>& t);

    ostream& Print(ostream& out, int indent) const;

    bool Check(const ASNModule& module);
    bool CheckValue(const ASNModule& module, const ASNValue& value);

private:
    string keyword;
    AutoPtr<ASNType> type;
};

class ASNSetOfType : public ASNOfType {
public:
    ASNSetOfType(const AutoPtr<ASNType>& type);
};

class ASNSequenceOfType : public ASNOfType {
public:
    ASNSequenceOfType(const AutoPtr<ASNType>& type);
};

class ASNMember {
public:
    ASNMember()
        : optional(false)
        {
        }

    ostream& Print(ostream& out, int indent) const;

    bool Check(const ASNModule& module);

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

    ASNContainerType(const string& kw);
    
    ostream& Print(ostream& out, int indent) const;

    bool Check(const ASNModule& module);

    TMembers members;

private:
    string keyword;
};

class ASNSetType : public ASNContainerType {
public:
    ASNSetType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNSequenceType : public ASNContainerType {
public:
    ASNSequenceType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

class ASNChoiceType : public ASNContainerType {
public:
    ASNChoiceType();

    bool CheckValue(const ASNModule& module, const ASNValue& value);
};

#endif
