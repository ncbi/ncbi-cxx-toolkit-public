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
*   Data descriptions module: equivalent of ASN.1 module
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>
#include "module.hpp"
#include "exceptions.hpp"
#include "type.hpp"
#include "srcutil.hpp"
#include "fileutil.hpp"
#include <serial/error_codes.hpp>
#include <typeinfo>


#define NCBI_USE_ERRCODE_X   Serial_Modules

BEGIN_NCBI_SCOPE

string CDataTypeModule::s_ModuleFileSuffix;

CDataTypeModule::CDataTypeModule(const string& n)
    : m_SourceLine(0), m_Errors(false), m_Name(n),
      m_TagDefault(CAsnBinaryDefs::eAutomatic)
{
}

CDataTypeModule::~CDataTypeModule()
{
}

void CDataTypeModule::SetSourceLine(int line)
{
    m_SourceLine = line;
}

void CDataTypeModule::AddDefinition(const string& name,
                                    const AutoPtr<CDataType>& type)
{
    CDataType*& oldType = m_LocalTypes[name];
    if ( oldType ) {
        type->Warning("redefinition, original: " +
                      oldType->LocationString(), 1);
        m_Errors = true;
        return;
    }
    CDataType* dataType = type.get();
    oldType = dataType;
    m_Definitions.push_back(make_pair(name, type));
    dataType->SetParent(this, name);
}

void CDataTypeModule::AddExports(const TExports& exports)
{
    m_Exports.insert(m_Exports.end(), exports.begin(), exports.end());
}

void CDataTypeModule::AddImports(const TImports& imports)
{
    m_Imports.insert(m_Imports.end(), imports.begin(), imports.end());
}

void CDataTypeModule::AddImports(const string& module, const list<string>& types)
{
    AutoPtr<Import> import(new Import());
    import->moduleName = module;
    import->types.insert(import->types.end(), types.begin(), types.end());
    m_Imports.push_back(import);
}

void CDataTypeModule::SetSubnamespace(const string& sub_ns)
{
    m_Subnamespace = sub_ns;
}

string CDataTypeModule::GetSubnamespace(void) const
{
    string sn(GetVar(GetName(),"_subnamespace",false));
    if (!sn.empty()) {
        return sn;
    }
    return m_Subnamespace;
}

const CNamespace& CDataTypeModule::GetNamespace(void) const
{
    if (m_Namespace.get()) {
        return *m_Namespace;
    }
    const CNamespace& def= CModuleContainer::GetNamespace();
    string sub_ns(GetSubnamespace());
    if (sub_ns.empty()) {
        return def;
    }
    m_Namespace.reset( new CNamespace(def.ToString() + sub_ns));
    return *m_Namespace;
}

void CDataTypeModule::PrintSampleDEF(CNcbiOstream& out) const
{
    map< string, set< string > >::const_iterator s = m_DefVars.begin();
    for (s = m_DefVars.begin(); s != m_DefVars.end(); ++s) {
        out << "[" << s->first << "]" << endl;
        ITERATE( set<string>, v, s->second) {
            if (NStr::EndsWith(*v, "._class")) {
                out << *v << " = " << endl;
            }
        }
        out << endl;
    }
}

void CDataTypeModule::PrintASN(CNcbiOstream& out) const
{
    m_Comments.PrintASN(out, 0, CComments::eMultiline);

    out << ToAsnName(GetName()) << " DEFINITIONS ";

    if (m_TagDefault == CAsnBinaryDefs::eImplicit) {
        out << "IMPLICIT";
    } else if (m_TagDefault == CAsnBinaryDefs::eExplicit) {
        out << "EXPLICIT";
    } else {
        out << "AUTOMATIC";
    }
    out << " TAGS ::=\nBEGIN\n\n";

    if ( !m_Exports.empty() ) {
        out << "EXPORTS ";
        ITERATE ( TExports, i, m_Exports ) {
            if ( i != m_Exports.begin() )
                out << ", ";
            out << *i;
        }
        out <<
            ";\n"
            "\n";
    }

    if ( !m_Imports.empty() ) {
        out << "IMPORTS ";
        ITERATE ( TImports, m, m_Imports ) {
            if ( m != m_Imports.begin() )
                out <<
                    "\n"
                    "        ";

            const Import& imp = **m;
            ITERATE ( list<string>, i, imp.types ) {
                if ( i != imp.types.begin() )
                    out << ", ";
                out << *i;
            }
            out << " FROM " << imp.moduleName;
        }
        out <<
            ";\n"
            "\n";
    }

    ITERATE ( TDefinitions, i, m_Definitions ) {
        i->second->PrintASNTypeComments(out, 0, CComments::eDoNotWriteBlankLine);
        out << ToAsnName(i->first) << " ::= ";
        i->second->PrintASN(out, 0);
        out <<
            "\n"
            "\n";
    }

    m_LastComments.PrintASN(out, 0, CComments::eMultiline);

    out <<
        "END\n"
        "\n";
}

