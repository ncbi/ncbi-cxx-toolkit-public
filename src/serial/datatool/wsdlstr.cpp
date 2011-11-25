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
* Author: Andrei Gourianov
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
#include "value.hpp"

BEGIN_NCBI_SCOPE


CWsdlTypeStrings::CWsdlTypeStrings(
    const string& externalName,  const string& className,
    const string& namespaceName, const CDataType* dataType, const CComments& comments)
    : CParent(externalName, className, namespaceName, dataType, comments)
{
}

CWsdlTypeStrings::~CWsdlTypeStrings(void)
{
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

void CWsdlTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    string httpclient("CSoapHttpClient");
    string codeClassName = GetClassNameDT();
    bool haveUserClass = HaveUserClass();
    if ( haveUserClass )
        codeClassName += "_Base";
    CClassCode code(ctx, codeClassName);
    code.HPPIncludes().insert("serial/soap/soap_client");
    code.SetParentClass(httpclient, CNamespace::KNCBINamespace);
    string methodPrefix = code.GetMethodPrefix();

    DeclareConstructor(code.ClassPublic(), codeClassName);
    DeclareDestructor(code.ClassPublic(), codeClassName, haveUserClass);

    BeginClassDeclaration(ctx);
    GenerateClassCode(code,
                      code.ClassPublic(),
                      methodPrefix, haveUserClass, ctx.GetMethodPrefix());

    // constructors/destructor code
    string location("\"\"");
    ITERATE ( TMembers, i, m_Members ) {
        if (i->dataType->IsStdType() && i->externalName == "#location") {
            location = i->defaultValue;
        }
    }
    CNcbiOstream& methods = code.Methods();
    methods <<
        "// constructor\n"<<
        methodPrefix<<codeClassName<<"(void)\n";
    methods <<
        "    : " << httpclient << "(\n";
    methods <<
        "        " << location << ",\n" <<
        "        \"" << GetNamespaceName() << "\")\n";

    methods <<
        "{\n";
    code.WriteConstructionCode(methods);
    methods <<
        "}\n"
        "\n";

    methods <<
        "// destructor\n"<<
        methodPrefix<<"~"<<codeClassName<<"(void)\n"
        "{\n";
    code.WriteDestructionCode(methods);
    methods <<
        "}\n"
        "\n";
}

static inline
const CWsdlDataType* x_WsdlDataType(const CDataType *type)
{
    return dynamic_cast<const CWsdlDataType*>(type);
}

static
void x_CollectMembers(
    list<CWsdlTypeStrings::TMembers::const_iterator>& container,
    const CWsdlDataType* memb_type,
    const CWsdlTypeStrings::TMembers& members)
{
    CDataMemberContainerType::TMembers memin = memb_type->GetMembers();
    ITERATE( CDataMemberContainerType::TMembers, mi, memin) {
        const string& name = (*mi)->GetName();
        ITERATE ( CWsdlTypeStrings::TMembers, ii, members ) {
            if (!x_WsdlDataType(ii->dataType) && ii->externalName == name) {
                container.push_back(ii);
            }
        }
    }
}
static
bool x_IsNullDataType(CWsdlTypeStrings::TMembers::const_iterator i)
{
    if (i->ref && i->dataType) {
        return dynamic_cast<const CNullDataType*>(i->dataType->Resolve()) != 0;
    }
    return false;
}

void CWsdlTypeStrings::GenerateClassCode(
    CClassCode& code, CNcbiOstream& setters,
    const string& methodPrefix, bool haveUserClass,
    const string& classPrefix) const
{
    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);
    CNcbiOstream& methods = code.Methods();
    CNcbiOstream& header = code.ClassPublic();

