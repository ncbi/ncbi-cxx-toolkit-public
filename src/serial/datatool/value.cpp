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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/12/15 15:38:51  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.14  2000/11/29 17:42:46  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependancy.
*
* Revision 1.13  2000/11/15 20:34:56  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.12  2000/08/25 15:59:25  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.11  2000/04/07 19:26:38  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.10  2000/02/01 21:48:10  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.9  1999/12/29 16:01:53  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.8  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/srcutil.hpp>

BEGIN_NCBI_SCOPE

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

CNullDataValue::~CNullDataValue(void)
{
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
void CDataValueTmpl<Int4>::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}

template<>
void CDataValueTmpl<string>::PrintASN(CNcbiOstream& out, int ) const
{
    out << '"';
    iterate ( string, i, GetValue() ) {
        char c = *i;
        if ( c == '"' )
            out << "\"\"";
        else
            out << c;
    }
    out << '"';
}

CBitStringDataValue::~CBitStringDataValue(void)
{
}

void CBitStringDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
}

CIdDataValue::~CIdDataValue(void)
{
}

void CIdDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetValue();
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
    PrintASNNewLine(out, indent - 1);
    out << '}';
}

bool CBlockDataValue::IsComplex(void) const
{
    return true;
}

END_NCBI_SCOPE
