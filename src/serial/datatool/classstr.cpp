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
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/classstr.hpp>
#include <serial/datatool/stdstr.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/datatool/comments.hpp>

BEGIN_NCBI_SCOPE

#define SET_PREFIX "m_set_State"
#define DELAY_PREFIX "m_delay_"

CClassTypeStrings::CClassTypeStrings(const string& externalName,
                                     const string& className)
    : m_IsObject(true), m_HaveUserClass(true), m_HaveTypeInfo(true),
      m_ExternalName(externalName), m_ClassName(className)
{
}

CClassTypeStrings::~CClassTypeStrings(void)
{
}

CTypeStrings::EKind CClassTypeStrings::GetKind(void) const
{
    return m_IsObject? eKindObject: eKindClass;
}

bool CClassTypeStrings::x_IsNullType(TMembers::const_iterator i) const
{
    return i->haveFlag ?
        (dynamic_cast<CNullTypeStrings*>(i->type.get()) != 0) : false;
}

bool CClassTypeStrings::x_IsNullWithAttlist(TMembers::const_iterator i) const
{
    if (i->ref && i->dataType) {
        const CDataType* resolved = i->dataType->Resolve();
        if (resolved && resolved != i->dataType) {
            CClassTypeStrings* typeStr = resolved->GetTypeStr();
            if (typeStr) {
                ITERATE ( TMembers, ir, typeStr->m_Members ) {
                    if (ir->simple) {
                        return x_IsNullType(ir);
                    }
                }
            }
        }
    }
    return false;
}


void CClassTypeStrings::AddMember(const string& name,
                                  const AutoPtr<CTypeStrings>& type,
                                  const string& pointerType,
                                  bool optional,
                                  const string& defaultValue,
                                  bool delayed, int tag,
                                  bool noPrefix, bool attlist, bool noTag,
                                  bool simple,const CDataType* dataType,
                                  bool nonempty)
{
    m_Members.push_back(SMemberInfo(name, type,
                                    pointerType,
                                    optional, defaultValue,
                                    delayed, tag, noPrefix,attlist,noTag,
                                    simple,dataType,nonempty));
}

CClassTypeStrings::SMemberInfo::SMemberInfo(const string& name,
                                            const AutoPtr<CTypeStrings>& t,
                                            const string& pType,
                                            bool opt, const string& defValue,
                                            bool del, int tag, bool noPrefx,
                                            bool attlst, bool noTg, bool simpl,
                                            const CDataType* dataTp, bool nEmpty)
    : externalName(name), cName(Identifier(name)),
      mName("m_"+cName), tName('T'+cName),
      type(t), ptrType(pType),
      optional(opt), delayed(del), memberTag(tag),
      defaultValue(defValue), noPrefix(noPrefx), attlist(attlst), noTag(noTg),
      simple(simpl),dataType(dataTp),nonEmpty(nEmpty)
{
    if ( cName.empty() ) {
        mName = "m_data";
        tName = "Tdata";
    }

    bool haveDefault = !defaultValue.empty();
    if ( haveDefault  &&  type ) {
        // Some aliased types need to cast default value
        defaultValue = type->GetDefaultCode(defaultValue);
    }

//    if ( optional && !haveDefault ) {
//    }
    // true [optional] CObject type should be implemented as CRef
    if ( ptrType.empty() ) {
        if ( type->GetKind() == eKindObject )
            ptrType = "Ref";
        else
            ptrType = "false";
    }

    if ( ptrType == "Ref" ) {
        ref = true;
    }
    else if ( /*ptrType.empty()  ||*/ ptrType == "false" ) {
        ref = false;
    }
    else {
        _ASSERT("Unknown reference type: "+ref);
    }
    
    if ( ref ) {
        valueName = "(*"+mName+")";
        haveFlag = false;
    }
    else {
        valueName = mName;
        haveFlag = /*optional &&*/ type->NeedSetFlag();
    }
    if ( haveDefault ) // cannot detect DEFAULT value
        haveFlag = true;
    
    canBeNull = ref && optional && !haveFlag;
}

string CClassTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return GetClassNameDT();
}

string CClassTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                           const string& methodPrefix) const
{
    string s;
    if (!HaveUserClass()) {
        s += methodPrefix;
    }
    return s + GetCType(ns);
}

string CClassTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "CLASS, ("+GetClassNameDT()+')';
}

string CClassTypeStrings::NewInstance(const string& init) const
{
    return "new "+GetCType(CNamespace::KEmptyNamespace)+"("+init+')';
}

string CClassTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

void CClassTypeStrings::SetParentClass(const string& className,
                                       const CNamespace& ns,
                                       const string& fileName)
{
    m_ParentClassName = className;
    m_ParentClassNamespace = ns;
    m_ParentClassFileName = fileName;
}

static
CNcbiOstream& DeclareConstructor(CNcbiOstream& out, const string className)
{
    return out <<
        "    // constructor\n"
        "    "<<className<<"(void);\n";
}

static
CNcbiOstream& DeclareDestructor(CNcbiOstream& out, const string className,
                                bool virt)
{
    out <<
        "    // destructor\n"
        "    ";
    if ( virt )
        out << "virtual ";
    return out << '~'<<className<<"(void);\n"
        "\n";
}

void CClassTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    bool haveUserClass = HaveUserClass();
    string codeClassName = GetClassNameDT();
    if ( haveUserClass )
        codeClassName += "_Base";
    CClassCode code(ctx, codeClassName);
    if ( !m_ParentClassName.empty() ) {
        code.SetParentClass(m_ParentClassName, m_ParentClassNamespace);
        if ( !m_ParentClassFileName.empty() )
            code.HPPIncludes().insert(m_ParentClassFileName);
    }
    else if ( GetKind() == eKindObject ) {
        code.SetParentClass("CSerialObject", CNamespace::KNCBINamespace);
    }
    string methodPrefix = code.GetMethodPrefix();

    DeclareConstructor(code.ClassPublic(), codeClassName);
    DeclareDestructor(code.ClassPublic(), codeClassName, haveUserClass);

    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    if (HaveTypeInfo()) {
        code.ClassPublic() <<
            "    // type info\n"
            "    DECLARE_INTERNAL_TYPE_INFO();\n"
            "\n";
    }

    GenerateClassCode(code,
                      code.ClassPublic(),
                      methodPrefix, haveUserClass, ctx.GetMethodPrefix());

    // constructors/destructor code
    code.Methods() <<
        "// constructor\n"<<
        methodPrefix<<codeClassName<<"(void)\n";
    if ( code.HaveInitializers() ) {
        code.Methods() <<
            "    : ";
        code.WriteInitializers(code.Methods());
        code.Methods() << '\n';
    }

    code.Methods() <<
        "{\n";
    if (typeid(*this) == typeid(CClassTypeStrings)) {
        code.Methods() <<
            "    memset("SET_PREFIX",0,sizeof("SET_PREFIX"));\n";
    }
    code.Methods() <<
        "}\n"
        "\n";

    code.Methods() <<
        "// destructor\n"<<
        methodPrefix<<"~"<<codeClassName<<"(void)\n"
        "{\n";
    code.WriteDestructionCode(code.Methods());
    code.Methods() <<
        "}\n"
        "\n";
}

