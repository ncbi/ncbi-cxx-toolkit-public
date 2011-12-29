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
#include "exceptions.hpp"
#include "type.hpp"
#include "aliasstr.hpp"
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
      m_RefType(&ref_type)
{
}

CAliasTypeStrings::~CAliasTypeStrings(void)
{
}

CAliasTypeStrings::EKind CAliasTypeStrings::GetKind(void) const
{
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
    return m_RefType->GetInitializer();
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
    string className = GetClassName() + "_Base";
    CClassCode code(ctx, className);
    string methodPrefix = code.GetMethodPrefix();
    bool is_class = false;

    BeginClassDeclaration(ctx);
    switch ( m_RefType->GetKind() ) {
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

    string parentNamespaceRef =
        code.GetNamespace().GetNamespaceRef(code.GetParentClassNamespace());
    // generate type info
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
        code.ClassPublic() <<
            "    // type info\n"
            "    DECLARE_INTERNAL_TYPE_INFO();\n"
            "\n";
//        m_RefType->GenerateTypeCode(ctx);
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
        code.MethodStart(true) <<
            "NCBI_NS_NCBI::CNcbiOstream& operator<<\n" <<
            "(NCBI_NS_NCBI::CNcbiOstream& str, const " << className << "& obj)\n" <<
            "{\n" <<
            "    return WriteObject(str,&obj,obj.GetTypeInfo());\n" <<
            "}\n\n";
        code.MethodStart(true) <<
            "NCBI_NS_NCBI::CNcbiIstream& operator>>\n" <<
            "(NCBI_NS_NCBI::CNcbiIstream& str, " << className << "& obj)\n" <<
            "{\n" <<
            "    return ReadObject(str,&obj,obj.GetTypeInfo());\n" <<
            "}\n\n";
    }

    // define typeinfo method
    {
//        code.CPPIncludes().insert("serial/aliasinfo");
        CNcbiOstream& methods = code.Methods();
        methods << "BEGIN";
        if (dynamic_cast<const CEnumRefTypeStrings*>(m_RefType.get())) {
            methods << "_ENUM";
        }
        methods << "_ALIAS_INFO(\""
            << GetExternalName() << "\", "
            << GetClassName() << ", "
            << m_RefType->GetRef(ns) << ")\n"
            "{\n";
        if ( !GetModuleName().empty() ) {
            methods <<
                "    SET_ALIAS_MODULE(\"" << GetModuleName() << "\");\n";
        }
        methods << "    SET_";
        if ( is_class ) {
            methods << "CLASS";
        }
        else {
            methods << "STD";
        }
        methods <<
            "_ALIAS_DATA_PTR;\n"
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
    switch ( m_RefType->GetKind() ) {
    case eKindClass:
    case eKindObject:
        is_class = true;
        break;
    default:
        is_class = false;
        break;
    }
    if ( !is_class ) {
        // Generate type convertions
        out <<
            "    /// Explicit constructor from the primitive type.\n" <<
            "    explicit " << className + "(const " + ref_name + "& data)" << "\n"
            "        : Tparent(data) {}\n\n";
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
    m_RefType->GenerateTypeCode(ctx);
}

void CAliasTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
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
