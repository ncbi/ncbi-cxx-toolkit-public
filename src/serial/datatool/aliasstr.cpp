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
* Author: Aleksey Grichenko
*
* File Description:
*   Type info for aliased type generation: includes, used classes, C code etc.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiutil.hpp>
#include "datatool.hpp"
#include "exceptions.hpp"
#include "type.hpp"
#include "aliasstr.hpp"
#include "reftype.hpp"
#include "statictype.hpp"
#include "stdstr.hpp"
#include "code.hpp"
#include "srcutil.hpp"
#include "classstr.hpp"
#include "enumstr.hpp"
#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

CAliasTypeStrings::CAliasTypeStrings(const string& externalName,
                                     const string& className,
                                     CTypeStrings& ref_type,
                                     const CComments& comments)
    : TParent(comments),
      m_ExternalName(externalName),
      m_ClassName(className),
      m_RefType(&ref_type),
      m_FullAlias(false),
      m_Nested(false)
{
}

CAliasTypeStrings::~CAliasTypeStrings(void)
{
}

CAliasTypeStrings::EKind CAliasTypeStrings::GetKind(void) const
{
    if (DataType() && DataType()->IsTypeAlias()) {
        EKind kind = m_RefType->GetKind();
        if (kind == eKindStd || kind == eKindEnum || kind == eKindString) {
            return eKindClass;
        }
        return eKindObject;
    }
    return eKindOther;
}

string CAliasTypeStrings::GetClassName(void) const
{
    return m_ClassName;
}

string CAliasTypeStrings::GetExternalName(void) const
{
    return m_ExternalName;
}

string CAliasTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return GetClassName();
}

string CAliasTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                           const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

bool CAliasTypeStrings::HaveSpecialRef(void) const
{
    return m_RefType->HaveSpecialRef();
}

string CAliasTypeStrings::GetRef(const CNamespace& ns) const
{
    if (DataType() && DataType()->IsTypeAlias()) {
        return "CLASS, ("+GetCType(ns)+')';
    }
    return m_RefType->GetRef(ns);
}

bool CAliasTypeStrings::CanBeKey(void) const
{
    return m_RefType->CanBeKey();
}

bool CAliasTypeStrings::CanBeCopied(void) const
{
    return m_RefType->CanBeCopied();
}

string CAliasTypeStrings::NewInstance(const string& init,
                                      const string& place) const
{
    return m_RefType->NewInstance(init, place);
}

string CAliasTypeStrings::GetInitializer(void) const
{
    string r = m_RefType->GetInitializer();
    CTypeStrings::EKind kind = m_RefType->GetKind();
    if (!r.empty() && kind != eKindClass && kind != eKindObject)
    {
        r = GetClassName() + "(" + r + ")";
    }
    return r;
}

string CAliasTypeStrings::GetDestructionCode(const string& expr) const
{
    return m_RefType->GetDestructionCode(expr);
}

string CAliasTypeStrings::GetIsSetCode(const string& var) const
{
    return m_RefType->GetIsSetCode(var);
}

string CAliasTypeStrings::GetResetCode(const string& var) const
{
    return m_RefType->GetResetCode(var);
}

string CAliasTypeStrings::GetDefaultCode(const string& var) const
{
    // No value - return empty string
    if ( var.empty() ) {
        return NcbiEmptyString;
    }
    // Return var for classes, else cast var to the aliased type
    return GetCType(GetNamespace()) + "(" + var + ")";
}