void CClassTypeStrings::GenerateClassCode(CClassCode& code,
                                          CNcbiOstream& setters,
                                          const string& methodPrefix,
                                          bool haveUserClass,
                                          const string& classPrefix) const
{
    bool delayed = false;
    bool generateDoNotDeleteThisObject = false;
    bool wrapperClass = SizeIsOne(m_Members) &&
        m_Members.front().cName.empty();
    // generate member methods
    {
        ITERATE ( TMembers, i, m_Members ) {
            if ( i->ref ) {
                i->type->GeneratePointerTypeCode(code);
            }
            else {
                i->type->GenerateTypeCode(code);
                if ( i->type->GetKind() == eKindObject )
                    generateDoNotDeleteThisObject = true;
            }
            if ( i->delayed )
                delayed = true;
        }
    }
    // check if the class is Attlist
    bool isAttlist = false;
    if (!m_Members.empty()) {
        TMembers::const_iterator i = m_Members.begin();
        if (i->dataType) {
            const CDataType* t2 = i->dataType->GetParentType();
            if (t2) {
                const CDataMember* d = t2->GetDataMember();
                if (d) {
                    isAttlist = d->Attlist();
                }
            }
        }
    }
    if ( GetKind() != eKindObject )
        generateDoNotDeleteThisObject = false;
    if ( delayed )
        code.HPPIncludes().insert("serial/delaybuf");

    // generate member types
    {
        code.ClassPublic() <<
            "    // types\n";
        ITERATE ( TMembers, i, m_Members ) {
            string cType = i->type->GetCType(code.GetNamespace());
            if (!x_IsNullType(i)) {
                code.ClassPublic() <<
                    "    typedef "<<cType<<" "<<i->tName<<";\n";
            }
        }
        code.ClassPublic() << 
            "\n";
    }

    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    CNcbiOstream& methods = code.Methods();
    CNcbiOstream& inlineMethods = code.InlineMethods();

    if ( wrapperClass ) {
        const SMemberInfo& info = m_Members.front();
        if ( info.type->CanBeCopied() ) {
            string cType = info.type->GetCType(code.GetNamespace());
            code.ClassPublic() <<
                "    /// Data copy constructor.\n"
                "    "<<code.GetClassNameDT()<<"(const "<<cType<<"& value);\n"
                "\n"
                "    /// Data assignment operator.\n"
                "    void operator=(const "<<cType<<"& value);\n"
                "\n";
            inlineMethods <<
                "inline\n"<<
                methodPrefix<<code.GetClassNameDT()<<"(const "<<info.tName<<"& value)\n"
                "    : "<<info.mName<<"(value)\n"
                "{\n"
                "}\n"
                "\n"
                "inline\n"
                "void "<<methodPrefix<<"operator=(const "<<info.tName<<"& value)\n"
                "{\n"
                "    "<<info.mName<<" = value;\n"
                "}\n"
                "\n";
        }
    }

    // generate member getters & setters
    {
        code.ClassPublic() <<
            "    // getters\n";
        setters <<
            "    // setters\n\n";
        size_t member_index = (size_t)-1;
        size_t set_index;
        size_t set_offset;
        Uint4  set_mask, set_mask_maybe;
        bool isNull, isNullWithAtt;
        ITERATE ( TMembers, i, m_Members ) {
            // generate IsSet... method
            ++member_index;
            set_index  = (2*member_index)/(8*sizeof(Uint4));
            set_offset = (2*member_index)%(8*sizeof(Uint4));
            set_mask   = (0x03 << set_offset);
            set_mask_maybe = (0x01 << set_offset);
            {
                isNull = x_IsNullType(i);
                isNullWithAtt = x_IsNullWithAttlist(i);
// IsSetX
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
                        "\n"
                        "    /// Check if a value has been assigned to "<<i->cName<<" data member.\n"
                        "    ///\n"
                        "    /// Data member "<<i->cName<<" is ";
                } else {
                    code.ClassPublic() <<
                        "    /// ";
                }
// comment: what is it
                if (i->optional) {
                    if (i->defaultValue.empty()) {
                        code.ClassPublic() << "optional";
                    } else {
                        code.ClassPublic() << "mandatory with default "
                                           << i->defaultValue;
                    }
                } else {
                    code.ClassPublic() << "mandatory";
                }
// comment: typedef
                if (!isNull) {
                    if (CClassCode::GetDoxygenComments()) {
                        code.ClassPublic()
                            <<";\n    /// its type is defined as \'typedef "
                            << i->type->GetCType(code.GetNamespace())
                            <<" "<<i->tName<<"\'\n";
                    } else {
                        code.ClassPublic()
                            << "\n    /// typedef "
                            << i->type->GetCType(code.GetNamespace())
                            <<" "<<i->tName<<"\n";
                    }
                } else {
                    code.ClassPublic() << "\n";
                }
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
                        "    /// @return\n"
                        "    ///   - true, if a value has been assigned.\n"
                        "    ///   - false, otherwise.\n";
                }
                code.ClassPublic() <<
                    "    bool IsSet" << i->cName<<"(void) const;\n";
                inlineMethods <<
                    "inline\n"
                    "bool "<<methodPrefix<<"IsSet"<<i->cName<<"(void) const\n"
                    "{\n";
                if ( i->haveFlag ) {
                    // use special boolean flag
                    inlineMethods <<
                        "    return (("SET_PREFIX"["<<set_index<<"] & 0x"<<
                            hex<<set_mask<<dec<<") != 0);\n";
                }
                else {
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    if ( "DELAY_PREFIX<<i->cName<<" )\n"
                            "        return true;\n";
                    }
                    if ( i->ref ) {
                        // CRef
                        if (isNullWithAtt) {
                            inlineMethods <<
                                "    return "<<i->mName<<"->IsSet"<<i->cName<<"();\n";
                        } else {
                            inlineMethods <<
                                "    return "<<i->mName<<".NotEmpty();\n";
                        }
                    }
                    else {
                        // doesn't need set flag -> use special code
                        inlineMethods <<
                            "    return "<<i->type->GetIsSetCode(i->mName)<<";\n";
                    }
                }
                inlineMethods <<
                    "}\n"
                    "\n";

// CanGetX
                if (CClassCode::GetDoxygenComments()) {
                    if (isNull || isNullWithAtt) {
                        code.ClassPublic() <<
                            "\n"
                            "    /// Check if value of "<<i->cName<<" member is getatable.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   - false; the data member of type \'NULL\' has no value.\n";
                    } else {
                        code.ClassPublic() <<
                            "\n"
                            "    /// Check if it is safe to call Get"<<i->cName<<" method.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   - true, if the data member is getatable.\n"
                            "    ///   - false, otherwise.\n";
                    }
                }
                code.ClassPublic() <<
                    "    bool CanGet" << i->cName<<"(void) const;\n";
                inlineMethods <<
                    "inline\n"
                    "bool "<<methodPrefix<<"CanGet"<<i->cName<<"(void) const\n"
                    "{\n";
                if (!i->defaultValue.empty() || i->type->GetKind() == eKindContainer) {
                    inlineMethods <<"    return true;\n";
                } else {
                    if (isNull || isNullWithAtt) {
                        inlineMethods <<"    return false;\n";
                    } else {
                        inlineMethods <<"    return IsSet"<<i->cName<<"();\n";
                    }
                }
                inlineMethods <<
                    "}\n"
                    "\n";

            }
            
            // generate Reset... method
            string destructionCode = i->type->GetDestructionCode(i->valueName);
            string assignValue = i->defaultValue;
            string resetCode;
            if ( assignValue.empty() && !i->ref ) {
                resetCode = i->type->GetResetCode(i->valueName);
                if ( resetCode.empty() )
                    assignValue = i->type->GetInitializer();
            }
            if (CClassCode::GetDoxygenComments()) {
                setters <<
                    "\n"
                    "    /// Reset "<<i->cName<<" data member.\n";
            }
            setters <<
                "    void Reset"<<i->cName<<"(void);\n";
            // inline only when non reference and doesn't have reset code
            bool inl = !i->ref && resetCode.empty();
            code.MethodStart(inl) <<
                "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                "{\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
            }
            WriteTabbed(code.Methods(inl), destructionCode);
            if ( i->ref ) {
                if ( !i->optional ) {
                    // just reset value
                    resetCode = i->type->GetResetCode(i->valueName);
                    if ( !resetCode.empty() ) {
                        WriteTabbed(code.Methods(inl), resetCode);
                    }
                    else {
                        code.Methods(inl) <<
                            "    "<<i->valueName<<" = "<<i->type->GetInitializer()<<";\n";
                    }
                }
                else if ( assignValue.empty() ) {
                    // plain OPTIONAL
                    code.Methods(inl) <<
                        "    "<<i->mName<<".Reset();\n";
                }
                else {
                    if ( assignValue.empty() )
                        assignValue = i->type->GetInitializer();
                    // assign default value
                    code.Methods(inl) <<
                        "    "<<i->mName<<".Reset(new "<<i->tName<<"("<<assignValue<<"));\n";
                }
            }
            else {
                if ( !assignValue.empty() ) {
                    // assign default value
                    if (!isNull) {
                        code.Methods(inl) <<
                            "    "<<i->mName<<" = "<<assignValue<<";\n";
                    }
                }
                else {
                    // no default value
                    WriteTabbed(code.Methods(inl), resetCode);
                }
            }
            if ( i->haveFlag ) {
                code.Methods(inl) <<
                    "    "SET_PREFIX"["<<set_index<<"] &= ~0x"<<hex<<set_mask<<dec<<";\n";
            }
            code.Methods(inl) <<
                "}\n"
                "\n";

            string cType = i->type->GetCType(code.GetNamespace());
            string rType = i->type->GetPrefixedCType(code.GetNamespace(),methodPrefix);
            CTypeStrings::EKind kind = i->type->GetKind();

            // generate getter
            inl = true;//!i->ref;
            if (!isNull) {
                if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive())) {
                    if (CClassCode::GetDoxygenComments()) {
                        code.ClassPublic() <<
                            "\n"
                            "    /// Get the "<<i->cName<<" member data.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Copy of the member data.\n";
                    }
                    code.ClassPublic() <<
                        "    "<<i->tName<<" Get"<<i->cName<<"(void) const;\n";
                    code.MethodStart(inl) <<
                        ""<<rType<<" "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                        "{\n";
                } else {
                    if (CClassCode::GetDoxygenComments()) {
                        code.ClassPublic() <<
                            "\n"
                            "    /// Get the "<<i->cName<<" member data.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Reference to the member data.\n";
                    }
                    code.ClassPublic() <<
                        "    const "<<i->tName<<"& Get"<<i->cName<<"(void) const;\n";
                    code.MethodStart(inl) <<
                        "const "<<rType<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                        "{\n";
                }
                if ( i->delayed ) {
                    code.Methods(inl) <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                if (i->defaultValue.empty() &&
                    i->type->GetKind() != eKindContainer && !isNullWithAtt) {
                    code.Methods(inl) <<
                        "    if (!CanGet"<< i->cName<<"()) {\n"
                        "        ThrowUnassigned("<<member_index<<");\n"
                        "    }\n";
                }
                code.Methods(inl) <<
                    "    return "<<i->valueName<<";\n"
                    "}\n"
                    "\n";
            }

            // generate setter
            if ( i->ref ) {
                if (!isNullWithAtt) {
                    // generate reference setter
                    if (CClassCode::GetDoxygenComments()) {
                        setters <<
                            "\n"
                            "    /// Assign a value to "<<i->cName<<" data member.\n"
                            "    ///\n"
                            "    /// @param value\n"
                            "    ///   Reference to value.\n";
                    }
                    setters <<
                        "    void Set"<<i->cName<<"("<<i->tName<<"& value);\n";
                    methods <<
                        "void "<<methodPrefix<<"Set"<<i->cName<<"("<<rType<<"& value)\n"
                        "{\n";
                    if ( i->delayed ) {
                        methods <<
                            "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
                    }
                    methods <<
                        "    "<<i->mName<<".Reset(&value);\n";
                    if ( i->haveFlag ) {
                        methods <<
                            "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                    }
                    methods <<
                        "}\n"
                        "\n";
                }
                if (CClassCode::GetDoxygenComments()) {
                    setters <<
                        "\n"
                        "    /// Assign a value to "<<i->cName<<" data member.\n"
                        "    ///\n"
                        "    /// @return\n"
                        "    ///   Reference to the data value.\n";
                }
                setters <<
                    "    "<<i->tName<<"& Set"<<i->cName<<"(void);\n";
                if ( i->canBeNull ) {
                    // we have to init ref before returning
                    _ASSERT(!i->haveFlag);
                    methods <<
                        rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    if ( i->delayed ) {
                        methods <<
                            "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                    }
                    methods <<
                        "    if ( !"<<i->mName<<" )\n"
                        "        "<<i->mName<<".Reset("<<i->type->NewInstance(NcbiEmptyString)<<");\n"
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }
                else {
                    // value already not null -> simple inline method
                    inlineMethods <<
                        "inline\n"<<
                        rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                    }
                    if ( i->haveFlag) {
                        inlineMethods <<
                            "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                    }
                    if (isNullWithAtt) {
                        inlineMethods <<
                            "    "<<i->mName<<"->Set"<<i->cName<<"();\n";
                    }
                    inlineMethods <<
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }
                if (i->dataType && !isNullWithAtt) {
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
                                            "    /// Assign a value to "<<i->cName<<" data member.\n"
                                            "    ///\n"
                                            "    /// @param value\n"
                                            "    ///   Reference to value.\n";
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
            else {
                if (isNull) {
                    if (CClassCode::GetDoxygenComments()) {
                        setters <<
                            "\n"
                            "    /// Set NULL data member (assign \'NULL\' value to "<<i->cName<<" data member).\n";
                    }
                    setters <<
                        "    void Set"<<i->cName<<"(void);\n";
                    inlineMethods <<
                        "inline\n"<<
                        "void "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    inlineMethods <<
                        "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                    inlineMethods <<
                        "}\n"
                        "\n";
                } else {
                    if ( i->type->CanBeCopied() ) {
                        if (CClassCode::GetDoxygenComments()) {
                            setters <<
                                "\n"
                                "    /// Assign a value to "<<i->cName<<" data member.\n"
                                "    ///\n"
                                "    /// @param value\n"
                                "    ///   Value to assign\n";
                        }
                        if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive())) {
                            setters <<
                                "    void Set"<<i->cName<<"("<<i->tName<<" value);\n";
                            inlineMethods <<
                                "inline\n"
                                "void "<<methodPrefix<<"Set"<<i->cName<<"("<<rType<<" value)\n"
                                "{\n";
                        } else {
                            setters <<
                                "    void Set"<<i->cName<<"(const "<<i->tName<<"& value);\n";
                            inlineMethods <<
                                "inline\n"
                                "void "<<methodPrefix<<"Set"<<i->cName<<"(const "<<rType<<"& value)\n"
                                "{\n";
                        }
                        if ( i->delayed ) {
                            inlineMethods <<
                                "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
                        }
                        inlineMethods <<                        
                            "    "<<i->valueName<<" = value;\n";
                        if ( i->haveFlag ) {
                            inlineMethods <<
                                "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                        }
                        inlineMethods <<
                            "}\n"
                            "\n";
                    }
                    if (CClassCode::GetDoxygenComments()) {
                        setters <<
                            "\n"
                            "    /// Assign a value to "<<i->cName<<" data member.\n"
                            "    ///\n"
                            "    /// @return\n"
                            "    ///   Reference to the data value.\n";
                    }
                    setters <<
                        "    "<<i->tName<<"& Set"<<i->cName<<"(void);\n";
                    inlineMethods <<
                        "inline\n"<<
                        rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                    }
                    if ( i->haveFlag ) {

                        if ((kind == eKindStd) || (kind == eKindEnum) || (kind == eKindString)) {
                            inlineMethods <<
                                "#ifdef _DEBUG\n"
                                "    if (!IsSet"<<i->cName<<"()) {\n"
                                "        ";
                            if (kind == eKindString) {
                                inlineMethods <<
                                    i->valueName << " = ms_UnassignedStr;\n";
                            } else {
                                inlineMethods <<
                                    "memset(&"<<i->valueName<<",ms_UnassignedByte,sizeof("<<i->valueName<<"));\n";
                            }
                            inlineMethods <<
                                "    }\n"
                                "#endif\n";
                        }

                        inlineMethods <<
                            "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask_maybe<<dec<<";\n";
                    }
                    inlineMethods <<
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }

            }


            // generate conversion operators
            if ( i->cName.empty() ) {
                if ( i->optional ) {
                    string loc(code.GetClassNameDT());
                    if (i->dataType) {
                        loc = i->dataType->LocationString();
                    }
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        loc + ": the only member of adaptor class is optional");
                }
                code.ClassPublic() <<
                    "    /// Conversion operator to \'const "<<i->tName<<"\' type.\n"
                    "    operator const "<<i->tName<<"& (void) const;\n\n"
                    "    /// Conversion operator to \'"<<i->tName<<"\' type.\n"
                    "    operator "<<i->tName<<"& (void);\n\n";
                inlineMethods <<
                    "inline\n"<<
                    methodPrefix<<"operator const "<<rType<<"& (void) const\n"
                    "{\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                inlineMethods << "    return ";
                if ( i->ref )
                    inlineMethods << "*";
                inlineMethods <<
                    i->mName<<";\n"
                    "}\n"
                    "\n";
                inlineMethods <<
                    "inline\n"<<
                    methodPrefix<<"operator "<<rType<<"& (void)\n"
                    "{\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                if ( i->haveFlag ) {

                    if ((kind == eKindStd) || (kind == eKindEnum) || (kind == eKindString)) {
                        inlineMethods <<
                            "#ifdef _DEBUG\n"
                            "    if (!IsSet"<<i->cName<<"()) {\n"
                            "        ";
                        if (kind == eKindString) {
                            inlineMethods <<
                                i->valueName << " = ms_UnassignedStr;\n";
                        } else {
                            inlineMethods <<
                                "memset(&"<<i->valueName<<",ms_UnassignedByte,sizeof("<<i->valueName<<"));\n";
                        }
                        inlineMethods <<
                            "    }\n"
                            "#endif\n";
                    }

                    inlineMethods <<
                        "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask_maybe<<dec<<";\n";
                }
                inlineMethods << "    return ";
                if ( i->ref )
                    inlineMethods << "*";
                inlineMethods <<
                    i->mName<<";\n"
                    "}\n"
                    "\n";
            }

            setters <<
                "\n";
        }
    }

    // generate member data
    {
        code.ClassPrivate() <<
            "    // Prohibit copy constructor and assignment operator\n" <<
            "    " << code.GetClassNameDT() <<
            "(const " << code.GetClassNameDT() << "&);\n" <<
            "    " << code.GetClassNameDT() << "& operator=(const " <<
            code.GetClassNameDT() << "&);\n" <<
            "\n";
        code.ClassPrivate() <<
            "    // data\n";
		{
	        if (m_Members.size() !=0) {
	            code.ClassPrivate() <<
                    "    Uint4 "SET_PREFIX<<"["<<
                    (2*m_Members.size()-1+8*sizeof(Uint4))/(8*sizeof(Uint4)) <<"];\n";
            }
        }
		{
	        ITERATE ( TMembers, i, m_Members ) {
		        if ( i->delayed ) {
			        code.ClassPrivate() <<
				        "    mutable NCBI_NS_NCBI::CDelayBuffer "DELAY_PREFIX<<i->cName<<";\n";
				}
			}
        }
		{
			ITERATE ( TMembers, i, m_Members ) {
                if ( i->ref ) {
                    code.ClassPrivate() <<
                        "    "<<ncbiNamespace<<"CRef< "<<i->tName<<" > "<<i->mName<<";\n";
                }
                else {
                    if (!x_IsNullType(i)) {
                        code.ClassPrivate() <<
                            "    "<<i->tName<<" "<<i->mName<<";\n";
                    }
                }
		    }
		}
    }

    // generate member initializers
    {
        {
            ITERATE ( TMembers, i, m_Members ) {
                if ( i->ref ) {
                    if ( !i->canBeNull ) {
                        string init = i->defaultValue;
                        if ( init.empty() ) {
                            init = i->type->GetInitializer();
                        }
                        code.AddInitializer(i->mName,
                                            i->type->NewInstance(init));
                    }
                }
                else {
                    if (!x_IsNullType(i)) {
                        string init = i->defaultValue;
                        if ( init.empty() )
                            init = i->type->GetInitializer();
                        if ( !init.empty() )
                            code.AddInitializer(i->mName, init);
                    }
                }
            }
        }
    }

    // generate Reset method
    code.ClassPublic() <<
        "    /// Reset the whole object\n"
        "    ";
    if ( !wrapperClass ) {
        if ( HaveUserClass() )
            code.ClassPublic() << "virtual ";
        code.ClassPublic() <<
            "void Reset(void);\n";
        methods <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        ITERATE ( TMembers, i, m_Members ) {
            methods <<
                "    Reset"<<i->cName<<"();\n";
        }
        methods <<
            "}\n"
            "\n";
    }
    code.ClassPublic() << "\n";

    if ( generateDoNotDeleteThisObject ) {
        code.ClassPublic() <<
            "    virtual void DoNotDeleteThisObject(void);\n"
            "\n";
        methods <<
            "void "<<methodPrefix<<"DoNotDeleteThisObject(void)\n"
            "{\n"
            "    "<<code.GetParentClassName()<<"::DoNotDeleteThisObject();\n";
        ITERATE ( TMembers, i, m_Members ) {
            if ( !i->ref && i->type->GetKind() == eKindObject ) {
                methods <<
                    "    "<<i->mName<<".DoNotDeleteThisObject();\n";
            }
        }
        methods <<
            "}\n"
            "\n";
    }

    // generate destruction code
    {
        for ( TMembers::const_reverse_iterator i = m_Members.rbegin();
              i != m_Members.rend(); ++i ) {
            code.AddDestructionCode(i->type->GetDestructionCode(i->valueName));
        }
    }

    // generate type info
    methods << "BEGIN_NAMED_";
    if ( haveUserClass )
        methods << "BASE_";
    if ( wrapperClass )
        methods << "IMPLICIT_";
    methods <<
        "CLASS_INFO(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassNameDT()<<")\n"
        "{\n";
    if ( !GetModuleName().empty() ) {
        methods <<
            "    SET_CLASS_MODULE(\""<<GetModuleName()<<"\");\n";
    }
    if ( !m_ParentClassName.empty() ) {
        code.SetParentClass(m_ParentClassName, m_ParentClassNamespace);
        methods <<
            "    SET_PARENT_CLASS("<<m_ParentClassNamespace.GetNamespaceRef(code.GetNamespace())<<m_ParentClassName<<");\n";
    }
    {
        // All or none of the members must be tagged
        bool useTags = false;
        bool hasUntagged = false;
        // All tags must be different
        map<int, bool> tag_map;

        size_t member_index = 0;
        ITERATE ( TMembers, i, m_Members ) {
            ++member_index;
            if ( i->memberTag >= 0 ) {
                if ( hasUntagged ) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        "No explicit tag for some members in " +
                        GetModuleName());
                }
                if ( tag_map[i->memberTag] )
                    NCBI_THROW(CDatatoolException,eInvalidData,
                        "Duplicate tag: " + i->cName +
                        " [" + NStr::IntToString(i->memberTag) + "] in " +
                        GetModuleName());
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
            
            bool ref = i->ref;
            if ( ref ) {
                methods << "REF_";
                addCType = true;
            }
            else {
                switch ( i->type->GetKind() ) {
                case eKindStd:
                case eKindString:
                    if ( i->type->HaveSpecialRef() ) {
                        addRef = true;
                    }
                    else {
                        methods << "STD_";
                    }
                    break;
                case eKindEnum:
                    methods << "ENUM_";
                    addEnum = true;
                    if ( !i->type->GetNamespace().IsEmpty() &&
                         code.GetNamespace() != i->type->GetNamespace()) {
                        _TRACE("EnumNamespace: "<<i->type->GetNamespace()<<" from "<<code.GetNamespace());
                        methods << "IN_";
                        addNamespace = true;
                    }
                    break;
                default:
                    addRef = true;
                    break;
                }
            }

            methods << "MEMBER(\""<<i->externalName<<"\"";
            if (!isNull) {
                methods << ", "<<i->mName;
            }
            if ( addNamespace )
                methods << ", "<<i->type->GetNamespace();
            if ( addCType )
                methods << ", "<<i->type->GetCType(code.GetNamespace());
            if ( addEnum )
                methods << ", "<<i->type->GetEnumName();
            if ( addRef )
                methods << ", "<<i->type->GetRef(code.GetNamespace());
            methods << ")";

            if ( !i->defaultValue.empty() ) {
                methods << "->SetDefault(";
                if ( ref )
                    methods << "new NCBI_NS_NCBI::CRef< "+i->tName+" >(";
                methods << "new "<<i->tName<<"("<<i->defaultValue<<')';
                if ( ref )
                    methods << ')';
                methods << ')';
              if ( i->haveFlag )
                    methods <<
                        "->SetSetFlag(MEMBER_PTR("SET_PREFIX"[0]))";
            }
            else if ( i->optional ) {
                methods << "->SetOptional()";
                if (i->haveFlag) {
                    methods <<
                        "->SetSetFlag(MEMBER_PTR("SET_PREFIX"[0]))";
                }
            }
            else if (i->haveFlag) {
                methods <<
                    "->SetSetFlag(MEMBER_PTR("SET_PREFIX"[0]))";
            }
            if ( i->delayed ) {
                methods <<
                    "->SetDelayBuffer(MEMBER_PTR("DELAY_PREFIX<<
                    i->cName<<"))";
            }
            if (i->nonEmpty) {
                methods << "->SetNonEmpty()";
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
        if ( isAttlist || useTags ) {
            // Tagged class is not sequential
            methods << "    info->SetRandomOrder(true);\n";
        }
        else {
            // Just query the flag to avoid warnings.
            methods << "    info->RandomOrder();\n";
        }
    }
    methods <<
        "}\n"
        "END_CLASS_INFO\n"
        "\n";
}

