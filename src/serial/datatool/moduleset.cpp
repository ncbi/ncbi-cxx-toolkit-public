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
*   Set of modules: equivalent of ASN.1 source file
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <typeinfo>
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "exceptions.hpp"
#include "fileutil.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Modules

BEGIN_NCBI_SCOPE

CFileModules::CFileModules(const string& name)
    : m_SourceFileName(name)
{
}

void CFileModules::AddModule(const AutoPtr<CDataTypeModule>& module)
{
    module->SetModuleContainer(this);
    CDataTypeModule*& mptr = m_ModulesByName[module->GetName()];
    if ( mptr ) {
        ERR_POST_X(4, GetSourceFileName() << ": duplicate module: " <<
                      module->GetName());
    }
    else {
        mptr = module.get();
        m_Modules.push_back(module);
    }
}

bool CFileModules::Check(void) const
{
    bool ok = true;
    ITERATE ( TModules, mi, m_Modules ) {
        if ( !(*mi)->Check() )
            ok = false;
    }
    return ok;
}

bool CFileModules::CheckNames(void) const
{
    bool ok = true;
    ITERATE ( TModules, mi, m_Modules ) {
        if ( !(*mi)->CheckNames() )
            ok = false;
    }
    return ok;
}

void CFileModules::PrintSampleDEF(const string& rootdir) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        string fileName = MakeAbsolutePath(
            Path(rootdir, Path(GetFileNamePrefix(), (*mi)->GetName() + "._sample_def")));
        CNcbiOfstream out(fileName.c_str());
        (*mi)->PrintSampleDEF(out);
        if ( !out )
            ERR_POST_X(5, Fatal << "Cannot write to file "<<fileName);
    }
}

void CFileModules::PrintASN(CNcbiOstream& out) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        PrintASNRefInfo(out);
        (*mi)->PrintASN(out);
    }
    m_LastComments.PrintASN(out, 0, CComments::eMultiline);
}

void CFileModules::PrintSpecDump(CNcbiOstream& out) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        out << "M," << mi->get()->GetSourceLine() << ',';
        out << CDirEntry(m_SourceFileName).GetName() << ':'
            << (*mi)->GetName();
        (*mi)->PrintSpecDump(out);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
void CFileModules::PrintXMLSchema(CNcbiOstream& out) const
{
    BeginXMLSchema(out);
    ITERATE ( TModules, mi, m_Modules ) {
        (*mi)->PrintXMLSchema(out);
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
    EndXMLSchema(out);
}

void CFileModules::GetRefInfo(list<string>& info) const
{
    info.clear();
    string s, h("::DATATOOL:: ");
    s = h + "Generated from \"" + GetSourceFileName() + "\"";
    info.push_back(s);
    s = h + "by application DATATOOL version ";
    s += CNcbiApplication::Instance()->GetVersion().Print();
    info.push_back(s);
    s = h + "on " + CTime(CTime::eCurrent).AsString();
    info.push_back(s);
}

void CFileModules::PrintASNRefInfo(CNcbiOstream& out) const
{
    list<string> info;
    GetRefInfo(info);
    out << "-- ============================================\n";
    ITERATE(list<string>, i, info) {
        out << "-- " << *i << "\n";
    }
    out << "-- ============================================\n\n";
}

void CFileModules::PrintXMLRefInfo(CNcbiOstream& out) const
{
    list<string> info;
    GetRefInfo(info);
    out << "<!-- ============================================\n";
    ITERATE(list<string>, i, info) {
        out << "     " << *i << "\n";
    }
    out << "     ============================================ -->\n\n";
}

void CFileModules::PrintDTD(CNcbiOstream& out) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        PrintXMLRefInfo(out);
        (*mi)->PrintDTD(out);
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

void CFileModules::PrintDTDModular(void) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        string fileNameBase = MakeAbsolutePath(
            (*mi)->GetDTDFileNameBase() + (*mi)->GetModuleFileSuffix());
        {
            string fileName = fileNameBase + ".mod.dtd";
            CNcbiOfstream out(fileName.c_str());
            PrintXMLRefInfo(out);
            (*mi)->PrintDTD(out);
            if ( !out )
                ERR_POST_X(5, Fatal << "Cannot write to file "<<fileName);
        }
        {
            string fileName = fileNameBase + ".dtd";
            CNcbiOfstream out(fileName.c_str());
            PrintXMLRefInfo(out);
            (*mi)->PrintDTDModular(out);
            if ( !out )
                ERR_POST_X(6, Fatal << "Cannot write to file "<<fileName);
        }
    }
}

