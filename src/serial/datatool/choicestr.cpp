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
* Revision 1.2  2000/02/02 14:57:06  vasilche
* Added missing NCBI_NS_NSBI and NSBI_NS_STD macros to generated code.
*
* Revision 1.1  2000/02/01 21:47:54  vasilche
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

#include <corelib/ncbiutil.hpp>
#include <serial/tool/choicestr.hpp>
#include <serial/tool/code.hpp>
#include <serial/tool/fileutil.hpp>

#define ENUM_NAME "E_choice"
#define STATE_MEMBER "m_choice"
#define STRING_MEMBER "m_string"
#define STRING_TYPE "NCBI_NS_STD::string"
#define NOT_SET_VALUE "e_not_set"

CChoiceTypeStrings::CChoiceTypeStrings(const string& externalName,
                                       const string& className)
    : CParent(externalName, className),
      m_HaveString(false), m_UnionVariants(0)
{
}

CChoiceTypeStrings::~CChoiceTypeStrings(void)
{
}

void CChoiceTypeStrings::AddVariant(const string& name,
                                    AutoPtr<CTypeStrings> type)
{
    m_Variants.push_back(SVariantInfo(name, type));
    if ( m_Variants.back().memberType == eStringMember ) {
        m_HaveString = true;
    }
    else {
        ++m_UnionVariants;
    }
}

CChoiceTypeStrings::SVariantInfo::SVariantInfo(const string& name,
                                               AutoPtr<CTypeStrings> t)
    : externalName(name), cName(Identifier(name)),
      type(t), cType(type->GetCType())
{
    if ( cType == STRING_TYPE ) {
        memberType = eStringMember;
        memberRef = STRING_MEMBER;
    }
    else if ( type->CanBeInSTL() ) {
        memberType = eSimpleMember;
        memberRef = "m_" + cName;
    }
    else {
        memberType = ePointerMember;
        memberRef = "*m_" + cName;
    }
}

