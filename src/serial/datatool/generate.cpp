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
#include "filecode.hpp"
#include "generate.hpp"
#include "exceptions.hpp"

USING_NCBI_SCOPE;

CCodeGenerator::CCodeGenerator(void)
    : m_MainFiles(*this), m_ImportFiles(*this), m_ExcludeRecursion(false)
{
}

CCodeGenerator::~CCodeGenerator(void)
{
}

const string& CCodeGenerator::GetSourceFileName(void) const
{
    return NcbiEmptyString;
}

const CNcbiRegistry& CCodeGenerator::GetConfig(void) const
{
    return m_Config;
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

CDataType* CCodeGenerator::InternalResolve(const string& module,
                                           const string& name) const
{
    return ExternalResolve(module, name);
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
    const CFileSet::TModuleSets& moduleSets = m_MainFiles.GetModuleSets();
    for ( CFileSet::TModuleSets::const_iterator msi = moduleSets.begin();
          msi != moduleSets.end();
          ++msi ) {
        const CModuleSet::TModules& modules = (*msi)->GetModules();
        for ( CModuleSet::TModules::const_iterator mi = modules.begin();
              mi != modules.end(); ++mi ) {
            const CDataTypeModule* module = mi->second.get();
            for ( CDataTypeModule::TDefinitions::const_iterator ti =
                      module->GetDefinitions().begin();
                  ti != module->GetDefinitions().end();
                  ++ti ) {
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
    for ( TTypeNames::const_iterator i = typeNames.begin();
          i != typeNames.end(); ++i ) {
        m_Config.Set(*i, "_class", "-");
    }
}

void CCodeGenerator::IncludeTypes(const string& typeList)
{
    GetTypes(m_GenerateTypes, typeList);
}

static inline
string MkDir(const string& s)
{
    SIZE_TYPE length = s.size();
    if ( length == 0 || s[length-1] == '/' )
        return s;
    else
        return s + '/';
}

void CCodeGenerator::GenerateCode(void)
{
    m_HeadersDir = MkDir(m_HeadersDir);
    m_SourcesDir = MkDir(m_SourcesDir);
    m_HeaderPrefix = MkDir(m_Config.Get("-", "headers_prefix"));

    // collect types
    for ( TTypeNames::const_iterator ti = m_GenerateTypes.begin();
          ti != m_GenerateTypes.end();
          ++ti ) {
        CollectTypes(ResolveMain(*ti), eRoot);
    }
    
    // generate output files
    for ( TOutputFiles::const_iterator filei = m_Files.begin();
          filei != m_Files.end();
          ++filei ) {
        CFileCode* code = filei->second.get();
        code->GenerateHPP(m_HeadersDir);
        code->GenerateCPP(m_SourcesDir);
        code->GenerateUserHPP(m_HeadersDir);
        code->GenerateUserCPP(m_SourcesDir);
    }

    if ( !m_FileListFileName.empty() ) {
        CNcbiOfstream fileList(m_FileListFileName.c_str());
        
        fileList << "GENFILES =";
        {
            for ( TOutputFiles::const_iterator filei = m_Files.begin();
                  filei != m_Files.end();
                  ++filei ) {
                fileList << ' ' << filei->first << "_Base";
            }
        }
        fileList << NcbiEndl;

        fileList << "GENUSERFILES =";
        {
            for ( TOutputFiles::const_iterator filei = m_Files.begin();
                  filei != m_Files.end();
                  ++filei ) {
                fileList << ' ' << filei->first;
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
        file = new CFileCode(fileName, m_HeaderPrefix);
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
    catch ( CTypeNotFound& exc ) {
    }
    return true;
}

void CCodeGenerator::CollectTypes(const CDataType* type, EContext context)
{
    const CUniSequenceDataType* array =
        dynamic_cast<const CUniSequenceDataType*>(type);
    if ( array != 0 ) {
        // SET OF or SEQUENCE OF
        if ( context != eOther ) {
            if ( !AddType(type) )
                return;
        }
        if ( m_ExcludeRecursion )
            return;
        // we should add element type
        CollectTypes(array->GetElementType());
        return;
    }

    const CReferenceDataType* user =
        dynamic_cast<const CReferenceDataType*>(type);
    if ( user != 0 ) {
        // reference to another type
        const CDataType* resolved = user->Resolve();
        if ( resolved->Skipped() ) {
            ERR_POST(Warning << "Skipping type: " << user->GetUserTypeName());
            return;
        }
        if ( context == eChoice ) {
            // in choice
            if ( /*user->InChoice() ||*/ resolved->GetChoices().size() != 1 ) {
                // in unnamed choice OR in several choices
                _TRACE("Will create adapter class for member " << user->LocationString() << ": " << resolved->ClassName() << " uc=" << user->GetChoices().size() << " rc=" << resolved->GetChoices().size());
                AddType(user);
            }
        }
        else if ( context == eRoot ) {
            // alias declaration
            // generate empty class
            AddType(user);
        }
        if ( !Imported(resolved) ) {
            CollectTypes(resolved, eReference);
        }
        return;
    }

    if ( dynamic_cast<const CStaticDataType*>(type) != 0 ||
         dynamic_cast<const CEnumDataType*>(type) != 0 ) {
        // STD type
        if ( context != eOther ) {
            AddType(type);
        }
        return;
    }

    if ( context != eOther ) {
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
        for ( CDataMemberContainerType::TMembers::const_iterator mi =
                  choice->GetMembers().begin();
              mi != choice->GetMembers().end(); ++mi ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eChoice);
        }
    }

    const CDataMemberContainerType* cont =
        dynamic_cast<const CDataMemberContainerType*>(type);
    if ( cont != 0 ) {
        if ( context != eOther ) {
            if ( !AddType(type) )
                return;
        }

        if ( m_ExcludeRecursion )
            return;

        // collect member's types
        for ( CDataMemberContainerType::TMembers::const_iterator mi =
                  cont->GetMembers().begin();
              mi != cont->GetMembers().end(); ++mi ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType);
        }
        return;
    }
    if ( !AddType(type) )
        return;
}