void CDataTypeModule::PrintSpecDump(CNcbiOstream& out) const
{
    m_Comments.PrintASN(out, 0, CComments::eNoEOL);
    m_LastComments.PrintASN(out, 0, CComments::eNoEOL);
    ITERATE ( TDefinitions, i, m_Definitions ) {
        PrintASNNewLine(out, 1);
        out << "T," << i->second->GetSourceLine() << ','
            << GetName() << ':' << i->second->GetMemberName();
    }
    PrintASNNewLine(out, 0);

    ITERATE ( TDefinitions, i, m_Definitions ) {
        i->second->PrintSpecDump(out, 0);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
void CDataTypeModule::PrintXMLSchema(CNcbiOstream& out) const
{
    out <<
        "<!-- ============================================ -->\n"
        "<!-- This section is mapped from module \"" << GetName() << "\"\n"
        "================================================= -->\n";

    m_Comments.PrintDTD(out, CComments::eMultiline);

    if ( !m_Exports.empty() ) {
        out <<
            "<!-- Elements used by other modules:\n";

        ITERATE ( TExports, i, m_Exports ) {
            if ( i != m_Exports.begin() )
                out << ",\n";
            out << "          " << *i;
        }

        out << " -->\n\n";
    }
    if ( !m_Imports.empty() ) {
        out <<
            "<!-- Elements referenced from other modules:\n";
        ITERATE ( TImports, i, m_Imports ) {
            if ( i != m_Imports.begin() )
                out << ",\n";
            const Import* imp = i->get();
            ITERATE ( list<string>, t, imp->types ) {
                if ( t != imp->types.begin() )
                    out << ",\n";
                out <<
                    "          " << *t;
            }
            out << " FROM "<< imp->moduleName;
        }
        out << " -->\n\n";
    }

    if ( !m_Exports.empty() || !m_Imports.empty() ) {
        out <<
            "<!-- ============================================ -->\n\n";
    }

    m_ExtraDefs.clear();
    ITERATE ( TDefinitions, i, m_Definitions ) {
        out << "\n";
#if !_DATATOOL_USE_SCHEMA_STYLE_COMMENTS
        i->second->PrintDTDTypeComments(out, 0);
#endif
        i->second->PrintXMLSchema(out,0);
    }
    out << m_ExtraDefs;

    m_LastComments.PrintDTD(out, CComments::eMultiline);
    out << "\n\n";
}

void CDataTypeModule::AddExtraSchemaOutput(const string& extra) const
{
    m_ExtraDefs += "\n";
    m_ExtraDefs += extra;
}

void CDataTypeModule::PrintDTD(CNcbiOstream& out) const
{
    out <<
        "<!-- ============================================ -->\n"
        "<!-- This section is mapped from module \"" << GetName() << "\"\n"
        "================================================= -->\n";

    m_Comments.PrintDTD(out, CComments::eNoEOL);

    if ( !m_Exports.empty() ) {
        out <<
            "\n\n<!-- Elements used by other modules:\n";

        ITERATE ( TExports, i, m_Exports ) {
            if ( i != m_Exports.begin() )
                out << ",\n";
            out << "          " << *i;
        }

        out << " -->";
    }
    if ( !m_Imports.empty() ) {
        out <<
            "\n\n<!-- Elements referenced from other modules:\n";
        ITERATE ( TImports, i, m_Imports ) {
            if ( i != m_Imports.begin() )
                out << ",\n";
            const Import* imp = i->get();
            ITERATE ( list<string>, t, imp->types ) {
                if ( t != imp->types.begin() )
                    out << ",\n";
                out <<
                    "          " << *t;
            }
            out << " FROM "<< imp->moduleName;
        }
        out << " -->";
    }

    if ( !m_Exports.empty() || !m_Imports.empty() ) {
        out <<
            "\n<!-- ============================================ -->";
    }

    ITERATE ( TDefinitions, i, m_Definitions ) {
//        out <<
//            "<!-- Definition of "<<i->first<<" -->\n\n";
        i->second->PrintDTD(out);
    }

    m_LastComments.PrintDTD(out, CComments::eMultiline);

    out << "\n\n";
}

static
string DTDFileNameBase(const string& name)
{
    string res;
    ITERATE ( string, i, name ) {
        char c = *i;
        if ( c == '-' )
            res += '_';
        else
            res += c;
    }
    return res;
}

static
string DTDPublicModuleName(const string& name)
{
    string res;
    ITERATE ( string, i, name ) {
        char c = *i;
        if ( !isalnum((unsigned char) c) )
            res += ' ';
        else
            res += c;
    }
    return res;
}

string CDataTypeModule::GetDTDPublicName(void) const
{
    return DTDPublicModuleName(GetName());
}

string CDataTypeModule::GetDTDFileNameBase(void) const
{
    return DTDFileNameBase(GetName());
}

static
void PrintModularDTDModuleReference(CNcbiOstream& out,
    const string& name, const string& suffix)
{
    string fileName = DTDFileNameBase(name);
    string pubName = DTDPublicModuleName(name);
    out 
        << "\n<!ENTITY % "
        << fileName << "_module PUBLIC \"-//NCBI//" << pubName << " Module//EN\" \""
        << fileName << suffix << ".mod.dtd\">\n%"
        << fileName << "_module;\n";
}

void CDataTypeModule::PrintDTDModular(CNcbiOstream& out) const
{
    out <<
        "<!-- "<<DTDFileNameBase(GetName())<<".dtd\n"
        "  This file is built from a series of basic modules.\n"
        "  The actual ELEMENT and ENTITY declarations are in the modules.\n"
        "  This file is used to put them together.\n"
        "-->\n";
    PrintModularDTDModuleReference(out, "NCBI-Entity", GetModuleFileSuffix());

    list<string> l;
//    l.assign(m_ImportRef.begin(), m_ImportRef.end());
    ITERATE( set<string>, s, m_ImportRef) {
        l.push_back(*s);
    }
    l.sort();
    ITERATE (list<string>, i, l) {
        PrintModularDTDModuleReference(out, (*i), GetModuleFileSuffix());
    }
}

void CDataTypeModule::PrintXMLSchemaModular(CNcbiOstream& out) const
{
    out <<
        "<!-- "<<DTDFileNameBase(GetName())<<".xsd\n"
        "  This file is built from a series of basic modules.\n"
        "  The actual declarations are in the modules.\n"
        "  This file is used to put them together.\n"
        "-->\n";

    list<string> l;
//    l.assign(m_ImportRef.begin(), m_ImportRef.end());
    ITERATE( set<string>, s, m_ImportRef) {
        l.push_back(*s);
    }
    l.sort();
    ITERATE (list<string>, i, l) {
        out << "<xs:include schemaLocation=\"" << DTDFileNameBase(*i)
            <<  GetModuleFileSuffix() << ".mod.xsd\"/>\n";
    }
}

bool CDataTypeModule::Check()
{
    bool ok = true;
    ITERATE ( TDefinitions, d, m_Definitions ) {
        if ( !d->second->Check() )
            ok = false;
    }
    return ok;
}

bool CDataTypeModule::CheckNames()
{
    bool ok = true;
    ITERATE ( TExports, e, m_Exports ) {
        const string& name = *e;
        TTypesByName::iterator it = m_LocalTypes.find(name);
        if ( it == m_LocalTypes.end() ) {
            ERR_POST_X(1, Warning << "undefined export type: " << name);
            ok = false;
        }
        else {
            m_ExportedTypes[name] = it->second;
        }
    }
    ITERATE ( TImports, i, m_Imports ) {
        const Import& imp = **i;
        const string& module = imp.moduleName;
        ITERATE ( list<string>, t, imp.types ) {
            const string& name = *t;
            if ( m_LocalTypes.find(name) != m_LocalTypes.end() ) {
                ERR_POST_X(2, Warning <<
                         "import conflicts with local definition: " << name);
                ok = false;
                continue;
            }
            pair<TImportsByName::iterator, bool> ins =
                m_ImportedTypes.insert(TImportsByName::value_type(name, module));
            if ( !ins.second ) {
                ERR_POST_X(3, Warning << "duplicated import: " << name);
                ok = false;
                continue;
            }
        }
    }
    return ok;
}

CDataType* CDataTypeModule::ExternalResolve(const string& typeName,
                                            bool allowInternal) const
{
    const TTypesByName& types = allowInternal? m_LocalTypes: m_ExportedTypes;
    TTypesByName::const_iterator t = types.find(typeName);
    if ( t != types.end() )
        return t->second;

    if ( !allowInternal &&
         m_LocalTypes.find(typeName) != m_LocalTypes.end() ) {
        NCBI_THROW(CNotFoundException,eType, "not exported type: "+typeName);
    }

    NCBI_THROW(CNotFoundException,eType, "undefined type: "+typeName);
}

CDataType* CDataTypeModule::Resolve(const string& typeName) const
{
    TTypesByName::const_iterator t = m_LocalTypes.find(typeName);
    if ( t != m_LocalTypes.end() )
        return t->second;
    TImportsByName::const_iterator i = m_ImportedTypes.find(typeName);
    if ( i != m_ImportedTypes.end() )
        return GetModuleContainer().InternalResolve(i->second, typeName);
    NCBI_THROW(CNotFoundException,eType, "undefined type: "+typeName);
}

string CDataTypeModule::GetFileNamePrefix(void) const
{
    _TRACE("module " << m_Name << ": " << GetModuleContainer().GetFileNamePrefixSource());
    if ( MakeFileNamePrefixFromModuleName() ) {
        if ( m_PrefixFromName.empty() )
            m_PrefixFromName = Identifier(m_Name);
        _TRACE("module " << m_Name << ": \"" << m_PrefixFromName << "\"");
        if ( UseAllFileNamePrefixes() ) {
            return Path(GetModuleContainer().GetFileNamePrefix(),
                        m_PrefixFromName);
        }
        else {
            return m_PrefixFromName;
        }
    }
    return GetModuleContainer().GetFileNamePrefix();
}

const string CDataTypeModule::GetVar(
    const string& typeName, const string& varName, bool collect) const
{
    _ASSERT(!typeName.empty());
    _ASSERT(!varName.empty());
    {
        const string s = x_GetVar(GetName() + '.' + typeName, varName);
        if ( !s.empty() )
            return s;
    }
    {
        const string s = x_GetVar(typeName, varName, collect);
        if ( !s.empty() )
            return s;
    }
    {
        const string s = x_GetVar(GetName(), varName);
        if ( !s.empty() )
            return s;
    }
    // default section
    return x_GetVar("-", varName);
}

const string CDataTypeModule::x_GetVar(
    const string& section, const string& value, bool collect) const
{
    if (collect) {
        m_DefVars[section].insert(value);
    }
    map< string, bool >::const_iterator i = m_DefSections.find(section);
    if (i == m_DefSections.end()) {
        m_DefSections[section] = GetConfig().HasEntry(section);
        i = m_DefSections.find(section);
        if (i == m_DefSections.end()) {
            return kEmptyStr;
        }
        if (i->second) {
            list<string> entries;
            GetConfig().EnumerateEntries(section,&entries);
            m_DefSectionEntries[section] = entries;
        }
    }
    if (!i->second) {
        return kEmptyStr;
    }
    map< string, list< string > >::const_iterator e =
        m_DefSectionEntries.find(section);
    bool found = e != m_DefSectionEntries.end() &&
        find(e->second.begin(), e->second.end(), value) != e->second.end();
    return found ? GetConfig().Get(section, value) : kEmptyStr;
}

bool CDataTypeModule::AddImportRef(const string& imp)
{
    if (m_ImportRef.find(imp) == m_ImportRef.end()) {
        m_ImportRef.insert(imp);
        return true;
    }
    return false;
}

string CDataTypeModule::ToAsnName(const string& name)
{
    string asn;
    asn.reserve(name.size());
    bool first = true, hyphen = false;
    for (string::const_iterator i = name.begin(); i != name.end();) {
        unsigned char u = (unsigned char)(*i);
        if (first) {
            if (isalpha(u)) {
                asn += toupper(u);
            } else {
                asn += 'A';
                if (isdigit(u)) {
                    asn += u;
                } else {
                    hyphen = true;
                    asn += '-';
                }
            }
            first = false;
        } else if (isalpha(u) || isdigit(u)) {
            hyphen = false;
            asn += u;
        } else if (!hyphen) {
            hyphen = true;
            asn += '-';
        }
        ++i;
    }
    if (hyphen) {
        asn.resize( asn.size()-1 );
    }
    return asn;
}

string CDataTypeModule::ToAsnId(const string& name)
{
    string asn(name);
    asn[0] = tolower((unsigned char)asn[0]);
    return asn;
}

void CDataTypeModule::CollectAllTypeinfo(set<TTypeInfo>& types) const
{
    ITERATE ( TDefinitions, i, m_Definitions ) {
        types.insert(i->second->GetTypeInfo().Get());
    }
}


END_NCBI_SCOPE
