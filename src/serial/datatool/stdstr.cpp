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
* Revision 1.14  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.13  2003/08/13 15:45:55  gouriano
* implemented generation of code, which uses AnyContent objects
*
* Revision 1.12  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.11  2002/01/15 21:38:27  grichenk
* Fixed NULL type initialization/reading/writing
*
* Revision 1.10  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.9  2000/11/29 17:42:45  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.8  2000/08/25 15:59:24  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.7  2000/07/11 20:36:29  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.6  2000/06/16 16:31:40  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.5  2000/04/17 19:11:09  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.4  2000/04/12 15:36:52  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.3  2000/04/07 19:26:34  vasilche
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
* Revision 1.1  2000/02/01 21:48:06  vasilche
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

#include <ncbi_pch.hpp>
#include <serial/datatool/stdstr.hpp>
#include <serial/datatool/classctx.hpp>

BEGIN_NCBI_SCOPE

CStdTypeStrings::CStdTypeStrings(const string& type)
    : m_CType(type)
{
    SIZE_TYPE colon = type.rfind("::");
    if ( colon != NPOS ) {
        m_CType = type.substr(colon + 2);
        m_Namespace = type.substr(0, colon);
    }
}

CTypeStrings::EKind CStdTypeStrings::GetKind(void) const
{
    return eKindStd;
}

string CStdTypeStrings::GetCType(const CNamespace& ns) const
{
    if ( m_Namespace )
        return ns.GetNamespaceRef(m_Namespace)+m_CType;
    else
        return m_CType;
}

string CStdTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                         const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CStdTypeStrings::GetRef(const CNamespace& ns) const
{
    return "STD, ("+GetCType(ns)+')';
}

string CStdTypeStrings::GetInitializer(void) const
{
    return "0";
}

CTypeStrings::EKind CNullTypeStrings::GetKind(void) const
{
    return eKindStd;
}

bool CNullTypeStrings::HaveSpecialRef(void) const
{
    return true;
}

string CNullTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return "bool";
}

string CNullTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                          const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CNullTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "null, ()";
}

string CNullTypeStrings::GetInitializer(void) const
{
    return "true";
}

CStringTypeStrings::CStringTypeStrings(const string& type)
    : CParent(type)
{
}

CTypeStrings::EKind CStringTypeStrings::GetKind(void) const
{
    return eKindString;
}

string CStringTypeStrings::GetInitializer(void) const
{
    return string();
}

string CStringTypeStrings::GetResetCode(const string& var) const
{
    return var+".erase();\n";
}

void CStringTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert("<string>");
}

CStringStoreTypeStrings::CStringStoreTypeStrings(const string& type)
    : CParent(type)
{
}

bool CStringStoreTypeStrings::HaveSpecialRef(void) const
{
    return true;
}

string CStringStoreTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "StringStore, ()";
}

CAnyContentTypeStrings::CAnyContentTypeStrings(const string& type)
    : CParent(type)
{
}

CTypeStrings::EKind CAnyContentTypeStrings::GetKind(void) const
{
    return eKindOther;
}

string CAnyContentTypeStrings::GetInitializer(void) const
{
    return string();
}

string CAnyContentTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

void CAnyContentTypeStrings::GenerateTypeCode(CClassContext& /*ctx*/) const
{
}

END_NCBI_SCOPE
