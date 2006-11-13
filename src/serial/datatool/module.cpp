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
#include <serial/datatool/module.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/datatool/fileutil.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

string CDataTypeModule::s_ModuleFileSuffix;

CDataTypeModule::CDataTypeModule(const string& n)
    : m_SourceLine(0), m_Errors(false), m_Name(n)
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
                      oldType->LocationString());
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

void CDataTypeModule::PrintASN(CNcbiOstream& out) const
{
    m_Comments.PrintASN(out, 0, CComments::eMultiline);

    out <<
        ToAsnName(GetName()) << " DEFINITIONS ::=\n"
        "BEGIN\n"
        "\n";

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

    ITERATE ( TDefinitions, i, m_Definitions ) {
        out << "\n";
        i->second->PrintDTDTypeComments(out, 0);
        i->second->PrintXMLSchema(out,0);
    }

    m_LastComments.PrintDTD(out, CComments::eMultiline);
    out << "\n\n";
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
            ERR_POST(Warning << "undefined export type: " << name);
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
                ERR_POST(Warning <<
                         "import conflicts with local definition: " << name);
                ok = false;
                continue;
            }
            pair<TImportsByName::iterator, bool> ins =
                m_ImportedTypes.insert(TImportsByName::value_type(name, module));
            if ( !ins.second ) {
                ERR_POST(Warning << "duplicated import: " << name);
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

const string CDataTypeModule::GetVar(const string& typeName,
                                      const string& varName) const
{
    _ASSERT(!typeName.empty());
    _ASSERT(!varName.empty());
    const CNcbiRegistry& registry = GetConfig();
    {
        const string& s = registry.Get(GetName() + '.' + typeName, varName);
        if ( !s.empty() )
            return s;
    }
    {
        const string& s = registry.Get(typeName, varName);
        if ( !s.empty() )
            return s;
    }
    {
        const string& s = registry.Get(GetName(), varName);
        if ( !s.empty() )
            return s;
    }
    // default section
    return registry.Get("-", varName);
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


END_NCBI_SCOPE
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.53  2006/11/13 15:56:54  gouriano
* Corrected parsing and printing ASN spec comments
*
* Revision 1.52  2006/10/31 16:18:30  gouriano
* Added source line info
*
* Revision 1.51  2006/10/30 21:03:33  gouriano
* Corrected data spec dump formatting
*
* Revision 1.50  2006/10/30 18:15:40  gouriano
* Added writing data specification in internal format
*
* Revision 1.49  2006/10/18 13:10:25  gouriano
* Moved Log to bottom
*
* Revision 1.48  2006/07/24 18:57:39  gouriano
* Preserve comments when parsing DTD
*
* Revision 1.47  2006/06/28 19:03:36  gouriano
* Print type comments when generating schema
*
* Revision 1.46  2006/06/19 17:34:06  gouriano
* Redesigned generation of XML schema
*
* Revision 1.45  2006/05/17 20:40:01  ucko
* Fix compilation errors with older versions of GCC.
*
* Revision 1.44  2006/05/16 14:30:13  gouriano
* Corrected generation of ASN spec - to make sure it is valid
*
* Revision 1.43  2006/05/09 15:16:43  gouriano
* Added XML namespace definition possibility
*
* Revision 1.42  2006/04/13 12:58:54  gouriano
* Added optional file name suffix to modular DTD or schema
*
* Revision 1.41  2005/07/11 18:20:37  gouriano
* Corrected schema module file name
*
* Revision 1.40  2005/06/29 15:09:58  gouriano
* Resolve all module dependencies when generating modular DTD or schema
*
* Revision 1.39  2005/06/06 17:40:42  gouriano
* Added generation of modular XML schema
*
* Revision 1.38  2005/06/03 17:05:33  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.37  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.36  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.35  2003/06/16 14:41:05  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.34  2003/05/14 14:42:22  gouriano
* added generation of XML schema
*
* Revision 1.33  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.32  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.31  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.30  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.29  2000/11/29 17:42:44  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.28  2000/11/20 17:26:32  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.27  2000/11/15 20:34:55  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.26  2000/11/14 21:41:25  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.25  2000/11/08 17:02:51  vasilche
* Added generation of modular DTD files.
*
* Revision 1.24  2000/09/26 17:38:26  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.23  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.22  2000/07/10 17:32:00  vasilche
* Macro arguments made more clear.
* All old ASN stuff moved to serialasn.hpp.
* Changed prefix of enum info functions to GetTypeInfo_enum_.
*
* Revision 1.21  2000/06/16 20:01:30  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.20  2000/05/24 20:09:29  vasilche
* Implemented DTD generation.
*
* Revision 1.19  2000/04/07 19:26:29  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.18  2000/02/01 21:48:02  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.17  1999/12/30 21:33:40  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.16  1999/12/29 16:01:51  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.15  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.14  1999/12/21 17:18:36  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.13  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.12  1999/11/22 21:04:49  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
* Revision 1.11  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
