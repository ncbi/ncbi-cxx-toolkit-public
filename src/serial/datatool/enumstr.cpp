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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2000/07/10 17:59:34  vasilche
* Moved macros needed in headers to serialbase.hpp.
* Use DECLARE_ENUM_INFO in generated code.
*
* Revision 1.8  2000/07/10 17:31:59  vasilche
* Macro arguments made more clear.
* All old ASN stuff moved to serialasn.hpp.
* Changed prefix of enum info functions to GetTypeInfo_enum_.
*
* Revision 1.7  2000/06/16 16:31:38  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.6  2000/05/24 20:09:28  vasilche
* Implemented DTD generation.
*
* Revision 1.5  2000/04/17 19:11:08  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.4  2000/04/12 15:36:51  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.3  2000/04/07 19:26:25  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.2  2000/02/17 20:05:07  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.1  2000/02/01 21:47:57  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.8  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.7  1999/12/28 18:56:00  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.6  1999/12/17 19:05:19  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.5  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:20  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/enumstr.hpp>
#include <serial/tool/classctx.hpp>
#include <serial/tool/fileutil.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CEnumTypeStrings::CEnumTypeStrings(const string& externalName,
                                   const string& enumName,
                                   const string& cType, bool isInteger,
                                   const TValues& values,
                                   const string& valuePrefix)
    : m_ExternalName(externalName), m_EnumName(enumName),
      m_CType(cType), m_IsInteger(isInteger),
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

string CEnumTypeStrings::GetRef(void) const
{
    return "ENUM, ("+m_CType+", "+m_EnumName+')';
}

string CEnumTypeStrings::GetInitializer(void) const
{
    return m_EnumName+"(0)";
}

void CEnumTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    string methodPrefix = ctx.GetMethodPrefix();
    bool inClass = !methodPrefix.empty();
    {
        // generated enum
        CNcbiOstrstream hpp;
        hpp <<
            "enum "<<m_EnumName<<" {";
        iterate ( TValues, i, m_Values ) {
            if ( i != m_Values.begin() )
                hpp << ',';
            string id = Identifier(i->first, false);
            hpp << "\n"
                "    "<<m_ValuesPrefix<<id<<" = "<<i->second;
        }
        hpp << "\n"
            "};\n"
            "\n";
        // prototype of GetTypeInfo_enum_* function
        if ( inClass )
            hpp << "DECLARE_INTERNAL_ENUM_INFO";
        else
            hpp << "DECLARE_ENUM_INFO";
        hpp << '('<<m_EnumName<<");\n"
            "\n";
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
        iterate ( TValues, i, m_Values ) {
            string id = Identifier(i->first, false);
            cpp <<
                "    ADD_ENUM_VALUE(\""<<i->first<<"\", "<<m_ValuesPrefix<<id<<");\n";
        }
        cpp <<
            "}\n"
            "END_ENUM_INFO\n"
            "\n";
        ctx.AddCPPCode(cpp);
    }
}

string CEnumTypeStrings::GetTypeInfoCode(const string& externalName,
                                         const string& memberName) const
{
    return "info->GetMembers().AddMember("
        "\""+externalName+"\", "
        "NCBI_NS_NCBI::EnumMember(MEMBER_PTR("+memberName+"), "
        "GetEnumInfo_"+m_EnumName+"()))";
}

CEnumRefTypeStrings::CEnumRefTypeStrings(const string& enumName,
                                         const string& cType,
                                         const CNamespace& ns,
                                         const string& fileName)
    : m_EnumName(enumName),
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

string CEnumRefTypeStrings::GetRef(void) const
{
    if ( !m_CType.empty() && m_CType != m_EnumName || m_Namespace.IsEmpty() )
        return "ENUM, ("+m_CType+", "+m_EnumName+')';
    else
        return "ENUM_IN, ("+m_CType+", "+m_Namespace.ToString()+", "+m_EnumName+')';
    //return "NCBI_NS_NCBI::CreateEnumeratedTypeInfo("+GetCType(CNamespace::KEmptyNamespace)+"(0), "+m_Namespace.ToString()+"GetEnumInfo_"+m_EnumName+"())";
}

string CEnumRefTypeStrings::GetInitializer(void) const
{
    return GetCType(CNamespace::KEmptyNamespace) + "(0)";
}

void CEnumRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

string CEnumRefTypeStrings::GetTypeInfoCode(const string& externalName,
                                            const string& memberName) const
{
    return "info->GetMembers().AddMember("
        "\""+externalName+"\", "
        "NCBI_NS_NCBI::EnumMember(MEMBER_PTR("+memberName+"), "
        +m_Namespace.ToString()+"GetEnumInfo_"+m_EnumName+"()))";
}

END_NCBI_SCOPE
