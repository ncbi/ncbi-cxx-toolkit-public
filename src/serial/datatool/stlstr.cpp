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
* Revision 1.16  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.15  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.14  2003/03/10 18:55:19  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.13  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.12  2000/11/29 17:42:45  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.11  2000/08/25 15:59:24  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.10  2000/07/11 20:36:29  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.9  2000/06/16 16:31:41  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.8  2000/04/17 19:11:09  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.7  2000/04/12 15:36:52  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.6  2000/04/07 19:26:34  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.5  2000/03/07 14:06:33  vasilche
* Added generation of reference counted objects.
*
* Revision 1.4  2000/02/03 20:16:15  vasilche
* Fixed bug in type info generation for templates.
*
* Revision 1.3  2000/02/02 19:08:20  vasilche
* Fixed variable conflict in generated files on MSVC.
*
* Revision 1.2  2000/02/02 14:57:06  vasilche
* Added missing NCBI_NS_NSBI and NSBI_NS_STD macros to generated code.
*
* Revision 1.1  2000/02/01 21:48:07  vasilche
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
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/stlstr.hpp>
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/namespace.hpp>
#include <serial/datatool/srcutil.hpp>

BEGIN_NCBI_SCOPE

CTemplate1TypeStrings::CTemplate1TypeStrings(const string& templateName,
                                             CTypeStrings* arg1Type)
    : m_TemplateName(templateName), m_Arg1Type(arg1Type)
{
}

CTemplate1TypeStrings::CTemplate1TypeStrings(const string& templateName,
                                             AutoPtr<CTypeStrings> arg1Type)
    : m_TemplateName(templateName), m_Arg1Type(arg1Type)
{
}

CTemplate1TypeStrings::~CTemplate1TypeStrings(void)
{
}

CTypeStrings::EKind CTemplate1TypeStrings::GetKind(void) const
{
    return eKindContainer;
}

string CTemplate1TypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(GetTemplateNamespace())+GetTemplateName()+"< "+GetArg1Type()->GetCType(ns)+" >";
}

string CTemplate1TypeStrings::GetPrefixedCType(const CNamespace& ns,
                                               const string& methodPrefix) const
{
    return ns.GetNamespaceRef(GetTemplateNamespace())+GetTemplateName()+"< "
        + GetArg1Type()->GetPrefixedCType(ns,methodPrefix)+" >";
}

string CTemplate1TypeStrings::GetRef(const CNamespace& ns) const
{
    return "STL_"+GetRefTemplate()+", ("+GetArg1Type()->GetRef(ns)+')';
}

string CTemplate1TypeStrings::GetRefTemplate(void) const
{
    return GetTemplateName();
}

string CTemplate1TypeStrings::GetIsSetCode(const string& var) const
{
    return "!("+var+").empty()";
}

void CTemplate1TypeStrings::AddTemplateInclude(CClassContext::TIncludes& hpp) const
{
    string header = GetTemplateName();
    if ( header == "multiset" )
        header = "<set>";
    else if ( header == "multimap" )
        header = "<map>";
    else if ( header == "AutoPtr" )
        header = "<corelib/ncbiutil.hpp>";
    else
        header = '<'+header+'>';
    hpp.insert(header);
}

const CNamespace& CTemplate1TypeStrings::GetTemplateNamespace(void) const
{
    if ( GetTemplateName() == "AutoPtr" )
        return CNamespace::KNCBINamespace;
    return CNamespace::KSTDNamespace;
}

void CTemplate1TypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    AddTemplateInclude(ctx.HPPIncludes());
    GetArg1Type()->GenerateTypeCode(ctx);
}

CTemplate2TypeStrings::CTemplate2TypeStrings(const string& templateName,
                                             AutoPtr<CTypeStrings> arg1Type,
                                             AutoPtr<CTypeStrings> arg2Type)
    : CParent(templateName, arg1Type), m_Arg2Type(arg2Type)
{
}

CTemplate2TypeStrings::~CTemplate2TypeStrings(void)
{
}

string CTemplate2TypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(GetTemplateNamespace())+GetTemplateName()+"< "+GetArg1Type()->GetCType(ns)+", "+GetArg2Type()->GetCType(ns)+" >";
}

string CTemplate2TypeStrings::GetPrefixedCType(const CNamespace& ns,
                                               const string& methodPrefix) const
{
    return ns.GetNamespaceRef(GetTemplateNamespace())+GetTemplateName()+"< "
        + GetArg1Type()->GetPrefixedCType(ns,methodPrefix)+", "
        + GetArg2Type()->GetPrefixedCType(ns,methodPrefix)+" >";
}

string CTemplate2TypeStrings::GetRef(const CNamespace& ns) const
{
    return "STL_"+GetRefTemplate()+", ("+GetArg1Type()->GetRef(ns)+", "+GetArg2Type()->GetRef(ns)+')';
}

