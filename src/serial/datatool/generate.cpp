#include <corelib/ncbistd.hpp>
#include <algorithm>
#include <typeinfo>
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "code.hpp"
#include "generate.hpp"

USING_NCBI_SCOPE;

CCodeGenerator::CCodeGenerator(void)
    : m_ExcludeAllTypes(false)
{
}

CCodeGenerator::~CCodeGenerator(void)
{
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

void CCodeGenerator::GetAllTypes(void)
{
    for ( CModuleSet::TModules::const_iterator mi = m_Modules.modules.begin();
          mi != m_Modules.modules.end();
          ++mi ) {
        const ASNModule* module = mi->second.get();
        for ( ASNModule::TDefinitions::const_iterator ti =
                  module->definitions.begin();
              ti != module->definitions.end();
              ++ti ) {
            const ASNType* type = ti->get();
            if ( !type->name.empty() && type->main &&
                 type->ClassName(m_Config) != "-" )
                m_GenerateTypes.insert(module->name + '.' + type->name);
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
        CollectTypes(m_Modules.ResolveFull(*ti));
    }
    
    // generate output files
    for ( TOutputFiles::const_iterator filei = m_Files.begin();
          filei != m_Files.end();
          ++filei ) {
        CFileCode* code = filei->second.get();
        code->GenerateHPP(m_HeadersDir);
        code->GenerateCPP(m_SourcesDir);
    }

    if ( !m_FileListFileName.empty() ) {
        CNcbiOfstream fileList(m_FileListFileName.c_str());
        
        fileList << "GENFILES =";
        for ( TOutputFiles::const_iterator filei = m_Files.begin();
              filei != m_Files.end();
              ++filei ) {
            fileList << ' ' << filei->first << "_Base";
        }
        fileList << endl;
    }
}

bool CCodeGenerator::AddType(const ASNType* type)
{
    string fileName = type->FileName(m_Config);
    AutoPtr<CFileCode>& file = m_Files[fileName];
    if ( !file )
        file = new CFileCode(m_Config, fileName, m_HeaderPrefix);
    return file->AddType(type);
}

CClassCode* CCodeGenerator::GetClassCode(const ASNType* type)
{
    string fileName = type->FileName(m_Config);
    TOutputFiles::const_iterator fi = m_Files.find(fileName);
    if ( fi == m_Files.end() ) {
        THROW1_TRACE(runtime_error,
                     "cannot get class info for \"" + type->name + "\" @ " +
                     type->context.GetFilePos().ToString() + " (filename: " +
                     fileName + ")");
    }
    return fi->second->GetClassCode(type);
}

void CCodeGenerator::CollectTypes(const ASNType* type, bool force)
{
    if ( m_ExcludeTypes.find(type->name) != m_ExcludeTypes.end() ) {
        ERR_POST(Warning << "Skipping type: " << type->name);
        return;
    }
    
    const ASNOfType* array = dynamic_cast<const ASNOfType*>(type);
    if ( array != 0 ) {
        // SET OF or SEQUENCE OF
        if ( force || !array->name.empty() ) {
            if ( !AddType(type) )
                return;
        }
        if ( m_ExcludeAllTypes )
            return;
        // we should add element type
        CollectTypes(array->type.get());
        return;
    }

    const ASNUserType* user = dynamic_cast<const ASNUserType*>(type);
    if ( user != 0 ) {
        // reference to another type
        CollectTypes(user->Resolve());
        return;
    }

    if ( dynamic_cast<const ASNFixedType*>(type) != 0 ||
         dynamic_cast<const ASNEnumeratedType*>(type) != 0 ) {
        // STD type
        if ( force || !type->name.empty() ) {
            AddType(type);
        }
        return;
    }

    if ( force || !type->name.empty() ) {
        if ( type->ClassName(m_Config) == "-" ) {
            ERR_POST(Warning << "Skipping type: " << type->name);
            return;
        }
    }
    
    const ASNChoiceType* choice =
        dynamic_cast<const ASNChoiceType*>(type);
    if ( choice != 0 ) {
        if ( !AddType(type) )
            return;

        if ( m_ExcludeAllTypes )
            return;

        // collect member's types
        for ( ASNMemberContainerType::TMembers::const_iterator mi =
                  choice->members.begin();
              mi != choice->members.end(); ++mi ) {
            const ASNType* memberType = mi->get()->type.get();
            CollectTypes(memberType, true);
        }
    }

    const ASNMemberContainerType* cont =
        dynamic_cast<const ASNMemberContainerType*>(type);
    if ( cont != 0 ) {
        if ( force || !cont->name.empty() ) {
            if ( !AddType(type) )
                return;
        }

        if ( m_ExcludeAllTypes )
            return;

        // collect member's types
        for ( ASNMemberContainerType::TMembers::const_iterator mi =
                  cont->members.begin();
              mi != cont->members.end(); ++mi ) {
            const ASNType* memberType = mi->get()->type.get();
            CollectTypes(memberType);
        }
        return;
    }
    if ( !AddType(type) )
        return;
}
