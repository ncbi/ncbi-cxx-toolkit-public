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
*   Main generator: collects all types, classes and files.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2000/01/06 16:13:18  vasilche
* Fail of file generation now fatal.
*
* Revision 1.24  1999/12/28 18:55:57  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.23  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.22  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* Revision 1.21  1999/12/17 19:05:19  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.20  1999/12/09 20:01:23  vasilche
* Fixed bug with missed internal classes.
*
* Revision 1.19  1999/12/03 21:42:12  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.18  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.17  1999/11/15 19:36:15  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <algorithm>
#include <typeinfo>
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "statictype.hpp"
#include "reftype.hpp"
#include "unitype.hpp"
#include "enumtype.hpp"
#include "blocktype.hpp"
#include "choicetype.hpp"
#include "filecode.hpp"
#include "generate.hpp"
#include "exceptions.hpp"
#include "fileutil.hpp"

USING_NCBI_SCOPE;

CCodeGenerator::CCodeGenerator(void)
    : m_ExcludeRecursion(false), m_FileNamePrefixSource(eFileName_FromNone)
{
	m_MainFiles.SetModuleContainer(this);
	m_ImportFiles.SetModuleContainer(this); 
}

CCodeGenerator::~CCodeGenerator(void)
{
}

const CNcbiRegistry& CCodeGenerator::GetConfig(void) const
{
    return m_Config;
}

string CCodeGenerator::GetFileNamePrefix(void) const
{
    return m_FileNamePrefix;
}

void CCodeGenerator::SetFileNamePrefix(const string& prefix)
{
    m_FileNamePrefix = prefix;
}

EFileNamePrefixSource CCodeGenerator::GetFileNamePrefixSource(void) const
{
    return m_FileNamePrefixSource;
}

void CCodeGenerator::SetFileNamePrefixSource(EFileNamePrefixSource source)
{
    m_FileNamePrefixSource =
        EFileNamePrefixSource(m_FileNamePrefixSource | source);
}

CDataType* CCodeGenerator::InternalResolve(const string& module,
                                           const string& name) const
{
    return ExternalResolve(module, name);
}

void CCodeGenerator::LoadConfig(const string& fileName)
{
    // load descriptions from registry file
    if ( fileName == "stdin" || fileName == "-" ) {
        m_Config.Read(NcbiCin);
    }
    else {
        CNcbiIfstream in(fileName.c_str());
        if ( !in )
            ERR_POST(Fatal << "cannot open file " << fileName);
        m_Config.Read(in);
    }
}

CDataType* CCodeGenerator::ExternalResolve(const string& module,
                                           const string& name,
                                           bool exported) const
{
    try {
        return m_MainFiles.ExternalResolve(module, name, exported);
    }
    catch ( CAmbiguiousTypes& exc ) {
        _TRACE(exc.what());
        throw;
    }
    catch ( CTypeNotFound& exc ) {
        _TRACE(exc.what());
        return m_ImportFiles.ExternalResolve(module, name, exported);
    }
}

CDataType* CCodeGenerator::ResolveInAnyModule(const string& name,
                                              bool exported) const
{
    try {
        return m_MainFiles.ResolveInAnyModule(name, exported);
    }
    catch ( CAmbiguiousTypes& exc ) {
        _TRACE(exc.what());
        throw;
    }
    catch ( CTypeNotFound& exc ) {
        _TRACE(exc.what());
        return m_ImportFiles.ResolveInAnyModule(name, exported);
    }
}

CDataType* CCodeGenerator::ResolveMain(const string& fullName) const
{
    SIZE_TYPE dot = fullName.find('.');
    if ( dot != NPOS ) {
        // module specified
        return m_MainFiles.ExternalResolve(fullName.substr(0, dot),
                                           fullName.substr(dot + 1),
                                           true);
    }
    else {
        // module not specified - we'll scan all modules for type
        return m_MainFiles.ResolveInAnyModule(fullName, true);
    }
}

void CCodeGenerator::IncludeAllMainTypes(void)
{
    iterate ( CFileSet::TModuleSets, msi, m_MainFiles.GetModuleSets() ) {
        iterate ( CFileModules::TModules, mi, (*msi)->GetModules() ) {
            const CDataTypeModule* module = mi->second.get();
            iterate ( CDataTypeModule::TDefinitions, ti,
                      module->GetDefinitions() ) {
                const string& name = ti->first;
                const CDataType* type = ti->second.get();
                if ( !name.empty() && !type->Skipped() ) {
                    m_GenerateTypes.insert(module->GetName() + '.' + name);
                }
            }
        }
    }
}

void CCodeGenerator::GetTypes(TTypeNames& typeSet, const string& types)
{
    SIZE_TYPE pos = 0;
    SIZE_TYPE next = types.find(',');
    while ( next != NPOS ) {
        typeSet.insert(types.substr(pos, next - pos));
        pos = next + 1;
        next = types.find(',', pos);
    }
    typeSet.insert(types.substr(pos));
}

bool CCodeGenerator::Check(void) const
{
    return m_MainFiles.CheckNames() && m_ImportFiles.CheckNames() &&
        m_MainFiles.Check();
}

