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
* Revision 1.8  2000/03/07 20:04:59  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.7  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.6  2000/02/17 20:05:06  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.5  2000/02/11 17:09:30  vasilche
* Removed unneeded flags.
*
* Revision 1.4  2000/02/03 21:58:30  vasilche
* Fixed tipo leading to memory leak in generated files.
*
* Revision 1.3  2000/02/03 20:16:15  vasilche
* Fixed bug in type info generation for templates.
*
* Revision 1.2  2000/02/02 14:57:06  vasilche
* Added missing NCBI_NS_NCBI and NCBI_NS_STD macros to generated code.
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

#define STATE_ENUM "E_Choice"
#define STATE_MEMBER "m_choice"
#define STRING_TYPE "NCBI_NS_STD::string"
#define STRING_MEMBER "m_string"
#define STATE_PREFIX "e_"
#define STATE_NOT_SET "e_not_set"

CChoiceTypeStrings::CChoiceTypeStrings(const string& externalName,
                                       const string& className)
    : CParent(externalName, className),
      m_HaveAssignment(false)
{
}

CChoiceTypeStrings::~CChoiceTypeStrings(void)
{
}

void CChoiceTypeStrings::AddVariant(const string& name,
                                    AutoPtr<CTypeStrings> type)
{
    m_Variants.push_back(SVariantInfo(name, type));
    if ( m_Variants.back().memberType == ePointerMember ) {
        CTypeStrings* type = m_Variants.back().type.get();
        if ( !type->IsObject() ) {
            CClassTypeStrings* classType =
                dynamic_cast<CClassTypeStrings*>(type);
            if ( classType ) {
                classType->SetObject(true);
            }
            else {
/*
                ERR_POST(Fatal<<"variant "<<name<<" in CHOICE "<<GetClassName()<<
                         " have non object/string pointer");
                m_HaveAssignment = false;
*/
            }
        }
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
    else if ( type->CanBeKey() ) {
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
    bool havePointers = false;
    bool haveSimple = false;
    bool haveString = false;
    // generate variants code
    {
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == ePointerMember ) {
                i->type->GeneratePointerTypeCode(code);
                havePointers = true;
            }
            else {
                i->type->GenerateTypeCode(code);
                if ( i->memberType == eStringMember )
                    haveString = true;
                else
                    haveSimple = true;
            }
        }
    }
    bool haveUnion = havePointers || haveSimple;
    if ( haveUnion ) {
        // convert string member to pointer member
        haveString = false;
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == eStringMember ) {
                const_cast<SVariantInfo&>(*i).memberType = ePointerMember;
                const_cast<SVariantInfo&>(*i).memberRef = "*m_" + i->cName;
                havePointers = true;
            }
        }
    }

    if ( HaveAssignment() ) {
        code.ClassPublic() <<
            "    // copy constructor and assignment operator\n"
            "    "<<codeClassName<<"(const "<<codeClassName<<"& src);\n"
            "    "<<codeClassName<<"& operator=(const "<<codeClassName<<"& src);\n"
            "\n";
    }

    // generated choice enum
    {
        code.ClassPublic() <<
            "    // choice state enum\n"
            "    enum "<<STATE_ENUM<<" {\n"
            "        "<<STATE_NOT_SET;
        iterate ( TVariants, i, m_Variants ) {
            code.ClassPublic() << ",\n"
                "        "<<STATE_PREFIX<<i->cName;
        }
        code.ClassPublic() << "\n"
            "    };\n"
            "\n";
    }

    code.ClassPublic() <<
        "    // reset selection to none\n"
        "    ";
    if ( HaveUserClass() )
        code.ClassPublic() << "virtual ";
    code.ClassPublic() <<
        "void Reset(void);\n"
        "\n";

    // generate choice methods
    getters <<
        "    // choice state\n"
        "    "<<STATE_ENUM<<" Which(void) const;\n"
        "    // throw exception if current variant is not as requested\n"
        "    void CheckSelected("<<STATE_ENUM<<" index) const;\n"
        "    // select requested variant if needed\n"
        "    void Select("<<STATE_ENUM<<" index, NCBI_NS_NCBI::EResetVariant reset = NCBI_NS_NCBI::eDoResetVariant);\n"
        "    // throw exception about wrong selection\n"
        "    void InvalidSelection("<<STATE_ENUM<<" index) const;\n"
        "    // return selection name (for diagnostic purposes)\n"
        "    static NCBI_NS_STD::string SelectionName("<<STATE_ENUM<<" index);\n"
        "\n";
    code.InlineMethods() <<
        "inline\n"<<
        methodPrefix<<STATE_ENUM<<" "<<methodPrefix<<"Which(void) const\n"
        "{\n"
        "    return "<<STATE_MEMBER<<";\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"CheckSelected("<<STATE_ENUM<<" index) const\n"
        "{\n"
        "    if ( "<<STATE_MEMBER<<" != index )\n"
        "        InvalidSelection(index);\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"Select("<<STATE_ENUM<<" index, NCBI_NS_NCBI::EResetVariant reset)\n"
        "{\n"
        "    if ( reset == NCBI_NS_NCBI::eDoResetVariant || "<<STATE_MEMBER<<" != index ) {\n"
        "        Reset();\n"
        "        DoSelect(index);\n"
        "    }\n"
        "}\n"
        "\n";

    if ( HaveAssignment() ) {
        code.InlineMethods() <<
            "inline\n"<<
            methodPrefix<<codeClassName<<"(const "<<codeClassName<<"& src)\n"
            "{\n"
            "    DoAssign(src);\n"
            "}\n"
            "\n"
            "inline\n"<<
            methodPrefix<<codeClassName<<"& "<<methodPrefix<<"operator=(const "<<codeClassName<<"& src)\n"
            "{\n"
            "    if ( this != &src ) {\n"
            "        Reset();\n"
            "        DoAssign(src);\n"
            "    }\n"
            "    return *this;\n"
            "}\n"
            "\n";
    }

    // generate choice state
    code.ClassPrivate() <<
        "    // choice state\n"
        "    "<<STATE_ENUM<<' '<<STATE_MEMBER<<";\n"
        "    // helper methods\n"
        "    void DoSelect("<<STATE_ENUM<<" index);\n";
    if ( HaveAssignment() ) {
        code.ClassPrivate() <<
            "    void DoAssign(const "<<codeClassName<<"& src);\n";
    }
    code.ClassPrivate() <<
        "    static void* x_Create(void);\n"
        "    static int x_Selected(const void*);\n"
        "    static void x_Select(void*, int);\n"
        "\n";

    // generate initialization code
    code.AddInitializer(STATE_MEMBER, STATE_NOT_SET);

    // generate destruction code
    code.AddDestructionCode("Reset();");

    // generate Reset method
    {
        code.Methods() <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        if ( havePointers || haveString ) {
            code.Methods() <<
                "    switch ( "<<STATE_MEMBER<<" ) {\n";
            // generate destruction code for pointers
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == ePointerMember ) {
                    code.Methods() <<
                        "    case "<<STATE_PREFIX<<i->cName<<":\n";
                    WriteTabbed(code.Methods(), 
                                i->type->GetDestructionCode(i->memberRef),
                                "        ");
                    if ( i->type->IsObject() ) {
                        code.Methods() <<
                            "        m_"<<i->cName<<"->RemoveReference();\n";
                    }
                    else {
                        code.Methods() <<
                            "        delete m_"<<i->cName<<";\n";
                    }
                    code.Methods() <<
                        "        break;\n";
                }
            }
            if ( haveString ) {
                // generate destruction code for string
                iterate ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eStringMember ) {
                        code.Methods() <<
                            "    case "<<STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                code.Methods() <<
                    "        "<<STRING_MEMBER<<".erase();\n"
                    "        break;\n";
            }
            code.Methods() <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        code.Methods() <<
            "    "<<STATE_MEMBER<<" = "<<STATE_NOT_SET<<";\n"
            "}\n"
            "\n";
    }

    // generate Assign method
    if ( HaveAssignment() ) {
        code.Methods() <<
            "void "<<methodPrefix<<"DoAssign(const "<<codeClassName<<"& src)\n"
            "{\n"
            "    "<<STATE_ENUM<<" index = src.Which();\n"
            "    switch ( index ) {\n";
        iterate ( TVariants, i, m_Variants ) {
            if ( i->memberType == eSimpleMember ) {
                code.Methods() <<
                    "    case "<<STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = src.m_"<<i->cName<<";\n"
                    "        break;\n";
            }
            else if ( i->memberType == ePointerMember ) {
                code.Methods() <<
                    "    case "<<STATE_PREFIX<<i->cName<<":\n";
                if ( i->type->IsObject() ) {
                    code.Methods() <<
                        "        (m_"<<i->cName<<" = src.m_"<<i->cName<<")->AddReference();\n";
                }
                else {
                    code.Methods() <<
                        "        m_"<<i->cName<<" = new T"<<i->cName<<"(*src.m_"<<i->cName<<");\n";
                }
                code.Methods() <<
                    "        break;\n";
            }
        }
        if ( haveString ) {
            // generate destruction code for string
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eStringMember ) {
                    code.Methods() <<
                        "    case "<<STATE_PREFIX<<i->cName<<":\n";
                }
            }
            code.Methods() <<
                "        "<<STRING_MEMBER<<" = src."<<STRING_MEMBER<<";\n"
                "        break;\n";
        }
        code.Methods() <<
            "    default:\n"
            "        break;\n"
            "    }\n"
            "    "<<STATE_MEMBER<<" = index;\n"
            "}\n"
            "\n";
    }

    // generate Select method
    {
        code.Methods() <<
            "void "<<methodPrefix<<"DoSelect("<<STATE_ENUM<<" index)\n"
            "{\n";
        if ( haveUnion ) {
            code.Methods() <<
                "    switch ( index ) {\n";
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eSimpleMember ) {
                    string init = i->type->GetInitializer();
                    code.Methods() <<
                        "    case "<<STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<" = "<<init<<";\n"
                        "        break;\n";
                }
                else if ( i->memberType == ePointerMember ) {
                    code.Methods() <<
                        "    case "<<STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<" = "<<i->type->NewInstance(NcbiEmptyString)<<";\n";
                    if ( i->type->IsObject() ) {
                        code.Methods() <<
                            "        m_"<<i->cName<<"->AddReference();\n";
                    }
                    code.Methods() <<
                        "        break;\n";
                }
            }
            code.Methods() <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        code.Methods() <<
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
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName("<<STATE_ENUM<<" index)\n"
            "{\n"
            "    if ( unsigned(index) > "<<m_Variants.size()<<" )\n"
            "        return \"?unknown?\";\n"
            "    return sm_SelectionNames[index];\n"
            "}\n"
            "\n"
            "void "<<methodPrefix<<"InvalidSelection("<<STATE_ENUM<<" index) const\n"
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
                "    bool Is"<<i->cName<<"(void) const;\n"
                "    const T"<<i->cName<<"& Get"<<i->cName<<"(void) const;\n"
                "    T"<<i->cName<<"& Get"<<i->cName<<"(void);\n"
                "    T"<<i->cName<<"& Set"<<i->cName<<"(void);\n";
            if ( i->memberType == ePointerMember && i->type->IsObject() ) {
                getters <<
                    "    void Set"<<i->cName<<"(const NCBI_NS_NCBI::CRef<T"<<i->cName<<">& ref);\n";
            }
            code.InlineMethods() <<
                "inline\n"
                "bool "<<methodPrefix<<"Is"<<i->cName<<"(void) const\n"
                "{\n"
                "    return "<<STATE_MEMBER<<" == "<<STATE_PREFIX<<i->cName<<";\n"
                "}\n"
                "\n"
                "inline\n"
                "const "<<methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                "{\n"
                "    CheckSelected("<<STATE_PREFIX<<i->cName<<");\n"
                "    return "<<i->memberRef<<";\n"
                "}\n"
                "\n"
                "inline\n"<<
                methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void)\n"
                "{\n"
                "    CheckSelected("<<STATE_PREFIX<<i->cName<<");\n"
                "    return "<<i->memberRef<<";\n"
                "}\n"
                "\n"
                "inline\n"<<
                methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                "{\n"
                "    Select("<<STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n"
                "    return "<<i->memberRef<<";\n"
                "}\n"
                "\n";
            if ( i->memberType != ePointerMember ) {
                getters <<
                    "    void Set"<<i->cName<<"(const T"<<i->cName<<"& value);\n";
                code.InlineMethods() <<
                    "inline\n"
                    "void "<<methodPrefix<<"Set"<<i->cName<<"(const T"<<i->cName<<"& value)\n"
                    "{\n"
                    "    Select("<<STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n"
                    "    "<<i->memberRef<<" = value;\n"
                    "}\n"
                    "\n";
            }
            if ( i->memberType == ePointerMember && i->type->IsObject() ) {
                code.Methods() <<
                    "void "<<methodPrefix<<"Set"<<i->cName<<"(const NCBI_NS_NCBI::CRef<T"<<i->cName<<">& ref)\n"
                    "{\n"
                    "    T"<<i->cName<<"* ptr = const_cast<T"<<i->cName<<"*>(&*ref);\n"
                    "    if ( "<<STATE_MEMBER<<" != "<<STATE_PREFIX<<i->cName<<" || m_"<<i->cName<<" != ptr ) {\n"
                    "        Reset();\n"
                    "        (m_"<<i->cName<<" = ptr)->AddReference();\n"
                    "        "<<STATE_MEMBER<<" = "<<STATE_PREFIX<<i->cName<<";\n"
                    "    }\n"
                    "}\n"
                    "\n";
            }
            getters <<
                "\n";
        }
    }

    // generate variants data
    {
        code.ClassPrivate() <<
            "    // variants data\n";
        if ( haveUnion ) {
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
        if ( haveString ) {
            code.ClassPrivate() <<
                "    "<<STRING_TYPE<<' '<<STRING_MEMBER<<";\n";
        }
    }

    // generate type info
    code.Methods() <<
        "// helper methods\n"
        "void* "<<methodPrefix<<"x_Create(void)\n"
        "{\n"
        "    return "<<GetClassName()<<"::NewInstance();\n"
        "}\n"
        "\n"
        "int "<<methodPrefix<<"x_Selected(const void* object)\n"
        "{\n"
        "    return static_cast<const "<<codeClassName<<"*>(static_cast<const "<<GetClassName()<<"*>(object))->Which()-1;\n"
        "}\n"
        "void "<<methodPrefix<<"x_Select(void* object, int index)\n"
        "{\n"
        "    static_cast<"<<codeClassName<<"*>(static_cast<"<<GetClassName()<<"*>(object))->Select("<<STATE_ENUM<<"(index+1));\n"
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
                    "        NCBI_NS_NCBI::AddMember(info->GetMembers(), \""<<i->externalName<<"\", NCBI_NS_NCBI::Check< "<<i->cType<<"* >::Ptr(MEMBER_PTR(m_"<<i->cName<<")), "<<i->type->GetRef()<<')';
                if ( i->type->IsObject() ) {
                    code.Methods() <<
                        "->SetObjectPointer()";
                }
                else {
                    code.Methods() <<
                        "->SetPointer()";
                }
                code.Methods() <<
                    ";\n";
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

bool CChoiceRefTypeStrings::CanBeInSTL(void) const
{
    return false;
}
