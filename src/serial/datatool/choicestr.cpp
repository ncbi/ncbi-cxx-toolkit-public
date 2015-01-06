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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiutil.hpp>
#include "exceptions.hpp"
#include "type.hpp"
#include "blocktype.hpp"
#include "choicestr.hpp"
#include "stdstr.hpp"
#include "code.hpp"
#include "srcutil.hpp"
#include <serial/serialdef.hpp>
#include "statictype.hpp"
#include "unitype.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_TypeInfo

BEGIN_NCBI_SCOPE

#define STATE_ENUM "E_Choice"
#define STATE_MEMBER "m_choice"
#define STRING_TYPE_FULL "NCBI_NS_STD::string"
#define STRING_TYPE "string"
#define STRING_MEMBER "m_string"
#define UTF8_STRING_MEMBER "m_string_utf8"
#define OBJECT_TYPE_FULL "NCBI_NS_NCBI::CSerialObject"
#define OBJECT_TYPE "CSerialObject"
#define OBJECT_MEMBER "m_object"
#define STATE_PREFIX "e_"
#define STATE_NOT_SET "e_not_set"
#define DELAY_MEMBER "m_delayBuffer"
#define DELAY_TYPE_FULL "NCBI_NS_NCBI::CDelayBuffer"

CChoiceTypeStrings::CChoiceTypeStrings(const string& externalName,
                                       const string& className,
                                       const string& namespaceName,
                                       const CDataType* dataType,
                                       const CComments& comments)
    : CParent(externalName, className, namespaceName, dataType, comments),
      m_HaveAssignment(false)
{
}

CChoiceTypeStrings::~CChoiceTypeStrings(void)
{
}

void CChoiceTypeStrings::AddVariant(const string& external_name,
                                    const string& name,
                                    const AutoPtr<CTypeStrings>& type,
                                    bool delayed, bool in_union, int tag,
                                    bool noPrefix, bool attlist, bool noTag,
                                    bool simple, const CDataType* dataType,
                                    const CComments& comments)
{
    m_Variants.push_back(SVariantInfo(external_name, name, type, delayed, in_union, tag,
                                      noPrefix,attlist,noTag,simple,dataType,
                                      comments));
}

CChoiceTypeStrings::SVariantInfo::SVariantInfo(const string& external_name,
                                               const string& name,
                                               const AutoPtr<CTypeStrings>& t,
                                               bool del, bool in_un, int tag,
                                               bool noPrefx, bool attlst,
                                               bool noTg,bool simpl,
                                               const CDataType* dataTp,
                                               const CComments& commnts)
    : externalName(external_name), cName(Identifier(name)),
      type(t), delayed(del), in_union(in_un), memberTag(tag),
      noPrefix(noPrefx), attlist(attlst), noTag(noTg), simple(simpl),
      dataType(dataTp), comments(commnts)
{
    switch ( type->GetKind() ) {
    case eKindString:
        memberType = eStringMember;
        {
            const CStringDataType* strtype =
                dynamic_cast<const CStringDataType*>(dataType);
            if (strtype && strtype->GetStringType() == CStringDataType::eStringTypeUTF8) {
                memberType = eUtf8StringMember;
            }
        }
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
        ERR_POST_X(3, "pointer as choice variant");
        memberType = ePointerMember;
        break;
    default:
        memberType = in_union? eBufferMember: ePointerMember;
        break;
    }
}

bool CChoiceTypeStrings::x_IsNullType(TVariants::const_iterator i) const
{
    return (dynamic_cast<CNullTypeStrings*>(i->type.get()) != 0);
}

