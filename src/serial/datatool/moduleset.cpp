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
* Revision 1.13  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <typeinfo>
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "exceptions.hpp"

CModuleContainer::~CModuleContainer(void)
{
}

CModuleSet::CModuleSet(const string& name)
    : m_SourceFileName(name), m_ModuleContainer(0)
{
}

void CModuleSet::AddModule(const AutoPtr<CDataTypeModule>& module)
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

bool CModuleSet::Check(void) const
{
    bool ok = true;
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        if ( !mi->second->Check() )
            ok = false;
    }
    return ok;
}

bool CModuleSet::CheckNames(void) const
{
    bool ok = true;
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        if ( !mi->second->CheckNames() )
            ok = false;
    }
    return ok;
}

void CModuleSet::PrintASN(CNcbiOstream& out) const
{
    for ( TModules::const_iterator mi = m_Modules.begin();
          mi != m_Modules.end(); ++mi ) {
        mi->second->PrintASN(out);
    }
}

const CNcbiRegistry& CModuleSet::GetConfig(void) const
{
    return GetModuleContainer().GetConfig();
}

const string& CModuleSet::GetSourceFileName(void) const
{
    return m_SourceFileName;
}

CDataType* CModuleSet::InternalResolve(const string& moduleName,
                                       const string& typeName) const
{
    return GetModuleContainer().InternalResolve(moduleName, typeName);
}

CDataType* CModuleSet::ExternalResolve(const string& moduleName,
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

CDataType* CModuleSet::ResolveInAnyModule(const string& typeName,
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

CFileSet::CFileSet(void)
    : m_Parent(0)
{
}

void CFileSet::AddFile(const AutoPtr<CModuleSet>& moduleSet)
{
    moduleSet->m_ModuleContainer = this;
    m_ModuleSets.push_back(moduleSet);
}

const CNcbiRegistry& CFileSet::GetConfig(void) const
{
    return GetModuleContainer().GetConfig();
}

const string& CFileSet::GetSourceFileName(void) const
{
    return GetModuleContainer().GetSourceFileName();
}

CDataType* CFileSet::InternalResolve(const string& module,
                                     const string& type) const
{
    return GetModuleContainer().InternalResolve(module, type);
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