void CTemplate2TypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    CParent::GenerateTypeCode(ctx);
    GetArg2Type()->GenerateTypeCode(ctx);
}

CSetTypeStrings::CSetTypeStrings(const string& templateName,
                                 AutoPtr<CTypeStrings> type)
    : CParent(templateName, type)
{
}

CSetTypeStrings::~CSetTypeStrings(void)
{
}

string CSetTypeStrings::GetDestructionCode(const string& expr) const
{
    string code;
    string iter;
    static int level = 0;
    try {
        level++;
        iter = "setIter"+NStr::IntToString(level);
        code = Tabbed(GetArg1Type()->GetDestructionCode('*'+iter), "        ");
    }
    catch (CDatatoolException& exp) {
        level--;
        NCBI_RETHROW_SAME(exp,"CSetTypeStrings::GetDestructionCode: failed");
    }
    catch (...) {
        level--;
        throw;
    }
    level--;
    if ( code.empty() )
        return string();
    return
        "{\n"
        "    for ( "+GetCType(CNamespace::KEmptyNamespace)+"::iterator "+iter+" = ("+expr+").begin(); "+iter+" != ("+expr+").end(); ++"+iter+" ) {\n"
        +code+
        "    }\n"
        "}\n";
}

string CSetTypeStrings::GetResetCode(const string& var) const
{
    return var+".clear();\n";
}

CListTypeStrings::CListTypeStrings(const string& templateName,
                                   AutoPtr<CTypeStrings> type,
                                   bool externalSet)
    : CParent(templateName, type), m_ExternalSet(externalSet)
{
}

CListTypeStrings::~CListTypeStrings(void)
{
}

string CListTypeStrings::GetRefTemplate(void) const
{
    string templ = GetTemplateName();
    if ( m_ExternalSet )
        templ += "_set";
    return templ;
}

string CListTypeStrings::GetDestructionCode(const string& expr) const
{
    string code;
    string iter;
    static int level = 0;
    try {
        level++;
        iter = "listIter"+NStr::IntToString(level);
        code = Tabbed(GetArg1Type()->GetDestructionCode('*'+iter), "        ");
    }
    catch (CDatatoolException& exp) {
        level--;
        NCBI_RETHROW_SAME(exp,"CListTypeStrings::GetDestructionCode: failed");
    }
    catch (...) {
        level--;
        throw;
    }
    level--;
    if ( code.empty() )
        return string();
    return
        "{\n"
        "    for ( "+GetCType(CNamespace::KEmptyNamespace)+"::iterator "+iter+" = ("+expr+").begin(); "+iter+" != ("+expr+").end(); ++"+iter+" ) {\n"
        +code+
        "    }\n"
        "}\n";
}

string CListTypeStrings::GetResetCode(const string& var) const
{
    return var+".clear();\n";
}

CMapTypeStrings::CMapTypeStrings(const string& templateName,
                                 AutoPtr<CTypeStrings> keyType,
                                 AutoPtr<CTypeStrings> valueType)
    : CParent(templateName, keyType, valueType)
{
}

CMapTypeStrings::~CMapTypeStrings(void)
{
}

string CMapTypeStrings::GetDestructionCode(const string& expr) const
{
    string code;
    string iter;
    static int level = 0;
    try {
        level++;
        iter = "mapIter"+NStr::IntToString(level);
        code = Tabbed(GetArg1Type()->GetDestructionCode(iter+"->first")+
                      GetArg2Type()->GetDestructionCode(iter+"->second"),
                      "        ");
    }
    catch (CDatatoolException& exp) {
        level--;
        NCBI_RETHROW_SAME(exp,"CMapTypeStrings::GetDestructionCode: failed");
    }
    catch (...) {
        level--;
        throw;
    }
    level--;
    if ( code.empty() )
        return string();
    return
        "{\n"
        "    for ( "+GetCType(CNamespace::KEmptyNamespace)+"::iterator "+iter+" = ("+expr+").begin(); "+iter+" != ("+expr+").end(); ++"+iter+" ) {\n"
        +code+
        "    }\n"
        "}\n";
}

string CMapTypeStrings::GetResetCode(const string& var) const
{
    return var+".clear();\n";
}

CVectorTypeStrings::CVectorTypeStrings(const string& charType)
    : m_CharType(charType)
{
}

CVectorTypeStrings::~CVectorTypeStrings(void)
{
}

CTypeStrings::EKind CVectorTypeStrings::GetKind(void) const
{
    return eKindOther;
}

void CVectorTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert("<vector>");
}

string CVectorTypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(CNamespace::KSTDNamespace)+"vector< " + m_CharType + " >";
}

string CVectorTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                            const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CVectorTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "STL_CHAR_vector, ("+m_CharType+')';
}

string CVectorTypeStrings::GetResetCode(const string& var) const
{
    return var+".clear();\n";
}

END_NCBI_SCOPE