bool CChoiceTypeStrings::x_IsNullWithAttlist(TVariants::const_iterator i) const
{
    if (i->dataType) {
        const CDataType* resolved = i->dataType->Resolve();
        if (resolved && resolved != i->dataType) {
            CClassTypeStrings* typeStr = resolved->GetTypeStr();
            if (typeStr) {
                ITERATE ( TMembers, ir, typeStr->m_Members ) {
                    if (ir->simple) {
                        return CClassTypeStrings::x_IsNullType(ir);
                    }
                }
            }
        }
    }
    return false;
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
    bool haveString = false, haveUtf8String = false;
    bool delayed = false;
    bool haveAttlist = false;
    bool haveBuffer = false;
    string utf8CType;
    string codeClassName = GetClassNameDT();
    if ( haveUserClass )
        codeClassName += "_Base";
    // generate variants code
    {
        ITERATE ( TVariants, i, m_Variants ) {
            switch ( i->memberType ) {
            case ePointerMember:
                havePointers = true;
                i->type->GeneratePointerTypeCode(code);
                break;
            case eObjectPointerMember:
                if (i->attlist) {
                    haveAttlist = true;
                } else {
                    haveObjectPointer = true;
                }
                i->type->GeneratePointerTypeCode(code);
                break;
            case eSimpleMember:
                haveSimple = true;
                i->type->GenerateTypeCode(code);
                break;
            case eBufferMember:
                haveBuffer = true;
                i->type->GenerateTypeCode(code);
                break;
            case eStringMember:
                if ( i->in_union ) {
                    haveBuffer = true;
                }
                haveString = true;
                i->type->GenerateTypeCode(code);
                break;
            case eUtf8StringMember:
                if ( i->in_union ) {
                    haveBuffer = true;
                }
                haveUtf8String = true;
                i->type->GenerateTypeCode(code);
                utf8CType = i->type->GetCType(code.GetNamespace());
                break;
            }
            if ( i->delayed )
                delayed = true;
        }
    }
    if ( delayed )
        code.HPPIncludes().insert("serial/delaybuf");

    bool haveUnion = havePointers || haveSimple || haveBuffer ||
        ((haveString || haveUtf8String) && haveObjectPointer);
    if ( (haveString || haveUtf8String) && haveUnion && !haveBuffer ) {
        // convert string member to pointer member
        havePointers = true;
    }

    string stdNamespace = 
        code.GetNamespace().GetNamespaceRef(CNamespace::KSTDNamespace);
    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    if ( HaveAssignment() ) {
        code.ClassPublic() <<
            "    /// Copy constructor.\n"
            "    "<<codeClassName<<"(const "<<codeClassName<<"& src);\n\n"
            "   /// Assignment operator\n"
            "    "<<codeClassName<<"& operator=(const "<<codeClassName<<"& src);\n\n\n";
    } else {
        code.ClassPrivate() <<
            "    // copy constructor and assignment operator\n"
            "    "<<codeClassName<<"(const "<<codeClassName<<"& );\n"
            "    "<<codeClassName<<"& operator=(const "<<codeClassName<<"& );\n";
    }

    // generated choice enum
    {
        string cName(STATE_NOT_SET);
        size_t currlen, maxlen = cName.size() + 2;
        ITERATE(TVariants, i, m_Variants) {
            if (!i->attlist) {
                maxlen = max(maxlen,i->cName.size());
            }
        }
        code.ClassPublic() <<
            "\n    /// Choice variants.\n"
            "    enum " STATE_ENUM " {\n"
            "        " STATE_NOT_SET " = "<<kEmptyChoice
            <<"," ;
        for (currlen = strlen(STATE_NOT_SET)+2; currlen < maxlen; ++currlen) {
            code.ClassPublic() << " ";
        }
        code.ClassPublic()
            <<"  ///< No variant selected\n" ;
        TMemberIndex currIndex = kEmptyChoice;
        bool needIni = false;
        for (TVariants::const_iterator i= m_Variants.begin(); i != m_Variants.end();) {
            ++currIndex;
            if (!i->attlist) {
                const CComments& comments = i->comments;
                cName = i->cName;
                code.ClassPublic() << "        " STATE_PREFIX<<cName;
                if (needIni) {
                    code.ClassPublic() << " = "<<currIndex;
                    needIni = false;
                }
                ++i;
                if (i != m_Variants.end()) {
                    code.ClassPublic() << ",";
                } else if ( !comments.Empty() ) {
                    code.ClassPublic() << " ";
                }
                if ( !comments.Empty() ) {
                    code.ClassPublic() << string(maxlen-cName.size(),' ');
                    comments.PrintHPPEnum(code.ClassPublic());
                }
//                code.ClassPublic() << "  ///< Variant "<<cName<<" is selected.";
                code.ClassPublic() << "\n";
            } else {
                ++i;
                needIni = true;
            }
        }
        code.ClassPublic() << "    };\n";

        code.ClassPublic() << "    /// Maximum+1 value of the choice variant enumerator.\n";
        code.ClassPublic() <<
            "    enum E_ChoiceStopper {\n"
            "        e_MaxChoice = " << currIndex+1 << " ///< == " STATE_PREFIX
                << m_Variants.rbegin()->cName << "+1\n"
            "    };\n"
            "\n";
    }

    code.ClassPublic() <<
        "    /// Reset the whole object\n"
        "    ";
    if ( HaveUserClass() )
        code.ClassPublic() << "virtual ";
    code.ClassPublic() << "void Reset(void);\n\n";

    code.ClassPublic() <<
        "    /// Reset the selection (set it to " STATE_NOT_SET ").\n"
        "    ";
    if ( HaveUserClass() )
        code.ClassPublic() << "virtual ";
    code.ClassPublic() << "void ResetSelection(void);\n\n";

    // generate choice methods
    code.ClassPublic() <<
        "    /// Which variant is currently selected.\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @return\n"
            "    ///   Choice state enumerator.\n";
    }
    code.ClassPublic() <<
        "    " STATE_ENUM " Which(void) const;\n\n"
        "    /// Verify selection, throw exception if it differs from the expected.\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   Expected selection.\n";
    }
    code.ClassPublic() <<
        "    void CheckSelected(" STATE_ENUM " index) const;\n\n"
        "    /// Throw \'InvalidSelection\' exception.\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   Expected selection.\n";
    }
    code.ClassPublic() <<
        "    NCBI_NORETURN void ThrowInvalidSelection(" STATE_ENUM " index) const;\n\n"
        "    /// Retrieve selection name (for diagnostic purposes).\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   One of possible selection states.\n"
            "    /// @return\n"
            "    ///   Name string.\n";
    }
    code.ClassPublic() <<
        "    static "<<stdNamespace<<"string SelectionName(" STATE_ENUM " index);\n"
        "\n";
    setters <<
        "    /// Select the requested variant if needed.\n";
    if (CClassCode::GetDoxygenComments()) {
        setters <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   New selection state.\n"
            "    /// @param reset\n"
            "    ///   Flag that defines the resetting of the variant data. The data will\n"
            "    ///   be reset if either the current selection differs from the new one,\n"
            "    ///   or the flag is set to eDoResetVariant.\n";
    }
    setters <<
        "    void Select(" STATE_ENUM " index, "<<ncbiNamespace<<"EResetVariant reset = "<<ncbiNamespace<<"eDoResetVariant);\n";
    setters <<
        "    /// Select the requested variant if needed,\n"
        "    /// allocating CObject variants from memory pool.\n";
    setters <<
        "    void Select(" STATE_ENUM " index,\n"
        "                "<<ncbiNamespace<<"EResetVariant reset,\n"
        "                "<<ncbiNamespace<<"CObjectMemoryPool* pool);\n";
    if ( delayed ) {
        setters <<
            "    /// Select the requested variant using delay buffer (for internal use).\n";
        if (CClassCode::GetDoxygenComments()) {
            setters <<
                "    ///\n"
                "    /// @param index\n"
                "    ///   New selection state.\n";
        }
        setters <<
            "    void SelectDelayBuffer(" STATE_ENUM " index);\n";
    }
    setters <<
        "\n";

    CNcbiOstream& methods = code.Methods();
    CNcbiOstream& inlineMethods = code.InlineMethods();

    inlineMethods <<
        "inline\n"<<
        methodPrefix<<STATE_ENUM " "<<methodPrefix<<"Which(void) const\n"
        "{\n"
        "    return " STATE_MEMBER ";\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"CheckSelected(" STATE_ENUM " index) const\n"
        "{\n"
        "    if ( " STATE_MEMBER " != index )\n"
        "        ThrowInvalidSelection(index);\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"Select(" STATE_ENUM " index, NCBI_NS_NCBI::EResetVariant reset, NCBI_NS_NCBI::CObjectMemoryPool* pool)\n"
        "{\n"
        "    if ( reset == NCBI_NS_NCBI::eDoResetVariant || " STATE_MEMBER " != index ) {\n"
        "        if ( " STATE_MEMBER " != " STATE_NOT_SET " )\n"
        "            ResetSelection();\n"
        "        DoSelect(index, pool);\n"
        "    }\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"Select(" STATE_ENUM " index, NCBI_NS_NCBI::EResetVariant reset)\n"
        "{\n"
        "    Select(index, reset, 0);\n"
        "}\n"
        "\n";
    if ( delayed ) {
        inlineMethods <<
            "inline\n"
            "void "<<methodPrefix<<"SelectDelayBuffer(" STATE_ENUM " index)\n"
            "{\n"
            "    if ( " STATE_MEMBER " != " STATE_NOT_SET " || " DELAY_MEMBER ".GetIndex() != (index - 1))\n"
            "        NCBI_THROW(ncbi::CSerialException,eIllegalCall, \"illegal call\");\n"
            "    " STATE_MEMBER " = index;\n"
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
        "    " STATE_ENUM " " STATE_MEMBER ";\n"
        "    // helper methods\n"
        "    void DoSelect(" STATE_ENUM " index, "<<ncbiNamespace<<"CObjectMemoryPool* pool = 0);\n";
    if ( HaveAssignment() ) {
        code.ClassPrivate() <<
            "    void DoAssign(const "<<codeClassName<<"& src);\n";
    }

    code.ClassPrivate() <<
        "\n";

    // generate initialization code
    code.AddInitializer(STATE_MEMBER, STATE_NOT_SET);
    if (haveAttlist) {
        ITERATE ( TVariants, i, m_Variants ) {
            if (i->attlist) {
                string member("m_");
                member += i->cName;
                string init("new C_");
                init += i->cName;
                init += "()";
                code.AddInitializer(member, init);
            }
        }
    }

    // generate destruction code
    code.AddDestructionCode("Reset();");

    // generate Reset method
    {
        methods <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        ITERATE ( TVariants, i, m_Variants ) {
            if (i->attlist) {
                methods << "    Reset" << i->cName << "();\n";
            }
        }
        methods << "    if ( " STATE_MEMBER " != " STATE_NOT_SET " )\n"
                << "        ResetSelection();\n";
        methods <<
            "}\n"
            "\n";
        methods <<
            "void "<<methodPrefix<<"ResetSelection(void)\n"
            "{\n";
        if ( haveObjectPointer || havePointers || haveString || haveUtf8String || haveBuffer ) {
            if ( delayed ) {
                methods <<
                    "    if ( " DELAY_MEMBER " )\n"
                    "        " DELAY_MEMBER ".Forget();\n"
                    "    else\n";
            }
            methods <<
                "    switch ( " STATE_MEMBER " ) {\n";
            // generate destruction code for pointers
            ITERATE ( TVariants, i, m_Variants ) {
                if (i->attlist) {
                    continue;
                }
                if ( i->memberType == ePointerMember ) {
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n";
                    WriteTabbed(methods, 
                                i->type->GetDestructionCode("*m_"+i->cName),
                                "        ");
                    methods <<
                        "        delete m_"<<i->cName<<";\n"
                        "        break;\n";
                }
                if ( i->memberType == eBufferMember ) {
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n";
                    WriteTabbed(methods, 
                                i->type->GetDestructionCode("*m_"+i->cName),
                                "        ");
                    methods <<
                        "        m_"<<i->cName<<".Destruct();\n"
                        "        break;\n";
                }
            }
            if ( haveString ) {
                // generate destruction code for string
                ITERATE ( TVariants, i, m_Variants ) {
                    if (i->attlist) {
                        continue;
                    }
                    if ( i->memberType == eStringMember ) {
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveUnion ) {
                    // string is pointer inside union
                    if ( haveBuffer ) {
                        methods <<
                            "        " STRING_MEMBER ".Destruct();\n";
                    }
                    else {
                        methods <<
                            "        delete " STRING_MEMBER ";\n";
                    }
                }
                else {
                    methods <<
                        "        " STRING_MEMBER ".erase();\n";
                }
                methods <<
                    "        break;\n";
            }
            if ( haveUtf8String ) {
                // generate destruction code for string
                ITERATE ( TVariants, i, m_Variants ) {
                    if (i->attlist) {
                        continue;
                    }
                    if ( i->memberType == eUtf8StringMember ) {
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveUnion ) {
                    // string is pointer inside union
                    if ( haveBuffer ) {
                        methods <<
                            "        " UTF8_STRING_MEMBER ".Destruct();\n";
                    }
                    else {
                        methods <<
                            "        delete " UTF8_STRING_MEMBER ";\n";
                    }
                }
                else {
                    methods <<
                        "        " UTF8_STRING_MEMBER ".erase();\n";
                }
                methods <<
                    "        break;\n";
            }
            if ( haveObjectPointer ) {
                // generate destruction code for pointers to CObject
                ITERATE ( TVariants, i, m_Variants ) {
                    if (i->attlist) {
                        continue;
                    }
                    if ( i->memberType == eObjectPointerMember ) {
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                methods <<
                    "        " OBJECT_MEMBER "->RemoveReference();\n"
                    "        break;\n";
            }
            methods <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        methods <<
            "    " STATE_MEMBER " = " STATE_NOT_SET ";\n"
            "}\n"
            "\n";
    }

    // generate Assign method
    if ( HaveAssignment() ) {
        methods <<
            "void "<<methodPrefix<<"DoAssign(const "<<codeClassName<<"& src)\n"
            "{\n"
            "    " STATE_ENUM " index = src.Which();\n"
            "    switch ( index ) {\n";
        ITERATE ( TVariants, i, m_Variants ) {
            switch ( i->memberType ) {
            case eSimpleMember:
                methods <<
                    "    case " STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = src.m_"<<i->cName<<";\n"
                    "        break;\n";
                break;
            case ePointerMember:
                methods <<
                    "    case " STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<" = new T"<<i->cName<<"(*src.m_"<<i->cName<<");\n"
                    "        break;\n";
                break;
            case eBufferMember:
                methods <<
                    "    case " STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<".Construct();\n"
                    "        *m_"<<i->cName<<" = *src.m_"<<i->cName<<";\n"
                    "        break;\n";
                break;
            case eStringMember:
                _ASSERT(haveString);
                // will be handled specially
                break;
            case eUtf8StringMember:
                _ASSERT(haveUtf8String);
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
            ITERATE ( TVariants, i, m_Variants ) {
                if ( i->memberType == eStringMember ) {
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n";
                }
            }
            if ( haveUnion ) {
                if ( haveBuffer ) {
                    methods <<
                        "        " STRING_MEMBER ".Construct();\n"
                        "        *" STRING_MEMBER " = *src." STRING_MEMBER ";\n";
                }
                else {
                    // string is pointer
                    methods <<
                        "        " STRING_MEMBER " = new " STRING_TYPE_FULL "(*src." STRING_MEMBER ");\n";
                }
            }
            else {
                methods <<
                    "        " STRING_MEMBER " = src." STRING_MEMBER ";\n";
            }
            methods <<
                "        break;\n";
        }
        if ( haveUtf8String ) {
            // generate copy code for string
            ITERATE ( TVariants, i, m_Variants ) {
                if ( i->memberType == eUtf8StringMember ) {
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n";
                }
            }
            if ( haveUnion ) {
                if ( haveBuffer ) {
                    methods <<
                        "        " UTF8_STRING_MEMBER ".Construct();\n"
                        "        *" UTF8_STRING_MEMBER " = *src." UTF8_STRING_MEMBER ";\n";
                }
                else {
                    // string is pointer
                    methods <<
                        "        " UTF8_STRING_MEMBER " = new " <<
                        utf8CType <<
                        "(*src." UTF8_STRING_MEMBER ");\n";
                }
            }
            else {
                methods <<
                    "        " UTF8_STRING_MEMBER " = src." UTF8_STRING_MEMBER ";\n";
            }
            methods <<
                "        break;\n";
        }
        if ( haveObjectPointer ) {
            // generate copy code for string
            ITERATE ( TVariants, i, m_Variants ) {
                if ( i->memberType == eObjectPointerMember ) {
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n";
                }
            }
            methods <<
                "        (" OBJECT_MEMBER " = src." OBJECT_MEMBER ")->AddReference();\n"
                "        break;\n";
        }

        methods <<
            "    default:\n"
            "        break;\n"
            "    }\n"
            "    " STATE_MEMBER " = index;\n"
            "}\n"
            "\n";
    }

    // generate Select method
    {
        methods <<
            "void "<<methodPrefix<<"DoSelect(" STATE_ENUM " index, NCBI_NS_NCBI::CObjectMemoryPool* ";
        if ( haveUnion || haveObjectPointer ) {
            ITERATE ( TVariants, i, m_Variants ) {
                if (!i->attlist && i->memberType == eObjectPointerMember) {
                    methods << "pool";
                    break;
                }
            }
        }
        methods << ")\n{\n";
        if ( haveUnion || haveObjectPointer ) {
            methods <<
                "    switch ( index ) {\n";
            ITERATE ( TVariants, i, m_Variants ) {
                if (i->attlist) {
                    continue;
                }
                switch ( i->memberType ) {
                case eSimpleMember:
                    if (!x_IsNullType(i)) {
                        string init = i->type->GetInitializer();
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n"
                            "        m_"<<i->cName<<" = "<<init<<";\n"
                            "        break;\n";
                    }
                    break;
                case ePointerMember:
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<" = "<<i->type->NewInstance(NcbiEmptyString)<<";\n"
                        "        break;\n";
                    break;
                case eBufferMember:
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<".Construct();\n"
                        "        break;\n";
                    break;
                case eObjectPointerMember:
                    methods <<
                        "    case " STATE_PREFIX<<i->cName<<":\n"
                        "        (" OBJECT_MEMBER " = "<<i->type->NewInstance(NcbiEmptyString, "(pool)")<<")->AddReference();\n"
                        "        break;\n";
                    break;
                case eStringMember:
                case eUtf8StringMember:
                    // will be handled specially
                    break;
                }
            }
            if ( haveString ) {
                ITERATE ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eStringMember ) {
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveBuffer ) {
                    methods <<
                        "        " STRING_MEMBER ".Construct();\n"
                        "        break;\n";
                }
                else {
                    methods <<
                        "        " STRING_MEMBER " = new " STRING_TYPE_FULL ";\n"
                        "        break;\n";
                }
            }
            if ( haveUtf8String ) {
                ITERATE ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eUtf8StringMember ) {
                        methods <<
                            "    case " STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveBuffer ) {
                    methods <<
                        "        " UTF8_STRING_MEMBER ".Construct();\n"
                        "        break;\n";
                }
                else {
                    methods <<
                        "        " UTF8_STRING_MEMBER " = new " <<
                        utf8CType <<
                        ";\n        break;\n";
                }
            }
            methods <<
                "    default:\n"
                "        break;\n"
                "    }\n";
        }
        methods <<
            "    " STATE_MEMBER " = index;\n"
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
        ITERATE ( TVariants, i, m_Variants ) {
            methods << ",\n"
                "    \""<<i->externalName<<"\"";
            if (i->attlist) {
                methods << " /* place holder */";
            }
        }
        methods << "\n"
            "};\n"
            "\n"
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName(" STATE_ENUM " index)\n"
            "{\n"
            "    return NCBI_NS_NCBI::CInvalidChoiceSelection::GetName(index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n"
            "void "<<methodPrefix<<"ThrowInvalidSelection(" STATE_ENUM " index) const\n"
            "{\n"
            "    throw NCBI_NS_NCBI::CInvalidChoiceSelection(DIAG_COMPILE_INFO";
        if ( 1 ) { // add extra argument for better error message
            methods << ", this";
        }
        methods << ", m_choice, index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n";
    }
    
    // generate variant types
    {
        code.ClassPublic() <<
            "    // types\n";
        ITERATE ( TVariants, i, m_Variants ) {
            string cType = i->type->GetCType(code.GetNamespace());
            if (!x_IsNullType(i)) {
                code.ClassPublic() <<
                    "    typedef "<<cType<<" T"<<i->cName<<";\n";
            }
        }
        code.ClassPublic() << 
            "\n";
    }

    // generate variant getters & setters
    {
        code.ClassPublic() <<
            "    // getters\n";
        setters <<
            "    // setters\n\n";
        ITERATE ( TVariants, i, m_Variants ) {
            string cType = i->type->GetCType(code.GetNamespace());
            string tType = "T" + i->cName;
            string tTypeStorage = tType;
            string cTypeStorage = i->type->GetStorageType(code.GetNamespace());
            if (cTypeStorage != cType) {
                tTypeStorage = cTypeStorage;
            }
#if 0
            string rType = i->type->GetPrefixedCType(code.GetNamespace(),methodPrefix);
#else
            //use defined types
            string rType = methodPrefix + tType;
#endif
            CTypeStrings::EKind kind = i->type->GetKind();
            bool isNull = x_IsNullType(i);
            bool isNullWithAtt = x_IsNullWithAttlist(i);

            if (!CClassCode::GetDoxygenComments()) {
                if (!isNull) {
                    code.ClassPublic()
                        << "    // typedef "<< cType <<" "<<tType<<"\n";
                } else {
                    code.ClassPublic() << "\n";
                }
            }
            if (i->attlist) {
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
                        "    /// Reset the attribute list.\n";
                }
                code.ClassPublic() <<
                    "    void Reset"<<i->cName<<"(void);\n";
            } else {
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
                        "\n"
                        "    /// Check if variant "<<i->cName<<" is selected.\n"
                        "    ///\n";
                    if (!isNull) {
                        code.ClassPublic()
                            << "    /// "<<i->cName<<" type is defined as \'typedef "<< cType <<" "<<tType<<"\'.\n";
                    }
                    code.ClassPublic() <<
                        "    /// @return\n"
                        "    ///   - true, if the variant is selected.\n"
                        "    ///   - false, otherwise.\n";
                }
                code.ClassPublic() <<
                    "    bool Is"<<i->cName<<"(void) const;\n";
            }
            if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive())) {
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
                        "\n"
                        "    /// Get the variant data.\n"
                        "    ///\n"
                        "    /// @return\n"
                        "    ///   Copy of the variant data.\n";
                }
                code.ClassPublic() <<
                    "    "<<tType<<" Get"<<i->cName<<"(void) const;\n";
            } else {
                if (!isNull) {
                    if (CClassCode::GetDoxygenComments()) {
                        if (i->attlist) {
                            code.ClassPublic() <<
                                "\n"
                                "    /// Get the attribute list data.\n";
                        } else {
                            code.ClassPublic() <<
                                "\n"
                                "    /// Get the variant data.\n";
                        }
                        code.ClassPublic() <<
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Reference to the data.\n";
                    }
                    code.ClassPublic() <<
                        "    const "<<tType<<"& Get"<<i->cName<<"(void) const;\n";
                }
            }
            if (isNull) {
                if (CClassCode::GetDoxygenComments()) {
                    setters <<
                        "\n"
                        "    /// Select the variant.\n";
                }
                setters <<
                    "    void Set"<<i->cName<<"(void);\n";
            } else {
                if (CClassCode::GetDoxygenComments()) {
                    if (i->attlist) {
                        setters <<
                            "\n"
                            "    /// Set the attribute list data.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Reference to the data.\n";
                    } else {
                        setters <<
                            "\n"
                            "    /// Select the variant.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Reference to the variant data.\n";
                    }
                }
                setters <<
                    "    "<<tType<<"& Set"<<i->cName<<"(void);\n";
            }
            if ( i->type->CanBeCopied() ) {
                if (i->attlist) {
                    if (CClassCode::GetDoxygenComments()) {
                        setters <<
                            "\n"
                            "    /// Set the attribute list data.\n"
                            "    ///\n"
                            "    /// @param value\n"
                            "    ///   Reference to data.\n";
                    }
                    setters <<
                        "    void Set"<<i->cName<<"("<<tType<<"& value);\n";
                } else {
                    if (!isNull) {
                        if (CClassCode::GetDoxygenComments()) {
                            setters <<
                                "\n"
                                "    /// Select the variant and set its data.\n"
                                "    ///\n"
                                "    /// @param value\n"
                                "    ///   Variant data.\n";
                        }
                        if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive())) {
                            setters <<
                                "    void Set"<<i->cName<<"("<<tType<<" value);\n";
                        } else {
                            setters <<
                                "    void Set"<<i->cName<<"(const "<<tType<<"& value);\n";
                        }
                    }
                }
            }
            if ( i->memberType == eObjectPointerMember && !isNullWithAtt) {
                if (CClassCode::GetDoxygenComments()) {
                    if (i->attlist) {
                        setters <<
                            "\n"
                            "    /// Set the attribute list data.\n";
                    } else {
                        setters <<
                            "    /// Select the variant and set its data.\n";
                    }
                    setters <<
                        "    ///\n"
                        "    /// @param value\n"
                        "    ///   Reference to the data.\n";
                }
                setters <<
                    "    void Set"<<i->cName<<"("<<tType<<"& value);\n";
            }
            string memberRef;
            string constMemberRef;
            bool inl = true;
            switch ( i->memberType ) {
            case eSimpleMember:
                memberRef = constMemberRef = "m_"+i->cName;
                if (tTypeStorage != tType) {
                    constMemberRef = "reinterpret_cast<const " + tType + "&>(m_" + i->cName + ")";
                    memberRef      = "reinterpret_cast<"       + tType + "&>(m_" + i->cName + ")";
                }
                break;
            case ePointerMember:
            case eBufferMember:
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
            case eUtf8StringMember:
                memberRef = UTF8_STRING_MEMBER;
                if ( haveUnion ) {
                    // string is pointer
                    memberRef = '*'+memberRef;
                }
                constMemberRef = memberRef;
                break;
            case eObjectPointerMember:
                memberRef = "*static_cast<T"+i->cName+"*>(" OBJECT_MEMBER ")";
                constMemberRef = "*static_cast<const T"+i->cName+"*>(" OBJECT_MEMBER ")";
                inl = false;
                break;
            }
            if ( i->delayed )
                inl = false;
            if (i->attlist) {
                code.MethodStart(inl) <<
                    "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                    "{\n"
                    "    (*m_" <<i->cName<< ").Reset();\n"
                    "}\n"
                    "\n";
                if (i->dataType && i->dataType->IsPrimitive()) {
                    code.MethodStart(inl) << rType;
                } else {
                    code.MethodStart(inl) << "const "<<rType<<"&";
                }
                code.Methods(inl) <<
                    " "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                    "{\n";
                code.Methods(inl) <<
                    "    return (*m_"<<i->cName<<");\n"
                    "}\n"
                    "\n";
                code.MethodStart(inl) <<
                    rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                    "{\n";
                code.Methods(inl) <<
                    "    return (*m_"<<i->cName<<");\n"
                    "}\n"
                    "\n";
                code.MethodStart(inl) <<
                    "void "<<methodPrefix<<"Set"<<i->cName<<"("<<rType<<"& value)\n"
                    "{\n";
                code.Methods(inl) <<
                    "    m_"<<i->cName<<".Reset(&value);\n"
                    "}\n"
                    "\n";
            } else {
                inlineMethods <<
                    "inline\n"
                    "bool "<<methodPrefix<<"Is"<<i->cName<<"(void) const\n"
                    "{\n"
                    "    return " STATE_MEMBER " == " STATE_PREFIX<<i->cName<<";\n"
                    "}\n"
                    "\n";
                if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive())) {
                    code.MethodStart(inl) << rType;
                } else if (!isNull) {
                    code.MethodStart(inl) << "const "<<rType<<"&";
                }
                if (!isNull) {
                    code.Methods(inl) <<
                        " "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                        "{\n"
                        "    CheckSelected(" STATE_PREFIX<<i->cName<<");\n";
                    if ( i->delayed ) {
                        code.Methods(inl) <<
                            "    " DELAY_MEMBER ".Update();\n";
                    }
                    code.Methods(inl) <<
                        "    return "<<constMemberRef<<";\n"
                        "}\n"
                        "\n";
                }
                if (isNull) {
                    code.MethodStart(inl) <<
                        "void "<<methodPrefix<<"Set"<<i->cName<<"(void)\n";
                } else {
                    code.MethodStart(inl) <<
                        rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n";
                }
                code.Methods(inl) <<
                    "{\n"
                    "    Select(" STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
                if (!isNull) {
                    if ( i->delayed ) {
                        code.Methods(inl) <<
                            "    " DELAY_MEMBER ".Update();\n";
                    }
                    if (isNullWithAtt) {
                        code.Methods(inl) <<
                            "    "<<rType<<"& value = "<<memberRef<<";\n" <<
                            "    value.Set"<<i->cName<<"();\n" <<
                            "    return value;\n";
                    } else {
                        code.Methods(inl) <<
                            "    return "<<memberRef<<";\n";
                    }
                }
                code.Methods(inl) <<
                    "}\n"
                    "\n";
                if ( i->type->CanBeCopied() ) {
                    bool set_inl = (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive()));
                    if (!isNull) {
                        code.MethodStart(set_inl) <<
                            "void "<<methodPrefix<<"Set"<<i->cName<<"(";
                        if (set_inl) {
                            code.Methods(set_inl) << rType;
                        } else {
                            code.Methods(set_inl) << "const " << rType << "&";
                        }
                        code.Methods(set_inl) << " value)\n"
                            "{\n"
                            "    Select(" STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
                        if ( i->delayed ) {
                            code.Methods(set_inl) <<
                                "    " DELAY_MEMBER ".Forget();\n";
                        }
                        code.Methods(set_inl) <<
                            "    "<<memberRef<<" = value;\n"
                            "}\n"
                            "\n";
                    }
                }
                if ( i->memberType == eObjectPointerMember  && !isNullWithAtt) {
                    methods <<
                        "void "<<methodPrefix<<"Set"<<i->cName<<"("<<rType<<"& value)\n"
                        "{\n"
                        "    T"<<i->cName<<"* ptr = &value;\n";
                    if ( i->delayed ) {
                        methods <<
                            "    if ( " STATE_MEMBER " != " STATE_PREFIX<<i->cName<<" || " DELAY_MEMBER " || " OBJECT_MEMBER " != ptr ) {\n";
                    }
                    else {
                        methods <<
                            "    if ( " STATE_MEMBER " != " STATE_PREFIX<<i->cName<<" || " OBJECT_MEMBER " != ptr ) {\n";
                    }
                    methods <<
                        "        ResetSelection();\n"
                        "        (" OBJECT_MEMBER " = ptr)->AddReference();\n"
                        "        " STATE_MEMBER " = " STATE_PREFIX<<i->cName<<";\n"
                        "    }\n"
                        "}\n"
                        "\n";
                    if (i->dataType) {
                        const CDataType* resolved = i->dataType->Resolve();
                        if (resolved && resolved != i->dataType) {
                            CClassTypeStrings* typeStr = resolved->GetTypeStr();
                            if (typeStr) {
                                ITERATE ( TMembers, ir, typeStr->m_Members ) {
                                    if (ir->simple) {
                                        string ircType(ir->type->GetCType(
                                            code.GetNamespace()));
                                        if (CClassCode::GetDoxygenComments()) {
                                            setters <<
                                                "\n"
                                                "    /// Select the variant and set its data.\n"
                                                "    ///\n"
                                                "    /// @param value\n"
                                                "    ///   Reference to variant data.\n";
                                        }
                                        setters <<
                                            "    void Set"<<i->cName<<"(const "<<
                                            ircType<<"& value);\n";
                                        methods <<
                                            "void "<<methodPrefix<<"Set"<<
                                            i->cName<<"(const "<<ircType<<
                                            "& value)\n"
                                            "{\n";
                                        methods <<
                                            "    Set" << i->cName <<
                                            "() = value;\n"
                                            "}\n"
                                            "\n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
            setters <<
                "\n";
        }
    }

    // generate variants data
    {
        code.ClassPrivate() <<
            "    // data\n";
        if (haveAttlist) {
            ITERATE ( TVariants, i, m_Variants ) {
                if (i->attlist) {
                    code.ClassPrivate() <<
                        "    "<<ncbiNamespace<<"CRef< T"<<i->cName<<" > m_"<<i->cName<<";\n";
                }
            }
        }
        if ( haveUnion ) {
            code.ClassPrivate() << "    union {\n";
            ITERATE ( TVariants, i, m_Variants ) {
                if ( i->memberType == eSimpleMember ) {
                    if (!x_IsNullType(i)) {
                        string tType = "T" + i->cName;
                        string cTypeStorage = i->type->GetStorageType(code.GetNamespace());
                        if (cTypeStorage != i->type->GetCType(code.GetNamespace())) {
                            tType = cTypeStorage;
                        }
                        code.ClassPrivate() <<
                            "        "<<tType<<" m_"<<i->cName<<";\n";
                    }
                }
                else if ( i->memberType == ePointerMember ) {
                    code.ClassPrivate() <<
                        "        T"<<i->cName<<" *m_"<<i->cName<<";\n";
                }
                else if ( i->memberType == eBufferMember ) {
                    code.ClassPrivate() <<
                        "        NCBI_NS_NCBI::CUnionBuffer<T"<<i->cName<<"> m_"<<i->cName<<";\n";
                }
            }
            if ( haveString ) {
                if ( haveBuffer ) {
                    code.ClassPrivate() <<
                        "        NCBI_NS_NCBI::CUnionBuffer<" STRING_TYPE_FULL "> " STRING_MEMBER ";\n";
                }
                else {
                    code.ClassPrivate() <<
                        "        " STRING_TYPE_FULL " *" STRING_MEMBER ";\n";
                }
            }
            if ( haveUtf8String ) {
                if ( haveBuffer ) {
                    code.ClassPrivate() <<
                        "        NCBI_NS_NCBI::CUnionBuffer<" <<
                        utf8CType <<
                        "> " UTF8_STRING_MEMBER ";\n";
                }
                else {
                    code.ClassPrivate() <<
                        "        " <<
                        utf8CType <<
                        " *" UTF8_STRING_MEMBER ";\n";
                }
            }
            if ( haveObjectPointer ) {
                code.ClassPrivate() <<
                    "        " OBJECT_TYPE_FULL " *" OBJECT_MEMBER ";\n";
            }
            if ( haveBuffer && !havePointers && !haveObjectPointer ) {
                // we should add some union member to force alignment
                // any pointer seems enough for this
                code.ClassPrivate() <<
                    "        void* m_dummy_pointer_for_alignment;\n";
            }
            code.ClassPrivate() <<
                "    };\n";
        }
        else if ( haveString || haveUtf8String ) {
            if (haveString) {
                code.ClassPrivate() <<
                    "    " STRING_TYPE_FULL " " STRING_MEMBER ";\n";
            }
            if (haveUtf8String) {
                code.ClassPrivate() <<
                    "    " <<
                    utf8CType <<
                    " " UTF8_STRING_MEMBER ";\n";
            }
        }
        else if ( haveObjectPointer ) {
            code.ClassPrivate() <<
                "    " OBJECT_TYPE_FULL " *" OBJECT_MEMBER ";\n";
        }
        if ( delayed ) {
            code.ClassPrivate() <<
                "    mutable " DELAY_TYPE_FULL " " DELAY_MEMBER ";\n";
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
        "(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassNameDT()<<")\n"
        "{\n";

    SInternalNames names;
    string module_name = GetModuleName(&names);
    if ( GetExternalName().empty() && !names.m_OwnerName.empty() ) {
        methods <<
            "    SET_INTERNAL_NAME(\""<<names.m_OwnerName<<"\", ";
        if ( !names.m_MemberName.empty() )
            methods << "\""<<names.m_MemberName<<"\"";
        else
            methods << "0";
        methods << ");\n";
    }
    if ( !module_name.empty() ) {
        methods <<
            "    SET_CHOICE_MODULE(\""<<module_name<<"\");\n";
    }
    const CDataType* dataType = DataType();
    if (dataType) {
        if (dataType->HasTag()) {
            methods <<
                "    SET_ASN_TAGGED_TYPE_INFO(" <<"SetTag, (" <<  dataType->GetTag() <<',' << 
                dataType->GetTagClassString(dataType->GetTagClass()) << ',' <<
                dataType->GetTagTypeString(dataType->GetTagType()) <<"));\n";
        } else if (dataType->GetTagType() != CAsnBinaryDefs::eAutomatic) {
            methods <<
                "    SET_ASN_TAGGED_TYPE_INFO(" <<"SetTagType, (" <<
                dataType->GetTagTypeString(dataType->GetTagType()) <<"));\n";
        }
    }

    ENsQualifiedMode defNsqMode = eNSQNotSet;
    if (DataType()) {
        defNsqMode = DataType()->IsNsQualified();
        if (defNsqMode == eNSQNotSet) {
            const CDataMember *dm = DataType()->GetDataMember();
            if (dm && dm->Attlist()) {
                defNsqMode = eNSUnqualified;
            }
        }
    }
    if ( !GetNamespaceName().empty() ) {
        methods <<
            "    SET_NAMESPACE(\""<<GetNamespaceName()<<"\")";
        if (defNsqMode != eNSQNotSet) {
            methods << "->SetNsQualified(";
            if (defNsqMode == eNSQualified) {
                methods << "true";
            } else {
                methods << "false";
            }
            methods << ")";
        }
        methods << ";\n";
    }
    if ( delayed ) {
        methods <<
            "    SET_CHOICE_DELAYED();\n";
    }
    {
        ITERATE ( TVariants, i, m_Variants ) {
            methods << "    ADD_NAMED_";
            bool isNull = x_IsNullType(i);
            if (isNull) {
                methods << "NULL_";
            }
            
            bool addNamespace = false;
            bool addCType = false;
            bool addEnum = false;
            bool addRef = false;

            switch ( i->memberType ) {
            case ePointerMember:
                methods << "PTR_";
                addRef = true;
                break;
            case eBufferMember:
                methods << "BUF_";
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
            case eUtf8StringMember:
                if ( haveUnion ) {
                    if ( haveBuffer ) {
                        methods << "BUF_";
                        if (i->memberType == eUtf8StringMember) {
                            addRef = true;
                        }
                    }
                    else {
                        methods << "PTR_";
                    }
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

            if (i->attlist) {
                methods << "MEMBER(\"";
            } else {
                methods << "CHOICE_VARIANT(\"";
            }
            methods <<i->externalName<<"\"";
            if (!isNull) {
                methods <<", ";
            }
            switch ( i->memberType ) {
            case eObjectPointerMember:
                if (i->attlist) {
                    methods << "m_" << i->cName;
                } else {
                    methods << OBJECT_MEMBER;
                }
                break;
            case eStringMember:
                methods << STRING_MEMBER;
                break;
            case eUtf8StringMember:
                methods << UTF8_STRING_MEMBER;
                break;
            default:
                if (!isNull) {
                    methods << "m_"<<i->cName;
                }
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
            methods <<")";
            
            if ( i->delayed ) {
                methods << "->SetDelayBuffer(MEMBER_PTR(m_delayBuffer))";
            }
            if (i->dataType && i->dataType->GetDataMember() && i->dataType->GetDataMember()->Optional()) {
                methods << "->SetOptional()";
            }
            if (i->noPrefix) {
                methods << "->SetNoPrefix()";
            }
            if (i->attlist) {
                methods << "->SetAttlist()";
            }
            if (i->noTag) {
                methods << "->SetNotag()";
            }
            if ( i->memberTag >= 0 ) {
                methods << "->GetId().SetTag(" << i->memberTag;
                if (i->dataType && 
                    (i->dataType->GetTagClass() != CAsnBinaryDefs::eContextSpecific ||
                     i->dataType->GetTagType()  == CAsnBinaryDefs::eImplicit)) {
                    methods << ',' << CDataType::GetTagClassString(i->dataType->GetTagClass())
                            << ',' << CDataType::GetTagTypeString(i->dataType->GetTagType());
                }
                methods << ")";
            }
            if (i->dataType) {
                const COctetStringDataType* octets =
                    dynamic_cast<const COctetStringDataType*>(i->dataType);
                if (octets) {
                    if (octets->IsCompressed()) {
                        methods << "->SetCompressed()";
                    }
                }
                const CUniSequenceDataType* uniseq =
                    dynamic_cast<const CUniSequenceDataType*>(i->dataType);
                if (uniseq && uniseq->IsNonEmpty()) {
                    methods << "->SetNonEmpty()";
                }
                ENsQualifiedMode memNsqMode = i->dataType->IsNsQualified();
                if (memNsqMode != eNSQNotSet && memNsqMode != defNsqMode) {
                    methods << "->SetNsQualified(";
                    if (memNsqMode == eNSQualified) {
                        methods << "true";
                    } else {
                        methods << "false";
                    }
                    methods << ")";
                }
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
                                             const string& fileName,
                                             const CComments& comments)
    : CParent(className, ns, fileName, comments)
{
}

END_NCBI_SCOPE
