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
#include "value.hpp"
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

CNullDataValue::~CNullDataValue(void)
{
}

void CNullDataValue::PrintASN(CNcbiOstream& out, int ) const
{
    out << "NULL";
}
/*
template<>
CDataValueTmpl<bool>::~CDataValueTmpl(void)
{
}

template<>
CDataValueTmpl<long>::~CDataValueTmpl(void)
{
}

template<>
CDataValueTmpl<string>::~CDataValueTmpl(void)
{
}
*/
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

