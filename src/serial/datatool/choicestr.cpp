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
* Revision 1.22  2000/08/25 15:59:20  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.21  2000/07/11 20:36:28  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.20  2000/07/03 18:42:57  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
*
* Revision 1.19  2000/06/27 16:34:48  vasilche
* Fixed generated comments.
* Fixed class names conflict. Now internal classes' names begin with "C_".
*
* Revision 1.18  2000/06/16 16:31:37  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.17  2000/05/03 14:38:17  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.16  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.15  2000/04/12 15:36:48  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.14  2000/04/07 19:26:23  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.13  2000/04/06 16:11:25  vasilche
* Removed unneeded calls to Reset().
*
* Revision 1.12  2000/04/03 18:47:29  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.11  2000/03/29 15:52:25  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.10  2000/03/17 16:49:54  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
* All setters/getters made public until we'll find better solution.
*
* Revision 1.9  2000/03/08 14:40:00  vasilche
* Renamed NewInstance() -> New().
*
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
#include <serial/datatool/choicestr.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/fileutil.hpp>

BEGIN_NCBI_SCOPE

#define STATE_ENUM "E_Choice"
#define STATE_MEMBER "m_choice"
#define STRING_TYPE_FULL "NCBI_NS_STD::string"
#define STRING_TYPE "string"
#define STRING_MEMBER "m_string"
#define OBJECT_TYPE_FULL "NCBI_NS_NCBI::CObject"
#define OBJECT_TYPE "CObject"
#define OBJECT_MEMBER "m_object"
#define STATE_PREFIX "e_"
#define STATE_NOT_SET "e_not_set"
#define DELAY_MEMBER "m_delayBuffer"
#define DELAY_TYPE_FULL "NCBI_NS_NCBI::CDelayBuffer"

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
                                    const AutoPtr<CTypeStrings>& type,
                                    bool delayed)
{
    m_Variants.push_back(SVariantInfo(name, type, delayed));
}

CChoiceTypeStrings::SVariantInfo::SVariantInfo(const string& name,
                                               const AutoPtr<CTypeStrings>& t,
                                               bool del)
    : externalName(name), cName(Identifier(name)),
      type(t), delayed(del)
{
    switch ( type->GetKind() ) {
    case eKindString:
        memberType = eStringMember;
        break;
    case eKindStd:
    case eKindEnum:
        memberType = eSimpleMember;
        break;
    case eKindObject:
        memberType = eObjectPointerMember;
        break;
    case eKindPointer:
    case eKindRef:
        ERR_POST("pointer as choice variant");
        memberType = ePointerMember;
        break;
    default:
        memberType = ePointerMember;
        break;
    }
}