// generate member methods
    ITERATE ( TMembers, i, m_Members ) {
        const CWsdlDataType* type = x_WsdlDataType(i->dataType);
        if (!type && i->externalName != "#location") {
            if ( i->ref ) {
                i->type->GeneratePointerTypeCode(code);
            }
            else {
                i->type->GenerateTypeCode(code);
            }
        }
    }

    set<string> regOut;
    ITERATE ( TMembers, i, m_Members ) {

// collect operation inputs and outputs
        const CWsdlDataType* type = x_WsdlDataType(i->dataType);
        if (!type || type->GetWsdlType() != CWsdlDataType::eWsdlOperation) {
            continue;
        }

        string soapaction("");
        list<TMembers::const_iterator> headerinputs;
        list<TMembers::const_iterator> inputs;
        list<TMembers::const_iterator> headeroutputs;
        list<TMembers::const_iterator> outputs;
        // operation
        CDataMemberContainerType::TMembers memb = type->GetMembers();
        ITERATE( CDataMemberContainerType::TMembers, m, memb) {
            const CWsdlDataType* memb_type = x_WsdlDataType((*m)->GetType());
            if (!memb_type) {
                const CDataType* st = (*m)->GetType();
                if (st->IsStdType() && st->GetMemberName() == "#soapaction" && st->GetDataMember()) {
                    soapaction = st->GetDataMember()->GetDefault()->GetXmlString();
                }
                continue;
            }
            switch (memb_type->GetWsdlType()) {
            case CWsdlDataType::eWsdlHeaderInput:
                x_CollectMembers( headerinputs,memb_type, m_Members);
                break;
            case CWsdlDataType::eWsdlInput:
                x_CollectMembers( inputs,memb_type, m_Members);
                break;
            case CWsdlDataType::eWsdlHeaderOutput:
                x_CollectMembers( headeroutputs,memb_type, m_Members);
                break;
            case CWsdlDataType::eWsdlOutput:
                x_CollectMembers( outputs,memb_type, m_Members);
                break;
            default:
                break;
            }
        }

// generate operation code

        //outputs
        string methodRet("void");
        list<string> methodOut;
        int out_counter=0;
        size_t out_total = headeroutputs.size() + outputs.size();

        ITERATE( list<TMembers::const_iterator>, out, headeroutputs) {
            ++out_counter;
            string reg("RegisterObjectType(");
            reg += (*out)->type->GetCType(code.GetNamespace());
            reg += "::GetTypeInfo);";
            regOut.insert(reg);
            string out_param = ncbiNamespace + "CConstRef< " +
                 (*out)->type->GetCType(code.GetNamespace()) + " >";
            if (out_total == 1) {
                methodRet = out_param;
            } else {
                out_param += "& out" + NStr::IntToString(out_counter);
                methodOut.push_back(out_param);
            }
        }
        ITERATE( list<TMembers::const_iterator>, out, outputs) {
            ++out_counter;
            string reg("RegisterObjectType(");
            reg += (*out)->type->GetCType(code.GetNamespace());
            reg += "::GetTypeInfo);";
            regOut.insert(reg);
            string out_param = ncbiNamespace + "CConstRef< " +
                 (*out)->type->GetCType(code.GetNamespace()) + " >";
            if (out_total == 1) {
                methodRet = out_param;
            } else {
                out_param += "& out" + NStr::IntToString(out_counter);
                methodOut.push_back(out_param);
            }
        }

        string separator("\n    ");
        string hpp_separator("\n        ");
        string comma_separator = "," + separator;
        string commahpp_separator = "," + hpp_separator;
        string inout_separator = "," + separator;
        string inouthpp_separator = "," + hpp_separator;
        if (methodOut.empty()) {
            inout_separator = "";
            inouthpp_separator = "";
        }

        // inputs
        list<string> methodIn;
        list<string> methodInNull;
        int in_counter=0, hin_counter=0;
        ITERATE( list<TMembers::const_iterator>, in, headerinputs) {
            ++in_counter;
            string in_type((*in)->type->GetCType(code.GetNamespace()));
            string in_name(string("in") + NStr::IntToString(in_counter));
            if (x_IsNullDataType(*in)) {
                methodInNull.push_back(in_type + " " + in_name + ";");
                methodInNull.push_back(in_name + ".Set();");
            } else {
                methodIn.push_back(string("const ") + in_type + "& " + in_name);
            }
        }
        hin_counter = in_counter;
        ITERATE( list<TMembers::const_iterator>, in, inputs) {
            ++in_counter;
            string in_type((*in)->type->GetCType(code.GetNamespace()));
            string in_name(string("in") + NStr::IntToString(in_counter));
            if (x_IsNullDataType(*in)) {
                methodInNull.push_back(in_type + " " + in_name + ";");
                methodInNull.push_back(in_name + ".Set();");
            } else {
                methodIn.push_back(string("const ") + in_type + "& " + in_name);
            }
        }
        methodIn.push_back(
            ncbiNamespace + "CConstRef< " + ncbiNamespace + "CSoapFault" + " >* fault");

        // declaration
        header << "\n";
        i->comments.PrintHPPMember(header);
        header << "    "
            << methodRet << separator << i->externalName << "(";
        header << hpp_separator
            << NStr::Join(methodOut,commahpp_separator)
            << inouthpp_separator
            << NStr::Join(methodIn,commahpp_separator)
            << " = 0) const;\n";

        // definition
        methods
            << methodRet << "\n"
            << methodPrefix << i->externalName << "(";
        methods << separator
            << NStr::Join(methodOut,comma_separator)
            << inout_separator
            << NStr::Join(methodIn,comma_separator)
            << ") const\n{";
        if (!methodInNull.empty()) {
            methods << separator
                << NStr::Join(methodInNull,separator);
        }
        methods << separator
            << ncbiNamespace << "CSoapMessage request, response;";
        for (int p=1; p <= in_counter; ++p) {
            methods << separator
                << "request.AddObject( in" << p
                << ", "<< ncbiNamespace;
            if (p <= hin_counter) {
                methods <<  "CSoapMessage::eMsgHeader";
            } else {
                methods <<  "CSoapMessage::eMsgBody";
            }
            methods <<  ");";
        }
        methods << separator << "Invoke(response,request,fault";
        if (!soapaction.empty()) {
            methods << "," << "\"" << soapaction << "\"";
        }
        methods << ");";
        out_counter=0;
        ITERATE( list<TMembers::const_iterator>, out, headeroutputs) {
            ++out_counter;
            methods << separator;
            if (out_total == 1) {
                methods << "return ";
            } else {
                methods << "out" << out_counter << " = ";
            }
            methods
                << ncbiNamespace << "SOAP_GetKnownObject< "
                << (*out)->type->GetCType(code.GetNamespace())
                << " >(response, " << ncbiNamespace << "CSoapMessage::eMsgHeader);";
        }
        ITERATE( list<TMembers::const_iterator>, out, outputs) {
            ++out_counter;
            methods << separator;
            if (out_total == 1) {
                methods << "return ";
            } else {
                methods << "out" << out_counter << " = ";
            }
            methods
                << ncbiNamespace << "SOAP_GetKnownObject< "
                << (*out)->type->GetCType(code.GetNamespace())
                << " >(response, " << ncbiNamespace << "CSoapMessage::eMsgBody);";
        }
        methods << "\n}\n\n";
    }
    ITERATE( set<string>, reg, regOut) {
        code.AddConstructionCode(*reg);
    }
}

END_NCBI_SCOPE
