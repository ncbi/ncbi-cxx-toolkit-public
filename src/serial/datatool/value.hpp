#ifndef ASNVALUE_HPP
#define ASNVALUE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <list>
#include "autoptr.hpp"
#include "typecontext.hpp"

USING_NCBI_SCOPE;

class ASNValue {
public:
    ASNValue(const CFilePosition& pos);
    virtual ~ASNValue();

    virtual CNcbiOstream& Print(CNcbiOstream& out, int indent) = 0;

    void Warning(const string& mess) const;
    
    CFilePosition filePos;
};

class ASNNullValue : public ASNValue {
public:
    ASNNullValue(const CFilePosition& pos);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);
};

class ASNBoolValue : public ASNValue {
public:
    ASNBoolValue(const CFilePosition& pos, bool v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    bool value;
};

class ASNIntegerValue : public ASNValue {
public:
    ASNIntegerValue(const CFilePosition& pos, int v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    int value;
};

class ASNStringValue : public ASNValue {
public:
    ASNStringValue(const CFilePosition& pos, const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string value;
};

class ASNBitStringValue : public ASNValue {
public:
    ASNBitStringValue(const CFilePosition& pos, const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string value;
};

class ASNIdValue : public ASNValue {
public:
    ASNIdValue(const CFilePosition& pos, const string& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string id;
};

class ASNNamedValue : public ASNValue {
public:
    ASNNamedValue(const CFilePosition& pos);
    ASNNamedValue(const CFilePosition& pos,
                  const string& id, const AutoPtr<ASNValue>& v);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    string name;
    AutoPtr<ASNValue> value;
};

class ASNBlockValue : public ASNValue {
public:
    typedef list<AutoPtr<ASNValue> > TValues;

    ASNBlockValue(const CFilePosition& pos);

    CNcbiOstream& Print(CNcbiOstream& out, int indent);

    TValues values;
};

#endif