void CChoiceTypeStrings::GenerateClassCode(CClassCode& code,
                                           CNcbiOstream& setters,
                                           const string& methodPrefix,
                                           bool haveUserClass,
                                           const string& classPrefix) const
{
    bool haveObjectPointer = false;
    bool havePointers = false;
    bool haveSimple = false;
    bool haveString = false;
    bool delayed = false;
    string codeClassName = GetClassName();
    if ( haveUserClass )
        codeClassName += "_Base";
    // generate variants code
    {
        iterate ( TVariants, i, m_Variants ) {
            switch ( i->memberType ) {
            case ePointerMember:
                havePointers = true;
                i->type->GeneratePointerTypeCode(code);
                break;
            case eObjectPointerMember:
                haveObjectPointer = true;
                i->type->GeneratePointerTypeCode(code);
                break;
            case eSimpleMember:
                haveSimple = true;
                i->type->GenerateTypeCode(code);
                break;
            case eStringMember:
                haveString = true;
                i->type->GenerateTypeCode(code);
                break;
            }
            if ( i->delayed )
                delayed = true;
        }
    }
    if ( delayed )
        code.HPPIncludes().insert("serial/delaybuf");

    bool haveUnion = havePointers || haveSimple ||
        (haveString && haveObjectPointer);
    if ( haveString && haveUnion ) {
        // convert string member to pointer member
        havePointers = true;
    }

    string stdNamespace = 
        code.GetNamespace().GetNamespaceRef(CNamespace::KSTDNamespace);
    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

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
            "    enum "STATE_ENUM" {\n"
            "        "STATE_NOT_SET" = "<<ncbiNamespace<<"kEmptyChoice";
        iterate ( TVariants, i, m_Variants ) {
            code.ClassPublic() << ",\n"
                "        "STATE_PREFIX<<i->cName;
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
    code.ClassPublic() <<
        "    // choice state\n"
        "    "STATE_ENUM" Which(void) const;\n"
        "    // throw exception if current selection is not as requested\n"
        "    void CheckSelected("STATE_ENUM" index) const;\n"
        "    // throw exception about wrong selection\n"
        "    void ThrowInvalidSelection("STATE_ENUM" index) const;\n"
        "    // return selection name (for diagnostic purposes)\n"
        "    static "<<stdNamespace<<"string SelectionName("STATE_ENUM" index);\n"
        "\n";
    setters <<
        "    // select requested variant if needed\n"
        "    void Select("STATE_ENUM" index, "<<ncbiNamespace<<"EResetVariant reset = "<<ncbiNamespace<<"eDoResetVariant);\n";
    if ( delayed ) {
        setters <<
            "    // select requested variant using delay buffer (for internal use)\n"
            "    void SelectDelayBuffer("STATE_ENUM" index);\n";
    }
    setters <<
        "\n";

    CNcbiOstream& methods = code.Methods();
    CNcbiOstream& inlineMethods = code.InlineMethods();

    inlineMethods <<
        "inline\n"<<
        methodPrefix<<STATE_ENUM" "<<methodPrefix<<"Which(void) const\n"
        "{\n"
        "    return "STATE_MEMBER";\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"CheckSelected("STATE_ENUM" index) const\n"
        "{\n"
        "    if ( "STATE_MEMBER" != index )\n"
        "        ThrowInvalidSelection(index);\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"Select("STATE_ENUM" index, NCBI_NS_NCBI::EResetVariant reset)\n"
        "{\n"
        "    if ( reset == NCBI_NS_NCBI::eDoResetVariant || "STATE_MEMBER" != index ) {\n"
        "        if ( "STATE_MEMBER" != "STATE_NOT_SET" )\n"
        "            Reset();\n"
        "        DoSelect(index);\n"
        "    }\n"
        "}\n"
        "\n";
    if ( delayed ) {
        inlineMethods <<
            "inline\n"
            "void "<<methodPrefix<<"SelectDelayBuffer("STATE_ENUM" index)\n"
            "{\n"
            "    if ( "STATE_MEMBER" != "STATE_NOT_SET" || "DELAY_MEMBER".GetIndex() != (index - 1))\n"
            "        THROW1_TRACE(NCBI_NS_STD::runtime_error, \"illegal call\");\n"
            "    "STATE_MEMBER" = index;\n"
            "}\n"
            "\n";
    }

    if ( HaveAssignment() ) {
        inlineMethods <<
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
        "    "STATE_ENUM" "STATE_MEMBER";\n"
        "    // helper methods\n"
        "    void DoSelect("STATE_ENUM" index);\n";
    if ( HaveAssignment() ) {
        code.ClassPrivate() <<
            "    void DoAssign(const "<<codeClassName<<"& src);\n";
    }

    code.ClassPrivate() <<
        "\n";

    // generate initialization code
    code.AddInitializer(STATE_MEMBER, STATE_NOT_SET);

    // generate destruction code
    code.AddDestructionCode("if ( "STATE_MEMBER" != "STATE_NOT_SET" )\n"
                            "    Reset();");

    // generate Reset method
    {
        methods <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        if ( haveObjectPointer || havePointers || haveString ) {
            if ( delayed ) {
                methods <<
                    "    if ( "DELAY_MEMBER" )\n"
                    "        "DELAY_MEMBER".Forget();\n"
                    "    else\n";
            }
            methods <<
                "    switch ( "STATE_MEMBER" ) {\n";
            // generate destruction code for pointers
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == ePointerMember ) {
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n";
                    WriteTabbed(methods, 
                                i->type->GetDestructionCode("*m_"+i->cName),
                                "        ");
                    methods <<
                        "        delete m_"<<i->cName<<";\n"
                        "        break;\n";
                }
            }
            if ( haveString ) {
                // generate destruction code for string
                iterate ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eStringMember ) {
                        methods <<
                            "    case "STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveUnion ) {
                    // string is pointer inside union
                    methods <<
                        "        delete "STRING_MEMBER";\n";
                }
                else {
                    methods <<
                        "        "STRING_MEMBER".erase();\n";
                }
                methods <<
                    "        break;\n";
            }
            if ( haveObjectPointer ) {
                // generate destruction code for pointers to CObject
                iterate ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eObjectPointerMember ) {
                        methods <<
                            "    case "STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                methods <<
                    "        "OBJECT_MEMBER"->RemoveReference();\n"
                    "        break;\n";
            }
            methods <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        methods <<
            "    "STATE_MEMBER" = "STATE_NOT_SET";\n"
            "}\n"
            "\n";
    }

    // generate Assign method
    if ( HaveAssignment() ) {
        methods <<
            "void "<<methodPrefix<<"DoAssign(const "<<codeClassName<<"& src)\n"
            "{\n"
            "    "STATE_ENUM" index = src.Which();\n"
            "    switch ( index ) {\n";
        iterate ( TVariants, i, m_Variants ) {
            switch ( i->memberType ) {
            case eSimpleMember:
                methods <<
                    "    case "STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = src.m_"<<i->cName<<";\n"
                    "        break;\n";
                break;
            case ePointerMember:
                methods <<
                    "    case "STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = new T"<<i->cName<<"(*src.m_"<<i->cName<<");\n"
                    "        break;\n";
                break;
            case eStringMember:
                _ASSERT(haveString);
                // will be handled specially
                break;
            case eObjectPointerMember:
                // will be handled specially
                _ASSERT(haveObjectPointer);
                break;
            }
        }
        if ( haveString ) {
            // generate copy code for string
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eStringMember ) {
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n";
                }
            }
            if ( haveUnion ) {
                // string is pointer
                methods <<
                    "        "STRING_MEMBER" = src."STRING_MEMBER";\n";
            }
            else {
                methods <<
                    "        "STRING_MEMBER" = new "STRING_TYPE_FULL"(*src."STRING_MEMBER");\n";
            }
            methods <<
                "        break;\n";
        }
        if ( haveObjectPointer ) {
            // generate copy code for string
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eObjectPointerMember ) {
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n";
                }
            }
            methods <<
                "        ("OBJECT_MEMBER" = src."OBJECT_MEMBER")->AddReference();\n"
                "        break;\n";
        }

        methods <<
            "    default:\n"
            "        break;\n"
            "    }\n"
            "    "STATE_MEMBER" = index;\n"
            "}\n"
            "\n";
    }

    // generate Select method
    {
        methods <<
            "void "<<methodPrefix<<"DoSelect("STATE_ENUM" index)\n"
            "{\n";
        if ( haveUnion || haveObjectPointer ) {
            methods <<
                "    switch ( index ) {\n";
            iterate ( TVariants, i, m_Variants ) {
                switch ( i->memberType ) {
                case eSimpleMember:
                    {
                        string init = i->type->GetInitializer();
                        methods <<
                            "    case "STATE_PREFIX<<i->cName<<":\n"
                            "        m_"<<i->cName<<" = "<<init<<";\n"
                            "        break;\n";
                    }
                    break;
                case ePointerMember:
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<" = "<<i->type->NewInstance(NcbiEmptyString)<<";\n"
                        "        break;\n";
                    break;
                case eObjectPointerMember:
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n"
                        "        ("OBJECT_MEMBER" = "<<i->type->NewInstance(NcbiEmptyString)<<")->AddReference();\n"
                        "        break;\n";
                    break;
                case eStringMember:
                    // will be handled specially
                    break;
                }
            }
            if ( haveString ) {
                iterate ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eStringMember ) {
                        methods <<
                            "    case "STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                methods <<
                    "        "STRING_MEMBER" = new "STRING_TYPE_FULL";\n"
                    "        break;\n";
            }
            methods <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        methods <<
            "    "STATE_MEMBER" = index;\n"
            "}\n"
            "\n";
    }

    // generate choice variants names
    code.ClassPrivate() <<
        "    static const char* const sm_SelectionNames[];\n";
    {
        methods <<
            "const char* const "<<methodPrefix<<"sm_SelectionNames[] = {\n"
            "    \"not set\"";
        iterate ( TVariants, i, m_Variants ) {
            methods << ",\n"
                "    \""<<i->externalName<<"\"";
        }
        methods << "\n"
            "};\n"
            "\n"
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName("STATE_ENUM" index)\n"
            "{\n"
            "    return NCBI_NS_NCBI::CInvalidChoiceSelection::GetName(index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n"
            "void "<<methodPrefix<<"ThrowInvalidSelection("STATE_ENUM" index) const\n"
            "{\n"
            "    throw NCBI_NS_NCBI::CInvalidChoiceSelection(m_choice, index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n";
    }
    
    // generate variant types
    {
        code.ClassPublic() <<
            "    // variants' types\n";
        iterate ( TVariants, i, m_Variants ) {
            string cType = i->type->GetCType(code.GetNamespace());
            code.ClassPublic() <<
                "    typedef "<<cType<<" T"<<i->cName<<";\n";
        }
        code.ClassPublic() << 
            "\n";
    }

    // generate variant getters & setters
    {
        code.ClassPublic() <<
            "    // variants' getters\n";
        setters <<
            "    // variants' setters\n";
        iterate ( TVariants, i, m_Variants ) {
            string cType = i->type->GetCType(code.GetNamespace());
            code.ClassPublic() <<
                "    bool Is"<<i->cName<<"(void) const;\n"
                "    const "<<cType<<"& Get"<<i->cName<<"(void) const;\n"
                "\n";
            setters <<
                "    "<<cType<<"& Get"<<i->cName<<"(void);\n"
                "    "<<cType<<"& Set"<<i->cName<<"(void);\n";
            if ( i->memberType == eSimpleMember ||
                 i->memberType == eStringMember ) {
                setters <<
                    "    void Set"<<i->cName<<"(const "<<cType<<"& value);\n";
            }
            if ( i->memberType == eObjectPointerMember ) {
                setters <<
                    "    void Set"<<i->cName<<"(const "<<ncbiNamespace<<"CRef<"<<cType<<">& ref);\n";
            }
            string memberRef;
            string constMemberRef;
            bool inl = true;
            switch ( i->memberType ) {
            case eSimpleMember:
                memberRef = constMemberRef = "m_"+i->cName;
                break;
            case ePointerMember:
                memberRef = constMemberRef = "*m_"+i->cName;
                break;
            case eStringMember:
                memberRef = STRING_MEMBER;
                if ( haveUnion ) {
                    // string is pointer
                    memberRef = '*'+memberRef;
                }
                constMemberRef = memberRef;
                break;
            case eObjectPointerMember:
                memberRef = "*static_cast<T"+i->cName+"*>("OBJECT_MEMBER")";
                constMemberRef = "*static_cast<const T"+i->cName+"*>("OBJECT_MEMBER")";
                inl = false;
                break;
            }
            if ( i->delayed )
                inl = false;
            inlineMethods <<
                "inline\n"
                "bool "<<methodPrefix<<"Is"<<i->cName<<"(void) const\n"
                "{\n"
                "    return "STATE_MEMBER" == "STATE_PREFIX<<i->cName<<";\n"
                "}\n"
                "\n";
            code.MethodStart(inl) <<
                "const "<<methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                "{\n"
                "    CheckSelected("STATE_PREFIX<<i->cName<<");\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_MEMBER".Update();\n";
            }
            code.Methods(inl) <<
                "    return "<<constMemberRef<<";\n"
                "}\n"
                "\n";
            code.MethodStart(inl) <<
                methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void)\n"
                "{\n"
                "    CheckSelected("STATE_PREFIX<<i->cName<<");\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_MEMBER".Update();\n";
            }
            code.Methods(inl) <<
                "    return "<<memberRef<<";\n"
                "}\n"
                "\n";
            code.MethodStart(inl) <<
                methodPrefix<<"T"<<i->cName<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                "{\n"
                "    Select("STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_MEMBER".Update();\n";
            }
            code.Methods(inl) <<
                "    return "<<memberRef<<";\n"
                "}\n"
                "\n";
            if ( i->memberType == eSimpleMember ||
                 i->memberType == eStringMember ) {
                inlineMethods <<
                    "inline\n"
                    "void "<<methodPrefix<<"Set"<<i->cName<<"(const T"<<i->cName<<"& value)\n"
                    "{\n"
                    "    Select("STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_MEMBER".Forget();\n";
                }
                inlineMethods <<
                    "    "<<memberRef<<" = value;\n"
                    "}\n"
                    "\n";
            }
            if ( i->memberType == eObjectPointerMember ) {
                methods <<
                    "void "<<methodPrefix<<"Set"<<i->cName<<"(const NCBI_NS_NCBI::CRef<T"<<i->cName<<">& ref)\n"
                    "{\n"
                    "    T"<<i->cName<<"* ptr = const_cast<T"<<i->cName<<"*>(&*ref);\n";
                if ( i->delayed ) {
                    methods <<
                        "    if ( "STATE_MEMBER" != "STATE_PREFIX<<i->cName<<" || "DELAY_MEMBER" || "OBJECT_MEMBER" != ptr ) {\n";
                }
                else {
                    methods <<
                        "    if ( "STATE_MEMBER" != "STATE_PREFIX<<i->cName<<" || "OBJECT_MEMBER" != ptr ) {\n";
                }
                methods <<
                    "        Reset();\n"
                    "        ("OBJECT_MEMBER" = ptr)->AddReference();\n"
                    "        "STATE_MEMBER" = "STATE_PREFIX<<i->cName<<";\n"
                    "    }\n"
                    "}\n"
                    "\n";
            }
            setters <<
                "\n";
        }
    }

    // generate variants data
    {
        code.ClassPrivate() <<
            "    // variants' data\n";
        if ( haveUnion ) {
            code.ClassPrivate() << "    union {\n";
            iterate ( TVariants, i, m_Variants ) {
                if ( i->memberType == eSimpleMember ) {
                    code.ClassPrivate() <<
                        "        T"<<i->cName<<" m_"<<i->cName<<";\n";
                }
                else if ( i->memberType == ePointerMember ) {
                    code.ClassPrivate() <<
                        "        T"<<i->cName<<" *m_"<<i->cName<<";\n";
                }
            }
            if ( haveString ) {
                code.ClassPrivate() <<
                    "        "STRING_TYPE_FULL" *"STRING_MEMBER";\n";
            }
            if ( haveObjectPointer ) {
                code.ClassPrivate() <<
                    "        "OBJECT_TYPE_FULL" *"OBJECT_MEMBER";\n";
            }
            code.ClassPrivate() <<
                "    };\n";
        }
        else if ( haveString ) {
            code.ClassPrivate() <<
                "    "STRING_TYPE_FULL" "STRING_MEMBER";\n";
        }
        else if ( haveObjectPointer ) {
            code.ClassPrivate() <<
                "    "OBJECT_TYPE_FULL" *"OBJECT_MEMBER";\n";
        }
        if ( delayed ) {
            code.ClassPrivate() <<
                "    mutable "DELAY_TYPE_FULL" "DELAY_MEMBER";\n";
        }
    }

    // generate type info
    methods <<
        "// helper methods\n"
        "\n"
        "// type info\n";
    if ( haveUserClass )
        methods << "BEGIN_NAMED_BASE_CHOICE_INFO";
    else
        methods << "BEGIN_NAMED_CHOICE_INFO";
    methods <<
        "(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassName()<<")\n"
        "{\n";
    if ( delayed ) {
        methods <<
            "    info->SetSelectDelay(&NCBI_NS_NCBI::CClassInfoHelper<CClass>::SelectDelayBuffer);\n";
    }
    {
        iterate ( TVariants, i, m_Variants ) {
            methods << "    ADD_NAMED_";
            
            bool addNamespace = false;
            bool addCType = false;
            bool addEnum = false;
            bool addRef = false;
            
            switch ( i->memberType ) {
            case ePointerMember:
                methods << "PTR_";
                addRef = true;
                break;
            case eObjectPointerMember:
                methods << "REF_";
                addCType = true;
                break;
            case eSimpleMember:
                if ( i->type->GetKind() == eKindEnum ) {
                    methods << "ENUM_";
                    addEnum = true;
                    if ( !i->type->GetNamespace().IsEmpty() &&
                         code.GetNamespace() != i->type->GetNamespace() ) {
                        _TRACE("EnumNamespace: "<<i->type->GetNamespace()<<" from "<<code.GetNamespace());
                        methods << "IN_";
                        addNamespace = true;
                    }
                }
                else if ( i->type->HaveSpecialRef() ) {
                    addRef = true;
                }
                else {
                    methods << "STD_";
                }
                break;
            case eStringMember:
                if ( haveUnion ) {
                    methods << "PTR_";
                    addRef = true;
                }
                else if ( i->type->HaveSpecialRef() ) {
                    addRef = true;
                }
                else {
                    methods << "STD_";
                }
                break;
            }

            methods << "CHOICE_VARIANT(\""<<i->externalName<<"\", ";
            switch ( i->memberType ) {
            case eObjectPointerMember:
                methods << OBJECT_MEMBER;
                break;
            case eStringMember:
                methods << STRING_MEMBER;
                break;
            default:
                methods << "m_"<<i->cName;
                break;
            }
            if ( addNamespace )
                methods << ", "<<i->type->GetNamespace();
            if ( addCType )
                methods << ", "<<i->type->GetCType(code.GetNamespace());
            if ( addEnum )
                methods << ", "<<i->type->GetEnumName();
            if ( addRef )
                methods << ", "<<i->type->GetRef(code.GetNamespace());
            methods <<')';
            
            if ( i->delayed ) {
                methods << "->SetDelayBuffer(MEMBER_PTR(m_delayBuffer))";
            }
            methods << ";\n";
        }
    }
    methods <<
        "}\n"
        "END_CHOICE_INFO\n"
        "\n";
}

CChoiceRefTypeStrings::CChoiceRefTypeStrings(const string& className,
                                             const CNamespace& ns,
                                             const string& fileName)
    : CParent(className, ns, fileName)
{
}

END_NCBI_SCOPE