void CAliasTypeStrings::GenerateCode(CClassContext& ctx) const
{
    const CNamespace& ns = ctx.GetNamespace();
    string ref_name = m_RefType->GetCType(ns);
    string className;
    if (m_Nested) {
        className = GetClassName();
    } else {
        className = GetClassName() + "_Base";
    }
    CClassCode code(ctx, className);
    string methodPrefix = code.GetMethodPrefix();
    string classFullName(GetClassName());
    if (m_Nested) {
        classFullName = NStr::TrimSuffix_Unsafe(methodPrefix, "::");
    }
    bool is_class = false;
    bool is_ref_to_alias = false;
    const CClassTypeStrings::SMemberInfo* mem_alias = nullptr;
    bool mem_isnull = false;
    bool type_alias = DataType() && DataType()->IsTypeAlias();
    AutoPtr<CTypeStrings> mem_type;

    CTypeStrings::EKind kind = m_RefType->GetKind();
    switch ( kind ) {
    case eKindClass:
    case eKindObject:
        {
            string name(ref_name);
            const CClassRefTypeStrings* cls =
                dynamic_cast<const CClassRefTypeStrings*>(m_RefType.get());
            if (cls) {
                name = cls->GetClassName();
            }
            code.SetParentClass(name, m_RefType->GetNamespace());
        }
        is_class = true;
        is_ref_to_alias = (dynamic_cast<const CAliasRefTypeStrings*>(m_RefType.get()) != NULL);

// only if we have two members, and one of them is attlist
// that is, if it was created by DTDParser::CompositeNode
        if (!is_ref_to_alias && DataType() && !DataType()->IsASNDataSpec() && DataType()->IsReference()) {
            const CReferenceDataType* reftype = dynamic_cast<const CReferenceDataType*>(DataType());
            const CDataType* resolved = reftype->Resolve();
            if (resolved && resolved != reftype) {
                CClassTypeStrings* typeStr = resolved->GetTypeStr();
                bool has_attlist = false;
                if (typeStr && typeStr->m_Members.size() == 2) {
                    for ( const auto& ir: typeStr->m_Members ) {
                        if (ir.attlist) {
                            has_attlist = true;
                        } else {
                            mem_alias = &ir;
                            mem_isnull = ir.haveFlag ? (dynamic_cast<CNullTypeStrings*>(ir.type.get()) != 0) : false;
                        }
                    }
                    if (!has_attlist || mem_alias->externalName != reftype->GetUserTypeName()) {
                        mem_alias = nullptr;
                    }
                } else {
                    mem_isnull = dynamic_cast<const CNullDataType*>(resolved) != nullptr;
                }
            }
        }
        break;
    case eKindStd:
    case eKindEnum:
        code.SetParentClass("CStdAliasBase< " + ref_name + " >",
            CNamespace::KNCBINamespace);
        break;
    case eKindString:
        code.SetParentClass("CStringAliasBase< " + ref_name + " >",
            CNamespace::KNCBINamespace);
        break;
    case eKindOther: // for vector< char >
        code.SetParentClass("CStringAliasBase< " + ref_name + " >",
            CNamespace::KNCBINamespace);
        break;
    case eKindPointer:
    case eKindRef:
    case eKindContainer:
        NCBI_THROW(CDatatoolException, eNotImplemented,
            "Invalid aliased type: " + ref_name);
    }

    if (m_Nested && type_alias && !mem_alias && !DataType()->HasTag() &&
        !DataType()->IsInUniSeq() &&
        DataType()->GetTagType() == CAsnBinaryDefs::eAutomatic)
    {
        bool is_std = kind == eKindStd || kind == eKindEnum || kind == eKindString || kind == eKindOther;
        if (!is_std) {
            m_RefType->GenerateTypeCode(ctx);
            code.SetEmptyClassCode();
            return;
        }
    }

    string parentNamespaceRef =
        code.GetNamespace().GetNamespaceRef(code.GetParentClassNamespace());
    BeginClassDeclaration(ctx);
    // constructor
    code.ClassPublic() <<
        "    " << className << "(void);\n" <<
        "\n";
    code.MethodStart(true) <<
        methodPrefix << className << "(void)\n" <<
        "{\n" <<
        "}\n" <<
        "\n";
    m_RefType->GenerateTypeCode(ctx);
    if ( is_class ) {
        if (is_ref_to_alias) {
            code.ClassPublic() <<
                "    // type info\n"
                "    DECLARE_STD_ALIAS_TYPE_INFO();\n"
                "\n";
        } else {
            code.ClassPublic() <<
                "    // type info\n"
                "    DECLARE_INTERNAL_TYPE_INFO();\n"
                "\n";
        }
//        m_RefType->GenerateTypeCode(ctx);

// I have strong feeling that this was a bad idea
// and these methods should be removed
        if (/*!mem_isnull &&*/ !type_alias) {
            code.ClassPublic() <<
                "    // parent type getter/setter\n" <<
                "    const " << ref_name << "& Get(void) const;\n" <<
                "    " << ref_name << "& Set(void);\n";
            code.MethodStart(true) <<
                "const " << ref_name << "& " << methodPrefix << "Get(void) const\n" <<
                "{\n" <<
                "    return *this;\n" <<
                "}\n\n";
            code.MethodStart(true) <<
                ref_name << "& " << methodPrefix << "Set(void)\n" <<
                "{\n" <<
                "    return *this;\n" <<
                "}\n\n";
        }

        if (type_alias && mem_alias) {
            string extname(m_ClassName);
            if (NStr::StartsWith(extname, "C_")) {
                NStr::TrimPrefixInPlace(extname, "C_");
            } else if (NStr::StartsWith(extname, "C")) {
                NStr::TrimPrefixInPlace(extname, "C");
            }
            string mem_extname(mem_alias->cName);
            code.ClassPublic() << "\n";
            if (!mem_isnull) {
                code.ClassPublic() << "    typedef "
                    << "Tparent::" << mem_alias->tName << " T" << extname << ";\n";
            }
            code.ClassPublic() <<
                "    bool IsSet" << extname << "(void) const {\n" <<
                "        return Tparent::IsSet" << mem_extname << "();\n" <<
                "    }\n";
            code.ClassPublic() <<
                "    bool CanGet" << extname << "(void) const {\n" <<
                "        return Tparent::CanGet" << mem_extname << "();\n" <<
                "    }\n";
            code.ClassPublic() <<
                "    void Reset" << extname << "(void) {\n" <<
                "        Tparent::Reset" << mem_extname << "();\n" <<
                "    }\n";
            if (mem_isnull) {
                code.ClassPublic() << "    " <<
                    "void" << " Set" << extname << "(void) {\n" <<
                    "        Tparent::Set" << mem_extname << "();\n" <<
                    "    }\n";
            } else {
                code.ClassPublic() << "    const " <<
                    "T" << extname << "& Get" << extname << "(void) const {\n" <<
                    "        return Tparent::Get" << mem_extname << "();\n" <<
                    "    }\n";
                code.ClassPublic() << "    " <<
                    "T" << extname << "& Set" << extname << "(void) {\n" <<
                    "        return Tparent::Set" << mem_extname << "();\n" <<
                    "    }\n";
//                if (mem_alias->dataType && mem_alias->dataType->IsPrimitive()) {
                if (mem_alias->type->CanBeCopied()) {
                    code.ClassPublic() << "    " <<
                        "void" << " Set" << extname << "(const T"  << extname << "& value) {\n" <<
                        "        Tparent::Set" << mem_extname << "(value);\n" <<
                        "    }\n";
                    if (mem_alias->simple && m_Nested) {
                        code.ClassPublic() << "    " <<
                            className << "(const" << " T" << extname << "& value) : Tparent(value) {\n    }\n";
                        code.ClassPublic() << "    " <<
                            className << "& operator=(const" << " T" << extname << "& value) {\n" <<
                            "        Tparent::operator=(value);\n" <<
                            "        return *this;\n" <<
                            "    }\n";
                    }
                }
            }
        }
    }
    else {
        code.ClassPublic() <<
            "    // type info\n"
            "    DECLARE_STD_ALIAS_TYPE_INFO();\n"
            "\n";
        string constr_decl = className + "(const " + ref_name + "& data)";
        code.ClassPublic() <<
            "    // explicit constructor from the primitive type\n" <<
            "    explicit " << constr_decl << ";\n";
        code.MethodStart(true) <<
            methodPrefix << constr_decl << "\n" <<
            "    : " << parentNamespaceRef << code.GetParentClassName() << "(data)\n" <<
            "{\n" <<
            "}\n" <<
            "\n";
        
        // I/O operators
        bool kindIO = kind == eKindStd    || kind == eKindEnum ||
                      kind == eKindString || kind == eKindPointer;
        code.MethodStart(true) <<
            "NCBI_NS_NCBI::CNcbiOstream& operator<<\n" <<
            "(NCBI_NS_NCBI::CNcbiOstream& str, const " << (m_Nested ? classFullName : className) << "& obj)\n" <<
            "{\n";
        if (kindIO) {
            code.Methods(true) <<
                "    if (NCBI_NS_NCBI::MSerial_Flags::HasSerialFormatting(str)) {\n" <<
                "        return WriteObject(str,&obj,obj.GetTypeInfo());\n" <<
                "    }\n" <<
                "    str << obj.Get();\n" <<
                "    return str;\n";
        } else {
            code.Methods(true) <<
                "    return WriteObject(str,&obj,obj.GetTypeInfo());\n";
        }
        code.Methods(true) << "}\n\n";
        code.MethodStart(true) <<
            "NCBI_NS_NCBI::CNcbiIstream& operator>>\n" <<
            "(NCBI_NS_NCBI::CNcbiIstream& str, " << (m_Nested ? classFullName : className) << "& obj)\n" <<
            "{\n";
        if (kindIO) {
            code.Methods(true) <<
                "    if (NCBI_NS_NCBI::MSerial_Flags::HasSerialFormatting(str)) {\n" <<
                "        return ReadObject(str,&obj,obj.GetTypeInfo());\n" <<
                "    }\n";
            if (kind == eKindEnum && m_RefType->GetEnumName() == ref_name)
            {
                code.Methods(true) <<
                    "    std::underlying_type<" << ref_name <<">::type v;\n" <<
                    "    str >> v;\n" <<
                    "    obj.Set((" << ref_name << ")v);\n";
            }
            else
            {
                code.Methods(true) <<
                    "    str >> obj.Set();\n";
            }
            code.Methods(true) <<
                "    return str;\n";
        } else {
            code.Methods(true) <<
                "    return ReadObject(str,&obj,obj.GetTypeInfo());\n";
        }
        code.Methods(true) << "}\n\n";
    }

    // define typeinfo method
    {
//        code.CPPIncludes().insert("serial/aliasinfo");
        CNcbiOstream& methods = code.Methods();
        methods << "BEGIN";
        if (m_Nested) {
            methods << "_NESTED";
        }
        if (dynamic_cast<const CEnumRefTypeStrings*>(m_RefType.get())) {
            methods << "_ENUM";
        }
        methods << "_ALIAS_INFO(\""
            << GetExternalName() << "\", "
            << classFullName << ", "
            << m_RefType->GetRef(ns) << ")\n"
            "{\n";
        if ( !GetModuleName().empty() ) {
            methods <<
                "    SET_ALIAS_MODULE(\"" << GetModuleName() << "\");\n";
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
        methods << "    SET_";
        if ( is_class ) {
            methods << "CLASS";
        }
        else {
            methods << "STD";
        }
        methods <<
            "_ALIAS_DATA_PTR;\n";
        if (mem_alias || type_alias || this->IsFullAlias()) {
            methods <<
                "    SET_FULL_ALIAS;\n";
        }
        methods <<  "    info->CodeVersion(" << DATATOOL_VERSION << ");\n";
        methods <<  "    info->DataSpec(" << CDataType::GetSourceDataSpecString() << ");\n";
        methods <<
            "}\n"
            "END_ALIAS_INFO\n"
            "\n";
    }
}

void CAliasTypeStrings::GenerateUserHPPCode(CNcbiOstream& out) const
{
    // m_RefType->GenerateUserHPPCode(out);
    const CNamespace& ns = GetNamespace();
    string ref_name = m_RefType->GetCType(ns);
    string className = GetClassName();
    if (CClassCode::GetDoxygenComments()) {
        out
            << "\n"
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
    out <<
        "/////////////////////////////////////////////////////////////////////////////\n";
    if (CClassCode::GetDoxygenComments()) {
        out <<
            "///\n"
            "/// " << className << " --\n"
            "///\n\n";
    }
    out << "class ";
    if ( !CClassCode::GetExportSpecifier().empty() )
        out << CClassCode::GetExportSpecifier() << " ";
    out << GetClassName()<<" : public "<<GetClassName()<<"_Base\n"
        "{\n"
        "    typedef "<<GetClassName()<<"_Base Tparent;\n"
        "public:\n";
    out <<
        "    " << GetClassName() << "(void) {}\n"
        "\n";

    bool is_class = false;
    const CClassTypeStrings::SMemberInfo* mem_alias = nullptr;
    bool mem_isnull = false;
    AutoPtr<CTypeStrings> mem_type;
    switch ( m_RefType->GetKind() ) {
    case eKindClass:
    case eKindObject:
        is_class = true;
        if (DataType()->IsReference()) {
            const CReferenceDataType* reftype = dynamic_cast<const CReferenceDataType*>(DataType());
            const CDataType* resolved = reftype->Resolve();
            if (resolved && resolved != reftype) {
                CClassTypeStrings* typeStr = resolved->GetTypeStr();
                bool has_attlist = false;
                if (typeStr) {
                    if (typeStr->m_Members.size() == 2) {
                        for ( const auto& ir: typeStr->m_Members ) {
                            if (ir.attlist) {
                                has_attlist = true;
                            } else {
                                mem_alias = &ir;
                                mem_isnull = ir.haveFlag ? (dynamic_cast<CNullTypeStrings*>(ir.type.get()) != 0) : false;
                            }
                        }
                        if (!has_attlist || mem_alias->externalName != reftype->GetUserTypeName()) {
                            mem_alias = nullptr;
                        }
                    }
                    if (mem_alias && mem_alias->dataType && !mem_alias->dataType->IsStdType()) {
                        mem_alias = nullptr;
                    }
                } else {
                    mem_isnull = dynamic_cast<const CNullDataType*>(resolved) != nullptr;
                    if (!mem_isnull && !resolved->IsAlias() && resolved->IsStdType()) {
                        mem_type = resolved->GenerateCode();
                        const CClassTypeStrings* mem_class = dynamic_cast<const CClassTypeStrings*>(mem_type.get());
                        if (mem_class && mem_class->m_Members.size() == 1) {
                            mem_alias = &(mem_class->m_Members.front());
                        }
                    }
                }
            }
        }
        break;
    default:
        is_class = false;
        break;
    }
    if ( !is_class ) {
        // Generate type convertions
        out <<
            "    /// Explicit constructor from the primitive type.\n" <<
            "    explicit " << className + "(const " + ref_name + "& value)" << "\n"
            "        : Tparent(value) {}\n\n";
    }
    else if (mem_alias) {
#if 0
        string tname = mem_alias->type->GetCType(ns); // or mem_alias->tName?
#else
        string tname = "Tparent::" + mem_alias->tName;
        out << "    // typedef " << mem_alias->type->GetCType(ns) << " " << mem_alias->tName << ";\n\n";
#endif
        string mem_extname(mem_alias->cName);
        out <<
            "    /// Constructor from the primitive type.\n" <<
            "    " << className << "(const " << tname << "& value) {\n" <<
            "        Set" << mem_extname << "(value);\n" <<
            "    }\n";
        out <<
            "    /// Assignment operator\n" <<
            "    " << className << "& operator=(const " << tname << "& value) {\n" <<
            "        Set" << mem_extname << "(value);\n" <<
            "        return *this;\n" <<
            "    }\n";
#if 0
        out <<
            "    /// Conversion operator\n" <<
            "    operator const " << tname << "& (void) {\n" <<
            "        return Get" << mem_extname << "();\n" <<
            "    }\n" <<
            "    operator " << tname << "& (void) {\n" <<
            "        return Set" << mem_extname << "();\n" <<
            "    }\n";
#endif
    }
    out << "};\n";
    if (CClassCode::GetDoxygenComments()) {
        out << "/* @} */\n";
    }
    out << "\n";
}

void CAliasTypeStrings::GenerateUserCPPCode(CNcbiOstream& /* out */) const
{
    //m_RefType->GenerateUserCPPCode(out);
}

void CAliasTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    if (DataType()->IsTypeAlias()) {
        m_Nested = true;
        GenerateCode(ctx);
        m_Nested = false;
        return;
    }
    m_RefType->GenerateTypeCode(ctx);
}

void CAliasTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    if (DataType()->IsTypeAlias()) {
        m_Nested = true;
        GenerateCode(ctx);
        m_Nested = false;
        return;
    }
    m_RefType->GeneratePointerTypeCode(ctx);
}


