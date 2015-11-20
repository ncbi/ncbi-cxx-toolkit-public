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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   ASN.1/XML RPC client generator
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "rpcgen.hpp"

#include "choicetype.hpp"
#include "classstr.hpp"
#include "code.hpp"
#include "generate.hpp"
#include "srcutil.hpp"
#include "statictype.hpp"
#include "stdstr.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_RPCGen

BEGIN_NCBI_SCOPE


// Does all the actual work
class CClientPseudoTypeStrings : public CClassTypeStrings
{
public:
    CClientPseudoTypeStrings(const CClientPseudoDataType& source);

    void GenerateClassCode(CClassCode& code, CNcbiOstream& getters,
                           const string& methodPrefix, bool haveUserClass,
                           const string& classPrefix) const;

private:
    const CClientPseudoDataType& m_Source;
};


static void s_SplitName(const string& name, string& type, string& field)
{
    for (SIZE_TYPE pos = name.find('.');  pos != NPOS;
         pos = name.find('.', pos + 1)) {
        if (islower((unsigned char) name[pos + 1])) {
            type.assign(name, 0, pos);
            field.assign(name, pos + 1, NPOS);
            return;
        }
    }
    type.assign(name);
    field.erase();
}


static const CChoiceDataType* s_ChoiceType(const CDataType* dtype,
                                           const string& element)
{
    vector<string> v;
    if ( !element.empty() ) {
        NStr::Split(element, ".", v, NStr::fSplit_NoMergeDelims);
    }
    ITERATE (vector<string>, subelement, v) {
        const CDataMemberContainerType* dct
            = dynamic_cast<const CDataMemberContainerType*>(dtype);
        if ( !dct ) {
            NCBI_THROW(CDatatoolException, eInvalidData,
                       dtype->GlobalName() + " is not a container type");
        }
        bool found = false;
        ITERATE (CDataMemberContainerType::TMembers, it, dct->GetMembers()) {
            if ((*it)->GetName() == *subelement) {
                found = true;
                dtype = (*it)->GetType()->Resolve();
                break;
            }
        }
        if (!found) {
            NCBI_THROW(CDatatoolException, eInvalidData,
                       dtype->GlobalName() + " has no element " + *subelement);
        }
    }
    const CChoiceDataType* choicetype
        = dynamic_cast<const CChoiceDataType*>(dtype);
    if ( !choicetype ) {
        NCBI_THROW(CDatatoolException, eInvalidData,
                   dtype->GlobalName() + " is not a choice type");
    }
    return choicetype;
}


static string s_SetterName(const string& element) {
    if (element.empty()) {
        return kEmptyStr;
    }
    SIZE_TYPE start = 0, dot;
    string    result;
    do {
        dot = element.find('.', start);
        result += ".Set" + Identifier(element.substr(start, dot - start))
            + "()";
        start = dot + 1;
    } while (dot != NPOS);
    return result;
}


CClientPseudoDataType::CClientPseudoDataType(const CCodeGenerator& generator,
                                             const string& section_name,
                                             const string& class_name)
    : m_Generator(generator), m_SectionName(section_name),
      m_ClassName(class_name)
{
    // Just take the first potential module; should normally give sane
    // results.
    SetParent(generator.GetMainModules().GetModuleSets().front()
              ->GetModules().front().get(),
              class_name);

    s_SplitName(generator.GetConfig().Get(m_SectionName, "request"),
                m_RequestType, m_RequestElement);
    s_SplitName(generator.GetConfig().Get(m_SectionName, "reply"),
                m_ReplyType, m_ReplyElement);
    if (m_RequestType.empty()) {
        NCBI_THROW(CDatatoolException, eInvalidData,
                   "No request type supplied for " + m_ClassName);
    } else if (m_ReplyType.empty()) {
        NCBI_THROW(CDatatoolException, eInvalidData,
                   "No reply type supplied for " + m_ClassName);
    }

    m_RequestDataType = m_Generator.ResolveMain(m_RequestType);
    m_ReplyDataType = m_Generator.ResolveMain(m_ReplyType);
    _ASSERT(m_RequestDataType  &&  m_ReplyDataType);
    m_RequestChoiceType = s_ChoiceType(m_RequestDataType, m_RequestElement);
    m_ReplyChoiceType   = s_ChoiceType(m_ReplyDataType,   m_ReplyElement);
}


AutoPtr<CTypeStrings>
CClientPseudoDataType::GenerateCode(void) const
{
    return new CClientPseudoTypeStrings(*this);
}


