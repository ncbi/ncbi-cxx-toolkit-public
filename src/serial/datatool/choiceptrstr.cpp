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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.6  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.5  2002/08/14 17:14:25  grichenk
* Fixed function name conflict on Win32: renamed
* GetClassName() -> GetClassNameDT()
*
* Revision 1.4  2002/07/01 15:42:08  grichenk
* Fixed 'unused variable' warnings, removed commented code.
*
* Revision 1.3  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.2  2000/11/29 17:42:42  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.1  2000/09/26 17:38:25  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/datatool/choiceptrstr.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/namespace.hpp>
#include <serial/datatool/srcutil.hpp>

BEGIN_NCBI_SCOPE

#define STATE_ENUM "E_Choice"
#define STATE_PREFIX "e_"
#define STATE_NOT_SET "e_not_set"
#define REFCHOICE_TYPE_METHOD "GetRefChoiceTypeInfo"

CChoicePtrTypeStrings::CChoicePtrTypeStrings(const string& externalName,
                                             const string& className)
    : CParent(externalName, className)
{
}

void CChoicePtrTypeStrings::AddVariant(const string& name,
                                       AutoPtr<CTypeStrings> type)
{
    m_Variants.push_back(SVariantInfo(name, type));
}

CChoicePtrTypeStrings::SVariantInfo::SVariantInfo(const string& n,
                                                  AutoPtr<CTypeStrings> t)
    : externalName(n), cName(Identifier(n)), type(t)
{
}

void CChoicePtrTypeStrings::GenerateClassCode(CClassCode& code,
                                              CNcbiOstream& /*setters*/,
                                              const string& methodPrefix,
                                              bool haveUserClass,
                                              const string& classPrefix) const
{
    string codeClassName = GetClassNameDT();
    if ( haveUserClass )
        codeClassName += "_Base";
    // generate variants code
    {
        ITERATE ( TVariants, i, m_Variants ) {
            i->type->GeneratePointerTypeCode(code);
        }
    }

    string stdNamespace = 
        code.GetNamespace().GetNamespaceRef(CNamespace::KSTDNamespace);
    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    code.ClassPublic() <<
        "    static const "<<ncbiNamespace<<"CTypeInfo* "REFCHOICE_TYPE_METHOD"(void);\n"
        "\n";

    // generated choice enum
    {
        code.ClassPublic() <<
            "    // choice state enum\n"
            "    enum "STATE_ENUM" {\n"
            "        "STATE_NOT_SET" = "<<kEmptyChoice;
        ITERATE ( TVariants, i, m_Variants ) {
            code.ClassPublic() << ",\n"
                "        "STATE_PREFIX<<i->cName;
        }
        code.ClassPublic() << "\n"
            "    };\n"
            "\n";
    }

    // generate choice methods
    code.ClassPublic() <<
        "    // return selection name (for diagnostic purposes)\n"
        "    static "<<stdNamespace<<"string SelectionName("STATE_ENUM" index);\n"
        "\n";

    // generate choice variants names
    code.ClassPrivate() <<
        "    static const char* const sm_SelectionNames[];\n";
    {
        code.Methods() <<
            "const char* const "<<methodPrefix<<"sm_SelectionNames[] = {\n"
            "    \"not set\"";
        ITERATE ( TVariants, i, m_Variants ) {
            code.Methods() << ",\n"
                "    \""<<i->externalName<<"\"";
        }
        code.Methods() << "\n"
            "};\n"
            "\n"
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName("STATE_ENUM" index)\n"
            "{\n"
            "    return NCBI_NS_NCBI::CInvalidChoiceSelection::GetName(index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n";
    }

    // generate variant types
    {
        code.ClassPublic() <<
            "    // variants' types\n";
        ITERATE ( TVariants, i, m_Variants ) {
            code.ClassPublic() <<
                "    typedef "<<i->type->GetCType(code.GetNamespace())<<" T"<<i->cName<<";\n";
        }
        code.ClassPublic() << 
            "\n";
    }

    // generate type info
    code.Methods() <<
        "// type info\n";
    if ( haveUserClass )
        code.Methods() << "BEGIN_NAMED_ABSTRACT_BASE_CLASS_INFO";
    else
        code.Methods() << "BEGIN_NAMED_ABSTRACT_CLASS_INFO";
    code.Methods() <<
        "(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassNameDT()<<")\n"
        "{\n";
    {
        ITERATE ( TVariants, i, m_Variants ) {
            code.Methods() <<
                "    ADD_NAMED_SUB_CLASS(\""<<i->externalName<<"\", "<<i->type->GetCType(code.GetNamespace())<<");\n";
        }
    }
    code.Methods() <<
        "}\n"
        "END_CLASS_INFO\n"
        "\n";

    // generate ref type info
    code.Methods() <<
        "const NCBI_NS_NCBI::CTypeInfo* "<<methodPrefix<<REFCHOICE_TYPE_METHOD"(void)\n"
        "{\n"
        "    return NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo(NCBI_NS_NCBI::CRefTypeInfo<"<<
        classPrefix<<GetClassNameDT()<<">::GetTypeInfo("<<classPrefix<<GetClassNameDT()<<
        "::GetTypeInfo()));\n"
        "}\n"
        "\n";
}

CChoicePtrRefTypeStrings::CChoicePtrRefTypeStrings(CTypeStrings* type)
    : CParent(type)
{
}

CChoicePtrRefTypeStrings::CChoicePtrRefTypeStrings(AutoPtr<CTypeStrings> type)
    : CParent(type)
{
}

string CChoicePtrRefTypeStrings::GetRef(const CNamespace& ns) const
{
    return "CHOICE, ("+CParent::GetRef(ns)+')';
}

END_NCBI_SCOPE
