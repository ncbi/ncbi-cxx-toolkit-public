#ifndef ASNVALUE_HPP
#define ASNVALUE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <list>
#include <autoptr.hpp>

USING_NCBI_SCOPE;

class ASNValue {
public:
    ASNValue();
    virtual ~ASNValue();

    virtual CNcbiOstream& Print(CNcbiOstream& out, int indent) = 0;

    void Warning(const string& mess) const;
    
    int line;
};

class ASNNullValue : public ASNValue {
public:

    CNcbiOstream& Print(CNcbiOstream& out, int indent);
};

class ASNBoolValue : public ASNValue {
public:
    ASNBoolValue(bool v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    bool value;
};

class ASNIntegerValue : public ASNValue {
public:
    ASNIntegerValue(int v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    int value;
};

class ASNStringValue : public ASNValue {
public:
    ASNStringValue(const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string value;
};

class ASNBitStringValue : public ASNValue {
public:
    ASNBitStringValue(const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string value;
};

class ASNIdValue : public ASNValue {
public:
    ASNIdValue(const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string id;
};

class ASNNamedValue : public ASNValue {
public:
    ASNNamedValue();
    ASNNamedValue(const string& id, const AutoPtr<ASNValue>& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string name;
    AutoPtr<ASNValue> value;
};

class ASNBlockValue : public ASNValue {
public:
    typedef list<AutoPtr<ASNValue> > TValues;

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    TValues values;
};

#endif