CAliasRefTypeStrings::CAliasRefTypeStrings(const string& className,
                                           const CNamespace& ns,
                                           const string& fileName,
                                           CTypeStrings& ref_type,
                                           const CComments& comments)
    : CParent(comments),
      m_ClassName(className),
      m_Namespace(ns),
      m_FileName(fileName),
      m_RefType(&ref_type),
      m_IsObject(m_RefType->GetKind() == eKindObject)
{
}


CTypeStrings::EKind CAliasRefTypeStrings::GetKind(void) const
{
    return m_IsObject ? eKindObject : eKindClass;
}

const CNamespace& CAliasRefTypeStrings::GetNamespace(void) const
{
    return m_Namespace;
}

void CAliasRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

void CAliasRefTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    ctx.AddForwardDeclaration(m_ClassName, m_Namespace);
    ctx.CPPIncludes().insert(m_FileName);
}

string CAliasRefTypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(m_Namespace) + m_ClassName;
}

string CAliasRefTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                              const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

bool CAliasRefTypeStrings::HaveSpecialRef(void) const
{
    return m_RefType->HaveSpecialRef();
}

string CAliasRefTypeStrings::GetRef(const CNamespace& ns) const
{
    return "CLASS, ("+GetCType(ns)+')';
}

bool CAliasRefTypeStrings::CanBeKey(void) const
{
    return m_RefType->CanBeKey();
}