void CFileModules::PrintXMLSchemaModular(void) const
{
    ITERATE ( TModules, mi, m_Modules ) {
        string fileNameBase = MakeAbsolutePath(
            (*mi)->GetDTDFileNameBase() + (*mi)->GetModuleFileSuffix());
        {
            string fileName = fileNameBase + ".mod.xsd";
            CNcbiOfstream out(fileName.c_str());
            if ( !out )
                ERR_POST_X(7, Fatal << "Cannot write to file "<<fileName);
            BeginXMLSchema(out);
            (*mi)->PrintXMLSchema(out);
            EndXMLSchema(out);
        }
        {
            string fileName = fileNameBase + ".xsd";
            CNcbiOfstream out(fileName.c_str());
            if ( !out )
                ERR_POST_X(8, Fatal << "Cannot write to file "<<fileName);
            BeginXMLSchema(out);
            (*mi)->PrintXMLSchemaModular(out);
            EndXMLSchema(out);
        }
    }
}

void CFileModules::BeginXMLSchema(CNcbiOstream& out) const
{
    string nsName("http://www.ncbi.nlm.nih.gov");
    string nsNcbi(nsName);
    string elementForm("qualified");
    string attributeForm("unqualified");
    if (!m_Modules.empty()) {
        const CDataTypeModule::TDefinitions& defs = 
            m_Modules.front()->GetDefinitions();
        if (!defs.empty()) {
            const string& ns = defs.front().second->GetNamespaceName();
            if (!ns.empty()) {
                nsName = ns;
            }
            if (!defs.front().second->IsNsQualified()) {
                elementForm = "unqualified";
            }
        }
    }
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if ( const CArgValue& px_ns = args["xmlns"] ) {
        nsName = px_ns.AsString();
    }
    out << "<?xml version=\"1.0\" ?>\n";
    PrintXMLRefInfo(out);
    out << "<xs:schema\n"
        << "  xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"\n"
        << "  xmlns:ncbi=\"" << nsNcbi << "\"\n";
    if (!nsName.empty()) {
        out << "  xmlns=\"" << nsName << "\"\n"
            << "  targetNamespace=\"" << nsName << "\"\n";
    }
    out << "  elementFormDefault=\"" << elementForm << "\"\n"
        << "  attributeFormDefault=\"" << attributeForm << "\">\n\n";
}

void CFileModules::EndXMLSchema(CNcbiOstream& out) const
{
    out << "</xs:schema>\n";
}

const string& CFileModules::GetSourceFileName(void) const
{
    return m_SourceFileName;
}

string CFileModules::GetFileNamePrefix(void) const
{
    if ( MakeFileNamePrefixFromSourceFileName() ) {
        if ( m_PrefixFromSourceFileName.empty() ) {
            m_PrefixFromSourceFileName = DirName(m_SourceFileName);
            if ( !IsLocalPath(m_PrefixFromSourceFileName) ) {
                // path absent or non local
                m_PrefixFromSourceFileName.erase();
                return GetModuleContainer().GetFileNamePrefix();
            }
        }
        _TRACE("file " << m_SourceFileName << ": \"" << m_PrefixFromSourceFileName << "\"");
        if ( UseAllFileNamePrefixes() ) {
            return Path(GetModuleContainer().GetFileNamePrefix(),
                        m_PrefixFromSourceFileName);
        }
        else {
            return m_PrefixFromSourceFileName;
        }
    }
    return GetModuleContainer().GetFileNamePrefix();
}

