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
#include <serial/datatool/enumstr.hpp>
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/datatool/enumtype.hpp>
#include <serial/datatool/code.hpp>
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
        // generated enum
        CNcbiOstrstream hpp;
        hpp <<
            "enum "<<m_EnumName<<" {";
        ITERATE ( TValues, i, m_Values ) {
            if ( i != m_Values.begin() )
                hpp << ',';
            string id = Identifier(i->GetName(), false);
            hpp << "\n"
                "    "<<m_ValuesPrefix<<id<<" = "<<i->GetValue();
        }
        hpp << "\n"
            "};\n"
            "\n";
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
        if ( !GetModuleName().empty() ) {
            cpp <<
                "    SET_ENUM_MODULE(\""<<GetModuleName()<<"\");\n";
        }
        ITERATE ( TValues, i, m_Values ) {
            string id = Identifier(i->GetName(), false);
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

/*
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2005/03/16 14:36:42  gouriano
* Handle custom data types for enums more accurately
*
* Revision 1.20  2004/05/24 15:10:33  gouriano
* Expose method to access named integers (or enums) in generated classes
*
* Revision 1.19  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.18  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.17  2003/04/10 18:13:29  ucko
* Add export specifiers to (external) DECLARE_ENUM_INFO calls.
* Move CVS log to end.
*
* Revision 1.16  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.15  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.14  2000/11/29 17:42:44  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.13  2000/11/15 20:34:54  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.12  2000/11/07 17:26:25  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.11  2000/08/25 15:59:21  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.10  2000/07/11 20:36:29  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
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
