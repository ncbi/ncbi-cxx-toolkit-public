#include "value.hpp"
#include "type.hpp"

inline
CNcbiOstream& NewLine(CNcbiOstream& out, int indent)
{
    return ASNType::NewLine(out, indent);
}

ASNValue::ASNValue(const CFilePosition& pos)
    : filePos(pos)
{
}

ASNValue::~ASNValue()
{
}

void ASNValue::Warning(const string& mess) const
{
    cerr << filePos << ": " << mess << endl;
}

ASNNullValue::ASNNullValue(const CFilePosition& pos)
    : ASNValue(pos)
{
}

CNcbiOstream& ASNNullValue::Print(CNcbiOstream& out, int )
{
    return out << "NULL";
}

ASNBoolValue::ASNBoolValue(const CFilePosition& pos, bool v)
    : ASNValue(pos), value(v)
{
}

CNcbiOstream& ASNBoolValue::Print(CNcbiOstream& out, int )
{
    return out << (value? "TRUE": "FALSE");
}

ASNIntegerValue::ASNIntegerValue(const CFilePosition& pos, int v)
    : ASNValue(pos), value(v)
{
}

CNcbiOstream& ASNIntegerValue::Print(CNcbiOstream& out, int )
{
    return out << value;
}

ASNStringValue::ASNStringValue(const CFilePosition& pos, const string& v)
    : ASNValue(pos), value(v)
{
}

CNcbiOstream& ASNStringValue::Print(CNcbiOstream& out, int )
{
    out << '"';
    for ( string::const_iterator i = value.begin(), end = value.end();
          i != end; ++i ) {
        char c = *i;
        if ( c == '"' )
            out << "\"\"";
        else
            out << c;
    }
    return out << '"';
}

ASNBitStringValue::ASNBitStringValue(const CFilePosition& pos, const string& v)
    : ASNValue(pos), value(v)
{
}

CNcbiOstream& ASNBitStringValue::Print(CNcbiOstream& out, int )
{
    return out << value;
}

ASNIdValue::ASNIdValue(const CFilePosition& pos, const string& v)
    : ASNValue(pos), id(v)
{
}

CNcbiOstream& ASNIdValue::Print(CNcbiOstream& out, int )
{
    return out << id;
}

ASNNamedValue::ASNNamedValue(const CFilePosition& pos)
    : ASNValue(pos)
{
}

ASNNamedValue::ASNNamedValue(const CFilePosition& pos,
                             const string& n, const AutoPtr<ASNValue>& v)
    : ASNValue(pos), name(n), value(v)
{
}

CNcbiOstream& ASNNamedValue::Print(CNcbiOstream& out, int indent)
{
    out << name;
    if ( dynamic_cast<const ASNNamedValue*>(value.get()) ||
         dynamic_cast<const ASNBlockValue*>(value.get()) ) {
        indent++;
        NewLine(out, indent);
    }
    else {
        out << ' ';
    }
    return value->Print(out, indent);
}

ASNBlockValue::ASNBlockValue(const CFilePosition& pos)
    : ASNValue(pos)
{
}

CNcbiOstream& ASNBlockValue::Print(CNcbiOstream& out, int indent)
{
    out << '{';
    indent++;
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i != values.begin() )
            out << ',';
        NewLine(out, indent);
        (*i)->Print(out, indent);
    }
    NewLine(out, indent - 1);
    return out << '}';
}
