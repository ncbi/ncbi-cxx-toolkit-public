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
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>
#include <typeinfo>
#include <serial/tool/module.hpp>
#include <serial/tool/exceptions.hpp>
#include <serial/tool/type.hpp>
#include <serial/tool/fileutil.hpp>

BEGIN_NCBI_SCOPE

CDataTypeModule::CDataTypeModule(const string& n)
    : m_Errors(false), m_Name(n)
{
}

CDataTypeModule::~CDataTypeModule()
{
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
    oldType = type.get();
    m_Definitions.push_back(make_pair(name, type));
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
    out <<
        GetName() << " DEFINITIONS ::=\n"
        "BEGIN\n"
        "\n";

    if ( !m_Exports.empty() ) {
        out << "EXPORTS ";
        for ( TExports::const_iterator begin = m_Exports.begin(),
                  end = m_Exports.end(), i = begin; i != end; ++i ) {
            if ( i != begin )
                out << ", ";
            out << *i;
        }
        out <<
            ";\n"
            "\n";
    }

    if ( !m_Imports.empty() ) {
        out << "IMPORTS ";
        for ( TImports::const_iterator mbegin = m_Imports.begin(),
                  mend = m_Imports.end(), m = mbegin; m != mend; ++m ) {
            if ( m != mbegin )
                out <<
                    "\n"
                    "        ";

            const Import& imp = **m;
            for ( list<string>::const_iterator begin = imp.types.begin(),
                  end = imp.types.end(), i = begin; i != end; ++i ) {
                if ( i != begin )
                    out << ", ";
                out << *i;
            }
            out << " FROM " << imp.moduleName;
        }
        out <<
            ";\n"
            "\n";
    }

    for ( TDefinitions::const_iterator i = m_Definitions.begin();
          i != m_Definitions.end(); ++i ) {
        out << i->first << " ::= ";
        i->second->PrintASN(out, 0);
        out <<
            "\n"
            "\n";
    }

    out <<
        "END\n"
        "\n";
}

bool CDataTypeModule::Check()
{
    bool ok = true;
    for ( TDefinitions::const_iterator d = m_Definitions.begin();
          d != m_Definitions.end(); ++d ) {
        if ( !d->second->Check() )
            ok = false;
    }
    return ok;
}

bool CDataTypeModule::CheckNames()
{
    iterate ( TDefinitions, d, m_Definitions ) {
        d->second->SetParent(this, d->first);
    }
    bool ok = true;
    for ( TExports::const_iterator e = m_Exports.begin();
          e != m_Exports.end(); ++e ) {
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
    for ( TImports::const_iterator i = m_Imports.begin();
          i != m_Imports.end(); ++i ) {
        const Import& imp = **i;
        const string& module = imp.moduleName;
        for ( list<string>::const_iterator t = imp.types.begin();
              t != imp.types.end(); ++t ) {
            const string& name = *t;
            if ( m_LocalTypes.find(name) != m_LocalTypes.end() ) {
                ERR_POST(Warning <<
                         "import conflicts with local defenition: " << name);
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

    if ( !allowInternal && m_LocalTypes.find(typeName) != m_LocalTypes.end() ) {
        THROW1_TRACE(CTypeNotFound, "not exported type: " + typeName);
    }

    THROW1_TRACE(CTypeNotFound, "undefined type: " + typeName);
}

CDataType* CDataTypeModule::Resolve(const string& typeName) const
{
    TTypesByName::const_iterator t = m_LocalTypes.find(typeName);
    if ( t != m_LocalTypes.end() )
        return t->second;
    TImportsByName::const_iterator i = m_ImportedTypes.find(typeName);
    if ( i != m_ImportedTypes.end() )
        return GetModuleContainer().InternalResolve(i->second, typeName);
    THROW1_TRACE(CTypeNotFound, "undefined type: " + typeName);
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

const string& CDataTypeModule::GetVar(const string& section,
                                      const string& value) const
{
    _ASSERT(!section.empty());
    _ASSERT(!value.empty());
    const CNcbiRegistry& registry = GetConfig();
    const string& s1 = registry.Get(GetName() + '.' + section, value);
    if ( !s1.empty() )
        return s1;
    const string& s2 = registry.Get(section, value);
    if ( !s2.empty() )
        return s2;
    return registry.Get(GetName(), value);
}

END_NCBI_SCOPE