CClientPseudoTypeStrings::CClientPseudoTypeStrings
(const CClientPseudoDataType& source)
    : CClassTypeStrings(kEmptyStr, source.m_ClassName, kEmptyStr, NULL, source.Comments()), m_Source(source)
{
    // SetClassNamespace(generator.GetNamespace()); // not defined(!)
    SetParentClass("CRPCClient<" + source.m_RequestDataType->ClassName()
                   + ", " + source.m_ReplyDataType->ClassName() + '>',
                   CNamespace::KNCBINamespace, "serial/rpcbase");
    SetObject(true);
    SetHaveUserClass(true);
    SetHaveTypeInfo(false);
}


static string s_QualClassName(const CDataType* dt)
{
    _ASSERT(dt);
    string result;
    const CDataType* parent = dt->GetParentType();
    if (parent) {
        result = s_QualClassName(parent) + "::";
    }
    result += dt->ClassName();
    return result;
}


void CClientPseudoTypeStrings::GenerateClassCode(CClassCode& code,
                                                 CNcbiOstream& /* getters */,
                                                 const string& /* methodPfx */,
                                                 bool /* haveUserClass */,
                                                 const string& /* classPfx */)
    const
{
    const string&         sect_name  = m_Source.m_SectionName;
    const string&         class_name = m_Source.m_ClassName;
    string                class_base = class_name + "_Base";
    const CCodeGenerator& generator  = m_Source.m_Generator;
    string                treq       = class_base + "::TRequest";
    string                trep       = class_base + "::TReply";
    const CNamespace&     ns         = code.GetNamespace();

    // Pull in the relevant headers, and add corresponding typedefs
    code.HPPIncludes().insert(m_Source.m_RequestDataType->FileName());
    code.ClassPublic() << "    typedef "
                       << m_Source.m_RequestDataType->ClassName()
                       << " TRequest;\n";
    code.HPPIncludes().insert(m_Source.m_ReplyDataType->FileName());
    code.ClassPublic() << "    typedef "
                       << m_Source.m_ReplyDataType->ClassName()
                       << " TReply;\n";
    if ( !m_Source.m_RequestElement.empty() ) {
        code.HPPIncludes().insert(m_Source.m_RequestChoiceType->FileName());
        code.ClassPublic() << "    typedef "
                           << s_QualClassName(m_Source.m_RequestChoiceType)
                           << " TRequestChoice;\n";
        code.ClassPrivate() << "    CRef<TRequest> m_DefaultRequest;\n\n";
    } else {
        code.ClassPublic() << "    typedef TRequest TRequestChoice;\n";
    }
    {{
        if ( !m_Source.m_ReplyElement.empty() ) {
            code.HPPIncludes().insert(m_Source.m_ReplyChoiceType->FileName());
            code.ClassPublic() << "    typedef "
                               << s_QualClassName(m_Source.m_ReplyChoiceType)
                               << " TReplyChoice;\n\n";
        } else {
            code.ClassPublic() << "    typedef TReply TReplyChoice;\n\n";
        }
        code.ClassPrivate()
            << "    TReplyChoice& x_Choice(TReply& reply);\n";
        code.MethodStart(true)
            << trep << "Choice& " << class_base << "::x_Choice(" << trep
            << "& reply)\n"
            << "{\n    return reply" << s_SetterName(m_Source.m_ReplyElement)
            << ";\n}\n\n";
    }}

    {{
        // Figure out arguments to parent's constructor
        string service = generator.GetConfig().Get(sect_name, "service");
        string format  = generator.GetConfig().Get(sect_name, "serialformat");
        string args;
        if (service.empty()) {
            ERR_POST_X(1, Warning << "No service name provided for " << class_name);
            args = "kEmptyStr";
        } else {
            args = '\"' + NStr::PrintableString(service) + '\"';
        }
        if ( !format.empty() ) {
            args += ", eSerial_" + format;
        }
        code.AddInitializer("Tparent", args);
    }}
    if ( !m_Source.m_RequestElement.empty() ) {
        code.AddInitializer("m_DefaultRequest", "new TRequest");
    }

    // This should just be a simple using-declaration, but that breaks
    // on GCC 2.9x at least, even with a full parent class name :-/
    code.ClassPublic()
        // << "    using Tparent::Ask;\n"
        << "    virtual void Ask(const TRequest& request, TReply& reply);\n"
        << "    virtual void Ask(const TRequest& request, TReply& reply,\n"
        << "                     TReplyChoice::E_Choice wanted);\n\n";
    // second version defined further down
    code.MethodStart(true)
        << "void " << class_base << "::Ask(const " << treq << "& request, "
        << trep << "& reply)\n"
        << "{\n    Tparent::Ask(request, reply);\n}\n\n\n";

    // Add appropriate infrastructure if TRequest is not itself the choice
    // (m_DefaultRequest declared earlier to reduce ugliness)
    if ( !m_Source.m_RequestElement.empty() ) {
        string setter = s_SetterName(m_Source.m_RequestElement);
        code.ClassPublic()
            << "\n"
            << "    virtual const TRequest& GetDefaultRequest(void) const;\n"
            << "    virtual TRequest&       SetDefaultRequest(void);\n"
            << "    virtual void            SetDefaultRequest(const TRequest& request);\n"
            << "\n"
            << "    virtual void Ask(const TRequestChoice& req, TReply& reply);\n"
            << "    virtual void Ask(const TRequestChoice& req, TReply& reply,\n"
            << "                     TReplyChoice::E_Choice wanted);\n\n";

        // inline methods
        code.MethodStart(true)
            << "const " << treq << "& " << class_base
            << "::GetDefaultRequest(void) const\n"
            << "{\n    return *m_DefaultRequest;\n}\n\n";
        code.MethodStart(true)
            << treq << "& " << class_base << "::SetDefaultRequest(void)\n"
            << "{\n    return *m_DefaultRequest;\n}\n\n";
        code.MethodStart(true)
            << "void " << class_base << "::SetDefaultRequest(const " << treq
            << "& request)\n"
            << "{\n    m_DefaultRequest->Assign(request);\n}\n\n\n";

        code.MethodStart(false)
            << "void " << class_base << "::Ask(const " << treq
            << "Choice& req, " << trep << "& reply)\n"
            << "{\n"
            << "    TRequest request;\n"
            << "    request.Assign(*m_DefaultRequest);\n"
            // We have to copy req because SetXxx() wants a non-const ref.
            << "    request" << setter << ".Assign(req);\n"
            << "    Ask(request, reply);\n"
            << "}\n\n\n";
        code.MethodStart(false)
            << "void " << class_base << "::Ask(const " << treq
            << "Choice& req, " << trep << "& reply, " << trep
            << "Choice::E_Choice wanted)\n"
            << "{\n"
            << "    TRequest request;\n"
            << "    request.Assign(*m_DefaultRequest);\n"
            // We have to copy req because SetXxx() wants a non-const ref.
            << "    request" << setter << ".Assign(req);\n"
            << "    Ask(request, reply, wanted);\n"
            << "}\n\n\n";
    }

    // Scan choice types for interesting elements
    typedef CChoiceDataType::TMembers       TChoices;
    typedef map<string, const CDataMember*> TChoiceMap;
    const TChoices& choices   = m_Source.m_RequestChoiceType->GetMembers();
    TChoiceMap      reply_map;
    bool            has_init  = false, has_fini = false, has_error = false;
    ITERATE (TChoices, it, choices) {
        const string& name = (*it)->GetName();
        if (name == "init") {
            if (dynamic_cast<const CNullDataType*>((*it)->GetType())) {
                has_init = true;
            } else {
                CNcbiOstrstream oss;
                (*it)->GetType()->PrintASN(oss, 0);
                string type = CNcbiOstrstreamToString(oss);
                _ASSERT(type != "NULL");
                ERR_POST_X(2, Warning << m_Source.m_RequestChoiceType->GlobalName()
                              << ": disabling special init handling because it"
                              << " requires a payload of type " << type);
            }
        } else if (name == "fini") {
            if (dynamic_cast<const CNullDataType*>((*it)->GetType())) {
                has_fini = true;
            } else {
                CNcbiOstrstream oss;
                (*it)->GetType()->PrintASN(oss, 0);
                string type = CNcbiOstrstreamToString(oss);
                _ASSERT(type != "NULL");
                ERR_POST_X(3, Warning << m_Source.m_RequestChoiceType->GlobalName()
                              << ": disabling special fini handling because it"
                              << " requires a payload of type " << type);
            }
        }
    }
    ITERATE (TChoices, it, m_Source.m_ReplyChoiceType->GetMembers()) {
        const string& name = (*it)->GetName();
        reply_map[name] = it->get();
        if (name == "error") {
            has_error = true;
        }
    }

    if (has_init) {
        code.ClassProtected()
            << "    void x_Connect(void);\n";
        code.MethodStart(false)
            << "void " << class_base << "::x_Connect(void)\n"
            << "{\n"
            << "    Tparent::x_Connect();\n"
            << "    AskInit();\n"
            << "}\n\n";
    }
    if (has_fini) {
        code.ClassProtected()
            << "    void x_Disconnect(void);\n";
        code.MethodStart(false)
            << "void " << class_base << "::x_Disconnect(void)\n"
            << "{\n"
            << "    AskFini();\n" // ignore/downgrade errors?
            << "    Tparent::x_Disconnect();\n"
            << "}\n\n";
    }

    // Make sure the reply's choice is correct -- rolled into Ask for
    // maximum flexibility.  (Split out methods for the two error cases?)
    code.MethodStart(false)
        << "void " << class_base << "::Ask(const " << treq << "& request, "
        << trep << "& reply, " << trep << "Choice::E_Choice wanted)\n"
        << "{\n"
        << "    Ask(request, reply);\n"
        << "    TReplyChoice& rc = x_Choice(reply);\n"
        << "    if (rc.Which() == wanted) {\n"
        << "        return; // ok\n";
    if (has_error) {
        code.Methods(false)
            << "    } else if (rc.IsError()) {\n"
            << "        CNcbiOstrstream oss;\n"
            << "        oss << \"" << class_name
            << ": server error: \" << rc.GetError();\n"
            << "        NCBI_THROW(CException, eUnknown, CNcbiOstrstreamToString(oss));\n";
    }
    code.Methods(false)
        << "    } else {\n"
        << "        rc.ThrowInvalidSelection(wanted);\n"
        << "    }\n"
        << "}\n\n";

    // Finally, generate all the actual Ask* methods....
    ITERATE (TChoices, it, choices) {
        typedef AutoPtr<CTypeStrings> TTypeStr;
        string name  = (*it)->GetName();
        string reply = m_Source.m_Generator.GetConfig().Get(sect_name,
                                                            "reply." + name);
        if (reply.empty()) {
            reply = name;
        } else if (reply == "special") {
            continue;
        }
        TChoiceMap::const_iterator rm = reply_map.find(reply);
        if (rm == reply_map.end()) {
            NCBI_THROW(CDatatoolException, eInvalidData,
                       "Invalid reply type " + reply + " for " + name);
        }

        string           method   = "Ask" + Identifier(name);
        const CDataType* req_type = (*it)->GetType()->Resolve();
        string           req_class;
        bool             null_req = false;
        if (dynamic_cast<const CNullDataType*>(req_type)) {
            req_class = "void";
            null_req  = true;
        } else if ( !req_type->GetParentType() ) {
            req_class = req_type->ClassName();
        } else {
            TTypeStr typestr = req_type->GetFullCType();
            typestr->GeneratePointerTypeCode(code);
            req_class = typestr->GetCType(ns);
        }

        const CDataType* rep_type = rm->second->GetType()->Resolve();
        string           rep_class;
        bool             use_cref = false;
        bool             null_rep = false;
        if (dynamic_cast<const CNullDataType*>(rep_type)) {
            rep_class = "void";
            null_rep  = true;
        } else if ( !rep_type->GetParentType()  &&  !rep_type->IsStdType() ) {
            rep_class = ns.GetNamespaceRef(CNamespace::KNCBINamespace)
                + "CRef<" + rep_type->ClassName() + '>';
            use_cref  = true;
            code.CPPIncludes().insert(rep_type->FileName());
        } else {
            TTypeStr typestr = rep_type->GetFullCType();
            typestr->GeneratePointerTypeCode(code);
            rep_class = typestr->GetCType(ns);
        }
        code.ClassPublic()
            << "    virtual " << rep_class << ' ' << method << "\n";
        if (null_req) {
            code.ClassPublic() << "        (TReply* reply = 0);\n\n";
        } else {
            code.ClassPublic() << "        (const " << req_class
            << "& req, TReply* reply = 0);\n\n";
        }
        code.MethodStart(false)
            << rep_class << ' ' << class_base << "::" << method;
        if (null_req) {
            code.Methods(false) << '(' << trep << "* reply)\n";
        } else {
            code.Methods(false)
                << "(const " << req_class << "& req, " << trep << "* reply)\n";
        }
        code.Methods(false)
            << "{\n"
            << "    TRequestChoice request;\n"
            << "    TReply         reply0;\n";
        if (null_req) {
            code.Methods(false)
                << "    request.Set" << Identifier(name) << "();\n";
        } else {
            code.Methods(false)
                << "    request.Set" << Identifier(name) << "(const_cast<"
                << req_class << "&>(req));\n";
        }
        code.Methods(false)
            << "    if ( !reply ) {\n"
            << "        reply = &reply0;\n"
            << "    }\n"
            << "    Ask(request, *reply, TReplyChoice::e_" << Identifier(reply)
            << ");\n";
        if (null_rep) {
            code.Methods(false) << "}\n\n";
        } else if (use_cref) {
            code.Methods(false)
                << "    return " << rep_class << "(&x_Choice(*reply).Set"
                << Identifier(reply) << "());\n"
                << "}\n\n";
        } else {
            code.Methods(false)
                << "    return x_Choice(*reply).Get" << Identifier(reply)
                << "();\n"
                << "}\n\n";
        }
    }
}


END_NCBI_SCOPE
