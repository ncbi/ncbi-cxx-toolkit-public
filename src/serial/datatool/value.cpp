#include <corelib/ncbidiag.hpp>
#include "value.hpp"
//#include "type.hpp"
#include "module.hpp"

inline
CNcbiOstream& NewLine(CNcbiOstream& out, int indent)
{
    out << NcbiEndl;
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

CDataValue::CDataValue(void)
    : m_Module(0), m_SourceLine(0)
{
}

CDataValue::~CDataValue(void)
{
}

void CDataValue::SetModule(const CDataTypeModule* module)
{
    _ASSERT(module != 0);
    _ASSERT(m_Module == 0);
    m_Module = module;
}

const string& CDataValue::GetSourceFileName(void) const
{
    _ASSERT(m_Module != 0);
    return m_Module->GetSourceFileName();
}

void CDataValue::SetSourceLine(int line)
{
    m_SourceLine = line;
}

string CDataValue::LocationString(void) const
{
    return GetSourceFileName() + ": " + NStr::IntToString(GetSourceLine());
}

void CDataValue::Warning(const string& mess) const
{
    CNcbiDiag() << LocationString() << ": " << mess;
}

bool CDataValue::IsComplex(void) const
{
    return false;
}

void CNullDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << "NULL";
}

template<>
void CDataValueTmpl<bool>::PrintASN(CNcbiOstream& out, int ) const
{
    out << (GetValue()? "TRUE": "FALSE");
}

template<>
void CDataValueTmpl<long>::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}

template<>
void CDataValueTmpl<string>::PrintASN(CNcbiOstream& out, int ) const
{
    out << '"';
    for ( string::const_iterator i = GetValue().begin(), end = GetValue().end();
          i != end; ++i ) {
        char c = *i;
        if ( c == '"' )
            out << "\"\"";
        else
            out << c;
    }
    out << '"';
}

void CBitStringDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}

void CIdDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}

void CNamedDataValue::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetName();
    if ( GetValue().IsComplex() ) {
        indent++;
        NewLine(out, indent);
    }
    else {
        out << ' ';
    }
    GetValue().PrintASN(out, indent);
}

bool CNamedDataValue::IsComplex(void) const
{
    return true;
}

void CBlockDataValue::PrintASN(CNcbiOstream& out, int indent) const
{
    out << '{';
    indent++;
    for ( TValues::const_iterator i = GetValues().begin();
          i != GetValues().end(); ++i ) {
        if ( i != GetValues().begin() )
            out << ',';
        NewLine(out, indent);
        (*i)->PrintASN(out, indent);
    }
    NewLine(out, indent - 1);
    out << '}';
}

bool CBlockDataValue::IsComplex(void) const
{
    return true;
}

