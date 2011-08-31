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
#include "exceptions.hpp"
#include "type.hpp"
#include "blocktype.hpp"
#include "unitype.hpp"
#include "classstr.hpp"
#include "stdstr.hpp"
#include "code.hpp"
#include "srcutil.hpp"
#include "comments.hpp"
#include "statictype.hpp"
#include "stlstr.hpp"
#include "ptrstr.hpp"
#include "reftype.hpp"

BEGIN_NCBI_SCOPE

#define SET_PREFIX "m_set_State"
#define DELAY_PREFIX "m_delay_"

CClassTypeStrings::CClassTypeStrings(const string& externalName,
                                     const string& className,
                                     const string& namespaceName,
                                     const CDataType* dataType,
                                     const CComments& comments)
    : CParent(namespaceName,dataType,comments),
      m_IsObject(true), m_HaveUserClass(true), m_HaveTypeInfo(true),
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

bool CClassTypeStrings::x_IsAnyContentType(TMembers::const_iterator i) const
{
    return (dynamic_cast<CAnyContentTypeStrings*>(i->type.get()) != 0);
}

bool CClassTypeStrings::x_IsUniSeq(TMembers::const_iterator i) const
{
    return i->dataType && i->dataType->IsUniSeq();
}

void CClassTypeStrings::AddMember(const string& name,
                                  const AutoPtr<CTypeStrings>& type,
                                  const string& pointerType,
                                  bool optional,
                                  const string& defaultValue,
                                  bool delayed, int tag,
                                  bool noPrefix, bool attlist, bool noTag,
                                  bool simple,const CDataType* dataType,
                                  bool nonempty, const CComments& comments)
{
    m_Members.push_back(SMemberInfo(name, type,
                                    pointerType,
                                    optional, defaultValue,
                                    delayed, tag, noPrefix,attlist,noTag,
                                    simple,dataType,nonempty, comments));
}

