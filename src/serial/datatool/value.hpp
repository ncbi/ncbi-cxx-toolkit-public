#ifndef ASNVALUE_HPP
#define ASNVALUE_HPP

#include <list>
#include <ostream>
#include <autoptr.hpp>

using namespace std;

class ASNValue {
public:
    ASNValue();
    virtual ~ASNValue();

    virtual ostream& Print(ostream& out, int indent) = 0;

    void Warning(const string& mess) const;
    
    int line;
};

class ASNNullValue : public ASNValue {
public:

    ostream& Print(ostream& out, int indent);
};

class ASNBoolValue : public ASNValue {
public:
    ASNBoolValue(bool v);

    ostream& Print(ostream& out, int indent);

    bool value;
};

class ASNIntegerValue : public ASNValue {
public:
    ASNIntegerValue(long v);

    ostream& Print(ostream& out, int indent);

    long value;
};

class ASNStringValue : public ASNValue {
public:
    ASNStringValue(const string& v);

    ostream& Print(ostream& out, int indent);

    string value;
};

class ASNBitStringValue : public ASNValue {
public:
    ASNBitStringValue(const string& v);

    ostream& Print(ostream& out, int indent);

    string value;
};

class ASNIdValue : public ASNValue {
public:
    ASNIdValue(const string& v);

    ostream& Print(ostream& out, int indent);

    string id;
};

class ASNNamedValue : public ASNValue {
public:
    ASNNamedValue();
    ASNNamedValue(const string& id, const AutoPtr<ASNValue>& v);

    ostream& Print(ostream& out, int indent);

    string name;
    AutoPtr<ASNValue> value;
};

class ASNBlockValue : public ASNValue {
public:
    typedef list<AutoPtr<ASNValue> > TValues;

    ostream& Print(ostream& out, int indent);

    TValues values;
};

#endif