void CCodeGenerator::ExcludeTypes(const string& typeList)
{
    TTypeNames typeNames;
    GetTypes(typeNames, typeList);
    iterate ( TTypeNames, i, typeNames ) {
        m_Config.Set(*i, "_class", "-");
    }
}

void CCodeGenerator::IncludeTypes(const string& typeList)
{
    GetTypes(m_GenerateTypes, typeList);
}

void CCodeGenerator::GenerateCode(void)
{
    // collect types
    iterate ( TTypeNames, ti, m_GenerateTypes ) {
        CollectTypes(ResolveMain(*ti), eRoot);
    }
    
    // generate output files
    iterate ( TOutputFiles, filei, m_Files ) {
        CFileCode* code = filei->second.get();
        code->GenerateHPP(m_HPPDir);
        code->GenerateCPP(m_CPPDir);
        code->GenerateUserHPP(m_HPPDir);
        code->GenerateUserCPP(m_CPPDir);
    }

    if ( !m_FileListFileName.empty() ) {
        CNcbiOfstream fileList(m_FileListFileName.c_str());
        if ( !fileList ) {
            ERR_POST(Fatal <<
                     "cannot create file list file: " << m_FileListFileName);
        }
        
        fileList << "GENFILES =";
        {
            iterate ( TOutputFiles, filei, m_Files ) {
                fileList << ' ' << filei->first;
            }
        }
        fileList << NcbiEndl;
        fileList << "GENFILES_LOCAL =";
        {
            iterate ( TOutputFiles, filei, m_Files ) {
                fileList << ' ' << BaseName(filei->first);
            }
        }
        fileList << NcbiEndl;
    }
}

bool CCodeGenerator::AddType(const CDataType* type)
{
    string fileName = type->FileName();
    AutoPtr<CFileCode>& file = m_Files[fileName];
    if ( !file )
        file = new CFileCode(fileName);
    return file->AddType(type);
}

bool CCodeGenerator::Imported(const CDataType* type) const
{
    try {
        m_MainFiles.ExternalResolve(type->GetModule()->GetName(),
                                    type->IdName(),
                                    true);
        return false;
    }
    catch ( CTypeNotFound& /* ignored */ ) {
    }
    return true;
}

void CCodeGenerator::CollectTypes(const CDataType* type, EContext context)
{
    const CUniSequenceDataType* array =
        dynamic_cast<const CUniSequenceDataType*>(type);
    if ( array != 0 ) {
        // SET OF or SEQUENCE OF
        if ( type->GetParentType() == 0 || context == eChoice ) {
            if ( !AddType(type) )
                return;
        }
        if ( m_ExcludeRecursion )
            return;
        // we should add element type
        CollectTypes(array->GetElementType(), eElement);
        return;
    }

    const CReferenceDataType* user =
        dynamic_cast<const CReferenceDataType*>(type);
    if ( user != 0 ) {
        // reference to another type
        const CDataType* resolved;
        try {
            resolved = user->Resolve();
        }
        catch (CTypeNotFound& exc) {
            ERR_POST(Warning <<
                     "Skipping type: " << user->GetUserTypeName() <<
                     ": " << exc.what());
            return;
        }
        if ( resolved->Skipped() ) {
            ERR_POST(Warning << "Skipping type: " << user->GetUserTypeName());
            return;
        }
        if ( context == eChoice ) {
            // in choice
            if ( resolved->InheritFromType() != user->GetParentType() ||
                 dynamic_cast<const CEnumDataType*>(resolved) != 0 ) {
                // add intermediate class
                AddType(user);
            }
        }
        else if ( type->GetParentType() == 0 ) {
            // alias declaration
            // generate empty class
            AddType(user);
        }
        if ( !Imported(resolved) ) {
            CollectTypes(resolved, eReference);
        }
        return;
    }

    if ( dynamic_cast<const CStaticDataType*>(type) != 0 ) {
        // STD type
        if ( type->GetParentType() == 0 || context == eChoice ) {
            AddType(type);
        }
        return;
    }

    if ( dynamic_cast<const CEnumDataType*>(type) != 0 ) {
        // ENUMERATED type
        if ( type->GetParentType() == 0 || context == eChoice ) {
            AddType(type);
        }
        return;
    }

    if ( type->GetParentType() == 0 || context == eChoice ) {
        if ( type->Skipped() ) {
            ERR_POST(Warning << "Skipping type: " << type->IdName());
            return;
        }
    }
    
    const CChoiceDataType* choice =
        dynamic_cast<const CChoiceDataType*>(type);
    if ( choice != 0 ) {
        if ( !AddType(type) )
            return;

        if ( m_ExcludeRecursion )
            return;

        // collect member's types
        iterate ( CDataMemberContainerType::TMembers, mi,
                  choice->GetMembers() ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eChoice);
        }
    }

    const CDataMemberContainerType* cont =
        dynamic_cast<const CDataMemberContainerType*>(type);
    if ( cont != 0 ) {
        if ( !AddType(type) )
            return;

        if ( m_ExcludeRecursion )
            return;

        // collect member's types
        iterate ( CDataMemberContainerType::TMembers, mi,
                  cont->GetMembers() ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eMember);
        }
        return;
    }
    if ( !AddType(type) )
        return;
}