CClassTypeStrings::SMemberInfo::SMemberInfo(const string& name,
                                            const AutoPtr<CTypeStrings>& t,
                                            const string& pType,
                                            bool opt, const string& defValue,
                                            bool del, int tag, bool noPrefx,
                                            bool attlst, bool noTg, bool simpl,
                                            const CDataType* dataTp, bool nEmpty,
                                            const CComments& commnts)
    : externalName(name), cName(Identifier(name)),
      mName("m_"+cName), tName('T'+cName),
      type(t), ptrType(pType),
      optional(opt), delayed(del), memberTag(tag),
      defaultValue(defValue), noPrefix(noPrefx), attlist(attlst), noTag(noTg),
      simple(simpl),dataType(dataTp),nonEmpty(nEmpty), comments(commnts)
{
    if ( cName.empty() ) {
        mName = "m_data";
        tName = "Tdata";
    }

    bool haveDefault = !defaultValue.empty();
    if (haveDefault && dataType && dataType->IsUniSeq()) {
        haveDefault = false;
    }
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
    if (dynamic_cast<CAnyContentTypeStrings*>(type.get()) != 0) {
        ptrType = "Ref";
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

    BeginClassDeclaration(ctx);
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
    code.WriteConstructionCode(code.Methods());
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
    bool thisNullWithAtt;
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
        TMembers::const_iterator j=m_Members.begin();
        thisNullWithAtt = m_Members.size() == 2 && j->attlist && x_IsNullType(++j);
    }
    // check if the class is Attlist
    bool isSet = false;
    if (!m_Members.empty()) {
        TMembers::const_iterator i = m_Members.begin();
        if (i->dataType) {
            const CDataType* t2 = i->dataType->GetParentType();
            if (t2) {
                isSet = dynamic_cast<const CDataSetType*>(t2) != 0;
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
        bool ce_defined = GetClassNameDT() == "C_E";
        ITERATE ( TMembers, i, m_Members ) {
            if (!ce_defined && i->dataType) {
                const CUniSequenceDataType* mem =
                    dynamic_cast<const CUniSequenceDataType*>(i->dataType);
                if (mem != 0) {
                    const CDataMemberContainerType* elem =
                        dynamic_cast<const CDataMemberContainerType*>(mem->GetElementType());
                    if (elem && elem->GetMemberName() == "E") {
                        string name;
                        if (elem->GetTypeStr()) {
                            name = elem->GetTypeStr()->GetClassNameDT();
                        } else {
                            name = elem->ClassName();
                        }
                        code.ClassPublic() <<
                            "    typedef "<< name <<" "<< "C_E;\n";
                        ce_defined = true;
                    }
                }
            }
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
        if ( info.type->CanBeCopied() && !x_IsNullType(m_Members.begin()) ) {
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
                "{\n"
                "    Set(value);\n"
                "}\n"
                "\n"
                "inline\n"
                "void "<<methodPrefix<<"operator=(const "<<info.tName<<"& value)\n"
                "{\n"
                "    Set(value);\n"
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
        bool isNull, isNullWithAtt, as_ref;
        ITERATE ( TMembers, i, m_Members ) {
            // generate IsSet... method
            ++member_index;
            set_index  = (2*member_index)/(8*sizeof(Uint4));
            set_offset = (2*member_index)%(8*sizeof(Uint4));
            set_mask   = (0x03 << set_offset);
            set_mask_maybe = (0x01 << set_offset);
            as_ref = i->ref /*|| x_IsAnyContentType(i)*/;
            {
                isNull = x_IsNullType(i);
                isNullWithAtt = x_IsNullWithAttlist(i);
// IsSetX
                i->comments.PrintHPPMember(code.ClassPublic());
                if (CClassCode::GetDoxygenComments()) {
                    code.ClassPublic() <<
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
                } else {
                    code.ClassPublic() <<
                        "    ///  Check whether the "<<i->cName<<" data member has been assigned a value.\n";
                }
                code.ClassPublic() <<
                    "    bool IsSet" << i->cName<<"(void) const;\n";


                if (!i->haveFlag && as_ref && isNullWithAtt) {
                    methods <<
                        "bool "<<methodPrefix<<"IsSet"<<i->cName<<"(void) const\n{\n" <<
                        "    return "<<i->mName<<" ? "<<i->mName<<"->IsSet"<<i->cName<<"() : false;\n}\n\n";
                } else {
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
                        if ( as_ref ) {
                            // CRef
                            inlineMethods <<
                                "    return "<<i->mName<<".NotEmpty();\n";
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
                }

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
                } else {
                    code.ClassPublic() <<
                        "    /// Check whether it is safe or not to call Get"<<i->cName<<" method.\n";
                }
                code.ClassPublic() <<
                    "    bool CanGet" << i->cName<<"(void) const;\n";
                inlineMethods <<
                    "inline\n"
                    "bool "<<methodPrefix<<"CanGet"<<i->cName<<"(void) const\n"
                    "{\n";
                if (!i->defaultValue.empty() ||
                    i->type->GetKind() == eKindContainer ||
                    (as_ref && !i->canBeNull)) {
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
            string assignValue = x_IsUniSeq(i) ? kEmptyStr : i->defaultValue;
            string resetCode;
            if ( assignValue.empty() && !as_ref ) {
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
            bool inl = !as_ref && resetCode.empty();
            code.MethodStart(inl) <<
                "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                "{\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
            }
            WriteTabbed(code.Methods(inl), destructionCode);
            if ( (as_ref && !i->canBeNull) ) {
                if ( assignValue.empty() )
                    assignValue = i->type->GetInitializer();
                code.Methods(inl) <<
                    "    if ( !"<<i->mName<<" ) {\n"
                    "        "<<i->mName<<".Reset(new "<<i->tName<<"("<<assignValue<<"));\n"
                    "        return;\n"
                    "    }\n";
            }
            if ( as_ref ) {
                if ( !i->optional ) {
                    // just reset value
                    resetCode = i->type->GetResetCode(i->valueName);
                    if ( !resetCode.empty() ) {
                        WriteTabbed(code.Methods(inl), resetCode);
                    }
                    else {
                        // WHEN DOES THIS HAPPEN???
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
#if 0
            string rType = i->type->GetPrefixedCType(code.GetNamespace(),methodPrefix);
#else
            //use defined types
            string rType = methodPrefix + i->tName;
#endif
            CTypeStrings::EKind kind = i->type->GetKind();

            // generate getter
            inl = true;//!i->ref;
            if (!isNull) {
// for 'simple' types we define conversion operator in UserHPP,
// which requires reference
                if (kind == eKindEnum || (i->dataType && i->dataType->IsPrimitive() && !i->simple)) {
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
                if ( (as_ref && !i->canBeNull) ) {
                    code.Methods(inl) <<
                        "    if ( !"<<i->mName<<" ) {\n"
                        "        const_cast<"<<code.GetClassNameDT()<<"*>(this)->Reset"<<i->cName<<"();\n"
                        "    }\n";
                }
                else if (i->defaultValue.empty() &&
                         i->type->GetKind() != eKindContainer &&
                         !isNullWithAtt) {
                    code.Methods(inl) <<
                        "    if (!CanGet"<< i->cName<<"()) {\n"
                        "        ThrowUnassigned("<<member_index;
#if 0
                    code.Methods(inl) << ", __FILE__, __LINE__";
#endif
                    code.Methods(inl) << ");\n"
                        "    }\n";
                }
                code.Methods(inl) <<
                    "    return "<<i->valueName<<";\n"
                    "}\n"
                    "\n";
            }

            // generate setter
            if ( as_ref ) {
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
                if (i->attlist && thisNullWithAtt) {
                    TMembers::const_iterator j = i;
                    methods << "    Set" << (++j)->cName << "();\n";
                }
                if ( i->haveFlag ) {
                    methods <<
                        "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                }
                methods <<
                    "}\n"
                    "\n";
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
                        "        "<<i->mName<<".Reset("<<i->type->NewInstance(NcbiEmptyString)<<");\n";
                    if (isNullWithAtt) {
                        methods <<
                            "    "<<i->mName<<"->Set"<<i->cName<<"();\n";
                    }
                    methods <<
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }
                else {
                    if (isNullWithAtt) {
                        methods <<
                            rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                            "{\n";
                        methods <<
                            "    if ( !"<<i->mName<<" ) {\n"
                            "        Reset"<<i->cName<<"();\n"
                            "    }\n";
                        methods <<
                            "    "<<i->mName<<"->Set"<<i->cName<<"();\n";
                        methods <<
                            "    return "<<i->valueName<<";\n"
                            "}\n"
                            "\n";
                    } else {
                        // value already not null -> simple inline method
                        inlineMethods <<
                            "inline\n"<<
                            rType<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                            "{\n";
                        if ( i->delayed ) {
                            inlineMethods <<
                                "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                        }
                        if ( (as_ref && !i->canBeNull) ) {
                            inlineMethods <<
                                "    if ( !"<<i->mName<<" ) {\n"
                                "        Reset"<<i->cName<<"();\n"
                                "    }\n";
                        }
                        if (i->attlist && thisNullWithAtt) {
                            TMembers::const_iterator j = i;
                            inlineMethods << "    Set" << (++j)->cName << "();\n";
                        }
                        if ( i->haveFlag) {
                            inlineMethods <<
                                "    "SET_PREFIX"["<<set_index<<"] |= 0x"<<hex<<set_mask<<dec<<";\n";
                        }
                        inlineMethods <<
                            "    return "<<i->valueName<<";\n"
                            "}\n"
                            "\n";
                    }
                }
                if (i->dataType && !isNullWithAtt) {
                    const CDataType* resolved = i->dataType->Resolve();
                    if (resolved && resolved != i->dataType) {
                        CClassTypeStrings* typeStr = resolved->GetTypeStr();
                        if (typeStr) {
                            ITERATE ( TMembers, ir, typeStr->m_Members ) {
                                if (ir->simple && ir->type->CanBeCopied()) {
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
            if ( i->cName.empty() && !isNull ) {
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
                if ( as_ref )
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
                if ( as_ref )
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
                if ( i->ref /*|| x_IsAnyContentType(i)*/) {
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
        bool has_non_null_refs = false;
        bool as_ref;
        ITERATE ( TMembers, i, m_Members ) {
            as_ref = i->ref /*|| x_IsAnyContentType(i)*/;
            if ( (as_ref && !i->canBeNull) ) {
                has_non_null_refs = true;
            }
            else if ( !as_ref && !x_IsNullType(i) ) {
                string init = x_IsUniSeq(i) ? kEmptyStr : i->defaultValue;
                if ( init.empty() )
                    init = i->type->GetInitializer();
                if ( !init.empty() )
                    code.AddInitializer(i->mName, init);
            }
        }
        code.AddConstructionCode
            ("memset("SET_PREFIX",0,sizeof("SET_PREFIX"));\n");
        if ( has_non_null_refs ) {
            code.AddConstructionCode("if ( !IsAllocatedInPool() ) {\n");
            ITERATE ( TMembers, i, m_Members ) {
                as_ref = i->ref /*|| x_IsAnyContentType(i)*/;
                if ( (as_ref && !i->canBeNull) ) {
                    code.AddConstructionCode("    Reset"+i->cName+"();\n");
                }
            }
            code.AddConstructionCode("}\n");
        }
    }

    // generate Reset method
    if ( !wrapperClass ) {
        code.ClassPublic() <<
            "    /// Reset the whole object\n"
            "    ";
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
    
    SInternalNames names;
    string module_name = GetModuleName(&names);
    if ( !names.m_OwnerName.empty() ) {
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
            "    SET_CLASS_MODULE(\""<<module_name<<"\");\n";
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
            
            bool ref = i->ref /*|| x_IsAnyContentType(i)*/;
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
                bool defref(ref);
                string defTName(i->tName);
                string defset("->SetDefault(");

                if (x_IsUniSeq(i)) {
                    const CTemplate1TypeStrings* uniStr =
                        dynamic_cast<const CTemplate1TypeStrings*>(i->type.get());
                    if (uniStr) {
                        defset = "->SetElementDefault(";
                        const CTypeStrings* argStr = uniStr->GetArg1Type();
                        defref = argStr->GetKind() == CTypeStrings::eKindRef;
                        defTName = argStr->GetCType(code.GetNamespace());
                        if (defref) {
                            const CRefTypeStrings* refStr = dynamic_cast<const CRefTypeStrings*>(argStr);
                            if (refStr) {
                                defTName = refStr->GetDataTypeStr()->GetCType(code.GetNamespace());
                                defTName += "::Tdata";
                            }
                            defref = false;
                        }
                    }
                }
                methods << defset;
                if ( defref )
                    methods << "new NCBI_NS_NCBI::CRef< "+defTName+" >(";
                methods << "new "<<defTName<<"("<<i->defaultValue<<')';
                if ( defref )
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
            if (i->noPrefix) {
                methods << "->SetNoPrefix()";
            }
            if (i->attlist) {
                methods << "->SetAttlist()";
            }
            if (i->noTag) {
                methods << "->SetNotag()";
            }
            if (x_IsAnyContentType(i)) {
                methods << "->SetAnyContent()";
            }
            if ( i->memberTag >= 0 ) {
                methods << "->GetId().SetTag(" << i->memberTag << ")";
            }
            if (i->dataType) {
                const COctetStringDataType* octets =
                    dynamic_cast<const COctetStringDataType*>(i->dataType);
                if (octets) {
                    if (octets->IsCompressed()) {
                        methods << "->SetCompressed()";
                    }
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
            if (i->nonEmpty) {
                methods << "->SetNonEmpty()";
            }
            methods << ";\n";
        }
        if ( isSet ) {
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
            out << "dataspec_" << GetDoxygenModuleName();
        }
        out
            << "\n *\n"
            << " * @{\n"
            << " */\n\n";
    }
    bool wrapperClass = (m_Members.size() == 1) &&
        m_Members.front().cName.empty();
    bool generateCopy = wrapperClass && m_Members.front().type->CanBeCopied()
         && !x_IsNullType(m_Members.begin());

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
        if (i->simple && !x_IsNullType(i) && i->type->CanBeCopied()) {
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
        if (i->simple && !x_IsNullType(i) && i->type->CanBeCopied()) {
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
        if (i->simple && !x_IsNullType(i) && i->type->CanBeCopied()) {
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
        if (i->simple && !x_IsNullType(i) && i->type->CanBeCopied()) {
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
                                           const string& fileName,
                                           const CComments& comments)
    : CTypeStrings(comments),
      m_ClassName(className),
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
    const CReferenceDataType* ref =
        dynamic_cast<const CReferenceDataType*>(DataType());
    if (ref && ref->IsRefToParent()) {
        return;
    }
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

string CClassRefTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

END_NCBI_SCOPE
