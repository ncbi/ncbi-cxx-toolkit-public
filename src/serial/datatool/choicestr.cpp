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
* Revision 1.53  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.52  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.51  2004/07/21 13:29:59  gouriano
* Set and return primitive type data by value
*
* Revision 1.50  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.49  2004/05/03 19:31:03  gouriano
* Made generation of DOXYGEN-style comments optional
*
* Revision 1.48  2004/04/29 20:11:40  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.47  2003/11/20 14:32:40  gouriano
* changed generated C++ code so NULL data types have no value
*
* Revision 1.46  2003/10/21 13:48:51  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.45  2003/10/06 18:40:15  grichenk
* Added e_MaxChoice enum
*
* Revision 1.44  2003/05/08 16:59:08  gouriano
* added comment about the meaning of typedef for each class member
*
* Revision 1.43  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.42  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.41  2003/03/11 17:59:16  gouriano
* reimplement CInvalidChoiceSelection exception
*
* Revision 1.40  2003/03/10 21:21:10  gouriano
* use CSerialException in generated code
*
* Revision 1.39  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.38  2003/02/12 21:39:52  gouriano
* corrected code generator so primitive data types (bool,int,etc)
* are returned by value, not by reference
*
* Revision 1.37  2002/11/19 19:48:28  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.36  2002/11/18 19:48:46  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.35  2002/11/14 21:03:40  gouriano
* added support of XML attribute lists
*
* Revision 1.34  2002/10/15 13:58:04  gouriano
* use "noprefix" flag
*
* Revision 1.33  2002/08/14 17:14:25  grichenk
* Fixed function name conflict on Win32: renamed
* GetClassName() -> GetClassNameDT()
*
* Revision 1.32  2002/07/25 15:02:41  grichenk
* Removed non-const GetXXX() method, use SetXXX() instead
*
* Revision 1.31  2002/05/15 20:22:04  grichenk
* Added CSerialObject -- base class for all generated ASN.1 classes
*
* Revision 1.30  2002/01/16 18:56:34  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.29  2001/06/21 19:47:39  grichenk
* Copy constructor and operator=() moved to "private" section
*
* Revision 1.28  2001/06/13 14:40:02  grichenk
* Modified class and choice info generation code to avoid warnings
*
* Revision 1.27  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.26  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.25  2000/11/29 17:42:42  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.24  2000/11/07 17:26:24  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.23  2000/08/28 13:22:03  vasilche
* Fixed reference to undefined kEmptyChoice.
*
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

#include <ncbi_pch.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/choicestr.hpp>
#include <serial/datatool/stdstr.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

#define STATE_ENUM "E_Choice"
#define STATE_MEMBER "m_choice"
#define STRING_TYPE_FULL "NCBI_NS_STD::string"
#define STRING_TYPE "string"
#define STRING_MEMBER "m_string"
#define OBJECT_TYPE_FULL "NCBI_NS_NCBI::CSerialObject"
#define OBJECT_TYPE "CSerialObject"
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
                                    bool delayed, bool in_union, int tag,
                                    bool noPrefix, bool attlist, bool noTag,
                                    bool simple, const CDataType* dataType)
{
    m_Variants.push_back(SVariantInfo(name, type, delayed, in_union, tag,
                                      noPrefix,attlist,noTag,simple,dataType));
}

