/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Eugene Vasilchenko
*
* File Description:
*   Value definition (used in DEFAULT clause)
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_DTValue

BEGIN_NCBI_SCOPE

CDataValue::CDataValue(void)
    : m_Module(0), m_SourceLine(0)
{
}

CDataValue::~CDataValue(void)
{
}

void CDataValue::SetModule(const CDataTypeModule* module) const
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

void CDataValue::Warning(const string& mess, int err_subcode) const
{
    CNcbiDiag() << ErrCode(NCBI_ERRCODE_X, err_subcode)
                << LocationString() << ": " << mess;
}

bool CDataValue::IsComplex(void) const
{
    return false;
}

CNullDataValue::~CNullDataValue(void)
{
}

void CNullDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << "NULL";
}

string CNullDataValue::GetXmlString(void) const
{
    return kEmptyStr;
}

EMPTY_TEMPLATE
void CDataValueTmpl<bool>::PrintASN(CNcbiOstream& out, int ) const
{
    out << (GetValue()? "TRUE": "FALSE");
}

EMPTY_TEMPLATE
string CDataValueTmpl<bool>::GetXmlString(void) const
{
    return (GetValue()? "true": "false");
}


EMPTY_TEMPLATE
void CDataValueTmpl<Int4>::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}
EMPTY_TEMPLATE
string CDataValueTmpl<Int4>::GetXmlString(void) const
{
    CNcbiOstrstream buffer;
    PrintASN( buffer, 0);
    return CNcbiOstrstreamToString(buffer);
}


EMPTY_TEMPLATE
void CDataValueTmpl<double>::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}
EMPTY_TEMPLATE
string CDataValueTmpl<double>::GetXmlString(void) const
{
    CNcbiOstrstream buffer;
    PrintASN( buffer, 0);
    return CNcbiOstrstreamToString(buffer);
}

EMPTY_TEMPLATE
void CDataValueTmpl<string>::PrintASN(CNcbiOstream& out, int ) const
{
    out << '"';
    ITERATE ( string, i, GetValue() ) {
        char c = *i;
        if ( c == '"' )
            out << "\"\"";
        else
            out << c;
    }
    out << '"';
}

EMPTY_TEMPLATE
string CDataValueTmpl<string>::GetXmlString(void) const
{
    CNcbiOstrstream buffer;
//    PrintASN( buffer, 0);
    ITERATE ( string, i, GetValue() ) {
        char c = *i;
        if ( c == '"' )
            buffer << "\"\"";
        else
            buffer << c;
    }
    return CNcbiOstrstreamToString(buffer);
}

CBitStringDataValue::~CBitStringDataValue(void)
{
}

void CBitStringDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}
string CBitStringDataValue::GetXmlString(void) const
{
    CNcbiOstrstream buffer;
    PrintASN( buffer, 0);
    return CNcbiOstrstreamToString(buffer);
}

CIdDataValue::~CIdDataValue(void)
{
}

void CIdDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}
string CIdDataValue::GetXmlString(void) const
{
    CNcbiOstrstream buffer;
    PrintASN( buffer, 0);
    return CNcbiOstrstreamToString(buffer);
}

CNamedDataValue::~CNamedDataValue(void)
{
}

void CNamedDataValue::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetName();
    if ( GetValue().IsComplex() ) {
        indent++;
        PrintASNNewLine(out, indent);
    }
    else {
        out << ' ';
    }
    GetValue().PrintASN(out, indent);
}
string CNamedDataValue::GetXmlString(void) const
{
    return "not implemented";
}


bool CNamedDataValue::IsComplex(void) const
{
    return true;
}

CBlockDataValue::~CBlockDataValue(void)
{
}

void CBlockDataValue::PrintASN(CNcbiOstream& out, int indent) const
{
    out << '{';
    indent++;
    for ( TValues::const_iterator i = GetValues().begin();
          i != GetValues().end(); ++i ) {
        if ( i != GetValues().begin() )
            out << ',';
        PrintASNNewLine(out, indent);
        (*i)->PrintASN(out, indent);
    }
    PrintASNNewLine(out, indent - 1) << '}';
}
string CBlockDataValue::GetXmlString(void) const
{
    return "not implemented";
}

bool CBlockDataValue::IsComplex(void) const
{
    return true;
}

END_NCBI_SCOPE