CDataType* CFileModules::ExternalResolve(const string& moduleName,
                                         const string& typeName,
                                         bool allowInternal) const
{
    // find module definition
    TModulesByName::const_iterator mi = m_ModulesByName.find(moduleName);
    if ( mi == m_ModulesByName.end() ) {
        // no such module
        NCBI_THROW(CNotFoundException,eModule,
                     "module not found: "+moduleName+" for type "+typeName);
    }
    return mi->second->ExternalResolve(typeName, allowInternal);
}

CDataType* CFileModules::ResolveInAnyModule(const string& typeName,
                                            bool allowInternal) const
{
    CResolvedTypeSet types(typeName);
    ITERATE ( TModules, i, m_Modules ) {
        try {
            types.Add((*i)->ExternalResolve(typeName, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CNotFoundException& /* ignored */) {
        }
    }
    return types.GetType();
}

void CFileModules::CollectAllTypeinfo(set<TTypeInfo>& types) const
{
    ITERATE ( TModules, i, m_Modules ) {
        (*i)->CollectAllTypeinfo(types);
    }
}

void CFileSet::AddFile(const AutoPtr<CFileModules>& moduleSet)
{
    moduleSet->SetModuleContainer(this);
    m_ModuleSets.push_back(moduleSet);
}

void CFileSet::PrintSampleDEF(const string& rootdir) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintSampleDEF(rootdir);
    }
}

void CFileSet::PrintASN(CNcbiOstream& out) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintASN(out);
    }
}

void CFileSet::PrintSpecDump(CNcbiOstream& out) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintSpecDump(out);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
void CFileSet::PrintXMLSchema(CNcbiOstream& out) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintXMLSchema(out);
    }
}

void CFileSet::PrintDTD(CNcbiOstream& out) const
{
#if 0
    out <<
        "<!-- ======================== -->\n"
        "<!-- NCBI DTD                 -->\n"
        "<!-- NCBI ASN.1 mapped to XML -->\n"
        "<!-- ======================== -->\n"
        "\n"
        "<!-- Entities used to give specificity to #PCDATA -->\n"
        "<!ENTITY % INTEGER '#PCDATA'>\n"
        "<!ENTITY % ENUM 'EMPTY'>\n"
        "<!ENTITY % BOOLEAN 'EMPTY'>\n"
        "<!ENTITY % NULL 'EMPTY'>\n"
        "<!ENTITY % REAL '#PCDATA'>\n"
        "<!ENTITY % OCTETS '#PCDATA'>\n"
        "<!-- ============================================ -->\n"
        "\n";
#endif
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintDTD(out);
    }
}

void CFileSet::PrintDTDModular(void) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintDTDModular();
    }
}

void CFileSet::PrintXMLSchemaModular(void) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintXMLSchemaModular();
    }
}

CDataType* CFileSet::ExternalResolve(const string& module, const string& name,
                                     bool allowInternal) const
{
    CResolvedTypeSet types(module, name);
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        try {
            types.Add((*i)->ExternalResolve(module, name, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CNotFoundException& /* ignored */) {
        }
    }
    return types.GetType();
}

CDataType* CFileSet::ResolveInAnyModule(const string& name,
                                        bool allowInternal) const
{
    CResolvedTypeSet types(name);
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        try {
            types.Add((*i)->ResolveInAnyModule(name, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CNotFoundException& /* ignored */) {
        }
    }
    return types.GetType();
}

bool CFileSet::Check(void) const
{
    bool ok = true;
    ITERATE ( TModuleSets, mi, m_ModuleSets ) {
        if ( !(*mi)->Check() )
            ok = false;
    }
    return ok;
}

bool CFileSet::CheckNames(void) const
{
    bool ok = true;
    ITERATE ( TModuleSets, mi, m_ModuleSets ) {
        if ( !(*mi)->CheckNames() )
            ok = false;
    }
    return ok;
}

void CFileSet::CollectAllTypeinfo(set<TTypeInfo>& types) const
{
    ITERATE ( TModuleSets, i, m_ModuleSets ) {
        (*i)->CollectAllTypeinfo(types);
    }
}

END_NCBI_SCOPE
