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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2000/02/01 21:48:03  vasilche
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
* Revision 1.16  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.15  1999/12/21 17:18:36  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.14  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.13  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <typeinfo>
#include <serial/tool/moduleset.hpp>
#include <serial/tool/module.hpp>
#include <serial/tool/type.hpp>
#include <serial/tool/exceptions.hpp>
#include <serial/tool/fileutil.hpp>

CFileModules::CFileModules(const string& name)
    : m_SourceFileName(name)
{
}

void CFileModules::AddModule(const AutoPtr<CDataTypeModule>& module)
{
    module->SetModuleContainer(this);
    AutoPtr<CDataTypeModule>& mptr = m_Modules[module->GetName()];
    if ( mptr ) {
        ERR_POST(GetSourceFileName() << ": duplicate module: " <<
                 module->GetName());
    }
    else {
        mptr = module;
    }
}

bool CFileModules::Check(void) const
{
    bool ok = true;
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        if ( !mi->second->Check() )
            ok = false;
    }
    return ok;
}

bool CFileModules::CheckNames(void) const
{
    bool ok = true;
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        if ( !mi->second->CheckNames() )
            ok = false;
    }
    return ok;
}

void CFileModules::PrintASN(CNcbiOstream& out) const
{
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        mi->second->PrintASN(out);
    }
}

const string& CFileModules::GetSourceFileName(void) const
{
    return m_SourceFileName;
}

string CFileModules::GetFileNamePrefix(void) const
{
    _TRACE("file " << m_SourceFileName << ": " << GetModuleContainer().GetFileNamePrefixSource());
    if ( MakeFileNamePrefixFromSourceFileName() ) {
        if ( m_PrefixFromSourceFileName.empty() ) {
            m_PrefixFromSourceFileName = DirName(m_SourceFileName);
            if ( m_PrefixFromSourceFileName.empty() ||
                 m_PrefixFromSourceFileName[0] == '.' ||
                 m_PrefixFromSourceFileName[0] == '/' ) {
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
    TModules::const_iterator mi = m_Modules.find(moduleName);
    if ( mi == m_Modules.end() ) {
        // no such module
        THROW1_TRACE(CModuleNotFound, "module not found: " + moduleName +
                     " for type " + typeName);
    }
    return mi->second->ExternalResolve(typeName, allowInternal);
}

CDataType* CFileModules::ResolveInAnyModule(const string& typeName,
                                          bool allowInternal) const
{
    int count = 0;
    CDataType* type = 0;
    for ( TModules::const_iterator i = m_Modules.begin();
          i != m_Modules.end(); ++i ) {
        try {
            type = i->second->ExternalResolve(typeName, allowInternal);
            count += 1;
        }
        catch ( CAmbiguiousTypes& /* ignored */ ) {
            count += 2;
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    switch ( count ) {
    case 0:
        THROW1_TRACE(CTypeNotFound, "type not found: " + typeName);
    case 1:
        return type;
    default:
        THROW1_TRACE(CAmbiguiousTypes,
                     "ambiguous type definition: " + typeName);
    }
}

void CFileSet::AddFile(const AutoPtr<CFileModules>& moduleSet)
{
    moduleSet->SetModuleContainer(this);
    m_ModuleSets.push_back(moduleSet);
}

void CFileSet::PrintASN(CNcbiOstream& out) const
{
    for ( TModuleSets::const_iterator i = m_ModuleSets.begin();
          i != m_ModuleSets.end(); ++i ) {
        (*i)->PrintASN(out);
    }
}

CDataType* CFileSet::ExternalResolve(const string& module, const string& name,
                                     bool allowInternal) const
{
    int count = 0;
    CDataType* type = 0;
    for ( TModuleSets::const_iterator i = m_ModuleSets.begin();
          i != m_ModuleSets.end(); ++i ) {
        try {
            type = (*i)->ExternalResolve(module, name, allowInternal);
            count += 1;
        }
        catch ( CAmbiguiousTypes& /* ignored */ ) {
            count += 2;
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    switch ( count ) {
    case 0:
        THROW1_TRACE(CTypeNotFound, "type not found: " + module + '.' + name);
    case 1:
        return type;
    default:
        THROW1_TRACE(CAmbiguiousTypes,
                     "ambiguous type definition: " + module + '.' + name);
    }
}

CDataType* CFileSet::ResolveInAnyModule(const string& name,
                                        bool allowInternal) const
{
    int count = 0;
    CDataType* type = 0;
    for ( TModuleSets::const_iterator i = m_ModuleSets.begin();
          i != m_ModuleSets.end(); ++i ) {
        try {
            type = (*i)->ResolveInAnyModule(name, allowInternal);
            count += 1;
        }
        catch ( CAmbiguiousTypes& /* ignored */ ) {
            count += 2;
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    switch ( count ) {
    case 0:
        THROW1_TRACE(CTypeNotFound, "type not found: " + name);
    case 1:
        return type;
    default:
        THROW1_TRACE(CAmbiguiousTypes,
                     "ambiguous type definition: " + name);
    }
}

bool CFileSet::Check(void) const
{
    bool ok = true;
    for ( TModuleSets::const_iterator mi = m_ModuleSets.begin();
          mi != m_ModuleSets.end(); ++mi ) {
        if ( !(*mi)->Check() )
            ok = false;
    }
    return ok;
}

bool CFileSet::CheckNames(void) const
{
    bool ok = true;
    for ( TModuleSets::const_iterator mi = m_ModuleSets.begin();
          mi != m_ModuleSets.end(); ++mi ) {
        if ( !(*mi)->CheckNames() )
            ok = false;
    }
    return ok;
}