CChoiceTypeStrings::SVariantInfo::SVariantInfo(const string& name,
                                               const AutoPtr<CTypeStrings>& t,
                                               bool del, bool in_un, int tag,
                                               bool noPrefx, bool attlst,
                                               bool noTg,bool simpl,
                                               const CDataType* dataTp)
    : externalName(name), cName(Identifier(name)),
      type(t), delayed(del), in_union(in_un), memberTag(tag),
      noPrefix(noPrefx), attlist(attlst), noTag(noTg), simple(simpl),
      dataType(dataTp)
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
    bool haveString = false;
    bool delayed = false;
    bool haveAttlist = false;
    bool haveBuffer = false;
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
            }
            if ( i->delayed )
                delayed = true;
        }
    }
    if ( delayed )
        code.HPPIncludes().insert("serial/delaybuf");

    bool haveUnion = havePointers || haveSimple || haveBuffer ||
        (haveString && haveObjectPointer);
    if ( haveString && haveUnion && !haveBuffer ) {
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
            "    enum "STATE_ENUM" {\n"
            "        "STATE_NOT_SET" = "<<kEmptyChoice
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
                cName = i->cName;
                code.ClassPublic() << "        "STATE_PREFIX<<cName;
                if (needIni) {
                    code.ClassPublic() << " = "<<currIndex;
                    needIni = false;
                }
                ++i;
                if (i != m_Variants.end()) {
                    code.ClassPublic() << ",";
                } else {
                    code.ClassPublic() << " ";
                }
                for (currlen = cName.size(); currlen < maxlen; ++currlen) {
                    code.ClassPublic() << " ";
                }
                code.ClassPublic() << "  ///< Variant "<<cName<<" is selected.";
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
            "        e_MaxChoice = " << currIndex+1 << " ///< == "STATE_PREFIX
                << m_Variants.rbegin()->cName << "+1\n"
            "    };\n"
            "\n";
    }

    code.ClassPublic() <<
        "    /// Reset the selection (set it to "STATE_NOT_SET").\n"
        "    ";
    if ( HaveUserClass() )
        code.ClassPublic() << "virtual ";
    code.ClassPublic() <<
        "void Reset(void);\n"
        "\n";

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
        "    "STATE_ENUM" Which(void) const;\n\n"
        "    /// Verify selection, throw exception if it differs from the expected.\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   Expected selection.\n";
    }
    code.ClassPublic() <<
        "    void CheckSelected("STATE_ENUM" index) const;\n\n"
        "    /// Throw \'InvalidSelection\' exception.\n";
    if (CClassCode::GetDoxygenComments()) {
        code.ClassPublic() <<
            "    ///\n"
            "    /// @param index\n"
            "    ///   Expected selection.\n";
    }
    code.ClassPublic() <<
        "    void ThrowInvalidSelection("STATE_ENUM" index) const;\n\n"
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
        "    static "<<stdNamespace<<"string SelectionName("STATE_ENUM" index);\n"
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
        "    void Select("STATE_ENUM" index, "<<ncbiNamespace<<"EResetVariant reset = "<<ncbiNamespace<<"eDoResetVariant);\n";
    setters <<
        "    /// Select the requested variant if needed,\n"
        "    /// allocating CObject variants from memory pool.\n";
    setters <<
        "    void Select("STATE_ENUM" index,\n"
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
        "void "<<methodPrefix<<"Select("STATE_ENUM" index, NCBI_NS_NCBI::EResetVariant reset, NCBI_NS_NCBI::CObjectMemoryPool* pool)\n"
        "{\n"
        "    if ( reset == NCBI_NS_NCBI::eDoResetVariant || "STATE_MEMBER" != index ) {\n"
        "        if ( "STATE_MEMBER" != "STATE_NOT_SET" )\n"
        "            Reset();\n"
        "        DoSelect(index, pool);\n"
        "    }\n"
        "}\n"
        "\n"
        "inline\n"
        "void "<<methodPrefix<<"Select("STATE_ENUM" index, NCBI_NS_NCBI::EResetVariant reset)\n"
        "{\n"
        "    Select(index, reset, 0);\n"
        "}\n"
        "\n";
    if ( delayed ) {
        inlineMethods <<
            "inline\n"
            "void "<<methodPrefix<<"SelectDelayBuffer("STATE_ENUM" index)\n"
            "{\n"
            "    if ( "STATE_MEMBER" != "STATE_NOT_SET" || "DELAY_MEMBER".GetIndex() != (index - 1))\n"
            "        NCBI_THROW(ncbi::CSerialException,eIllegalCall, \"illegal call\");\n"
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
        "    void DoSelect("STATE_ENUM" index, "<<ncbiNamespace<<"CObjectMemoryPool* pool = 0);\n";
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
    code.AddDestructionCode("if ( "STATE_MEMBER" != "STATE_NOT_SET" )\n"
                            "    Reset();");

    // generate Reset method
    {
        methods <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        if (haveAttlist) {
            ITERATE ( TVariants, i, m_Variants ) {
                if (i->attlist) {
                    methods <<
                        "    Reset" << i->cName << "();\n";
                }
            }
        }
        if ( haveObjectPointer || havePointers || haveString || haveBuffer ) {
            if ( delayed ) {
                methods <<
                    "    if ( "DELAY_MEMBER" )\n"
                    "        "DELAY_MEMBER".Forget();\n"
                    "    else\n";
            }
            methods <<
                "    switch ( "STATE_MEMBER" ) {\n";
            // generate destruction code for pointers
            ITERATE ( TVariants, i, m_Variants ) {
                if (i->attlist) {
                    continue;
                }
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
                if ( i->memberType == eBufferMember ) {
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n";
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
                            "    case "STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveUnion ) {
                    // string is pointer inside union
                    if ( haveBuffer ) {
                        methods <<
                            "        "STRING_MEMBER".Destruct();\n";
                    }
                    else {
                        methods <<
                            "        delete "STRING_MEMBER";\n";
                    }
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
                ITERATE ( TVariants, i, m_Variants ) {
                    if (i->attlist) {
                        continue;
                    }
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
        ITERATE ( TVariants, i, m_Variants ) {
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
            case eBufferMember:
                methods <<
                    "    case "STATE_PREFIX<<i->cName<<":\n"
                    "        m_"<<i->cName<<".Construct();\n"
                    "        *m_"<<i->cName<<" = *src.m_"<<i->cName<<";\n"
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
            ITERATE ( TVariants, i, m_Variants ) {
                if ( i->memberType == eStringMember ) {
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n";
                }
            }
            if ( haveUnion ) {
                if ( haveBuffer ) {
                    methods <<
                        "        "STRING_MEMBER".Construct();\n"
                        "        *"STRING_MEMBER" = *src."STRING_MEMBER";\n";
                }
                else {
                    // string is pointer
                    methods <<
                        "        "STRING_MEMBER" = new "STRING_TYPE_FULL"(*src."STRING_MEMBER");\n";
                }
            }
            else {
                methods <<
                    "        "STRING_MEMBER" = src."STRING_MEMBER";\n";
            }
            methods <<
                "        break;\n";
        }
        if ( haveObjectPointer ) {
            // generate copy code for string
            ITERATE ( TVariants, i, m_Variants ) {
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
            "void "<<methodPrefix<<"DoSelect("STATE_ENUM" index, NCBI_NS_NCBI::CObjectMemoryPool* pool)\n"
            "{\n";
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
                case eBufferMember:
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n"
                        "        m_"<<i->cName<<".Construct();\n"
                        "        break;\n";
                    break;
                case eObjectPointerMember:
                    methods <<
                        "    case "STATE_PREFIX<<i->cName<<":\n"
                        "        ("OBJECT_MEMBER" = "<<i->type->NewInstance(NcbiEmptyString, "(pool)")<<")->AddReference();\n"
                        "        break;\n";
                    break;
                case eStringMember:
                    // will be handled specially
                    break;
                }
            }
            if ( haveString ) {
                ITERATE ( TVariants, i, m_Variants ) {
                    if ( i->memberType == eStringMember ) {
                        methods <<
                            "    case "STATE_PREFIX<<i->cName<<":\n";
                    }
                }
                if ( haveBuffer ) {
                    methods <<
                        "        "STRING_MEMBER".Construct();\n"
                        "        break;\n";
                }
                else {
                    methods <<
                        "        "STRING_MEMBER" = new "STRING_TYPE_FULL";\n"
                        "        break;\n";
                }
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
            "NCBI_NS_STD::string "<<methodPrefix<<"SelectionName("STATE_ENUM" index)\n"
            "{\n"
            "    return NCBI_NS_NCBI::CInvalidChoiceSelection::GetName(index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
            "}\n"
            "\n"
            "void "<<methodPrefix<<"ThrowInvalidSelection("STATE_ENUM" index) const\n"
            "{\n"
            "    throw NCBI_NS_NCBI::CInvalidChoiceSelection(DIAG_COMPILE_INFO, m_choice, index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));\n"
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
            string rType = i->type->GetPrefixedCType(code.GetNamespace(),methodPrefix);
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
            case eObjectPointerMember:
                memberRef = "*static_cast<T"+i->cName+"*>("OBJECT_MEMBER")";
                constMemberRef = "*static_cast<const T"+i->cName+"*>("OBJECT_MEMBER")";
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
                    "    return "STATE_MEMBER" == "STATE_PREFIX<<i->cName<<";\n"
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
                        "    CheckSelected("STATE_PREFIX<<i->cName<<");\n";
                    if ( i->delayed ) {
                        code.Methods(inl) <<
                            "    "DELAY_MEMBER".Update();\n";
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
                    "    Select("STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
                if (!isNull) {
                    if ( i->delayed ) {
                        code.Methods(inl) <<
                            "    "DELAY_MEMBER".Update();\n";
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
                            "    Select("STATE_PREFIX<<i->cName<<", NCBI_NS_NCBI::eDoNotResetVariant);\n";
                        if ( i->delayed ) {
                            code.Methods(set_inl) <<
                                "    "DELAY_MEMBER".Forget();\n";
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
                        code.ClassPrivate() <<
                            "        T"<<i->cName<<" m_"<<i->cName<<";\n";
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
                        "        NCBI_NS_NCBI::CUnionBuffer<"STRING_TYPE_FULL"> "STRING_MEMBER";\n";
                }
                else {
                    code.ClassPrivate() <<
                        "        "STRING_TYPE_FULL" *"STRING_MEMBER";\n";
                }
            }
            if ( haveObjectPointer ) {
                code.ClassPrivate() <<
                    "        "OBJECT_TYPE_FULL" *"OBJECT_MEMBER";\n";
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
        "(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassNameDT()<<")\n"
        "{\n";
    if ( !GetModuleName().empty() ) {
        methods <<
            "    SET_CHOICE_MODULE(\""<<GetModuleName()<<"\");\n";
    }
    if ( delayed ) {
        methods <<
            "    SET_CHOICE_DELAYED();\n";
    }
    {
        // All or none of the choices must be tagged
        bool useTags = false;
        bool hasUntagged = false;
        // All tags must be different
        map<int, bool> tag_map;

        ITERATE ( TVariants, i, m_Variants ) {
            // Save member info
            if ( i->memberTag >= 0 ) {
                if ( hasUntagged ) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        "No explicit tag for some members in " +
                        GetModuleName());
                }
                if ( tag_map[i->memberTag] ) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        "Duplicate tag: " + i->cName +
                        " [" + NStr::IntToString(i->memberTag) + "] in " +
                        GetModuleName());
                }
                tag_map[i->memberTag] = true;
                useTags = true;
            }
            else {
                hasUntagged = true;
                if ( useTags ) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        "No explicit tag for " + i->cName + " in " +
                        GetModuleName());
                }
            }

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
                if ( haveUnion ) {
                    if ( haveBuffer ) {
                        methods << "BUF_";
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
                methods << "->GetId().SetTag(" << i->memberTag << ")";
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