void CClassTypeStrings::GenerateUserHPPCode(CNcbiOstream& out) const
{
    if (CClassCode::GetDoxygenComments()) {
        out << "\n"
            << "/** @addtogroup ";
        if (!CClassCode::GetDoxygenGroup().empty()) {
            out << CClassCode::GetDoxygenGroup();
        } else {
            out << "dataspec_" << GetModuleName();
        }
        out
            << "\n *\n"
            << " * @{\n"
            << " */\n\n";
    }
    bool wrapperClass = (m_Members.size() == 1) &&
        m_Members.front().cName.empty();
    bool generateCopy = wrapperClass && m_Members.front().type->CanBeCopied();

    out <<
        "/////////////////////////////////////////////////////////////////////////////\n";
    if (CClassCode::GetDoxygenComments()) {
        out <<
            "///\n"
            "/// " << GetClassNameDT() << " --\n"
            "///\n\n";
    }
    out << "class ";
    if ( !CClassCode::GetExportSpecifier().empty() )
        out << CClassCode::GetExportSpecifier() << " ";
    out << GetClassNameDT()<<" : public "<<GetClassNameDT()<<"_Base\n"
        "{\n"
        "    typedef "<<GetClassNameDT()<<"_Base Tparent;\n"
        "public:\n";
    DeclareConstructor(out, GetClassNameDT());
    ITERATE ( TMembers, i, m_Members ) {
        if (i->simple && !x_IsNullType(i)) {
            out <<
                "    " << GetClassNameDT() <<"(const "<<
                i->type->GetCType(GetNamespace()) << "& value);" <<
                "\n";
            break;
        }
    }
    DeclareDestructor(out, GetClassNameDT(), false);

    if ( generateCopy ) {
        const SMemberInfo& info = m_Members.front();
        string cType = info.type->GetCType(GetNamespace());
        out <<
            "    /// Copy constructor\n"
            "    "<<GetClassNameDT()<<"(const "<<cType<<"& value);\n\n"
            "\n"
            "    /// Assignment operator\n"
            "    "<<GetClassNameDT()<<"& operator=(const "<<cType<<"& value);\n\n"
            "\n";
    }
    ITERATE ( TMembers, i, m_Members ) {
        if (i->simple && !x_IsNullType(i)) {
            out <<
            "    /// Conversion operator to \'"
            << i->type->GetCType(GetNamespace()) << "\' type.\n"
            "    operator const " << i->type->GetCType(GetNamespace()) <<
            "&(void) const;\n\n";
            out <<
            "    /// Assignment operator.\n"
            "    " << GetClassNameDT() << "& operator="<<"(const "<<
            i->type->GetCType(GetNamespace()) << "& value);\n" <<
            "\n";
            break;
        }
    }
    out << "private:\n" <<
        "    // Prohibit copy constructor and assignment operator\n"
        "    "<<GetClassNameDT()<<"(const "<<GetClassNameDT()<<"& value);\n"
        "    "<<GetClassNameDT()<<"& operator=(const "<<GetClassNameDT()<<
        "& value);\n"
        "\n";

    out << "};\n";
    if (CClassCode::GetDoxygenComments()) {
        out << "/* @} */\n";
    }
    out << "\n";
    out <<
        "/////////////////// "<<GetClassNameDT()<<" inline methods\n"
        "\n"
        "// constructor\n"
        "inline\n"<<
        GetClassNameDT()<<"::"<<GetClassNameDT()<<"(void)\n"
        "{\n"
        "}\n"
        "\n";
    ITERATE ( TMembers, i, m_Members ) {
        if (i->simple && !x_IsNullType(i)) {
            out <<
            "inline\n" <<
            GetClassNameDT()<<"::"<<GetClassNameDT()<<"(const "<<
            i->type->GetCType(GetNamespace()) << "& value)\n"<<
            "{\n"
            "    Set" << i->cName << "(value);\n" <<
            "}\n"
            "\n";
        }
    }
    if ( generateCopy ) {
        const SMemberInfo& info = m_Members.front();
        out <<
            "// data copy constructors\n"
            "inline\n"<<
            GetClassNameDT()<<"::"<<GetClassNameDT()<<"(const "<<info.tName<<"& value)\n"
            "    : Tparent(value)\n"
            "{\n"
            "}\n"
            "\n"
            "// data assignment operators\n"
            "inline\n"<<
            GetClassNameDT()<<"& "<<GetClassNameDT()<<"::operator=(const "<<info.tName<<"& value)\n"
            "{\n"
            "    Set(value);\n"
            "    return *this;\n"
            "}\n"
            "\n";
    }
    ITERATE ( TMembers, i, m_Members ) {
        if (i->simple && !x_IsNullType(i)) {
            out <<
            "inline\n"<<
            GetClassNameDT() << "::"
            "operator const " << i->type->GetCType(GetNamespace()) <<
            "&(void) const\n" <<
            "{\n" <<
            "    return Get" << i->cName << "();\n" <<
            "}\n" <<
            "\n";
            out <<
            "inline\n"<<
            GetClassNameDT() << "& " << GetClassNameDT() << "::"
            "operator="<<"(const "<<
            i->type->GetCType(GetNamespace()) << "& value)\n" <<
            "{\n" <<
            "    Set" << i->cName << "(value);\n" <<
            "    return *this;\n" <<
            "}\n"
            "\n";
            break;
        }
    }
    out <<
        "\n"
        "/////////////////// end of "<<GetClassNameDT()<<" inline methods\n"
        "\n"
        "\n";
}