void CChoiceTypeStrings::GenerateClassCode(CClassCode& code,
                                           CNcbiOstream& getters,
                                           const string& methodPrefix,
                                           const string& codeClassName) const
{
    // generate variants code
    {
        iterate ( TVariants, i, m_Variants ) {
        if ( i->memberType != ePointerMember )
            i->type->GeneratePointerTypeCode(code);
        else
            i->type->GenerateTypeCode(code);
        }
    }

    // generated choice enum
    {
        code.ClassPublic() <<
            "    // choice state enum\n"
            "    enum "<<ENUM_NAME<<" {\n"
            "        "<<NOT_SET_VALUE;
        iterate ( TVariants, i, m_Variants ) {
            code.ClassPublic() << ",\n"
                "        e" << i->cName;
        }
        code.ClassPublic() << "\n"
            "    };\n"
            "\n";
    }

    // generate choice methods
    getters <<
        "    // choice state\n"
        "    "<<ENUM_NAME<<" Selected(void) const\n"
        "        {\n"
        "            return "<<STATE_MEMBER<<";\n"
        "        }\n"
        "    // throw exception if current variant is not as requested\n"
        "    void CheckSelected("<<ENUM_NAME<<" index) const\n"
        "        {\n"
        "            if ( "<<STATE_MEMBER<<" != index )\n"
        "                InvalidSelection(index);\n"
        "        }\n"
        "    // reset selection\n"
        "    void Reset(void);\n"
        "    // select requested variant if needed\n"
        "    void Select("<<ENUM_NAME<<" index);\n"
        "    // throw exception about wrong selection\n"
        "    void InvalidSelection("<<ENUM_NAME<<" index) const;\n"
        "    // return selection name (for diagnostic purposes)\n"
        "    static NCBI_NS_STD::string SelectionName("<<ENUM_NAME<<" index);\n"
        "\n";

    // generate choice state
    code.ClassPrivate() <<
        "    // choice state\n"
        "    "<<ENUM_NAME<<' '<<STATE_MEMBER<<";\n"
        "    // helper methods\n"
        "    static void* x_Create(void);\n"
        "    static int x_Selected(const void*);\n"
        "    static void x_Select(void*, int);\n"
        "\n";

    // generate initialization code
    code.AddInitializer(STATE_MEMBER, NOT_SET_VALUE);

    // generate destruction code
    code.AddDestructionCode("Reset();");

    // generate Reset method
    {
        code.Methods() <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n"
            "    switch ( "<<STATE_MEMBER<<" ) {\n";
        // generate destruction code for pointers
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == ePointerMember ) {
                code.Methods() <<
                    "    case e"<<i->cName<<":\n";
                WriteTabbed(code.Methods(), 
                            i->type->GetDestructionCode(i->memberRef),
                            "        ");
                code.Methods() <<
                    "        delete m_"<<i->cName<<";\n"
                    "        break;\n";
            }
            
        }
        if ( m_HaveString ) {
            // generate destruction code for string
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eStringMember ) {
                    code.Methods() <<
                        "    case e"<<i->cName<<":\n";
                }
            }
            code.Methods() <<
                "        "<<STRING_MEMBER<<".erase();\n"
                "        break;\n";
        }
        code.Methods() <<
            "    default:\n"
            "        break;\n"
            "    }\n"
            "    "<<STATE_MEMBER<<" = "<<NOT_SET_VALUE<<";\n"
            "}\n"
            "\n";
    }

    // generate Select method
    {
        code.Methods() <<
            "void "<<methodPrefix<<"Select("<<ENUM_NAME<<" index)\n"
            "{\n"
            "    if ( "<<STATE_MEMBER<<" == index )\n"
            "        return;\n"
            "    Reset();\n"
            "    switch ( index ) {\n";
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == eSimpleMember ) {
                string init = i->type->GetInitializer();
                code.Methods() <<
                    "    case e"<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = "<<init<<";\n"
                    "        break;\n";
            }
            else if ( i->memberType == ePointerMember ) {
                code.Methods() <<
                    "    case e"<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = new "<<i->cType<<";\n"
                    "        break;\n";
            }
        }
        code.Methods() <<
            "    default:\n"
            "        break;\n"
            "    }\n"
            "    "<<STATE_MEMBER<<" = index;\n"
            "}\n"
            "\n";
    }

    // generate choice variants names
    code.ClassPrivate() <<
        "    static const char* const sm_SelectionNames[];\n";
    {
        code.Methods() <<
            "const char* const "<<methodPrefix<<"sm_SelectionNames[] = {\n"
            "    \"not set\"";
        iterate ( TVariants, i, m_Variants ) {
            code.Methods() << ",\n"
                "    \""<<i->externalName<<"\"";
        }
        code.Methods() << "\n"
            "};\n"
            "\n"
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName("<<ENUM_NAME<<" index)\n"
            "{\n"
            "    if ( index < 0 || index > "<<m_Variants.size()<<" )\n"
            "        return \"?unknown?\";\n"
            "    return sm_SelectionNames[index];\n"
            "}\n"
            "\n"
            "void "<<methodPrefix<<"InvalidSelection("<<ENUM_NAME<<" index) const\n"
            "{\n"
            "    throw NCBI_NS_STD::runtime_error(\""<<GetExternalName()<<": invalid selection \"+SelectionName("<<STATE_MEMBER<<")+\". Expected: \"+SelectionName(index));\n"
            "}\n"
            "\n";
    }
    
    // generate variant types
    {
        code.ClassPublic() <<
            "    // variants types\n";
        iterate ( TVariants, i, m_Variants ) {
            code.ClassPublic() <<
                "    typedef "<<i->cType<<" T"<<i->cName<<";\n";
        }
        code.ClassPublic() << 
            "\n";
    }

    // generate variant getters
    {
        getters <<
            "    // variants\n";
        iterate ( TVariants, i, m_Variants ) {
            getters <<
                "    bool Is"<<i->cName<<"(void) const\n"
                "        {\n"
                "            return "<<STATE_MEMBER<<" == e"<<i->cName<<";\n"
                "        }\n"
                "    const T"<<i->cName<<"& Get"<<i->cName<<"(void) const\n"
                "        {\n"
                "            CheckSelected(e"<<i->cName<<");\n"
                "            return "<<i->memberRef<<";\n"
                "        }\n"
                "    T"<<i->cName<<"& Get"<<i->cName<<"(void)\n"
                "        {\n"
                "            CheckSelected(e"<<i->cName<<");\n"
                "            return "<<i->memberRef<<";\n"
                "        }\n"
                "    T"<<i->cName<<"& Set"<<i->cName<<"(void)\n"
                "        {\n"
                "            Select(e"<<i->cName<<");\n"
                "            return "<<i->memberRef<<";\n"
                "        }\n";
            if ( i->memberType != ePointerMember ) {
                getters <<
                    "    void Set"<<i->cName<<"(const T"<<i->cName<<"& value)\n"
                    "        {\n"
                    "            Select(e"<<i->cName<<");\n"
                    "            "<<i->memberRef<<" = value;\n"
                    "        }\n";
            }
            getters <<
                "\n";
        }
    }

    // generate variants data
    {
        code.ClassPrivate() <<
            "    // variants data\n";
        if ( m_UnionVariants != 0 ) {
            code.ClassPrivate() << "    union {\n";
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType != eStringMember ) {
                    code.ClassPrivate() <<
                        "        T"<<i->cName<<' '<<i->memberRef<<";\n";
                }
            }
            code.ClassPrivate() <<
                "    };\n";
        }
        if ( m_HaveString ) {
            code.ClassPrivate() <<
                "    "<<STRING_TYPE<<' '<<STRING_MEMBER<<";\n";
        }
    }

    // generate type info
    code.Methods() <<
        "// helper methods\n"
        "void* "<<methodPrefix<<"x_Create(void)\n"
        "{\n"
        "    return new "<<GetClassName()<<";\n"
        "}\n"
        "\n"
        "int "<<methodPrefix<<"x_Selected(const void* object)\n"
        "{\n"
        "    return static_cast<const "<<codeClassName<<"*>(static_cast<const "<<GetClassName()<<"*>(object))->Selected()-1;\n"
        "}\n"
        "void "<<methodPrefix<<"x_Select(void* object, int index)\n"
        "{\n"
        "    static_cast<"<<codeClassName<<"*>(static_cast<"<<GetClassName()<<"*>(object))->Select("<<ENUM_NAME<<"(index+1));\n"
        "}\n"
        "\n"
        "// type info\n"
        "const NCBI_NS_NCBI::CTypeInfo* "<<methodPrefix<<"GetTypeInfo(void)\n"
        "{\n"
        "    static NCBI_NS_NCBI::CGeneratedChoiceTypeInfo* info = 0;\n"
        "    if ( !info ) {\n"
        "        typedef "<<codeClassName<<" CClass_Base;\n"
        "        typedef "<<GetClassName()<<" CClass;\n"
        "        info = new NCBI_NS_NCBI::CGeneratedChoiceTypeInfo(\""<<GetExternalName()<<"\", sizeof(CClass), &x_Create, &x_Selected, &x_Select);\n";
    {
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == ePointerMember ) {
                code.Methods() <<
                    "        NCBI_NS_NCBI::AddMember(info->GetMembers(), \""<<i->externalName<<"\", NCBI_NS_NCBI::Check< "<<i->cType<<"* >::Ptr(MEMBER_PTR(m_"<<i->cName<<")), "<<i->type->GetRef()<<")->SetPointer();\n";
            }
            else {
                code.Methods() <<
                    "        "<<i->type->GetTypeInfoCode(i->externalName,
                                                         i->memberRef)<<";\n";
            }
        }
    }
    code.Methods() <<
        "    }\n"
        "    return info;\n"
        "}\n"
        "\n";
}

CChoiceRefTypeStrings::CChoiceRefTypeStrings(const string& className,
                                             const string& namespaceName,
                                             const string& fileName)
    : CParent(className, namespaceName, fileName)
{
}
