#include <asnvalue.hpp>
#include <asntype.hpp>

inline
ostream& NewLine(ostream& out, int indent)
{
    return ASNType::NewLine(out, indent);
}

ASNValue::ASNValue()
    : line(0)
{
}

ASNValue::~ASNValue()
{
}

void ASNValue::Warning(const string& mess) const
{
    if ( line > 0 )
        cerr << line;
    cerr << ": " << mess << endl;
}

ostream& ASNNullValue::Print(ostream& out, int indent)
{
    return out << "NULL";
}

ASNBoolValue::ASNBoolValue(bool v)
    : value(v)
{
}

ostream& ASNBoolValue::Print(ostream& out, int indent)
{
    return out << (value? "TRUE": "FALSE");
}

ASNIntegerValue::ASNIntegerValue(long v)
    : value(v)
{
}

ostream& ASNIntegerValue::Print(ostream& out, int indent)
{
    return out << value;
}

ASNStringValue::ASNStringValue(const string& v)
    : value(v)
{
}

ostream& ASNStringValue::Print(ostream& out, int indent)
{
    return out << '"' << value << '"';
}

ASNBitStringValue::ASNBitStringValue(const string& v)
    : value(v)
{
}

ostream& ASNBitStringValue::Print(ostream& out, int indent)
{
    return out << '\'' << value << "'B";
}

ASNIdValue::ASNIdValue(const string& v)
    : id(v)
{
}

ostream& ASNIdValue::Print(ostream& out, int indent)
{
    return out << id;
}

ASNNamedValue::ASNNamedValue()
{
}

ASNNamedValue::ASNNamedValue(const string& n, const AutoPtr<ASNValue>& v)
    : name(n), value(v)
{
}

ostream& ASNNamedValue::Print(ostream& out, int indent)
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

ostream& ASNBlockValue::Print(ostream& out, int indent)
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