void CClassTypeStrings::GenerateUserCPPCode(CNcbiOstream& out) const
{
    out <<
        "// destructor\n"<<
        GetClassNameDT()<<"::~"<<GetClassNameDT()<<"(void)\n"
        "{\n"
        "}\n"
        "\n";
}

CClassRefTypeStrings::CClassRefTypeStrings(const string& className,
                                           const CNamespace& ns,
                                           const string& fileName)
    : m_ClassName(className),
      m_Namespace(ns),
      m_FileName(fileName)
{
}

CTypeStrings::EKind CClassRefTypeStrings::GetKind(void) const
{
    return eKindObject;
}

const CNamespace& CClassRefTypeStrings::GetNamespace(void) const
{
    return m_Namespace;
}

void CClassRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

void CClassRefTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    ctx.AddForwardDeclaration(m_ClassName, m_Namespace);
    ctx.CPPIncludes().insert(m_FileName);
}

string CClassRefTypeStrings::GetClassName(void) const
{
    return m_ClassName;
}

string CClassRefTypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(m_Namespace)+m_ClassName;
}

string CClassRefTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                              const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CClassRefTypeStrings::GetRef(const CNamespace& ns) const
{
    return "CLASS, ("+GetCType(ns)+')';
}

string CClassRefTypeStrings::NewInstance(const string& init) const
{
    return "new "+GetCType(CNamespace::KEmptyNamespace)+"("+init+')';
}

string CClassRefTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.68  2005/04/25 15:45:33  gouriano
* Improve diagnostics
*
* Revision 1.67  2005/01/24 20:19:39  ucko
* When describing a field with a default value, note what it is rather
* than just that it exists.
*
* Revision 1.66  2005/01/12 15:38:49  vasilche
* Use CRef<>::NotEmpty() to avoid performance warning on MSVC.
*
* Revision 1.65  2004/09/07 14:09:45  grichenk
* Fixed assignment of default value to aliased types
*
* Revision 1.64  2004/07/21 13:29:59  gouriano
* Set and return primitive type data by value
*
* Revision 1.63  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.62  2004/05/03 19:31:03  gouriano
* Made generation of DOXYGEN-style comments optional
*
* Revision 1.61  2004/04/29 20:11:39  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.60  2004/03/08 20:08:53  gouriano
* Correct namespaces of generated classes
*
* Revision 1.59  2003/11/20 14:32:41  gouriano
* changed generated C++ code so NULL data types have no value
*
* Revision 1.58  2003/06/24 20:55:42  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.57  2003/06/03 15:06:46  gouriano
* corrected generation of conversion operator for implicit members
*
* Revision 1.56  2003/05/08 16:59:08  gouriano
* added comment about the meaning of typedef for each class member
*
* Revision 1.55  2003/05/06 13:34:36  gouriano
* inline trivial Get methods
*
* Revision 1.54  2003/05/05 20:10:28  gouriano
* removed CanGet check for members which are always "gettable"
*
* Revision 1.53  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.52  2003/04/10 20:13:41  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.50  2003/04/04 15:22:29  gouriano
* disable throwing CUnassignedMember exception
*
* Revision 1.49  2003/04/03 21:48:45  gouriano
* verify initialization of data members
*
* Revision 1.48  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.47  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.46  2003/02/12 21:39:51  gouriano
* corrected code generator so primitive data types (bool,int,etc)
* are returned by value, not by reference
*
* Revision 1.45  2002/12/23 18:40:07  dicuccio
* Added new command-line option: -oex <export-specifier> for adding WIn32 export
* specifiers to generated objects.
*
* Revision 1.44  2002/12/17 21:51:41  gouriano
* corrected generation of "simple" members
*
* Revision 1.43  2002/12/12 21:04:17  gouriano
* changed code generation so XML attribute list became random access class
*
* Revision 1.42  2002/11/19 19:48:28  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.41  2002/11/14 21:03:39  gouriano
* added support of XML attribute lists
*
* Revision 1.40  2002/11/13 00:44:00  ucko
* Made type info declaration optional (but on by default); CVS logs to end
*
* Revision 1.39  2002/10/15 13:58:05  gouriano
* use "noprefix" flag
*
* Revision 1.38  2002/08/14 17:14:25  grichenk
* Fixed function name conflict on Win32: renamed
* GetClassName() -> GetClassNameDT()
*
* Revision 1.37  2002/05/15 20:22:04  grichenk
* Added CSerialObject -- base class for all generated ASN.1 classes
*
* Revision 1.36  2001/12/12 17:47:35  grichenk
* Updated setters/getters generator to avoid using CRef<>s
*
* Revision 1.35  2001/12/07 18:56:15  grichenk
* Most generated class members are stored in CRef<>-s by default
*
* Revision 1.34  2001/06/25 16:36:23  grichenk
* // Hide -> // Prohibit
*
* Revision 1.33  2001/06/21 19:47:39  grichenk
* Copy constructor and operator=() moved to "private" section
*
* Revision 1.32  2001/06/13 14:40:02  grichenk
* Modified class and choice info generation code to avoid warnings
*
* Revision 1.31  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.30  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.29  2000/11/29 17:42:43  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.28  2000/11/07 17:26:25  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.27  2000/11/02 14:42:34  vasilche
* MSVC doesn't understand namespaces in parent class specifier in subclass methods.
*
* Revision 1.26  2000/11/01 20:38:58  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.25  2000/09/26 17:38:25  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.24  2000/08/25 15:59:20  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.23  2000/08/15 19:45:28  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.22  2000/07/11 20:36:28  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.21  2000/06/27 16:34:47  vasilche
* Fixed generated comments.
* Fixed class names conflict. Now internal classes' names begin with "C_".
*
* Revision 1.20  2000/06/27 13:22:00  vasilche
* Added const modifier to generated IsSet*() methods.
*
* Revision 1.19  2000/06/16 16:31:38  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.18  2000/05/03 14:38:18  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.17  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.16  2000/04/12 15:36:49  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.15  2000/04/07 19:26:24  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.14  2000/04/06 16:11:25  vasilche
* Removed unneeded calls to Reset().
*
* Revision 1.13  2000/04/03 18:47:30  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.12  2000/03/29 15:52:26  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.11  2000/03/17 16:49:55  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
* All setters/getters made public until we'll find better solution.
*
* Revision 1.10  2000/03/15 14:18:39  vasilche
* Forgot to fix _ASSERT() expression.
*
* Revision 1.9  2000/03/14 18:32:58  vasilche
* Fixed class includes generation.
*
* Revision 1.8  2000/03/14 14:43:10  vasilche
* All OPTIONAL members implemented via CRef<> by default.
*
* Revision 1.7  2000/03/08 14:40:00  vasilche
* Renamed NewInstance() -> New().
*
* Revision 1.6  2000/03/07 20:05:00  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.5  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.4  2000/02/17 20:05:06  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.3  2000/02/02 19:08:20  vasilche
* Fixed variable conflict in generated files on MSVC.
*
* Revision 1.2  2000/02/02 18:54:15  vasilche
* Fixed variable conflict in MSVC.
* Added missing #include <serial/choice.hpp>
*
* Revision 1.1  2000/02/01 21:47:55  vasilche
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
