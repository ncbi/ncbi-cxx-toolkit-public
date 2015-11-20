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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
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
#include "rpcgen.hpp"
#include "code.hpp"
#include "classstr.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_MainGen

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

const CMemoryRegistry& CCodeGenerator::GetConfig(void) const
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

void CCodeGenerator::ResetDefaultNamespace(void)
{
    m_DefaultNamespace.Reset();
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
                ERR_POST_X(1, Warning << "cannot open file " << fileName);
            } else {
                ERR_POST_X(2, Fatal << "cannot open file " << fileName);
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
        ERR_POST_X(3, Fatal << "bad config line: " << line);
    
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

void CCodeGenerator::UndoGenerateCode(void)
{
    if ( m_FileListFileName.empty() ) {
        return;
    }
    string fileName( MakeAbsolutePath(
        Path(m_CPPDir,Path(m_FileNamePrefix,m_FileListFileName))));
    CNcbiIfstream fileList(fileName.c_str());
    if ( fileList.is_open() ) {
        string strline;
        string fileroot;
        while ( NcbiGetlineEOL(fileList, strline) ) {
            if ( !NStr::StartsWith(strline, "GENFILES =") ) {
                continue;
            }
            NStr::ReplaceInPlace(strline,"GENFILES =","");
            list<string> files;
            NStr::Split( strline, " ", files);
            ITERATE(list<string>, f, files) {
                CFile fo;
                fo.Reset( MakeAbsolutePath( Path(m_HPPDir, *f)) + "_.hpp");
                if (fo.Exists()) {
                    fo.Remove();
                }
                fo.Reset( MakeAbsolutePath( Path(m_CPPDir, *f)) + "_.cpp");
                if (fo.Exists()) {
                    fo.Remove();
                }
            }
        }
        fileList.close();
        CFile fo(fileName);
        if (fo.Exists()) {
            fo.Remove();
        }
    }
}

void CCodeGenerator::CheckFileNames(void)
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

void CCodeGenerator::GenerateCode(void)
{
    // collect types
    ITERATE ( TTypeNames, ti, m_GenerateTypes ) {
        CollectTypes(ResolveMain(*ti), eRoot);
    }
    CheckFileNames();

    // generate output files
    string outdir_cpp, outdir_hpp;
    list<string> listGenerated, listUntouched;
    list<string> allGeneratedHpp, allGeneratedCpp, allSkippedHpp, allSkippedCpp;
    map<string, pair<string,string> > module_names;
    ITERATE ( TOutputFiles, filei, m_Files ) {
        CFileCode* code = filei->second.get();
        code->GetModuleNames( module_names);
        code->UseQuotedForm(m_UseQuotedForm);
        code->GenerateCode();
        string fileName;
        code->GenerateHPP(m_HPPDir, fileName);
        allGeneratedHpp.push_back(fileName);
        if (outdir_hpp.empty()) {
            CDirEntry entry(fileName);
            outdir_hpp = entry.GetDir();
        }
        code->GenerateCPP(m_CPPDir, fileName);
        allGeneratedCpp.push_back(fileName);
        if (outdir_cpp.empty()) {
            CDirEntry entry(fileName);
            outdir_cpp = entry.GetDir();
        }
        if (code->GenerateUserHPP(m_HPPDir, fileName)) {
            listGenerated.push_back( fileName);
            allGeneratedHpp.push_back(fileName);
        } else {
            listUntouched.push_back( fileName);
            allSkippedHpp.push_back(fileName);
        }
        if (code->GenerateUserCPP(m_CPPDir, fileName)) {
            listGenerated.push_back( fileName);
            allGeneratedCpp.push_back(fileName);
        } else {
            listUntouched.push_back( fileName);
            allSkippedCpp.push_back(fileName);
        }
    }
    list<string> module_inc, module_src;
    GenerateModuleHPP(Path(m_HPPDir,m_FileNamePrefix), module_inc);
    GenerateModuleCPP(Path(m_CPPDir,m_FileNamePrefix), module_src);

    GenerateDoxygenGroupDescription(module_names);
    GenerateCombiningFile(module_inc, module_src, allGeneratedHpp, allGeneratedCpp);
	listGenerated.insert(listGenerated.end(), module_inc.begin(), module_inc.end());
	allGeneratedHpp.insert(allGeneratedHpp.end(), module_inc.begin(), module_inc.end());
	listGenerated.insert(listGenerated.end(), module_src.begin(), module_src.end());
	allGeneratedCpp.insert(allGeneratedCpp.end(), module_src.begin(), module_src.end());
    GenerateFileList(listGenerated, listUntouched,
        allGeneratedHpp, allGeneratedCpp, allSkippedHpp, allSkippedCpp);
    GenerateCvsignore(outdir_cpp, outdir_hpp, listGenerated, module_names);
    GenerateClientCode();
}


void CCodeGenerator::GenerateDoxygenGroupDescription(
    map<string, pair<string,string> >& module_names)
{
    if (!CClassCode::GetDoxygenComments() || module_names.empty()) {
        return;
    }
    string ingroup_name =
        m_DoxygenIngroup.empty() ? "DatatoolGeneratedClasses" : m_DoxygenIngroup;
    CDirEntry entry(GetMainModules().GetModuleSets().front()->GetSourceFileName());
    string fileName = MakeAbsolutePath(
        Path(m_HPPDir, Path(m_FileNamePrefix, entry.GetBase() + "_doxygen.h")));
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
            map<string, pair<string,string> >::iterator i;
            for (i = module_names.begin(); i != module_names.end(); ++i) {
                doxyfile << "\n\n/** @defgroup dataspec_" << i->second.second << " ";
                if (m_DoxygenGroupDescription.empty()) {
                    doxyfile << "Code generated by DATATOOL from "
                        << i->second.first << " (module \'" << i->first << "\')";
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


void CCodeGenerator::GenerateFileList(
    const list<string>& generated, const list<string>& untouched,
    list<string>& allGeneratedHpp, list<string>& allGeneratedCpp,
    list<string>& allSkippedHpp, list<string>& allSkippedCpp)
{
    if ( m_FileListFileName.empty() ) {
        return;
    }
    string fileName( MakeAbsolutePath(
        Path(m_CPPDir,Path(m_FileNamePrefix,m_FileListFileName))));
    CNcbiOfstream fileList(fileName.c_str());
    if ( !fileList ) {
        ERR_POST_X(4, Fatal <<
                    "cannot create file list file: " << m_FileListFileName);
    }
    
    fileList << "GENFILES =";
    {
        ITERATE ( TOutputFiles, filei, m_Files ) {
            string tmp(filei->second->GetFileBaseName());
#if defined(NCBI_OS_MSWIN)
            tmp = NStr::Replace(tmp,"\\","/");
#endif                
            fileList << ' ' << tmp;
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
        const list<string> *lst = (user ? &untouched : &generated);
        for (list<string>::const_iterator i=lst->begin();
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
                    string tmp(ent.GetDir(CDirEntry::eIfEmptyPath_Empty));
#if defined(NCBI_OS_MSWIN)
                    tmp = NStr::Replace(tmp,"\\","/");
#endif                
                    fileList << tmp << ent.GetBase();
                }
            }

        }
        fileList << endl;
    }
    }
    }

    string flist, flist_local;
    ITERATE( list<string>, p, allGeneratedHpp) {
        string tmp(*p);
        flist_local += ' ' + CDirEntry(tmp).GetName();
        if (!m_HPPDir.empty() && tmp.find(m_HPPDir) == 0) {
            tmp.erase(0,m_HPPDir.length()+1);
        }
        tmp = CDirEntry::ConvertToOSPath(tmp);
#if defined(NCBI_OS_MSWIN)
        tmp = NStr::Replace(tmp,"\\","/");
#endif                
        flist += ' ' +tmp;
    }
    fileList << "ALLGENERATED_HPP =" << flist << '\n';
    fileList << "ALLGENERATED_HPP_LOCAL =" << flist_local << '\n';
    
    flist.erase(); flist_local.erase();
    ITERATE( list<string>, p, allSkippedHpp) {
        string tmp(*p);
        flist_local += ' ' + CDirEntry(tmp).GetName();
        if (!m_HPPDir.empty() && tmp.find(m_HPPDir) == 0) {
            tmp.erase(0,m_HPPDir.length()+1);
        }
        tmp = CDirEntry::ConvertToOSPath(tmp);
#if defined(NCBI_OS_MSWIN)
        tmp = NStr::Replace(tmp,"\\","/");
#endif                
        flist += ' ' +tmp;
    }
    fileList << "ALLSKIPPED_HPP =" << flist << '\n';
    fileList << "ALLSKIPPED_HPP_LOCAL =" << flist_local << '\n';

    flist.erase(); flist_local.erase();
    ITERATE( list<string>, p, allGeneratedCpp) {
        string tmp(*p);
        flist_local += ' ' + CDirEntry(tmp).GetName();
        if (!m_CPPDir.empty() && tmp.find(m_CPPDir) == 0) {
            tmp.erase(0,m_CPPDir.length()+1);
        }
        tmp = CDirEntry::ConvertToOSPath(tmp);
#if defined(NCBI_OS_MSWIN)
        tmp = NStr::Replace(tmp,"\\","/");
#endif                
        flist += ' ' +tmp;
    }
    fileList << "ALLGENERATED_CPP =" << flist << '\n';
    fileList << "ALLGENERATED_CPP_LOCAL =" << flist_local << '\n';
    
    flist.erase(); flist_local.erase();
    ITERATE( list<string>, p, allSkippedCpp) {
        string tmp(*p);
        flist_local += ' ' + CDirEntry(tmp).GetName();
        if (!m_CPPDir.empty() && tmp.find(m_CPPDir) == 0) {
            tmp.erase(0,m_CPPDir.length()+1);
        }
        tmp = CDirEntry::ConvertToOSPath(tmp);
#if defined(NCBI_OS_MSWIN)
        tmp = NStr::Replace(tmp,"\\","/");
#endif                
        flist += ' ' +tmp;
    }
    fileList << "ALLSKIPPED_CPP =" << flist << '\n';
    fileList << "ALLSKIPPED_CPP_LOCAL =" << flist_local << '\n';

}

void CCodeGenerator::GenerateCombiningFile(
    const list<string>& module_inc, const list<string>& module_src,
    list<string>& allHpp, list<string>& allCpp)
{
    if ( m_CombiningFileName.empty() ) {
        return;
    }
    // write combined files *__.cpp and *___.cpp
    for ( int i = 0; i < 2; ++i ) {
        const char* suffix = i? "_.cpp": ".cpp";
        string fileName = m_CombiningFileName + "__" + suffix;
        fileName = Path(m_CPPDir,Path(m_FileNamePrefix,fileName));
        allCpp.push_back(fileName);
        fileName = MakeAbsolutePath(fileName);
        CNcbiOfstream out(fileName.c_str());
        if ( !out )
            ERR_POST_X(5, Fatal << "Cannot create file: "<<fileName);
        
        if (!CFileCode::GetPchHeader().empty()) {
            out <<
                "#include <" << CFileCode::GetPchHeader() << ">\n";
        }

        ITERATE ( TOutputFiles, filei, m_Files ) {
            out << "#include \""<<BaseName(
                filei->second->GetFileBaseName())<<
                suffix<<"\"\n";
        }
        if (i) {
            ITERATE( list<string>, m, module_src) {
                out << "#include \"" << CDirEntry(*m).GetBase() << ".cpp\"\n";
            }
        }

        out.close();
        if ( !out )
            ERR_POST_X(6, Fatal << "Error writing file "<<fileName);
    }
    // write combined *__.hpp file
    const char* suffix = ".hpp";
    // save to the includes directory
    string fileName = Path(m_HPPDir, Path(m_FileNamePrefix,
                           m_CombiningFileName + "__" + suffix));
    allHpp.push_back(fileName);
    fileName = MakeAbsolutePath(fileName);

    CNcbiOfstream out(fileName.c_str());
    if ( !out )
        ERR_POST_X(7, Fatal << "Cannot create file: " << fileName);

    ITERATE ( TOutputFiles, filei, m_Files ) {
        out << "#include " << (m_UseQuotedForm ? '\"' : '<') << GetStdPath(
            Path(m_FileNamePrefix, BaseName(
                filei->second->GetFileBaseName())) + suffix) <<
            (m_UseQuotedForm ? '\"' : '>') << "\n";
    }
    ITERATE( list<string>, m, module_inc) {
        out << "#include " << (m_UseQuotedForm ? '\"' : '<')
        << GetStdPath(Path(m_FileNamePrefix, CDirEntry(*m).GetBase())) << ".hpp"
        << (m_UseQuotedForm ? '\"' : '>') << "\n";
    }

    out.close();
    if ( !out )
        ERR_POST_X(8, Fatal << "Error writing file " << fileName);
}

void CCodeGenerator::GenerateCvsignore(
    const string& outdir_cpp, const string& outdir_hpp,
    const list<string>& generated, map<string, pair<string,string> >& module_names)
{
    if (!m_CreateCvsignore) {
        return;
    }
    string ignoreName(".cvsignore");
    string extraName(".cvsignore.extra");

    for (int i=0; i<2; ++i) {
        bool is_cpp = (i==0);
        bool different_dirs = (outdir_cpp != outdir_hpp);
        string out_dir(is_cpp ? outdir_cpp : outdir_hpp);

        string ignorePath(MakeAbsolutePath(Path(out_dir,ignoreName)));
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
                        CTempString sbuf(NStr::TruncateSpaces_Unsafe(buf));
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
            for (list<string>::const_iterator it = generated.begin();
                it != generated.end(); ++it) {
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

// specification dump (somewhat hackishly)
            if ( const CArgValue& f
                 = CNcbiApplication::Instance()->GetArgs()["fd"] ) {
                ignoreFile << f.AsString() << endl;
            }
        }
    }
}

void CCodeGenerator::GenerateModuleHPP(const string& path, list<string>& generated) const
{
    set<string> modules;
    string module_name, current_module, filename, hppDefine;
    auto_ptr<CDelayedOfstream> out;
    CNamespace ns;

    bool isfound = false;
    do {    
        isfound = false;
        bool types_found = false;
        ITERATE ( TOutputFiles, filei, m_Files ) {
            CFileCode* code = filei->second.get();
            list<CTypeStrings*> filetypes;
            code->GetClasses( filetypes );
            module_name.clear();
            ITERATE(list<CTypeStrings*>, t, filetypes) {
                string module = (*t)->GetDoxygenModuleName();
                if (module_name.empty() || module.size() < module_name.size()) {
                    module_name = module;
                }
            }
            if (current_module.empty()) {
                if (modules.find(module_name) != modules.end()) {
                    continue;
                }
                modules.insert(module_name);
                current_module = module_name;
            } else if (current_module != module_name) {
                continue;
            }
            isfound = true;
            if ( !out.get()  ||  !out->is_open() ) {
                if (isdigit((unsigned char)current_module[0])) {
                    current_module.insert(current_module.begin(),'x');
                }
                filename = Path(path, current_module + "_module.hpp");
                out.reset(new CDelayedOfstream(filename.c_str()));
                if (!out->is_open()) {
                    ERR_POST_X(9, Fatal << "Cannot create file: " << filename);
                    return;
                }
                generated.push_back(filename);
                hppDefine = current_module + "_REGISTERMODULECLASSES_HPP";
                code->WriteCopyright(*out, false) <<
                    "\n"
                    "#ifndef " << hppDefine << "\n"
                    "#define " << hppDefine << "\n"
                    "\n#include <serial/serialbase.hpp>\n\n";
                ns.Set(code->GetNamespace(), *out, true);
                *out << '\n';
                if (!CClassCode::GetExportSpecifier().empty()) {
                    *out << CClassCode::GetExportSpecifier() << '\n';
                }
                *out <<
                    "void " << current_module << "_RegisterModuleClasses(void);\n\n";
            }
            ITERATE(list<CTypeStrings*>, t, filetypes) {
                if ((*t)->GetKind() == CTypeStrings::eKindObject) {
                    CClassTypeStrings* classtype = dynamic_cast<CClassTypeStrings*>(*t);
                    if (classtype && classtype->HaveTypeInfo()) {
                        types_found = true;
                    }
                }
            }
        }
        if (isfound && !types_found) {
            generated.pop_back();
            out->Discard();
        }
        if (out->is_open()) {
            ns.Reset(*out);
            *out <<
                "\n"
                "#endif // " << hppDefine << "\n";
            out->close();
            if ( !*out )
                ERR_POST_X(10, Fatal << "Error writing file " << filename);
        }
        current_module.erase();
    } while (isfound);
}

void CCodeGenerator::GenerateModuleCPP(const string& path, list<string>& generated) const
{
    set<string> modules;
    string module_name, current_module, filename, hppDefine;
    auto_ptr<CDelayedOfstream> out;
    CNcbiOstrstream out_inc;
    CNcbiOstrstream out_code;
    CNamespace ns;

    bool isfound = false;
    do {    
        isfound = false;
        bool types_found = false;
        ITERATE ( TOutputFiles, filei, m_Files ) {
            CFileCode* code = filei->second.get();
            list<CTypeStrings*> filetypes;
            code->GetClasses( filetypes );
            module_name.clear();
            ITERATE(list<CTypeStrings*>, t, filetypes) {
                string module = (*t)->GetDoxygenModuleName();
                if (module_name.empty() || module.size() < module_name.size()) {
                    module_name = module;
                }
            }
            if (current_module.empty()) {
                if (modules.find(module_name) != modules.end()) {
                    continue;
                }
                modules.insert(module_name);
                current_module = module_name;
            } else if (current_module != module_name) {
                continue;
            }
            isfound = true;
            if ( !out.get()  ||  !out->is_open()) {
                if (isdigit((unsigned char)current_module[0])) {
                    current_module.insert(current_module.begin(),'x');
                }
                filename = Path(path, current_module + "_module.cpp");
                string module_inc =
                    CDirEntry::ConcatPath( CDirEntry( code->GetUserHPPName() ).GetDir(),
                                           current_module + "_module.hpp");
                out_inc <<
                    "#include " << code->Include(module_inc) << "\n";
                out.reset(new CDelayedOfstream(filename.c_str()));
                if (!out->is_open()) {
                    ERR_POST_X(11, Fatal << "Cannot create file: " << filename);
                    return;
                }
                generated.push_back(filename);
                code->WriteCopyright(*out, false);
                *out << "\n";
                if (!CFileCode::GetPchHeader().empty()) {
                    *out <<
                        "#include <" << CFileCode::GetPchHeader() << ">\n";
                }
                ns.Set(code->GetNamespace(), out_code, false);
                out_code <<
                    "void " << current_module << "_RegisterModuleClasses(void)\n{\n";
            }
            set<string> user_includes;
            ITERATE(list<CTypeStrings*>, t, filetypes) {
                if ((*t)->GetKind() == CTypeStrings::eKindObject) {
                    CClassTypeStrings* classtype = dynamic_cast<CClassTypeStrings*>(*t);
                    if (classtype && classtype->HaveTypeInfo()) {
                        types_found = true;
                        string userhpp(code->Include(code->GetUserHPPName()));
                        if (user_includes.find(userhpp) == user_includes.end()) {
                            user_includes.insert(userhpp);
                            out_inc <<
                                "#include " << code->Include(code->GetUserHPPName()) << "\n";
                        }
                        out_code << "    "
                                 << code->GetClassNamespace(*t).ToString()
                                 << classtype->GetClassNameDT() << "::GetTypeInfo();\n";
                    }
                }
            }
        }
        if (isfound && !types_found) {
            generated.pop_back();
            out->Discard();
        }
        if (out->is_open()) {
            out_code << "}\n\n";
            ns.Reset(out_code);
            *out << string(CNcbiOstrstreamToString(out_inc))
                 << "\n\n"
                 << string(CNcbiOstrstreamToString(out_code));
            out_inc.seekp(0);
            out_code.seekp(0);
#ifdef NCBI_SHUN_OSTRSTREAM
            out_inc.str(kEmptyStr);
            out_code.str(kEmptyStr);
#endif
            out->close();
            if ( !*out )
                ERR_POST_X(12, Fatal << "Error writing file " << filename);
        }
        current_module.erase();
    } while (isfound);
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
        NStr::Split(clients, ", \t", l, NStr::fSplit_MergeDelimiters);
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
            ERR_POST_X(13, Fatal << "No configuration for mandatory client " + name);
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
        const CWsdlDataType* w = dynamic_cast<const CWsdlDataType*>(type);
        if (!w || w->GetWsdlType() == CWsdlDataType::eWsdlEndpoint) {
            if ( !AddType(type) ) {
                return;
            }
        }
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
            ERR_POST_X(14, Warning <<
                       "Skipping type: " << user->GetUserTypeName() <<
                       ": " << exc.what());
            return;
        }
        if ( resolved->Skipped() ) {
            ERR_POST_X(15, Warning << "Skipping type: " << user->GetUserTypeName());
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
            ERR_POST_X(16, Warning <<
                       "Skipping type: " << user->GetUserTypeName() <<
                       ": " << exc.what());
            return;
        }
        if ( resolved->Skipped() ) {
            ERR_POST_X(17, Warning << "Skipping type: " << user->GetUserTypeName());
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
            ERR_POST_X(18, Warning << "Skipping type: " << type->IdName());
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

void CCodeGenerator::ResolveImportRefs(void)
{
    ITERATE( CFileSet::TModuleSets, fs, GetMainModules().GetModuleSets()) {
        ITERATE(CFileModules::TModules, fm, (*fs)->GetModules()) {
            CDataTypeModule* head = const_cast<CDataTypeModule*>(fm->get());
            head->AddImportRef( head->GetName());
            ResolveImportRefs(*head,head);
        }
    }
}

void CCodeGenerator::ResolveImportRefs(CDataTypeModule& head, const CDataTypeModule* ref)
{
    if (ref) {
        ITERATE ( CDataTypeModule::TImports, i, ref->GetImports() ) {
            const string& s = (*i)->moduleName;
            if (head.AddImportRef(s)) {
                ResolveImportRefs(head,FindModuleByName(s));
            }
        }
    }
}

const CDataTypeModule* CCodeGenerator::FindModuleByName(const string& name) const
{
    ITERATE( CFileSet::TModuleSets, mm, GetMainModules().GetModuleSets()) {
        ITERATE(CFileModules::TModules, fm, (*mm)->GetModules()) {
            if ((*fm)->GetName() == name) {
                return fm->get();
            }
        }
    }
    ITERATE( CFileSet::TModuleSets, mi, GetImportModules().GetModuleSets()) {
        ITERATE(CFileModules::TModules, fm, (*mi)->GetModules()) {
            if ((*fm)->GetName() == name) {
                return fm->get();
            }
        }
    }
    ERR_POST_X(19, Error << "cannot find module " << name);
    return 0;
}

bool CCodeGenerator::GetOpt(const string& opt, string* value)
{
    string result;
    string key("-");
    string xopt = key + opt;
    if (m_Config.HasEntry(key, xopt)) {
        result = m_Config.Get(key, xopt);
        if (value) {
            *value = result;
        }
        return (result != key);
    }
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    const CArgValue& argv = args[opt];
    if (argv.HasValue()) {
        if (value) {
            *value = argv.AsString();
        }
        return true;
    }
    return false;
}


END_NCBI_SCOPE