bool CAliasRefTypeStrings::CanBeCopied(void) const
{
    return m_RefType->CanBeCopied();
}

string CAliasRefTypeStrings::GetInitializer(void) const
{
    return m_IsObject ?
        NcbiEmptyString :
        GetDefaultCode(m_RefType->GetInitializer());
}

string CAliasRefTypeStrings::GetDestructionCode(const string& expr) const
{
    return m_IsObject ?
        NcbiEmptyString :
        m_RefType->GetDestructionCode(expr);
}

string CAliasRefTypeStrings::GetIsSetCode(const string& var) const
{
    return m_RefType->GetIsSetCode(var);
}

string CAliasRefTypeStrings::GetResetCode(const string& var) const
{
    return m_IsObject ?
        var + ".Reset();\n" :
        m_RefType->GetResetCode(var + ".Set()");
}

string CAliasRefTypeStrings::GetDefaultCode(const string& var) const
{
    // No value - return empty string
    if ( m_IsObject  ||  var.empty() ) {
        return NcbiEmptyString;
    }
    // Cast var to the aliased type
    return GetCType(GetNamespace()) + "(" + var + ")";
}

void CAliasRefTypeStrings::GenerateCode(CClassContext& ctx) const
{
    m_RefType->GenerateCode(ctx);
}

void CAliasRefTypeStrings::GenerateUserHPPCode(CNcbiOstream& out) const
{
    m_RefType->GenerateUserHPPCode(out);
}

void CAliasRefTypeStrings::GenerateUserCPPCode(CNcbiOstream& out) const
{
    m_RefType->GenerateUserCPPCode(out);
}


END_NCBI_SCOPE
