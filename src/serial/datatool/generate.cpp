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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>
#include <typeinfo>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/datatool/reftype.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/enumtype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/choicetype.hpp>
#include <serial/datatool/filecode.hpp>
#include <serial/datatool/generate.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/fileutil.hpp>
#include <serial/datatool/rpcgen.hpp>
#include <serial/datatool/code.hpp>

BEGIN_NCBI_SCOPE

CCodeGenerator::CCodeGenerator(void)
    : m_ExcludeRecursion(false), m_FileNamePrefixSource(eFileName_FromNone)
{
    m_MainFiles.SetModuleContainer(this);
    m_ImportFiles.SetModuleContainer(this); 
    m_UseQuotedForm = false;
    m_CreateCvsignore = false;
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

void CCodeGenerator::UseQuotedForm(bool use)
{
    m_UseQuotedForm = use;
}

void CCodeGenerator::CreateCvsignore(bool create)
{
    m_CreateCvsignore = create;
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

const CNamespace& CCodeGenerator::GetNamespace(void) const
{
    return m_DefaultNamespace;
}

void CCodeGenerator::SetDefaultNamespace(const string& ns)
{
    m_DefaultNamespace = ns;
}

void CCodeGenerator::LoadConfig(CNcbiIstream& in)
{
    m_Config.Read(in);
}

void CCodeGenerator::LoadConfig(const string& fileName,
    bool ignoreAbsense, bool warningAbsense)
{
    m_DefFile.erase();
    // load descriptions from registry file
    if ( fileName == "stdin" || fileName == "-" ) {
        LoadConfig(NcbiCin);
    }
    else {
        CNcbiIfstream in(fileName.c_str());
        if ( !in ) {
            if ( ignoreAbsense ) {
                return;
            } else if (warningAbsense) {
                ERR_POST(Warning << "cannot open file " << fileName);
            } else {
                ERR_POST(Fatal << "cannot open file " << fileName);
            }
        }
        else {
            m_DefFile = fileName;
            LoadConfig(in);
        }
    }
}

void CCodeGenerator::AddConfigLine(const string& line)
{
    SIZE_TYPE bra = line.find('[');
    SIZE_TYPE ket = line.find(']');
    SIZE_TYPE eq = line.find('=', ket + 1);
    if ( bra != 0 || ket == NPOS || eq == NPOS )
        ERR_POST(Fatal << "bad config line: " << line);
    
    m_Config.Set(line.substr(bra + 1, ket - bra - 1),
                 line.substr(ket + 1, eq - ket - 1),
                 line.substr(eq + 1));
}

CDataType* CCodeGenerator::ExternalResolve(const string& module,
                                           const string& name,
                                           bool exported) const
{
    string loc("CCodeGenerator::ExternalResolve: failed");
    try {
        return m_MainFiles.ExternalResolve(module, name, exported);
    }
    catch ( CAmbiguiousTypes& exc) {
        _TRACE(exc.what());
        NCBI_RETHROW_SAME(exc,loc);
    }
    catch ( CNotFoundException& _DEBUG_ARG(exc)) {
        _TRACE(exc.what());
        return m_ImportFiles.ExternalResolve(module, name, exported);
    }
    return NULL;  // Eliminate "return value expected" warning
}

CDataType* CCodeGenerator::ResolveInAnyModule(const string& name,
                                              bool exported) const
{
    string loc("CCodeGenerator::ResolveInAnyModule: failed");
    try {
        return m_MainFiles.ResolveInAnyModule(name, exported);
    }
    catch ( CAmbiguiousTypes& exc) {
        _TRACE(exc.what());
        NCBI_RETHROW_SAME(exc,loc);
    }
    catch ( CNotFoundException& _DEBUG_ARG(exc)) {
        _TRACE(exc.what());
        return m_ImportFiles.ResolveInAnyModule(name, exported);
    }
    return NULL;  // Eliminate "return value expected" warning
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

const string& CCodeGenerator::ResolveFileName(const string& name) const
{
    TOutputFiles::const_iterator i = m_Files.find(name);
    if (i != m_Files.end()) {
        return i->second->GetFileBaseName();
    }
    return name;
}

void CCodeGenerator::IncludeAllMainTypes(void)
{
    ITERATE ( CFileSet::TModuleSets, msi, m_MainFiles.GetModuleSets() ) {
        ITERATE ( CFileModules::TModules, mi, (*msi)->GetModules() ) {
            const CDataTypeModule* module = mi->get();
            ITERATE ( CDataTypeModule::TDefinitions, ti,
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
    ITERATE ( TTypeNames, i, typeNames ) {
        m_Config.Set(*i, "_class", "-");
        m_GenerateTypes.erase(*i);
    }
}

void CCodeGenerator::IncludeTypes(const string& typeList)
{
    GetTypes(m_GenerateTypes, typeList);
}

struct string_nocase_less
{
    bool operator()(const string& s1, const string& s2) const
    {
        return (NStr::CompareNocase(s1, s2) < 0);
    }
};

void CCodeGenerator::GenerateCode(void)
{
    // collect types
    ITERATE ( TTypeNames, ti, m_GenerateTypes ) {
        CollectTypes(ResolveMain(*ti), eRoot);
    }

    {
        set<string,string_nocase_less> names;
        ITERATE ( TOutputFiles, filei, m_Files ) {
            CFileCode* code = filei->second.get();
            string fname;
            for ( fname = code->GetFileBaseName();
                  names.find(fname) != names.end();) {
                fname = code->ChangeFileBaseName();
            }
            names.insert(fname);
        }
    }

    // generate output files
    string outdir_cpp, outdir_hpp;
    list<string> listGenerated, listUntouched;
    map<string,string> module_names;
    ITERATE ( TOutputFiles, filei, m_Files ) {
        CFileCode* code = filei->second.get();
        code->GetModuleNames( module_names);
        code->UseQuotedForm(m_UseQuotedForm);
        code->GenerateCode();
        string fileName;
        code->GenerateHPP(m_HPPDir, fileName);
        if (outdir_hpp.empty()) {
            CDirEntry entry(fileName);
            outdir_hpp = entry.GetDir();
        }
        code->GenerateCPP(m_CPPDir, fileName);
        if (outdir_cpp.empty()) {
            CDirEntry entry(fileName);
            outdir_cpp = entry.GetDir();
        }
        if (code->GenerateUserHPP(m_HPPDir, fileName)) {
            listGenerated.push_back( fileName);
        } else {
            listUntouched.push_back( fileName);
        }
        if (code->GenerateUserCPP(m_CPPDir, fileName)) {
            listGenerated.push_back( fileName);
        } else {
            listUntouched.push_back( fileName);
        }
    }

    // generate Doxygen group description
    if (CClassCode::GetDoxygenComments() && !module_names.empty()) {
        string ingroup_name =
            m_DoxygenIngroup.empty() ? "DatatoolGeneratedClasses" : m_DoxygenIngroup;
        CDirEntry entry(GetMainModules().GetModuleSets().front()->GetSourceFileName());
        string fileName =
            Path(m_HPPDir, Path(m_FileNamePrefix, entry.GetBase() + "_doxygen.h"));
        CNcbiOfstream doxyfile(fileName.c_str());
        if ( doxyfile.is_open() ) {
            CFileCode::WriteCopyrightHeader( doxyfile);
            doxyfile <<
                " *  File Description:\n"
                " *    This file was generated by application DATATOOL\n"
                " *    It contains comment blocks for DOXYGEN metamodules\n"
                " *\n"
                " * ===========================================================================\n"
                " */\n";
            if (CClassCode::GetDoxygenGroup().empty()) {
                map<string,string>::iterator i;
                for (i = module_names.begin(); i != module_names.end(); ++i) {
                    doxyfile << "\n\n/** @defgroup dataspec_" << i->first << " ";
                    if (m_DoxygenGroupDescription.empty()) {
                        doxyfile << "Code generated by DATATOOL from "
                            << i->second << " (module \'" << i->first << "\')";
                    } else {
                        doxyfile << m_DoxygenGroupDescription;
                    }
                    doxyfile << "\n *  @ingroup " << ingroup_name << "\n */\n\n";
                }
            } else {
                doxyfile << "\n\n/** @defgroup ";
                doxyfile << CClassCode::GetDoxygenGroup() << " ";
                if (m_DoxygenGroupDescription.empty()) {
                    doxyfile << "Code generated by DATATOOL";
                } else {
                    doxyfile << m_DoxygenGroupDescription;
                }
                doxyfile << "\n *  @ingroup " << ingroup_name << "\n */\n\n";
            }
        }
    }

    if ( !m_FileListFileName.empty() ) {
        string fileName(Path(m_CPPDir,Path(m_FileNamePrefix,m_FileListFileName)));
        CNcbiOfstream fileList(fileName.c_str());
        if ( !fileList ) {
            ERR_POST(Fatal <<
                     "cannot create file list file: " << m_FileListFileName);
        }
        
        fileList << "GENFILES =";
        {
            ITERATE ( TOutputFiles, filei, m_Files ) {
                fileList << ' ' << filei->second->GetFileBaseName();
            }
        }
        fileList << "\n";
        fileList << "GENFILES_LOCAL =";
        {
            ITERATE ( TOutputFiles, filei, m_Files ) {
                fileList << ' ' << BaseName(
                    filei->second->GetFileBaseName());
            }
        }
        fileList << "\n";
        
        // generation report
        for (int  user=0;  user<2; ++user)  {
        for (int local=0; local<2; ++local) {
        for (int   cpp=0;   cpp<2; ++cpp)   {
            fileList << (user ? "SKIPPED" : "GENERATED") << "_"
                << (cpp ? "CPP" : "HPP") << (local ? "_LOCAL" : "") << " =";
            list<string> *lst = (user ? &listUntouched : &listGenerated);
            for (list<string>::iterator i=lst->begin();
                i != lst->end(); ++i) {
                CDirEntry entry(*i);
                bool is_cpp = (NStr::CompareNocase(entry.GetExt(),".cpp")==0);
                if ((is_cpp && cpp) || (!is_cpp && !cpp)) {
                    fileList << ' ';
                    if (local) {
                        fileList << entry.GetBase();
                    } else {
                        string pp = entry.GetPath();
                        size_t found;
                        if (is_cpp) {
                            if (!m_CPPDir.empty() &&
                                (found = pp.find(m_CPPDir)) == 0) {
                                pp.erase(0,m_CPPDir.length()+1);
                            }
                        } else {
                            if (!m_HPPDir.empty() &&
                                (found = pp.find(m_HPPDir)) == 0) {
                                pp.erase(0,m_HPPDir.length()+1);
                            }
                        }
                        CDirEntry ent(CDirEntry::ConvertToOSPath(pp));
                        fileList << ent.GetDir() << ent.GetBase();
                    }
                }

            }
            fileList << endl;
        }
        }
        }
    }

    if ( !m_CombiningFileName.empty() ) {
        // write combined files *__.cpp and *___.cpp
        for ( int i = 0; i < 2; ++i ) {
            const char* suffix = i? "_.cpp": ".cpp";
            string fileName = m_CombiningFileName + "__" + suffix;
            fileName = Path(m_CPPDir,Path(m_FileNamePrefix,fileName));
            CDelayedOfstream out(fileName);
            if ( !out )
                ERR_POST(Fatal << "Cannot create file: "<<fileName);
            
            if (!CFileCode::GetPchHeader().empty()) {
                out <<
                    "#include <" << CFileCode::GetPchHeader() << ">\n";
            }

            ITERATE ( TOutputFiles, filei, m_Files ) {
                out << "#include \""<<BaseName(
                    filei->second->GetFileBaseName())<<
                    suffix<<"\"\n";
            }

            out.close();
            if ( !out )
                ERR_POST(Fatal << "Error writing file "<<fileName);
        }
        // write combined *__.hpp file
        const char* suffix = ".hpp";
        // save to the includes directory
        string fileName = Path(m_HPPDir,
                               Path(m_FileNamePrefix,
                                    m_CombiningFileName + "__" + suffix));

        CDelayedOfstream out(fileName);
        if ( !out )
            ERR_POST(Fatal << "Cannot create file: " << fileName);

        ITERATE ( TOutputFiles, filei, m_Files ) {
            out << "#include " << (m_UseQuotedForm ? '\"' : '<') << GetStdPath(
                Path(m_FileNamePrefix, BaseName(
                    filei->second->GetFileBaseName())) + suffix) <<
                (m_UseQuotedForm ? '\"' : '>') << "\n";
        }

        out.close();
        if ( !out )
            ERR_POST(Fatal << "Error writing file " << fileName);
    }

    if (m_CreateCvsignore) {
        string ignoreName(".cvsignore");
        string extraName(".cvsignore.extra");

        for (int i=0; i<2; ++i) {
            bool is_cpp = (i==0);
            bool different_dirs = (outdir_cpp != outdir_hpp);
            string out_dir(is_cpp ? outdir_cpp : outdir_hpp);

            string ignorePath(Path(out_dir,ignoreName));
            // ios::out should be redundant, but some compilers
            // (GCC 2.9x, for one) seem to need it. :-/
            CNcbiOfstream ignoreFile(ignorePath.c_str(),
                ios::out | ((different_dirs || is_cpp) ? ios::trunc : ios::app));

            if (ignoreFile.is_open()) {

                if (different_dirs || is_cpp) {
                    ignoreFile << ignoreName << endl;
                }

// .cvsignore.extra
                if (different_dirs || is_cpp) {
                    string extraPath(Path(out_dir,extraName));
                    CNcbiIfstream extraFile(extraPath.c_str());
                    if (extraFile.is_open()) {
                        char buf[256];
                        while (extraFile.good()) {
                            extraFile.getline(buf, sizeof(buf));
                            string sbuf(NStr::TruncateSpaces(buf));
                            if (!sbuf.empty()) {
                                ignoreFile << sbuf << endl;
                            }
                        }
                    }
                }

// base classes (always generated)
                ITERATE ( TOutputFiles, filei, m_Files ) {
                    ignoreFile
                        << BaseName(filei->second->GetFileBaseName())
                        << "_." << (is_cpp ? "cpp" : "hpp") << endl;
                }

// user classes
                for (list<string>::iterator it = listGenerated.begin();
                    it != listGenerated.end(); ++it) {
                    CDirEntry entry(*it);
                    if (is_cpp == (NStr::CompareNocase(entry.GetExt(),".cpp")==0)) {
                        ignoreFile << entry.GetName() << endl;
                    }
                }

// combining files
                if ( !m_CombiningFileName.empty() ) {
                    if (is_cpp) {
                        ignoreFile << m_CombiningFileName << "__" << "_.cpp" << endl;
                        ignoreFile << m_CombiningFileName << "__" << ".cpp" << endl;
                    } else {
                        ignoreFile << m_CombiningFileName << "__" << ".hpp" << endl;
                    }
                }

// doxygen header
                if ( !is_cpp  &&  CClassCode::GetDoxygenComments()
                     &&  !module_names.empty() ) {
                    CDirEntry entry(GetMainModules().GetModuleSets().front()
                                    ->GetSourceFileName());
                    ignoreFile << entry.GetBase() << "_doxygen.h" << endl;
                }

// file list
                if ( is_cpp && !m_FileListFileName.empty() ) {
                    CDirEntry entry(Path(m_FileNamePrefix,m_FileListFileName));
                    ignoreFile << entry.GetName() << endl;
                }
            }
        }
    }

    GenerateClientCode();
}

void CCodeGenerator::GenerateClientCode(void)
{
    string clients = m_Config.Get("-", "clients");
    if (clients.empty()) {
        // // for compatibility with older specifications
        // GenerateClientCode("client", false);
    } else {
        // explicit name; must be enabled
        list<string> l;
        // if multiple items, may have whitespace, commas, or both...
        NStr::Split(clients, ", \t", l);
        ITERATE (list<string>, it, l) {
            if ( !it->empty() ) {
                GenerateClientCode(*it, true);
            }
        }
    }
}

void CCodeGenerator::GenerateClientCode(const string& name, bool mandatory)
{
    string class_name = m_Config.Get(name, "class");
    if (class_name.empty()) {
        if (mandatory) {
            ERR_POST(Fatal << "No configuration for mandatory client " + name);
        }
        return; // not configured
    }
    CFileCode code(this,Path(m_FileNamePrefix, name));
    code.UseQuotedForm(m_UseQuotedForm);
    code.AddType(new CClientPseudoDataType(*this, name, class_name));
    code.GenerateCode();
    string filename;
    code.GenerateHPP(m_HPPDir, filename);
    code.GenerateCPP(m_CPPDir, filename);
    code.GenerateUserHPP(m_HPPDir, filename);
    code.GenerateUserCPP(m_CPPDir, filename);
}

bool CCodeGenerator::AddType(const CDataType* type)
{
    string fileName = type->FileName();
    AutoPtr<CFileCode>& file = m_Files[fileName];
    if ( !file )
        file = new CFileCode(this,fileName);
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
    catch ( CNotFoundException& /* ignored */) {
    }
    return true;
}

void CCodeGenerator::CollectTypes(const CDataType* type, EContext /*context*/)
{
    if ( type->GetParentType() == 0 ) {
        if ( !AddType(type) )
            return;
    }

    if ( m_ExcludeRecursion )
        return;

    const CUniSequenceDataType* array =
        dynamic_cast<const CUniSequenceDataType*>(type);
    if ( array != 0 ) {
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
        catch ( CNotFoundException& exc) {
            ERR_POST(Warning <<
                     "Skipping type: " << user->GetUserTypeName() <<
                     ": " << exc.what());
            return;
        }
        if ( resolved->Skipped() ) {
            ERR_POST(Warning << "Skipping type: " << user->GetUserTypeName());
            return;
        }
        if ( !Imported(resolved) ) {
            CollectTypes(resolved, eReference);
        }
        return;
    }

    const CDataMemberContainerType* cont =
        dynamic_cast<const CDataMemberContainerType*>(type);
    if ( cont != 0 ) {
        // collect member's types
        ITERATE ( CDataMemberContainerType::TMembers, mi,
                  cont->GetMembers() ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eMember);
        }
        return;
    }
}

#if 0
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
        catch ( CNotFoundException& exc) {
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
        ITERATE ( CDataMemberContainerType::TMembers, mi,
                  choice->GetMembers() ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eMember); // eChoice
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
        ITERATE ( CDataMemberContainerType::TMembers, mi,
                  cont->GetMembers() ) {
            const CDataType* memberType = mi->get()->GetType();
            CollectTypes(memberType, eMember);
        }
        return;
    }
    if ( !AddType(type) )
        return;
}
#endif

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.59  2004/05/19 15:46:19  gouriano
* Add precompiled header into combining files as well
*
* Revision 1.58  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.57  2004/05/13 15:49:49  ucko
* When generating doxygen headers, list them in .cvsignore.
*
* Revision 1.56  2004/05/03 19:31:03  gouriano
* Made generation of DOXYGEN-style comments optional
*
* Revision 1.55  2004/04/30 02:05:05  ucko
* Make ingroup_name a full-fledged string rather than a reference,
* since it may be initialized by a temporary and should never be big
* enough for copying to be an issue.
*
* Revision 1.54  2004/04/29 20:11:39  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.53  2003/06/16 19:03:03  ucko
* Explicitly turn on ios::out when opening ignoreFile; needed with GCC 2.9x.
*
* Revision 1.52  2003/05/29 17:25:34  gouriano
* added possibility of generation .cvsignore file
*
* Revision 1.51  2003/04/18 20:40:17  ucko
* Oops, s/iterate/ITERATE/ in my latest change.
*
* Revision 1.50  2003/04/08 20:40:08  ucko
* Get client name(s) from [-]clients rather than hardcoding "client"
*
* Revision 1.49  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.48  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.47  2003/02/24 21:57:46  gouriano
* added odw flag - to issue a warning about missing DEF file
*
* Revision 1.46  2002/12/17 19:03:26  gouriano
* corrected ResolveFileName for external definitions
*
* Revision 1.45  2002/12/17 16:22:47  gouriano
* separated class name from the name of the file in which it will be written
*
* Revision 1.44  2002/11/13 00:46:07  ucko
* Add RPC client generator; CVS logs to end in generate.?pp
*
* Revision 1.43  2002/10/22 15:06:13  gouriano
* added possibillity to use quoted syntax form for generated include files
*
* Revision 1.42  2002/10/01 17:04:31  gouriano
* corrections to eliminate redundant info in the generation report
*
* Revision 1.41  2002/10/01 14:20:30  gouriano
* added more generation report data
*
* Revision 1.40  2002/09/30 19:16:10  gouriano
* changed location of "*.files" and "combining" files to CPP dir
*
* Revision 1.39  2001/12/07 18:56:51  grichenk
* Paths in "#include"-s made system-independent
*
* Revision 1.38  2001/12/03 14:50:27  juran
* Eliminate "return value expected" warning.
*
* Revision 1.37  2001/10/22 15:18:19  grichenk
* Fixed combined HPP generation.
*
* Revision 1.36  2001/10/18 20:10:34  grichenk
* Save combining header on -oc
*
* Revision 1.35  2001/08/31 20:05:46  ucko
* Fix ICC build.
*
* Revision 1.34  2000/11/27 18:19:48  vasilche
* Datatool now conforms CNcbiApplication requirements.
*
* Revision 1.33  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.32  2000/06/16 16:31:39  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.31  2000/05/24 20:57:14  vasilche
* Use new macro _DEBUG_ARG to avoid warning about unused argument.
*
* Revision 1.30  2000/05/24 20:09:28  vasilche
* Implemented DTD generation.
*
* Revision 1.29  2000/04/10 19:33:22  vakatov
* Get rid of a minor compiler warning
*
* Revision 1.28  2000/04/07 19:26:27  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.27  2000/03/07 14:06:32  vasilche
* Added generation of reference counted objects.
*
* Revision 1.26  2000/02/01 21:48:00  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
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
