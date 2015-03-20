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
*   Type info for class generation: includes, used classes, C code etc.
*/

#include <ncbi_pch.hpp>
#include "enumstr.hpp"
#include "classctx.hpp"
#include "srcutil.hpp"
#include "enumtype.hpp"
#include "code.hpp"
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CEnumTypeStrings::CEnumTypeStrings(const string& externalName,
                                   const string& enumName,
                                   const string& packedType,
                                   const string& cType, bool isInteger,
                                   const TValues& values,
                                   const string& valuePrefix,
                                   const string& namespaceName,
                                   const CDataType* dataType,
                                   const CComments& comments)
    : CParent(namespaceName, dataType, comments),
      m_ExternalName(externalName), m_EnumName(enumName),
      m_PackedType(packedType),
      m_CType(cType), m_IsInteger(isInteger), m_IsBitset(false),
      m_Values(values), m_ValuesPrefix(valuePrefix)
{
}

CTypeStrings::EKind CEnumTypeStrings::GetKind(void) const
{
    return eKindEnum;
}

const string& CEnumTypeStrings::GetEnumName(void) const
{
    return m_EnumName;
}

string CEnumTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return m_CType;
}

string CEnumTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                          const string& methodPrefix) const
{
    string s;
    if (!m_IsInteger) {
        s += methodPrefix;
    }
    return  s + GetCType(ns);
}

string CEnumTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "ENUM, ("+m_CType+", "+m_EnumName+')';
}

string CEnumTypeStrings::GetInitializer(void) const
{
    return "(" + m_EnumName + ")(0)";
}

void CEnumTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    string methodPrefix = ctx.GetMethodPrefix();
    bool inClass = !methodPrefix.empty();
    {
        // alignments
        size_t maxlen = 0, maxwidth=0;
        ITERATE ( TValues, i, m_Values ) {
            maxlen = max(maxlen,i->GetName().size());
            size_t w = 0;
            TEnumValueType val = i->GetValue();
            if (val < 0) {
                ++w; val = -val;
            }
            while (val > 0) {
                ++w; val /= 10;
            }
            maxwidth = max(maxwidth,w);
        }
        // generated enum
        CNcbiOstrstream hpp;
        PrintHPPComments(hpp);
        hpp << "enum " << m_EnumName;
        if (!m_PackedType.empty()) {
            hpp << " NCBI_PACKED_ENUM_TYPE( " << m_PackedType << " )";
        }
        hpp << " {";
        ITERATE ( TValues, i, m_Values ) {
            string id = Identifier( i->GetEnumId(), false );
            hpp << "\n    " << m_ValuesPrefix << id;
            hpp << string( maxlen - i->GetName().size(),' ' ) << " = ";
            hpp.width( maxwidth );
            hpp << i->GetValue();
            TValues::const_iterator next = i;
            if ( ++next != m_Values.end() ) {
                hpp.put(',');
            }
            i->GetComments().PrintHPPEnum(hpp);
        }
        hpp << "\n}";
        if (!m_PackedType.empty()) {
            hpp << " NCBI_PACKED_ENUM_END()";
        }
        hpp << ";\n\n";
        // prototype of GetTypeInfo_enum_* function

#if 0
        if ( inClass )
            hpp << "DECLARE_INTERNAL_ENUM_INFO";
        else
            hpp << CClassCode::GetExportSpecifier() << " DECLARE_ENUM_INFO";
        hpp << '('<<m_EnumName<<");\n\n";
#else
        hpp << "/// Access to " << m_EnumName
            << "'s attributes (values, names) as defined in spec\n";
        if ( inClass ) {
            hpp << "static";
        } else {
            hpp << CClassCode::GetExportSpecifier();
        }
        hpp << " const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME";
        hpp << '('<<m_EnumName<<")(void);\n\n";
#endif
        ctx.AddHPPCode(hpp);
    }
    {
        // definition of GetTypeInfo_enum_ function
        CNcbiOstrstream cpp;
        if ( methodPrefix.empty() ) {
            cpp <<
                "BEGIN_NAMED_ENUM_INFO(\""<<GetExternalName()<<'\"';
        }
        else {
            cpp <<
                "BEGIN_NAMED_ENUM_IN_INFO(\""<<GetExternalName()<<"\", "<<
                methodPrefix;
        }
        cpp <<", "<<m_EnumName<<", "<<(m_IsInteger?"true":"false")<<")\n"
            "{\n";
        SInternalNames names;
        string module_name = GetModuleName(&names);
        if ( GetExternalName().empty() && !names.m_OwnerName.empty() ) {
            cpp <<
                "    SET_ENUM_INTERNAL_NAME(\""<<names.m_OwnerName<<"\", ";
            if ( !names.m_MemberName.empty() )
                cpp << "\""<<names.m_MemberName<<"\"";
            else
                cpp << "0";
            cpp << ");\n";
        }
        if ( !module_name.empty() ) {
            cpp <<
                "    SET_ENUM_MODULE(\""<<module_name<<"\");\n";
        }
        if (IsBitset()) {
            cpp <<
                "    SET_ENUM_BITSET;\n";
        }
        ITERATE ( TValues, i, m_Values ) {
            string id = Identifier(i->GetEnumId(), false);
            cpp <<
                "    ADD_ENUM_VALUE(\""<<i->GetName()<<"\", "<<m_ValuesPrefix<<id<<");\n";
        }
        cpp <<
            "}\n"
            "END_ENUM_INFO\n"
            "\n";
        ctx.AddCPPCode(cpp);
    }
}

CEnumRefTypeStrings::CEnumRefTypeStrings(const string& enumName,
                                         const string& cType,
                                         const CNamespace& ns,
                                         const string& fileName,
                                         const CComments& comments)
    : CParent(comments),
      m_EnumName(enumName),
      m_CType(cType), m_Namespace(ns),
      m_FileName(fileName)
{
}

CTypeStrings::EKind CEnumRefTypeStrings::GetKind(void) const
{
    return eKindEnum;
}

const CNamespace& CEnumRefTypeStrings::GetNamespace(void) const
{
    return m_Namespace;
}

const string& CEnumRefTypeStrings::GetEnumName(void) const
{
    return m_EnumName;
}

string CEnumRefTypeStrings::GetCType(const CNamespace& ns) const
{
    if ( !m_CType.empty() && m_CType != m_EnumName )
        return m_CType;

    return ns.GetNamespaceRef(m_Namespace)+m_EnumName;
}

string CEnumRefTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                             const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CEnumRefTypeStrings::GetRef(const CNamespace& ns) const
{
    string ref = "ENUM";
    bool haveNamespace = !m_Namespace.IsEmpty() && m_Namespace != ns;
    if ( haveNamespace )
        ref += "_IN";
    ref += ", (";
    if ( m_CType.empty() )
        ref += m_EnumName;
    else
        ref += m_CType;
    if ( haveNamespace ) {
        ref += ", ";
        ref += m_Namespace.ToString();
    }
    return ref+", "+m_EnumName+')';
}

string CEnumRefTypeStrings::GetInitializer(void) const
{
    return "(" + GetCType(CNamespace::KEmptyNamespace) + ")(0)";
}

void CEnumRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

END_NCBI_SCOPE
